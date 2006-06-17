/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2006 Imendio AB
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <dbus/dbus-glib.h>
#include <bonobo/bonobo-ui-component.h>
#include <panel-applet-gconf.h>

#include "peekaboo-applet.h"
#include "peekaboo-galago.h"

typedef struct {
	GtkWidget   *applet_widget;
	GtkWidget   *image;
	GtkWidget   *entry;
	GtkTooltips *tooltips;
} PeekabooApplet;

static void applet_toggle_roster_cb (BonoboUIComponent *uic,
				     PeekabooApplet    *applet, 
				     const gchar       *verb_name);
static void applet_preferences_cb   (BonoboUIComponent *uic,
				     PeekabooApplet    *applet, 
				     const gchar       *verb_name);
static void applet_about_cb         (BonoboUIComponent *uic,
				     PeekabooApplet    *applet, 
				     const gchar       *verb_name);

static const BonoboUIVerb applet_menu_verbs [] = {
	BONOBO_UI_VERB ("toggle_roster", applet_toggle_roster_cb),
	BONOBO_UI_VERB ("preferences", applet_preferences_cb),
	BONOBO_UI_VERB ("about", applet_about_cb),
	BONOBO_UI_VERB_END
};

static const char* authors[] = {
	"Martyn Russell <martyn@imendio.com>", 
	NULL
};

static void
applet_dbus_toggle_roster (PeekabooApplet *applet)
{
	DBusGConnection  *bus;
	DBusGProxy       *remote_object;
	GError           *error = NULL;

	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_warning ("Could not connect to session bus");
		return;
	}
	
	remote_object = dbus_g_proxy_new_for_name (bus,
						   "org.gnome.Gossip",
						   "/org/gnome/Gossip",
						   "org.gnome.Gossip");
	
	if (!dbus_g_proxy_call (remote_object, "ToggleRoster", &error,
				G_TYPE_INVALID,
				G_TYPE_INVALID)) {
		g_warning ("Failed to complete 'ToggleRoster' request. %s", 
			   error->message);
	}
	
  	g_object_unref (G_OBJECT (remote_object));
}

static void
applet_dbus_send_message (PeekabooApplet *applet, 
			  const gchar    *contact_id)
{
	DBusGConnection  *bus;
	DBusGProxy       *remote_object;
	GError           *error = NULL;

	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_warning ("Could not connect to session bus");
		return;
	}
	
	remote_object = dbus_g_proxy_new_for_name (bus,
						   "org.gnome.Gossip",
						   "/org/gnome/Gossip",
						   "org.gnome.Gossip");
	
	if (!dbus_g_proxy_call (remote_object, "SendMessage", &error,
				G_TYPE_STRING, contact_id, G_TYPE_INVALID,
				G_TYPE_INVALID)) {
		g_warning ("Failed to complete 'SendMessage' request. %s", 
			   error->message);
	}
	
  	g_object_unref (G_OBJECT (remote_object));
}

static void
applet_set_tooltip (PeekabooApplet *applet,
		    const gchar    *message) 
{
	g_return_if_fail (applet != NULL);
	
	if (message) {
		gtk_tooltips_set_tip (applet->tooltips, 
				      applet->applet_widget, 
				      message, 
				      NULL);
	} else {
		gtk_tooltips_set_tip (applet->tooltips, 
				      applet->applet_widget, 
				      _("Send someone an Instant Message"), 
				      NULL);
	}
}

static void
applet_toggle_roster_cb (BonoboUIComponent *uic, 
			 PeekabooApplet    *applet, 
			 const gchar       *verb_name)
{
	applet_dbus_toggle_roster (applet);	
}

static void
applet_preferences_cb (BonoboUIComponent *uic, 
		       PeekabooApplet    *applet, 
		       const gchar       *verb_name)
{
 	GList *services;
 	GList *people;

	services = peekaboo_galago_get_services ();	
	people = peekaboo_galago_get_people ();	
}

static void
applet_about_cb (BonoboUIComponent *uic, 
		 PeekabooApplet    *applet, 
		 const gchar       *verb_name)
{
	gtk_show_about_dialog (NULL,
			       "name", "Peekaboo", 
			       "version", PACKAGE_VERSION,
			       "copyright", "Copyright \xc2\xa9 2006 Imendio AB",
			       "comments", _("An instant messaging applet."),
			       "authors", authors,
			       "logo-icon-name", "stock_contact",
			       NULL);
}

static void
applet_entry_activate_cb (GtkEntry       *entry,
			  PeekabooApplet *applet)
{
	const gchar *text;

	text = gtk_entry_get_text (entry);
	if (strlen (text) < 1) {
		return;
	}

	applet_dbus_send_message (applet, text);

	gtk_entry_set_text (entry, "");
}

static gboolean
applet_entry_button_press_event_cb (GtkWidget      *widget, 
				    GdkEventButton *event, 
				    PeekabooApplet *applet)
{
#ifndef USE_OLDER_PANEL_APPLET
	panel_applet_request_focus (PANEL_APPLET (applet->applet_widget), event->time);
#endif

	return FALSE;
}

static void
applet_change_size_cb (PanelApplet   *widget, 
		       gint           size, 
		       PeekabooApplet *applet)
{
	gtk_image_set_from_icon_name (GTK_IMAGE (applet->image), 
				      "stock_contact", 
				      GTK_ICON_SIZE_MENU);
}

/*
 * Callback from Bonobo when the control is destroyed.
 */
static void
applet_destroy_cb (BonoboObject  *object, 
		   PeekabooApplet *applet)
{
	g_free (applet);
}

static gboolean
applet_new (PanelApplet *parent_applet)
{
	PeekabooApplet *applet;
	GtkWidget      *hbox;
  
	applet = g_new0 (PeekabooApplet, 1);

	applet->applet_widget = GTK_WIDGET (parent_applet);

	applet->tooltips = gtk_tooltips_new ();
	applet_set_tooltip (applet, NULL);
  
	hbox = gtk_hbox_new (FALSE, 0);  
	gtk_container_add (GTK_CONTAINER (parent_applet), hbox);
	gtk_widget_show (hbox);

	applet->image = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (hbox), applet->image, FALSE, FALSE, 0);
	gtk_image_set_from_icon_name (GTK_IMAGE (applet->image),
				      "gossip",
				      GTK_ICON_SIZE_MENU);
	gtk_widget_show (applet->image);

	applet->entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), applet->entry, TRUE, TRUE, 0);
	gtk_entry_set_width_chars (GTK_ENTRY (applet->entry), 15);
	gtk_widget_show (applet->entry);

	g_signal_connect (applet->entry, "activate",
			  G_CALLBACK (applet_entry_activate_cb), applet);
	g_signal_connect (applet->entry, "button_press_event", 
			  G_CALLBACK (applet_entry_button_press_event_cb), applet);

  	panel_applet_setup_menu_from_file (PANEL_APPLET (applet->applet_widget),
					   NULL,
					   PKGDATADIR "/GNOME_Peekaboo_Applet.xml",
					   NULL,
					   applet_menu_verbs,
					   applet);                               

	gtk_widget_show (applet->applet_widget);
  
	g_signal_connect (G_OBJECT (applet->applet_widget), "change_size",
			  G_CALLBACK (applet_change_size_cb), applet);
	g_signal_connect (panel_applet_get_control (PANEL_APPLET (applet->applet_widget)), "destroy",
			  G_CALLBACK (applet_destroy_cb), applet);

	/* Initialise other modules */
	peekaboo_galago_init ();

	return TRUE;
}

/*
 * The entry point for this factory. If the OAFIID matches, create an instance
 * of the applet.
 */
static gboolean
peekaboo_applet_factory (PanelApplet *applet, 
			 const gchar *iid, 
			 gpointer data)
{
	if (!strcmp (iid, "OAFIID:GNOME_Peekaboo_Applet")) {
		return applet_new (applet);
	}

	return FALSE;
}

/*
 * Generate the boilerplate to hook into GObject/Bonobo.
 */
PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_Peekaboo_Applet_Factory",
                             PANEL_TYPE_APPLET,
                             PACKAGE_NAME, PACKAGE_VERSION,
                             peekaboo_applet_factory,
                             NULL);
