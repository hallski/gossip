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
 * 
 * Author: Brian Pepple <bpepple@fedoraproject.org>
 *         Martyn Russell <martyn@imendio.com>
 */

#ifndef __GOSSIP_EBOOK_H__
#define __GOSSIP_EBOOK_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_EBOOK         (gossip_ebook_get_type ())
#define GOSSIP_EBOOK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_EBOOK, GossipEBook))
#define GOSSIP_EBOOK_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_EBOOK, GossipEBookClass))
#define GOSSIP_IS_EBOOK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_EBOOK))
#define GOSSIP_IS_EBOOK_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_EBOOK))
#define GOSSIP_EBOOK_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_EBOOK, GossipEBookClass))

typedef struct _GossipEBook      GossipEBook;
typedef struct _GossipEBookClass GossipEBookClass;

struct _GossipEBook {
	GObject      parent;
};

struct _GossipEBookClass {
	GObjectClass parent_class;
};

GType         gossip_ebook_get_type      (void) G_GNUC_CONST;

gchar *       gossip_ebook_get_jabber_id (void);
gchar *       gossip_ebook_get_name      (void);
gchar *       gossip_ebook_get_nickname  (void);
gchar *       gossip_ebook_get_email     (void);
gchar *       gossip_ebook_get_website   (void);
gchar *       gossip_ebook_get_birthdate (void);
GossipAvatar *gossip_ebook_get_avatar    (void);

gboolean      gossip_ebook_set_jabber_id (const gchar  *id);
gboolean      gossip_ebook_set_name      (const gchar  *name);
gboolean      gossip_ebook_set_nickname  (const gchar  *nickname);
gboolean      gossip_ebook_set_email     (const gchar  *email);
gboolean      gossip_ebook_set_website   (const gchar  *website);
gboolean      gossip_ebook_set_avatar    (GossipAvatar *avatar);

G_END_DECLS

#endif /* __GOSSIP_EBOOK_H__ */
