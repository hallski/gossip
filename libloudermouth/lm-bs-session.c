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

#include <loudmouth/lm-connection.h>
#include <loudmouth/lm-utils.h>

#include "lm-debug.h"
#include "lm-internals.h"
#include "lm-parser.h"

#include "lm-bs-listener.h"
#include "lm-bs-client.h"
#include "lm-bs-transfer.h"
#include "lm-bs-receiver.h"
#include "lm-bs-sender.h"
#include "lm-bs-session.h"
#include "lm-bs-private.h"

#include "libloudermouth-marshal.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), LM_TYPE_BS_SESSION, LmBsSessionPriv))

#define PROFILE_FILE_TRANSFER "http://jabber.org/protocol/si/profile/file-transfer"
#define XMLNS_SI              "http://jabber.org/protocol/si"
#define XMLNS_FEATURE_NEG     "http://jabber.org/protocol/feature-neg"
#define XMLNS_X_DATA          "jabber:x:data"

typedef struct _LmBsSessionPriv LmBsSessionPriv;

struct _LmBsSessionPriv {
	GMainContext *context;

	GHashTable   *float_clients;
	GHashTable   *float_shas;
	GHashTable   *iq_ids;
	GHashTable   *transfers;

	LmBsListener *listener;

	LmCallback   *progress_cb;
	LmCallback   *failure_cb;
};

enum {
	TRANSFER_COMPLETE,
	TRANSFER_PROGRESS,
	TRANSFER_ERROR,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void bs_session_finalize                (GObject      *object);
static void bs_session_new_client_connected_cb (guint         fd,
						LmBsSession  *session);
static void bs_session_transfer_complete_cb    (LmBsTransfer *transfer,
						LmBsSession  *session);
static void bs_session_transfer_progress_cb    (LmBsTransfer *transfer,
						gdouble       progress,
						LmBsSession  *session);
static void bs_session_transfer_error_cb       (LmBsTransfer *transfer,
						GError       *error,
						LmBsSession  *session);

G_DEFINE_TYPE (LmBsSession, lm_bs_session, G_TYPE_OBJECT);

static void
lm_bs_session_class_init (LmBsSessionClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = bs_session_finalize;

	signals[TRANSFER_COMPLETE] =
		g_signal_new ("transfer-complete",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE,
			      1, G_TYPE_UINT);
	signals[TRANSFER_PROGRESS] =
		g_signal_new ("transfer-progress",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libloudermouth_marshal_VOID__UINT_DOUBLE,
			      G_TYPE_NONE,
			      2, G_TYPE_UINT, G_TYPE_DOUBLE);
	signals[TRANSFER_ERROR] =
		g_signal_new ("transfer-error",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libloudermouth_marshal_VOID__UINT_POINTER,
			      G_TYPE_NONE,
			      2, G_TYPE_UINT, G_TYPE_POINTER);
	
	g_type_class_add_private (object_class, sizeof (LmBsSessionPriv));
}

static void
lm_bs_session_init (LmBsSession *session)
{
	LmBsSessionPriv *priv;

	priv = GET_PRIV (session);

	priv->float_clients = g_hash_table_new_full (g_direct_hash,
						     g_direct_equal,
						     NULL,
						     (GDestroyNotify) lm_bs_sender_unref);
	priv->transfers = g_hash_table_new_full (g_direct_hash, 
						 g_direct_equal, 
						 NULL, 
						 (GDestroyNotify) g_object_unref);
	priv->float_shas = g_hash_table_new_full (g_str_hash, 
						  g_str_equal,
						  g_free,
						  NULL);
	priv->iq_ids = g_hash_table_new_full (g_str_hash, 
					      g_str_equal,
					      g_free,
					      NULL);
}

static void
bs_session_finalize (GObject *object)
{
	LmBsSessionPriv *priv;
	
	priv = GET_PRIV (object);

	_lm_utils_free_callback (priv->progress_cb);
	_lm_utils_free_callback (priv->failure_cb);

	g_hash_table_destroy (priv->float_clients);
	g_hash_table_destroy (priv->float_shas);
	g_hash_table_destroy (priv->iq_ids);
	g_hash_table_destroy (priv->transfers);

	if (priv->listener) {
		lm_bs_listener_unref (priv->listener);
		priv->listener = NULL;
	}

	(G_OBJECT_CLASS (lm_bs_session_parent_class)->finalize) (object);
}

static void 
bs_session_new_client_connected_cb (guint        fd, 
				    LmBsSession *session)
{
	LmBsSessionPriv *priv;
	LmBsClient      *client;
	LmBsSender      *sender;

	priv = GET_PRIV (session);

	client = lm_bs_client_new_from_fd (fd, priv->context);
	sender = lm_bs_sender_new (client, session);
	
	g_hash_table_insert (priv->float_clients,
			     GUINT_TO_POINTER (fd),
			     sender);
}

static void
bs_session_transfer_complete_cb (LmBsTransfer *transfer,
				 LmBsSession  *session)
{
	guint id;

	g_return_if_fail (LM_IS_BS_SESSION (session));

	id = lm_bs_transfer_get_id (transfer);
	lm_verbose ("[%d] File transfer complete\n", id);

	g_signal_emit (session, signals[TRANSFER_COMPLETE], 0, id);

	lm_bs_session_remove_transfer (session, id);
}

static void
bs_session_transfer_progress_cb (LmBsTransfer *transfer,
				 gdouble       progress,
				 LmBsSession  *session)
{
	guint id;

	g_return_if_fail (LM_IS_BS_SESSION (session));

	id = lm_bs_transfer_get_id (transfer);
	lm_verbose ("[%d] File transfer progress: %f %%\n", id, progress * 100);

	g_signal_emit (session, signals[TRANSFER_PROGRESS], 0, id, progress);
}

static void
bs_session_transfer_error_cb (LmBsTransfer *transfer,
			      GError       *error,
			      LmBsSession  *session)
{
	guint id;

	g_return_if_fail (LM_IS_BS_SESSION (session));

	id = lm_bs_transfer_get_id (transfer);
	lm_verbose ("[%d] File transfer error, %s\n", 
		    id, 
		    error ? error->message : "no error given");

	g_signal_emit (session, signals[TRANSFER_ERROR], 0, id, error);

	lm_bs_session_remove_transfer (session, id);
}

void 
_lm_bs_session_remove_sender (LmBsSession *session,
			      guint        fd)
{
	LmBsSessionPriv *priv;

	priv = GET_PRIV (session);

	g_hash_table_remove (priv->float_clients, GUINT_TO_POINTER (fd));
}

void
_lm_bs_session_match_sha (LmBsSession *session, 
			  const gchar *sha, 
			  guint        fd)
{
	LmBsSessionPriv *priv;
	LmBsSender      *sender;
	LmBsTransfer    *transfer;
	gpointer         id_ptr;

	g_return_if_fail (LM_IS_BS_SESSION (session));

	priv = GET_PRIV (session);

	if (priv->float_clients == NULL) {
		g_warning ("Float clients hash table was NULL");
		return;
	}

	sender = g_hash_table_lookup (priv->float_clients,
				      GUINT_TO_POINTER (fd));

	if (sender == NULL) {
		g_warning ("Could not find sender by fd:%d", fd);
		return;
	}

	id_ptr = g_hash_table_lookup (priv->float_shas, sha);
	g_hash_table_remove (priv->float_shas, sha);

	if (!id_ptr) {
		_lm_bs_session_remove_sender (session, fd);
		return;
	}

	transfer = g_hash_table_lookup (priv->transfers, id_ptr);

	if (transfer == NULL) {
		g_warning ("Could not find transfer by id:%p", id_ptr);
		return;
	}

	lm_bs_sender_set_transfer (lm_bs_sender_ref (sender), transfer);
	_lm_bs_session_remove_sender (session, fd);
}

GMainContext *
_lm_bs_session_get_context (LmBsSession *session)
{
	LmBsSessionPriv *priv;
	
	priv = GET_PRIV (session);

	return priv->context;
}

/**
 * lm_bs_session_new:
 * @context: main context to be used for io events , could be NULL
 * 
 * Creates a new instance of #LmBsSession.
 *
 * Return value: the newly created instance
 **/
LmBsSession *
lm_bs_session_new (GMainContext *context) 
{
	LmBsSession     *session;
	LmBsSessionPriv *priv;

	session = g_object_new (LM_TYPE_BS_SESSION, NULL);

	priv = GET_PRIV (session);

	if (context) {
		priv->context = g_main_context_ref (context);
	}

	return session;
}

/**
 * lm_bs_session_get_default:
 * @context: main context to be used for io events , could be NULL
 * 
 * Creates a new instance of #LmBsSession, or returns a reference to
 * the existing one.
 *
 * Return value: reference to default bytestream session object.
 **/
LmBsSession *
lm_bs_session_get_default (GMainContext *context)
{
	static LmBsSession *session = NULL;

	/* One instance for all accounts/transfers */
	if (session) {
		return g_object_ref (session);
	}

	session = lm_bs_session_new (context);

	return g_object_ref (session);
}

/**
 * lm_bs_session_receive_file:
 * @session:
 * @connection: an active jabber connection, used for streamhost 
 * activation reply.
 * @id: id of the file transfer within this session.
 * @sid: session id of the file transfer, used by the jabber session.
 * @sender: full jid (user@server/resource) of the sender
 * @location: place to store the file. This argument should contain a
 * local file path, not URI
 * @file_size: size of the file. When @file_size bytes are being 
 * received, it is assumed that file has been fully retrieved. 
 * 
 * Add info on a file transfer request. In order the transfer to start
 * you need to set up at least one streamhost. 
 **/
void
lm_bs_session_receive_file (LmBsSession  *session, 
			    LmConnection *connection,
			    guint         id,
			    const gchar  *sid,
			    const gchar  *sender,
			    const gchar  *location,
			    guint64       file_size)
{
	LmBsSessionPriv *priv;
	LmBsTransfer    *transfer;

	g_return_if_fail (sid != NULL);
	g_return_if_fail (location != NULL);
	g_return_if_fail (sender != NULL);
	g_return_if_fail (connection != NULL);

 	lm_verbose ("[%d] Attempting to receive file...\n",
		    id);

	priv = GET_PRIV (session);
	
	if (priv->transfers == NULL) {
		g_warning ("Transfers hash table is NULL");
		return;
	}

	transfer = lm_bs_transfer_new (session,
				       connection,
				       LM_BS_TRANSFER_DIRECTION_RECEIVER,
				       id,
				       sid,
				       sender,
				       location,
				       file_size);

	g_signal_connect (transfer, "complete",
			  G_CALLBACK (bs_session_transfer_complete_cb),
			  session);
	g_signal_connect (transfer, "progress",
			  G_CALLBACK (bs_session_transfer_progress_cb),
			  session);
	g_signal_connect (transfer, "error",
			  G_CALLBACK (bs_session_transfer_error_cb),
			  session);
	
	g_hash_table_insert (priv->transfers,
			     GUINT_TO_POINTER (id),
			     transfer);
}

/**
 * lm_bs_session_send_file:
 * @session:
 * @connection: an active jabber connection.
 * @id: id of the file transfer within this session.
 * @sid: session id of the file transfer, used by the jabber session.
 * @receiver: full jid (user@server/resource) of the recipient
 * @location: place to get the file from. This argument should contain
 * a local file path, not URI
 * @file_size: size of the file. When @file_size bytes are being 
 * sent, it is assumed that file has been fully transfered.
 * 
 * Add info on a file to be sent. In order the transfer to start
 * you need to activate a streamhost.
 **/
void
lm_bs_session_send_file (LmBsSession  *session, 
			 LmConnection *connection,
			 guint         id,
			 const gchar  *sid,
			 const gchar  *receiver,
			 const gchar  *location,
			 guint64       file_size)
{
	LmBsSessionPriv *priv;
	LmBsTransfer    *transfer;
	gchar           *auth_sha;

	g_return_if_fail (sid != NULL);
	g_return_if_fail (location != NULL);
	g_return_if_fail (receiver != NULL);
	g_return_if_fail (connection != NULL);

 	lm_verbose ("[%d] Attempting to send file...\n",
		    id);

	priv = GET_PRIV (session);
	
	if (priv->transfers == NULL) {
		g_warning ("Transfers hash table is NULL");
		return;
	}

	transfer = lm_bs_transfer_new (session,
				       connection,
				       LM_BS_TRANSFER_DIRECTION_SENDER,
				       id,
				       sid,
				       receiver,
				       location,
				       file_size);

	g_signal_connect (transfer, "complete",
			  G_CALLBACK (bs_session_transfer_complete_cb),
			  session);
	g_signal_connect (transfer, "progress",
			  G_CALLBACK (bs_session_transfer_progress_cb),
			  session);
	g_signal_connect (transfer, "error",
			  G_CALLBACK (bs_session_transfer_error_cb),
			  session);

	auth_sha = lm_bs_transfer_get_auth_sha (transfer);

	g_hash_table_insert (priv->transfers,
			     GUINT_TO_POINTER (id),
			     transfer);
	g_hash_table_insert (priv->float_shas,
			     auth_sha,
			     GUINT_TO_POINTER (id));
}

/**
 * lm_bs_session_set_iq_id:
 * @session:
 * @id: id of the file transfer.
 * @iq_id: id of the query iq stanza.
 * 
 * Registers the id of stanzas, used for streamhost negotiation.
 *
 **/
void
lm_bs_session_set_iq_id (LmBsSession *session, 
			 guint        id,
			 const gchar *iq_id)
{
	LmBsSessionPriv *priv;
	LmBsTransfer    *transfer;

	g_return_if_fail (LM_IS_BS_SESSION (session));
	g_return_if_fail (iq_id != NULL);

	priv = GET_PRIV (session);
	
	if (priv->transfers == NULL) {
		g_warning ("Transfers hash table is NULL");
		return;
	}

	transfer = g_hash_table_lookup (priv->transfers,
					GUINT_TO_POINTER (id));
	if (priv->transfers == NULL) {
		g_warning ("Could not find transfer from id:%d", id);
		return;
	}

	lm_bs_transfer_set_iq_id (transfer, iq_id);

	if (lm_bs_transfer_get_direction (transfer) == LM_BS_TRANSFER_DIRECTION_SENDER) {
		g_hash_table_insert (priv->iq_ids, 
				     g_strdup (iq_id),
				     GUINT_TO_POINTER (id));
	}
}

/**
 * lm_bs_session_streamhost_add:
 * @session:
 * @id: id of the file transfer.
 * @host: hostname or ip of the streamhost
 * @port: port to connect on
 * @jid: unique id of the streamhost
 * 
 * Add a streamhost to be used for the transfer. File transfer is 
 * identified by @sid . Loudmouth will try to establish connection
 * to one of the attached streamhost and return its @jid back to the
 * sender.
 **/
void
lm_bs_session_streamhost_add (LmBsSession *session,
			      guint        id,
			      const gchar *host,
			      const gchar *port,
			      const gchar *jid)
{
	LmBsSessionPriv *priv;
	LmBsTransfer    *transfer;
	guint64          u_port;

	g_return_if_fail (LM_IS_BS_SESSION (session));
	g_return_if_fail (host != NULL);
	g_return_if_fail (port != NULL);
	g_return_if_fail (jid != NULL);

 	lm_verbose ("[%d] Adding file transfer stream host:'%s', port:'%s', JID:'%s'\n",
		    id,
		    host,
		    port,
		    jid);

	priv = GET_PRIV (session);

	if (priv->transfers == NULL) {
		g_warning ("Transfers hash table is NULL");
		return;
	}

	transfer = g_hash_table_lookup (priv->transfers,
					GUINT_TO_POINTER (id));

	if (priv->transfers == NULL) {
		g_warning ("Could not find transfer from id:%d", id);
		return;
	}

	if (lm_bs_transfer_has_streamhost (transfer, jid)) {
		/* some clients send more than one streamhost with same jids */
		return;
	}

	u_port = g_ascii_strtoull (port, NULL, 10);
	lm_bs_transfer_add_streamhost (transfer, host, u_port, jid);
}

/**
 * lm_bs_session_streamhost_activate:
 * @session:
 * @iq_id: id of the query iq stanza.
 * @jid: jid of the streamhost.
 * 
 * Tells the bytestream session that recepient has connected on a 
 * streamhost identified by jid.
 *
 **/
void
lm_bs_session_streamhost_activate (LmBsSession *session,
				   const gchar *iq_id,
				   const gchar *jid)
{
	LmBsSessionPriv *priv;
	LmBsTransfer    *transfer;
	gpointer         id_ptr;

	g_return_if_fail (LM_IS_BS_SESSION (session));
	g_return_if_fail (iq_id != NULL);
	g_return_if_fail (jid != NULL);

 	lm_verbose ("[%s] Activating file transfer stream host with JID:'%s'\n",
		    iq_id,
		    jid);

	priv = GET_PRIV (session);

	if (priv->iq_ids == NULL) {
		g_warning ("IQ ID hash table is NULL");
		return;
	}

	if (priv->transfers == NULL) {
		g_warning ("Transfers hash table is NULL");
		return;
	}

	id_ptr = g_hash_table_lookup (priv->iq_ids, iq_id);
	if (!id_ptr) {
		return;
	}

	transfer = g_hash_table_lookup (priv->transfers, id_ptr);
	if (!transfer) {
		g_hash_table_remove (priv->iq_ids, iq_id);
		return;
	}

	lm_bs_transfer_activate (transfer, jid);
}

/**
 * lm_bs_session_remove_transfer:
 * @session: #LmBsSession to remove the transfer from.
 * @id: id of the transfer.
 * 
 * Removes a previously added transfer.
 *
 **/
void
lm_bs_session_remove_transfer (LmBsSession *session, 
			       guint        id)
{
	LmBsSessionPriv *priv;
	LmBsTransfer    *transfer;
	const gchar     *iq_id;

	g_return_if_fail (LM_IS_BS_SESSION (session));

 	lm_verbose ("[%d] Cleaning up file transfer\n",
		    id);

	priv = GET_PRIV (session);

	transfer = g_hash_table_lookup (priv->transfers,
					GUINT_TO_POINTER (id));

	if (transfer == NULL) {
		g_warning ("Could not find transfer from id:%d", id);
		return;
	}

	iq_id = lm_bs_transfer_get_iq_id (transfer);
	if (iq_id) {
		g_hash_table_remove (priv->iq_ids, iq_id);
	}

	g_object_ref (session);
	g_hash_table_remove (priv->transfers, GUINT_TO_POINTER (id));

	if (g_hash_table_size (priv->transfers) == 0 &&
	    priv->listener != NULL) {
		lm_bs_listener_unref (priv->listener);
		priv->listener = NULL;
	}

	g_object_unref (session);
}

/**
 * lm_bs_session_start_listener:
 * @session: the bytestream session object
 * 
 * Starts listening on a server socket. When there are no more
 * active transfers the socket will be closed automatically.
 *
 * Return value: the port of the server socket
 **/
guint
lm_bs_session_start_listener (LmBsSession *session)
{
	LmBsSessionPriv *priv;

	g_return_val_if_fail (LM_IS_BS_SESSION (session), -1);

 	lm_verbose ("Starting file transfer listener\n");

	priv = GET_PRIV (session);

	if (priv->listener) {
		return lm_bs_listener_get_port (priv->listener);
	}

	priv->listener = lm_bs_listener_new_with_context (priv->context);

	lm_bs_listener_set_new_client_function (priv->listener,
						(LmBsNewClientFunction)
						bs_session_new_client_connected_cb,
						session,
						NULL);

	return lm_bs_listener_start (priv->listener);
}

