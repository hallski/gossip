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

#ifndef __GOSSIP_CONTACT_H__
#define __GOSSIP_CONTACT_H__

#include <glib-object.h>

#include "gossip-jid.h"
#include "gossip-presence.h"

#define GOSSIP_TYPE_CONTACT         (gossip_contact_get_gtype ())
#define GOSSIP_CONTACT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CONTACT, GossipContact))
#define GOSSIP_CONTACT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_CONTACT, GossipContactClass))
#define GOSSIP_IS_CONTACT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CONTACT))
#define GOSSIP_IS_CONTACT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CONTACT))
#define GOSSIP_CONTACT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CONTACT, GossipContactClass))

typedef struct _GossipContact      GossipContact;
typedef struct _GossipContactClass GossipContactClass;

struct _GossipContact {
	GObject parent;
};

struct _GossipContactClass {
	GObjectClass parent_class;
};

typedef enum {
	GOSSIP_CONTACT_TYPE_TEMPORARY,
	GOSSIP_CONTACT_TYPE_CONTACTLIST,
	GOSSIP_CONTACT_TYPE_CHATROOM,
	GOSSIP_CONTACT_TYPE_USER /* Wonder what I had in mind for this one */
} GossipContactType;

typedef enum {
	GOSSIP_SUBSCRIPTION_NONE,
	GOSSIP_SUBSCRIPTION_TO,
	GOSSIP_SUBSCRIPTION_FROM,
	GOSSIP_SUBSCRIPTION_BOTH
} GossipSubscription;


GType              gossip_contact_get_gtype      (void) G_GNUC_CONST;

GossipContact *    gossip_contact_new            (GossipContactType type);
GossipContact *    gossip_contact_new_full       (GossipContactType type,
						  const gchar      *id,
						  const gchar      *name);
GossipContactType  gossip_contact_get_type       (GossipContact    *contact);

const gchar *      gossip_contact_get_id         (GossipContact    *contact);
void               gossip_contact_set_id         (GossipContact    *contact,
						  const gchar      *id);
void               gossip_contact_set_name       (GossipContact    *contact,
						  const gchar      *name);
const gchar *      gossip_contact_get_name       (GossipContact    *contact);

void               gossip_contact_set_presence   (GossipContact    *contact,
						  GossipPresence   *presence);
GossipPresence *   gossip_contact_get_presence   (GossipContact    *contact);

gboolean           gossip_contact_is_online      (GossipContact    *contact);

gboolean           gossip_contact_set_groups     (GossipContact    *contact,
						  GList            *categories);
GList *            gossip_contact_get_groups     (GossipContact    *contact);

GossipSubscription gossip_contact_get_subscription (GossipContact  *contact);
void               gossip_contact_set_subscription (GossipContact  *contact,
						    GossipSubscription subscription);
						
gint               gossip_contact_compare        (gconstpointer     a,
						  gconstpointer     b);
gint          gossip_contact_name_compare        (gconstpointer     a,
						  gconstpointer     b);
gint          gossip_contact_name_case_compare   (gconstpointer     a,
						  gconstpointer     b);
gint          gossip_contact_name_case_n_compare (gconstpointer     a,
						  gconstpointer     b,
						  gsize             n);
gboolean           gossip_contact_equal          (gconstpointer     v1,
						  gconstpointer     v2);
guint              gossip_contact_hash           (gconstpointer     key);

/* -- Convenience functions -- */
GdkPixbuf *        gossip_contact_get_pixbuf     (GossipContact    *contact);
const gchar *      gossip_contact_get_status     (GossipContact    *contact);

#endif /* __GOSSIP_CONTACT_H__ */

