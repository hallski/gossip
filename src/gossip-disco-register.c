/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Martyn Russell (ginxd@btopenworld.com)
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
#include <gtk/gtk.h>
#include <loudmouth/loudmouth.h>
#include "gossip-utils.h"
#include "gossip-app.h"
#include "gossip-log.h"
#include "gossip-disco-register.h"

#define d(x) x


struct _GossipDiscoRegister {
        LmMessageHandler        *message_handler;

	GossipJID               *to;

	GossipDiscoRegisterFunc  func;
	gpointer                 user_data;

        gchar                   *key;

        gchar                   *instructions;

        gchar                   *username;
        gchar                   *password;
        gchar                   *email;
	gchar                   *nickname;

	gboolean                 already_registered;

        gboolean                 require_username;
        gboolean                 require_password;
        gboolean                 require_email;
        gboolean                 require_nickname;
};


static void                 disco_register_init            (void);
static GossipDiscoRegister *disco_register_new             (void);
#if 0
static void                 disco_register_destroy         (GossipDiscoRegister *reg);
#endif
static LmHandlerResult      disco_register_message_handler (LmMessageHandler    *handler,
							    LmConnection        *connection,
							    LmMessage           *m,
							    gpointer             user_data);
static void                 disco_register_requirements    (GossipDiscoRegister *reg);
static void                 disco_register_request         (GossipDiscoRegister *reg);
static void                 disco_register_handle_response (GossipDiscoRegister *reg,
							    LmMessage           *m,
							    gpointer             user_data);



static GHashTable *disco_registers = NULL;


static void
disco_register_init (void)
{
        static gboolean inited = FALSE;

	if (inited) {
		return;
	}

        inited = TRUE;

        disco_registers = g_hash_table_new_full (gossip_jid_hash,
						   gossip_jid_equal,
						   (GDestroyNotify) gossip_jid_unref,
						   (GDestroyNotify) g_object_unref);
}

static GossipDiscoRegister *
disco_register_new (void)
{
	GossipDiscoRegister *reg = NULL;
	LmConnection        *connection;
	LmMessageHandler    *handler;
	
	reg = g_new0 (GossipDiscoRegister, 1);

	connection = gossip_app_get_connection ();

	handler = lm_message_handler_new (disco_register_message_handler, reg, NULL);
	reg->message_handler = handler;
	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_IQ,
						LM_HANDLER_PRIORITY_NORMAL);
	
	return reg;
}

#if 0
static void
disco_register_destroy (GossipDiscoRegister *reg)
{
	LmConnection     *connection;
        LmMessageHandler *handler;

        connection = gossip_app_get_connection ();

        handler = reg->message_handler;
        if (handler) {
                lm_connection_unregister_message_handler (
                                connection, handler, LM_MESSAGE_TYPE_IQ);
                lm_message_handler_unref (handler);
        }

	gossip_jid_unref (reg->to);

        g_free (reg);
}
#endif

GossipDiscoRegister *
gossip_disco_register_requirements (const char                *to, 
				      GossipDiscoRegisterFunc  func,
				      gpointer                 user_data)
{
	GossipDiscoRegister *reg;
	GossipJID           *jid;

	disco_register_init ();

	g_return_val_if_fail (to != NULL && strlen (to) > 0, NULL); 
	g_return_val_if_fail (func != NULL, NULL); 

	jid = gossip_jid_new (to);

	reg = g_hash_table_lookup (disco_registers, jid);

	if (reg) {
		return reg;
	}

	reg = disco_register_new ();
	g_hash_table_insert (disco_registers, gossip_jid_ref (jid), reg);

	reg->to = jid;

	reg->func = func;
	reg->user_data = user_data;

	/* send initial request */
	disco_register_requirements (reg);

	return reg;
}

static void
disco_register_requirements (GossipDiscoRegister *reg)
{
        LmMessage     *m;
	LmMessageNode *node;

	/* create message */
        m = lm_message_new_with_sub_type (gossip_jid_get_full (reg->to),
                                          LM_MESSAGE_TYPE_IQ,
                                          LM_MESSAGE_SUB_TYPE_GET);

	d(g_print ("to: %s (disco register requirements)\n", gossip_jid_get_full (reg->to)));
	
	lm_message_node_add_child (m->node, "query", NULL);
	node = lm_message_node_get_child (m->node, "query");

        lm_message_node_set_attribute (node, "xmlns", "jabber:iq:register");

        lm_connection_send (gossip_app_get_connection (), m, NULL);
        lm_message_unref (m);
}

GossipDiscoRegister *
gossip_disco_register_request (GossipDiscoRegister     *reg, 
			       GossipDiscoRegisterFunc  func,
			       gpointer                 user_data)
{
	g_return_val_if_fail (reg != NULL, NULL); 
	g_return_val_if_fail (func != NULL, NULL); 

	g_return_val_if_fail (reg->key != NULL, NULL);

	if (reg->require_username) {
		g_return_val_if_fail (reg->username != NULL, NULL);
	}

	if (reg->require_password) {
		g_return_val_if_fail (reg->password != NULL, NULL);
	}

	if (reg->require_email) {
		g_return_val_if_fail (reg->email != NULL, NULL);
	}

	if (reg->require_nickname) {
		g_return_val_if_fail (reg->nickname != NULL, NULL);
	}

	reg->func = func;
	reg->user_data = user_data;

	disco_register_request (reg);

	return reg;
}

static void
disco_register_request (GossipDiscoRegister *reg)
{
        LmMessage       *m;
	LmMessageNode   *node;

	/* create message */
        m = lm_message_new_with_sub_type (gossip_jid_get_full (reg->to),
                                          LM_MESSAGE_TYPE_IQ,
                                          LM_MESSAGE_SUB_TYPE_SET);

	d(g_print ("to: %s (disco register requirements)\n", gossip_jid_get_full (reg->to)));
	
	lm_message_node_add_child (m->node, "query", NULL);
	node = lm_message_node_get_child (m->node, "query");

        lm_message_node_set_attribute (node, "xmlns", "jabber:iq:register");
	
	lm_message_node_add_child (node, "key", reg->key);

	if (reg->require_username) {
		lm_message_node_add_child (node, "username", reg->username);
	}

	if (reg->require_password) {
		lm_message_node_add_child (node, "password", reg->password);
	}

	if (reg->require_email) {
		lm_message_node_add_child (node, "email", reg->email);
	}

	if (reg->require_nickname) {
		lm_message_node_add_child (node, "nickname", reg->nickname);
	}

        lm_connection_send (gossip_app_get_connection (), m, NULL);
        lm_message_unref (m);
}

static LmHandlerResult
disco_register_message_handler (LmMessageHandler *handler,
				  LmConnection   *connection,
				  LmMessage      *m,
				  gpointer        user_data)
{
        GossipDiscoRegister *reg = (GossipDiscoRegister*) user_data;
        const gchar         *from;
        GossipJID           *from_jid;
	LmMessageNode       *node; 
	const char          *xmlns;

        from = lm_message_node_get_attribute (m->node, "from");
        from_jid = gossip_jid_new (from);

	if (lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_RESULT && 
	    lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_ERROR) {
		gossip_jid_unref (from_jid);
		
                return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}
	
	node = lm_message_node_get_child (m->node, "query");
        if (node) {
		xmlns = lm_message_node_get_attribute (node, "xmlns");

		if (!xmlns) {
			return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;	
		}

		if (strcmp (xmlns, "jabber:iq:register") == 0) {
			disco_register_handle_response (reg, m, user_data);
		} else {
			return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
		}
        }

        return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
disco_register_handle_response (GossipDiscoRegister *reg,
				LmMessage           *m,
				gpointer             user_data)
{
	LmMessageNode *node;
	const gchar   *error_code = NULL;
	const gchar   *error_reason = NULL;

	node = lm_message_node_find_child (m->node, "instructions");
	if (node) {
		reg->instructions = g_strdup (lm_message_node_get_value (node));
	}

	node = lm_message_node_find_child (m->node, "key");
	if (node) {
		reg->key = g_strdup (lm_message_node_get_value (node));
	}

	node = lm_message_node_find_child (m->node, "registered");
	if (node) {
		reg->already_registered = TRUE;
	}

	node = lm_message_node_find_child (m->node, "username");
	if (node) {
		reg->require_username = TRUE;
		reg->username = g_strdup (lm_message_node_get_value (node));
	}

	node = lm_message_node_find_child (m->node, "password");
	if (node) {
		reg->require_password = TRUE;
		reg->password = g_strdup (lm_message_node_get_value (node));
	}

	node = lm_message_node_find_child (m->node, "email");
	if (node) {
		reg->require_email = TRUE;
		reg->email = g_strdup (lm_message_node_get_value (node));
	}

	node = lm_message_node_find_child (m->node, "nickname");
	if (node) {
		reg->require_nickname = TRUE;
		reg->nickname = g_strdup (lm_message_node_get_value (node));
	}

	/* handle error conditions */
	node = lm_message_node_find_child (m->node, "error");

	if (node) {
		error_code = lm_message_node_get_attribute (node, "code");
		error_reason = lm_message_node_get_value (node);
	}

	d(g_print ("disco already registered:'%s', username:'%s', nickname:'%s'\n",
		   reg->already_registered ? "yes" : "no", reg->username, reg->nickname)); 

 	(reg->func) (reg, error_code, error_reason, reg->user_data);
}

gchar *
gossip_disco_register_get_instructions (GossipDiscoRegister *reg)
{
	g_return_val_if_fail (reg != NULL, NULL);

	return reg->instructions;
}

gboolean
gossip_disco_register_get_require_username (GossipDiscoRegister *reg)
{
	g_return_val_if_fail (reg != NULL, FALSE);

	return reg->require_username;
}

gboolean
gossip_disco_register_get_require_password (GossipDiscoRegister *reg)
{
	g_return_val_if_fail (reg != NULL, FALSE);

	return reg->require_password;
}

gboolean
gossip_disco_register_get_require_email (GossipDiscoRegister *reg)
{
	g_return_val_if_fail (reg != NULL, FALSE);

	return reg->require_email;
}

gboolean
gossip_disco_register_get_require_nickname (GossipDiscoRegister *reg)
{
	g_return_val_if_fail (reg != NULL, FALSE);

	return reg->require_nickname;
}


void                   
gossip_disco_register_set_username (GossipDiscoRegister *reg, 
				    const gchar         *username)
{
	g_return_if_fail (reg != NULL);
	g_return_if_fail (username != NULL);
	
	g_free (reg->username);
	reg->username = g_strdup (username);
}

void                   
gossip_disco_register_set_password (GossipDiscoRegister *reg, 
				    const gchar         *password)
{
	g_return_if_fail (reg != NULL);
	g_return_if_fail (password != NULL);
	
	g_free (reg->password);
	reg->password = g_strdup (password);
}

void                   
gossip_disco_register_set_email (GossipDiscoRegister *reg, 
				 const gchar         *email)
{
	g_return_if_fail (reg != NULL);
	g_return_if_fail (email != NULL);
	
	g_free (reg->email);
	reg->email = g_strdup (email);
}

void                   
gossip_disco_register_set_nickname (GossipDiscoRegister *reg, 
				    const gchar         *nickname)
{
	g_return_if_fail (reg != NULL);
	g_return_if_fail (nickname != NULL);
	
	g_free (reg->nickname);
	reg->nickname = g_strdup (nickname);
}

gboolean
gossip_disco_register_check_registered (GossipJID *jid)
{
	GossipRoster *roster;
	GList        *items;
	GList        *l;

	g_return_val_if_fail (jid != NULL, FALSE);

	roster = gossip_app_get_roster ();

	items = gossip_roster_get_all_items (roster);
	for (l = items; l; l = l->next) {
		GossipRosterItem *roster_item;
		GossipJID        *roster_jid;

		roster_item = (GossipRosterItem*) l->data;
		roster_jid = gossip_roster_item_get_jid (roster_item);
		
		if (!gossip_jid_is_service (roster_jid)) {
			continue;
		}

		if (gossip_jid_equals_without_resource (jid, roster_jid)) {
			return TRUE;
		}
	}

	return FALSE;
}
