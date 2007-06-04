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

#include <loudmouth/lm-error.h>
#include "lm-sock.h"
#include "lm-debug.h"
#include "lm-bs-session.h"
#include "lm-bs-transfer.h"
#include "lm-bs-client.h"
#include "lm-bs-sender.h"

#define IO_READ_FLAGS  G_IO_IN | G_IO_PRI | G_IO_HUP
#define IO_WRITE_FLAGS G_IO_OUT | G_IO_ERR

typedef enum {
	STATE_INITIAL,
	STATE_AUTH_READ,
	STATE_AUTH_REPLIED,
	STATE_CONNECT_REQUEST_READ,
	STATE_CONNECT_REQUEST_REPLIED,
	STATE_TRANSFER_STARTED
} SenderState;

struct _LmBsSender {
	gint          ref_count;

	LmBsSession  *session;
	LmBsClient   *client;
	LmBsTransfer *transfer;
	SenderState   state;

	const gchar  *append_data;

	gchar        *jid_used;
};

static void bs_sender_destroy                (LmBsSender   *sender);
static void bs_sender_write_next             (LmBsSender   *sender);
static void bs_sender_read_cb                (LmBsClient   *client,
					      LmBsSender   *sender,
					      GString      *data);
static void bs_sender_write_cb               (LmBsClient   *client,
					      LmBsSender   *sender);
static void bs_sender_client_disconnected_cb (LmBsClient   *client,
					      LmBsSender   *sender);
static void bs_sender_match_sha              (LmBsSender   *sender,
					      const gchar  *sha);
static void bs_sender_fill_request_buf       (LmBsClient   *client,
					      GString     **buf,
					      const gchar  *msg);
static void bs_sender_activate_streamhost    (gpointer      ptr,
					      LmBsSender   *sender);

static void
bs_sender_destroy (LmBsSender *sender)
{
	guint fd;

	fd = lm_bs_client_get_fd (sender->client);
	_lm_bs_session_remove_sender (sender->session, fd);
}

static void 
bs_sender_read_cb (LmBsClient *client, 
		   LmBsSender *sender, 
		   GString    *data)
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
	case STATE_INITIAL:
		if (data->len < 3) {
			/* no auth mechanisms supported */
			return bs_sender_destroy (sender);
		}

		ver = bytes[0];
		num_auth = bytes[1];
		if (ver != '\x05') {
			return bs_sender_destroy (sender);
		}

		if ((gint) num_auth < 1) {
			/* zero auth mechanisms supported */
			return bs_sender_destroy (sender);
		}

		send_data = g_string_new_len ("\x05\x00", 2);
		sender->state = STATE_AUTH_READ;
		break;

	case STATE_AUTH_REPLIED:
		if (data->len < 5) {
			/* wrong connect request */
			return bs_sender_destroy (sender);
		}

		if (bytes[1] != '\x01') {
			/* request type is not 'connect' */
			return bs_sender_destroy (sender);
		}

		host_type = bytes[3];
		if (host_type == '\x01') {
			if (data->len < 8) {
				return bs_sender_destroy (sender);
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
				return bs_sender_destroy (sender);
			}

			host_addr = bytes + 5;
			host_addr[addrlen] = '\0';
			host_addr = g_strdup (host_addr);
		} else {
			/*  unknown host type */
			return bs_sender_destroy (sender);
		}

		bs_sender_match_sha (sender, host_addr);
		g_free (host_addr);
		/* lm_bs_client_remove_watch (client); */
		sender->state = STATE_CONNECT_REQUEST_READ;
		break;

	default:
		g_assert_not_reached ();
	}

	if (send_data) {
		lm_bs_client_write_data (sender->client, send_data);
	}
}

static void
bs_sender_write_next (LmBsSender *sender)
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
	} else {
		lm_bs_client_write_data (sender->client, data);
	}

	lm_bs_transfer_unref (transfer);
}

static void
bs_sender_write_cb (LmBsClient *client,
		    LmBsSender *sender)
{
	switch (sender->state) {
	case STATE_AUTH_READ:
		sender->state = STATE_AUTH_REPLIED;
		break;

	case STATE_CONNECT_REQUEST_READ:
		sender->state = STATE_CONNECT_REQUEST_REPLIED;
		lm_bs_client_remove_watch (client);
		return;

	case STATE_TRANSFER_STARTED:
		bs_sender_write_next (sender);
		return;

	default:
		break;
	}

	lm_bs_client_do_read (client);
}

static void
bs_sender_client_disconnected_cb (LmBsClient *client,
				  LmBsSender *sender)
{
	LmBsTransferStatus  status;
	LmBsTransfer       *transfer;

	transfer = sender->transfer;

	if (transfer != NULL) {
		lm_bs_transfer_close_file (sender->transfer);
		status = lm_bs_transfer_get_status (sender->transfer);

		if (status != LM_BS_TRANSFER_STATUS_COMPLETED) {
			GError *error;

			bs_sender_destroy (sender);

			error = g_error_new (lm_error_quark (),
					     LM_BS_TRANSFER_ERROR_CLIENT_DISCONNECTED,
					     _("The other party disconnected"));
			lm_bs_transfer_error (sender->transfer, error);
			g_error_free (error);
			return;
		}
	}

	bs_sender_destroy (sender);
}

static void
bs_sender_match_sha (LmBsSender  *sender, 
		     const gchar *sha)
{
	guint fd;

	fd = lm_bs_client_get_fd (sender->client);
	_lm_bs_session_match_sha (sender->session, sha, fd);
}

static void 
bs_sender_fill_request_buf (LmBsClient  *client,
			    GString    **buf,
			    const gchar *msg)
{
	*buf = g_string_new_len ("\x05\x00\x00\x03", 4);

	g_string_append_printf (*buf, "%c%s", (int)strlen (msg), msg);
	g_string_append_len (*buf, "\x00\x00", 2);
}

static void
bs_sender_activate_streamhost (gpointer    ptr,
			       LmBsSender *sender)
{
	sender->state = STATE_TRANSFER_STARTED;
	bs_sender_write_next (sender);
}

LmBsSender *
lm_bs_sender_new (LmBsClient  *client, 
		  LmBsSession *session)
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

	sender->ref_count = 1;
	sender->client = client;
	sender->transfer = NULL;
	sender->jid_used = NULL;
	sender->state = STATE_INITIAL;
	sender->session = session;

	read_func = (LmBsClientReadFunction) bs_sender_read_cb;
	read_cb = _lm_utils_new_callback (read_func, sender, NULL);
	lm_bs_client_set_data_read_cb (client, read_cb);

	func = (LmBsClientFunction) bs_sender_write_cb;
	written_cb = _lm_utils_new_callback (func, sender, NULL);
	lm_bs_client_set_data_written_cb (client, written_cb);
	
	func = (LmBsClientFunction) bs_sender_client_disconnected_cb;
	discon_cb = _lm_utils_new_callback (func, sender, NULL);
	lm_bs_client_set_disconnected_cb (client, discon_cb);

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
lm_bs_sender_set_transfer (LmBsSender   *sender,
			   LmBsTransfer *transfer)
{
	GString            *send_data;
	gchar              *auth_sha;
	LmBsClientFunction  func;
	LmCallback         *activate_cb;

	sender->transfer = transfer;

	func = (LmBsClientFunction) bs_sender_activate_streamhost;
	activate_cb = _lm_utils_new_callback (func, sender, NULL);
	lm_bs_transfer_set_activate_cb (transfer, activate_cb);

	auth_sha = lm_bs_transfer_get_auth_sha (transfer);
	bs_sender_fill_request_buf (sender->client, &send_data, auth_sha);
	g_free (auth_sha);

	lm_bs_client_write_data (sender->client, send_data);
}
