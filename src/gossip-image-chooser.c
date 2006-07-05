/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2006 Imendio AB.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Based on Novell's e-image-chooser.
 */

#include <config.h>

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkdnd.h>
#include <libgnomevfs/gnome-vfs-ops.h>

#include <libgossip/gossip-debug.h>

#include "gossip-marshal.h"
#include "gossip-image-chooser.h"

#define DEBUG_DOMAIN "ImageChooser"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_IMAGE_CHOOSER_TYPE, GossipImageChooserPriv))

typedef struct {
	GtkWidget    *image;

	gchar        *image_data;
	gint          image_data_size;

	gint          image_max_width;
	gint          image_max_height;

	GdkPixbuf    *pixbuf;

	gboolean      editable;
} GossipImageChooserPriv;

static void       image_chooser_finalize              (GObject            *object);
static GdkPixbuf *image_chooser_scale_pixbuf          (GdkPixbuf          *pixbuf,
						       gint                max_width,
						       gint                max_height);
static gboolean   image_chooser_set_image_from_data   (GossipImageChooser *chooser,
						       const gchar        *data,
						       gsize               size);
static gboolean   image_chooser_drag_motion_cb        (GtkWidget          *widget,
						       GdkDragContext     *context,
						       gint                x,
						       gint                y,
						       guint               time,
						       GossipImageChooser *chooser);
static void       image_chooser_drag_leave_cb         (GtkWidget          *widget,
						       GdkDragContext     *context,
						       guint               time,
						       GossipImageChooser *chooser);
static gboolean   image_chooser_drag_drop_cb          (GtkWidget          *widget,
						       GdkDragContext     *context,
						       gint                x,
						       gint                y,
						       guint               time,
						       GossipImageChooser *chooser);
static void       image_chooser_drag_data_received_cb (GtkWidget          *widget,
						       GdkDragContext     *context,
						       gint                x,
						       gint                y,
						       GtkSelectionData   *selection_data,
						       guint               info,
						       guint               time,
						       GossipImageChooser *chooser);

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

G_DEFINE_TYPE (GossipImageChooser, gossip_image_chooser, GTK_TYPE_VBOX);

/*
 * Drag and drop stuff
 */
#define URI_LIST_TYPE "text/uri-list"

enum DndTargetType {
	DND_TARGET_TYPE_URI_LIST
};

static const GtkTargetEntry drop_types[] = {
	{ URI_LIST_TYPE, 0, DND_TARGET_TYPE_URI_LIST },
};

static void
gossip_image_chooser_class_init (GossipImageChooserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = image_chooser_finalize;

	signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (object_class, sizeof (GossipImageChooserPriv));
}

static void
gossip_image_chooser_init (GossipImageChooser *chooser)
{
	GossipImageChooserPriv *priv;

	priv = GET_PRIV (chooser);

	priv->image_max_width = -1;
	priv->image_max_height = -1;

	gtk_box_set_homogeneous (GTK_BOX (chooser), FALSE);

	priv->image = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (chooser), priv->image, TRUE, TRUE, 0);
	gtk_widget_show (priv->image);

	gtk_drag_dest_set (priv->image, 
			   GTK_DEST_DEFAULT_ALL,
			   drop_types, 
			   G_N_ELEMENTS (drop_types),
			   GDK_ACTION_COPY);

	g_signal_connect (priv->image, "drag-motion", 
			  G_CALLBACK (image_chooser_drag_motion_cb), 
			  chooser);
	g_signal_connect (priv->image, "drag-leave", 
			  G_CALLBACK (image_chooser_drag_leave_cb), 
			  chooser);
	g_signal_connect (priv->image, "drag-drop",
			  G_CALLBACK (image_chooser_drag_drop_cb), 
			  chooser);
	g_signal_connect (priv->image, "drag-data-received", 
			  G_CALLBACK (image_chooser_drag_data_received_cb), 
			  chooser);

	priv->editable = TRUE;
}

static void
image_chooser_finalize (GObject *object)
{
	GossipImageChooserPriv *priv;

	priv = GET_PRIV (object);

	if (priv->image_data) {
		g_free (priv->image_data);
		priv->image_data = NULL;
	}

	if (priv->pixbuf) {
		g_object_unref (priv->pixbuf);
		priv->pixbuf = NULL;
	}

	G_OBJECT_CLASS (gossip_image_chooser_parent_class)->finalize (object);
}

static GdkPixbuf *
image_chooser_scale_pixbuf (GdkPixbuf *pixbuf,
			    gint       max_width,
			    gint       max_height)
{
	GdkPixbuf *scaled = NULL;
	gfloat     scale;
	gint       new_height, new_width;

	new_height = gdk_pixbuf_get_height (pixbuf);
	new_width = gdk_pixbuf_get_width (pixbuf);

	gossip_debug (DEBUG_DOMAIN, "Scaling pixbuf size "
		      "(width:%d, height:%d, max width:%d, max height:%d)...", 
		      new_width, new_height, max_width, max_height);
	
	if ((max_width <= 0 && max_height <= 0) || 
	    (max_width > new_width && max_height > new_height)) {
		gossip_debug (DEBUG_DOMAIN, "Scaling pixbuf to 1.0");
		scale = 1.0;
	} else if (new_width > max_width || new_height > max_height) {
		/* Scale down */
		if ((new_width - max_width) > (new_height - max_height)) {
			scale = (gfloat) max_width / new_width;
			gossip_debug (DEBUG_DOMAIN, "Scaling pixbuf down to %f "
				      "(width is bigger)", 
				      scale);
		} else {
			scale = (gfloat) max_height / new_height;
			gossip_debug (DEBUG_DOMAIN, "Scaling pixbuf down to %f "
				      "(height is bigger)", 
				      scale);
		}
	} else {
		/* Scale up */
		if (new_height > new_width) {
			scale = (gfloat) new_height / max_height;
			gossip_debug (DEBUG_DOMAIN, "Scaling pixbuf up to %f "
				      "(height is bigger)", 
				      scale);
		} else { 
			scale = (gfloat) new_width / max_width;
			gossip_debug (DEBUG_DOMAIN, "Scaling pixbuf up to %f "
				      "(width is bigger)", 
				      scale);
		}	
	}
	
	if (scale == 1.0) {
		scaled = g_object_ref (pixbuf);
		
		gossip_debug (DEBUG_DOMAIN, "Using width:%d, height:%d", 
			      new_width, new_height);
	} else {
		new_width *= scale;
		new_height *= scale;
		new_width = MIN (new_width, max_width);
		new_height = MIN (new_height, max_height);
		
		gossip_debug (DEBUG_DOMAIN, "Using width:%d, height:%d", 
			      new_width, new_height);
		
		scaled = gdk_pixbuf_scale_simple (pixbuf,
						  new_width, new_height,
						  GDK_INTERP_BILINEAR);
	}
	
	return scaled;
}	

static gboolean
image_chooser_set_image_from_data (GossipImageChooser *chooser,
				   const gchar        *data, 
				   gsize               size)
{
	GossipImageChooserPriv *priv;
	GdkPixbufLoader        *loader;
	GdkPixbuf              *pixbuf;  
	gboolean                success = FALSE;

	priv = GET_PRIV (chooser);

	if (!data) {
		/* Set image_data to NULL */
		g_free (priv->image_data);

		priv->image_data = NULL;
		priv->image_data_size = 0;

		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image), 
					      "stock_person", 
					      GTK_ICON_SIZE_DIALOG);

		return TRUE;
	}

	loader = gdk_pixbuf_loader_new ();
	gdk_pixbuf_loader_write (loader, data, size, NULL);
	
	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
	if (pixbuf) {  
		GdkPixbuf *scaled_pixbuf;
		gchar     *scaled_data;
		gsize      scaled_size; 

		/* Remember pixbuf */
		if (priv->pixbuf) {
			g_object_unref (priv->pixbuf);
		}
		
		priv->pixbuf = g_object_ref (pixbuf);

		/* Scale the image data */
		scaled_pixbuf = image_chooser_scale_pixbuf (pixbuf,
							    priv->image_max_width,
							    priv->image_max_height);
		
		gdk_pixbuf_save_to_buffer (scaled_pixbuf,
					   &scaled_data,
					   &scaled_size,
					   "png", 
					   NULL,
					   "compression", "9",
					   NULL);
		
		g_free (priv->image_data);
		
		priv->image_data = scaled_data;
		priv->image_data_size = scaled_size;
		
		/* Now update image */
		scaled_pixbuf = image_chooser_scale_pixbuf (priv->pixbuf,
							    priv->image->allocation.width,
							    priv->image->allocation.height);
		
		gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image), scaled_pixbuf);
		g_object_unref (scaled_pixbuf);
		
		success = TRUE;
	} 
	
	gdk_pixbuf_loader_close (loader, NULL);
	g_object_unref (loader);
	
	return success;
}

static gboolean
image_chooser_drag_motion_cb (GtkWidget          *widget,
			      GdkDragContext     *context,
			      gint                x, 
			      gint                y, 
			      guint               time, 
			      GossipImageChooser *chooser)
{
	GossipImageChooserPriv *priv;
	GList                  *p;

	priv = GET_PRIV (chooser);

	if (!priv->editable) {
		return FALSE;
	}

	for (p = context->targets; p != NULL; p = p->next) {
		gchar *possible_type;

		possible_type = gdk_atom_name (GDK_POINTER_TO_ATOM (p->data));

		if (!strcmp (possible_type, URI_LIST_TYPE)) {
			g_free (possible_type);
			gdk_drag_status (context, GDK_ACTION_COPY, time);

			return TRUE;
		}

		g_free (possible_type);
	}

	return FALSE;
}

static void
image_chooser_drag_leave_cb (GtkWidget          *widget,
			     GdkDragContext     *context,
			     guint               time, 
			     GossipImageChooser *chooser)
{
	GossipImageChooserPriv *priv;

	priv = GET_PRIV (chooser);
}

static gboolean
image_chooser_drag_drop_cb (GtkWidget          *widget,
			    GdkDragContext     *context,
			    gint                x,
			    gint                y,
			    guint               time,
			    GossipImageChooser *chooser)
{
	GossipImageChooserPriv *priv;
	GList                  *p;

	priv = GET_PRIV (chooser);

	if (!priv->editable) {
		return FALSE;
	}

	if (context->targets == NULL) {
		return FALSE;
	}

	for (p = context->targets; p != NULL; p = p->next) {
		char *possible_type;

		possible_type = gdk_atom_name (GDK_POINTER_TO_ATOM (p->data));
		if (!strcmp (possible_type, URI_LIST_TYPE)) {
			g_free (possible_type);
			gtk_drag_get_data (widget, context,
					   GDK_POINTER_TO_ATOM (p->data),
					   time);

			return TRUE;
		}

		g_free (possible_type);
	}

	return FALSE;
}

static void
image_chooser_drag_data_received_cb (GtkWidget          *widget,
				     GdkDragContext     *context,
				     gint                x, 
				     gint                y,
				     GtkSelectionData   *selection_data,
				     guint               info, 
				     guint               time, 
				     GossipImageChooser *chooser)
{
	gchar    *target_type;
	gboolean  handled = FALSE;

	target_type = gdk_atom_name (selection_data->target);

	if (!strcmp (target_type, URI_LIST_TYPE)) {
		GnomeVFSHandle   *handle;
		GnomeVFSResult    result;
		GnomeVFSFileInfo  info;
		gchar            *uri;
		gchar            *nl;
		gchar            *data = NULL;

		nl = strstr (selection_data->data, "\r\n");
		if (nl) {
			uri = g_strndup (selection_data->data, 
					 nl - (gchar*) selection_data->data);
		} else {
			uri = g_strdup (selection_data->data); 
		}

		result = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ);
		if (result == GNOME_VFS_OK) {
			result = gnome_vfs_get_file_info_from_handle (handle, 
								      &info, 
								      GNOME_VFS_FILE_INFO_DEFAULT);
			if (result == GNOME_VFS_OK) {
				GnomeVFSFileSize data_size;

				data = g_malloc (info.size);

				result = gnome_vfs_read (handle, data, info.size, &data_size);
				if (result == GNOME_VFS_OK) {
					if (image_chooser_set_image_from_data (chooser, 
									       data, 
									       data_size)) {
						handled = TRUE;
					} else {
						g_free (data);
					}
				} else {
					g_free (data);
				}
			}

			gnome_vfs_close (handle);

		}

		g_free (uri);
	}

	gtk_drag_finish (context, handled, FALSE, time);
}

GtkWidget *
gossip_image_chooser_new (void)
{
	return g_object_new (GOSSIP_IMAGE_CHOOSER_TYPE, NULL);
}

gboolean
gossip_image_chooser_set_from_file (GossipImageChooser *chooser,
				    const char         *filename)
{
	gchar *data;
	gsize  data_size;

	g_return_val_if_fail (GOSSIP_IS_IMAGE_CHOOSER (chooser), FALSE);
	g_return_val_if_fail (filename, FALSE);

	if (!g_file_get_contents (filename, &data, &data_size, NULL)) {
		return FALSE;
	}

	if (!image_chooser_set_image_from_data (chooser, data, data_size)) {
		g_free (data);
	}

	return TRUE;
}

gboolean
gossip_image_chooser_set_image_data (GossipImageChooser *chooser, 
				     const gchar        *data, 
				     gsize               data_size)
{
	g_return_val_if_fail (GOSSIP_IS_IMAGE_CHOOSER (chooser), FALSE);

	if (!image_chooser_set_image_from_data (chooser, data, data_size)) {
		return FALSE;
	}	

	/* Tell the world */
	g_signal_emit (chooser, signals[CHANGED], 0);

	return TRUE;
}

void
gossip_image_chooser_set_editable (GossipImageChooser *chooser, 
				   gboolean            editable)
{
	GossipImageChooserPriv *priv;

	g_return_if_fail (GOSSIP_IS_IMAGE_CHOOSER (chooser));

	priv = GET_PRIV (chooser);

	priv->editable = editable;
}

void
gossip_image_chooser_set_image_max_size   (GossipImageChooser  *chooser,
					   gint                 width,
					   gint                 height)
{
	GossipImageChooserPriv *priv;

	g_return_if_fail (GOSSIP_IS_IMAGE_CHOOSER (chooser));
	g_return_if_fail (width > 0 && height > 0);

	priv = GET_PRIV (chooser);

	if (width > 0) {
		priv->image_max_width = width;
	}

	if (height > 0) {
		priv->image_max_height = height;
	}
}

gboolean
gossip_image_chooser_get_image_data (GossipImageChooser  *chooser, 
				     gchar              **data,
				     gsize               *data_size)
{
	GossipImageChooserPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_IMAGE_CHOOSER (chooser), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (data_size != NULL, FALSE);

	priv = GET_PRIV (chooser);

	*data_size = priv->image_data_size;
	*data = g_malloc (*data_size);

	memcpy (*data, priv->image_data, *data_size);

	return TRUE;
}

void
gossip_image_chooser_get_image_max_size (GossipImageChooser  *chooser,
					 gint                *width,
					 gint                *height)
{
	GossipImageChooserPriv *priv;

	g_return_if_fail (GOSSIP_IS_IMAGE_CHOOSER (chooser));
	g_return_if_fail (width != NULL || height != NULL);

	priv = GET_PRIV (chooser);

	if (width) {
		*width = priv->image_max_width;
	}

	if (height) {
		*height = priv->image_max_height;
	}	
}
