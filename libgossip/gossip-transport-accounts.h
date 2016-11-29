/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2004 Martyn Russell <mr@gnome.org>
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

#ifndef __GOSSIP_TRANSPORT_ACCOUNTS_H__
#define __GOSSIP_TRANSPORT_ACCOUNTS_H__

#include <glib-object.h>
#include "gossip-jid.h"
#include "gossip-jabber.h"

G_BEGIN_DECLS

typedef struct _GossipTransportAccount GossipTransportAccount;
typedef struct _GossipTransportAccountList GossipTransportAccountList;

GList *                     gossip_transport_account_lists_get_all      (void);

GossipTransportAccountList *gossip_transport_account_list_new           (GossipJabber               *jabber);
void                        gossip_transport_account_list_free          (GossipTransportAccountList *al);

GossipTransportAccountList *gossip_transport_account_list_find          (GossipTransportAccount     *account);
GossipJabber               *gossip_transport_account_list_get_jabber    (GossipTransportAccountList *al);


GList *                     gossip_transport_accounts_get               (GossipTransportAccountList *al);
GList *                     gossip_transport_accounts_get_all           (void);

const gchar *               gossip_transport_account_guess_disco_type   (GossipJID                  *jid);

GossipTransportAccount *    gossip_transport_account_ref                (GossipTransportAccount     *account);
void                        gossip_transport_account_unref              (GossipTransportAccount     *account);

GossipJID *                 gossip_transport_account_get_jid            (GossipTransportAccount     *account);
const gchar *               gossip_transport_account_get_disco_type     (GossipTransportAccount     *account);
const gchar *               gossip_transport_account_get_name           (GossipTransportAccount     *account);
const gchar *               gossip_transport_account_get_username       (GossipTransportAccount     *account);
const gchar *               gossip_transport_account_get_password       (GossipTransportAccount     *account);
GossipTransportAccount *    gossip_transport_account_find_by_disco_type (GossipTransportAccountList *al,
                                                                         const gchar                *disco_type);
gint                        gossip_transport_account_count_contacts     (GossipTransportAccount     *account);

void                        gossip_transport_account_remove             (GossipTransportAccount     *account);


G_END_DECLS

#endif /* __GOSSIP_TRANSPORT_ACCOUNTS_H__ */
