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

#include <stdio.h>
#include <config.h>

#include <gtk/gtkmain.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libgnome/gnome-i18n.h>

#include "gossip-app.h"
#include "gossip-utils.h"
#include "gossip-marshal.h"
#include "gossip-roster.h"

#define d(x)

#define UNSORTED_GROUP_N N_("Unsorted")
#define UNSORTED_GROUP _(UNSORTED_GROUP_N)

struct _GossipRosterPriv {
        GHashTable       *items;
	GHashTable       *groups;

	LmConnection     *connection;
        LmMessageHandler *presence_handler;
        LmMessageHandler *iq_handler;
};
   
struct _GossipRosterGroup {
	GossipRoster *roster;
        gchar        *name;
        GList        *items; /* List of RosterItems */

	gint          ref_count;
};

struct _GossipRosterItem {
	GossipJID    *jid;
        gchar        *name;
	gboolean      online;
        gchar        *subscription;
        gchar        *ask;
        GList        *groups;      /* List of groups */
        GList        *connections; /* List of RosterConnection */

	gint          ref_count;
};
   
typedef struct {
	GossipJID  *jid;
	gint        priority;
	gchar      *status;
	GossipShow  show;
	time_t      last_updated; /* Timestamp to tell when last updated */
} RosterConnection;

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
/* Item handling */
static GossipRosterItem * roster_item_new    (GossipJID         *jid);

static void     roster_item_update           (GossipRoster      *roster,
					      GossipRosterItem  *item,
					      LmMessageNode     *node);
static gboolean roster_item_update_presence  (GossipRoster      *roster,
					      GossipRosterItem  *item,
					      GossipJID         *from,
					      LmMessage         *presence);
static void     roster_item_remove           (GossipRoster      *roster, 
					      GossipRosterItem  *item);
static void     roster_item_free             (GossipRosterItem  *item);

static void     roster_item_add_group        (GossipRoster      *roster, 
					      GossipRosterItem  *item,
					      const gchar       *name);
static void     roster_item_remove_group     (GossipRoster      *roster, 
					      GossipRosterItem  *item,
					      GossipRosterGroup *group);

static RosterConnection *
roster_item_add_connection                   (GossipRosterItem  *item,
					      GossipJID         *from);
static void    roster_item_remove_connection (GossipRosterItem  *item,
					      RosterConnection  *connection);
static gint    roster_item_sort_connections  (gconstpointer      a,
					      gconstpointer      b);
static void    roster_connection_free        (RosterConnection   *connection);

/* Group handling */
static GossipRosterGroup * roster_group_new  (const gchar       *name);
static void     roster_group_remove          (GossipRoster      *roster,
					      GossipRosterGroup *group);

static void     roster_group_free            (GossipRosterGroup *group);
static gboolean roster_group_is_internal     (GossipRosterGroup *group);
 
/* Signals for item_added, item_removed, ... */
enum {
	ITEM_ADDED,
	ITEM_REMOVED,
	ITEM_UPDATED,
	GROUP_ADDED,
	GROUP_REMOVED,
	GROUP_ITEM_REMOVED,
	GROUP_ITEM_ADDED,
        LAST_SIGNAL
};

GObjectClass *parent_class;
static guint signals[LAST_SIGNAL];

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
	signals[GROUP_ADDED] =
		g_signal_new ("group_added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	signals[GROUP_REMOVED] =
		g_signal_new ("group_removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	signals[GROUP_ITEM_ADDED] =
		g_signal_new ("group_item_added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE,
			      2, G_TYPE_POINTER, G_TYPE_POINTER);
	signals[GROUP_ITEM_REMOVED] =
		g_signal_new ("group_item_removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE,
			      2, G_TYPE_POINTER, G_TYPE_POINTER);
}

static void
roster_init (GossipRoster *roster)
{
        GossipRosterPriv *priv;

        priv = g_new0 (GossipRosterPriv, 1);
        roster->priv = priv;

        priv->items  = g_hash_table_new_full (gossip_jid_hash, 
					      gossip_jid_equal,
					      (GDestroyNotify) gossip_jid_unref,
					      (GDestroyNotify) gossip_roster_item_unref);
	priv->groups = g_hash_table_new_full (g_str_hash, g_str_equal,
					      NULL,
					      (GDestroyNotify) gossip_roster_group_unref);
        
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

static gboolean
roster_clear_foreach_item (gpointer          key, 
			   GossipRosterItem *item,
			   GossipRoster     *roster)
{
	GList *l;

	for (l = item->groups; l; l = l->next) {
		GossipRosterGroup *group = (GossipRosterGroup *) l->data;

		g_signal_emit (roster, signals[GROUP_ITEM_REMOVED], 0,
			       group, item);
	}

	return TRUE;
}

static gboolean 
roster_clear_foreach_group (gpointer           key,
			    GossipRosterGroup *group,
			    GossipRoster      *roster)
{
	g_signal_emit (roster, signals[GROUP_REMOVED], 0, group);
	return TRUE;
}

static void
roster_clear (GossipRoster *roster)
{
	GossipRosterPriv *priv;

	priv = roster->priv;

	/* Go through items and signal group_item_removed ... */
	if (priv->items) {
		g_hash_table_foreach_remove (priv->items,
				      (GHRFunc) roster_clear_foreach_item,
				      roster);
	}

	/* Signal group removed */
	if (priv->groups) {
		g_hash_table_foreach_remove (priv->groups,
				      (GHRFunc) roster_clear_foreach_group,
				      roster);
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

	item = (GossipRosterItem *) g_hash_table_lookup (priv->items, from);

	if (!item) {
		gossip_jid_unref (from);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	} 

	if (roster_item_update_presence (roster, item, from, m)) {
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
								 jid);
		
		subscription = lm_message_node_get_attribute (node, 
							      "subscription");
		if (subscription && strcmp (subscription, "remove") == 0) {
			roster_item_remove (roster, item);
			continue;
		}
	
		if (item) {
			roster_item_update (roster, item, node);
			continue;
		}

		/* It's a new item */
		item = roster_item_new (jid);
		gossip_jid_unref (jid);
	
		g_hash_table_insert (priv->items,
				     gossip_jid_ref (item->jid),
				     item);
		
		g_signal_emit (roster, signals[ITEM_ADDED], 0, item);
		
		roster_item_update (roster, item, node);
	}
	
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static GossipRosterItem *
roster_item_new (GossipJID *jid)
{
	GossipRosterItem *item;

	item = g_new0 (GossipRosterItem, 1);
	item->ref_count = 1;
	item->jid = gossip_jid_ref (jid);

	/* Set name to be jid to have something to show */
	item->name = g_strdup (gossip_jid_get_without_resource (jid));
	item->subscription = NULL;
	item->ask = NULL;

	item->groups = NULL;
	item->connections = NULL;

	item->online = FALSE;

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
	gboolean       in_a_group;
	GList         *l;
	GList         *groups;

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
		item->name = g_strdup (name);
	}

	/* FIXME: We should probably check if groups changed before doing
	 *        it this way. */
	groups = g_list_copy (item->groups);
	for (l = groups; l; l = l->next) {
		GossipRosterGroup *group = (GossipRosterGroup *) l->data;

		roster_item_remove_group (roster, item, group);
	}

	g_list_free (groups);
	in_a_group = FALSE;
	
	for (child = node->children; child; child = child->next) {
		if (strcmp (child->name, "group") == 0 && child->value) {
			roster_item_add_group (roster, item, child->value);
			in_a_group = TRUE;
		}
	}
	/* End of FIXME */

	if (!in_a_group) {
		roster_item_add_group (roster, item, UNSORTED_GROUP);
	}

	g_signal_emit (roster, signals[ITEM_UPDATED], 0, item);
}

static gboolean
roster_item_update_presence (GossipRoster     *roster,
			     GossipRosterItem *item,
			     GossipJID        *from,
			     LmMessage        *presence)
{
	LmMessageSubType  type;
	LmMessageNode    *node;
	RosterConnection *con = NULL;
	GList            *l;
	
	type = lm_message_get_sub_type (presence);

	for (l = item->connections; l; l = l->next) {
		RosterConnection *connection = (RosterConnection *)l->data;
		if (gossip_jid_equals (from, connection->jid)) {
			con = connection;
			break;
		}
	}

	if (lm_message_node_find_child (presence->node, "error")) {
		item->online = FALSE;
		if (con) {
			roster_item_remove_connection (item, con);
		}
		goto item_updated;
	}
	
	switch (type) {
	case LM_MESSAGE_SUB_TYPE_AVAILABLE:
		item->online = TRUE;
		break;
	case LM_MESSAGE_SUB_TYPE_UNAVAILABLE: 
		if (con) {
			roster_item_remove_connection (item, con);
		}
		if (g_list_length (item->connections) == 0) {
			item->online = FALSE;
		}
		goto item_updated; 
		break;
	default:
		return FALSE;
	}

	if (!con) { /* New connection */
		con = roster_item_add_connection (item, from);
	}

	node = lm_message_node_get_child (presence->node, "show");
	if (node) {
		con->show = gossip_utils_show_from_string (node->value);
	} else {
		con->show = GOSSIP_SHOW_AVAILABLE;
	}

	g_free (con->status);

	node = lm_message_node_get_child (presence->node, "status");
	if (node) {
		con->status = g_strdup (node->value);
	} else {
		con->status = NULL;
	}

	node = lm_message_node_get_child (presence->node, "priority");
	if (node) {
		con->priority = atoi (node->value);
	} else {
		con->priority = 0;
	}

	con->last_updated = time (NULL);
	item->connections = g_list_sort (item->connections, 
					 roster_item_sort_connections);

item_updated:
	g_signal_emit (roster, signals[ITEM_UPDATED], 0, item);

	return TRUE;
}

static void
roster_item_remove (GossipRoster *roster, GossipRosterItem *item) 
{
	GossipRosterPriv *priv;
	GList            *l;
	GList            *groups;
	
	priv = roster->priv;

	gossip_roster_item_ref (item);
	g_hash_table_remove (priv->items, item->jid);
	g_signal_emit (roster, signals[ITEM_REMOVED], 0, item);

	groups = g_list_copy (item->groups);
	for (l = groups; l; l = l->next) {
		GossipRosterGroup *group = (GossipRosterGroup *) l->data;
	
		roster_item_remove_group (roster, item, group);
	}
	g_list_free (groups);
	
	g_signal_emit (roster, signals[ITEM_REMOVED], 0, item);
	
	/* Remove these here since there can be external references to the
	 * item. But the groups might be gone */
	g_list_free (item->groups);
	item->groups = NULL;
	
	gossip_roster_item_unref (item);
}

static void
roster_item_free (GossipRosterItem *item)
{
	GList *l;
	
	d(g_print ("Freeing item: %s\n", item->name));
	gossip_jid_unref (item->jid);
	g_free (item->name);
	g_free (item->subscription);
	g_free (item->ask);

	/* Groups have been freed in roster_item_remove */
	
	for (l = item->connections; l; l = l->next) {
		RosterConnection *con = (RosterConnection *) l->data;
		roster_connection_free (con);
	}

	g_list_free (item->connections);
	
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
		g_signal_emit (roster, signals[GROUP_ADDED], 0, group);
	}
	
	if (!g_list_find (group->items, item)) {
		group->items = g_list_prepend (group->items, item);
	}

	if (!g_list_find (item->groups, group)) {
		item->groups = g_list_prepend (item->groups, group);
	}

	g_signal_emit (roster, signals[GROUP_ITEM_ADDED], 0, group, item);
}

static void
roster_item_remove_group (GossipRoster      *roster, 
			  GossipRosterItem  *item,
			  GossipRosterGroup *group)
{
	item->groups = g_list_remove (item->groups, group);
	group->items = g_list_remove (group->items, item);

	g_signal_emit (roster, signals[GROUP_ITEM_REMOVED], 0, group, item);

	if (g_list_length (group->items) == 0) {
		roster_group_remove (roster, group);
	}
}

static RosterConnection *
roster_item_add_connection (GossipRosterItem *item, GossipJID *jid)
{
	RosterConnection *connection;

	g_return_val_if_fail (item != NULL, NULL);
	g_return_val_if_fail (jid != NULL, NULL);

	connection = g_new0 (RosterConnection, 1);
	connection->jid = gossip_jid_ref (jid);
	connection->show = GOSSIP_SHOW_AVAILABLE;
	connection->priority = 0;
	connection->status = g_strdup ("");

	item->connections = g_list_prepend (item->connections, connection);
	item->connections = g_list_sort (item->connections, 
					 roster_item_sort_connections);

	d(g_print ("Adding connection: %s, nr of connections: %d\n", 
		   gossip_jid_get_full (jid), 
		   g_list_length (item->connections)));
	
	
	return connection;
}

static void 
roster_item_remove_connection (GossipRosterItem *item,
			       RosterConnection *connection)
{
	g_return_if_fail (item != NULL);
	g_return_if_fail (connection != NULL);

	d(g_print ("Removing connection: %s, nr of connections: %d\n", 
		   gossip_jid_get_full (connection->jid),
		   g_list_length (item->connections) - 1));
	
	item->connections = g_list_remove (item->connections, connection);
	
	roster_connection_free (connection);

	item->connections = g_list_sort (item->connections, 
					 roster_item_sort_connections);
}

static gint
roster_item_sort_connections (gconstpointer a, gconstpointer b) 
{
	RosterConnection *con_a = (RosterConnection *)a;
	RosterConnection *con_b = (RosterConnection *)b;

	if (con_a->priority > con_b->priority) {
		return -1;
	} 
	else if (con_a->priority == con_b->priority) {
		if (con_a->last_updated > con_b->last_updated) {
			return -1;
		}
	}

	return 1;
}

static void
roster_connection_free (RosterConnection *connection)
{
	g_return_if_fail (connection != NULL);
	
	g_free (connection->status);
	gossip_jid_unref (connection->jid);
	g_free (connection);
}

static GossipRosterGroup *
roster_group_new (const gchar *name)
{
	GossipRosterGroup *group;

	group = g_new0 (GossipRosterGroup, 1);

	group->name = g_strdup (name);
	group->items = NULL;
	group->ref_count = 1;

	return group;
}

static void
roster_group_remove (GossipRoster *roster, GossipRosterGroup *group)
{
	GossipRosterPriv *priv;

	g_return_if_fail (GOSSIP_IS_ROSTER (roster));
	g_return_if_fail (group != NULL);

	priv = roster->priv;

	/* Don't need to worry about the items, if we get here there are no
	 * items in this groups items list. */
	gossip_roster_group_ref (group);
	g_hash_table_remove (priv->groups, group->name);
	g_signal_emit (roster, signals[GROUP_REMOVED], 0, group);
	gossip_roster_group_unref (group);
}

static void
roster_group_free (GossipRosterGroup *group) 
{
	/* items list are freed in item_remove, that's why we get here */

	d(g_print ("Freeing group\n"));
	g_free (group->name);
	g_free (group);
}

static gboolean 
roster_group_is_internal (GossipRosterGroup *group)
{
	if (strcmp (group->name, UNSORTED_GROUP) == 0) {
		return TRUE;
	}

	return FALSE;
}
 
GossipRoster *
gossip_roster_new (void)
{
	GossipRoster *roster;

	roster = g_object_new (GOSSIP_TYPE_ROSTER, NULL);

	return roster;
}

GossipRosterGroup *
gossip_roster_get_group (GossipRoster *roster, const gchar *name)
{
	GossipRosterPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ROSTER (roster), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	priv = roster->priv;

	return g_hash_table_lookup (priv->groups, name);
}

static void
roster_add_group_to_list (gpointer            key, 
			  GossipRosterGroup  *group,
			  GList             **l)
{
	*l = g_list_prepend (*l, gossip_roster_group_ref (group));
}

GList *
gossip_roster_get_all_groups (GossipRoster *roster)
{
	GossipRosterPriv *priv;
	GList            *list = NULL;

	g_return_val_if_fail (GOSSIP_IS_ROSTER (roster), NULL);

	priv = roster->priv;
	
	g_hash_table_foreach (priv->groups,
			      (GHFunc) roster_add_group_to_list,
			      &list);
	return list;
}

void
gossip_roster_free_group_list (GList *list)
{
	GList *l;
	
	if (!list) {
		return;
	}
	
	for (l = list; l; l = l->next) {
		GossipRosterGroup *group = (GossipRosterGroup *) l->data;
		gossip_roster_group_unref (group);
	}

	g_list_free (list);
}

static void
roster_add_item_to_list (gpointer           key, 
			 GossipRosterItem  *item,
			 GList            **l)
{
	*l = g_list_prepend (*l, gossip_roster_item_ref (item));
}

GList *
gossip_roster_get_all_items (GossipRoster *roster)
{
	GossipRosterPriv *priv;
	GList            *list = NULL;

	g_return_val_if_fail (GOSSIP_IS_ROSTER (roster), NULL);

	priv = roster->priv;
	
	g_hash_table_foreach (priv->items,
			      (GHFunc) roster_add_item_to_list,
			      &list);
	return list;
}

void
gossip_roster_free_item_list (GList *list)
{
	GList *l;
	
	if (!list) {
		return;
	}

	for (l = list; l; l = l->next) {
		GossipRosterItem * item = (GossipRosterItem *) l->data;
		gossip_roster_item_unref (item);
	}

	g_list_free (list);
}

GossipRosterItem *
gossip_roster_get_item (GossipRoster *roster,
			GossipJID    *jid)
{
	GossipRosterPriv *priv;
	
	g_return_val_if_fail (GOSSIP_IS_ROSTER (roster), NULL);
	g_return_val_if_fail (jid != NULL, NULL);

	priv = roster->priv;
	
	return g_hash_table_lookup (priv->items, jid);
}

void
gossip_roster_remove_item (GossipRoster *roster, GossipRosterItem *item)
{
	GossipRosterPriv *priv;
	LmMessage        *m;
	LmMessageNode    *node;
	const gchar      *jid_str;

	g_return_if_fail (GOSSIP_IS_ROSTER (roster));
	g_return_if_fail (item != NULL);

	priv = roster->priv;

	d(g_print ("Removing '%s'\n", gossip_roster_item_get_name (item)));

	jid_str = gossip_jid_get_without_resource (item->jid);
	
	m = lm_message_new_with_sub_type (NULL,
 					  LM_MESSAGE_TYPE_IQ,
 					  LM_MESSAGE_SUB_TYPE_SET);
	
 	node = lm_message_node_add_child (m->node, "query", NULL);
 	lm_message_node_set_attribute (node,
				       "xmlns",
				       "jabber:iq:roster");
	
 	node = lm_message_node_add_child (node, "item", NULL);
 	lm_message_node_set_attributes (node,
					"jid", jid_str,
					"subscription", "remove",
					NULL);
 	
 	lm_connection_send (priv->connection, m, NULL);
 	lm_message_unref (m);

	m = lm_message_new_with_sub_type (jid_str,
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_UNSUBSCRIBE);
	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);
}

void
gossip_roster_rename_item (GossipRoster     *roster,
			   GossipRosterItem *item,
			   const gchar      *name)
{
	GossipRosterPriv *priv;
	LmMessage        *m;
	LmMessageNode    *node;
	const gchar      *jid_str;
	GList            *l; 

	g_return_if_fail (GOSSIP_IS_ROSTER (roster));
	g_return_if_fail (item != NULL);

	priv = roster->priv;
	
	d(g_print ("Renaming '%s' to '%s'\n", gossip_roster_item_get_name (item),
		   name));
	
	jid_str = gossip_jid_get_without_resource (item->jid);

	m = lm_message_new_with_sub_type (NULL, 
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_SET);
	node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attributes (node,
					"xmlns", "jabber:iq:roster",
					NULL);

	node = lm_message_node_add_child (node, "item", NULL);
	lm_message_node_set_attributes (node, 
					"jid", jid_str,
					"name", name,
					NULL);
	for (l = item->groups; l; l = l->next) {
		GossipRosterGroup *group = (GossipRosterGroup *) l->data;

		if (!roster_group_is_internal (group)) {
			lm_message_node_add_child (node, "group", group->name);
		}
	}	
	
	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);
}

void
gossip_roster_rename_group (GossipRoster      *roster,
			    GossipRosterGroup *group,
			    const gchar       *name)
{
	GossipRosterPriv *priv;
	LmMessage        *m;
	LmMessageNode    *q_node;
	GList            *i;

	g_return_if_fail (GOSSIP_IS_ROSTER (roster));
	g_return_if_fail (group != NULL);

	priv = roster->priv;

	d(g_print ("Renaming group from '%s' to '%s'\n", 
		   gossip_roster_group_get_name (group),
		   name));

	m = lm_message_new_with_sub_type (NULL,
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_SET);
	q_node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attributes (q_node,
					"xmlns", "jabber:iq:roster",
					NULL);
	
	/* Iterate over all children in the group */
	for (i = group->items; i; i = i->next) {
		GossipRosterItem *item = (GossipRosterItem *) i->data;
		LmMessageNode    *node;
		const gchar      *jid_str;
		GList            *l;
		
		jid_str = gossip_jid_get_without_resource (item->jid);
		
		node = lm_message_node_add_child (q_node, "item", NULL);
		lm_message_node_set_attributes (node,
						"jid", jid_str,
						"name", item->name,
						NULL);
		
		/* Iterate over all groups in each child */
		for (l = item->groups; l; l = l->next) {
			GossipRosterGroup *g = (GossipRosterGroup *) l->data;
			if (g != group && !roster_group_is_internal (g)) {
				lm_message_node_add_child (node, 
							   "group", g->name);
			}
		}

		lm_message_node_add_child (node, "group", name);
	}
	
	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);
}

const gchar * 
gossip_roster_group_get_name (GossipRosterGroup *group)
{
	g_return_val_if_fail (group != NULL, NULL);

	return group->name;
}

GList *
gossip_roster_group_get_items (GossipRosterGroup *group)
{
	g_return_val_if_fail (group != NULL, NULL);

	return group->items;
}

GossipRosterGroup *
gossip_roster_group_ref (GossipRosterGroup *group)
{
	g_return_val_if_fail (group != NULL, NULL);

	group->ref_count++;
	
	return group;
}

void
gossip_roster_group_unref (GossipRosterGroup *group)
{
       g_return_if_fail (group != NULL);
 
       group->ref_count--;
       if (group->ref_count <= 0) {
	       roster_group_free (group);
       }
}

GossipJID *
gossip_roster_item_get_jid (GossipRosterItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);

	if (item->online) {
		return ((RosterConnection *)item->connections->data)->jid;
	} 
	
	return item->jid;
}

const gchar *
gossip_roster_item_get_name      (GossipRosterItem  *item)
{
	g_return_val_if_fail (item != NULL, NULL);
	
	return item->name;
}

GossipShow
gossip_roster_item_get_show (GossipRosterItem *item)
{
	g_return_val_if_fail (item != NULL, GOSSIP_SHOW_AVAILABLE);

	if (item->online) {
		return ((RosterConnection *)item->connections->data)->show;
	}

	return GOSSIP_SHOW_AVAILABLE;
}

const gchar *
gossip_roster_item_get_status (GossipRosterItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);

	if (item->online) {
		return ((RosterConnection *)item->connections->data)->status;
	}

	return "";
}

GList * 
gossip_roster_item_get_groups    (GossipRosterItem  *item)
{
	g_return_val_if_fail (item != NULL, NULL);

	return item->groups;
}

gboolean
gossip_roster_item_is_offline (GossipRosterItem *item)
{
	g_return_val_if_fail (item != NULL, TRUE);
	
	return !item->online;
}

GossipRosterItem *
gossip_roster_item_ref (GossipRosterItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);

	item->ref_count++;
	
	return item;
}

void
gossip_roster_item_unref (GossipRosterItem *item)
{
        g_return_if_fail (item != NULL);
 
        item->ref_count--;
        if (item->ref_count <= 0) {
                roster_item_free (item);
        }
}



