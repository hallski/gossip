/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio AB
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

#include "gossip-version-info.h"
#include "gossip-time.h"
#include "gossip-jabber-helper.h"

typedef struct {
	gpointer callback;
	gpointer user_data;
} AsyncCallbackData;

static LmHandlerResult 
jabber_helper_async_get_vcard_cb (LmMessageHandler *handler,
				  LmConnection     *connection,
				  LmMessage        *m,
				  AsyncCallbackData   *data)
{
	GossipVCard   *vcard;
	LmMessageNode *top_node, *node;

	if (!data->callback) {
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

	top_node = lm_message_node_get_child (m->node, "vCard");
	if (!top_node) {
		((GossipAsyncVCardCallback) data->callback) (GOSSIP_ASYNC_ERROR_INVALID_REPLY, NULL,
				  data->user_data);
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}
	
	vcard = gossip_vcard_new ();
	
	node = lm_message_node_get_child (top_node, "FN");
	if (node) {
		gossip_vcard_set_name (vcard, node->value);
	}

	node = lm_message_node_get_child (top_node, "NICKNAME");
	if (node) {
		gossip_vcard_set_nickname (vcard, node->value);
	}

	node = lm_message_node_get_child (top_node, "EMAIL");
	if (node) {
		gchar *email = NULL;

		if (node->value) {
			email = node->value;
		} else {
			node = lm_message_node_get_child (node, "USERID");
			if (node) {
				email = node->value;
			}
		}
			
		gossip_vcard_set_email (vcard, email);
	}

	node = lm_message_node_get_child (top_node, "URL");
	if (node) {
		gossip_vcard_set_url (vcard, node->value);
	}

	node = lm_message_node_get_child (top_node, "DESC");
	if (node) {
		gossip_vcard_set_description (vcard, node->value);
	}

	((GossipAsyncVCardCallback)data->callback) (GOSSIP_ASYNC_OK, vcard, data->user_data);

	g_object_unref (vcard);

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

gboolean
gossip_jabber_helper_async_get_vcard (LmConnection              *connection,
				      const gchar               *jid_str, 
				      GossipAsyncVCardCallback   callback,
				      gpointer                   user_data,
				      GError                   **error)
{
	LmMessage         *m;
	LmMessageNode     *node;
	LmMessageHandler  *handler;
	AsyncCallbackData *data;

	m = lm_message_new (jid_str, LM_MESSAGE_TYPE_IQ);
	node = lm_message_node_add_child (m->node, "vCard", NULL);
	lm_message_node_set_attribute (node, "xmlns", "vcard-temp");

	data = g_new0 (AsyncCallbackData, 1);
	data->callback = callback;
	data->user_data = user_data;
	
	handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_helper_async_get_vcard_cb,
					  data, g_free);
	
	if (!lm_connection_send_with_reply (connection, m, 
					    handler, error)) {
		lm_message_unref (m);
		lm_message_handler_unref (handler);
		return FALSE;
	}

	/* FIXME: Set a timeout */
	
	lm_message_handler_unref (handler);

	return TRUE;
}

static LmHandlerResult 
jabber_helper_async_set_vcard_cb (LmMessageHandler  *handler,
				  LmConnection      *connection,
				  LmMessage         *m,
				  AsyncCallbackData *data)
{
	/* FIXME: Error checking and reply error if failed */
	((GossipAsyncResultCallback )data->callback) (GOSSIP_ASYNC_OK, data->user_data);
	
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

gboolean
gossip_jabber_helper_async_set_vcard (LmConnection               *connection,
				      GossipVCard                *vcard,
				      GossipAsyncResultCallback   callback,
				      gpointer                    user_data,
				      GError                    **error)
{
	LmMessage         *m;
	LmMessageNode     *node;
	LmMessageHandler  *handler;
	AsyncCallbackData *data;
	gboolean           result;

	m = lm_message_new_with_sub_type (NULL,
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_SET);
	node = lm_message_node_add_child (m->node, "vCard", NULL);
	lm_message_node_set_attribute (node, "xmlns", "vcard-temp");

	lm_message_node_add_child (node, "FN", gossip_vcard_get_name (vcard));
	lm_message_node_add_child (node, "NICKNAME", 
				   gossip_vcard_get_nickname (vcard));
	lm_message_node_add_child (node, "URL", gossip_vcard_get_url (vcard));
	lm_message_node_add_child (node, "EMAIL",
				   gossip_vcard_get_email (vcard));
	lm_message_node_add_child (node, "DESC", 
				   gossip_vcard_get_description (vcard));
	
	data = g_new0 (AsyncCallbackData, 1);
	data->callback = callback;
	data->user_data = user_data;
	
	handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_helper_async_set_vcard_cb,
					  data, g_free);
 
	result = lm_connection_send_with_reply (connection, m, handler, error);
	
	lm_message_unref (m);
	lm_message_handler_unref (handler);

	return result;
}

static LmHandlerResult 
jabber_helper_async_get_version_cb (LmMessageHandler  *handler,
				    LmConnection      *connection,
				    LmMessage         *m,
				    AsyncCallbackData *data)
{
	GossipVersionInfo *info;
	LmMessageNode     *query, *node;

	query = lm_message_node_get_child (m->node, "query");
	if (!query) {
		((GossipAsyncVersionCallback )data->callback) (GOSSIP_ASYNC_ERROR_INVALID_REPLY,
							       NULL,
							       data->user_data);
			
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

	info = gossip_version_info_new ();

	node = lm_message_node_get_child (query, "name");
	if (node) {
		gossip_version_info_set_name (info, 
					      lm_message_node_get_value (node));
	}

	node = lm_message_node_get_child (query, "version");
	if (node) {
		gossip_version_info_set_version (info,
						 lm_message_node_get_value (node));
	}

	node = lm_message_node_get_child (query, "os");
	if (node) {
		gossip_version_info_set_os (info,
					    lm_message_node_get_value (node));
	}

	((GossipAsyncVersionCallback )data->callback) (GOSSIP_ASYNC_OK,
						       info,
						       data->user_data);

	g_object_unref (info);
	
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

gboolean
gossip_jabber_helper_async_get_version (LmConnection                *connection,
					GossipContact               *contact,
					GossipAsyncVersionCallback   callback,
					gpointer                     user_data,
					GError                     **error)
{
	LmMessage         *m;
	LmMessageNode     *node;
	LmMessageHandler  *handler;
	AsyncCallbackData *data;
	gboolean           result;
	GossipPresence    *p;
	const gchar       *id, *resource;
	gchar             *jid_str;

	p = gossip_contact_get_presence (contact);
	resource = gossip_presence_get_resource (p);
	id = gossip_contact_get_id (contact);

	if (resource && strcmp (resource, "") != 0) {
		jid_str = g_strdup_printf ("%s/%s", id, resource);
	} else {
		jid_str = (gchar *) id;
	}

	m = lm_message_new (jid_str, LM_MESSAGE_TYPE_IQ);

	if (jid_str != id) {
		g_free (jid_str);
	}

	node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attribute (node, "xmlns", "jabber:iq:version");

	data = g_new0 (AsyncCallbackData, 1);
	data->callback = callback;
	data->user_data = user_data;
	
	handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_helper_async_get_version_cb,
					  data, g_free);
	
	result = lm_connection_send_with_reply (connection, m, handler, error);


	lm_message_unref (m);
	lm_message_handler_unref (handler);

	return result;
}

const gchar * 
gossip_jabber_helper_presence_state_to_string (GossipPresence *presence)
{
	switch (gossip_presence_get_state (presence)) {
	case GOSSIP_PRESENCE_STATE_BUSY:
		return "dnd";
	case GOSSIP_PRESENCE_STATE_AWAY:
		return "away";
	case GOSSIP_PRESENCE_STATE_EXT_AWAY:
		return "xa";
	default:
		return NULL;
	}

	return NULL;
}

GossipPresenceState
gossip_jabber_helper_presence_state_from_str (const gchar *str)
{
	if (!str || !str[0]) {
		return GOSSIP_PRESENCE_STATE_AVAILABLE;
	}
	else if (strcmp (str, "dnd") == 0) {
		return GOSSIP_PRESENCE_STATE_BUSY;
	}
	else if (strcmp (str, "away") == 0) {
		return GOSSIP_PRESENCE_STATE_AWAY;
	}
	else if (strcmp (str, "xa") == 0) {
		return GOSSIP_PRESENCE_STATE_EXT_AWAY;
	}
	else if (strcmp (str, "chat") == 0) {
		/* We treat this as available */
		return GOSSIP_PRESENCE_STATE_AVAILABLE;
	}

	return GOSSIP_PRESENCE_STATE_AVAILABLE;
}

gossip_time_t
gossip_jabber_helper_get_timestamp_from_lm_message (LmMessage *m)
{
	LmMessageNode *node;
	const gchar   *timestamp_s = NULL;
	const gchar   *xmlns;
	struct tm     *tm;
	
	for (node = m->node->children; node; node = node->next) {
		if (strcmp (node->name, "x") == 0) {
			xmlns = lm_message_node_get_attribute (node, "xmlns");
			if (xmlns && strcmp (xmlns, "jabber:x:delay") == 0) {
                                timestamp_s = lm_message_node_get_attribute 
					(node, "stamp");
                        }
                }
        }

	if (!timestamp_s) {
		return gossip_time_get_current ();
	} 
	
	tm = lm_utils_get_localtime (timestamp_s);

	return gossip_time_from_tm (tm);
}

