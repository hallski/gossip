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

#include <glib/gi18n.h>
#include <gtk/gtkstatusicon.h>

#include "gossip-app.h"
#include "gossip-stock.h"
#include "gossip-ui-utils.h"
#include "gossip-status-icon.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_STATUS_ICON, GossipStatusIconPriv))

typedef struct _GossipStatusIconPriv GossipStatusIconPriv;

struct _GossipStatusIconPriv {
	GList *events;

	guint  flash_timeout_id;
};

static void     status_icon_finalize           (GObject          *object);
static void     status_icon_activate           (GtkStatusIcon    *status_icon);

G_DEFINE_TYPE (GossipStatusIcon, gossip_status_icon, GTK_TYPE_STATUS_ICON);

static void
gossip_status_icon_class_init (GossipStatusIconClass *klass)
{
	GObjectClass       *object_class = G_OBJECT_CLASS (klass);
	GtkStatusIconClass *status_icon_class = GTK_STATUS_ICON_CLASS (klass);

	object_class->finalize = status_icon_finalize;

	status_icon_class->activate = status_icon_activate;

	g_type_class_add_private (object_class, sizeof (GossipStatusIconPriv));
}

static void
gossip_status_icon_init (GossipStatusIcon *status_icon)
{
	GdkPixbuf *pixbuf;

	pixbuf = gossip_pixbuf_from_stock (GOSSIP_STOCK_OFFLINE,
					   GTK_ICON_SIZE_MENU);

	gtk_status_icon_set_from_pixbuf (GTK_STATUS_ICON (status_icon), pixbuf);
	g_object_unref (pixbuf);
}

static void
status_icon_finalize (GObject *object)
{
	GossipStatusIconPriv *priv;

	priv = GET_PRIV (object);

	G_OBJECT_CLASS (gossip_status_icon_parent_class)->finalize (object);
}

static void
status_icon_activate (GtkStatusIcon  *status_icon)
{
	if (!gossip_status_icon_get_events (GOSSIP_STATUS_ICON (status_icon))) {
		gossip_app_toggle_visibility ();
	} else {
		gossip_event_manager_activate (gossip_app_get_event_manager (),
					       gossip_status_icon_get_next_event (GOSSIP_STATUS_ICON (status_icon)));
	}
}

GtkStatusIcon *
gossip_status_icon_get (void)
{
	static GtkStatusIcon *status_icon = NULL;

	if (!status_icon) {
		status_icon = g_object_new (GOSSIP_TYPE_STATUS_ICON, NULL);
	}

	return status_icon;
}

void 
gossip_status_icon_add_event (GossipStatusIcon *status_icon, GossipEvent *event)
{
	GossipStatusIconPriv *priv;
	GList                *l;

	priv = GET_PRIV (status_icon);

	l = g_list_find_custom (priv->events, event, gossip_event_compare);
	if (l) {
		/* Already in list */
		return;
	}

	priv->events = g_list_append (priv->events, g_object_ref (event));
}

void
gossip_status_icon_remove_event (GossipStatusIcon *status_icon, 
				 GossipEvent      *event)
{
	GossipStatusIconPriv *priv;
	GList                *l;

	priv = GET_PRIV (status_icon);

	l = g_list_find_custom (priv->events, event, gossip_event_compare);

	if (!l) {
		/* Not flashing this event */
		return;
	}

	priv->events = g_list_delete_link (priv->events, l);
	
	g_object_unref (event);
}

GList *
gossip_status_icon_get_events (GossipStatusIcon *status_icon)
{
	GossipStatusIconPriv *priv;

	priv = GET_PRIV (status_icon);

	return priv->events;
}

GossipEvent *
gossip_status_icon_get_next_event (GossipStatusIcon *status_icon)
{
	GossipStatusIconPriv *priv;

	priv = GET_PRIV (status_icon);

	return (GossipEvent *) priv->events->data;
}

void
gossip_status_icon_update_tooltip (GossipStatusIcon *status_icon)
{
	GossipEvent *event;

	if (!gossip_status_icon_get_events (status_icon)) {
		const gchar *status;

		if (gossip_app_is_connected ()) {
			GossipPresence      *presence;
			GossipPresenceState  state;

			presence = gossip_self_presence_get_effective (gossip_app_get_self_presence ());
			state = gossip_presence_get_state (presence);
			status = gossip_presence_get_status (presence);

			if (!status) {
				status = gossip_presence_state_get_default_status (state);
			}
		} else {
			/* i18n: The current state of the connection. */
			status = _("Offline");
		}

		gtk_status_icon_set_tooltip (gossip_status_icon_get (), status);
		return;
	}

	event = gossip_status_icon_get_next_event (status_icon);

	gtk_status_icon_set_tooltip (gossip_status_icon_get (),
				     gossip_event_get_message (event));
}

