/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2005 Imendio AB
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

#include <libgossip/gossip-chatroom-provider.h>
#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-presence.h>

#include "gossip-jabber.h"

G_BEGIN_DECLS

typedef struct _GossipJabberChatrooms GossipJabberChatrooms;

GossipJabberChatrooms *
	       gossip_jabber_chatrooms_init                (GossipJabber          *jabber);
void           gossip_jabber_chatrooms_finalize            (GossipJabberChatrooms *chatrooms);
GossipChatroomId
	       gossip_jabber_chatrooms_join                (GossipJabberChatrooms *chatrooms,
							    GossipChatroom        *chatroom,
							    GossipChatroomJoinCb   callback,
							    gpointer               user_data);
void           gossip_jabber_chatrooms_cancel              (GossipJabberChatrooms *chatrooms,
							    GossipChatroomId       id);
void           gossip_jabber_chatrooms_send                (GossipJabberChatrooms *chatrooms,
							    GossipChatroomId       id,
							    const gchar           *message);
void           gossip_jabber_chatrooms_change_topic        (GossipJabberChatrooms *chatrooms,
							    GossipChatroomId       id,
							    const gchar           *new_topic);
void           gossip_jabber_chatrooms_change_nick         (GossipJabberChatrooms *chatrooms,
							    GossipChatroomId       id,
							    const gchar           *new_nick);
void           gossip_jabber_chatrooms_leave               (GossipJabberChatrooms *chatrooms,
							    GossipChatroomId       id);
void           gossip_jabber_chatrooms_kick                (GossipJabberChatrooms *chatrooms,
							    GossipChatroomId       id,
							    GossipContact         *contact,
							    const gchar           *reason);
GossipChatroom *
	       gossip_jabber_chatrooms_find_by_id          (GossipJabberChatrooms *chatrooms,
							    GossipChatroomId       id);
GossipChatroom *
	       gossip_jabber_chatrooms_find                (GossipJabberChatrooms *chatrooms,
							    GossipChatroom        *chatroom);
void           gossip_jabber_chatrooms_invite              (GossipJabberChatrooms *chatrooms,
							    GossipChatroomId       id,
							    GossipContact         *contact,
							    const gchar           *reason);
void           gossip_jabber_chatrooms_invite_accept       (GossipJabberChatrooms *chatrooms,
							    GossipChatroomJoinCb   callback,
							    GossipChatroomInvite  *invite,
							    const gchar           *nickname);
void           gossip_jabber_chatrooms_invite_decline      (GossipJabberChatrooms *chatrooms,
							    GossipChatroomInvite  *invite,
							    const gchar           *reason);
GList *        gossip_jabber_chatrooms_get_rooms           (GossipJabberChatrooms *chatrooms);
void           gossip_jabber_chatrooms_browse_rooms        (GossipJabberChatrooms  *chatrooms,
							    const gchar            *server,
							    GossipChatroomBrowseCb  callback,
							    gpointer                user_data);

void           gossip_jabber_chatrooms_set_presence        (GossipJabberChatrooms *chatrooms,
							    GossipPresence        *presence);
gboolean       gossip_jabber_chatrooms_get_jid_is_chatroom (GossipJabberChatrooms *chatrooms,
							    const gchar           *jid_str);

G_END_DECLS

#endif /* __GOSSIP_JABBER_CHATROOM_H__ */
