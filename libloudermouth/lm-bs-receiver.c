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
#include <glib.h>
#include <glib/gi18n.h>

#include <loudmouth/lm-error.h>
#include "lm-sock.h"
#include "lm-debug.h"
#include "lm-bs-transfer.h"
#include "lm-bs-client.h"
#include "lm-bs-receiver.h"

#define IO_READ_FLAGS  G_IO_IN | G_IO_PRI | G_IO_HUP
#define IO_WRITE_FLAGS G_IO_OUT | G_IO_ERR

typedef enum {
	STATE_INITIAL,
	STATE_AUTH_SENT,
	STATE_AUTH_REPLIED,
	STATE_CONNECT_REQUEST_SENT,
	STATE_CONNECT_REQUEST_REPLIED,
	STATE_TRANSFER_STARTED
} ReceiverState;

struct _LmBsReceiver {
	gint           ref_count;

	LmBsClient    *client;
	LmBsTransfer  *transfer;

	const gchar   *append_data;
	ReceiverState  state;

	gchar         *jid_used;
};

static void bs_receiver_client_disconnected (LmBsClient    *client,
					     LmBsReceiver  *receiver);
static void bs_receiver_fill_request_buf    (LmBsClient    *client,
					     GString      **buf,
					     const gchar   *msg);
static void bs_receiver_read_cb             (LmBsClient    *client,
					     LmBsReceiver  *receiver,
					     GString       *data);
static void bs_receiver_write_cb            (LmBsClient    *client,
					     LmBsReceiver  *receiver);
static void bs_receiver_transfer_error      (LmBsReceiver  *receiver);

static void
bs_receiver_client_disconnected (LmBsClient   *client, 
				 LmBsReceiver *receiver)
{
	LmBsTransferStatus status;
	
	g_return_if_fail (client != NULL);
	g_return_if_fail (receiver != NULL);
	g_return_if_fail (receiver->transfer != NULL);

	lm_bs_client_unref (client);

	status = lm_bs_transfer_get_status (receiver->transfer);

	if (status != LM_BS_TRANSFER_STATUS_COMPLETED) {
		GError *error;

		error = g_error_new (lm_error_quark (),
				     LM_BS_TRANSFER_ERROR_CLIENT_DISCONNECTED,
				     _("The other party disconnected"));
		lm_bs_transfer_error (receiver->transfer, error);
		g_error_free (error);
	}

	lm_bs_transfer_close_file (receiver->transfer);
}

static void 
bs_receiver_fill_request_buf (LmBsClient   *client, 
			      GString     **buf, 
			      const gchar  *msg)
{
	*buf = g_string_new_len ("\x05\x01\x00\x03", 4);

	g_string_append_printf (*buf, "%c%s", (int)strlen (msg), msg);
	g_string_append_len (*buf, "\x00\x00", 2);
}

static void
bs_receiver_transfer_error (LmBsReceiver *receiver)
{
	GError *error;

	error = g_error_new (lm_error_quark (),
			     LM_BS_TRANSFER_ERROR_PROTOCOL_SPECIFIC,
			     _("A protocol error occurred during the transfer"));
	lm_bs_transfer_error (receiver->transfer, error);
	g_error_free (error);
}

static void 
bs_receiver_read_cb (LmBsClient   *client, 
		     LmBsReceiver *receiver, 
		     GString      *data)
{
	GString      *send_data;
	LmBsTransfer *transfer;
	gchar        *bytes;
	gchar        *auth_sha;
	guint         addrlen;
	gboolean      result;

	send_data = NULL;
	bytes = data->str;

	transfer = receiver->transfer;

	switch (receiver->state) {
	case STATE_AUTH_SENT:
		if (data->len < 2) {
			lm_bs_receiver_unref (receiver);
			bs_receiver_transfer_error (receiver);
			return;
		}
		
		receiver->state = STATE_AUTH_REPLIED;
		
		if (bytes[0] != '\x05' || bytes[1] == '\xff') {
			lm_bs_receiver_unref (receiver);
			bs_receiver_transfer_error (receiver);
			return;
		}
		
		auth_sha = lm_bs_transfer_get_auth_sha (transfer);
		bs_receiver_fill_request_buf (client, &send_data, auth_sha);
		g_free (auth_sha);
		break;
		
	case STATE_CONNECT_REQUEST_SENT:
		if (data->len <= 4 ) {
			lm_bs_receiver_unref (receiver);
			bs_receiver_transfer_error (receiver);
			return;
		}
		
		receiver->state = STATE_TRANSFER_STARTED;
		
		if (bytes[3] == '\x03') {
			gchar* address;
			
			addrlen = (guint) bytes[4];
			address = bytes + 5;
			address[addrlen] = '\0';
			
			if (data->len - addrlen - 6 < 0) {
				lm_bs_receiver_unref (receiver);
				bs_receiver_transfer_error (receiver);
				return;
			}
		}
		
		lm_bs_transfer_send_success_reply (transfer,
						   receiver->jid_used);
		break;
		
	case STATE_TRANSFER_STARTED:
		lm_bs_transfer_ref (transfer);
		result = lm_bs_transfer_append_to_file (transfer, data);
		
		if (!result) {
			lm_bs_receiver_unref (receiver);
		}
		
		lm_bs_transfer_unref (transfer);
		break;
		
	default:
		g_assert_not_reached ();
	}
	
	if (send_data) {
		lm_bs_client_write_data (receiver->client, send_data);
	}
}

static void 
bs_receiver_write_cb (LmBsClient   *client, 
		      LmBsReceiver *receiver)
{
	switch (receiver->state) {
	case STATE_INITIAL:
		receiver->state = STATE_AUTH_SENT;
		break;
	case STATE_AUTH_REPLIED:
		receiver->state = STATE_CONNECT_REQUEST_SENT;
		break;
	default:
		g_assert_not_reached ();
	}

	lm_bs_client_do_read (client);
}

LmBsReceiver *
lm_bs_receiver_new (LmBsClient   *client,
		    LmBsTransfer *transfer,
		    const gchar  *jid_used)
{
	LmBsReceiver           *receiver;
	LmBsClientReadFunction  read_func;
	LmBsClientFunction      func;
	LmCallback             *read_cb;
	LmCallback             *written_cb;
	LmCallback             *discon_cb;

	_lm_sock_library_init ();
	lm_debug_init ();
	receiver = g_new0 (LmBsReceiver, 1);
	receiver->client = client;
	lm_bs_client_ref (client);
	receiver->transfer = transfer;
	receiver->jid_used = g_strdup (jid_used);

	read_func = (LmBsClientReadFunction) bs_receiver_read_cb;
	read_cb = _lm_utils_new_callback (read_func, receiver, NULL);
	lm_bs_client_set_data_read_cb (client, read_cb);

	func = (LmBsClientFunction) bs_receiver_write_cb;
	written_cb = _lm_utils_new_callback (func, receiver, NULL);
	lm_bs_client_set_data_written_cb (client, written_cb);
	
	func = (LmBsClientFunction) bs_receiver_client_disconnected;
	discon_cb = _lm_utils_new_callback (func, receiver, NULL);
	lm_bs_client_set_disconnected_cb (client, discon_cb);

	receiver->ref_count = 1;

	return receiver;
}

void
lm_bs_receiver_unref (LmBsReceiver *receiver)
{
	g_return_if_fail (receiver != NULL);

	receiver->ref_count--;

	if (receiver->ref_count == 0) {
		lm_bs_client_unref (receiver->client);
		g_free (receiver->jid_used);
		g_free (receiver);
	}
}

void
lm_bs_receiver_start_transfer (LmBsReceiver *receiver)
{
	receiver->state = STATE_INITIAL;

	lm_bs_client_write_data (receiver->client,
				 g_string_new_len ("\x05\x01\x00", 3));
}
