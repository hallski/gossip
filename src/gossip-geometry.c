/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006-2007 Imendio AB
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
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <sys/stat.h>

#include <glib.h>

#include <libgossip/gossip-chatroom.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-contact.h>

#include "gossip-chat.h"
#include "gossip-geometry.h"

#define DEBUG_DOMAIN "Geometry"

#define GEOMETRY_DIR_CREATE_MODE  (S_IRUSR | S_IWUSR | S_IXUSR)
#define GEOMETRY_FILE_CREATE_MODE (S_IRUSR | S_IWUSR)

#define GEOMETRY_KEY_FILENAME     "geometry.ini"

#define GEOMETRY_FORMAT           "%d,%d,%d,%d"

static gchar *geometry_get_filename (void);

static gchar *
geometry_get_filename (void)
{
	gchar *dir;
	gchar *filename;

	dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		gossip_debug (DEBUG_DOMAIN, "Creating directory:'%s'", dir);
		g_mkdir_with_parents (dir, GEOMETRY_DIR_CREATE_MODE);
	}

	filename = g_build_filename (dir, GEOMETRY_KEY_FILENAME, NULL);
	g_free (dir);

	return filename;
}

void
gossip_geometry_save_for_chat (GossipChat *chat,
			       gint        x,
			       gint        y,
			       gint        w,
			       gint        h)
{
	GError      *error = NULL;
	GKeyFile    *key_file;
	const gchar *id;
	gchar       *filename;
	GdkScreen   *screen;
	gint         max_width;
	gint         max_height;
	gchar       *content;
	gsize        length;
	gchar       *str;

	gossip_debug (DEBUG_DOMAIN, "Saving for chat: x:%d, y:%d, w:%d, h:%d\n",
		      x, y, w, h);

	if (gossip_chat_is_group_chat (chat)) {
		GossipChatroom *chatroom;

		chatroom = gossip_chat_get_chatroom (chat);
		id = gossip_chatroom_get_id_str (chatroom);
	} else {
		GossipContact *contact;

		contact = gossip_chat_get_contact (chat);
		id = gossip_contact_get_id (contact);
	}

	screen = gdk_screen_get_default ();
	max_width = gdk_screen_get_width (screen);
	max_height = gdk_screen_get_height (screen);

	w = CLAMP (w, 100, max_width);
	h = CLAMP (h, 100, max_height);

	x = CLAMP (x, 0, max_width - w);
	y = CLAMP (y, 0, max_height - h);

	str = g_strdup_printf (GEOMETRY_FORMAT, x, y, w, h);

	key_file = g_key_file_new ();

	filename = geometry_get_filename ();

	g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL);
	g_key_file_set_string (key_file, "Chat", id, str);

	g_free (str);

	content = g_key_file_to_data (key_file, &length, NULL);
	if (!g_file_set_contents (filename, content, length, &error)) {
		g_warning ("Couldn't save chat window geometry, error:%d->'%s'",
			   error->code, error->message);
		g_error_free (error);
	}

	g_free (content);
	g_free (filename);
	g_key_file_free (key_file);
}

void
gossip_geometry_load_for_chat (GossipChat *chat,
			       gint       *x,
			       gint       *y,
			       gint       *w,
			       gint       *h)
{
	GKeyFile    *key_file;
	const gchar *id;
	gchar       *filename;
	gchar       *str = NULL;

	if (x) {
		*x = -1;
	}

	if (y) {
		*y = -1;
	}

	if (w) {
		*w = -1;
	}

	if (h) {
		*h = -1;
	}

	if (gossip_chat_is_group_chat (chat)) {
		GossipChatroom *chatroom;

		chatroom = gossip_chat_get_chatroom (chat);
		id = gossip_chatroom_get_id_str (chatroom);
	} else {
		GossipContact *contact;

		contact = gossip_chat_get_contact (chat);
		id = gossip_contact_get_id (contact);
	}

	key_file = g_key_file_new ();

	filename = geometry_get_filename ();

	if (g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL)) {
		str = g_key_file_get_string (key_file, "Chat", id, NULL);
	}

	if (str) {
		gint tmp_x, tmp_y, tmp_w, tmp_h;

		sscanf (str, GEOMETRY_FORMAT, &tmp_x, &tmp_y, &tmp_w, &tmp_h);

		if (x) {
			*x = tmp_x;
		}

		if (y) {
			*y = tmp_y;
		}

		if (w) {
			*w = tmp_w;
		}

		if (h) {
			*h = tmp_h;
		}

		g_free (str);
	}

	gossip_debug (DEBUG_DOMAIN, "Loading for chat: x:%d, y:%d, w:%d, h:%d\n",
		      x ? *x : -1,
		      y ? *y : -1,
		      w ? *w : -1,
		      h ? *h : -1);

	g_free (filename);
	g_key_file_free (key_file);
}

void
gossip_geometry_save_for_main_window (gint x,
				      gint y,
				      gint w,
				      gint h)
{
	GError    *error = NULL;
	GKeyFile  *key_file;
	gchar     *filename;
	GdkScreen *screen;
	gint       max_width;
	gint       max_height;
	gchar     *content;
	gsize      length;
	gchar     *str;

	gossip_debug (DEBUG_DOMAIN, "Saving for main window: x:%d, y:%d, w:%d, h:%d\n",
		      x, y, w, h);

	screen = gdk_screen_get_default ();
	max_width = gdk_screen_get_width (screen);
	max_height = gdk_screen_get_height (screen);

	w = CLAMP (w, 0, max_width);
	h = CLAMP (h, 0, max_height);

	x = CLAMP (x, 0, max_width - w);
	y = CLAMP (y, 0, max_height - h);

	str = g_strdup_printf (GEOMETRY_FORMAT, x, y, w, h);

	key_file = g_key_file_new ();

	filename = geometry_get_filename ();

	g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL);
	g_key_file_set_string (key_file, "Windows", "main", str);

	g_free (str);

	content = g_key_file_to_data (key_file, &length, NULL);
	if (!g_file_set_contents (filename, content, length, &error)) {
		g_warning ("Couldn't save main window geometry, error:%d->'%s'",
			   error->code, error->message);
		g_error_free (error);
	}

	g_free (content);
	g_free (filename);
	g_key_file_free (key_file);
}

void
gossip_geometry_load_for_main_window (gint *x,
				      gint *y,
				      gint *w,
				      gint *h)
{
	GKeyFile *key_file;
	gchar    *filename;
	gchar    *str = NULL;

	if (x) {
		*x = 0;
	}

	if (y) {
		*y = 0;
	}

	if (w) {
		*w = 0;
	}

	if (h) {
		*h = 0;
	}

	key_file = g_key_file_new ();

	filename = geometry_get_filename ();

	if (g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL)) {
		str = g_key_file_get_string (key_file, "Windows", "main", NULL);
	}

	if (str) {
		gint tmp_x, tmp_y, tmp_w, tmp_h;

		sscanf (str, GEOMETRY_FORMAT, &tmp_x, &tmp_y, &tmp_w, &tmp_h);

		if (x) {
			*x = tmp_x;
		}

		if (y) {
			*y = tmp_y;
		}

		if (w) {
			*w = tmp_w;
		}

		if (h) {
			*h = tmp_h;
		}

		g_free (str);
	}

	gossip_debug (DEBUG_DOMAIN, "Loading for main window: x:%d, y:%d, w:%d, h:%d\n",
		      x ? *x : -1,
		      y ? *y : -1,
		      w ? *w : -1,
		      h ? *h : -1);

	g_free (filename);
	g_key_file_free (key_file);
}
