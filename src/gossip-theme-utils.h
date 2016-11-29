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

#ifndef __GOSSIP_THEME_UTILS_H__
#define __GOSSIP_THEME_UTILS_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

void          gossip_theme_utils_ensure_tag_by_name  (GtkTextBuffer   *buffer,
                                                      const gchar     *name);

GtkTextTag *  gossip_theme_utils_init_tag_by_name    (GtkTextTagTable *table,
                                                      const gchar     *name);
void          gossip_theme_utils_add_tag             (GtkTextTagTable *table,
                                                      GtkTextTag      *tag);

G_END_DECLS

#endif /* __GOSSIP_THEME_UTILS_H__ */


