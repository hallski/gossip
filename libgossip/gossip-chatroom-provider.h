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

#define GOSSIP_TYPE_CHATROOM_PROVIDER           (gossip_chatroom_provider_get_type ())
#define GOSSIP_CHATROOM_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOSSIP_TYPE_CHATROOM_PROVIDER, GossipChatroomProvider))
#define GOSSIP_IS_CHATROOM_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOSSIP_TYPE_CHATROOM_PROVIDER))
#define GOSSIP_CHATROOM_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GOSSIP_TYPE_CHATROOM_PROVIDER, GossipChatroomProviderIface))


typedef struct _GossipChatroomProvider      GossipChatroomProvider;
typedef struct _GossipChatroomProviderIface GossipChatroomProviderIface;

typedef gint GossipChatroomId;

typedef enum {
	GOSSIP_CHATROOM_JOIN_OK,
	GOSSIP_CHATROOM_JOIN_NICK_IN_USE,
	GOSSIP_CHATROOM_JOIN_NEED_PASSWORD,
	GOSSIP_CHATROOM_JOIN_ALREADY_OPEN,
	GOSSIP_CHATROOM_JOIN_TIMED_OUT,
	GOSSIP_CHATROOM_JOIN_UNKNOWN_HOST,
	GOSSIP_CHATROOM_JOIN_UNKNOWN_ERROR
} GossipChatroomJoinResult;


typedef void (*GossipChatroomJoinCb) (GossipChatroomProvider   *provider,
				      GossipChatroomJoinResult  result,
                                      gint                      id,
				      gpointer                  user_data);


struct _GossipChatroomProviderIface {
	GTypeInterface g_iface;

	/* Virtual Table */
	GossipChatroomId (*join)          (GossipChatroomProvider *provider,
					   const gchar            *room,
					   const gchar            *server,
					   const gchar            *nick,
					   const gchar            *password,
					   GossipChatroomJoinCb    callback,
					   gpointer                user_data);
	void             (*cancel)        (GossipChatroomProvider *provider,
					   GossipChatroomId        id);
	void             (*send)          (GossipChatroomProvider *provider,
					   GossipChatroomId        id,
					   const gchar            *message);
	void             (*set_title)     (GossipChatroomProvider *provider,
					   GossipChatroomId        id,
					   const gchar            *new_title);
	void             (*change_nick)   (GossipChatroomProvider *provider,
					   GossipChatroomId        id,
					   const gchar            *new_nick);
	void             (*leave)         (GossipChatroomProvider *provider,
					   GossipChatroomId        id);
	const gchar *    (*get_room_name) (GossipChatroomProvider *provider,
					   GossipChatroomId        id);
	void             (*invite)        (GossipChatroomProvider *provider,
					   GossipChatroomId        id,
					   const gchar            *contact_id,
					   const gchar            *invite);
	void             (*invite_accept) (GossipChatroomProvider *provider,
					   GossipChatroomJoinCb    callback,
					   const gchar            *nickname,
					   const gchar            *invite_id);
	GList *          (*get_rooms)     (GossipChatroomProvider *provider);
};


GType            gossip_chatroom_provider_get_type      (void) G_GNUC_CONST;

GossipChatroomId gossip_chatroom_provider_join          (GossipChatroomProvider *provider,
							 const gchar            *room,
							 const gchar            *server,
							 const gchar            *nick,
							 const gchar            *password,
							 GossipChatroomJoinCb    callback,
							 gpointer                user_data);
void             gossip_chatroom_provider_cancel        (GossipChatroomProvider *provider,
							 GossipChatroomId        id);
void             gossip_chatroom_provider_send          (GossipChatroomProvider *provider,
							 GossipChatroomId        id,
							 const gchar            *message);
void             gossip_chatroom_provider_set_title     (GossipChatroomProvider *provider,
							 GossipChatroomId        id,
							 const gchar            *new_title);
void             gossip_chatroom_provider_change_nick   (GossipChatroomProvider *provider,
							 GossipChatroomId        id,
							 const gchar            *new_nick);
void             gossip_chatroom_provider_leave         (GossipChatroomProvider *provider,
							 GossipChatroomId        id);
const gchar *    gossip_chatroom_provider_get_room_name (GossipChatroomProvider *provider,
							 GossipChatroomId        id);
void             gossip_chatroom_provider_invite        (GossipChatroomProvider *provider,
							 GossipChatroomId        id,
							 const gchar            *contact_id,
							 const gchar            *invite);
void             gossip_chatroom_provider_invite_accept (GossipChatroomProvider *provider,
							 GossipChatroomJoinCb    callback,
							 const gchar            *nickname,
							 const gchar            *invite_id);
GList *          gossip_chatroom_provider_get_rooms     (GossipChatroomProvider *provider);


#endif /* __GOSSIP_CHATROOM_PROVIDER_H__ */
