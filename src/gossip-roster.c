/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio HB
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

#include "gossip-roster.h"

struct _GossipRoster {
        GHashTable       *items;
	GHashTable       *groups;

	LmConnect        *connection;
        LmMessageHandler *presence_handler;
        LmMessageHandler *iq_handler;

	gint              ref_count;
};
   
struct _GossipRosterGroup {
        gchar *name;
        GList *items; /* List of RosterItems */

	gint   ref_count;
};

struct _GossipRosterItem {
        gchar        *jid;
        gchar        *name;
        GossipStatus *show;
        gchar        *status;
        gchar        *subscription;
        gchar        *ask;
        GList        *groups;    /* Group names as strings */
        GList        *resources; /* Should be {Resource, Priority} */
};
   
typedef struct {
	gchar *resource;
	gint   priority;
} ResourcePriority;

/* Signals for item_added, item_removed, ... */

enum {
        LAST_SIGNAL
};

GObjectClass *parent_class;
static guint signals[LAST_SIGNAL];

static void     roster_class_init            (GossipRosterClass *klass);
static void     roster_init                  (GossipRoster      *roster);
static void     roster_finalize              (GObject           *object);
static void     roster_connected_cb          (GossipApp         *app,
					      GossipRoster      *roster);
static void     roster_disconnected_cb       (GossipApp         *app,
					      GossipRoster      *roster); 
static void     roster_reset_connection      (GossipRoster      *roster);
static void     roster_clear                 (GossipRoster      *roster);
static LmHandlerResult
roster_presence_handler                      (LmMessageHandler  *handler,
					      LmConnection      *connection,
					      LmMessage         *m,
					      GossipRoster      *roster);
static LmHandlerResult
roster_iq_handler                            (LmMessageHandler  *handler,
					      LmConnection      *connection,
					      LmMessage         *m,
					      GossipRoster      *roster);
static void     roster_update_item_status    (GossipRoster      *roster,
					      GossipRosterItem  *item,
					      LmMessage         *presence);
static GossipRosterItem *
roster_item_new                              (const gchar       *jid,
					      const gchar       *name,
					      const gchar       *subscription,
					      const gchar       *ask);

GType
gossip_roster_get_type (void)
{
        static GType object_type = 0;
         
        if (!object_type) {
                static const GTypeInfo object_info = {
                        sizeof (GossipRosterClass),
                        NULL,           /* base_init */
                        NULL,           /* base_finalize */
                        (GClassInitFunc) roster_class_init,
                        NULL,           /* class_finalize */
                        NULL,           /* class_data */
                        sizeof (GossipRoster),
                        0,              /* n_preallocs */
                        (GInstanceInitFunc) roster_init,
                };
 
                object_type = g_type_register_static (G_TYPE_OBJECT,
                                                      "GossipRoster",
                                                      &object_info,
                                                      0);
        }
 
        return object_type;
}

static void     
roster_class_init (GossipRosterClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS(klass);

        parent_class = G_OBJECT_CLASS(g_type_class_peak_parent(klass));
        
        object_class->finalize = roster_finalize;
        
        /* Create signals */
}

static void
roster_init (GossipRos *roster)
{
        GossipRosterPriv *priv;

        priv = g_new0 (GossipRosterPriv, 1);
        roster->priv = priv;

        priv->items  = g_hash_table_new_full (g_str_hash, g_str_equal,
					      (GDestroyNotify) gossip_jid_unref,
					      (GDestroyNotify) gossip_roster_item_unref);
	priv->groups = g_hash_table_new_full (g_str_hash, g_str_equal,
					      (GDestroyNotify) gossip_jid_unref,
					      (GDestroyNotify) gossip_roster_group_unref);

        priv->connection       = NULL;
        priv->presence_handler = NULL;
        priv->iq_handler       = NULL;
}

static void
roster_finalize (GObject *object)
{
        GossipRoster     *roster;
        GossipRosterPriv *priv;

        roster = GOSSIP_ROSTER(object);
        priv   = roster->priv;

        g_hash_table_destroy (priv->items);
        g_hash_table_destroy (priv->groups);
}

static void
roster_connected_cb (GossipApp *app, GossipRoster *roster)
{
	GossipRosterPriv *priv;
	LmMessage        *m;
	LmMessageNode    *node;

	g_return_if_fail (GOSSIP_IS_APP (app));
	g_return_if_fail (GOSSIP_IS_ROSTER (roster));

	priv = roster->priv;

	priv->connection = lm_connect_ref (gossip_app_get_connection ());

	priv->presence_handler = 
		lm_message_handler_new ((LmHandleMessageFunction) roster_presence_handler,
					roster, NULL);
	
	lm_connection_register_message_handler (priv->connection,
						priv->presence_handler,
						LM_MESSAGE_TYPE_PRESENCE,
						LM_HANDLER_PRIORITY_NORMAL);

	priv->iq_handler = 
		lm_message_handler_new ((LmHandleMessageFunction) roster_iq_handler,
					roster, NULL);

	lm_connection_register_message_handler (priv->connection,
						priv->iq_handler,
						LM_MESSAGE_TYPE_IQ,
						LM_HANDLER_PRIORITY_NORMAL);

	m = lm_message_new_with_sub_type (NULL,
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_GET);
	node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attributes (node,
					"xmlns", "jabber:iq:roster",
					NULL);
	lm_connection_send_with_reply (priv->connection, m,
				       priv->iq_handler, NULL);
	lm_message_unref (m);
}

static void
roster_disconnected_cb (GossipApp *app, GossipRoster *roster)
{
	GossipRosterPriv *priv;

	g_return_if_fail (GOSSIP_IS_APP (app));
	g_return_if_fail (GOSSIP_IS_ROSTER (roster));

	priv = roster->priv;

	roster_reset_connection (roster);
	roster_clear (roster);
}

static void
roster_reset_connection (GossipRoster *roster)
{
	GossipRosterPriv *priv;

	priv = roster->priv;

	if (priv->presence_handler) {
		lm_connection_unregister_message_handler (priv->connection,
							  priv->presence_handler,
							  LM_MESSAGE_TYPE_PRESENCE);
		lm_message_handler_unref (priv->presence_handler);
		priv->presence_handler = NULL;
	}

	if (priv->iq_handler) {
		lm_connection_unregister_message_handler (priv->connection,
							  priv->iq_handler,
							  LM_MESSAGE_TYPE_IQ);
		lm_message_handler_unref (priv->iq_handler);
		priv->iq_handler = NULL;
	}

	if (priv->connection) {
		lm_connection_unref (priv->connection);
		priv->connect = NULL;
	}
}

static void
roster_clear (GossipRoster *roster)
{
	GossipRosterPriv *priv;

	priv = roster->priv;

	if (priv->items) {
		g_hash_table_foreach_remove (priv->items, 
					     (GHRFunc) gtk_true,
					     NULL);
		priv->items = NULL;
	}

	if (priv->groups) {
		g_hash_table_foreach_remove (priv->groups,
					     (GHRFunc) gtk_true,
					     NULL);
		priv->groups = NULL;
	}
}

static LmHandlerResult
roster_presence_handler (LmMessageHandler *handler,
			 LmConnection     *connection,
			 LmMessage        *m,
			 GossipRoster     *roster)
{
	GossipRosterPriv *priv;
	GossipJID        *from;
	GossipRosterItem *item;

	g_return_val_if_fail (GOSSIP_IS_ROSTER (roster), 
			      LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS);

	priv = roster->priv;

	from = gossip_jid_new (lm_message_node_get_attribute (m->node, "from"));

	item = (GossipRosterItem *) g_hash_table_lookup (priv->items,
							 gossip_jid_get_without_resource (from));

	if (!item) {
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	} 

	roster_update_item_status (roster, item, m);

	gossip_jid_unref (jid);
	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static LmHandlerResult
roster_iq_handler (LmMessageHandler *handler,
		   LmConnection     *connection,
		   LmMessage        *m,
		   GossipRoster     *roster)
{
	GossipRosterPriv *priv;
	LmMessageNode    *node;
	const gchar      *xmlns;

	g_return_val_if_fail (GOSSIP_IS_ROSTER (roster), 
			      LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS);

	priv = roster->priv;

	node = lm_message_node_get_child (m->node, "query");
	if (!node) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	xmlns = lm_message_node_get_attribute (node, "xmlns");
	if (!xmlns || strcmp (xmlns, "jabber:iq:roster") != 0) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	for (node = node->children; node; node = node->next) {
		GossipRosterItem *item;
		const gchar      *jid;
		const gchar      *name;
		const gchar      *subscription;
		const gchar      *ask;
		const gchar      *group = NULL;
		LmMessageNode    *child;

		if (strcmp (node->name, "item") != 0) { 
			continue;
		}

		jid = lm_message_node_get_attribute (node, "jid");
		if (!jid) {
			continue;
		}

		item = g_hash_table_lookup (priv->items, jid);
		if (item) {
			/* FIXME: We are updating */
		}

		subscription = lm_message_node_get_attribute (node, 
							      "subscription");
		if (!subscription) {
			subscription = "";
		}
		
		if (strcmp (subscription, "remove") == 0) {
			g_hash_table_remove (priv->items, jid);
			/* FIXME: Don't allow more handlers */
			/* FIXME: Signal removal */
			return LM_HANDLER_RESULT_REMOVE_MESSAGE;
		}

		ask = lm_message_node_get_attribute (node, "ask");
		if (!ask) {
			ask = "";
		}
		
		name = lm_message_node_get_attribute (node, "name");
		if (!name) {
			name = jid;
		}
		
		item = roster_item_new (jid, name, subscription, ask);

		/* Go through the list of groups */
		for (child = node->children; child; child = child->next) {
			/* Add group */
		}

		/* FIXME: Signal added */
		
		/* Do some stuff ... */
	}
		
	/* Add, Remove, Move user and stuff */
	
	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static void
roster_update_item_status (GossipRoster     *roster,
			   GossipRosterItem *item,
			   LmMessage        *presence)
{
	/* Read the fields of the message and set item attributes accordingly */
}

static GossipRosterItem *
roster_item_new (const gchar *jid,
		 const gchar *name,
		 const gchar *subscription,
		 const gchar *ask)
{
	GossipRosterItem *item;

	item = g_new0 (GossipRosterItem, 1);
	item->jid = g_strdup (jid);
	item->name = g_strdup (name);
	item->subscription = g_strdup (subscription);
	item->ask = g_strdup (ask);

	item->groups = NULL;
	item->resources = NULL;

	item->show = GOSSIP_STATUS_OFFLINE;
	item->status = g_strdup ("");

	item->ref_count = 1;

	return item;
}

GossipRoster *
gossip_roster_new (void)
{
	GossipRoster *roster;

	roster = g_object_new (GOSSIP_TYPE_ROSTER, NULL);

	return roster;
}


