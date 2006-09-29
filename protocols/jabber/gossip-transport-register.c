/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Martyn Russell <mr@gnome.org>
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
 */

#include <config.h>

#include <string.h>
#include <loudmouth/loudmouth.h>
#include <libgossip/gossip-utils.h>

#include "gossip-jabber-private.h"
#include "gossip-transport-register.h"

#define DEBUG_MSG(x)
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n"); */


typedef struct {
	GossipJID *jid;

	GossipTransportUnregisterFunc func;
	gpointer user_data;

	gboolean got_requirements;
} TransportUnregister;


typedef struct {
	GossipJID *jid;

	GossipTransportRequirementsFunc func;
	gpointer user_data;
} TransportRequirements;


typedef struct {
	GossipJID *jid;

	GossipTransportRegisterFunc func;
	gpointer user_data;
} TransportRegister;


static void transport_register_init                (void);


static LmHandlerResult
	    transport_unregister_message_handler   (LmMessageHandler      *handler,
						    LmConnection          *connection,
						    LmMessage             *m,
						    gpointer               user_data);
static LmHandlerResult
	    transport_requirements_message_handler (LmMessageHandler      *handler,
						    LmConnection          *connection,
						    LmMessage             *m,
						    gpointer               user_data);
static LmHandlerResult
	    transport_register_message_handler     (LmMessageHandler      *handler,
						    LmConnection          *connection,
						    LmMessage             *m,
						    gpointer               user_data);


static GHashTable *unregisters = NULL;
static GHashTable *requirements = NULL;
static GHashTable *registers = NULL;


static void
transport_register_init (void)
{
	static gboolean inited = FALSE;

	if (inited) {
		return;
	}

	unregisters = g_hash_table_new_full (gossip_jid_hash,
					     gossip_jid_equal,
					     (GDestroyNotify) gossip_jid_unref,
					     (GDestroyNotify) g_free);

	requirements = g_hash_table_new_full (gossip_jid_hash,
					      gossip_jid_equal,
					      (GDestroyNotify) gossip_jid_unref,
					      (GDestroyNotify) g_free);

	registers = g_hash_table_new_full (gossip_jid_hash,
					   gossip_jid_equal,
					   (GDestroyNotify) gossip_jid_unref,
					   (GDestroyNotify) g_free);

	inited = TRUE;
}


/*
 * unregister
 */
void
gossip_transport_unregister (GossipJabber                  *jabber,
			     GossipJID                     *jid,
			     GossipTransportUnregisterFunc  func,
			     gpointer                       user_data)
{
	TransportUnregister *tu;
	LmConnection        *connection;
	LmMessageHandler    *handler;
	LmMessage           *m;
	LmMessageNode       *node;

	g_return_if_fail (jabber != NULL);
	g_return_if_fail (jid != NULL);
	g_return_if_fail (func != NULL);

	transport_register_init ();

	tu = g_new0 (TransportUnregister, 1);

	tu->jid = gossip_jid_ref (jid);

	tu->func = func;
	tu->user_data = user_data;

	g_hash_table_insert (unregisters, tu->jid, tu);

	DEBUG_MSG (("ProtocolTransport: Disco unregister to: %s (requirements)",
		   gossip_jid_get_full (jid)));

	/* set up handler */
	connection = gossip_jabber_get_connection (jabber);

	handler = lm_message_handler_new (transport_unregister_message_handler,
					  NULL, NULL);

	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_IQ,
						LM_HANDLER_PRIORITY_NORMAL);

	/* send message */
	m = lm_message_new_with_sub_type (gossip_jid_get_full (jid),
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_GET);

	lm_message_node_add_child (m->node, "query", NULL);
	node = lm_message_node_get_child (m->node, "query");

	lm_message_node_set_attribute (node, "xmlns", "jabber:iq:register");

	lm_connection_send (connection, m, NULL);
	lm_message_unref (m);
}

static LmHandlerResult
transport_unregister_message_handler (LmMessageHandler    *handler,
				      LmConnection        *connection,
				      LmMessage           *m,
				      gpointer             user_data)
{
	TransportUnregister *tu;

	LmMessage           *new_message;
	LmMessageNode       *node;

	gboolean             require_username = FALSE;
	gboolean             require_password = FALSE;
	gboolean             require_nick = FALSE;
	gboolean             require_email = FALSE;

	const gchar         *from = NULL;
	GossipJID           *from_jid;

	const gchar         *xmlns;

	const gchar         *key = NULL;
	const gchar         *username = NULL;
	const gchar         *password;
	const gchar         *nick;
	const gchar         *email;

	const gchar         *error_code;
	const gchar         *error_reason;

	if (lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_RESULT &&
	    lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_ERROR) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	from = lm_message_node_get_attribute (m->node, "from");
	if (!from) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	from_jid = gossip_jid_new (from);
	tu = g_hash_table_lookup (unregisters, from_jid);
	gossip_jid_unref (from_jid);

	if (!tu) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	node = lm_message_node_get_child (m->node, "query");
	if (!node) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	xmlns = lm_message_node_get_attribute (node, "xmlns");

	if (!xmlns || strcmp (xmlns, "jabber:iq:register") != 0) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	node = lm_message_node_find_child (m->node, "key");
	if (node) {
		key = lm_message_node_get_value (node);
	}

	node = lm_message_node_find_child (m->node, "username");
	if (node) {
		require_username = TRUE;
		username = lm_message_node_get_value (node);
	}

	node = lm_message_node_find_child (m->node, "password");
	if (node) {
		require_password = TRUE;
		password = lm_message_node_get_value (node);
	}

	node = lm_message_node_find_child (m->node, "email");
	if (node) {
		require_email = TRUE;
		email = lm_message_node_get_value (node);
	}

	node = lm_message_node_find_child (m->node, "nick");
	if (node) {
		require_nick = TRUE;
		nick = lm_message_node_get_value (node);
	}

	/* handle error conditions */
	node = lm_message_node_find_child (m->node, "error");

	error_code = error_reason = NULL;
	if (node) {
		error_code = lm_message_node_get_attribute (node, "code");
		error_reason = lm_message_node_get_value (node);
	}

	/* if error or finished, report back */
	if (error_code || error_reason || tu->got_requirements) {
		(tu->func) (tu->jid, error_code, error_reason, tu->user_data);

		lm_connection_unregister_message_handler (connection,
							  handler,
							  LM_MESSAGE_TYPE_IQ);
		lm_message_handler_unref (handler);

		g_hash_table_remove (unregisters, tu->jid);

		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

	/* set flag so we know if this is the first stage complete of not */
	tu->got_requirements = TRUE;

	/* send actual unregister message */
	new_message = lm_message_new_with_sub_type (gossip_jid_get_full (tu->jid),
						    LM_MESSAGE_TYPE_IQ,
						    LM_MESSAGE_SUB_TYPE_SET);

	DEBUG_MSG (("ProtocolTransport: Disco unregister to: %s (request)", gossip_jid_get_full (tu->jid)));

	lm_message_node_add_child (new_message->node, "query", NULL);
	node = lm_message_node_get_child (new_message->node, "query");

	lm_message_node_set_attribute (node, "xmlns", "jabber:iq:register");

	lm_message_node_add_child (node, "key", key);

	if (require_username) {
		lm_message_node_add_child (node, "username", username);
	}

	lm_message_node_add_child (node, "remove", NULL);

	lm_connection_send (connection, new_message, NULL);
	lm_message_unref (new_message);

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

gboolean
gossip_transport_unregister_cancel (GossipJID *jid)
{
	TransportUnregister *p;

	g_return_val_if_fail (jid != NULL, FALSE);

	p = g_hash_table_lookup (unregisters, jid);
	if (p) {
		return g_hash_table_remove (unregisters, p->jid);
	}

	return FALSE;
}


/*
 * transport requirements
 */
void
gossip_transport_requirements (GossipJabber                    *jabber,
			       GossipJID                       *jid,
			       GossipTransportRequirementsFunc  func,
			       gpointer                         user_data)
{
	TransportRequirements *trq;

	LmConnection          *connection;
	LmMessageHandler      *handler;
	LmMessageNode         *node;
	LmMessage             *m;

	g_return_if_fail (jabber != NULL);
	g_return_if_fail (jid != NULL);
	g_return_if_fail (func != NULL);

	transport_register_init ();

	trq = g_new0 (TransportRequirements, 1);

	trq->jid = gossip_jid_ref (jid);

	trq->func = func;
	trq->user_data = user_data;

	g_hash_table_insert (requirements, trq->jid, trq);

	/* set up handler */
	connection = gossip_jabber_get_connection (jabber);

	handler = lm_message_handler_new (transport_requirements_message_handler,
					  NULL, NULL);

	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_IQ,
						LM_HANDLER_PRIORITY_NORMAL);

	/* send message */
	m = lm_message_new_with_sub_type (gossip_jid_get_full (jid),
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_GET);

	DEBUG_MSG (("ProtocolTransport: Disco register to: %s (requirements)", gossip_jid_get_full (jid)));

	lm_message_node_add_child (m->node, "query", NULL);
	node = lm_message_node_get_child (m->node, "query");

	lm_message_node_set_attribute (node, "xmlns", "jabber:iq:register");

	lm_connection_send (connection, m, NULL);
	lm_message_unref (m);
}

static LmHandlerResult
transport_requirements_message_handler (LmMessageHandler      *handler,
					LmConnection          *connection,
					LmMessage             *m,
					gpointer               user_data)
{
	TransportRequirements *trq;

	LmMessageNode         *node;

	gboolean               require_username = FALSE;
	gboolean               require_password = FALSE;
	gboolean               require_nick = FALSE;
	gboolean               require_email = FALSE;

	gboolean               is_registered = FALSE;

	const gchar           *from = NULL;
	GossipJID             *from_jid;

	const gchar           *xmlns = NULL;

	const gchar           *key = NULL;
	const gchar           *instructions = NULL;
	const gchar           *username = NULL;
	const gchar           *password = NULL;
	const gchar           *nick = NULL;
	const gchar           *email = NULL;

	const gchar           *error_code = NULL;
	const gchar           *error_reason = NULL;

	if (lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_RESULT &&
	    lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_ERROR) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	from = lm_message_node_get_attribute (m->node, "from");
	if (!from) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	from_jid = gossip_jid_new (from);
	trq = g_hash_table_lookup (requirements, from_jid);
	gossip_jid_unref (from_jid);

	if (!trq) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	node = lm_message_node_get_child (m->node, "query");
	if (!node) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	xmlns = lm_message_node_get_attribute (node, "xmlns");
	if (!xmlns) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	if (strcmp (xmlns, "jabber:iq:register") != 0) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	node = lm_message_node_find_child (m->node, "instructions");
	if (node) {
		instructions = lm_message_node_get_value (node);
	}

	node = lm_message_node_find_child (m->node, "key");
	if (node) {
		key = lm_message_node_get_value (node);
	}

	node = lm_message_node_find_child (m->node, "username");
	if (node) {
		require_username = TRUE;
		username = lm_message_node_get_value (node);
	}

	node = lm_message_node_find_child (m->node, "password");
	if (node) {
		require_password = TRUE;
		password = lm_message_node_get_value (node);
	}

	node = lm_message_node_find_child (m->node, "email");
	if (node) {
		require_email = TRUE;
		email = lm_message_node_get_value (node);
	}

	node = lm_message_node_find_child (m->node, "nick");
	if (node) {
		require_nick = TRUE;
		nick = lm_message_node_get_value (node);
	}

	node = lm_message_node_find_child (m->node, "registered");
	if (node) {
		is_registered = TRUE;
	}

	/* handle error conditions */
	node = lm_message_node_find_child (m->node, "error");

	if (node) {
		error_code = lm_message_node_get_attribute (node, "code");
		error_reason = lm_message_node_get_value (node);
	}

	/* if error or finished, report back */
	(trq->func) (trq->jid,
		     key,
		     username,
		     password,
		     nick,
		     email,
		     require_username,
		     require_password,
		     require_nick,
		     require_email,
		     is_registered,
		     error_code,
		     error_reason,
		     trq->user_data);

	lm_connection_unregister_message_handler (connection,
						  handler,
						  LM_MESSAGE_TYPE_IQ);
	lm_message_handler_unref (handler);

	g_hash_table_remove (requirements, trq->jid);

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

gboolean
gossip_transport_requirements_cancel (GossipJID *jid)
{
	TransportRequirements *p;

	g_return_val_if_fail (jid != NULL, FALSE);

	p = g_hash_table_lookup (requirements, jid);
	if (p) {
		return g_hash_table_remove (requirements, p->jid);
	}

	return FALSE;
}

/*
 * transport register
 */
void
gossip_transport_register (GossipJabber                *jabber,
			   GossipJID                   *jid,
			   const gchar                 *key,
			   const gchar                 *username,
			   const gchar                 *password,
			   const gchar                 *nick,
			   const gchar                 *email,
			   GossipTransportRegisterFunc  func,
			   gpointer                     user_data)
{
	TransportRegister *trg;

	LmConnection      *connection;
	LmMessageHandler  *handler;
	LmMessage         *m;
	LmMessageNode     *node;

	g_return_if_fail (jabber != NULL);
	g_return_if_fail (jid != NULL);
	g_return_if_fail (func != NULL);
	g_return_if_fail (key != NULL);
	g_return_if_fail (username != NULL);
	g_return_if_fail (password != NULL);

	transport_register_init ();

	trg = g_new0 (TransportRegister, 1);

	trg->jid = gossip_jid_ref (jid);

	trg->func = func;
	trg->user_data = user_data;

	g_hash_table_insert (registers, trg->jid, trg);

	/* set up handler */
	connection = gossip_jabber_get_connection (jabber);

	handler = lm_message_handler_new (transport_register_message_handler,
					  NULL, NULL);

	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_IQ,
						LM_HANDLER_PRIORITY_NORMAL);

	/* send message */
	m = lm_message_new_with_sub_type (gossip_jid_get_full (jid),
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_SET);

	DEBUG_MSG (("ProtocolTransport: Disco register to: %s (register)", gossip_jid_get_full (jid)));

	lm_message_node_add_child (m->node, "query", NULL);
	node = lm_message_node_get_child (m->node, "query");

	lm_message_node_set_attribute (node, "xmlns", "jabber:iq:register");

	lm_message_node_add_child (node, "key", key);

	if (username) {
		lm_message_node_add_child (node, "username", username);
	}

	if (password) {
		lm_message_node_add_child (node, "password", password);
	}

	if (nick) {
		lm_message_node_add_child (node, "nick", nick);
	}

	if (email) {
		lm_message_node_add_child (node, "email", email);
	}

	lm_connection_send (connection, m, NULL);
	lm_message_unref (m);
}

static LmHandlerResult
transport_register_message_handler (LmMessageHandler  *handler,
				    LmConnection      *connection,
				    LmMessage         *m,
				    gpointer           user_data)
{
	TransportRegister *trg;

	LmMessageNode     *node;

	const gchar       *from = NULL;
	GossipJID         *from_jid;

	const gchar       *error_code = NULL;
	const gchar       *error_reason = NULL;

	if (lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_RESULT &&
	    lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_ERROR) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	from = lm_message_node_get_attribute (m->node, "from");
	if (!from) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	from_jid = gossip_jid_new (from);
	trg = g_hash_table_lookup (registers, from_jid);
	gossip_jid_unref (from_jid);

	if (!trg) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	/* look for error */
	node = lm_message_node_get_child (m->node, "query");
	if (node) {
		const gchar *xmlns;

		xmlns = lm_message_node_get_attribute (node, "xmlns");

		if (xmlns && strcmp (xmlns, "jabber:iq:register") != 0) {
			return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
		}

		node = lm_message_node_find_child (m->node, "error");

		if (node) {
			error_code = lm_message_node_get_attribute (node, "code");
			error_reason = lm_message_node_get_value (node);
		}
	}

	(trg->func) (trg->jid, error_code, error_reason, trg->user_data);

	lm_connection_unregister_message_handler (connection,
						  handler,
						  LM_MESSAGE_TYPE_IQ);
	lm_message_handler_unref (handler);

	g_hash_table_remove (registers, trg->jid);

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

gboolean
gossip_transport_register_cancel (GossipJID *jid)
{
	TransportRegister *p;

	g_return_val_if_fail (jid != NULL, FALSE);

	p = g_hash_table_lookup (registers, jid);
	if (p) {
		return g_hash_table_remove (registers, p->jid);
	}

	return FALSE;
}

