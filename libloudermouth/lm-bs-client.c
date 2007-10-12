/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Free Software Foundation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 */

#include <config.h>

#include <string.h>

#include <sys/stat.h> 
#include <sys/types.h>
#include <fcntl.h>

#if 0
/*#include <sys/socket.h>*/
/*#include <netinet/tcp.h>*/
/*#include <arpa/inet.h> */
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include <loudmouth/lm-error.h>
#include <loudmouth/lm-connection.h>
#include <loudmouth/lm-utils.h>

#include "lm-debug.h"
#include "lm-internals.h"
#include "lm-sock.h"
#include "lm-bs-session.h"
#include "lm-bs-client.h"

#define IO_READ_FLAGS    G_IO_IN | G_IO_PRI | G_IO_HUP | G_IO_ERR
#define IO_WRITE_FLAGS   G_IO_OUT | G_IO_ERR
#define IO_CONNECT_FLAGS G_IO_OUT | G_IO_ERR

#define IN_BUFFER_SIZE   1024

/* Stop connect attempt after 30 sec. */
#define CONNECT_TIMEOUT  30000

struct _LmBsClient {
	gint              ref_count;

	GMainContext     *context;
	gchar            *host;
	guint64           port;
	LmBsClientStatus  status;

	LmCallback       *connected_cb;
	LmCallback       *disconnected_cb;
	LmCallback       *data_read_cb;
	LmCallback       *data_written_cb;

	LmSocket          fd;
	struct addrinfo  *server_addr;

	GIOChannel       *io_channel;
	guint             io_watch;

	guint             timeout_id;

	GString          *out_buf;
};

static gboolean bs_client_read_cb                 (GIOChannel   *source,
						   GIOCondition  condition,
						   gpointer      user_data);
static gboolean bs_client_write_cb                (GIOChannel   *source,
						   GIOCondition  condition,
						   gpointer      user_data);
static guint    bs_client_add_watch               (LmBsClient   *client,
						   GIOChannel   *channel,
						   GIOCondition  condition,
						   GIOFunc       func);
static void     bs_client_remove_watch            (LmBsClient   *client);
static gboolean bs_client_connect_timeout         (LmBsClient   *client);
static gboolean bs_client_connection_failed       (LmBsClient   *client);
static gboolean bs_client_connection_succeeded    (LmBsClient   *client);
static gboolean bs_client_connect_cb              (GIOChannel   *source,
						   GIOCondition  condition,
						   gpointer      user_data);
static gint     bs_client_try_to_connect          (LmBsClient   *client);
static gboolean bs_client_set_server_address_info (LmBsClient   *client);
static void     bs_client_free                    (LmBsClient   *client);

static gboolean
bs_client_read_cb (GIOChannel   *source,
		   GIOCondition  condition,
		   gpointer      user_data)
{
	LmBsClient *client;
	LmCallback *cb;
	GIOStatus   io_status;
	GString    *read_data;
	gchar       buf[IN_BUFFER_SIZE];
	gsize       bytes_read;

	client = LM_BS_CLIENT (user_data);
	io_status = G_IO_STATUS_AGAIN;
	read_data = g_string_new (NULL);

	while (io_status == G_IO_STATUS_AGAIN) {
		io_status = g_io_channel_read_chars (client->io_channel,
						     buf, IN_BUFFER_SIZE,
						     &bytes_read,
						     NULL);
		g_string_append_len (read_data, buf, bytes_read);
	}

	if (io_status != G_IO_STATUS_NORMAL) {
		g_assert (read_data->len == 0);
		lm_bs_client_stop (client);
		bs_client_connection_failed (client);
		g_string_free (read_data, TRUE);
	
		return FALSE;
	}

	/* otherwise io_status != G_IO_STATUS_NORMAL */
	g_assert (read_data->len > 0);

	cb = client->data_read_cb;
	if (cb && cb->func) {
		(* ((LmBsClientReadFunction) cb->func)) (client,
							 cb->user_data,
							 read_data);
	}

	g_string_free (read_data, TRUE);

	return TRUE;
}

static gboolean
bs_client_write_cb (GIOChannel   *source,
		    GIOCondition  condition,
		    gpointer      user_data)
{
	LmBsClient   *client;
	LmCallback   *cb;
	GIOStatus     io_status;
	gsize         bytes_written;
	guint         total_written;
	
	client = LM_BS_CLIENT (user_data);
	io_status = G_IO_STATUS_AGAIN;
	
	total_written = 0;

	while (io_status == G_IO_STATUS_AGAIN) {
		io_status = g_io_channel_write_chars (client->io_channel, 
						      client->out_buf->str,
						      client->out_buf->len, 
						      &bytes_written,
						      NULL);
		g_string_erase (client->out_buf, 0, bytes_written);
		total_written += bytes_written;

		if (client->out_buf->len == 0) {
			break;
		}
	}

	if (io_status != G_IO_STATUS_NORMAL || total_written == 0) {
		lm_bs_client_stop (client);
		bs_client_connection_failed (client);
		return FALSE;
	}

	if (client->out_buf->len == 0) {
		bs_client_remove_watch (client);
		g_string_free (client->out_buf, FALSE);
		client->out_buf = NULL;
		cb = client->data_written_cb;

		if (cb && cb->func) {
			(* ((LmBsClientFunction) cb->func)) (client,
							     cb->user_data);
		}

		/* no more data to write, unregister write handler */
		return FALSE;
	}

	return TRUE;
}

static guint
bs_client_add_watch (LmBsClient   *client,
		     GIOChannel   *channel,
		     GIOCondition  condition,
		     GIOFunc       func)
{
	GSource *source;
	guint    id;

	g_return_val_if_fail (channel != NULL, 0);

	source = g_io_create_watch (channel, condition);

	g_source_set_callback (source, (GSourceFunc) func, client, NULL);

	id = g_source_attach (source, client->context);

	g_source_unref (source);

	return id;
}

static void
bs_client_remove_watch (LmBsClient *client)
{
	GSource *source;

	if (client->io_watch != 0) {
		source = g_main_context_find_source_by_id (client->context,
							   client->io_watch);
		if (source) {
			g_source_destroy (source);
		}

		client->io_watch = 0;
	}

	if (client->timeout_id) {
		source = g_main_context_find_source_by_id (client->context,
							   client->timeout_id);
		if (source) {
			g_source_destroy (source);
		}

		client->timeout_id = 0;
	}
}

static gboolean
bs_client_connect_timeout (LmBsClient *client)
{
	g_return_val_if_fail (client != NULL, FALSE);

	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
	       "Connect timeout, host: %s.\n", client->host);

	lm_bs_client_stop (client);
	bs_client_connection_failed (client);

	return FALSE;
}

static gboolean
bs_client_connection_failed (LmBsClient *client)
{
	LmCallback *cb;

	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
	       "Connection to %s failed.\n", client->host);

	bs_client_remove_watch (client);

	cb = client->disconnected_cb;
	if (cb && cb->func) {
		(* ((LmBsClientFunction) cb->func)) (client,
						     cb->user_data);
	}

	return FALSE;
}

static gboolean 
bs_client_connection_succeeded (LmBsClient *client)
{
	LmCallback *cb;

	bs_client_remove_watch (client);

	if (client->status == LM_BS_CLIENT_STATUS_CONNECTING) {
		client->status = LM_BS_CLIENT_STATUS_CONNECTED;
	}

	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
	       "Connection to %s succeeded.\n",
	       client->host);

	cb = client->connected_cb;
	if (cb && cb->func) {
		(* ((LmBsClientFunction) cb->func)) (client, 
						     cb->user_data);
	}

	return FALSE;
}

static gboolean 
bs_client_connect_cb (GIOChannel   *source,
		      GIOCondition  condition,
		      gpointer      user_data)
{
	LmBsClient *client;
	gint        result;
	gint        err;
	socklen_t   len;

	client = LM_BS_CLIENT (user_data);

	if (condition == G_IO_ERR) { 
		len = sizeof (err);
		_lm_sock_get_error (client->fd, &err, &len);

		if (!_lm_sock_is_blocking_error (err)) {
			g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
			       "Connection to %s %d failed.\n",
			       client->host,
			       (gint) client->port);

			bs_client_connection_failed (client);
			client->io_watch = 0;

			return FALSE;
		}
	}

	if (client->status != LM_BS_CLIENT_STATUS_CONNECTING) {
		return FALSE;
	}

	result = bs_client_try_to_connect (client);
	if (result < 0) {
		err = _lm_sock_get_last_error ();

		if (_lm_sock_is_blocking_success (err)) {
			bs_client_connection_succeeded (client);
		}

		if (client->status == LM_BS_CLIENT_STATUS_CONNECTING && 
		    !_lm_sock_is_blocking_error (err)) {
			g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
			       "Connection to %s failed.\n",
			       client->host);

			_lm_sock_close (client->fd);
			bs_client_connection_failed (client);
			client->io_watch = 0;

			return FALSE;
		}
	}

	return TRUE; 
}

static gint
bs_client_try_to_connect (LmBsClient *client)
{
	gint result;

	result = _lm_sock_connect (client->fd, 
				   client->server_addr->ai_addr, 
				   (int) client->server_addr->ai_addrlen);
	return result;
}

static gboolean
bs_client_set_server_address_info (LmBsClient *client)
{
	struct addrinfo  req;
	struct addrinfo *ans;
	int              result;

	memset (&req, 0, sizeof (req));

	req.ai_family   = AF_UNSPEC;
	req.ai_socktype = SOCK_STREAM;
	req.ai_protocol = IPPROTO_TCP;

	result = getaddrinfo (_lm_utils_hostname_to_punycode (client->host),
			      NULL,
			      &req,
			      &ans);

	if (result != 0) {
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
		       "Unable to resolve hostname: %s.\n", client->host);

		return FALSE;
	}

	((struct sockaddr_in *) ans->ai_addr)->sin_port = htons (client->port);
	client->server_addr = ans;

	return TRUE;
}

static void 
bs_client_free (LmBsClient *client)
{
	_lm_utils_free_callback (client->connected_cb);
	_lm_utils_free_callback (client->disconnected_cb);
	_lm_utils_free_callback (client->data_read_cb);
	_lm_utils_free_callback (client->data_written_cb);

	if (client->timeout_id != 0) {
		g_source_remove (client->timeout_id);
		client->timeout_id = 0;
	}

	lm_bs_client_stop (client);

	freeaddrinfo (client->server_addr);

	if (client->out_buf) {
		g_string_free (client->out_buf, TRUE);
		client->out_buf = NULL;
	}

	g_free (client->host);
	g_free (client);
}

/**
 * Creates a new instance of #LmBsClient . It is used to establish
 * a client socket to a @host / @port .Use #lm_bs_client_connect to
 * start the connect attempt and #lm_bs_client_stop to force a stop.
 */
LmBsClient * lm_bs_client_new (guint64      port,
			       const gchar *host,
			       LmCallback  *connected_cb,
			       LmCallback  *disconnected_cb,
			       LmCallback  *data_read_cb,
			       LmCallback  *data_written_cb)
{
	LmBsClient *client;
	
	_lm_sock_library_init ();
	lm_debug_init ();

	client = g_new0 (LmBsClient, 1);

	client->host              = g_strdup (host);
	client->port              = port;
	client->connected_cb      = connected_cb;
	client->disconnected_cb   = disconnected_cb;
	client->data_read_cb      = data_read_cb;
	client->data_written_cb   = data_written_cb;
	client->context           = NULL;
	client->io_channel        = NULL;
	client->fd                = -1;
	client->timeout_id        = 0;
	client->ref_count         = 1;

	return client;
}

LmBsClient * lm_bs_client_new_from_fd (gint          fd,
				       GMainContext *context)
{
	LmBsClient *client;

	client = lm_bs_client_new (0, NULL, NULL, NULL, NULL, NULL);
	client->fd = fd;
	client->context = context;

	client->io_channel = g_io_channel_unix_new (client->fd);
	g_io_channel_set_encoding (client->io_channel, NULL, NULL);
	g_io_channel_set_buffered (client->io_channel, FALSE);
	
	_lm_sock_set_blocking (client->fd, FALSE);
	client->status = LM_BS_CLIENT_STATUS_CONNECTED;

	return client;
}

LmBsClient *
lm_bs_client_new_with_context (guint64       port,
			       const gchar  *host,
			       LmCallback   *connected_cb,
			       LmCallback   *disconnected_cb,
			       LmCallback   *data_read_cb,
			       LmCallback   *data_written_cb,
			       GMainContext *context)
{
	LmBsClient *client;

	client = lm_bs_client_new (port, 
				   host, 
				   connected_cb, 
				   disconnected_cb,
				   data_read_cb,
				   data_written_cb);
	client->context = context;

	return client;
}

guint
lm_bs_client_connect (LmBsClient *client)
{
	GSource         *source;
	struct addrinfo *addr;

	if (!bs_client_set_server_address_info (client)) {
		g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
		       "Invalid address %s.\n",
		       client->host);
		return -1;
	}

	addr = client->server_addr;
	g_log (LM_LOG_DOMAIN, LM_LOG_LEVEL_NET,
	       "Establishing connection to %s.\n",
	       client->host);

	client->fd = _lm_sock_makesocket (addr->ai_family,
					  addr->ai_socktype,
					  addr->ai_protocol);

	client->io_channel = g_io_channel_unix_new (client->fd);
	g_io_channel_set_encoding (client->io_channel, NULL, NULL);
	g_io_channel_set_buffered (client->io_channel, FALSE);

	_lm_sock_set_blocking (client->fd, FALSE);

	client->status = LM_BS_CLIENT_STATUS_CONNECTING;
	client->io_watch = bs_client_add_watch (client,
						client->io_channel,
						IO_CONNECT_FLAGS,
						(GIOFunc) bs_client_connect_cb);

	source = g_timeout_source_new (CONNECT_TIMEOUT);
	g_source_set_callback (source,
			       (GSourceFunc) bs_client_connect_timeout,
			       client,
			       NULL);
	client->timeout_id = g_source_attach (source, client->context);

	return 0;
}

guint
lm_bs_client_stop (LmBsClient *client)
{
	bs_client_remove_watch (client);

	if (client->fd != -1) {
		_lm_sock_shutdown (client->fd);
		_lm_sock_close (client->fd);
		client->fd = -1;
	}

	if (client->io_channel != NULL) {
		g_io_channel_unref (client->io_channel);
	}

	client->io_channel = NULL;
	client->status = LM_BS_CLIENT_STATUS_INTERRUPTED;

	return 0;
}

LmBsClient*
lm_bs_client_ref (LmBsClient *client)
{
	g_return_val_if_fail (client != NULL, NULL);

	client->ref_count++;

	return client;
}

void
lm_bs_client_unref (LmBsClient *client)
{
	g_return_if_fail (client != NULL);

	client->ref_count--;

	if (client->ref_count == 0) {
		bs_client_free (client);
	}
}

gchar *
lm_bs_client_get_host (LmBsClient *client)
{
	return client->host;
}

LmBsClientStatus 
lm_bs_client_get_status (LmBsClient *client)
{
	return client->status;
}

LmSocket 
lm_bs_client_get_fd (LmBsClient *client)
{
	return client->fd;
}

void
lm_bs_client_set_data_read_cb (LmBsClient *client,
			       LmCallback *data_read_cb)
{
	_lm_utils_free_callback (client->data_read_cb);
	client->data_read_cb = data_read_cb;
}

void
lm_bs_client_set_data_written_cb (LmBsClient *client,
				  LmCallback *data_written_cb)
{
	_lm_utils_free_callback (client->data_written_cb);
	client->data_written_cb = data_written_cb;
}

void
lm_bs_client_set_disconnected_cb (LmBsClient *client,
				  LmCallback *disconnected_cb)
{
	_lm_utils_free_callback (client->disconnected_cb);
	client->disconnected_cb = disconnected_cb;
}

void
lm_bs_client_write_data (LmBsClient *client,
			 GString    *data)
{
	bs_client_remove_watch (client);

	if (data->len == 0) {
		g_string_free (data, FALSE);
		return;
	}

	if (client->out_buf) {
		g_string_append_len (client->out_buf, data->str, data->len);
		g_string_free (data, TRUE);
	} else {
		client->out_buf = data;
	}

	client->io_watch = bs_client_add_watch (client,
						client->io_channel,
						IO_WRITE_FLAGS,
						(GIOFunc) bs_client_write_cb);
}

void
lm_bs_client_do_read (LmBsClient *client)
{
	bs_client_remove_watch (client);

	client->io_watch = bs_client_add_watch (client,
						client->io_channel,
						IO_READ_FLAGS,
						(GIOFunc) bs_client_read_cb);
}

void
lm_bs_client_remove_watch (LmBsClient *client)
{
	bs_client_remove_watch (client);
}
