/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio AB
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
#include <glib.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-url.h>

#include "gossip-utils.h"
#include "gossip-time.h"
#include "gossip-chat-view.h"

/* Number of seconds between timestamps when using normal mode, 5 minutes */
#define TIMESTAMP_INTERVAL 300

struct _GossipChatViewPriv {
	GtkTextBuffer *buffer;

	GTimeVal       last_timestamp;
	GDate         *last_datestamp;
};

typedef enum {
	GOSSIP_SMILEY_NORMAL,      /*  :)   */
	GOSSIP_SMILEY_WINK,        /*  ;)   */
	GOSSIP_SMILEY_BIGEYE,      /*  =)   */
	GOSSIP_SMILEY_NOSE,        /*  :-)  */
	GOSSIP_SMILEY_CRY,         /*  :'(  */
	GOSSIP_SMILEY_SAD,         /*  :(   */
	GOSSIP_SMILEY_SCEPTICAL,   /*  :/   */
	GOSSIP_SMILEY_BIGSMILE,    /*  :D   */
	GOSSIP_SMILEY_INDIFFERENT, /*  :|   */
	GOSSIP_SMILEY_TOUNGE,      /*  :p   */
	GOSSIP_SMILEY_SHOCKED,     /*  :o   */
	GOSSIP_SMILEY_COOL,        /*  8)   */
	NUM_SMILEYS
} GossipSmiley;

typedef struct {
	GossipSmiley  smiley;
	gchar        *pattern;
	gint          index;
} GossipSmileyPattern;

static GossipSmileyPattern smileys[] = {
	{ GOSSIP_SMILEY_NORMAL,     ":)",  0 },
	{ GOSSIP_SMILEY_WINK,       ";)",  0 },
	{ GOSSIP_SMILEY_WINK,       ";-)", 0 },
	{ GOSSIP_SMILEY_BIGEYE,     "=)",  0 },
	{ GOSSIP_SMILEY_NOSE,       ":-)", 0 },
	{ GOSSIP_SMILEY_CRY,        ":'(", 0 },
	{ GOSSIP_SMILEY_SAD,        ":(",  0 },
	{ GOSSIP_SMILEY_SAD,        ":-(", 0 },
	{ GOSSIP_SMILEY_SCEPTICAL,  ":/",  0 },
	{ GOSSIP_SMILEY_SCEPTICAL,  ":\\",  0 },
	{ GOSSIP_SMILEY_BIGSMILE,   ":D",  0 },
	{ GOSSIP_SMILEY_BIGSMILE,   ":-D",  0 },
	{ GOSSIP_SMILEY_INDIFFERENT, ":|", 0 },
	{ GOSSIP_SMILEY_TOUNGE,      ":p", 0 },
	{ GOSSIP_SMILEY_TOUNGE,      ":-p", 0 },
	{ GOSSIP_SMILEY_TOUNGE,      ":P", 0 },
	{ GOSSIP_SMILEY_TOUNGE,      ":-P", 0 },
	{ GOSSIP_SMILEY_TOUNGE,      ";p", 0 },
	{ GOSSIP_SMILEY_TOUNGE,      ";-p", 0 },
	{ GOSSIP_SMILEY_TOUNGE,      ";P", 0 },
	{ GOSSIP_SMILEY_TOUNGE,      ";-P", 0 },
	{ GOSSIP_SMILEY_SHOCKED,     ":o", 0 },
	{ GOSSIP_SMILEY_SHOCKED,     ":O", 0 },
	{ GOSSIP_SMILEY_COOL,        "8)", 0 },
	{ GOSSIP_SMILEY_COOL,        "B)", 0 }
};

static gint num_smileys = G_N_ELEMENTS (smileys);

static void       gossip_chat_view_class_init (GossipChatViewClass *klass);
static void       gossip_chat_view_init       (GossipChatView      *view);
static void       chat_view_finalize        (GObject             *object);
static void       chat_view_size_allocate   (GtkWidget           *widget,
					     GtkAllocation       *alloc);
static void       chat_view_setup_tags      (GossipChatView      *view);
static void       chat_view_populate_popup  (GossipChatView      *view,
					     GtkMenu             *menu,
					     gpointer             user_data); 
static gboolean   chat_view_event_cb        (GossipChatView      *view,
					     GdkEventMotion      *event,
					     GtkTextTag          *tag);
static gboolean   chat_view_url_event_cb    (GtkTextTag          *tag,
					     GObject             *object,
					     GdkEvent            *event,
					     GtkTextIter         *iter,
					     GtkTextBuffer       *buffer);

static void       chat_view_open_address    (const gchar         *url);


static void       chat_view_open_address_cb (GtkMenuItem         *menuitem,
					     const gchar         *url);

static void       chat_view_copy_address_cb (GtkMenuItem         *menuitem,
					     const gchar         *url);
static void       chat_view_realize_cb      (GossipChatView      *widget,
					     gpointer             data);

static void       chat_view_clear_view_cb   (GtkMenuItem *menuitem,
					     GossipChatView *view);

static void       
chat_view_insert_text_with_emoticons        (GtkTextBuffer       *buf,
					     GtkTextIter         *iter, 
					     const gchar         *str);

static GdkPixbuf *chat_view_get_smiley       (GossipSmiley          smiley);

static void              
chat_view_maybe_append_timestamp             (GossipChatView *view,
					      gossip_time_t   timestamp);
static void
chat_view_maybe_append_datestamp             (GossipChatView *view);

static gboolean chat_view_is_scrolled_down   (GossipChatView *view);


extern GConfClient *gconf_client;
static GObjectClass *parent_class = NULL;

G_DEFINE_TYPE (GossipChatView, gossip_chat_view, GTK_TYPE_TEXT_VIEW);

static void
gossip_chat_view_class_init (GossipChatViewClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = chat_view_finalize;
	widget_class->size_allocate = chat_view_size_allocate;
}

static void
gossip_chat_view_init (GossipChatView *view)
{
	GossipChatViewPriv *priv;

	priv = g_new0 (GossipChatViewPriv, 1);
	view->priv = priv;
	
	priv->buffer = gtk_text_buffer_new (NULL);
	
	gtk_text_view_set_buffer (GTK_TEXT_VIEW (view), priv->buffer);

	g_object_set (view,
		      "justification", GTK_JUSTIFY_LEFT,
		      "wrap-mode", GTK_WRAP_WORD,
		      "editable", FALSE,
		      "cursor-visible", FALSE,
		      NULL);
	
	priv->last_timestamp.tv_sec = priv->last_timestamp.tv_usec = 0;
	priv->last_datestamp = g_date_new ();
	g_date_set_time (priv->last_datestamp, time (NULL));

	g_signal_connect (view,
			  "populate_popup",
			  G_CALLBACK (chat_view_populate_popup),
			  NULL);

	chat_view_setup_tags (view);
}

static void
chat_view_finalize (GObject *object)
{
	GossipChatView     *view = GOSSIP_CHAT_VIEW (object);
	GossipChatViewPriv *priv;

	priv = view->priv;

	g_object_unref (priv->buffer);
	g_date_free (priv->last_datestamp);
	 
	g_free (view->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
chat_view_size_allocate (GtkWidget     *widget,
			 GtkAllocation *alloc)
{
	gboolean down;
	
	down = chat_view_is_scrolled_down (GOSSIP_CHAT_VIEW (widget));
		
	GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, alloc);

	if (down) {
		gossip_chat_view_scroll_down (GOSSIP_CHAT_VIEW (widget));
	}
}

static void
chat_view_setup_tags (GossipChatView *view)
{
	GossipChatViewPriv *priv;
	GtkTextTag         *tag;
	
	priv = view->priv;
	
	gtk_text_buffer_create_tag (priv->buffer,
				    "nick-me",
				    "foreground", "sea green",
				    NULL);	
	
	gtk_text_buffer_create_tag (priv->buffer,
				    "nick-other",
				    "foreground", "steelblue4",
				    NULL);	
	
	gtk_text_buffer_create_tag (priv->buffer,
				    "nick-highlight",
				    "foreground", "indian red",
				    NULL);	

	gtk_text_buffer_create_tag (priv->buffer,
				    "notice",
				    "foreground", "steelblue4",
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "event-tag",
				    "foreground", "darkgrey",
				    "justification", GTK_JUSTIFY_CENTER,
				    NULL);

	tag = gtk_text_buffer_create_tag (priv->buffer,
					  "url",
					  "foreground", "steelblue",
					  "underline", PANGO_UNDERLINE_SINGLE,
					  NULL);	

	g_signal_connect (tag,
			  "event",
			  G_CALLBACK (chat_view_url_event_cb),
			  priv->buffer);


	g_signal_connect (view,
			  "event",
			  G_CALLBACK (chat_view_event_cb),
			  tag);
}



static void
chat_view_populate_popup (GossipChatView *view,
			  GtkMenu        *menu,
			  gpointer        user_data)
{
	GossipChatViewPriv *priv;
	GtkTextTagTable    *table;
	GtkTextTag         *tag;
	gint                x, y;
	GtkTextIter         iter, start, end;
	GtkWidget          *item;
	gchar              *str = NULL;

	priv = view->priv;
	
	table = gtk_text_buffer_get_tag_table (priv->buffer);
	tag = gtk_text_tag_table_lookup (table, "url");

	gtk_widget_get_pointer (GTK_WIDGET (view), &x, &y);
	
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (view), 
					       GTK_TEXT_WINDOW_WIDGET,
					       x, y,
					       &x, &y);
	
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &iter, x, y);

	start = end = iter;
	
	if (gtk_text_iter_backward_to_tag_toggle (&start, tag) &&
	    gtk_text_iter_forward_to_tag_toggle (&end, tag)) {
					
		str = gtk_text_buffer_get_text (priv->buffer, 
						&start, &end, FALSE);
	}

	item = gtk_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
	
	item = gtk_menu_item_new_with_mnemonic (_("C_lear"));
	g_signal_connect (item,
			  "activate",
			  G_CALLBACK (chat_view_clear_view_cb),
			  view);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	if (!str || strlen (str) == 0) {
		return;
	}

	/* Set data just to get the string freed when not needed. */
	g_object_set_data_full (G_OBJECT (menu), 
				"url", str, 
				(GDestroyNotify) g_free);

	item = gtk_menu_item_new_with_mnemonic (_("_Copy Link Address"));
	g_signal_connect (item,
			  "activate",
			  G_CALLBACK (chat_view_copy_address_cb),
			  str);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	item = gtk_menu_item_new_with_mnemonic (_("_Open Link"));
	g_signal_connect (item,
			  "activate",
			  G_CALLBACK (chat_view_open_address_cb),
			  str);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
}

static gboolean
chat_view_event_cb (GossipChatView *view,
		    GdkEventMotion *event,
		    GtkTextTag     *tag)
{
	static GdkCursor  *hand = NULL;
	static GdkCursor  *beam = NULL;
	GtkTextWindowType  type;
	GtkTextIter        iter;
	GdkWindow         *win;
	gint               x, y, buf_x, buf_y;

	type = gtk_text_view_get_window_type (GTK_TEXT_VIEW (view), 
					      event->window);
	
	if (type != GTK_TEXT_WINDOW_TEXT) {
		return FALSE;
	}

	/* Get where the pointer really is. */
	win = gtk_text_view_get_window (GTK_TEXT_VIEW (view), type);
	gdk_window_get_pointer (win, &x, &y, NULL);
	
	/* Get the iter where the cursor is at */
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (view), type, 
					       x, y, 
					       &buf_x, &buf_y);
	
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), 
					    &iter, 
					    buf_x, buf_y);

	if (!hand) {
		hand = gdk_cursor_new (GDK_HAND2);
		beam = gdk_cursor_new (GDK_XTERM);
	}
	
	if (gtk_text_iter_has_tag (&iter, tag)) {
		gdk_window_set_cursor (win, hand);
	} else {
		gdk_window_set_cursor (win, beam);
	}
	
	return FALSE;
}

static gboolean  
chat_view_url_event_cb (GtkTextTag    *tag,
			GObject       *object,
			GdkEvent      *event,
			GtkTextIter   *iter,
			GtkTextBuffer *buffer)
{
	GtkTextIter  start, end;
	gchar       *str;

	/* If the link is being selected, don't do anything. */
	gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
	if (gtk_text_iter_get_offset (&start) != gtk_text_iter_get_offset (&end)) {
		return FALSE;
	}

	if (event->type == GDK_BUTTON_RELEASE && event->button.button == 1) {
		start = end = *iter;
		
		if (gtk_text_iter_backward_to_tag_toggle (&start, tag) &&
		    gtk_text_iter_forward_to_tag_toggle (&end, tag)) {
			str = gtk_text_buffer_get_text (buffer,
							&start,
							&end,
							FALSE);

			chat_view_open_address (str);
			g_free (str);
		}
	}
	
	return FALSE;
}

static void
chat_view_open_address (const gchar *url)
{
	if (!url || strlen (url) == 0) {
		return;
	}

	/* gnome_url_show doesn't work when there's no protocol, so we might
	 * need to add one.
	 */
	if (strstr (url, "://") == NULL) {
		gchar *tmp;
		
		tmp = g_strconcat ("http://", url, NULL);
		gnome_url_show (tmp, NULL);
		g_free (tmp);
		return;
	}
	
	gnome_url_show (url, NULL);
}

static void
chat_view_open_address_cb (GtkMenuItem *menuitem, const gchar *url)
{
	chat_view_open_address (url);
}

static void
chat_view_copy_address_cb (GtkMenuItem *menuitem, const gchar *url)
{
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clipboard, url, -1);
}

static void
chat_view_realize_cb (GossipChatView *view, gpointer data)
{
	GdkWindow *win;

	win = gtk_text_view_get_window (GTK_TEXT_VIEW (view), 
					GTK_TEXT_WINDOW_TOP);
	gdk_window_set_background (win, &GTK_WIDGET(view)->style->base[GTK_STATE_NORMAL]);

	win = gtk_text_view_get_window (GTK_TEXT_VIEW (view), 
					GTK_TEXT_WINDOW_BOTTOM);
	gdk_window_set_background (win, &GTK_WIDGET(view)->style->base[GTK_STATE_NORMAL]);
}

static void
chat_view_clear_view_cb (GtkMenuItem *menuitem, GossipChatView *view)
{
	gossip_chat_view_clear (view);
}

static void
chat_view_insert_text_with_emoticons (GtkTextBuffer *buf,
				      GtkTextIter   *iter, 
				      const gchar   *str)
{
	const gchar *p;
	gunichar     c, prev_c;
	gint         i;
	gint         match;
	gint         submatch;
	gboolean     use_smileys;

	use_smileys = gconf_client_get_bool (
		gconf_client,
		"/apps/gossip/conversation/graphical_smileys",
		NULL);
	
	if (!use_smileys) {
		gtk_text_buffer_insert (buf, iter, str, -1);
		return;
	}
	
	while (*str) {
		for (i = 0; i < num_smileys; i++) {
			smileys[i].index = 0;
		}

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
				
				for (i = 0; i < num_smileys; i++) {
					if (smileys[i].pattern[smileys[i].index] == c) {
						submatch = i;
						
						smileys[i].index++;
						if (!smileys[i].pattern[smileys[i].index]) {
							match = i;
						}
					} else {
						smileys[i].index = 0;
					}
				}
			}
			
			prev_c = c;
			p = g_utf8_next_char (p);
		}
		
		if (match != -1) {
			GdkPixbuf   *pixbuf;
			gint         len;
			const gchar *start;

			start = p - strlen (smileys[match].pattern);

			if (start > str) {
				len = start - str;
				gtk_text_buffer_insert (buf, iter, str, len);
			}
			
			pixbuf = chat_view_get_smiley (smileys[match].smiley);
			gtk_text_buffer_insert_pixbuf (buf, iter, pixbuf);
			gtk_text_buffer_insert (buf, iter, " ", 1);
		} else {
			gtk_text_buffer_insert (buf, iter, str, -1);
			return;
		}

		str = g_utf8_find_next_char (p, NULL);
	}
}

static GdkPixbuf *
chat_view_get_smiley (GossipSmiley smiley)
{
	static GdkPixbuf *pixbufs[NUM_SMILEYS];
	static gboolean   inited = FALSE;

	
	if (!inited) {
		pixbufs[GOSSIP_SMILEY_NORMAL] =
			gdk_pixbuf_new_from_file (IMAGEDIR "/emoticon-face1.png", NULL);
		pixbufs[GOSSIP_SMILEY_WINK] =
			gdk_pixbuf_new_from_file (IMAGEDIR "/emoticon-face3.png", NULL);
		pixbufs[GOSSIP_SMILEY_BIGEYE] =
			gdk_pixbuf_new_from_file (IMAGEDIR "/emoticon-face2.png", NULL);
		pixbufs[GOSSIP_SMILEY_NOSE] =
			gdk_pixbuf_new_from_file (IMAGEDIR "/emoticon-face7.png", NULL);
		pixbufs[GOSSIP_SMILEY_CRY] =
			gdk_pixbuf_new_from_file (IMAGEDIR "/emoticon-face11.png", NULL);
		pixbufs[GOSSIP_SMILEY_SAD] =
			gdk_pixbuf_new_from_file (IMAGEDIR "/emoticon-face4.png", NULL);
		pixbufs[GOSSIP_SMILEY_SCEPTICAL] =
			gdk_pixbuf_new_from_file (IMAGEDIR "/emoticon-face9.png", NULL);
		pixbufs[GOSSIP_SMILEY_BIGSMILE] =
			gdk_pixbuf_new_from_file (IMAGEDIR "/emoticon-face6.png", NULL);
		pixbufs[GOSSIP_SMILEY_INDIFFERENT] =
			gdk_pixbuf_new_from_file (IMAGEDIR "/emoticon-face8.png", NULL);
		pixbufs[GOSSIP_SMILEY_TOUNGE] =
			gdk_pixbuf_new_from_file (IMAGEDIR "/emoticon-face10.png", NULL);
		pixbufs[GOSSIP_SMILEY_SHOCKED] =
			gdk_pixbuf_new_from_file (IMAGEDIR "/emoticon-face5.png", NULL);
		pixbufs[GOSSIP_SMILEY_COOL] =
			gdk_pixbuf_new_from_file (IMAGEDIR "/emoticon-face12.png", NULL);

		inited = TRUE;
	}

	return pixbufs[smiley];
}

static void
chat_view_maybe_append_timestamp (GossipChatView *view, gossip_time_t timestamp)
{
	GossipChatViewPriv *priv;
	GTimeVal            cur_time;
	
	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	priv = view->priv;
	
	g_get_current_time (&cur_time);

	if (priv->last_timestamp.tv_sec + TIMESTAMP_INTERVAL < cur_time.tv_sec){
		gchar  *stamp;
		time_t  t;
		
		priv->last_timestamp.tv_sec = cur_time.tv_sec;

		if (timestamp < 0) {
			t = gossip_time_get_current ();
		} else {
			t = timestamp;
		}

		stamp = gossip_time_to_timestamp (t);
		gossip_chat_view_append_event_msg (view, stamp, FALSE);
		g_free (stamp);
	} 
}

static void
chat_view_maybe_append_datestamp (GossipChatView *view)
{
	GossipChatViewPriv *priv;
	GDate              *cur_date;
	char                date_str[256];
	GTimeVal            cur_time;

	priv = view->priv;

	cur_date = g_date_new ();
	g_date_set_time (cur_date, time (NULL));

	if (g_date_compare (cur_date, priv->last_datestamp) <= 0) {
		return;
	}

	g_date_strftime (date_str, 256, _("%A %d %B %Y"), cur_date);
	
	gossip_chat_view_append_event_msg (view, date_str, TRUE);

	g_get_current_time (&cur_time);
	priv->last_timestamp.tv_sec = cur_time.tv_sec;
	
	g_date_free (priv->last_datestamp);
	priv->last_datestamp = cur_date;
}

GossipChatView * 
gossip_chat_view_new (void)
{
	return g_object_new (GOSSIP_TYPE_CHAT_VIEW, NULL);
}

static gboolean
chat_view_is_scrolled_down (GossipChatView *view)
{
	GtkWidget *sw;

	sw = gtk_widget_get_parent (GTK_WIDGET (view));
	if (GTK_IS_SCROLLED_WINDOW (sw)) {
		GtkAdjustment *vadj;
		
		vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (sw));

		if (vadj->value + vadj->page_size / 2 < vadj->upper - vadj->page_size) {
			return FALSE;
		}
	}

	return TRUE;
}

void
gossip_chat_view_scroll_down (GossipChatView *view)
{
	GtkTextBuffer *buffer;
	GtkTextIter    iter;
	GtkTextMark   *mark;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	gtk_text_buffer_get_end_iter (buffer, &iter);
	mark = gtk_text_buffer_create_mark (buffer,
					    NULL,
					    &iter,
					    FALSE);
	
	gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
				      mark,
				      0.0,
				      FALSE,
				      0,
				      0);
}

static gboolean
chat_view_check_nick_highlight (const gchar *msg, const gchar *to)
{
	gboolean  ret_val;
	gchar    *cf_msg, *cf_to;
	gchar    *ch;

	ret_val = FALSE;
	
	cf_msg = g_utf8_casefold (msg, -1);
	cf_to = g_utf8_casefold (to, -1);

	ch = strstr (cf_msg, cf_to);

	if (ch == NULL) {
		goto finished;
	}
		
	if (ch != cf_msg) {
		/* Not first in the message */
		if ((*(ch - 1) != ' ') &&
		    (*(ch - 1) != ',') &&
		    (*(ch - 1) != '.')) {
			goto finished;
		}
	}

	ch = ch + g_utf8_strlen (cf_to, -1);
	if (ch >= cf_msg + g_utf8_strlen (cf_msg, -1)) {
		ret_val = TRUE;
		goto finished;
	}

	if ((*ch == ' ') ||
	    (*ch == ',') ||
	    (*ch == '.') ||
	    (*ch == ':')) {
		ret_val = TRUE;
		goto finished;
	}

finished:
	g_free (cf_msg);
	g_free (cf_to);

	return ret_val;
}

void
gossip_chat_view_append_chat_message (GossipChatView *view,
				      gossip_time_t   timestamp,
				      const gchar    *to,
				      const gchar    *from,
				      const gchar    *msg)
{
	GtkTextBuffer *buffer;
	GtkTextIter    iter;
	gchar         *nick_tag;
	gint           num_matches, i;
	GArray        *start, *end;
	gboolean       bottom;

	if (msg == NULL || msg[0] == 0) {
		return;
	}

	chat_view_maybe_append_datestamp (view);
	chat_view_maybe_append_timestamp (view, timestamp);

	bottom = chat_view_is_scrolled_down (view);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	if (from) {
		if (strncmp (msg, "/me ", 4) != 0) {
			/* Regular message. */
			
			gtk_text_buffer_get_end_iter (buffer, &iter);

			/* FIXME: This only works if nick == name... */
			if (to && strcmp (from, to) == 0) {
				nick_tag = "nick-me";
			}
			else if (to && chat_view_check_nick_highlight (msg, to)) {
				nick_tag = "nick-highlight";
			} else {
				nick_tag = "nick-other";
			}

			gtk_text_buffer_insert_with_tags_by_name (buffer,
								  &iter,
								  from,
								  -1,
								  nick_tag,
								  NULL);
					
			gtk_text_buffer_get_end_iter (buffer, &iter);
			gtk_text_buffer_insert_with_tags_by_name (buffer,
								  &iter,
								  ": ",
								  2,
								  nick_tag,
								  NULL);
		} else {
			/* /me style message. */
			gtk_text_buffer_get_end_iter (buffer, &iter);
			gtk_text_buffer_insert_with_tags_by_name (buffer,
								  &iter,
								  " * ",
								  3,
								  "notice",
								  NULL);
			
			gtk_text_buffer_get_end_iter (buffer, &iter);
			
			gtk_text_buffer_get_end_iter (buffer, &iter);
			gtk_text_buffer_insert_with_tags_by_name (buffer,
								  &iter,
								  from,
								  -1,
								  "notice",
								  NULL);

			/* Remove the /me. */
			msg += 3;
		}
	} else {
		gtk_text_buffer_get_end_iter (buffer, &iter);
		gtk_text_buffer_insert_with_tags_by_name (buffer,
							  &iter,
							  " - ",
							  3,
							  "notice",
							  NULL);
	}

	start = g_array_new (FALSE, FALSE, sizeof (gint));
	end = g_array_new (FALSE, FALSE, sizeof (gint));
	
	num_matches = gossip_utils_url_regex_match (msg, start, end);

	if (num_matches == 0) {
		gtk_text_buffer_get_end_iter (buffer, &iter);
		chat_view_insert_text_with_emoticons (buffer, &iter, msg);
	} else {
		gint   last = 0;
		gint   s = 0, e = 0;
		gchar *tmp;

		for (i = 0; i < num_matches; i++) {

			s = g_array_index (start, gint, i);
			e = g_array_index (end, gint, i);

			if (s > last) {
				tmp = gossip_utils_substring (msg, last, s);

				gtk_text_buffer_get_end_iter (buffer, &iter);
				chat_view_insert_text_with_emoticons (buffer,
								      &iter,
								      tmp);
				g_free (tmp);
			}

			tmp = gossip_utils_substring (msg, s, e);

			gtk_text_buffer_get_end_iter (buffer, &iter);
			gtk_text_buffer_insert_with_tags_by_name (buffer,
								  &iter,
								  tmp,
								  -1,
								  "url",
								  NULL);

			g_free (tmp);

			last = e;
		}

		if (e < strlen (msg)) {
			tmp = gossip_utils_substring (msg, e, strlen (msg));

			gtk_text_buffer_get_end_iter (buffer, &iter);
			chat_view_insert_text_with_emoticons (buffer,
							      &iter, tmp);
			g_free (tmp);
		}
	}

	g_array_free (start, TRUE);
	g_array_free (end, TRUE);
	
	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert (buffer,
				&iter,
				"\n",
				1);

	/* Scroll to the end of the newly inserted text, if we were at the
	 * bottom before.
	 */
	if (bottom) {
		gossip_chat_view_scroll_down (view);
	}
}

void
gossip_chat_view_append_event_msg (GossipChatView *view, 
				   const gchar    *str, 
				   gboolean        timestamp)
{
	GtkTextBuffer *buffer;
	GtkTextIter    iter;
	gchar         *stamp;
	gchar         *msg;
	gboolean       bottom;

	bottom = chat_view_is_scrolled_down (view);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	gtk_text_buffer_get_end_iter (buffer, &iter);

	if (!gtk_text_iter_starts_line (&iter)) {
		gtk_text_buffer_insert (buffer,	&iter, "\n", 1);
	}

	if (timestamp) {
		stamp = gossip_time_to_timestamp (-1);
		msg = g_strdup_printf (" %s - %s", str, stamp);
		g_free (stamp);
	} else {
		msg = (gchar *) str;
	}
	
	
	gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
						  msg, -1, "event-tag",
						  NULL);
	if (msg != str) {
		g_free (msg);
	}
		
	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert (buffer,
				&iter,
				"\n",
				1);

	if (bottom) {
		gossip_chat_view_scroll_down (view);
	}
}

void
gossip_chat_view_set_margin (GossipChatView *view, gint margin)
{
	gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), margin);
	gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view), margin);
	
	gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (view), 
					      GTK_TEXT_WINDOW_TOP, margin);
	gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (view), 
					      GTK_TEXT_WINDOW_BOTTOM, margin);

	g_signal_connect (view,
			  "realize",
			  G_CALLBACK (chat_view_realize_cb),
			  NULL);
}

void
gossip_chat_view_clear (GossipChatView *view)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));
	
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	gtk_text_buffer_set_text (buffer, "", -1);
}

gboolean
gossip_chat_view_get_selection_bounds (GossipChatView *view,
				       GtkTextIter    *start,
				       GtkTextIter    *end)
{
	GtkTextBuffer *buffer;
	
	g_return_val_if_fail (GOSSIP_IS_CHAT_VIEW (view), FALSE);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	return gtk_text_buffer_get_selection_bounds (buffer, start, end);
}

void
gossip_chat_view_copy_clipboard (GossipChatView *view)
{
	GtkTextBuffer *buffer;
	GtkClipboard  *clipboard;
	
	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	gtk_text_buffer_copy_clipboard (buffer, clipboard);
}

