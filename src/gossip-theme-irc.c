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

#include <glib/gi18n.h>
#include <libgossip/gossip-debug.h>

#include "gossip-chat.h"
#include "gossip-theme-utils.h"
#include "gossip-theme-irc.h"

#define DEBUG_DOMAIN "Theme"

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
theme_irc_setup_with_view                      (GossipTheme         *theme,
						GossipChatView      *view);
static void         theme_irc_append_message   (GossipTheme        *theme,
						GossipThemeContext *context,
						GossipChatView     *view,
						GossipMessage      *message,
						gboolean            from_self);
static void         theme_irc_append_action    (GossipTheme        *theme,
						GossipThemeContext *context,
						GossipChatView     *view,
						GossipMessage      *message,
						gboolean            from_self);
static void         theme_irc_append_event     (GossipTheme        *theme,
						GossipThemeContext *context,
						GossipChatView     *view,
						const gchar        *str);
static void         theme_irc_append_timestamp (GossipTheme        *theme,
						GossipThemeContext *context,
						GossipChatView     *view,
						GossipMessage      *message,
						gboolean            show_date,
						gboolean            show_time);


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

	theme_class->setup_with_view  = theme_irc_setup_with_view;
	theme_class->append_message   = theme_irc_append_message;
	theme_class->append_action    = theme_irc_append_action;
	theme_class->append_event     = theme_irc_append_event;
	theme_class->append_timestamp = theme_irc_append_timestamp;

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

static void
theme_irc_append_message (GossipTheme        *theme,
			  GossipThemeContext *context,
			  GossipChatView     *view,
			  GossipMessage      *message,
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

	gossip_theme_maybe_append_date_and_time (theme, context, view, message);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	contact = gossip_message_get_sender (message);
	name = gossip_contact_get_name (contact);

	if (from_self) {
		nick_tag = "irc-nick-self";
		body_tag = "irc-body-self";
	} else {
		if (gossip_chat_should_highlight_nick (message, 
						       gossip_message_get_recipient (message))) {
			nick_tag = "irc-nick-highlight";
		} else {
			nick_tag = "irc-nick-other";
		}

		body_tag = "irc-body-other";
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
				  gossip_message_get_body (message),
				  body_tag, "irc-link");
}

static void 
theme_irc_append_action (GossipTheme        *theme,
			 GossipThemeContext *context,
			 GossipChatView     *view,
			 GossipMessage      *message,
			 gboolean            from_self)
{
	const gchar   *name;
	gchar         *tmp;
	const gchar   *tag;
	GossipContact *contact;

	gossip_theme_maybe_append_date_and_time (theme, context, view, message);

	contact = gossip_message_get_sender (message);
	name = gossip_contact_get_name (contact);

	gossip_debug (DEBUG_DOMAIN, "Add IRC action");

	/* Skip the "/me ". */
	if (from_self) {
		tag = "irc-action-self";
	} else {
		tag = "irc-action-other";
	}

	tmp = gossip_message_get_action_string (message);
	gossip_theme_append_text (theme, context, view, tmp, tag, "irc-link");
	g_free (tmp);
}

static void
theme_irc_append_event (GossipTheme        *theme,
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
						  "irc-event",
						  NULL);
	g_free (msg);
}

static void
theme_irc_append_timestamp (GossipTheme        *theme,
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
							  "irc-time",
							  NULL);

		gossip_chat_view_set_last_timestamp (view, timestamp);
	}

	g_string_free (str, TRUE);
}

