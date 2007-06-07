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

#include <libgossip/gossip-protocol.h>

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
	GossipProtocol parent;
};

struct _GossipJabberClass {
	GossipProtocolClass parent_class;
};

void
gossip_jabber_setup (GossipProtocol *protocol, GossipAccount *account);

GQuark         gossip_jabber_error_quark               (void) G_GNUC_CONST;

GType          gossip_jabber_get_type                  (void) G_GNUC_CONST;
void           gossip_jabber_login                     (GossipProtocol *protocol);
void           gossip_jabber_logout                    (GossipProtocol *protocol);
gboolean       gossip_jabber_get_vcard                 (GossipJabber        *jabber,
							GossipContact       *contact,
							GossipVCardCallback  callback);
GossipAccount *gossip_jabber_get_account               (GossipJabber        *jabber);

GossipContact *gossip_jabber_get_own_contact           (GossipJabber        *jabber);
GossipContact *gossip_jabber_get_contact_from_jid      (GossipJabber        *jabber,
							const gchar         *jid,
							gboolean            *new_item,
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

G_END_DECLS

#endif /* __GOSSIP_JABBER_H__ */
