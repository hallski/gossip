/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Imendio AB
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
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libgossip/gossip-conf.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-utils.h>

#include "gossip-chat.h"
#include "gossip-preferences.h"
#include "gossip-smiley.h"
#include "gossip-theme.h"

#define DEBUG_DOMAIN "Theme"

/* Number of seconds between timestamps when using normal mode, 5 minutes. */
#define TIMESTAMP_INTERVAL 300

typedef enum {
	THEME_CLEAN,
	THEME_SIMPLE,
	THEME_BLUE,
	THEME_CLASSIC
} ThemeStyle;

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_THEME, GossipThemePriv))

typedef struct _GossipThemePriv GossipThemePriv;

struct _GossipThemePriv {
	ThemeStyle style;
};

static void         theme_finalize            (GObject            *object);
static void         theme_apply_theme_classic (GossipTheme        *theme,
					       GossipChatView     *view);
static void         theme_apply_theme_clean   (GossipTheme        *theme,
					       GossipChatView     *view);
static void         theme_apply_theme_blue    (GossipTheme        *theme,
					       GossipChatView     *view);
static void         theme_apply_theme_simple  (GossipTheme        *theme,
					       GossipChatView     *view);
static void         theme_fixup_tag_table     (GossipTheme        *theme,
					       GossipChatView     *view);
static GossipThemeContext *
theme_setup_with_view                         (GossipTheme        *theme,
					       GossipChatView     *view);
static void         theme_view_cleared        (GossipTheme        *theme,
					       GossipThemeContext *context,
					       GossipChatView     *view);
static void         theme_append_message      (GossipTheme        *theme,
					       GossipThemeContext *context,
					       GossipChatView     *view,
					       GossipMessage      *message,
					       gboolean            from_self);
static void         theme_append_action       (GossipTheme        *theme,
					       GossipThemeContext *context,
					       GossipChatView     *view,
					       GossipMessage      *message,
					       gboolean            from_self);
static void         theme_append_event        (GossipTheme        *theme,
					       GossipThemeContext *context,
					       GossipChatView     *view,
					       const gchar        *str);
static void         theme_append_timestamp    (GossipTheme        *theme,
					       GossipThemeContext *context,
					       GossipChatView     *view,
					       GossipMessage      *message,
					       gboolean            show_date,
					       gboolean            show_time);

G_DEFINE_TYPE (GossipTheme, gossip_theme, G_TYPE_OBJECT);

static void
gossip_theme_class_init (GossipThemeClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize     = theme_finalize;

	class->setup_with_view  = theme_setup_with_view;
	class->view_cleared     = theme_view_cleared;
	class->append_message   = theme_append_message;
	class->append_action    = theme_append_action;
	class->append_event     = theme_append_event;
	class->append_timestamp = theme_append_timestamp;

	g_type_class_add_private (object_class, sizeof (GossipThemePriv));
}

static void
gossip_theme_init (GossipTheme *presence)
{
	GossipThemePriv *priv;

	priv = GET_PRIV (presence);
}

static void
theme_finalize (GObject *object)
{
	GossipThemePriv *priv;

	priv = GET_PRIV (object);

	(G_OBJECT_CLASS (gossip_theme_parent_class)->finalize) (object);
}

static GtkTextTag *
theme_init_tag_by_name (GtkTextTagTable *table, const gchar *name)
{
	GtkTextTag *tag;

	tag = gtk_text_tag_table_lookup (table, name);

	if (!tag) {
		return gtk_text_tag_new (name);
	}

	/* Clear the old values so that we don't affect the new theme. */
	g_object_set (tag,
		      "background-set", FALSE,
		      "foreground-set", FALSE,
		      "invisible-set", FALSE,
		      "justification-set", FALSE,
		      "paragraph-background-set", FALSE,
		      "pixels-above-lines-set", FALSE,
		      "pixels-below-lines-set", FALSE,
		      "rise-set", FALSE,
		      "scale-set", FALSE,
		      "size-set", FALSE,
		      "style-set", FALSE,
		      "weight-set", FALSE,
		      NULL);

	return tag;
}

static void
theme_add_tag (GtkTextTagTable *table, GtkTextTag *tag)
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

	g_object_unref (tag);
}


static void
theme_apply_theme_classic (GossipTheme *theme, GossipChatView *view)
{
	GossipThemePriv *priv;
	GtkTextBuffer   *buffer;
	GtkTextTagTable *table;
	GtkTextTag      *tag;

	priv = GET_PRIV (theme);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	table = gtk_text_buffer_get_tag_table (buffer);

	tag = theme_init_tag_by_name (table, "irc-spacing");
	g_object_set (tag,
		      "size", 2000,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "irc-nick-self");
	g_object_set (tag,
		      "foreground", "sea green",
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "irc-body-self");
	g_object_set (tag,
		      /* To get the default theme color: */
		      "foreground-set", FALSE,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "irc-action-self");
	g_object_set (tag,
		      "foreground", "brown4",
		      "style", PANGO_STYLE_ITALIC,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "irc-nick-highlight");
	g_object_set (tag,
		      "foreground", "indian red",
		      "weight", PANGO_WEIGHT_BOLD,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "irc-nick-other");
	g_object_set (tag,
		      "foreground", "skyblue4",
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "irc-body-other");
	g_object_set (tag,
		      /* To get the default theme color: */
		      "foreground-set", FALSE,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "irc-action-other");
	g_object_set (tag,
		      "foreground", "brown4",
		      "style", PANGO_STYLE_ITALIC,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "irc-time");
	g_object_set (tag,
		      "foreground", "darkgrey",
		      "justification", GTK_JUSTIFY_CENTER,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "irc-event");
	g_object_set (tag,
		      "foreground", "PeachPuff4",
		      "justification", GTK_JUSTIFY_LEFT,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "invite");
	g_object_set (tag,
		      "foreground", "sienna",
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "irc-link");
	g_object_set (tag,
		      "foreground", "steelblue",
		      "underline", PANGO_UNDERLINE_SINGLE,
		      NULL);
	theme_add_tag (table, tag);
}


static void
theme_apply_theme_clean (GossipTheme *theme, GossipChatView *view)
{
	GossipThemePriv *priv;
	GtkTextBuffer   *buffer;
	GtkTextTagTable *table;
	GtkTextTag      *tag;

	priv = GET_PRIV (theme);

	/* Inherit the simple theme. */
	theme_apply_theme_simple (theme, view);

#define ELEGANT_HEAD "#efefdf"
#define ELEGANT_LINE "#e3e3d3"

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	table = gtk_text_buffer_get_tag_table (buffer);

	tag = theme_init_tag_by_name (table, "fancy-spacing");
	g_object_set (tag,
		      "size", PANGO_SCALE * 10,
		      NULL);

	tag = theme_init_tag_by_name (table, "fancy-header-self");
	g_object_set (tag,
		      "foreground", "black",
		      "weight", PANGO_WEIGHT_BOLD,
		      "paragraph-background", ELEGANT_HEAD,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      NULL);

	tag = theme_init_tag_by_name (table, "fancy-avatar-self");
	g_object_set (tag,
		      "paragraph-background", ELEGANT_HEAD,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      NULL);

	tag = theme_init_tag_by_name (table, "fancy-line-top-self");
	g_object_set (tag,
		      "size", 1 * PANGO_SCALE,
		      "paragraph-background", ELEGANT_LINE,
		      NULL);

	tag = theme_init_tag_by_name (table, "fancy-line-bottom-self");
	g_object_set (tag,
		      "size", 1 * PANGO_SCALE,
		      NULL);

	tag = theme_init_tag_by_name (table, "fancy-action-self");
	g_object_set (tag,
		      "foreground", "brown4",
		      "style", PANGO_STYLE_ITALIC,
		      NULL);

	tag = theme_init_tag_by_name (table, "fancy-highlight-self");
	g_object_set (tag,
		      "foreground", "black",
		      "weight", PANGO_WEIGHT_BOLD,
		      NULL);

	tag = theme_init_tag_by_name (table, "fancy-header-other");
	g_object_set (tag,
		      "foreground", "black",
		      "weight", PANGO_WEIGHT_BOLD,
		      "paragraph-background", ELEGANT_HEAD,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      NULL);

	tag = theme_init_tag_by_name (table, "fancy-avatar-other");
	g_object_set (tag,
		      "paragraph-background", ELEGANT_HEAD,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      NULL);

	tag = theme_init_tag_by_name (table, "fancy-line-top-other");
	g_object_set (tag,
		      "size", 1 * PANGO_SCALE,
		      "paragraph-background", ELEGANT_LINE,
		      NULL);

	tag = theme_init_tag_by_name (table, "fancy-line-bottom-other");
	g_object_set (tag,
		      "size", 1 * PANGO_SCALE,
		      NULL);

	tag = theme_init_tag_by_name (table, "fancy-action-other");
	g_object_set (tag,
		      "foreground", "brown4",
		      "style", PANGO_STYLE_ITALIC,
		      NULL);

	tag = theme_init_tag_by_name (table, "fancy-time");
	g_object_set (tag,
		      "foreground", "darkgrey",
		      "justification", GTK_JUSTIFY_CENTER,
		      NULL);

	tag = theme_init_tag_by_name (table, "fancy-event");
	g_object_set (tag,
		      "foreground", "darkgrey",
		      "justification", GTK_JUSTIFY_LEFT,
		      NULL);

	tag = theme_init_tag_by_name (table, "invite");
	g_object_set (tag,
		      "foreground", "sienna",
		      NULL);

	tag = theme_init_tag_by_name (table, "fancy-link");
	g_object_set (tag,
		      "foreground", "#49789e",
		      "underline", PANGO_UNDERLINE_SINGLE,
		      NULL);
}

static void
theme_apply_theme_blue (GossipTheme *theme, GossipChatView *view)
{
	GossipThemePriv *priv;
	GtkTextBuffer   *buffer;
	GtkTextTagTable *table;
	GtkTextTag      *tag;

	priv = GET_PRIV (theme);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	table = gtk_text_buffer_get_tag_table (buffer);

#define BLUE_BODY_SELF "#dcdcdc"
#define BLUE_HEAD_SELF "#b9b9b9"
#define BLUE_LINE_SELF "#aeaeae"

#define BLUE_BODY_OTHER "#adbdc8"
#define BLUE_HEAD_OTHER "#88a2b4"
#define BLUE_LINE_OTHER "#7f96a4"

	tag = theme_init_tag_by_name (table, "fancy-spacing");
	g_object_set (tag,
		      "size", 3000,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-header-self");
	g_object_set (tag,
		      "foreground", "black",
		      "paragraph-background", BLUE_HEAD_SELF,
		      "weight", PANGO_WEIGHT_BOLD,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-header-self-avatar");
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-avatar-self");
	g_object_set (tag,
		      "paragraph-background", BLUE_HEAD_SELF,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-line-top-self");
	g_object_set (tag,
		      "size", 1,
		      "paragraph-background", BLUE_LINE_SELF,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-line-bottom-self");
	g_object_set (tag,
		      "size", 1,
		      "paragraph-background", BLUE_LINE_SELF,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-body-self");
	g_object_set (tag,
		      "foreground", "black",
		      "paragraph-background", BLUE_BODY_SELF,
		      "pixels-above-lines", 4,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-action-self");
	g_object_set (tag,
		      "foreground", "brown4",
		      "style", PANGO_STYLE_ITALIC,
		      "paragraph-background", BLUE_BODY_SELF,
		      "pixels-above-lines", 4,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-highlight-self");
	g_object_set (tag,
		      "foreground", "black",
		      "weight", PANGO_WEIGHT_BOLD,
		      "paragraph-background", BLUE_BODY_SELF,
		      "pixels-above-lines", 4,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-header-other");
	g_object_set (tag,
		      "foreground", "black",
		      "paragraph-background", BLUE_HEAD_OTHER,
		      "weight", PANGO_WEIGHT_BOLD,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-header-other-avatar");
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-avatar-other");
	g_object_set (tag,
		      "paragraph-background", BLUE_HEAD_OTHER,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-line-top-other");
	g_object_set (tag,
		      "size", 1,
		      "paragraph-background", BLUE_LINE_OTHER,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-line-bottom-other");
	g_object_set (tag,
		      "size", 1,
		      "paragraph-background", BLUE_LINE_OTHER,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-body-other");
	g_object_set (tag,
		      "foreground", "black",
		      "paragraph-background", BLUE_BODY_OTHER,
		      "pixels-above-lines", 4,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-action-other");
	g_object_set (tag,
		      "foreground", "brown4",
		      "style", PANGO_STYLE_ITALIC,
		      "paragraph-background", BLUE_BODY_OTHER,
		      "pixels-above-lines", 4,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-highlight-other");
	g_object_set (tag,
		      "foreground", "black",
		      "weight", PANGO_WEIGHT_BOLD,
		      "paragraph-background", BLUE_BODY_OTHER,
		      "pixels-above-lines", 4,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-time");
	g_object_set (tag,
		      "foreground", "darkgrey",
		      "justification", GTK_JUSTIFY_CENTER,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-event");
	g_object_set (tag,
		      "foreground", BLUE_LINE_OTHER,
		      "justification", GTK_JUSTIFY_LEFT,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "invite");
	g_object_set (tag,
		      "foreground", "sienna",
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-link");
	g_object_set (tag,
		      "foreground", "#49789e",
		      "underline", PANGO_UNDERLINE_SINGLE,
		      NULL);
	theme_add_tag (table, tag);
}

static void
theme_apply_theme_simple (GossipTheme *theme, GossipChatView *view)
{
	GossipThemePriv *priv;
	GtkTextBuffer   *buffer;
	GtkTextTagTable *table;
	GtkTextTag      *tag;
	GtkWidget       *widget;
	GtkStyle        *style;

	priv = GET_PRIV (theme);

	widget = gtk_entry_new ();
	style = gtk_widget_get_style (widget);
	gtk_widget_destroy (widget);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	table = gtk_text_buffer_get_tag_table (buffer);

	tag = theme_init_tag_by_name (table, "fancy-spacing");
	g_object_set (tag,
		      "size", 3000,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-header-self");
	g_object_set (tag,
		      "weight", PANGO_WEIGHT_BOLD,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-header-self-avatar");
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-avatar-self");
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-line-top-self");
	g_object_set (tag,
		      "size", 6 * PANGO_SCALE,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-line-bottom-self");
	g_object_set (tag,
		      "size", 1,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-body-self");
	g_object_set (tag,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-action-self");
	g_object_set (tag,
		      "foreground-gdk", &style->base[GTK_STATE_SELECTED],
		      "style", PANGO_STYLE_ITALIC,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-highlight-self");
	g_object_set (tag,
		      "weight", PANGO_WEIGHT_BOLD,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-header-other");
	g_object_set (tag,
		      "weight", PANGO_WEIGHT_BOLD,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-header-other-avatar");
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-avatar-other");
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-line-top-other");
	g_object_set (tag,
		      "size", 6 * PANGO_SCALE,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-line-bottom-other");
	g_object_set (tag,
		      "size", 1,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-body-other");
	g_object_set (tag,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-action-other");
	g_object_set (tag,
		      "foreground-gdk", &style->base[GTK_STATE_SELECTED],
		      "style", PANGO_STYLE_ITALIC,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-highlight-other");
	g_object_set (tag,
		      "weight", PANGO_WEIGHT_BOLD,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-time");
	g_object_set (tag,
		      "foreground", "darkgrey",
		      "justification", GTK_JUSTIFY_CENTER,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-event");
	g_object_set (tag,
		      "foreground", "darkgrey",
		      "justification", GTK_JUSTIFY_LEFT,
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "invite");
	g_object_set (tag,
		      "foreground", "darkgrey",
		      NULL);
	theme_add_tag (table, tag);

	tag = theme_init_tag_by_name (table, "fancy-link");
	g_object_set (tag,
		      "foreground-gdk", &style->base[GTK_STATE_SELECTED],
		      "underline", PANGO_UNDERLINE_SINGLE,
		      NULL);
	theme_add_tag (table, tag);
}

static void
theme_ensure_tag_by_name (GtkTextBuffer *buffer, const gchar *name)
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

static void
theme_fixup_tag_table (GossipTheme *theme, GossipChatView *view)
{
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	/* "Fancy" style tags. */
	theme_ensure_tag_by_name (buffer, "fancy-header-self");
	theme_ensure_tag_by_name (buffer, "fancy-header-self-avatar");
	theme_ensure_tag_by_name (buffer, "fancy-avatar-self");
	theme_ensure_tag_by_name (buffer, "fancy-line-top-self");
	theme_ensure_tag_by_name (buffer, "fancy-line-bottom-self");
	theme_ensure_tag_by_name (buffer, "fancy-body-self");
	theme_ensure_tag_by_name (buffer, "fancy-action-self");
	theme_ensure_tag_by_name (buffer, "fancy-highlight-self");

	theme_ensure_tag_by_name (buffer, "fancy-header-other");
	theme_ensure_tag_by_name (buffer, "fancy-header-other-avatar");
	theme_ensure_tag_by_name (buffer, "fancy-avatar-other");
	theme_ensure_tag_by_name (buffer, "fancy-line-top-other");
	theme_ensure_tag_by_name (buffer, "fancy-line-bottom-other");
	theme_ensure_tag_by_name (buffer, "fancy-body-other");
	theme_ensure_tag_by_name (buffer, "fancy-action-other");
	theme_ensure_tag_by_name (buffer, "fancy-highlight-other");

	theme_ensure_tag_by_name (buffer, "fancy-spacing");
	theme_ensure_tag_by_name (buffer, "fancy-time");
	theme_ensure_tag_by_name (buffer, "fancy-event");
	theme_ensure_tag_by_name (buffer, "fancy-link");

	/* IRC style tags. */
	theme_ensure_tag_by_name (buffer, "irc-nick-self");
	theme_ensure_tag_by_name (buffer, "irc-body-self");
	theme_ensure_tag_by_name (buffer, "irc-action-self");

	theme_ensure_tag_by_name (buffer, "irc-nick-other");
	theme_ensure_tag_by_name (buffer, "irc-body-other");
	theme_ensure_tag_by_name (buffer, "irc-action-other");

	theme_ensure_tag_by_name (buffer, "irc-nick-highlight");
	theme_ensure_tag_by_name (buffer, "irc-spacing");
	theme_ensure_tag_by_name (buffer, "irc-time");
	theme_ensure_tag_by_name (buffer, "irc-event");
	theme_ensure_tag_by_name (buffer, "irc-link");
}


static GossipThemeContext *
theme_setup_with_view (GossipTheme *theme, GossipChatView *view)
{
	GossipThemePriv *priv;
	gint             margin;

	g_return_val_if_fail (GOSSIP_IS_THEME (theme), NULL);

	priv = GET_PRIV (theme);

	theme_fixup_tag_table (theme, view);

	switch (priv->style) {
	case THEME_CLEAN:
		theme_apply_theme_clean (theme, view);
		gossip_chat_view_set_is_irc_style (view, FALSE);
		margin = 3;
		break;
	case THEME_SIMPLE:
		theme_apply_theme_simple (theme, view);
		gossip_chat_view_set_is_irc_style (view, FALSE);
		margin = 3;
		break;
	case THEME_BLUE:
		theme_apply_theme_blue (theme, view);
		gossip_chat_view_set_is_irc_style (view, FALSE);
		margin = 0;
		break;
	case THEME_CLASSIC:
		theme_apply_theme_classic (theme, view);
		gossip_chat_view_set_is_irc_style (view, TRUE);
		margin = 3;
		break;
	};
	
	gossip_chat_view_set_margin (view, margin);

	return NULL;
#if 0
} else {
		FancyContext *context;

		context = g_slice_new (FancyContext);

		return context;
	}
#endif

}

static void
theme_view_cleared (GossipTheme        *theme,
		    GossipThemeContext *context,
		    GossipChatView     *view)
{
	/* Clear the context data */
}

static GDate *
theme_get_date_and_time_from_message (GossipMessage *message,
				      time_t        *timestamp)
{
	GDate *date;

	*timestamp = 0;
	if (message) {
		*timestamp = gossip_message_get_timestamp (message);
	}

	if (timestamp <= 0) {
		*timestamp = gossip_time_get_current ();
	}

	date = g_date_new ();
	g_date_set_time (date, *timestamp);

	return date;
}


static void
theme_maybe_append_date_and_time (GossipTheme        *theme,
				  GossipThemeContext *context,
				  GossipChatView     *view,
				  GossipMessage      *message)
{
	time_t    timestamp;
	GDate    *date, *last_date;
	gboolean  append_date, append_time;

	if (gossip_chat_view_get_last_block_type (view) == BLOCK_TYPE_TIME) {
		return;
	}

	date = theme_get_date_and_time_from_message (message, &timestamp);

	last_date = g_date_new ();
	g_date_set_time (last_date, gossip_chat_view_get_last_timestamp (view));

	append_date = FALSE;
	append_time = FALSE;

	if (g_date_compare (date, last_date) > 0) {
		append_date = TRUE;
		append_time = TRUE;
	}
	
	g_date_free (last_date);
	g_date_free (date);

	if (gossip_chat_view_get_last_timestamp (view) + TIMESTAMP_INTERVAL < timestamp) {
		append_time = TRUE;
	}

	if (append_time || append_date) {
		gossip_theme_append_timestamp (theme, context,
					       view, message,
					       append_date, append_time);
	}
}



static void
theme_append_irc_message (GossipTheme        *theme,
			  GossipThemeContext *context,
			  GossipChatView     *view,
			  GossipMessage      *msg,
			  gboolean            from_self)
{
	GtkTextBuffer *buffer;
	const gchar   *name;
	const gchar   *nick_tag;
	const gchar   *body_tag;
	GtkTextIter    iter;
	gchar         *tmp;
	GossipContact *contact;

	gossip_debug (DEBUG_DOMAIN, "Add IRC message");

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	contact = gossip_message_get_sender (msg);
	name = gossip_contact_get_name (contact);

	if (from_self) {
		nick_tag = "irc-nick-self";
		body_tag = "irc-body-self";
	} else {
		if (gossip_chat_should_highlight_nick (msg, 
						       gossip_message_get_recipient (msg))) {
			nick_tag = "irc-nick-highlight";
		} else {
			nick_tag = "irc-nick-other";
		}

		body_tag = "irc-body-other";
	}
		
	if (gossip_chat_view_get_last_block_type (view) != BLOCK_TYPE_SELF &&
	    gossip_chat_view_get_last_block_type (view) != BLOCK_TYPE_OTHER) {
		gossip_theme_append_spacing (theme, context, view);
	}

	gtk_text_buffer_get_end_iter (buffer, &iter);

	/* The nickname. */
	tmp = g_strdup_printf ("%s: ", name);
	gtk_text_buffer_insert_with_tags_by_name (buffer,
						  &iter,
						  tmp,
						  -1,
						  "cut",
						  nick_tag,
						  NULL);
	g_free (tmp);

	/* The text body. */
	gossip_theme_append_text (theme, context, view, 
				  gossip_message_get_body (msg),
				  body_tag);
}

static void
theme_maybe_append_fancy_header (GossipTheme        *theme,
				 GossipThemeContext *context,
				 GossipChatView     *view,
				 GossipMessage      *msg,
				 gboolean            from_self)
{
	GossipContact      *contact;
	GdkPixbuf          *avatar;
	GtkTextBuffer      *buffer;
	const gchar        *name;
	gboolean            header;
	GtkTextIter         iter;
	gchar              *tmp;
	const gchar        *tag;
	const gchar        *avatar_tag;
	const gchar        *line_top_tag;
	const gchar        *line_bottom_tag;

	contact = gossip_message_get_sender (msg);
	avatar = gossip_contact_get_avatar_pixbuf (contact);
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	gossip_debug (DEBUG_DOMAIN, "Maybe add fancy header");

	name = gossip_contact_get_name (contact);

	if (from_self) {
		tag = "fancy-header-self";
		line_top_tag = "fancy-line-top-self";
		line_bottom_tag = "fancy-line-bottom-self";
	} else {
		tag = "fancy-header-other";
		line_top_tag = "fancy-line-top-other";
		line_bottom_tag = "fancy-line-bottom-other";
	}

	header = FALSE;

	/* Only insert a header if the previously inserted block is not the same
	 * as this one. This catches all the different cases:
	 */
	if (gossip_chat_view_get_last_block_type (view) != BLOCK_TYPE_SELF &&
	    gossip_chat_view_get_last_block_type (view) != BLOCK_TYPE_OTHER) {
		header = TRUE;
	}
	else if (from_self &&
		 gossip_chat_view_get_last_block_type (view) == BLOCK_TYPE_OTHER) {
		header = TRUE;
	}
	else if (!from_self && 
		 gossip_chat_view_get_last_block_type (view) == BLOCK_TYPE_SELF) {
		header = TRUE;
	}
	else if (!from_self &&
		 (!gossip_chat_view_get_last_contact (view) ||
		  !gossip_contact_equal (contact, gossip_chat_view_get_last_contact (view)))) {
		header = TRUE;
	}

	if (!header) {
		return;
	}

	gossip_theme_append_spacing (theme, context, view);

	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (buffer,
						  &iter,
						  "\n",
						  -1,
						  line_top_tag,
						  NULL);

	if (avatar) {
		GtkTextIter start;

		gtk_text_buffer_get_end_iter (buffer, &iter);
		gtk_text_buffer_insert_pixbuf (buffer, &iter, avatar);

		gtk_text_buffer_get_end_iter (buffer, &iter);
		start = iter;
		gtk_text_iter_backward_char (&start);

		if (from_self) {
			gtk_text_buffer_apply_tag_by_name (buffer,
							   "fancy-avatar-self",
							   &start, &iter);
			avatar_tag = "fancy-header-self-avatar";
		} else {
			gtk_text_buffer_apply_tag_by_name (buffer,
							   "fancy-avatar-other",
							   &start, &iter);
			avatar_tag = "fancy-header-other-avatar";
		}

	} else {
		avatar_tag = NULL;
	}

	tmp = g_strdup_printf ("%s\n", name);

	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (buffer,
						  &iter,
						  tmp,
						  -1,
						  tag,
						  avatar_tag,
						  NULL);
	g_free (tmp);

	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (buffer,
						  &iter,
						  "\n",
						  -1,
						  line_bottom_tag,
						  NULL);
}

static void
theme_append_fancy_message (GossipTheme        *theme,
			    GossipThemeContext *context,
			    GossipChatView     *view,
			    GossipMessage      *msg,
			    gboolean            from_self)
{
	const gchar *tag;

	theme_maybe_append_fancy_header (theme, context, view, msg,
					 from_self);

	if (from_self) {
		tag = "fancy-body-self";
	} else {
		tag = "fancy-body-other";

		/* FIXME: Might want to support nick highlighting here... */
	}

	gossip_theme_append_text (theme, context, view, 
				  gossip_message_get_body (msg),
				  tag);
}

static void
theme_append_message (GossipTheme        *theme,
		      GossipThemeContext *context,
		      GossipChatView     *view,
		      GossipMessage      *message,
		      gboolean            from_self)
{
	theme_maybe_append_date_and_time (theme, context, view, message);

	if (gossip_chat_view_is_irc_style (view)) {
		theme_append_irc_message (theme, context, view, message,
					  from_self);
	} else {
		theme_append_fancy_message (theme, context, view, message, 
					    from_self);
	}

	if (from_self) {
		gossip_chat_view_set_last_block_type (view, BLOCK_TYPE_SELF);
		gossip_chat_view_set_last_contact (view, NULL);
	} else {
		gossip_chat_view_set_last_block_type (view, BLOCK_TYPE_OTHER);
		gossip_chat_view_set_last_contact (view, 
						   gossip_message_get_sender (message));
	}
}

static void
theme_append_irc_action (GossipTheme        *theme,
			 GossipThemeContext *context,
			 GossipChatView     *view,
			 GossipMessage      *msg,
			 gboolean            from_self)
{
	const gchar *name;
	gchar       *tmp;
	const gchar *tag;
	GossipContact *contact;

	contact = gossip_message_get_sender (msg);
	name = gossip_contact_get_name (contact);

	gossip_debug (DEBUG_DOMAIN, "Add IRC action");

	/* Skip the "/me ". */
	if (from_self) {
		tag = "irc-action-self";
	} else {
		tag = "irc-action-other";
	}

	if (gossip_chat_view_get_last_block_type (view) != BLOCK_TYPE_SELF &&
	    gossip_chat_view_get_last_block_type (view) != BLOCK_TYPE_OTHER) {
		gossip_theme_append_spacing (theme, context, view);
	}

	tmp = gossip_message_get_action_string (msg);
	gossip_theme_append_text (theme, context, view, tmp, tag);
	g_free (tmp);
}

static void
theme_append_fancy_action (GossipTheme        *theme,
			   GossipThemeContext *context,
			   GossipChatView     *view,
			   GossipMessage      *msg,
			   gboolean            from_self)
{
	GossipContact *contact;
	const gchar   *name;
	gchar         *tmp;
	const gchar   *tag;
	const gchar   *line_tag;

	gossip_debug (DEBUG_DOMAIN, "Add fancy action");

	contact = gossip_message_get_sender (msg);
	
	theme_maybe_append_fancy_header (theme, context, view, msg,
					 from_self);

	contact = gossip_message_get_sender (msg);
	name = gossip_contact_get_name (contact);

	if (from_self) {
		tag = "fancy-action-self";
		line_tag = "fancy-line-self";
	} else {
		tag = "fancy-action-other";
		line_tag = "fancy-line-other";
	}

	tmp = gossip_message_get_action_string (msg);
	gossip_theme_append_text (theme, context, view, tmp, tag);
	g_free (tmp);
}


static void
theme_append_action (GossipTheme        *theme,
		     GossipThemeContext *context,
		     GossipChatView     *view,
		     GossipMessage      *message,
		     gboolean            from_self)
{
	theme_maybe_append_date_and_time (theme, context, view, message);

	if (gossip_chat_view_is_irc_style (view)) {
		theme_append_irc_action (theme, context, view, message, 
					 from_self);
	} else {
		theme_append_fancy_action (theme, context, view, message, 
					   from_self);
	}

	if (from_self) {
		gossip_chat_view_set_last_block_type (view, BLOCK_TYPE_SELF);
		gossip_chat_view_set_last_contact (view, NULL);
	} else {
		gossip_chat_view_set_last_block_type (view, BLOCK_TYPE_OTHER);
		gossip_chat_view_set_last_contact (view, 
						   gossip_message_get_sender (message));
	}
}

static void
theme_append_event (GossipTheme        *theme,
		    GossipThemeContext *context,
		    GossipChatView     *view,
		    const gchar        *str)
{
	GtkTextBuffer *buffer;
	GtkTextIter    iter;
	gchar         *msg;
	const gchar   *tag;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	if (gossip_chat_view_is_irc_style (view)) {
		tag = "irc-event";
		msg = g_strdup_printf (" - %s\n", str);
	} else {
		tag = "fancy-event";
		msg = g_strdup_printf (" - %s\n", str);
	}

	theme_maybe_append_date_and_time (theme, context, view, NULL);

	gtk_text_buffer_get_end_iter (buffer, &iter);

	gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
						  msg, -1,
						  tag,
						  NULL);
	g_free (msg);

	gossip_chat_view_set_last_block_type (view, BLOCK_TYPE_EVENT);
}

static void
theme_append_timestamp (GossipTheme        *theme,
			GossipThemeContext *context,
			GossipChatView     *view,
			GossipMessage      *message,
			gboolean            show_date,
			gboolean            show_time)
{
	GtkTextBuffer *buffer;
	const gchar   *tag;
	time_t         timestamp;
	GDate         *date;
	GtkTextIter    iter;
	GString       *str;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	if (gossip_chat_view_is_irc_style (view)) {
		tag = "irc-time";
	} else {
		tag = "fancy-time";
	}

	date = theme_get_date_and_time_from_message (message, &timestamp);

	str = g_string_new (NULL);

	if (show_time || show_date) {
		gossip_theme_append_spacing (theme, 
					     context,
					     view);

		g_string_append (str, "- ");
	}

	if (show_date) {
		gchar buf[256];

		g_date_strftime (buf, 256, _("%A %d %B %Y"), date);
		g_string_append (str, buf);

		if (show_time) {
			g_string_append (str, ", ");
		}
	}

	g_date_free (date);

	if (show_time) {
		gchar *tmp;

		tmp = gossip_time_to_string_local (timestamp, GOSSIP_TIME_FORMAT_DISPLAY_SHORT);
		g_string_append (str, tmp);
		g_free (tmp);
	}

	if (show_time || show_date) {
		g_string_append (str, " -\n");

		gtk_text_buffer_get_end_iter (buffer, &iter);
		gtk_text_buffer_insert_with_tags_by_name (buffer,
							  &iter,
							  str->str, -1,
							  tag,
							  NULL);

		gossip_chat_view_set_last_block_type (view, BLOCK_TYPE_TIME);
		gossip_chat_view_set_last_timestamp (view, timestamp);
	}

	g_string_free (str, TRUE);
	
}

GossipTheme *
gossip_theme_new (const gchar *name)
{
	GossipTheme     *theme;
	GossipThemePriv *priv;

	theme = g_object_new (GOSSIP_TYPE_THEME, NULL);
	priv  = GET_PRIV (theme);

	if (strcmp (name, "clean") == 0) {
		priv->style = THEME_CLEAN;
	}
	else if (strcmp (name, "simple") == 0) {
		priv->style = THEME_SIMPLE;
	}
	else if (strcmp (name, "blue") == 0) {
		priv->style = THEME_BLUE;
	} else {
		priv->style = THEME_CLASSIC;
	}

	return theme;
}

GossipThemeContext *
gossip_theme_setup_with_view (GossipTheme    *theme,
			      GossipChatView *view)
{
	return GOSSIP_THEME_GET_CLASS(theme)->setup_with_view (theme, view);
}

void
gossip_theme_view_cleared (GossipTheme        *theme,
			   GossipThemeContext *context,
			   GossipChatView     *view)
{
	GOSSIP_THEME_GET_CLASS(theme)->view_cleared (theme, context, view);
}

void
gossip_theme_append_message (GossipTheme        *theme,
			     GossipThemeContext *context,
			     GossipChatView     *view,
			     GossipMessage      *message,
			     gboolean            from_self)
{
	GOSSIP_THEME_GET_CLASS(theme)->append_message (theme, context, view,
						       message, from_self);
}

void
gossip_theme_append_action (GossipTheme        *theme,
			    GossipThemeContext *context,
			    GossipChatView     *view,
			    GossipMessage      *message,
			    gboolean            from_self)
{
	GOSSIP_THEME_GET_CLASS(theme)->append_action (theme, context, view,
						      message, from_self);
}

static void
theme_insert_text_with_emoticons (GtkTextBuffer *buf,
				  GtkTextIter   *iter,
				  const gchar   *str)
{
	const gchar *p;
	gunichar     c, prev_c;
	gint         i;
	gint         match;
	gint         submatch;
	gboolean     use_smileys = FALSE;

	gossip_conf_get_bool (gossip_conf_get (),
			      GOSSIP_PREFS_CHAT_SHOW_SMILEYS,
			      &use_smileys);

	if (!use_smileys) {
		gtk_text_buffer_insert (buf, iter, str, -1);
		return;
	}

	while (*str) {
		gint         smileys_index[G_N_ELEMENTS (smileys)];
		GdkPixbuf   *pixbuf;
		gint         len;
		const gchar *start;

		memset (smileys_index, 0, sizeof (smileys_index));

		match = -1;
		submatch = -1;
		p = str;
		prev_c = 0;

		while (*p) {
			c = g_utf8_get_char (p);

			if (match != -1 && g_unichar_isspace (c)) {
				break;
			} else {
				match = -1;
			}

			if (submatch != -1 || prev_c == 0 || g_unichar_isspace (prev_c)) {
				submatch = -1;

				for (i = 0; i < G_N_ELEMENTS (smileys); i++) {
					/* Only try to match if we already have
					 * a beginning match for the pattern, or
					 * if it's the first character in the
					 * pattern, if it's not in the middle of
					 * a word.
					 */
					if (((smileys_index[i] == 0 && (prev_c == 0 || g_unichar_isspace (prev_c))) ||
					     smileys_index[i] > 0) &&
					    smileys[i].pattern[smileys_index[i]] == c) {
						submatch = i;

						smileys_index[i]++;
						if (!smileys[i].pattern[smileys_index[i]]) {
							match = i;
						}
					} else {
						smileys_index[i] = 0;
					}
				}
			}

			prev_c = c;
			p = g_utf8_next_char (p);
		}

		if (match == -1) {
			gtk_text_buffer_insert (buf, iter, str, -1);
			return;
		}

		start = p - strlen (smileys[match].pattern);

		if (start > str) {
			len = start - str;
			gtk_text_buffer_insert (buf, iter, str, len);
		}

		pixbuf = gossip_chat_view_get_smiley_image (smileys[match].smiley);
		gtk_text_buffer_insert_pixbuf (buf, iter, pixbuf);

		gtk_text_buffer_insert (buf, iter, " ", 1);

		str = g_utf8_find_next_char (p, NULL);
	}
}

void
gossip_theme_append_text (GossipTheme        *theme,
			  GossipThemeContext *context,
			  GossipChatView     *view,
			  const gchar        *body,
			  const gchar        *tag)
{
	GossipThemePriv *priv;
	GtkTextBuffer   *buffer;
	GtkTextIter      start_iter, end_iter;
	GtkTextMark     *mark;
	GtkTextIter      iter;
	gint             num_matches, i;
	GArray          *start, *end;
	const gchar     *link_tag;

	priv = GET_PRIV (theme);
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	if (gossip_chat_view_is_irc_style (view)) {
		link_tag = "irc-link";
	} else {
		link_tag = "fancy-link";
	}

	gtk_text_buffer_get_end_iter (buffer, &start_iter);
	mark = gtk_text_buffer_create_mark (buffer, NULL, &start_iter, TRUE);

	start = g_array_new (FALSE, FALSE, sizeof (gint));
	end = g_array_new (FALSE, FALSE, sizeof (gint));

	num_matches = gossip_regex_match (GOSSIP_REGEX_ALL, body, start, end);

	if (num_matches == 0) {
		gtk_text_buffer_get_end_iter (buffer, &iter);
		theme_insert_text_with_emoticons (buffer, &iter, body);
	} else {
		gint   last = 0;
		gint   s = 0, e = 0;
		gchar *tmp;

		for (i = 0; i < num_matches; i++) {
			s = g_array_index (start, gint, i);
			e = g_array_index (end, gint, i);

			if (s > last) {
				tmp = gossip_substring (body, last, s);

				gtk_text_buffer_get_end_iter (buffer, &iter);
				theme_insert_text_with_emoticons (buffer,
								  &iter,
								  tmp);
				g_free (tmp);
			}

			tmp = gossip_substring (body, s, e);

			gtk_text_buffer_get_end_iter (buffer, &iter);
			gtk_text_buffer_insert_with_tags_by_name (buffer,
								  &iter,
								  tmp,
								  -1,
								  link_tag,
								  "link",
								  NULL);

			g_free (tmp);

			last = e;
		}

		if (e < strlen (body)) {
			tmp = gossip_substring (body, e, strlen (body));

			gtk_text_buffer_get_end_iter (buffer, &iter);
			theme_insert_text_with_emoticons (buffer,
							  &iter,
							  tmp);
			g_free (tmp);
		}
	}

	g_array_free (start, TRUE);
	g_array_free (end, TRUE);

	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert (buffer, &iter, "\n", 1);

	/* Apply the style to the inserted text. */
	gtk_text_buffer_get_iter_at_mark (buffer, &start_iter, mark);
	gtk_text_buffer_get_end_iter (buffer, &end_iter);

	gtk_text_buffer_apply_tag_by_name (buffer,
					   tag,
					   &start_iter,
					   &end_iter);

	gtk_text_buffer_delete_mark (buffer, mark);
}

void
gossip_theme_append_spacing (GossipTheme        *theme, 
			     GossipThemeContext *context,
			     GossipChatView     *view)
{
	GtkTextBuffer *buffer;
	const gchar   *tag;
	GtkTextIter    iter;

	g_return_if_fail (GOSSIP_IS_THEME (theme));
	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	if (gossip_chat_view_is_irc_style (view)) {
		tag = "irc-spacing";
	} else {
		tag = "fancy-spacing";
	}

	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (buffer,
						  &iter,
						  "\n",
						  -1,
						  "cut",
						  tag,
						  NULL);
}

void 
gossip_theme_append_event (GossipTheme        *theme,
			   GossipThemeContext *context,
			   GossipChatView     *view,
			   const gchar        *str)
{
	GOSSIP_THEME_GET_CLASS(theme)->append_event (theme, context, view, str);
}

void 
gossip_theme_append_timestamp (GossipTheme        *theme,
			       GossipThemeContext *context,
			       GossipChatView     *view,
			       GossipMessage      *message,
			       gboolean            show_date,
			       gboolean            show_time)
{
	GOSSIP_THEME_GET_CLASS(theme)->append_timestamp (theme, context, view,
							 message, show_date,
							 show_time);
}

typedef struct {
	BlockType last_block_type;
	time_t    last_timestamp;
} FancyContext;

void
gossip_theme_context_free (GossipTheme *theme, gpointer context)
{
	g_return_if_fail (GOSSIP_IS_THEME (theme));
#if 0
	if (!gossip_chat_view_is_irc_style (view)) {
		g_slice_free (FancyContext, context);
	}
#endif
}


