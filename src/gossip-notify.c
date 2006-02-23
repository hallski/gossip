/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Imendio AB
 * Copyright (C) 2005 Ross Burton <ross@openedhand.com>
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

#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>

#include <libnotify/notify.h>

#include "gossip-app.h"
#include "gossip-contact-info-dialog.h"
#include "gossip-notify.h"
#include "gossip-stock.h"

#define d(x)

#define NOTIFY_MESSAGE_TIMEOUT 10

#define NOTIFY_MESSAGE_MAX_LEN 64 /* Max length of the body part of a
				   * message we show in the notification.
				   */

static gchar *       notify_get_filename_from_stock    (const gchar        *stock);
static gchar *       notify_get_filename_from_presence (GossipPresence     *presence);
static const gchar * notify_get_status_from_presence   (GossipPresence     *presence);
static void          notify_new_message_cb             (NotifyHandle       *handle,
							guint32             id,
							GossipEventManager *event_manager);
static NotifyHandle *notify_new_message                (GossipEventManager *event_manager,
							GossipMessage      *message);
static void          notify_event_added_cb             (GossipEventManager *event_manager,
							GossipEvent        *event,
							gpointer            user_data);
static gboolean      notify_event_remove_foreach       (gpointer            key,
							GossipEvent        *event,
							GossipEvent        *event_to_compare);
static void          notify_event_removed_cb           (GossipEventManager *event_manager,
							GossipEvent        *event,
							gpointer            user_data);

enum {
	NOTIFY_SHOW_MESSAGE,
	NOTIFY_SHOW_ROSTER,
};

static GHashTable *message_notifications = NULL;
/* static GHashTable *presence_notifications = NULL; */
static GHashTable *event_notifications = NULL;


static gchar *
notify_get_filename_from_stock (const gchar *stock)
{
	gchar *filename;

	filename = g_strdup_printf (IMAGEDIR "/%s.png", stock);
	return filename;
}


static gchar *
notify_get_filename_from_presence (GossipPresence *presence)
{
	const gchar *stock; 

	stock = GOSSIP_STOCK_OFFLINE;

	if (presence) {
		switch (gossip_presence_get_state (presence)) {
		case GOSSIP_PRESENCE_STATE_AVAILABLE:
			stock = GOSSIP_STOCK_AVAILABLE;
			break;
		case GOSSIP_PRESENCE_STATE_BUSY:
			stock = GOSSIP_STOCK_BUSY;
			break;
		case GOSSIP_PRESENCE_STATE_AWAY:
			stock = GOSSIP_STOCK_AWAY;
			break;
		case GOSSIP_PRESENCE_STATE_EXT_AWAY:
			stock = GOSSIP_STOCK_EXT_AWAY;
			break;
		default:
			break;
		}
	}

	return notify_get_filename_from_stock (stock);
}

static const gchar *
notify_get_status_from_presence (GossipPresence *presence)
{
	const gchar *status;

	status = gossip_presence_get_status (presence);
	if (!status) {
		GossipPresenceState state;

		state = gossip_presence_get_state (presence);
		status = gossip_presence_state_get_default_status (state);
	}
	
	return status;
}

void
gossip_notify_contact_online (GossipContact *contact)
{
	GossipPresence *presence;
	NotifyIcon     *icon = NULL;
	NotifyHandle   *handle;
	gchar          *filename;
	gchar          *title;
	const gchar    *status;

	d(g_print ("Notify: Contact online:'%s'\n", 
		   gossip_contact_get_id (contact)));

	presence = gossip_contact_get_active_presence (contact);

	filename = notify_get_filename_from_presence (presence);
	icon = notify_icon_new_from_uri (filename);
	g_free (filename);

	title = g_strdup_printf (_("%s has come online"), 
			       gossip_contact_get_name (contact));
	status = notify_get_status_from_presence (presence);
	
	handle = notify_send_notification (NULL,
					   NULL, 
					   NOTIFY_URGENCY_NORMAL,
					   title,
					   status,
					   icon,   /* icon */
					   TRUE,   /* should auto disappear */
					   0,      /* timeout */
					   NULL,   /* hints */
					   NULL,   /* user data */
					   0,      /* number of actions */ 
					   NULL);  /* actions (uint32, string, callback) */
	
	g_free (title);

	if (icon) {
		notify_icon_destroy (icon);
	}
}

void
gossip_notify_contact_offline (GossipContact *contact)
{
	GossipPresence *presence;
	NotifyIcon     *icon = NULL;
	NotifyHandle   *handle;
	gchar          *filename;
	gchar          *title;
	const gchar    *status;

	d(g_print ("Notify: Contact offline:'%s'\n", 
		   gossip_contact_get_id (contact)));

	presence = gossip_contact_get_active_presence (contact);

	filename = notify_get_filename_from_presence (presence);
	icon = notify_icon_new_from_uri (filename);
	g_free (filename);

	title = g_strdup_printf (_("%s has gone offline"), 
				 gossip_contact_get_name (contact));
	status = notify_get_status_from_presence (presence);
	
	handle = notify_send_notification (NULL, 
					   NULL, 
					   NOTIFY_URGENCY_NORMAL,
					   title,
					   status,
					   icon,   /* icon */
					   TRUE,   /* should auto disappear */
					   0,      /* timeout */
					   NULL,   /* hints */
					   NULL,   /* user data */
					   0,      /* number of actions */ 
					   NULL);  /* actions (uint32, string, callback) */

	g_free (title);

	if (icon) {
		notify_icon_destroy (icon);
	}
}

static void
notify_new_message_cb (NotifyHandle       *handle,
		       guint32             id,
		       GossipEventManager *event_manager)
{
	GossipEvent   *event;
	GossipMessage *message;
	GossipContact *contact = NULL;

	event = g_hash_table_lookup (event_notifications, handle);
	if (event) {
		message = GOSSIP_MESSAGE (gossip_event_get_data (event));
		contact = gossip_message_get_sender (message);
	
		switch (id) {
		case 0:
		case 1:
			gossip_event_manager_activate (event_manager, event);
			break;
		case 2:
			gossip_contact_info_dialog_show (contact);
			break;
		}
	} else {
		g_warning ("No event found for NotifyHandle:0x%.8x\n", 
			   (gint) handle);
	}

	if (id == 0 || id == 1) {
		g_hash_table_remove (event_notifications, handle);
		g_hash_table_remove (message_notifications, contact);

		g_object_unref (event_manager);
	}
}       

static NotifyHandle *
notify_new_message (GossipEventManager *event_manager,
		    GossipMessage      *message)
{
	GossipContact *contact;
	NotifyIcon    *icon = NULL;
	NotifyHandle  *handle;
	gchar         *filename;
	gchar         *title;
	gchar         *msg;
	const gchar   *body;
	gchar         *str;
	gint           len;

	contact = gossip_message_get_sender (message);

	d(g_print ("Notify: New message:'%s'\n", 
		   gossip_contact_get_id (contact)));

	filename = notify_get_filename_from_stock (GOSSIP_STOCK_MESSAGE);
	icon = notify_icon_new_from_uri (filename);
	g_free (filename);

	title = g_strdup_printf (_("New message from %s"), 
				 gossip_contact_get_name (contact));

	body = gossip_message_get_body (message);
	len = g_utf8_strlen (body, -1);
	len = MIN (len, NOTIFY_MESSAGE_MAX_LEN);
	str = g_strndup (body, len);

	msg = g_strdup_printf ("\"%s%s\"",
			       str, 
			       len >= NOTIFY_MESSAGE_MAX_LEN ? "..." : "");
	g_free (str);

	handle = 
		notify_send_notification (NULL, 
					  "new_message", 
					  NOTIFY_URGENCY_NORMAL,
					  title,
					  msg,
					  icon,  
					  TRUE,   /* should auto disappear */
					  NOTIFY_MESSAGE_TIMEOUT,
					  NULL,   /* hints */
					  g_object_ref (event_manager),
					  3,      /* number of actions */ 
					  0, "default", notify_new_message_cb,
					  1, _("Respond"), notify_new_message_cb,
					  2, _("Contact Information"), notify_new_message_cb);

	g_free (msg);
	g_free (title);

	if (icon) {
		notify_icon_destroy (icon);
	}

	return handle;
}

static void
notify_event_added_cb (GossipEventManager *event_manager,
		       GossipEvent        *event,
		       gpointer            user_data)
{
	GossipEventType type;
	
	type = gossip_event_get_type (event);

	if (type == GOSSIP_EVENT_NEW_MESSAGE) {
		NotifyHandle  *handle;
		GossipMessage *message;
		GossipContact *contact;
		
		message = GOSSIP_MESSAGE (gossip_event_get_data (event));
		contact = gossip_message_get_sender (message);

		/* Find out if there are any other messages waiting,
		 * if not, show a notification.
		 */
		if (! g_hash_table_lookup (message_notifications, contact)) {
			handle = notify_new_message (event_manager, message);
			g_hash_table_insert (message_notifications,
					     contact,
					     g_object_ref (event));
  			g_hash_table_insert (event_notifications,   
  					     handle,   
  					     g_object_ref (event));  
		}
	}
}

static gboolean
notify_event_remove_foreach (gpointer key,
			     GossipEvent *event,
			     GossipEvent *event_to_compare)
{
	if (gossip_event_equal (event, event_to_compare)) {
		return TRUE;
	}

	return FALSE;
}

static void
notify_event_removed_cb (GossipEventManager *event_manager,
			 GossipEvent        *event,
			 gpointer            user_data)
{
	g_hash_table_foreach_remove (message_notifications, 
				     (GHRFunc) notify_event_remove_foreach,
				     event);
	g_hash_table_foreach_remove (event_notifications, 
				     (GHRFunc) notify_event_remove_foreach,
				     event);
}

void
gossip_notify_init (GossipSession      *session,
		    GossipEventManager *event_manager)
{
	g_return_if_fail (GOSSIP_IS_SESSION (session));
	g_return_if_fail (GOSSIP_IS_EVENT_MANAGER (event_manager));
	
	d(g_print ("Notify: Initiating...\n"));

	if (!notify_glib_init (PACKAGE_NAME, NULL)) {
		g_warning ("Cannot initialise Notify integration");
		return;
	}

	message_notifications = g_hash_table_new_full (gossip_contact_hash,
						       gossip_contact_equal,
						       (GDestroyNotify) g_object_unref,
						       (GDestroyNotify) g_object_unref);

	event_notifications = g_hash_table_new_full (g_direct_hash,
						     g_direct_equal,
						     (GDestroyNotify) notify_close,
						     (GDestroyNotify) g_object_unref);

	g_signal_connect (event_manager, "event-added",
			  G_CALLBACK (notify_event_added_cb),
			  NULL);
	g_signal_connect (event_manager, "event-removed",
			  G_CALLBACK (notify_event_removed_cb),
			  NULL);

}
