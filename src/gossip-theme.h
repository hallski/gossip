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

#ifndef __GOSSIP_THEME_H__
#define __GOSSIP_THEME_H__

#include <glib-object.h>
#include <gtk/gtktextbuffer.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_THEME            (gossip_theme_get_type ())
#define GOSSIP_THEME(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_THEME, GossipTheme))
#define GOSSIP_THEME_CLASS(k)        (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_THEME, GossipThemeClass))
#define GOSSIP_IS_THEME(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_THEME))
#define GOSSIP_IS_THEME_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_THEME))
#define GOSSIP_THEME_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_THEME, GossipThemeClass))

typedef struct _GossipTheme      GossipTheme;
typedef struct _GossipThemeClass GossipThemeClass;

struct _GossipTheme {
	GObject parent;
};

struct _GossipThemeClass {
	GObjectClass parent_class;
};

GType        gossip_theme_get_type             (void) G_GNUC_CONST;

void         gossip_theme_insert_action        (GossipTheme   *theme,
						GtkTextBuffer *buffer);
void         gossip_theme_insert_text          (GossipTheme   *theme,
						GtkTextBuffer *buffer,
						const gchar   *body,
						const gchar   *tag);

/* Refactor-temp functions */
gboolean     gossip_theme_is_irc_style         (GossipTheme   *theme);
void         gossip_theme_set_is_irc_style     (GossipTheme   *theme,
						gboolean       is_irc_style);


G_END_DECLS

#endif /* __GOSSIP_THEME_H__ */

