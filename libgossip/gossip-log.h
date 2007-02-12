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

#include <glib-object.h>

G_BEGIN_DECLS


#define GOSSIP_TYPE_LOG_MANAGER         (gossip_log_manager_get_type ())
#define GOSSIP_LOG_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_LOG_MANAGER, GossipLogManager))
#define GOSSIP_LOG_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_LOG_MANAGER, GossipLogManagerClass))
#define GOSSIP_IS_LOG_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_LOG_MANAGER))
#define GOSSIP_IS_LOG_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_LOG_MANAGER))
#define GOSSIP_LOG_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_LOG_MANAGER, GossipLogManagerClass))

typedef struct _GossipLogManagerClass GossipLogManagerClass;

struct _GossipLogManager {
	GObject parent;
};

struct _GossipLogManagerClass {
	GObjectClass parent_class;
};

GType             gossip_log_manager_get_type          (void) G_GNUC_CONST;
GossipLogManager *gossip_log_manager_new               (GossipSession         *session,
							GossipAccountManager  *account_manager,
							GossipContactManager  *contact_manager,
							GossipChatroomManager *chatroom_manager);


/* Log message handlers */
void              gossip_log_handler_add_for_contact   (GossipLogManager      *manager,
							GossipContact         *contact,
							GossipLogMessageFunc   func,
							gpointer               user_data);
void              gossip_log_handler_add_for_chatroom  (GossipLogManager      *manager,
							GossipChatroom        *chatroom,
							GossipLogMessageFunc   func,
							gpointer               user_data);
void              gossip_log_handler_remove            (GossipLogManager      *manager,
							GossipLogMessageFunc   func);


/* Utils */
GossipContact *   gossip_log_get_own_contact           (GossipLogManager      *manager,
							GossipAccount         *account);
GList *           gossip_log_get_contacts              (GossipLogManager      *manager,
							GossipAccount         *account);
GList *           gossip_log_get_chatrooms             (GossipLogManager      *manager,
							GossipAccount         *account);
gchar *           gossip_log_get_date_readable         (const gchar           *date);


/* Contact functions */
GList *           gossip_log_get_dates_for_contact     (GossipContact         *contact);
GList *           gossip_log_get_messages_for_contact  (GossipLogManager      *manager,
							GossipContact         *contact,
							const gchar           *date);
void              gossip_log_message_for_contact       (GossipLogManager      *manager,
							GossipMessage         *message,
							gboolean               incoming);
gboolean          gossip_log_exists_for_contact        (GossipContact         *contact);
GList *           gossip_log_get_last_for_contact      (GossipLogManager      *manager,
							GossipContact         *contact);


/* Chatroom functions */
GList *           gossip_log_get_dates_for_chatroom    (GossipChatroom        *chatroom);
GList *           gossip_log_get_messages_for_chatroom (GossipLogManager      *manager,
							GossipChatroom        *chatroom,
							const gchar           *date);
void              gossip_log_message_for_chatroom      (GossipLogManager      *manager,
							GossipChatroom        *chatroom,
							GossipMessage         *message,
							gboolean               incoming);
gboolean          gossip_log_exists_for_chatroom       (GossipChatroom        *chatroom);


/* Searching */
GList *           gossip_log_search_new                (GossipLogManager      *manager,
							const gchar           *text);
void              gossip_log_search_free               (GList                 *hits);
GossipAccount *   gossip_log_search_hit_get_account    (GossipLogSearchHit    *hit);
GossipContact *   gossip_log_search_hit_get_contact    (GossipLogSearchHit    *hit);
const gchar *     gossip_log_search_hit_get_date       (GossipLogSearchHit    *hit);
const gchar *     gossip_log_search_hit_get_filename   (GossipLogSearchHit    *hit);

G_END_DECLS

#endif /* __GOSSIP_LOG_H__ */
