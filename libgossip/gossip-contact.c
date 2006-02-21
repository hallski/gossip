/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio AB
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

#include <glib/gi18n.h>
#include <string.h>

#include "gossip-utils.h"
#include "gossip-contact.h"

#define d(x)

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CONTACT, GossipContactPriv))

/* Make sure any additions to the private contact structure are copied
 * in the gossip_contact_copy() function further down.
 */
typedef struct _GossipContactPriv GossipContactPriv;

struct _GossipContactPriv {
	GossipContactType   type;

	gchar              *name;

	gchar              *id;

	GList              *presences;
	GList              *groups;

	GossipSubscription  subscription;

	GossipAccount      *account;
};


static void contact_class_init        (GossipContactClass   *class);
static void contact_init              (GossipContact        *contact);
static void contact_finalize          (GObject              *object);
static void contact_get_property      (GObject              *object,
				       guint                 param_id,
				       GValue               *value,
				       GParamSpec           *pspec);
static void contact_set_property      (GObject              *object,
				       guint                 param_id,
				       const GValue         *value,
				       GParamSpec           *pspec);

/* properties */
enum {
	PROP_0,
	PROP_TYPE,
	PROP_NAME,
	PROP_ID,
	PROP_ACTIVE_PRESENCE,
	PROP_SUBSCRIPTION,
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
					 PROP_ACTIVE_PRESENCE,
					 g_param_spec_object ("active-presence",
							      "Contact active presence",
							      "Current presence of contact",
							      GOSSIP_TYPE_PRESENCE,
							      G_PARAM_READABLE));
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
	
	priv->type      = GOSSIP_CONTACT_TYPE_TEMPORARY;
	priv->name      = NULL;
	priv->id        = NULL;
	priv->presences = NULL;

	priv->account   = NULL;

        priv->groups    = NULL;
}

static void
contact_finalize (GObject *object)
{
	GossipContactPriv *priv;

	priv = GET_PRIV (object);
	
	g_free (priv->name);
	g_free (priv->id);

	if (priv->presences) {
		GList *l;

		for (l = priv->presences; l; l = l->next) {
			g_object_unref (l->data);
		}

		g_list_free (priv->presences);
	}

	if (priv->groups) {
		GList *l;

		for (l = priv->groups; l; l = l->next) {
			g_free (l->data);
		}

		g_list_free (priv->groups);
	}
	
	g_object_unref (priv->account);
	
	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
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
	case PROP_ACTIVE_PRESENCE:
		if (priv->presences) {
			g_value_set_object (value, G_OBJECT (priv->presences->data));
		} else {
			g_value_set_object (value, NULL);
		}
		break;
	case PROP_SUBSCRIPTION:
		g_value_set_int (value, priv->subscription);
		break;
	case PROP_ACCOUNT:
		if (priv->account) {
			g_value_set_object (value, G_OBJECT (priv->account));
		} else {
			g_value_set_object (value, NULL);
		}
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
		priv->type = g_value_get_int (value);
		break;
	case PROP_NAME:
		gossip_contact_set_name (GOSSIP_CONTACT (object),
					 g_value_get_string (value));
		break;
	case PROP_ID:
		gossip_contact_set_id (GOSSIP_CONTACT (object), 
				       g_value_get_string (value));
		break;
	case PROP_SUBSCRIPTION:
		priv->subscription = g_value_get_int (value);
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
	GossipContact     *new_contact;
	GossipContactPriv *new_priv;
	GossipContactPriv *priv;
	GList             *l;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);
	
	priv = GET_PRIV (contact);

	new_contact = gossip_contact_new_full (priv->type, 
					       priv->account,
					       priv->id,
					       priv->name);

	new_priv = GET_PRIV (new_contact);
	
	new_priv->subscription = priv->subscription;
	
	for (l = priv->groups; l; l = l->next) {
		new_priv->groups = g_list_append (new_priv->groups, 
						  g_strdup (l->data));
	}

	for (l = priv->presences; l; l = l->next) {
		new_priv->presences = g_list_append (new_priv->presences, 
						     g_strdup (l->data));
	}

	return new_contact;
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
	
	priv->account = g_object_ref (account);
}

void 
gossip_contact_add_presence (GossipContact  *contact, 
			     GossipPresence *presence)
{
	GossipContactPriv *priv;
	GList             *l;
	gboolean           old;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (GOSSIP_IS_PRESENCE (presence));

	priv = GET_PRIV (contact);

	old = FALSE;

	for (l = priv->presences; l; l = l->next) {
		if (gossip_presence_resource_equal (l->data, presence)) {
			/* Replace the old presence with the new */
			g_object_unref (l->data);
			l->data = g_object_ref (presence);
			old = TRUE;
		}
	}

	if (!old) {
		priv->presences = g_list_prepend (priv->presences, 
						  g_object_ref (presence));
	}

	priv->presences = g_list_sort (priv->presences,
				       gossip_presence_priority_sort_func);
	/* Highest priority first */
	priv->presences = g_list_reverse (priv->presences);
}

void 
gossip_contact_remove_presence (GossipContact  *contact, 
				GossipPresence *presence)
{
	GossipContactPriv *priv;
	GList             *l;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (GOSSIP_IS_PRESENCE (presence));

	priv = GET_PRIV (contact);

	for (l = priv->presences; l; l = l->next) {
		if (gossip_presence_resource_equal (l->data, presence)) {
			break;
		}
	}

	if (l) {
		priv->presences = g_list_remove_link (priv->presences, l);
		
		g_object_unref (l->data);
		g_list_free_1 (l);
	}

	priv->presences = g_list_sort (priv->presences,
				       gossip_presence_priority_sort_func);
	/* highest priority first */
	priv->presences = g_list_reverse (priv->presences);
}

gboolean
gossip_contact_set_groups (GossipContact *contact, GList *groups)
{
	GossipContactPriv *priv;
	GList             *l;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);
	
	priv = GET_PRIV (contact);

	if (priv->groups) {
		g_list_foreach (priv->groups, (GFunc)g_free, NULL);
		priv->groups = NULL;
	}

	for (l = groups; l; l = l->next) {
		priv->groups = g_list_append (priv->groups, 
					      g_strdup (l->data));
	}

	return TRUE;
}

void
gossip_contact_set_subscription (GossipContact      *contact,
				 GossipSubscription  subscription)
{
	GossipContactPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (contact);

	priv->subscription = subscription;
}



gint
gossip_contact_compare (gconstpointer a,
			gconstpointer b)
{
	const gchar *id_a;
	const gchar *id_b;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (a), 0);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (b), 0);

	id_a = gossip_contact_get_id (GOSSIP_CONTACT (a));
	id_b = gossip_contact_get_id (GOSSIP_CONTACT (b));

	/* FIXME: Maybe use utf8 strcmp? */
	return strcmp (id_a, id_b);
}

gint
gossip_contact_name_compare (gconstpointer a,
			     gconstpointer b)
{
	g_return_val_if_fail (GOSSIP_IS_CONTACT (a), 0);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (b), 0);
	
	d(g_print ("COMPARE: %s vs. %s\n",
		   gossip_contact_get_name (GOSSIP_CONTACT (a)),
		   gossip_contact_get_name (GOSSIP_CONTACT (b))));

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
					  
	return gossip_utils_str_n_case_cmp (name_a, name_b, -1);
}

gboolean
gossip_contact_equal (gconstpointer v1,
		      gconstpointer v2)
{
	const gchar *id_a;
	const gchar *id_b;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (v1), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (v2), FALSE);
	
	id_a = gossip_contact_get_id (GOSSIP_CONTACT (v1));
	id_b = gossip_contact_get_id (GOSSIP_CONTACT (v2));

	return g_str_equal (id_a, id_b);
}

guint
gossip_contact_hash (gconstpointer key)
{
	g_return_val_if_fail (GOSSIP_IS_CONTACT (key), +1);

	return g_str_hash (gossip_contact_get_id (GOSSIP_CONTACT (key)));
}

/* convenience functions */

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

