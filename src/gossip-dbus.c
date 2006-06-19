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

#include <libgossip/gossip-debug.h>

#include "gossip-app.h"
#include "gossip-dbus.h"

#define DEBUG_DOMAIN "DBUS"

#define GOSSIP_DBUS_ERROR_DOMAIN "GossipDBus"

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
#define GOSSIP_DBUS_INTERFACE         "org.gnome.Gossip"
#define GOSSIP_DBUS_PATH              "/org/gnome/Gossip"

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
gboolean gossip_dbus_send_message       (GossipDBus   *obj,
					 const gchar  *contact_id,
					 GError      **error);
gboolean gossip_dbus_toggle_roster      (GossipDBus   *obj,
					 GError      **error);
gboolean gossip_dbus_get_roster_visible (GossipDBus   *obj,
					 gboolean     *visible,
					 GError      **error);
gboolean gossip_dbus_get_open_chats     (GossipDBus   *obj,
					 char       ***contacts,
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
	gossip_debug (DEBUG_DOMAIN, "Setting network status %s", up ? "up" : "down");
	
	if (up) {
		gossip_app_net_up ();
	} else {
		gossip_app_net_down ();
	}
 
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
gossip_dbus_toggle_roster (GossipDBus  *obj,
			   GError     **error)
{
	gossip_debug (DEBUG_DOMAIN, "Toggling roster visibility");
	gossip_app_toggle_visibility ();

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

GQuark
gossip_dbus_error_quark (void)
{
        return g_quark_from_static_string (GOSSIP_DBUS_ERROR_DOMAIN);
}

gboolean
gossip_dbus_init_for_session (GossipSession *session)
{
	DBusGConnection *bus;
	DBusGProxy      *bus_proxy;
	GError          *error = NULL;
	GossipDBus      *obj;
	guint            request_name_result;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), FALSE);

	if (saved_session) {
		gossip_debug (DEBUG_DOMAIN, "Already initiated");
		return TRUE;
	}

	gossip_debug (DEBUG_DOMAIN, "Initiating...");
	
	saved_session = g_object_ref (session);

	dbus_g_object_type_install_info (GOSSIP_TYPE_DBUS, &dbus_glib_gossip_dbus_object_info);

	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		g_warning ("Couldn't connect to the session bus");
		return FALSE;
	}
	
	bus_proxy = dbus_g_proxy_new_for_name (bus, 
					       "org.freedesktop.DBus",
					       "/org/freedesktop/DBus",
					       "org.freedesktop.DBus");

	if (!dbus_g_proxy_call (bus_proxy, "RequestName", &error,
				G_TYPE_STRING, GOSSIP_DBUS_SERVICE,
				G_TYPE_UINT, 0,
				G_TYPE_INVALID,
				G_TYPE_UINT, &request_name_result,
				G_TYPE_INVALID)) {
		g_warning ("Failed to acquire %s", GOSSIP_DBUS_SERVICE);
		return FALSE;
	}
	
	obj = g_object_new (GOSSIP_TYPE_DBUS, NULL);

	dbus_g_connection_register_g_object (bus, GOSSIP_DBUS_PATH, G_OBJECT (obj));
	
	gossip_debug (DEBUG_DOMAIN, "Service running");

	return TRUE;
}
