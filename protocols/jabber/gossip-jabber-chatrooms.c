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
 */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>

#include <libgossip/gossip-utils.h>

#include "gossip-jabber-chatrooms.h"
#include "gossip-jid.h"
#include "gossip-jabber-utils.h"
#include "gossip-jabber-private.h"

#define DEBUG_MSG(x)
/*#define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n"); */

#define JOIN_TIMEOUT       20000

#define JOIN_MSG_ID_PREFIX "gc_join_"

struct _GossipJabberChatrooms {
	GossipJabber          *jabber;
	GossipPresence        *presence;
	LmConnection          *connection;

	GHashTable            *room_id_hash;
	GHashTable            *room_jid_hash;
};

typedef struct {
	gint                   ref_count;

	GossipChatroom        *chatroom;

	GossipJID             *jid;
	GossipContact         *own_contact;

	GSList                *contacts;

	guint                  timeout_id;

 	GossipJabberChatrooms *chatrooms; 
        GossipChatroomJoinCb   callback;
        gpointer               user_data;

	LmConnection          *connection;
	LmMessageHandler      *join_handler;
} JabberChatroom;

static void             jabber_chatrooms_logged_out_cb        (GossipProtocol         *jabber,
							       GossipAccount          *account,
							       GossipJabberChatrooms  *chatrooms);
static JabberChatroom * jabber_chatrooms_chatroom_new         (GossipJabberChatrooms  *chatrooms,
							       GossipChatroom         *chatroom);
static JabberChatroom * jabber_chatrooms_chatroom_ref         (JabberChatroom         *room);
static void             jabber_chatrooms_chatroom_unref       (JabberChatroom         *room);
static GossipChatroomId jabber_chatrooms_chatroom_get_id      (JabberChatroom         *room);
static LmHandlerResult  jabber_chatrooms_message_handler      (LmMessageHandler       *handler,
							       LmConnection           *conn,
							       LmMessage              *message,
							       GossipJabberChatrooms  *chatrooms);
static LmHandlerResult  jabber_chatrooms_presence_handler     (LmMessageHandler       *handler,
							       LmConnection           *conn,
							       LmMessage              *message,
							       GossipJabberChatrooms  *chatrooms);
static LmHandlerResult  jabber_chatrooms_join_cb              (LmMessageHandler       *handler,
							       LmConnection           *connection,
							       LmMessage              *message,
							       JabberChatroom         *room);
static GossipContact *  jabber_chatrooms_get_contact          (JabberChatroom         *room,
							       GossipJID              *jid,
							       gboolean               *new_contact);
static void             jabber_chatrooms_get_rooms_foreach    (gpointer                key,
							       JabberChatroom         *room,
							       GList                 **list);
static void             jabber_chatrooms_set_presence_foreach (gpointer                key,
							       JabberChatroom         *room,
							       GossipJabberChatrooms  *chatrooms);

GossipJabberChatrooms *
gossip_jabber_chatrooms_init (GossipJabber *jabber)
{
	GossipJabberChatrooms *chatrooms;
	LmConnection          *connection;
	LmMessageHandler      *handler;

	g_return_val_if_fail (GOSSIP_IS_JABBER (jabber), NULL);

	connection = gossip_jabber_get_connection (jabber);
	g_return_val_if_fail (connection != NULL, NULL);

	chatrooms = g_new0 (GossipJabberChatrooms, 1);
	
	chatrooms->jabber = g_object_ref (jabber);
	chatrooms->connection = lm_connection_ref (connection);
	chatrooms->presence = NULL;

	chatrooms->room_id_hash = g_hash_table_new (NULL, NULL);
	chatrooms->room_jid_hash = g_hash_table_new_full (gossip_jid_hash,
							  gossip_jid_equal,
							  (GDestroyNotify) gossip_jid_unref,
							  NULL);

	g_signal_connect (GOSSIP_PROTOCOL (chatrooms->jabber), "logged-out",
			  G_CALLBACK (jabber_chatrooms_logged_out_cb), 
			  chatrooms);

	handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_chatrooms_message_handler,
					  chatrooms, NULL);

	lm_connection_register_message_handler (chatrooms->connection,
						handler, 
						LM_MESSAGE_TYPE_MESSAGE,
						LM_HANDLER_PRIORITY_NORMAL);
	lm_message_handler_unref (handler);
	
	handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_chatrooms_presence_handler,
					  chatrooms, NULL);

	lm_connection_register_message_handler (chatrooms->connection,
						handler, 
						LM_MESSAGE_TYPE_PRESENCE,
						LM_HANDLER_PRIORITY_FIRST);
	lm_message_handler_unref (handler);

	return chatrooms;
}	

void
gossip_jabber_chatrooms_finalize (GossipJabberChatrooms *chatrooms)
{
	if (!chatrooms) {
		/* We don't error here, because if no connection is
		 * made, then we can clean up a GossipJabber object
		 * without any chatrooms ever existing.
		 */
		return;
	}

	g_signal_handlers_disconnect_by_func (GOSSIP_PROTOCOL (chatrooms->jabber), 
					      jabber_chatrooms_logged_out_cb, 
					      chatrooms);

	g_hash_table_destroy (chatrooms->room_id_hash);
	g_hash_table_destroy (chatrooms->room_jid_hash);

	if (chatrooms->presence) {
		g_object_unref (chatrooms->presence);
	}

	lm_connection_unref (chatrooms->connection);
	g_object_unref (chatrooms->jabber);
	
	g_free (chatrooms);
}

static void
jabber_chatrooms_logged_out_cb (GossipProtocol        *jabber,
				GossipAccount         *account,
				GossipJabberChatrooms *chatrooms)
{
	GList *rooms;
	GList *l;

	/* Clean up chat rooms */
	rooms = gossip_jabber_chatrooms_get_rooms (chatrooms);

	for (l = rooms; l; l = l->next) {
		GossipChatroomId id;

		id = GPOINTER_TO_INT (l->data);
		gossip_jabber_chatrooms_leave (chatrooms, id);
	}

	g_list_free (rooms);
}

static JabberChatroom *
jabber_chatrooms_chatroom_new (GossipJabberChatrooms *chatrooms,
			       GossipChatroom        *chatroom)
{
	GossipJabber   *jabber;
	GossipContact  *own_contact;
	JabberChatroom *room;
	gchar          *jid_str;

	jabber = chatrooms->jabber;

	room = g_new0 (JabberChatroom, 1);

	room->ref_count = 1;
	room->chatroom = g_object_ref (chatroom);

	jid_str = g_strdup_printf ("%s/%s", 
				   gossip_chatroom_get_id_str (chatroom), 
				   gossip_chatroom_get_nick (chatroom));
	room->jid = gossip_jid_new (jid_str);
	g_free (jid_str);

	room->contacts = NULL;

	/* What we do here is copy the contact instead of reference
	 * it, plus we change the name according to how the user wants
	 * their nickname in the chat room. 
	 */
	own_contact = gossip_jabber_get_own_contact (jabber);
	room->own_contact = gossip_contact_copy (own_contact);
	gossip_contact_set_name (room->own_contact, 
				 gossip_chatroom_get_nick (chatroom));

	return room;
}

static JabberChatroom *
jabber_chatrooms_chatroom_ref (JabberChatroom *room)
{
	if (!room) {
		return NULL;
	}
	
	room->ref_count++;
	return room;
}

static void
jabber_chatrooms_chatroom_unref (JabberChatroom *room)
{
	if (!room) {
		return;
	}

	room->ref_count--;

	if (room->ref_count > 0) {
		return;
	}

	g_object_unref (room->chatroom);

	gossip_jid_unref (room->jid);

	g_slist_foreach (room->contacts, (GFunc)g_object_unref, NULL);
	g_slist_free (room->contacts);

	g_object_unref (room->own_contact);

	if (room->timeout_id) {
		g_source_remove (room->timeout_id);
		room->timeout_id = 0;
	}

	if (room->join_handler) {
		lm_connection_unregister_message_handler (room->connection, 
							  room->join_handler, 
							  LM_MESSAGE_TYPE_PRESENCE);	
		room->join_handler = NULL;
	}

	if (room->connection) {
		lm_connection_unref (room->connection);
		room->connection = NULL;
	}

	g_free (room);
}

static GossipChatroomId 
jabber_chatrooms_chatroom_get_id (JabberChatroom *room)
{
	g_return_val_if_fail (room != NULL, 0);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (room->chatroom), 0);

	return gossip_chatroom_get_id (room->chatroom);
}

static LmHandlerResult
jabber_chatrooms_message_handler (LmMessageHandler      *handler,
				  LmConnection          *conn,
				  LmMessage             *m,
				  GossipJabberChatrooms *chatrooms)
{
	GossipJID        *jid;
	GossipMessage    *message;
	GossipContact    *contact;
	GossipChatroomId  id;
	JabberChatroom   *room;
	LmMessageNode    *node;
	const gchar      *from;


	if (lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_GROUPCHAT) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	from = lm_message_node_get_attribute (m->node, "from");
	jid = gossip_jid_new (from);

	room = g_hash_table_lookup (chatrooms->room_jid_hash, jid);
	id = jabber_chatrooms_chatroom_get_id (room);

	if (!room) {
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}
	
	node = lm_message_node_get_child (m->node, "body");
	if (node) {
		if (gossip_jid_get_resource (jid) == NULL) {
			g_signal_emit_by_name (chatrooms->jabber,
					       "chatroom-new-event",
					       id, node->value);
		} else {
			gossip_time_t timestamp;

			timestamp = gossip_jabber_get_message_timestamp (m);

			message = gossip_message_new (GOSSIP_MESSAGE_TYPE_CHAT_ROOM,
						      room->own_contact);
			
			timestamp = gossip_jabber_get_message_timestamp (m);
			gossip_message_set_timestamp (message, timestamp);
			
			contact = jabber_chatrooms_get_contact (room, jid, NULL);
			gossip_message_set_sender (message, contact);
			gossip_message_set_body (message, node->value);
			
			g_signal_emit_by_name (chatrooms->jabber,
					       "chatroom-new-message", 
					       id, message);
			
			g_object_unref (message);
		}
	}

	node = lm_message_node_get_child (m->node, "subject");
	if (node) {
		/* Note: I'm not sure if there is a better way to fix this? It
		 * happens when the history is too old so the topic is just set
		 * by the room itself, not a member.
		 */
		if (gossip_jid_get_resource (jid) != NULL) {
			contact = jabber_chatrooms_get_contact (room, jid, NULL);
			
			g_signal_emit_by_name (chatrooms->jabber,
					       "chatroom-topic-changed", 
					       id, contact, node->value);
		}
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
	GossipChatroomId     id;
	LmMessageSubType     type;
	LmMessageNode       *node;
	gboolean             new_contact;
	gboolean             was_offline;
	
	from = lm_message_node_get_attribute (m->node, "from");
	jid = gossip_jid_new (from);
		
	room = g_hash_table_lookup (chatrooms->room_jid_hash, jid);
	if (!room) {
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}
	
	id = jabber_chatrooms_chatroom_get_id (room);

	DEBUG_MSG (("ProtocolChatrooms: Presence from: %s", from));

	type = lm_message_get_sub_type (m);
	switch (type) {
	case LM_MESSAGE_SUB_TYPE_AVAILABLE:
		/* get details */
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

		/* should signal joined if contact was found but offline */
		was_offline = !gossip_contact_is_online (contact);
		gossip_contact_add_presence (contact, presence);

		/* is contact new or updated */
		if (new_contact || was_offline) {
			DEBUG_MSG (("ProtocolChatrooms: ID[%d] Presence for new joining contact:'%s'",
				   id, gossip_jid_get_full (jid)));
			g_signal_emit_by_name (chatrooms->jabber,
					       "chatroom-contact-joined",
					       id, contact);
		} else {
			DEBUG_MSG (("ProtocolChatrooms: ID[%d] Presence updated for contact:'%s'", 
				   id, gossip_jid_get_full (jid)));
			g_signal_emit_by_name (chatrooms->jabber,
					       "chatroom-contact-presence-updated", 
					       id, contact);
		}
		break;

	case LM_MESSAGE_SUB_TYPE_UNAVAILABLE:
		contact = jabber_chatrooms_get_contact (room, jid, NULL);
		if (contact) {
			DEBUG_MSG (("ProtocolChatrooms: ID[%d] Contact left:'%s'",
				   id, gossip_jid_get_full (jid)));
			g_signal_emit_by_name (chatrooms->jabber, 
					       "chatroom-contact-left",
					       id, contact);
			room->contacts = g_slist_remove (room->contacts, contact);
			g_object_unref (contact);
		}
		break;

	default:
		DEBUG_MSG (("ProtocolChatrooms: Presence not handled for:'%s'",
			   gossip_jid_get_full (jid)));
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}	
	
	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
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

	c = gossip_contact_new_full (GOSSIP_CONTACT_TYPE_CHATROOM, 
				     gossip_contact_get_account (room->own_contact),
				     id, 
				     gossip_jid_get_resource (jid));
	room->contacts = g_slist_prepend (room->contacts, c);

	return c;
}

static gboolean
jabber_chatrooms_join_timeout_cb (JabberChatroom *room)
{
	GossipChatroomId       id;
	GossipJabberChatrooms *chatrooms;
	const gchar           *last_error;

	room->timeout_id = 0;

	if (room->join_handler) {
		lm_message_handler_unref (room->join_handler);
		room->join_handler = NULL;
	}

	id = jabber_chatrooms_chatroom_get_id (room);
	DEBUG_MSG (("ProtocolChatrooms: ID[%d] Join timed out (internally)", id));

	/* set chatroom status and error */
	gossip_chatroom_set_status (room->chatroom, GOSSIP_CHATROOM_STATUS_ERROR);

	last_error = gossip_chatroom_provider_join_result_as_str (GOSSIP_CHATROOM_JOIN_TIMED_OUT);
	gossip_chatroom_set_last_error (room->chatroom, last_error);

	/* call callback */
	chatrooms = room->chatrooms;
		
	if (room->callback != NULL) {
		DEBUG_MSG (("ProtocolChatrooms: ID[%d] Calling back...", id));
		(room->callback) (GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
				  GOSSIP_CHATROOM_JOIN_TIMED_OUT, 
				  id, room->user_data);
	}

	/* clean up */
	g_hash_table_remove (chatrooms->room_id_hash, 
			     GINT_TO_POINTER (id));
	g_hash_table_remove (chatrooms->room_jid_hash, 
			     room->jid);

	/* clean up callback data */
	room->callback = NULL;
	room->user_data = NULL;

	jabber_chatrooms_chatroom_unref (room);

	return FALSE;
}

GossipChatroomId
gossip_jabber_chatrooms_join (GossipJabberChatrooms *chatrooms,
			      GossipChatroom        *chatroom,
			      GossipChatroomJoinCb   callback,
			      gpointer               user_data)
{
	GossipChatroomId   id;
	JabberChatroom    *room, *existing_room;
	LmMessage         *m;
	const gchar       *show = NULL;
        gchar             *id_str;
	
	g_return_val_if_fail (chatrooms != NULL, 0);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), 0);
	g_return_val_if_fail (callback != NULL, 0);

	room = jabber_chatrooms_chatroom_new (chatrooms, chatroom);
	existing_room = g_hash_table_lookup (chatrooms->room_jid_hash, room->jid);

	if (existing_room) {
 		jabber_chatrooms_chatroom_unref (room); 

		/* Duplicate room already exists. */
		id = jabber_chatrooms_chatroom_get_id (existing_room);

		DEBUG_MSG (("ProtocolChatrooms: ID[%d] Join chatroom:'%s', room already exists.", 
			   id,
			   gossip_chatroom_get_room (chatroom)));

		(callback) (GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
			    GOSSIP_CHATROOM_JOIN_ALREADY_OPEN,
			    id,
			    user_data); 
 		
		return id;
	}

	/* Get real chatroom. */
	id = jabber_chatrooms_chatroom_get_id (room);

        DEBUG_MSG (("ProtocolChatrooms: ID[%d] Join chatroom:'%s' on server:'%s'", 
		   id,
		   gossip_chatroom_get_room (chatroom),
		   gossip_chatroom_get_server (chatroom)));


	/* Add timeout for server response. */
	room->timeout_id = g_timeout_add (JOIN_TIMEOUT, 
					  (GSourceFunc) jabber_chatrooms_join_timeout_cb, 
					  room);

	room->chatrooms = chatrooms;

	/* Set callback data. */
        room->callback = callback;
        room->user_data = user_data;
	
	gossip_chatroom_set_status (chatroom, GOSSIP_CHATROOM_STATUS_JOINING);
	gossip_chatroom_set_last_error (room->chatroom, NULL);

	/* Compose message. */
	m = lm_message_new_with_sub_type (gossip_jid_get_full (room->jid),
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_AVAILABLE);

	g_hash_table_insert (chatrooms->room_id_hash, 
			     GINT_TO_POINTER (id), 
			     jabber_chatrooms_chatroom_ref (room));
	g_hash_table_insert (chatrooms->room_jid_hash, 
			     room->jid, 
			     jabber_chatrooms_chatroom_ref (room));

	jabber_chatrooms_chatroom_unref (room); 

        show = gossip_jabber_presence_state_to_str (chatrooms->presence);

	if (show) {
                lm_message_node_add_child (m->node, "show", show);
	}

        id_str = g_strdup_printf (JOIN_MSG_ID_PREFIX "%d", id);
        lm_message_node_set_attribute (m->node, "id", id_str);
        g_free (id_str);

	/* Send message. */
	room->connection = lm_connection_ref (chatrooms->connection);
        room->join_handler = lm_message_handler_new ((LmHandleMessageFunction) 
						     jabber_chatrooms_join_cb,
						     room, NULL);
	
	lm_connection_register_message_handler (chatrooms->connection,
						room->join_handler, 
						LM_MESSAGE_TYPE_PRESENCE,
						LM_HANDLER_PRIORITY_FIRST);
        
	/* Some servers don't honor the id so we don't get a reply and
	   are waiting forever. */
 	lm_connection_send (chatrooms->connection, m,  NULL);

	lm_message_unref (m);

	return id;
}

static LmHandlerResult
jabber_chatrooms_join_cb (LmMessageHandler *handler,
                          LmConnection     *connection,
                          LmMessage        *m,
                          JabberChatroom   *room)
{
 	GossipJabberChatrooms    *chatrooms; 
	GossipChatroomJoinResult  result;
	GossipChatroomStatus      status;
	GossipChatroomId          id;
	GossipChatroomId          id_found;
	LmMessageSubType          type;
	LmMessageNode            *node = NULL;
	const gchar              *from;
	GossipJID                *jid;
	JabberChatroom           *room_found;
	gboolean                  room_match = FALSE;

	if (!room || !room->join_handler) {
 		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS; 
	}

	chatrooms = room->chatrooms;

	/* get room id */
	id = jabber_chatrooms_chatroom_get_id (room);

	from = lm_message_node_get_attribute (m->node, "from");
	jid = gossip_jid_new (from);

	room_found = g_hash_table_lookup (chatrooms->room_jid_hash, jid);
	gossip_jid_unref (jid);

	if (room_found) {
		id_found = jabber_chatrooms_chatroom_get_id (room_found);
		if (id == id_found) {
			room_match = TRUE;
		}
	} 

	if (!room_match) {
 		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS; 
	}

	/* clean up the join timeout */
	if (room->timeout_id) {
		g_source_remove (room->timeout_id);
		room->timeout_id = 0;
	}

	/* clean up handler */
	if (room->join_handler) {
		lm_message_handler_unref (room->join_handler);
		room->join_handler = NULL;
	}

        /* check for error */
 	type = lm_message_get_sub_type (m);
	if (type == LM_MESSAGE_SUB_TYPE_ERROR) {
		node = lm_message_node_get_child (m->node, "error");
	}
	
	if (node) {
		const gchar *str;
		gint         code;		
		str = lm_message_node_get_attribute (node, "code");
		code = str ? atoi (str) : 0;
		
		switch (code) {
		case 404: 
 			/* conflicting nickname */
			DEBUG_MSG (("ProtocolChatrooms: ID[%d] Chatroom not found", id));
			result = GOSSIP_CHATROOM_JOIN_UNKNOWN_HOST;
			break;
			
		case 409: 
 			/* conflicting nickname */
			DEBUG_MSG (("ProtocolChatrooms: ID[%d] Conflicting nickname", id));
			result = GOSSIP_CHATROOM_JOIN_NICK_IN_USE;
			break;
			
		case 502: 
			/* unresolved hostname */
			DEBUG_MSG (("ProtocolChatrooms: ID[%d] Unable to resolve hostname", id));
			result = GOSSIP_CHATROOM_JOIN_UNKNOWN_HOST;
			break;
			
		case 504: 
			/* remote server timeout */
			DEBUG_MSG (("ProtocolChatrooms: ID[%d] Join timed out", id));
			result = GOSSIP_CHATROOM_JOIN_TIMED_OUT;
			break;
			
		default:
			DEBUG_MSG (("ProtocolChatrooms: ID[%d] Unhandled presence error:%d", id, code));
			result = GOSSIP_CHATROOM_JOIN_UNKNOWN_ERROR;
			break;
		}

		/* set room state */
		status = GOSSIP_CHATROOM_STATUS_ERROR;
	} else {
		result = GOSSIP_CHATROOM_JOIN_OK;
		status = GOSSIP_CHATROOM_STATUS_ACTIVE;
	}

	gossip_chatroom_set_status (room->chatroom, status);
	gossip_chatroom_set_last_error (room->chatroom, 
					gossip_chatroom_provider_join_result_as_str (result));

	if (room->callback != NULL) {
		DEBUG_MSG (("ProtocolChatrooms: ID[%d] Calling back...", id));
		(room->callback) (GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
				  result, id, room->user_data);
	}

	/* Clean up callback data */
	room->callback = NULL;
	room->user_data = NULL;

	/* Articulate own contact presence so we appear in group chat */
	if (result == GOSSIP_CHATROOM_JOIN_OK) {
		gossip_contact_add_presence (room->own_contact, 
					     chatrooms->presence);
		
		g_signal_emit_by_name (chatrooms->jabber,
				       "chatroom-joined",
				       id);
	} else {
		/* Clean up */
		g_hash_table_remove (chatrooms->room_id_hash, 
				     GINT_TO_POINTER (id));
		g_hash_table_remove (chatrooms->room_jid_hash, 
				     room->jid);
	}

        return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

void
gossip_jabber_chatrooms_cancel (GossipJabberChatrooms *chatrooms,
				GossipChatroomId       id)
{
	JabberChatroom *room;

	g_return_if_fail (chatrooms != NULL);

	room = g_hash_table_lookup (chatrooms->room_id_hash, 
				    GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("ProtocolChatrooms: Unknown chatroom id: %d", id);
		return;
	}

	DEBUG_MSG (("ProtocolChatrooms: ID[%d] Cancel joining room", id));

	if (room->timeout_id) {
		g_source_remove (room->timeout_id);
		room->timeout_id = 0;
	}

	if (room->join_handler) {
		lm_message_handler_unref (room->join_handler);
		room->join_handler = NULL;
	}

	
	gossip_chatroom_set_status (room->chatroom, GOSSIP_CHATROOM_STATUS_INACTIVE);
	gossip_chatroom_set_last_error (room->chatroom, NULL);

	if (room->callback != NULL) {
		DEBUG_MSG (("ProtocolChatrooms: ID[%d] Calling back...", id));
		(room->callback) (GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
				  GOSSIP_CHATROOM_JOIN_CANCELED, id, room->user_data);
	}

	/* clean up callback data */
	room->callback = NULL;
	room->user_data = NULL;

	g_hash_table_remove (chatrooms->room_id_hash, 
			     GINT_TO_POINTER (id));
	g_hash_table_remove (chatrooms->room_jid_hash, 
			     room->jid);
}

void 
gossip_jabber_chatrooms_send (GossipJabberChatrooms *chatrooms, 
			      GossipChatroomId       id,
			      const gchar           *message)
{
	LmMessage      *m;
	JabberChatroom *room;

	g_return_if_fail (chatrooms != NULL);
	g_return_if_fail (message != NULL);

	room = (JabberChatroom*) g_hash_table_lookup (chatrooms->room_id_hash, 
						       GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("ProtocolChatrooms: Unknown chatroom id: %d", id);
		return;
	}
	
	DEBUG_MSG (("ProtocolChatrooms: ID[%d] Send message", id));
	
	m = lm_message_new_with_sub_type (gossip_jid_get_without_resource (room->jid),
					  LM_MESSAGE_TYPE_MESSAGE,
					  LM_MESSAGE_SUB_TYPE_GROUPCHAT);
	lm_message_node_add_child (m->node, "body", message);

	lm_connection_send (chatrooms->connection, m, NULL);
	lm_message_unref (m);
}

void
gossip_jabber_chatrooms_change_topic (GossipJabberChatrooms *chatrooms,
				      GossipChatroomId       id,
				      const gchar           *new_topic)
{
	JabberChatroom *room;
	const gchar    *without_resource;
	LmMessage      *m;

	g_return_if_fail (chatrooms != NULL);
	g_return_if_fail (new_topic != NULL);

	room = (JabberChatroom*) g_hash_table_lookup (chatrooms->room_id_hash, 
						       GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("ProtocolChatrooms: Unknown chatroom id: %d", id);
		return;
	}

	DEBUG_MSG (("ProtocolChatrooms: ID[%d] Change topic to:'%s'", 
		   id, new_topic));
	
	without_resource = gossip_jid_get_without_resource (room->jid);
	
	m = lm_message_new_with_sub_type (without_resource,
					  LM_MESSAGE_TYPE_MESSAGE,
					  LM_MESSAGE_SUB_TYPE_GROUPCHAT);
	
	lm_message_node_add_child (m->node, "subject", new_topic);

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

	g_return_if_fail (chatrooms != NULL);
	g_return_if_fail (new_nick != NULL);

	room = (JabberChatroom*) g_hash_table_lookup (chatrooms->room_id_hash, 
						       GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("ProtocolChatrooms: Unknown chatroom id: %d", id);
		return;
	}

	DEBUG_MSG (("ProtocolChatrooms: ID[%d] Change chatroom nick to:'%s'", 
		   id, new_nick));

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

	g_return_if_fail (chatrooms != NULL);

	room = (JabberChatroom*) g_hash_table_lookup (chatrooms->room_id_hash, 
						       GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("ProtocolChatrooms: Unknown chatroom id: %d", id);
		return;
	}

	gossip_chatroom_set_status (room->chatroom, GOSSIP_CHATROOM_STATUS_INACTIVE);
	gossip_chatroom_set_last_error (room->chatroom, NULL);

	m = lm_message_new_with_sub_type (gossip_jid_get_full (room->jid),
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_UNAVAILABLE);

	lm_connection_send (chatrooms->connection, m, NULL);
	lm_message_unref (m);

	g_hash_table_remove (chatrooms->room_id_hash,
			     GINT_TO_POINTER (id));
	g_hash_table_remove (chatrooms->room_jid_hash,
			     room->jid);

	DEBUG_MSG (("ProtocolChatrooms: ID[%d] Leaving room, ref count is %d", 
		   id, room->ref_count - 1));
	
	jabber_chatrooms_chatroom_unref (room);
}

GossipChatroom *
gossip_jabber_chatrooms_find (GossipJabberChatrooms *chatrooms,
			      GossipChatroomId       id)
{
	JabberChatroom *room;

	g_return_val_if_fail (chatrooms != NULL, NULL);

	room = (JabberChatroom*) g_hash_table_lookup (chatrooms->room_id_hash,
						      GINT_TO_POINTER (id));
	if (!room) {
		return NULL;
	}

	return room->chatroom;
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

	g_return_if_fail (chatrooms != NULL);
	g_return_if_fail (contact_id != NULL);

	room = (JabberChatroom *) g_hash_table_lookup (chatrooms->room_id_hash, 
						       GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("ProtocolChatrooms: Unknown chatroom id: %d", id);
		return;
	}

	DEBUG_MSG (("ProtocolChatrooms: ID[%d] Inviting contact:'%s'", 
		   id, contact_id));

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
				       GossipChatroomJoinCb   callback,
				       const gchar           *nickname,
				       const gchar           *invite_id)
{
	GossipChatroom *chatroom;
	gchar          *room = NULL;
	const gchar    *server;

	g_return_if_fail (invite_id != NULL);
	g_return_if_fail (callback != NULL);

	server = strstr (invite_id, "@");

	g_return_if_fail (server != NULL);
	g_return_if_fail (nickname != NULL);
	
	if (server) {
		room = g_strndup (invite_id, server - invite_id);
		server++;
	}

	chatroom = g_object_new (GOSSIP_TYPE_CHATROOM, 
				 "type", GOSSIP_CHATROOM_TYPE_NORMAL,
				 "server", server,
				 "name", room,
				 "room", room,
				 "nick", nickname,
				 NULL);

	gossip_jabber_chatrooms_join (chatrooms,
				      chatroom,
				      callback,
				      NULL);

	g_object_unref (chatroom);
	g_free (room);
}

GList * 
gossip_jabber_chatrooms_get_rooms (GossipJabberChatrooms *chatrooms)
{
	GList *list = NULL;

	g_return_val_if_fail (chatrooms != NULL, NULL);

	g_hash_table_foreach (chatrooms->room_id_hash, 
			      (GHFunc) jabber_chatrooms_get_rooms_foreach,
			      &list);	
	
	return list;
}

static void
jabber_chatrooms_get_rooms_foreach (gpointer               key,
				    JabberChatroom        *room, 
				    GList                **list)
{
	*list = g_list_append (*list, key);
}

void
gossip_jabber_chatrooms_set_presence (GossipJabberChatrooms  *chatrooms,
				      GossipPresence         *presence)
{
	g_return_if_fail (chatrooms != NULL);
	
	if (chatrooms->presence) {
		g_object_unref (chatrooms->presence);
	}

	chatrooms->presence = g_object_ref (presence);

	g_hash_table_foreach (chatrooms->room_id_hash, 
			      (GHFunc) jabber_chatrooms_set_presence_foreach,
			      chatrooms);
}

static void
jabber_chatrooms_set_presence_foreach (gpointer               key,
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

	show   = gossip_jabber_presence_state_to_str (presence);
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
