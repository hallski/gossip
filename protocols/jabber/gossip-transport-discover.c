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
#include <stdlib.h>
#include <loudmouth/loudmouth.h>
#include <libgossip/gossip-utils.h>

#include "gossip-jabber-private.h"

#include "gossip-transport-discover.h"

#define d(x)

/* in seconds */
#define DISCO_TIMEOUT      20
#define DISCO_INFO_TIMEOUT 10


struct _GossipTransportDisco {
	GossipJabber *jabber;
	GossipJID *to;

        LmMessageHandler *message_handler;

	gpointer user_data;

	/* items */
	GossipTransportDiscoItemFunc item_func;

	GList *items;
	gint items_remaining;
	gint items_total;
 
	/* flags */
	gboolean item_lookup;
	gboolean destroying;

	/* errors */
	GError *last_error;

	/* misc */
	guint timeout_id;
};


typedef struct {
	gchar *category;
	gchar *type;
	gchar *name;
} GossipTransportDiscoIdentity;


typedef struct {
	GList *identities;
	GList *features;
} GossipTransportDiscoInfo;


struct _GossipTransportDiscoItem {
	GossipJID *jid;
	
	gchar *node;
	gchar *name;
	
	guint timeout_id;

	GossipTransportDiscoInfo *info;
};


typedef struct {
	gboolean found;
	GossipJID *jid;
} FindInfo;


static GossipTransportDisco *transport_disco_new                      (GossipJabber                 *jabber);
static void                  transport_disco_init                     (void);
static void                  transport_disco_destroy_items_foreach    (GossipTransportDiscoItem     *item,
								       gpointer                      user_data);
static void                  transport_disco_destroy_info_foreach     (GossipTransportDiscoInfo     *info,
								       gpointer                      user_data);
static void                  transport_disco_destroy_ident_foreach    (GossipTransportDiscoIdentity *ident,
								       gpointer                      user_data);
static gboolean              transport_disco_find_item_func           (GossipJID                    *jid,
								       GossipTransportDisco         *disco,
								       GossipTransportDiscoItem     *item);
static void                  transport_disco_set_last_error           (GossipTransportDisco         *disco,
								       LmMessage                    *m);
static LmHandlerResult       transport_disco_message_handler          (LmMessageHandler             *handler,
								       LmConnection                 *connection,
								       LmMessage                    *m,
								       gpointer                      user_data);
static void                  transport_disco_request_items            (GossipTransportDisco         *disco);
static gboolean              transport_disco_request_info_timeout_cb  (GossipTransportDiscoItem     *item);
static gboolean              transport_disco_request_items_timeout_cb (GossipTransportDisco         *disco);
static void                  transport_disco_handle_items             (GossipTransportDisco         *disco,
								       LmMessage                    *m,
								       gpointer                      user_data);
static void                  transport_disco_request_info             (GossipTransportDisco         *disco);
static void                  transport_disco_handle_info              (GossipTransportDisco         *disco,
								       LmMessage                    *m,
								       gpointer                      user_data);


static GHashTable *discos = NULL; 


static GossipTransportDisco *
transport_disco_new (GossipJabber *jabber)
{
	GossipTransportDisco *disco;
	LmConnection         *connection;
	LmMessageHandler     *handler;
	
	disco = g_new0 (GossipTransportDisco, 1);

	disco->jabber = g_object_ref (jabber);

	connection = _gossip_jabber_get_connection (jabber);

	handler = lm_message_handler_new (transport_disco_message_handler, disco, NULL);
	disco->message_handler = handler;
	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_IQ,
						LM_HANDLER_PRIORITY_NORMAL);
	
	return disco;
}

static void
transport_disco_init (void)
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

static void 
transport_disco_destroy_items_foreach (GossipTransportDiscoItem *item, 
				       gpointer                  user_data)
{
	gossip_jid_unref (item->jid);
	
	g_free (item->node);
	g_free (item->name);

	if (item->timeout_id) {
		g_source_remove (item->timeout_id);
		item->timeout_id = 0;
	}

	if (item->info) {
		transport_disco_destroy_info_foreach (item->info, NULL);
	}
}

static void 
transport_disco_destroy_info_foreach (GossipTransportDiscoInfo *info, 
				      gpointer                  user_data)
{
	g_list_foreach (info->identities, (GFunc)transport_disco_destroy_ident_foreach, NULL);
	g_list_free (info->identities);

 	g_list_foreach (info->features, (GFunc)g_free, NULL); 
	g_list_free (info->features);
}

static void 
transport_disco_destroy_ident_foreach (GossipTransportDiscoIdentity *ident,
				       gpointer                      user_data)
{
	g_free (ident->category);
	g_free (ident->type);
	g_free (ident->name);
}

static gboolean 
transport_disco_find_item_func (GossipJID                *jid,
				GossipTransportDisco     *disco,
				GossipTransportDiscoItem *item)
{
	GList *l;

	if (!disco) {
		return FALSE;
	}

	for (l=disco->items; l; l=l->next) {
		if (l->data == item) {
			return TRUE;
		}
	}

	return FALSE;
}

static void
transport_disco_set_last_error (GossipTransportDisco *disco,
				LmMessage            *m)
{
	LmMessageNode *node;
	const gchar   *xmlns;
	const gchar   *error_code;
	const gchar   *error_reason;

	node = lm_message_node_get_child (m->node, "query");	
	if (!node) {
		return;
	}

	xmlns = lm_message_node_get_attribute (node, "xmlns");
	if (!xmlns) {
		return;	
	}
	
	if (strcmp (xmlns, "http://jabber.org/protocol/disco#items") != 0) {
		/* FIXME: currently only handle errors for this namespace */ 
		return;
	}
	
	node = lm_message_node_get_child (m->node, "error");	
	if (!node) {
		return;
	}
	
	error_code = lm_message_node_get_attribute (node, "code");
	error_reason = lm_message_node_get_value (node);

	if (error_code || error_reason) {
		GQuark quark; 
		GError *error;
		
		quark = g_quark_from_string ("gossip-transport-discover");
		error = g_error_new_literal (quark,
					     atoi (error_code), 
					     error_reason);

		if (disco->last_error) {
			g_error_free (disco->last_error);
			disco->last_error = NULL;
		}

		disco->last_error = error;
	}
}

static LmHandlerResult
transport_disco_message_handler (LmMessageHandler *handler,
				 LmConnection     *connection,
				 LmMessage        *m,
				 gpointer          user_data)
{
        GossipTransportDisco *disco;
        GossipJID            *from_jid;
        const gchar          *from;
	LmMessageNode        *node; 
	const char           *xmlns;

        from = lm_message_node_get_attribute (m->node, "from");
        from_jid = gossip_jid_new (from);

	/* used for info look ups */
	disco = g_hash_table_lookup (discos, from_jid);

	/* if not listed under the from jid, try the user data */
	if (!disco) {
		disco = (GossipTransportDisco *) user_data; 
	}

	if (lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_RESULT && 
	    lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_ERROR) {
 		gossip_jid_unref (from_jid); 
		
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

	if (strcmp (xmlns, "http://jabber.org/protocol/disco#items") == 0) {
		/* remove timeout: we do this because at this
		   stage, the server has responded */
		if (disco->timeout_id) {
			g_source_remove (disco->timeout_id);
			disco->timeout_id = 0;
		}

		if (lm_message_get_sub_type (m) == LM_MESSAGE_SUB_TYPE_ERROR) {
			transport_disco_set_last_error (disco, m);
		
			if (!disco->item_lookup && disco->items_remaining > 0) {
				disco->items_remaining--;
			}
			
			/* call callback and inform of last item */
			if (disco->item_func) {
				(disco->item_func) (disco, 
						    NULL, 
						    disco->items_remaining < 1 ? TRUE : FALSE,
						    FALSE,
						    disco->last_error,
						    disco->user_data);
			}
			
			if (!disco->item_lookup && disco->items_remaining < 1) {
				gossip_transport_disco_destroy (disco);
			}
		} else {
			transport_disco_handle_items (disco, m, user_data);
			transport_disco_request_info (disco);
		}
	} else if (strcmp (xmlns, "http://jabber.org/protocol/disco#info") == 0) {
		transport_disco_handle_info (disco, m, user_data);
	} else {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

        return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
transport_disco_request_items (GossipTransportDisco *disco)
{
	LmConnection  *connection;
        LmMessage     *m;
	LmMessageNode *node;

	/* create message */
        m = lm_message_new_with_sub_type (gossip_jid_get_full (disco->to),
                                          LM_MESSAGE_TYPE_IQ,
                                          LM_MESSAGE_SUB_TYPE_GET);

	d(g_print ("disco items to: %s\n", 
		   gossip_jid_get_full (disco->to)));
	
	connection = _gossip_jabber_get_connection (disco->jabber);

	lm_message_node_add_child (m->node, "query", NULL);
	node = lm_message_node_get_child (m->node, "query");

        lm_message_node_set_attribute (node, "xmlns", "http://jabber.org/protocol/disco#items");

        lm_connection_send (connection, m, NULL);
        lm_message_unref (m);
}

static gboolean
transport_disco_request_items_timeout_cb (GossipTransportDisco *disco)
{
	d(g_print ("disco items to:'%s' have timed out after %d seconds, cleaning up...\n", 
		   gossip_jid_get_full (disco->to), DISCO_TIMEOUT));

	/* stop timeout */
	disco->timeout_id = 0;

	/* call callback and inform of last item */
	if (disco->item_func) {
		(disco->item_func) (disco, 
				    NULL, 
				    FALSE,
				    TRUE,
				    NULL,
				    disco->user_data);
	}

	/* remove disco */
	gossip_transport_disco_destroy (disco);

	return FALSE;
}

static gboolean
transport_disco_request_info_timeout_cb (GossipTransportDiscoItem *item)
{
	GossipTransportDisco *disco;

	disco = g_hash_table_find (discos, (GHRFunc)transport_disco_find_item_func, item);
	if (!disco) {
		return FALSE;
	}

	d(g_print ("disco info to:'%s' has timed out after %d seconds, cleaning up...\n", 
		   gossip_jid_get_full (disco->to), DISCO_INFO_TIMEOUT));

	if (!disco->item_lookup) {
		disco->items_remaining--;
	}

	/* stop timeout */
	item->timeout_id = 0;

	/* call callback and inform of last item */
	if (disco->item_func) {
		(disco->item_func) (disco, 
				    item, 
				    disco->items_remaining < 1 ? TRUE : FALSE,
				    TRUE,
				    NULL,
				    disco->user_data);
	}

	if (!disco->item_lookup && disco->items_remaining < 1) {
		gossip_transport_disco_destroy (disco);
	}

	return FALSE;
}

static void
transport_disco_handle_items (GossipTransportDisco *disco,
			      LmMessage            *m,
			      gpointer              user_data)
{
	GossipTransportDiscoItem *item;
	const char               *jid_str;
	const char               *node_str;
	const char               *name_str;
	LmMessageNode            *node;
	
	node = lm_message_node_find_child (m->node, "item");

	while (node) {
		item = g_new0 (GossipTransportDiscoItem, 1);
		
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
transport_disco_request_info (GossipTransportDisco *disco)
{
	LmConnection *connection;
	GList        *l;
	GossipJID    *jid;
	
	connection = _gossip_jabber_get_connection (disco->jabber);
	jid = gossip_jid_new ("users.jabber.org");

	disco->items_total = disco->items_remaining = g_list_length (disco->items);


	for (l = disco->items; l; l = l->next) {
		GossipTransportDiscoItem *item;
		LmMessage                *m;
		LmMessageNode            *node;

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
		
		d(g_print ("disco info to: %s\n", 
			   gossip_jid_get_full (item->jid)));
		
		/* start timeout */
		item->timeout_id = g_timeout_add (DISCO_INFO_TIMEOUT * 1000,  
						  (GSourceFunc) transport_disco_request_info_timeout_cb,  
						  item);  


		lm_message_node_add_child (m->node, "query", NULL);
		node = lm_message_node_get_child (m->node, "query");
		
		lm_message_node_set_attribute (node, "xmlns", "http://jabber.org/protocol/disco#info");
		
		lm_connection_send (connection, m, NULL);
		lm_message_unref (m);
	}

	gossip_jid_unref (jid);
}

static void
transport_disco_handle_info (GossipTransportDisco *disco,
			     LmMessage            *m,
			     gpointer              user_data)
{
 	GossipTransportDiscoInfo *info; 
	GossipTransportDiscoItem *item;
 	LmMessageNode            *node; 
	GList                    *l;
	const char               *from;
	const char               *category;
	const char               *type;
	const char               *name;
	const char               *feature;
	
	if (!disco->item_lookup) {
		disco->items_remaining--;
	}

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

	info = g_new0 (GossipTransportDiscoInfo, 1);

	node = lm_message_node_find_child (m->node, "identity");

	while (node && strcmp (node->name, "identity") == 0) {
		GossipTransportDiscoIdentity *ident;

		ident = g_new0 (GossipTransportDiscoIdentity, 1);
		
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

	/* remove timeout */
	if (item->timeout_id) {
		g_source_remove (item->timeout_id);
		item->timeout_id = 0;
	}

	if (disco->item_func) {
		(disco->item_func) (disco, 
				    item, 
				    disco->items_remaining < 1 ? TRUE : FALSE,
				    FALSE,
				    NULL,
				    disco->user_data);
	}

	if (!disco->item_lookup && disco->items_remaining < 1) {
		gossip_transport_disco_destroy (disco);
	}
}

GossipTransportDisco *
gossip_transport_disco_request (GossipJabber                 *jabber,
				const char                   *to, 
				GossipTransportDiscoItemFunc  item_func,
				gpointer                      user_data)
{
	GossipTransportDisco *disco;
	GossipJID            *jid;

	g_return_val_if_fail (jabber != NULL, NULL);
	g_return_val_if_fail (to != NULL, NULL);
	g_return_val_if_fail (item_func != NULL, NULL);

	transport_disco_init ();

	jid = gossip_jid_new (to);

	disco = g_hash_table_lookup (discos, jid);

	if (disco) {
		return disco;
	}

	disco = transport_disco_new (jabber);
	g_hash_table_insert (discos, gossip_jid_ref (jid), disco);

	disco->to = jid;

	disco->item_func = item_func;
	disco->user_data = user_data;

	disco->items_remaining = 1;
	disco->items_total = 1;

	/* start timeout */
	disco->timeout_id = g_timeout_add (DISCO_TIMEOUT * 1000, 
					   (GSourceFunc) transport_disco_request_items_timeout_cb,
					   disco);

	/* send initial request */
	transport_disco_request_items (disco);

	return disco;
}

void
gossip_transport_disco_destroy (GossipTransportDisco *disco)
{
	LmConnection     *connection;
        LmMessageHandler *handler;

	/* we don't mind if NULL is supplied, an error message is
	   unnecessary. */ 
	if (!disco || disco->destroying) {
		return;
	}

	disco->destroying = TRUE;

        connection = _gossip_jabber_get_connection (disco->jabber);

        handler = disco->message_handler;
        if (handler) {
                lm_connection_unregister_message_handler (connection, 
							  handler, 
							  LM_MESSAGE_TYPE_IQ);
                lm_message_handler_unref (handler);
        }

	g_list_foreach (disco->items, (GFunc)transport_disco_destroy_items_foreach, NULL);
	g_list_free (disco->items);

	if (disco->timeout_id) {
		g_source_remove (disco->timeout_id);
	}

	if (disco->last_error) {
		g_error_free (disco->last_error);
	}

	g_object_unref (disco->jabber);

 	g_hash_table_remove (discos, disco->to); 
}

GossipTransportDisco *
gossip_transport_disco_request_info (GossipJabber                 *jabber,
				     const char                   *to, 
				     GossipTransportDiscoItemFunc  item_func,
				     gpointer                      user_data)
{
	GossipTransportDisco     *disco;
	GossipTransportDiscoItem *item;
	GossipJID                *jid;
	LmConnection             *connection;
	LmMessageHandler         *handler;

	g_return_val_if_fail (jabber != NULL, NULL);
	g_return_val_if_fail (to != NULL, NULL);
	g_return_val_if_fail (item_func != NULL, NULL);

	transport_disco_init ();

	jid = gossip_jid_new (to);

	disco = g_hash_table_lookup (discos, jid);

	if (disco) {
		gossip_jid_unref (jid);
		return disco;
	}

	/* create disco */
	disco = g_new0 (GossipTransportDisco, 1);

	disco->jabber = g_object_ref (jabber);

	/* set up handler */
	connection = _gossip_jabber_get_connection (jabber);

	handler = lm_message_handler_new (transport_disco_message_handler, disco, NULL);
	disco->message_handler = handler;
	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_IQ,
						LM_HANDLER_PRIORITY_NORMAL);

	/* add disco and configure members */
	g_hash_table_insert (discos, gossip_jid_ref (jid), disco);

	disco->to = jid;

	disco->item_lookup = TRUE;

	disco->item_func = item_func;
	disco->user_data = user_data;

	disco->items_remaining = 1;
	disco->items_total = 1;

	/* add item */
	item = g_new0 (GossipTransportDiscoItem, 1);

	item->jid = gossip_jid_ref (jid);

	disco->items = g_list_append (disco->items, item);

	/* send request for info */
	transport_disco_request_info (disco);

	return disco;
}

GList *
gossip_transport_disco_get_category (GossipTransportDisco *disco, 
				     const char           *category)
{
	GList *l1;
	GList *services;

	g_return_val_if_fail (disco != NULL, NULL);
	g_return_val_if_fail (category != NULL, NULL);
	 
	services = NULL;

	for (l1 = disco->items; l1; l1 = l1->next) {
		GossipTransportDiscoItem *item;
		GossipTransportDiscoInfo *info;
		gboolean                  have_category;
		gboolean                  can_register;
		GList                    *l2;

		item = l1->data;
		info = item->info;
		
		if (!info) {
			continue;
		}

		have_category = FALSE;
		for (l2 = info->identities; l2; l2 = l2->next) {
			GossipTransportDiscoIdentity *ident = l2->data;
			
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
gossip_transport_disco_get_category_and_type (GossipTransportDisco *disco,
					      const gchar          *category,
					      const gchar          *type)
{
	GList *l1;
	GList *services;

	g_return_val_if_fail (disco != NULL, NULL);
	g_return_val_if_fail (category != NULL, NULL);
	g_return_val_if_fail (type != NULL, NULL);

	services = NULL;
	for (l1 = disco->items; l1; l1 = l1->next) {
		GossipTransportDiscoItem *item;
		GossipTransportDiscoInfo *info;
		GList                    *l2;
		gboolean                  have_category; /* e.g. gateway */
		gboolean                  have_type;     /* e.g. icq or msn, etc */
		gboolean                  can_register;  /* server allows registration */

		item = l1->data;
		info = item->info;

		if (!info) {
			continue;
		}

		have_category = FALSE;
		have_type     = FALSE;
		can_register  = FALSE;

		for (l2 = info->identities; l2; l2 = l2->next) {
			GossipTransportDiscoIdentity *ident = l2->data;
			
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

gint
gossip_transport_disco_get_items_remaining (GossipTransportDisco *disco)
{
	g_return_val_if_fail (disco != NULL, -1);

	return disco->items_remaining;
}

gint
gossip_transport_disco_get_items_total (GossipTransportDisco *disco)
{
	g_return_val_if_fail (disco != NULL, -1);

	return disco->items_total;
}

GossipTransportDiscoItem *
gossip_transport_disco_get_item (GossipTransportDisco *disco,
				 GossipJID            *jid)
{
	GList *l;

	g_return_val_if_fail (disco != NULL, NULL);
	
	for (l = disco->items; l; l = l->next) {
		GossipTransportDiscoItem *item;
		
		item = l->data;

		if (gossip_jid_equals (item->jid, jid)) {
			return item;
		}
	}

	return NULL;
}

GossipJID *
gossip_transport_disco_item_get_jid (GossipTransportDiscoItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);

	return item->jid;
}

const char *
gossip_transport_disco_item_get_name (GossipTransportDiscoItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);

	return item->name;
}

const gchar *
gossip_transport_disco_item_get_type (GossipTransportDiscoItem *item)
{
	GList       *l;
	const gchar *type = NULL;

	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (item->info != NULL, FALSE);
	
	for (l = item->info->identities; l; l = l->next) {
		GossipTransportDiscoIdentity *ident;
		
		ident = l->data;
		
		/* if more than one type, we leave - this function is
		   really for requests on a per item basis, if a FULL
		   request for all services is asked, then there will
		   be types for each item */
		if (type) {
			return NULL;
		}

		if (ident) {
			type = ident->type;
		}
	}

	return type;
}

const GList *
gossip_transport_disco_item_get_features (GossipTransportDiscoItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);
	g_return_val_if_fail (item->info != NULL, NULL);

	return item->info->features;
}

gboolean 
gossip_transport_disco_item_has_category (GossipTransportDiscoItem *item,
					  const gchar              *category)
{
	GList *l;
	
	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (item->info != NULL, FALSE);
	g_return_val_if_fail (category != NULL, FALSE);
	
	for (l = item->info->identities; l; l = l->next) {
		GossipTransportDiscoIdentity *ident;
		
		ident = l->data;

		if (strcmp (ident->category, category) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

gboolean 
gossip_transport_disco_item_has_type (GossipTransportDiscoItem *item,
				      const gchar              *type)
{
	GList *l;
	
	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (item->info != NULL, FALSE);
	g_return_val_if_fail (type != NULL, FALSE);
	
	for (l = item->info->identities; l; l = l->next) {
		GossipTransportDiscoIdentity *ident;
		
		ident = l->data;

		if (strcmp (ident->type, type) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

gboolean 
gossip_transport_disco_item_has_feature (GossipTransportDiscoItem *item,
					 const gchar              *feature)
{
	GList *l;
	
	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (feature != NULL, FALSE);

	if (!item->info) {
		return FALSE;
	}
	
	for (l = item->info->features; l; l = l->next) {
		const char *str;
		
		str = l->data;

		if (strcmp (str, feature) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

