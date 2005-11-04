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
#include <string.h>

#include "gossip-app.h"
#include "gossip-dbus.h"

static void              dbus_unregistered_func     (DBusConnection *connection,
						     gpointer        user_data);
static void              dbus_send_ok_reply         (DBusConnection *bus,
						     DBusMessage    *message);
static DBusHandlerResult dbus_handle_set_presence   (DBusConnection *bus,
						     DBusMessage    *message);
static DBusHandlerResult dbus_handle_force_non_away (DBusConnection *bus,
						     DBusMessage    *message);
static DBusHandlerResult dbus_message_func          (DBusConnection *connection,
						     DBusMessage    *message,
						     gpointer        user_data);

static DBusHandlerResult dbus_handle_pre_net_down   (DBusConnection *bus,
						     DBusMessage    *message);

static DBusHandlerResult dbus_handle_post_net_up    (DBusConnection *bus, 
						     DBusMessage    *message);

static GossipSession *saved_session = NULL;

static DBusObjectPathVTable vtable = {
	dbus_unregistered_func,
	dbus_message_func,
	NULL,
};


gboolean
gossip_dbus_init (GossipSession *session)
{
	DBusConnection  *bus;
	DBusError        error;

	static gboolean  inited = FALSE;

	g_return_val_if_fail (session != NULL, FALSE);

	/* FIXME: better way to do this? */
	if (saved_session) {
		g_object_unref (saved_session);
		saved_session = NULL;
	}
	
	saved_session = g_object_ref (session);

	if (inited) {
		return TRUE;
	}

	inited = TRUE;

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

	dbus_bus_request_name (bus, GOSSIP_DBUS_SERVICE, 0, &error);
	if (dbus_error_is_set (&error)) {
		g_warning ("Failed to acquire gossip service.");
		dbus_error_free (&error);
		return FALSE;
	}
	
	if (!dbus_connection_register_object_path (bus,
						   GOSSIP_DBUS_OBJECT,
						   &vtable,
						   NULL)) {
		g_warning ("Failed to register object path.");
		return FALSE;
	}
	
	return TRUE;
}

static void
dbus_unregistered_func (DBusConnection *connection,
			gpointer        user_data)
{
}

static void
dbus_send_ok_reply (DBusConnection *bus,
		    DBusMessage    *message)
{
	DBusMessage *reply;
	
	reply = dbus_message_new_method_return (message);
	
	dbus_connection_send (bus, reply, NULL);
	dbus_message_unref (reply);
}

static DBusHandlerResult
dbus_handle_set_presence (DBusConnection *bus,
			  DBusMessage    *message)
{
	DBusMessageIter  iter;
	GossipPresence  *presence;
	gint             show;
	gchar           *status = NULL;
	
	dbus_message_iter_init (message, &iter);

	dbus_message_iter_get_basic (&iter, &show);
	if (dbus_message_iter_next (&iter)) {
		dbus_message_iter_get_basic (&iter, &status);
	}

	presence = gossip_presence_new_full (show, status);
	gossip_session_set_presence (saved_session, presence);
	g_object_unref (presence);

	if (status) {
		dbus_free (status);
	}
	
	dbus_send_ok_reply (bus, message);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
dbus_handle_force_non_away (DBusConnection *bus,
			    DBusMessage    *message)
{	
	gossip_app_force_non_away ();
	
	dbus_send_ok_reply (bus, message);
	
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
dbus_handle_pre_net_down (DBusConnection *bus, DBusMessage *message)
{	
	g_print ("Pre net down\n");
	gossip_app_net_down ();

	dbus_send_ok_reply (bus, message);
	
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
dbus_handle_post_net_up (DBusConnection *bus, DBusMessage *message)
{	
	g_print ("Post net up\n");
	gossip_app_net_up ();

	dbus_send_ok_reply (bus, message);
	
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
dbus_message_func (DBusConnection *connection,
		   DBusMessage    *message,
		   gpointer        user_data)
{

	if (dbus_message_is_method_call (message,
					 GOSSIP_DBUS_INTERFACE,
					 GOSSIP_DBUS_SET_PRESENCE)) {
		return dbus_handle_set_presence (connection, message);
	}
	else if (dbus_message_is_method_call (message,
					      GOSSIP_DBUS_INTERFACE,
					      GOSSIP_DBUS_FORCE_NON_AWAY)) {
		return dbus_handle_force_non_away (connection, message);
	}
	else if (dbus_message_is_method_call (message,
					      GOSSIP_DBUS_INTERFACE,
					      GOSSIP_DBUS_PRE_NET_DOWN)) {
		return dbus_handle_pre_net_down (connection, message);
	}
	else if (dbus_message_is_method_call (message,
					      GOSSIP_DBUS_INTERFACE,
					      GOSSIP_DBUS_POST_NET_UP)) {
		return dbus_handle_post_net_up (connection, message);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

