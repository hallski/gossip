/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Eitan Isaacson <eitan@ascender.com>
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

#ifndef __GOSSIP_TELEPATHY_CONTACT_LIST_H__
#define __GOSSIP_TELEPATHY_CONTACT_LIST_H__

#include "gossip-telepathy.h"

G_BEGIN_DECLS

typedef struct _GossipTelepathyContactList GossipTelepathyContactList;

GossipTelepathyContactList * gossip_telepathy_contact_list_init             (GossipTelepathy            *telepathy);
void                         gossip_telepathy_contact_list_finalize         (GossipTelepathyContactList *list);
void                         gossip_telepathy_contact_list_add              (GossipTelepathyContactList *list,
									     const gchar                *id,
									     const gchar                *message);
void                         gossip_telepathy_contact_list_remove           (GossipTelepathyContactList *list,
									     guint                       handle);
void                         gossip_telepathy_contact_list_set_subscription (GossipTelepathyContactList *list,
									     guint                       handle,
									     gboolean                    subscribed);
void                         gossip_telepathy_contact_list_newchannel       (GossipTelepathyContactList *list,
									     TpChan                     *new_chan);
void                         gossip_telepathy_contact_list_contact_update   (GossipTelepathyContactList *list,
									     guint                       contact_handle,
									     GList                      *groups);
void                         gossip_telepathy_contact_list_rename_group     (GossipTelepathyContactList *list,
									     const gchar                *group,
									     const gchar                *new_name);
GList *                      gossip_telepathy_contact_list_get_groups       (GossipTelepathyContactList *list);

G_END_DECLS

#endif /* __GOSSIP_TELEPATHY_CONTACT_LIST_H__ */
