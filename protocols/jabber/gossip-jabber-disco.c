/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2006 Imendio AB 
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
 * Author: Martyn Russell <martyn@imendio.com>
 */

#include <config.h>
#include <string.h>
#include <stdlib.h>

#include <glib/gi18n.h>

#include <loudmouth/loudmouth.h>

#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-utils.h>

#include "gossip-jabber-private.h"
#include "gossip-jabber-disco.h"

#define DEBUG_DOMAIN "JabberDisco"

/* Common XMPP XML namespaces we use. */
#define XMPP_DISCO_ITEMS_XMLNS "http://jabber.org/protocol/disco#items"
#define XMPP_DISCO_INFO_XMLNS  "http://jabber.org/protocol/disco#info"

/* In seconds */
#define DISCO_TIMEOUT      20
#define DISCO_INFO_TIMEOUT 15

typedef struct {
	gchar *category;
	gchar *type;
	gchar *name;
} JabberDiscoIdentity;

typedef struct {
	GList *identities;
	GList *features;
} JabberDiscoInfo;

typedef struct {
	GossipJID *jid;
	gboolean   found;
} FindInfo;

struct _GossipJabberDisco {
	GossipJabber *jabber;
	GossipJID    *to;

	LmMessageHandler *message_handler;

	gpointer      user_data;

	/* Items */
	GossipJabberDiscoItemFunc item_func;

	GList        *items;
	gint          items_remaining;
	gint          items_total;

	/* Flags */
	gboolean      item_lookup;
	gboolean      destroying;

	/* Errors */
	GError       *last_error;

	/* Misc */
	guint         timeout_id;
};

struct _GossipJabberDiscoItem {
	GossipJID *jid;

	gchar     *node;
	gchar     *name;

	guint      timeout_id;

	JabberDiscoInfo *info;
};

static GossipJabberDisco *jabber_disco_new                      (GossipJabber          *jabber);
static void               jabber_disco_init                     (void);
static void               jabber_disco_destroy_items_foreach    (GossipJabberDiscoItem *item,
								 gpointer               user_data);
static void               jabber_disco_destroy_info_foreach     (JabberDiscoInfo       *info,
								 gpointer               user_data);
static void               jabber_disco_destroy_ident_foreach    (JabberDiscoIdentity   *ident,
								 gpointer               user_data);
static gboolean           jabber_disco_find_item_func           (GossipJID             *jid,
								 GossipJabberDisco     *disco,
								 GossipJabberDiscoItem *item);
static void               jabber_disco_set_last_error           (GossipJabberDisco     *disco,
								 LmMessage             *m);
static LmHandlerResult    jabber_disco_message_handler          (LmMessageHandler      *handler,
								 LmConnection          *connection,
								 LmMessage             *m,
								 gpointer               user_data);
static void               jabber_disco_request_items            (GossipJabberDisco     *disco);
static gboolean           jabber_disco_request_info_timeout_cb  (GossipJabberDiscoItem *item);
static gboolean           jabber_disco_request_items_timeout_cb (GossipJabberDisco     *disco);
static void               jabber_disco_handle_items             (GossipJabberDisco     *disco,
								 LmMessage             *m,
								 gpointer               user_data);
static void               jabber_disco_request_info             (GossipJabberDisco     *disco);
static void               jabber_disco_handle_info              (GossipJabberDisco     *disco,
								 LmMessage             *m,
								 gpointer               user_data);

static GHashTable *discos = NULL;

static GossipJabberDisco *
jabber_disco_new (GossipJabber *jabber)
{
	GossipJabberDisco *disco;
	LmConnection      *connection;
	LmMessageHandler  *handler;

	disco = g_new0 (GossipJabberDisco, 1);

	disco->jabber = g_object_ref (jabber);

	connection = gossip_jabber_get_connection (jabber);

	handler = lm_message_handler_new (jabber_disco_message_handler, disco, NULL);
	disco->message_handler = handler;
	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_IQ,
						LM_HANDLER_PRIORITY_NORMAL);

	return disco;
}

static void
jabber_disco_init (void)
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
jabber_disco_destroy_items_foreach (GossipJabberDiscoItem *item,
				    gpointer               user_data)
{
	gossip_jid_unref (item->jid);

	g_free (item->node);
	g_free (item->name);

	if (item->timeout_id) {
		g_source_remove (item->timeout_id);
		item->timeout_id = 0;
	}

	if (item->info) {
		jabber_disco_destroy_info_foreach (item->info, NULL);
	}
}

static void
jabber_disco_destroy_info_foreach (JabberDiscoInfo *info,
				   gpointer         user_data)
{
	g_list_foreach (info->identities, (GFunc)jabber_disco_destroy_ident_foreach, NULL);
	g_list_free (info->identities);

	g_list_foreach (info->features, (GFunc)g_free, NULL);
	g_list_free (info->features);
}

static void
jabber_disco_destroy_ident_foreach (JabberDiscoIdentity *ident,
				    gpointer             user_data)
{
	g_free (ident->category);
	g_free (ident->type);
	g_free (ident->name);
}

static gboolean
jabber_disco_find_item_func (GossipJID             *jid,
			     GossipJabberDisco     *disco,
			     GossipJabberDiscoItem *item)
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
jabber_disco_set_last_error (GossipJabberDisco *disco,
			     LmMessage            *m)
{
	LmMessageNode *node;
	const gchar   *xmlns;
	const gchar   *error_code;
	const gchar   *error_reason = NULL;

	node = lm_message_node_get_child (m->node, "query");
	if (!node) {
		return;
	}

	xmlns = lm_message_node_get_attribute (node, "xmlns");
	if (!xmlns) {
		return;
	}

	if (strcmp (xmlns, XMPP_DISCO_ITEMS_XMLNS) != 0) {
		/* FIXME: currently only handle errors for this namespace */
		return;
	}

	node = lm_message_node_get_child (m->node, "error");
	if (!node) {
		return;
	}

	error_code = lm_message_node_get_attribute (node, "code");
	if (error_code) {
		switch (atoi (error_code)) {
		case 302:
			error_reason = _("Service has gone and is no longer available");
			break;
		case 400:
			error_reason = _("Bad or malformed request to this service");
			break;
		case 401:
			error_reason = _("Unauthorized request to this service");
			break;
		case 402:
			error_reason = _("Payment is required for this service");
			break;
		case 403:
		case 405:
			error_reason = _("This service is forbidden");
			break;
		case 404:
		case 503:
			error_reason = _("This service is unavailable or not found");
			break;
		case 406:
			error_reason = _("Unacceptable request sent to this services");
			break;
		case 407:
			error_reason = _("Registration is required");
			break;
		case 409:
			error_reason = _("There was a conflict of interest trying to use this service");
			break;
		case 500:
			error_reason = _("There was an internal service error");
			break;
		case 501:
			error_reason = _("This feature is not implemented");
			break;
		case 504:
			error_reason = _("The remove service timed out");
			break;
		}
	}

	if (error_code || error_reason) {
		GQuark quark;
		GError *error;

		quark = g_quark_from_string ("gossip-jabber-disco");
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
jabber_disco_message_handler (LmMessageHandler *handler,
			      LmConnection     *connection,
			      LmMessage        *m,
			      gpointer          user_data)
{
	GossipJabberDisco *disco;
	GossipJID         *from_jid;
	const gchar       *from;
	LmMessageNode     *node;
	const char        *xmlns;

	from = lm_message_node_get_attribute (m->node, "from");
	from_jid = gossip_jid_new (from);

	/* used for info look ups */
	disco = g_hash_table_lookup (discos, from_jid);

	/* if not listed under the from jid, try the user data */
	if (!disco) {
		disco = (GossipJabberDisco *) user_data;
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

	if (strcmp (xmlns, XMPP_DISCO_ITEMS_XMLNS) == 0) {
		/* remove timeout: we do this because at this
		   stage, the server has responded */
		if (disco->timeout_id) {
			g_source_remove (disco->timeout_id);
			disco->timeout_id = 0;
		}

		if (lm_message_get_sub_type (m) == LM_MESSAGE_SUB_TYPE_ERROR) {
			jabber_disco_set_last_error (disco, m);

			if (!disco->item_lookup && disco->items_remaining > 0) {
				disco->items_remaining--;
			}

			/* Call callback and inform of last item */
			if (disco->item_func) {
				(disco->item_func) (disco,
						    NULL,
						    disco->items_remaining < 1 ? TRUE : FALSE,
						    FALSE,
						    disco->last_error,
						    disco->user_data);
			}

			if (!disco->item_lookup && disco->items_remaining < 1) {
				gossip_jabber_disco_destroy (disco);
			}
		} else {
			jabber_disco_handle_items (disco, m, user_data);
			jabber_disco_request_info (disco);
		}
	} else if (strcmp (xmlns, XMPP_DISCO_INFO_XMLNS) == 0) {
		jabber_disco_handle_info (disco, m, user_data);
	} else {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
jabber_disco_request_items (GossipJabberDisco *disco)
{
	LmConnection  *connection;
	LmMessage     *m;
	LmMessageNode *node;

	/* create message */
	m = lm_message_new_with_sub_type (gossip_jid_get_full (disco->to),
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_GET);

	gossip_debug (DEBUG_DOMAIN, 
		      "disco items to: %s",
		      gossip_jid_get_full (disco->to));

	connection = gossip_jabber_get_connection (disco->jabber);

	lm_message_node_add_child (m->node, "query", NULL);
	node = lm_message_node_get_child (m->node, "query");

	lm_message_node_set_attribute (node, "xmlns", XMPP_DISCO_ITEMS_XMLNS);

	lm_connection_send (connection, m, NULL);
	lm_message_unref (m);
}

static gboolean
jabber_disco_request_items_timeout_cb (GossipJabberDisco *disco)
{
	gossip_debug (DEBUG_DOMAIN, 
		      "disco items to:'%s' have timed out after %d seconds, cleaning up...",
		      gossip_jid_get_full (disco->to), DISCO_TIMEOUT);

	/* Stop timeout */
	disco->timeout_id = 0;

	/* Call callback and inform of last item */
	if (disco->item_func) {
		(disco->item_func) (disco,
				    NULL,
				    FALSE,
				    TRUE,
				    NULL,
				    disco->user_data);
	}

	/* Remove disco */
	gossip_jabber_disco_destroy (disco);

	return FALSE;
}

static gboolean
jabber_disco_request_info_timeout_cb (GossipJabberDiscoItem *item)
{
	GossipJabberDisco *disco;

	disco = g_hash_table_find (discos, (GHRFunc)jabber_disco_find_item_func, item);
	if (!disco) {
		return FALSE;
	}

	gossip_debug (DEBUG_DOMAIN, 
		      "disco info to:'%s' has timed out after %d seconds, cleaning up...",
		      gossip_jid_get_full (disco->to), DISCO_INFO_TIMEOUT);

	if (!disco->item_lookup) {
		disco->items_remaining--;
	}

	/* Stop timeout */
	item->timeout_id = 0;

	/* Call callback and inform of last item */
	if (disco->item_func) {
		(disco->item_func) (disco,
				    item,
				    disco->items_remaining < 1 ? TRUE : FALSE,
				    TRUE,
				    NULL,
				    disco->user_data);
	}

	if (!disco->item_lookup && disco->items_remaining < 1) {
		gossip_jabber_disco_destroy (disco);
	}

	return FALSE;
}

static void
jabber_disco_handle_items (GossipJabberDisco *disco,
			   LmMessage         *m,
			   gpointer           user_data)
{
	GossipJabberDiscoItem *item;
	const char               *jid_str;
	const char               *node_str;
	const char               *name_str;
	LmMessageNode            *node;

	node = lm_message_node_find_child (m->node, "item");

	while (node) {
		item = g_new0 (GossipJabberDiscoItem, 1);

		jid_str = lm_message_node_get_attribute (node, "jid");
		item->jid = gossip_jid_new (jid_str);

		node_str = lm_message_node_get_attribute (node, "node");
		item->node = g_strdup (node_str);

		name_str = lm_message_node_get_attribute (node, "name");
		item->name = g_strdup (name_str);

		disco->items = g_list_append (disco->items, item);

		gossip_debug (DEBUG_DOMAIN, 
			      "disco item - jid:'%s', node:'%s', name:'%s'",
			      jid_str, node_str, name_str);

		/* Go to next item */
		node = node->next;
	}
}

static void
jabber_disco_request_info (GossipJabberDisco *disco)
{
	LmConnection *connection;
	GList        *l;
	GossipJID    *jid;

	connection = gossip_jabber_get_connection (disco->jabber);
	jid = gossip_jid_new ("users.jabber.org");

	disco->items_total = disco->items_remaining = g_list_length (disco->items);


	for (l = disco->items; l; l = l->next) {
		GossipJabberDiscoItem *item;
		LmMessage                *m;
		LmMessageNode            *node;

		item = l->data;

		/* NOTE: This is a temporary measure to ignore the
		 * users.jabber.org JID because it never seems to
		 * respond.  If this code is uncommented it will treat
		 * it as any other JID. 
		 */
		if (gossip_jid_equals (item->jid, jid)) {
			gossip_debug (DEBUG_DOMAIN, 
				      "ignoring JID:'users.jabber.org', it doesn't tend to respond");
			disco->items_remaining--;
			continue;
		}

		/* Create message */
		m = lm_message_new_with_sub_type (gossip_jid_get_full (item->jid),
						  LM_MESSAGE_TYPE_IQ,
						  LM_MESSAGE_SUB_TYPE_GET);

		gossip_debug (DEBUG_DOMAIN, 
			      "disco info to: %s",
			      gossip_jid_get_full (item->jid));

		/* Start timeout */
		item->timeout_id = g_timeout_add (DISCO_INFO_TIMEOUT * 1000,
						  (GSourceFunc) jabber_disco_request_info_timeout_cb,
						  item);


		lm_message_node_add_child (m->node, "query", NULL);
		node = lm_message_node_get_child (m->node, "query");

		lm_message_node_set_attribute (node, "xmlns", XMPP_DISCO_INFO_XMLNS);

		lm_connection_send (connection, m, NULL);
		lm_message_unref (m);
	}

	gossip_jid_unref (jid);
}

static void
jabber_disco_handle_info (GossipJabberDisco *disco,
			  LmMessage         *m,
			  gpointer           user_data)
{
	LmMessageNode         *node;
	GossipJabberDiscoItem *item;
	JabberDiscoInfo       *info;
	GList                 *l;
	const char            *from;
	const char            *category;
	const char            *type;
	const char            *name;
	const char            *feature;

	if (!disco->item_lookup) {
		disco->items_remaining--;
	}

	from = lm_message_node_get_attribute (m->node, "from");
	gossip_debug (DEBUG_DOMAIN, 
		      "sorting disco info for:'%s'....", 
		      from);

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
		/* This usually occurs when a message from a previous
		 * request has been received and the data is no longer
		 * relevant.
		 */
		return;
	}

	info = g_new0 (JabberDiscoInfo, 1);

	node = lm_message_node_find_child (m->node, "identity");

	while (node && strcmp (node->name, "identity") == 0) {
		JabberDiscoIdentity *ident;

		ident = g_new0 (JabberDiscoIdentity, 1);

		category = lm_message_node_get_attribute (node, "category");
		ident->category = g_strdup (category);

		type = lm_message_node_get_attribute (node, "type");
		ident->type = g_strdup (type);

		name = lm_message_node_get_attribute (node, "name");
		ident->name = g_strdup (name);

		gossip_debug (DEBUG_DOMAIN, 
			      "disco item - category:'%s', type:'%s', name:'%s'",
			      category, type, name);

		info->identities = g_list_append (info->identities, ident);

		node = node->next;
	}

	node = lm_message_node_find_child (m->node, "feature");

	while (node && strcmp (node->name, "feature") == 0) {
		feature = lm_message_node_get_attribute (node, "var");

		gossip_debug (DEBUG_DOMAIN, 
			      "disco item - feature:'%s'", 
			      feature);

		info->features = g_list_append (info->features, g_strdup (feature));

		node = node->next;
	}

	item->info = info;

	/* Remove timeout */
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
		gossip_jabber_disco_destroy (disco);
	}
}

GossipJabberDisco *
gossip_jabber_disco_request (GossipJabber              *jabber,
			     const char                *to,
			     GossipJabberDiscoItemFunc  item_func,
			     gpointer                   user_data)
{
	GossipJabberDisco *disco;
	GossipJID            *jid;

	g_return_val_if_fail (jabber != NULL, NULL);
	g_return_val_if_fail (to != NULL, NULL);
	g_return_val_if_fail (item_func != NULL, NULL);

	jabber_disco_init ();

	jid = gossip_jid_new (to);

	disco = g_hash_table_lookup (discos, jid);

	if (disco) {
		return disco;
	}

	disco = jabber_disco_new (jabber);
	g_hash_table_insert (discos, gossip_jid_ref (jid), disco);

	disco->to = jid;

	disco->item_func = item_func;
	disco->user_data = user_data;

	disco->items_remaining = 1;
	disco->items_total = 1;

	/* Start timeout */
	disco->timeout_id = g_timeout_add (DISCO_TIMEOUT * 1000,
					   (GSourceFunc) jabber_disco_request_items_timeout_cb,
					   disco);

	/* Send initial request */
	jabber_disco_request_items (disco);

	return disco;
}

void
gossip_jabber_disco_destroy (GossipJabberDisco *disco)
{
	LmConnection     *connection;
	LmMessageHandler *handler;

	/* We don't mind if NULL is supplied, an error message is
	 * unnecessary. 
	 */
	if (!disco || disco->destroying) {
		return;
	}

	disco->destroying = TRUE;

	connection = gossip_jabber_get_connection (disco->jabber);

	handler = disco->message_handler;
	if (handler) {
		lm_connection_unregister_message_handler (connection,
							  handler,
							  LM_MESSAGE_TYPE_IQ);
		lm_message_handler_unref (handler);
	}

	g_list_foreach (disco->items, (GFunc)jabber_disco_destroy_items_foreach, NULL);
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

GossipJabberDisco *
gossip_jabber_disco_request_info (GossipJabber              *jabber,
				  const char                *to,
				  GossipJabberDiscoItemFunc  item_func,
				  gpointer                   user_data)
{
	GossipJabberDisco     *disco;
	GossipJabberDiscoItem *item;
	GossipJID             *jid;
	LmConnection          *connection;
	LmMessageHandler      *handler;

	g_return_val_if_fail (jabber != NULL, NULL);
	g_return_val_if_fail (to != NULL, NULL);
	g_return_val_if_fail (item_func != NULL, NULL);

	jabber_disco_init ();

	jid = gossip_jid_new (to);

	disco = g_hash_table_lookup (discos, jid);

	if (disco) {
		gossip_jid_unref (jid);
		return disco;
	}

	/* Create disco */
	disco = g_new0 (GossipJabberDisco, 1);

	disco->jabber = g_object_ref (jabber);

	/* Set up handler */
	connection = gossip_jabber_get_connection (jabber);

	handler = lm_message_handler_new (jabber_disco_message_handler, disco, NULL);
	disco->message_handler = handler;
	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_IQ,
						LM_HANDLER_PRIORITY_NORMAL);

	/* Add disco and configure members */
	g_hash_table_insert (discos, gossip_jid_ref (jid), disco);

	disco->to = jid;

	disco->item_lookup = TRUE;

	disco->item_func = item_func;
	disco->user_data = user_data;

	disco->items_remaining = 1;
	disco->items_total = 1;

	/* Add item */
	item = g_new0 (GossipJabberDiscoItem, 1);

	item->jid = gossip_jid_ref (jid);

	disco->items = g_list_append (disco->items, item);

	/* Send request for info */
	jabber_disco_request_info (disco);

	return disco;
}

GList *
gossip_jabber_disco_get_category (GossipJabberDisco *disco,
				  const char        *category)
{
	GList *l1;
	GList *services;

	g_return_val_if_fail (disco != NULL, NULL);
	g_return_val_if_fail (category != NULL, NULL);

	services = NULL;

	for (l1 = disco->items; l1; l1 = l1->next) {
		GossipJabberDiscoItem *item;
		JabberDiscoInfo       *info;
		gboolean               have_category;
		gboolean               can_register;
		GList                 *l2;

		item = l1->data;
		info = item->info;

		if (!info) {
			continue;
		}

		have_category = FALSE;
		for (l2 = info->identities; l2; l2 = l2->next) {
			JabberDiscoIdentity *ident;

			ident = l2->data;

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
gossip_jabber_disco_get_category_and_type (GossipJabberDisco *disco,
					   const gchar       *category,
					   const gchar       *type)
{
	GList *l1;
	GList *services;

	g_return_val_if_fail (disco != NULL, NULL);
	g_return_val_if_fail (category != NULL, NULL);
	g_return_val_if_fail (type != NULL, NULL);

	services = NULL;
	for (l1 = disco->items; l1; l1 = l1->next) {
		GossipJabberDiscoItem *item;
		JabberDiscoInfo       *info;
		GList                 *l2;
		gboolean               have_category; /* e.g. gateway */
		gboolean               have_type;     /* e.g. icq or msn, etc */
		gboolean               can_register;  /* server allows registration */

		item = l1->data;
		info = item->info;

		if (!info) {
			continue;
		}

		have_category = FALSE;
		have_type     = FALSE;
		can_register  = FALSE;

		for (l2 = info->identities; l2; l2 = l2->next) {
			JabberDiscoIdentity *ident;

			ident = l2->data;

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
			const gchar *features;

			features = l2->data;

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
gossip_jabber_disco_get_items_remaining (GossipJabberDisco *disco)
{
	g_return_val_if_fail (disco != NULL, -1);

	return disco->items_remaining;
}

gint
gossip_jabber_disco_get_items_total (GossipJabberDisco *disco)
{
	g_return_val_if_fail (disco != NULL, -1);

	return disco->items_total;
}

GossipJabberDiscoItem *
gossip_jabber_disco_get_item (GossipJabberDisco *disco,
			      GossipJID         *jid)
{
	GList *l;

	g_return_val_if_fail (disco != NULL, NULL);

	for (l = disco->items; l; l = l->next) {
		GossipJabberDiscoItem *item;

		item = l->data;

		if (gossip_jid_equals (item->jid, jid)) {
			return item;
		}
	}

	return NULL;
}

GossipJID *
gossip_jabber_disco_item_get_jid (GossipJabberDiscoItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);

	return item->jid;
}

const char *
gossip_jabber_disco_item_get_name (GossipJabberDiscoItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);

	return item->name;
}

const gchar *
gossip_jabber_disco_item_get_type (GossipJabberDiscoItem *item)
{
	GList       *l;
	const gchar *type = NULL;

	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (item->info != NULL, FALSE);

	for (l = item->info->identities; l; l = l->next) {
		JabberDiscoIdentity *ident;

		ident = l->data;

		/* If more than one type, we leave - this function is
		 * really for requests on a per item basis, if a FULL
		 * request for all services is asked, then there will
		 * be types for each item.
		 */
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
gossip_jabber_disco_item_get_features (GossipJabberDiscoItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);
	g_return_val_if_fail (item->info != NULL, NULL);

	return item->info->features;
}

gboolean
gossip_jabber_disco_item_has_category (GossipJabberDiscoItem *item,
				       const gchar           *category)
{
	GList *l;

	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (item->info != NULL, FALSE);
	g_return_val_if_fail (category != NULL, FALSE);

	for (l = item->info->identities; l; l = l->next) {
		JabberDiscoIdentity *ident;

		ident = l->data;

		if (strcmp (ident->category, category) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

gboolean
gossip_jabber_disco_item_has_type (GossipJabberDiscoItem *item,
				   const gchar            *type)
{
	GList *l;

	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (item->info != NULL, FALSE);
	g_return_val_if_fail (type != NULL, FALSE);

	for (l = item->info->identities; l; l = l->next) {
		JabberDiscoIdentity *ident;

		ident = l->data;

		if (strcmp (ident->type, type) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

gboolean
gossip_jabber_disco_item_has_feature (GossipJabberDiscoItem *item,
				      const gchar           *feature)
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

