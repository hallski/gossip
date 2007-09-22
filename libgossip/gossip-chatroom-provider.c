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

#include <glib/gi18n.h>

#include "gossip-chatroom-provider.h"
#include "gossip-message.h"

#include "libgossip-marshal.h"

static void chatroom_provider_base_init (gpointer g_class);

enum {
	CHATROOM_JOINED,
	CHATROOM_KICKED,
	CHATROOM_NEW_MESSAGE,
	CHATROOM_NEW_EVENT,
	CHATROOM_TOPIC_CHANGED,
	CHATROOM_ERROR,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

GType
gossip_chatroom_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =
		{
			sizeof (GossipChatroomProviderIface),
			chatroom_provider_base_init,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "GossipChatroomProvider",
					       &info, 0);

		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

static void
chatroom_provider_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		signals[CHATROOM_JOINED] =
			g_signal_new ("chatroom-joined",
				      G_TYPE_FROM_CLASS (g_class),
				      G_SIGNAL_RUN_LAST,
				      0,
				      NULL, NULL,
				      libgossip_marshal_VOID__INT,
				      G_TYPE_NONE,
				      1, G_TYPE_INT);

		signals[CHATROOM_KICKED] =
			g_signal_new ("chatroom-kicked",
				      G_TYPE_FROM_CLASS (g_class),
				      G_SIGNAL_RUN_LAST,
				      0,
				      NULL, NULL,
				      libgossip_marshal_VOID__INT,
				      G_TYPE_NONE,
				      1, G_TYPE_INT);

		signals[CHATROOM_NEW_MESSAGE] =
			g_signal_new ("chatroom-new-message",
				      G_TYPE_FROM_CLASS (g_class),
				      G_SIGNAL_RUN_LAST,
				      0,
				      NULL, NULL,
				      libgossip_marshal_VOID__INT_OBJECT,
				      G_TYPE_NONE,
				      2, G_TYPE_INT, GOSSIP_TYPE_MESSAGE);
		signals[CHATROOM_NEW_EVENT] =
			g_signal_new ("chatroom-new-event",
				      G_TYPE_FROM_CLASS (g_class),
				      G_SIGNAL_RUN_LAST,
				      0,
				      NULL, NULL,
				      libgossip_marshal_VOID__INT_STRING,
				      G_TYPE_NONE,
				      2, G_TYPE_INT, G_TYPE_STRING);
		signals[CHATROOM_TOPIC_CHANGED] =
			g_signal_new ("chatroom-topic-changed",
				      G_TYPE_FROM_CLASS (g_class),
				      G_SIGNAL_RUN_LAST,
				      0,
				      NULL, NULL,
				      libgossip_marshal_VOID__INT_OBJECT_STRING,
				      G_TYPE_NONE,
				      3, G_TYPE_INT, GOSSIP_TYPE_CONTACT, G_TYPE_STRING);
		signals[CHATROOM_ERROR] =
			g_signal_new ("chatroom-error",
				      G_TYPE_FROM_CLASS (g_class),
				      G_SIGNAL_RUN_LAST,
				      0,
				      NULL, NULL,
				      libgossip_marshal_VOID__INT_INT,
				      G_TYPE_NONE,
				      2, G_TYPE_INT, G_TYPE_INT);
	
		initialized = TRUE;
	}
}

GossipChatroomId
gossip_chatroom_provider_join (GossipChatroomProvider *provider,
			       GossipChatroom         *chatroom,
			       GossipChatroomJoinCb    callback,
			       gpointer                user_data)
{
	g_return_val_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider), 0);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), 0);
	g_return_val_if_fail (callback != NULL, 0);

	if (GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->join) {
		return GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->join (provider,
									    chatroom,
									    callback,
									    user_data);
	}

	return 0;
}

void
gossip_chatroom_provider_cancel (GossipChatroomProvider *provider,
				 GossipChatroomId        id)
{
	g_return_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider));
	g_return_if_fail (id > 0);

	if (GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->cancel) {
		GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->cancel (provider,
								       id);
	}
}

void
gossip_chatroom_provider_send (GossipChatroomProvider *provider,
			       GossipChatroomId        id,
			       const gchar            *message)
{
	g_return_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider));
	g_return_if_fail (id > 0);
	g_return_if_fail (message != NULL);

	if (GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->send) {
		GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->send (provider,
								      id,
								      message);
	}
}

void
gossip_chatroom_provider_change_topic (GossipChatroomProvider *provider,
				       GossipChatroomId        id,
				       const gchar            *new_topic)
{
	g_return_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider));
	g_return_if_fail (id > 0);
	g_return_if_fail (new_topic != NULL);

	if (GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->change_topic) {
		GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->change_topic(provider,
									    id,
									    new_topic);
	}
}

void
gossip_chatroom_provider_change_nick (GossipChatroomProvider *provider,
				      GossipChatroomId        id,
				      const gchar            *new_nick)
{
	g_return_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider));
	g_return_if_fail (id > 0);
	g_return_if_fail (new_nick != NULL);

	if (GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->change_nick) {
		GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->change_nick (provider,
									    id,
									    new_nick);
	}
}

void
gossip_chatroom_provider_leave (GossipChatroomProvider *provider,
				GossipChatroomId        id)
{
	g_return_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider));
	g_return_if_fail (id > 0);

	if (GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->leave) {
		GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->leave (provider,
								       id);
	}
}

void
gossip_chatroom_provider_kick (GossipChatroomProvider *provider,
			       GossipChatroomId        id,
			       GossipContact          *contact,
			       const gchar            *reason)
{
	g_return_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (id > 0);

	if (GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->kick) {
		GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->kick (provider,
								     id,
								     contact,
								     reason);
	}
}

GSList *
gossip_chatroom_provider_get_contacts (GossipChatroomProvider *provider,
				       GossipChatroomId        id)
{
	g_return_val_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider), NULL);
	g_return_val_if_fail (id > 0, NULL);

	if (GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->get_contacts) {
		return GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->get_contacts (provider,
										    id);
	}

	return NULL;
}

GossipChatroom *
gossip_chatroom_provider_find_by_id (GossipChatroomProvider *provider,
				     GossipChatroomId        id)
{
	g_return_val_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider), NULL);
	g_return_val_if_fail (id > 0, NULL);

	if (GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->find_by_id) {
		return GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->find_by_id (provider, id);
	}

	return NULL;
}

GossipChatroom *
gossip_chatroom_provider_find (GossipChatroomProvider *provider,
			       GossipChatroom         *chatroom)
{
	g_return_val_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider), NULL);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	if (GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->find) {
		return GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->find (provider, chatroom);
	}

	return NULL;
}

void
gossip_chatroom_provider_invite (GossipChatroomProvider *provider,
				 GossipChatroomId        id,
				 GossipContact          *contact,
				 const gchar            *reason)
{
	g_return_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider));
	g_return_if_fail (id > 0);
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	/* The invite reason can be NULL */
	if (GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->invite) {
		GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->invite (provider,
								       id,
								       contact,
								       reason);
	}
}

void
gossip_chatroom_provider_invite_accept (GossipChatroomProvider *provider,
					GossipChatroomJoinCb    callback,
					GossipChatroomInvite   *invite,
					const gchar            *nickname)
{
	g_return_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider));
	g_return_if_fail (callback != NULL);
	g_return_if_fail (invite != NULL);
	g_return_if_fail (nickname != NULL);

	if (GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->invite_accept) {
		GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->invite_accept (provider,
									      callback,
									      invite,
									      nickname);
	}
}

void
gossip_chatroom_provider_invite_decline (GossipChatroomProvider *provider,
					 GossipChatroomInvite   *invite,
					 const gchar            *reason)
{
	g_return_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider));
	g_return_if_fail (invite != NULL);

	if (GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->invite_decline) {
		GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->invite_decline (provider,
									       invite,
									       reason);
	}
}

GList *
gossip_chatroom_provider_get_rooms (GossipChatroomProvider *provider)
{
	g_return_val_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider), NULL);

	if (GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->get_rooms) {
		return GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->get_rooms (provider);
	}

	return NULL;
}

void
gossip_chatroom_provider_browse_rooms (GossipChatroomProvider *provider,
				       const gchar            *server,
				       GossipChatroomBrowseCb  callback,
				       gpointer                user_data)
{
	g_return_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider));
	g_return_if_fail (server != NULL);
	g_return_if_fail (callback != NULL);

	if (GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->browse_rooms) {
		GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->browse_rooms (provider, 
									     server,
									     callback,
									     user_data);
	}
}

const gchar *
gossip_chatroom_provider_error_to_string (GossipChatroomError error)
{
	const gchar *str = NULL;

	switch (error) {
	case GOSSIP_CHATROOM_ERROR_PASSWORD_INVALID_OR_MISSING:
		str = _("The chat room you tried to join requires a password. "
			"You either failed to supply a password or the password you tried was incorrect.");
		break;

	case GOSSIP_CHATROOM_ERROR_USER_BANNED:
		str = _("You have been banned from this chatroom.");
		break;

	case GOSSIP_CHATROOM_ERROR_ROOM_NOT_FOUND:
		str = _("The conference room you tried to join could not be found.");
		break;

	case GOSSIP_CHATROOM_ERROR_ROOM_CREATION_RESTRICTED:
		str = _("Chatroom creation is restricted on this server.");
		break;

	case GOSSIP_CHATROOM_ERROR_USE_RESERVED_ROOM_NICK:
		str = _("Chatroom reserved nick names must be used on this server.");
		break;

	case GOSSIP_CHATROOM_ERROR_NOT_ON_MEMBERS_LIST:
		str = _("You are not on the chatroom's members list.");
		break;

	case GOSSIP_CHATROOM_ERROR_NICK_IN_USE:
		str = _("The nickname you have chosen is already in use.");
		break;

	case GOSSIP_CHATROOM_ERROR_MAXIMUM_USERS_REACHED:
		str = _("The maximum number of users for this chatroom has been reached.");
		break;

	case GOSSIP_CHATROOM_ERROR_TIMED_OUT:
		str = _("The remote conference server did not respond in a sensible time.");
		break;

	case GOSSIP_CHATROOM_ERROR_UNKNOWN:
		str = _("An unknown error occurred, check your details are correct.");
		break;

	case GOSSIP_CHATROOM_ERROR_CANCELED:
		str = _("Joining the chatroom was canceled.");
		break;

	default:
		break;
	}

	return str;
}
