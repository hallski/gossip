/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Martyn Russell (ginxd@btopenworld.com)
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

#ifndef __GOSSIP_DISCO_H__
#define __GOSSIP_DISCO_H__

#include <glib-object.h>
#include <loudmouth/loudmouth.h>
#include "gossip-app.h"
#include "gossip-jid.h"

G_BEGIN_DECLS

typedef struct _GossipDisco GossipDisco;
typedef struct _GossipDiscoItem GossipDiscoItem;

typedef void (*GossipDiscoItemFunc) (GossipDisco     *disco,
				     GossipDiscoItem *item,
				     gboolean         last_item,
				     gboolean         timeout,
				     gpointer         user_data);

GossipDisco      *gossip_disco_request               (const char          *to,
						      GossipDiscoItemFunc  callback,
						      gpointer             user_data);
void              gossip_disco_destroy               (GossipDisco         *disco);

GList            *gossip_disco_get_category          (GossipDisco         *disco,
						      const gchar         *category);
GList            *gossip_disco_get_category_and_type (GossipDisco         *disco,
						      const gchar         *category,
						      const gchar         *type);

GossipDiscoItem  *gossip_disco_get_item              (GossipDisco         *disco,
						      GossipJID           *jid);

GossipJID        *gossip_disco_item_get_jid          (GossipDiscoItem     *item);
gchar            *gossip_disco_item_get_name         (GossipDiscoItem     *item);
const GList      *gossip_disco_item_get_features     (GossipDiscoItem     *item);

gboolean          gossip_disco_item_has_category     (GossipDiscoItem     *item,
						      const gchar         *category);
gboolean          gossip_disco_item_has_feature      (GossipDiscoItem     *item,
						      const gchar         *feature);
gboolean          gossip_disco_item_has_type         (GossipDiscoItem     *item,
						      const gchar         *type);

gboolean          gossip_disco_servers               (void);

        
G_END_DECLS

#endif /* __GOSSIP_DISCO_H__ */
