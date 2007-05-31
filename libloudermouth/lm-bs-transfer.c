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

#include <loudmouth/lm-connection.h>

#include "lm-sha.h"
#include "lm-bs-session.h"
#include "lm-bs-client.h"
#include "lm-bs-transfer.h"
#include "lm-bs-receiver.h"

#define XMLNS_BYTESTREAMS "http://jabber.org/protocol/bytestreams"

#define FILE_BUFFER_SIZE 1024

struct _LmBsTransfer{
	TransferType   type;
	gchar         *peer_jid;
	gchar         *location;
	guint64        file_size;
	guint64        bytes_transfered;
	gint           id;
	gchar         *sid;
	gchar         *iq_id;
	TransferStatus status;
	GHashTable    *streamhosts;
	LmConnection  *connection;
	LmBsSession   *session;
	GIOChannel    *file_channel;
	LmCallback    *activate_cb;

	gint           ref_count;
};

typedef struct {
	gchar         *jid;
	LmBsTransfer  *transfer;
} StreamhostData;

static void     free_streamhost_data          (gpointer         data);
static void     client_connnected_cb          (LmBsClient      *client,
					       StreamhostData  *sreamhost_data);
static void     client_disconnnected_cb       (LmBsClient      *client,
					       StreamhostData  *sreamhost_data);
static gboolean file_channel_open_for_writing (LmBsTransfer    *transfer,
					       GError         **error);
static gboolean file_channel_open_for_reading (LmBsTransfer    *transfer,
					       GError         **error);
static gchar *  io_error_to_string            (GError          *error);
static void     transfer_completed            (LmBsTransfer    *transfer);
static void     transfer_free                 (LmBsTransfer    *transfer);
static gchar *  transfer_get_initiator        (LmBsTransfer    *transfer);
static gchar *  transfer_get_target           (LmBsTransfer    *transfer);

static void
free_streamhost_data (gpointer data)
{
	g_free (((StreamhostData *) data)->jid);
	g_free (data);
}

static void
client_connnected_cb (LmBsClient *client, StreamhostData *sreamhost_data)
{
	LmBsTransfer *transfer;
	LmBsReceiver *receiver;

	transfer = sreamhost_data->transfer;
	transfer->status = T_STATUS_CONNECTED;
	receiver = lm_bs_receiver_new (client, transfer, sreamhost_data->jid);
	g_hash_table_remove_all (transfer->streamhosts);
	lm_bs_receiver_start_transfer (receiver);
}

static void
client_disconnnected_cb (LmBsClient *client, StreamhostData *sreamhost_data)
{
	LmBsTransfer *transfer;
	
	transfer = sreamhost_data->transfer;
	g_hash_table_remove (transfer->streamhosts, sreamhost_data->jid);
	if (!g_hash_table_size (transfer->streamhosts)) {
		/* unable to connect to any of the supplied streamhosts */
		_lm_bs_session_transfer_error (transfer->session,
					       transfer->id,
					       _("Unable to connect to any host"));
	}
}

static gboolean 
file_channel_open_for_writing (LmBsTransfer  *transfer,
			       GError       **error)
{
	if (transfer->file_channel) {
		return TRUE;
	}
	transfer->file_channel = g_io_channel_new_file (transfer->location,
							"w",
							error);
	if (!transfer->file_channel) {
		return FALSE;
	}
	g_io_channel_set_encoding (transfer->file_channel, NULL, NULL);
	g_io_channel_set_buffered (transfer->file_channel, FALSE);
	return TRUE;
}

static gboolean 
file_channel_open_for_reading (LmBsTransfer *transfer,
			       GError **error)
{
	if (transfer->file_channel) {
		return TRUE;
	}
	transfer->file_channel = g_io_channel_new_file (transfer->location,
							"r",
							error);
	if (!transfer->file_channel) {
		return FALSE;
	}
	g_io_channel_set_encoding (transfer->file_channel, NULL, NULL);
	g_io_channel_set_buffered (transfer->file_channel, FALSE);
	return TRUE;
}

static gchar*
io_error_to_string (GError *error)
{
	g_return_val_if_fail(error != NULL, NULL);
	if (error->domain == G_FILE_ERROR) {
		switch(error->code) {
			case G_FILE_ERROR_EXIST:
			case G_FILE_ERROR_ACCES:
			case G_FILE_ERROR_PERM:
				return _("Permission denied");
			case G_FILE_ERROR_NAMETOOLONG:
				return _("File name too long");
			case G_FILE_ERROR_NOENT:
				return _("File doesn't exist");
			case G_FILE_ERROR_ISDIR:
				return _("File is a directory");
			case G_FILE_ERROR_ROFS:
				return _("Read only file system");
			case G_FILE_ERROR_TXTBSY:
				return _("File busy");
			case G_FILE_ERROR_FAULT:
				return _("Bad memory");
			case G_FILE_ERROR_LOOP:
				return _("Too many levels of symbolic links");
			case G_FILE_ERROR_NOSPC:
				return _("No space left on device");
			case G_FILE_ERROR_NOMEM:
				return _("Virtual memory exhausted");
			case G_FILE_ERROR_MFILE:
				return _("Too many open files");
			case G_FILE_ERROR_BADF:
			case G_FILE_ERROR_IO:
				return _("Input/output error");
			case G_FILE_ERROR_NODEV:
			case G_FILE_ERROR_NXIO:
				return _("No such device");
			default:
				break; /* unknown error */
		}

	} else if (error->domain == G_IO_CHANNEL_ERROR) {
		switch(error->code) {
			case G_IO_CHANNEL_ERROR_FBIG:
				return _("File too large");
			case G_IO_CHANNEL_ERROR_IO:
				return _("Input/output error");
			case G_IO_CHANNEL_ERROR_ISDIR:
				return _("File is a directory");
			case G_IO_CHANNEL_ERROR_NOSPC:
				return _("No space left on device");
			case G_IO_CHANNEL_ERROR_NXIO:
				return _("No such device");
			default:
				break; /* unknown error */
		}
	}
	return _("Unknown error");
}

static void
transfer_completed (LmBsTransfer *transfer)
{
	LmBsSession *session;
	transfer->status = T_STATUS_COMPLETED;
	session = transfer->session;
	lm_bs_transfer_close_file (transfer);
	_lm_bs_session_transfer_completed (session, transfer->id);
}

static void
transfer_free (LmBsTransfer *transfer)
{
	LmConnection *connection;
	LmBsSession  *session;

	connection = transfer->connection;
	session = transfer->session;

	if (transfer->activate_cb) {
		_lm_utils_free_callback (transfer->activate_cb);
	}
	g_hash_table_destroy (transfer->streamhosts);
	lm_bs_transfer_close_file (transfer);
	g_free (transfer->peer_jid);
	g_free (transfer->location);
	g_free (transfer->sid);
	g_free (transfer->iq_id);
	g_free (transfer);
	lm_bs_session_unref (session);
	lm_connection_unref (connection);
}

static gchar *
transfer_get_initiator (LmBsTransfer *transfer)
{
	if (transfer->type == TRANSFER_TYPE_RECEIVER) {
		return g_strdup (transfer->peer_jid);
	}
	return lm_connection_get_full_jid (transfer->connection);
}

static gchar *
transfer_get_target (LmBsTransfer *transfer)
{
	if (transfer->type == TRANSFER_TYPE_RECEIVER) {
		return lm_connection_get_full_jid (transfer->connection);
	}
	return g_strdup (transfer->peer_jid);
}

LmBsTransfer *
lm_bs_transfer_new (LmBsSession    *session,
		    LmConnection   *connection,
		    TransferType    type,
		    gint            id,
		    const gchar    *sid,
		    const gchar    *peer_jid,
		    const gchar    *location,
		    gint64          file_size)
{
	LmBsTransfer *transfer;

	transfer = g_new0 (LmBsTransfer, 1);
	transfer->peer_jid = g_strdup (peer_jid);
	transfer->connection = connection;
	lm_connection_ref (connection);
	transfer->type = type;
	transfer->file_size = file_size;
	transfer->location = g_strdup (location);
	transfer->sid = g_strdup (sid);
	transfer->id = id;
	transfer->iq_id = NULL;
	transfer->status = T_STATUS_INITIAL;
	transfer->streamhosts = g_hash_table_new_full (g_str_hash, 
						       g_str_equal, 
						       g_free, 
						       (GDestroyNotify) lm_bs_client_unref);
	transfer->session = lm_bs_session_ref (session);
	transfer->activate_cb = NULL;
	transfer->file_channel = NULL;
	transfer->bytes_transfered = 0;

	transfer->ref_count = 1;
	return transfer;
}

void
lm_bs_transfer_error(LmBsTransfer *transfer, const gchar *error_msg)
{
	LmBsSession *session;
	transfer->status = T_STATUS_INTERRUPTED;
	session = transfer->session;
	lm_bs_transfer_close_file(transfer);
	_lm_bs_session_transfer_error(session, transfer->id, error_msg);
}


gboolean
lm_bs_transfer_append_to_file (LmBsTransfer *transfer, GString *data)
{
	gsize        bytes_written;
	GIOStatus    io_status;
	GError      *error;
	const gchar *error_msg;
	
	g_return_val_if_fail (transfer->type == TRANSFER_TYPE_RECEIVER, FALSE);
	error = NULL;
	if (!file_channel_open_for_writing (transfer, &error)) {
		error_msg = io_error_to_string (error);
		g_error_free (error);
		lm_bs_transfer_error (transfer, error_msg);
		return FALSE;
	}
	io_status = g_io_channel_write_chars (transfer->file_channel, 
					      data->str,
					      data->len,
					      &bytes_written,
					      &error);
	if (io_status != G_IO_STATUS_NORMAL) {
		error_msg = io_error_to_string (error);
		g_error_free (error);
		lm_bs_transfer_error (transfer, error_msg);
		return FALSE;
	}
	transfer->bytes_transfered += bytes_written;
	if (transfer->bytes_transfered >= transfer->file_size) {
		transfer_completed (transfer);
		return FALSE;
	}
	return TRUE;
}

gboolean
lm_bs_transfer_get_file_content (LmBsTransfer *transfer, GString **data)
{
	gsize        bytes_read;
	GIOStatus    io_status;
	GError      *error;
	const gchar *error_msg;
	gchar       *buffer;
	
	g_return_val_if_fail (transfer->type == TRANSFER_TYPE_SENDER, FALSE);

	error = NULL;
	if (transfer->bytes_transfered >= transfer->file_size) {
		transfer_completed (transfer);
		return FALSE;
	}
	if (!file_channel_open_for_reading (transfer, &error)) {
		error_msg = io_error_to_string (error);
		g_error_free (error);
		lm_bs_transfer_error (transfer, error_msg);
		return FALSE;
	}
	buffer = g_malloc (FILE_BUFFER_SIZE);
	io_status = g_io_channel_read_chars (transfer->file_channel, 
					      buffer,
					      FILE_BUFFER_SIZE,
					      &bytes_read,
					      &error);
	
	if (io_status != G_IO_STATUS_NORMAL) {
		error_msg = io_error_to_string (error);
		g_error_free (error);
		lm_bs_transfer_error (transfer, error_msg);
		return FALSE;
	}
	*data =  g_string_new_len (buffer, bytes_read);
	g_free (buffer);
	transfer->bytes_transfered += bytes_read;
	return TRUE;
}

TransferStatus
lm_bs_transfer_get_status (LmBsTransfer *transfer)
{
	return transfer->status;
}

void
lm_bs_transfer_close_file (LmBsTransfer *transfer)
{
	if (!transfer->file_channel) {
		return;
	}
	g_io_channel_unref (transfer->file_channel);
	transfer->file_channel = NULL;
}

LmBsTransfer *
lm_bs_transfer_ref (LmBsTransfer *transfer)
{
	g_return_val_if_fail (transfer != NULL, NULL);
	transfer->ref_count++;
	return transfer;
}

void
lm_bs_transfer_unref (LmBsTransfer *transfer)
{
	g_return_if_fail (transfer != NULL);
	transfer->ref_count--;
	
	if (transfer->ref_count == 0) {
		transfer_free (transfer);
	}
}

void 
lm_bs_transfer_set_iq_id (LmBsTransfer *transfer, const gchar *iq_id)
{
	if (transfer->iq_id != NULL) {
		/* id is already set. no need to override it, 
		 * because it is same for all streamhosts */
		return;
	}
	transfer->iq_id = g_strdup (iq_id);
}

void
lm_bs_transfer_add_streamhost (LmBsTransfer *transfer,
			       const gchar  *host,
			       guint64       port,
			       const gchar  *jid)
{
	GMainContext       *context;
	LmBsClient         *streamhost;
	LmBsClientFunction  func;
	LmCallback         *connected_cb;
	LmCallback         *disconnect_cb;
	StreamhostData     *sreamhost_data;

	context = _lm_bs_session_get_context (transfer->session);
	if (transfer->type == TRANSFER_TYPE_RECEIVER) {
		sreamhost_data = g_new0 (StreamhostData, 1);
		sreamhost_data->jid = g_strdup (jid);
		sreamhost_data->transfer = transfer;
		func = (LmBsClientFunction) client_connnected_cb;
		connected_cb = _lm_utils_new_callback (func,
						       sreamhost_data,
						       NULL);
		func = (LmBsClientFunction) client_disconnnected_cb;
		disconnect_cb = _lm_utils_new_callback (func, 
							sreamhost_data,
							(GDestroyNotify) free_streamhost_data);
		streamhost = lm_bs_client_new_with_context (port,
							    host,
							    connected_cb,
							    disconnect_cb,
							    NULL,
							    NULL,
							    context);
	} else {
		streamhost = lm_bs_client_new_with_context (port,
							    NULL,
							    NULL,
							    NULL,
							    NULL,
							    NULL,
							    context);
	}

	g_hash_table_insert (transfer->streamhosts,
			     g_strdup (jid),
			     streamhost);
	if (transfer->status == T_STATUS_INITIAL &&
	    transfer->type == TRANSFER_TYPE_RECEIVER) {
		lm_bs_client_connect (streamhost);
	}
}

const gchar *
lm_bs_transfer_get_sid (LmBsTransfer *transfer)
{
	return transfer->sid;
}

gchar * 
lm_bs_transfer_get_auth_sha (LmBsTransfer *transfer)
{
	gchar       *concat;
	const gchar *sha;
	gchar       *target;
	gchar       *initiator;
	
	initiator = transfer_get_initiator (transfer);
	target = transfer_get_target (transfer);
	concat = g_strconcat (transfer->sid,
			      initiator,
			      target,
			      NULL);
	sha = lm_sha_hash (concat);

	g_free (initiator);
	g_free (target);
	g_free (concat);

	return g_strdup (sha);
}

TransferType
lm_bs_transfer_get_type (LmBsTransfer *transfer)
{
	return transfer->type;
}

const gchar *
lm_bs_transfer_get_iq_id (LmBsTransfer *transfer)
{
	return transfer->iq_id;
}

gboolean
lm_bs_transfer_has_streamhost (LmBsTransfer *transfer, const gchar *jid)
{
	if (g_hash_table_lookup (transfer->streamhosts, jid)) {
		return TRUE;
	}
	return FALSE;
}

void 
lm_bs_transfer_set_activate_cb (LmBsTransfer *transfer,
				LmCallback *activate_cb)
{
	g_return_if_fail (transfer != NULL);
	if (transfer->activate_cb != NULL) {
		_lm_utils_free_callback (transfer->activate_cb);
	}
	transfer->activate_cb = activate_cb;
}

void
lm_bs_transfer_send_success_reply (LmBsTransfer *transfer, const gchar *jid)
{
	LmMessage     *m;
	LmMessageNode *node;
	LmMessageNode *node1;
	GError        *error;
	gchar         *target_jid;

	g_return_if_fail (transfer->type == TRANSFER_TYPE_RECEIVER);

	m = lm_message_new_with_sub_type (transfer->peer_jid, 
					  LM_MESSAGE_TYPE_IQ, 
					  LM_MESSAGE_SUB_TYPE_RESULT);
	lm_message_node_set_attribute (m->node, "id", transfer->iq_id);
	target_jid = transfer_get_target (transfer);
	lm_message_node_set_attribute (m->node,
				       "from",
				       target_jid);

	node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attribute (node, "xmlns", XMLNS_BYTESTREAMS);

	node1 = lm_message_node_add_child (node, "streamhost-used", NULL);
	lm_message_node_set_attribute (node1, "jid", jid);

	error = NULL;
	if (!lm_connection_send (transfer->connection, m, &error)) {
		g_printerr ("Failed to send message:'%s'\n", 
			    lm_message_node_to_string (m->node));
	} 

	g_free (target_jid);
	lm_message_unref (m);
}

void 
lm_bs_transfer_activate (LmBsTransfer *transfer, const gchar *jid)
{
	LmCallback *cb;

	g_return_if_fail (lm_bs_transfer_has_streamhost (transfer, jid));

	g_hash_table_remove_all (transfer->streamhosts);

	cb = transfer->activate_cb;
	if (cb && cb->func) {
		(* ((LmBsClientFunction) cb->func)) (NULL,
						     cb->user_data);
	}
}
