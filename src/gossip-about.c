/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio AB
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

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <glib/gi18n.h>

#include <libgnome/gnome-url.h>

#include "gossip-about.h"

extern GConfClient *gconf_client;

#define TOP       48
#define LEFT      225
#define WIDTH     210
#define HEIGHT    196
#define INDENT    10
#define Y_PADDING 1

#define LINK_LEFT   5
#define LINK_TOP    262
#define LINK_RIGHT  320
#define LINK_BOTTOM 286

#define FONT_LARGE 10
#define FONT_SMALL 8

#undef TEST_ABOUT

#ifdef TEST_ABOUT
#include "../po/po-list.h"

static gint n;
#endif

typedef struct {
	GtkWidget *window;
	gchar     *title;
	gchar     *written_by;
	gchar     *translated_by;
	gchar     *artwork_by;
	gboolean   has_hand;
} AboutData;


static gboolean
motion_notify_event_cb (GtkWidget      *widget,
			GdkEventMotion *event,
			AboutData      *data)
{
	GdkCursor *cursor;
	
	if (event->x > LINK_LEFT && event->x < LINK_RIGHT &&
	    event->y > LINK_TOP && event->y < LINK_BOTTOM) {
		if (!data->has_hand) {
			cursor = gdk_cursor_new (GDK_HAND2);
			gdk_window_set_cursor (data->window->window, cursor);
			gdk_cursor_unref (cursor);
			data->has_hand = TRUE;
		}
	} else {
		if (data->has_hand) {
			gdk_window_set_cursor (data->window->window, NULL);
			data->has_hand = FALSE;
		}
	}

	return FALSE;
}

static gboolean
button_press_event_cb (GtkWidget      *widget,
		       GdkEventButton *event,
		       AboutData      *data)
{
	if (event->x > LINK_LEFT && event->x < LINK_RIGHT &&
	    event->y > LINK_TOP && event->y < LINK_BOTTOM) {

		gnome_url_show ("http://developer.imendio.com/wiki/gossip", NULL);
		return FALSE;
	}
	
#ifdef TEST_ABOUT
	n++;

	if (po_list[n] == NULL) {
		gtk_widget_destroy (data->window);
	} else {
		data->translated_by = (gchar*) po_list[n];
		
		gtk_widget_queue_draw (data->window);
	}
#else
	gtk_widget_destroy (data->window);
#endif
	
	return FALSE;
}

static gboolean
key_press_event_cb (GtkWidget   *widget,
		    GdkEventKey *event,
		    AboutData   *data)
{
	if (event->keyval == GDK_Escape ||
	    event->keyval == GDK_Return ||
	    event->keyval == GDK_ISO_Enter ||
	    event->keyval == GDK_KP_Enter ||
	    event->keyval == GDK_space) {
		gtk_widget_destroy (data->window);
	}
	
	return FALSE;
}

static gint
draw_text (PangoLayout *layout,
	   GdkDrawable *drawable,
	   GdkGC       *gc,
	   const gchar *str,
	   gint         x,
	   gint         y,
	   gdouble      size,
	   PangoWeight  weight)
{
	PangoAttrList  *attr_list;
	PangoAttribute *attr;
	gint            width, height;

	pango_layout_set_text (layout, str, -1);
	
	attr_list = pango_attr_list_new ();

	attr = pango_attr_family_new ("sans");
	attr->start_index = 0;
	attr->end_index = -1;
	pango_attr_list_insert (attr_list, attr);

	attr = pango_attr_size_new (PANGO_SCALE * size);
	attr->start_index = 0;
	attr->end_index = -1;
	pango_attr_list_insert (attr_list, attr);

	attr = pango_attr_weight_new (weight);
	attr->start_index = 0;
	attr->end_index = -1;
	pango_attr_list_insert (attr_list, attr);

	pango_layout_set_attributes (layout, attr_list);

	gdk_draw_layout (drawable,
			 gc,
			 x, y,
			 layout);

	pango_layout_get_pixel_size (layout, &width, &height);
	
	pango_attr_list_unref (attr_list);

	return y + height;
}

static gboolean
expose_event_cb (GtkWidget      *widget,
		 GdkEventExpose *event,
		 AboutData      *data)
{
	PangoLayout    *layout;
	GdkGC          *gc;
	gint            y;
	GdkColormap    *colormap;
	GdkColor        color;
	GdkRectangle    rect;
	gint            dpi;
	gdouble         scale;
	GError         *error = NULL;

	/* Hack :/ We depend on getting a pixel size of the text, so scale
	 * against the dpi, try the getting it from GTK+ first and fallback to
	 * the gconf key.
	 */
#if 0
	settings = gtk_settings_get_default ();
	if (&& settings &&
	    g_object_class_find_property (G_OBJECT_GET_CLASS (settings),
					  "gtk-xft-dpi")) {
		g_object_get (settings, "gtk-xft-dpi", &dpi, NULL);
		
		dpi /= 1024;
	}
#endif
	
	dpi = gconf_client_get_float (gconf_client,
				      "/desktop/gnome/font_rendering/dpi",
				      &error);
	
	if (error) {
		dpi = 96;
		g_error_free (error);
	}
	
	/* Be safe */
	if (dpi < 50 || dpi > 300) {
		dpi = 96;
	}

	scale = 96.0 / dpi;
	
	colormap = gtk_widget_get_colormap (widget);
	
	gdk_color_parse ("white", &color);
	gdk_rgb_find_color (colormap, &color);
	
	layout = gtk_widget_create_pango_layout (widget, NULL);

	gc = widget->style->text_gc[GTK_STATE_NORMAL];

	gc = widget->style->white_gc;

	gc = gdk_gc_new (widget->window);
	gdk_gc_set_foreground (gc, &color);
	
	draw_text (layout, widget->window, gc,
		   data->title,
		   10, 10,
		   scale * 16, PANGO_WEIGHT_BOLD);

	g_object_unref (gc);
	
	gdk_color_parse ("black", &color);
	gdk_rgb_find_color (colormap, &color);

	gc = gdk_gc_new (widget->window);
	gdk_gc_set_foreground (gc, &color);
	
	rect.x = LEFT;
	rect.y = TOP;
	rect.width = WIDTH;
	rect.height = HEIGHT;
	
	gdk_gc_set_clip_rectangle (gc, &rect);

#if 0
	/* Test the clip rectangle. */
	gdk_draw_rectangle (widget->window,
			    gc,
			    FALSE,
			    rect.x,
			    rect.y,
			    rect.width-1,
			    rect.height-1);
#endif

	y = draw_text (layout, widget->window, gc,
		       _("Written by:"),
		       LEFT, TOP,
		       scale * FONT_LARGE, PANGO_WEIGHT_BOLD);
	
	y = draw_text (layout, widget->window, gc,
		       data->written_by,
		       LEFT + INDENT, y + Y_PADDING,
		       scale * FONT_SMALL, PANGO_WEIGHT_NORMAL);
	
	y = draw_text (layout, widget->window, gc,
		       _("Artwork by:"),
		       LEFT, y + Y_PADDING,
		       scale * FONT_LARGE, PANGO_WEIGHT_BOLD);

	y = draw_text (layout, widget->window, gc,
		       data->artwork_by,
		       LEFT + INDENT, y + Y_PADDING,
		       scale * FONT_SMALL, PANGO_WEIGHT_NORMAL);

	if (data->translated_by) {
		y = draw_text (layout, widget->window, gc,
			       _("Translated by:"),
			       LEFT, y + Y_PADDING,
			       scale * FONT_LARGE, PANGO_WEIGHT_BOLD);
		
		y = draw_text (layout, widget->window, gc,
			       data->translated_by,
			       LEFT + INDENT, y + Y_PADDING,
			       scale * FONT_SMALL, PANGO_WEIGHT_NORMAL);
	}

	g_object_unref (layout);
	g_object_unref (gc);

	return FALSE;
}

static void
about_destroy_cb (GtkWidget *widget, gpointer data)
{
	g_free (data);
}

GtkWidget *
gossip_about_new (void)
{
	static GtkWidget *window = NULL;
	GtkWidget        *image;
	AboutData        *data;
	gchar            *translator_credits;

	data = g_new0 (AboutData, 1);

	data->title = PACKAGE_STRING;

	data->written_by =
		"Mikael Hallendal\n"
		"Richard Hult\n"
		"Martyn Russell\n"
		"Geert-Jan Van den Bogaerde\n"
		"Kevin Dougherty";

	/* Note to translators: put your name here so it will shop up in the
	 * "about" box. Please add ONLY names, separated by newlines. No mail
	 * addresses, empty lines, full sentences or years. Just the
	 * names. Thanks!
	 */
	translator_credits = _("translator_credits");
	
	if (strcmp (translator_credits, "translator_credits") != 0) {
		data->translated_by = translator_credits;
	} else {
		data->translated_by = NULL;
	}

	data->artwork_by = "Daniel Taylor";
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	data->window = window;
	
	gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
	gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);

	gtk_window_set_title (GTK_WINDOW (window), PACKAGE_NAME);
	
	image = gtk_image_new_from_file (IMAGEDIR "/gossip-about.png");
	gtk_container_add (GTK_CONTAINER (window), image);

	gtk_widget_show (image);
	
	gtk_misc_set_alignment (GTK_MISC (image), 0, 0);

	gtk_widget_add_events (window, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK);

	g_signal_connect_after (image,
				"expose_event", G_CALLBACK (expose_event_cb),
				data);
	
	g_signal_connect (window,
			  "destroy", G_CALLBACK (about_destroy_cb),
			  data);

	g_signal_connect (window,
			  "motion_notify_event", G_CALLBACK (motion_notify_event_cb),
			  data);
	
	g_signal_connect (window,
			  "button_press_event", G_CALLBACK (button_press_event_cb),
			  data);
	
	g_signal_connect (window,
			  "key_press_event", G_CALLBACK (key_press_event_cb),
			  data);

	return window;
}


