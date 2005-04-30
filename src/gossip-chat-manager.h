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

#ifndef __GOSSIP_CHAT_MANAGER_H__
#define __GOSSIP_CHAT_MANAGER_H__

#include <glib-object.h>

#include <libgossip/gossip-contact.h>

#define GOSSIP_TYPE_CHAT_MANAGER         (gossip_chat_manager_get_type ())
#define GOSSIP_CHAT_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CHAT_MANAGER, GossipChatManager))
#define GOSSIP_CHAT_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_CHAT_MANAGER, GossipChatManagerClass))
#define GOSSIP_IS_CHAT_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CHAT_MANAGER))
#define GOSSIP_IS_CHAT_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CHAT_MANAGER))
#define GOSSIP_CHAT_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CHAT_MANAGER, GossipChatManagerClass))

typedef struct _GossipChatManager      GossipChatManager;
typedef struct _GossipChatManagerClass GossipChatManagerClass;

#include "gossip-private-chat.h"

struct _GossipChatManager {
	GObject parent;
};

struct _GossipChatManagerClass {
	GObjectClass parent_class;
};

GType               gossip_chat_manager_get_type  (void) G_GNUC_CONST;

GossipChatManager * gossip_chat_manager_new       (void);

GossipPrivateChat * gossip_chat_manager_get_chat  (GossipChatManager *manager,
						   GossipContact     *contact);
void                gossip_chat_manager_show_chat (GossipChatManager *manager,
						   GossipContact     *contact);
GossipGroupChat * 
gossip_chat_manager_get_group_chat (GossipChatManager *manager,
                                    const gchar       *name,
                                    gint               id);
#endif /* __GOSSIP_CHAT_MANAGER_H__ */

