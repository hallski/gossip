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

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-chatroom.h>
#include <libgossip/gossip-protocol.h>

#include "gossip-telepathy-chatrooms.h"
#include "gossip-telepathy-group.h"
#include "gossip-telepathy-message.h"
#include "gossip-telepathy-private.h"


#define DEBUG_DOMAIN "TelepathyChatrooms"

struct _GossipTelepathyChatrooms {
	GossipTelepathy        *telepathy;
	GossipTelepathyMessage *message;
	GHashTable             *rooms;
	GHashTable             *muc_channels;
};

typedef struct {
	GossipTelepathy      *telepathy;
	GossipChatroom       *chatroom;
	GossipTelepathyGroup *group;
	TpChan               *room_channel;
} TelepathyChatroom;

static void                telepathy_chatrooms_disconnecting_cb   (GossipProtocol           *telepathy,
								   GossipAccount            *account,
								   GossipTelepathyChatrooms *chatrooms);
static void                telepathy_chatrooms_closed_cb          (TpChan                   *tp_chan,
								   GossipTelepathyChatrooms *chatrooms);
static void                telepathy_chatrooms_join_cb            (DBusGProxy               *proxy,
								   GArray                   *handles,
								   GError                   *error,
								   GossipCallbackData       *data);
static TelepathyChatroom * telepathy_chatrooms_find               (GossipTelepathyChatrooms *chatrooms,
								   const gchar              *name);
static TelepathyChatroom * telepathy_chatrooms_chatroom_new       (GossipTelepathyChatrooms *chatrooms,
								   GossipChatroom           *room,
								   TpChan                   *room_channel);
static void                telepathy_chatrooms_chatroom_free      (TelepathyChatroom        *room);
static void                telepathy_chatrooms_get_rooms_foreach  (gpointer                  key,
								   TelepathyChatroom        *room,
								   GList                   **list);
static void                telepathy_chatrooms_members_added_cb   (GossipTelepathyGroup     *group,
								   GArray                   *members,
								   guint                     actor_handle,
								   guint                     reason,
								   gchar                    *message,
								   TelepathyChatroom        *room);
static void                telepathy_chatrooms_members_removed_cb (GossipTelepathyGroup     *group,
								   GArray                   *members,
								   guint                     actor_handle,
								   guint                     reason,
								   gchar                    *message,
								   TelepathyChatroom        *room);
static void                telepathy_chatrooms_local_pending_cb   (GossipTelepathyGroup     *group,
								   GArray                   *members,
								   guint                     actor_handle,
								   guint                     reason,
								   gchar                    *message,
								   TelepathyChatroom        *room);
GossipTelepathyChatrooms *
gossip_telepathy_chatrooms_init (GossipTelepathy *telepathy)
{
	GossipTelepathyChatrooms *chatrooms;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY (telepathy), NULL);

	chatrooms = g_new0 (GossipTelepathyChatrooms, 1);
	chatrooms->telepathy = telepathy;
	chatrooms->message = gossip_telepathy_message_init (telepathy);
	chatrooms->rooms = g_hash_table_new (g_direct_hash, g_direct_equal);
	chatrooms->muc_channels = g_hash_table_new_full (g_direct_hash,
							 g_direct_equal,
							 NULL,
							 (GDestroyNotify) telepathy_chatrooms_chatroom_free);

	g_signal_connect (telepathy, "disconnecting",
			  G_CALLBACK (telepathy_chatrooms_disconnecting_cb),
			  chatrooms);

	return chatrooms;
}

void
gossip_telepathy_chatrooms_finalize (GossipTelepathyChatrooms *chatrooms)
{
	g_return_if_fail (chatrooms != NULL);

	gossip_telepathy_message_finalize (chatrooms->message);
	g_hash_table_destroy (chatrooms->rooms);
	g_hash_table_destroy (chatrooms->muc_channels);
	g_free (chatrooms);
}

static void
telepathy_chatrooms_disconnecting_cb (GossipProtocol           *telepathy,
				      GossipAccount            *account,
				      GossipTelepathyChatrooms *chatrooms)
{
	g_hash_table_remove_all (chatrooms->rooms);
	g_hash_table_remove_all (chatrooms->muc_channels);
}

void
gossip_telepathy_chatrooms_newchannel (GossipTelepathyChatrooms *chatrooms,
				       TpChan                   *new_chan)
{
	g_return_if_fail (chatrooms != NULL);
	g_return_if_fail (TELEPATHY_IS_CHAN (new_chan));

	telepathy_chatrooms_chatroom_new (chatrooms, NULL, new_chan); 
}

GossipChatroomId
gossip_telepathy_chatrooms_join (GossipTelepathyChatrooms *chatrooms,
				 GossipChatroom           *room,
				 GossipChatroomJoinCb      callback,
				 gpointer                  user_data)
{
	GossipChatroomId    room_id;
	TelepathyChatroom  *tp_room;
	const char         *room_names[2] = {NULL, NULL};
	GossipCallbackData *data;
	TpConn             *tp_conn;

	room_id = gossip_chatroom_get_id (room);
	tp_room = g_hash_table_lookup (chatrooms->rooms,
				       GINT_TO_POINTER (room_id));

	if (tp_room) {
		gossip_debug (DEBUG_DOMAIN, "ID[%d] Join chatroom:'%s', room already exists.",
			      room_id,
			      gossip_chatroom_get_room (room));

		if (callback) {
			(callback) (GOSSIP_CHATROOM_PROVIDER (chatrooms->telepathy),
				    GOSSIP_CHATROOM_JOIN_ALREADY_OPEN,
				    room_id, user_data);
		}

		return room_id;
	}

	tp_conn = gossip_telepathy_get_connection (chatrooms->telepathy);

	/*
	room_names[0] = g_strdup_printf ("%s@%s",
					 gossip_chatroom_get_room (room),
					 gossip_chatroom_get_server (room));
	*/
	gossip_debug (DEBUG_DOMAIN, "Joining a chatroom: %s",
		      gossip_chatroom_get_room (room));
	gossip_chatroom_set_status (room, GOSSIP_CHATROOM_STATUS_JOINING);
	gossip_chatroom_set_last_error (room, NULL);

	/* Get a handle for the room */
	room_names[0] = gossip_chatroom_get_room (room);
	data = g_slice_new0 (GossipCallbackData);
	data->callback = callback;
	data->user_data = user_data;
	data->data1 = chatrooms;
	data->data2 = g_object_ref (room);

	tp_conn_request_handles_async (DBUS_G_PROXY (tp_conn),
				       TP_HANDLE_TYPE_ROOM,
				       room_names,
				       (tp_conn_request_handles_reply)
				       telepathy_chatrooms_join_cb,
				       data);

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

	g_return_if_fail (chatrooms != NULL);

	room = g_hash_table_lookup (chatrooms->rooms,
				    GINT_TO_POINTER (id));
	g_return_if_fail (room != NULL);

	gossip_chatroom_set_status (room->chatroom,
				    GOSSIP_CHATROOM_STATUS_INACTIVE);
	gossip_chatroom_set_last_error (room->chatroom, NULL);

	if (!tp_chan_close (DBUS_G_PROXY (room->room_channel), &error)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "Error closing room channel: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
	}

}

GossipChatroom *
gossip_telepathy_chatrooms_find (GossipTelepathyChatrooms *chatrooms,
				 GossipChatroom           *chatroom)
{
	GossipChatroomId id;

	g_return_val_if_fail (chatrooms != NULL, NULL);

	id = gossip_chatroom_get_id (chatroom);

	return gossip_telepathy_chatrooms_find_by_id (chatrooms, id);
}

GossipChatroom *
gossip_telepathy_chatrooms_find_by_id (GossipTelepathyChatrooms *chatrooms,
                                       GossipChatroomId          id) 
{
	TelepathyChatroom *tp_room;

	g_return_val_if_fail (chatrooms != NULL, NULL);

	tp_room = g_hash_table_lookup (chatrooms->rooms,
				       GINT_TO_POINTER (id));

	if (tp_room) {
		return tp_room->chatroom;
	}

	return NULL;
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
	GossipChatroom *chatroom;
	GossipContact  *contact;
	GossipAccount  *account;
	gchar          *room = NULL;
	const gchar    *id;

	g_return_if_fail (chatrooms != NULL);
	g_return_if_fail (invite != NULL);
	g_return_if_fail (callback != NULL);

	id = gossip_chatroom_invite_get_id (invite);
	contact = gossip_chatroom_invite_get_invitor (invite);

	gossip_debug (DEBUG_DOMAIN, "Invitation accepted to:'%s' into room:'%s'",
		      gossip_contact_get_id (contact), id);

	account = gossip_contact_get_account (contact);
	chatroom = g_object_new (GOSSIP_TYPE_CHATROOM,
				 "type", GOSSIP_CHATROOM_TYPE_NORMAL,
				 "account", account,
				 "name", id,
				 "room", id,
				 "nick", nickname,
				 NULL);

	gossip_telepathy_chatrooms_join (chatrooms,
					 chatroom,
					 callback,
					 NULL);

	g_object_unref (chatroom);
	g_free (room);
}

void
gossip_telepathy_chatrooms_invite_decline (GossipTelepathyChatrooms *chatrooms,
					   GossipChatroomInvite     *invite,
					   const gchar              *reason)
{
	GossipContact     *contact;
	TelepathyChatroom *tp_room;
	const gchar       *id;
	guint              self_handle;
	GError            *error = NULL;

	g_return_if_fail (chatrooms != NULL);
	g_return_if_fail (invite != NULL);

	contact = gossip_chatroom_invite_get_invitor (invite);
	id = gossip_chatroom_invite_get_id (invite);

	gossip_debug (DEBUG_DOMAIN, "Invitation decline to:'%s' into room:'%s'",
		      gossip_contact_get_id (contact), id);

	tp_room = telepathy_chatrooms_find (chatrooms, id);
	if (tp_room == NULL) {
		return;
	}

	self_handle = gossip_telepathy_group_get_self_handle (tp_room->group);
	gossip_telepathy_group_remove_member (tp_room->group, 
					      self_handle,
					      reason);

	if (!tp_chan_close (DBUS_G_PROXY (tp_room->room_channel), &error)) {
		gossip_debug (DEBUG_DOMAIN,
			      "Error closing room channel: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
	}
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

	room = g_hash_table_lookup (chatrooms->muc_channels, 
	                            GINT_TO_POINTER(tp_chan->handle));
	g_return_if_fail (room != NULL);

	gossip_debug (DEBUG_DOMAIN, "Channel closed: %d", tp_chan->handle);

	if (room->chatroom) {
		g_hash_table_remove (chatrooms->rooms,
				     GINT_TO_POINTER (gossip_chatroom_get_id (room->chatroom)));
	}

	g_hash_table_remove (chatrooms->muc_channels, 
	                     GINT_TO_POINTER (tp_chan->handle));
}

static void
telepathy_chatrooms_join_cb (DBusGProxy         *proxy,
			     GArray             *handles,
			     GError             *error,
			     GossipCallbackData *data)
{
	GossipChatroomJoinResult  result = GOSSIP_CHATROOM_JOIN_UNKNOWN_ERROR;
	GossipTelepathyChatrooms *chatrooms;
	GossipChatroom           *room;
	GossipChatroomId          room_id;
	GossipChatroomJoinCb      callback;
	TelepathyChatroom        *tp_room = NULL;
	guint                     room_handle;

	callback = data->callback;
	chatrooms = data->data1;
	room = data->data2;
	room_id = gossip_chatroom_get_id (room);

	if (error) {
		gossip_debug (DEBUG_DOMAIN,
			      "Error requesting room handle: %s",
			      error ? error->message : "No error given");
		goto exit;
	}

	room_handle = g_array_index (handles, guint, 0);

	tp_room = g_hash_table_lookup (chatrooms->muc_channels, 
	                               GINT_TO_POINTER (room_handle));

	if (tp_room) {
		guint self_handle;

		self_handle = gossip_telepathy_group_get_self_handle (tp_room->group);
		gossip_telepathy_group_add_member (tp_room->group,
						   self_handle,
						   "Just for fun");
		tp_room->chatroom = g_object_ref (room);
	} else {
		gchar       *room_object_path;
		TpChan      *room_channel;
		TpConn      *tp_conn;
		const gchar *bus_name;

		tp_conn = gossip_telepathy_get_connection (chatrooms->telepathy);

		/* Request the object path */
		if (!tp_conn_request_channel (DBUS_G_PROXY (tp_conn),
					      TP_IFACE_CHANNEL_TYPE_TEXT,
					      TP_HANDLE_TYPE_ROOM,
					      room_handle,
					      TRUE,
					      &room_object_path,
					      &error)) {
			gossip_debug (DEBUG_DOMAIN,
				      "Error requesting room channel:%s",
				      error ? error->message : "No error given");
			g_clear_error (&error);
			goto exit;
		}

		/* Create the channel */
		bus_name = dbus_g_proxy_get_bus_name (DBUS_G_PROXY (tp_conn));
		room_channel = tp_chan_new (tp_get_bus (),
					    bus_name,
					    room_object_path,
					    TP_IFACE_CHANNEL_TYPE_TEXT,
					    TP_HANDLE_TYPE_ROOM,
					    room_handle);
		g_free (room_object_path);

		tp_room = telepathy_chatrooms_chatroom_new (chatrooms,
							    room,
							    room_channel);
		g_object_unref (room_channel);
	}

	g_hash_table_insert (chatrooms->rooms,
			     GINT_TO_POINTER (room_id),
			     tp_room);

	gossip_chatroom_set_status (room, GOSSIP_CHATROOM_STATUS_ACTIVE);
	gossip_chatroom_set_last_error (room,
		gossip_chatroom_provider_join_result_as_str (GOSSIP_CHATROOM_JOIN_OK));

	/* Setup message sending/receiving for the new text channel */
	gossip_telepathy_message_newchannel (chatrooms->message,
					     tp_room->room_channel,
					     room_id);

	/* Setup the chatroom's contact list */
	g_signal_connect (tp_room->group, "members-added",
			  G_CALLBACK (telepathy_chatrooms_members_added_cb),
			  tp_room);
	g_signal_connect (tp_room->group, "members-removed",
			  G_CALLBACK (telepathy_chatrooms_members_removed_cb),
			  tp_room);

	result = GOSSIP_CHATROOM_JOIN_OK;

exit:

	if (callback) {
		(callback) (GOSSIP_CHATROOM_PROVIDER (chatrooms->telepathy),
			    result,
			    room_id,
			    data->user_data);
	}

	if (result == GOSSIP_CHATROOM_JOIN_OK) {
		GArray *members;

		g_signal_emit_by_name (chatrooms->telepathy, "chatroom-joined",
				       room_id);

		members = gossip_telepathy_group_get_members (tp_room->group);
		telepathy_chatrooms_members_added_cb (tp_room->group, 
						      members,
						      0, TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
						      NULL,
						      tp_room);
		g_array_free (members, TRUE);          
	}

	g_object_unref (room);
	g_slice_free (GossipCallbackData, data);
}

static TelepathyChatroom *
telepathy_chatrooms_find (GossipTelepathyChatrooms *chatrooms, 
			  const gchar              *name) 
{
	const gchar *room_names[2] = {name, NULL};
	GArray      *room_handles;
	guint        room_handle;
	TpConn      *tp_conn;
	GError      *error = NULL;

	tp_conn = gossip_telepathy_get_connection (chatrooms->telepathy);

	if (!tp_conn_request_handles (DBUS_G_PROXY (tp_conn),
				      TP_CONN_HANDLE_TYPE_ROOM,
				      room_names,
				      &room_handles,
				      &error)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "Error requesting room handle:%s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		return NULL;
	}

	room_handle = g_array_index (room_handles, guint, 0);
	g_array_free (room_handles, TRUE);

	return g_hash_table_lookup (chatrooms->muc_channels, 
				    GINT_TO_POINTER (room_handle));
}

static TelepathyChatroom *
telepathy_chatrooms_chatroom_new (GossipTelepathyChatrooms *chatrooms,
				  GossipChatroom           *room,
				  TpChan                   *room_channel)
{
	TelepathyChatroom *tp_room;

	tp_room = g_slice_new0 (TelepathyChatroom);

	if (room) {
		tp_room->chatroom = g_object_ref (room);
	}
	tp_room->telepathy = chatrooms->telepathy;
	tp_room->room_channel = g_object_ref (room_channel);
	tp_room->group = gossip_telepathy_group_new (tp_room->telepathy,
						     room_channel);

	g_signal_connect (tp_room->group, "local-pending",
	                  G_CALLBACK (telepathy_chatrooms_local_pending_cb),
	                  tp_room);

	gossip_telepathy_group_setup (tp_room->group); 

	dbus_g_proxy_connect_signal (DBUS_G_PROXY (room_channel),
	                            "Closed",
	                            G_CALLBACK (telepathy_chatrooms_closed_cb),
	                            chatrooms, NULL);

	g_hash_table_insert (chatrooms->muc_channels,
			     GINT_TO_POINTER (room_channel->handle),
			     tp_room);
	return tp_room;
}

static void
telepathy_chatrooms_chatroom_free (TelepathyChatroom *room)
{
	if (room->chatroom) {
		g_object_unref (room->chatroom);
	}
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
                                      guint                 actor_handle,
                                      guint                 reason,
                                      gchar                *message,
                                      TelepathyChatroom    *room)
{
	GossipTelepathyContacts *contacts;
	GList                   *added_list, *l;

	gossip_debug (DEBUG_DOMAIN, "adding %d members", members->len);

	contacts = gossip_telepathy_get_contacts (room->telepathy);
	added_list = gossip_telepathy_contacts_get_from_handles (contacts,
								 members);

	for (l = added_list; l; l = l->next) {
		gossip_chatroom_contact_joined (room->chatroom, l->data, NULL);
	}

	g_list_free (added_list);
}

static void
telepathy_chatrooms_members_removed_cb (GossipTelepathyGroup *group,
					GArray               *members,
					guint                 actor_handle,
					guint                 reason,
					gchar                *message,
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

		gossip_chatroom_contact_left (room->chatroom, contact);
	}
}

static void
telepathy_chatrooms_local_pending_cb (GossipTelepathyGroup *group,
                                      GArray               *members,
                                      guint                 actor_handle,
                                      guint                 reason,
                                      gchar                *message,
                                      TelepathyChatroom    *room) 
{
	GossipTelepathyContacts *contacts;
	GossipMessage           *invite_message;
	GossipChatroomInvite    *invite;
	GossipContact           *invitor;
	guint                    self_handle;
	gint                     i;
	const gchar             *m;

	if (actor_handle == 0) {
		GError *error = NULL;

		/* FIXME Gossip can't handle invitations by nobody */
		gossip_debug (DEBUG_DOMAIN, "Ignoring invitation to %s, no invitor",
		              gossip_telepathy_group_get_name(group));
		
		if (!tp_chan_close (DBUS_G_PROXY (room->room_channel), &error)) {
			gossip_debug (DEBUG_DOMAIN, 
				      "Error closing room channel: %s",
				      error ? error->message : "No error given");
			g_clear_error (&error);
		}

		return;
	}

	contacts = gossip_telepathy_get_contacts (room->telepathy);
	invitor = gossip_telepathy_contacts_get_from_handle (contacts,
							     actor_handle);

	self_handle = gossip_telepathy_group_get_self_handle (group);

	if (self_handle == 0) { 
		gossip_debug (DEBUG_DOMAIN, "Self handle not in the room, ignoring...");
		return;
	}

	for (i = 0 ; i < members->len ; i++) {
		if (self_handle == g_array_index (members, guint, i)) {
			break;
		}
	}

	if (i == members->len) {
		gossip_debug (DEBUG_DOMAIN, "Self handle not in added local pending, ignoring...");
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "Invited to room %s: '%s'", 
	              gossip_telepathy_group_get_name (group),
	              message);

	m = (message == NULL) ? "You've been invited!" : message;

	invite_message = gossip_message_new (GOSSIP_MESSAGE_TYPE_NORMAL,
					     gossip_telepathy_get_own_contact (room->telepathy));
	gossip_message_set_body (invite_message, m);
	gossip_message_set_sender (invite_message, invitor);

	invite = gossip_chatroom_invite_new (invitor,
	                                     gossip_telepathy_group_get_name (group),
	                                     m);
	gossip_message_set_invite (invite_message, invite);

	g_signal_emit_by_name (room->telepathy, "new-message", invite_message);

	gossip_chatroom_invite_unref (invite);
	g_object_unref (invite_message);
}

