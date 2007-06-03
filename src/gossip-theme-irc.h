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
 */

#ifndef __GOSSIP_THEME_IRC_H__
#define __GOSSIP_THEME_IRC_H__

#include <glib-object.h>

#include "gossip-theme.h"

G_BEGIN_DECLS

#define GOSSIP_TYPE_THEME_IRC            (gossip_theme_irc_get_type ())
#define GOSSIP_THEME_IRC(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_THEME_IRC, GossipThemeIrc))
#define GOSSIP_THEME_IRC_CLASS(k)        (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_THEME_IRC, GossipThemeIrcClass))
#define GOSSIP_IS_THEME_IRC(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_THEME_IRC))
#define GOSSIP_IS_THEME_IRC_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_THEME_IRC))
#define GOSSIP_THEME_IRC_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_THEME_IRC, GossipThemeIrcClass))

typedef struct _GossipThemeIrc      GossipThemeIrc;
typedef struct _GossipThemeIrcClass GossipThemeIrcClass;

struct _GossipThemeIrc {
	GossipTheme parent;
};

struct _GossipThemeIrcClass {
	GossipThemeClass parent_class;
};

GType               gossip_theme_irc_get_type                 (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GOSSIP_THEME_IRC_H__ */

