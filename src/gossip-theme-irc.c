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

#include "config.h"

#include "gossip-theme-utils.h"
#include "gossip-theme-irc.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_THEME_IRC, GossipThemeIrcPriv))

typedef struct _GossipThemeIrcPriv GossipThemeIrcPriv;

struct _GossipThemeIrcPriv {
	gint my_prop;
};

static void         theme_irc_finalize      (GObject             *object);
static void         theme_irc_get_property  (GObject             *object,
					     guint                param_id,
					     GValue              *value,
					     GParamSpec          *pspec);
static void         theme_irc_set_property  (GObject             *object,
					     guint                param_id,
					     const GValue        *value,
					     GParamSpec          *pspec);
static GossipThemeContext *
theme_irc_setup_with_view                   (GossipTheme         *theme,
					     GossipChatView      *view);


enum {
	PROP_0,
	PROP_MY_PROP
};

G_DEFINE_TYPE (GossipThemeIrc, gossip_theme_irc, GOSSIP_TYPE_THEME);

static void
gossip_theme_irc_class_init (GossipThemeIrcClass *class)
{
	GObjectClass *object_class;
	GossipThemeClass *theme_class;

	object_class = G_OBJECT_CLASS (class);
	theme_class  = GOSSIP_THEME_CLASS (class);

	object_class->finalize     = theme_irc_finalize;
	object_class->get_property = theme_irc_get_property;
	object_class->set_property = theme_irc_set_property;

	theme_class->setup_with_view = theme_irc_setup_with_view;

	g_object_class_install_property (object_class,
					 PROP_MY_PROP,
					 g_param_spec_int ("my-prop",
							   "",
							   "",
							   0, 1,
							   1,
							   G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GossipThemeIrcPriv));
}

static void
gossip_theme_irc_init (GossipThemeIrc *presence)
{
	GossipThemeIrcPriv *priv;

	priv = GET_PRIV (presence);
}

static void
theme_irc_finalize (GObject *object)
{
	GossipThemeIrcPriv *priv;

	priv = GET_PRIV (object);

	(G_OBJECT_CLASS (gossip_theme_irc_parent_class)->finalize) (object);
}

static void
theme_irc_get_property (GObject    *object,
		    guint       param_id,
		    GValue     *value,
		    GParamSpec *pspec)
{
	GossipThemeIrcPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_MY_PROP:
		g_value_set_int (value, priv->my_prop);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}
static void
theme_irc_set_property (GObject      *object,
		    guint         param_id,
		    const GValue *value,
		    GParamSpec   *pspec)
{
	GossipThemeIrcPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_MY_PROP:
		priv->my_prop = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
theme_irc_fixup_tag_table (GossipTheme *theme, GossipChatView *view)
{
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	/* IRC style tags. */
	gossip_theme_utils_ensure_tag_by_name (buffer, "irc-nick-self");
	gossip_theme_utils_ensure_tag_by_name (buffer, "irc-body-self");
	gossip_theme_utils_ensure_tag_by_name (buffer, "irc-action-self");

	gossip_theme_utils_ensure_tag_by_name (buffer, "irc-nick-other");
	gossip_theme_utils_ensure_tag_by_name (buffer, "irc-body-other");
	gossip_theme_utils_ensure_tag_by_name (buffer, "irc-action-other");

	gossip_theme_utils_ensure_tag_by_name (buffer, "irc-nick-highlight");
	gossip_theme_utils_ensure_tag_by_name (buffer, "irc-spacing");
	gossip_theme_utils_ensure_tag_by_name (buffer, "irc-time");
	gossip_theme_utils_ensure_tag_by_name (buffer, "irc-event");
	gossip_theme_utils_ensure_tag_by_name (buffer, "irc-link");
}

static void
theme_irc_apply_theme_classic (GossipTheme *theme, GossipChatView *view)
{
	GossipThemeIrcPriv *priv;
	GtkTextBuffer      *buffer;
	GtkTextTagTable    *table;
	GtkTextTag         *tag;

	priv = GET_PRIV (theme);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	table = gtk_text_buffer_get_tag_table (buffer);

	tag = gossip_theme_utils_init_tag_by_name (table, "irc-spacing");
	g_object_set (tag,
		      "size", 2000,
		      NULL);
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "irc-nick-self");
	g_object_set (tag,
		      "foreground", "sea green",
		      NULL);
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "irc-body-self");
	g_object_set (tag,
		      /* To get the default theme color: */
		      "foreground-set", FALSE,
		      NULL);
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "irc-action-self");
	g_object_set (tag,
		      "foreground", "brown4",
		      "style", PANGO_STYLE_ITALIC,
		      NULL);
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "irc-nick-highlight");
	g_object_set (tag,
		      "foreground", "indian red",
		      "weight", PANGO_WEIGHT_BOLD,
		      NULL);
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "irc-nick-other");
	g_object_set (tag,
		      "foreground", "skyblue4",
		      NULL);
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "irc-body-other");
	g_object_set (tag,
		      /* To get the default theme color: */
		      "foreground-set", FALSE,
		      NULL);
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "irc-action-other");
	g_object_set (tag,
		      "foreground", "brown4",
		      "style", PANGO_STYLE_ITALIC,
		      NULL);
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "irc-time");
	g_object_set (tag,
		      "foreground", "darkgrey",
		      "justification", GTK_JUSTIFY_CENTER,
		      NULL);
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "irc-event");
	g_object_set (tag,
		      "foreground", "PeachPuff4",
		      "justification", GTK_JUSTIFY_LEFT,
		      NULL);
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "invite");
	g_object_set (tag,
		      "foreground", "sienna",
		      NULL);
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "irc-link");
	g_object_set (tag,
		      "foreground", "steelblue",
		      "underline", PANGO_UNDERLINE_SINGLE,
		      NULL);
	gossip_theme_utils_add_tag (table, tag);
}


static GossipThemeContext *
theme_irc_setup_with_view (GossipTheme *theme, GossipChatView *view)
{
	theme_irc_fixup_tag_table (theme, view);
	theme_irc_apply_theme_classic (theme, view);
	gossip_chat_view_set_margin (view, 3);
	gossip_chat_view_set_is_irc_style (view, TRUE);

	return NULL;
}

