/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2005 Imendio AB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
 * Copyright (C) 2005      Martyn Russell <mr@gnome.org>
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

#ifndef __GOSSIP_UI_UTILS_H__
#define __GOSSIP_UI_UTILS_H__

#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkmessagedialog.h> 
#include <gtk/gtksizegroup.h>
#include <glade/glade.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-presence.h>
#include <libgossip/gossip-chatroom.h>

#include "gossip-chat-view.h"

/* glade */
void       gossip_glade_get_file_simple      (const gchar         *filename,
					      const gchar         *root,
					      const gchar         *domain,
					      const gchar         *first_required_widget,
					      ...);
GladeXML * gossip_glade_get_file             (const gchar         *filename,
					      const gchar         *root,
					      const gchar         *domain,
					      const gchar         *first_required_widget,
					      ...);
void       gossip_glade_connect              (GladeXML            *gui,
					      gpointer             user_data,
					      gchar               *first_widget,
					      ...);
void       gossip_glade_setup_size_group     (GladeXML            *gui,
					      GtkSizeGroupMode     mode,
					      gchar               *first_widget,
					      ...);

/* dialogs */
gchar *    gossip_password_dialog_run        (GossipAccount       *account,
					      GtkWindow           *parent);

/* pixbufs */
GdkPixbuf *gossip_pixbuf_from_stock          (const gchar         *stock,
					      GtkIconSize          size);
GdkPixbuf *gossip_pixbuf_from_account_type   (GossipAccountType    type,
					      GtkIconSize          icon_size);
GdkPixbuf *gossip_pixbuf_from_account        (GossipAccount       *account,
					      GtkIconSize          icon_size);
GdkPixbuf *gossip_pixbuf_from_account_status (GossipAccount       *account,
					      GtkIconSize          icon_size,
					      gboolean             online);
GdkPixbuf *gossip_pixbuf_from_account_error  (GossipAccount       *account,
					      GtkIconSize          icon_size);
GdkPixbuf *gossip_pixbuf_from_smiley         (GossipSmiley         type,
					      GtkIconSize          icon_size);
GdkPixbuf *gossip_pixbuf_for_presence_state  (GossipPresenceState  state);
GdkPixbuf *gossip_pixbuf_for_presence        (GossipPresence      *presence);
GdkPixbuf *gossip_pixbuf_for_contact         (GossipContact       *contact);
GdkPixbuf *gossip_pixbuf_offline             (void);

GdkPixbuf *gossip_pixbuf_for_chatroom_status (GossipChatroom      *chatroom,
					      GtkIconSize          icon_size);
	

/* windows */
gboolean   gossip_window_get_is_visible      (GtkWindow           *window);
void       gossip_window_present             (GtkWindow           *window);

#endif /*  __GOSSIP_UI_UTILS_H__ */
