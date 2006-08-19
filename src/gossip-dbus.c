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
#include <dbus/dbus-glib-lowlevel.h>

#include <libgossip/gossip-debug.h>

#include "gossip-app.h"
#include "gossip-dbus.h"
#include "gossip-new-message-dialog.h"

#define DEBUG_DOMAIN "DBUS"

#define GOSSIP_DBUS_ERROR_DOMAIN "GossipDBus"

/* None-generated functions */
static DBusGProxy * dbus_freedesktop_init            (void);
static const gchar *dbus_nm_state_to_string          (guint32     state);
static void         dbus_nm_state_cb                 (DBusGProxy *proxy,
						      guint       state,
						      gpointer    user_data);
static gboolean     dbus_nm_proxy_restart_timeout_cb (gpointer    user_data);
static void         dbus_nm_proxy_notify_cb          (gpointer    data,
						      GObject    *where_the_object_was);
static gboolean     dbus_nm_init                     (void);
static gboolean     dbus_gossip_show                 (gboolean    show);

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

static gboolean gossip_dbus_set_presence         (GossipDBus    *obj,
						  const char    *state,
						  const char    *status,
						  GError       **error);
static gboolean gossip_dbus_set_not_away         (GossipDBus    *obj,
						  GError       **error);
static gboolean gossip_dbus_set_network_status   (GossipDBus    *obj,
						  gboolean       up,
						  GError       **error);
static gboolean gossip_dbus_set_roster_visible   (GossipDBus    *obj,
						  gboolean       visible,
						  GError       **error);
static gboolean gossip_dbus_get_presence         (GossipDBus    *obj,
						  const gchar   *id,
						  char         **state,
						  char         **status,
						  GError       **error);
static gboolean gossip_dbus_get_name             (GossipDBus    *obj,
						  const gchar   *id,
						  char         **name,
						  GError       **error);
static gboolean gossip_dbus_get_roster_visible   (GossipDBus    *obj,
						  gboolean      *visible,
						  GError       **error);
static gboolean gossip_dbus_get_open_chats       (GossipDBus    *obj,
						  char        ***contacts,
						  GError       **error);
static gboolean gossip_dbus_send_message         (GossipDBus    *obj,
						  const gchar   *contact_id,
						  GError       **error);
static gboolean gossip_dbus_new_message          (GossipDBus    *obj,
						  GError       **error);
static gboolean gossip_dbus_toggle_roster        (GossipDBus    *obj,
						  GError       **error);

#include "gossip-dbus-glue.h"

static GossipSession *saved_session = NULL;
static GossipDBus    *gossip_dbus = NULL;
static DBusGProxy    *bus_proxy = NULL;

static DBusGProxy    *nm_proxy = NULL;
static gint           nm_proxy_restart_retries = 0;
static guint          nm_proxy_restart_timeout_id = 0;


static void
gossip_dbus_init (GossipDBus *obj)
{
}

static void
gossip_dbus_class_init (GossipDBusClass *klass)
{
}

static gboolean
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

static gboolean
gossip_dbus_set_not_away (GossipDBus  *obj, 
			  GError     **error)
{
	gossip_debug (DEBUG_DOMAIN, "Setting presence to NOT AWAY");
	gossip_app_set_not_away ();
	
	return TRUE;
}

static gboolean
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

static gboolean
gossip_dbus_set_roster_visible (GossipDBus  *obj,
				gboolean     visible,
				GError     **error)
{
	gossip_debug (DEBUG_DOMAIN, "Setting roster window to be %s",
		      visible ? "shown" : "hidden");

	gossip_app_set_visibility (visible);

	return TRUE;
}

static gboolean
gossip_dbus_get_presence (GossipDBus   *obj, 
			  const gchar  *id,
			  char        **state, 
			  char        **status,
			  GError      **error)
{
	GossipContact  *contact;
	GossipPresence *presence;
	const gchar    *str;

	gossip_debug (DEBUG_DOMAIN, "Getting presence for:'%s'...", 
		      id ? id : "SELF");

	if (id && strlen (id) > 0) {
		contact = gossip_session_find_contact (saved_session, id);
		if (!contact) {
			gossip_debug (DEBUG_DOMAIN, "Contact:'%s' not recognised", id);
			
			g_set_error (error, gossip_dbus_error_quark (), 0, 
				     "Contact:'%s' unrecognised", id);
			
			return FALSE;
		}

		presence = gossip_contact_get_active_presence (contact);
	} else {
		presence = gossip_session_get_presence (saved_session);
	}

	if (!presence) {
		*state = g_strdup_printf ("unavailable");
		*status = NULL;
	} else {
		switch (gossip_presence_get_state (presence)) {
		case GOSSIP_PRESENCE_STATE_AVAILABLE:
			*state = g_strdup ("available");
			break;
		case GOSSIP_PRESENCE_STATE_BUSY:
			*state = g_strdup ("busy");
			break;
		case GOSSIP_PRESENCE_STATE_AWAY:
			*state = g_strdup ("away");
			break;
		case GOSSIP_PRESENCE_STATE_EXT_AWAY:
			*state = g_strdup ("xa");
			break;
		case GOSSIP_PRESENCE_STATE_HIDDEN:
		case GOSSIP_PRESENCE_STATE_UNAVAILABLE:
			*state = g_strdup ("unavailable");
			break;
		}

		str = gossip_presence_get_status (presence);
		if (str) {
			*status = g_strdup (str);
		} else {
			*status = NULL;
		}
	}
	
	return TRUE;
}

static gboolean
gossip_dbus_get_name (GossipDBus   *obj, 
		      const gchar  *id,
		      char        **name, 
		      GError      **error)
{
	GossipContact *contact;
	const gchar   *str;

	gossip_debug (DEBUG_DOMAIN, "Getting name for:'%s'...", 
		      id ? id : "SELF");

	*name = NULL;

	if (id && strlen (id) > 0) {
		contact = gossip_session_find_contact (saved_session, id);
		if (!contact) {
			gossip_debug (DEBUG_DOMAIN, "Contact:'%s' not recognised", id);
			
			g_set_error (error, gossip_dbus_error_quark (), 0, 
				     "Contact:'%s' unrecognised", id);
			
			return FALSE;
		}
	} else {
		gossip_debug (DEBUG_DOMAIN, "Can not get own name, not yet implemented");
		
		g_set_error (error, gossip_dbus_error_quark (), 0, 
			     "Can not get own name, not yet implemented");
		
		return FALSE;
	}

	str = gossip_contact_get_name (contact);
	if (str) {
		*name = g_strdup (str);
	} else {
		*name = NULL;
	}
	
	return TRUE;
}

static gboolean 
gossip_dbus_get_roster_visible (GossipDBus   *obj,
				gboolean     *visible,
				GError      **error)
{
	gossip_debug (DEBUG_DOMAIN, "Getting roster visiblity");

	*visible = gossip_app_is_window_visible ();

	return TRUE;
}

static gboolean 
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

static gboolean
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

static gboolean 
gossip_dbus_new_message (GossipDBus  *obj,
			 GError     **error)
{
	gossip_debug (DEBUG_DOMAIN, "New message");
	gossip_new_message_dialog_show (NULL);

	return TRUE;
}

static gboolean 
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
	DBusGProxy      *proxy;
	GError          *error = NULL;
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
	
	proxy = dbus_freedesktop_init ();
	if (!proxy) {
		return FALSE;
	}

	dbus_g_object_type_install_info (GOSSIP_TYPE_DBUS, &dbus_glib_gossip_dbus_object_info);

	gossip_dbus = g_object_new (GOSSIP_TYPE_DBUS, NULL);

	dbus_g_connection_register_g_object (bus, GOSSIP_DBUS_PATH, G_OBJECT (gossip_dbus));
	
	if (!org_freedesktop_DBus_request_name (proxy,
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
		gdk_notify_startup_complete ();

		exit (EXIT_SUCCESS);
	}
	
	/* Set up GNOME Network Manager if available so we get signals. */
	dbus_nm_init ();

	gossip_debug (DEBUG_DOMAIN, "Ready");

	return TRUE;
}

void 
gossip_dbus_finalize_for_session (void)
{
	if (bus_proxy) {
		g_object_unref (bus_proxy);
	}
	if (nm_proxy) {
		g_object_weak_unref (G_OBJECT (nm_proxy), dbus_nm_proxy_notify_cb, NULL);
		g_object_unref (nm_proxy);
	}

	if (gossip_dbus) {
		g_object_unref (gossip_dbus);
	}
	if (saved_session) {
		g_object_unref (saved_session);
	}
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
	DBusGConnection *bus;
	GError          *error = NULL;

	if (bus_proxy) {
		return bus_proxy;
	}

	gossip_debug (DEBUG_DOMAIN, "Initializing Freedesktop proxy");

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

static gboolean
dbus_nm_proxy_restart_timeout_cb (gpointer user_data)
{
	if (dbus_nm_init ()) {
		nm_proxy_restart_timeout_id = 0;
		return FALSE;
	}

	nm_proxy_restart_retries--;
	if (nm_proxy_restart_retries == 0) {
		nm_proxy_restart_timeout_id = 0;
		return FALSE;
	}

	return TRUE;
}
		
static void
dbus_nm_proxy_notify_cb (gpointer  data,
			 GObject  *where_the_object_was)
{
	nm_proxy = NULL;
	nm_proxy_restart_retries = 5;

	if (nm_proxy_restart_timeout_id) {
		g_source_remove (nm_proxy_restart_timeout_id);
	}
	
	nm_proxy_restart_timeout_id = 
		g_timeout_add (10*1000,
			       dbus_nm_proxy_restart_timeout_cb,
			       NULL);
}

static gboolean
dbus_nm_init (void)
{
	DBusGConnection *bus;
	DBusConnection  *conn;

	if (nm_proxy) {
		return TRUE;
	}

	gossip_debug (DEBUG_DOMAIN, "Initialising Network Manager proxy");

	bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
	if (!bus) {
		g_warning ("Could not connect to system bus");
		return FALSE;
	}

	/* Note that we are kind of leaking this here, although it doesn't
	 * really matter since there is only one anyway, during the whole
	 * session.
	 */
	conn = dbus_g_connection_get_connection (bus);
	dbus_connection_set_exit_on_disconnect (conn, FALSE);
	
	nm_proxy = dbus_g_proxy_new_for_name (bus,
					       NM_DBUS_SERVICE, 
					       NM_DBUS_PATH, 
					       NM_DBUS_INTERFACE);

	if (!nm_proxy) {
		/* Don't warn because the user might just not have nm. */
		return FALSE;
	}

	g_object_weak_ref (G_OBJECT (nm_proxy), dbus_nm_proxy_notify_cb, NULL);
	
	dbus_g_object_register_marshaller (g_cclosure_marshal_VOID__UINT, 
					   G_TYPE_NONE, G_TYPE_UINT, G_TYPE_INVALID);

	dbus_g_proxy_add_signal (nm_proxy, "StateChange",
				 G_TYPE_UINT, G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (nm_proxy, "StateChange", 
				     G_CALLBACK (dbus_nm_state_cb),
				     NULL, NULL);

	return TRUE;
} 

gboolean 
gossip_dbus_nm_get_state (gboolean *connected)
{
	GError  *error = NULL;
	guint32  state;
	
	g_return_val_if_fail (connected != NULL, FALSE);

	/* Set the initial value of connected in case we have to return. */
	*connected = FALSE;

	if (nm_proxy_restart_timeout_id) {
		/* We are still trying to reconnect to the restarted bus. */
		return FALSE;
	}

	/* Make sure we have set up Network Manager connections */
	if (!dbus_nm_init ()) {
		return FALSE;
	}

	if (!dbus_g_proxy_call (nm_proxy, "state", &error,
				G_TYPE_INVALID, 
				G_TYPE_UINT, &state, G_TYPE_INVALID)) {
		gossip_debug (DEBUG_DOMAIN, "Failed to complete 'state' request. %s", 
			      error->message);
		return FALSE;
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
	DBusGProxy      *proxy;
	GError          *error = NULL;
	gboolean         success = TRUE;
	
	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_warning ("Could not connect to session bus");
		return FALSE;
	}

	/* We are doing this because normally we wouldn't have a
	 * connection to ourselves.
	 */
	proxy = dbus_g_proxy_new_for_name (bus, 
					   GOSSIP_DBUS_SERVICE,
					   GOSSIP_DBUS_PATH,
					   GOSSIP_DBUS_INTERFACE);
	
	if (!proxy) {
		g_warning ("Could not connect to other instance of Gossip");
		return FALSE;
	}
		
	if (!dbus_g_proxy_call (proxy, "SetRosterVisible", &error,
				G_TYPE_BOOLEAN, show,
				G_TYPE_INVALID,
				G_TYPE_INVALID)) {
		g_warning ("Failed to complete 'SetRosterVisible' request. %s", 
			   error->message);
		success = FALSE;
	}

	g_object_unref (proxy);

	return success;
}

