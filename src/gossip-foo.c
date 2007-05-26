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

#include "gossip-app.h"
#include "gossip-foo.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_FOO, GossipFooPriv))

typedef struct _GossipFooPriv GossipFooPriv;

struct _GossipFooPriv {
	gint            my_prop;
	
	/* Presence set by the user (available/busy) */
	GossipPresence *presence;
	
	/* Away presence (away/xa), overrides priv->presence */
	GossipPresence *away_presence;
};

static void         foo_finalize           (GObject             *object);
static void         foo_get_property       (GObject             *object,
					    guint                param_id,
					    GValue              *value,
					    GParamSpec          *pspec);
static void         foo_set_property       (GObject             *object,
					    guint                param_id,
					    const GValue        *value,
					    GParamSpec          *pspec);

enum {
	PROP_0,
	PROP_MY_PROP
};

G_DEFINE_TYPE (GossipFoo, gossip_foo, G_TYPE_OBJECT);

static void
gossip_foo_class_init (GossipFooClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize     = foo_finalize;
	object_class->get_property = foo_get_property;
	object_class->set_property = foo_set_property;

	g_object_class_install_property (object_class,
					 PROP_MY_PROP,
					 g_param_spec_int ("my-prop",
							   "",
							   "",
							   0, 1,
							   1,
							   G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GossipFooPriv));
}

static void
gossip_foo_init (GossipFoo *presence)
{
	GossipFooPriv *priv;

	priv = GET_PRIV (presence);

	priv->presence = gossip_presence_new ();
	gossip_presence_set_state (priv->presence,
				   GOSSIP_PRESENCE_STATE_AVAILABLE);
}

static void
foo_finalize (GObject *object)
{
	GossipFooPriv *priv;

	priv = GET_PRIV (object);

	if (priv->presence) {
		g_object_unref (priv->presence);
	}

	if (priv->away_presence) {
		g_object_unref (priv->away_presence);
	}

	(G_OBJECT_CLASS (gossip_foo_parent_class)->finalize) (object);
}

static void
foo_get_property (GObject    *object,
		    guint       param_id,
		    GValue     *value,
		    GParamSpec *pspec)
{
	GossipFooPriv *priv;

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
foo_set_property (GObject      *object,
		    guint         param_id,
		    const GValue *value,
		    GParamSpec   *pspec)
{
	GossipFooPriv *priv;

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

GossipPresence *
gossip_foo_get_presence (GossipFoo *foo)
{
	GossipFooPriv *priv;
	
	g_return_val_if_fail (GOSSIP_IS_FOO (foo), NULL);

	priv = GET_PRIV (foo);

	return priv->presence;
}

void
gossip_foo_set_presence (GossipFoo *foo, GossipPresence *presence)
{
	GossipFooPriv *priv;

	g_return_if_fail (GOSSIP_IS_FOO (foo));
	g_return_if_fail (GOSSIP_IS_PRESENCE (presence));

	priv = GET_PRIV (foo);

	if (priv->presence) {
		g_object_unref (priv->presence);
		priv->presence = NULL;
	}

	priv->presence = g_object_ref (presence);
}

GossipPresence *
gossip_foo_get_away_presence (GossipFoo *foo)
{
	GossipFooPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_FOO (foo), NULL);

	priv = GET_PRIV (foo);

	return priv->away_presence;
}

void
gossip_foo_set_away_presence (GossipFoo *foo, GossipPresence *presence)
{
	GossipFooPriv *priv;

	g_return_if_fail (GOSSIP_IS_FOO (foo));

	priv = GET_PRIV (foo);

	if (priv->away_presence) {
		g_object_unref (priv->away_presence);
		priv->away_presence = NULL;
	}

	if (presence) {
		priv->away_presence = g_object_ref (presence);
	} 
}

GossipPresence *
gossip_foo_get_effective_presence (GossipFoo *foo)
{
	GossipFooPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_FOO (foo), NULL);

	priv = GET_PRIV (foo);

	if (priv->away_presence) {
		return priv->away_presence;
	}

	return priv->presence;
}

GossipPresenceState
gossip_foo_get_current_state (GossipFoo *foo)
{
	g_return_val_if_fail (GOSSIP_IS_FOO (foo), 
			      GOSSIP_PRESENCE_STATE_UNAVAILABLE);

	if (!gossip_session_is_connected (gossip_app_get_session (), NULL)) {
		return GOSSIP_PRESENCE_STATE_UNAVAILABLE;
	}

	return gossip_presence_get_state (gossip_foo_get_effective_presence (foo));
}

