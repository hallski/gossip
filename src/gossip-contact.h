/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio HB
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

#include <glib.h>

#include "gossip-presence.h"

#define GOSSIP_CONTACT(x) (GossipContact *) x

typedef struct _GossipContact GossipContact;

typedef enum {
	GOSSIP_CONTACT_TYPE_TEMPORARY,
	GOSSIP_CONTACT_TYPE_CONTACTLIST,
	GOSSIP_CONTACT_TYPE_USER
} GossipContactType;

typedef enum {
	GOSSIP_CONTACT_PROTOCOL_INVALID,
	GOSSIP_CONTACT_PROTOCOL_JABBER,
	GOSSIP_CONTACT_PROTOCOL_ICQ,
	GOSSIP_CONTACT_PROTOCOL_MSN,
	GOSSIP_CONTACT_PROTOCOL_YAHOO
} GossipContactProtocol;

GossipContact *    gossip_contact_new            (GossipContactType type);
GossipContactType  gossip_contact_get_type       (GossipContact    *contact);

void               gossip_contact_set_id         (GossipContact    *contact,
						  const gchar      *id);
const gchar *      gossip_contact_get_id         (GossipContact    *contact);

void               gossip_contact_set_name       (GossipContact    *contact,
						  const gchar      *name);
const gchar *      gossip_contact_get_name       (GossipContact    *contact);

void               gossip_contact_set_presence   (GossipContact    *contact,
						  GossipPresence   *presence);
GossipPresence *   gossip_contact_get_presence   (GossipContact    *contact);

gboolean           gossip_contact_is_online      (GossipContact    *contact);

gboolean           gossip_contact_set_categories (GossipContact    *contact,
						  GSList           *categories);
GSList *           gossip_contact_get_categories (GossipContact    *contact);
						
GossipContact *    gossip_contact_ref            (GossipContact    *contact);
void               gossip_contact_unref          (GossipContact    *contact);

gint               gossip_contact_compare        (gconstpointer    *a,
						  gconstpointer    *b);
gboolean           gossip_contact_equal          (gconstpointer     v1,
						  gconstpointer     v2);
guint              gossip_contact_hash           (gconstpointer     key);

#endif /* __GOSSIP_CONTACT_H__ */

