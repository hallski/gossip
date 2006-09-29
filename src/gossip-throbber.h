/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2000 Eazel, Inc.
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
 *
 */

#ifndef __GOSSIP_THROBBER_H__
#define __GOSSIP_THROBBER_H__

#include <gtk/gtkeventbox.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_THROBBER		(gossip_throbber_get_type ())
#define GOSSIP_THROBBER(obj)		(GTK_CHECK_CAST ((obj), GOSSIP_TYPE_THROBBER, GossipThrobber))
#define GOSSIP_THROBBER_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), GOSSIP_TYPE_THROBBER, GossipThrobberClass))
#define GOSSIP_IS_THROBBER(obj)	        (GTK_CHECK_TYPE ((obj), GOSSIP_TYPE_THROBBER))
#define GOSSIP_IS_THROBBER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GOSSIP_TYPE_THROBBER))

typedef struct GossipThrobber GossipThrobber;
typedef struct GossipThrobberClass GossipThrobberClass;
typedef struct GossipThrobberDetails GossipThrobberDetails;

struct GossipThrobber {
	GtkEventBox parent;
	GossipThrobberDetails *details;
};

struct GossipThrobberClass {
	GtkEventBoxClass parent_class;

	/* Signals */
	void (* location_changed) (GossipThrobber *throbber,
				   const char     *location);
};

GType      gossip_throbber_get_type       (void);
GtkWidget *gossip_throbber_new            (void);
void       gossip_throbber_start          (GossipThrobber *throbber);
void       gossip_throbber_stop           (GossipThrobber *throbber);
void       gossip_throbber_set_small_mode (GossipThrobber *throbber,
					   gboolean        new_mode);

G_END_DECLS

#endif /* GOSSIP_THROBBER_H */


