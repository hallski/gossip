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

#ifndef __GOSSIP_STATUS_ICON_H__
#define __GOSSIP_STATUS_ICON_H__

#include <glib-object.h>
#include <gtk/gtkstatusicon.h>

#include <libgossip/gossip-event.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_STATUS_ICON                (gossip_status_icon_get_type ())
#define GOSSIP_STATUS_ICON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_STATUS_ICON, GossipStatusIcon))
#define GOSSIP_STATUS_ICON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_STATUS_ICON, GossipStatusIconClass))
#define GOSSIP_IS_STATUS_ICON(o)               (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_STATUS_ICON))
#define GOSSIP_IS_STATUS_ICON_CLASS(k)         (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_STATUS_ICON))
#define GOSSIP_STATUS_ICON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_STATUS_ICON, GossipStatusIconClass))

typedef struct _GossipStatusIcon      GossipStatusIcon;
typedef struct _GossipStatusIconClass GossipStatusIconClass;

struct _GossipStatusIcon {
	GtkStatusIcon parent;
};

struct _GossipStatusIconClass {
	GtkStatusIconClass parent_class;
};

GType           gossip_status_icon_get_type     (void) G_GNUC_CONST;
GtkStatusIcon *
gossip_status_icon_get                          (void);

void          gossip_status_icon_add_event      (GossipStatusIcon *status_icon,
						 GossipEvent      *event);
void          gossip_status_icon_remove_event   (GossipStatusIcon *status_icon,
						 GossipEvent      *event);
GList *       gossip_status_icon_get_events     (GossipStatusIcon *status_icon);
GossipEvent * gossip_status_icon_get_next_event (GossipStatusIcon *status_icon);
void          gossip_status_icon_update_tooltip (GossipStatusIcon *status_icon);

G_END_DECLS

#endif /* __GOSSIP_STATUS_ICON_H__ */

