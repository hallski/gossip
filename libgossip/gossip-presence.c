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

#include <config.h>
#include "gossip-stock.h"
#include "gossip-utils.h"
#include "gossip-presence.h"

struct _GossipPresence {
	GossipPresenceState  state;
	GossipPresenceType   type;
	gchar               *status;

	guint                ref_count;
};

GossipPresence *
gossip_presence_new (GossipPresenceState state)
{
	GossipPresence *presence;

	presence = g_new0 (GossipPresence, 1);
	presence->ref_count = 1;
	presence->state = state;
	presence->status = NULL;

	return presence;
}

GossipPresence *
gossip_presence_new_full (GossipPresenceState state,
			  GossipPresenceType  type,
			  const gchar        *status)
{
	GossipPresence *presence;

	presence = gossip_presence_new (state);
	
	gossip_presence_set_type   (presence, type);
	gossip_presence_set_status (presence, status);

	return presence;
}

GossipPresenceState 
gossip_presence_get_state (GossipPresence *presence)
{
	g_return_val_if_fail (presence != NULL, GOSSIP_PRESENCE_STATE_OFFLINE);
	
	return presence->state;
}

void
gossip_presence_set_state (GossipPresence *presence, GossipPresenceState state)
{
	g_return_if_fail (presence != NULL);

	presence->state = state;
}

GossipPresenceType
gossip_presence_get_type (GossipPresence *presence)
{
	g_return_val_if_fail (presence != NULL, 
			      GOSSIP_PRESENCE_TYPE_AVAILABLE);

	return presence->type;
}

void
gossip_presence_set_type (GossipPresence *presence, GossipPresenceType type)
{
	g_return_if_fail (presence != NULL);

	presence->type = type;
}

const gchar *
gossip_presence_get_status (GossipPresence *presence)
{
	g_return_val_if_fail (presence != NULL, NULL);

	if (presence->status) {
		return presence->status;
	}

	return "";
}

void
gossip_presence_set_status (GossipPresence *presence, const gchar *status)
{
	g_return_if_fail (presence != NULL);

	g_free (presence->status);
	if (status) {
		presence->status = g_strdup (status);
	} else {
		presence->status = NULL;
	}
}

GossipPresence *
gossip_presence_ref (GossipPresence *presence)
{
	g_return_val_if_fail (presence != NULL, NULL);

	presence->ref_count++;

	return presence;
}

void
gossip_presence_unref (GossipPresence *presence)
{
	g_return_if_fail (presence != NULL);

	presence->ref_count--;
	
	if (presence->ref_count > 0) {
		return;
	}

	g_free (presence->status);
	g_free (presence);
}

GdkPixbuf *
gossip_presence_get_pixbuf (GossipPresence *presence)
{
	const gchar *stock = NULL;
         
        switch (gossip_presence_get_type (presence)) {
        case GOSSIP_PRESENCE_TYPE_AVAILABLE:
                stock = GOSSIP_STOCK_AVAILABLE;
                break;
        case GOSSIP_PRESENCE_TYPE_BUSY:
                stock = GOSSIP_STOCK_BUSY;
                break;
        case GOSSIP_PRESENCE_TYPE_AWAY:
                stock = GOSSIP_STOCK_AWAY;
                break;
        case GOSSIP_PRESENCE_TYPE_EXT_AWAY:
                stock = GOSSIP_STOCK_EXT_AWAY;
                break;
        }

	return gossip_utils_get_pixbuf_from_stock (stock);
}

