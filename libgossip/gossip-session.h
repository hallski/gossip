/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#include "gossip-account.h"
#include "gossip-account-manager.h"
#include "gossip-async.h"
#include "gossip-chatroom-provider.h"
#include "gossip-contact.h"
#include "gossip-ft-provider.h"
#include "gossip-message.h"
#include "gossip-presence.h"
#include "gossip-protocol.h"
#include "gossip-vcard.h"

#define GOSSIP_TYPE_SESSION         (gossip_session_get_type ())
#define GOSSIP_SESSION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_SESSION, GossipSession))
#define GOSSIP_SESSION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_SESSION, GossipSessionClass))
#define GOSSIP_IS_SESSION(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_SESSION))
#define GOSSIP_IS_SESSION_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_SESSION))
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
GossipSession * gossip_session_new                     (GossipAccountManager    *manager);

/* get protocol */
GossipProtocol *gossip_session_get_protocol            (GossipSession           *session,
							GossipAccount           *account);

/* providers */
GossipChatroomProvider *
                gossip_session_get_chatroom_provider   (GossipSession           *session,
							GossipAccount           *account);
GossipFTProvider *
                gossip_session_get_ft_provider         (GossipSession           *session,
							GossipAccount           *account);

/* accounts */
GossipAccountManager *
                gossip_session_get_account_manager     (GossipSession           *session);
GList *         gossip_session_get_accounts            (GossipSession           *session);
gdouble         gossip_session_get_connected_time      (GossipSession           *session,
							GossipAccount           *account);

void            gossip_session_count_accounts          (GossipSession           *session,
							guint                   *connected,
							guint                   *connecting,
							guint                   *disconnected);

gboolean        gossip_session_add_account             (GossipSession           *session,
							GossipAccount           *account);
gboolean        gossip_session_remove_account          (GossipSession           *session,
							GossipAccount           *account);
GossipAccount * gossip_session_find_account            (GossipSession           *session,
							GossipContact           *contact);
void            gossip_session_connect                 (GossipSession           *session,
							GossipAccount           *account,
							gboolean                 startup);
void            gossip_session_disconnect              (GossipSession           *session,
							GossipAccount           *account);
gboolean        gossip_session_is_connected            (GossipSession           *session,
							GossipAccount           *account);
void            gossip_session_send_message            (GossipSession           *session,
							GossipMessage           *message);
void            gossip_session_send_composing          (GossipSession           *session,
							GossipContact           *contact,
							gboolean                 composing);
void            gossip_session_set_presence            (GossipSession           *session,
							GossipPresence          *presence);
gboolean        gossip_session_set_vcard               (GossipSession           *session,
							GossipAccount           *account,
							GossipVCard             *vcard,
							GossipResultCallback     callback,
							gpointer                 user_data,
							GError                 **error);
/* contact management */
GossipContact * gossip_session_find_contact            (GossipSession           *session,
							const gchar             *str);
void            gossip_session_add_contact             (GossipSession           *session,
							GossipAccount           *account,
							const gchar             *id,
							const gchar             *name,
							const gchar             *group,
							const gchar             *message);
void            gossip_session_rename_contact          (GossipSession           *session,
							GossipContact           *contact,
							const gchar             *new_name);
void            gossip_session_remove_contact          (GossipSession           *session,
							GossipContact           *contact);
void            gossip_session_update_contact          (GossipSession           *session,
							GossipContact           *contact);
void            gossip_session_rename_group            (GossipSession           *session,
							const gchar             *group,
							const gchar             *new_name);
GossipPresence *gossip_session_get_presence            (GossipSession           *session);
const GList *   gossip_session_get_contacts            (GossipSession           *session);
GList *         gossip_session_get_contacts_by_account (GossipSession           *session,
							GossipAccount           *account);
GossipContact * gossip_session_get_own_contact         (GossipSession           *session,
							GossipAccount           *account);
const gchar *   gossip_session_get_nickname            (GossipSession           *session,
							GossipAccount           *account);
GList *         gossip_session_get_groups              (GossipSession           *session);

const gchar *   gossip_session_get_active_resource     (GossipSession           *session,
							GossipContact           *contact);
gboolean        gossip_session_get_vcard               (GossipSession           *session,
							GossipAccount           *account,
							GossipContact           *contact,
							GossipVCardCallback      callback,
							gpointer                 user_data,
							GError                 **error);
gboolean        gossip_session_get_version             (GossipSession           *session,
							GossipContact           *contact,
							GossipVersionCallback    callback,
							gpointer                 user_data,
							GError                 **error);
void            gossip_session_register_account        (GossipSession           *session,
							GossipAccount           *account,
							GossipVCard             *vcard,
							GossipRegisterCallback   callback,
							gpointer                 user_data);
void            gossip_session_register_cancel         (GossipSession           *session,
							GossipAccount           *account);

#endif /* __GOSSIP_SESSION_H__ */

