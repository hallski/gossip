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
#include <stdlib.h>  

#include "gossip-jabber-services.h"

#define d(x)


static LmHandlerResult 
jabber_services_get_version_cb (LmMessageHandler   *handler,
				LmConnection       *connection,
				LmMessage          *m,
				GossipCallbackData *data)
{
	GossipVersionInfo     *info;
	GossipVersionCallback  callback;
	LmMessageNode         *query_node, *node;
	LmMessageSubType       type;

	d(g_print ("Services: Received client information\n", 
		   gossip_contact_get_id (contact)));

	callback = data->callback;
	if (!callback) {
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

        /* check for error */
	type = lm_message_get_sub_type (m);

	if (type == LM_MESSAGE_SUB_TYPE_ERROR) {
		GossipResult result = GOSSIP_RESULT_ERROR_INVALID_REPLY;

		node = lm_message_node_get_child (m->node, "error");
		if (node) {
			const gchar *str;
			gint         code;

			str = lm_message_node_get_attribute (node, "code");
			code = str ? atoi (str) : 0;
			
			switch (code) {
			case 404: {
				/* not found */
				d(g_print ("Version: Not found\n"));
				result = GOSSIP_RESULT_ERROR_UNAVAILABLE;
				break;
			}

			case 502: {
				/* service not available */
				d(g_print ("Version: Service not available\n"));
				result = GOSSIP_RESULT_ERROR_UNAVAILABLE;
				break;
			}

			default:
				d(g_print ("Version: Unhandled presence error:%d\n", code));
				result = GOSSIP_RESULT_ERROR_INVALID_REPLY;
				break;
			}
		}

		(callback) (result, 
			    NULL,
			    data->user_data);

		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

	/* no vcard node */
	query_node = lm_message_node_get_child (m->node, "query");
	if (!query_node) {
		(callback) (GOSSIP_RESULT_ERROR_INVALID_REPLY, 
			    NULL,
			    data->user_data);

		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

	info = gossip_version_info_new ();

	node = lm_message_node_get_child (query_node, "name");
	if (node) {
		gossip_version_info_set_name (info, 
					      lm_message_node_get_value (node));
	}
	
	node = lm_message_node_get_child (query_node, "version");
	if (node) {
		gossip_version_info_set_version (info,
						 lm_message_node_get_value (node));
	}
	
	node = lm_message_node_get_child (query_node, "os");
	if (node) {
		gossip_version_info_set_os (info,
					    lm_message_node_get_value (node));
	}
	
	(callback) (GOSSIP_RESULT_OK,
		    info,
		    data->user_data);
	
	g_object_unref (info);
	
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}
 
gboolean
gossip_jabber_services_get_version (LmConnection           *connection,
				    GossipContact          *contact,
				    GossipVersionCallback   callback,
				    gpointer                user_data,
				    GError                **error)
{
	LmMessage          *m;
	LmMessageNode      *node;
	LmMessageHandler   *handler;
	GossipCallbackData *data;
	gboolean            result;
	GossipPresence     *p;
	const gchar        *id, *resource;
	gchar              *jid_str;

	d(g_print ("Services: Requesting client information from contact:'%s'\n", 
		   gossip_contact_get_id (contact)));

	p = gossip_contact_get_active_presence (contact);
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

	data = g_new0 (GossipCallbackData, 1);
	data->callback = callback;
	data->user_data = user_data;
	
	handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_services_get_version_cb,
					  data, g_free);
	
	result = lm_connection_send_with_reply (connection, m, handler, error);


	lm_message_unref (m);
	lm_message_handler_unref (handler);

	return result;
}
