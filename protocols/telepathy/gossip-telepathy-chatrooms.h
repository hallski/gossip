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

#ifndef __GOSSIP_TELEPATHY_CHATROOM_H__
#define __GOSSIP_TELEPATHY_CHATROOM_H__

#include <libgossip/gossip-chatroom.h>
#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-chatroom-provider.h>

#include "gossip-telepathy.h"

G_BEGIN_DECLS

typedef struct _GossipTelepathyChatrooms GossipTelepathyChatrooms;

GossipTelepathyChatrooms * gossip_telepathy_chatrooms_init           (GossipTelepathy          *jabber);
void                       gossip_telepathy_chatrooms_finalize       (GossipTelepathyChatrooms *chatrooms);
GossipChatroomId           gossip_telepathy_chatrooms_join           (GossipTelepathyChatrooms *chatrooms,
								      GossipChatroom           *chatroom,
								      GossipChatroomJoinCb      callback,
								      gpointer                  user_data);
void                       gossip_telepathy_chatrooms_cancel         (GossipTelepathyChatrooms *chatrooms,
								      GossipChatroomId          id);
void                       gossip_telepathy_chatrooms_send           (GossipTelepathyChatrooms *chatrooms,
								      GossipChatroomId          id,
								      const gchar              *message);
void                       gossip_telepathy_chatrooms_change_topic   (GossipTelepathyChatrooms *chatrooms,
								      GossipChatroomId          id,
								      const gchar              *new_topic);
void                       gossip_telepathy_chatrooms_change_nick    (GossipTelepathyChatrooms *chatrooms,
								      GossipChatroomId          id,
								      const gchar              *new_nick);
void                       gossip_telepathy_chatrooms_leave          (GossipTelepathyChatrooms *chatrooms,
								      GossipChatroomId          id);
GossipChatroom *           gossip_telepathy_chatrooms_find           (GossipTelepathyChatrooms *chatrooms,
								      GossipChatroom           *chatroom);
void                       gossip_telepathy_chatrooms_invite         (GossipTelepathyChatrooms *chatrooms,
								      GossipChatroomId          id,
								      GossipContact            *contact,
								      const gchar              *reason);
void                       gossip_telepathy_chatrooms_invite_accept  (GossipTelepathyChatrooms *chatrooms,
								      GossipChatroomJoinCb      callback,
								      GossipChatroomInvite     *invite,
								      const gchar              *nickname);
void                       gossip_telepathy_chatrooms_invite_decline (GossipTelepathyChatrooms *chatrooms,
								      GossipChatroomInvite     *invite,
								      const gchar              *reason);
GList *                    gossip_telepathy_chatrooms_get_rooms      (GossipTelepathyChatrooms *chatrooms);

G_END_DECLS

#endif /* __GOSSIP_TELEPATHY_CHATROOM_H__ */
