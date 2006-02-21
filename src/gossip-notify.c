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
#include "gossip-notify.h"
#include "gossip-stock.h"

#define d(x) x


static gchar *      notify_presence_get_filename (GossipPresence *presence);
static const gchar *notify_presence_get_status   (GossipPresence *presence);


static gchar *
notify_presence_get_filename (GossipPresence *presence)
{
	gchar       *filename;
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

	filename = g_strdup_printf (IMAGEDIR "/%s.png", stock);
	return filename;
}

static const gchar *
notify_presence_get_status (GossipPresence *presence)
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

	filename = notify_presence_get_filename (presence);
	icon = notify_icon_new_from_uri (filename);
	g_free (filename);

	title = g_strdup_printf (_("%s has come online"), 
			       gossip_contact_get_name (contact));
	status = notify_presence_get_status (presence);
	
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

	filename = notify_presence_get_filename (presence);
	icon = notify_icon_new_from_uri (filename);
	g_free (filename);

	title = g_strdup_printf (_("%s has gone offline"), 
				 gossip_contact_get_name (contact));
	status = notify_presence_get_status (presence);
	
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
gossip_notify_new_message (GossipContact *contact)
{
	GossipPresence *presence;
	NotifyIcon     *icon = NULL;
	NotifyHandle   *handle;
	gchar          *title;
	gchar          *status;

	d(g_print ("Notify: New Message:'%s'\n", 
		   gossip_contact_get_id (contact)));

	presence = gossip_contact_get_active_presence (contact);

	title = g_strdup_printf (_("You have a new message"));
	status = g_strdup_printf (_("From %s"), gossip_contact_get_name (contact));
	
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

	g_free (status);
	g_free (title);

	if (icon) {
		notify_icon_destroy (icon);
	}
}

void
gossip_notify_init (GossipSession *session)
{
	g_return_if_fail (GOSSIP_IS_SESSION (session));
	
	d(g_print ("Notify: Initiating...\n"));

	if (!notify_glib_init (PACKAGE_NAME, NULL)) {
		g_warning ("Cannot initialise Notify integration");
		return;
	}
}
