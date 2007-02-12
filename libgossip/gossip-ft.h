/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Imendio AB
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

#ifndef __GOSSIP_FT_H__
#define __GOSSIP_FT_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_FT         (gossip_ft_get_gtype ())
#define GOSSIP_FT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_FT, GossipFT))
#define GOSSIP_FT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_FT, GossipFTClass))
#define GOSSIP_IS_FT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_FT))
#define GOSSIP_IS_FT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_FT))
#define GOSSIP_FT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_FT, GossipFTClass))

typedef struct _GossipFT      GossipFT;
typedef struct _GossipFTClass GossipFTClass;

typedef gint GossipFTId;

struct _GossipFT {
	GObject parent;
};

struct _GossipFTClass {
	GObjectClass parent_class;
};

typedef enum {
	GOSSIP_FT_TYPE_RECEIVING,
	GOSSIP_FT_TYPE_SENDING
} GossipFTType;

typedef enum {
	GOSSIP_FT_ERROR_DECLINED,
	GOSSIP_FT_ERROR_UNSUPPORTED,
	GOSSIP_FT_ERROR_UNKNOWN
} GossipFTError;

GType          gossip_ft_get_gtype           (void) G_GNUC_CONST;

GossipFT *     gossip_ft_new                (void);
GossipFTType   gossip_ft_get_type           (GossipFT      *ft);
void           gossip_ft_set_type           (GossipFT      *ft,
					     GossipFTType   type);
GossipContact *gossip_ft_get_contact        (GossipFT      *ft);
void           gossip_ft_set_contact        (GossipFT      *ft,
					     GossipContact *contact);
GossipFTId     gossip_ft_get_id             (GossipFT      *ft);
const gchar *  gossip_ft_get_file_name      (GossipFT      *ft);
void           gossip_ft_set_file_name      (GossipFT      *ft,
					     const gchar   *file_name);
guint64        gossip_ft_get_file_size      (GossipFT      *ft);
void           gossip_ft_set_file_size      (GossipFT      *ft,
					     guint64        file_size);
const gchar *  gossip_ft_get_file_mime_type (GossipFT      *ft);
void           gossip_ft_set_file_mime_type (GossipFT      *ft,
					     const gchar   *file_mime_type);

gboolean       gossip_ft_equal              (gconstpointer  a,
					     gconstpointer  b);

G_END_DECLS

#endif /* __GOSSIP_FT_H__ */
