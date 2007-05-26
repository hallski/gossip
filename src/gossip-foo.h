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

#ifndef __GOSSIP_FOO_H__
#define __GOSSIP_FOO_H__

#include <glib-object.h>
#include <gdk/gdkpixbuf.h>

#include <libgossip/gossip-presence.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_FOO            (gossip_foo_get_type ())
#define GOSSIP_FOO(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_FOO, GossipFoo))
#define GOSSIP_FOO_CLASS(k)        (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_FOO, GossipFooClass))
#define GOSSIP_IS_FOO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_FOO))
#define GOSSIP_IS_FOO_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_FOO))
#define GOSSIP_FOO_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_FOO, GossipFooClass))

typedef struct _GossipFoo      GossipFoo;
typedef struct _GossipFooClass GossipFooClass;

struct _GossipFoo {
	GObject parent;
};

struct _GossipFooClass {
	GObjectClass parent_class;
};

GType                gossip_foo_get_type                  (void) G_GNUC_CONST;

GossipPresence *     gossip_foo_get_presence              (GossipFoo      *foo);
void                 gossip_foo_set_presence              (GossipFoo      *foo,
							   GossipPresence *presence);
GossipPresence *     gossip_foo_get_away_presence         (GossipFoo *foo);
void                 gossip_foo_set_away_presence         (GossipFoo *foo,
							   GossipPresence *presence);
void                 gossip_foo_set_away                  (GossipFoo   *foo,
							   const gchar *status);

GossipPresence *     gossip_foo_get_effective_presence    (GossipFoo *foo);
GossipPresenceState  gossip_foo_get_current_state         (GossipFoo *foo);
GossipPresenceState  gossip_foo_get_previous_state        (GossipFoo *foo);
GdkPixbuf *          gossip_foo_get_current_status_pixbuf (GossipFoo *foo);

time_t               gossip_foo_get_leave_time            (GossipFoo *foo);
void                 gossip_foo_set_leave_time            (GossipFoo *foo,
							   time_t     t);
void                 gossip_foo_start_flash               (GossipFoo *foo);
void                 gossip_foo_stop_flash                (GossipFoo *foo);
void                 gossip_foo_updated                   (GossipFoo *foo);

/* clears status data from autoaway mode */
void                 gossip_foo_clear_away                (GossipFoo *foo);
gboolean             gossip_foo_idle_check_cb             (GossipFoo *foo);

G_END_DECLS

#endif /* __GOSSIP_FOO_H__ */

