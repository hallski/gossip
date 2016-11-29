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

#ifndef __GOSSIP_DISCO_REGISTER_H__
#define __GOSSIP_DISCO_REGISTER_H__

#include <glib-object.h>
#include <loudmouth/loudmouth.h>

#include "gossip-transport-register.h"
#include "gossip-jid.h"
#include "gossip-jabber.h"

G_BEGIN_DECLS

typedef void (*GossipTransportRegisterFunc)     (GossipJID   *jid,
                                                 const gchar *error_code,
                                                 const gchar *error_reason,
                                                 gpointer     user_data);

typedef void (*GossipTransportUnregisterFunc)   (GossipJID   *jid,
                                                 const gchar *error_code,
                                                 const gchar *error_reason,
                                                 gpointer     user_data);
/* also used to check registered */
typedef void (*GossipTransportRequirementsFunc) (GossipJID   *jid,
                                                 const gchar *key,
                                                 const gchar *username,
                                                 const gchar *password,
                                                 const gchar *nick,
                                                 const gchar *email,
                                                 gboolean     require_username,
                                                 gboolean     require_password,
                                                 gboolean     require_nick,
                                                 gboolean     require_email,
                                                 gboolean     is_registered,
                                                 const gchar *error_code,
                                                 const gchar *error_reason,
                                                 gpointer     user_data);


void     gossip_transport_unregister          (GossipJabber                    *jabber,
                                               GossipJID                       *jid,
                                               GossipTransportUnregisterFunc    func,
                                               gpointer                         user_data);
gboolean gossip_transport_unregister_cancel   (GossipJID                       *jid);
void     gossip_transport_requirements        (GossipJabber                    *jabber,
                                               GossipJID                       *jid,
                                               GossipTransportRequirementsFunc  func,
                                               gpointer                         user_data);
gboolean gossip_transport_requirements_cancel (GossipJID                       *jid);
void     gossip_transport_register            (GossipJabber                    *jabber,
                                               GossipJID                       *jid,
                                               const gchar                     *key,
                                               const gchar                     *username,
                                               const gchar                     *password,
                                               const gchar                     *nick,
                                               const gchar                     *email,
                                               GossipTransportRegisterFunc      func,
                                               gpointer                         user_data);
gboolean gossip_transport_register_cancel     (GossipJID                       *jid);






G_END_DECLS

#endif /* __GOSSIP_TRANSPORT_REGISTER_H__ */
