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

typedef struct _GossipContact GossipContact;

typedef enum {
	GOSSIP_CONTACT_TYPE_INVALID,
	GOSSIP_CONTACT_TYPE_JABBER
} GossipContactType;

GossipContact *    gossip_contact_new      (void);

GossipContactType  gossip_contact_get_type (GossipContact *contact);

GossipContact *    gossip_contact_ref      (GossipContact *contact);
void               gossip_contact_unref    (GossipContact *contact);

#endif /* __GOSSIP_CONTACT_H__ */

