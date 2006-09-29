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

#ifndef __GOSSIP_TRAY_H__
#define __GOSSIP_TRAY_H__

#include <glib-object.h>

#define GOSSIP_TYPE_TRAY         (gossip_tray_get_type ())
#define GOSSIP_TRAY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_TRAY, GossipTray))
#define GOSSIP_TRAY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_TRAY, GossipTrayClass))
#define GOSSIP_IS_TRAY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_TRAY))
#define GOSSIP_IS_TRAY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_TRAY))
#define GOSSIP_TRAY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_TRAY, GossipTrayClass))

typedef struct _GossipTray      GossipTray;
typedef struct _GossipTrayClass GossipTrayClass;

struct _GossipTray {
	GObject parent;
};

struct _GossipTrayClass {
	GObjectClass parent_class;
};

GType       gossip_tray_get_type   (void) G_GNUC_CONST;
GossipTray *gossip_tray_new        (void);
void        gossip_tray_set_icon   (GossipTray  *tray,
				    const gchar *stock_id);

#endif /* __GOSSIP_TRAY_H__ */

