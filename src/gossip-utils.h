/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Richard Hult <rhult@imendo.com>
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

#include <gtk/gtkwidget.h>
#include <gtk/gtktextview.h>
#include <glade/glade.h>
#include "gossip-app.h"

void          gossip_option_menu_setup             (GtkWidget     *option_menu,
						    GCallback      func,
						    gpointer       user_data,
						    gconstpointer  str1,
						    ...);

void          gossip_option_menu_set_history       (GtkOptionMenu *option_menu,
						    gpointer       user_data);

gpointer      gossip_option_menu_get_history       (GtkOptionMenu *option_menu);

void          gossip_status_menu_setup             (GtkWidget     *option_menu,
						    GCallback      func,
						    gpointer       user_data,
						    gconstpointer  str1,
						    ...);

void          gossip_status_menu_set_status        (GtkWidget     *option_menu,
						    GossipStatus   status);

GossipStatus  gossip_status_menu_get_status        (GtkWidget     *option_menu);

void          gossip_glade_get_file_simple         (const gchar   *filename,
						    const gchar   *root,
						    const gchar   *domain,
						    const gchar   *first_required_widget,
						    ...);

GladeXML *    gossip_glade_get_file                (const gchar   *filename,
						    const gchar   *root,
						    const gchar   *domain,
						    const gchar   *first_required_widget,
						    ...);

void          gossip_glade_connect                 (GladeXML      *gui,
						    gpointer       user_data,
						    gchar         *first_widget,
						    ...);

void          gossip_text_view_set_margin          (GtkTextView   *tv,
						    gint           margin);

void          gossip_text_view_append_chat_message (GtkTextView   *text_view,
						    const gchar   *timestamp,
						    const gchar   *to,
						    const gchar   *from,
						    const gchar   *msg);

void          gossip_text_view_append_normal_message (GtkTextView   *text_view,
						      const gchar   *msg);

#if 0
gchar *       gossip_jid_get_name                  (const gchar   *str);

gchar *       gossip_jid_get_server                (const gchar   *str);

gchar *       gossip_jid_create                    (const gchar   *name,
						    const gchar   *server,
						    const gchar   *resource);

gchar *       gossip_jid_strip_resource            (const gchar   *str);

gchar *       gossip_jid_get_resource              (const gchar   *str);

#endif
void          gossip_text_view_setup_tags          (GtkTextView   *view);

const gchar * gossip_status_to_icon_filename       (GossipStatus   status);

const gchar * gossip_status_to_string             (GossipStatus   status);
#if 0
gboolean      gossip_utils_is_valid_jid            (const gchar   *jid);
#endif
const gchar * gossip_utils_get_show_filename       (const gchar   *show);

const gchar * gossip_utils_get_timestamp_from_message (LmMessage  *message);
GossipStatus  gossip_utils_get_status_from_type_show  (LmMessageSubType type,
						       const gchar *show);

#endif /*  __GOSSIP_UTILS_H__ */
