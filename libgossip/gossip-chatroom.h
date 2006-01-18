/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Imendio AB
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

#ifndef __GOSSIP_CHATROOM_H__
#define __GOSSIP_CHATROOM_H__

#include <glib-object.h>

#include "gossip-account.h"

#define GOSSIP_TYPE_CHATROOM         (gossip_chatroom_get_gtype ())
#define GOSSIP_CHATROOM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CHATROOM, GossipChatroom))
#define GOSSIP_CHATROOM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_CHATROOM, GossipChatroomClass))
#define GOSSIP_IS_CHATROOM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CHATROOM))
#define GOSSIP_IS_CHATROOM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CHATROOM))
#define GOSSIP_CHATROOM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CHATROOM, GossipChatroomClass))

typedef struct _GossipChatroom      GossipChatroom;
typedef struct _GossipChatroomClass GossipChatroomClass;

typedef gint GossipChatroomId;

struct _GossipChatroom {
	GObject parent;
};


struct _GossipChatroomClass {
	GObjectClass parent_class;
};


typedef enum {
        GOSSIP_CHATROOM_TYPE_NORMAL,
} GossipChatroomType;


typedef enum {
	GOSSIP_CHATROOM_CLOSED,
	GOSSIP_CHATROOM_CONNECTING,
	GOSSIP_CHATROOM_OPEN,
	GOSSIP_CHATROOM_ERROR,
	GOSSIP_CHATROOM_UNKNOWN,
} GossipChatroomStatus;


GType              gossip_chatroom_get_gtype         (void) G_GNUC_CONST;

GossipChatroomType gossip_chatroom_get_type          (GossipChatroom       *chatroom);
GossipChatroomId   gossip_chatroom_get_id            (GossipChatroom       *chatroom);
const gchar *      gossip_chatroom_get_name          (GossipChatroom       *chatroom);
const gchar *      gossip_chatroom_get_nick          (GossipChatroom       *chatroom);
const gchar *      gossip_chatroom_get_server        (GossipChatroom       *chatroom);
const gchar *      gossip_chatroom_get_room          (GossipChatroom       *chatroom);
const gchar *      gossip_chatroom_get_password      (GossipChatroom       *chatroom);
gboolean           gossip_chatroom_get_auto_connect  (GossipChatroom       *chatroom);
GossipChatroomStatus
                   gossip_chatroom_get_status        (GossipChatroom       *chatroom);
const gchar *      gossip_chatroom_get_last_error    (GossipChatroom       *chatroom);
GossipAccount *    gossip_chatroom_get_account       (GossipChatroom       *chatroom);

void               gossip_chatroom_set_type          (GossipChatroom       *chatroom,
						      GossipChatroomType    type);
void               gossip_chatroom_set_name          (GossipChatroom       *chatroom,
						      const gchar          *name);
void               gossip_chatroom_set_nick          (GossipChatroom       *chatroom,
						      const gchar          *nick);
void               gossip_chatroom_set_server        (GossipChatroom       *chatroom,
						      const gchar          *server);
void               gossip_chatroom_set_room          (GossipChatroom       *chatroom,
						      const gchar          *room);
void               gossip_chatroom_set_password      (GossipChatroom       *chatroom,
						      const gchar          *password);
void               gossip_chatroom_set_auto_connect  (GossipChatroom       *chatroom,
						      gboolean              auto_connect);
void               gossip_chatroom_set_status        (GossipChatroom       *chatroom,
						      GossipChatroomStatus  status);
void               gossip_chatroom_set_last_error    (GossipChatroom       *chatroom,
						      const gchar          *last_error);
void               gossip_chatroom_set_account       (GossipChatroom       *chatroom,
						      GossipAccount        *account);

guint              gossip_chatroom_hash              (gconstpointer         key);
gboolean           gossip_chatroom_equal             (gconstpointer         v1,
						      gconstpointer         v2);

const gchar *      gossip_chatroom_get_type_as_str   (GossipChatroomType    type);
const gchar *      gossip_chatroom_get_status_as_str (GossipChatroomStatus  status);

#endif /* __GOSSIP_CHATROOM_H__ */
