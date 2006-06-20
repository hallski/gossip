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

#include <sys/utsname.h>

#include "gossip-version-info.h"

#define GOSSIP_VERSION_INFO_GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_VERSION_INFO, GossipVersionInfoPriv))

typedef struct _GossipVersionInfoPriv GossipVersionInfoPriv;
struct _GossipVersionInfoPriv { 
	gchar *name;
	gchar *version;
	gchar *os;
};

static void version_info_finalize          (GObject              *object);
static void version_info_get_property      (GObject              *object,
					    guint                 param_id,
					    GValue               *value,
					    GParamSpec           *pspec);
static void version_info_set_property      (GObject              *object,
					    guint                 param_id,
					    const GValue         *value,
					    GParamSpec           *pspec);

/* -- Properties -- */
enum {
	PROP_0,
	PROP_NAME,
	PROP_VERSION,
	PROP_OS
};

G_DEFINE_TYPE (GossipVersionInfo, gossip_version_info, G_TYPE_OBJECT);


static void
gossip_version_info_class_init (GossipVersionInfoClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize     = version_info_finalize;
	object_class->get_property = version_info_get_property;
	object_class->set_property = version_info_set_property;

	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Name field",
							      "The name field",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_VERSION,
					 g_param_spec_string ("version",
							      "Version field",
							      "The version field",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_OS,
					 g_param_spec_string ("os",
							      "Os field",
							      "The operating system field",
							      NULL,
							      G_PARAM_READWRITE));
	g_type_class_add_private (object_class, sizeof (GossipVersionInfoPriv));
}

static void
gossip_version_info_init (GossipVersionInfo *VERSION_INFO)
{
	GossipVersionInfoPriv *priv;

	priv = GOSSIP_VERSION_INFO_GET_PRIV (VERSION_INFO);

	priv->name    = NULL;
	priv->version = NULL;
	priv->os      = NULL;
}

static void                
version_info_finalize (GObject *object)
{
	GossipVersionInfoPriv *priv;

	priv = GOSSIP_VERSION_INFO_GET_PRIV (object);
	
	g_free (priv->name);
	g_free (priv->version);
	g_free (priv->os);

	(G_OBJECT_CLASS (gossip_version_info_parent_class)->finalize) (object);
}

static void
version_info_get_property (GObject    *object,
			   guint       param_id,
			   GValue     *value,
			   GParamSpec *pspec)
{
	GossipVersionInfoPriv *priv;

	priv = GOSSIP_VERSION_INFO_GET_PRIV (object);

	switch (param_id) {
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_VERSION:
		g_value_set_string (value, priv->version);
		break;
	case PROP_OS:
		g_value_set_string (value, priv->os);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};		
}

static void
version_info_set_property (GObject      *object,
			   guint         param_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
	GossipVersionInfoPriv *priv;

	priv = GOSSIP_VERSION_INFO_GET_PRIV (object);

	switch (param_id) {
	case PROP_NAME:
		gossip_version_info_set_name (GOSSIP_VERSION_INFO (object), 
					      g_value_get_string (value));
		break;
	case PROP_VERSION:
		gossip_version_info_set_version (GOSSIP_VERSION_INFO (object), 
						 g_value_get_string (value));
		break;
	case PROP_OS:
		gossip_version_info_set_os (GOSSIP_VERSION_INFO (object), 
					    g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};		
}

GossipVersionInfo *
gossip_version_info_new (void)
{
	return g_object_new (GOSSIP_TYPE_VERSION_INFO, NULL);
}

const gchar *
gossip_version_info_get_name (GossipVersionInfo *info)
{
	GossipVersionInfoPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_VERSION_INFO (info), NULL);
	
	priv = GOSSIP_VERSION_INFO_GET_PRIV (info);

	return priv->name;
}

void
gossip_version_info_set_name (GossipVersionInfo *info, const gchar *name)
{
	GossipVersionInfoPriv *priv;

	g_return_if_fail (GOSSIP_IS_VERSION_INFO (info));
	g_return_if_fail (name != NULL);
	
	priv = GOSSIP_VERSION_INFO_GET_PRIV (info);

	g_free (priv->name);
	priv->name = g_strdup (name);
}

const gchar *
gossip_version_info_get_version (GossipVersionInfo *info) 
{
	GossipVersionInfoPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_VERSION_INFO (info), NULL);
	
	priv = GOSSIP_VERSION_INFO_GET_PRIV (info);

	return priv->version;
}

void
gossip_version_info_set_version (GossipVersionInfo *info, const gchar *version)
{
	GossipVersionInfoPriv *priv;

	g_return_if_fail (GOSSIP_IS_VERSION_INFO (info));
	g_return_if_fail (version != NULL);

	priv = GOSSIP_VERSION_INFO_GET_PRIV (info);

	g_free (priv->version);
	priv->version = g_strdup (version);
}

const gchar *
gossip_version_info_get_os (GossipVersionInfo *info) 
{
	GossipVersionInfoPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_VERSION_INFO (info), NULL);
	
	priv = GOSSIP_VERSION_INFO_GET_PRIV (info);

	return priv->os;
}

void
gossip_version_info_set_os (GossipVersionInfo *info, const gchar *os)
{
	GossipVersionInfoPriv *priv;

	g_return_if_fail (GOSSIP_IS_VERSION_INFO (info));
	g_return_if_fail (os != NULL);

	priv = GOSSIP_VERSION_INFO_GET_PRIV (info);

	g_free (priv->os);
	priv->os = g_strdup (os);
}

GossipVersionInfo *
gossip_version_info_get_own (void)
{
	static GossipVersionInfo *info = NULL;

	if (!info) {
		struct utsname osinfo;
		
		uname (&osinfo);
		info = g_object_new (GOSSIP_TYPE_VERSION_INFO,
				     "name", "Imendio Gossip",
				     "version", PACKAGE_VERSION,
				     "os", osinfo.sysname,
				     NULL);
	}

	return info;
}

