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

#include <libgossip/gossip-presence.h>

#include "peekaboo-dbus.h"

/* #define DEBUG_MSG(x)  */
#define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n"); 

void
peekaboo_dbus_send_message (const gchar *contact_id)
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

void
peekaboo_dbus_toggle_roster (void)
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

gboolean
peekaboo_dbus_get_roster_visible (void)
{
	DBusGConnection  *bus;
	DBusGProxy       *remote_object;
	GError           *error = NULL;
	gboolean          visible;

	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_warning ("Could not connect to session bus");
		return;
	}
	
	remote_object = dbus_g_proxy_new_for_name (bus,
						   "org.gnome.Gossip",
						   "/org/gnome/Gossip",
						   "org.gnome.Gossip");
	
	if (!dbus_g_proxy_call (remote_object, "GetRosterVisible", &error,
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &visible, G_TYPE_INVALID)) {
		g_warning ("Failed to complete 'GetRosterVisible' request. %s", 
			   error->message);
	}
	
  	g_object_unref (G_OBJECT (remote_object));

	return visible;
}

gchar **
peekaboo_dbus_get_open_chats (void)
{
	DBusGConnection  *bus;
	DBusGProxy       *remote_object;
	GError           *error = NULL;
	char            **chats;

	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_warning ("Could not connect to session bus");
		return;
	}
	
	remote_object = dbus_g_proxy_new_for_name (bus,
						   "org.gnome.Gossip",
						   "/org/gnome/Gossip",
						   "org.gnome.Gossip");
	
	if (!dbus_g_proxy_call (remote_object, "GetOpenChats", &error,
				G_TYPE_INVALID,
				G_TYPE_STRV, &chats, G_TYPE_INVALID)) {
		g_warning ("Failed to complete 'GetOpenChats' request. %s", 
			   error->message);
	}
	
  	g_object_unref (G_OBJECT (remote_object));

	return chats;
}
