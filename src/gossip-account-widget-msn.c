/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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
 * 
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libgossip/gossip-protocol.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-utils.h>
#include <libgossip/gossip-vcard.h>
#include <libgossip/gossip-account-manager.h>

#include "gossip-account-widget-msn.h"
#include "gossip-app.h"
#include "gossip-marshal.h"
#include "gossip-ui-utils.h"

typedef struct {
	GossipAccount *account;
	gboolean       account_changed;

	GtkWidget     *vbox_settings;

	GtkWidget     *button_forget;

	GtkWidget     *entry_id;
	GtkWidget     *entry_server;
	GtkWidget     *entry_password;
	GtkWidget     *entry_port;
} GossipAccountWidgetMSN;

static void     account_widget_msn_save                      (GossipAccountWidgetMSN *settings);
static void     account_widget_msn_protocol_connected_cb     (GossipSession          *session,
							      GossipAccount          *account,
							      GossipProtocol         *protocol,
							      GossipAccountWidgetMSN *settings);
static void     account_widget_msn_protocol_disconnected_cb  (GossipSession          *session,
							      GossipAccount          *account,
							      GossipProtocol         *protocol,
							      gint                    reason,
							      GossipAccountWidgetMSN *settings);
static gboolean account_widget_msn_entry_focus_cb            (GtkWidget              *widget,
							      GdkEventFocus          *event,
							      GossipAccountWidgetMSN *settings);
static void     account_widget_msn_entry_changed_cb          (GtkWidget              *widget,
							      GossipAccountWidgetMSN *settings);
static void     account_widget_msn_entry_port_insert_text_cb (GtkEditable            *editable,
							      gchar                  *new_text,
							      gint                    len,
							      gint                   *position,
							      GossipAccountWidgetMSN *settings);
static void     account_widget_msn_button_forget_clicked_cb  (GtkWidget              *button,
							      GossipAccountWidgetMSN *settings);
static void     account_widget_msn_destroy_cb                (GtkWidget              *widget,
							      GossipAccountWidgetMSN *settings);
static void     account_widget_msn_setup                     (GossipAccountWidgetMSN *settings);

static void
account_widget_msn_save (GossipAccountWidgetMSN *settings)
{
	GossipSession        *session;
	GossipAccountManager *manager;
 	const gchar          *id;
 	const gchar          *password;
 	const gchar          *server;
 	const gchar          *port_str;
	guint                 port;

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	id = gtk_entry_get_text (GTK_ENTRY (settings->entry_id));
	password = gtk_entry_get_text (GTK_ENTRY (settings->entry_password));
	server = gtk_entry_get_text (GTK_ENTRY (settings->entry_server));
	port_str = gtk_entry_get_text (GTK_ENTRY (settings->entry_port));
	port = strtol (port_str, NULL, 10);

	gossip_account_set_param (settings->account,
				  "password", password,
				  "server", server,
				  "port", port,
				  "account", id,
				  NULL);

	gossip_account_manager_store (manager);

	settings->account_changed = FALSE;
}

static void
account_widget_msn_protocol_connected_cb (GossipSession             *session,
					  GossipAccount             *account,
					  GossipProtocol            *protocol,
					  GossipAccountWidgetMSN *settings)
{
	if (gossip_account_equal (account, settings->account)) {
		/* FIXME: IMPLEMENT */
	}
}

static void
account_widget_msn_protocol_disconnected_cb (GossipSession             *session,
					     GossipAccount             *account,
					     GossipProtocol            *protocol,
					     gint                       reason,
					     GossipAccountWidgetMSN *settings)
{
	if (gossip_account_equal (account, settings->account)) {
		/* FIXME: IMPLEMENT */
	}
}

static void
account_widget_msn_protocol_error_cb (GossipSession             *session,
				      GossipProtocol            *protocol,
				      GossipAccount             *account,
				      GError                    *error,
				      GossipAccountWidgetMSN *settings)
{
	if (gossip_account_equal (account, settings->account)) {
		/* FIXME: HANDLE ERRORS */
	}	
}

static gboolean
account_widget_msn_entry_focus_cb (GtkWidget                 *widget,
				   GdkEventFocus             *event,
				   GossipAccountWidgetMSN *settings)
{
	if (widget == settings->entry_id) {
		GossipSession  *session;
		GossipProtocol *protocol;

		session = gossip_app_get_session ();
		protocol = gossip_session_get_protocol (session, settings->account);

		if (protocol) {
			const gchar *str;

			str = gtk_entry_get_text (GTK_ENTRY (widget));

			if (!gossip_protocol_is_valid_username (protocol, str)) {
				gossip_account_get_param (settings->account,
							  "account", &str,
							  NULL);
				settings->account_changed = FALSE;
			} else {
				gchar *server;

				server = gossip_protocol_get_default_server (protocol, str);
				gtk_entry_set_text (GTK_ENTRY (settings->entry_server), server);
				g_free (server);
			}

			gtk_entry_set_text (GTK_ENTRY (widget), str);
		}
	}

	if (widget == settings->entry_password ||
	    widget == settings->entry_server) {
		const gchar *str;

		str = gtk_entry_get_text (GTK_ENTRY (widget));
		if (G_STR_EMPTY (str)) {
			if (widget == settings->entry_password) {
				gossip_account_get_param (settings->account, "password", &str, NULL);
			} else if (widget == settings->entry_server) {
				gossip_account_get_param (settings->account, "server", &str, NULL);
			}

			gtk_entry_set_text (GTK_ENTRY (widget), str ? str : "");
			settings->account_changed = FALSE;
		}
	}

	if (widget == settings->entry_port) {
		const gchar *str;
		
		str = gtk_entry_get_text (GTK_ENTRY (widget));
		if (G_STR_EMPTY (str)) {
			gchar   *port_str;
			guint    port;

			gossip_account_get_param (settings->account, "port", &port, NULL);
			port_str = g_strdup_printf ("%d", port);
			gtk_entry_set_text (GTK_ENTRY (widget), port_str);
			g_free (port_str);

			settings->account_changed = FALSE;
		}
	}

	if (settings->account_changed) {
 		account_widget_msn_save (settings); 
	}

	return FALSE;
}

static void
account_widget_msn_entry_changed_cb (GtkWidget                 *widget,
				     GossipAccountWidgetMSN *settings)
{
	if (widget == settings->entry_port) {
		const gchar *str;
		gint         pnr;

		str = gtk_entry_get_text (GTK_ENTRY (widget));
		pnr = strtol (str, NULL, 10);
		
		if ((pnr <= 0 || pnr >= 65556) && !G_STR_EMPTY (str)) {
			guint  port;
			gchar *port_str;
			
			gossip_account_get_param (settings->account, "port", &port, NULL);
			port_str = g_strdup_printf ("%d", port);
			g_signal_handlers_block_by_func (settings->entry_port, 
							 account_widget_msn_entry_changed_cb, 
							 settings);
			gtk_entry_set_text (GTK_ENTRY (widget), port_str);
			g_signal_handlers_unblock_by_func (settings->entry_port, 
							   account_widget_msn_entry_changed_cb, 
							   settings);
			g_free (port_str);

			return;
		}
	} else if (widget == settings->entry_password) {
		const gchar *str;

		str = gtk_entry_get_text (GTK_ENTRY (widget));
		gtk_widget_set_sensitive (settings->button_forget, !G_STR_EMPTY (str));
	}

	/* Save */
	settings->account_changed = TRUE;
}

static void
account_widget_msn_entry_port_insert_text_cb (GtkEditable               *editable,
					      gchar                     *new_text,
					      gint                       len,
					      gint                      *position,
					      GossipAccountWidgetMSN *settings)
{
	gint i;

	for (i = 0; i < len; ++i) {
		gchar *ch;

		ch = new_text + i;
		if (!isdigit (*ch)) {
			g_signal_stop_emission_by_name (editable,
							"insert-text");
			return;
		}
	}
}

static void
account_widget_msn_button_forget_clicked_cb (GtkWidget                 *button,
					     GossipAccountWidgetMSN *settings)
{
	GossipSession        *session;
	GossipAccountManager *manager;

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	gossip_account_set_param (settings->account, "password", "", NULL);
	gossip_account_manager_store (manager);

	gtk_entry_set_text (GTK_ENTRY (settings->entry_password), "");
}

static void
account_widget_msn_destroy_cb (GtkWidget                 *widget,
			       GossipAccountWidgetMSN *settings)
{
	GossipSession *session;

	if (settings->account_changed) {
 		account_widget_msn_save (settings); 
	}

	session = gossip_app_get_session ();

	g_signal_handlers_disconnect_by_func (session,
					      account_widget_msn_protocol_connected_cb,
					      settings);

	g_signal_handlers_disconnect_by_func (session,
					      account_widget_msn_protocol_disconnected_cb,
					      settings);

	g_signal_handlers_disconnect_by_func (session,
					      account_widget_msn_protocol_error_cb,
					      settings);

	g_object_unref (settings->account);
	g_free (settings);
}

static void
account_widget_msn_setup (GossipAccountWidgetMSN *settings)
{
	GossipSession  *session;
	GossipProtocol *protocol;
	guint           port;
	gchar          *port_str; 
	const gchar    *id;
	const gchar    *server;
	const gchar    *password;
	gboolean        is_connected;

	session = gossip_app_get_session ();
	protocol = gossip_session_get_protocol (session, settings->account);

	gossip_account_get_param (settings->account,
				  "password", &password,
				  "server", &server,
				  "port", &port,
				  "account", &id,
				  NULL);

	gtk_entry_set_text (GTK_ENTRY (settings->entry_id), id ? id : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_password), password ? password : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_server), server ? server : "");

	port_str = g_strdup_printf ("%d", port);
	gtk_entry_set_text (GTK_ENTRY (settings->entry_port), port_str);
	g_free (port_str);

	gtk_widget_set_sensitive (settings->button_forget, !G_STR_EMPTY (password));

	/* Set up connection specific buttons */
	is_connected = gossip_session_is_connected (session, settings->account);

	/* Set up protocol signals */
	g_signal_connect (session, "protocol-connected",
			  G_CALLBACK (account_widget_msn_protocol_connected_cb),
			  settings);

	g_signal_connect (session, "protocol-disconnected",
			  G_CALLBACK (account_widget_msn_protocol_disconnected_cb),
			  settings);

	g_signal_connect (session, "protocol-error",
			  G_CALLBACK (account_widget_msn_protocol_error_cb),
			  settings);
}

GtkWidget *
gossip_account_widget_msn_new (GossipAccount *account,
			       GtkWidget     *label_name)
{
	GossipAccountWidgetMSN *settings;
	GladeXML               *glade;
	GtkSizeGroup           *size_group;
	GtkWidget              *label_id;
	GtkWidget              *label_password;
	GtkWidget              *label_server;
	GtkWidget              *label_port; 

	settings = g_new0 (GossipAccountWidgetMSN, 1);
	settings->account = g_object_ref (account);

	glade = gossip_glade_get_file ("main.glade",
				       "vbox_msn_settings",
				       NULL,
				       "vbox_msn_settings", &settings->vbox_settings,
				       "button_forget", &settings->button_forget,
				       "label_id", &label_id,
				       "label_password", &label_password,
				       "label_server", &label_server,
				       "label_port", &label_port,
				       "entry_id", &settings->entry_id,
				       "entry_server", &settings->entry_server,
				       "entry_password", &settings->entry_password,
				       "entry_port", &settings->entry_port,
				       NULL);

	account_widget_msn_setup (settings);

	gossip_glade_connect (glade, 
			      settings,
			      "vbox_msn_settings", "destroy", account_widget_msn_destroy_cb,
			      "button_forget", "clicked", account_widget_msn_button_forget_clicked_cb,
			      "entry_id", "changed", account_widget_msn_entry_changed_cb,
			      "entry_password", "changed", account_widget_msn_entry_changed_cb,
			      "entry_server", "changed", account_widget_msn_entry_changed_cb,
			      "entry_port", "changed", account_widget_msn_entry_changed_cb,
			      "entry_id", "focus-out-event", account_widget_msn_entry_focus_cb,
			      "entry_password", "focus-out-event", account_widget_msn_entry_focus_cb,
			      "entry_server", "focus-out-event", account_widget_msn_entry_focus_cb,
			      "entry_port", "focus-out-event", account_widget_msn_entry_focus_cb,
			      "entry_port", "insert_text", account_widget_msn_entry_port_insert_text_cb,
			      NULL);

	g_object_unref (glade);

	/* Set up remaining widgets */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, label_id);
	gtk_size_group_add_widget (size_group, label_password);

	g_object_unref (size_group);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, label_server);
	gtk_size_group_add_widget (size_group, label_port);

	if (label_name) {
		gtk_size_group_add_widget (size_group, label_name);
	}

	g_object_unref (size_group);

	return settings->vbox_settings;
}
