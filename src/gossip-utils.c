/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2003 CodeFactory AB
 * Copyright (C) 2002-2003 Richard Hult <rhult@codefactory.se>
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
#include <time.h>
#include <regex.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <libgnome/gnome-url.h>
#include <loudmouth/loudmouth.h>
#include "gossip-utils.h"


#define DINGUS "(((mailto|news|telnet|nttp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?(/[-A-Za-z0-9_\\$\\.\\+\\!\\*\\(\\),;:@&=\\?/~\\#\\%]*[^]'\\.}>\\) ,\\\"])?"

static regex_t dingus;


static gboolean utils_url_event_cb          (GtkTextTag     *tag,
					     GObject        *object,
					     GdkEvent       *event,
					     GtkTextIter    *iter,
					     GtkTextBuffer  *buffer);
static gboolean utils_text_view_event_cb    (GtkTextView    *view,
					     GdkEventMotion *event,
					     GtkTextTag     *tag);
static void     utils_insert_with_emoticons (GtkTextBuffer *buf,
					     GtkTextIter   *iter, 
					     const gchar   *text);

void
gossip_status_menu_setup (GtkWidget     *option_menu,
			  GCallback      func,
			  gpointer       user_data,
			  gconstpointer  str1, ...)
{
	GtkWidget     *menu;
	GtkWidget     *item;
	gint           i;
	va_list        args;
	gconstpointer  str;
	gconstpointer  imagefile;
	gint           type;
	GtkWidget     *image;

	GtkWidget     *hbox;
	GtkWidget     *label;
	
       	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu));
	if (menu) {
		gtk_widget_destroy (menu);
	}
	
	menu = gtk_menu_new ();

	va_start (args, str1);
	for (str = str1, i = 0; str != NULL; str = va_arg (args, gpointer), i++) {
		imagefile = va_arg (args, gpointer);

		if (((gchar *)str)[0] == 0) {
			/* Separator item. */
			item = gtk_separator_menu_item_new ();
		} else {
			/* Pack our own image menu item since we won't get the
			 * image in the active option menu item
			 * otherwise. (Using image menu items only shows the
			 * icon when the menu is popped up, not as the active
			 * menu item in the option menu otherwise.)
			 */
			hbox = gtk_hbox_new (FALSE, 4);
			gtk_widget_show (hbox);
						
			image = gtk_image_new_from_file (imagefile);
			gtk_widget_show (image);

			gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, TRUE, 0);
			
			label = gtk_label_new (str);
			gtk_widget_show (label);
			gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
			
			item = gtk_menu_item_new ();
			
			gtk_container_add (GTK_CONTAINER (item), hbox);
		}
		
		gtk_widget_show (item);
		
		gtk_menu_append (GTK_MENU (menu), item);

		type = va_arg (args, gint);
		
		g_object_set_data (G_OBJECT (item),
				   "data",
				   GINT_TO_POINTER (type));
		if (func) {
			g_signal_connect (item,
					  "activate",
					  func,
					  user_data);
		}
	}
	va_end (args);

	gtk_widget_show (menu);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
}

void
gossip_status_menu_set_status (GtkWidget    *option_menu,
			       GossipStatus  status)
{
	GtkWidget *menu;
	GtkWidget *item;
	GList     *children, *l;
	gint       i;
	gpointer   ptr = GINT_TO_POINTER (status);
	
       	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu));

	children = GTK_MENU_SHELL (menu)->children;
	for (i = 0, l = children; l; i++, l = l->next) {
		item = l->data;

		if (ptr == g_object_get_data (G_OBJECT (item), "data")) {
			gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), i);
			break;
		}
	}
}

GossipStatus
gossip_status_menu_get_status (GtkWidget *option_menu)
{
	GtkWidget    *menu;
	GtkWidget    *item;
	GossipStatus  status;
	
       	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu));

	item = gtk_menu_get_active (GTK_MENU (menu));

	status = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "data"));

	return status;
}

void
gossip_option_menu_setup (GtkWidget     *option_menu,
			  GCallback      func,
			  gpointer       user_data,
			  gconstpointer  str1, ...)
{
	GtkWidget     *menu;
	GtkWidget     *item;
	gint           i;
	va_list        args;
	gconstpointer  str;
	gint           type;
	
       	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu));
	if (menu) {
		gtk_widget_destroy (menu);
	}
	
	menu = gtk_menu_new ();

	va_start (args, str1);
	for (str = str1, i = 0; str != NULL; str = va_arg (args, gpointer), i++) {
		item = gtk_menu_item_new_with_label (str);
		gtk_widget_show (item);

		gtk_menu_append (GTK_MENU (menu), item);

		type = va_arg (args, gint);
		
		g_object_set_data (G_OBJECT (item),
				   "data",
				   GINT_TO_POINTER (type));
		if (func) {
			g_signal_connect (item,
					  "activate",
					  func,
					  user_data);
		}
	}
	va_end (args);

	gtk_widget_show (menu);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
}

void
gossip_option_menu_set_history (GtkOptionMenu *option_menu,
				gpointer       user_data)
{
	GtkWidget *menu;
	GtkWidget *item;
	GList     *children, *l;
	gint       i;
	
       	menu = gtk_option_menu_get_menu (option_menu);

	children = GTK_MENU_SHELL (menu)->children;
	for (i = 0, l = children; l; i++, l = l->next) {
		item = l->data;

		if (user_data == g_object_get_data (G_OBJECT (item), "data")) {
			gtk_option_menu_set_history (option_menu, i);
			break;
		}
	}
}

gpointer
gossip_option_menu_get_history (GtkOptionMenu *option_menu)
{
	GtkWidget    *menu;
	GtkWidget    *item;
	
       	menu = gtk_option_menu_get_menu (option_menu);

	item = gtk_menu_get_active (GTK_MENU (menu));

	return g_object_get_data (G_OBJECT (item), "data");
}

static GladeXML *
get_glade_file (const gchar *filename,
		const gchar *root,
		const gchar *domain,
		const gchar *first_required_widget, va_list args)
{
	GladeXML   *gui;
	const char *name;
	GtkWidget **widget_ptr;
	GList      *ptrs, *l;

	if (!(gui = glade_xml_new (filename, root, domain))) {
		g_warning ("Couldn't find necessary glade file '%s'", filename);
		return NULL;
	}

	ptrs = NULL;
	for (name = first_required_widget; name; name = va_arg (args, char *)) {
		widget_ptr = va_arg (args, void *);
		
		*widget_ptr = glade_xml_get_widget (gui, name);

		if (!*widget_ptr) {
			g_warning ("Glade file '%s' is missing widget '%s', aborting",
				   filename, name);
			
			for (l = ptrs; l; l = l->next) {
				*((gpointer *)l->data) = NULL;
			}
			g_list_free (ptrs);
			g_object_unref (gui);
			return NULL;
		} else {
			ptrs = g_list_prepend (ptrs, widget_ptr);
		}
	}

	return gui;
}

/* Stolen and modified from eel: */
void
gossip_glade_get_file_simple (const gchar *filename,
			      const gchar *root,
			      const gchar *domain,
			      const gchar *first_required_widget, ...)
{
	va_list   args;
	GladeXML *gui;

	va_start (args, first_required_widget);

	gui = get_glade_file (filename,
			      root,
			      domain,
			      first_required_widget,
			      args);
	
	va_end (args);

	if (!gui) {
		return;
	}

	g_object_unref (gui);
}

GladeXML *
gossip_glade_get_file (const gchar *filename,
		       const gchar *root,
		       const gchar *domain,
		       const gchar *first_required_widget, ...)
{
	va_list   args;
	GladeXML *gui;

	va_start (args, first_required_widget);

	gui = get_glade_file (filename,
			      root,
			      domain,
			      first_required_widget,
			      args);
	
	va_end (args);

	if (!gui) {
		return NULL;
	}

	return gui;
}

void
gossip_glade_connect (GladeXML *gui,
		      gpointer  user_data,
		      gchar     *first_widget, ...)
{
	va_list      args;
	const gchar *name;
	const gchar *signal;
	GtkWidget   *widget;
	gpointer    *callback;

	va_start (args, first_widget);
	
	for (name = first_widget; name; name = va_arg (args, char *)) {
		signal = va_arg (args, void *);
		callback = va_arg (args, void *);

		widget = glade_xml_get_widget (gui, name);
		if (!widget) {
			g_warning ("Glade file is missing widget '%s', aborting",
				   name);
			
			g_object_unref (gui);
			return;
		} else {
			g_signal_connect (widget,
					  signal,
					  G_CALLBACK (callback),
					  user_data);
		}
	}

	va_end (args);
}
#if 0
gchar *
gossip_jid_get_name (const gchar *str)
{
	const gchar *p;
	
	for (p = str; *p; p++) {
		if (*p == '@') {
			return g_strndup (str, p - str);
		}
	}

	return g_strdup (str);
}

gchar *
gossip_jid_get_server (const gchar *str)
{
	gchar *p;

	p = strstr (str, "@");
	if (!p) {
		return NULL;
	}

	p++;
	
	return gossip_jid_strip_resource (p);
}

gchar *
gossip_jid_strip_resource (const gchar *str)
{
	gchar *tmp;
	
	tmp = strchr (str, '/');
	if (tmp) {
		return g_strndup (str, tmp - str);
	}

	return g_strdup (str);
}

gchar *
gossip_jid_get_resource (const gchar *str)
{
	gchar *tmp;
	
	tmp = strchr (str, '/');
	if (tmp) {
		return g_strdup (tmp + 1);
	}

	return NULL;
}

gchar *
gossip_jid_create (const gchar *name, const gchar *server, const gchar *resource)
{
	gchar *tmp;

	tmp = g_strconcat (name, "@", server, resource ? "/" : NULL, resource);

	return tmp;
}
#endif

static gchar *
get_stamp (const gchar *timestamp)
{
	time_t     t;
	struct tm *tm;
	gchar      buf[128];

	if (!timestamp) {
		t  = time (NULL);
		tm = localtime (&t);
	} else {
		tm = lm_utils_get_localtime (timestamp);
	}
	
	buf[0] = 0;
	strftime (buf, sizeof (buf), "%H:%M ", tm);

	return g_strdup (buf);
}

static gint
url_regex_match (const gchar  *msg,
		 GArray       *start,
		 GArray       *end)
{
	static gboolean inited = FALSE;
	regmatch_t      matches[1];
	gint            ret = 0;
	gint            num_matches = 0;
	gint            offset = 0;

	if (!inited) {
		memset (&dingus, 0, sizeof (regex_t));
		regcomp (&dingus, DINGUS, REG_EXTENDED);
		inited = TRUE;
	}

	while (!ret) {
		ret = regexec (&dingus, msg + offset, 1, matches, 0);

		if (ret == 0) {
			gint s, e;
			
			num_matches++;

			s = matches[0].rm_so + offset;
			e = matches[0].rm_eo + offset;
			
			g_array_append_val (start, s);
			g_array_append_val (end, e);

			offset += e;
		}
	}
		
	return num_matches;
}

static gchar *
utils_get_substring (const gchar *str, gint start, gint end)
{
	return g_strndup (str + start, end - start);
}

void
gossip_text_view_append_chat_message (GtkTextView  *text_view,
				      const gchar  *timestamp,
				      const gchar  *to,
				      const gchar  *from,
				      const gchar  *msg)
{
	GtkTextBuffer *buffer;
	GtkTextIter    iter;
	gchar         *nick_tag;
	GtkTextMark   *mark;
	gchar         *stamp;
	gint           num_matches, i;
	GArray        *start, *end;
	gboolean       bottom = TRUE;
	GtkWidget     *parent;

	if (msg == NULL || msg[0] == 0) {
		return;
	}

	parent = gtk_widget_get_parent (GTK_WIDGET (text_view));
	if (GTK_IS_SCROLLED_WINDOW (parent)) {
		GtkAdjustment *vadj;

		vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (parent));

		if (vadj->value < vadj->upper - vadj->page_size) {
			bottom = FALSE;
		}
	}
	
	/* Turn off this for now since it doesn't work. */
	bottom = TRUE;

	buffer = gtk_text_view_get_buffer (text_view);

	gtk_text_buffer_get_end_iter (buffer, &iter);
	if (!gtk_text_iter_is_start (&iter)) {
		gtk_text_buffer_insert (buffer,
					&iter,
					"\n",
					1);
	}
	
	if (from) {
		if (strncmp (msg, "/me ", 4) != 0) {
			/* Regular message. */
			stamp = get_stamp (timestamp);
			
			gtk_text_buffer_get_end_iter (buffer, &iter);
			gtk_text_buffer_insert_with_tags_by_name (buffer,
								  &iter,
								  stamp,
								  -1,
								  "shadow",
								  NULL);

			g_free (stamp);

			/* FIXME: This only works if nick == name... */
			if (!strcmp (from, to)) {
				nick_tag = "nick-me";
			}
			else if (strcmp (from, to) && strstr (msg, to)) {
				nick_tag = "nick-highlight";
			} else {
				nick_tag = "nick-other";
			}

			gtk_text_buffer_insert_with_tags_by_name (buffer,
								  &iter,
								  "<",
								  1,
								  nick_tag,
								  NULL);
				
			gtk_text_buffer_get_end_iter (buffer, &iter);
			gtk_text_buffer_insert_with_tags_by_name (buffer,
								  &iter,
								  from,
								  -1,
								  nick_tag,
								  NULL);
					
			gtk_text_buffer_get_end_iter (buffer, &iter);
			gtk_text_buffer_insert_with_tags_by_name (buffer,
								  &iter,
								  "> ",
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
	
	num_matches = url_regex_match (msg, start, end);

	if (num_matches == 0) {
		gtk_text_buffer_get_end_iter (buffer, &iter);
		utils_insert_with_emoticons (buffer, &iter, msg);
	} else {
		gint   last = 0;
		gint   s = 0, e = 0;
		gchar *tmp;

		for (i = 0; i < num_matches; i++) {

			s = g_array_index (start, gint, i);
			e = g_array_index (end, gint, i);

			if (s > last + 1) {
				tmp = utils_get_substring (msg, last, s);
				
				gtk_text_buffer_get_end_iter (buffer, &iter);
				utils_insert_with_emoticons (buffer, &iter, tmp);
				g_free (tmp);
			}

			tmp = utils_get_substring (msg, s, e);
			
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
			tmp = utils_get_substring (msg, e, strlen (msg));
			
			gtk_text_buffer_get_end_iter (buffer, &iter);
			utils_insert_with_emoticons (buffer, &iter, tmp);
			g_free (tmp);
		}
	}

	g_array_free (start, FALSE);
	g_array_free (end, FALSE);
	
	/* Scroll to the end of the newly inserted text, if we were at the
	 * bottom before.
	 */
	if (bottom) {
		gtk_text_buffer_get_end_iter (buffer, &iter);
		mark = gtk_text_buffer_create_mark (buffer,
						    NULL,
						    &iter,
						    FALSE);
		
		gtk_text_view_scroll_to_mark (text_view,
					      mark,
					      0.0,
					      FALSE,
					      0,
					      0);
	}
}

void
gossip_text_view_append_normal_message (GtkTextView *text_view,
					const gchar *msg)
{
	GtkTextBuffer *buffer;
	GtkTextIter    iter;
	GtkTextMark   *mark;
	gboolean       bottom = TRUE;
	GtkWidget     *parent;

	if (msg == NULL || msg[0] == 0) {
		return;
	}

	parent = gtk_widget_get_parent (GTK_WIDGET (text_view));
	if (GTK_IS_SCROLLED_WINDOW (parent)) {
		GtkAdjustment *vadj;
		
		vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (parent));

		if (vadj->value < vadj->upper - vadj->page_size) {
			bottom = FALSE;
		}
	}

	/* Turn off for now. */ 
	bottom = TRUE;

	buffer = gtk_text_view_get_buffer (text_view);

	gtk_text_buffer_get_end_iter (buffer, &iter);
	if (!gtk_text_iter_is_start (&iter)) {
		gtk_text_buffer_insert (buffer,
					&iter,
					"\n",
					1);
		gtk_text_buffer_get_end_iter (buffer, &iter);
	}

	utils_insert_with_emoticons (buffer, &iter, msg);

	if (bottom) {
		/* Scroll to the end of the newly inserted text. */
		gtk_text_buffer_get_end_iter (buffer, &iter);
		mark = gtk_text_buffer_create_mark (buffer,
						    NULL,
						    &iter,
						    FALSE);
		
		gtk_text_view_scroll_to_mark (text_view,
					      mark,
					      0.0,
					      FALSE,
					      0,
					      0);
	}
}

void
gossip_text_view_set_margin (GtkTextView *tv, gint margin)
{
	GtkWidget *widget;
	GdkWindow *win;

	widget = GTK_WIDGET (tv);
	
	gtk_text_view_set_left_margin (tv, margin);
	gtk_text_view_set_right_margin (tv, margin);
	
	gtk_text_view_set_border_window_size (tv, GTK_TEXT_WINDOW_TOP, margin);
	gtk_text_view_set_border_window_size (tv, GTK_TEXT_WINDOW_BOTTOM, margin);
	
	win = gtk_text_view_get_window (tv, GTK_TEXT_WINDOW_TOP);
	gdk_window_set_background (win, &widget->style->base[GTK_STATE_NORMAL]);

	win = gtk_text_view_get_window (tv, GTK_TEXT_WINDOW_BOTTOM);
	gdk_window_set_background (win, &widget->style->base[GTK_STATE_NORMAL]);
}

static gboolean
utils_url_event_cb (GtkTextTag    *tag,
		    GObject       *object,
		    GdkEvent      *event,
		    GtkTextIter   *iter,
		    GtkTextBuffer *buffer)
{
	GtkTextIter  start, end;
	gchar       *str;

	if (event->type == GDK_BUTTON_RELEASE && event->button.button == 1) {
		start = end = *iter;
		
		if (gtk_text_iter_backward_to_tag_toggle (&start, NULL) &&
		    gtk_text_iter_forward_to_tag_toggle (&end, NULL)) {
			str = gtk_text_buffer_get_text (buffer,
							&start,
							&end,
							FALSE);
			
			gnome_url_show (str, NULL);
			
			g_free (str);
		}
	}
	
	return FALSE;
}

static gboolean
utils_text_view_event_cb (GtkTextView    *view,
			  GdkEventMotion *event,
			  GtkTextTag     *tag)
{
	static GdkCursor  *hand = NULL;
	static GdkCursor  *beam = NULL;
	GtkTextWindowType  type;
	GtkTextIter        iter;
	GdkWindow         *win;
	gint               x, y, buf_x, buf_y;

	type = gtk_text_view_get_window_type (view, event->window);
	
	if (type != GTK_TEXT_WINDOW_TEXT) {
		return FALSE;
	}

	/* Get where the pointer really is. */
	win = gtk_text_view_get_window (view, type);
	gdk_window_get_pointer (win, &x, &y, NULL);
	
	/* Get the iter where the cursor is at */
	gtk_text_view_window_to_buffer_coords (view, type, x, y, &buf_x, &buf_y);
	gtk_text_view_get_iter_at_location (view, &iter, buf_x, buf_y);

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

void
gossip_text_view_setup_tags (GtkTextView *view)
{
	GtkTextBuffer   *buffer;
	GtkTextTagTable *table;
	GtkTextTag      *tag;
	
	buffer = gtk_text_view_get_buffer (view);
	
	gtk_text_buffer_create_tag (buffer,
				    "nick-me",
				    "foreground", "sea green",
				    NULL);	
	
	gtk_text_buffer_create_tag (buffer,
				    "nick-other",
				    "foreground", "steelblue4",
				    NULL);	
	
	gtk_text_buffer_create_tag (buffer,
				    "nick-highlight",
				    "foreground", "indian red",
				    NULL);	

	gtk_text_buffer_create_tag (buffer,
				    "notice",
				    "foreground", "steelblue4",
				    NULL);

	gtk_text_buffer_create_tag (buffer,
				    "shadow",
				    "foreground", "darkgrey",
				    NULL);
	
	gtk_text_buffer_create_tag (buffer,
				    "url",
				    "foreground", "steelblue",
				    "underline", PANGO_UNDERLINE_SINGLE,
				    NULL);	

	table = gtk_text_buffer_get_tag_table (buffer);

	tag = gtk_text_tag_table_lookup (table, "url");
	
	g_signal_connect (tag,
			  "event",
			  G_CALLBACK (utils_url_event_cb),
			  buffer);

	g_signal_connect (view,
			  "event",
			  G_CALLBACK (utils_text_view_event_cb),
			  tag);
}

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
	{ GOSSIP_SMILEY_INDIFFERENT, ":|", 0 },
	{ GOSSIP_SMILEY_TOUNGE,      ":p", 0 },
	{ GOSSIP_SMILEY_TOUNGE,      ":P", 0 },
	{ GOSSIP_SMILEY_SHOCKED,     ":o", 0 },
	{ GOSSIP_SMILEY_SHOCKED,     ":O", 0 },
	{ GOSSIP_SMILEY_COOL,        "8)", 0 },
	{ GOSSIP_SMILEY_COOL,        "B)", 0 }
};

static gint num_smileys = G_N_ELEMENTS (smileys);

static GdkPixbuf *
utils_get_smiley (GossipSmiley smiley)
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
utils_insert_with_emoticons (GtkTextBuffer *buf,
			     GtkTextIter   *iter, 
			     const gchar   *str)
{
	const gchar *p;
	gunichar     c, prev_c;
	gint         i;
	gint         match;
	gint         submatch;

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
			
			pixbuf = utils_get_smiley (smileys[match].smiley);
			gtk_text_buffer_insert_pixbuf (buf, iter, pixbuf);
			gtk_text_buffer_insert (buf, iter, " ", 1);
		} else {
			gtk_text_buffer_insert (buf, iter, str, -1);
			return;
		}

		str = g_utf8_find_next_char (p, NULL);
	}
}

const gchar *
gossip_status_to_icon_filename (GossipStatus status)
{
	switch (status) {
	case GOSSIP_STATUS_OFFLINE:
		return IMAGEDIR "/gossip-offline.png";
	case GOSSIP_STATUS_AVAILABLE:
		return IMAGEDIR "/gossip-online.png";
	case GOSSIP_STATUS_AWAY:
		return IMAGEDIR "/gossip-away.png";
	case GOSSIP_STATUS_EXT_AWAY:
		return IMAGEDIR "/gossip-extended-away.png";
	case GOSSIP_STATUS_BUSY:
		return IMAGEDIR "/gossip-busy.png";
	case GOSSIP_STATUS_FREE:
		return IMAGEDIR "/gossip-chat.png";
	default:
		g_assert_not_reached ();
	}

	return NULL;
}

const gchar *
gossip_status_to_string (GossipStatus status)
{
	switch (status) {
	case GOSSIP_STATUS_AVAILABLE:
		return NULL;
		break;
	case GOSSIP_STATUS_FREE:
		return "chat";
		break;
	case GOSSIP_STATUS_AWAY:
		return "away";
		break;
	case GOSSIP_STATUS_EXT_AWAY:
		return "xa";
		break;
	case GOSSIP_STATUS_BUSY:
		return "dnd";
		break;
	default:
		return NULL;
	}
	
	return NULL;
}
#if 0
gboolean
gossip_utils_is_valid_jid (const gchar *jid)
{
	const gchar *at;
	const gchar *dot;
	gint         jid_len;
	
	if (!jid || strcmp (jid, "") == 0) {
		return FALSE;
	}

	jid_len = strlen (jid);

	at = strchr (jid, '@');
	if (!at || at == jid || at == jid + jid_len - 1) {
		return FALSE;
	}
	
	dot = strchr (at, '.');
	if (dot == at + 1 
	    || dot == jid + jid_len - 1 
	    || dot == jid + jid_len - 2) {
		return FALSE;
	}

	dot = strrchr (jid, '.');
	if (dot == jid + jid_len - 1 ||
	    dot == jid + jid_len - 2) {
		return FALSE;
	}

	return TRUE;
}
#endif

const gchar *
gossip_utils_get_show_filename (const gchar *show)
{
	if (!show) {
		return IMAGEDIR "/gossip-online.png";
	}
	else if (strcmp (show, "chat") == 0) {
		return IMAGEDIR "/gossip-chat.png";
	}
	else if (strcmp (show, "away") == 0) {
		return IMAGEDIR "/gossip-away.png";
	}
	else if (strcmp (show, "xa") == 0) {
		return IMAGEDIR "/gossip-extended-away.png";
	}
	else if (strcmp (show, "dnd") == 0) {
		return IMAGEDIR "/gossip-busy.png";
	}
	
	return IMAGEDIR "/gossip-online.png";
}

const gchar *
gossip_utils_get_timestamp_from_message (LmMessage *m)
{
	LmMessageNode *node;
	const gchar   *timestamp = NULL;
	const gchar   *xmlns;
	
	for (node = m->node->children; node; node = node->next) {
                if (strcmp (node->name, "x") == 0) {
			xmlns = lm_message_node_get_attribute (node, "xmlns");
			if (xmlns && strcmp (xmlns, "jabber:x:delay") == 0) {
                                timestamp = lm_message_node_get_attribute 
					(node, "stamp");
                        }
                }
        }
	
	return timestamp;
}

GossipStatus
gossip_utils_get_status_from_type_show (LmMessageSubType  type,
					const gchar      *show)
{
	if (type == LM_MESSAGE_SUB_TYPE_UNAVAILABLE) {
		return GOSSIP_STATUS_OFFLINE;
	}

	if (!show) {
		return GOSSIP_STATUS_AVAILABLE;
	}
	else if (strcmp (show, "chat") == 0) {
		return GOSSIP_STATUS_FREE;
	}
	else if (strcmp (show, "away") == 0) {
		return GOSSIP_STATUS_AWAY;
	}
	else if (strcmp (show, "xa") == 0) {
		return GOSSIP_STATUS_EXT_AWAY;
	}
	else if (strcmp (show, "dnd") == 0) {
		return GOSSIP_STATUS_BUSY;
	}
	
	return GOSSIP_STATUS_AVAILABLE;
}

