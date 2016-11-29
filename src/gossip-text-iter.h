/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2002-2007 Imendio AB
 * Copyright (C) 2000 - 2005 Paolo Maggi 
 * Copyright (C) 2002, 2003 Jeroen Zwartepoorte

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
 * From GtkSourceView (gtksourceiter.c).
 */

#ifndef __GOSSIP_TEXT_ITER_H__
#define __GOSSIP_TEXT_ITER_H__

#include <glib.h>
#include <gtk/gtk.h>

    G_BEGIN_DECLS

    gboolean   gossip_text_iter_forward_search          (const GtkTextIter   *iter,
                                                         const gchar         *str,
                                                         GtkTextIter         *match_start,
                                                         GtkTextIter         *match_end,
                                                         const GtkTextIter   *limit);
gboolean   gossip_text_iter_backward_search         (const GtkTextIter   *iter,
                                                     const gchar         *str,
                                                     GtkTextIter         *match_start,
                                                     GtkTextIter         *match_end,
                                                     const GtkTextIter   *limit);

G_END_DECLS

#endif /* __GOSSIP_TEXT_ITER_H__ */
