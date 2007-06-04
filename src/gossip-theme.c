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
#include "gossip-theme-utils.h"
#include "gossip-theme.h"

#define DEBUG_DOMAIN "Theme"

/* Number of seconds between timestamps when using normal mode, 5 minutes. */
#define TIMESTAMP_INTERVAL 300

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_THEME, GossipThemePriv))

typedef struct _GossipThemePriv GossipThemePriv;

struct _GossipThemePriv {
	gint my_prop;
};

static void         theme_finalize            (GObject            *object);

G_DEFINE_TYPE (GossipTheme, gossip_theme, G_TYPE_OBJECT);

static void
gossip_theme_class_init (GossipThemeClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize     = theme_finalize;

	class->setup_with_view  = NULL;
	class->view_cleared     = NULL;
	class->append_message   = NULL;
	class->append_action    = NULL;
	class->append_event     = NULL;
	class->append_timestamp = NULL;
	class->append_spacing   = NULL;

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

void
gossip_theme_maybe_append_date_and_time (GossipTheme        *theme,
					 GossipThemeContext *context,
					 GossipChatView     *view,
					 GossipMessage      *message)
{
	time_t    timestamp;
	GDate    *date, *last_date;
	gboolean  append_date, append_time;

	date = gossip_message_get_date_and_time (message, &timestamp);

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

GossipTheme *
gossip_theme_new (void)
{
	GossipTheme     *theme;

	theme = g_object_new (GOSSIP_TYPE_THEME, NULL);

	return theme;
}

GossipThemeContext *
gossip_theme_setup_with_view (GossipTheme    *theme,
			      GossipChatView *view)
{
	if (!GOSSIP_THEME_GET_CLASS(theme)->setup_with_view) {
		g_error ("Theme must override setup_with_view");
	}

	return GOSSIP_THEME_GET_CLASS(theme)->setup_with_view (theme, view);
}

void
gossip_theme_detach_from_view (GossipTheme        *theme,
			       GossipThemeContext *context,
			       GossipChatView     *view)
{
	if (!GOSSIP_THEME_GET_CLASS(theme)->detach_from_view) {
		g_error ("Theme must override detach_from_view");
	}

	return GOSSIP_THEME_GET_CLASS(theme)->detach_from_view (theme, context,
								view);
}

void
gossip_theme_view_cleared (GossipTheme        *theme,
			   GossipThemeContext *context,
			   GossipChatView     *view)
{
	if (!GOSSIP_THEME_GET_CLASS(theme)->view_cleared) {
		return;
	}

	GOSSIP_THEME_GET_CLASS(theme)->view_cleared (theme, context, view);
}

void
gossip_theme_append_message (GossipTheme        *theme,
			     GossipThemeContext *context,
			     GossipChatView     *view,
			     GossipMessage      *message,
			     gboolean            from_self)
{
	if (!GOSSIP_THEME_GET_CLASS(theme)->append_message) {
		g_warning ("Theme should override append_message");
		return;
	}

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
	if (!GOSSIP_THEME_GET_CLASS(theme)->append_action) {
		g_warning ("Theme should override append_message");
		return;
	}

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
			  const gchar        *tag,
			  const gchar        *link_tag)
{
	GossipThemePriv *priv;
	GtkTextBuffer   *buffer;
	GtkTextIter      start_iter, end_iter;
	GtkTextMark     *mark;
	GtkTextIter      iter;
	gint             num_matches, i;
	GArray          *start, *end;

	priv = GET_PRIV (theme);
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

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
			if (!link_tag) {
				gtk_text_buffer_insert (buffer, &iter,
							tmp, -1);
			} {
				gtk_text_buffer_insert_with_tags_by_name (buffer,
									  &iter,
									  tmp,
									  -1,
									  link_tag,
									  "link",
									  NULL);
			}

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
gossip_theme_append_event (GossipTheme        *theme,
			   GossipThemeContext *context,
			   GossipChatView     *view,
			   const gchar        *str)
{
	GOSSIP_THEME_GET_CLASS(theme)->append_event (theme, context, view, str);
}

void
gossip_theme_append_spacing (GossipTheme        *theme, 
			     GossipThemeContext *context,
			     GossipChatView     *view)
{
	if (!GOSSIP_THEME_GET_CLASS(theme)->append_action) {
		return;
	}

	GOSSIP_THEME_GET_CLASS(theme)->append_spacing (theme, context, view);
}


void 
gossip_theme_append_timestamp (GossipTheme        *theme,
			       GossipThemeContext *context,
			       GossipChatView     *view,
			       GossipMessage      *message,
			       gboolean            show_date,
			       gboolean            show_time)
{
	if (!GOSSIP_THEME_GET_CLASS(theme)->append_action) {
		return;
	}

	GOSSIP_THEME_GET_CLASS(theme)->append_timestamp (theme, context, view,
							 message, show_date,
							 show_time);
}


