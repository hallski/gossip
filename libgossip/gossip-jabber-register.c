/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <stdlib.h>

#include <glib/gi18n.h>

#include "gossip-debug.h"
#include "gossip-jid.h"
#include "gossip-jabber-register.h"
#include "gossip-jabber-private.h"
#include "gossip-jabber-ns.h"
#include "gossip-jabber-utils.h"

#define DEBUG_DOMAIN "JabberRegister"

typedef struct {
	LmConnection        *connection;
	LmMessageHandler    *message_handler;

	GossipAccount       *account;
	GossipVCard         *vcard;

	gboolean             cancelled;

	GossipErrorCallback  callback;
	gpointer             user_data;
} RegisterData;

static RegisterData *  jabber_register_data_new        (GossipAccount       *account,
							GossipJabber        *jabber,
							GossipVCard         *vcard,
							GossipErrorCallback  callback,
							gpointer             user_data);
static void            jabber_register_data_free       (RegisterData        *rd);
static void            jabber_register_connected_cb    (LmConnection        *connection,
							gboolean             result,
							RegisterData        *rd);
static void            jabber_register_disconnected_cb (LmConnection        *connection,
							LmDisconnectReason   reason,
							RegisterData        *rd);
static LmHandlerResult jabber_register_message_handler (LmMessageHandler    *handler,
							LmConnection        *conn,
							LmMessage           *m,
							RegisterData        *rd);

static GHashTable *registrations = NULL;

static RegisterData *
jabber_register_data_new (GossipAccount       *account,
			  GossipJabber        *jabber,
			  GossipVCard         *vcard,
			  GossipErrorCallback  callback,
			  gpointer             user_data)
{
	RegisterData *rd;

	if (!registrations) {
		registrations = g_hash_table_new_full (gossip_account_hash,
						       gossip_account_equal,
						       (GDestroyNotify) g_object_unref,
						       (GDestroyNotify) jabber_register_data_free);
	} 
	
	if (g_hash_table_lookup (registrations, account)) {
		g_warning ("Already registering this account");
		return NULL;
	}
	
	rd = g_new0 (RegisterData, 1);

	rd->connection = _gossip_jabber_new_connection (jabber, account);
	if (!rd->connection) {
		g_free (rd);
		return NULL;
	}

	lm_connection_set_disconnect_function (rd->connection,
					       (LmDisconnectFunction) jabber_register_disconnected_cb,
					       rd, NULL);

	rd->account = g_object_ref (account);

	/* Save vcard information so the next time we connect we can
	 * set it, this could be done better */
	rd->vcard = g_object_ref (vcard);

	rd->callback = callback;
	rd->user_data = user_data;

	g_hash_table_insert (registrations, g_object_ref (account), rd);

	return rd;
}

static void
jabber_register_data_free (RegisterData *rd)
{
	if (rd->vcard) {
		g_object_unref (rd->vcard);
	}

	if (rd->message_handler) {
		lm_message_handler_unref (rd->message_handler);
	}

	if (rd->connection) { 
		lm_connection_set_disconnect_function (rd->connection, NULL, NULL, NULL);
		lm_connection_close (rd->connection, NULL);
		lm_connection_unref (rd->connection);
	}

	g_free (rd);
}

static void
jabber_register_connected_cb (LmConnection *connection,
			      gboolean      result,
			      RegisterData *rd)
{
	LmMessage     *m;
	LmMessageNode *node;
	const gchar   *error_message = NULL;
	const gchar   *id;
	const gchar   *server;
	const gchar   *password;
	gchar         *username;
	gboolean       ok = FALSE;

	if (rd->cancelled) {
		g_hash_table_remove (registrations, rd->account);
		return;
	}

	if (result == FALSE) {
		GError *error;

		error_message = _("Connection could not be opened");
		error = gossip_jabber_error_create (GOSSIP_JABBER_NO_CONNECTION,
						    error_message);

		gossip_debug (DEBUG_DOMAIN, "%s", error_message);

		if (rd->callback) {
			(rd->callback) (GOSSIP_RESULT_ERROR_FAILED,
					error,
					rd->user_data);
		}

		g_error_free (error);
		g_hash_table_remove (registrations, rd->account);
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "Connection open!");

	rd->message_handler = lm_message_handler_new ((LmHandleMessageFunction)
						      jabber_register_message_handler,
						      rd,
						      NULL);

	g_object_get (rd->account,
		      "id", &id,
		      "server", &server,
		      "password", &password,
		      NULL);

	m = lm_message_new_with_sub_type (server,
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_SET);

	lm_message_node_add_child (m->node, "query", NULL);
	node = lm_message_node_get_child (m->node, "query");

	lm_message_node_set_attribute (node, "xmlns", XMPP_REGISTER_XMLNS);

	username = gossip_jid_string_get_part_name (id);
	lm_message_node_add_child (node, "username", username);
	g_free (username);

	lm_message_node_add_child (node, "password", password);

	ok = lm_connection_send_with_reply (connection, m,
					    rd->message_handler,
					    NULL);
	lm_message_unref (m);

	if (!ok) {
		GError *error;

		lm_connection_close (connection, NULL);

		error_message = _("Couldn't send message!");
		error = gossip_jabber_error_create (GOSSIP_JABBER_SPECIFIC_ERROR,
						    error_message);

		gossip_debug (DEBUG_DOMAIN, "%s", error_message);

		if (rd->callback) {
			(rd->callback) (GOSSIP_RESULT_ERROR_FAILED,
					error,
					rd->user_data);
		}

		g_error_free (error);
		g_hash_table_remove (registrations, rd->account);
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "Sent registration details");
}

static void
jabber_register_disconnected_cb (LmConnection       *connection,
				 LmDisconnectReason  reason,
				 RegisterData       *rd)
{
	GError            *error = NULL;
	GossipJabberError  error_code;
	const gchar       *error_message;

	/* If we are here, it is because we were disconnected before we should be */
	if (rd->cancelled) {
		g_hash_table_remove (registrations, rd->account);
		return;
	}

	error_code = GOSSIP_JABBER_SPECIFIC_ERROR;
	error_message = gossip_jabber_error_to_string (error_code);
	error = gossip_jabber_error_create (error_code,
					    error_message);

	if (rd->callback) {
		(rd->callback) (GOSSIP_RESULT_ERROR_FAILED,
				error,
				rd->user_data);
	}

	g_clear_error (&error);
	g_hash_table_remove (registrations, rd->account);
}

static LmHandlerResult
jabber_register_message_handler (LmMessageHandler *handler,
				 LmConnection     *connection,
				 LmMessage        *m,
				 RegisterData     *rd)
{
	LmMessageNode *node;
	GossipResult   result = GOSSIP_RESULT_OK;
	GError        *error = NULL;

	if (rd->cancelled) {
		g_hash_table_remove (registrations, rd->account);
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

	node = lm_message_node_get_child (m->node, "error");
	if (node) {
		GossipJabberError  error_code;
		const gchar       *error_code_str;
		const gchar       *error_message;

		result = GOSSIP_RESULT_ERROR_FAILED;
		error_code_str = lm_message_node_get_attribute (node, "code");

		switch (atoi (error_code_str)) {
		case 401: /* Not Authorized */
		case 407: /* Registration Required */
			error_code = GOSSIP_JABBER_UNAUTHORIZED;
			break;

		case 501: /* Not Implemented */
		case 503: /* Service Unavailable */
			error_code = GOSSIP_JABBER_UNAVAILABLE;
			break;

		case 409: /* Conflict */
			error_code = GOSSIP_JABBER_DUPLICATE_USER;
			break;

		case 408: /* Request Timeout */
		case 504: /* Remote Server Timeout */
			error_code = GOSSIP_JABBER_TIMED_OUT;
			break;

		case 302: /* Redirect */
		case 400: /* Bad Request */
		case 402: /* Payment Required */
		case 403: /* Forbidden */
		case 404: /* Not Found */
		case 405: /* Not Allowed */
		case 406: /* Not Acceptable */
		case 500: /* Internal Server Error */
		case 502: /* Remote Server Error */
		case 510: /* Disconnected */
		default:
			error_code = GOSSIP_JABBER_SPECIFIC_ERROR;
			break;
		};

		error_message = gossip_jabber_error_to_string (error_code);
		error = gossip_jabber_error_create (error_code,
						    error_message);

		gossip_debug (DEBUG_DOMAIN, "Registration failed with error:%s->'%s'",
			      error_code_str, error_message);
	} else {
		gossip_debug (DEBUG_DOMAIN, "Registration success");
	}

	if (rd->callback) {
		(rd->callback) (result,
				error,
				rd->user_data);
	}

	g_clear_error (&error);
	g_hash_table_remove (registrations, rd->account);

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

void
gossip_jabber_register_account (GossipJabber        *jabber,
				GossipAccount       *account,
				GossipVCard         *vcard,
				GossipErrorCallback  callback,
				gpointer             user_data)
{
	RegisterData      *rd;
	GossipJabberError  error_code;
	const gchar       *error_message;
	GError            *error;
	gboolean           result;

	g_return_if_fail (GOSSIP_IS_JABBER (jabber));
	g_return_if_fail (callback != NULL);

	gossip_debug (DEBUG_DOMAIN, "Registering with Jabber server...");

	rd = jabber_register_data_new (account, jabber, vcard, callback, user_data);
	if (rd) {
		result = lm_connection_open (rd->connection,
					     (LmResultFunction) jabber_register_connected_cb,
					     rd, NULL, NULL);
		if (result) {
			return;
		}
	}

	error_code = GOSSIP_JABBER_NO_CONNECTION;
	error_message = gossip_jabber_error_to_string (error_code);
	error = gossip_jabber_error_create (error_code, error_message);

	gossip_debug (DEBUG_DOMAIN, "%s", error_message);

	if (callback) {
		(callback) (GOSSIP_RESULT_ERROR_FAILED,
			    error,
			    user_data);
	}

	g_error_free (error);
	g_hash_table_remove (registrations, account);
}

void
gossip_jabber_register_cancel (GossipJabber *jabber)
{
	GossipAccount *account;
	RegisterData  *rd;

	g_return_if_fail (GOSSIP_IS_JABBER (jabber));

	gossip_debug (DEBUG_DOMAIN, "Cancelling registration with Jabber server...");

	account = gossip_jabber_get_account (jabber);
	rd = g_hash_table_lookup (registrations, account);
	
	if (!rd) {
		g_warning ("No registration was found for account: '%s'", 
			   gossip_account_get_name (account));
		return;
	}

	rd->cancelled = TRUE;

	g_hash_table_remove (registrations, account);
}
