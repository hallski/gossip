/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 */

#ifndef __GOSSIP_THEME_BOXES_H__
#define __GOSSIP_THEME_BOXES_H__

#include <glib-object.h>

#include "gossip-theme.h"

G_BEGIN_DECLS

#define GOSSIP_TYPE_THEME_BOXES            (gossip_theme_boxes_get_type ())
#define GOSSIP_THEME_BOXES(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_THEME_BOXES, GossipThemeBoxes))
#define GOSSIP_THEME_BOXES_CLASS(k)        (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_THEME_BOXES, GossipThemeBoxesClass))
#define GOSSIP_IS_THEME_BOXES(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_THEME_BOXES))
#define GOSSIP_IS_THEME_BOXES_CLASS(k)     (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_THEME_BOXES))
#define GOSSIP_THEME_BOXES_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_THEME_BOXES, GossipThemeBoxesClass))

typedef struct _GossipThemeBoxes      GossipThemeBoxes;
typedef struct _GossipThemeBoxesClass GossipThemeBoxesClass;

struct _GossipThemeBoxes {
    GossipTheme parent;
};

struct _GossipThemeBoxesClass {
    GossipThemeClass parent_class;
};

GType         gossip_theme_boxes_get_type      (void) G_GNUC_CONST;

GossipTheme * gossip_theme_boxes_new           (const gchar *name);

G_END_DECLS

#endif /* __GOSSIP_THEME_BOXES_H__ */

