/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio HB
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
#include <string.h>
#include "gossip-app.h"
#include "gossip-dbus.h"

static void              unregistered_func (DBusConnection *connection,
					    gpointer        user_data);
static DBusHandlerResult message_func      (DBusConnection *connection,
					    DBusMessage    *message,
					    gpointer        user_data);


static const char     *object_path = "/org/imendio/Gossip";

static DBusObjectPathVTable vtable = {
	unregistered_func,
	message_func,
	NULL,
};


gboolean
gossip_dbus_init (void)
{
	DBusConnection *bus;
	DBusError       error;

	bus = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	if (!bus) {
		return FALSE;
	}
  
	dbus_error_init (&error);
	dbus_connection_setup_with_g_main(bus, NULL);
	if (dbus_error_is_set (&error)) {
		g_warning ("Failed to setup dbus connection");
		dbus_error_free (&error);
		return FALSE;
  }

	dbus_bus_acquire_service (bus, GOSSIP_SERVICE, 0, &error);
	if (dbus_error_is_set (&error)) {
		g_warning ("Failed to acquire gossip service.");
		dbus_error_free (&error);
		return FALSE;
	}
	
	if (!dbus_connection_register_object_path (bus,
						   object_path,
						   &vtable,
						   NULL)) {
		g_warning ("Failed to register object path.");
		return FALSE;
	}
	
	return TRUE;
}

static void
unregistered_func (DBusConnection *connection,
		   gpointer        user_data)
{
}

static void
send_ok_reply (DBusConnection *bus,
	       DBusMessage    *message)
{
	DBusMessage *reply;
	
	reply = dbus_message_new_method_return (message);
	
	dbus_connection_send (bus, reply, NULL);
	dbus_message_unref (reply);
}

static DBusHandlerResult
handle_set_presence (DBusConnection *bus,
		     DBusMessage    *message,
		     GossipApp      *app)
{
	DBusMessageIter  iter;
	gint             show;
	gchar           *status = NULL;
	
	dbus_message_iter_init (message, &iter);

	show = dbus_message_iter_get_uint32 (&iter);
	if (dbus_message_iter_next (&iter)) {
		status = dbus_message_iter_get_string (&iter);
	}
	
	gossip_app_set_presence (show, status);

	if (status) {
		dbus_free (status);
	}
	
	send_ok_reply (bus, message);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
handle_force_non_away (DBusConnection *bus,
		       DBusMessage    *message,
		       GossipApp      *app)
{
	gossip_app_force_non_away ();
	
	send_ok_reply (bus, message);
	
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
message_func (DBusConnection *connection,
	      DBusMessage    *message,
	      gpointer        user_data)
{
	GossipApp *app;
	
	app = gossip_app_get ();
	
	if (dbus_message_is_method_call (message,
					 GOSSIP_INTERFACE,
					 GOSSIP_SET_PRESENCE)) {
		return handle_set_presence (connection, message, app);
	}
	else if (dbus_message_is_method_call (message,
					      GOSSIP_INTERFACE,
					      GOSSIP_FORCE_NON_AWAY)) {
		return handle_force_non_away (connection, message, app);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

