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

#include "gossip-contact.h"

struct _GossipContact {
	GossipContactType  type;
	
	guint ref_count;
};

static void contact_free (GossipContact *contact);

static void
contact_free (GossipContact *contact)
{

	g_free (contact);
}

GossipContact *
gossip_contact_new ()
{
	GossipContact *contact;

	contact = g_new0 (GossipContact, 1);
	contact->ref_count = 1;

	return contact;
}

GossipContactType
gossip_contact_get_type (GossipContact *contact)
{
	g_return_val_if_fail (contact != NULL, GOSSIP_CONTACT_TYPE_INVALID);

	return contact->type;
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
	if (contact->ref_count <= 0) {
		contact_free (contact);
	}
}

