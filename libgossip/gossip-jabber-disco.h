/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2006 Imendio AB 
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
 * Author: Martyn Russell <martyn@imendio.com>
 */

#ifndef __GOSSIP_JABBER_DISCO_H__
#define __GOSSIP_JABBER_DISCO_H__

#include <glib-object.h>
#include <loudmouth/loudmouth.h>

#include "gossip-jid.h"
#include "gossip-jabber.h"

G_BEGIN_DECLS

typedef struct _GossipJabberDisco     GossipJabberDisco;
typedef struct _GossipJabberDiscoItem GossipJabberDiscoItem;

typedef void (*GossipJabberDiscoItemFunc) (GossipJabberDisco     *disco,
					   GossipJabberDiscoItem *item,
					   gboolean               last_item,
					   gboolean               timeout,
					   GError                *error,
					   gpointer               user_data);

GossipJabberDisco *    gossip_jabber_disco_request               (GossipJabber              *jabber,
								  const char                *to,
								  GossipJabberDiscoItemFunc  item_func,
								  gpointer                   user_data);
GossipJabberDisco *    gossip_jabber_disco_request_info          (GossipJabber              *jabber,
								  const char                *to,
								  GossipJabberDiscoItemFunc  item_func,
								  gpointer                   user_data);
void                   gossip_jabber_disco_destroy               (GossipJabberDisco         *disco);
GList *                gossip_jabber_disco_get_category          (GossipJabberDisco         *disco,
								  const gchar               *category);
GList *                gossip_jabber_disco_get_category_and_type (GossipJabberDisco         *disco,
								  const gchar               *category,
								  const gchar               *type);
gint                   gossip_jabber_disco_get_items_remaining   (GossipJabberDisco         *disco);
gint                   gossip_jabber_disco_get_items_total       (GossipJabberDisco         *disco);
GossipJabberDiscoItem *gossip_jabber_disco_get_item              (GossipJabberDisco         *disco,
								  GossipJID                 *jid);
GossipJID *            gossip_jabber_disco_item_get_jid          (GossipJabberDiscoItem     *item);
const gchar *          gossip_jabber_disco_item_get_type         (GossipJabberDiscoItem     *item);
const gchar *          gossip_jabber_disco_item_get_name         (GossipJabberDiscoItem     *item);
const GList *          gossip_jabber_disco_item_get_features     (GossipJabberDiscoItem     *item);
LmMessageNode *        gossip_jabber_disco_item_get_data         (GossipJabberDiscoItem     *item);
gboolean               gossip_jabber_disco_item_has_category     (GossipJabberDiscoItem     *item,
								  const gchar               *category);
gboolean               gossip_jabber_disco_item_has_feature      (GossipJabberDiscoItem     *item,
								  const gchar               *feature);
gboolean               gossip_jabber_disco_item_has_type         (GossipJabberDiscoItem     *item,
								  const gchar               *type);
gboolean               gossip_jabber_disco_servers               (void);
void                   gossip_jabber_disco_init                  (GossipJabber              *jabber);

G_END_DECLS

#endif /* __GOSSIP_JABBER_DISCO_H__ */
