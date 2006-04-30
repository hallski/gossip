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

#include <glib/gi18n.h>

#include "gossip-contact.h"
#include "gossip-message.h"
#include "gossip-chatroom-provider.h"
#include "libgossip-marshal.h"

static void chatroom_provider_base_init (gpointer g_class);

enum {
	CHATROOM_JOINED,
	CHATROOM_NEW_MESSAGE,
	CHATROOM_NEW_EVENT,
	CHATROOM_TOPIC_CHANGED,
	CHATROOM_CONTACT_JOINED,
	CHATROOM_CONTACT_LEFT,
	CHATROOM_CONTACT_PRESENCE_UPDATED,
	CHATROOM_CONTACT_UPDATED,
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
	
		signals[CHATROOM_CONTACT_JOINED] = 
			g_signal_new ("chatroom-contact-joined",
				      G_TYPE_FROM_CLASS (g_class),
				      G_SIGNAL_RUN_LAST,
				      0,
				      NULL, NULL,
				      libgossip_marshal_VOID__INT_OBJECT,
				      G_TYPE_NONE,
				      2, G_TYPE_INT, GOSSIP_TYPE_CONTACT);

		signals[CHATROOM_CONTACT_LEFT] = 
			g_signal_new ("chatroom-contact-left",
				      G_TYPE_FROM_CLASS (g_class),
				      G_SIGNAL_RUN_LAST,
				      0,
				      NULL, NULL,
				      libgossip_marshal_VOID__INT_OBJECT,
				      G_TYPE_NONE,
				      2, G_TYPE_INT, GOSSIP_TYPE_CONTACT);

		signals[CHATROOM_CONTACT_PRESENCE_UPDATED] = 
			g_signal_new ("chatroom-contact-presence-updated",
				      G_TYPE_FROM_CLASS (g_class),
				      G_SIGNAL_RUN_LAST,
				      0,
				      NULL, NULL,
				      libgossip_marshal_VOID__INT_OBJECT,
				      G_TYPE_NONE,
				      2, G_TYPE_INT, GOSSIP_TYPE_CONTACT);

		signals[CHATROOM_CONTACT_UPDATED] =
			g_signal_new ("chatroom-contact-updated",
				      G_TYPE_FROM_CLASS (g_class),
				      G_SIGNAL_RUN_LAST,
				      0,
				      NULL, NULL,
				      libgossip_marshal_VOID__INT_OBJECT,
				      G_TYPE_NONE,
				      2, G_TYPE_INT, GOSSIP_TYPE_CONTACT);

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

GossipChatroom *
gossip_chatroom_provider_find (GossipChatroomProvider *provider,
			       GossipChatroomId        id)
{
	g_return_val_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider), NULL);
	g_return_val_if_fail (id > 0, NULL);

	if (GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->find) {
		return GOSSIP_CHATROOM_PROVIDER_GET_IFACE (provider)->find (provider, id);
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

const gchar *
gossip_chatroom_provider_join_result_as_str (GossipChatroomJoinResult result)
{
	const gchar *last_error = NULL;

	switch (result) {
	case GOSSIP_CHATROOM_JOIN_NICK_IN_USE:
		last_error = _("The nickname you have chosen is already in use.");
		break;

	case GOSSIP_CHATROOM_JOIN_NEED_PASSWORD:
		last_error = _("The chat room you tried to join requires a password.");
		break;

	case GOSSIP_CHATROOM_JOIN_TIMED_OUT:
		last_error = _("The remote conference server did not respond in a sensible time.");
		break;

	case GOSSIP_CHATROOM_JOIN_UNKNOWN_HOST:
		last_error = _("The conference server you tried to join could not be found.");
		break;

	case GOSSIP_CHATROOM_JOIN_UNKNOWN_ERROR:
		last_error = _("An unknown error occurred, check your details are correct.");
		break;

	default:
		break;
	}

	return last_error;
}
