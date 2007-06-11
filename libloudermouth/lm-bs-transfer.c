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
#include <loudmouth/lm-error.h>

#include "lm-sha.h"
#include "lm-bs-client.h"
#include "lm-bs-transfer.h"
#include "lm-bs-receiver.h"
#include "lm-bs-private.h"

#include "libloudermouth-marshal.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), LM_TYPE_BS_TRANSFER, LmBsTransferPriv))

#define XMLNS_BYTESTREAMS "http://jabber.org/protocol/bytestreams"

#define FILE_BUFFER_SIZE 1024

typedef struct _LmBsTransferPriv LmBsTransferPriv;

struct _LmBsTransferPriv {
	LmBsTransferDirection  direction;
	LmBsTransferStatus     status;

	LmConnection          *connection;
	LmBsSession           *session;

	GHashTable            *streamhosts;

	gchar                 *peer_jid;
	gchar                 *location;
	guint                  id;
	gchar                 *sid;
	gchar                 *iq_id;

	GIOChannel            *file_channel;
	LmCallback            *activate_cb;

	guint64                bytes_total;
	guint64                bytes_transferred;
};

typedef struct {
	LmBsTransfer *transfer;
	gchar        *jid;
} StreamHostData;

enum {
	COMPLETE,
	PROGRESS,
	ERROR,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void     bs_transfer_finalize               (GObject         *object);
static void     bs_transfer_free_streamhost_data   (gpointer         data);
static void     bs_transfer_client_connected_cb    (LmBsClient      *client,
						    StreamHostData  *sreamhost_data);
static void     bs_transfer_client_disconnected_cb (LmBsClient      *client,
						    StreamHostData  *sreamhost_data);
static gboolean bs_transfer_channel_open_for_write (LmBsTransfer    *transfer,
						    GError         **error);
static gboolean bs_transfer_channel_open_for_read  (LmBsTransfer    *transfer,
						    GError         **error);
static gchar *  bs_transfer_io_error_to_string     (GError          *error);
static void     bs_transfer_complete               (LmBsTransfer    *transfer);
static void     bs_transfer_progress               (LmBsTransfer    *transfer);
static void     bs_transfer_error                  (LmBsTransfer    *transfer,
						    const gchar     *error_msg);
static gchar *  bs_transfer_get_initiator          (LmBsTransfer    *transfer);
static gchar *  bs_transfer_get_target             (LmBsTransfer    *transfer);

G_DEFINE_TYPE (LmBsTransfer, lm_bs_transfer, G_TYPE_OBJECT);

static void
lm_bs_transfer_class_init (LmBsTransferClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = bs_transfer_finalize;
	
	signals[COMPLETE] =
		g_signal_new ("complete",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	signals[PROGRESS] =
		g_signal_new ("progress",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__DOUBLE,
			      G_TYPE_NONE,
			      1, G_TYPE_DOUBLE);
	signals[ERROR] =
		g_signal_new ("error",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	g_type_class_add_private (object_class, sizeof (LmBsTransferPriv));
}

static void
lm_bs_transfer_init (LmBsTransfer *transfer)
{
	LmBsTransferPriv *priv;

	priv = GET_PRIV (transfer);

	priv->streamhosts = g_hash_table_new_full (g_str_hash, 
						   g_str_equal, 
						   g_free, 
						   (GDestroyNotify) lm_bs_client_unref);
}

static void
bs_transfer_finalize (GObject *object)
{
	LmBsTransferPriv *priv;
	
	priv = GET_PRIV (object);

	if (priv->activate_cb) {
		_lm_utils_free_callback (priv->activate_cb);
	}

	lm_bs_transfer_close_file (LM_BS_TRANSFER (object));

	g_free (priv->peer_jid);
	g_free (priv->location);
	g_free (priv->sid);
	g_free (priv->iq_id);

	g_hash_table_destroy (priv->streamhosts);

	if (priv->session) {
		g_object_unref (priv->session);
	}

	if (priv->connection) {
		lm_connection_unref (priv->connection);
	}

	(G_OBJECT_CLASS (lm_bs_transfer_parent_class)->finalize) (object);
}

static void
bs_transfer_free_streamhost_data (gpointer data)
{
	StreamHostData *streamhost_data;

	streamhost_data = data;

	if (streamhost_data->transfer) {
		g_object_unref (streamhost_data->transfer);
	}

	g_free (streamhost_data->jid);
	g_free (streamhost_data);
}

static void
bs_transfer_client_connected_cb (LmBsClient     *client, 
				 StreamHostData *streamhost_data)
{
	LmBsTransfer     *transfer;
	LmBsTransferPriv *priv;
	LmBsReceiver     *receiver;

	transfer = streamhost_data->transfer;

	priv = GET_PRIV (transfer);

	priv->status = LM_BS_TRANSFER_STATUS_CONNECTED;

	receiver = lm_bs_receiver_new (client, transfer, streamhost_data->jid);
	g_hash_table_remove_all (priv->streamhosts);

	lm_bs_receiver_start_transfer (receiver);
}

static void
bs_transfer_client_disconnected_cb (LmBsClient     *client,
				    StreamHostData *streamhost_data)
{
	LmBsTransfer     *transfer;
	LmBsTransferPriv *priv;
	
	transfer = streamhost_data->transfer;

	priv = GET_PRIV (transfer);

	g_hash_table_remove (priv->streamhosts, streamhost_data->jid);

	if (!g_hash_table_size (priv->streamhosts)) {
		bs_transfer_error (transfer, _("Unable to connect to the other party"));
	}
}

static gboolean 
bs_transfer_channel_open_for_write (LmBsTransfer  *transfer,
				    GError       **error)
{
	LmBsTransferPriv *priv;

	priv = GET_PRIV (transfer);

	if (priv->file_channel) {
		return TRUE;
	}

	priv->file_channel = g_io_channel_new_file (priv->location,
						    "w",
						    error);

	if (!priv->file_channel) {
		return FALSE;
	}

	g_io_channel_set_encoding (priv->file_channel, NULL, NULL);
	g_io_channel_set_buffered (priv->file_channel, FALSE);

	return TRUE;
}

static gboolean 
bs_transfer_channel_open_for_read (LmBsTransfer  *transfer,
				   GError       **error)
{
	LmBsTransferPriv *priv;

	priv = GET_PRIV (transfer);

	if (priv->file_channel) {
		return TRUE;
	}

	priv->file_channel = g_io_channel_new_file (priv->location,
						    "r",
						    error);

	if (!priv->file_channel) {
		return FALSE;
	}

	g_io_channel_set_encoding (priv->file_channel, NULL, NULL);
	g_io_channel_set_buffered (priv->file_channel, FALSE);

	return TRUE;
}

static gchar*
bs_transfer_io_error_to_string (GError *error)
{
	g_return_val_if_fail (error != NULL, NULL);

	if (error->domain == G_FILE_ERROR) {
		switch(error->code) {
		case G_FILE_ERROR_EXIST:
		case G_FILE_ERROR_ACCES:
		case G_FILE_ERROR_PERM:
			return _("Permission denied");
		case G_FILE_ERROR_NAMETOOLONG:
			return _("File name is too long");
		case G_FILE_ERROR_NOENT:
			return _("File doesn't exist");
		case G_FILE_ERROR_ISDIR:
			return _("File is a directory");
		case G_FILE_ERROR_ROFS:
			return _("Read only file system");
		case G_FILE_ERROR_TXTBSY:
			return _("File is busy");
		case G_FILE_ERROR_FAULT:
			return _("Bad memory");
		case G_FILE_ERROR_LOOP:
			return _("Too many levels of symbolic links");
		case G_FILE_ERROR_NOSPC:
			return _("No space is available");
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
			return _("File is too large");
		case G_IO_CHANNEL_ERROR_IO:
			return _("Input/output error");
		case G_IO_CHANNEL_ERROR_ISDIR:
			return _("File is a directory");
		case G_IO_CHANNEL_ERROR_NOSPC:
			return _("No space is available");
		case G_IO_CHANNEL_ERROR_NXIO:
			return _("No such device");
		default:
			break; /* unknown error */
		}
	}

	return _("Unknown error");
}

static void
bs_transfer_complete (LmBsTransfer *transfer)
{
	LmBsTransferPriv *priv;

	priv = GET_PRIV (transfer);

	priv->status = LM_BS_TRANSFER_STATUS_COMPLETE;

	lm_bs_transfer_close_file (transfer);

	g_signal_emit (transfer, signals[COMPLETE], 0);
}

static void
bs_transfer_progress (LmBsTransfer *transfer)
{
	LmBsTransferPriv *priv;
	gdouble           progress;

	priv = GET_PRIV (transfer);

	progress = 0.0;

	if (priv->bytes_total > 0) {
		progress = (gdouble) priv->bytes_transferred / priv->bytes_total;
	}

	g_signal_emit (transfer, signals[PROGRESS], 0, progress);
}

static void
bs_transfer_error (LmBsTransfer *transfer,
		   const gchar  *error_msg)
{
	GError *error;

	error = g_error_new (lm_error_quark (),
			     LM_BS_TRANSFER_ERROR_UNABLE_TO_CONNECT,
			     error_msg);
	lm_bs_transfer_error (transfer, error);
	g_error_free (error);
}

static gchar *
bs_transfer_get_initiator (LmBsTransfer *transfer)
{
	LmBsTransferPriv *priv;

	priv = GET_PRIV (transfer);

	if (priv->direction == LM_BS_TRANSFER_DIRECTION_RECEIVER) {
		return g_strdup (priv->peer_jid);
	}

	return lm_connection_get_full_jid (priv->connection);
}

static gchar *
bs_transfer_get_target (LmBsTransfer *transfer)
{
	LmBsTransferPriv *priv;

	priv = GET_PRIV (transfer);

	if (priv->direction == LM_BS_TRANSFER_DIRECTION_RECEIVER) {
		return lm_connection_get_full_jid (priv->connection);
	}
	return g_strdup (priv->peer_jid);
}

LmBsTransfer *
lm_bs_transfer_new (LmBsSession           *session,
		    LmConnection          *connection,
		    LmBsTransferDirection  direction,
		    guint                  id,
		    const gchar           *sid,
		    const gchar           *peer_jid,
		    const gchar           *location,
		    gint64                 bytes_total)
{
	LmBsTransfer     *transfer;
	LmBsTransferPriv *priv;

	g_return_val_if_fail (connection != NULL, NULL);
	g_return_val_if_fail (LM_IS_BS_SESSION (session), NULL);
	g_return_val_if_fail (peer_jid != NULL, NULL);
	g_return_val_if_fail (location != NULL, NULL);
	g_return_val_if_fail (sid != NULL, NULL);

	transfer = g_object_new (LM_TYPE_BS_TRANSFER, NULL);

	priv = GET_PRIV (transfer);

	priv->status = LM_BS_TRANSFER_STATUS_INITIAL;
	priv->direction = direction;

	priv->connection = lm_connection_ref (connection);
	priv->session = g_object_ref (session);

	priv->peer_jid = g_strdup (peer_jid);
	priv->location = g_strdup (location);
	priv->id = id;
	priv->sid = g_strdup (sid);
	priv->iq_id = NULL;

	priv->file_channel = NULL;
	priv->activate_cb = NULL;

	priv->bytes_total = bytes_total;
	priv->bytes_transferred = 0;

	return transfer;
}

void
lm_bs_transfer_error (LmBsTransfer *transfer,
		      GError       *error)
{
	LmBsTransferPriv *priv;

	g_return_if_fail (LM_IS_BS_TRANSFER (transfer));

	priv = GET_PRIV (transfer);

	priv->status = LM_BS_TRANSFER_STATUS_INTERRUPTED;

	lm_bs_transfer_close_file (transfer);

	g_signal_emit (transfer, signals[ERROR], 0, error);
}

gboolean
lm_bs_transfer_append_to_file (LmBsTransfer *transfer, 
			       GString      *data)
{
	LmBsTransferPriv *priv;
	GIOStatus         io_status;
	GError           *error;
	const gchar      *error_msg;
	gsize             bytes_written;

	g_return_val_if_fail (LM_IS_BS_TRANSFER (transfer), FALSE);

	priv = GET_PRIV (transfer);

	g_return_val_if_fail (priv->direction == LM_BS_TRANSFER_DIRECTION_RECEIVER, FALSE);

	error = NULL;

	if (!bs_transfer_channel_open_for_write (transfer, &error)) {
		error_msg = bs_transfer_io_error_to_string (error);
		g_error_free (error);
		bs_transfer_error (transfer, error_msg);
		return FALSE;
	}

	io_status = g_io_channel_write_chars (priv->file_channel, 
					      data->str,
					      data->len,
					      &bytes_written,
					      &error);

	if (io_status != G_IO_STATUS_NORMAL) {
		error_msg = bs_transfer_io_error_to_string (error);
		g_error_free (error);
		bs_transfer_error (transfer, error_msg);
		return FALSE;
	}

	priv->bytes_transferred += bytes_written;

	if (priv->bytes_transferred >= priv->bytes_total) {
		bs_transfer_complete (transfer);
		return FALSE;
	}

	bs_transfer_progress (transfer);

	return TRUE;
}

gboolean
lm_bs_transfer_get_file_content (LmBsTransfer  *transfer, 
				 GString      **data)
{
	LmBsTransferPriv *priv;
	GIOStatus         io_status;
	GError           *error;
	const gchar      *error_msg;
	gchar            *buffer;
	gsize             bytes_read;

	g_return_val_if_fail (LM_IS_BS_TRANSFER (transfer), FALSE);
	
	priv = GET_PRIV (transfer);

	g_return_val_if_fail (priv->direction == LM_BS_TRANSFER_DIRECTION_SENDER, FALSE);

	error = NULL;

	if (priv->bytes_transferred >= priv->bytes_total) {
		bs_transfer_complete (transfer);
		return FALSE;
	}

	if (!bs_transfer_channel_open_for_read (transfer, &error)) {
		error_msg = bs_transfer_io_error_to_string (error);
		g_error_free (error);
		bs_transfer_error (transfer, error_msg);
		return FALSE;
	}

	buffer = g_malloc (FILE_BUFFER_SIZE);
	io_status = g_io_channel_read_chars (priv->file_channel, 
					     buffer,
					     FILE_BUFFER_SIZE,
					     &bytes_read,
					     &error);
	
	if (io_status != G_IO_STATUS_NORMAL) {
		error_msg = bs_transfer_io_error_to_string (error);
		g_error_free (error);
		bs_transfer_error (transfer, error_msg);
		return FALSE;
	}

	*data = g_string_new_len (buffer, bytes_read);
	g_free (buffer);

	priv->bytes_transferred += bytes_read;
	bs_transfer_progress (transfer);

	return TRUE;
}

LmBsTransferStatus
lm_bs_transfer_get_status (LmBsTransfer *transfer)
{
	LmBsTransferPriv *priv;

	g_return_val_if_fail (LM_IS_BS_TRANSFER (transfer), LM_BS_TRANSFER_STATUS_INITIAL);

	priv = GET_PRIV (transfer);

	return priv->status;
}

guint64
lm_bs_transfer_get_bytes_transferred (LmBsTransfer *transfer)
{
	LmBsTransferPriv *priv;

	g_return_val_if_fail (LM_IS_BS_TRANSFER (transfer), 0);

	priv = GET_PRIV (transfer);

	return priv->bytes_transferred;
}

guint64
lm_bs_transfer_get_bytes_total (LmBsTransfer *transfer)
{
	LmBsTransferPriv *priv;

	g_return_val_if_fail (LM_IS_BS_TRANSFER (transfer), 0);

	priv = GET_PRIV (transfer);

	return priv->bytes_total;
}

void
lm_bs_transfer_close_file (LmBsTransfer *transfer)
{
	LmBsTransferPriv *priv;

	g_return_if_fail (LM_IS_BS_TRANSFER (transfer));

	priv = GET_PRIV (transfer);

	if (!priv->file_channel) {
		return;
	}

	g_io_channel_unref (priv->file_channel);
	priv->file_channel = NULL;
}
void 
lm_bs_transfer_set_iq_id (LmBsTransfer *transfer, 
			  const gchar  *iq_id)
{
	LmBsTransferPriv *priv;

	g_return_if_fail (LM_IS_BS_TRANSFER (transfer));

	priv = GET_PRIV (transfer);

	if (priv->iq_id != NULL) {
		/* The id is already set. no need to override it, 
		 * because it is same for all streamhosts
		 */
		return;
	}

	priv->iq_id = g_strdup (iq_id);
}

void
lm_bs_transfer_add_streamhost (LmBsTransfer *transfer,
			       const gchar  *host,
			       guint64       port,
			       const gchar  *jid)
{
	GMainContext       *context;
	LmBsTransferPriv   *priv;
	LmBsClient         *streamhost;
	LmBsClientFunction  func;
	LmCallback         *connected_cb;
	LmCallback         *disconnect_cb;
	StreamHostData     *streamhost_data;

	g_return_if_fail (LM_IS_BS_TRANSFER (transfer));

	priv = GET_PRIV (transfer);

	context = _lm_bs_session_get_context (priv->session);

	if (priv->direction == LM_BS_TRANSFER_DIRECTION_RECEIVER) {
		streamhost_data = g_new0 (StreamHostData, 1);
		streamhost_data->transfer = g_object_ref (transfer);
		streamhost_data->jid = g_strdup (jid);

		func = (LmBsClientFunction) bs_transfer_client_connected_cb;
		connected_cb = _lm_utils_new_callback (func,
						       streamhost_data,
						       NULL);

		func = (LmBsClientFunction) bs_transfer_client_disconnected_cb;
		disconnect_cb = _lm_utils_new_callback (func, 
							streamhost_data,
							(GDestroyNotify) bs_transfer_free_streamhost_data);

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

	g_hash_table_insert (priv->streamhosts,
			     g_strdup (jid),
			     streamhost);

	if (priv->status == LM_BS_TRANSFER_STATUS_INITIAL &&
	    priv->direction == LM_BS_TRANSFER_DIRECTION_RECEIVER) {
		lm_bs_client_connect (streamhost);
	}
}

guint
lm_bs_transfer_get_id (LmBsTransfer *transfer)
{
	LmBsTransferPriv *priv;

	g_return_val_if_fail (LM_IS_BS_TRANSFER (transfer), 0);

	priv = GET_PRIV (transfer);

	return priv->id;
}

const gchar *
lm_bs_transfer_get_sid (LmBsTransfer *transfer)
{
	LmBsTransferPriv *priv;

	g_return_val_if_fail (LM_IS_BS_TRANSFER (transfer), NULL);

	priv = GET_PRIV (transfer);

	return priv->sid;
}

gchar * 
lm_bs_transfer_get_auth_sha (LmBsTransfer *transfer)
{
	LmBsTransferPriv *priv;
	gchar            *concat;
	const gchar      *sha;
	gchar            *target;
	gchar            *initiator;

	g_return_val_if_fail (LM_IS_BS_TRANSFER (transfer), NULL);

	priv = GET_PRIV (transfer);
	
	initiator = bs_transfer_get_initiator (transfer);
	target = bs_transfer_get_target (transfer);
	concat = g_strconcat (priv->sid, initiator, target, NULL);
	sha = lm_sha_hash (concat);

	g_free (initiator);
	g_free (target);
	g_free (concat);

	return g_strdup (sha);
}

LmBsTransferDirection
lm_bs_transfer_get_direction (LmBsTransfer *transfer)
{
	LmBsTransferPriv *priv;

	g_return_val_if_fail (LM_IS_BS_TRANSFER (transfer), LM_BS_TRANSFER_DIRECTION_SENDER);

	priv = GET_PRIV (transfer);

	return priv->direction;
}

const gchar *
lm_bs_transfer_get_iq_id (LmBsTransfer *transfer)
{
	LmBsTransferPriv *priv;

	g_return_val_if_fail (LM_IS_BS_TRANSFER (transfer), NULL);

	priv = GET_PRIV (transfer);

	return priv->iq_id;
}

gboolean
lm_bs_transfer_has_streamhost (LmBsTransfer *transfer, 
			       const gchar  *jid)
{
	LmBsTransferPriv *priv;

	g_return_val_if_fail (LM_IS_BS_TRANSFER (transfer), FALSE);

	priv = GET_PRIV (transfer);

	if (g_hash_table_lookup (priv->streamhosts, jid)) {
		return TRUE;
	}

	return FALSE;
}

void 
lm_bs_transfer_set_activate_cb (LmBsTransfer *transfer,
				LmCallback   *activate_cb)
{
	LmBsTransferPriv *priv;

	g_return_if_fail (LM_IS_BS_TRANSFER (transfer));

	priv = GET_PRIV (transfer);

	if (priv->activate_cb != NULL) {
		_lm_utils_free_callback (priv->activate_cb);
	}

	priv->activate_cb = activate_cb;
}

void
lm_bs_transfer_send_success_reply (LmBsTransfer *transfer, 
				   const gchar  *jid)
{
	LmBsTransferPriv *priv;
	LmMessage        *m;
	LmMessageNode    *node;
	LmMessageNode    *node1;
	GError           *error;
	gchar            *target_jid;

	g_return_if_fail (LM_IS_BS_TRANSFER (transfer));

	priv = GET_PRIV (transfer);

	g_return_if_fail (priv->direction == LM_BS_TRANSFER_DIRECTION_RECEIVER);

	m = lm_message_new_with_sub_type (priv->peer_jid, 
					  LM_MESSAGE_TYPE_IQ, 
					  LM_MESSAGE_SUB_TYPE_RESULT);

	lm_message_node_set_attribute (m->node, "id", priv->iq_id);
	target_jid = bs_transfer_get_target (transfer);
	lm_message_node_set_attribute (m->node,
				       "from",
				       target_jid);

	node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attribute (node, "xmlns", XMLNS_BYTESTREAMS);

	node1 = lm_message_node_add_child (node, "streamhost-used", NULL);
	lm_message_node_set_attribute (node1, "jid", jid);

	error = NULL;

	if (!lm_connection_send (priv->connection, m, &error)) {
		g_printerr ("Failed to send message:'%s'\n", 
			    lm_message_node_to_string (m->node));
	} 

	g_free (target_jid);
	lm_message_unref (m);
}

void 
lm_bs_transfer_activate (LmBsTransfer *transfer,
			 const gchar  *jid)
{
	LmBsTransferPriv *priv;
	LmCallback       *cb;

	g_return_if_fail (LM_IS_BS_TRANSFER (transfer));
	g_return_if_fail (lm_bs_transfer_has_streamhost (transfer, jid));

	priv = GET_PRIV (transfer);

	g_hash_table_remove_all (priv->streamhosts);

	cb = priv->activate_cb;
	if (cb && cb->func) {
		(* ((LmBsClientFunction) cb->func)) (NULL, cb->user_data);
	}
}
