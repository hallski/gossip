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

#include "gossip-vcard.h"

#define GOSSIP_VCARD_GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_VCARD, GossipVCardPriv))

typedef struct _GossipVCardPriv GossipVCardPriv;
struct _GossipVCardPriv { 
	gchar *name;
	gchar *nickname;
	gchar *email;
	gchar *url;
        gchar *country;
	gchar *description;
};

static void vcard_finalize          (GObject              *object);
static void vcard_get_property      (GObject              *object,
				     guint                 param_id,
				     GValue               *value,
				     GParamSpec           *pspec);
static void vcard_set_property      (GObject              *object,
				     guint                 param_id,
				     const GValue         *value,
				     GParamSpec           *pspec);

/* -- Properties -- */
enum {
	PROP_0,
	PROP_NAME,
	PROP_NICKNAME,
	PROP_EMAIL,
	PROP_URL,
        PROP_COUNTRY,
	PROP_DESCRIPTION
};

G_DEFINE_TYPE (GossipVCard, gossip_vcard, G_TYPE_OBJECT);

static gpointer parent_class = NULL;

static void
gossip_vcard_class_init (GossipVCardClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	object_class->finalize     = vcard_finalize;
	object_class->get_property = vcard_get_property;
	object_class->set_property = vcard_set_property;

	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Name field",
							      "The name field",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_NICKNAME,
					 g_param_spec_string ("nickname",
							      "Nickname field",
							      "The nickname field",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_EMAIL,
					 g_param_spec_string ("email",
							      "Email field",
							      "The email field",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_URL,
					 g_param_spec_string ("url",
							      "URL field",
							      "The URL field",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_COUNTRY,
					 g_param_spec_string ("country",
							      "Country field",
							      "The COUNTRY field",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_DESCRIPTION,
					 g_param_spec_string ("description",
							      "Description field",
							      "The description field",
							      NULL,
							      G_PARAM_READWRITE));
	
	g_type_class_add_private (object_class, sizeof (GossipVCardPriv));
}

static void
gossip_vcard_init (GossipVCard *vcard)
{
	GossipVCardPriv *priv;

	priv = GOSSIP_VCARD_GET_PRIV (vcard);

	priv->name        = NULL;
	priv->nickname    = NULL;
	priv->email       = NULL;
	priv->url         = NULL;
        priv->country     = NULL;
	priv->description = NULL;
}

static void                
vcard_finalize (GObject *object)
{
	GossipVCardPriv *priv;

	priv = GOSSIP_VCARD_GET_PRIV (object);
	
	g_free (priv->name);
	g_free (priv->nickname);
	g_free (priv->email);
	g_free (priv->url);
        g_free (priv->country);
	g_free (priv->description);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);

}

static void
vcard_get_property (GObject    *object,
		    guint       param_id,
		    GValue     *value,
		    GParamSpec *pspec)
{
	GossipVCardPriv *priv;

	priv = GOSSIP_VCARD_GET_PRIV (object);

	switch (param_id) {
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_NICKNAME:
		g_value_set_string (value, priv->nickname);
		break;
	case PROP_EMAIL:
		g_value_set_string (value, priv->email);
		break;
	case PROP_URL:
		g_value_set_string (value, priv->url);
		break;
        case PROP_COUNTRY:
                g_value_set_string (value, priv->country);
                break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, priv->description);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};		
}

static void
vcard_set_property (GObject      *object,
		    guint         param_id,
		    const GValue *value,
		    GParamSpec   *pspec)
{
	GossipVCardPriv *priv;

	priv = GOSSIP_VCARD_GET_PRIV (object);

	switch (param_id) {
	case PROP_NAME:
		gossip_vcard_set_name (GOSSIP_VCARD (object), 
				       g_value_get_string (value));
		break;
	case PROP_NICKNAME:
		gossip_vcard_set_nickname (GOSSIP_VCARD (object), 
					   g_value_get_string (value));
		break;
	case PROP_EMAIL:
		gossip_vcard_set_email (GOSSIP_VCARD (object), 
					g_value_get_string (value));
		break;
	case PROP_URL:
		gossip_vcard_set_url (GOSSIP_VCARD (object), 
				      g_value_get_string (value));
		break;
        case PROP_COUNTRY:
                gossip_vcard_set_country (GOSSIP_VCARD (object),
                                          g_value_get_string (value));
                break;
	case PROP_DESCRIPTION:
		gossip_vcard_set_description (GOSSIP_VCARD (object), 
					      g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};		
}

GossipVCard *
gossip_vcard_new (void)
{
	return g_object_new (GOSSIP_TYPE_VCARD, NULL);
}

const gchar *
gossip_vcard_get_name (GossipVCard *vcard)
{
	GossipVCardPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);
	
	priv = GOSSIP_VCARD_GET_PRIV (vcard);

	return priv->name;
}

void
gossip_vcard_set_name (GossipVCard *vcard, const gchar *name)
{
	GossipVCardPriv *priv;

	g_return_if_fail (GOSSIP_IS_VCARD (vcard));
	
	priv = GOSSIP_VCARD_GET_PRIV (vcard);

	g_free (priv->name);
	if (name) {
		priv->name = g_strdup (name);
	} else {
		priv->name = NULL;
	}

}

const gchar *
gossip_vcard_get_nickname (GossipVCard *vcard) 
{
	GossipVCardPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);
	
	priv = GOSSIP_VCARD_GET_PRIV (vcard);

	return priv->nickname;
}

void
gossip_vcard_set_nickname (GossipVCard *vcard, const gchar *nickname)
{
	GossipVCardPriv *priv;

	g_return_if_fail (GOSSIP_IS_VCARD (vcard));

	priv = GOSSIP_VCARD_GET_PRIV (vcard);

	g_free (priv->nickname);
	if (nickname) {
		priv->nickname = g_strdup (nickname);
	} else {
		priv->nickname = NULL;
	}
}

const gchar *
gossip_vcard_get_email (GossipVCard *vcard)
{
	GossipVCardPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);

	priv = GOSSIP_VCARD_GET_PRIV (vcard);

	return priv->email;
}

void
gossip_vcard_set_email (GossipVCard *vcard, const gchar *email)
{
	GossipVCardPriv *priv;

	g_return_if_fail (GOSSIP_IS_VCARD (vcard));

	priv = GOSSIP_VCARD_GET_PRIV (vcard);

	g_free (priv->email);

	if (email) {
		priv->email = g_strdup (email);
	} else {
		priv->email = NULL;
	}
}

const gchar *
gossip_vcard_get_url (GossipVCard *vcard)
{
	GossipVCardPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);
	
	priv = GOSSIP_VCARD_GET_PRIV (vcard);

	return priv->url;
}

void   
gossip_vcard_set_url (GossipVCard *vcard, const gchar *url)
{
	GossipVCardPriv *priv;

	g_return_if_fail (GOSSIP_IS_VCARD (vcard));
	
	priv = GOSSIP_VCARD_GET_PRIV (vcard);

	g_free (priv->url);

	if (url) {
		priv->url = g_strdup (url);
	} else {
		priv->url = NULL;
	}
}

const gchar *
gossip_vcard_get_country (GossipVCard *vcard)
{
	GossipVCardPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);
	
	priv = GOSSIP_VCARD_GET_PRIV (vcard);

	return priv->country;
}

void   
gossip_vcard_set_country (GossipVCard *vcard, const gchar *country)
{
	GossipVCardPriv *priv;

	g_return_if_fail (GOSSIP_IS_VCARD (vcard));
	
	priv = GOSSIP_VCARD_GET_PRIV (vcard);

	g_free (priv->country);

	if (country) {
		priv->country = g_strdup (country);
	} else {
		priv->country = NULL;
	}
}

const gchar *
gossip_vcard_get_description (GossipVCard *vcard)
{
	GossipVCardPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);
	
	priv = GOSSIP_VCARD_GET_PRIV (vcard);

	return priv->description;
}

void
gossip_vcard_set_description (GossipVCard *vcard, const gchar *desc)
{
	GossipVCardPriv *priv;

	g_return_if_fail (GOSSIP_IS_VCARD (vcard));

	priv = GOSSIP_VCARD_GET_PRIV (vcard);

	g_free (priv->description);

	if (desc) {
		priv->description = g_strdup (desc);
	} else {
		priv->description = NULL;
	}

}


