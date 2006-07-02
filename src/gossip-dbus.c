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
#include <stdlib.h>
#include <string.h>
#include <dbus/dbus-glib-bindings.h>

#include <libgossip/gossip-debug.h>

#include "gossip-app.h"
#include "gossip-dbus.h"
#include "gossip-new-message-dialog.h"

#define DEBUG_DOMAIN "DBUS"

#define GOSSIP_DBUS_ERROR_DOMAIN "GossipDBus"

/* None-generated functions */
static DBusGProxy *dbus_freedesktop_init (void);
static DBusGProxy *dbus_nm_init          (void);
static gboolean    dbus_gossip_show      (gboolean show);

/* Set up the DBus GObject to use */
typedef struct GossipDBus GossipDBus;
typedef struct GossipDBusClass GossipDBusClass;

GType gossip_dbus_get_type (void);

struct GossipDBus {
	GObject parent;
};

struct GossipDBusClass {
	GObjectClass parent;
};

#define GOSSIP_DBUS_SERVICE           "org.gnome.Gossip"
#define GOSSIP_DBUS_PATH              "/org/gnome/Gossip"
#define GOSSIP_DBUS_INTERFACE         "org.gnome.Gossip"

#define GOSSIP_TYPE_DBUS              (gossip_dbus_get_type ())
#define GOSSIP_DBUS(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GOSSIP_TYPE_DBUS, GossipDBus))
#define GOSSIP_DBUS_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GOSSIP_TYPE_DBUS, GossipDBusClass))
#define GOSSIP_IS_OBJECT(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GOSSIP_TYPE_DBUS))
#define GOSSIP_IS_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GOSSIP_TYPE_DBUS))
#define GOSSIP_DBUS_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GOSSIP_TYPE_DBUS, GossipDBusClass))

G_DEFINE_TYPE(GossipDBus, gossip_dbus, G_TYPE_OBJECT)

gboolean gossip_dbus_set_presence       (GossipDBus   *obj,
					 const char   *state,
					 const char   *status,
					 GError      **error);
gboolean gossip_dbus_set_not_away       (GossipDBus   *obj,
					 GError      **error);
gboolean gossip_dbus_set_network_status (GossipDBus   *obj,
					 gboolean      up,
					 GError      **error);
gboolean gossip_dbus_set_roster_visible (GossipDBus   *obj,
					 gboolean      visible,
					 GError      **error);
gboolean gossip_dbus_get_roster_visible (GossipDBus   *obj,
					 gboolean     *visible,
					 GError      **error);
gboolean gossip_dbus_get_open_chats     (GossipDBus   *obj,
					 char       ***contacts,
					 GError      **error);
gboolean gossip_dbus_send_message       (GossipDBus   *obj,
					 const gchar  *contact_id,
					 GError      **error);
gboolean gossip_dbus_new_message        (GossipDBus   *obj,
					 GError      **error);
gboolean gossip_dbus_toggle_roster      (GossipDBus   *obj,
					 GError      **error);

#include "gossip-dbus-glue.h"

static GossipSession *saved_session = NULL;

static void
gossip_dbus_init (GossipDBus *obj)
{
}

static void
gossip_dbus_class_init (GossipDBusClass *klass)
{
}

gboolean
gossip_dbus_set_presence (GossipDBus   *obj, 
			  const char   *state, 
			  const char   *status,
			  GError      **error)
{
	GossipPresenceState show;

	gossip_debug (DEBUG_DOMAIN, "Setting presence to state:'%s', status:'%s'", state, status);

	if (strcasecmp (state, "available") == 0) {
		show = GOSSIP_PRESENCE_STATE_AVAILABLE;
	} 
	else if (strcasecmp (state, "busy") == 0) {
  		show = GOSSIP_PRESENCE_STATE_BUSY;
	}
	else if (strcasecmp (state, "away") == 0) {
  		show = GOSSIP_PRESENCE_STATE_AWAY;
	}
	else if (strcasecmp (state, "xa") == 0) {
  		show = GOSSIP_PRESENCE_STATE_EXT_AWAY;
	} else {
		gossip_debug (DEBUG_DOMAIN, "Presence state:'%s' not recognised, try 'available', "
			    "'busy', 'away', or 'xa'", state);

		g_set_error (error, gossip_dbus_error_quark (), 0, 
			     "State:'%s' unrecognised, try 'available', 'busy', 'away', or 'xa'", 
			     state);

		return FALSE;
	}
	
	gossip_app_set_presence (show, status);

	return TRUE;
}

gboolean
gossip_dbus_set_not_away (GossipDBus  *obj, 
			  GError     **error)
{
	gossip_debug (DEBUG_DOMAIN, "Setting presence to NOT AWAY");
	gossip_app_set_not_away ();
	
	return TRUE;
}

gboolean
gossip_dbus_set_network_status (GossipDBus *obj, 
				gboolean    up, 
				GError     **error)
{
	gossip_debug (DEBUG_DOMAIN, "Setting network status %s", 
		      up ? "up" : "down");
	
	if (up) {
		gossip_app_net_up ();
	} else {
		gossip_app_net_down ();
	}
 
	return TRUE;
}

gboolean
gossip_dbus_set_roster_visible (GossipDBus  *obj,
				gboolean     visible,
				GError     **error)
{
	gossip_debug (DEBUG_DOMAIN, "Setting roster window to be %s",
		      visible ? "shown" : "hidden");

	gossip_app_set_visibility (visible);

	return TRUE;
}

gboolean 
gossip_dbus_get_roster_visible (GossipDBus   *obj,
				gboolean     *visible,
				GError      **error)
{
	gossip_debug (DEBUG_DOMAIN, "Getting roster visiblity");

	*visible = gossip_app_is_window_visible ();

	return TRUE;
}

gboolean 
gossip_dbus_get_open_chats (GossipDBus   *obj,
			    char       ***contacts,
			    GError      **error)
{
	GossipChatManager *manager;
	GList             *chats;
	GList             *l;
	gint               n, i;

	gossip_debug (DEBUG_DOMAIN, "Getting open chats");

	manager = gossip_app_get_chat_manager ();
	chats = gossip_chat_manager_get_chats (manager);
	
	n = g_list_length (chats) + 1;
	*contacts = g_new0 (char*, n);

	for (l = chats, i = 0; l; l = l->next, i++) {
		(*contacts)[i] = l->data;   
	}
	
 	g_list_free (chats); 

	return TRUE;
}

gboolean
gossip_dbus_send_message (GossipDBus   *obj, 
			  const gchar  *contact_id, 
			  GError      **error)
{
	GossipChatManager *manager;
	GossipContact     *contact;

	gossip_debug (DEBUG_DOMAIN, "Sending message to contact:'%s'", contact_id);

	contact = gossip_session_find_contact (saved_session, contact_id);
	if (!contact) {
		g_set_error (error, gossip_dbus_error_quark (), 0, 
			     "Contact:'%s' not found", contact_id);
		return FALSE;
	}

	manager = gossip_app_get_chat_manager ();
	gossip_chat_manager_show_chat (manager, contact);

	return TRUE;
}

gboolean 
gossip_dbus_new_message (GossipDBus  *obj,
			 GError     **error)
{
	gossip_debug (DEBUG_DOMAIN, "New message");
	gossip_new_message_dialog_show (NULL);

	return TRUE;
}

gboolean 
gossip_dbus_toggle_roster (GossipDBus  *obj,
			   GError     **error)
{
	gossip_debug (DEBUG_DOMAIN, "Toggling roster visibility");
	gossip_app_toggle_visibility ();

	return TRUE;
}

GQuark
gossip_dbus_error_quark (void)
{
        return g_quark_from_static_string (GOSSIP_DBUS_ERROR_DOMAIN);
}

gboolean
gossip_dbus_init_for_session (GossipSession *session,
			      gboolean       multiple_instances)
{
	DBusGConnection *bus;
	DBusGProxy      *bus_proxy;
	GError          *error = NULL;
	GossipDBus      *obj;
	guint            result;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), FALSE);

	if (saved_session) {
		gossip_debug (DEBUG_DOMAIN, "Already initiated");
		return TRUE;
	}

	saved_session = g_object_ref (session);

	gossip_debug (DEBUG_DOMAIN, "Initialising Gossip object");
	
	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_warning ("Couldn't connect to the session bus");
		return FALSE;
	}

	if ((bus_proxy = dbus_freedesktop_init ()) == NULL) {
		return FALSE;
	}

	dbus_g_object_type_install_info (GOSSIP_TYPE_DBUS, &dbus_glib_gossip_dbus_object_info);

	obj = g_object_new (GOSSIP_TYPE_DBUS, NULL);

	dbus_g_connection_register_g_object (bus, GOSSIP_DBUS_PATH, G_OBJECT (obj));
	
	if (!org_freedesktop_DBus_request_name (bus_proxy,
						GOSSIP_DBUS_SERVICE,
						DBUS_NAME_FLAG_DO_NOT_QUEUE,
						&result, &error)) {
		g_warning ("Failed to acquire %s: %s",
			   GOSSIP_DBUS_SERVICE,
			   error->message);
		g_error_free (error);
		return FALSE;
	}

	if (!multiple_instances && result == DBUS_REQUEST_NAME_REPLY_EXISTS) {
		gossip_debug (DEBUG_DOMAIN, "Gossip is already running");
		
		dbus_gossip_show (TRUE);
		exit (EXIT_SUCCESS);
	}
	
	/* Set up GNOME Netwrk Manager if available so we get signals */
	dbus_nm_init ();

	gossip_debug (DEBUG_DOMAIN, "Ready");

	return TRUE;
}


void 
gossip_dbus_finalize_for_session (void)
{
	g_object_unref (saved_session);
}

/*
 * Freedesktop
 */

#define FREEDESKTOP_DBUS_SERVICE      "org.freedesktop.DBus"
#define FREEDESKTOP_DBUS_PATH         "/org/freedesktop/DBus"
#define FREEDESKTOP_DBUS_INTERFACE    "org.freedesktop.DBus"

static DBusGProxy *
dbus_freedesktop_init (void)
{
	DBusGConnection   *bus;
	static DBusGProxy *bus_proxy = NULL;
	GError            *error = NULL;

	if (bus_proxy) {
		return bus_proxy;
	}

	gossip_debug (DEBUG_DOMAIN, "Initialising Freedesktop proxy");

	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_warning ("Couldn't connect to the session bus");
		return NULL;
	}
	
	bus_proxy = dbus_g_proxy_new_for_name (bus, 
					       FREEDESKTOP_DBUS_SERVICE,
					       FREEDESKTOP_DBUS_PATH,
					       FREEDESKTOP_DBUS_INTERFACE);
	
	if (!bus_proxy) {
		g_warning ("Could not connect to Freedesktop");
	}

	return bus_proxy;
}

/*
 * GNOME Network Manager
 */

#define NM_DBUS_SERVICE   "org.freedesktop.NetworkManager"
#define NM_DBUS_PATH      "/org/freedesktop/NetworkManager"
#define NM_DBUS_INTERFACE "org.freedesktop.NetworkManager"

typedef enum NMState {
	NM_STATE_UNKNOWN = 0,
	NM_STATE_ASLEEP,
	NM_STATE_CONNECTING,
	NM_STATE_CONNECTED,
	NM_STATE_DISCONNECTED
} NMState;

static const gchar *
dbus_nm_state_to_string (guint32 state)
{
	switch (state) {
	case NM_STATE_ASLEEP:
		return "asleep";
	case NM_STATE_CONNECTING:   
		return "connecting";
	case NM_STATE_CONNECTED:    
		return "connected";
	case NM_STATE_DISCONNECTED: 
		return "disconnected";
	case NM_STATE_UNKNOWN:
	default:                    
		return "unknown";
	}
}

static void
dbus_nm_state_cb (DBusGProxy *proxy, 
		  guint       state,
		  gpointer    user_data) 
{
	gossip_debug (DEBUG_DOMAIN, "New network state:'%s'", 
		      dbus_nm_state_to_string (state));

	switch (state) {
	case NM_STATE_ASLEEP:
	case NM_STATE_DISCONNECTED:
	case NM_STATE_UNKNOWN:
		gossip_app_net_down ();
		break;
	case NM_STATE_CONNECTED:
		gossip_app_net_up ();
		break;
	default:
		break;
	}
}

static DBusGProxy *
dbus_nm_init (void)
{
	DBusGConnection   *bus;
	static DBusGProxy *bus_proxy = NULL;
	GError            *error = NULL;

	if (bus_proxy) {
		return bus_proxy;
	}

	gossip_debug (DEBUG_DOMAIN, "Initialising Network Manager proxy");

	bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (!bus) {
		g_warning ("Could not connect to system bus");
		return FALSE;
	}
	
	bus_proxy = dbus_g_proxy_new_for_name (bus, 
					       NM_DBUS_SERVICE, 
					       NM_DBUS_PATH, 
					       NM_DBUS_INTERFACE);
	
	if (!bus_proxy) {
		g_warning ("Could not connect to Network Manager");
		return NULL;
	}

	dbus_g_object_register_marshaller (g_cclosure_marshal_VOID__UINT, 
					   G_TYPE_NONE, G_TYPE_UINT, G_TYPE_INVALID);

	/* Tell DBus what the type signature of the signal callback is; this
	 * allows us to sanity-check incoming messages before invoking the
	 * callback.  You need to do this once for each proxy you create,
	 * not every time you want to connect to the signal.
	 */
	dbus_g_proxy_add_signal (bus_proxy, "StateChange",
				 G_TYPE_UINT, G_TYPE_INVALID);

	/* Actually connect to the signal. Note you can call
	 * dbus_g_proxy_connect_signal multiple times for one invocation of
	 * dbus_g_proxy_add_signal.
	 */
	dbus_g_proxy_connect_signal (bus_proxy, "StateChange", 
				     G_CALLBACK (dbus_nm_state_cb),
				     NULL, NULL);

	return bus_proxy;
} 

gboolean 
gossip_dbus_nm_get_state (gboolean *connected)
{
	DBusGConnection *bus;
	DBusGProxy      *bus_proxy;
	GError          *error = NULL;
	guint32          state;
	
	g_return_val_if_fail (connected != NULL, FALSE);

	/* Set the initial value of connected incase we have to return */
	*connected = FALSE;

	/* Make sure we have set up Network Manager connections */
	if ((bus_proxy = dbus_nm_init ()) == NULL) {
		return FALSE;
	}

	bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (!bus) {
		g_warning ("Could not connect to system bus");
		return FALSE;
	}
	
	if (!dbus_g_proxy_call (bus_proxy, "state", &error,
				G_TYPE_INVALID, 
				G_TYPE_UINT, &state, G_TYPE_INVALID)) {
		g_warning ("Failed to complete 'state' request. %s", 
			   error->message);
	}

	gossip_debug (DEBUG_DOMAIN, "Current network state:'%s'", 
		      dbus_nm_state_to_string (state));

	if (connected) {
		*connected = state == NM_STATE_CONNECTED;
	}

	return TRUE;
}

/*
 * Gossip 
 */
static gboolean
dbus_gossip_show (gboolean show)
{
	DBusGConnection *bus;
	DBusGProxy      *bus_proxy;
	GError          *error = NULL;
	gboolean         success = TRUE;
	
	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_warning ("Could not connect to system bus");
		return FALSE;
	}

	/* We are doing this because normally we wouldn't have a
	 * connection to ourselves.
	 */
	bus_proxy = dbus_g_proxy_new_for_name (bus, 
					       GOSSIP_DBUS_SERVICE,
					       GOSSIP_DBUS_PATH,
					       GOSSIP_DBUS_INTERFACE);

	if (!bus_proxy) {
		g_warning ("Could not connect to other instance of Gossip");
		return FALSE;
	}
		
	if (!dbus_g_proxy_call (bus_proxy, "SetRosterVisible", &error,
				G_TYPE_BOOLEAN, show,
				G_TYPE_INVALID,
				G_TYPE_INVALID)) {
		g_warning ("Failed to complete 'SetRosterVisible' request. %s", 
			   error->message);
		success = FALSE;
	}

	g_object_unref (bus_proxy);

	return success;
}

