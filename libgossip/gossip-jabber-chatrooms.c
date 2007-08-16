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

#include "config.h"

#include <stdlib.h>
#include <string.h>


#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-chatroom.h>
#include <libgossip/gossip-ft.h>
#include <libgossip/gossip-ft-provider.h>
#include <libgossip/gossip-message.h>
#include <libgossip/gossip-utils.h>

#include "gossip-jabber-chatrooms.h"
#include "gossip-jabber-disco.h"
#include "gossip-jabber-utils.h"
#include "gossip-jabber-private.h"
#include "gossip-jid.h"

#define DEBUG_DOMAIN "JabberChatrooms"

#define XMPP_MUC_XMLNS       "http://jabber.org/protocol/muc"
#define XMPP_MUC_OWNER_XMLNS "http://jabber.org/protocol/muc#owner"
#define XMPP_MUC_USER_XMLNS  "http://jabber.org/protocol/muc#user"

#define JOIN_TIMEOUT 20000

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

static void             jabber_chatrooms_logged_out_cb        (GossipJabber           *jabber,
							       GossipAccount          *account,
							       gint                    reason,
							       GossipJabberChatrooms  *chatrooms);
static JabberChatroom * jabber_chatrooms_chatroom_new         (GossipJabberChatrooms  *chatrooms,
							       GossipChatroom         *chatroom);
static JabberChatroom * jabber_chatrooms_chatroom_ref         (JabberChatroom         *room);
static void             jabber_chatrooms_chatroom_unref       (JabberChatroom         *room);
static GossipChatroomId jabber_chatrooms_chatroom_get_id      (JabberChatroom         *room);
static GossipContact *  jabber_chatrooms_get_contact          (JabberChatroom         *room,
							       GossipJID              *jid,
							       gboolean               *new_contact);
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
static GossipChatroomError
                        jabber_chatrooms_error_from_code      (gint                    code);
static GArray *         jabber_chatrooms_get_status           (LmMessage              *m);
static gboolean         jabber_chatrooms_has_status           (GArray                 *status, 
							       gint                    code);
static void             jabber_chatrooms_get_rooms_foreach    (gpointer                key,
							       JabberChatroom         *room,
							       GList                 **list);
static void             jabber_chatrooms_browse_rooms_cb      (GossipJabberDisco      *disco,
							       GossipJabberDiscoItem  *item,
							       gboolean                last_item,
							       gboolean                timeout,
							       GError                 *error,
							       GossipCallbackData     *data);
static void             jabber_chatrooms_set_presence_foreach (gpointer                key,
							       JabberChatroom         *room,
							       GossipJabberChatrooms  *chatrooms);

static LmMessageNode *  jabber_chatrooms_find_muc_user_node   (LmMessageNode          *parent_node);
static GossipChatroomRole 
                        jabber_chatrooms_get_role             (LmMessageNode          *muc_node);
static GossipChatroomAffiliation 
                        jabber_chatrooms_get_affiliation      (LmMessageNode       *muc_node);

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

	g_signal_connect (chatrooms->jabber, "disconnected",
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

	g_signal_handlers_disconnect_by_func (chatrooms->jabber,
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
jabber_chatrooms_logged_out_cb (GossipJabber          *jabber,
				GossipAccount         *account,
				gint                   reason,
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
	if (!room) {
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	id = jabber_chatrooms_chatroom_get_id (room);

	node = lm_message_node_get_child (m->node, "body");
	if (node) {
		if (gossip_jid_get_resource (jid) == NULL) {
			g_signal_emit_by_name (chatrooms->jabber,
					       "chatroom-new-event",
					       id, node->value);
		} else {
			GossipTime timestamp;

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
		contact = jabber_chatrooms_get_contact (room, jid, NULL);

		g_signal_emit_by_name (chatrooms->jabber,
				       "chatroom-topic-changed",
				       id, contact, node->value);
	}

	gossip_jid_unref (jid);

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;

}

static GossipContact *
jabber_chatrooms_get_contact (JabberChatroom *room,
			      GossipJID      *jid,
			      gboolean       *new_contact)
{
	GossipContact *contact;
	GSList        *l;
	const gchar   *id;
	const gchar   *name;

	id = gossip_jid_get_full (jid);

	for (l = room->contacts; l; l = l->next) {
		contact = GOSSIP_CONTACT (l->data);

		if (g_ascii_strcasecmp (gossip_contact_get_id (contact), id) == 0) {
			if (new_contact) {
				*new_contact = FALSE;
			}

			return contact;
		}
	}

	if (new_contact) {
		*new_contact = TRUE;
	}

	name = gossip_jid_get_resource (jid);
	if (!name) {
		name = id;
	}

	contact = g_object_new (GOSSIP_TYPE_CONTACT,
				"account", gossip_contact_get_account (room->own_contact),
				"id", id,
				"name", name,
				NULL);

	room->contacts = g_slist_prepend (room->contacts, contact);

	return contact;
}

static GossipChatroomError 
jabber_chatrooms_error_from_code (gint code)
{
	switch (code) {
	case 401: return GOSSIP_CHATROOM_ERROR_PASSWORD_INVALID_OR_MISSING;
	case 403: return GOSSIP_CHATROOM_ERROR_USER_BANNED;
	case 404: return GOSSIP_CHATROOM_ERROR_ROOM_NOT_FOUND;
	case 405: return GOSSIP_CHATROOM_ERROR_ROOM_CREATION_RESTRICTED;
	case 406: return GOSSIP_CHATROOM_ERROR_USE_RESERVED_ROOM_NICK;
	case 407: return GOSSIP_CHATROOM_ERROR_NOT_ON_MEMBERS_LIST;
	case 409: return GOSSIP_CHATROOM_ERROR_NICK_IN_USE;
	case 503: return GOSSIP_CHATROOM_ERROR_MAXIMUM_USERS_REACHED;

	/* Legacy Errors */
	case 502: return GOSSIP_CHATROOM_ERROR_ROOM_NOT_FOUND;
	case 504: return GOSSIP_CHATROOM_ERROR_TIMED_OUT;

	default:
		break;
	}
	
	return GOSSIP_CHATROOM_ERROR_UNKNOWN;
}

static GArray *
jabber_chatrooms_get_status (LmMessage *m)
{
	LmMessageNode *node;
	GArray        *status = NULL;

	if (!m) {
		return NULL;
	}

	node = lm_message_node_get_child (m->node, "x");
	if (!node) {
		return NULL;
	}

	node = node->children;

	while (node) {
		if (!node) {
			break;
		}

		if (node->name && strcmp (node->name, "status") == 0) {
			const gchar *code;

			code = lm_message_node_get_attribute (node, "code");
			if (code) {
				gint value;

				if (!status) {
					status = g_array_new (FALSE, FALSE, sizeof (gint));
				}

				value = atoi (code);
				g_array_append_val (status, value);
			}
		}

		node = node->next;
	}

	return status;
}

static gboolean 
jabber_chatrooms_has_status (GArray *status, 
			     gint    code)
{
	gint i = 0;

	if (!status) {
		return FALSE;
	}
	
	for (i = 0; i < status->len; i++) {
		if (g_array_index (status, gint, i) == code) {
			return TRUE;
		}
	}
	
	return FALSE;
}

static LmHandlerResult
jabber_chatrooms_presence_handler (LmMessageHandler      *handler,
				   LmConnection          *conn,
				   LmMessage             *m,
				   GossipJabberChatrooms *chatrooms)
{
	const gchar               *from;
	GossipJID                 *jid;
	JabberChatroom            *room;
	GossipContact             *contact;
	GossipPresence            *presence;
	GossipPresenceState        p_state;
	GossipChatroomId           id;
	LmMessageSubType           type;
	LmMessageNode             *node;
	gboolean                   new_contact;
	gboolean                   was_offline;
	LmMessageNode             *muc_user_node;
	GossipChatroomContactInfo  muc_contact_info;

	from = lm_message_node_get_attribute (m->node, "from");
	jid = gossip_jid_new (from);

	room = g_hash_table_lookup (chatrooms->room_jid_hash, jid);
	if (!room) {
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	id = jabber_chatrooms_chatroom_get_id (room);

	gossip_debug (DEBUG_DOMAIN, "Presence from: %s", from);

	type = lm_message_get_sub_type (m);
	switch (type) {
	case LM_MESSAGE_SUB_TYPE_AVAILABLE:
		/* get details */
		contact = jabber_chatrooms_get_contact (room, jid,
							&new_contact);

		presence = gossip_presence_new ();
		node = lm_message_node_get_child (m->node, "show");
		if (node) {
			p_state = gossip_jabber_presence_state_from_str (node->value);
			gossip_presence_set_state (presence, p_state);
		}
		node = lm_message_node_get_child (m->node, "status");
		if (node) {
			gossip_presence_set_status (presence, node->value);
		}

		/* Should signal joined if contact was found but offline */
		was_offline = !gossip_contact_is_online (contact);
		gossip_contact_add_presence (contact, presence);
		g_object_unref (presence);

		muc_user_node = jabber_chatrooms_find_muc_user_node (m->node);
		muc_contact_info.role = jabber_chatrooms_get_role (muc_user_node);
		muc_contact_info.affiliation = jabber_chatrooms_get_affiliation (muc_user_node);

		/* Is contact new or updated */
		if (new_contact || was_offline) {
			gossip_debug (DEBUG_DOMAIN, "ID[%d] Presence for new joining contact:'%s'",
				      id, gossip_jid_get_full (jid));
			gossip_chatroom_contact_joined (room->chatroom,
							contact,
							&muc_contact_info);
		} else {
			gossip_chatroom_set_contact_info (room->chatroom,
							  contact,
							  &muc_contact_info);
		}
		break;

	case LM_MESSAGE_SUB_TYPE_UNAVAILABLE:
		contact = jabber_chatrooms_get_contact (room, jid, NULL);
		if (contact) {
			gossip_debug (DEBUG_DOMAIN, "ID[%d] Contact left:'%s'",
				      id, gossip_jid_get_full (jid));
			gossip_chatroom_contact_left (room->chatroom, contact);
			room->contacts = g_slist_remove (room->contacts, contact);
			g_object_unref (contact);
		}
		break;

	case LM_MESSAGE_SUB_TYPE_ERROR:
		node = lm_message_node_get_child (m->node, "error");
		if (node) {
			GossipChatroomError  error;
			const gchar         *str;
			gint                 code;
			
			str = lm_message_node_get_attribute (node, "code");
			code = str ? atoi (str) : 0;
			
			error = jabber_chatrooms_error_from_code (code);
			gossip_debug (DEBUG_DOMAIN, "ID[%d] %s", 
				      id, gossip_chatroom_provider_error_to_string (error));
			
			g_signal_emit_by_name (chatrooms->jabber,
					       "chatroom-error",
					       id, error);
		}
		break;

	default:
		gossip_debug (DEBUG_DOMAIN, "Presence not handled for:'%s'",
			      gossip_jid_get_full (jid));
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
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
	gossip_debug (DEBUG_DOMAIN, "ID[%d] Join timed out (internally)", id);

	/* Set chatroom status and error */
	gossip_chatroom_set_status (room->chatroom, GOSSIP_CHATROOM_STATUS_ERROR);

	last_error = gossip_chatroom_provider_error_to_string (GOSSIP_CHATROOM_ERROR_TIMED_OUT);
	gossip_chatroom_set_last_error (room->chatroom, last_error);

	/* Call callback */
	chatrooms = room->chatrooms;

	if (room->callback != NULL) {
		gossip_debug (DEBUG_DOMAIN, "ID[%d] Calling back... (timed out)", id);
		(room->callback) (GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
				  id,
				  GOSSIP_CHATROOM_ERROR_TIMED_OUT,
				  room->user_data);
	}

	/* Clean up */
	g_hash_table_remove (chatrooms->room_id_hash, GINT_TO_POINTER (id));
	g_hash_table_remove (chatrooms->room_jid_hash, room->jid);

	/* Clean up callback data */
	room->callback = NULL;
	room->user_data = NULL;

	jabber_chatrooms_chatroom_unref (room);

	return FALSE;
}

static void
jabber_chatrooms_create_instant_room (JabberChatroom *room)
{
	LmMessage     *m;
	LmMessageNode *node;
	GossipAccount *account;
	gchar         *from;
	const gchar   *to;

	m = lm_message_new_with_sub_type (NULL,
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_SET);

	account = gossip_contact_get_account (room->own_contact);
	from = g_strconcat (gossip_account_get_id (account),
			    "/",
			    gossip_account_get_resource (account),
			    NULL);
	lm_message_node_set_attribute (m->node, "from", from);
	g_free (from);

	to = gossip_chatroom_get_id_str (room->chatroom);
	lm_message_node_set_attribute (m->node, "to", to);

	node = lm_message_node_add_child (m->node, "query", NULL);
        lm_message_node_set_attributes (node, "xmlns", XMPP_MUC_OWNER_XMLNS, NULL);

        node = lm_message_node_add_child (node, "x", NULL);
        lm_message_node_set_attributes (node, 
					"xmlns", "jabber:x:data", 
					"type", "submit",
					NULL);

	lm_connection_send (room->connection, m,  NULL);
	lm_message_unref (m);
}

static void
jabber_chatrooms_create_reserved_room (JabberChatroom *room)
{
	LmMessage     *m;
	LmMessageNode *node;
	LmMessageNode *child;
	GossipAccount *account;
	gchar         *from;
	const gchar   *to;
	const gchar   *name;
	const gchar   *password;

	m = lm_message_new_with_sub_type (NULL,
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_SET);

	account = gossip_contact_get_account (room->own_contact);
	from = g_strconcat (gossip_account_get_id (account),
			    "/",
			    gossip_account_get_resource (account),
			    NULL);
	lm_message_node_set_attribute (m->node, "from", from);
	g_free (from);

	to = gossip_chatroom_get_id_str (room->chatroom);
	lm_message_node_set_attribute (m->node, "to", to);

	node = lm_message_node_add_child (m->node, "query", NULL);
        lm_message_node_set_attributes (node, "xmlns", XMPP_MUC_OWNER_XMLNS, NULL);

        node = lm_message_node_add_child (node, "x", NULL);
        lm_message_node_set_attributes (node, 
					"xmlns", "jabber:x:data", 
					"type", "submit",
					NULL);

	/* FIXME: This is a shortcut for now, we should use their forms */
	name = gossip_chatroom_get_name (room->chatroom);
	password = gossip_chatroom_get_password (room->chatroom);

        child = lm_message_node_add_child (node, "field", NULL);
        lm_message_node_set_attributes (child, "var", "muc#roomconfig_roomname", NULL);
        lm_message_node_add_child (child, "value", name);
	
        child = lm_message_node_add_child (node, "field", NULL);
        lm_message_node_set_attributes (child, "var", "muc#roomconfig_passwordprotectedroom", NULL);
        lm_message_node_add_child (child, "value", password ? "1" : "0");

	child = lm_message_node_add_child (node, "field", NULL);
	lm_message_node_set_attributes (child, "var", "muc#roomconfig_roomsecret", NULL);
	lm_message_node_add_child (child, "value", password ? password : "");
	
	/* Finally send */
	lm_connection_send (room->connection, m,  NULL);
	lm_message_unref (m);
}

static void
jabber_chatrooms_request_reserved_room (JabberChatroom *room)
{
	LmMessage     *m;
	LmMessageNode *node;
	GossipAccount *account;
	gchar         *from;
	const gchar   *to;

	m = lm_message_new_with_sub_type (NULL,
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_GET);

	account = gossip_contact_get_account (room->own_contact);
	from = g_strconcat (gossip_account_get_id (account),
			    "/",
			    gossip_account_get_resource (account),
			    NULL);
	lm_message_node_set_attribute (m->node, "from", from);
	g_free (from);

	to = gossip_chatroom_get_id_str (room->chatroom);
	lm_message_node_set_attribute (m->node, "to", to);

	node = lm_message_node_add_child (m->node, "query", NULL);
        lm_message_node_set_attributes (node, "xmlns", XMPP_MUC_OWNER_XMLNS, NULL);

	lm_connection_send (room->connection, m,  NULL);
	lm_message_unref (m);
}

static LmHandlerResult
jabber_chatrooms_join_cb (LmMessageHandler *handler,
			  LmConnection     *connection,
			  LmMessage        *m,
			  JabberChatroom   *room)
{
	GossipJabberChatrooms *chatrooms;
	GossipChatroomError    error;
	GossipChatroomStatus   status;
	GossipChatroomId       id;
	GossipChatroomId       id_found;
	LmMessageSubType       type;
	LmMessageNode         *node = NULL;
	const gchar           *from;
	GossipJID             *jid;
	JabberChatroom        *room_found;
	GArray                *status_codes;
	gboolean               room_match = FALSE;

	if (!room || !room->join_handler) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	chatrooms = room->chatrooms;

	/* Get room id */
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

	/* Clean up the join timeout */
	if (room->timeout_id) {
		g_source_remove (room->timeout_id);
		room->timeout_id = 0;
	}

	/* Clean up handler */
	if (room->join_handler) {
		lm_message_handler_unref (room->join_handler);
		room->join_handler = NULL;
	}

	/* Check status code */
	status_codes = jabber_chatrooms_get_status (m);
	if (status_codes) {
		if (jabber_chatrooms_has_status (status_codes, 201)) {
			/* Room was created for us */
			if (0) {
				jabber_chatrooms_request_reserved_room (room);
				jabber_chatrooms_create_instant_room (room); 
			} else {
				jabber_chatrooms_create_reserved_room (room);
			}
		}

		g_array_free (status_codes, TRUE);
	}

	/* Check for error */
	type = lm_message_get_sub_type (m);
	if (type == LM_MESSAGE_SUB_TYPE_ERROR) {
		node = lm_message_node_get_child (m->node, "error");
	}

	if (node) {
		const gchar *str;
		gint         code;

		str = lm_message_node_get_attribute (node, "code");
		code = str ? atoi (str) : 0;

		error = jabber_chatrooms_error_from_code (code);
		gossip_debug (DEBUG_DOMAIN, "ID[%d] %s", 
			      id, gossip_chatroom_provider_error_to_string (error));

		/* Set room state */
		status = GOSSIP_CHATROOM_STATUS_ERROR;
	} else {
		error = GOSSIP_CHATROOM_ERROR_NONE;
		status = GOSSIP_CHATROOM_STATUS_ACTIVE;
	}

	gossip_chatroom_set_status (room->chatroom, status);
	gossip_chatroom_set_last_error (room->chatroom,
					gossip_chatroom_provider_error_to_string (error));

	if (room->callback != NULL) {
		gossip_debug (DEBUG_DOMAIN, "ID[%d] Calling back...", id);
		(room->callback) (GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
				  id, 
				  error, 
				  room->user_data);
	}

	/* Clean up callback data */
	room->callback = NULL;
	room->user_data = NULL;

	/* Articulate own contact presence so we appear in group chat */
	if (error == GOSSIP_CHATROOM_ERROR_NONE) {
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

GossipChatroomId
gossip_jabber_chatrooms_join (GossipJabberChatrooms *chatrooms,
			      GossipChatroom        *chatroom,
			      GossipChatroomJoinCb   callback,
			      gpointer               user_data)
{
	GossipChatroomId   id;
	JabberChatroom    *room, *existing_room;
	LmMessage         *m;
	LmMessageNode     *node;
	const gchar       *show = NULL;
	gchar             *id_str;
	const gchar       *password;

	g_return_val_if_fail (chatrooms != NULL, 0);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), 0);
	g_return_val_if_fail (callback != NULL, 0);

	room = jabber_chatrooms_chatroom_new (chatrooms, chatroom);
	existing_room = g_hash_table_lookup (chatrooms->room_jid_hash, room->jid);

	if (existing_room) {
		jabber_chatrooms_chatroom_unref (room);

		/* Duplicate room already exists. */
		id = jabber_chatrooms_chatroom_get_id (existing_room);

		gossip_debug (DEBUG_DOMAIN, "ID[%d] Join chatroom:'%s', room already exists.",
			      id,
			      gossip_chatroom_get_room (chatroom));

		(callback) (GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
			    id, 
			    GOSSIP_CHATROOM_ERROR_ALREADY_OPEN,
			    user_data);

		return id;
	}

	/* Get real chatroom. */
	id = jabber_chatrooms_chatroom_get_id (room);

	gossip_debug (DEBUG_DOMAIN, "ID[%d] Join chatroom:'%s' on server:'%s'",
		      id,
		      gossip_chatroom_get_room (chatroom),
		      gossip_chatroom_get_server (chatroom));


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

	node = lm_message_node_add_child (m->node, "x", NULL);
	lm_message_node_set_attribute (node, "xmlns", XMPP_MUC_XMLNS);

	/* If we have a password, set one */
	password = gossip_chatroom_get_password (chatroom);
	if (!G_STR_EMPTY (password)) {
		lm_message_node_add_child (node, "password", password);
	}

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

	id_str = g_strdup_printf ("muc_join_%d", id);
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
	 * are waiting forever. 
	 */
	lm_connection_send (chatrooms->connection, m,  NULL);
	lm_message_unref (m);

	return id;
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
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "ID[%d] Cancel joining room", id);

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
		gossip_debug (DEBUG_DOMAIN, "ID[%d] Calling back...", id);
		(room->callback) (GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
				  id, 
				  GOSSIP_CHATROOM_ERROR_CANCELED, 
				  room->user_data);
	}

	/* Clean up callback data */
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

	room = g_hash_table_lookup (chatrooms->room_id_hash,
				    GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("ProtocolChatrooms: Unknown chatroom id: %d", id);
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "ID[%d] Send message", id);

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

	room = g_hash_table_lookup (chatrooms->room_id_hash,
				    GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("ProtocolChatrooms: Unknown chatroom id: %d", id);
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "ID[%d] Change topic to:'%s'",
		      id, new_topic);

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

	room = g_hash_table_lookup (chatrooms->room_id_hash,
				    GINT_TO_POINTER (id));
	if (!room) {
		g_warning ("ProtocolChatrooms: Unknown chatroom id: %d", id);
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "ID[%d] Change chatroom nick to:'%s'",
		      id, new_nick);

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

	room = g_hash_table_lookup (chatrooms->room_id_hash,
				    GINT_TO_POINTER (id));
	if (!room) {
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

	gossip_debug (DEBUG_DOMAIN, "ID[%d] Leaving room, ref count is %d",
		      id, room->ref_count - 1);

	jabber_chatrooms_chatroom_unref (room);
}

GossipChatroom *
gossip_jabber_chatrooms_find_by_id (GossipJabberChatrooms *chatrooms,
				    GossipChatroomId       id)
{
	JabberChatroom *room;

	g_return_val_if_fail (chatrooms != NULL, NULL);

	room = g_hash_table_lookup (chatrooms->room_id_hash,
				    GINT_TO_POINTER (id));
	if (!room) {
		return NULL;
	}

	return room->chatroom;
}

GossipChatroom *
gossip_jabber_chatrooms_find (GossipJabberChatrooms *chatrooms,
			      GossipChatroom        *chatroom)
{
	JabberChatroom *room;
	GossipJID      *jid;
	gchar          *jid_str;

	g_return_val_if_fail (chatrooms != NULL, NULL);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	jid_str = g_strdup_printf ("%s/%s",
				   gossip_chatroom_get_id_str (chatroom),
				   gossip_chatroom_get_nick (chatroom));
	jid = gossip_jid_new (jid_str);
	g_free (jid_str);

	room = g_hash_table_lookup (chatrooms->room_jid_hash, jid);
	gossip_jid_unref (jid);

	if (!room) {
		return NULL;
	}

	return room->chatroom;
}

void
gossip_jabber_chatrooms_invite (GossipJabberChatrooms *chatrooms,
				GossipChatroomId       id,
				GossipContact         *contact,
				const gchar           *reason)
{
	LmMessage      *m;
	LmMessageNode  *parent;
	LmMessageNode  *node;
	JabberChatroom *room;

	g_return_if_fail (chatrooms != NULL);
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	room = g_hash_table_lookup (chatrooms->room_id_hash,
				    GINT_TO_POINTER (id));

	if (!room) {
		g_warning ("ProtocolChatrooms: Unknown chatroom id: %d", id);
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "ID[%d] Invitation to contact:'%s' from:'%s'",
		      id,
		      gossip_contact_get_id (contact),
		      gossip_contact_get_id (room->own_contact));

	m = lm_message_new (gossip_jid_get_without_resource (room->jid),
			    LM_MESSAGE_TYPE_MESSAGE);
	lm_message_node_set_attributes (m->node,
					"from", gossip_contact_get_id (room->own_contact),
					NULL);

	parent = lm_message_node_add_child (m->node, "x", NULL);
	lm_message_node_set_attributes (parent, "xmlns", XMPP_MUC_USER_XMLNS, NULL);

	node = lm_message_node_add_child (parent, "invite", NULL);
	lm_message_node_set_attributes (node, "to", gossip_contact_get_id (contact), NULL);

	lm_message_node_add_child (node, "reason", reason);

	lm_connection_send (chatrooms->connection, m, NULL);
	lm_message_unref (m);
}

void
gossip_jabber_chatrooms_invite_accept (GossipJabberChatrooms *chatrooms,
				       GossipChatroomJoinCb   callback,
				       GossipChatroomInvite  *invite,
				       const gchar           *nickname)
{
	GossipChatroom *chatroom;
	GossipContact  *contact;
	GossipAccount  *account;
	gchar          *room = NULL;
	const gchar    *id;
	const gchar    *server;

	g_return_if_fail (chatrooms != NULL);
	g_return_if_fail (invite != NULL);
	g_return_if_fail (callback != NULL);

	id = gossip_chatroom_invite_get_id (invite);
	contact = gossip_chatroom_invite_get_invitor (invite);

	server = strstr (id, "@");

	g_return_if_fail (server != NULL);
	g_return_if_fail (nickname != NULL);

	if (server) {
		room = g_strndup (id, server - id);
		server++;
	}

	account = gossip_contact_get_account (contact);

	chatroom = g_object_new (GOSSIP_TYPE_CHATROOM,
				 "type", GOSSIP_CHATROOM_TYPE_NORMAL,
				 "account", account,
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

void
gossip_jabber_chatrooms_invite_decline (GossipJabberChatrooms *chatrooms,
					GossipChatroomInvite  *invite,
					const gchar           *reason)
{
	LmMessage     *m;
	LmMessageNode *n;
	GossipContact *own_contact;
	GossipContact *contact;
	const gchar   *id;

	g_return_if_fail (chatrooms != NULL);
	g_return_if_fail (invite != NULL);

	own_contact = gossip_jabber_get_own_contact (chatrooms->jabber);
	contact = gossip_chatroom_invite_get_invitor (invite);
	id = gossip_chatroom_invite_get_id (invite);

	gossip_debug (DEBUG_DOMAIN, "Invitation decline to:'%s' into room:'%s'",
		      gossip_contact_get_id (contact), id);

	m = lm_message_new (id, LM_MESSAGE_TYPE_MESSAGE);
	lm_message_node_set_attributes (m->node,
					"from", gossip_contact_get_id (own_contact),
					NULL);

	n = lm_message_node_add_child (m->node, "x", NULL);
	lm_message_node_set_attributes (n, "xmlns", XMPP_MUC_USER_XMLNS, NULL);

	n = lm_message_node_add_child (n, "decline", NULL);
	lm_message_node_set_attributes (n, "to", gossip_contact_get_id (contact), NULL);

	n = lm_message_node_add_child (n, "reason", reason);

	lm_connection_send (chatrooms->connection, m, NULL);
	lm_message_unref (m);
}

static void
jabber_chatrooms_get_rooms_foreach (gpointer               key,
				    JabberChatroom        *room,
				    GList                **list)
{
	*list = g_list_append (*list, key);
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
jabber_chatrooms_browse_rooms_cb (GossipJabberDisco     *disco,
				  GossipJabberDiscoItem *item,
				  gboolean               last_item,
				  gboolean               timeout,
				  GError                *error,
				  GossipCallbackData    *data)
{
	GossipJID             *jid = NULL;
	GossipJabberChatrooms *chatrooms;
	GossipChatroom        *chatroom = NULL;
	JabberChatroom        *room = NULL;
	GList                 *list;
	gchar                 *server;

	if (timeout && !last_item) {
		return;
	}

	chatrooms = data->data1;
	server = data->data2;
	list = data->data3;

	if (item) {
		jid = gossip_jabber_disco_item_get_jid (item);
		room = g_hash_table_lookup (chatrooms->room_jid_hash, jid);
	}

	if (room) {
		chatroom = room->chatroom;
	} 

	if (!room && !timeout && !error) {
		GossipAccount         *account;
		const gchar           *server;
		gchar                 *room;
		
		gossip_debug (DEBUG_DOMAIN, 
			      "Chatroom found on server not set up here, creating for:'%s'...",
			      gossip_jid_get_full (jid));

		account = gossip_jabber_get_account (chatrooms->jabber);
		server = gossip_jid_get_part_host (jid);
		room = gossip_jid_get_part_name (jid);

		/* Create new chatroom */
		chatroom = g_object_new (GOSSIP_TYPE_CHATROOM,
					 "type", GOSSIP_CHATROOM_TYPE_NORMAL,
					 "account", account,
					 "server", server,
					 "room", room,
					 NULL);

		g_free (room);
	}

	if (chatroom) {
		GossipChatroomFeature  features = 0;
		LmMessageNode         *node;
		const gchar           *name;

		name = gossip_jabber_disco_item_get_name (item);
		gossip_chatroom_set_name (chatroom, name);

		/* Sort ouf the features */
		if (gossip_jabber_disco_item_has_feature (item, "muc_hidden")) { 
			features |= GOSSIP_CHATROOM_FEATURE_HIDDEN;
		}

		if (gossip_jabber_disco_item_has_feature (item, "muc_membersonly")) { 
			features |= GOSSIP_CHATROOM_FEATURE_MEMBERS_ONLY;
		}

		if (gossip_jabber_disco_item_has_feature (item, "muc_moderated")) { 
			features |= GOSSIP_CHATROOM_FEATURE_MODERATED;
		}

		if (gossip_jabber_disco_item_has_feature (item, "muc_nonanonymous")) { 
			features |= GOSSIP_CHATROOM_FEATURE_NONANONYMOUS;
		}

		if (gossip_jabber_disco_item_has_feature (item, "muc_open")) { 
			features |= GOSSIP_CHATROOM_FEATURE_OPEN;
		}

		if (gossip_jabber_disco_item_has_feature (item, "muc_passwordprotected")) { 
			features |= GOSSIP_CHATROOM_FEATURE_PASSWORD_PROTECTED;
		}

		if (gossip_jabber_disco_item_has_feature (item, "muc_persistent")) { 
			features |= GOSSIP_CHATROOM_FEATURE_PERSISTENT;
		}

		if (gossip_jabber_disco_item_has_feature (item, "muc_public")) { 
			features |= GOSSIP_CHATROOM_FEATURE_PUBLIC;
		}

		if (gossip_jabber_disco_item_has_feature (item, "muc_semianonymous")) { 
			features |= GOSSIP_CHATROOM_FEATURE_SEMIANONYMOUS;
		}

		if (gossip_jabber_disco_item_has_feature (item, "muc_temporary")) { 
			features |= GOSSIP_CHATROOM_FEATURE_TEMPORARY;
		}

		if (gossip_jabber_disco_item_has_feature (item, "muc_unmoderated")) { 
			features |= GOSSIP_CHATROOM_FEATURE_PERSISTENT;
		}

		if (gossip_jabber_disco_item_has_feature (item, "muc_unsecured")) { 
			features |= GOSSIP_CHATROOM_FEATURE_UNSECURED;
		}

		gossip_chatroom_set_features (chatroom, features);

		/* Get the MUC specific data */
		node = gossip_jabber_disco_item_get_data (item);
		if (node) {
			node = node->children;

			while (node) {
				if (node->name && strcmp (node->name, "field") == 0) {
					const gchar *var;
					const gchar *val;

					var = lm_message_node_get_attribute (node, "var");
					val = lm_message_node_get_value (node->children);

					if (var && val) {
						if (strcmp (var, "muc#roominfo_description") == 0) {
							gossip_chatroom_set_description (chatroom, val);
						} 
						else if (strcmp (var, "muc#roominfo_subject") == 0) {
							gossip_chatroom_set_subject (chatroom, val);
						}
						else if (strcmp (var, "muc#roominfo_occupants") == 0) {
							gossip_chatroom_set_occupants (chatroom, atoi (val));
						}
					}
				}

				node = node->next;
			}
		}

		gossip_debug (DEBUG_DOMAIN, 
			      "Chatroom:'%s' added to list found on server:'%s'...",
			      gossip_chatroom_get_room (chatroom),
			      gossip_chatroom_get_server (chatroom));

		list = g_list_prepend (list, chatroom);
		data->data3 = list;
	}

	if (last_item) {
		GossipChatroomBrowseCb callback;
		
		callback = data->callback;
		(callback)(GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
			   server, list, error, data->user_data);

		g_list_foreach (list, (GFunc) g_object_unref, NULL);
		g_list_free (list);

		g_free (server);

		g_free (data);
	}
}

void
gossip_jabber_chatrooms_browse_rooms (GossipJabberChatrooms  *chatrooms,
				      const gchar            *server,
				      GossipChatroomBrowseCb  callback,
				      gpointer                user_data)
{
	GossipJabberDisco  *disco;
	GossipCallbackData *data;
	
	data = g_new0 (GossipCallbackData, 1);
	
	data->callback = callback;
	data->user_data = user_data;
	data->data1 = chatrooms;
	data->data2 = g_strdup (server);

	disco = gossip_jabber_disco_request (chatrooms->jabber,
					     server,
					     (GossipJabberDiscoItemFunc) 
					     jabber_chatrooms_browse_rooms_cb,
					     data);
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

	show = gossip_jabber_presence_state_to_str (presence);
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

static LmMessageNode *
jabber_chatrooms_find_muc_user_node (LmMessageNode *parent_node)
{
	LmMessageNode *child;

	/* Should have a function in Loudmouth to find a child with xmlns */
	child = parent_node->children;

	if (!child) {
		return NULL;
	}

	while (child) {
		if (strcmp (child->name, "x") == 0) {
			const gchar *xmlns;

			xmlns = lm_message_node_get_attribute (child, "xmlns");

			if (xmlns && strcmp (xmlns, XMPP_MUC_USER_XMLNS) == 0) {
				return child;
			}
		}

		child = child->next;
	}

	return NULL;
}

static GossipChatroomRole
jabber_chatrooms_get_role (LmMessageNode *muc_node)
{
	LmMessageNode *item_node;
	const gchar   *role;

	if (!muc_node) {
		return GOSSIP_CHATROOM_ROLE_NONE;
	}

	item_node = lm_message_node_get_child (muc_node, "item");
	if (!item_node) {
		return GOSSIP_CHATROOM_ROLE_NONE;
	}

	role = lm_message_node_get_attribute (item_node, "role");
	if (!role) {
		return GOSSIP_CHATROOM_ROLE_NONE;
	}

	if (strcmp (role, "moderator") == 0) {
		return GOSSIP_CHATROOM_ROLE_MODERATOR;
	}
	else if (strcmp (role, "participant") == 0) {
		return GOSSIP_CHATROOM_ROLE_PARTICIPANT;
	}
	else if (strcmp (role, "visitor") == 0) {
		return GOSSIP_CHATROOM_ROLE_VISITOR;
	} else {
		return GOSSIP_CHATROOM_ROLE_NONE;
	}
}

static GossipChatroomAffiliation
jabber_chatrooms_get_affiliation (LmMessageNode *muc_node)
{
	LmMessageNode *item_node;
	const gchar   *affiliation;

	if (!muc_node) {
		return GOSSIP_CHATROOM_AFFILIATION_NONE;
	}

	item_node = lm_message_node_get_child (muc_node, "item");
	if (!item_node) {
		return GOSSIP_CHATROOM_AFFILIATION_NONE;
	}

	affiliation = lm_message_node_get_attribute (item_node, "affiliation");
	if (!affiliation) {
		return GOSSIP_CHATROOM_AFFILIATION_NONE;
	}

	if (strcmp (affiliation, "owner") == 0) {
		return GOSSIP_CHATROOM_AFFILIATION_OWNER;
	}
	else if (strcmp (affiliation, "admin") == 0) {
		return GOSSIP_CHATROOM_AFFILIATION_ADMIN;
	}
	else if (strcmp (affiliation, "member") == 0) {
		return GOSSIP_CHATROOM_AFFILIATION_MEMBER;
	}
	else if (strcmp (affiliation, "outcast") == 0) {
		return GOSSIP_CHATROOM_AFFILIATION_OUTCAST;
	} else {
		return GOSSIP_CHATROOM_AFFILIATION_NONE;
	}
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
