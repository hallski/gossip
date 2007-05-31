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

#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>

#include "lm-sock.h"
#include "lm-debug.h"
#include "lm-bs-session.h"
#include "lm-bs-transfer.h"
#include "lm-bs-client.h"
#include "lm-bs-sender.h"

#define IO_READ_FLAGS G_IO_IN|G_IO_PRI|G_IO_HUP
#define IO_WRITE_FLAGS G_IO_OUT|G_IO_ERR

struct _LmBsSender {
	LmBsSession   *session;
	LmBsClient    *client;
	LmBsTransfer  *transfer;
	const gchar   *append_data;
	SenderState    state;
	gchar         *jid_used;

	gint           ref_count;
};

static void destroy_sender      (LmBsSender *sender);
static void write_next          (LmBsSender *sender);
static void data_written_cb     (LmBsClient *client, LmBsSender *sender);
static void client_disconnected (LmBsClient *client, LmBsSender *sender);
static void match_sha           (LmBsSender *sender, const gchar* sha);
static void fill_request_buf    (LmBsClient *client,
				 GString **buf,
				 const gchar* msg);
static void data_read_cb        (LmBsClient *client,
				 LmBsSender *sender,
				 GString *data);
static void activate_streamhost (gpointer ptr, LmBsSender *sender);


static void
destroy_sender (LmBsSender *sender)
{
	guint fd;

	fd = lm_bs_client_get_fd (sender->client);
	_lm_bs_session_remove_sender (sender->session, fd);
}


static void
write_next (LmBsSender *sender)
{
	GString      *data;
	LmBsTransfer *transfer;
	gboolean      result;

	transfer = sender->transfer;
	g_return_if_fail (transfer != NULL);

	data = NULL;
	lm_bs_transfer_ref (transfer);
	result = lm_bs_transfer_get_file_content (sender->transfer, &data);
	if (!result) {
		lm_bs_sender_unref (sender);
	}
	else {
		lm_bs_client_write_data (sender->client, data);
	}
	lm_bs_transfer_unref (transfer);
}

static void
data_written_cb (LmBsClient *client, LmBsSender *sender)
{
	switch (sender->state) {
		case SS_AUTH_READ:
			sender->state = SS_AUTH_REPLIED;
			break;
		case SS_CONNECT_REQUEST_READ:
			sender->state = SS_CONNECT_REQUEST_REPLIED;
			lm_bs_client_remove_watch (client);
			return;
		case SS_TRANSFER_STARTED:
			write_next (sender);
			return;
		default:
			break;
	}
	lm_bs_client_do_read (client);
}

static void
client_disconnected (LmBsClient *client, LmBsSender *sender)
{
	TransferStatus  status;
	LmBsTransfer   *transfer;

	transfer = sender->transfer;
	if (transfer != NULL) {
		lm_bs_transfer_close_file (sender->transfer);
		status = lm_bs_transfer_get_status (sender->transfer);
		if (status != T_STATUS_COMPLETED) {
			destroy_sender (sender);
			lm_bs_transfer_error (sender->transfer,
					      _("Transfer stopped"));
			return;
		}
	}
	destroy_sender (sender);
}

static void
match_sha (LmBsSender *sender, const gchar* sha)
{
	guint fd;

	fd = lm_bs_client_get_fd (sender->client);
	_lm_bs_session_match_sha (sender->session, sha, fd);
}

static void 
fill_request_buf (LmBsClient *client,
		  GString **buf,
		  const gchar* msg)
{
	*buf = g_string_new_len ("\x05\x00\x00\x03", 4);
	g_string_append_printf (*buf, "%c%s", strlen (msg), msg);
	g_string_append_len (*buf, "\x00\x00", 2);
}

static void 
data_read_cb (LmBsClient *client, LmBsSender *sender, GString *data)
{
	GString *send_data;
	gchar   *bytes;
	gchar    ver;
	gchar    num_auth;
	gchar    host_type;
	gchar   *host_addr;
	guint    addrlen;

	send_data = NULL;
	bytes = data->str;

	switch (sender->state) {
		case SS_INITIAL:
			if (data->len < 3) {
				/* no auth mechanisms supported */
				return destroy_sender (sender);
			}
			ver = bytes[0];
			num_auth = bytes[1];
			if (ver != '\x05') {
				return destroy_sender (sender);
			}
			if ((gint) num_auth < 1) {
				/* zero auth mechanisms supported */
				return destroy_sender (sender);
			}
			send_data = g_string_new_len ("\x05\x00", 2);
			sender->state = SS_AUTH_READ;
			break;
		case SS_AUTH_REPLIED:
			if (data->len < 5) {
				/* wrong connect request */
				return destroy_sender (sender);
			}
			if (bytes[1] != '\x01') {
				/* request type is not 'connect' */
				return destroy_sender (sender);
			}
			host_type = bytes[3];
			if (host_type == '\x01') {
				if (data->len < 8) {
					return destroy_sender (sender);
				}
				host_addr = g_strdup_printf ("%d.%d.%d.%d", 
							     (guint) bytes[4],
							     (guint) bytes[5],
							     (guint) bytes[6],
							     (guint) bytes[7]);
				addrlen = strlen (host_addr);
			} else if (host_type == '\x03') {
				addrlen = (guint) bytes[4];
				if (data->len < addrlen + 5) {
					return destroy_sender (sender);
				}
				host_addr = bytes + 5;
				host_addr[addrlen] = '\0';
				host_addr = g_strdup (host_addr);
			} else {
				/*  unknown host type */
				return destroy_sender (sender);
			}
			match_sha (sender, host_addr);
			g_free (host_addr);
			/* lm_bs_client_remove_watch (client); */
			sender->state = SS_CONNECT_REQUEST_READ;
			break;
		default:
			g_assert_not_reached ();
	}
	if (send_data) {
		lm_bs_client_write_data (sender->client, send_data);
	}
}

static void
activate_streamhost (gpointer ptr, LmBsSender *sender)
{
	sender->state = SS_TRANSFER_STARTED;
	write_next (sender);
}

LmBsSender *
lm_bs_sender_new (LmBsClient *client, LmBsSession *session)
{
	LmBsSender             *sender;
	LmBsClientReadFunction  read_func;
	LmBsClientFunction      func;
	LmCallback             *read_cb;
	LmCallback             *written_cb;
	LmCallback             *discon_cb;

	_lm_sock_library_init ();
	lm_debug_init ();
	sender = g_new0 (LmBsSender, 1);
	sender->client = client;
	sender->transfer = NULL;
	sender->jid_used = NULL;
	sender->state = SS_INITIAL;
	sender->session = session;

	read_func = (LmBsClientReadFunction) data_read_cb;
	read_cb = _lm_utils_new_callback (read_func, sender, NULL);
	lm_bs_client_set_data_read_cb (client, read_cb);


	func = (LmBsClientFunction) data_written_cb;
	written_cb = _lm_utils_new_callback (func, sender, NULL);
	lm_bs_client_set_data_written_cb (client, written_cb);
	
	func = (LmBsClientFunction) client_disconnected;
	discon_cb = _lm_utils_new_callback (func, sender, NULL);
	lm_bs_client_set_disconnected_cb (client, discon_cb);

	sender->ref_count = 1;
	lm_bs_client_do_read (client);
	return sender;
}

LmBsSender *
lm_bs_sender_ref (LmBsSender *sender)
{
	g_return_val_if_fail (sender != NULL, NULL);
	sender->ref_count++;
	return sender;
}

void
lm_bs_sender_unref (LmBsSender *sender)
{
	g_return_if_fail (sender != NULL);

	sender->ref_count--;
	if (sender->ref_count == 0) {
		lm_bs_client_unref (sender->client);
		g_free (sender->jid_used);
		g_free (sender);
	}
}

void
lm_bs_sender_set_transfer (LmBsSender *sender, LmBsTransfer *transfer)
{
	GString            *send_data;
	gchar              *auth_sha;
	LmBsClientFunction  func;
	LmCallback         *activate_cb;

	sender->transfer = transfer;
	func = (LmBsClientFunction) activate_streamhost;
	activate_cb = _lm_utils_new_callback (func, sender, NULL);
	lm_bs_transfer_set_activate_cb (transfer, activate_cb);
	auth_sha = lm_bs_transfer_get_auth_sha (transfer);
	fill_request_buf (sender->client, &send_data, auth_sha);
	g_free (auth_sha);
	lm_bs_client_write_data (sender->client, send_data);
}
