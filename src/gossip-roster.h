/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio HB
 * Copyright (C) 2002 Richard Hult <richard@imendio.com>
 * Copyright (C) 2002 CodeFactory AB
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

#ifndef __GOSSIP_ROSTER_H__
#define __GOSSIP_ROSTER_H__

#include <gtk/gtktreeview.h>
#include <loudmouth/loudmouth.h>

#define GOSSIP_TYPE_ROSTER         (gossip_roster_get_type ())
#define GOSSIP_ROSTER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_ROSTER, GossipRoster))
#define GOSSIP_ROSTER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_ROSTER, GossipRosterClass))
#define GOSSIP_IS_ROSTER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_ROSTER))
#define GOSSIP_IS_ROSTER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_ROSTER))
#define GOSSIP_ROSTER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_ROSTER, GossipRosterClass))

typedef struct _GossipRoster      GossipRoster;
typedef struct _GossipRosterClass GossipRosterClass;
typedef struct _GossipRosterPriv  GossipRosterPriv;

#include "gossip-app.h"

struct _GossipRoster {
        GtkTreeView       parent;

        GossipRosterPriv *priv;
};

struct _GossipRosterClass {
        GtkTreeViewClass parent_class;
};

GType          gossip_roster_get_type           (void) G_GNUC_CONST;
 
GossipRoster * gossip_roster_new                (GossipApp    *app);

const gchar *  gossip_roster_get_nick_from_jid  (GossipRoster *roster,
						 GossipJID    *jid);

GdkPixbuf *    
gossip_roster_get_status_pixbuf_for_jid         (GossipRoster *roster,
						 GossipJID    *jid);
gboolean       gossip_roster_have_jid           (GossipRoster *roster,
						 GossipJID    *jid);
GList *        gossip_roster_get_groups         (GossipRoster *roster);
GList *        gossip_roster_get_jids           (GossipRoster *roster);
void           gossip_roster_free_jid_list      (GList        *jids);

GossipJID *    gossip_roster_get_selected_jid   (GossipRoster *roster);

#endif /* __GOSSIP_ROSTER_H__ */
