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

#ifndef __GOSSIP_EVENT_H__
#define __GOSSIP_EVENT_H__

#include <glib-object.h>

#include <libgossip/gossip-contact.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_EVENT         (gossip_event_get_gtype ())
#define GOSSIP_EVENT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_EVENT, GossipEvent))
#define GOSSIP_EVENT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_EVENT, GossipEventClass))
#define GOSSIP_IS_EVENT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_EVENT))
#define GOSSIP_IS_EVENT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_EVENT))
#define GOSSIP_EVENT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_EVENT, GossipEventClass))

typedef struct _GossipEvent      GossipEvent;
typedef struct _GossipEventClass GossipEventClass;
typedef gint                     GossipEventId;

struct _GossipEvent {
	GObject parent;
};

struct _GossipEventClass {
	GObjectClass parent_class;
};

typedef enum {
	GOSSIP_EVENT_NEW_MESSAGE,
	GOSSIP_EVENT_SUBSCRIPTION_REQUEST,
	GOSSIP_EVENT_SERVER_MESSAGE,
	GOSSIP_EVENT_FILE_TRANSFER_REQUEST,
	GOSSIP_EVENT_USER_ONLINE,
	GOSSIP_EVENT_USER_OFFLINE,
	GOSSIP_EVENT_ERROR
} GossipEventType;

GType           gossip_event_get_gtype    (void) G_GNUC_CONST;
GossipEvent *   gossip_event_new          (GossipEventType  type);
GossipEventId   gossip_event_get_id       (GossipEvent     *event);
GossipEventType gossip_event_get_type     (GossipEvent     *event);
const gchar *   gossip_event_get_message  (GossipEvent     *event);
GossipContact * gossip_event_get_contact  (GossipEvent     *event);

/* Should probably subclass event instead */
GObject *       gossip_event_get_data     (GossipEvent     *event);
void            gossip_event_set_data     (GossipEvent     *event,
					   GObject         *data);
guint           gossip_event_hash         (gconstpointer    key);
gboolean        gossip_event_equal        (gconstpointer    a,
					   gconstpointer    b);
gint            gossip_event_compare      (gconstpointer    a,
					   gconstpointer    b);
const gchar *   gossip_event_get_stock_id (GossipEvent     *event); 

G_END_DECLS

#endif /* __GOSSIP_EVENT_H__ */

