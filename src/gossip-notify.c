/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
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
#include <glib/gi18n.h>
#include <libnotify/notify.h>

#include "gossip-app.h"
#include "gossip-contact-info-dialog.h"
#include "gossip-notify.h"
#include "gossip-stock.h"

#define DEBUG_MSG(x)   
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n");   */

#define NOTIFY_MESSAGE_TIMEOUT 20000

#define NOTIFY_MESSAGE_MAX_LEN 64 /* Max length of the body part of a
				   * message we show in the notification.
				   */

static const gchar *       notify_get_status_from_presence (GossipPresence     *presence);
static void                notify_online_send_message_cb   (NotifyNotification *notify,
							    gchar              *label,
							    GossipContact      *contact);
static void                notify_new_message_contact_cb   (NotifyNotification *notify,
							    gchar              *label,
							    GossipEventManager *event_manager);
static NotifyNotification *notify_new_message              (GossipEventManager *event_manager,
							    GossipMessage      *message);
static void                notify_event_added_cb           (GossipEventManager *event_manager,
							    GossipEvent        *event,
							    gpointer            user_data);
static gboolean            notify_event_remove_foreach     (gpointer            key,
							    GossipEvent        *event,
							    GossipEvent        *event_to_compare);
static void                notify_event_removed_cb         (GossipEventManager *event_manager,
							    GossipEvent        *event,
							    gpointer            user_data);
static void                notify_event_destroy_cb         (NotifyNotification *notify);

enum {
	NOTIFY_SHOW_MESSAGE,
	NOTIFY_SHOW_ROSTER,
};

static GHashTable *message_notifications = NULL;
static GHashTable *event_notifications = NULL;
static GtkWidget  *attach_widget = NULL;

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

static void
notify_online_send_message_cb (NotifyNotification *notify,
			       gchar              *label,
			       GossipContact      *contact)
{
	GossipSession     *session;
	GossipChatManager *chat_manager;

	session = gossip_app_get_session ();
	chat_manager = gossip_app_get_chat_manager ();
	gossip_chat_manager_show_chat (chat_manager, contact);
}

void
gossip_notify_contact_online (GossipContact *contact)
{
	GossipPresence     *presence;
	NotifyNotification *notify;
	GdkPixbuf          *pixbuf;
	gchar              *title;
	const gchar        *status;
	GError             *error = NULL;

	DEBUG_MSG (("Notify: Contact online:'%s'", 
		   gossip_contact_get_id (contact)));

	presence = gossip_contact_get_active_presence (contact);
	pixbuf = gossip_pixbuf_for_presence (presence);

	title = g_strdup_printf (_("%s has come online"), 
			       gossip_contact_get_name (contact));
	status = notify_get_status_from_presence (presence);

	notify = notify_notification_new (title, status, NULL, NULL);
	notify_notification_set_urgency (notify, NOTIFY_URGENCY_LOW);
	notify_notification_set_icon_from_pixbuf (notify, pixbuf);

	if (attach_widget) {
		notify_notification_attach_to_widget (notify, attach_widget);
	}

	notify_notification_add_action (notify, "send_message", _("Send Message"),
					(NotifyActionCallback) notify_online_send_message_cb,
					g_object_ref (contact), NULL);

	if (!notify_notification_show (notify, &error)) {
		g_warning ("Failed to send notification: %s",
			   error->message);
		g_error_free (error);
	}
	
	g_free (title);

	if (pixbuf) {
		g_object_unref (pixbuf);
	}
}

void
gossip_notify_contact_offline (GossipContact *contact)
{
	GossipPresence     *presence;
	NotifyNotification *notify;
	GdkPixbuf          *pixbuf;
	gchar              *title;
	const gchar        *status;
	GError             *error = NULL;

	DEBUG_MSG (("Notify: Contact offline:'%s'", 
		   gossip_contact_get_id (contact)));

	presence = gossip_contact_get_active_presence (contact);
	pixbuf = gossip_pixbuf_for_presence (presence);

	title = g_strdup_printf (_("%s has gone offline"), 
				 gossip_contact_get_name (contact));
	status = notify_get_status_from_presence (presence);

	notify = notify_notification_new (title, status, NULL, NULL);
	notify_notification_set_urgency (notify, NOTIFY_URGENCY_LOW);
	notify_notification_set_icon_from_pixbuf (notify, pixbuf);

	if (attach_widget) {
		notify_notification_attach_to_widget (notify, attach_widget);
	}

	if (!notify_notification_show (notify, &error)) {
		g_warning ("Failed to send notification: %s",
			   error->message);
		g_error_free (error);
	}
	
	g_free (title);

	if (pixbuf) {
		g_object_unref (pixbuf);
	}					
}

static void
notify_new_message_default_cb (NotifyNotification *notify,
			       gchar              *label,
			       GossipEventManager *event_manager)
{
	GossipEvent   *event;
	GossipMessage *message;
	GossipContact *contact = NULL;

	event = g_hash_table_lookup (event_notifications, notify);
	if (event) {
		message = GOSSIP_MESSAGE (gossip_event_get_data (event));
		contact = gossip_message_get_sender (message);
	
		gossip_event_manager_activate (event_manager, event);
	} else {
		g_warning ("No event found for NotifyNotification: %p", notify);
	}

	g_hash_table_remove (event_notifications, notify);
	g_hash_table_remove (message_notifications, contact);

	g_object_unref (event_manager);
}

static void
notify_new_message_contact_cb (NotifyNotification *notify,
			       gchar              *label,
			       GossipEventManager *event_manager)
{
	GossipEvent   *event;
	GossipMessage *message;
	GossipContact *contact = NULL;

	event = g_hash_table_lookup (event_notifications, notify);
	if (event) {
		message = GOSSIP_MESSAGE (gossip_event_get_data (event));
		contact = gossip_message_get_sender (message);

		gossip_contact_info_dialog_show (contact);
	} else {
		g_warning ("No event found for Notification: %p", notify);
	}

	g_hash_table_remove (event_notifications, notify);
	g_hash_table_remove (message_notifications, contact);

	g_object_unref (event_manager);
}

static NotifyNotification *
notify_new_message (GossipEventManager *event_manager,
		    GossipMessage      *message)
{ 
	GossipContact      *contact;
	NotifyNotification *notify = NULL;
	GdkPixbuf          *pixbuf;
	gchar              *title;
	gchar              *msg;
	const gchar        *body;
	gchar              *str;
	gint                len;
	GError             *error = NULL;

	contact = gossip_message_get_sender (message);

	DEBUG_MSG (("Notify: New message:'%s'", 
		   gossip_contact_get_id (contact)));

	pixbuf = gossip_pixbuf_from_stock (GOSSIP_STOCK_MESSAGE, GTK_ICON_SIZE_MENU);

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

	notify = notify_notification_new (title, msg, NULL, NULL);
	notify_notification_set_urgency (notify, NOTIFY_URGENCY_NORMAL);
	notify_notification_set_icon_from_pixbuf (notify, pixbuf);
	notify_notification_set_timeout (notify, NOTIFY_MESSAGE_TIMEOUT);

	if (attach_widget) {
		notify_notification_attach_to_widget (notify, attach_widget);
	}
	
	notify_notification_add_action (notify, "default", _("Default"),
					(NotifyActionCallback) notify_new_message_default_cb,
					g_object_ref (event_manager), NULL);
	notify_notification_add_action (notify, "respond", _("Respond"),
					(NotifyActionCallback) notify_new_message_default_cb,
					g_object_ref (event_manager), NULL);
	notify_notification_add_action (notify, "contact", _("Contact Information"),
					(NotifyActionCallback) notify_new_message_contact_cb,
					g_object_ref (event_manager), NULL);
	
	if (!notify_notification_show (notify, &error)) {
		g_warning ("Failed to send notification: %s",
			   error->message);
		g_error_free (error);
	}

	g_free (msg);
	g_free (title);

	if (pixbuf) {
		g_object_unref (pixbuf);
	}

	return notify;
}

static void
notify_event_added_cb (GossipEventManager *event_manager,
		       GossipEvent        *event,
		       gpointer            user_data)
{
	GossipEventType type;
	
	type = gossip_event_get_type (event);

	if (type == GOSSIP_EVENT_NEW_MESSAGE) {
		NotifyNotification  *notify;
		GossipMessage       *message;
		GossipContact       *contact;
		
		message = GOSSIP_MESSAGE (gossip_event_get_data (event));
		contact = gossip_message_get_sender (message);

		/* Find out if there are any other messages waiting,
		 * if not, show a notification.
		 */
		if (! g_hash_table_lookup (message_notifications, contact)) {
			notify = notify_new_message (event_manager, message);
			g_hash_table_insert (message_notifications,
					     contact,
					     g_object_ref (event));
  			g_hash_table_insert (event_notifications,   
  					     notify,   
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

static void
notify_event_destroy_cb (NotifyNotification *notify)
{
	notify_notification_close (notify, NULL);
}

void
gossip_notify_set_attach_widget (GtkWidget *new_attach_widget)
{
	if (attach_widget) {
		g_object_remove_weak_pointer (G_OBJECT (attach_widget),
					      (gpointer) &attach_widget);
	}
	
	attach_widget = new_attach_widget;
	if (attach_widget) {
		g_object_add_weak_pointer (G_OBJECT (new_attach_widget),
					   (gpointer) &attach_widget);
	}
}

void
gossip_notify_init (GossipSession      *session,
		    GossipEventManager *event_manager)
{
	g_return_if_fail (GOSSIP_IS_SESSION (session));
	g_return_if_fail (GOSSIP_IS_EVENT_MANAGER (event_manager));
	
	DEBUG_MSG (("Notify: Initiating..."));
	
	if (!notify_init (PACKAGE_NAME)) {
		g_warning ("Cannot initialize Notify integration");
		return;
	}

	message_notifications = g_hash_table_new_full (gossip_contact_hash,
						       gossip_contact_equal,
						       (GDestroyNotify) g_object_unref,
						       (GDestroyNotify) g_object_unref);

	event_notifications = g_hash_table_new_full (g_direct_hash,
						     g_direct_equal,
						     (GDestroyNotify) notify_event_destroy_cb,
						     (GDestroyNotify) g_object_unref);

	g_signal_connect (event_manager, "event-added",
			  G_CALLBACK (notify_event_added_cb),
			  NULL);
	g_signal_connect (event_manager, "event-removed",
			  G_CALLBACK (notify_event_removed_cb),
			  NULL);
}
