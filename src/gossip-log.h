/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2006 Imendio AB
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

#ifndef __GOSSIP_LOG_H__
#define __GOSSIP_LOG_H__

#include <glib.h>
#include <libgossip/gossip-message.h>

/* Log message handlers */
typedef void (* GossipLogMessageFunc)  (GossipContact  *own_contact,
					GossipMessage  *message,
				        gpointer        user_data);

void           gossip_log_handler_add_for_contact   (GossipContact        *contact,
						     GossipLogMessageFunc  func,
						     gpointer              user_data);
void           gossip_log_handler_add_for_chatroom  (GossipChatroom       *chatroom,
						     GossipLogMessageFunc  func,
						     gpointer              user_data);
void           gossip_log_handler_remove            (GossipLogMessageFunc  func);

/* Utils */
GossipContact *gossip_log_get_own_contact           (GossipAccount        *account);
GList *        gossip_log_get_contacts              (GossipAccount        *account);
GList *        gossip_log_get_chatrooms             (GossipAccount        *account);
gchar *        gossip_log_get_date_readable         (const gchar          *date);

/* Contact functions */
GList *        gossip_log_get_dates_for_contact     (GossipContact        *contact);
GList *        gossip_log_get_messages_for_contact  (GossipContact        *contact,
						     const gchar          *date);
void           gossip_log_message_for_contact       (GossipMessage        *message,
						     gboolean              incoming);
gboolean       gossip_log_exists_for_contact        (GossipContact        *contact);

/* Chatroom functions */
GList *        gossip_log_get_dates_for_chatroom    (GossipChatroom       *chatroom);
GList *        gossip_log_get_messages_for_chatroom (GossipChatroom       *chatroom,
						     const gchar          *date);
void           gossip_log_message_for_chatroom      (GossipChatroom       *chatroom,
						     GossipMessage        *message,
						     gboolean              incoming);
gboolean       gossip_log_exists_for_chatroom       (GossipChatroom       *chatroom);

/* Searching */
 typedef struct _GossipLogSearchHit GossipLogSearchHit;  
GList *        gossip_log_search_new                (const gchar          *text);
void           gossip_log_search_free               (GList                *hits);
GossipAccount *gossip_log_search_hit_get_account    (GossipLogSearchHit   *hit);
GossipContact *gossip_log_search_hit_get_contact    (GossipLogSearchHit   *hit);
const gchar *  gossip_log_search_hit_get_date       (GossipLogSearchHit   *hit);
const gchar *  gossip_log_search_hit_get_filename   (GossipLogSearchHit   *hit);


#ifdef DEPRECATED
void           gossip_log_show_for_contact            (GtkWidget          *window,
						       GossipContact      *contact);
void           gossip_log_show_for_chatroom           (GtkWidget          *window,
						       GossipChatroom     *chatroom);
#endif /* DEPRECATED */

#endif /* __GOSSIP_LOG_H__ */
