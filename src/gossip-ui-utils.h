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

#include <glade/glade.h>
#include <gtk/gtksizegroup.h>
#include <libgossip/gossip-account.h>
#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-vcard.h>
#include <libgossip/gossip-presence.h>
#include <libgossip/gossip-chatroom.h>
#include <libgossip/gossip-avatar.h>

#include "gossip-chat-view.h"

G_BEGIN_DECLS

/* Glade */
void       gossip_glade_get_file_simple             (const gchar         *filename,
						     const gchar         *root,
						     const gchar         *domain,
						     const gchar         *first_required_widget,
						     ...);
GladeXML * gossip_glade_get_file                    (const gchar         *filename,
						     const gchar         *root,
						     const gchar         *domain,
						     const gchar         *first_required_widget,
						     ...);
void       gossip_glade_connect                     (GladeXML            *gui,
						     gpointer             user_data,
						     gchar               *first_widget,
						     ...);
void       gossip_glade_setup_size_group            (GladeXML            *gui,
						     GtkSizeGroupMode     mode,
						     gchar               *first_widget,
						     ...);

/* Dialogs */
gchar *    gossip_password_dialog_run               (GossipAccount       *account,
						     GtkWindow           *parent);
gboolean   gossip_hint_dialog_show                  (const gchar         *conf_path,
						     const gchar         *message1,
						     const gchar         *message2,
						     GtkWindow           *parent,
						     GFunc                func,
						     gpointer             user_data);
gboolean   gossip_hint_show                         (const gchar         *conf_path,
						     const gchar         *message1,
						     const gchar         *message2,
						     GtkWindow           *parent,
						     GFunc                func,
						     gpointer             user_data);

/* Pixbufs */
GdkPixbuf *gossip_pixbuf_from_stock                 (const gchar         *stock,
						     GtkIconSize          size);
GdkPixbuf *gossip_pixbuf_from_account_type          (GossipAccountType    type,
						     GtkIconSize          icon_size);
GdkPixbuf *gossip_pixbuf_from_account               (GossipAccount       *account,
						     GtkIconSize          icon_size);
GdkPixbuf *gossip_pixbuf_from_account_status        (GossipAccount       *account,
						     GtkIconSize          icon_size,
						     gboolean             online);
GdkPixbuf *gossip_pixbuf_from_account_error         (GossipAccount       *account,
						     GtkIconSize          icon_size);
GdkPixbuf *gossip_pixbuf_from_smiley                (GossipSmiley         type,
						     GtkIconSize          icon_size);
GdkPixbuf *gossip_pixbuf_for_presence_state         (GossipPresenceState  state);
GdkPixbuf *gossip_pixbuf_for_presence               (GossipPresence      *presence);
GdkPixbuf *gossip_pixbuf_for_contact                (GossipContact       *contact);
GdkPixbuf *gossip_pixbuf_offline                    (void);
GdkPixbuf *gossip_pixbuf_for_chatroom_status        (GossipChatroom      *chatroom,
						     GtkIconSize          icon_size);
GdkPixbuf *gossip_pixbuf_from_avatar_scaled         (GossipAvatar        *avatar,
						     gint                 width,
						     gint                 height);
GdkPixbuf *gossip_pixbuf_avatar_from_vcard          (GossipVCard         *vcard);
GdkPixbuf *gossip_pixbuf_avatar_from_vcard_scaled   (GossipVCard         *vcard,
						     GtkIconSize          size);
GdkPixbuf *gossip_pixbuf_avatar_from_contact        (GossipContact       *contact);
GdkPixbuf *gossip_pixbuf_avatar_from_contact_scaled (GossipContact       *contact,
						     gint                 width,
						     gint                 height);

/* Text view */
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

/* Windows */
gboolean   gossip_window_get_is_visible             (GtkWindow           *window);
void       gossip_window_present                    (GtkWindow           *window,
						     gboolean             steal_focus);
void       gossip_window_set_default_icon_name      (const gchar         *name);

void       gossip_url_show                          (const char          *url);
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
