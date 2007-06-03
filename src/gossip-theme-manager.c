/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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
 * Authors: Richard Hult <richard@imendio.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libgossip/gossip-utils.h>
#include <libgossip/gossip-conf.h>

#include "gossip-app.h"
#include "gossip-chat-view.h"
#include "gossip-preferences.h"
#include "gossip-theme.h"
#include "gossip-theme-fancy.h"
#include "gossip-theme-irc.h"
#include "gossip-theme-manager.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_THEME_MANAGER, GossipThemeManagerPriv))

typedef struct {
	gchar       *name;
	guint        name_notify_id;
	guint        room_notify_id;

	gboolean     show_avatars;
	guint        show_avatars_notify_id;

	GossipTheme *clean_theme;
	GossipTheme *simple_theme;
	GossipTheme *blue_theme;
	GossipTheme *classic_theme;

	gboolean     irc_style;
} GossipThemeManagerPriv;

static void        theme_manager_finalize                 (GObject            *object);
static void        theme_manager_notify_name_cb           (GossipConf         *conf,
							   const gchar        *key,
							   gpointer            user_data);
static void        theme_manager_notify_room_cb           (GossipConf         *conf,
							   const gchar        *key,
							   gpointer            user_data);
static void        theme_manager_notify_show_avatars_cb   (GossipConf         *conf,
							   const gchar        *key,
							   gpointer            user_data);
static void        theme_manager_apply_theme              (GossipThemeManager *manager,
							   GossipChatView     *view,
							   const gchar        *name);

enum {
	THEME_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static const gchar *themes[] = {
	"classic", N_("Classic"),
	"simple", N_("Simple"),
	"clean", N_("Clean"),
	"blue", N_("Blue"),
	NULL
};

G_DEFINE_TYPE (GossipThemeManager, gossip_theme_manager, G_TYPE_OBJECT);

static void
gossip_theme_manager_class_init (GossipThemeManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	signals[THEME_CHANGED] =
		g_signal_new ("theme-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_type_class_add_private (object_class, sizeof (GossipThemeManagerPriv));

	object_class->finalize = theme_manager_finalize;
}

static void
gossip_theme_manager_init (GossipThemeManager *manager)
{
	GossipThemeManagerPriv *priv;

	priv = GET_PRIV (manager);

	priv->name_notify_id =
		gossip_conf_notify_add (gossip_conf_get (),
					GOSSIP_PREFS_CHAT_THEME,
					theme_manager_notify_name_cb,
					manager);

	priv->room_notify_id =
		gossip_conf_notify_add (gossip_conf_get (),
					GOSSIP_PREFS_CHAT_THEME_CHAT_ROOM,
					theme_manager_notify_room_cb,
					manager);

	gossip_conf_get_string (gossip_conf_get (),
				GOSSIP_PREFS_CHAT_THEME,
				&priv->name);

	/* Unused right now, but will be used soon. */
	priv->show_avatars_notify_id =
		gossip_conf_notify_add (gossip_conf_get (),
					GOSSIP_PREFS_UI_SHOW_AVATARS,
					theme_manager_notify_show_avatars_cb,
					manager);

	gossip_conf_get_bool (gossip_conf_get (),
			      GOSSIP_PREFS_UI_SHOW_AVATARS,
			      &priv->show_avatars);

	priv->clean_theme   = gossip_theme_fancy_new ("clean");
	priv->simple_theme  = gossip_theme_fancy_new ("simple");
	priv->blue_theme    = gossip_theme_fancy_new ("blue");
	priv->classic_theme = g_object_new (GOSSIP_TYPE_THEME_IRC, NULL);
}

static void
theme_manager_finalize (GObject *object)
{
	GossipThemeManagerPriv *priv;

	priv = GET_PRIV (object);

	gossip_conf_notify_remove (gossip_conf_get (), priv->name_notify_id);
	gossip_conf_notify_remove (gossip_conf_get (), priv->room_notify_id);
	gossip_conf_notify_remove (gossip_conf_get (), priv->show_avatars_notify_id);

	g_free (priv->name);

	g_object_unref (priv->clean_theme);
	g_object_unref (priv->simple_theme);
	g_object_unref (priv->blue_theme);
	g_object_unref (priv->classic_theme);

	G_OBJECT_CLASS (gossip_theme_manager_parent_class)->finalize (object);
}

static void
theme_manager_notify_name_cb (GossipConf  *conf,
			      const gchar *key,
			      gpointer     user_data)
{
	GossipThemeManager     *manager;
	GossipThemeManagerPriv *priv;
	gchar                  *name;

	manager = user_data;
	priv = GET_PRIV (manager);

	g_free (priv->name);

	name = NULL;
	if (!gossip_conf_get_string (conf, key, &name) ||
	    name == NULL || name[0] == 0) {
		priv->name = g_strdup ("classic");
		g_free (name);
	} else {
		priv->name = name;
	}

	g_signal_emit (manager, signals[THEME_CHANGED], 0, NULL);
}

static void
theme_manager_notify_room_cb (GossipConf  *conf,
			      const gchar *key,
			      gpointer     user_data)
{
	g_signal_emit (user_data, signals[THEME_CHANGED], 0, NULL);
}

static void
theme_manager_notify_show_avatars_cb (GossipConf  *conf,
				      const gchar *key,
				      gpointer     user_data)
{
	GossipThemeManager     *manager;
	GossipThemeManagerPriv *priv;
	gboolean                value;

	manager = user_data;
	priv = GET_PRIV (manager);

	if (!gossip_conf_get_bool (conf, key, &value)) {
		priv->show_avatars = FALSE;
	} else {
		priv->show_avatars = value;
	}
}

static gboolean
theme_manager_ensure_theme_exists (const gchar *name)
{
	gint i;

	if (G_STR_EMPTY (name)) {
		return FALSE;
	}

	for (i = 0; themes[i]; i += 2) {
		if (strcmp (themes[i], name) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

static void
theme_manager_apply_theme (GossipThemeManager *manager,
			   GossipChatView     *view,
			   const gchar        *name)
{
	GossipThemeManagerPriv *priv;
	GossipTheme            *theme;

	priv = GET_PRIV (manager);

	/* Make sure all tags are present. Note: not useful now but when we have
	 * user defined theme it will be.
	 */
	if (theme_manager_ensure_theme_exists (name)) {
		if (strcmp (name, "clean") == 0) {
			theme = priv->clean_theme;
		}
		else if (strcmp (name, "simple") == 0) {
			theme = priv->simple_theme;
		}
		else if (strcmp (name, "blue") == 0) {
			theme = priv->blue_theme;
		} else {
			theme = priv->classic_theme;
		}
	} else {
		theme = priv->classic_theme;
	}

	gossip_chat_view_set_theme (view, theme);
}

GossipThemeManager *
gossip_theme_manager_get (void)
{
	static GossipThemeManager *manager = NULL;

	if (!manager) {
		manager = g_object_new (GOSSIP_TYPE_THEME_MANAGER, NULL);
	}

	return manager;
}

const gchar **
gossip_theme_manager_get_themes (void)
{
	return themes;
}

void
gossip_theme_manager_apply (GossipThemeManager *manager,
			    GossipChatView     *view,
			    const gchar        *name)
{
	GossipThemeManagerPriv *priv;

	priv = GET_PRIV (manager);

	theme_manager_apply_theme (manager, view, name);
}

void
gossip_theme_manager_apply_saved (GossipThemeManager *manager,
				  GossipChatView     *view)
{
	GossipThemeManagerPriv *priv;

	priv = GET_PRIV (manager);

	theme_manager_apply_theme (manager, view, priv->name);
}

/* FIXME: A bit ugly. We should probably change the scheme so that instead of
 * the manager signalling, views are registered and applied to automatically.
 */
void
gossip_theme_manager_update_show_avatars (GossipThemeManager *manager,
					  GossipChatView     *view,
					  gboolean            show)
{
	GossipThemeManagerPriv *priv;
	GtkTextBuffer          *buffer;
	GtkTextTagTable        *table;
	GtkTextTag             *tag_text_self, *tag_text_other;
	GtkTextTag             *tag_image_self, *tag_image_other;

	priv = GET_PRIV (manager);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	table = gtk_text_buffer_get_tag_table (buffer);

	tag_text_self = gtk_text_tag_table_lookup (table,
						   "fancy-header-self-avatar");
	tag_text_other = gtk_text_tag_table_lookup (table,
						    "fancy-header-other-avatar");

	tag_image_self = gtk_text_tag_table_lookup (table,
						    "fancy-avatar-self");
	tag_image_other = gtk_text_tag_table_lookup (table, 
						     "fancy-avatar-other");

	if (!show) {
		g_object_set (tag_text_self,
			      "rise", 0,
			      NULL);
		g_object_set (tag_text_other,
			      "rise", 0,
			      NULL);
		g_object_set (tag_image_self,
			      "invisible", TRUE,
			      NULL);
		g_object_set (tag_image_other,
			      "invisible", TRUE,
			      NULL);
	} else {
		GtkTextAttributes *attrs;
		gint               size;
		gint               rise;

		attrs = gtk_text_view_get_default_attributes (GTK_TEXT_VIEW (view));
		size = pango_font_description_get_size (attrs->font);
		rise = MAX (0, (32 * PANGO_SCALE - size) / 2.0);

		g_object_set (tag_text_self,
			      "rise", rise,
			      NULL);
		g_object_set (tag_text_other,
			      "rise", rise,
			      NULL);
		g_object_set (tag_image_self,
			      "invisible", FALSE,
			      NULL);
		g_object_set (tag_image_other,
			      "invisible", FALSE,
			      NULL);

		gtk_text_attributes_unref (attrs);
	}
}

