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

#include "gossip-heartbeat.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_HEARTBEAT, GossipHeartbeatPriv))

typedef struct _GossipHeartbeatPriv GossipHeartbeatPriv;

struct _GossipHeartbeatPriv {
	gint interval;
};

static void         heartbeat_finalize           (GObject             *object);
static void         heartbeat_get_property       (GObject             *object,
					      guint                param_id,
					      GValue              *value,
					      GParamSpec          *pspec);
static void         heartbeat_set_property       (GObject             *object,
					      guint                param_id,
					      const GValue        *value,
					      GParamSpec          *pspec);

enum {
	PROP_0,
	PROP_INTERVAL
};

G_DEFINE_TYPE (GossipHeartbeat, gossip_heartbeat, G_TYPE_OBJECT);

static void
gossip_heartbeat_class_init (GossipHeartbeatClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize     = heartbeat_finalize;
	object_class->get_property = heartbeat_get_property;
	object_class->set_property = heartbeat_set_property;

	g_object_class_install_property (object_class,
					 PROP_INTERVAL,
					 g_param_spec_int ("interval",
							   "Interval",
							   "Heartbeat interval.",
							   0, G_MAXINT,
							   0,
							   G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GossipHeartbeatPriv));
}

static void
gossip_heartbeat_init (GossipHeartbeat *presence)
{
	GossipHeartbeatPriv *priv;

	priv = GET_PRIV (presence);
}

static void
heartbeat_finalize (GObject *object)
{
	GossipHeartbeatPriv *priv;

	priv = GET_PRIV (object);

	(G_OBJECT_CLASS (gossip_heartbeat_parent_class)->finalize) (object);
}

static void
heartbeat_get_property (GObject    *object,
			guint       param_id,
			GValue     *value,
			GParamSpec *pspec)
{
	GossipHeartbeatPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_INTERVAL:
		g_value_set_int (value, priv->interval);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}
static void
heartbeat_set_property (GObject      *object,
			guint         param_id,
			const GValue *value,
			GParamSpec   *pspec)
{
	GossipHeartbeatPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_INTERVAL:
		priv->interval = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

guint
gossip_heartbeat_add_callback (GossipHeartbeat     *beat,
			       GossipHeartbeatFunc  func,
			       gpointer             user_data)
{

	return 0;
}

void
gossip_heartbeat_remove_callback (GossipHeartbeat *beat, guint id)
{
}

