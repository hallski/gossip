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

#include "config.h"

#include <gtk/gtkstatusicon.h>

#include "gossip-status-icon.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_STATUS_ICON, GossipStatusIconPriv))

typedef struct _GossipStatusIconPriv GossipStatusIconPriv;

struct _GossipStatusIconPriv {
	gint my_prop;
};

static void     status_icon_finalize           (GObject            *object);

G_DEFINE_TYPE (GossipStatusIcon, gossip_status_icon, GTK_TYPE_STATUS_ICON);

static void
gossip_status_icon_class_init (GossipStatusIconClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = status_icon_finalize;

	g_type_class_add_private (object_class, sizeof (GossipStatusIconPriv));
}

static void
gossip_status_icon_init (GossipStatusIcon *status_icon)
{
}

static void
status_icon_finalize (GObject *object)
{
	GossipStatusIconPriv *priv;

	priv = GET_PRIV (object);

	G_OBJECT_CLASS (gossip_status_icon_parent_class)->finalize (object);
}

GossipStatusIcon *
gossip_status_icon_get (void)
{
	static GossipStatusIcon *status_icon = NULL;

	if (!status_icon) {
		status_icon = g_object_new (GOSSIP_TYPE_STATUS_ICON, NULL);
	}

	return status_icon;
}

