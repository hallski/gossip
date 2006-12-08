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

#include <config.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "gossip-chatroom-contact.h"

#define DEBUG_DOMAIN "ChatroomContact"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CHATROOM_CONTACT, GossipChatroomContactPriv))

#define CHATROOMS_XML_FILENAME "chatrooms.xml"
#define CHATROOMS_DTD_FILENAME "gossip-chatroom.dtd"

typedef struct _GossipChatroomContactPriv GossipChatroomContactPriv;

struct _GossipChatroomContactPriv {
	GossipChatroomRole         role;
	GossipChatroomAffiliation  affiliation;
};

static void     chatroom_contact_finalize       (GObject            *object);
static void     chatroom_contact_get_property   (GObject            *object,
						 guint               param_id,
						 GValue             *value,
						 GParamSpec         *pspec);
static void     chatroom_contact_set_property   (GObject            *object,
						 guint               param_id,
						 const GValue       *value,
						 GParamSpec         *pspec);


enum {
	PROP_0,
	PROP_ROLE,
	PROP_AFFILIATION
};

G_DEFINE_TYPE (GossipChatroomContact, gossip_chatroom_contact, GOSSIP_TYPE_CONTACT);

static void
gossip_chatroom_contact_class_init (GossipChatroomContactClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = chatroom_contact_finalize;
	object_class->get_property = chatroom_contact_get_property;
	object_class->set_property = chatroom_contact_set_property;

	g_object_class_install_property (object_class, PROP_ROLE,
					 g_param_spec_enum ("role",
							    NULL, NULL,
							    GOSSIP_TYPE_CHATROOM_ROLE,
							    GOSSIP_CHATROOM_ROLE_NONE,
							    G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_AFFILIATION,
					 g_param_spec_enum ("affiliation",
							    NULL, NULL,
							    GOSSIP_TYPE_CHATROOM_AFFILIATION,
							    GOSSIP_CHATROOM_AFFILIATION_NONE,
							    G_PARAM_READWRITE));

	g_type_class_add_private (object_class,
				  sizeof (GossipChatroomContactPriv));
}

static void
gossip_chatroom_contact_init (GossipChatroomContact *contact)
{

}

static void
chatroom_contact_finalize (GObject *object)
{
	(G_OBJECT_CLASS (gossip_chatroom_contact_parent_class)->finalize) (object);
}

static void
chatroom_contact_get_property (GObject            *object,
			       guint               param_id,
			       GValue             *value,
			       GParamSpec         *pspec)
{
	GossipChatroomContactPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ROLE:
		g_value_set_enum (value, priv->role);
		break;
	case PROP_AFFILIATION:
		g_value_set_enum (value, priv->affiliation);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};

}

static void
chatroom_contact_set_property (GObject            *object,
			       guint               param_id,
			       const GValue       *value,
			       GParamSpec         *pspec)
{
	GossipChatroomContactPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ROLE:
		priv->role = g_value_get_enum (value);
		break;
	case PROP_AFFILIATION:
		priv->affiliation = g_value_get_enum (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

GossipChatroomContact *
gossip_chatroom_contact_new (GossipAccount *account)
{
	return g_object_new (GOSSIP_TYPE_CHATROOM_CONTACT,
			     "type", GOSSIP_CONTACT_TYPE_CHATROOM,
			     "account", account,
			     NULL);
}

GossipChatroomContact *
gossip_chatroom_contact_new_full (GossipAccount  *account,
				  const gchar    *id,
				  const gchar    *name)
{
	return g_object_new (GOSSIP_TYPE_CHATROOM_CONTACT,
			     "type", GOSSIP_CONTACT_TYPE_CHATROOM,
			     "account", account,
			     "name", name,
			     "id", id,
			     NULL);
}
GossipChatroomRole
gossip_chatroom_contact_get_role (GossipChatroomContact *contact)
{
	GossipChatroomContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_CONTACT (contact),
			      GOSSIP_CHATROOM_ROLE_NONE);

	priv = GET_PRIV (contact);

	return priv->role;
}

void
gossip_chatroom_contact_set_role (GossipChatroomContact *contact,
				  GossipChatroomRole     role)
{
	GossipChatroomContactPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM_CONTACT (contact));

	priv = GET_PRIV (contact);
	priv->role = role;

	g_object_notify (G_OBJECT (contact), "role");
}

GossipChatroomAffiliation
gossip_chatroom_contact_get_affiliation (GossipChatroomContact *contact)
{
	GossipChatroomContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_CONTACT (contact),
			      GOSSIP_CHATROOM_AFFILIATION_NONE);

	priv = GET_PRIV (contact);

	return priv->affiliation;
}

void
gossip_chatroom_contact_set_affiliation (GossipChatroomContact *contact,
					 GossipChatroomAffiliation affiliation)
{
	GossipChatroomContactPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM_CONTACT (contact));

	priv = GET_PRIV (contact);
	priv->affiliation = affiliation;

	g_object_notify (G_OBJECT (contact), "affiliation");
}

