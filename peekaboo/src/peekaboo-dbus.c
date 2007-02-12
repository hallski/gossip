/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2006-2007 Imendio AB
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
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <string.h>

#include <libgossip/gossip.h>

#include "peekaboo-dbus.h"

/* #define DEBUG_MSG(x)  */
#define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n"); 

#define GOSSIP_DBUS_SERVICE           "org.gnome.Gossip"
#define GOSSIP_DBUS_PATH              "/org/gnome/Gossip"
#define GOSSIP_DBUS_INTERFACE         "org.gnome.Gossip"

gboolean
peekaboo_dbus_get_presence (const gchar          *id,
			    GossipPresenceState  *state,
			    gchar               **status)
{
	DBusGConnection  *bus;
	DBusGProxy       *remote_object;
	GError           *error = NULL;
	char             *state_str;
	gboolean          ok;

	g_return_val_if_fail (id != NULL, FALSE);
	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (status != NULL, FALSE);

	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_warning ("Could not connect to session bus");
		return FALSE;
	}
	
	remote_object = dbus_g_proxy_new_for_name (bus,
						   GOSSIP_DBUS_SERVICE,
						   GOSSIP_DBUS_PATH,
						   GOSSIP_DBUS_INTERFACE);
 
	ok = dbus_g_proxy_call (remote_object, "GetPresence", &error,
				G_TYPE_STRING, id, 
				G_TYPE_INVALID,
				G_TYPE_STRING, &state_str, 
				G_TYPE_STRING, status, 
				G_TYPE_INVALID);
	if (!ok) {
		g_warning ("Failed to complete 'GetPresence' request. %s", 
			   error->message);
		g_clear_error (&error);
	} 

	if (state_str) {
		if (strcmp (state_str, "available") == 0) {
			*state = GOSSIP_PRESENCE_STATE_AVAILABLE;
		} 
		else if (strcmp (state_str, "busy") == 0) {
			*state = GOSSIP_PRESENCE_STATE_BUSY;
		}
		else if (strcmp (state_str, "away") == 0) {
			*state = GOSSIP_PRESENCE_STATE_AWAY;
		}
		else if (strcmp (state_str, "xa") == 0) {
			*state = GOSSIP_PRESENCE_STATE_EXT_AWAY;
		}
		else if (strcmp (state_str, "unavailable") == 0) {
			*state = GOSSIP_PRESENCE_STATE_UNAVAILABLE;
		}
	}
	
  	g_object_unref (G_OBJECT (remote_object));

	return ok;
}

gboolean
peekaboo_dbus_get_name (const gchar  *id,
			gchar       **name)
{
	DBusGConnection  *bus;
	DBusGProxy       *remote_object;
	GError           *error = NULL;
	gboolean          ok;

	g_return_val_if_fail (id != NULL, FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	*name = NULL;

	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_warning ("Could not connect to session bus");
		return FALSE;
	}
	
	remote_object = dbus_g_proxy_new_for_name (bus,
						   GOSSIP_DBUS_SERVICE,
						   GOSSIP_DBUS_PATH,
						   GOSSIP_DBUS_INTERFACE);
 
	ok = dbus_g_proxy_call (remote_object, "GetName", &error,
				G_TYPE_STRING, id, G_TYPE_INVALID,
				G_TYPE_STRING, name, G_TYPE_INVALID);
	if (!ok) {
		g_warning ("Failed to complete 'GetName' request. %s", 
			   error->message);
		g_clear_error (&error);
	} 

  	g_object_unref (G_OBJECT (remote_object));

	return ok;
}

gboolean
peekaboo_dbus_get_roster_visible (gboolean *visible)
{
	DBusGConnection  *bus;
	DBusGProxy       *remote_object;
	GError           *error = NULL;
	gboolean          ok;

	g_return_val_if_fail (visible != NULL, FALSE);

	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_warning ("Could not connect to session bus");
		return FALSE;
	}
	
	remote_object = dbus_g_proxy_new_for_name (bus,
						   GOSSIP_DBUS_SERVICE,
						   GOSSIP_DBUS_PATH,
						   GOSSIP_DBUS_INTERFACE);
 
	ok = dbus_g_proxy_call (remote_object, "GetRosterVisible", &error,
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, visible, G_TYPE_INVALID);
	if (!ok) {
		g_warning ("Failed to complete 'GetRosterVisible' request. %s", 
			   error->message);
		g_clear_error (&error);
	}
	
  	g_object_unref (G_OBJECT (remote_object));

	return ok;
}

gboolean
peekaboo_dbus_get_open_chats (gchar ***open_chats)
{
	DBusGConnection  *bus;
	DBusGProxy       *remote_object;
	GError           *error = NULL;
	char            **chats = NULL;

	g_return_val_if_fail (open_chats != NULL, FALSE);

	*open_chats = NULL;

	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_warning ("Could not connect to session bus");
		return FALSE;;
	}
	
	remote_object = dbus_g_proxy_new_for_name (bus,
						   GOSSIP_DBUS_SERVICE,
						   GOSSIP_DBUS_PATH,
						   GOSSIP_DBUS_INTERFACE);
	
	if (!dbus_g_proxy_call (remote_object, "GetOpenChats", &error,
				G_TYPE_INVALID,
				G_TYPE_STRV, &chats, G_TYPE_INVALID)) {
		g_warning ("Failed to complete 'GetOpenChats' request. %s", 
			   error->message);
		g_clear_error (&error);
	}
	
  	g_object_unref (remote_object);

	if (chats) {
		*open_chats = chats;
		return TRUE;
	}

	return FALSE;
}

gboolean
peekaboo_dbus_send_message (const gchar *id)
{
	DBusGConnection  *bus;
	DBusGProxy       *remote_object;
	GError           *error = NULL;
	gboolean          ok;

	g_return_val_if_fail (id != NULL, FALSE);

	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_warning ("Could not connect to session bus");
		return FALSE;
	}
	
	remote_object = dbus_g_proxy_new_for_name (bus,
						   GOSSIP_DBUS_SERVICE,
						   GOSSIP_DBUS_PATH,
						   GOSSIP_DBUS_INTERFACE);
	
	ok = dbus_g_proxy_call (remote_object, "SendMessage", &error,
				G_TYPE_STRING, id, G_TYPE_INVALID,
				G_TYPE_INVALID);
	
	if (!ok) {
		g_warning ("Failed to complete 'SendMessage' request. %s", 
			   error->message);
		g_clear_error (&error);
	}
	
  	g_object_unref (remote_object);
	
	return ok;
}

gboolean
peekaboo_dbus_new_message (void)
{
	DBusGConnection  *bus;
	DBusGProxy       *remote_object;
	GError           *error = NULL;
	gboolean          ok;

	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_warning ("Could not connect to session bus");
		return FALSE;
	}
	
	remote_object = dbus_g_proxy_new_for_name (bus,
						   GOSSIP_DBUS_SERVICE,
						   GOSSIP_DBUS_PATH,
						   GOSSIP_DBUS_INTERFACE);
	
	ok = dbus_g_proxy_call (remote_object, "NewMessage", &error,
				G_TYPE_INVALID,
				G_TYPE_INVALID);

	if (!ok) {
		g_warning ("Failed to complete 'NewMessage' request. %s", 
			   error->message);
		g_clear_error (&error);
	}
	
  	g_object_unref (G_OBJECT (remote_object));

	return ok;
}

gboolean
peekaboo_dbus_toggle_roster (void)
{
	DBusGConnection  *bus;
	DBusGProxy       *remote_object;
	GError           *error = NULL;
	gboolean          ok;

	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_warning ("Could not connect to session bus");
		return FALSE;
	}
	
	remote_object = dbus_g_proxy_new_for_name (bus,
						   GOSSIP_DBUS_SERVICE,
						   GOSSIP_DBUS_PATH,
						   GOSSIP_DBUS_INTERFACE);
	
	ok = dbus_g_proxy_call (remote_object, "ToggleRoster", &error,
				G_TYPE_INVALID,
				G_TYPE_INVALID);
	if (!ok) {
		g_warning ("Failed to complete 'ToggleRoster' request. %s", 
			   error->message);
		g_clear_error (&error);
	}
	
  	g_object_unref (G_OBJECT (remote_object));

	return ok;
}
