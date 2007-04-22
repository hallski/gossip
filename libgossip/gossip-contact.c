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

#include <string.h>

#include <glib/gi18n.h>

#include "gossip-contact.h"
#include "gossip-utils.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CONTACT, GossipContactPriv))

/* Make sure any additions to the private contact structure are copied
 * in the gossip_contact_copy_values() function further down.
 */
typedef struct _GossipContactPriv GossipContactPriv;

struct _GossipContactPriv {
	GossipContactType   type;

	gchar              *id;
	gchar              *name;

	GList              *presences;
	GList              *groups;

	GossipSubscription  subscription;
	GossipAvatar       *avatar;
	GossipAccount      *account;
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
static void contact_set_type      (GossipContact      *contact,
				   GossipContactType   type);
static void contact_set_presences (GossipContact      *contact,
				   GList              *presences);

enum {
	PROP_0,
	PROP_TYPE,
	PROP_NAME,
	PROP_ID,
	PROP_PRESENCES,
	PROP_GROUPS,
	PROP_SUBSCRIPTION,
	PROP_AVATAR,
	PROP_ACCOUNT
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
							   GOSSIP_CONTACT_TYPE_CONTACTLIST,
							   G_PARAM_READWRITE));

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
							      "String identifying contact",
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
							   G_PARAM_READWRITE));

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

	g_type_class_add_private (object_class, sizeof (GossipContactPriv));
}

static void
contact_init (GossipContact *contact)
{
	GossipContactPriv *priv;

	priv = GET_PRIV (contact);

	priv->type        = GOSSIP_CONTACT_TYPE_TEMPORARY;
	priv->name        = NULL;
	priv->id          = NULL;
	priv->presences   = NULL;
	priv->account     = NULL;
	priv->groups      = NULL;
	priv->avatar      = NULL;
}

static void
contact_finalize (GObject *object)
{
	GossipContactPriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->name);
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

	(G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
contact_get_property (GObject    *object,
		      guint       param_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	GossipContactPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_TYPE:
		g_value_set_int (value, priv->type);
		break;
	case PROP_NAME:
		g_value_set_string (value,
				    gossip_contact_get_name (GOSSIP_CONTACT (object)));
		break;
	case PROP_ID:
		g_value_set_string (value,
				    gossip_contact_get_id (GOSSIP_CONTACT (object)));
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
	GossipContactPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_TYPE:
		contact_set_type (GOSSIP_CONTACT (object),
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
	case PROP_PRESENCES:
		contact_set_presences (GOSSIP_CONTACT (object),
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
			 const gchar       *name)
{
	return g_object_new (GOSSIP_TYPE_CONTACT,
			     "type", type,
			     "account", account,
			     "name", name,
			     "id", id,
			     NULL);
}

GossipContact *
gossip_contact_copy (GossipContact *contact)
{
	GossipContact *new_contact;

	new_contact = g_object_new (GOSSIP_TYPE_CONTACT, NULL);
	gossip_contact_copy_values (contact, new_contact);

	return new_contact;
}

void
gossip_contact_copy_values (GossipContact *contact_from,
			    GossipContact *contact_to)
{
	GossipContactPriv *from_priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact_from));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact_to));

	from_priv = GET_PRIV (contact_from);

	gossip_contact_set_name (contact_to, from_priv->name);
	gossip_contact_set_id (contact_to, from_priv->id);
	gossip_contact_set_subscription (contact_to, from_priv->subscription);
	gossip_contact_set_avatar (contact_to, from_priv->avatar);
	gossip_contact_set_account (contact_to, from_priv->account);
	gossip_contact_set_groups (contact_to, from_priv->groups);
	contact_set_presences (contact_to, from_priv->presences);
}

GossipContactType
gossip_contact_get_type (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact),
			      GOSSIP_CONTACT_TYPE_TEMPORARY);

	priv = GET_PRIV (contact);

	return priv->type;
}

const gchar *
gossip_contact_get_id (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), "");

	priv = GET_PRIV (contact);

	if (priv->id) {
		return priv->id;
	}

	return "";
}

const gchar *
gossip_contact_get_name (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), "");

	priv = GET_PRIV (contact);

	if (priv->name == NULL) {
		return gossip_contact_get_id (contact);
	}

	return priv->name;
}

GossipAvatar *
gossip_contact_get_avatar (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (contact);

	return priv->avatar;
}

GossipAccount *
gossip_contact_get_account (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (contact);

	return priv->account;
}

GossipPresence *
gossip_contact_get_active_presence (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (contact);

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
	GossipContactPriv *priv;
	GList             *l;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);
	g_return_val_if_fail (resource != NULL, NULL);

	priv = GET_PRIV (contact);

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
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (contact);

	return priv->presences;
}

GList *
gossip_contact_get_groups (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (contact);

	return priv->groups;
}

GossipSubscription
gossip_contact_get_subscription (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact),
			      GOSSIP_SUBSCRIPTION_NONE);

	priv = GET_PRIV (contact);

	return priv->subscription;
}

static void
contact_set_type (GossipContact      *contact,
		  GossipContactType   type)
{
	GossipContactPriv *priv;

	priv = GET_PRIV (contact);

	priv->type = type;

	g_object_notify (G_OBJECT (contact), "type");
}

void
gossip_contact_set_id (GossipContact *contact,
		       const gchar   *id)
{
	GossipContactPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (id != NULL);

	priv = GET_PRIV (contact);

	g_free (priv->id);
	priv->id = g_strdup (id);

	g_object_notify (G_OBJECT (contact), "id");
}

void
gossip_contact_set_name (GossipContact *contact,
			 const gchar   *name)
{
	GossipContactPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (name != NULL);

	priv = GET_PRIV (contact);

	g_free (priv->name);
	priv->name = g_strdup (name);

	g_object_notify (G_OBJECT (contact), "name");
}

static void
contact_set_presences (GossipContact *contact,
		       GList         *presences)
{
	GossipContactPriv *priv;

	priv = GET_PRIV (contact);

	if (priv->presences) {
		g_list_foreach (priv->presences, (GFunc) g_object_unref, NULL);
		g_list_free (priv->presences);
	}

	priv->presences = g_list_copy (presences);
	g_list_foreach (priv->presences, (GFunc) g_object_ref, NULL);

	g_object_notify (G_OBJECT (contact), "presences");
}

void
gossip_contact_set_avatar (GossipContact *contact,
			   GossipAvatar  *avatar)
{
	GossipContactPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (contact);

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
	GossipContactPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GET_PRIV (contact);

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
gossip_contact_add_presence (GossipContact  *contact,
			     GossipPresence *presence)
{
	GossipContactPriv *priv;
	GossipPresence    *this_presence;
	GList             *l;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (GOSSIP_IS_PRESENCE (presence));

	priv = GET_PRIV (contact);

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
	GossipContactPriv *priv;
	GossipPresence    *this_presence;
	GList             *l;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (GOSSIP_IS_PRESENCE (presence));

	priv = GET_PRIV (contact);

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
	GossipContactPriv *priv;
	GList             *old_groups, *l;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (contact);

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
	GossipContactPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (contact);

	priv->subscription = subscription;

	g_object_notify (G_OBJECT (contact), "subscription");
}

gint
gossip_contact_name_compare (gconstpointer a,
			     gconstpointer b)
{
	g_return_val_if_fail (GOSSIP_IS_CONTACT (a), 0);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (b), 0);

	return strcmp (gossip_contact_get_name (GOSSIP_CONTACT (a)),
		       gossip_contact_get_name (GOSSIP_CONTACT (b)));
}

gint
gossip_contact_name_case_compare (gconstpointer a,
				  gconstpointer b)
{
	g_return_val_if_fail (GOSSIP_IS_CONTACT (a), 0);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (b), 0);

	return gossip_contact_name_case_n_compare (a, b, -1);
}

gint
gossip_contact_name_case_n_compare (gconstpointer a,
				    gconstpointer b,
				    gsize         n)
{
	const gchar *name_a, *name_b;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (a), 0);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (b), 0);

	name_a = gossip_contact_get_name (GOSSIP_CONTACT (a));
	name_b = gossip_contact_get_name (GOSSIP_CONTACT (b));

	return gossip_strncasecmp (name_a, name_b, -1);
}

gboolean
gossip_contact_equal (gconstpointer v1,
		      gconstpointer v2)
{
	GossipAccount *account_a;
	GossipAccount *account_b;
	const gchar   *id_a;
	const gchar   *id_b;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (v1), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (v2), FALSE);

	account_a = gossip_contact_get_account (GOSSIP_CONTACT (v1));
	account_b = gossip_contact_get_account (GOSSIP_CONTACT (v2));

	id_a = gossip_contact_get_id (GOSSIP_CONTACT (v1));
	id_b = gossip_contact_get_id (GOSSIP_CONTACT (v2));

	return gossip_account_equal (account_a, account_b) && g_str_equal (id_a, id_b);
}

guint
gossip_contact_hash (gconstpointer key)
{
	GossipContactPriv *priv;
	guint              hash = 0;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (key), +1);

	priv = GET_PRIV (GOSSIP_CONTACT (key));

	hash += gossip_account_hash (gossip_contact_get_account (GOSSIP_CONTACT (key)));
	hash += g_str_hash (gossip_contact_get_id (GOSSIP_CONTACT (key)));

	return hash;
}

gboolean
gossip_contact_is_online (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);

	priv = GET_PRIV (contact);

	if (priv->presences == NULL) {
		return FALSE;
	}

	return TRUE;
}

gboolean
gossip_contact_is_in_group (GossipContact *contact,
			    const gchar   *group)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);
	g_return_val_if_fail (!G_STR_EMPTY (group), FALSE);

	priv = GET_PRIV (contact);

	if (g_list_find_custom (priv->groups, group, (GCompareFunc) strcmp)) {
		return TRUE;
	}

	return FALSE;
}

const gchar *
gossip_contact_get_status (GossipContact *contact)
{
	GossipContactPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), "");

	priv = GET_PRIV (contact);

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

