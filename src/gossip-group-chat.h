/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Richard Hult <rhult@imendo.com>
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

#ifndef __GOSSIP_GROUP_CHAT_H__
#define __GOSSIP_GROUP_CHAT_H__

#include "gossip-app.h"

typedef struct _GossipGroupChat GossipGroupChat;

GossipGroupChat * gossip_group_chat_new             (GossipApp       *app,
						     GossipJID       *jid,
						     const gchar     *nick);

GtkWidget *       gossip_group_chat_get_window      (GossipGroupChat *chat);


#endif /* __GOSSIP_GROUP_CHAT_H__ */
