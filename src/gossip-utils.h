/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003      Imendio HB
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

#ifndef __GOSSIP_UTILS_H__
#define __GOSSIP_UTILS_H__

#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkmessagedialog.h> 
#include <gtk/gtksizegroup.h>
#include <glade/glade.h>
#include <loudmouth/loudmouth.h>

#include "gossip-account.h"
#include "gossip-presence.h"

/* Note: We don't support "free to chat". */
typedef enum {
	GOSSIP_SHOW_AVAILABLE, /* available (null) */
	GOSSIP_SHOW_BUSY,      /* busy (dnd) */
	GOSSIP_SHOW_AWAY,      /* away (away) */
	GOSSIP_SHOW_EXT_AWAY   /* extended away (xa) */
} GossipShow;

typedef struct {
	GossipShow  show;
	gchar      *string;
} GossipStatusEntry;


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

const gchar *gossip_utils_get_timestamp_from_message (LmMessage        *message);
gchar *      gossip_utils_get_timestamp              (const gchar      *time_str);
gchar *      gossip_password_dialog_run              (GossipAccount    *account,
						      GtkWindow        *parent);
GtkWidget *  gossip_hig_dialog_new                   (GtkWindow        *parent,
						      GtkDialogFlags    flags,
						      GtkMessageType    type,
						      GtkButtonsType    buttons,
						      const gchar      *header,
						      const gchar      *messagefmt,
						      ...);

gchar *      gossip_utils_substring                  (const gchar      *str,
						      gint              start,
						      gint              end);

const gchar *gossip_utils_show_to_string             (GossipShow        show);
GossipShow   gossip_utils_show_from_string           (const gchar      *str);
const gchar *gossip_utils_get_stock_from_show        (GossipShow        show);
GdkPixbuf *  gossip_utils_get_pixbuf_from_stock      (const gchar      *stock);
GdkPixbuf *  gossip_utils_get_pixbuf_offline         (void);
GdkPixbuf *  gossip_utils_get_pixbuf_from_show       (GossipShow        show);

/* FIXME: Deprecate */
const gchar *gossip_utils_get_default_status_show    (GossipShow        show);
const gchar *gossip_utils_get_default_status         (GossipPresenceType type);
GList *      gossip_utils_get_status_messages        (void);
void         gossip_utils_set_status_messages        (GList            *list);
void         gossip_utils_free_status_messages       (GList            *list);

gint         gossip_utils_url_regex_match            (const gchar      *msg,
						      GArray           *start,
						      GArray           *end);
GossipPresenceType
gossip_utils_get_presence_type_from_show_string      (const gchar *str); 
gint         gossip_utils_str_case_cmp               (const gchar        *s1, 
						      const gchar        *s2);
gint         gossip_utils_str_n_case_cmp             (const gchar        *s1, 
						      const gchar        *s2,
						      gsize               n);
void         gossip_utils_set_proxy                  (LmConnection       *conn);

#endif /*  __GOSSIP_UTILS_H__ */
