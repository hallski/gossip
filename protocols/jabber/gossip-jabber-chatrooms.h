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

#ifndef __GOSSIP_JABBER_CHATROOM_H__
#define __GOSSIP_JABBER_CHATROOM_H__

#include <loudmouth/loudmouth.h>

#include "gossip-chatroom-provider.h"
#include "gossip-contact.h"
#include "gossip-jabber.h"
#include "gossip-presence.h"

typedef struct _GossipJabberChatrooms GossipJabberChatrooms;

GossipJabberChatrooms * 
gossip_jabber_chatrooms_new              (GossipJabber            *jabber,
					  LmConnection            *connection);
void gossip_jabber_chatrooms_free        (GossipJabberChatrooms   *chatrooms);

void gossip_jabber_chatrooms_join        (GossipJabberChatrooms   *chatroom,
					  const gchar             *room,
					  const gchar             *server,
					  const gchar             *nick,
					  const gchar             *password,
					  GossipJoinChatroomCb     callback,
					  gpointer                 user_data);
void gossip_jabber_chatrooms_send        (GossipJabberChatrooms   *chatrooms, 
					  GossipChatroomId         id,
					  const gchar             *message);
void gossip_jabber_chatrooms_set_title   (GossipJabberChatrooms   *chatrooms,
					  GossipChatroomId         id,
					  const gchar             *new_title);
void gossip_jabber_chatrooms_change_nick (GossipJabberChatrooms   *chatrooms,
					  GossipChatroomId         id,
					  const gchar             *new_nick);
void gossip_jabber_chatrooms_leave       (GossipJabberChatrooms   *chatrooms,
					  GossipChatroomId         id);
void gossip_jabber_chatrooms_set_presence (GossipJabberChatrooms  *chatrooms,
					   GossipPresence         *presence);
gboolean
gossip_jabber_chatrooms_get_jid_is_chatroom (GossipJabberChatrooms *chatrooms,
					     const gchar           *jid_str);
GossipContact * 
gossip_jabber_chatrooms_get_contact      (GossipJabberChatrooms   *chatrooms,
					  LmMessage               *message,
					  gint                    *chat_id);

#endif /* __GOSSIP_JABBER_CHATROOM_H__ */
