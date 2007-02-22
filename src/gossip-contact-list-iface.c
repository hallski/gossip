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
 *
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include "gossip-contact-list-iface.h"
#include "gossip-marshal.h"

static void contact_list_base_init (gpointer klass);

GType
gossip_contact_list_iface_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo type_info = {
			sizeof (GossipContactListIfaceClass),
			contact_list_base_init,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "GossipContactListIface",
					       &type_info, 0);
	}

	return type;
}

static void
contact_list_base_init (gpointer klass)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		g_signal_new ("contact_added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GossipContactListIfaceClass,
					       contact_added),
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

		g_signal_new ("contact_removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GossipContactListIfaceClass,
					       contact_removed),
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

		initialized = TRUE;
	}
}

GList *
gossip_contact_list_iface_get_contacts (GossipContactListIface *list)
{
	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST (list), NULL);

	if (GOSSIP_CONTACT_LIST_IFACE_GET_CLASS (list)->get_contacts) {
		return GOSSIP_CONTACT_LIST_IFACE_GET_CLASS (list)->get_contacts (list);
	}

	return NULL;
}
