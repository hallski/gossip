/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio AB
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

#include "gossip-async.h"
#include "gossip-chatroom-provider.h"
#include "gossip-contact.h"
#include "gossip-message.h"
#include "gossip-presence.h"
#include "gossip-vcard.h"

#define GOSSIP_TYPE_SESSION         (gossip_session_get_type ())
#define GOSSIP_SESSION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), \
 			             GOSSIP_TYPE_SESSION, GossipSession))
#define GOSSIP_SESSION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), \
				     GOSSIP_TYPE_SESSION, \
			             GossipSessionClass))
#define GOSSIP_IS_SESSION(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o),\
			             GOSSIP_TYPE_SESSION))
#define GOSSIP_IS_SESSION_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), \
				     GOSSIP_TYPE_SESSION))
#define GOSSIP_SESSION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),\
				     GOSSIP_TYPE_SESSION, \
				     GossipSessionClass))

typedef struct _GossipSession      GossipSession;
typedef struct _GossipSessionClass GossipSessionClass;

struct _GossipSession {
	GObject parent;
};

struct _GossipSessionClass {
	GObjectClass parent_class;
};

typedef enum {
        GOSSIP_ACCOUNT_TYPE_JABBER
} GossipAccountType;

GType            gossip_session_get_type       (void) G_GNUC_CONST;
GossipSession *  gossip_session_new            (void);
void             gossip_session_connect        (GossipSession  *session);
void             gossip_session_disconnect     (GossipSession  *session);

void             gossip_session_send_message   (GossipSession  *session,
						GossipMessage  *message);

void             gossip_session_send_composing (GossipSession  *session,
						GossipContact  *contact,
						gboolean        composing);

GossipPresence * gossip_session_get_presence   (GossipSession  *session);
void             gossip_session_set_presence   (GossipSession  *session,
						GossipPresence *presence);
gboolean         gossip_session_is_connected   (GossipSession  *session);

const gchar *    gossip_session_get_active_resource (GossipSession *session,
						     GossipContact *contact);
/* Should be a list so that the user can choose which account to join a 
 * group chat in */
GossipChatroomProvider *
gossip_session_get_chatroom_provider           (GossipSession *session);
/* Contact management */
GossipContact *  gossip_session_find_contact   (GossipSession *session,
						const gchar   *str);
/* FIXME: Include the account to add to */
void             gossip_session_add_contact    (GossipSession *session,
                                                const gchar   *id,
                                                const gchar   *name,
                                                const gchar   *group,
                                                const gchar   *message);
void             gossip_session_rename_contact (GossipSession *session,
                                                GossipContact *contact,
                                                const gchar   *new_name);
void             gossip_session_remove_contact (GossipSession *session,
                                                GossipContact *contact);
void             gossip_session_update_contact (GossipSession *session,
                                                GossipContact *contact);

/* Add, remove, move */
const GList *   gossip_session_get_contacts   (GossipSession  *session);
GList *         gossip_session_get_groups     (GossipSession  *session);

const gchar *   gossip_session_get_nickname   (GossipSession  *session);

/* Async operations */
gboolean        gossip_session_async_register  (GossipSession  *session,
                                                GossipAccountType type,
                                                const gchar    *id,
                                                const gchar    *password,
                                                gboolean        use_ssl,
                                                GossipAsyncRegisterCallback callback,
                                                gpointer        user_data,
                                                GError        **error);
                                                
gboolean        gossip_session_async_get_vcard (GossipSession  *session,
						GossipContact  *contact,
						GossipAsyncVCardCallback callback,
						gpointer        user_data,
						GError         **error);
gboolean        gossip_session_async_set_vcard (GossipSession  *session,
						GossipVCard    *vcard,
						GossipAsyncResultCallback callback,
						gpointer        user_data,
						GError         **error);
gboolean        gossip_session_async_get_version (GossipSession *session,
						  GossipContact *contact,
						  GossipAsyncVersionCallback callback,
						  gpointer                   user_data,
						  GError       **error);

#endif /* __GOSSIP_SESSION_H__ */

