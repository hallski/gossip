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

#include <gtk/gtkmain.h>
#include <string.h>

#include "gossip-utils.h"
#include "gossip-marshal.h"
#include "gossip-roster.h"

struct _GossipRosterPriv {
        GHashTable       *items;
	GHashTable       *groups;

	LmConnection     *connection;
        LmMessageHandler *presence_handler;
        LmMessageHandler *iq_handler;

	gint              ref_count;
};
   
struct _GossipRosterGroup {
        gchar *name;
        GList *items; /* List of RosterItems */
};

struct _GossipRosterItem {
        gchar        *jid;
        gchar        *name;
	gboolean      online;
        GossipShow    show;
        gchar        *status;
        gchar        *subscription;
        gchar        *ask;
        GList        *groups;    /* List of groups */
        GList        *resources; /* Should be {Resource, Priority} */
};
   
typedef struct {
	gchar *resource;
	gint   priority;
} ResourcePriority;

/* Signals for item_added, item_removed, ... */

enum {
	ITEM_ADDED,
	ITEM_REMOVED,
	ITEM_UPDATED,
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
static GossipRosterItem * roster_item_new    (const gchar       *jid);

static void     roster_item_update           (GossipRoster      *roster,
					      GossipRosterItem  *item,
					      LmMessageNode     *node);
static gboolean roster_item_update_presence  (GossipRoster      *roster,
					      GossipRosterItem  *item,
					      LmMessage         *presence);
static void     roster_item_remove           (GossipRoster      *roster, 
					      GossipRosterItem  *item);
static void     roster_item_free             (GossipRosterItem  *item);

static void     roster_item_add_group        (GossipRoster      *roster, 
					      GossipRosterItem  *item,
					      const gchar       *name);
static GossipRosterGroup * roster_group_new  (const gchar       *name);
static void     roster_group_free            (GossipRosterGroup *group);

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
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
        
        object_class->finalize = roster_finalize;
        
        /* Create signals */
	signals[ITEM_ADDED] =
		g_signal_new ("item_added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	signals[ITEM_REMOVED] = 
		g_signal_new ("item_removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	signals[ITEM_UPDATED] =
		g_signal_new ("item_updated",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

}

static void
roster_init (GossipRoster *roster)
{
        GossipRosterPriv *priv;

        priv = g_new0 (GossipRosterPriv, 1);
        roster->priv = priv;

        priv->items  = g_hash_table_new_full (g_str_hash, g_str_equal,
					      NULL,
					      (GDestroyNotify) roster_item_free);
	priv->groups = g_hash_table_new_full (g_str_hash, g_str_equal,
					      NULL,
					      (GDestroyNotify) roster_group_free);
        
	priv->connection       = NULL;
        priv->presence_handler = NULL;
        priv->iq_handler       = NULL;

	g_signal_connect (gossip_app_get (), "connected",
			  G_CALLBACK (roster_connected_cb),
			  roster);

	g_signal_connect (gossip_app_get (), "disconnected",
			  G_CALLBACK (roster_disconnected_cb),
			  roster);
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

	priv->connection = lm_connection_ref (gossip_app_get_connection ());

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
		priv->connection = NULL;
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
	gboolean          ret_val = LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

	g_return_val_if_fail (GOSSIP_IS_ROSTER (roster), 
			      LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS);

	priv = roster->priv;

	from = gossip_jid_new (lm_message_node_get_attribute (m->node, "from"));

	item = (GossipRosterItem *) g_hash_table_lookup (priv->items,
							 gossip_jid_get_without_resource (from));

	if (!item) {
		gossip_jid_unref (from);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	} 

	if (roster_item_update_presence (roster, item, m)) {
		ret_val = LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}
	
	gossip_jid_unref (from);
	
	return ret_val;
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
		GossipJID        *jid;
		const gchar      *jid_str;
		const gchar      *subscription;

		if (strcmp (node->name, "item") != 0) { 
			continue;
		}

		jid_str= lm_message_node_get_attribute (node, "jid");
		if (!jid_str) {
			continue;
		}

		jid = gossip_jid_new (jid_str);

		item = (GossipRosterItem *) g_hash_table_lookup (priv->items, 
								 gossip_jid_get_without_resource (jid));
		
		if (item) {
			roster_item_update (roster, item, node);
		}

		subscription = lm_message_node_get_attribute (node, "subscription");
		if (subscription && strcmp (subscription, "remove") == 0) {
			roster_item_remove (roster, item);
			return LM_HANDLER_RESULT_REMOVE_MESSAGE;
		}

		/* It's a new item */
		item = roster_item_new (jid_str);
	
		g_hash_table_insert (priv->items, 
				     g_strdup (gossip_jid_get_without_resource (jid)), 
				     item);
		gossip_jid_unref (jid);
		g_signal_emit (roster, signals[ITEM_ADDED], 0, item);

		roster_item_update (roster, item, node);
	}
	
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static GossipRosterItem *
roster_item_new (const gchar *jid)
{
	GossipRosterItem *item;

	item = g_new0 (GossipRosterItem, 1);
	item->jid = g_strdup (jid);

	/* Set name to be jid to have something to show */
	item->name = g_strdup (jid);
	item->subscription = NULL;
	item->ask = NULL;

	item->groups = NULL;
	item->resources = NULL;

	item->online = FALSE;
	item->show = GOSSIP_SHOW_AVAILABLE;
	item->status = NULL;

	return item;
}

static void
roster_item_update (GossipRoster     *roster,
		    GossipRosterItem *item,
		    LmMessageNode    *node)
{
	const gchar   *subscription;
	const gchar   *ask;
	const gchar   *name;
	LmMessageNode *child;

	/* Update the item, can be name change, group change, etc.. */

	subscription = lm_message_node_get_attribute (node, "subscription");
	if (subscription) {
		g_free (item->subscription);
		item->subscription = g_strdup (subscription);
	}
	
	ask = lm_message_node_get_attribute (node, "ask");
	if (ask) {
		g_free (item->ask);
		item->ask = g_strdup (ask);
	}
		
	name = lm_message_node_get_attribute (node, "name");
	if (name) {
		g_free (item->name);
		name = g_strdup (name);
	}

	/* Go through the list of groups */
	for (child = node->children; child; child = child->next) {
		if (strcmp (child->name, "group") == 0) {
			roster_item_add_group (roster, item, child->value);
		}
	}

	g_signal_emit (roster, signals[ITEM_UPDATED], 0, item);
}

static gboolean
roster_item_update_presence (GossipRoster     *roster,
			     GossipRosterItem *item,
			     LmMessage        *presence)
{
	LmMessageSubType  type;
	LmMessageNode    *node;

	type = lm_message_get_sub_type (presence);

	switch (type) {
	case LM_MESSAGE_SUB_TYPE_AVAILABLE:
		item->online = TRUE;
		break;
	case LM_MESSAGE_SUB_TYPE_UNAVAILABLE: 
		item->online = FALSE;
		break;
	default:
		return FALSE;
	}

	node = lm_message_node_get_child (presence->node, "show");
	if (node) {
		item->show = gossip_show_from_string (node->value);
	} else {
		item->show = GOSSIP_SHOW_AVAILABLE;
	}

	node = lm_message_node_get_child (presence->node, "status");
	if (node) {
		g_free (item->status);
		item->status = g_strdup (node->value);
	}

	/* FIXME: Handle priority/resource-combos */
	node = lm_message_node_get_child (presence->node, "priority");
	if (node) {
	}

	g_signal_emit (roster, signals[ITEM_UPDATED], 0, item);

	return TRUE;
}

static void
roster_item_remove (GossipRoster *roster, GossipRosterItem *item) 
{
	GossipRosterPriv *priv;
	GList            *l;

	priv = roster->priv;

	for (l = item->groups; l; l = l->next) {
		GossipRosterGroup *group = (GossipRosterGroup *) l->data;
		
		group->items = g_list_remove (group->items, item);
		/* FIXME: Should we remove the group if there are no items *
		 *        in it?                                           */
	}
	
	g_signal_emit (roster, signals[ITEM_REMOVED], 0, item);
	
	g_hash_table_remove (priv->items, item->jid);
}

static void
roster_item_free (GossipRosterItem *item)
{
	g_return_if_fail (item->groups != NULL);

	g_free (item->jid);
	g_free (item->name);
	g_free (item->subscription);
	g_free (item->ask);
	g_free (item->status);

	g_free (item);
}

static void
roster_item_add_group (GossipRoster     *roster, 
		       GossipRosterItem *item,
		       const gchar      *name) 
{
	GossipRosterPriv  *priv;
	GossipRosterGroup *group;
	
	priv = roster->priv;

	group = (GossipRosterGroup *) g_hash_table_lookup (priv->groups, name);

	if (!group) {
		group = roster_group_new (name);
		g_hash_table_insert (priv->groups, group->name, group);
	}
	
	if (!g_list_find (group->items, item)) {
		group->items = g_list_prepend (group->items, item);
	}

	if (!g_list_find (item->groups, group)) {
		item->groups = g_list_prepend (item->groups, group);
	}
}

static GossipRosterGroup *
roster_group_new (const gchar *name)
{
	GossipRosterGroup *group;

	group = g_new0 (GossipRosterGroup, 1);

	group->name = g_strdup (name);
	group->items = NULL;

	return group;
}

static void
roster_group_free (GossipRosterGroup *group) 
{
	g_return_if_fail (group->items != NULL);
	
	g_free (group->name);

	g_free (group);
}

GossipRoster *
gossip_roster_new (void)
{
	GossipRoster *roster;

	roster = g_object_new (GOSSIP_TYPE_ROSTER, NULL);

	return roster;
}


