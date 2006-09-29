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

#ifndef __GOSSIP_TRANSPORT_PROTOCOL_H__
#define __GOSSIP_TRANSPORT_PROTOCOL_H__

#include "gossip-jid.h"

G_BEGIN_DECLS

/* protocols */
typedef struct _GossipTransportProtocol GossipTransportProtocol;

typedef void (*GossipTransportProtocolIDFunc) (GossipJID  *jid,
					       const gchar *id,
					       gpointer    user_data);


GList *      gossip_transport_protocol_get_all            (void);
GossipTransportProtocol *
	     gossip_transport_protocol_find_by_disco_type (const gchar                   *disco_type);
GossipTransportProtocol *
	     gossip_transport_protocol_ref                (GossipTransportProtocol       *protocol);
void         gossip_transport_protocol_unref              (GossipTransportProtocol       *protocol);
const gchar *gossip_transport_protocol_get_name           (GossipTransportProtocol       *protocol);
const gchar *gossip_transport_protocol_get_disco_type     (GossipTransportProtocol       *protocol);
const gchar *gossip_transport_protocol_get_stock_icon     (GossipTransportProtocol       *protocol);
const gchar *gossip_transport_protocol_get_description    (GossipTransportProtocol       *protocol);
const gchar *gossip_transport_protocol_get_example        (GossipTransportProtocol       *protocol);
const gchar *gossip_transport_protocol_get_icon           (GossipTransportProtocol       *protocol);
const gchar *gossip_transport_protocol_get_url            (GossipTransportProtocol       *protocol);
GList *      gossip_transport_protocol_get_services       (GossipTransportProtocol       *protocol);
void         gossip_transport_protocol_id_to_jid          (GossipTransportProtocol       *protocol,
							   const gchar                   *id,
							   GossipTransportProtocolIDFunc  func,
							   gpointer                       user_data);

/* services */
typedef struct _GossipTransportService GossipTransportService;

const gchar *gossip_transport_service_get_name            (GossipTransportService *service);
const gchar *gossip_transport_service_get_host            (GossipTransportService *service);
gint         gossip_transport_service_get_port            (GossipTransportService *service);
void         gossip_transport_service_free                (GossipTransportService *service);


G_END_DECLS

#endif /* __GOSSIP_TRANSPORT_PROTOCOL_H__ */
