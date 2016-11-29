/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef __GOSSIP_CHATROOM_PROVIDER_H__
#define __GOSSIP_CHATROOM_PROVIDER_H__

#include <glib-object.h>

#include "gossip-chatroom.h"
#include "gossip-chatroom-invite.h"
#include "gossip-contact.h"

G_BEGIN_DECLS

#define GOSSIP_TYPE_CHATROOM_PROVIDER           (gossip_chatroom_provider_get_type ())
#define GOSSIP_CHATROOM_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOSSIP_TYPE_CHATROOM_PROVIDER, GossipChatroomProvider))
#define GOSSIP_IS_CHATROOM_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOSSIP_TYPE_CHATROOM_PROVIDER))
#define GOSSIP_CHATROOM_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GOSSIP_TYPE_CHATROOM_PROVIDER, GossipChatroomProviderIface))

typedef struct _GossipChatroomProvider      GossipChatroomProvider;
typedef struct _GossipChatroomProviderIface GossipChatroomProviderIface;

typedef void (*GossipChatroomJoinCb)   (GossipChatroomProvider   *provider,
                                        GossipChatroomId          id,
                                        GossipChatroomError       error,
                                        gpointer                  user_data);
typedef void (*GossipChatroomBrowseCb) (GossipChatroomProvider   *provider,
                                        const gchar              *server,
                                        GList                    *rooms,
                                        GError                   *error,
                                        gpointer                  user_data);

struct _GossipChatroomProviderIface {
    GTypeInterface g_iface;

    /* Virtual Table */
    GossipChatroomId (*join)            (GossipChatroomProvider *provider,
                                         GossipChatroom         *chatroom,
                                         GossipChatroomJoinCb    callback,
                                         gpointer                user_data);
    void             (*cancel)          (GossipChatroomProvider *provider,
                                         GossipChatroomId        id);
    void             (*send)            (GossipChatroomProvider *provider,
                                         GossipChatroomId        id,
                                         const gchar            *message);
    void             (*change_subject)  (GossipChatroomProvider *provider,
                                         GossipChatroomId        id,
                                         const gchar            *new_subject);
    void             (*change_nick)     (GossipChatroomProvider *provider,
                                         GossipChatroomId        id,
                                         const gchar            *new_nick);
    void             (*leave)           (GossipChatroomProvider *provider,
                                         GossipChatroomId        id);
    void             (*kick)            (GossipChatroomProvider *provider,
                                         GossipChatroomId        id,
                                         GossipContact          *contact,
                                         const gchar            *reason);
    GossipChatroom * (*find_by_id)      (GossipChatroomProvider *provider,
                                         GossipChatroomId        id);
    GossipChatroom * (*find)            (GossipChatroomProvider *provider,
                                         GossipChatroom         *chatroom);
    void             (*invite)          (GossipChatroomProvider *provider,
                                         GossipChatroomId        id,
                                         GossipContact          *contact,
                                         const gchar            *reason);
    void             (*invite_accept)   (GossipChatroomProvider *provider,
                                         GossipChatroomJoinCb    callback,
                                         GossipChatroomInvite   *invite,
                                         const gchar            *nickname);
    void             (*invite_decline)  (GossipChatroomProvider *provider,
                                         GossipChatroomInvite   *invite,
                                         const gchar            *reason);
    GList *          (*get_rooms)       (GossipChatroomProvider *provider);
    void             (*browse_rooms)    (GossipChatroomProvider *provider,
                                         const gchar            *server,
                                         GossipChatroomBrowseCb  callback,
                                         gpointer                user_data);
};

GType        gossip_chatroom_provider_get_type           (void) G_GNUC_CONST;

GossipChatroomId
gossip_chatroom_provider_join               (GossipChatroomProvider *provider,
                                             GossipChatroom         *chatroom,
                                             GossipChatroomJoinCb    callback,
                                             gpointer                user_data);
void         gossip_chatroom_provider_cancel             (GossipChatroomProvider *provider,
                                                          GossipChatroomId        id);
void         gossip_chatroom_provider_send               (GossipChatroomProvider *provider,
                                                          GossipChatroomId        id,
                                                          const gchar            *message);
void         gossip_chatroom_provider_change_subject     (GossipChatroomProvider *provider,
                                                          GossipChatroomId        id,
                                                          const gchar            *new_subject);
void         gossip_chatroom_provider_change_nick        (GossipChatroomProvider *provider,
                                                          GossipChatroomId        id,
                                                          const gchar            *new_nick);
void         gossip_chatroom_provider_leave              (GossipChatroomProvider *provider,
                                                          GossipChatroomId        id);
void         gossip_chatroom_provider_kick               (GossipChatroomProvider *provider,
                                                          GossipChatroomId        id,
                                                          GossipContact          *contact,
                                                          const gchar            *reason);
GSList *     gossip_chatroom_provider_get_contacts       (GossipChatroomProvider *provider,
                                                          GossipChatroomId        id);
GossipContact *
gossip_chatroom_provider_get_own_contacts   (GossipChatroomProvider *provider,
                                             GossipChatroomId        id);
GossipChatroom *
gossip_chatroom_provider_find_by_id         (GossipChatroomProvider *provider,
                                             GossipChatroomId        id);
GossipChatroom *
gossip_chatroom_provider_find               (GossipChatroomProvider *provider,
                                             GossipChatroom         *chatroom);
void         gossip_chatroom_provider_invite             (GossipChatroomProvider *provider,
                                                          GossipChatroomId        id,
                                                          GossipContact          *contact,
                                                          const gchar            *reason);
void         gossip_chatroom_provider_invite_accept      (GossipChatroomProvider *provider,
                                                          GossipChatroomJoinCb    callback,
                                                          GossipChatroomInvite   *invite,
                                                          const gchar            *nickname);
void         gossip_chatroom_provider_invite_decline     (GossipChatroomProvider *provider,
                                                          GossipChatroomInvite   *invite,
                                                          const gchar            *reason);

GList *      gossip_chatroom_provider_get_rooms          (GossipChatroomProvider *provider);
void         gossip_chatroom_provider_browse_rooms       (GossipChatroomProvider *provider,
                                                          const gchar            *server,
                                                          GossipChatroomBrowseCb  callback,
                                                          gpointer                user_data);

G_END_DECLS

#endif /* __GOSSIP_CHATROOM_PROVIDER_H__ */
