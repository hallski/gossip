/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#ifndef __GOSSIP_TRANSPORT_DISCO_H__
#define __GOSSIP_TRANSPORT_DISCO_H__

#include <glib-object.h>
#include <loudmouth/loudmouth.h>
#include "gossip-app.h"
#include "gossip-jid.h"
#include "gossip-jabber.h"

G_BEGIN_DECLS

typedef struct _GossipTransportDisco GossipTransportDisco;
typedef struct _GossipTransportDiscoItem GossipTransportDiscoItem;

typedef void (*GossipTransportDiscoItemFunc) (GossipTransportDisco     *disco,
					      GossipTransportDiscoItem *item,
					      gboolean                  last_item,
					      gboolean                  timeout,
					      gpointer                  user_data);

GossipTransportDisco *    gossip_transport_disco_request               (GossipJabber                 *jabber,
									const char                   *to,
									GossipTransportDiscoItemFunc  item_func,
									gpointer                      user_data);
GossipTransportDisco *    gossip_transport_disco_request_info          (GossipJabber                 *jabber,
									const char                   *to,
									GossipTransportDiscoItemFunc  item_func,
									gpointer                      user_data);
void                      gossip_transport_disco_destroy               (GossipTransportDisco         *disco);
GList *                   gossip_transport_disco_get_category          (GossipTransportDisco         *disco,
									const gchar                  *category);
GList *                   gossip_transport_disco_get_category_and_type (GossipTransportDisco         *disco,
									const gchar                  *category,
									const gchar                  *type);
gint                      gossip_transport_disco_get_items_remaining   (GossipTransportDisco         *disco);
gint                      gossip_transport_disco_get_items_total       (GossipTransportDisco         *disco);
GossipTransportDiscoItem *gossip_transport_disco_get_item              (GossipTransportDisco         *disco,
									GossipJID                    *jid);
GossipJID *               gossip_transport_disco_item_get_jid          (GossipTransportDiscoItem     *item);
const gchar *             gossip_transport_disco_item_get_type         (GossipTransportDiscoItem     *item);
const gchar *             gossip_transport_disco_item_get_name         (GossipTransportDiscoItem     *item);
const GList *             gossip_transport_disco_item_get_features     (GossipTransportDiscoItem     *item);

gboolean                  gossip_transport_disco_item_has_category     (GossipTransportDiscoItem     *item,
									const gchar                  *category);
gboolean                  gossip_transport_disco_item_has_feature      (GossipTransportDiscoItem     *item,
									const gchar                  *feature);
gboolean                  gossip_transport_disco_item_has_type         (GossipTransportDiscoItem     *item,
									const gchar                  *type);
gboolean                  gossip_transport_disco_servers               (void);

      
G_END_DECLS

#endif /* __GOSSIP_TRANSPORT_DISCO_H__ */
