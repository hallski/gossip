/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2002, 2003 Richard Hult <richard@imendio.com>
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
#include <string.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <libgnome/gnome-config.h>
#include "gossip-favorite.h"

static gchar *  favorite_get_value  (const gchar    *path,
				     const gchar    *value_name);
static void     favorite_free       (GossipFavorite *favorite);

static gchar *
favorite_get_value (const gchar *path, const gchar *value_name)
{
	gchar *key;
	gchar *str;
	
	key = g_strdup_printf ("%s/%s", path, value_name);
	str = gnome_config_get_string (key);
	g_free (key);
	
	return str;
}

static void
favorite_free (GossipFavorite *favorite)
{
	g_return_if_fail (favorite != NULL);
	
	g_free (favorite->name);
	g_free (favorite->nick);
	g_free (favorite->room);
	g_free (favorite->server);
	
	g_free (favorite);
}

GossipFavorite *
gossip_favorite_new (const gchar *name,
		     const gchar *nick,
		     const gchar *room,
		     const gchar *server)
{
	GossipFavorite *favorite;
	const gchar   *str;
	
	favorite = g_new0 (GossipFavorite, 1);
	
	str = name ? name : "";
	favorite->name = g_strdup (str);

	str = nick ? nick : "";
	favorite->nick = g_strdup (str);
	
	str = room ? room : "";
	favorite->room = g_strdup (str);
	
	str = server ? server : "";
	favorite->server = g_strdup (str);
	
	favorite->ref_count = 1;

	return favorite;
}

GossipFavorite *
gossip_favorite_get_default ()
{
	GossipFavorite *favorite;
	gchar         *name;
	
	name = gnome_config_get_string (GOSSIP_FAVORITES_PATH "/Favorites/Default");
	
	if (!name) {
		return NULL;
	}

	favorite = gossip_favorite_get (name);
	g_free (name);
	
	return favorite;
}

GossipFavorite *
gossip_favorite_get (const gchar *name)
{
	GossipFavorite *favorite = g_new0 (GossipFavorite, 1);
	gchar         *path;
	
	path = g_strdup_printf ("%s/Favorite: %s", GOSSIP_FAVORITES_PATH, name);

	if (!gnome_config_has_section (path)) {
		return NULL;
	}
	
	favorite->name   = g_strdup (name);
	favorite->nick   = favorite_get_value (path, "nick");
	favorite->room   = favorite_get_value (path, "room");
	favorite->server = favorite_get_value (path, "server");
	
	favorite->ref_count = 1;
	
	return favorite;
}

GSList *
gossip_favorite_get_all (void)
{
	GSList    *ret_val = NULL;
	gpointer   iter;
	gchar     *key;
 	
	iter = gnome_config_init_iterator_sections (GOSSIP_FAVORITES_PATH);

	while ((iter = gnome_config_iterator_next (iter, &key, NULL))) {
		if (strncmp ("Favorite: ", key, 10) == 0) {
			GossipFavorite *favorite;
			
			favorite = gossip_favorite_get (key + 10);
			ret_val = g_slist_prepend (ret_val, favorite);
		}

		g_free (key);
	}

	return ret_val;
}

GossipFavorite *
gossip_favorite_ref (GossipFavorite *favorite)
{
	g_return_val_if_fail (favorite != NULL, NULL);
	
	favorite->ref_count++;

	return favorite;
}

void
gossip_favorite_unref (GossipFavorite *favorite)
{
	g_return_if_fail (favorite != NULL);
	
	favorite->ref_count--;
	
	if (favorite->ref_count <= 0) {
		favorite_free (favorite);
	}
}

