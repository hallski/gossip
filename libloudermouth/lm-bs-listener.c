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
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <glib.h>

#include <loudmouth/lm-connection.h>
#include <loudmouth/lm-utils.h>

#include "lm-sock.h"
#include "lm-debug.h"
#include "lm-internals.h"

#include "lm-bs-client.h"
#include "lm-bs-listener.h"

#define IO_READ_FLAGS G_IO_IN | G_IO_PRI | G_IO_HUP

struct _LmBsListener {
	gint          ref_count;

	GMainContext *context;
	guint         port;

	LmCallback   *new_client_cb;
	LmCallback   *disconnected_cb;
	LmSocket      fd;

	GIOChannel   *io_channel;
	guint         io_watch;
};

static void     bs_listener_free                 (LmBsListener  *listener);
static guint    bs_listener_add_watch            (LmBsListener  *listener,
						  GIOChannel    *channel,
						  GIOCondition   condition,
						  GIOFunc        func);
static gboolean bs_listener_accept_connection_cb (GIOChannel    *source,
						  GIOCondition   condition,
						  gpointer       user_data);
static gboolean bs_listener_start                (LmBsListener  *listener);
static void     bs_listener_bind                 (LmBsListener  *listener,
						  GError       **error);
static void     bs_listener_create               (LmBsListener  *listener);

static void
bs_listener_free (LmBsListener *listener)
{
	_lm_utils_free_callback (listener->disconnected_cb);
	lm_bs_listener_stop (listener);

	if (listener->context != NULL) {
		g_main_context_unref (listener->context);
	}

	g_free (listener);
}

static guint
bs_listener_add_watch (LmBsListener *listener,
		       GIOChannel   *channel,
		       GIOCondition  condition,
		       GIOFunc       func)
{
	GSource *source;
	guint    id;

	g_return_val_if_fail (channel != NULL, 0);

	source = g_io_create_watch (channel, condition);

	g_source_set_callback (source, (GSourceFunc) func, listener, NULL);

	id = g_source_attach (source, listener->context);

	g_source_unref (source);

	return id;
}

static gboolean bs_listener_accept_connection_cb (GIOChannel   *source, 
						  GIOCondition  condition,
						  gpointer      user_data)
{
	LmBsListener *listener;
	gint          new_fd;

	listener = LM_BS_LISTENER (user_data);

	new_fd = accept (listener->fd, NULL, NULL);
	if (new_fd != -1) {
		LmCallback *cb = listener->new_client_cb;
		if (cb) {
			(* ((LmBsNewClientFunction) cb->func)) ((guint) new_fd,
								cb->user_data);
		}
	}

	return TRUE;
}


static gboolean
bs_listener_start (LmBsListener *listener)
{
	int result;

	listener->io_channel = g_io_channel_unix_new (listener->fd);
	g_io_channel_set_encoding (listener->io_channel, NULL, NULL);
	g_io_channel_set_buffered (listener->io_channel, FALSE);

	_lm_sock_set_blocking (listener->fd, FALSE);

	listener->io_watch = bs_listener_add_watch (listener,
						    listener->io_channel,
						    IO_READ_FLAGS,
						    (GIOFunc) bs_listener_accept_connection_cb);

	result = listen (listener->fd, SOMAXCONN);

	return (result != -1);
}

static void 
bs_listener_bind (LmBsListener *listener, GError **error)
{

	struct sockaddr_in  serveraddr;
	int                 serveraddr_len;
	int                 result;

	g_return_if_fail (listener->fd != -1);

	serveraddr_len = sizeof (serveraddr);
	memset (&serveraddr, 0, serveraddr_len);

	serveraddr.sin_family = PF_INET;
	serveraddr.sin_addr.s_addr =  htonl (INADDR_ANY);

	result = bind (listener->fd,
		       (struct sockaddr *) &serveraddr,
		       serveraddr_len);
	memset (&serveraddr, 0, serveraddr_len);

	/* Get info on the bound port */
	if (getsockname (listener->fd,
			 (struct sockaddr *) &serveraddr, 
			 (socklen_t *) &serveraddr_len)) {
		return;
	}

	listener->port = ntohs (serveraddr.sin_port);
}

static void 
bs_listener_create (LmBsListener *listener)
{
	int flag;

	listener->fd = _lm_sock_makesocket (PF_INET, SOCK_STREAM, IPPROTO_TCP);

	flag = 1;
	setsockopt (listener->fd,
		    SOL_SOCKET,
		    SO_REUSEADDR,
		    &flag,
		    sizeof (int));

	/* Remove the Nagle algorhytm */
	setsockopt (listener->fd,
		    IPPROTO_TCP,
		    TCP_NODELAY,
		    &flag,
		    sizeof (int));
}

LmBsListener *
lm_bs_listener_new (void)
{
	LmBsListener *listener;
	
	_lm_sock_library_init ();

	listener = g_new0 (LmBsListener, 1);

	listener->port              = 0;
	listener->disconnected_cb   = NULL;
	listener->new_client_cb     = NULL;
	listener->fd                = -1;

	listener->ref_count         = 1;

	return listener;
}

LmBsListener *
lm_bs_listener_new_with_context (GMainContext *context)
{
	LmBsListener *listener;
	
	listener = lm_bs_listener_new ();
	listener->context = context;

	if (context != NULL) {
		g_main_context_ref (context);
	}

	return listener;
}

void
lm_bs_listener_set_new_client_function (LmBsListener          *listener,
					LmBsNewClientFunction  function,
					gpointer               user_data,
					GDestroyNotify         notify)
{
	g_return_if_fail (listener != NULL);

	if (listener->new_client_cb) {
		_lm_utils_free_callback (listener->new_client_cb);
	}

	if (function) {
		listener->new_client_cb = _lm_utils_new_callback (function,
								  user_data,
								  notify);
	} else {
		listener->new_client_cb = NULL;
	}
}

guint
lm_bs_listener_start (LmBsListener *listener)
{
	bs_listener_create (listener);
	bs_listener_bind (listener, NULL);

	if (bs_listener_start (listener)) {
		return listener->port;
	}

	return -1;
}

guint
lm_bs_listener_get_port (LmBsListener *listener)
{
	return listener->port;
}

void
lm_bs_listener_stop (LmBsListener *listener)
{
	LmCallback *cb;
	GSource    *source;

	cb = listener->disconnected_cb;
	_lm_utils_free_callback (listener->new_client_cb);

	if (listener->io_watch != 0) {
		source = g_main_context_find_source_by_id (listener->context,
							   listener->io_watch);
		if (source) {
			g_source_destroy (source);
		}

		listener->io_watch = 0;
	}

	if (listener->fd != -1) {
		_lm_sock_shutdown (listener->fd);
		_lm_sock_close (listener->fd);
		listener->fd = -1;
	}

	if (cb && cb->func) {
		(* ((LmBsClientFunction) cb->func)) (NULL, cb->user_data);
	}
}

LmBsListener*
lm_bs_listener_ref (LmBsListener *listener)
{
	g_return_val_if_fail (listener != NULL, NULL);
	
	listener->ref_count++;
	
	return listener;
}

void
lm_bs_listener_unref (LmBsListener *listener)
{
	g_return_if_fail (listener != NULL);

	listener->ref_count--;

	if (listener->ref_count == 0) {
		bs_listener_free (listener);
	}
}
