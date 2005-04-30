/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2005 Imendio AB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
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

void         gossip_option_menu_setup                (GtkWidget        *option_menu,
						      GCallback         func,
						      gpointer          user_data,
						      gconstpointer     str1,
						      ...);
void         gossip_option_image_menu_setup          (GtkWidget        *option_menu,
						      GCallback         func,
						      gpointer          user_data,
						      gconstpointer     str1,
						      ...);
void         gossip_option_menu_set_history          (GtkOptionMenu    *option_menu,
						      gpointer          user_data);
gpointer     gossip_option_menu_get_history          (GtkOptionMenu    *option_menu);
void         gossip_glade_get_file_simple            (const gchar      *filename,
						      const gchar      *root,
						      const gchar      *domain,
						      const gchar      *first_required_widget,
						      ...);
GladeXML *   gossip_glade_get_file                   (const gchar      *filename,
						      const gchar      *root,
						      const gchar      *domain,
						      const gchar      *first_required_widget,
						      ...);
void         gossip_glade_connect                    (GladeXML         *gui,
						      gpointer          user_data,
						      gchar            *first_widget,
						      ...);
void         gossip_glade_setup_size_group           (GladeXML         *gui,
						      GtkSizeGroupMode  mode,
						      gchar            *first_widget, ...);

gchar *      gossip_password_dialog_run              (GossipAccount    *account,
						      GtkWindow        *parent);
GtkWidget *  gossip_hig_dialog_new                   (GtkWindow        *parent,
						      GtkDialogFlags    flags,
						      GtkMessageType    type,
						      GtkButtonsType    buttons,
						      const gchar      *header,
						      const gchar      *messagefmt,
						      ...);

GdkPixbuf *  gossip_ui_utils_get_pixbuf_from_stock   (const gchar      *stock);
GdkPixbuf *  gossip_ui_utils_get_pixbuf_offline      (void);

gboolean     gossip_ui_utils_window_get_is_visible   (GtkWindow        *window);
void         gossip_ui_utils_window_present          (GtkWindow        *window);

GdkPixbuf * gossip_ui_utils_presence_state_get_pixbuf(GossipPresenceState state);
GdkPixbuf *  gossip_ui_utils_presence_get_pixbuf     (GossipPresence   *presence);
GdkPixbuf *  gossip_ui_utils_contact_get_pixbuf      (GossipContact    *contact);

#endif /*  __GOSSIP_UI_UTILS_H__ */
