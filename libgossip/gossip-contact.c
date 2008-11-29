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
 */

#include <config.h>

#include <string.h>

#include <glib/gi18n.h>

#include "gossip-contact.h"
#include "gossip-jid.h"
#include "gossip-utils.h"

#define GOSSIP_CONTACT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CONTACT, GossipContactPrivate))

/* Make sure any additions to the private contact structure are copied
 * in the gossip_contact_copy_values() function further down.
 */
typedef struct _GossipContactPrivate GossipContactPrivate;

struct _GossipContactPrivate {
	GossipContactType   type;

	gchar              *id;
	gchar              *display_id;
	gchar              *name;

	GList              *presences;
	GList              *groups;

	GossipSubscription  subscription;
	GossipAvatar       *avatar;
	GossipAccount      *account;

	GossipVCard        *vcard;
};

static void contact_class_init    (GossipContactClass *class);
static void contact_init          (GossipContact      *contact);
static void contact_finalize      (GObject            *object);
static void contact_get_property  (GObject            *object,
				   guint               param_id,
				   GValue             *value,
				   GParamSpec         *pspec);
static void contact_set_property  (GObject            *object,
				   guint               param_id,
				   const GValue       *value,
				   GParamSpec         *pspec);

enum {
	PROP_0,
	PROP_TYPE,
	PROP_NAME,
	PROP_ID,
	PROP_DISPLAY_ID,
	PROP_PRESENCES,
	PROP_GROUPS,
	PROP_SUBSCRIPTION,
	PROP_AVATAR,
	PROP_ACCOUNT,
	PROP_VCARD
};

static gpointer parent_class = NULL;

GType
gossip_contact_get_gtype (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GossipContactClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) contact_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GossipContact),
			0,    /* n_preallocs */
			(GInstanceInitFunc) contact_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "GossipContact",
					       &info, 0);
	}

	return type;
}

static void
contact_class_init (GossipContactClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	object_class->finalize     = contact_finalize;
	object_class->get_property = contact_get_property;
	object_class->set_property = contact_set_property;

	g_object_class_install_property (object_class,
					 PROP_TYPE,
					 g_param_spec_int ("type",
							   "Contact Type",
							   "The type of the contact",
							   GOSSIP_CONTACT_TYPE_TEMPORARY,
							   GOSSIP_CONTACT_TYPE_USER,
							   GOSSIP_CONTACT_TYPE_TEMPORARY,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Contact Name",
							      "The name of the contact",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_ID,
					 g_param_spec_string ("id",
							      "Contact id",
							      "String identifying the Jabber ID",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_DISPLAY_ID,
					 g_param_spec_string ("display-id",
							      "Display id",
							      "String identifying the REAL ID",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_PRESENCES,
					 g_param_spec_pointer ("presences",
							       "Contact presences",
							       "Presences of contact",
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_GROUPS,
					 g_param_spec_pointer ("groups",
							       "Contact groups",
							       "Groups of contact",
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SUBSCRIPTION,
					 g_param_spec_int ("subscription",
							   "Contact Subscription",
							   "The subscription status of the contact",
							   GOSSIP_SUBSCRIPTION_NONE,
							   GOSSIP_SUBSCRIPTION_BOTH,
							   GOSSIP_SUBSCRIPTION_NONE,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_AVATAR,
					 g_param_spec_boxed ("avatar",
							     "Avatar image",
							     "The avatar image",
							     GOSSIP_TYPE_AVATAR,
							     G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_ACCOUNT,
					 g_param_spec_object ("account",
							      "Contact Account",
							      "The account associated with the contact",
							      GOSSIP_TYPE_ACCOUNT,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_VCARD,
					 g_param_spec_object ("vcard",
							      "Contact VCard",
							      "The vcard for contact",
							      GOSSIP_TYPE_VCARD,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GossipContactPrivate));
}

static void
contact_init (GossipContact *contact)
{
}

static void
contact_finalize (GObject *object)
{
	GossipContactPrivate *priv;

	priv = GOSSIP_CONTACT_GET_PRIVATE (object);

	g_free (priv->name);
	g_free (priv->display_id);
	g_free (priv->id);

	if (priv->avatar) {
		gossip_avatar_unref (priv->avatar);
	}

	if (priv->presences) {
		g_list_foreach (priv->presences, (GFunc) g_object_unref, NULL);
		g_list_free (priv->presences);
	}

	if (priv->groups) {
		g_list_foreach (priv->groups, (GFunc) g_free, NULL);
		g_list_free (priv->groups);
	}

	if (priv->account) {
		g_object_unref (priv->account);
	}

	if (priv->vcard) {
		g_object_unref (priv->vcard);
	}

	(G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
contact_get_property (GObject    *object,
		      guint       param_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	GossipContactPrivate *priv;

	priv = GOSSIP_CONTACT_GET_PRIVATE (object);

	switch (param_id) {
	case PROP_TYPE:
		g_value_set_int (value, priv->type);
		break;
	case PROP_NAME:
		/* We call the function here because it returns
		 * something else if priv->name == NULL.
		 */
		g_value_set_string (value, gossip_contact_get_name (GOSSIP_CONTACT (object)));
		break;
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	case PROP_DISPLAY_ID:
		g_value_set_string (value, priv->display_id);
		break;
	case PROP_PRESENCES:
		g_value_set_pointer (value, priv->presences);
		break;
	case PROP_GROUPS:
		g_value_set_pointer (value, priv->groups);
		break;
	case PROP_SUBSCRIPTION:
		g_value_set_int (value, priv->subscription);
		break;
	case PROP_AVATAR:
		g_value_set_boxed (value, priv->avatar);
		break;
	case PROP_ACCOUNT:
		g_value_set_object (value, priv->account);
		break;
	case PROP_VCARD:
		g_value_set_object (value, priv->vcard);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
contact_set_property (GObject      *object,
		      guint         param_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	GossipContactPrivate *priv;

	priv = GOSSIP_CONTACT_GET_PRIVATE (object);

	switch (param_id) {
	case PROP_TYPE:
		gossip_contact_set_type (GOSSIP_CONTACT (object),
					 g_value_get_int (value));
		break;
	case PROP_NAME:
		gossip_contact_set_name (GOSSIP_CONTACT (object),
					 g_value_get_string (value));
		break;
	case PROP_ID:
		gossip_contact_set_id (GOSSIP_CONTACT (object),
				       g_value_get_string (value));
		break;
	case PROP_DISPLAY_ID:
		gossip_contact_set_display_id (GOSSIP_CONTACT (object),
					       g_value_get_string (value));
		break;
	case PROP_PRESENCES:
		gossip_contact_set_presence_list (GOSSIP_CONTACT (object),
						  g_value_get_pointer (value));
		break;
	case PROP_GROUPS:
		gossip_contact_set_groups (GOSSIP_CONTACT (object),
					   g_value_get_pointer (value));
		break;
	case PROP_SUBSCRIPTION:
		gossip_contact_set_subscription (GOSSIP_CONTACT (object),
						 g_value_get_int (value));
		break;
	case PROP_AVATAR:
		gossip_contact_set_avatar (GOSSIP_CONTACT (object),
					   g_value_get_boxed (value));
		break;
	case PROP_ACCOUNT:
		gossip_contact_set_account (GOSSIP_CONTACT (object),
					    GOSSIP_ACCOUNT (g_value_get_object (value)));
		break;
	case PROP_VCARD:
		gossip_contact_set_vcard (GOSSIP_CONTACT (object),
					  GOSSIP_VCARD (g_value_get_object (value)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

GossipContact *
gossip_contact_new (GossipContactType  type,
		    GossipAccount     *account)
{
	return g_object_new (GOSSIP_TYPE_CONTACT,
			     "type", type,
			     "account", account,
			     NULL);
}

GossipContact *
gossip_contact_new_full (GossipContactType  type,
			 GossipAccount     *account,
			 const gchar       *id,
			 const gchar       *display_id,
			 const gchar       *name)
{
	return g_object_new (GOSSIP_TYPE_CONTACT,
			     "type", type,
			     "account", account,
			     "name", name,
			     "id", id,
			     "display-id", display_id,
			     NULL);
}

GossipContact *
gossip_contact_copy (GossipContact *contact)
{
	GossipContact     *new_contact;
	GossipContactPrivate *priv;
	
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	new_contact = gossip_contact_new_full (gossip_contact_get_type (contact),
					       priv->account,
					       priv->id,
					       priv->display_id,
					       priv->name);

	gossip_contact_set_subscription (new_contact, priv->subscription);
	gossip_contact_set_avatar (new_contact, priv->avatar);
	gossip_contact_set_groups (new_contact, priv->groups);
	gossip_contact_set_presence_list (new_contact, priv->presences);

	return new_contact;
}

GossipContactType
gossip_contact_get_type (GossipContact *contact)
{
	GossipContactPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact),
			      GOSSIP_CONTACT_TYPE_TEMPORARY);

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	return priv->type;
}

const gchar *
gossip_contact_get_id (GossipContact *contact)
{
	GossipContactPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), "");

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	if (priv->id) {
		return priv->id;
	}

	return "";
}

const gchar *
gossip_contact_get_display_id (GossipContact *contact)
{
	GossipContactPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), "");

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	if (priv->display_id) {
		return priv->display_id;
	}

	return "";
}

const gchar *
gossip_contact_get_name (GossipContact *contact)
{
	GossipContactPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), "");

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	if (priv->name == NULL) {
		return gossip_contact_get_id (contact);
	}

	return priv->name;
}

GossipAvatar *
gossip_contact_get_avatar (GossipContact *contact)
{
	GossipContactPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	return priv->avatar;
}

GdkPixbuf *
gossip_contact_get_avatar_pixbuf (GossipContact *contact)
{
	GossipContactPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	if (!priv->avatar) {
		return NULL;
	}

	return gossip_avatar_get_pixbuf (priv->avatar);
}

GossipAccount *
gossip_contact_get_account (GossipContact *contact)
{
	GossipContactPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	return priv->account;
}

GossipVCard *
gossip_contact_get_vcard (GossipContact *contact)
{
	GossipContactPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	return priv->vcard;
}

GossipPresence *
gossip_contact_get_active_presence (GossipContact *contact)
{
	GossipContactPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	if (priv->presences) {
		/* Highest priority of the presences is first */
		return GOSSIP_PRESENCE (priv->presences->data);
	}

	return NULL;
}

GossipPresence *
gossip_contact_get_presence_for_resource (GossipContact *contact,
					  const gchar   *resource)
{
	GossipContactPrivate *priv;
	GList             *l;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);
	g_return_val_if_fail (resource != NULL, NULL);

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	for (l = priv->presences; l; l = l->next) {
		const gchar *p_res;

		p_res = gossip_presence_get_resource (GOSSIP_PRESENCE (l->data));
		if (p_res && strcmp (resource, p_res) == 0) {
			return GOSSIP_PRESENCE (l->data);
		}
	}

	return NULL;
}

GList *
gossip_contact_get_presence_list (GossipContact *contact)
{
	GossipContactPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	return priv->presences;
}

void
gossip_contact_set_presence_list (GossipContact *contact,
				  GList         *presences)
{
	GossipContactPrivate *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	if (priv->presences) {
		g_list_foreach (priv->presences, (GFunc) g_object_unref, NULL);
		g_list_free (priv->presences);
	}

	if (presences) {
		priv->presences = g_list_copy (presences);
		g_list_foreach (priv->presences, (GFunc) g_object_ref, NULL);
	} else {
		priv->presences = NULL;
	}

	g_object_notify (G_OBJECT (contact), "presences");
}

GList *
gossip_contact_get_groups (GossipContact *contact)
{
	GossipContactPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	return priv->groups;
}

GossipSubscription
gossip_contact_get_subscription (GossipContact *contact)
{
	GossipContactPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact),
			      GOSSIP_SUBSCRIPTION_NONE);

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	return priv->subscription;
}

void
gossip_contact_set_type (GossipContact     *contact,
			 GossipContactType  type)
{
	GossipContactPrivate *priv;

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	priv->type = type;

	g_object_notify (G_OBJECT (contact), "type");
}

void
gossip_contact_set_id (GossipContact *contact,
		       const gchar   *id)
{
	GossipContactPrivate *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (id != NULL);

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	g_free (priv->id);
	priv->id = g_strdup (id);

	g_object_notify (G_OBJECT (contact), "id");
}

void
gossip_contact_set_display_id (GossipContact *contact,
			       const gchar   *display_id)
{
	GossipContactPrivate *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (display_id != NULL);

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	g_free (priv->display_id);
	priv->display_id = g_strdup (display_id);

	g_object_notify (G_OBJECT (contact), "display-id");
}

void
gossip_contact_set_name (GossipContact *contact,
			 const gchar   *name)
{
	GossipContactPrivate *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (name != NULL);

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	g_free (priv->name);
	priv->name = g_strdup (name);

	g_object_notify (G_OBJECT (contact), "name");
}

void
gossip_contact_set_avatar (GossipContact *contact,
			   GossipAvatar  *avatar)
{
	GossipContactPrivate *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	if (priv->avatar) {
		gossip_avatar_unref (priv->avatar);
		priv->avatar = NULL;
	}

	if (avatar) {
		priv->avatar = gossip_avatar_ref (avatar);
	}

	g_object_notify (G_OBJECT (contact), "avatar");
}

void
gossip_contact_set_account (GossipContact *contact,
			    GossipAccount *account)
{
	GossipContactPrivate *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	if (priv->account) {
		g_object_unref (priv->account);
	}

	if (account) {
		priv->account = g_object_ref (account);
	} else {
		priv->account = NULL;
	}

	g_object_notify (G_OBJECT (contact), "account");
}

void 
gossip_contact_set_vcard (GossipContact *contact, GossipVCard *vcard)
{
	GossipContactPrivate *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (GOSSIP_IS_VCARD (vcard));

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	if (priv->vcard) {
		g_object_unref (priv->vcard);
	}

	priv->vcard = g_object_ref (vcard);

	g_object_notify (G_OBJECT (contact), "vcard");
}

void
gossip_contact_add_presence (GossipContact  *contact,
			     GossipPresence *presence)
{
	GossipContactPrivate *priv;
	GossipPresence    *this_presence;
	GList             *l;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (GOSSIP_IS_PRESENCE (presence));

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	for (l = priv->presences; l && presence; l = l->next) {
		this_presence = l->data;

		if (gossip_presence_resource_equal (this_presence, presence)) {
			gint ref_count;

			ref_count = G_OBJECT (presence)->ref_count;

			/* Remove old presence for this resource, we
			 * would use g_list_remove_all() here but we
			 * want to make sure we unref for each
			 * instance we find it in the list.
			 */
			priv->presences = g_list_remove (priv->presences, this_presence);
			g_object_unref (this_presence);

			if (!priv->presences || ref_count <= 1) {
				break;
			}

			/* Reset list to beginnging to make sure we
			 * didn't miss any duplicates.
			 */
			l = priv->presences;
		}
	}

	/* Add new presence */
	priv->presences = g_list_insert_sorted (priv->presences,
						g_object_ref (presence),
						gossip_presence_sort_func);

	g_object_notify (G_OBJECT (contact), "presences");
}

void
gossip_contact_remove_presence (GossipContact  *contact,
				GossipPresence *presence)
{
	GossipContactPrivate *priv;
	GossipPresence    *this_presence;
	GList             *l;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (GOSSIP_IS_PRESENCE (presence));

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	for (l = priv->presences; l; l = l->next) {
		this_presence = l->data;

		if (gossip_presence_resource_equal (this_presence, presence)) {
			gint ref_count;

			ref_count = G_OBJECT (presence)->ref_count;

			/* Remove old presence for this resource, we
			 * would use g_list_remove_all() here but we
			 * want to make sure we unref for each
			 * instance we find it in the list.
			 */
			priv->presences = g_list_remove (priv->presences, this_presence);
			g_object_unref (this_presence);

			if (!priv->presences || ref_count <= 1) {
				break;
			}

			/* Reset list to beginnging to make sure we
			 * didn't miss any duplicates.
			 */
			l = priv->presences;
		}
	}

	priv->presences = g_list_sort (priv->presences,
				       gossip_presence_sort_func);

	g_object_notify (G_OBJECT (contact), "presences");
}

void
gossip_contact_set_groups (GossipContact *contact,
			   GList         *groups)
{
	GossipContactPrivate *priv;
	GList             *old_groups, *l;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	old_groups = priv->groups;
	priv->groups = NULL;

	for (l = groups; l; l = l->next) {
		priv->groups = g_list_append (priv->groups,
					      g_strdup (l->data));
	}

	g_list_foreach (old_groups, (GFunc) g_free, NULL);
	g_list_free (old_groups);

	g_object_notify (G_OBJECT (contact), "groups");
}

void
gossip_contact_set_subscription (GossipContact      *contact,
				 GossipSubscription  subscription)
{
	GossipContactPrivate *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	priv->subscription = subscription;

	g_object_notify (G_OBJECT (contact), "subscription");
}

guint
gossip_contact_hash (gconstpointer key)
{
	GossipContactPrivate *priv;
	guint              hash = 0;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (key), +1);

	priv = GOSSIP_CONTACT_GET_PRIVATE (key);

	hash += gossip_account_hash (gossip_contact_get_account (GOSSIP_CONTACT (key)));

	/* Use simple JID hash for all contacts except chatroom contacts */
	if (priv->type == GOSSIP_CONTACT_TYPE_CHATROOM) {	
		if (priv->presences && priv->presences->data) {
			GossipPresence *presence;
			const gchar    *resource;

			presence = priv->presences->data;
			resource = gossip_presence_get_resource (presence);

			if (resource) {
				hash += g_str_hash (resource);
			}
		}
	}

	return hash;
}

gboolean
gossip_contact_equal (gconstpointer v1,
		      gconstpointer v2)
{
	GossipAccount     *account_a;
	GossipAccount     *account_b;
	GossipContactType  type_a;
	GossipContactType  type_b;
	const gchar       *id_a;
	const gchar       *id_b;
	gboolean           equal;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (v1), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (v2), FALSE);

	type_a = gossip_contact_get_type (GOSSIP_CONTACT (v1));
	type_b = gossip_contact_get_type (GOSSIP_CONTACT (v2));

	account_a = gossip_contact_get_account (GOSSIP_CONTACT (v1));
	account_b = gossip_contact_get_account (GOSSIP_CONTACT (v2));

	id_a = gossip_contact_get_id (GOSSIP_CONTACT (v1));
	id_b = gossip_contact_get_id (GOSSIP_CONTACT (v2));

	equal  = TRUE;
	equal &= type_a == type_b; 
	equal &= gossip_account_equal (account_a, account_b);
	equal &= g_str_equal (id_a, id_b);

	return equal;
}

gboolean
gossip_contact_is_online (GossipContact *contact)
{
	GossipContactPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	if (priv->presences == NULL) {
		return FALSE;
	}

	return TRUE;
}

gboolean
gossip_contact_is_in_group (GossipContact *contact,
			    const gchar   *group)
{
	GossipContactPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);
	g_return_val_if_fail (!G_STR_EMPTY (group), FALSE);

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	if (g_list_find_custom (priv->groups, group, (GCompareFunc) strcmp)) {
		return TRUE;
	}

	return FALSE;
}

const gchar *
gossip_contact_get_status (GossipContact *contact)
{
	GossipContactPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), "");

	priv = GOSSIP_CONTACT_GET_PRIVATE (contact);

	if (priv->presences) {
		GossipPresence *p;
		const gchar    *status;

		p = GOSSIP_PRESENCE (priv->presences->data);
		status = gossip_presence_get_status (p);
		if (!status) {
			status = gossip_presence_state_get_default_status (gossip_presence_get_state (p));
		}
		return status;
	} else {
		return _("Offline");
	}
}

const gchar *
gossip_contact_type_to_string (GossipContactType type)
{
	switch (type) {
	case GOSSIP_CONTACT_TYPE_TEMPORARY:
		return "GOSSIP_CONTACT_TYPE_TEMPORARY";
	case GOSSIP_CONTACT_TYPE_CONTACTLIST:
		return "GOSSIP_CONTACT_TYPE_CONTACTLIST";
	case GOSSIP_CONTACT_TYPE_CHATROOM:
		return "GOSSIP_CONTACT_TYPE_CHATROOM";
	case GOSSIP_CONTACT_TYPE_USER:
		return "GOSSIP_CONTACT_TYPE_USER";
	}

	return "";
}
