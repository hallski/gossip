/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
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
 * 
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 */

#ifndef __GOSSIP_JABBER_UTILS_H__
#define __GOSSIP_JABBER_UTILS_H__

#include <loudmouth/loudmouth.h>

G_BEGIN_DECLS

typedef enum {
	GOSSIP_JABBER_NO_CONNECTION,
	GOSSIP_JABBER_NO_SUCH_HOST,
	GOSSIP_JABBER_TIMED_OUT,
	GOSSIP_JABBER_AUTH_FAILED,
	GOSSIP_JABBER_DUPLICATE_USER,
	GOSSIP_JABBER_INVALID_USER,
	GOSSIP_JABBER_UNAVAILABLE,
	GOSSIP_JABBER_UNAUTHORIZED,
	GOSSIP_JABBER_SPECIFIC_ERROR
} GossipJabberError;

typedef struct {
	GossipJabber       *jabber;
	LmMessageHandler   *message_handler;

	GossipErrorCallback callback;
	gpointer            user_data;
} GossipJabberAsyncData;

/* Data utils */
GossipJabberAsyncData *gossip_jabber_async_data_new           (GossipJabber          *jabber,
							       GossipErrorCallback    callback,
							       gpointer               user_data);
void                   gossip_jabber_async_data_free          (GossipJabberAsyncData *ad);

/* Presence utils */
const gchar *          gossip_jabber_presence_state_to_str    (GossipPresence        *presence);
GossipPresenceState    gossip_jabber_presence_state_from_str  (const gchar           *str);

/* Message utils */
GossipTime             gossip_jabber_get_message_timestamp    (LmMessage             *m);
GossipChatroomInvite * gossip_jabber_get_message_conference   (GossipJabber          *jabber,
							       LmMessage             *m);
gboolean               gossip_jabber_get_message_is_event     (LmMessage             *m);
gboolean               gossip_jabber_get_message_is_composing (LmMessage             *m);

/* Contact utils */
gchar *                gossip_jabber_get_name_to_use          (const gchar           *jid_str,
							       const gchar           *nickname,
							       const gchar           *full_name);

/* Error utils */
GError *               gossip_jabber_error_create             (GossipJabberError      code,
							       const gchar           *reason);
void                   gossip_jabber_error                    (GossipJabber          *jabber,
							       GossipJabberError      code);
const gchar *          gossip_jabber_error_to_string          (GossipJabberError      error);

G_END_DECLS

#endif /* __GOSSIP_JABBER_UTILS_H__ */
