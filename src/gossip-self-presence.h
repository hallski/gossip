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

#ifndef __GOSSIP_SELF_PRESENCE_H__
#define __GOSSIP_SELF_PRESENCE_H__

#include <glib-object.h>
#include <gdk/gdkpixbuf.h>

#include <libgossip/gossip-presence.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_SELF_PRESENCE            (gossip_self_presence_get_type ())
#define GOSSIP_SELF_PRESENCE(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_SELF_PRESENCE, GossipSelfPresence))
#define GOSSIP_SELF_PRESENCE_CLASS(k)        (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_SELF_PRESENCE, GossipSelfPresenceClass))
#define GOSSIP_IS_SELF_PRESENCE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_SELF_PRESENCE))
#define GOSSIP_IS_SELF_PRESENCE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_SELF_PRESENCE))
#define GOSSIP_SELF_PRESENCE_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_SELF_PRESENCE, GossipSelfPresenceClass))

typedef struct _GossipSelfPresence      GossipSelfPresence;
typedef struct _GossipSelfPresenceClass GossipSelfPresenceClass;

struct _GossipSelfPresence {
	GObject parent;
};

struct _GossipSelfPresenceClass {
	GObjectClass parent_class;
};

GType                gossip_self_presence_get_type                  (void) G_GNUC_CONST;

GossipPresence *     gossip_self_presence_get_effective_presence    (GossipSelfPresence *self_presence);
GossipPresenceState  gossip_self_presence_get_current_state         (GossipSelfPresence *self_presence);
GossipPresenceState  gossip_self_presence_get_previous_state        (GossipSelfPresence *self_presence);
GdkPixbuf *          gossip_self_presence_get_current_status_pixbuf (GossipSelfPresence *self_presence);
GdkPixbuf *          gossip_self_presence_get_explicit_status_pixbuf (GossipSelfPresence *self_presence);

time_t               gossip_self_presence_get_leave_time            (GossipSelfPresence *self_presence);
void                 gossip_self_presence_stop_flash                (GossipSelfPresence *self_presence);
void                 gossip_self_presence_updated                   (GossipSelfPresence *self_presence);

void                 gossip_self_presence_set_not_away              (GossipSelfPresence *self_presence);
void                 gossip_self_presence_set_state_status          (GossipSelfPresence *self_presence,
							   GossipPresenceState state,
							   const gchar *status);

G_END_DECLS

#endif /* __GOSSIP_SELF_PRESENCE_H__ */

