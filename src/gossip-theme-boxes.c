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
#include <gtk/gtkentry.h>

#include <libgossip/gossip-debug.h>

#include "gossip-app.h"
#include "gossip-theme-utils.h"
#include "gossip-theme-boxes.h"

#define DEBUG_DOMAIN "FancyTheme"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_THEME_BOXES, GossipThemeBoxesPriv))

typedef struct _GossipThemeBoxesPriv GossipThemeBoxesPriv;

struct _GossipThemeBoxesPriv {
	gchar *header_foreground;
	gchar *header_background;
	gchar *header_other_foreground;
	gchar *header_other_background;
	gchar *text_foreground;
	gchar *text_background;
	gchar *text_other_foreground;
	gchar *text_other_background;
	gchar *action_foreground;
	gchar *action_other_foreground;
	gchar *highlight_foreground;
	gchar *highlight_other_foreground;
	gchar *line_background;
	gchar *line_other_background;
	gchar *time_foreground;
	gchar *event_foreground;
	gchar *invite_foreground;
	gchar *link_foreground;
};

static void     theme_boxes_finalize          (GObject            *object);
static void     theme_boxes_get_property      (GObject            *object,
					       guint               param_id,
					       GValue             *value,
					       GParamSpec         *pspec);
static void     theme_boxes_set_property      (GObject            *object,
					       guint               param_id,
					       const GValue       *value,
					       GParamSpec         *pspec);
static void     theme_boxes_define_theme_tags (GossipTheme        *theme,
					       GossipChatView     *view);
static GossipThemeContext *
theme_boxes_setup_with_view                   (GossipTheme        *theme,
					       GossipChatView     *view);
static void     theme_boxes_detach_from_view  (GossipTheme        *theme,
					       GossipThemeContext *context,
					       GossipChatView     *view);
static void     theme_boxes_view_cleared      (GossipTheme        *theme,
					       GossipThemeContext *context,
					       GossipChatView     *view);

static void     theme_boxes_append_message    (GossipTheme        *theme,
					       GossipThemeContext *context,
					       GossipChatView     *view,
					       GossipMessage      *message,
					       gboolean            from_self);
static void     theme_boxes_append_action     (GossipTheme        *theme,
					       GossipThemeContext *context,
					       GossipChatView     *view,
					       GossipMessage      *message,
					       gboolean            from_self);
static void     theme_boxes_append_event      (GossipTheme        *theme,
					       GossipThemeContext *context,
					       GossipChatView     *view,
					       const gchar        *str);
static void     theme_boxes_append_timestamp  (GossipTheme        *theme,
					       GossipThemeContext *context,
					       GossipChatView     *view,
					       GossipMessage      *message,
					       gboolean            show_date,
					       gboolean            show_time);
static void     theme_boxes_append_spacing    (GossipTheme        *theme,
					       GossipThemeContext *context,
					       GossipChatView     *view);

enum {
	PROP_0,
	PROP_HEADER_FOREGROUND,
	PROP_HEADER_BACKGROUND,
	PROP_HEADER_OTHER_FOREGROUND,
	PROP_HEADER_OTHER_BACKGROUND,
	PROP_TEXT_FOREGROUND,
	PROP_TEXT_BACKGROUND,
	PROP_TEXT_OTHER_FOREGROUND,
	PROP_TEXT_OTHER_BACKGROUND,
	PROP_ACTION_FOREGROUND,
	PROP_ACTION_OTHER_FOREGROUND,
	PROP_HIGHLIGHT_FOREGROUND,
	PROP_HIGHLIGHT_OTHER_FOREGROUND,
	PROP_LINE_BACKGROUND,
	PROP_LINE_OTHER_BACKGROUND,
	PROP_TIME_FOREGROUND,
	PROP_EVENT_FOREGROUND,
	PROP_INVITE_FOREGROUND,
	PROP_LINK_FOREGROUND
};

enum {
	PROP_FLOP,
	PROP_MY_PROP
};

G_DEFINE_TYPE (GossipThemeBoxes, gossip_theme_boxes, GOSSIP_TYPE_THEME);

static void
gossip_theme_boxes_class_init (GossipThemeBoxesClass *class)
{
	GObjectClass     *object_class;
	GossipThemeClass *theme_class;

	object_class = G_OBJECT_CLASS (class);
	theme_class  = GOSSIP_THEME_CLASS (class);

	object_class->finalize       = theme_boxes_finalize;
	object_class->get_property   = theme_boxes_get_property;
	object_class->set_property   = theme_boxes_set_property;

	theme_class->setup_with_view  = theme_boxes_setup_with_view;
	theme_class->detach_from_view = theme_boxes_detach_from_view;
	theme_class->view_cleared     = theme_boxes_view_cleared;
	theme_class->append_message   = theme_boxes_append_message;
	theme_class->append_action    = theme_boxes_append_action;
	theme_class->append_event     = theme_boxes_append_event;
	theme_class->append_timestamp = theme_boxes_append_timestamp;
	theme_class->append_spacing   = theme_boxes_append_spacing;

	g_object_class_install_property (object_class,
					 PROP_HEADER_FOREGROUND,
					 g_param_spec_string ("header-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_HEADER_BACKGROUND,
					 g_param_spec_string ("header-background",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_HEADER_OTHER_FOREGROUND,
					 g_param_spec_string ("header-other-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_HEADER_OTHER_BACKGROUND,
					 g_param_spec_string ("header-other-background",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_TEXT_FOREGROUND,
					 g_param_spec_string ("text-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_TEXT_BACKGROUND,
					 g_param_spec_string ("text-background",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_TEXT_OTHER_FOREGROUND,
					 g_param_spec_string ("text-other-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_TEXT_OTHER_BACKGROUND,
					 g_param_spec_string ("text-other-background",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_ACTION_FOREGROUND,
					 g_param_spec_string ("action-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_ACTION_OTHER_FOREGROUND,
					 g_param_spec_string ("action-other-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_HIGHLIGHT_FOREGROUND,
					 g_param_spec_string ("highlight-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_HIGHLIGHT_OTHER_FOREGROUND,
					 g_param_spec_string ("highlight-other-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_LINE_BACKGROUND,
					 g_param_spec_string ("line-background",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_LINE_OTHER_BACKGROUND,
					 g_param_spec_string ("line-other-background",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_TIME_FOREGROUND,
					 g_param_spec_string ("time-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_EVENT_FOREGROUND,
					 g_param_spec_string ("event-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_INVITE_FOREGROUND,
					 g_param_spec_string ("invite-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_LINK_FOREGROUND,
					 g_param_spec_string ("link-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GossipThemeBoxesPriv));
}

static void
gossip_theme_boxes_init (GossipThemeBoxes *theme)
{
	GossipThemeBoxesPriv *priv;

	priv = GET_PRIV (theme);
}

static void
theme_boxes_finalize (GObject *object)
{
	GossipThemeBoxesPriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->header_foreground);
	g_free (priv->header_background);
	g_free (priv->header_other_foreground);
	g_free (priv->header_other_background);
	g_free (priv->text_foreground);
	g_free (priv->text_background);
	g_free (priv->text_other_foreground);
	g_free (priv->text_other_background);
	g_free (priv->action_foreground);
	g_free (priv->action_other_foreground);
	g_free (priv->highlight_foreground);
	g_free (priv->highlight_other_foreground);
	g_free (priv->line_background);
	g_free (priv->line_other_background);
	g_free (priv->time_foreground);
	g_free (priv->event_foreground);
	g_free (priv->invite_foreground);
	g_free (priv->link_foreground);
	
	(G_OBJECT_CLASS (gossip_theme_boxes_parent_class)->finalize) (object);
}

static void
theme_boxes_get_property (GObject    *object,
			  guint       param_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
	GossipThemeBoxesPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_HEADER_FOREGROUND:
		g_value_set_string (value, priv->header_foreground);
		break;
	case PROP_HEADER_BACKGROUND:
		g_value_set_string (value, priv->header_background);
		break;
	case PROP_HEADER_OTHER_FOREGROUND:
		g_value_set_string (value, priv->header_other_foreground);
		break;
	case PROP_HEADER_OTHER_BACKGROUND:
		g_value_set_string (value, priv->header_other_background);
		break;
	case PROP_TEXT_FOREGROUND:
		g_value_set_string (value, priv->text_foreground);
		break;
	case PROP_TEXT_BACKGROUND:
		g_value_set_string (value, priv->text_background);
		break;
	case PROP_TEXT_OTHER_FOREGROUND:
		g_value_set_string (value, priv->text_other_foreground);
		break;
	case PROP_TEXT_OTHER_BACKGROUND:
		g_value_set_string (value, priv->text_other_background);
		break;
	case PROP_ACTION_FOREGROUND:
		g_value_set_string (value, priv->action_foreground);
		break;
	case PROP_ACTION_OTHER_FOREGROUND:
		g_value_set_string (value, priv->action_other_foreground);
		break;
	case PROP_HIGHLIGHT_FOREGROUND:
		g_value_set_string (value, priv->highlight_foreground);
		break;
	case PROP_HIGHLIGHT_OTHER_FOREGROUND:
		g_value_set_string (value, priv->highlight_other_foreground);
		break;
	case PROP_LINE_BACKGROUND:
		g_value_set_string (value, priv->line_background);
		break;
	case PROP_LINE_OTHER_BACKGROUND:
		g_value_set_string (value, priv->line_other_background);
		break;
	case PROP_TIME_FOREGROUND:
		g_value_set_string (value, priv->time_foreground);
		break;
	case PROP_EVENT_FOREGROUND:
		g_value_set_string (value, priv->event_foreground);
		break;
	case PROP_INVITE_FOREGROUND:
		g_value_set_string (value, priv->invite_foreground);
		break;
	case PROP_LINK_FOREGROUND:
		g_value_set_string (value, priv->link_foreground);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}
static void
theme_boxes_set_property (GObject      *object,
		    guint         param_id,
		    const GValue *value,
		    GParamSpec   *pspec)
{
	GossipThemeBoxesPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_HEADER_FOREGROUND:
		g_free (priv->header_foreground);
		priv->header_foreground = g_value_dup_string (value);
		break;
	case PROP_HEADER_BACKGROUND:
		g_free (priv->header_background);
		priv->header_background = g_value_dup_string (value);
		break;
	case PROP_HEADER_OTHER_FOREGROUND:
		g_free (priv->header_other_foreground);
		priv->header_other_foreground = g_value_dup_string (value);
		break;
	case PROP_HEADER_OTHER_BACKGROUND:
		g_free (priv->header_other_background);
		priv->header_other_background = g_value_dup_string (value);
		break;
	case PROP_TEXT_FOREGROUND:
		g_free (priv->text_foreground);
		priv->text_foreground = g_value_dup_string (value);
		break;
	case PROP_TEXT_BACKGROUND:
		g_free (priv->text_background);
		priv->text_background = g_value_dup_string (value);
		break;
	case PROP_TEXT_OTHER_FOREGROUND:
		g_free (priv->text_other_foreground);
		priv->text_other_foreground = g_value_dup_string (value);
		break;
	case PROP_TEXT_OTHER_BACKGROUND:
		g_free (priv->text_other_background);
		priv->text_other_background = g_value_dup_string (value);
		break;
	case PROP_ACTION_FOREGROUND:
		g_free (priv->action_foreground);
		priv->action_foreground = g_value_dup_string (value);
		break;
	case PROP_ACTION_OTHER_FOREGROUND:
		g_free (priv->action_other_foreground);
		priv->action_other_foreground = g_value_dup_string (value);
		break;
	case PROP_HIGHLIGHT_FOREGROUND:
		g_free (priv->highlight_foreground);
		priv->highlight_foreground = g_value_dup_string (value);
		break;
	case PROP_HIGHLIGHT_OTHER_FOREGROUND:
		g_free (priv->highlight_other_foreground);
		priv->highlight_other_foreground = g_value_dup_string (value);
		break;
	case PROP_LINE_BACKGROUND:
		g_free (priv->line_background);
		priv->line_background = g_value_dup_string (value);
		break;
	case PROP_LINE_OTHER_BACKGROUND:
		g_free (priv->line_other_background);
		priv->line_other_background = g_value_dup_string (value);
		break;
	case PROP_TIME_FOREGROUND:
		g_free (priv->time_foreground);
		priv->time_foreground = g_value_dup_string (value);
		break;
	case PROP_EVENT_FOREGROUND:
		g_free (priv->event_foreground);
		priv->event_foreground = g_value_dup_string (value);
		break;
	case PROP_INVITE_FOREGROUND:
		g_free (priv->invite_foreground);
		priv->invite_foreground = g_value_dup_string (value);
		break;
	case PROP_LINK_FOREGROUND:
		g_free (priv->link_foreground);
		priv->link_foreground = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
theme_boxes_set_body_background (GossipTheme *theme, 
				 GtkTextTag  *tag,
				 gboolean     from_self)
{
	GossipThemeBoxesPriv *priv;

	priv = GET_PRIV (theme);

	if (from_self && priv->text_background) {
		g_object_set (tag,
			      "paragraph-background", priv->text_background,
			      NULL);
	} 
	else if (priv->text_other_background) {
		g_object_set (tag,
			     "paragraph-background", priv->text_other_background,
			     NULL);
	}
}

static void
theme_boxes_define_theme_tags (GossipTheme *theme, GossipChatView *view)
{
	GossipThemeBoxesPriv *priv;
	GtkTextBuffer   *buffer;
	GtkTextTagTable *table;
	GtkTextTag      *tag;

	priv = GET_PRIV (theme);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	table = gtk_text_buffer_get_tag_table (buffer);

	tag = gossip_theme_utils_init_tag_by_name (table, "fancy-spacing");
	g_object_set (tag,
		      "size", 3000,
		      NULL);
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, 
						   "fancy-header-self");
	g_object_set (tag,
		      "weight", PANGO_WEIGHT_BOLD,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      "left-margin", 4,
		      "right-margin", 4,
		      NULL);
	if (priv->header_foreground) {
		g_object_set (tag,
			      "foreground", priv->header_foreground,
			      "paragraph-background", priv->header_background,
			      NULL);
	}
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "fancy-header-self-avatar");
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "fancy-avatar-self");
	g_object_set (tag,
		      "left-margin", 4,
		      "right-margin", 4,
		      NULL);
	if (priv->header_background) {
		g_object_set (tag,
			      "paragraph-background", priv->header_background,
			      NULL);
	}
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "fancy-line-top-self");
	g_object_set (tag,
		      "size", 1,
		      "left-margin", 4,
		      "right-margin", 4,
		      NULL);
	if (priv->line_background) {
		g_object_set (tag,
			      "paragraph-background", priv->line_background,
			      NULL);
	}

	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table,
						   "fancy-line-bottom-self");
	g_object_set (tag,
		      "size", 1,
		      "left-margin", 4,
		      "right-margin", 4,
		      NULL);
	if (priv->line_background) {
		g_object_set (tag,
			      "paragraph-background", priv->line_background,
			      NULL);
	}
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "fancy-body-self");
	g_object_set (tag,
		      "pixels-above-lines", 4,
		      "left-margin", 4,
		      "right-margin", 4,
		      NULL);
	theme_boxes_set_body_background (theme, tag, TRUE);

	if (priv->text_foreground) {
		g_object_set (tag,
			      "foreground", priv->text_foreground,
			      NULL);
	}
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "fancy-action-self");
	g_object_set (tag,
		      "style", PANGO_STYLE_ITALIC,
		      "pixels-above-lines", 4,
		      "left-margin", 4,
		      "right-margin", 4,
		      NULL);

	theme_boxes_set_body_background (theme, tag, TRUE);

	if (priv->action_foreground) {
		g_object_set (tag,
			      "foreground", priv->action_foreground,
			      NULL);
	} 

	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "fancy-highlight-self");
	g_object_set (tag,
		      "weight", PANGO_WEIGHT_BOLD,
		      "pixels-above-lines", 4,
		      NULL);
	theme_boxes_set_body_background (theme, tag, TRUE);

	if (priv->highlight_foreground) {
		g_object_set (tag,
			      "foreground", priv->highlight_foreground,
			      NULL);
	}
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "fancy-header-other");
	g_object_set (tag,
		      "weight", PANGO_WEIGHT_BOLD,
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      "left-margin", 4,
		      "right-margin", 4,
		      NULL);
	if (priv->header_other_foreground) {
		g_object_set (tag,
			      "foreground", priv->header_other_foreground,
			      "paragraph-background", priv->header_other_background,
			      NULL);
	}
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "fancy-header-other-avatar");
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "fancy-avatar-other");
	g_object_set (tag,
		      "left-margin", 4,
		      "right-margin", 4,
		      NULL);
	if (priv->header_other_background) {
		g_object_set (tag,
			      "paragraph-background", priv->header_other_background,
			      NULL);
	}
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "fancy-line-top-other");
	g_object_set (tag,
		      "size", 1,
		      "left-margin", 4,
		      "right-margin", 4,
		      NULL);
	if (priv->line_other_background) {
		g_object_set (tag,
			      "paragraph-background", priv->line_other_background,
			      NULL);
	}
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "fancy-line-bottom-other");
	g_object_set (tag,
		      "size", 1,
		      "left-margin", 4,
		      "right-margin", 4,
		      NULL);
	if (priv->line_other_background) {
		g_object_set (tag,
			      "paragraph-background", priv->line_other_background,
			      NULL);
	}
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "fancy-body-other");
	g_object_set (tag,
		      "pixels-above-lines", 4,
		      "left-margin", 4,
		      "right-margin", 4,
		      NULL);
	theme_boxes_set_body_background (theme, tag, FALSE);

	if (priv->text_other_foreground) {
		g_object_set (tag,
			      "foreground", priv->text_other_foreground,
			      NULL);
	}
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table,
						   "fancy-action-other");
	g_object_set (tag,
		      "style", PANGO_STYLE_ITALIC,
		      "pixels-above-lines", 4,
		      "left-margin", 4,
		      "right-margin", 4,
		      NULL);
	theme_boxes_set_body_background (theme, tag, FALSE);
	if (priv->action_other_foreground) {
		g_object_set (tag,
			      "foreground", priv->action_other_foreground,
			      NULL);
	}
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table,
						   "fancy-highlight-other");
	g_object_set (tag,
		      "weight", PANGO_WEIGHT_BOLD,
		      "pixels-above-lines", 4,
		      NULL);
	theme_boxes_set_body_background (theme, tag, FALSE);

	if (priv->highlight_other_foreground) {
		g_object_set (tag,
			      "foreground", priv->highlight_other_foreground,
			      NULL);
	}
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "fancy-time");
	g_object_set (tag,
		      "justification", GTK_JUSTIFY_CENTER,
		      NULL);
	if (priv->time_foreground) {
		g_object_set (tag,
			      "foreground", priv->time_foreground,
			      NULL);
	}
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "fancy-event");
	g_object_set (tag,
		      "justification", GTK_JUSTIFY_LEFT,
		      NULL);
	if (priv->event_foreground) {
		g_object_set (tag,
			      "foreground", priv->event_foreground,
			      NULL);
	}
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "invite");
	if (priv->invite_foreground) {
		g_object_set (tag,
			      "foreground", priv->invite_foreground,
			      NULL);
	}
	gossip_theme_utils_add_tag (table, tag);

	tag = gossip_theme_utils_init_tag_by_name (table, "fancy-link");
	g_object_set (tag,
		      "underline", PANGO_UNDERLINE_SINGLE,
		      NULL);
	if (priv->link_foreground) {
		g_object_set (tag,
			      "foreground", priv->link_foreground,
			      NULL);
	} 
	gossip_theme_utils_add_tag (table, tag);
}

static void
theme_boxes_fixup_tag_table (GossipTheme *theme, GossipChatView *view)
{
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	/* "Fancy" style tags. */
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-header-self");
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-header-self-avatar");
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-avatar-self");
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-line-top-self");
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-line-bottom-self");
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-body-self");
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-action-self");
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-highlight-self");

	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-header-other");
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-header-other-avatar");
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-avatar-other");
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-line-top-other");
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-line-bottom-other");
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-body-other");
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-action-other");
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-highlight-other");

	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-spacing");
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-time");
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-event");
	gossip_theme_utils_ensure_tag_by_name (buffer, "fancy-link");
}

typedef struct {
	BlockType last_block_type;
	time_t    last_timestamp;
} FancyContext;

static GossipThemeContext *
theme_boxes_setup_with_view (GossipTheme *theme, GossipChatView *view)
{
	GossipThemeBoxesPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_THEME_BOXES (theme), NULL);

	priv = GET_PRIV (theme);

	theme_boxes_fixup_tag_table (theme, view);

	theme_boxes_define_theme_tags (theme, view);
	
	gossip_chat_view_set_margin (view, 0);

	return NULL;
}

static void
theme_boxes_detach_from_view (GossipTheme        *theme,
			      GossipThemeContext *context,
			      GossipChatView     *view)
{
	/* FIXME: Free the context */
}

static void 
theme_boxes_view_cleared (GossipTheme        *theme,
			  GossipThemeContext *context,
			  GossipChatView     *view)
{
	/* FIXME: clear the context data */
}

static void
theme_boxes_maybe_append_header (GossipTheme        *theme,
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
		gtk_text_buffer_insert_with_tags_by_name (buffer,
							  &iter,
							  " ",
							  -1,
							  tag,
							  avatar_tag,
							  NULL);
	} else {
		if (from_self) {
			avatar_tag = "fancy-header-self";
		} else {
			avatar_tag = "fancy-header-other";
		}
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
theme_boxes_append_message (GossipTheme        *theme,
			    GossipThemeContext *context,
			    GossipChatView     *view,
			    GossipMessage      *message,
			    gboolean            from_self)
{
	const gchar *tag;

	gossip_theme_maybe_append_date_and_time (theme, context, view, message);

	theme_boxes_maybe_append_header (theme, context, view, message,
					 from_self);

	if (from_self) {
		tag = "fancy-body-self";
	} else {
		tag = "fancy-body-other";

		/* FIXME: Might want to support nick highlighting here... */
	}

	gossip_theme_append_text (theme, context, view, 
				  gossip_message_get_body (message),
				  tag, "fancy-link");

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
theme_boxes_append_action (GossipTheme        *theme,
			   GossipThemeContext *context,
			   GossipChatView     *view,
			   GossipMessage      *message,
			   gboolean            from_self)
{
	GossipContact *contact;
	const gchar   *name;
	gchar         *tmp;
	const gchar   *tag;
	const gchar   *line_tag;

	gossip_debug (DEBUG_DOMAIN, "Add fancy action");

	gossip_theme_maybe_append_date_and_time (theme, context, view, message);

	contact = gossip_message_get_sender (message);
	
	theme_boxes_maybe_append_header (theme, context, view, message,
					 from_self);

	contact = gossip_message_get_sender (message);
	name = gossip_contact_get_name (contact);

	if (from_self) {
		tag = "fancy-action-self";
		line_tag = "fancy-line-self";
	} else {
		tag = "fancy-action-other";
		line_tag = "fancy-line-other";
	}

	tmp = gossip_message_get_action_string (message);
	gossip_theme_append_text (theme, context, view, tmp, tag, "fancy-link");
	g_free (tmp);

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
theme_boxes_append_event (GossipTheme        *theme,
			  GossipThemeContext *context,
			  GossipChatView     *view,
			  const gchar        *str)
{
	GtkTextBuffer *buffer;
	GtkTextIter    iter;
	gchar         *msg;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	gossip_theme_maybe_append_date_and_time (theme, context, view, NULL);

	gtk_text_buffer_get_end_iter (buffer, &iter);

	msg = g_strdup_printf (" - %s\n", str);

	gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
						  msg, -1,
						  "fancy-event",
						  NULL);
	g_free (msg);

	gossip_chat_view_set_last_block_type (view, BLOCK_TYPE_EVENT);
}

static void
theme_boxes_append_timestamp (GossipTheme        *theme,
			      GossipThemeContext *context,
			      GossipChatView     *view,
			      GossipMessage      *message,
			      gboolean            show_date,
			      gboolean            show_time)
{
	GtkTextBuffer *buffer;
	time_t         timestamp;
	GDate         *date;
	GtkTextIter    iter;
	GString       *str;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	date = gossip_message_get_date_and_time (message, &timestamp);

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
							  "fancy-time",
							  NULL);

		gossip_chat_view_set_last_block_type (view, BLOCK_TYPE_TIME);
		gossip_chat_view_set_last_timestamp (view, timestamp);
	}

	g_string_free (str, TRUE);
	
}

static void
theme_boxes_append_spacing (GossipTheme        *theme,
			    GossipThemeContext *context,
			    GossipChatView     *view)
{
	GtkTextBuffer *buffer;
	GtkTextIter    iter;

	g_return_if_fail (GOSSIP_IS_THEME (theme));
	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (buffer,
						  &iter,
						  "\n",
						  -1,
						  "cut",
						  "fancy-spacing",
						  NULL);
}

static void
theme_boxes_setup_clean (GossipTheme *theme)
{
	g_object_set (theme,
		      "header-foreground", "black",
		      "header-background", "#efefdf",
		      "header_other_foreground", "black",
		      "header_other_background", "#efefdf",
		      "action_foreground", "brown4",
		      "action_other_foreground", "brown4",
		      "line_background", "#e3e3d3",
		      "line_other_background", "#e3e3d3",
		      "time_foreground", "darkgrey",
		      "event_foreground", "darkgrey",
		      "invite_foreground", "sienna",
		      "link_foreground","#49789e",
		      NULL);
}

static void
theme_boxes_gdk_color_to_hex (GdkColor *gdk_color, gchar *str_color)
{
	g_snprintf (str_color, 10, 
		    "#%02x%02x%02x", 
		    gdk_color->red >> 8, 
		    gdk_color->green >> 8, 
		    gdk_color->blue >> 8);
}

static void
theme_boxes_setup_themed (GossipTheme *theme)
{
	GossipThemeBoxesPriv *priv;
	GtkStyle             *style;
	gchar                 color[10];

	priv = GET_PRIV (theme);

	style = gtk_widget_get_style (gossip_app_get_window ());

	theme_boxes_gdk_color_to_hex (&style->base[GTK_STATE_SELECTED], color);

	g_object_set (theme,
		      "action-foreground", color,
		      "action-other-foreground", color,
		      "link-foreground", color,
		      NULL);

	theme_boxes_gdk_color_to_hex (&style->bg[GTK_STATE_SELECTED], color);

	g_object_set (theme,
		      "header-background", color,
		      "header-other-background", color,
		      NULL);

	theme_boxes_gdk_color_to_hex (&style->dark[GTK_STATE_SELECTED], color);

	g_object_set (theme,
		      "line-background", color,
		      "line-other-background", color,
		      NULL);

	theme_boxes_gdk_color_to_hex (&style->fg[GTK_STATE_SELECTED], color);

	g_object_set (theme,
		      "header-foreground", color,
		      "header-other-foreground", color,
		      NULL);
}

static void
theme_boxes_theme_changed_cb (GtkWidget *widget,
			      GtkStyle  *previous_style,
			      gpointer   user_data)
{
	theme_boxes_setup_themed (GOSSIP_THEME (user_data));

	g_signal_emit_by_name (G_OBJECT (user_data), "updated");
}

static void
theme_boxes_setup_blue (GossipTheme *theme)
{
	g_object_set (theme,
		      "header_foreground", "black",
		      "header_background", "#b9b9b9",
		      "header_other_foreground", "black",
		      "header_other_background", "#88a2b4",
		      "text_foreground", "black",
		      "text_background", "#dcdcdc",
		      "text_other_foreground", "black",
		      "text_other_background", "#adbdc8",
		      "highlight_foreground", "black",
		      "highlight_other_foreground", "black",
		      "action_foreground", "brown4",
		      "action_other_foreground", "brown4",
		      "line_background", "#aeaeae",
		      "line_other_background", "#7f96a4",
		      "time_foreground", "darkgrey",
		      "event_foreground", "#7f96a4",
		      "invite_foreground", "sienna",
		      "link_foreground", "#49789e",
		      NULL);
}

GossipTheme *
gossip_theme_boxes_new (const gchar *name)
{
	GossipTheme          *theme;
	GossipThemeBoxesPriv *priv;

	theme = g_object_new (GOSSIP_TYPE_THEME_BOXES, NULL);
	priv  = GET_PRIV (theme);

	if (strcmp (name, "clean") == 0) {
		theme_boxes_setup_clean (theme);
	}
	else if (strcmp (name, "simple") == 0) {
		g_signal_connect (gossip_app_get_window (),
				  "style-set",
				  G_CALLBACK (theme_boxes_theme_changed_cb),
				  theme);

		theme_boxes_setup_themed (theme);
	}
	else if (strcmp (name, "blue") == 0) {
		theme_boxes_setup_blue (theme);
	}

	return theme;
}


