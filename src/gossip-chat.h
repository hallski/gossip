/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2003 Imendio HB
 * Copyright (C) 2003-2004 Geert-Jan Van den Bogaerde <gvdbogaerde@pandora.be>
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

#ifndef __GOSSIP_CHAT_H__
#define __GOSSIP_CHAT_H__

#include <glib-object.h>
#include <loudmouth/loudmouth.h>
#include "gossip-app.h"
#include "gossip-roster.h"
#include "gossip-jid.h"

G_BEGIN_DECLS

#define GOSSIP_TYPE_CHAT         (gossip_chat_get_type ())
#define GOSSIP_CHAT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CHAT, GossipChat))
#define GOSSIP_CHAT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_CHAT, GossipChatClass))
#define GOSSIP_IS_CHAT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CHAT))
#define GOSSIP_IS_CHAT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CHAT))
#define GOSSIP_CHAT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CHAT, GossipChatClass))

typedef struct _GossipChat GossipChat;
typedef struct _GossipChatClass GossipChatClass;
typedef struct _GossipChatPriv GossipChatPriv;

#include "gossip-chat-window.h"

struct _GossipChat {
        GObject         parent;
        GossipChatPriv *priv;
};

struct _GossipChatClass {
        GObjectClass parent;

        /* Signals */
	void (*new_message)      (GossipChat  *chat);
        void (*composing)        (GossipChat  *chat, 
			          gboolean     composing);
	void (*name_changed)     (GossipChat  *chat,
				  const gchar *name);
};

GType             gossip_chat_get_type           (void);
GossipChat *      gossip_chat_get_for_item       (GossipRosterItem *item,
						  gboolean          create);
GossipChat *      gossip_chat_get_for_group_chat (GossipRosterItem *item);
void              gossip_chat_append_message     (GossipChat       *chat,
						  LmMessage        *message);
void              gossip_chat_present            (GossipChat       *chat);
LmHandlerResult   gossip_chat_handle_message     (LmMessage        *message);
GtkWidget *       gossip_chat_get_widget         (GossipChat       *chat);
GossipRosterItem *gossip_chat_get_item           (GossipChat       *chat);
void              gossip_chat_set_window         (GossipChat       *chat,
						  GossipChatWindow *window);
GossipChatWindow *gossip_chat_get_window         (GossipChat       *chat);
void              gossip_chat_clear              (GossipChat       *chat);
gchar *           gossip_chat_get_history        (GossipChat       *chat,
						  gint              lines);
void              gossip_chat_scroll_down        (GossipChat       *chat);

        
G_END_DECLS

#endif /* __GOSSIP_CHAT_H__ */
