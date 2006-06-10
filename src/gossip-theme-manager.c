/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Imendio AB
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

/* TODO:
 *
 * add another theme
 * add _get_all() + UI to choose
 * clear the tags before reusing them
 *
 */

#include <config.h>
#include <string.h>
#include <gconf/gconf-client.h>
#include <gtk/gtk.h>

#include "gossip-theme-manager.h"
#include "gossip-preferences.h"
#include "gossip-app.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_THEME_MANAGER, GossipThemeManagerPriv))

typedef struct {
	gchar    *name;
	guint     notify_id;
} GossipThemeManagerPriv;

static void theme_manager_finalize    (GObject     *object);
static void theme_manager_notify_func (GConfClient *client,
				       guint        id,
				       GConfEntry  *entry,
				       gpointer     user_data);

enum {
	THEME_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

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

	priv->notify_id = gconf_client_notify_add (
		gossip_app_get_gconf_client (),
		GCONF_CHAT_THEME,
		theme_manager_notify_func,
		manager,
		NULL,
		NULL);

	priv->name = gconf_client_get_string (gossip_app_get_gconf_client (),
					      GCONF_CHAT_THEME,
					      NULL);
}

static void
theme_manager_finalize (GObject *object)
{
	GossipThemeManagerPriv *priv;

	priv = GET_PRIV (object);

	if (priv->notify_id) {
		gconf_client_notify_remove (gossip_app_get_gconf_client (),
					    priv->notify_id);
	}
		
	g_free (priv->name);
	
	G_OBJECT_CLASS (gossip_theme_manager_parent_class)->finalize (object);
}

static void
theme_manager_notify_func (GConfClient *client,
			   guint        id,
			   GConfEntry  *entry,
			   gpointer     user_data)
{
	GossipThemeManager     *manager = user_data;
	GossipThemeManagerPriv *priv;
	GConfValue             *value;

	priv = GET_PRIV (manager);

	g_free (priv->name);
	
	value = gconf_entry_get_value (entry);
	if (value == NULL || value->type != GCONF_VALUE_STRING) {
		priv->name = g_strdup ("classic");
	} else {
		priv->name = g_strdup (gconf_value_get_string (value));
	}
	
	g_signal_emit (manager, signals[THEME_CHANGED], 0, NULL);
}

static void
theme_manager_ensure_tag_by_name (GtkTextBuffer *buffer,
				  const gchar   *name)
{
	GtkTextTagTable *table;
	GtkTextTag      *tag;

	table = gtk_text_buffer_get_tag_table (buffer);
	tag = gtk_text_tag_table_lookup (table, name);

	if (!tag) {
		gtk_text_buffer_create_tag (buffer,
					    name,
					    NULL);
	}
}

static GtkTextTag *
theme_manager_init_tag_by_name (GtkTextTagTable *table,
				const gchar     *name)
{
	GtkTextTag *tag;

	tag = gtk_text_tag_table_lookup (table, name);

	if (!tag) {
		return gtk_text_tag_new (name);
	}

	/* FIXME: Clear it... */

	return tag;
}

static void
theme_manager_add_tag (GtkTextTagTable *table,
		       GtkTextTag      *tag)
{
	gchar      *name;
	GtkTextTag *check_tag;

	g_object_get (tag, "name", &name, NULL);
	check_tag = gtk_text_tag_table_lookup (table, name);
	g_free (name);
	if (check_tag) {
		return;
	}	

	gtk_text_tag_table_add (table, tag);
}

static void
theme_manager_fixup_tag_table (GossipThemeManager *theme_manager,
			       GossipChatView     *view)
{
	GtkTextBuffer *buffer;
	
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	theme_manager_ensure_tag_by_name (buffer, "fancy-spacing");
	theme_manager_ensure_tag_by_name (buffer, "fancy-header-self");
	theme_manager_ensure_tag_by_name (buffer, "fancy-line-self");
	theme_manager_ensure_tag_by_name (buffer, "fancy-body-self");
	theme_manager_ensure_tag_by_name (buffer, "fancy-action-self");
	theme_manager_ensure_tag_by_name (buffer, "fancy-highlight-self");
	theme_manager_ensure_tag_by_name (buffer, "fancy-header-other");
	theme_manager_ensure_tag_by_name (buffer, "fancy-line-other");
	theme_manager_ensure_tag_by_name (buffer, "fancy-body-other");
	theme_manager_ensure_tag_by_name (buffer, "fancy-action-other");
	theme_manager_ensure_tag_by_name (buffer, "fancy-highlight-other");
	theme_manager_ensure_tag_by_name (buffer, "fancy-time");
	theme_manager_ensure_tag_by_name (buffer, "fancy-event");
	theme_manager_ensure_tag_by_name (buffer, "fancy-invite");
	theme_manager_ensure_tag_by_name (buffer, "fancy-link");

	theme_manager_ensure_tag_by_name (buffer, "irc-spacing");
	theme_manager_ensure_tag_by_name (buffer, "irc-nick-self");
	theme_manager_ensure_tag_by_name (buffer, "irc-body-self");
	theme_manager_ensure_tag_by_name (buffer, "irc-action-self");
	theme_manager_ensure_tag_by_name (buffer, "irc-nick-highlight");
	theme_manager_ensure_tag_by_name (buffer, "irc-nick-other");
	theme_manager_ensure_tag_by_name (buffer, "irc-body-other");
	theme_manager_ensure_tag_by_name (buffer, "irc-action-other");
	theme_manager_ensure_tag_by_name (buffer, "irc-time");
	theme_manager_ensure_tag_by_name (buffer, "irc-event");
	theme_manager_ensure_tag_by_name (buffer, "irc-invite");
	theme_manager_ensure_tag_by_name (buffer, "irc-link");
}

static void
theme_manager_apply_theme_classic (GossipThemeManager *manager,
				   GossipChatView     *view)
{
	GossipThemeManagerPriv *priv;
	GtkTextBuffer          *buffer;
	GtkTextTagTable        *table;
	GtkTextTag             *tag;

	priv = GET_PRIV (manager);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	table = gtk_text_buffer_get_tag_table (buffer);

	gossip_chat_view_set_irc_style (view, TRUE);

	tag = theme_manager_init_tag_by_name (table, "irc-spacing");
	g_object_set (tag,
		      "size", 2000,
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "irc-nick-self");
	g_object_set (tag,
		      "foreground", "sea green",
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "irc-body-self");
	g_object_set (tag,
		      /* To get the default theme color: */
		      "foreground-set", FALSE, 
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "irc-action-self");
	g_object_set (tag,
		      "foreground", "brown4",
		      "style", PANGO_STYLE_ITALIC,
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "irc-nick-highlight");
	g_object_set (tag,
		      "foreground", "indian red",
		      "weight", PANGO_WEIGHT_BOLD,
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "irc-nick-other");
	g_object_set (tag,
		      "foreground", "skyblue4",
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "irc-body-other");
	g_object_set (tag,
		      /* To get the default theme color: */
		      "foreground-set", FALSE,
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "irc-action-other");
	g_object_set (tag,
		      "foreground", "brown4",
		      "style", PANGO_STYLE_ITALIC,
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "irc-time");
	g_object_set (tag,
		      "foreground", "darkgrey",
		      "justification", GTK_JUSTIFY_CENTER,
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "irc-event");
	g_object_set (tag,
		      "foreground", "PeachPuff4",
		      "justification", GTK_JUSTIFY_LEFT,
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "irc-invite");
	g_object_set (tag,
		      "foreground", "sienna",
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "irc-link");
	g_object_set (tag,
		      "foreground", "steelblue",
		      "underline", PANGO_UNDERLINE_SINGLE,
		      NULL);
	theme_manager_add_tag (table, tag);
}

static void
theme_manager_apply_theme_blue (GossipThemeManager *manager,
				GossipChatView     *view)
{
	GossipThemeManagerPriv *priv;
	GtkTextBuffer          *buffer;
	GtkTextTagTable        *table;
	GtkTextTag             *tag;

	priv = GET_PRIV (manager);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	table = gtk_text_buffer_get_tag_table (buffer);

	gossip_chat_view_set_irc_style (view, FALSE);

#define BLUE_BODY_SELF "#dcdcdc"
#define BLUE_HEAD_SELF "#b9b9b9"
#define BLUE_LINE_SELF "#aeaeae"

#define BLUE_BODY_OTHER "#adbdc8"
#define BLUE_HEAD_OTHER "#88a2b4"
#define BLUE_LINE_OTHER "#7f96a4"

	tag = theme_manager_init_tag_by_name (table, "fancy-spacing");
	g_object_set (tag,
		      "size", 3000,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-header-self");
	g_object_set (tag,
		      "foreground", "black",
		      "paragraph-background", BLUE_HEAD_SELF,
		      "weight", PANGO_WEIGHT_BOLD,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-line-self");
	g_object_set (tag,
		      "size", 1,
		      "paragraph-background", BLUE_LINE_SELF,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-body-self");
	g_object_set (tag,
		      "foreground", "black",
		      "paragraph-background", BLUE_BODY_SELF,
		      "pixels-above-lines", 2,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-action-self");
	g_object_set (tag,
		      "foreground", "brown4",
		      "style", PANGO_STYLE_ITALIC,
		      "paragraph-background", BLUE_BODY_SELF,
		      "pixels-above-lines", 2,
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "fancy-highlight-self");
	g_object_set (tag,
		      "foreground", "black",
		      "weight", PANGO_WEIGHT_BOLD,
		      "paragraph-background", BLUE_BODY_SELF,
		      "pixels-above-lines", 2,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-header-other");
	g_object_set (tag,
		      "foreground", "black",
		      "paragraph-background", BLUE_HEAD_OTHER,
		      "weight", PANGO_WEIGHT_BOLD,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-line-other");
	g_object_set (tag,
		      "size", 1,
		      "paragraph-background", BLUE_LINE_OTHER,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-body-other");
	g_object_set (tag,
		      "foreground", "black",
		      "paragraph-background", BLUE_BODY_OTHER,
		      "pixels-above-lines", 2,
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "fancy-action-other");
	g_object_set (tag,
		      "foreground", "brown4",
		      "style", PANGO_STYLE_ITALIC,
		      "paragraph-background", BLUE_BODY_OTHER,
		      "pixels-above-lines", 2,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-highlight-other");
	g_object_set (tag,
		      "foreground", "black",
		      "weight", PANGO_WEIGHT_BOLD,
		      "paragraph-background", BLUE_BODY_OTHER,
		      "pixels-above-lines", 2,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-time");
	g_object_set (tag,
		      "foreground", "darkgrey",
		      "justification", GTK_JUSTIFY_CENTER,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-event");
	g_object_set (tag,
		      "foreground", BLUE_LINE_OTHER,
		      "justification", GTK_JUSTIFY_LEFT,
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "fancy-invite");
	g_object_set (tag,
		      "foreground", "sienna",
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "fancy-link");
	g_object_set (tag,
		      "foreground", "#49789e",
		      "underline", PANGO_UNDERLINE_SINGLE,
		      NULL);
	theme_manager_add_tag (table, tag);
}

static void
theme_manager_apply_theme_dark (GossipThemeManager *manager,
				GossipChatView     *view)
{
	GossipThemeManagerPriv *priv;
	GtkTextBuffer          *buffer;
	GtkTextTagTable        *table;
	GtkTextTag             *tag;

	priv = GET_PRIV (manager);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	table = gtk_text_buffer_get_tag_table (buffer);

	gossip_chat_view_set_irc_style (view, FALSE);

#define DARK_BODY_SELF "#6d6056"
#define DARK_HEAD_SELF "#584c43" //60564e"
#define DARK_LINE_SELF "#534b45"
#define DARK_TEXT_SELF "#eeeeee"

#define DARK_BODY_OTHER "#969292"
#define DARK_HEAD_OTHER "#827878"
#define DARK_LINE_OTHER "#787474"
#define DARK_TEXT_OTHER "#eeeeee"

	tag = theme_manager_init_tag_by_name (table, "fancy-spacing");
	g_object_set (tag,
		      "size", 3000,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-header-self");
	g_object_set (tag,
		      "foreground", DARK_TEXT_SELF,
		      "paragraph-background", DARK_HEAD_SELF,
		      "weight", PANGO_WEIGHT_BOLD,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-line-self");
	g_object_set (tag,
		      "size", 1,
		      "paragraph-background", DARK_LINE_SELF,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-body-self");
	g_object_set (tag,
		      "foreground", DARK_TEXT_SELF,
		      "paragraph-background", DARK_BODY_SELF,
		      "pixels-above-lines", 2,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-action-self");
	g_object_set (tag,
		      "foreground", "brown4",
		      "style", PANGO_STYLE_ITALIC,
		      "paragraph-background", DARK_BODY_SELF,
		      "pixels-above-lines", 2,
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "fancy-highlight-self");
	g_object_set (tag,
		      "foreground", DARK_TEXT_SELF,
		      "weight", PANGO_WEIGHT_BOLD,
		      "paragraph-background", DARK_BODY_SELF,
		      "pixels-above-lines", 2,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-header-other");
	g_object_set (tag,
		      "foreground", DARK_TEXT_OTHER,
		      "paragraph-background", DARK_HEAD_OTHER,
		      "weight", PANGO_WEIGHT_BOLD,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-line-other");
	g_object_set (tag,
		      "size", 1,
		      "paragraph-background", DARK_LINE_OTHER,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-body-other");
	g_object_set (tag,
		      "foreground", DARK_TEXT_OTHER,
		      "paragraph-background", DARK_BODY_OTHER,
		      "pixels-above-lines", 2,
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "fancy-action-other");
	g_object_set (tag,
		      "foreground", "brown4",
		      "style", PANGO_STYLE_ITALIC,
		      "paragraph-background", DARK_BODY_OTHER,
		      "pixels-above-lines", 2,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-highlight-other");
	g_object_set (tag,
		      "foreground", DARK_TEXT_OTHER,
		      "weight", PANGO_WEIGHT_BOLD,
		      "paragraph-background", DARK_BODY_OTHER,
		      "pixels-above-lines", 2,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-time");
	g_object_set (tag,
		      "foreground", "darkgrey",
		      "justification", GTK_JUSTIFY_CENTER,
		      NULL);
	theme_manager_add_tag (table, tag);
	
	tag = theme_manager_init_tag_by_name (table, "fancy-event");
	g_object_set (tag,
		      "foreground", DARK_LINE_OTHER,
		      "justification", GTK_JUSTIFY_LEFT,
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "fancy-invite");
	g_object_set (tag,
		      "foreground", "sienna",
		      NULL);
	theme_manager_add_tag (table, tag);

	tag = theme_manager_init_tag_by_name (table, "fancy-link");
	g_object_set (tag,
		      "foreground", "#49789e",
		      "underline", PANGO_UNDERLINE_SINGLE,
		      NULL);
	theme_manager_add_tag (table, tag);
}

static void
theme_manager_apply_theme (GossipThemeManager *manager,
			   GossipChatView     *view,
			   const gchar        *name)
{
	if (strcmp (name, "dark") == 0) {
		theme_manager_apply_theme_dark (manager, view);
	}
	else if (strcmp (name, "blue") == 0) {
		theme_manager_apply_theme_blue (manager, view);
	} else {
		theme_manager_apply_theme_classic (manager, view);
	}
	
	/* Make sure all tags are present. Note: not useful now but when we have
	 * user defined theme it will be.
	 */
	theme_manager_fixup_tag_table (manager, view);
}

void
gossip_theme_manager_apply (GossipThemeManager *manager,
			    GossipChatView     *view)
{
	GossipThemeManagerPriv *priv;

	priv = GET_PRIV (manager);
	
	theme_manager_apply_theme (manager, view, priv->name);
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

