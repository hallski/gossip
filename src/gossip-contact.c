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

#include <config.h>

#include <string.h>

#include "gossip-contact.h"

struct _GossipContact {
	GossipContactType  type;

	gchar             *name;
	gchar             *id;

	GossipPresence    *presence;
	GSList            *categories;
	
	guint ref_count;
};

static void contact_free (GossipContact *contact);

static void
contact_free (GossipContact *contact)
{
	g_free (contact->name);
	g_free (contact->id);

	if (contact->presence) {
		gossip_presence_unref (contact->presence);
	}

	if (contact->categories) {
		GSList *l;

		for (l = contact->categories; l; l = l->next) {
			g_free (l->data);
		}

		g_slist_free (contact->categories);
	}
		
	g_free (contact);
}

GossipContact *
gossip_contact_new (GossipContactType type)
{
	GossipContact *contact;

	contact = g_new0 (GossipContact, 1);
	contact->ref_count = 1;
	contact->type = type;
	contact->name = NULL;
	contact->id = NULL;
	contact->presence = gossip_presence_new ();

	return contact;
}

GossipContactType
gossip_contact_get_type (GossipContact *contact)
{
	g_return_val_if_fail (contact != NULL, GOSSIP_CONTACT_TYPE_TEMPORARY);

	return contact->type;
}

void
gossip_contact_set_id (GossipContact *contact, const gchar *id)
{
	g_return_if_fail (contact != NULL);
	g_return_if_fail (id != NULL);

	g_free (contact->id);
	contact->id = g_strdup (id);
}

const gchar *
gossip_contact_get_id (GossipContact *contact)
{
	g_return_val_if_fail (contact != NULL, NULL);
	
	return contact->id;
}

const gchar *
gossip_contact_get_name (GossipContact *contact)
{
	g_return_val_if_fail (contact != NULL, NULL);
	
	return contact->name;
}

void
gossip_contact_set_name (GossipContact *contact, const gchar *name)
{
	g_return_if_fail (contact != NULL);
	g_return_if_fail (name != NULL);

	g_free (contact->name);
	contact->name = g_strdup (name);
}

GossipPresence *
gossip_contact_get_presence (GossipContact *contact)
{
	g_return_val_if_fail (contact != NULL, NULL);
	
	return contact->presence;
}
void
gossip_contact_set_presence (GossipContact *contact, GossipPresence *presence)
{
	g_return_if_fail (contact != NULL);
	g_return_if_fail (presence != NULL);

	if (contact->presence) {
		gossip_presence_unref (contact->presence);
	}

	contact->presence = gossip_presence_ref (presence);
}

gboolean
gossip_contact_is_online (GossipContact *contact)
{
	g_return_val_if_fail (contact != NULL, FALSE);

	if (gossip_presence_get_state (contact->presence) == GOSSIP_PRESENCE_STATE_OFFLINE) {
		return FALSE;
	}
	
	return TRUE;
}

GSList *
gossip_contact_get_categories (GossipContact *contact)
{
	/* FIXME: Implement */
	return contact->categories;
}

gboolean
gossip_contact_set_categories (GossipContact *contact, GSList *categories)
{
	/* FIXME: Implement */
	return FALSE;
}

GossipContact * 
gossip_contact_ref (GossipContact *contact)
{
	g_return_val_if_fail (contact != NULL, NULL);

	contact->ref_count++;

	return contact;
}


void
gossip_contact_unref (GossipContact *contact)
{
	g_return_if_fail (contact != NULL);

	contact->ref_count--;
	if (contact->ref_count > 0) {
		return;
	}
	
	contact_free (contact);
}

gint
gossip_contact_compare (gconstpointer *a, gconstpointer *b)
{
	g_return_val_if_fail (a != NULL, 0);
	g_return_val_if_fail (b != NULL, 0);

	return strcmp (gossip_contact_get_id (GOSSIP_CONTACT (a)),
		       gossip_contact_get_id (GOSSIP_CONTACT (b)));
}

gboolean
gossip_contact_equal (gconstpointer v1, gconstpointer v2)
{
	const gchar *id_a, *id_b;

	id_a = gossip_contact_get_id (GOSSIP_CONTACT (v1));
	id_b = gossip_contact_get_id (GOSSIP_CONTACT (v2));

	if (!id_a || !id_b) {
		return FALSE;
	}

	if (strcmp (id_a, id_b) == 0)
		return TRUE;

	return FALSE;
}

guint
gossip_contact_hash (gconstpointer key)
{
	return g_str_hash (gossip_contact_get_id (GOSSIP_CONTACT (key)));
}

