/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2006 Imendio AB
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include "peekaboo-utils.h"
#include "peekaboo-stock.h"

/* #define DEBUG_MSG(x)  */
#define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n"); 

const gchar *
peekaboo_presence_state_to_stock_id (GossipPresenceState state)
{
	const gchar *stock_id = NULL; 

	switch (state) {
	case GOSSIP_PRESENCE_STATE_AVAILABLE:
		stock_id = PEEKABOO_STOCK_AVAILABLE;
		break;
	case GOSSIP_PRESENCE_STATE_BUSY:
		stock_id = PEEKABOO_STOCK_BUSY;
		break;
	case GOSSIP_PRESENCE_STATE_AWAY:
		stock_id = PEEKABOO_STOCK_AWAY;
		break;
	case GOSSIP_PRESENCE_STATE_EXT_AWAY:
		stock_id = PEEKABOO_STOCK_EXT_AWAY;
		break;
	case GOSSIP_PRESENCE_STATE_HIDDEN:
	case GOSSIP_PRESENCE_STATE_UNAVAILABLE:
	default:
		stock_id = PEEKABOO_STOCK_OFFLINE;
		break;
	}

	return stock_id;
}
