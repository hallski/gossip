/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio HB
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

#ifndef __GOSSIP_PRESENCE_H__
#define __GOSSIP_PRESENCE_H__

#include <glib.h>

typedef struct _GossipPresence GossipPresence;

typedef enum {
	GOSSIP_PRESENCE_TYPE_AVAILABLE, /* available (null) */
	GOSSIP_PRESENCE_TYPE_BUSY,      /* busy (dnd) */
	GOSSIP_PRESENCE_TYPE_AWAY,      /* away (away) */
	GOSSIP_PRESENCE_TYPE_EXT_AWAY        /* extended away (xa) */
} GossipPresenceType;

GossipPresence *    gossip_presence_new      (void);

GossipPresenceType  gossip_presence_get_type   (GossipPresence *presence);
const gchar *       gossip_presence_get_status (GossipPresence *presence);

GossipPresence *    gossip_presence_ref        (GossipPresence *presence);
void                gossip_presence_unref      (GossipPresence *presence);

#endif /* __GOSSIP_PRESENCE_H__ */

