/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Imendio AB
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

#ifndef __GOSSIP_HEARTBEAT_H__
#define __GOSSIP_HEARTBEAT_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_HEARTBEAT            (gossip_heartbeat_get_type ())
#define GOSSIP_HEARTBEAT(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_HEARTBEAT, GossipHeartbeat))
#define GOSSIP_HEARTBEAT_CLASS(k)        (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_HEARTBEAT, GossipHeartbeatClass))
#define GOSSIP_IS_HEARTBEAT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_HEARTBEAT))
#define GOSSIP_IS_HEARTBEAT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_HEARTBEAT))
#define GOSSIP_HEARTBEAT_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_HEARTBEAT, GossipHeartbeatClass))

typedef struct _GossipHeartbeat      GossipHeartbeat;
typedef struct _GossipHeartbeatClass GossipHeartbeatClass;

struct _GossipHeartbeat {
	GObject parent;
};

struct _GossipHeartbeatClass {
	GObjectClass parent_class;
};

typedef gboolean (*GossipHeartbeatFunc)     (GossipHeartbeat *beat,
					     gpointer         user_data);

GType      gossip_heartbeat_get_type        (void) G_GNUC_CONST;

guint      gossip_heartbeat_add_callback    (GossipHeartbeat     *heartbeat,
					     GossipHeartbeatFunc  func,
					     gpointer             user_data);
void       gossip_heartbeat_remove_callback (GossipHeartbeat     *heartbeat,
					     guint                id);

G_END_DECLS

#endif /* __GOSSIP_HEARTBEAT_H__ */

