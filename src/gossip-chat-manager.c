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

#include <glib/gi18n.h>

#include "gossip-chat.h"
#include "gossip-chat-manager.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CHAT_MANAGER, GossipChatManagerPriv))

typedef struct _GossipChatManagerPriv GossipChatManagerPriv;
struct _GossipChatManagerPriv {
	GHashTable *chats;

	GHashTable *events;
};

/* -- Private functions -- */
static void chat_manager_finalize           (GObject            *object);
static void chat_manager_new_message_cb     (GossipSession      *session,
					     GossipMessage      *msg,
					     GossipChatManager  *manager);
static void chat_manager_event_activated_cb (GossipEventManager *event_manager,
					     GossipEvent        *event,
					     GObject            *object);

G_DEFINE_TYPE (GossipChatManager, gossip_chat_manager, G_TYPE_OBJECT);

static gpointer parent_class = NULL;

static void
gossip_chat_manager_class_init (GossipChatManagerClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	object_class->finalize = chat_manager_finalize;

	g_type_class_add_private (object_class, sizeof (GossipChatManagerPriv));
}

static void
gossip_chat_manager_init (GossipChatManager *manager)
{
	GossipChatManagerPriv *priv;

	priv = GET_PRIV (manager);

	priv->chats = g_hash_table_new_full (gossip_contact_hash,
					     gossip_contact_equal,
					     (GDestroyNotify) g_object_unref,
					     (GDestroyNotify) g_object_unref);

	priv->events = g_hash_table_new_full (gossip_contact_hash,
					      gossip_contact_equal,
					      (GDestroyNotify) g_object_unref,
					      (GDestroyNotify) g_object_unref);

	/* Connect to signals on GossipSession to listen for new messages */
	g_signal_connect (gossip_app_get_session (),
			  "new-message",
			  G_CALLBACK (chat_manager_new_message_cb),
			  manager);
}

static void
chat_manager_finalize (GObject *object)
{
	GossipChatManagerPriv *priv;

	priv = GET_PRIV (object);

	g_hash_table_destroy (priv->chats);
	g_hash_table_destroy (priv->events);
}

static void
chat_manager_new_message_cb (GossipSession     *session,
			     GossipMessage     *msg,
			     GossipChatManager *manager)
{
	GossipChatManagerPriv *priv;
	GossipPrivateChat     *chat;
	GossipContact         *sender;
	GossipEvent           *event = NULL;

	priv = GET_PRIV (manager);

	sender = gossip_message_get_sender (msg);
	chat = g_hash_table_lookup (priv->chats, sender);

	/* Add event to event manager */

	if (!chat) {
		g_print ("new chat for: %s\n", gossip_contact_get_id (sender));
		chat = gossip_chat_manager_get_chat (manager, sender);

		event = gossip_event_new (GOSSIP_EVENT_NEW_MESSAGE);
	} else {
		GossipChatWindow *window;

		window = gossip_chat_get_window (GOSSIP_CHAT (chat));

		if (!window) {
			event = gossip_event_new (GOSSIP_EVENT_NEW_MESSAGE);
		}
	}
				
	gossip_private_chat_append_message (chat, msg);

	if (event) {
		gchar *str;

		str = g_strdup_printf (_("New message from %s"), 
				       gossip_contact_get_name (sender));
		g_object_set (event, 
			      "message", str, 
			      "data", sender,
			      NULL);
		g_free (str);

		gossip_event_manager_add (gossip_app_get_event_manager (),
					  event, 
					  chat_manager_event_activated_cb,
					  G_OBJECT (manager));

		g_hash_table_insert (priv->events, 
				     g_object_ref (sender),
				     g_object_ref (event));
	}
}

static void
chat_manager_event_activated_cb (GossipEventManager *event_manager,
				 GossipEvent        *event,
				 GObject            *object)
{
	GossipContact *contact;

	contact = GOSSIP_CONTACT (gossip_event_get_data (event));

	gossip_chat_manager_show_chat (GOSSIP_CHAT_MANAGER (object), contact);
}

GossipChatManager *
gossip_chat_manager_new (void)
{
	return g_object_new (GOSSIP_TYPE_CHAT_MANAGER, NULL);
}

GossipPrivateChat *
gossip_chat_manager_get_chat (GossipChatManager *manager, GossipContact *contact)
{
	GossipChatManagerPriv *priv;
	GossipPrivateChat     *chat;

	g_return_val_if_fail (GOSSIP_IS_CHAT_MANAGER (manager), NULL);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);
	
	priv = GET_PRIV (manager);

	chat = g_hash_table_lookup (priv->chats, contact);
	
	if (!chat) {
		chat = gossip_private_chat_new (contact);
		g_hash_table_insert (priv->chats, 
				     g_object_ref (contact),
				     chat);
		g_print ("Creating a new chat: %s\n",
			 gossip_contact_get_id (contact));
	}

	return chat;
}

void
gossip_chat_manager_show_chat (GossipChatManager *manager, 
			       GossipContact     *contact)
{
	GossipChatManagerPriv *priv;
	GossipPrivateChat     *chat;
	GossipEvent           *event;

	g_return_if_fail (GOSSIP_IS_CHAT_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	
	priv = GET_PRIV (manager);

	chat = gossip_chat_manager_get_chat (manager, contact);

	gossip_chat_present (GOSSIP_CHAT (chat));
	
	event = g_hash_table_lookup (priv->events, contact);
	if (event) {
		gossip_event_manager_remove (gossip_app_get_event_manager (),
					     event, G_OBJECT (manager));
		g_hash_table_remove (priv->events, contact);
	}
}

GossipGroupChat *
gossip_chat_manager_get_group_chat (GossipChatManager *manager,
                                    const gchar       *name,
                                    gint               id)
{
        return NULL;
}

