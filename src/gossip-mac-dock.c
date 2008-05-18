/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Imendio AB
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
#include <gtk/gtk.h>
#include <ige-mac-integration.h>

#include "libgossip/gossip-stock.h"
#include "libgossip/gossip-paths.h"

#include "gossip-app.h"
#include "gossip-mac-dock.h"

typedef struct {
        GList *events;
} GossipDockPriv;

static void dock_finalize         (GObject            *object);
static void dock_clicked_cb       (IgeMacDock         *dock);
static void dock_event_added_cb   (GossipEventManager *manager,
                                   GossipEvent        *event,
                                   GossipDock         *dock);
static void dock_event_removed_cb (GossipEventManager *manager,
                                   GossipEvent        *event,
                                   GossipDock         *dock);

G_DEFINE_TYPE (GossipDock, gossip_dock, IGE_TYPE_MAC_DOCK);
#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_DOCK, GossipDockPriv))

static void
gossip_dock_class_init (GossipDockClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = dock_finalize;

        g_type_class_add_private (object_class, sizeof (GossipDockPriv));
}

static void
gossip_dock_init (GossipDock *dock)
{
        /*gtk_dock_set_from_stock (GOSSIP_DOCK (dock),
          GOSSIP_STOCK_OFFLINE);*/

        g_signal_connect (gossip_app_get_event_manager (), "event-added",
                          G_CALLBACK (dock_event_added_cb),
                          dock);

        g_signal_connect (gossip_app_get_event_manager (), "event-removed",
                          G_CALLBACK (dock_event_removed_cb),
                          dock);

        g_signal_connect (dock, "clicked",
                          G_CALLBACK (dock_clicked_cb),
                          dock);
}

static void
dock_finalize (GObject *object)
{
        GossipDockPriv *priv;

        priv = GET_PRIV (object);

        g_signal_handlers_disconnect_by_func (gossip_app_get_event_manager (),
                                              dock_event_added_cb,
                                              object);

        g_signal_handlers_disconnect_by_func (gossip_app_get_event_manager (),
                                              dock_event_removed_cb,
                                              object);

        G_OBJECT_CLASS (gossip_dock_parent_class)->finalize (object);
}

static GossipEvent *
dock_get_next_event (GossipDock *dock)
{
        GossipDockPriv *priv;

        priv = GET_PRIV (dock);

	if (priv->events) {
		return priv->events->data;
	}

	return NULL;
}

static void
dock_clicked_cb (IgeMacDock *dock)
{
        GossipDockPriv *priv;

        priv = GET_PRIV (dock);

        if (!priv->events) {
                gossip_app_set_visibility (TRUE);
        } else {
                gossip_event_manager_activate (gossip_app_get_event_manager (),
                                               dock_get_next_event (GOSSIP_DOCK (dock)));
        }
}

static GdkPixbuf *
dock_get_event_pixbuf (GossipDock *dock)
{
        GossipDockPriv *priv;
	GossipEvent    *event;
	gchar          *path;
	GdkPixbuf      *pixbuf;

        priv = GET_PRIV (dock);

        event = dock_get_next_event (dock);
	if (!event) {
		return NULL;
	}

	path = gossip_paths_get_image_path ("gossip-mac-overlay-new-message.png");
	pixbuf = gdk_pixbuf_new_from_file (path, NULL);
	g_free (path);

	return pixbuf;
}

static void
dock_update_overlay (GossipDock *dock)
{
	GdkPixbuf *pixbuf;

	pixbuf = dock_get_event_pixbuf (dock);
	if (pixbuf) {
		ige_mac_dock_set_overlay_from_pixbuf (IGE_MAC_DOCK (dock), pixbuf);
		g_object_unref (pixbuf);
	} else {
		gchar     *path;
		GdkPixbuf *pixbuf;

		path = gossip_paths_get_image_path ("gossip-logo.png");
		pixbuf = gdk_pixbuf_new_from_file (path, NULL);
		g_free (path);

		if (pixbuf) {
			ige_mac_dock_set_icon_from_pixbuf (IGE_MAC_DOCK (dock), pixbuf);
			g_object_unref (pixbuf);
		}
	}
}

static void
dock_add_event (GossipDock *dock, GossipEvent *event)
{
        GossipDockPriv *priv;
        GList          *l;

        priv = GET_PRIV (dock);

        l = g_list_find_custom (priv->events, event, gossip_event_compare);
        if (!l) {
		priv->events = g_list_append (priv->events, g_object_ref (event));

		dock_update_overlay (dock);
	}
}

static void
dock_remove_event (GossipDock *dock, GossipEvent *event)
{
        GossipDockPriv *priv;
        GList          *l;

        priv = GET_PRIV (dock);

        l = g_list_find_custom (priv->events, event, gossip_event_compare);
        if (l) {
		priv->events = g_list_delete_link (priv->events, l);
		g_object_unref (event);

		dock_update_overlay (dock);
	}
}

static void
dock_event_added_cb (GossipEventManager *manager,
                     GossipEvent        *event,
                     GossipDock         *dock)
{
        dock_add_event (dock, event);
}

static void
dock_event_removed_cb (GossipEventManager *manager,
                       GossipEvent        *event,
                       GossipDock         *dock)
{
        dock_remove_event (dock, event);
}

GossipDock *
gossip_dock_get (void)
{
        static GossipDock *dock;

        if (!dock) {
                dock = g_object_new (GOSSIP_TYPE_DOCK, NULL);
		dock_update_overlay (dock);
        }

        return dock;
}
