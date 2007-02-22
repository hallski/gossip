/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Xavier Claessens <xclaesse@gmail.com>
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

#ifndef __GOSSIP_TELEPATHY_CONTACTS_H__
#define __GOSSIP_TELEPATHY_CONTACTS_H__

#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-avatar.h>
#include <libgossip/gossip-presence.h>
#include <libgossip/gossip-async.h>

#include "gossip-telepathy.h"

G_BEGIN_DECLS

typedef struct _GossipTelepathyContacts GossipTelepathyContacts;

GossipTelepathyContacts *gossip_telepathy_contacts_init             (GossipTelepathy                *telepathy);
void                     gossip_telepathy_contacts_finalize         (GossipTelepathyContacts        *contacts);
GossipContact *          gossip_telepathy_contacts_find             (GossipTelepathyContacts        *contacts,
								     const gchar                    *id);
GossipContact *          gossip_telepathy_contacts_new              (GossipTelepathyContacts        *contacts,
								     const gchar                    *id,
								     const gchar                    *name);
guint                    gossip_telepathy_contacts_get_handle       (GossipTelepathyContacts        *contacts,
								     const gchar                    *id);
GossipContact *          gossip_telepathy_contacts_get_from_handle  (GossipTelepathyContacts        *contacts,
								     guint                           handle);
GList *                  gossip_telepathy_contacts_get_from_handles (GossipTelepathyContacts        *contacts,
								     GArray                         *handles);
gboolean                 gossip_telepathy_contacts_set_avatar       (GossipTelepathyContacts        *contacts,
								     GossipAvatar                   *avatar,
								     GossipCallback                  callback,
								     gpointer                        user_data);
void                     gossip_telepathy_contacts_get_avatar_requirements (GossipTelepathyContacts *contacts,
								     guint                          *min_width,
								     guint                          *min_height,
								     guint                          *max_width,
								     guint                          *max_height,
								     gsize                          *max_size,
								     gchar                         **format);
void                    gossip_telepathy_contacts_rename            (GossipTelepathyContacts        *contacts,
								     GossipContact                  *contact,
								     const gchar                    *new_name);
void                    gossip_telepathy_contacts_send_presence     (GossipTelepathyContacts        *contacts,
								     GossipPresence                 *presence);
G_END_DECLS

#endif /* __GOSSIP_TELEPATHY_CONTACTS_H__ */

