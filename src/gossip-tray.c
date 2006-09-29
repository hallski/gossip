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

#include <gtk/gtktooltips.h>

#include "eggtrayicon.h"
#include "gossip-contact.h"
#include "gossip-tray.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_TRAY, GossipTrayPriv))

typedef struct _GossipTrayPriv GossipTrayPriv;
struct _GossipTrayPriv {
	EggTrayIcon *tray_icon;
	GtkWidget   *tray_event_box;
	GtkWidget   *tray_image;
	GtkTooltips *tray_tooltips;
	GList       *tray_flash_icons;
	guint        tray_flash_timeout_id;

	GtkWidget   *popup_menu;
	GtkWidget   *popup_menu_status_item;
	GtkWidget   *show_popup_item;
	GtkWidget   *hide_popup_item;

};

static void     tray_finalize           (GObject *object);
static gboolean tray_destroy_cb         (GtkWidget          *widget,
					 gpointer            user_data);
static void     tray_create_menu        (GossipTray *tray);
static void     tray_create             (GossipTray *tray);
static gboolean tray_pop_message        (GossipTray *tray,
					 GossipContact *contact);
static void     tray_update_tooltip     (GossipTray *tray);
static gboolean tray_applet_exists      (void);
static void     tray_flash_start        (GossipTray *tray);
static void     tray_flash_maybe_stop   (GossipTray *tray);

G_DEFINE_TYPE (GossipTray, gossip_tray, G_TYPE_OBJECT);

static void
gossip_tray_class_init (GossipTrayClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tray_finalize;

	g_type_class_add_private (object_class, sizeof (GossipTrayPriv));
}

static void
gossip_tray_init (GossipTray *tray)
{
}

static void
tray_finalize (GObject *object)
{
	GossipTrayPriv *priv;

	priv = GET_PRIV (object);

	if (priv->tray_flash_timeout_id) {
		g_source_remove (priv->tray_flash_timeout_id);
	}

	G_OBJECT_CLASS (gossip_tray_parent_class)->finalize (object);
}

static gboolean
tray_destroy_cb (GtkWidget *widget, gpointer user_data)
{
	GossipTrayPriv *priv;

	priv = GET_PRIV (widget);

	gtk_widget_destroy (GTK_WIDGET (priv->tray_icon));
	priv->tray_icon = NULL;
	priv->tray_event_box = NULL;
	priv->tray_image = NULL;
	priv->tray_tooltips = NULL;

	if (priv->tray_flash_timeout_id) {
		g_source_remove (priv->tray_flash_timeout_id);
		priv->tray_flash_timeout_id = 0;
	}

	tray_create (GOSSIP_TRAY (widget));

	/* Show the window in case the notification area was removed. */
	/*if (!tray_applet_exists ()) {
		gtk_widget_show (priv->window);
	}*/

	return TRUE;
}

static gboolean
tray_button_press_cb (GtkWidget      *widget,
		      GdkEventButton *event,
		      GossipTray     *tray)
{
	GossipTrayPriv *priv;
	GtkWidget     *submenu;

	priv = GET_PRIV (tray);

	if (event->type == GDK_2BUTTON_PRESS ||
	    event->type == GDK_3BUTTON_PRESS) {
		return FALSE;
	}

	switch (event->button) {
	case 1:
		if (tray_pop_message (GOSSIP_TRAY (widget), NULL)) {
			break;
		}

		/* FIXME: app_toggle_visibility (tray); */
		break;

	case 3:
#if 0 /* FIXME */
		if (app_is_visible ()) {
			gtk_widget_show (priv->hide_popup_item);
			gtk_widget_hide (priv->show_popup_item);
		} else {
			gtk_widget_hide (priv->hide_popup_item);
			gtk_widget_show (priv->show_popup_item);
		}

		submenu = app_create_status_menu (FALSE);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (priv->popup_menu_status_item),
					   submenu);

		gtk_menu_popup (GTK_MENU (priv->popup_menu), NULL, NULL, NULL,
				NULL, event->button, event->time);
#endif
		return TRUE;

		break;

	default:
		return FALSE;
	}

	return TRUE;
}

static void
tray_create_menu (GossipTray *tray)
{
	GossipTrayPriv *priv;
	GladeXML       *glade;
	GtkWidget      *message_item;

	priv = GET_PRIV (tray);

	glade = gossip_glade_get_file ("main.glade",
				       "tray_menu",
				       NULL,
				       "tray_menu", &priv->popup_menu,
				       "tray_show_list", &priv->show_popup_item,
				       "tray_hide_list", &priv->hide_popup_item,
				       "tray_new_message", &message_item,
				       "tray_status", &priv->popup_menu_status_item,
				       NULL);

	gossip_glade_connect (glade,
			      app,
			      "tray_show_list", "activate", app_show_hide_activate_cb,
			      "tray_hide_list", "activate", app_show_hide_activate_cb,
			      "tray_new_message", "activate", app_popup_new_message_cb,
			      "tray_quit", "activate", app_quit_cb,
			      NULL);

	priv->enabled_connected_widgets = g_list_prepend (priv->enabled_connected_widgets,
							  priv->popup_menu_status_item);

	priv->enabled_connected_widgets = g_list_prepend (priv->enabled_connected_widgets,
							  message_item);

	g_object_unref (glade);
}

static void
tray_create (GossipTray *tray)
{
	GossipTrayPriv *priv;

	priv = GET_PRIV (tray);

	priv->tray_icon = egg_tray_icon_new (_("Gossip, Instant Messaging Client"));

	priv->tray_event_box = gtk_event_box_new ();
	priv->tray_image = gtk_image_new ();

	gtk_image_set_from_stock (GTK_IMAGE (priv->tray_image),
				  app_get_current_status_icon (),
				  GTK_ICON_SIZE_MENU);

	gtk_container_add (GTK_CONTAINER (priv->tray_event_box),
			   priv->tray_image);

	priv->tray_tooltips = gtk_tooltips_new ();

	gtk_widget_show (priv->tray_event_box);
	gtk_widget_show (priv->tray_image);

	gtk_container_add (GTK_CONTAINER (priv->tray_icon),
			   priv->tray_event_box);
	gtk_widget_show (GTK_WIDGET (priv->tray_icon));

	gtk_widget_add_events (GTK_WIDGET (priv->tray_icon),
			       GDK_BUTTON_PRESS_MASK);

	g_signal_connect (priv->tray_icon,
			  "button_press_event",
			  G_CALLBACK (tray_button_press_cb),
			  app);

	/* Handles when the area is removed from the panel. */
	g_signal_connect (priv->tray_icon,
			  "destroy",
			  G_CALLBACK (tray_destroy_cb),
			  priv->tray_event_box);
}

static gboolean
tray_pop_message (GossipTray *tray, GossipContact *contact)
{
	GossipTrayPriv     *priv;
	GossipPrivateChat *chat;
	GList             *l;

	priv = GET_PRIV (tray);

	if (!priv->tray_flash_icons) {
		return FALSE;
	}

	if (!contact) {
		contact = priv->tray_flash_icons->data;
	}

	chat = gossip_chat_manager_get_chat (priv->chat_manager, contact);
	if (!chat) {
		return FALSE;
	}

	gossip_chat_present (GOSSIP_CHAT (chat));

	l = g_list_find_custom (priv->tray_flash_icons, contact,
				(GCompareFunc) gossip_contact_compare);

	if (!l) {
		return FALSE;
	}

	priv->tray_flash_icons = g_list_delete_link (priv->tray_flash_icons, l);

	tray_flash_maybe_stop (tray);
	tray_update_tooltip (tray);

	if (contact) {
	/*	gossip_contact_list_flash_contact (priv->contact_list,
						   contact, FALSE);*/
	}

	g_object_unref (contact);

	return TRUE;
}

static void
tray_update_tooltip (GossipTray *tray)
{
	GossipTrayPriv    *priv;
	const gchar      *name;
	gchar            *str;
	GossipContact    *contact;

	priv = GET_PRIV (tray);

	if (!priv->tray_flash_icons) {
		gtk_tooltips_set_tip (GTK_TOOLTIPS (priv->tray_tooltips),
				      priv->tray_event_box,
				      NULL, NULL);
		return;
	}

	contact = priv->tray_flash_icons->data;

	name = gossip_contact_get_name (contact);

	str = g_strdup_printf (_("New message from %s"), name);

	gtk_tooltips_set_tip (GTK_TOOLTIPS (priv->tray_tooltips),
			      priv->tray_event_box,
			      str, str);

	g_free (str);
}

static gboolean
tray_applet_exists ()
{
	Screen *xscreen = DefaultScreenOfDisplay (gdk_display);
	Atom    selection_atom;
	char   *selection_atom_name;

	selection_atom_name = g_strdup_printf ("_NET_SYSTEM_TRAY_S%d",
					       XScreenNumberOfScreen (xscreen));
	selection_atom = XInternAtom (DisplayOfScreen (xscreen), selection_atom_name, False);
	g_free (selection_atom_name);

	if (XGetSelectionOwner (DisplayOfScreen (xscreen), selection_atom)) {
		return TRUE;
	} else {
		return FALSE;
	}
}

static gboolean
tray_flash_timeout_func (gpointer data)
{
	GossipTrayPriv   *priv;
	static gboolean  on = FALSE;
	const gchar     *icon = NULL;

	priv = GET_PRIV (widget);

#if 0
	/* Debug code */
	if (priv->tray_flash_icons == NULL && priv->status_flash_timeout_id == 0) {
		g_print ("no flash\n");
	}

	if (priv->status_flash_timeout_id != 0 && priv->explicit_show == priv->auto_show) {
		g_print ("expl == auto, flashing\n");
	}
#endif

	if (priv->status_flash_timeout_id != 0) {
		if (on) {
			switch (priv->explicit_show) {
			case GOSSIP_SHOW_BUSY:
				icon = GOSSIP_STOCK_BUSY;
				break;
			case GOSSIP_SHOW_AVAILABLE:
				icon = GOSSIP_STOCK_AVAILABLE;
				break;
			default:
				g_assert_not_reached ();
				break;
			}
		}
	}
	else if (priv->tray_flash_icons != NULL) {
		if (on) {
			icon = GOSSIP_STOCK_MESSAGE;
		}
	}

	if (icon == NULL) {
		icon = app_get_current_status_icon ();
	}

	gtk_image_set_from_stock (GTK_IMAGE (priv->tray_image),
				  icon,
				  GTK_ICON_SIZE_MENU);

	on = !on;

	return TRUE;
}

static void
tray_flash_start (void)
{
	GossipTrayPriv *priv = GET_PRIV (widget);

	if (!priv->tray_flash_timeout_id) {
		priv->tray_flash_timeout_id = g_timeout_add (FLASH_TIMEOUT,
							     tray_flash_timeout_func,
							     NULL);
	}
}

/* Stop if there are no flashing messages or status change. */
static void
tray_flash_maybe_stop (GossipTray *tray)
{
	GossipTrayPriv *priv;

	priv = GET_PRIV (tray);

	if (priv->tray_flash_icons != NULL || priv->leave_time > 0) {
		return;
	}

	gtk_image_set_from_stock (GTK_IMAGE (priv->tray_image),
				  app_get_current_status_icon (),
				  GTK_ICON_SIZE_MENU);

	if (priv->tray_flash_timeout_id) {
		g_source_remove (priv->tray_flash_timeout_id);
		priv->tray_flash_timeout_id = 0;
	}
}


GossipTray *
gossip_tray_new (void)
{
	return g_object_new (GOSSIP_TYPE_TRAY, NULL);
}

void
gossip_tray_set_icon (GossipTray *tray, const gchar *stock_id)
{
	GossipTrayPriv *priv;

	g_return_if_fail (GOSSIP_IS_TRAY (tray));
	g_return_if_fail (stock_id != NULL);

	priv = GET_PRIV (tray);

	gtk_image_set_from_stock (GTK_IMAGE (priv->tray_image),
				  stock_id,
				  GTK_ICON_SIZE_MENU);
}

