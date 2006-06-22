/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc.
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
 *
 */

#include <config.h>
#include <math.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtksignal.h>
#include <glib/gi18n.h>
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-util.h>
#include <gtk/gtkicontheme.h>

#include "gossip-throbber.h"

#define THROBBER_DEFAULT_TIMEOUT 100	/* Milliseconds Per Frame */

struct GossipThrobberDetails {
	GList	  *image_list;

	GdkPixbuf *quiescent_pixbuf;
	
	int	   max_frame;
	int	   delay;
	int	   current_frame;	
	guint	   timer_task;
	
	gboolean   ready;
	gboolean   small_mode;

	guint      icon_theme_changed_tag;
};

static void gossip_throbber_load_images            (GossipThrobber *throbber);
static void gossip_throbber_unload_images          (GossipThrobber *throbber);
static void gossip_throbber_theme_changed          (GtkIconTheme   *icon_theme,
						    GossipThrobber *throbber);
static void gossip_throbber_remove_update_callback (GossipThrobber *throbber);

GNOME_CLASS_BOILERPLATE (GossipThrobber, gossip_throbber, GtkEventBox, GTK_TYPE_EVENT_BOX)

static gboolean
is_throbbing (GossipThrobber *throbber)
{
	return throbber->details->timer_task != 0;
}

static void
get_throbber_dimensions (GossipThrobber *throbber, 
			 gint           *throbber_width,
			 gint           *throbber_height)
{
	GList     *image_list;
	GdkPixbuf *pixbuf;
	int        current_width, current_height;
	int        pixbuf_width, pixbuf_height;
	
	current_width = 0;
	current_height = 0;

	if (throbber->details->quiescent_pixbuf != NULL) {
		/* Start with the quiescent image */
		current_width = gdk_pixbuf_get_width (throbber->details->quiescent_pixbuf);
		current_height = gdk_pixbuf_get_height (throbber->details->quiescent_pixbuf);
	}

	/* Union with the animation image */
	image_list = throbber->details->image_list;
	if (image_list != NULL) {
		pixbuf = GDK_PIXBUF (image_list->data);
		pixbuf_width = gdk_pixbuf_get_width (pixbuf);
		pixbuf_height = gdk_pixbuf_get_height (pixbuf);
		
		if (pixbuf_width > current_width) {
			current_width = pixbuf_width;
		}
		
		if (pixbuf_height > current_height) {
			current_height = pixbuf_height;
		}
	}
		
	*throbber_width = current_width;
	*throbber_height = current_height;
}

static void
gossip_throbber_instance_init (GossipThrobber *throbber)
{
	GtkWidget *widget = GTK_WIDGET (throbber);
	
	GTK_WIDGET_UNSET_FLAGS (throbber, GTK_NO_WINDOW);

	gtk_widget_set_events (widget, 
			       gtk_widget_get_events (widget)
			       | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
			       | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
	
	throbber->details = g_new0 (GossipThrobberDetails, 1);
	
	throbber->details->delay = THROBBER_DEFAULT_TIMEOUT;
	
	throbber->details->icon_theme_changed_tag =
		g_signal_connect (gtk_icon_theme_get_default (),
				  "changed",
				  G_CALLBACK (gossip_throbber_theme_changed),
				  throbber);


	gossip_throbber_load_images (throbber);
	gtk_widget_show (widget);
}

static void
gossip_throbber_theme_changed (GtkIconTheme   *icon_theme,
			       GossipThrobber *throbber)
{
	gtk_widget_hide (GTK_WIDGET (throbber));
	gossip_throbber_load_images (throbber);

	gtk_widget_show (GTK_WIDGET (throbber));	
	gtk_widget_queue_resize ( GTK_WIDGET (throbber));
}

static GdkPixbuf *
select_throbber_image (GossipThrobber *throbber)
{
	GList *element;

	if (throbber->details->timer_task == 0) {
		if (throbber->details->quiescent_pixbuf == NULL) {
			return NULL;
		} else {
			return g_object_ref (throbber->details->quiescent_pixbuf);
		}
	}
	
	if (throbber->details->image_list == NULL) {
		return NULL;
	}
	
	element = g_list_nth (throbber->details->image_list, throbber->details->current_frame);
	
	return g_object_ref (element->data);
}

static int
gossip_throbber_expose (GtkWidget      *widget, 
			GdkEventExpose *event)
{
	GossipThrobber *throbber;
	GdkPixbuf *pixbuf;
	int x_offset, y_offset, width, height;
	GdkRectangle pix_area, dest;

	g_return_val_if_fail (GOSSIP_IS_THROBBER (widget), FALSE);

	throbber = GOSSIP_THROBBER (widget);
	if (!throbber->details->ready) {
		return FALSE;
	}

	pixbuf = select_throbber_image (throbber);
	if (pixbuf == NULL) {
		return FALSE;
	}

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	/* Compute the offsets for the image centered on our allocation */
	x_offset = (widget->allocation.width - width) / 2;
	y_offset = (widget->allocation.height - height) / 2;

	pix_area.x = x_offset;
	pix_area.y = y_offset;
	pix_area.width = width;
	pix_area.height = height;

	if (!gdk_rectangle_intersect (&event->area, &pix_area, &dest)) {
		g_object_unref (pixbuf);
		return FALSE;
	}
	
	gdk_draw_pixbuf (widget->window, NULL, pixbuf,
			 dest.x - x_offset, dest.y - y_offset,
			 dest.x, dest.y,
			 dest.width, dest.height,
			 GDK_RGB_DITHER_MAX, 0, 0);

	g_object_unref (pixbuf);

	return FALSE;
}

static void
gossip_throbber_map (GtkWidget *widget)
{
	GossipThrobber *throbber;
	
	throbber = GOSSIP_THROBBER (widget);
	
	GNOME_CALL_PARENT (GTK_WIDGET_CLASS, map, (widget));
	throbber->details->ready = TRUE;
}

static gboolean 
bump_throbber_frame (gpointer callback_data)
{
	GossipThrobber *throbber;

	throbber = GOSSIP_THROBBER (callback_data);
	if (!throbber->details->ready) {
		return TRUE;
	}

	throbber->details->current_frame += 1;
	if (throbber->details->current_frame > throbber->details->max_frame - 1) {
		throbber->details->current_frame = 0;
	}

	gtk_widget_draw (GTK_WIDGET (throbber), NULL);
	return TRUE;
}

void
gossip_throbber_start (GossipThrobber *throbber)
{
	if (is_throbbing (throbber)) {
		return;
	}

	if (throbber->details->timer_task != 0) {
		g_source_remove (throbber->details->timer_task);
	}
	
	/* reset the frame count */
	throbber->details->current_frame = 0;
	throbber->details->timer_task = g_timeout_add_full (G_PRIORITY_HIGH,
							    throbber->details->delay,
							    bump_throbber_frame,
							    throbber,
							    NULL);
}

static void
gossip_throbber_remove_update_callback (GossipThrobber *throbber)
{
	if (throbber->details->timer_task != 0) {
		g_source_remove (throbber->details->timer_task);
	}
	
	throbber->details->timer_task = 0;
}

void
gossip_throbber_stop (GossipThrobber *throbber)
{
	if (!is_throbbing (throbber)) {
		return;
	}

	gossip_throbber_remove_update_callback (throbber);
	gtk_widget_queue_draw (GTK_WIDGET (throbber));

}

static void
gossip_throbber_unload_images (GossipThrobber *throbber)
{
	GList *current_entry;

	if (throbber->details->quiescent_pixbuf != NULL) {
		g_object_unref (throbber->details->quiescent_pixbuf);
		throbber->details->quiescent_pixbuf = NULL;
	}

	/* Unref all the images in the list, and then let go of the list itself */
	current_entry = throbber->details->image_list;
	while (current_entry != NULL) {
		g_object_unref (current_entry->data);
		current_entry = current_entry->next;
	}
	
	g_list_free (throbber->details->image_list);
	throbber->details->image_list = NULL;
}

static GdkPixbuf *
scale_to_real_size (GossipThrobber *throbber, 
		    GdkPixbuf      *pixbuf)
{
	GdkPixbuf *result;
	int        size;

	size = gdk_pixbuf_get_height (pixbuf);

	if (throbber->details->small_mode) {
		result = gdk_pixbuf_scale_simple (pixbuf,
						  size * 2 / 3,
						  size * 2 / 3,
						  GDK_INTERP_BILINEAR);
	} else {
		result = g_object_ref (pixbuf);
	}

	return result;
}

static GdkPixbuf *
extract_frame (GossipThrobber *throbber, GdkPixbuf *grid_pixbuf, int x, int y, int size)
{
	GdkPixbuf *pixbuf, *result;

	if (x + size > gdk_pixbuf_get_width (grid_pixbuf) ||
	    y + size > gdk_pixbuf_get_height (grid_pixbuf)) {
		return NULL;
	}

	pixbuf = gdk_pixbuf_new_subpixbuf (grid_pixbuf,
					   x, y,
					   size, size);
	g_return_val_if_fail (pixbuf != NULL, NULL);

	result = scale_to_real_size (throbber, pixbuf);
	g_object_unref (pixbuf);

	return result;
}

static void
gossip_throbber_load_images (GossipThrobber *throbber)
{
	GtkIconInfo *icon_info;
	const char  *icon;
	GdkPixbuf   *icon_pixbuf, *pixbuf;
	GList       *image_list;
	int          grid_width, grid_height, x, y, size;

	gossip_throbber_unload_images (throbber);

	/* Load the animation */
	icon_info = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (),
						"gnome-spinner", -1, 0);
	if (icon_info == NULL) {
		g_warning ("Throbber animation not found");
		return;
	}

	size = gtk_icon_info_get_base_size (icon_info);
	icon = gtk_icon_info_get_filename (icon_info);
	g_return_if_fail (icon != NULL);
	
	icon_pixbuf = gdk_pixbuf_new_from_file (icon, NULL);
	if (icon_pixbuf == NULL) {
		g_warning ("Could not load the spinner file\n");
		gtk_icon_info_free (icon_info);
		return;
	}
	
	grid_width = gdk_pixbuf_get_width (icon_pixbuf);
	grid_height = gdk_pixbuf_get_height (icon_pixbuf);

	image_list = NULL;
	for (y = 0; y < grid_height; y += size) {
		for (x = 0; x < grid_width ; x += size) {
			pixbuf = extract_frame (throbber, icon_pixbuf, x, y, size);

			if (pixbuf) {
				image_list = g_list_prepend (image_list, pixbuf);
			} else {
				g_warning ("Cannot extract frame from the grid");
			}
		}
	}
	throbber->details->image_list = g_list_reverse (image_list);
	throbber->details->max_frame = g_list_length (throbber->details->image_list);

	gtk_icon_info_free (icon_info);
	g_object_unref (icon_pixbuf);

	/* Load the rest icon */
	icon_info = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (),
						"gnome-spinner-rest", -1, 0);
	if (icon_info == NULL) {
		g_warning ("Throbber rest icon not found\n");
		return;
	}

	size = gtk_icon_info_get_base_size (icon_info);
	icon = gtk_icon_info_get_filename (icon_info);
	g_return_if_fail (icon != NULL);

	icon_pixbuf = gdk_pixbuf_new_from_file (icon, NULL);
	throbber->details->quiescent_pixbuf = scale_to_real_size (throbber, icon_pixbuf);

	g_object_unref (icon_pixbuf);
	gtk_icon_info_free (icon_info);
}

void
gossip_throbber_set_small_mode (GossipThrobber *throbber, 
				gboolean        new_mode)
{
	if (new_mode != throbber->details->small_mode) {
		throbber->details->small_mode = new_mode;
		gossip_throbber_load_images (throbber);

		gtk_widget_queue_resize (GTK_WIDGET (throbber));
	}
}

static void
gossip_throbber_size_request (GtkWidget      *widget, 
			      GtkRequisition *requisition)
{
	GossipThrobber *throbber;;
	int             throbber_width, throbber_height;

	throbber = GOSSIP_THROBBER (widget);
	get_throbber_dimensions (throbber, &throbber_width, &throbber_height);
	
	/* Allocate some extra margin so we don't butt up against toolbar edges */
	requisition->width = throbber_width + 8;
   	requisition->height = throbber_height;
}

static void
gossip_throbber_finalize (GObject *object)
{
	GossipThrobber *throbber;

	throbber = GOSSIP_THROBBER (object);

	gossip_throbber_remove_update_callback (throbber);
	gossip_throbber_unload_images (throbber);
	
	if (throbber->details->icon_theme_changed_tag != 0) {
		g_signal_handler_disconnect (gtk_icon_theme_get_default (),
					     throbber->details->icon_theme_changed_tag);
		throbber->details->icon_theme_changed_tag = 0;
	}

	g_free (throbber->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gossip_throbber_class_init (GossipThrobberClass *class)
{
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (class);
	
	G_OBJECT_CLASS (class)->finalize = gossip_throbber_finalize;

	widget_class->expose_event = gossip_throbber_expose;
	widget_class->size_request = gossip_throbber_size_request;	
	widget_class->map          = gossip_throbber_map;
}

GtkWidget *
gossip_throbber_new (void)
{
	return g_object_new (GOSSIP_TYPE_THROBBER, NULL);
}
