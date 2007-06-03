/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Imendio AB
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

#include "config.h"

#include "gossip-theme-irc.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_THEME_IRC, GossipThemeIrcPriv))

typedef struct _GossipThemeIrcPriv GossipThemeIrcPriv;

struct _GossipThemeIrcPriv {
	gint my_prop;
};

static void         theme_irc_finalize           (GObject             *object);
static void         theme_irc_get_property       (GObject             *object,
					      guint                param_id,
					      GValue              *value,
					      GParamSpec          *pspec);
static void         theme_irc_set_property       (GObject             *object,
					      guint                param_id,
					      const GValue        *value,
					      GParamSpec          *pspec);

enum {
	PROP_0,
	PROP_MY_PROP
};

G_DEFINE_TYPE (GossipThemeIrc, gossip_theme_irc, G_TYPE_OBJECT);

static void
gossip_theme_irc_class_init (GossipThemeIrcClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize     = theme_irc_finalize;
	object_class->get_property = theme_irc_get_property;
	object_class->set_property = theme_irc_set_property;

	g_object_class_install_property (object_class,
					 PROP_MY_PROP,
					 g_param_spec_int ("my-prop",
							   "",
							   "",
							   0, 1,
							   1,
							   G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GossipThemeIrcPriv));
}

static void
gossip_theme_irc_init (GossipThemeIrc *presence)
{
	GossipThemeIrcPriv *priv;

	priv = GET_PRIV (presence);
}

static void
theme_irc_finalize (GObject *object)
{
	GossipThemeIrcPriv *priv;

	priv = GET_PRIV (object);

	(G_OBJECT_CLASS (gossip_theme_irc_parent_class)->finalize) (object);
}

static void
theme_irc_get_property (GObject    *object,
		    guint       param_id,
		    GValue     *value,
		    GParamSpec *pspec)
{
	GossipThemeIrcPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_MY_PROP:
		g_value_set_int (value, priv->my_prop);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}
static void
theme_irc_set_property (GObject      *object,
		    guint         param_id,
		    const GValue *value,
		    GParamSpec   *pspec)
{
	GossipThemeIrcPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_MY_PROP:
		priv->my_prop = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

