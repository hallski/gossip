/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
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

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#ifdef HAVE_GNOME
#include <gdk/gdkx.h>
#endif

#include "gossip-avatar-image.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       GOSSIP_TYPE_AVATAR_IMAGE, GossipAvatarImagePriv))

#define MAX_SMALL  48
#define MAX_LARGE 400

typedef struct {
	GtkWidget   *image;
	GtkWidget   *popup;
	GtkTooltips *tooltips;	
	GdkPixbuf   *pixbuf;
} GossipAvatarImagePriv;

static void     avatar_image_finalize                (GObject           *object);
static void     avatar_image_add_filter              (GossipAvatarImage *avatar_image);
static void     avatar_image_remove_filter           (GossipAvatarImage *avatar_image);
static gboolean avatar_image_button_press_event_cb   (GtkWidget         *widget,
						      GdkEventButton    *event,
						      GossipAvatarImage *avatar_image);
static gboolean avatar_image_button_release_event_cb (GtkWidget         *widget,
						      GdkEventButton    *event,
						      GossipAvatarImage *avatar_image);

G_DEFINE_TYPE (GossipAvatarImage, gossip_avatar_image, GTK_TYPE_EVENT_BOX);

static void
gossip_avatar_image_class_init (GossipAvatarImageClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass*) klass;

	object_class->finalize = avatar_image_finalize;

	g_type_class_add_private (object_class, sizeof (GossipAvatarImagePriv));
}

static void
gossip_avatar_image_init (GossipAvatarImage *avatar_image)
{
	GossipAvatarImagePriv *priv;

	priv = GET_PRIV (avatar_image);
	
	priv->image = gtk_image_new ();
	
	gtk_container_add (GTK_CONTAINER (avatar_image), priv->image);

	/* FIXME: Should just override the methods instead. */
	g_signal_connect (avatar_image,
			  "button-press-event",
			  G_CALLBACK (avatar_image_button_press_event_cb),
			  avatar_image);
	g_signal_connect (avatar_image,
			  "button-release-event",
			  G_CALLBACK (avatar_image_button_release_event_cb),
			  avatar_image);

	priv->tooltips = gtk_tooltips_new ();
	g_object_ref (priv->tooltips);
	gtk_object_sink (GTK_OBJECT (priv->tooltips));

	avatar_image_add_filter (avatar_image);

	gtk_widget_show (priv->image);
}

static void
avatar_image_finalize (GObject *object)
{
	GossipAvatarImagePriv *priv;

	priv = GET_PRIV (object);

	avatar_image_remove_filter (GOSSIP_AVATAR_IMAGE (object));

	if (priv->popup) {
		gtk_widget_destroy (priv->popup);
	}
	
	if (priv->pixbuf) {
		g_object_unref (priv->pixbuf);
	}

	g_object_unref (priv->tooltips);
	
	G_OBJECT_CLASS (gossip_avatar_image_parent_class)->finalize (object);
}

#ifdef HAVE_GNOME
static GdkFilterReturn
avatar_image_filter_func (GdkXEvent  *gdkxevent,
			  GdkEvent   *event,
			  gpointer    data)
{
	XEvent                *xevent = gdkxevent;
	Atom                   atom;
	GossipAvatarImagePriv *priv;

	priv = GET_PRIV (data);
	
	switch (xevent->type) {
	case PropertyNotify:
		atom = gdk_x11_get_xatom_by_name ("_NET_CURRENT_DESKTOP");
		if (xevent->xproperty.atom == atom) {
			if (priv->popup) {
				gtk_widget_destroy (priv->popup);
				priv->popup = NULL;
			}
		}		
		break;
	}
	
	return GDK_FILTER_CONTINUE;
}
#endif

static void
avatar_image_add_filter (GossipAvatarImage *avatar_image)
{
#ifdef HAVE_GNOME
	Window     window;
	GdkWindow *gdkwindow;
	gint       mask;

	mask = PropertyChangeMask;
	
	window = GDK_ROOT_WINDOW ();
	gdkwindow = gdk_xid_table_lookup (window);
	
	gdk_error_trap_push ();
	if (gdkwindow) {
		XWindowAttributes attrs;
		XGetWindowAttributes (gdk_display, window, &attrs);
		mask |= attrs.your_event_mask;
	}
	
	XSelectInput (gdk_display, window, mask);
	
	gdk_error_trap_pop ();
	
	gdk_window_add_filter (NULL, avatar_image_filter_func, avatar_image);
#endif
}

static void
avatar_image_remove_filter (GossipAvatarImage *avatar_image)
{
#ifdef HAVE_GNOME
	gdk_window_remove_filter (NULL, avatar_image_filter_func, avatar_image);
#endif
}

static GdkPixbuf *
avatar_image_scale_down_if_necessary (GdkPixbuf *pixbuf, gint max_size)
{
	gint      width, height;
	gdouble   factor;

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	if (width > max_size || height > max_size) {
		factor = (gdouble) max_size / MAX (width, height);
		
		width = width * factor;
		height = height * factor;
		
		return gdk_pixbuf_scale_simple (pixbuf,
						width, height,
						GDK_INTERP_HYPER);
	}

	return g_object_ref (pixbuf);
}

static gboolean
avatar_image_button_press_event_cb (GtkWidget         *widget,
				    GdkEventButton    *event,
				    GossipAvatarImage *avatar_image)
{
	GossipAvatarImagePriv *priv;
	GtkWidget             *popup;
	GtkWidget             *frame;
	GtkWidget             *image;
	gint                   x, y;
	gint                   popup_width, popup_height;
	gint                   width, height;
	GdkPixbuf             *pixbuf;

	priv = GET_PRIV (avatar_image);
	
	if (priv->popup) {
		gtk_widget_destroy (priv->popup);
		priv->popup = NULL;
	}
	
	if (event->button != 1 || event->type != GDK_BUTTON_PRESS) {
		return FALSE;
	}

	popup_width = gdk_pixbuf_get_width (priv->pixbuf);
	popup_height = gdk_pixbuf_get_height (priv->pixbuf);
	
	width = priv->image->allocation.width;
	height = priv->image->allocation.height;

	/* Don't show a popup if the popup is smaller then the currently avatar
	 * image.
	 */
	if (popup_height <= height || popup_width <= width) {
		return TRUE;
	}

	pixbuf = avatar_image_scale_down_if_necessary (priv->pixbuf, MAX_LARGE);
	popup_width = gdk_pixbuf_get_width (pixbuf);
	popup_height = gdk_pixbuf_get_height (pixbuf);
	
	popup = gtk_window_new (GTK_WINDOW_POPUP);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);

	gtk_container_add (GTK_CONTAINER (popup), frame);
	
	image = gtk_image_new ();
	gtk_container_add (GTK_CONTAINER (frame), image);

	gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
	g_object_unref (pixbuf);
	
	gdk_window_get_origin (priv->image->window, &x, &y);

	x = x - (popup_width - width) / 2;
	y = y - (popup_height - height) / 2;
	
	gtk_window_move (GTK_WINDOW (popup), x, y);

	priv->popup = popup;
	
	gtk_widget_show_all (popup);

	return TRUE;
}

static gboolean
avatar_image_button_release_event_cb (GtkWidget         *widget,
				      GdkEventButton    *event,
				      GossipAvatarImage *avatar_image)
{
	GossipAvatarImagePriv *priv;

	priv = GET_PRIV (avatar_image);
		
	if (event->button != 1 || event->type != GDK_BUTTON_RELEASE) {
		return FALSE;
	}

	if (!priv->popup) {
		return TRUE;
	}
	
	gtk_widget_destroy (priv->popup);
	priv->popup = NULL;
		
	return TRUE;
}

GtkWidget *
gossip_avatar_image_new (GdkPixbuf *pixbuf)
{
	GossipAvatarImage *avatar_image;

	avatar_image = g_object_new (GOSSIP_TYPE_AVATAR_IMAGE, NULL);
	
	gossip_avatar_image_set_pixbuf (avatar_image, pixbuf);

	return GTK_WIDGET (avatar_image);
}

void
gossip_avatar_image_set_pixbuf (GossipAvatarImage *avatar_image,
				GdkPixbuf         *pixbuf)
{
	GossipAvatarImagePriv *priv;
	GdkPixbuf             *scaled_pixbuf;
	
	priv = GET_PRIV (avatar_image);

	if (priv->pixbuf) {
		g_object_unref (priv->pixbuf);
		priv->pixbuf = NULL;
	}
	
	if (!pixbuf) {
		gtk_widget_hide (priv->image);
		return;
	}

	priv->pixbuf = g_object_ref (pixbuf);

	scaled_pixbuf = avatar_image_scale_down_if_necessary (priv->pixbuf, MAX_SMALL);
	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image), scaled_pixbuf);

	if (scaled_pixbuf != priv->pixbuf) {
		gtk_tooltips_set_tip (priv->tooltips,
				      GTK_WIDGET (avatar_image),
				      _("Click to enlarge"),
				      NULL);
	} else {
		gtk_tooltips_set_tip (priv->tooltips,
				      GTK_WIDGET (avatar_image),
				      NULL, NULL);
	}
	
	g_object_unref (scaled_pixbuf);

	gtk_widget_show (priv->image);
}

