/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Imendio AB
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

#include "config.h"

#include <libgossip/gossip-vcard.h>

#include "gossip-ui-utils.h"
#include "gossip-email.h"

gboolean
gossip_email_available (GossipContact *contact)
{
	GossipVCard *vcard;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);

	vcard = gossip_contact_get_vcard (contact);

	if (!vcard) {
		g_print ("No vcard\n");
	}

	if (vcard && gossip_vcard_get_email (vcard)) {
		return TRUE;
	}

	return FALSE;
}

void
gossip_email_open (GossipContact *contact)
{
	GossipVCard *vcard;
	gchar       *url;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	vcard = gossip_contact_get_vcard (contact);
	if (!vcard) {
		g_warning ("Failed to email contact as it had no vcard");
	}

	url = g_strdup_printf ("mailto:%s", gossip_vcard_get_email (vcard));

	gossip_url_show (url);
	
	g_free (url);
}

