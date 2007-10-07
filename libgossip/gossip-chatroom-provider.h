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

typedef enum {
	GOSSIP_CHATROOM_ERROR_NONE,

	/* MUC errors */
	GOSSIP_CHATROOM_ERROR_PASSWORD_INVALID_OR_MISSING,
	GOSSIP_CHATROOM_ERROR_USER_BANNED,
	GOSSIP_CHATROOM_ERROR_ROOM_NOT_FOUND,
	GOSSIP_CHATROOM_ERROR_ROOM_CREATION_RESTRICTED,
	GOSSIP_CHATROOM_ERROR_USE_RESERVED_ROOM_NICK,
	GOSSIP_CHATROOM_ERROR_NOT_ON_MEMBERS_LIST,
	GOSSIP_CHATROOM_ERROR_NICK_IN_USE,
	GOSSIP_CHATROOM_ERROR_MAXIMUM_USERS_REACHED,

	/* Internal errors */
	GOSSIP_CHATROOM_ERROR_ALREADY_OPEN,
	GOSSIP_CHATROOM_ERROR_TIMED_OUT,
	GOSSIP_CHATROOM_ERROR_CANCELED,
	GOSSIP_CHATROOM_ERROR_UNKNOWN
} GossipChatroomError;

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
	void             (*change_topic)    (GossipChatroomProvider *provider,
					     GossipChatroomId        id,
					     const gchar            *new_topic);
	void             (*change_nick)     (GossipChatroomProvider *provider,
					     GossipChatroomId        id,
					     const gchar            *new_nick);
	void             (*leave)           (GossipChatroomProvider *provider,
					     GossipChatroomId        id);
	void             (*kick)            (GossipChatroomProvider *provider,
					     GossipChatroomId        id,
					     GossipContact          *contact,
					     const gchar            *reason);
	GSList *         (*get_contacts)    (GossipChatroomProvider *provider,
					     GossipChatroomId        id);
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
void         gossip_chatroom_provider_change_topic       (GossipChatroomProvider *provider,
							  GossipChatroomId        id,
							  const gchar            *new_topic);
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

const gchar *gossip_chatroom_provider_error_to_string    (GossipChatroomError     error);

G_END_DECLS

#endif /* __GOSSIP_CHATROOM_PROVIDER_H__ */
