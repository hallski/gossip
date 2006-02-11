/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2003 Imendio AB
 * Copyright (C) 2003-2004 Geert-Jan Van den Bogaerde <geertjan@gnome.org>
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

#ifndef __GOSSIP_PRIVATE_CHAT_H__
#define __GOSSIP_PRIVATE_CHAT_H__

#include <glib-object.h>

#include "gossip-app.h"
#include "gossip-group-chat.h"

#include <libgossip/gossip-message.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_PRIVATE_CHAT         (gossip_private_chat_get_type ())
#define GOSSIP_PRIVATE_CHAT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_PRIVATE_CHAT, GossipPrivateChat))
#define GOSSIP_PRIVATE_CHAT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_PRIVATE_CHAT, GossipPrivateChatClass))
#define GOSSIP_IS_PRIVATE_CHAT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_PRIVATE_CHAT))
#define GOSSIP_IS_PRIVATE_CHAT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_PRIVATE_CHAT))
#define GOSSIP_PRIVATE_CHAT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_PRIVATE_CHAT, GossipPrivateChatClass))

typedef struct _GossipPrivateChat GossipPrivateChat;
typedef struct _GossipPrivateChatClass GossipPrivateChatClass;
typedef struct _GossipPrivateChatPriv GossipPrivateChatPriv;

#include "gossip-chat-window.h"

struct _GossipPrivateChat {
	GossipChat             parent;
        GossipPrivateChatPriv *priv;
};

struct _GossipPrivateChatClass {
        GossipChatClass parent;
};

GType               gossip_private_chat_get_type           (void);
GossipPrivateChat * gossip_private_chat_new                (GossipContact     *own_contact,
							    GossipContact     *contact);
GossipPrivateChat * gossip_private_chat_get_for_group_chat (GossipContact     *contact,
						            GossipGroupChat   *g_chat);
void                gossip_private_chat_append_message     (GossipPrivateChat *chat,
							    GossipMessage     *message);
void                gossip_private_chat_present            (GossipPrivateChat *chat);
void                gossip_private_chat_handle_message     (GossipMessage     *msg);
gchar *             gossip_private_chat_get_history        (GossipPrivateChat *chat,
		                                            gint               lines);
        
G_END_DECLS

#endif /* __GOSSIP_PRIVATE_CHAT_H__ */
