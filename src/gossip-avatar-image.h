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

#ifndef __GOSSIP_AVATAR_IMAGE_H__
#define __GOSSIP_AVATAR_IMAGE_H__

#include <gtk/gtkeventbox.h>

#define GOSSIP_TYPE_AVATAR_IMAGE         (gossip_avatar_image_get_type ())
#define GOSSIP_AVATAR_IMAGE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_AVATAR_IMAGE, GossipAvatarImage))
#define GOSSIP_AVATAR_IMAGE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_AVATAR_IMAGE, GossipAvatarImageClass))
#define GOSSIP_IS_AVATAR_IMAGE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_AVATAR_IMAGE))
#define GOSSIP_IS_AVATAR_IMAGE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_AVATAR_IMAGE))
#define GOSSIP_AVATAR_IMAGE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_AVATAR_IMAGE, GossipAvatarImageClass))

typedef struct _GossipAvatarImage      GossipAvatarImage;
typedef struct _GossipAvatarImageClass GossipAvatarImageClass;

struct _GossipAvatarImage {
	GtkEventBox      parent;
};

struct _GossipAvatarImageClass {
	GtkEventBoxClass parent_class;
};

GType       gossip_avatar_image_get_type   (void) G_GNUC_CONST;
GtkWidget * gossip_avatar_image_new        (GdkPixbuf         *pixbuf);
void        gossip_avatar_image_set_pixbuf (GossipAvatarImage *avatar_image,
					    GdkPixbuf         *pixbuf);


#endif /* __GOSSIP_AVATAR_IMAGE_H__ */
