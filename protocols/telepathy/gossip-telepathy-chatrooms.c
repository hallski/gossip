/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Raphael Slinckx <raphael@slinckx.net>
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

#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-chan.h>
#include <libtelepathy/tp-chan-gen.h>
#include <libtelepathy/tp-interfaces.h>
#include <libtelepathy/tp-constants.h>

#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-chatroom-contact.h>

#include "gossip-telepathy-chatrooms.h"
#include "gossip-telepathy-group.h"
#include "gossip-telepathy-message.h"
#include "gossip-telepathy-private.h"


#define DEBUG_DOMAIN "TelepathyChatrooms"

struct _GossipTelepathyChatrooms {
	GossipTelepathy        *telepathy;
	GossipTelepathyMessage *message;
	GHashTable             *rooms;
};

typedef struct {
	GossipTelepathy      *telepathy;
	GossipChatroom       *chatroom;
	GossipTelepathyGroup *group;
	TpChan               *room_channel;
} TelepathyChatroom;

static void     telepathy_chatrooms_disconnected_cb    (GossipProtocol            *telepathy,
							GossipAccount             *account,
							gint                       reason,
							GossipTelepathyChatrooms  *chatrooms);
static void     telepathy_chatrooms_closed_cb          (TpChan                    *tp_chan,
							GossipTelepathyChatrooms  *chatrooms);
static gboolean telepathy_chatrooms_find_chan          (guint                      key,
							TelepathyChatroom         *room,
							TpChan                    *tp_chan);
static void     telepathy_chatrooms_chatroom_free      (TelepathyChatroom         *room);
static void     telepathy_chatrooms_get_rooms_foreach  (gpointer                   key,
							TelepathyChatroom         *room,
							GList                    **list);
static void     telepathy_chatrooms_members_added_cb   (GossipTelepathyGroup      *group,
							GArray                    *members,
							TelepathyChatroom         *room);
static void     telepathy_chatrooms_members_removed_cb (GossipTelepathyGroup      *group,
							GArray                    *members,
							TelepathyChatroom         *room);

GossipTelepathyChatrooms *
gossip_telepathy_chatrooms_init (GossipTelepathy *telepathy)
{
	GossipTelepathyChatrooms *chatrooms;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY (telepathy), NULL);

	chatrooms = g_new0 (GossipTelepathyChatrooms, 1);
	chatrooms->telepathy = telepathy;
	chatrooms->rooms = g_hash_table_new_full (g_direct_hash,
						  g_direct_equal,
						  NULL,
						  (GDestroyNotify)
						  telepathy_chatrooms_chatroom_free);
	chatrooms->message = gossip_telepathy_message_init (telepathy);

	g_signal_connect (telepathy, "disconnected",
			  G_CALLBACK (telepathy_chatrooms_disconnected_cb),
			  chatrooms);

	return chatrooms;
}

static void
telepathy_chatrooms_disconnected_cb (GossipProtocol           *telepathy,
				     GossipAccount            *account,
				     gint                      reason,
				     GossipTelepathyChatrooms *chatrooms)
{
	g_hash_table_remove_all (chatrooms->rooms);
}

void
gossip_telepathy_chatrooms_finalize (GossipTelepathyChatrooms *chatrooms)
{
	g_return_if_fail (chatrooms != NULL);

	gossip_telepathy_message_finalize (chatrooms->message);
	g_hash_table_destroy (chatrooms->rooms);
	g_free (chatrooms);
}

GossipChatroomId
gossip_telepathy_chatrooms_join (GossipTelepathyChatrooms *chatrooms,
				 GossipChatroom           *room,
				 GossipChatroomJoinCb      callback,
				 gpointer                  user_data)
{
	GossipChatroomId   room_id;
	TelepathyChatroom *tp_room;

	room_id = gossip_chatroom_get_id (room);
	tp_room = g_hash_table_lookup (chatrooms->rooms,
				       GINT_TO_POINTER (room_id));
	if (!tp_room) {
		const char  *room_names[2] = {NULL, NULL};
		GArray      *room_handles;
		guint        room_handle;
		gchar       *room_object_path;
		GError      *error = NULL;
		TpConn      *tp_conn;
		TpChan      *room_channel;
		const gchar *bus_name;

		tp_conn = gossip_telepathy_get_connection (chatrooms->telepathy);

		/*
		room_names[0] = g_strdup_printf ("%s@%s",
						 gossip_chatroom_get_room (room),
						 gossip_chatroom_get_server (room));
		}
		*/
		gossip_debug (DEBUG_DOMAIN, "Joining a chatroom: %s",
			      gossip_chatroom_get_room (room));
		gossip_chatroom_set_status (room, GOSSIP_CHATROOM_STATUS_JOINING);
		gossip_chatroom_set_last_error (room, NULL);

		/* FIXME: One of those tp calls do network stuff,
		 *        we should make this call async */
		/* Get a handle for the room */
		room_names[0] = gossip_chatroom_get_room (room);
		if (!tp_conn_request_handles (DBUS_G_PROXY (tp_conn),
					      TP_CONN_HANDLE_TYPE_ROOM,
					      room_names,
					      &room_handles,
					      &error)) {
			gossip_debug (DEBUG_DOMAIN, "Error requesting room handle:%s",
				      error->message);
			g_clear_error (&error);
			return 0;
		}
		room_handle = g_array_index (room_handles, guint, 0);
		g_array_free (room_handles, TRUE);

		/* Request the object path */
		if (!tp_conn_request_channel (DBUS_G_PROXY (tp_conn),
					      TP_IFACE_CHANNEL_TYPE_TEXT,
					      TP_CONN_HANDLE_TYPE_ROOM,
					      room_handle,
					      TRUE,
					      &room_object_path,
					      &error)) {
			gossip_debug (DEBUG_DOMAIN, "Error requesting room channel:%s",
				      error->message);
			g_clear_error (&error);
			return 0;
		}

		/* Create the channel */
		bus_name = dbus_g_proxy_get_bus_name (DBUS_G_PROXY (tp_conn));
		room_channel = tp_chan_new (tp_get_bus (),
					    bus_name,
					    room_object_path,
					    TP_IFACE_CHANNEL_TYPE_TEXT,
					    TP_CONN_HANDLE_TYPE_ROOM,
					    room_handle);
		g_free (room_object_path);

		/* Room joined, configure it ... */
		tp_room = g_slice_new0 (TelepathyChatroom);
		tp_room->chatroom = g_object_ref (room);
		tp_room->telepathy = chatrooms->telepathy;
		tp_room->room_channel = room_channel;
		tp_room->group = gossip_telepathy_group_new (tp_room->telepathy,
							     tp_room->room_channel);

		g_hash_table_insert (chatrooms->rooms,
				     GINT_TO_POINTER (room_id),
				     tp_room);

		dbus_g_proxy_connect_signal (DBUS_G_PROXY (room_channel),
					     "Closed",
					     G_CALLBACK (telepathy_chatrooms_closed_cb),
					     chatrooms, NULL);

		gossip_chatroom_set_status (room, GOSSIP_CHATROOM_STATUS_ACTIVE);
		gossip_chatroom_set_last_error (room,
			gossip_chatroom_provider_join_result_as_str (GOSSIP_CHATROOM_JOIN_OK));

		if (callback) {
			(callback) (GOSSIP_CHATROOM_PROVIDER (chatrooms->telepathy),
				    GOSSIP_CHATROOM_JOIN_OK,
				    room_id, user_data);
		}

		g_signal_emit_by_name (chatrooms->telepathy,
				       "chatroom-joined",
				       room_id);

		/* Setup message sending/receiving for the new text channel */
		gossip_telepathy_message_newchannel (chatrooms->message,
						     room_channel, room_id);

		/* Setup the chatroom's contact list */
		g_signal_connect (tp_room->group, "members-added",
				  G_CALLBACK (telepathy_chatrooms_members_added_cb),
				  tp_room);
		g_signal_connect (tp_room->group, "members-removed",
				  G_CALLBACK (telepathy_chatrooms_members_removed_cb),
				  tp_room);
		gossip_telepathy_group_setup (tp_room->group);
	}
	else if (callback) {
		(callback) (GOSSIP_CHATROOM_PROVIDER (chatrooms->telepathy),
			    GOSSIP_CHATROOM_JOIN_ALREADY_OPEN,
			    room_id, user_data);
	}

	return room_id;
}

void
gossip_telepathy_chatrooms_cancel (GossipTelepathyChatrooms *chatrooms,
				   GossipChatroomId          id)
{
/*
	g_hash_table_remove (chatrooms->rooms,
			     GINT_TO_POINTER (id));
*/
}

void
gossip_telepathy_chatrooms_send (GossipTelepathyChatrooms *chatrooms,
				 GossipChatroomId          id,
				 const gchar              *message)
{
	g_return_if_fail (chatrooms != NULL);

	gossip_debug (DEBUG_DOMAIN, "ID[%d] Send message: %s", id, message);
	gossip_telepathy_message_send (chatrooms->message, id, message);
}

void
gossip_telepathy_chatrooms_change_topic (GossipTelepathyChatrooms *chatrooms,
					 GossipChatroomId          id,
					 const gchar              *new_topic)
{
	TelepathyChatroom *room;

	room = g_hash_table_lookup (chatrooms->rooms,
				    GINT_TO_POINTER (id));
	g_return_if_fail (room != NULL);

	gossip_debug (DEBUG_DOMAIN, "ID[%d] Change topic to:'%s'",
		      id, new_topic);

	/* Change topic */
}

void
gossip_telepathy_chatrooms_change_nick (GossipTelepathyChatrooms *chatrooms,
					GossipChatroomId          id,
					const gchar              *new_nick)
{
	TelepathyChatroom *room;

	room = g_hash_table_lookup (chatrooms->rooms,
				    GINT_TO_POINTER (id));
	g_return_if_fail (room != NULL);

	gossip_debug (DEBUG_DOMAIN, "ID[%d] Change chatroom nick to:'%s'",
		      id, new_nick);

	/* Set nick */
}

void
gossip_telepathy_chatrooms_leave (GossipTelepathyChatrooms *chatrooms,
				  GossipChatroomId          id)
{
	TelepathyChatroom *room;
	GError            *error = NULL;

	room = g_hash_table_lookup (chatrooms->rooms,
				    GINT_TO_POINTER (id));
	g_return_if_fail (room != NULL);

	gossip_chatroom_set_status (room->chatroom,
				    GOSSIP_CHATROOM_STATUS_INACTIVE);
	gossip_chatroom_set_last_error (room->chatroom, NULL);

	if (!tp_chan_close (DBUS_G_PROXY (room->room_channel), &error)) {
		gossip_debug (DEBUG_DOMAIN, "Error closing room channel: %s",
			      error->message);
		g_clear_error (&error);
	}

}

GossipChatroom *
gossip_telepathy_chatrooms_find (GossipTelepathyChatrooms *chatrooms,
				 GossipChatroom           *chatroom)
{
	TelepathyChatroom *room;
	GossipChatroomId   id;

	id = gossip_chatroom_get_id (chatroom);
	gossip_debug (DEBUG_DOMAIN, "Finding room with id=%d", id);

	room = g_hash_table_lookup (chatrooms->rooms,
				    GINT_TO_POINTER (id));
	if (!room) {
		return NULL;
	}

	return room->chatroom;
}

void
gossip_telepathy_chatrooms_invite (GossipTelepathyChatrooms *chatrooms,
				   GossipChatroomId          id,
				   GossipContact            *contact,
				   const gchar              *reason)
{
	TelepathyChatroom       *room;
	GossipTelepathyContacts *contacts;
	guint                    handle;
	const gchar             *contact_id;

	g_return_if_fail (chatrooms != NULL);
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	room = g_hash_table_lookup (chatrooms->rooms,
				    GINT_TO_POINTER (id));
	g_return_if_fail (room != NULL);

	gossip_debug (DEBUG_DOMAIN, "Room: %s Invitation to contact:'%s'",
		      gossip_chatroom_get_name (room->chatroom),
		      gossip_contact_get_id (contact));

	contacts = gossip_telepathy_get_contacts (room->telepathy);
	contact_id = gossip_contact_get_id (contact);
	handle = gossip_telepathy_contacts_get_handle (contacts, contact_id);

	g_return_if_fail (handle > 0);

	gossip_telepathy_group_add_member (room->group, handle, reason);
}

void
gossip_telepathy_chatrooms_invite_accept (GossipTelepathyChatrooms *chatrooms,
					  GossipChatroomJoinCb      callback,
					  GossipChatroomInvite     *invite,
					  const gchar              *nickname)
{
	/*
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

	gossip_telepathy_chatrooms_join (chatrooms,
					 chatroom,
					 callback,
					 NULL);

	g_object_unref (chatroom);
	g_free (room);
	*/
}

void
gossip_telepathy_chatrooms_invite_decline (GossipTelepathyChatrooms *chatrooms,
					   GossipChatroomInvite     *invite,
					   const gchar              *reason)
{
	/*
	GossipContact *own_contact;
	GossipContact *contact;
	const gchar   *id;

	g_return_if_fail (chatrooms != NULL);
	g_return_if_fail (invite != NULL);

	own_contact = gossip_telepathy_get_own_contact (chatrooms->telepathy);
	contact = gossip_chatroom_invite_get_invitor (invite);
	id = gossip_chatroom_invite_get_id (invite);

	gossip_debug (DEBUG_DOMAIN, "Invitation decline to:'%s' into room:'%s'",
		      gossip_contact_get_id (contact), id);
	*/
}

GList *
gossip_telepathy_chatrooms_get_rooms (GossipTelepathyChatrooms *chatrooms)
{
	GList *list = NULL;

	g_return_val_if_fail (chatrooms != NULL, NULL);

	g_hash_table_foreach (chatrooms->rooms,
			      (GHFunc) telepathy_chatrooms_get_rooms_foreach,
			      &list);
	return list;
}

static void
telepathy_chatrooms_closed_cb (TpChan                   *tp_chan,
			       GossipTelepathyChatrooms *chatrooms)
{
	TelepathyChatroom *room;

	room = g_hash_table_find (chatrooms->rooms,
				  (GHRFunc) telepathy_chatrooms_find_chan,
				  tp_chan);
	g_return_if_fail (room != NULL);

	gossip_debug (DEBUG_DOMAIN, "Channel closed: %d", tp_chan->handle);

	g_hash_table_remove (chatrooms->rooms,
			     GINT_TO_POINTER (gossip_chatroom_get_id (room->chatroom)));
}

static gboolean
telepathy_chatrooms_find_chan (guint              key,
			       TelepathyChatroom *room,
			       TpChan            *tp_chan)
{
	if (tp_chan->handle == room->room_channel->handle) {
		return TRUE;
	}

	return FALSE;
}

static void
telepathy_chatrooms_chatroom_free (TelepathyChatroom *room)
{
	g_object_unref (room->chatroom);
	g_object_unref (room->room_channel);
	g_object_unref (room->group);
	g_slice_free (TelepathyChatroom, room);
}

static void
telepathy_chatrooms_get_rooms_foreach (gpointer            key,
				       TelepathyChatroom  *room,
				       GList             **list)
{
	*list = g_list_append (*list, key);
}

static void
telepathy_chatrooms_members_added_cb (GossipTelepathyGroup *group,
				      GArray               *members,
				      TelepathyChatroom    *room)
{
	GossipTelepathyContacts *contacts;
	GList                   *added_list, *l;

	gossip_debug (DEBUG_DOMAIN, "adding %d members", members->len);

	contacts = gossip_telepathy_get_contacts (room->telepathy);
	added_list = gossip_telepathy_contacts_get_from_handles (contacts,
								 members);

	for (l = added_list; l; l = l->next) {
		GossipChatroomContact *contact;

		contact = l->data;
		gossip_chatroom_contact_set_role (contact,
						  GOSSIP_CHATROOM_ROLE_PARTICIPANT);
		gossip_chatroom_contact_set_affiliation (contact,
							 GOSSIP_CHATROOM_AFFILIATION_MEMBER);

		g_signal_emit_by_name (room->telepathy, "chatroom-contact-joined",
				       gossip_chatroom_get_id (room->chatroom),
				       GOSSIP_CONTACT (contact));
	}

	g_list_free (added_list);
}

static void
telepathy_chatrooms_members_removed_cb (GossipTelepathyGroup *group,
					GArray               *members,
					TelepathyChatroom    *room)
{
	GossipTelepathyContacts *contacts;
	guint                    i;

	gossip_debug (DEBUG_DOMAIN, "removing %d members", members->len);

	contacts = gossip_telepathy_get_contacts (room->telepathy);

	for (i = 0; i < members->len; i++) {
		GossipContact *contact;
		guint          handle;

		handle = g_array_index (members, guint, i);
		contact = gossip_telepathy_contacts_get_from_handle (contacts,
								     handle);

		g_signal_emit_by_name (room->telepathy, "chatroom-contact-left",
				       gossip_chatroom_get_id (room->chatroom),
				       contact);
	}
}

