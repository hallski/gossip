/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2003 Imendio HB
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

#ifndef __GOSSIP_FAVORITE_H__
#define __GOSSIP_FAVORITE_H__

#include <gtk/gtkmenuitem.h>
#include <gtk/gtkentry.h>

#define GOSSIP_FAVORITES_PATH "/Gossip/Favorites"

typedef struct {
	gchar *name;
	gchar *nick;
	gchar *room;
	gchar *server;
	
	gint   ref_count;
} GossipFavorite;

GossipFavorite * gossip_favorite_new          (const gchar    *name,
					       const gchar    *nick,
					       const gchar    *room,
					       const gchar    *server);
GossipFavorite * gossip_favorite_get_default  (void);
GSList *         gossip_favorite_get_all      (void);
GossipFavorite * gossip_favorite_get          (const gchar    *name);
GossipFavorite * gossip_favorite_ref          (GossipFavorite *favorite);
void             gossip_favorite_unref        (GossipFavorite *favorite);


#endif /* __GOSSIP_FAVORITE_H__ */
