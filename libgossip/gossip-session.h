/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2004-2006 Imendio AB
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

#ifndef __GOSSIP_SESSION_H__
#define __GOSSIP_SESSION_H__

#include <glib-object.h>

#include "gossip-presence.h"
#include "gossip-account.h"
#include "gossip-contact.h"
#include "gossip-jabber.h"
#include "gossip-log.h"
#include "gossip-async.h"
#include "gossip-chatroom-provider.h"
#include "gossip-account-manager.h"
#include "gossip-message.h"
#include "gossip-vcard.h"
#include "gossip-contact-manager.h"
#include "gossip-chatroom-manager.h"
#include "gossip-ft-provider.h"

G_BEGIN_DECLS

#define GOSSIP_TYPE_SESSION         (gossip_session_get_type ())
#define GOSSIP_SESSION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_SESSION, GossipSession))
#define GOSSIP_SESSION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_SESSION, GossipSessionClass))
#define GOSSIP_IS_SESSION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_SESSION))
#define GOSSIP_IS_SESSION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_SESSION))
#define GOSSIP_SESSION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_SESSION, GossipSessionClass))

typedef struct _GossipSession      GossipSession;
typedef struct _GossipSessionClass GossipSessionClass;

struct _GossipSession {
    GObject parent;
};

struct _GossipSessionClass {
    GObjectClass parent_class;
};

GType           gossip_session_get_type                (void) G_GNUC_CONST;

GossipSession * gossip_session_new                     (const gchar             *accounts_file,
                                                        const gchar             *contacts_file,
                                                        const gchar             *chatrooms_file);

/* Get protocol */
GossipJabber *  gossip_session_get_protocol            (GossipSession           *session,
                                                        GossipAccount           *account);

/* Providers */
GossipChatroomProvider *
gossip_session_get_chatroom_provider   (GossipSession          *session,
                                        GossipAccount          *account);
GossipFTProvider *
gossip_session_get_ft_provider         (GossipSession          *session,
                                        GossipAccount          *account);

/* Accounts */
GossipAccountManager *
gossip_session_get_account_manager     (GossipSession          *session);
GossipContactManager *
gossip_session_get_contact_manager     (GossipSession          *session);
GossipChatroomManager *
gossip_session_get_chatroom_manager    (GossipSession          *session);
GossipLogManager *
gossip_session_get_log_manager         (GossipSession          *session);
GList *         gossip_session_get_accounts            (GossipSession          *session);
gdouble         gossip_session_get_connected_time      (GossipSession          *session,
                                                        GossipAccount          *account);

void            gossip_session_count_accounts          (GossipSession          *session,
                                                        guint                  *connected,
                                                        guint                  *connecting,
                                                        guint                  *disconnected);
GossipAccount * gossip_session_new_account             (GossipSession          *session);
gboolean        gossip_session_add_account             (GossipSession          *session,
                                                        GossipAccount          *account);
gboolean        gossip_session_remove_account          (GossipSession          *session,
                                                        GossipAccount          *account);
GossipAccount * gossip_session_find_account_for_own_contact 
(GossipSession          *session,
 GossipContact          *own_contact);
void            gossip_session_connect                 (GossipSession          *session,
                                                        GossipAccount          *account,
                                                        gboolean                startup);
void            gossip_session_disconnect              (GossipSession          *session,
                                                        GossipAccount          *account);
gboolean        gossip_session_is_connected            (GossipSession          *session,
                                                        GossipAccount          *account);
gboolean        gossip_session_is_connecting           (GossipSession          *session,
                                                        GossipAccount          *account);
void            gossip_session_send_message            (GossipSession          *session,
                                                        GossipMessage          *message);
void            gossip_session_send_composing          (GossipSession          *session,
                                                        GossipContact          *contact,
                                                        gboolean                composing);
void            gossip_session_set_presence            (GossipSession          *session,
                                                        GossipPresence         *presence);
gboolean        gossip_session_set_vcard               (GossipSession          *session,
                                                        GossipAccount          *account,
                                                        GossipVCard            *vcard,
                                                        GossipCallback          callback,
                                                        gpointer                user_data,
                                                        GError                **error);
void            gossip_session_get_avatar_requirements (GossipSession          *session,
                                                        GossipAccount          *account,
                                                        guint                  *min_width,
                                                        guint                  *min_height,
                                                        guint                  *max_width,
                                                        guint                  *max_height,
                                                        gsize                  *max_size,
                                                        gchar                 **format);
/* Contact management */
void            gossip_session_add_contact             (GossipSession          *session,
                                                        GossipAccount          *account,
                                                        const gchar            *id,
                                                        const gchar            *name,
                                                        const gchar            *group,
                                                        const gchar            *message);
void            gossip_session_rename_contact          (GossipSession          *session,
                                                        GossipContact          *contact,
                                                        const gchar            *new_name);
void            gossip_session_remove_contact          (GossipSession          *session,
                                                        GossipContact          *contact);
void            gossip_session_update_contact          (GossipSession          *session,
                                                        GossipContact          *contact);
void            gossip_session_rename_group            (GossipSession          *session,
                                                        const gchar            *group,
                                                        const gchar            *new_name);
GossipPresence *gossip_session_get_presence            (GossipSession          *session);
const GList *   gossip_session_get_contacts            (GossipSession          *session);
GList *         gossip_session_get_contacts_by_account (GossipSession          *session,
                                                        GossipAccount          *account);
GossipContact * gossip_session_get_own_contact         (GossipSession          *session,
                                                        GossipAccount          *account);
const gchar *   gossip_session_get_nickname            (GossipSession          *session,
                                                        GossipAccount          *account);
GList *         gossip_session_get_groups              (GossipSession          *session);
const gchar *   gossip_session_get_active_resource     (GossipSession          *session,
                                                        GossipContact          *contact);
gboolean        gossip_session_get_vcard               (GossipSession          *session,
                                                        GossipAccount          *account,
                                                        GossipContact          *contact,
                                                        GossipVCardCallback     callback,
                                                        gpointer                user_data,
                                                        GError                **error);
gboolean        gossip_session_get_version             (GossipSession          *session,
                                                        GossipContact          *contact,
                                                        GossipVersionCallback   callback,
                                                        gpointer                user_data,
                                                        GError                **error);
void            gossip_session_register_account        (GossipSession          *session,
                                                        GossipAccount          *account,
                                                        GossipVCard            *vcard,
                                                        GossipErrorCallback     callback,
                                                        gpointer                user_data);
void            gossip_session_register_cancel         (GossipSession          *session,
                                                        GossipAccount          *account);
void            gossip_session_change_password         (GossipSession          *session,
                                                        GossipAccount          *account,
                                                        const gchar            *new_password,
                                                        GossipErrorCallback     callback,
                                                        gpointer                user_data);
void            gossip_session_change_password_cancel  (GossipSession          *session,
                                                        GossipAccount          *account);
void            gossip_session_chatroom_join_favorites (GossipSession          *session);

G_END_DECLS

#endif /* __GOSSIP_SESSION_H__ */

