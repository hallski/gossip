/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003      Imendio HB
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

#ifndef __GOSSIP_ROSTER_VIEW_H__
#define __GOSSIP_ROSTER_VIEW_H__

#include <gtk/gtktreeview.h>

#include "gossip-roster.h"

#define GOSSIP_TYPE_ROSTER_VIEW         (gossip_roster_view_get_type ())
#define GOSSIP_ROSTER_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_ROSTER_VIEW, GossipRosterView))
#define GOSSIP_ROSTER_VIEW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_ROSTER_VIEW, GossipRosterViewClass))
#define GOSSIP_IS_ROSTER_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_ROSTER_VIEW))
#define GOSSIP_IS_ROSTER_VIEW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_ROSTER_VIEW))
#define GOSSIP_ROSTER_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_ROSTER_VIEW, GossipRosterViewClass))

typedef struct _GossipRosterView      GossipRosterView;
typedef struct _GossipRosterViewClass GossipRosterViewClass;
typedef struct _GossipRosterViewPriv  GossipRosterViewPriv;

struct _GossipRosterView {
	GtkTreeView         parent;

	GossipRosterViewPriv *priv;
};

struct _GossipRosterViewClass {
	GtkTreeViewClass    parent_class;
};

GType              gossip_roster_view_get_type     (void) G_GNUC_CONST;

GossipRosterView * gossip_roster_view_new          (GossipRoster     *roster);

GossipRosterItem * 
gossip_roster_view_get_selected_item               (GossipRosterView *view);
void               gossip_roster_view_flash_item   (GossipRosterView *view,
						    GossipRosterItem *item,
						    gboolean          flash);

void gossip_roster_view_set_show_offline (GossipRosterView *roster, gboolean show_offline);
gboolean gossip_roster_view_get_show_offline (GossipRosterView *roster);

#endif /* __GOSSIP_ROSTER_VIEW_H__ */
