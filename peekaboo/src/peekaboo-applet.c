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
#include <bonobo/bonobo-ui-component.h>
#include <panel-applet-gconf.h>

#include "peekaboo-applet.h"
#include "peekaboo-dbus.h"
#include "peekaboo-galago.h"
#include "peekaboo-stock.h"
#include "peekaboo-utils.h"

/* #define DEBUG_MSG(x)  */
#define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n"); 

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
	BONOBO_UI_UNSAFE_VERB ("toggle_roster", applet_toggle_roster_cb),
	BONOBO_UI_UNSAFE_VERB ("preferences", applet_preferences_cb),
	BONOBO_UI_UNSAFE_VERB ("about", applet_about_cb),
	BONOBO_UI_VERB_END
};

static const char* authors[] = {
	"Martyn Russell <martyn@imendio.com>", 
	NULL
};

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
	DEBUG_MSG (("Applet: Toggling roster visibility"));
	peekaboo_dbus_toggle_roster ();	
}

static void
applet_preferences_cb (BonoboUIComponent *uic, 
		       PeekabooApplet    *applet, 
		       const gchar       *verb_name)
{
 	GList *services;
 	GList *people;
 	GList *accounts;

	services = peekaboo_galago_get_services ();	
	people = peekaboo_galago_get_people ();	
	accounts = peekaboo_galago_get_accounts ();	
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

	peekaboo_dbus_send_message (text);

	gtk_entry_set_text (entry, "");
}

static gboolean
applet_entry_button_press_event_cb (GtkWidget      *widget, 
				    GdkEventButton *event, 
				    PeekabooApplet *applet)
{
	return FALSE;
}

static void
applet_menu_item_activate_cb (GtkMenuItem *item,
			      gpointer     user_data)
{
	const gchar *contact_id;

	contact_id = g_object_get_data (G_OBJECT (item), "contact_id");

	DEBUG_MSG (("Applet: Sending message to:'%s'", contact_id));
	peekaboo_dbus_send_message (contact_id);
}

static void
applet_menu_new_message_activate_cb (GtkMenuItem *item,
				     gpointer     user_data)
{
	DEBUG_MSG (("Applet: New message"));
	peekaboo_dbus_new_message ();
}

static void
applet_menu_position_func (GtkMenu        *menu,
			   gint           *x,
			   gint           *y,
			   gboolean       *push_in,
			   PeekabooApplet *applet)
{
	GtkWidget      *widget;
	GtkRequisition  req;
	GdkScreen      *screen;
	gint            screen_height;

	widget = GTK_WIDGET (applet->applet_widget);

	gtk_widget_size_request (GTK_WIDGET (menu), &req);

	gdk_window_get_origin (widget->window, x, y); 

	*x += widget->allocation.x;
	*y += widget->allocation.y;

	screen = gtk_widget_get_screen (GTK_WIDGET (menu));
	screen_height = gdk_screen_get_height (screen);	

	if (req.height > screen_height) {
		/* Too big for screen height anyway. */
		*y = 0;
		return;
	}

	if ((*y + req.height + widget->allocation.height) > screen_height) {
		/* Can't put it below the button. */
		*y -= req.height;
		*y += 1;
	} else {
		/* Put menu below button. */
		*y += widget->allocation.height;
		*y -= 1;
	}

	*push_in = FALSE;
}

static gboolean
applet_button_press_event_cb (GtkWidget      *widget,
			      GdkEventButton *event, 
			      PeekabooApplet *applet)
{
	GtkWidget            *menu;
	GtkWidget            *item;
	GtkWidget            *image;
	const gchar          *stock_id;
	gchar               **chats;
	gchar               **p;
	GossipPresenceState   state;

	if (event->button != 1 || 
	    event->type != GDK_BUTTON_PRESS) {
		return FALSE;
	}

	menu = gtk_menu_new ();

	item = gtk_image_menu_item_new_with_label (_("New Message..."));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	image = gtk_image_new_from_stock (PEEKABOO_STOCK_MESSAGE, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	gtk_widget_show (image);

	g_signal_connect (item, "activate", 
			  G_CALLBACK (applet_menu_new_message_activate_cb),
			  NULL);

	if (peekaboo_dbus_get_open_chats (&chats) && chats && chats[0]) {
		item = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);
		
		DEBUG_MSG (("Applet: Open chats:"));
		for (p = chats; *p; p++) {
			gchar *status;
			gchar *name;

/* 			if (!peekaboo_galago_get_state_and_name (*p, &name, &state)) { */
/* 				DEBUG_MSG (("\t\"%s\"", *p)); */
/* 				continue; */
/* 			} */

			if (!peekaboo_dbus_get_name (*p, &name) || 
			    !name || strlen (name) < 1) {
				g_warning ("Couldn't get name for contact:'%s'", *p);
				continue;
			}
			
			if (!peekaboo_dbus_get_presence (*p, &state, &status)) {
				g_warning ("Couldn't get presence for contact:'%s'", *p);
				g_free (name);
				continue;
			}

			DEBUG_MSG (("\tid:'%s', name:'%s', state:%d, status:'%s'", 
				    *p, name, state, status));

			item = gtk_image_menu_item_new_with_label (name);
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
			gtk_widget_show (item);
			
			stock_id = peekaboo_presence_state_to_stock_id (state);
			image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
			gtk_widget_show (image);
			
			g_object_set_data_full (G_OBJECT (item), "contact_id", 
						g_strdup (*p), g_free);
			
			g_signal_connect (item, "activate", 
					  G_CALLBACK (applet_menu_item_activate_cb),
					  NULL);
			
			g_free (name);
			g_free (status);
		}
	}

	g_strfreev (chats);
		
	/* Popup menu */
	gtk_widget_show (menu);

	gtk_menu_popup (GTK_MENU (menu), 
			NULL, NULL, 
			(GtkMenuPositionFunc) applet_menu_position_func,
			applet, 
			event->button, event->time);

	/* FIXME: attach menu to widget or at least hook up the hide
	 * signal and destroy the menu then. 
	 */

	return TRUE;
}

static void
applet_size_allocate_cb (GtkWidget      *widget,
			 GtkAllocation  *allocation,
			 PeekabooApplet *applet)
{
        gint              size;
	PanelAppletOrient orient;
	
	orient = panel_applet_get_orient (PANEL_APPLET (widget));
        if (orient == PANEL_APPLET_ORIENT_LEFT ||
            orient == PANEL_APPLET_ORIENT_RIGHT) {
                size = allocation->width;
        } else {
                size = allocation->height;
        }

        gtk_image_set_pixel_size (GTK_IMAGE (applet->image), size - 2);
}

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
				      GTK_ICON_SIZE_INVALID);
	gtk_widget_show (applet->image);

	applet->entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), applet->entry, TRUE, TRUE, 0);
	gtk_entry_set_width_chars (GTK_ENTRY (applet->entry), 15);
/* 	gtk_widget_show (applet->entry); */

	g_signal_connect (applet->entry, 
			  "activate",
			  G_CALLBACK (applet_entry_activate_cb), applet);
	g_signal_connect (applet->entry, 
			  "button_press_event", 
			  G_CALLBACK (applet_entry_button_press_event_cb), applet);

	panel_applet_set_flags (PANEL_APPLET (applet->applet_widget), 
				PANEL_APPLET_EXPAND_MINOR);
	panel_applet_set_background_widget (PANEL_APPLET (applet->applet_widget),
					    GTK_WIDGET (applet->applet_widget));

  	panel_applet_setup_menu_from_file (PANEL_APPLET (applet->applet_widget),
					   NULL,
					   PKGDATADIR "/GNOME_Peekaboo_Applet.xml",
					   NULL,
					   applet_menu_verbs,
					   applet);                               

	gtk_widget_show (applet->applet_widget);

	g_signal_connect (applet->applet_widget,
			  "button_press_event",
			  G_CALLBACK (applet_button_press_event_cb), applet);
	g_signal_connect (applet->applet_widget,
			  "size_allocate",
			  G_CALLBACK (applet_size_allocate_cb), applet);
	g_signal_connect (panel_applet_get_control (PANEL_APPLET (applet->applet_widget)), 
			  "destroy",
			  G_CALLBACK (applet_destroy_cb), applet);

	/* Initialise other modules */
	peekaboo_galago_init ();
	peekaboo_stock_init ();

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
