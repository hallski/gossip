/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2005 Imendio AB
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

#include <glib/gi18n.h>

#include <libgossip/gossip-utils.h>

#include "gossip-jid.h"
#include "gossip-jabber-helper.h"
#include "gossip-jabber-chatrooms.h"

#define d(x) 

#define JOIN_MSG_ID_PREFIX "gc_join_"

struct _GossipJabberChatrooms {
	GossipJabber   *jabber;
	GossipPresence *presence;
	LmConnection   *connection;

	GHashTable     *room_id_hash;
	GHashTable     *room_jid_hash;
};

typedef struct {
	gint           id;
	GossipJID     *jid;
	gchar         *name;

	GossipContact *own_contact;
	
	GSList        *contacts;
} JabberChatroom;


typedef struct {
	GossipJabberChatrooms *chatrooms;
        gpointer               callback;
        gpointer               user_data;
} AsyncCallbackData;

static JabberChatroom *jabber_chatrooms_chatroom_new         (const gchar           *room_name,
							      const gchar           *server,
							      const gchar           *nick);
static void            jabber_chatrooms_chatroom_free        (JabberChatroom        *room);
static LmHandlerResult jabber_chatrooms_message_handler      (LmMessageHandler      *handler,
							      LmConnection          *conn,
							      LmMessage             *message,
							      GossipJabberChatrooms *chatrooms);
static LmHandlerResult jabber_chatrooms_presence_handler     (LmMessageHandler      *handler,
							      LmConnection          *conn,
							      LmMessage             *message,
							      GossipJabberChatrooms *chatrooms);
static LmHandlerResult jabber_chatrooms_join_cb              (LmMessageHandler      *handler,
							      LmConnection          *connection,
							      LmMessage             *message,
							      AsyncCallbackData     *data);
static GossipContact * jabber_chatrooms_get_contact          (JabberChatroom        *room,
							      GossipJID             *jid,
							      gboolean              *new_contact);
static void            jabber_chatrooms_foreach_set_presence (gpointer               key,
							      JabberChatroom        *room,
							      GossipJabberChatrooms *chatrooms);

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
	room->name = g_strdup (room_name);

	jid_str = g_strdup_printf ("%s@%s/%s", room_name, server, nick);
	room->jid = gossip_jid_new (jid_str);
	g_free (jid_str);

	room->contacts = NULL;
	
	room->own_contact = gossip_contact_new_full (GOSSIP_CONTACT_TYPE_USER,
						     gossip_jid_get_full (room->jid),
						     gossip_jid_get_resource (room->jid));

	return room;
}

static void
jabber_chatrooms_chatroom_free (JabberChatroom *room)
{
	GSList *l;
	
	g_free (room->name);
	gossip_jid_unref (room->jid);

	for (l = room->contacts; l; l = l->next) {
		g_object_unref (l->data);
	}

	g_object_unref (room->own_contact);
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
	
	node = lm_message_node_get_child (m->node, "body");
	if (node) {
		if (gossip_jid_get_resource (jid) == NULL) {
			g_signal_emit_by_name (chatrooms->jabber,
					       "chatroom-new-room-event",
					       room->id, node->value);
		} else {
			gossip_time_t  timestamp;
			GossipContact *contact;

			contact = jabber_chatrooms_get_contact (room, jid, 
								NULL);
			timestamp = gossip_jabber_helper_get_timestamp_from_lm_message (m);
			d(g_print ("Emitting\n"));
			message = gossip_message_new (GOSSIP_MESSAGE_TYPE_CHAT_ROOM,
						      room->own_contact);
			
			timestamp = gossip_jabber_helper_get_timestamp_from_lm_message (m);
			gossip_message_set_timestamp (message, timestamp);
			
			gossip_message_set_sender (message, contact);
			gossip_message_set_body (message, node->value);
			
			g_signal_emit_by_name (chatrooms->jabber,
					       "chatroom-new-message", 
					       room->id, message);
			
			g_object_unref (message);
		}
	}

	node = lm_message_node_get_child (m->node, "subject");
	if (node) {
		g_signal_emit_by_name (chatrooms->jabber,
				       "chatroom-title-changed", 
				       room->id, node->value);
	}
	
	gossip_jid_unref (jid);
	 
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;

}

static LmHandlerResult
jabber_chatrooms_presence_handler (LmMessageHandler      *handler,
				   LmConnection          *conn,
				   LmMessage             *m,
				   GossipJabberChatrooms *chatrooms)
{
	const gchar         *from;
	GossipJID           *jid;
	JabberChatroom      *room;
	GossipContact       *contact;
	GossipPresence      *presence;
	GossipPresenceState  p_state;
	LmMessageSubType     type;
	LmMessageNode       *node;
	gboolean             new_contact;
	gboolean             was_offline;
	
	from = lm_message_node_get_attribute (m->node, "from");
	jid = gossip_jid_new (from);
		
	room = g_hash_table_lookup (chatrooms->room_jid_hash, jid);

	if (!room) {
		d(g_print ("Not a chatroom: %s\n", from));
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}
	
	d(g_print ("Chatroom message handler: %s\n", from));

	type = lm_message_get_sub_type (m);
	switch (type) {
	case LM_MESSAGE_SUB_TYPE_AVAILABLE:
		contact = jabber_chatrooms_get_contact (room, jid, 
							&new_contact);

		presence = gossip_presence_new ();
		node = lm_message_node_get_child (m->node, "show");
		if (node) {
			p_state = gossip_utils_get_presence_state_from_show_string (node->value);
			gossip_presence_set_state (presence, p_state);
		}
		node = lm_message_node_get_child (m->node, "status");
		if (node) {
			gossip_presence_set_status (presence, node->value);
		}

		/* Should signal joined if contact was found but offline */
		was_offline = !gossip_contact_is_online (contact);
		gossip_contact_add_presence (contact, presence);

		if (new_contact || was_offline) {
			d(g_print ("Chatrooms::presence_handler: Emit joined\n"));
			g_signal_emit_by_name (chatrooms->jabber,
					       "chatroom-contact-joined",
					       room->id, contact);
		} else {
			d(g_print ("Chatrooms::presence_handler: Emit updated\n"));
			g_signal_emit_by_name (chatrooms->jabber,
					       "chatroom-contact-presence-updated", 
					       room->id, contact);
		}
		break;

	case LM_MESSAGE_SUB_TYPE_UNAVAILABLE:
		contact = jabber_chatrooms_get_contact (room, jid, NULL);
		if (contact) {
			d(g_print ("Chatrooms::presence_handler: Emit removed\n"));
			g_signal_emit_by_name (chatrooms->jabber, 
					       "chatroom-contact-left",
					       room->id, contact);
			room->contacts = g_slist_remove (room->contacts,
							 contact);
			g_object_unref (contact);
		}
		break;

	default:
		d(g_print ("Chatrooms::presence_handler: Do nothing\n"));
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}	
	
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
	JabberChatroom        *room;

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

	room = (JabberChatroom *) g_hash_table_lookup (chatrooms->room_id_hash, 
						       GINT_TO_POINTER (id));
	
	((GossipJoinChatroomCb)data->callback) (GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
                                                GOSSIP_JOIN_CHATROOM_OK,
                                                id,
                                                data->user_data); 

	gossip_contact_add_presence (room->own_contact, chatrooms->presence);
				     
	g_signal_emit_by_name (chatrooms->jabber,
			       "chatroom-contact-joined",
			       room->id, room->own_contact);

        return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static GossipContact *
jabber_chatrooms_get_contact (JabberChatroom *room, 
			      GossipJID      *jid, 
			      gboolean       *new_contact) 
{
	GSList        *l;
	GossipContact *c;
	const gchar   *id;

	id = gossip_jid_get_full (jid);

	if (gossip_jid_equals (jid, room->jid)) {
		if (new_contact) {
			*new_contact = FALSE;
		}
		return room->own_contact;
	}

	for (l = room->contacts; l; l = l->next) {
		c = GOSSIP_CONTACT (l->data);

		if (g_ascii_strcasecmp (gossip_contact_get_id (c), id) == 0) {
			if (new_contact) {
				*new_contact = FALSE;
			}
			return c;
		}
	}

	if (new_contact) {
		*new_contact = TRUE;
	}

	c = gossip_contact_new_full (GOSSIP_CONTACT_TYPE_CHATROOM, id, 
				     gossip_jid_get_resource (jid));
	room->contacts = g_slist_prepend (room->contacts, c);

	return c;
}

static void
jabber_chatrooms_foreach_set_presence (gpointer               key,
				       JabberChatroom        *room, 
				       GossipJabberChatrooms *chatrooms)
{
	LmConnection   *connection;
	LmMessage      *m;
	const gchar    *show;
	const gchar    *status;
	GossipPresence *presence;
	
	connection = chatrooms->connection;
	presence = chatrooms->presence;

	show   = gossip_jabber_helper_presence_state_to_string (presence);
	status = gossip_presence_get_status (presence);

	m = lm_message_new_with_sub_type (gossip_jid_get_full (room->jid),
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_AVAILABLE);

	if (show) {
		lm_message_node_add_child (m->node, "show", show);
	}

	if (status) {
		lm_message_node_add_child (m->node, "status", status);
	}
	
	lm_connection_send (connection, m, NULL);
	lm_message_unref (m);
}

GossipJabberChatrooms *
gossip_jabber_chatrooms_new (GossipJabber   *jabber,
			     LmConnection   *connection)
{
	GossipJabberChatrooms *chatrooms;
	LmMessageHandler      *handler;

	chatrooms = g_new0 (GossipJabberChatrooms, 1);
	
	chatrooms->jabber     = jabber;
	chatrooms->connection = connection;
	chatrooms->presence   = NULL;
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
						LM_HANDLER_PRIORITY_FIRST);
	lm_message_handler_unref (handler);

	return chatrooms;
}	

void
gossip_jabber_chatrooms_free (GossipJabberChatrooms *chatrooms)
{
	g_hash_table_destroy (chatrooms->room_id_hash);
	g_hash_table_destroy (chatrooms->room_jid_hash);

	if (chatrooms->presence) {
		g_object_unref (chatrooms->presence);
	}
	
	g_free (chatrooms);
}

void
gossip_jabber_chatrooms_join (GossipJabberChatrooms *chatrooms,
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

        show = gossip_jabber_helper_presence_state_to_string (chatrooms->presence);

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

const gchar *
gossip_jabber_chatrooms_get_room_name (GossipJabberChatrooms *chatrooms,
				       GossipChatroomId       id)
{
	JabberChatroom *room;

	room = (JabberChatroom *) g_hash_table_lookup (chatrooms->room_id_hash,
						       GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("Unknown chatroom id: %d", id);
		return NULL;
	}

	return room->name;
}

void 
gossip_jabber_chatrooms_invite (GossipJabberChatrooms *chatrooms,
				GossipChatroomId       id,
				const gchar           *contact_id,
				const gchar           *invite)
{
	LmMessage      *m;
	LmMessageNode  *n;
	JabberChatroom *room;

	room = (JabberChatroom *) g_hash_table_lookup (chatrooms->room_id_hash, 
						       GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("Unknown chatroom id: %d", id);
		return;
	}

	d(g_print ("Inviting contact:'%s' to chatroom: %d\n", 
		   contact_id, id));

	m = lm_message_new (contact_id,
			    LM_MESSAGE_TYPE_MESSAGE);

	if (invite) {
		lm_message_node_add_child (m->node, "body", invite); 
	}
	
	n = lm_message_node_add_child (m->node, "x", NULL);
	lm_message_node_set_attributes (n, 
					"jid", gossip_jid_get_without_resource (room->jid),
					"xmlns", "jabber:x:conference",
					NULL);

	lm_connection_send (chatrooms->connection, m, NULL);
	lm_message_unref (m);
}

void 
gossip_jabber_chatrooms_invite_accept (GossipJabberChatrooms *chatrooms,
				       GossipJoinChatroomCb   callback,
				       const gchar           *nickname,
				       const gchar           *invite_id)
{
	gchar       *room = NULL;
	const gchar *server;

	g_return_if_fail (invite_id != NULL);
	g_return_if_fail (callback != NULL);
	server = strstr (invite_id, "@");

	g_return_if_fail (server != NULL);
	g_return_if_fail (nickname != NULL);
	
	if (server) {
		room = g_strndup (invite_id, server - invite_id);
		server++;
	}

	gossip_jabber_chatrooms_join (chatrooms,
				      room,
				      server,
				      nickname,
				      NULL,
				      callback,
				      NULL);

	g_free (room);
}

GList * 
gossip_jabber_chatrooms_get_rooms (GossipJabberChatrooms *chatrooms)
{
	return NULL;
}


void
gossip_jabber_chatrooms_set_presence (GossipJabberChatrooms  *chatrooms,
				      GossipPresence         *presence)
{
	if (chatrooms->presence) {
		g_object_unref (chatrooms->presence);
	}
	chatrooms->presence = g_object_ref (presence);

	g_hash_table_foreach (chatrooms->room_id_hash, 
			      (GHFunc) jabber_chatrooms_foreach_set_presence,
			      chatrooms);
}

gboolean
gossip_jabber_chatrooms_get_jid_is_chatroom (GossipJabberChatrooms *chatrooms,
					     const gchar           *jid_str)
{
	GossipJID *jid;
	gboolean   ret_val = FALSE;

	jid = gossip_jid_new (jid_str);

	if (g_hash_table_lookup (chatrooms->room_jid_hash, jid)) {
		ret_val = TRUE;
	}

	gossip_jid_unref (jid);

	return ret_val;
}

GossipContact * 
gossip_jabber_chatrooms_get_contact (GossipJabberChatrooms *chatrooms,
				     LmMessage             *message,
				     gint                  *chat_id)
{
	return NULL;
}
