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

#include <stdlib.h>
#include <string.h>

#include "gossip-app.h"
#include "gossip-jid.h"
#include "gossip-jabber-helper.h"
#include "gossip-jabber-chatrooms.h"

#define JOIN_MSG_ID_PREFIX "gc_join_"

#define d(x) 

struct _GossipJabberChatrooms {
	GossipJabber *jabber;
	LmConnection *connection;

	GHashTable   *room_id_hash;
	GHashTable   *room_jid_hash;
};

typedef struct {
	gint       id;
	GossipJID *jid;

	GSList    *contacts;
} JabberChatroom;

typedef struct {
	GossipJabberChatrooms *chatrooms;
        gpointer               callback;
        gpointer               user_data;
} AsyncCallbackData;

static JabberChatroom * jabber_chatrooms_chatroom_new  (const gchar    *room_name,
							const gchar    *server,
							const gchar    *nick);
static void             jabber_chatrooms_chatroom_free (JabberChatroom *room);
static LmHandlerResult
jabber_chatrooms_message_handler     (LmMessageHandler      *handler,
				      LmConnection          *conn,
				      LmMessage             *message,
				      GossipJabberChatrooms *chatrooms);
static LmHandlerResult
jabber_chatrooms_presence_handler    (LmMessageHandler      *handler,
				      LmConnection          *conn,
				      LmMessage             *message,
				      GossipJabberChatrooms *chatrooms);

static LmHandlerResult jabber_chatrooms_join_cb (LmMessageHandler  *handler,
						 LmConnection      *connection,
						 LmMessage         *message,
						 AsyncCallbackData *data);
static GossipContact * jabber_chatrooms_get_contact (JabberChatroom *room,
						     GossipJID      *jid);

static JabberChatroom *
jabber_chatrooms_chatroom_new (const gchar *room_name, 
			       const gchar *server,
			       const gchar *nick)
{
	JabberChatroom *room;
	gchar          *jid_str;
	static int      id = 1;

	room = g_new0 (JabberChatroom, 1);

	room->id = id++;

	jid_str = g_strdup_printf ("%s@%s/%s", room_name, server, nick);
	room->jid = gossip_jid_new (jid_str);
	g_free (jid_str);

	room->contacts = NULL;

	return room;
}

static void
jabber_chatrooms_chatroom_free (JabberChatroom *room)
{
	GSList *l;
	
	gossip_jid_unref (room->jid);

	for (l = room->contacts; l; l = l->next) {
		g_object_unref (l->data);
	}

	g_slist_free (room->contacts);
	g_free (room);
}

static LmHandlerResult
jabber_chatrooms_message_handler (LmMessageHandler      *handler,
				  LmConnection          *conn,
				  LmMessage             *m,
				  GossipJabberChatrooms *chatrooms)
{
	const gchar    *from;
	GossipJID      *jid;
	JabberChatroom *room;
	GossipMessage  *message;
	GossipContact  *contact;
	LmMessageNode  *node;

	if (lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_GROUPCHAT) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	from = lm_message_node_get_attribute (m->node, "from");
	jid = gossip_jid_new (from);

	room = g_hash_table_lookup (chatrooms->room_jid_hash, jid);
	
	if (!room) {
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}
	
	contact = jabber_chatrooms_get_contact (room, jid);

	node = lm_message_node_get_child (m->node, "body");
	if (node) {
		gossip_time_t timestamp;

		d(g_print ("Emitting\n"));
		message = gossip_message_new (GOSSIP_MESSAGE_TYPE_CHAT_ROOM,
					      gossip_jabber_get_own_contact (chatrooms->jabber));

		timestamp = gossip_jabber_helper_get_timestamp_from_lm_message (m);
		gossip_message_set_timestamp (message, timestamp);

		gossip_message_set_sender (message, contact);
		gossip_message_set_body (message, node->value);
	
		g_signal_emit_by_name (chatrooms->jabber,
				       "chatroom-new-message", 
				       room->id, message);

		g_object_unref (message);
	}
	
	gossip_jid_unref (jid);
	 
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;

}
static LmHandlerResult
jabber_chatrooms_presence_handler (LmMessageHandler      *handler,
				   LmConnection          *conn,
				   LmMessage             *message,
				   GossipJabberChatrooms *chatrooms)
{
	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static LmHandlerResult
jabber_chatrooms_join_cb (LmMessageHandler  *handler,
                          LmConnection      *connection,
                          LmMessage         *m,
                          AsyncCallbackData *data)
{
	GossipJabberChatrooms *chatrooms;
	const gchar           *id_str;
        gint                   id;
	gchar                 *int_str;
        /* Check if message == ERROR */

	chatrooms = data->chatrooms;

	id_str = lm_message_node_get_attribute (m->node, "id");
	if (!id_str) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	if (strlen (id_str) < strlen (JOIN_MSG_ID_PREFIX)) {
		g_warning ("Wrong id string when joining chatroom");
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	int_str = g_strdup (id_str + strlen (JOIN_MSG_ID_PREFIX));
	id = atoi (int_str);
	g_free (int_str);
	
	((GossipJoinChatroomCb)data->callback) (GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
                                                GOSSIP_JOIN_CHATROOM_OK,
                                                id,
                                                data->user_data); 

	
        return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static GossipContact *
jabber_chatrooms_get_contact (JabberChatroom *room, GossipJID *jid)
{
	GSList        *l;
	GossipContact *c;
	const gchar   *id;

	id = gossip_jid_get_full (jid);

	for (l = room->contacts; l; l = l->next) {
		c = GOSSIP_CONTACT (l->data);

		if (g_ascii_strcasecmp (gossip_contact_get_id (c), id) == 0) {
			return c;
		}
	}

	c = gossip_contact_new_full (GOSSIP_CONTACT_TYPE_CHATROOM, id, 
				     gossip_jid_get_resource (jid));
	room->contacts = g_slist_prepend (room->contacts, c);

	return c;
}

GossipJabberChatrooms *
gossip_jabber_chatrooms_new (GossipJabber *jabber, LmConnection *connection)
{
	GossipJabberChatrooms *chatrooms;
	LmMessageHandler      *handler;

	chatrooms = g_new0 (GossipJabberChatrooms, 1);
	
	chatrooms->jabber     = jabber;
	chatrooms->connection = connection;
	chatrooms->room_id_hash = g_hash_table_new (NULL, NULL);
	chatrooms->room_jid_hash = g_hash_table_new_full (gossip_jid_hash,
							  gossip_jid_equal,
							  (GDestroyNotify) gossip_jid_unref,
							  NULL);

	handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_chatrooms_message_handler,
					  chatrooms, NULL);

	lm_connection_register_message_handler (connection,
						handler, 
						LM_MESSAGE_TYPE_MESSAGE,
						LM_HANDLER_PRIORITY_NORMAL);
	lm_message_handler_unref (handler);
	
	handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_chatrooms_presence_handler,
					  chatrooms, NULL);

	lm_connection_register_message_handler (connection,
						handler, 
						LM_MESSAGE_TYPE_PRESENCE,
						LM_HANDLER_PRIORITY_NORMAL);
	lm_message_handler_unref (handler);

	return chatrooms;
}	

void
gossip_jabber_chatrooms_free (GossipJabberChatrooms *chatrooms)
{
	g_hash_table_destroy (chatrooms->room_id_hash);
	g_hash_table_destroy (chatrooms->room_jid_hash);

	g_free (chatrooms);
}

void
gossip_jabber_chatrooms_join (GossipJabberChatrooms *chatrooms,
			      GossipPresence        *presence,
			      const gchar           *room_name,
			      const gchar           *server,
			      const gchar           *nick,
			      const gchar           *password,
			      GossipJoinChatroomCb   callback,
			      gpointer               user_data)
{
	LmMessage         *m;
	const gchar       *show = NULL;
        AsyncCallbackData *data;
        LmMessageHandler  *handler;
        gchar             *id_str;
	JabberChatroom    *room;
	
        d(g_print ("Join chatroom: %s\n", room_name));
	
	room = jabber_chatrooms_chatroom_new (room_name, server, nick);

	m = lm_message_new_with_sub_type (gossip_jid_get_full (room->jid),
					  LM_MESSAGE_TYPE_PRESENCE,
					  
					  LM_MESSAGE_SUB_TYPE_AVAILABLE);

	g_hash_table_insert (chatrooms->room_id_hash, 
			     GINT_TO_POINTER (room->id), room);
	g_hash_table_insert (chatrooms->room_jid_hash, room->jid, room);

        show = gossip_jabber_helper_presence_state_to_string (presence);

	if (show) {
                lm_message_node_add_child (m->node, "show", show);
	}

        id_str = g_strdup_printf (JOIN_MSG_ID_PREFIX "%d", room->id);
        lm_message_node_set_attribute (m->node, "id", id_str);
        g_free (id_str);

        data = g_new0 (AsyncCallbackData, 1);

	data->chatrooms = chatrooms;
        data->callback = callback;
        data->user_data = user_data;

        handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_chatrooms_join_cb,
                                          data, g_free);
        
	lm_connection_send_with_reply (chatrooms->connection, m, 
                                       handler, NULL);
	lm_message_unref (m);
        lm_message_handler_unref (handler);
}

void 
gossip_jabber_chatrooms_send (GossipJabberChatrooms *chatrooms, 
			      GossipChatroomId       id,
			      const gchar           *message)
{
	LmMessage      *m;
	JabberChatroom *room;

	room = (JabberChatroom *) g_hash_table_lookup (chatrooms->room_id_hash, 
						       GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("Unknown chatroom id: %d", id);
		return;
	}
	
	d(g_print ("Send message to chatroom: %d\n", id));
	
	m = lm_message_new_with_sub_type (gossip_jid_get_without_resource (room->jid),
					  LM_MESSAGE_TYPE_MESSAGE,
					  LM_MESSAGE_SUB_TYPE_GROUPCHAT);
	lm_message_node_add_child (m->node, "body", message);

	lm_connection_send (chatrooms->connection, m, NULL);
	lm_message_unref (m);
}

void
gossip_jabber_chatrooms_set_title (GossipJabberChatrooms *chatrooms,
				   GossipChatroomId       id,
				   const gchar           *new_title)
{
	JabberChatroom *room;
	const gchar    *without_resource;
	LmMessage      *m;

	room = (JabberChatroom *) g_hash_table_lookup (chatrooms->room_id_hash, 
						       GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("Unknown chatroom id: %d", id);
		return;
	}
	d(g_print ("Set chatroom title: %s\n", new_title));
	
	without_resource = gossip_jid_get_without_resource (room->jid);
	
	m = lm_message_new_with_sub_type (without_resource,
					  LM_MESSAGE_TYPE_MESSAGE,
					  LM_MESSAGE_SUB_TYPE_GROUPCHAT);
	
	lm_message_node_add_child (m->node, "subject", new_title);

	lm_connection_send (chatrooms->connection, m, NULL);
	lm_message_unref (m);
}

void 
gossip_jabber_chatrooms_change_nick (GossipJabberChatrooms *chatrooms,
				     GossipChatroomId       id,
				     const gchar           *new_nick)
{
	LmMessage      *m;
	JabberChatroom *room;

	room = (JabberChatroom *) g_hash_table_lookup (chatrooms->room_id_hash, 
						       GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("Unknown chatroom id: %d", id);
		return;
	}

	d(g_print ("Change chatroom nick: %s\n", new_nick));

	gossip_jid_set_resource (room->jid, new_nick);
	
	m = lm_message_new (gossip_jid_get_full (room->jid),
			    LM_MESSAGE_TYPE_PRESENCE);
	
	lm_connection_send (chatrooms->connection, m, NULL);
	lm_message_unref (m);
}					 

void 
gossip_jabber_chatrooms_leave (GossipJabberChatrooms *chatrooms,
			       GossipChatroomId       id)
{
	LmMessage      *m;
	JabberChatroom *room;

	room = (JabberChatroom *) g_hash_table_lookup (chatrooms->room_id_hash, 
						       GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("Unknown chatroom id: %d", id);
		return;
	}

	d(g_print ("Leave chatroom: %d\n", id));

	m = lm_message_new_with_sub_type (gossip_jid_get_full (room->jid),
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_UNAVAILABLE);

	lm_connection_send (chatrooms->connection, m, NULL);
	lm_message_unref (m);

	g_hash_table_remove (chatrooms->room_id_hash, 
			     GINT_TO_POINTER (room->id));
	g_hash_table_remove (chatrooms->room_jid_hash, room->jid);
	jabber_chatrooms_chatroom_free (room);
}

GossipContact * 
gossip_jabber_chatrooms_get_contact (GossipJabberChatrooms *chatrooms,
				     LmMessage             *message,
				     gint                  *chat_id)
{
	return NULL;
}
