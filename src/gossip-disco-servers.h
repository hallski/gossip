/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Martyn Russell (mr@gnome.org)
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

#ifndef __GOSSIP_DISCO_SERVERS_H__
#define __GOSSIP_DISCO_SERVERS_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef void (*GossipDiscoServersFunc) (GList    *servers,
					gpointer  user_data);

gboolean gossip_disco_servers_fetch (GossipDiscoServersFunc func,
				     gpointer               user_data);


/* protocols functions */
typedef void (*GossipDiscoProtocolsFunc) (GList    *protocols,
					  gpointer  user_data);


gboolean     gossip_disco_protocols_get_supported  (GossipDiscoProtocolsFunc  func,
						    gpointer                  user_data);


/* protocol items */
typedef struct _GossipDiscoProtocol GossipDiscoProtocol;

const gchar *gossip_disco_protocol_get_name        (GossipDiscoProtocol *protocol);
const gchar *gossip_disco_protocol_get_description (GossipDiscoProtocol *protocol);
const gchar *gossip_disco_protocol_get_disco_type  (GossipDiscoProtocol *protocol);
const gchar *gossip_disco_protocol_get_icon        (GossipDiscoProtocol *protocol);
const gchar *gossip_disco_protocol_get_stock_icon  (GossipDiscoProtocol *protocol);

const gchar *gossip_disco_protocol_get_url         (GossipDiscoProtocol *protocol);
GList *      gossip_disco_protocol_get_servers     (GossipDiscoProtocol *protocol);

/* user must free protocol list and each protocol in it */
void         gossip_disco_protocol_free            (GossipDiscoProtocol *protocol);


/* protocol item servers */
typedef struct _GossipDiscoServer GossipDiscoServer;

const gchar *gossip_disco_server_get_name          (GossipDiscoServer   *server);
const gchar *gossip_disco_server_get_host          (GossipDiscoServer   *server);
gint         gossip_disco_server_get_port          (GossipDiscoServer   *server);

void         gossip_disco_server_free              (GossipDiscoServer   *server);


G_END_DECLS

#endif /* __GOSSIP_DISCO_SERVERS_H__ */
