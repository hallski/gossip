/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
 * Part of this file is copied from GtkSourceView (gtksourceiter.c).
 */

#ifndef __GOSSIP_UI_UTILS_H__
#define __GOSSIP_UI_UTILS_H__

#include <gtk/gtk.h>

#include <libgossip/gossip.h>

G_BEGIN_DECLS

/* Pixbufs */
GdkPixbuf *gossip_pixbuf_for_presence_state         (GossipPresenceState  state);
GdkPixbuf *gossip_pixbuf_for_presence               (GossipPresence      *presence);
GdkPixbuf *gossip_pixbuf_for_contact                (GossipContact       *contact);
GdkPixbuf *gossip_pixbuf_offline                    (void);
GdkPixbuf *gossip_pixbuf_for_chatroom_status        (GossipChatroom      *chatroom,
						     GtkIconSize          icon_size);

/* Windows */
gboolean   gossip_window_get_is_visible             (GtkWindow           *window);
void       gossip_window_present                    (GtkWindow           *window,
						     gboolean             steal_focus);
void       gossip_window_set_default_icon_name      (const gchar         *name);

gboolean   gossip_url_show                          (const char          *url);
void       gossip_help_show                         (void);

void       gossip_toggle_button_set_state_quietly   (GtkWidget           *widget,
						     GCallback            callback,
						     gpointer             user_data,
						     gboolean             active);
GtkWidget *gossip_link_button_new                   (const gchar         *url,
						     const gchar         *title);

void       gossip_request_user_attention            (void);

G_END_DECLS

#endif /*  __GOSSIP_UI_UTILS_H__ */
