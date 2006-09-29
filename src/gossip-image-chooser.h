/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006  Imendio AB.
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

#ifndef __GOSSIP_IMAGE_CHOOSER_H__
#define __GOSSIP_IMAGE_CHOOSER_H__

#include <gtk/gtkvbox.h>

G_BEGIN_DECLS

#define GOSSIP_IMAGE_CHOOSER_TYPE	     (gossip_image_chooser_get_type ())
#define GOSSIP_IMAGE_CHOOSER(obj)	     (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOSSIP_IMAGE_CHOOSER_TYPE, GossipImageChooser))
#define GOSSIP_IMAGE_CHOOSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GOSSIP_IMAGE_CHOOSER_TYPE, GossipImageChooserClass))
#define GOSSIP_IS_IMAGE_CHOOSER(obj)	     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOSSIP_IMAGE_CHOOSER_TYPE))
#define GOSSIP_IS_IMAGE_CHOOSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), GOSSIP_IMAGE_CHOOSER_TYPE))

typedef struct _GossipImageChooser        GossipImageChooser;
typedef struct _GossipImageChooserClass   GossipImageChooserClass;
typedef struct _GossipImageChooserPrivate GossipImageChooserPrivate;

struct _GossipImageChooser {
	GtkVBox parent;
};

struct _GossipImageChooserClass {
	GtkVBoxClass parent_class;

	/* signals */
	void (*changed) (GossipImageChooser *chooser);
};

GType      gossip_image_chooser_get_type           (void);
GtkWidget *gossip_image_chooser_new                (void);
gboolean   gossip_image_chooser_set_from_file      (GossipImageChooser  *chooser,
						    const gchar         *filename);
gboolean   gossip_image_chooser_set_image_data     (GossipImageChooser  *chooser,
						    const gchar         *data,
						    gsize                data_size);
void       gossip_image_chooser_set_image_max_size (GossipImageChooser  *chooser,
						    gint                 width,
						    gint                 height);
void       gossip_image_chooser_set_editable       (GossipImageChooser  *chooser,
						    gboolean             editable);
gboolean   gossip_image_chooser_get_image_data     (GossipImageChooser  *chooser,
						    gchar              **data,
						    gsize               *data_size);
void       gossip_image_chooser_get_image_max_size (GossipImageChooser  *chooser,
						    gint                *width,
						    gint                *height);

#endif /* __GOSSIP_IMAGE_CHOOSER_H__ */
