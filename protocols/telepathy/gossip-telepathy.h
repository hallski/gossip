/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio AB
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

#ifndef __GOSSIP_TELEPATHY_H__
#define __GOSSIP_TELEPATHY_H__

#include <libtelepathy/tp-conn.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-protocol.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_TELEPATHY         (gossip_telepathy_get_type ())
#define GOSSIP_TELEPATHY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_TELEPATHY, GossipTelepathy))
#define GOSSIP_TELEPATHY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_TELEPATHY, GossipTelepathyClass))
#define GOSSIP_IS_TELEPATHY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_TELEPATHY))
#define GOSSIP_IS_TELEPATHY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_TELEPATHY))
#define GOSSIP_TELEPATHY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_TELEPATHY, GossipTelepathyClass))

typedef struct _GossipTelepathy      GossipTelepathy;
typedef struct _GossipTelepathyClass GossipTelepathyClass;
typedef struct _GossipTelepathyPriv  GossipTelepathyPriv;

struct _GossipTelepathy {
	GossipProtocol parent;
};

struct _GossipTelepathyClass {
	GossipProtocolClass parent_class;
};

GType           gossip_telepathy_get_type        (void) G_GNUC_CONST;
GossipAccount * gossip_telepathy_get_account     (GossipTelepathy *telepathy);
TpConn *        gossip_telepathy_get_connection  (GossipTelepathy *telepathy);
GossipContact * gossip_telepathy_get_own_contact (GossipTelepathy *telepathy);

G_END_DECLS

#endif /* __GOSSIP_TELEPATHY_H__ */
