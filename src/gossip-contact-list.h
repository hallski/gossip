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

#include <glib-object.h>

#include "gossip-contact.h"

#ifndef __GOSSIP_CONTACT_LIST_H__
#define __GOSSIP_CONTACT_LIST_H__

#define GOSSIP_TYPE_CONTACT_LIST         (gossip_contact_list_get_type ())
#define GOSSIP_CONTACT_LIST(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CONTACT_LIST, GossipContactList))
#define GOSSIP_CONTACT_LIST_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_CONTACT_LIST, GossipContactListClass))
#define GOSSIP_IS_CONTACT_LIST(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CONTACT_LIST))
#define GOSSIP_IS_CONTACT_LIST_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CONTACT_LIST))
#define GOSSIP_CONTACT_LIST_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CONTACT_LIST, GossipContactListClass))

typedef struct _GossipContactList      GossipContactList;    /* Dummy typedef */
typedef struct _GossipContactListClass GossipContactListClass;

struct _GossipContactListClass {
	GTypeInterface   base_iface;

	/* signals */
	void (*contact_added)             (GossipContactList  *list,
					   GossipContact      *contact);
	void (*contact_removed)           (GossipContactList  *list,
					   GossipContact      *contact);
	void (*contact_updated)           (GossipContactList  *list,
					   GossipContact      *contact);
	void (*contact_presence_updated)  (GossipContactList  *list,
					   GossipContact      *contact);

	/* vtable */
	
	GList * (*get_contacts)           (GossipContactList  *list);
};

GType   gossip_contact_list_get_type     (void) G_GNUC_CONST;

GList * gossip_contact_list_get_contacts (GossipContactList *list);

#endif /* __GOSSIP_CONTACT_LIST_H__ */
