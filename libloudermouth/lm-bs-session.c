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

#define PROFILE_FILE_TRANSFER   "http://jabber.org/protocol/si/profile/file-transfer"
#define XMLNS_SI                "http://jabber.org/protocol/si"
#define XMLNS_FEATURE_NEG       "http://jabber.org/protocol/feature-neg"
#define XMLNS_X_DATA            "jabber:x:data"


struct _LmBsSession {
	GMainContext *context;
	GHashTable   *float_clients;
	GHashTable   *float_shas;
	GHashTable   *iq_ids;
	GHashTable   *transfers;
	LmBsListener *listener;

	LmCallback   *complete_cb;
	LmCallback   *progress_cb;
	LmCallback   *failure_cb;
	
	gint         ref_count;
};

static void session_free         (LmBsSession *session);
static void new_client_connected (guint        fd,
				  LmBsSession *session);


static void
session_free (LmBsSession *session)
{
	_lm_utils_free_callback (session->complete_cb);
	_lm_utils_free_callback (session->progress_cb);
	_lm_utils_free_callback (session->failure_cb);

	g_hash_table_destroy (session->float_clients);
	g_hash_table_destroy (session->float_shas);
	g_hash_table_destroy (session->iq_ids);
	g_hash_table_destroy (session->transfers);

	if (session->listener) {
		lm_bs_listener_unref (session->listener);
		session->listener = NULL;
	}
	g_free (session);
}

static void 
new_client_connected (guint fd, LmBsSession *session)
{
	LmBsClient *client;
	LmBsSender *sender;

	client = lm_bs_client_new_from_fd (fd, session->context);
	sender = lm_bs_sender_new (client, session);
	
	g_hash_table_insert (session->float_clients,
			     GUINT_TO_POINTER (fd),
			     sender);
}

void 
_lm_bs_session_remove_sender (LmBsSession *session, guint fd)
{
	g_hash_table_remove (session->float_clients, GUINT_TO_POINTER (fd));
}

void
_lm_bs_session_match_sha (LmBsSession *session, const gchar *sha, guint fd)
{
	gpointer      id_ptr;
	LmBsSender   *sender;
	LmBsTransfer *transfer;

	g_return_if_fail (session != NULL);
	g_return_if_fail (session->float_clients != NULL);

	sender = g_hash_table_lookup (session->float_clients,
				      GUINT_TO_POINTER (fd));
	g_return_if_fail (sender != NULL);

	id_ptr = g_hash_table_lookup (session->float_shas, sha);
	g_hash_table_remove (session->float_shas, sha);
	if (!id_ptr) {
		_lm_bs_session_remove_sender (session, fd);
		return;
	}

	transfer = g_hash_table_lookup (session->transfers, id_ptr);
	g_return_if_fail (transfer != NULL);
	lm_bs_sender_set_transfer (lm_bs_sender_ref (sender), transfer);
	_lm_bs_session_remove_sender (session, fd);
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
	if (session->listener) {
		return lm_bs_listener_get_port (session->listener);
	}
	session->listener = lm_bs_listener_new_with_context (session->context);
	lm_bs_listener_set_new_client_function (session->listener,
						(LmBsNewClientFunction) new_client_connected,
						session,
						NULL);
	return lm_bs_listener_start (session->listener);
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
lm_bs_session_new (GMainContext *context) {
	LmBsSession *session = NULL;

	GDestroyNotify destroy_transfer;
	GDestroyNotify destroy_sender;
	
	session = g_new0 (LmBsSession, 1);
	destroy_transfer = (GDestroyNotify) lm_bs_transfer_unref;
	destroy_sender = (GDestroyNotify) lm_bs_sender_unref;
	session->float_clients = g_hash_table_new_full (g_direct_hash,
						        g_direct_equal,
						        NULL,
						        destroy_sender);
	session->transfers = g_hash_table_new_full (g_direct_hash, 
						    g_direct_equal, 
						    NULL, 
						    destroy_transfer);
	session->float_shas = g_hash_table_new_full (g_str_hash, 
						     g_str_equal,
						     g_free,
						     NULL);
	session->iq_ids = g_hash_table_new_full (g_str_hash, 
						 g_str_equal,
						 g_free,
						 NULL);

	session->context = context;
	if (context) {
		g_main_context_ref (context);
	}

	session->listener = NULL;
	session->ref_count = 1;

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

	/* one instance for all accounts/transfers */
	if (session) {
		return lm_bs_session_ref (session);
	}

	session = lm_bs_session_new (context);
	session->ref_count = 2;

	return session;
}

/**
 * lm_bs_session_set_failure_function:
 * @session: the bitestream session
 * @function: the function that will be called on transfer failure
 * @user_data: user supplied pointer. It will be passed to function
 * @notify: function that will be executed on callback destroy, 
 * could be NULL
 * 
 * Registers a function to be called when certain file transfer
 * has failed within a bitestream session.
 *
 **/
void
lm_bs_session_set_failure_function (LmBsSession         *session,
				    LmBsFailureFunction  function,
				    gpointer             user_data,
				    GDestroyNotify       notify)
{
	LmCallback *failure_cb;

	g_return_if_fail (session != NULL);
	failure_cb = _lm_utils_new_callback (function, 
					     user_data,
					     notify);
	if (session->failure_cb != NULL) {
		_lm_utils_free_callback (session->failure_cb);
	}
	session->failure_cb = failure_cb;
}

/**
 * lm_bs_session_set_complete_function:
 * @session: the bitestream session
 * @function: the function that will be called when transfer is 
 * complete
 * @user_data: user supplied pointer. It will be passed to function
 * @notify: function that will be executed on callback destroy, 
 * could be NULL
 *
 * Registers a function to be called when certain file transfer
 * completes within a bitestream session.
 *
 **/
void
lm_bs_session_set_complete_function (LmBsSession          *session,
				     LmBsCompleteFunction  function,
				     gpointer              user_data,
				     GDestroyNotify        notify)
{
	LmCallback *complete_cb;

	g_return_if_fail (session != NULL);
	complete_cb = _lm_utils_new_callback (function, 
					      user_data,
					      notify);
	if (session->complete_cb != NULL) {
		_lm_utils_free_callback (session->complete_cb);
	}
	session->complete_cb = complete_cb;
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
	LmBsTransfer *transfer;

	g_return_if_fail (sid != NULL);
	g_return_if_fail (session->transfers != NULL);
	g_return_if_fail (location != NULL);
	g_return_if_fail (sender != NULL);
	g_return_if_fail (connection != NULL);

	transfer = lm_bs_transfer_new (session,
				       connection,
				       TRANSFER_TYPE_RECEIVER,
				       id,
				       sid,
				       sender,
				       location,
				       file_size);
	
	g_hash_table_insert (session->transfers,
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
	LmBsTransfer *transfer;
	gchar        *auth_sha;

	g_return_if_fail (sid != NULL);
	g_return_if_fail (session->transfers != NULL);
	g_return_if_fail (location != NULL);
	g_return_if_fail (receiver != NULL);
	g_return_if_fail (connection != NULL);

	transfer = lm_bs_transfer_new (session,
				       connection,
				       TRANSFER_TYPE_SENDER,
				       id,
				       sid,
				       receiver,
				       location,
				       file_size);
	auth_sha = lm_bs_transfer_get_auth_sha (transfer);
	g_hash_table_insert (session->transfers,
			     GUINT_TO_POINTER (id),
			     transfer);
	g_hash_table_insert (session->float_shas,
			     auth_sha,
			     GUINT_TO_POINTER (id));
}

/**
 * lm_bs_session_add_streamhost:
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
lm_bs_session_add_streamhost (LmBsSession *session,
			      guint        id,
			      const gchar *host,
			      const gchar *port,
			      const gchar *jid)
{
	guint64       u_port;
	LmBsTransfer *transfer;

	g_return_if_fail (session->transfers != NULL);

	transfer = g_hash_table_lookup (session->transfers,
					GUINT_TO_POINTER (id));
	g_return_if_fail (transfer != NULL);

	if (lm_bs_transfer_has_streamhost (transfer, jid)) {
		/* some clients send more than one streamhost with same jids */
		return;
	}
	u_port = g_ascii_strtoull (port, NULL, 10);
	lm_bs_transfer_add_streamhost (transfer, host, u_port, jid);
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
lm_bs_session_set_iq_id (LmBsSession *session, guint id, const gchar *iq_id)
{
	LmBsTransfer *transfer;

	g_return_if_fail (iq_id != NULL);
	g_return_if_fail (session->transfers != NULL);

	transfer = g_hash_table_lookup (session->transfers,
					GUINT_TO_POINTER (id));
	g_return_if_fail (transfer != NULL);
	lm_bs_transfer_set_iq_id (transfer, iq_id);
	if (lm_bs_transfer_get_type (transfer) == TRANSFER_TYPE_SENDER) {
		g_hash_table_insert (session->iq_ids, 
				     g_strdup (iq_id),
				     GUINT_TO_POINTER (id));
	}
}

/**
 * lm_bs_session_activate_streamhost:
 * @session:
 * @iq_id: id of the query iq stanza.
 * @jid: jid of the streamhost.
 * 
 * Tells the bytestream session that recepient has connected on a 
 * streamhost identified by jid.
 *
 **/
void
lm_bs_session_activate_streamhost (LmBsSession *session,
				   const gchar *iq_id,
				   const gchar *jid)
{
	gpointer      id_ptr;
	LmBsTransfer *transfer;

	g_return_if_fail (session != NULL);
	g_return_if_fail (session->iq_ids != NULL);
	g_return_if_fail (session->transfers != NULL);

	id_ptr = g_hash_table_lookup (session->iq_ids, iq_id);
	
	if (!id_ptr) {
		return;
	}
	transfer = g_hash_table_lookup (session->transfers, id_ptr);
	if (!transfer) {
		g_hash_table_remove (session->iq_ids, iq_id);
		return;
	}
	lm_bs_transfer_activate (transfer, jid);
}

/**
 * lm_bs_session_ref:
 * @session: bytestream session to add a reference to.
 * 
 * Add a reference to @session.
 * 
 * Return value: Returns the same session.
 **/
LmBsSession *
lm_bs_session_ref (LmBsSession *session)
{
	g_return_val_if_fail (session != NULL, NULL);

	session->ref_count++;
	return session;
}

/**
 * lm_bs_session_unref:
 * @session: #LmBsSession to remove reference from.
 * 
 * Removes a reference from @session. When there are no references to
 * @session it will be freed.
 **/
void
lm_bs_session_unref (LmBsSession *session)
{
	g_return_if_fail (session != NULL);

	session->ref_count--;
	if (session->ref_count == 0) {
		session_free (session);
	}
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
lm_bs_session_remove_transfer (LmBsSession *session, guint id)
{
	const gchar  *iq_id;
	LmBsTransfer *transfer;

	g_return_if_fail (session != NULL);
	transfer = g_hash_table_lookup (session->transfers,
					GUINT_TO_POINTER (id));
	g_return_if_fail (transfer != NULL);
	iq_id = lm_bs_transfer_get_iq_id (transfer);
	if (iq_id) {
		g_hash_table_remove (session->iq_ids, iq_id);
	}
	lm_bs_session_ref (session);
	g_hash_table_remove (session->transfers, GUINT_TO_POINTER (id));
	if (g_hash_table_size (session->transfers) == 0 &&
	    session->listener != NULL) {
		lm_bs_listener_unref (session->listener);
		session->listener = NULL;
	}
	lm_bs_session_unref (session);
}

void
_lm_bs_session_transfer_error (LmBsSession *session, guint id, const gchar *msg)
{
	LmCallback *cb;

	g_return_if_fail (session != NULL);

	cb = session->failure_cb;
	if (cb && cb->func) {
		(* ((LmBsFailureFunction) cb->func)) (cb->user_data,
						      id,
						      msg);
	}
	lm_bs_session_remove_transfer (session, id);
}

void
_lm_bs_session_transfer_completed (LmBsSession *session, guint id)
{
	LmCallback *cb;

	g_return_if_fail (session != NULL);

	cb = session->complete_cb;
	if (cb && cb->func) {
		(* ((LmBsCompleteFunction) cb->func)) (cb->user_data,
						       id);
	}
	lm_bs_session_remove_transfer (session, id);
}

GMainContext *
_lm_bs_session_get_context (LmBsSession *session)
{
	return session->context;
}

