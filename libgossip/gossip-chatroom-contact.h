/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
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

#ifndef __GOSSIP_CHATROOM_CONTACT_H__
#define __GOSSIP_CHATROOM_CONTACT_H__

#include <glib-object.h>

#include "gossip-contact.h"
#include "gossip-chatroom.h"

#define GOSSIP_TYPE_CHATROOM_CONTACT         (gossip_chatroom_contact_get_type ())
#define GOSSIP_CHATROOM_CONTACT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CHATROOM_CONTACT, GossipChatroomContact))
#define GOSSIP_CHATROOM_CONTACT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_CHATROOM_CONTACT, GossipChatroomContactClass))
#define GOSSIP_IS_CHATROOM_CONTACT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CHATROOM_CONTACT))
#define GOSSIP_IS_CHATROOM_CONTACT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CHATROOM_CONTACT))
#define GOSSIP_CHATROOM_CONTACT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CHATROOM_CONTACT, GossipChatroomContactClass))

typedef struct _GossipChatroomContact      GossipChatroomContact;
typedef struct _GossipChatroomContactClass GossipChatroomContactClass;

struct _GossipChatroomContact {
	GossipContact parent;
};

struct _GossipChatroomContactClass {
	GossipContactClass parent_class;
};

GType                   gossip_chatroom_contact_get_type         (void) G_GNUC_CONST;

GossipChatroomContact * gossip_chatroom_contact_new              (GossipAccount             *account);
GossipChatroomContact * gossip_chatroom_contact_new_full         (GossipAccount             *account,
								  const gchar               *id,
								  const gchar               *name);
GossipChatroomContact * gossip_chatroom_contact_new_from_contact (GossipContact             *contact);
GossipChatroomRole      gossip_chatroom_contact_get_role         (GossipChatroomContact     *contact);
void                    gossip_chatroom_contact_set_role         (GossipChatroomContact     *contact,
								  GossipChatroomRole         role);
GossipChatroomAffiliation gossip_chatroom_contact_get_affiliation (GossipChatroomContact    *contact);
void                    gossip_chatroom_contact_set_affiliation  (GossipChatroomContact     *contact,
								  GossipChatroomAffiliation  affiliation);

#endif /* __GOSSIP_CHATROOM_CONTACT_H__ */
