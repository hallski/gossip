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

#include <libgossip/gossip-conf.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-utils.h>

#include "gossip-preferences.h"
#include "gossip-smiley.h"
#include "gossip-theme.h"

#define DEBUG_DOMAIN "Theme"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_THEME, GossipThemePriv))

typedef struct _GossipThemePriv GossipThemePriv;

struct _GossipThemePriv {
	gint my_prop;
	gboolean is_irc_style;
};

static void         theme_finalize           (GObject             *object);
static void         theme_get_property       (GObject             *object,
					      guint                param_id,
					      GValue              *value,
					      GParamSpec          *pspec);
static void         theme_set_property       (GObject             *object,
					      guint                param_id,
					      const GValue        *value,
					      GParamSpec          *pspec);

enum {
	PROP_0,
	PROP_MY_PROP
};

G_DEFINE_TYPE (GossipTheme, gossip_theme, G_TYPE_OBJECT);

static void
gossip_theme_class_init (GossipThemeClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize     = theme_finalize;
	object_class->get_property = theme_get_property;
	object_class->set_property = theme_set_property;

	g_object_class_install_property (object_class,
					 PROP_MY_PROP,
					 g_param_spec_int ("my-prop",
							   "",
							   "",
							   0, 1,
							   1,
							   G_PARAM_READWRITE));

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

static void
theme_get_property (GObject    *object,
		    guint       param_id,
		    GValue     *value,
		    GParamSpec *pspec)
{
	GossipThemePriv *priv;

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
theme_set_property (GObject      *object,
		    guint         param_id,
		    const GValue *value,
		    GParamSpec   *pspec)
{
	GossipThemePriv *priv;

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
theme_append_irc_action (GossipTheme    *theme,
			 GossipChatView *view,
			 GossipMessage  *msg,
			 GossipContact  *my_contact,
			 gboolean        from_self)
{
	const gchar *name;
	gchar       *tmp;
	const gchar *tag;

	gossip_debug (DEBUG_DOMAIN, "Add IRC action");

	/* Skip the "/me ". */
	if (from_self) {
		name = gossip_contact_get_name (my_contact);

		tag = "irc-action-self";
	} else {
		GossipContact *contact;

		contact = gossip_message_get_sender (msg);
		name = gossip_contact_get_name (contact);

		tag = "irc-action-other";
	}

	if (gossip_chat_view_get_last_block_type (view) != BLOCK_TYPE_SELF &&
	    gossip_chat_view_get_last_block_type (view) != BLOCK_TYPE_OTHER) {
		gossip_theme_append_spacing (theme, view);
	}

	tmp = gossip_message_get_action_string (msg);
	gossip_theme_append_text (theme, view, tmp, tag);
	g_free (tmp);
}

static void
theme_append_fancy_action (GossipTheme    *theme,
			   GossipChatView *view,
			   GossipMessage  *msg,
			   GossipContact  *my_contact,
			   gboolean        from_self)
{
	GossipContact *contact;
	const gchar   *name;
	gchar         *tmp;
	const gchar   *tag;
	const gchar   *line_tag;

	gossip_debug (DEBUG_DOMAIN, "Add fancy action");

	contact = gossip_message_get_sender (msg);

	if (from_self) {
		name = gossip_contact_get_name (my_contact);

		tag = "fancy-action-self";
		line_tag = "fancy-line-self";
	} else {
		GossipContact *contact;

		contact = gossip_message_get_sender (msg);
		name = gossip_contact_get_name (contact);

		tag = "fancy-action-other";
		line_tag = "fancy-line-other";
	}

	tmp = gossip_message_get_action_string (msg);
	gossip_theme_append_text (theme, view, tmp, tag);
	g_free (tmp);
}



void
gossip_theme_append_action (GossipTheme    *theme,
			    GossipChatView *view,
			    GossipMessage  *msg,
			    GossipContact  *contact,
			    gboolean        from_self)
{
	if (gossip_theme_is_irc_style (theme)) {
		theme_append_irc_action (theme, view, msg, contact, from_self);
	} else {
		theme_append_fancy_action (theme, view, msg, 
					   contact, from_self);
	}
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
gossip_theme_append_text (GossipTheme    *theme,
			  GossipChatView *view,
			  const gchar    *body,
			  const gchar    *tag)
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

	if (gossip_theme_is_irc_style (theme)) {
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
gossip_theme_append_spacing (GossipTheme *theme, GossipChatView *view)
{
	GtkTextBuffer *buffer;
	const gchar   *tag;
	GtkTextIter    iter;

	g_return_if_fail (GOSSIP_IS_THEME (theme));
	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	if (gossip_theme_is_irc_style (theme)) {
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

gboolean
gossip_theme_is_irc_style (GossipTheme *theme)
{
	GossipThemePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_THEME (theme), TRUE);

	priv = GET_PRIV (theme);

	return priv->is_irc_style;
}

void
gossip_theme_set_is_irc_style (GossipTheme *theme, gboolean is_irc_style)
{
	GossipThemePriv *priv;

	g_return_if_fail (GOSSIP_IS_THEME (theme));

	priv = GET_PRIV (theme);

	priv->is_irc_style = is_irc_style;
}


