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
#include "gossip-disco.h"

#define d(x) x

/* in seconds */
#define TIMEOUT 30


struct _GossipDisco {
        LmMessageHandler    *message_handler;

	GossipJID           *to;

	GossipDiscoItemFunc  func;
	gpointer             user_data;

	GList               *items;
	gint                 items_remaining;

	guint                timeout_id;
};

typedef struct {
	gchar *category;
	gchar *type;
	gchar *name;
} GossipDiscoIdentity;


typedef struct {
	GList *identities;
	GList *features;
} GossipDiscoInfo;


struct _GossipDiscoItem {
	GossipJID *jid;
	
	gchar     *node;
	gchar     *name;
	
	GossipDiscoInfo *info;
};


static GossipDisco    *disco_new                      (void);
static void            disco_init                     (void);
static void            disco_destroy_items_foreach    (GossipDiscoItem     *item,
						       gpointer             user_data);
static void            disco_destroy_info_foreach     (GossipDiscoInfo     *info,
						       gpointer             user_data);
static void            disco_destroy_ident_foreach    (GossipDiscoIdentity *ident,
						       gpointer             user_data);

static LmHandlerResult disco_message_handler          (LmMessageHandler    *handler,
						       LmConnection        *connection,
						       LmMessage           *m,
						       gpointer             user_data);

static void            disco_request_items            (GossipDisco         *disco);
static gboolean        disco_request_items_timeout_cb (GossipDisco         *disco);

static void            disco_handle_items             (GossipDisco         *disco,
						       LmMessage           *m,
						       gpointer             user_data);

static void            disco_request_info             (GossipDisco         *disco);

static void            disco_handle_info              (GossipDisco         *disco,
						       LmMessage           *m,
						       gpointer             user_data);


static GHashTable *discos = NULL;


static GossipDisco *
disco_new (void)
{
	GossipDisco      *disco;
	LmConnection     *connection;
	LmMessageHandler *handler;
	
	disco = g_new0 (GossipDisco, 1);

	connection = gossip_app_get_connection ();

	handler = lm_message_handler_new (disco_message_handler, disco, NULL);
	disco->message_handler = handler;
	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_IQ,
						LM_HANDLER_PRIORITY_NORMAL);
	
	return disco;
}

void
gossip_disco_destroy (GossipDisco *disco)
{
	LmConnection     *connection;
        LmMessageHandler *handler;

	if (!disco) {
		return;
	}

        connection = gossip_app_get_connection ();

        handler = disco->message_handler;
        if (handler) {
                lm_connection_unregister_message_handler (connection, 
							  handler, 
							  LM_MESSAGE_TYPE_IQ);
                lm_message_handler_unref (handler);
        }

	g_list_foreach (disco->items, (GFunc)disco_destroy_items_foreach, NULL);
	g_list_free (disco->items);

	if (disco->timeout_id) {
		g_source_remove (disco->timeout_id);
	}

 	g_hash_table_remove (discos, disco->to); 
}

static void 
disco_destroy_items_foreach (GossipDiscoItem *item, gpointer user_data)
{
	gossip_jid_unref (item->jid);
	
	g_free (item->node);
	g_free (item->name);

	if (item->info) {
		disco_destroy_info_foreach (item->info, NULL);
	}
}

static void 
disco_destroy_info_foreach (GossipDiscoInfo *info, gpointer user_data)
{
	g_list_foreach (info->identities, (GFunc)disco_destroy_ident_foreach, NULL);
	g_list_free (info->identities);

 	g_list_foreach (info->features, (GFunc)g_free, NULL); 
	g_list_free (info->features);
}

static void 
disco_destroy_ident_foreach (GossipDiscoIdentity *ident, gpointer user_data)
{
	g_free (ident->category);
	g_free (ident->type);
	g_free (ident->name);
}

static void
disco_init (void)
{
        static gboolean inited = FALSE;

	if (inited) {
		return;
	}

        inited = TRUE;

        discos = g_hash_table_new_full (gossip_jid_hash,
					gossip_jid_equal,
					(GDestroyNotify) gossip_jid_unref,
					(GDestroyNotify) g_free);
}

GossipDisco *
gossip_disco_request (const char          *to, 
		      GossipDiscoItemFunc  func,
		      gpointer             user_data)
{
	GossipDisco *disco;
	GossipJID   *jid;

	disco_init ();

	if (!to || !to[0]) {
		return NULL;
	}

	jid = gossip_jid_new (to);

	disco = g_hash_table_lookup (discos, jid);

	if (disco) {
		return disco;
	}

	disco = disco_new ();
	g_hash_table_insert (discos, gossip_jid_ref (jid), disco);

	disco->to = jid;

	disco->func = func;
	disco->user_data = user_data;

	/* start timeout */
	disco->timeout_id = g_timeout_add (TIMEOUT * 1000, 
					   (GSourceFunc) disco_request_items_timeout_cb,
					   disco);

	/* send initial request */
	disco_request_items (disco);

	return disco;
}

static void
disco_request_items (GossipDisco *disco)
{
        LmMessage     *m;
	LmMessageNode *node;

	/* create message */
        m = lm_message_new_with_sub_type (gossip_jid_get_full (disco->to),
                                          LM_MESSAGE_TYPE_IQ,
                                          LM_MESSAGE_SUB_TYPE_GET);

	d(g_print ("to: %s (disco items request)\n", 
		   gossip_jid_get_full (disco->to)));
	
	lm_message_node_add_child (m->node, "query", NULL);
	node = lm_message_node_get_child (m->node, "query");

        lm_message_node_set_attribute (node, "xmlns", "http://jabber.org/protocol/disco#items");

        lm_connection_send (gossip_app_get_connection (), m, NULL);
        lm_message_unref (m);
}

static gboolean
disco_request_items_timeout_cb (GossipDisco *disco)
{
	d(g_print ("disco to:'%s' has timed out after %d seconds, cleaning up...\n", 
		   gossip_jid_get_full (disco->to), TIMEOUT));

	/* stop timeout */
	disco->timeout_id = 0;

	/* call callback and inform of last item */
	if (disco->func) {
		(disco->func) (disco, 
			       NULL, 
			       FALSE,
			       TRUE,
			       disco->user_data);
	}

	/* remove disco */
	gossip_disco_destroy (disco);

	return FALSE;
}


static LmHandlerResult
disco_message_handler (LmMessageHandler *handler,
		       LmConnection     *connection,
		       LmMessage        *m,
		       gpointer          user_data)
{
        GossipDisco   *disco;
        const gchar   *from;
        GossipJID     *from_jid;
	LmMessageNode *node; 
	const char    *xmlns;

	disco = (GossipDisco *) user_data;

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

		if (strcmp (xmlns, "http://jabber.org/protocol/disco#items") == 0) {
			/* remove timeout: we do this because at this
			   stage, the server has responded */
			if (disco->timeout_id) {
				g_source_remove (disco->timeout_id);
				disco->timeout_id = 0;
			}

			disco_handle_items (disco, m, user_data);
			disco_request_info (disco);
		} else if (strcmp (xmlns, "http://jabber.org/protocol/disco#info") == 0) {
			disco_handle_info (disco, m, user_data);
		} else {
			return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
		}
        }

        return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}


/*
 * items
 */

static void
disco_handle_items (GossipDisco *disco,
		    LmMessage   *m,
		    gpointer     user_data)
{
	GossipDiscoItem *item;
	const char      *jid_str;
	const char      *node_str;
	const char      *name_str;
	LmMessageNode   *node;
	
	node = lm_message_node_find_child (m->node, "item");

	while (node) {
		item = g_new0 (GossipDiscoItem, 1);
		
		jid_str = lm_message_node_get_attribute (node, "jid");
		item->jid = gossip_jid_new (jid_str);
		
		node_str = lm_message_node_get_attribute (node, "node");
		item->node = g_strdup (node_str);
		
		name_str = lm_message_node_get_attribute (node, "name");
		item->name = g_strdup (name_str);

		disco->items = g_list_append (disco->items, item);

		d(g_print ("disco item - jid:'%s', node:'%s', name:'%s'\n", 
			   jid_str, node_str, name_str));

		/* go to next item */
		node = node->next;
	}
}

static void
disco_request_info (GossipDisco *disco)
{
	GList     *l;
	GossipJID *jid;
	
	jid = gossip_jid_new ("users.jabber.org");

	disco->items_remaining = g_list_length (disco->items);

	for (l = disco->items; l; l = l->next) {
		GossipDiscoItem *item;
		LmMessage       *m;
		LmMessageNode   *node;

		item = l->data;

		/* NOTE: This is a temporary measure to ignore the
		   users.jabber.org JID because it never seems to
		   respond.  If this code is uncommented it will treat
		   it as any other JID. */
		if (gossip_jid_equals (item->jid, jid)) {
			d(g_print ("ignoring JID:'users.jabber.org', it doesn't tend to respond\n"));
			disco->items_remaining--;
			continue;
		}

		/* create message */
		m = lm_message_new_with_sub_type (gossip_jid_get_full (item->jid),
						  LM_MESSAGE_TYPE_IQ,
						  LM_MESSAGE_SUB_TYPE_GET);
		
		d(g_print ("to: %s (disco info request)\n", 
			   gossip_jid_get_full (item->jid)));
		
		lm_message_node_add_child (m->node, "query", NULL);
		node = lm_message_node_get_child (m->node, "query");
		
		lm_message_node_set_attribute (node, "xmlns", "http://jabber.org/protocol/disco#info");
		
		lm_connection_send (gossip_app_get_connection (), m, NULL);
		lm_message_unref (m);
	}

	gossip_jid_unref (jid);
}

static void
disco_handle_info (GossipDisco *disco,
		   LmMessage   *m,
		   gpointer     user_data)
{
 	GossipDiscoInfo *info; 
	GossipDiscoItem *item;
 	LmMessageNode   *node; 
	GList           *l;
	const char      *from;
	const char      *category;
	const char      *type;
	const char      *name;
	const char      *feature;
	
	disco->items_remaining--;

        from = lm_message_node_get_attribute (m->node, "from");
	d(g_print ("sorting disco info for:'%s'....\n", from));

	item = NULL;
	for (l = disco->items; l; l = l->next) {
		const gchar *jid;

		item = l->data;
		jid = gossip_jid_get_full (item->jid);

		if (strcmp (jid, from) == 0) {
			break;
		}
	}

	if (!item) {
		/* this usually occurs when a message from a previous
		   request has been received and the data is no longer 
		   relevant */
		return;
	}

	info = g_new0 (GossipDiscoInfo, 1);

	node = lm_message_node_find_child (m->node, "identity");

	while (node && strcmp (node->name, "identity") == 0) {
		GossipDiscoIdentity *ident;

		ident = g_new0 (GossipDiscoIdentity, 1);
		
		category = lm_message_node_get_attribute (node, "category");
		ident->category = g_strdup (category);

		type = lm_message_node_get_attribute (node, "type");
		ident->type = g_strdup (type);

		name = lm_message_node_get_attribute (node, "name");
		ident->name = g_strdup (name);

		d(g_print ("disco item - category:'%s', type:'%s', name:'%s'\n", 
			   category, type, name));

		info->identities = g_list_append (info->identities, ident);

		node = node->next;
	}

	node = lm_message_node_find_child (m->node, "feature");

	while (node && strcmp (node->name, "feature") == 0) {
		feature = lm_message_node_get_attribute (node, "var");

		d(g_print ("disco item - feature:'%s'\n", feature));

		info->features = g_list_append (info->features, g_strdup (feature));

		node = node->next;
	}

	item->info = info;

	if (disco->func) {
		(disco->func) (disco, 
			       item, 
			       disco->items_remaining < 1 ? TRUE : FALSE,
			       FALSE,
			       disco->user_data);
	}

	if (disco->items_remaining < 1) {
		gossip_disco_destroy (disco);
	}
}

GList *
gossip_disco_get_category (GossipDisco *disco, 
			   const char  *category)
{
	GList *l1;
	GList *services;

	g_return_val_if_fail (disco != NULL, NULL);
	g_return_val_if_fail (category != NULL, NULL);
	 
	services = NULL;

	for (l1 = disco->items; l1; l1 = l1->next) {
		GossipDiscoItem *item;
		GossipDiscoInfo *info;
		gboolean         have_category;
		gboolean         can_register;
		GList           *l2;

		item = l1->data;
		info = item->info;
		
		if (!info) {
			continue;
		}

		have_category = FALSE;
		for (l2 = info->identities; l2; l2 = l2->next) {
			GossipDiscoIdentity *ident = l2->data;
			
			if (strcmp (ident->category, category) == 0) {
				have_category = TRUE;
				break;
			}
		}

		can_register = FALSE;
		for (l2 = info->features; l2; l2 = l2->next) {
			const gchar *features = l2->data;

			if (strcmp (features, "jabber:iq:register") == 0) {
				can_register = TRUE;
				break;
			}
		}

		if (have_category && can_register) {
			services = g_list_append (services, gossip_jid_ref (item->jid));
		}
	}

	return services;
}

GList *
gossip_disco_get_category_and_type (GossipDisco *disco,
				    const gchar *category,
				    const gchar *type)
{
	GList *l1;
	GList *services;

	g_return_val_if_fail (disco != NULL, NULL);
	g_return_val_if_fail (category != NULL, NULL);
	g_return_val_if_fail (type != NULL, NULL);

	services = NULL;
	for (l1 = disco->items; l1; l1 = l1->next) {
		GossipDiscoItem *item;
		GossipDiscoInfo *info;
		GList           *l2;
		gboolean         have_category; /* e.g. gateway */
		gboolean         have_type;     /* e.g. icq or msn, etc */
		gboolean         can_register;  /* server allows registration */

		item = l1->data;
		info = item->info;

		if (!info) {
			continue;
		}

		have_category = FALSE;
		have_type     = FALSE;
		can_register  = FALSE;

		for (l2 = info->identities; l2; l2 = l2->next) {
			GossipDiscoIdentity *ident = l2->data;
			
			have_category = FALSE;
			have_type = FALSE;
	
			if (strcmp (ident->category, category) == 0) {
				have_category = TRUE;
			}

			if (strcmp (ident->type, type) == 0) {
				have_type = TRUE;
			}

			if (have_category && have_type) {
				break;
			}
		}

		for (l2 = info->features; l2; l2 = l2->next) {
			const gchar *features = l2->data;

			if (strcmp (features, "jabber:iq:register") == 0) {
				can_register = TRUE;
				break;
			}
		}

		if (have_category && have_type && can_register) {
			services = g_list_append (services, gossip_jid_ref (item->jid));
		}
	}

	return services;
}

GossipDiscoItem *
gossip_disco_get_item (GossipDisco *disco,
		       GossipJID   *jid)
{
	GList *l;

	g_return_val_if_fail (disco != NULL, NULL);
	
	for (l = disco->items; l; l = l->next) {
		GossipDiscoItem *item;
		
		item = l->data;

		if (gossip_jid_equals (item->jid, jid)) {
			return item;
		}
	}

	return NULL;
}

GossipJID *
gossip_disco_item_get_jid (GossipDiscoItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);

	return item->jid;
}

char *
gossip_disco_item_get_name (GossipDiscoItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);

	return item->name;
}

const GList *
gossip_disco_item_get_features (GossipDiscoItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);
	g_return_val_if_fail (item->info != NULL, NULL);

	return item->info->features;
}

gboolean 
gossip_disco_item_has_category (GossipDiscoItem *item,
				const gchar     *category)
{
	GList *l;
	
	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (item->info != NULL, FALSE);
	g_return_val_if_fail (category != NULL, FALSE);
	
	for (l = item->info->identities; l; l = l->next) {
		GossipDiscoIdentity *ident;
		
		ident = l->data;

		if (strcmp (ident->category, category) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

gboolean 
gossip_disco_item_has_type (GossipDiscoItem *item,
			    const gchar     *type)
{
	GList *l;
	
	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (item->info != NULL, FALSE);
	g_return_val_if_fail (type != NULL, FALSE);
	
	for (l = item->info->identities; l; l = l->next) {
		GossipDiscoIdentity *ident;
		
		ident = l->data;

		if (strcmp (ident->type, type) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

gboolean 
gossip_disco_item_has_feature (GossipDiscoItem *item,
			       const gchar     *feature)
{
	GList *l;
	
	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (item->info != NULL, FALSE);
	g_return_val_if_fail (feature != NULL, FALSE);
	
	for (l = item->info->features; l; l = l->next) {
		const char *str;
		
		str = l->data;

		if (strcmp (str, feature) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}
