/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006-2007 Imendio AB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Richard Hult <richard@imendio.com>
 */

#include "gossip-paths.h"

/* Gets the root data dir. If the GOSSIP_DATA_PREFIX or
 * GTK_DATA_PREFIX environment variables are set, they're used.
 * Otherwise we use the regular UNIX style $(datadir) directory is
 * used.
 */
static gchar *
paths_get_root_dir (void)
{
	static gchar *root = NULL;

	if (!root) {
		const gchar *env;

		env = g_getenv ("GOSSIP_DATA_PREFIX");
		if (env) {
			root = g_build_filename (env, "share", NULL);
		} else {
			root = g_strdup (SHAREDIR);
		}
	}

	return root;
}

gchar *
gossip_paths_get_glade_path (const gchar *filename)
{
	return g_build_filename (paths_get_root_dir (), "gossip", filename, NULL);
}

gchar *
gossip_paths_get_image_path (const gchar *filename)
{
	return g_build_filename (paths_get_root_dir (), "gossip", filename, NULL);
}

gchar *
gossip_paths_get_dtd_path (const gchar *filename)
{
	return g_build_filename (paths_get_root_dir (), "gossip", filename, NULL);
}

gchar *
gossip_paths_get_sound_path (const gchar *filename)
{
	return g_build_filename (paths_get_root_dir (), "sounds", "gossip", filename, NULL);
}

gchar *
gossip_paths_get_locale_path (void)
{
	return g_build_filename (paths_get_root_dir (), "locale", NULL);
}
