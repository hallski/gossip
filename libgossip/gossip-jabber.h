/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
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

#ifndef __GOSSIP_JABBER_H__
#define __GOSSIP_JABBER_H__

#include <glib-object.h>

#include "gossip-async.h"
#include "gossip-message.h"

G_BEGIN_DECLS

#define GOSSIP_TYPE_JABBER         (gossip_jabber_get_type ())
#define GOSSIP_JABBER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_JABBER, GossipJabber))
#define GOSSIP_JABBER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_JABBER, GossipJabberClass))
#define GOSSIP_IS_JABBER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_JABBER))
#define GOSSIP_IS_JABBER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_JABBER))
#define GOSSIP_JABBER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_JABBER, GossipJabberClass))

#define GOSSIP_JABBER_ERROR        gnome_jabber_error_quark()

typedef struct _GossipJabber      GossipJabber;
typedef struct _GossipJabberClass GossipJabberClass;
typedef struct _GossipJabberPriv  GossipJabberPriv;

struct _GossipJabber {
	GObject parent;
};

struct _GossipJabberClass {
	GObjectClass parent_class;
};

typedef enum {
	GOSSIP_JABBER_DISCONNECT_ASKED,
	GOSSIP_JABBER_DISCONNECT_ERROR
} GossipJabberDisconnectReason;

GType          gossip_jabber_get_type                  (void) G_GNUC_CONST;

GQuark         gossip_jabber_error_quark               (void) G_GNUC_CONST;

GossipJabber * gossip_jabber_new                       (gpointer session);

void           gossip_jabber_setup                     (GossipJabber        *jabber,
							GossipAccount       *account);
void           gossip_jabber_login                     (GossipJabber        *jabber);
void           gossip_jabber_logout                    (GossipJabber        *jabber);
GossipAccount *gossip_jabber_get_account               (GossipJabber        *jabber);

GossipContact *gossip_jabber_get_own_contact           (GossipJabber        *jabber);
GossipContact *gossip_jabber_get_contact_from_jid      (GossipJabber        *jabber,
							const gchar         *jid,
							gboolean             own_contact,
							gboolean             set_permanent,
							gboolean             get_vcard);
void           gossip_jabber_send_presence             (GossipJabber        *jabber,
							GossipPresence      *presence);
void           gossip_jabber_send_subscribed           (GossipJabber        *jabber,
							GossipContact       *contact);
void           gossip_jabber_send_unsubscribed         (GossipJabber        *jabber,
							GossipContact       *contact);
void           gossip_jabber_subscription_allow_all    (GossipJabber        *jabber);
void           gossip_jabber_subscription_disallow_all (GossipJabber        *jabber);
GossipAccount *gossip_jabber_new_account               (void);
gchar *        gossip_jabber_get_default_server        (const gchar    *username);
guint          gossip_jabber_get_default_port          (gboolean        use_ssl);
gboolean       gossip_jabber_is_ssl_supported          (void);
gboolean       gossip_jabber_is_connected              (GossipJabber        *jabber);
gboolean       gossip_jabber_is_connecting             (GossipJabber        *jabber);
void           gossip_jabber_send_message              (GossipJabber        *jabber,
							GossipMessage       *message);
void           gossip_jabber_send_composing            (GossipJabber        *jabber,
							GossipContact       *contact,
							gboolean             typing);
void           gossip_jabber_set_presence              (GossipJabber        *jabber,
							GossipPresence      *presence);
void           gossip_jabber_set_subscription          (GossipJabber        *jabber,
							GossipContact       *contact,
							gboolean             subscribed);
gboolean       gossip_jabber_set_vcard                 (GossipJabber        *jabber,
							GossipVCard         *vcard,
							GossipCallback       callback,
							gpointer             user_data,
							GError             **error);
void            gossip_jabber_add_contact              (GossipJabber        *jabber,
							const gchar         *id,
							const gchar         *name,
							const gchar         *group,
							const gchar         *message);
void            gossip_jabber_rename_contact           (GossipJabber        *jabber,
							GossipContact       *contact,
							const gchar         *new_name);
void            gossip_jabber_remove_contact           (GossipJabber        *jabber,
							GossipContact       *contact);
void            gossip_jabber_update_contact           (GossipJabber        *jabber,
							GossipContact       *contact);
void            gossip_jabber_rename_group             (GossipJabber        *jabber,
							const gchar         *group,
							const gchar         *new_name);
const gchar *   gossip_jabber_get_active_resource      (GossipJabber        *jabber,
							GossipContact       *contact);
GList *         gossip_jabber_get_groups               (GossipJabber        *jabber);
gboolean        gossip_jabber_get_vcard                (GossipJabber        *jabber,
							GossipContact       *contact,
							GossipVCardCallback  callback,
							gpointer             user_data,
							GError             **error);
gboolean        gossip_jabber_get_version              (GossipJabber        *jabber,
							GossipContact          *contact,
							GossipVersionCallback   callback,
							gpointer                user_data,
							GError                **error);
void            gossip_jabber_change_password          (GossipJabber        *jabber,
							const gchar         *new_password,
							GossipErrorCallback  callback,
							gpointer             user_data);
void            gossip_jabber_change_password_cancel   (GossipJabber        *jabber);
void            gossip_jabber_get_avatar_requirements  (GossipJabber        *jabber,
							guint               *min_width,
							guint               *min_height,
							guint               *max_width,
							guint               *max_height,
							gsize               *max_size,
							gchar             **format);

G_END_DECLS

#endif /* __GOSSIP_JABBER_H__ */
