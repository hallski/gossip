/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef __GOSSIP_EVENT_MANAGER_H__
#define __GOSSIP_EVENT_MANAGER_H__

#include <glib-object.h>

#include "gossip-event.h"

G_BEGIN_DECLS

#define GOSSIP_TYPE_EVENT_MANAGER         (gossip_event_manager_get_type ())
#define GOSSIP_EVENT_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_EVENT_MANAGER, GossipEventManager))
#define GOSSIP_EVENT_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_EVENT_MANAGER, GossipEventManagerClass))
#define GOSSIP_IS_EVENT_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_EVENT_MANAGER))
#define GOSSIP_IS_EVENT_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_EVENT_MANAGER))
#define GOSSIP_EVENT_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_EVENT_MANAGER, GossipEventManagerClass))

typedef struct _GossipEventManager      GossipEventManager;
typedef struct _GossipEventManagerClass GossipEventManagerClass;

struct _GossipEventManager {
    GObject parent;
};

struct _GossipEventManagerClass {
    GObjectClass parent_class;
};

typedef void (* GossipEventActivateFunction) (GossipEventManager *manager,
                                              GossipEvent        *event,
                                              GObject            *object);

GType        gossip_event_manager_get_type        (void) G_GNUC_CONST;
GossipEventManager *
gossip_event_manager_new             (void);
void         gossip_event_manager_add             (GossipEventManager          *manager,
                                                   GossipEvent                 *event,
                                                   GossipEventActivateFunction  callback,
                                                   GObject                     *object);
void         gossip_event_manager_remove          (GossipEventManager          *manager,
                                                   GossipEvent                 *event,
                                                   GObject                     *object);
void         gossip_event_manager_activate        (GossipEventManager          *manager,
                                                   GossipEvent                 *event);
void         gossip_event_manager_activate_by_id  (GossipEventManager          *manager,
                                                   GossipEventId                id);
GossipEvent *gossip_event_manager_get_first       (GossipEventManager          *manager);
GList       *gossip_event_manager_get_events      (GossipEventManager          *manager);
guint        gossip_event_manager_get_event_count (GossipEventManager          *manager);

G_END_DECLS

#endif /* __GOSSIP_EVENT_MANAGER_H__ */
