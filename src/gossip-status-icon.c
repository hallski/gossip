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
#include <gtk/gtkstock.h>

#include "gossip-app.h"
#include "gossip-stock.h"
#include "gossip-ui-utils.h"
#include "gossip-status-icon.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_STATUS_ICON, GossipStatusIconPriv))

typedef struct _GossipStatusIconPriv GossipStatusIconPriv;

struct _GossipStatusIconPriv {
	GList    *events;

	gboolean  is_status_flashing;

	gint      flash_interval;
	guint     flash_timeout_id;
};

static void     status_icon_finalize           (GObject          *object);
static void     status_icon_activate           (GtkStatusIcon    *status_icon);
static void     status_icon_flash_start_cb     (GossipSelfPresence *self_presence,
						GossipStatusIcon   *status_icon);
static void     status_icon_flash_stop_cb      (GossipSelfPresence *self_presence,
						GossipStatusIcon   *status_icon);

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

	g_signal_connect (gossip_app_get_self_presence (), "start-flash",
			  G_CALLBACK (status_icon_flash_start_cb),
			  status_icon);

	g_signal_connect (gossip_app_get_self_presence (), "stop-flash",
			  G_CALLBACK (status_icon_flash_stop_cb),
			  status_icon);
}

static void
status_icon_finalize (GObject *object)
{
	GossipStatusIconPriv *priv;

	priv = GET_PRIV (object);

	g_signal_handlers_disconnect_by_func (gossip_app_get_self_presence (),
					      status_icon_flash_stop_cb,
					      object);

	g_signal_handlers_disconnect_by_func (gossip_app_get_self_presence (),
					      status_icon_flash_start_cb,
					      object);

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

static void
status_icon_flash_start_cb (GossipSelfPresence *self_presence,
			    GossipStatusIcon   *status_icon)
{
	GossipStatusIconPriv *priv;

	priv = GET_PRIV (status_icon);

	priv->is_status_flashing = TRUE;

	gossip_status_icon_start_flash (status_icon);
}

static void
status_icon_flash_stop_cb (GossipSelfPresence *self_presence,
			   GossipStatusIcon   *status_icon)
{
	GossipStatusIconPriv *priv;

	priv = GET_PRIV (status_icon);

	priv->is_status_flashing = FALSE;
	
	gossip_status_icon_maybe_stop_flash (status_icon);
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

void
gossip_status_icon_maybe_stop_flash (GossipStatusIcon *status_icon)
{
	GossipStatusIconPriv *priv;
	GdkPixbuf            *pixbuf;

	priv = GET_PRIV (status_icon);
	
	if (gossip_status_icon_get_events (status_icon) != NULL || gossip_self_presence_get_leave_time (gossip_app_get_self_presence ()) > 0) {
		return;
	}

	pixbuf = gossip_self_presence_get_current_pixbuf (gossip_app_get_self_presence ());
	gtk_status_icon_set_from_pixbuf (GTK_STATUS_ICON (status_icon), pixbuf);
	g_object_unref (pixbuf);

	if (priv->flash_timeout_id) {
		g_source_remove (priv->flash_timeout_id);
		priv->flash_timeout_id = 0;
	}
}

static gboolean
status_icon_flash_timeout_func (gpointer data)
{
	GossipStatusIcon     *status_icon;
	GossipStatusIconPriv *priv;
	GdkPixbuf            *pixbuf = NULL;
	static gboolean       on = FALSE;

	status_icon = GOSSIP_STATUS_ICON (data);
	priv = GET_PRIV (status_icon);

	if (on) {
		if (priv->is_status_flashing) {
			pixbuf = gossip_self_presence_get_explicit_pixbuf (gossip_app_get_self_presence ());
		}
		else if (gossip_status_icon_get_events (GOSSIP_STATUS_ICON (status_icon)) != NULL) {
			GossipEvent *event;
			const gchar *stock_id = NULL;

			event = gossip_status_icon_get_next_event (GOSSIP_STATUS_ICON (status_icon));
			
			switch (gossip_event_get_type (event)) {
			case GOSSIP_EVENT_NEW_MESSAGE:
			case GOSSIP_EVENT_SERVER_MESSAGE:
				stock_id = GOSSIP_STOCK_MESSAGE;
				break;

			case GOSSIP_EVENT_SUBSCRIPTION_REQUEST:
			case GOSSIP_EVENT_FILE_TRANSFER_REQUEST:
				stock_id = GTK_STOCK_DIALOG_QUESTION;
				break;

			default:
				/* Shouldn't happen */
				stock_id = GTK_STOCK_DIALOG_WARNING;
				break;
			}

			if (stock_id) {
				pixbuf = gossip_pixbuf_from_stock (stock_id,
								   GTK_ICON_SIZE_MENU);
			}
		}
	}

	if (pixbuf == NULL) {
		pixbuf = gossip_self_presence_get_current_pixbuf (gossip_app_get_self_presence ());
	}

	gtk_status_icon_set_from_pixbuf (GTK_STATUS_ICON (status_icon), pixbuf);
	g_object_unref (pixbuf);

	on = !on;

	return TRUE;
}

void
gossip_status_icon_start_flash (GossipStatusIcon *status_icon)
{
	GossipStatusIconPriv *priv;

	priv = GET_PRIV (status_icon);

	if (!priv->flash_timeout_id) {
		priv->flash_timeout_id =
			g_timeout_add (priv->flash_interval,
				       status_icon_flash_timeout_func,
				       status_icon);
	}
}

void
gossip_status_icon_set_flash_interval (GossipStatusIcon *status_icon,
				       gint              interval)
{
	GossipStatusIconPriv *priv;

	priv = GET_PRIV (status_icon);

	priv->flash_interval = interval;
}

