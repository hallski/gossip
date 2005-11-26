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
#include "gossip-jabber-chatrooms.h"
#include "gossip-jabber-utils.h"

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
	gint              ref_count;

	GossipChatroomId  id;

	GossipJID        *jid;
	GossipContact    *own_contact;

	gchar            *name;
	GSList           *contacts;

	LmConnection     *connection;
	LmMessageHandler *join_handler;
} JabberChatroom;


typedef struct {
	GossipJabberChatrooms *chatrooms;
        gpointer               callback;
        gpointer               user_data;
} AsyncCallbackData;


static JabberChatroom *jabber_chatrooms_chatroom_new         (GossipJabberChatrooms  *chatrooms,
							      const gchar            *room_name,
							      const gchar            *server,
							      const gchar            *nick);
static JabberChatroom *jabber_chatrooms_chatroom_ref         (JabberChatroom         *room);
static void            jabber_chatrooms_chatroom_unref       (JabberChatroom         *room);
static LmHandlerResult jabber_chatrooms_message_handler      (LmMessageHandler       *handler,
							      LmConnection           *conn,
							      LmMessage              *message,
							      GossipJabberChatrooms  *chatrooms);
static LmHandlerResult jabber_chatrooms_presence_handler     (LmMessageHandler       *handler,
							      LmConnection           *conn,
							      LmMessage              *message,
							      GossipJabberChatrooms  *chatrooms);
static LmHandlerResult jabber_chatrooms_join_cb              (LmMessageHandler       *handler,
							      LmConnection           *connection,
							      LmMessage              *message,
							      AsyncCallbackData      *data);
static GossipContact * jabber_chatrooms_get_contact          (JabberChatroom         *room,
							      GossipJID              *jid,
							      gboolean               *new_contact);
static void            jabber_chatrooms_get_rooms_foreach    (gpointer                key,
							      JabberChatroom         *room,
							      GList                 **list);
static void            jabber_chatrooms_set_presence_foreach (gpointer                key,
							      JabberChatroom         *room,
							      GossipJabberChatrooms  *chatrooms);


static JabberChatroom *
jabber_chatrooms_chatroom_new (GossipJabberChatrooms *chatrooms,
			       const gchar           *room_name, 
			       const gchar           *server,
			       const gchar           *nick)
{
	GossipJabber   *jabber;
	GossipContact  *own_contact;
	GossipAccount  *account;
	JabberChatroom *room;
	gchar          *jid_str;
	static int      id = 1;

	/* FIXME: can we use the own contact instead of creating a new one? */
	jabber = chatrooms->jabber;
	own_contact = gossip_jabber_get_own_contact (jabber);
	account = gossip_contact_get_account (own_contact);

	room = g_new0 (JabberChatroom, 1);

	room->ref_count = 1;

	room->id = id++;
	room->name = g_strdup (room_name);

	jid_str = g_strdup_printf ("%s@%s/%s", room_name, server, nick);
	room->jid = gossip_jid_new (jid_str);
	g_free (jid_str);

	room->contacts = NULL;
	
	room->own_contact = gossip_contact_new_full (GOSSIP_CONTACT_TYPE_USER,
						     account,
						     gossip_jid_get_full (room->jid),
						     gossip_jid_get_resource (room->jid));

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

	g_free (room->name);
	gossip_jid_unref (room->jid);

	g_slist_foreach (room->contacts, (GFunc)g_object_unref, NULL);
	g_slist_free (room->contacts);

	g_object_unref (room->own_contact);

	if (room->join_handler) {
		lm_connection_unregister_message_handler (room->connection, 
							  room->join_handler, 
							  LM_MESSAGE_TYPE_PRESENCE);	
	}

	if (room->connection) {
		lm_connection_unref (room->connection);
	}

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
			timestamp = gossip_jabber_get_message_timestamp (m);

			message = gossip_message_new (GOSSIP_MESSAGE_TYPE_CHAT_ROOM,
						      room->own_contact);
			
			timestamp = gossip_jabber_get_message_timestamp (m);
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
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}
	
	d(g_print ("Protocol Chatrooms: Presence from: %s\n", from));

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
			d(g_print ("Protocol Chatrooms: ID[%d] Presence for new joining contact:'%s'\n",
				   room->id,
				   gossip_jid_get_full (jid)));
			g_signal_emit_by_name (chatrooms->jabber,
					       "chatroom-contact-joined",
					       room->id, contact);
		} else {
			d(g_print ("Protocol Chatrooms: ID[%d] Presence updated for contact:'%s'\n", 
				   room->id,
				   gossip_jid_get_full (jid)));
			g_signal_emit_by_name (chatrooms->jabber,
					       "chatroom-contact-presence-updated", 
					       room->id, contact);
		}
		break;

	case LM_MESSAGE_SUB_TYPE_UNAVAILABLE:
		contact = jabber_chatrooms_get_contact (room, jid, NULL);
		if (contact) {
			d(g_print ("Protocol Chatrooms: ID[%d] Contact left:'%s'\n",
				   room->id,
				   gossip_jid_get_full (jid)));
			g_signal_emit_by_name (chatrooms->jabber, 
					       "chatroom-contact-left",
					       room->id, contact);
			room->contacts = g_slist_remove (room->contacts,
							 contact);
			g_object_unref (contact);
		}
		break;

	default:
		d(g_print ("Protocol Chatrooms: Presence not handled for:'%s'\n",
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

	c = gossip_contact_new_full (GOSSIP_CONTACT_TYPE_CHATROOM, 
				     gossip_contact_get_account (room->own_contact),
				     id, 
				     gossip_jid_get_resource (jid));
	room->contacts = g_slist_prepend (room->contacts, c);

	return c;
}

GossipJabberChatrooms *
gossip_jabber_chatrooms_new (GossipJabber   *jabber,
			     LmConnection   *connection)
{
	GossipJabberChatrooms *chatrooms;
	LmMessageHandler      *handler;

	g_return_val_if_fail (jabber != NULL, NULL);
	g_return_val_if_fail (connection != NULL, NULL);

	chatrooms = g_new0 (GossipJabberChatrooms, 1);
	
	chatrooms->jabber     = g_object_ref (jabber);
	chatrooms->connection = lm_connection_ref (connection);
	chatrooms->presence   = NULL;
	chatrooms->room_id_hash = g_hash_table_new_full (NULL, 
							 NULL, 
							 NULL, 
							 (GDestroyNotify) jabber_chatrooms_chatroom_unref);
	chatrooms->room_jid_hash = g_hash_table_new_full (gossip_jid_hash,
							  gossip_jid_equal,
							  (GDestroyNotify) gossip_jid_unref,
							  (GDestroyNotify) jabber_chatrooms_chatroom_unref);

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
	g_return_if_fail (chatrooms != NULL);

	if (chatrooms->jabber) {
		g_object_unref (chatrooms->jabber);
	}

	if (chatrooms->connection) {
		lm_connection_unref (chatrooms->connection);
	}
     
	if (chatrooms->presence) {
		g_object_unref (chatrooms->presence);
	}

	g_hash_table_destroy (chatrooms->room_id_hash);
	g_hash_table_destroy (chatrooms->room_jid_hash);
	
	g_free (chatrooms);
}

GossipChatroomId
gossip_jabber_chatrooms_join (GossipJabberChatrooms *chatrooms,
			      const gchar           *room_name,
			      const gchar           *server,
			      const gchar           *nick,
			      const gchar           *password,
			      GossipChatroomJoinCb   callback,
			      gpointer               user_data)
{
	LmMessage         *m;
	const gchar       *show = NULL;
        AsyncCallbackData *data;
        gchar             *id_str;
	JabberChatroom    *room, *existing_room;
	
	g_return_val_if_fail (chatrooms != NULL, 0);
	g_return_val_if_fail (room_name != NULL, 0);
	g_return_val_if_fail (server != NULL, 0);
	g_return_val_if_fail (nick != NULL, 0);
	g_return_val_if_fail (callback != NULL, 0);

        d(g_print ("Protocol Chatrooms: Join chat room:'%s' on server:'%s'\n", 
		   room_name, server));

	room = jabber_chatrooms_chatroom_new (chatrooms, room_name, server, nick);
	existing_room = g_hash_table_lookup (chatrooms->room_jid_hash, room->jid);

	if (existing_room) {
 		jabber_chatrooms_chatroom_unref (room); 

		/* duplicate room already exists */
		(callback) (GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
			    GOSSIP_CHATROOM_JOIN_ALREADY_OPEN,
			    existing_room->id,
			    user_data); 
		
		return 0;
	}

	m = lm_message_new_with_sub_type (gossip_jid_get_full (room->jid),
					  LM_MESSAGE_TYPE_PRESENCE,
					  
					  LM_MESSAGE_SUB_TYPE_AVAILABLE);

	g_hash_table_insert (chatrooms->room_id_hash, 
			     GINT_TO_POINTER (room->id), 
			     jabber_chatrooms_chatroom_ref (room));
	g_hash_table_insert (chatrooms->room_jid_hash, 
			     room->jid, 
			     jabber_chatrooms_chatroom_ref (room));

	/* release the local ref */
	jabber_chatrooms_chatroom_unref (room); 

        show = gossip_jabber_presence_state_to_str (chatrooms->presence);

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

	room->connection = lm_connection_ref (chatrooms->connection);
        room->join_handler = lm_message_handler_new ((LmHandleMessageFunction) 
						     jabber_chatrooms_join_cb,
						     data, g_free);
	
	lm_connection_register_message_handler (chatrooms->connection,
						room->join_handler, 
						LM_MESSAGE_TYPE_PRESENCE,
						LM_HANDLER_PRIORITY_FIRST);
        
	/* some servers don't honor the id so we don't get a reply and
	   are waiting forever */
 	lm_connection_send (chatrooms->connection, m,  NULL);

	lm_message_unref (m);

	return room->id;
}

static LmHandlerResult
jabber_chatrooms_join_cb (LmMessageHandler  *handler,
                          LmConnection      *connection,
                          LmMessage         *m,
                          AsyncCallbackData *data)
{
	GossipJabberChatrooms    *chatrooms;
	GossipChatroomJoinResult  result = GOSSIP_CHATROOM_JOIN_OK;
	GossipChatroomJoinCb      callback;	
	JabberChatroom           *room;
	LmMessageSubType          type;
	LmMessageNode            *node;
	const gchar              *jid_str;
	const gchar              *id_str;
        gint                      id;

	chatrooms = data->chatrooms;

	id_str = lm_message_node_get_attribute (m->node, "id");
	jid_str = lm_message_node_get_attribute (m->node, "from");

	room = NULL;
	/* some servers don't use the ID in the response, so this
	   fails, so we check the JID too */
	if (id_str) {
		gchar *int_str;

		if (strlen (id_str) < strlen (JOIN_MSG_ID_PREFIX)) {
			g_warning ("Wrong id string when joining chatroom");
			return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
		}

		int_str = g_strdup (id_str + strlen (JOIN_MSG_ID_PREFIX));
		id = atoi (int_str);
		g_free (int_str);

		room = g_hash_table_lookup (chatrooms->room_id_hash, 
					    GINT_TO_POINTER (id));
	} else if (jid_str) {
		GossipJID *jid;

		jid = gossip_jid_new (jid_str);
		room = g_hash_table_lookup (chatrooms->room_jid_hash, jid);
		gossip_jid_unref (jid);
	} 

	if (!room) {
		/* can't do much else, but should we respond to async
		   callback with error? */
 		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS; 
	}

        /* check for error */
	type = lm_message_get_sub_type (m);
	
	if (type == LM_MESSAGE_SUB_TYPE_ERROR) {
		node = lm_message_node_get_child (m->node, "error");
		if (node) {
			const gchar *str;
			gint         code;

			str = lm_message_node_get_attribute (node, "code");
			code = str ? atoi (str) : 0;
			
			switch (code) {
			case 409: {
				/* conflicting nickname */
				d(g_print ("Protocol Chatrooms: Conflicting nickname\n"));
				result = GOSSIP_CHATROOM_JOIN_NICK_IN_USE;
				break;
			}

			case 502: {
				/* unresolved hostname */
				d(g_print ("Protocol Chatrooms: Unable to resolve hostname\n"));
				result = GOSSIP_CHATROOM_JOIN_UNKNOWN_HOST;
				break;
			}

			case 504: {
				/* remote server timeout */
				d(g_print ("Protocol Chatrooms: Join timed out\n"));
				result = GOSSIP_CHATROOM_JOIN_TIMED_OUT;
				break;
			}

			default:
				d(g_print ("Protocol Chatrooms: Unhandled presence error:%d\n", code));
				result = GOSSIP_CHATROOM_JOIN_UNKNOWN_ERROR;
				break;
			}
		}
	} 

	callback = data->callback;
	(callback) (GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
		    result,
		    room->id,
		    data->user_data); 

	if (result == GOSSIP_CHATROOM_JOIN_OK) {
		gossip_contact_add_presence (room->own_contact, 
					     chatrooms->presence);
		
		g_signal_emit_by_name (chatrooms->jabber,
				       "chatroom-contact-joined",
				       room->id,
				       room->own_contact);

	} else {
		/* clean up */
		g_hash_table_remove (chatrooms->room_id_hash, 
				     GINT_TO_POINTER (room->id));
		g_hash_table_remove (chatrooms->room_jid_hash, 
				     room->jid);
	}

	/* clean up handler */
	lm_connection_unregister_message_handler (connection, 
						  handler, 
						  LM_MESSAGE_TYPE_PRESENCE);
	room->join_handler = NULL;

        return LM_HANDLER_RESULT_REMOVE_MESSAGE;
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
		g_warning ("Unknown chatroom id: %d", id);
		return;
	}

	d(g_print ("Protocol Chatrooms: ID[%d] Cancel joining room\n", id));

	g_hash_table_remove (chatrooms->room_id_hash, 
			     GINT_TO_POINTER (room->id));
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
		g_warning ("Unknown chatroom id: %d", id);
		return;
	}
	
	d(g_print ("Protocol Chatrooms: ID[%d] Send message\n", id));
	
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

	g_return_if_fail (chatrooms != NULL);
	g_return_if_fail (new_title != NULL);

	room = (JabberChatroom*) g_hash_table_lookup (chatrooms->room_id_hash, 
						       GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("Unknown chatroom id: %d", id);
		return;
	}
	d(g_print ("Protocol Chatrooms: ID[%d] Set chatroom title to:'%s'\n", 
		   id, new_title));
	
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

	g_return_if_fail (chatrooms != NULL);
	g_return_if_fail (new_nick != NULL);

	room = (JabberChatroom*) g_hash_table_lookup (chatrooms->room_id_hash, 
						       GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("Unknown chatroom id: %d", id);
		return;
	}

	d(g_print ("Protocol Chatrooms: ID[%d] Change chatroom nick to:'%s'\n", 
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
		g_warning ("Unknown chatroom id: %d", id);
		return;
	}

	d(g_print ("Protocol Chatrooms: ID[%d] Leaving room\n", id));

	m = lm_message_new_with_sub_type (gossip_jid_get_full (room->jid),
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_UNAVAILABLE);

	lm_connection_send (chatrooms->connection, m, NULL);
	lm_message_unref (m);

	g_hash_table_remove (chatrooms->room_id_hash, 
			     GINT_TO_POINTER (room->id));
	g_hash_table_remove (chatrooms->room_jid_hash, 
			     room->jid);
}

const gchar *
gossip_jabber_chatrooms_get_room_name (GossipJabberChatrooms *chatrooms,
				       GossipChatroomId       id)
{
	JabberChatroom *room;

	g_return_val_if_fail (chatrooms != NULL, NULL);

	room = (JabberChatroom*) g_hash_table_lookup (chatrooms->room_id_hash,
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

	g_return_if_fail (chatrooms != NULL);
	g_return_if_fail (contact_id != NULL);

	room = (JabberChatroom *) g_hash_table_lookup (chatrooms->room_id_hash, 
						       GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("Unknown chatroom id: %d", id);
		return;
	}

	d(g_print ("Protocol Chatrooms: ID[%d] Inviting contact:'%s'\n", 
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

GossipContact * 
gossip_jabber_chatrooms_get_contact (GossipJabberChatrooms *chatrooms,
				     LmMessage             *message,
				     gint                  *chat_id)
{
	return NULL;
}
