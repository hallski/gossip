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

#include "gossip-utils.h"
#include "gossip-contact.h"

struct _GossipContact {
	GossipContactType  type;

	gchar             *name;

	GossipJID         *jid;

	GossipPresence    *presence;
	GList             *groups;
	
	guint ref_count;
};

static void contact_free (GossipContact *contact);

static void
contact_free (GossipContact *contact)
{
	g_free (contact->name);

	if (contact->jid) {
		gossip_jid_unref (contact->jid);
	}

	if (contact->presence) {
		gossip_presence_unref (contact->presence);
	}

	if (contact->groups) {
		GList *l;

		for (l = contact->groups; l; l = l->next) {
			g_free (l->data);
		}

		g_list_free (contact->groups);
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
	contact->jid = NULL;
	contact->presence = gossip_presence_new (GOSSIP_PRESENCE_STATE_OFFLINE);

	return contact;
}

GossipContact *
gossip_contact_new_full (GossipContactType  type,
			 GossipJID         *jid,
			 const gchar       *name)
{
	GossipContact *contact;

	contact = gossip_contact_new (type);

	gossip_contact_set_jid (contact, jid);
	gossip_contact_set_name (contact, name);

	return contact;
}

GossipContactType
gossip_contact_get_type (GossipContact *contact)
{
	g_return_val_if_fail (contact != NULL, GOSSIP_CONTACT_TYPE_TEMPORARY);

	return contact->type;
}

void
gossip_contact_set_jid (GossipContact *contact, GossipJID *jid)
{
	g_return_if_fail (contact != NULL);
	g_return_if_fail (jid != NULL);

	if (contact->jid) {
		gossip_jid_unref (contact->jid);
	}

	contact->jid = gossip_jid_ref (jid);
}

GossipJID *
gossip_contact_get_jid (GossipContact *contact)
{
	g_return_val_if_fail (contact != NULL, NULL);
	
	return contact->jid;
}

const gchar *
gossip_contact_get_name (GossipContact *contact)
{
	g_return_val_if_fail (contact != NULL, NULL);

	if (contact->name == NULL) {
		return gossip_jid_get_without_resource (contact->jid);
	}

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

GList *
gossip_contact_get_groups (GossipContact *contact)
{
	/* FIXME: Implement */
	return contact->groups;
}

gboolean
gossip_contact_set_groups (GossipContact *contact, GList *groups)
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
gossip_contact_compare (gconstpointer a, gconstpointer b)
{
	GossipJID *jid_a;
	GossipJID *jid_b;
	
	jid_a = gossip_contact_get_jid (GOSSIP_CONTACT (a));
	jid_b = gossip_contact_get_jid (GOSSIP_CONTACT (b));

	return gossip_jid_case_compare (jid_a, jid_b);
}

gint
gossip_contact_name_compare (gconstpointer a, gconstpointer b)
{
	g_return_val_if_fail (a != NULL, 0);
	g_return_val_if_fail (b != NULL, 0);

	g_print ("COMPARE: %s vs. %s\n",
		 gossip_contact_get_name (GOSSIP_CONTACT (a)),
		 gossip_contact_get_name (GOSSIP_CONTACT (b)));

	return strcmp (gossip_contact_get_name (GOSSIP_CONTACT (a)),
		       gossip_contact_get_name (GOSSIP_CONTACT (b)));
}

gint           
gossip_contact_name_case_compare (gconstpointer a, gconstpointer b)
{
	g_return_val_if_fail (a != NULL, 0);
	g_return_val_if_fail (b != NULL, 0);

	return gossip_contact_name_case_n_compare (a, b, -1);
}

gint           
gossip_contact_name_case_n_compare (gconstpointer a, gconstpointer b, gsize n)
{
	const gchar *name_a, *name_b;
	
	g_return_val_if_fail (a != NULL, 0);
	g_return_val_if_fail (b != NULL, 0);

	name_a = gossip_contact_get_name (GOSSIP_CONTACT (a));
	name_b = gossip_contact_get_name (GOSSIP_CONTACT (b));
					  
	return gossip_utils_str_n_case_cmp (name_a, name_b, -1);
}

gboolean
gossip_contact_equal (gconstpointer v1, gconstpointer v2)
{
	GossipJID *jid_a;
	GossipJID *jid_b;
	
	jid_a = gossip_contact_get_jid (GOSSIP_CONTACT (v1));
	jid_b = gossip_contact_get_jid (GOSSIP_CONTACT (v2));

	return gossip_jid_equals_without_resource (jid_a, jid_b);
}

guint
gossip_contact_hash (gconstpointer key)
{
	return gossip_jid_hash (gossip_contact_get_jid (GOSSIP_CONTACT (key)));
}

