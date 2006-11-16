/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Martyn Russell <martyn@imendio.com>
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
#include <ctype.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-protocol.h>
#include <libgossip/gossip-session.h>

#include "gossip-account-widget-jabber.h"
#include "gossip-app.h"
#include "gossip-marshal.h"
#include "gossip-ui-utils.h"

#define STRING_EMPTY(x) ((x) == NULL || (x)[0] == '\0')

typedef struct {
	GossipAccount *account;
	gboolean       account_changed;

	GtkWidget     *vbox_settings;

	GtkWidget     *button_forget;

	GtkWidget     *entry_id;
	GtkWidget     *entry_resource;
	GtkWidget     *entry_server;
	GtkWidget     *entry_password;
	GtkWidget     *entry_port;

	GtkWidget     *checkbutton_ssl;
} GossipAccountWidgetJabber;

static void     account_widget_jabber_save                      (GossipAccountWidgetJabber *settings);
static gboolean account_widget_jabber_entry_focus_cb            (GtkWidget                 *widget,
								 GdkEventFocus             *event,
								 GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_entry_changed_cb          (GtkWidget                 *widget,
								 GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_checkbutton_toggled_cb    (GtkWidget                 *widget,
								 GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_entry_port_insert_text_cb (GtkEditable               *editable,
								 gchar                     *new_text,
								 gint                       len,
								 gint                      *position,
								 GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_button_forget_clicked_cb  (GtkWidget                 *button,
								 GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_destroy_cb                (GtkWidget                 *widget,
								 GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_setup                     (GossipAccountWidgetJabber *settings);

static void
account_widget_jabber_save (GossipAccountWidgetJabber *settings)
{
	GossipSession        *session;
	GossipAccountManager *manager;
 	const gchar          *id;
 	const gchar          *password;
 	const gchar          *resource;
 	const gchar          *server;
 	const gchar          *port_str;
	guint                 port;
	gboolean              use_ssl;

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	id = gtk_entry_get_text (GTK_ENTRY (settings->entry_id));
	password = gtk_entry_get_text (GTK_ENTRY (settings->entry_password));
	resource = gtk_entry_get_text (GTK_ENTRY (settings->entry_resource));
	server = gtk_entry_get_text (GTK_ENTRY (settings->entry_server));
	port_str = gtk_entry_get_text (GTK_ENTRY (settings->entry_port));
	port = strtol (port_str, NULL, 10);
	use_ssl = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (settings->checkbutton_ssl));

	gossip_account_set_id (settings->account, id);

	gossip_account_param_set (settings->account,
				  "password", password,
				  "resource", resource,
				  "server", server,
				  "port", port,
				  "use_ssl", use_ssl,
				  NULL);

	gossip_account_manager_store (manager);

	settings->account_changed = FALSE;
}

static gboolean
account_widget_jabber_entry_focus_cb (GtkWidget                 *widget,
				      GdkEventFocus             *event,
				      GossipAccountWidgetJabber *settings)
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
				str = gossip_account_get_id (settings->account);
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
	    widget == settings->entry_resource ||
	    widget == settings->entry_server) {
		const gchar *str;

		str = gtk_entry_get_text (GTK_ENTRY (widget));
		if (STRING_EMPTY (str)) {
			if (widget == settings->entry_password) {
				gossip_account_param_get (settings->account, "password", &str, NULL);
			} else if (widget == settings->entry_resource) {
				gossip_account_param_get (settings->account, "resource", &str, NULL);
			} else if (widget == settings->entry_server) {
				gossip_account_param_get (settings->account, "server", &str, NULL);
			}

			gtk_entry_set_text (GTK_ENTRY (widget), str);
			settings->account_changed = FALSE;
		}
	}

	if (widget == settings->entry_port) {
		const gchar *str;
		
		str = gtk_entry_get_text (GTK_ENTRY (widget));
		if (STRING_EMPTY (str)) {
			gchar   *port_str;
			guint    port;

			gossip_account_param_get (settings->account, "port", &port, NULL);
			port_str = g_strdup_printf ("%d", port);
			gtk_entry_set_text (GTK_ENTRY (widget), port_str);
			g_free (port_str);

			settings->account_changed = FALSE;
		}
	}

	if (settings->account_changed) {
 		account_widget_jabber_save (settings); 
	}

	return FALSE;
}

static void
account_widget_jabber_entry_changed_cb (GtkWidget                 *widget,
					GossipAccountWidgetJabber *settings)
{
	if (widget == settings->entry_port) {
		const gchar *str;
		gint         pnr;

		str = gtk_entry_get_text (GTK_ENTRY (widget));
		pnr = strtol (str, NULL, 10);
		
		if (pnr <= 0 || pnr >= 65556) {
			guint  port;
			gchar *port_str;
			
			gossip_account_param_get (settings->account, "port", &port, NULL);
			port_str = g_strdup_printf ("%d", port);
			g_signal_handlers_block_by_func (settings->entry_port, 
							 account_widget_jabber_entry_changed_cb, 
							 settings);
			gtk_entry_set_text (GTK_ENTRY (widget), port_str);
			g_signal_handlers_unblock_by_func (settings->entry_port, 
							   account_widget_jabber_entry_changed_cb, 
							   settings);
			g_free (port_str);

			return;
		}
	} else if (widget == settings->entry_password) {
		const gchar *str;

		str = gtk_entry_get_text (GTK_ENTRY (widget));
		gtk_widget_set_sensitive (settings->button_forget, ! STRING_EMPTY (str));
	}

	/* Save */
	settings->account_changed = TRUE;
}

static void  
account_widget_jabber_checkbutton_toggled_cb (GtkWidget                 *widget,
					      GossipAccountWidgetJabber *settings)
{
	if (widget == settings->checkbutton_ssl) {
		gboolean       active;
		gboolean       changed;
		GossipSession  *session;
		GossipProtocol *protocol;
		guint           port;
		guint           port_with_ssl;
		guint           account_port;

		session = gossip_app_get_session ();
		protocol = gossip_session_get_protocol (session, settings->account);

		port = gossip_protocol_get_default_port (protocol, FALSE);
		port_with_ssl = gossip_protocol_get_default_port (protocol, TRUE);
		gossip_account_param_get (settings->account, "port", &account_port, NULL);

		active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
		
		if (active && 
		    (account_port == port)) {
			account_port = port_with_ssl;
			changed = TRUE;
		} else if (!active && 
			   (account_port == port_with_ssl)) {
			account_port = port;
			changed = TRUE;
		} else {
			changed = FALSE;
		}
		
		gossip_account_param_set (settings->account,
					  "port", account_port,
					  "use_ssl", active,
					  NULL);

		if (changed) {
			gchar *port_str;

			port_str = g_strdup_printf ("%d", account_port);
			gtk_entry_set_text (GTK_ENTRY (settings->entry_port), port_str);
			g_free (port_str);
		}
	}

 	account_widget_jabber_save (settings); 
}

static void
account_widget_jabber_entry_port_insert_text_cb (GtkEditable               *editable,
						 gchar                     *new_text,
						 gint                       len,
						 gint                      *position,
						 GossipAccountWidgetJabber *settings)
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
account_widget_jabber_button_forget_clicked_cb (GtkWidget                 *button,
						GossipAccountWidgetJabber *settings)
{
	GossipSession        *session;
	GossipAccountManager *manager;

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	gossip_account_param_set (settings->account, "password", "", NULL);
	gossip_account_manager_store (manager);

	gtk_entry_set_text (GTK_ENTRY (settings->entry_password), "");
}

static void
account_widget_jabber_destroy_cb (GtkWidget                 *widget,
				  GossipAccountWidgetJabber *settings)
{
	if (settings->account_changed) {
 		account_widget_jabber_save (settings); 
	}

	g_object_unref (settings->account);
	g_free (settings);
}

static void
account_widget_jabber_setup (GossipAccountWidgetJabber *settings)
{
	GossipSession  *session;
	GossipProtocol *protocol;
	guint           port;
	gchar          *port_str; 
	const gchar    *id;
	const gchar    *resource;
	const gchar    *server;
	const gchar    *password;
	gboolean        use_ssl;

	session = gossip_app_get_session ();
	protocol = gossip_session_get_protocol (session, settings->account);

	id = gossip_account_get_id (settings->account);

	gossip_account_param_get (settings->account,
				  "use_ssl", &use_ssl,
				  "password", &password,
				  "resource", &resource,
				  "server", &server,
				  "port", &port,
				  NULL);

	if (gossip_protocol_is_ssl_supported (protocol)) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (settings->checkbutton_ssl),
					      use_ssl);
	} else {
		gtk_widget_set_sensitive (settings->checkbutton_ssl, FALSE);
	}

	gtk_entry_set_text (GTK_ENTRY (settings->entry_id), id);
	gtk_entry_set_text (GTK_ENTRY (settings->entry_password), password ? password : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_resource), resource ? resource : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_server), server ? server : "");

	port_str = g_strdup_printf ("%d", port);
	gtk_entry_set_text (GTK_ENTRY (settings->entry_port), port_str);
	g_free (port_str);

	gtk_widget_set_sensitive (settings->button_forget, ! STRING_EMPTY (password));
}

GtkWidget *
gossip_account_widget_jabber_new (GossipAccount *account,
				  GtkWidget     *label_name)
{
	GossipAccountWidgetJabber *settings;
	GladeXML                  *glade;
	GtkSizeGroup              *size_group;
	GtkWidget                 *label_id, *label_password;
	GtkWidget                 *label_server, *label_resource, *label_port; 

	settings = g_new0 (GossipAccountWidgetJabber, 1);
	settings->account = g_object_ref (account);

	glade = gossip_glade_get_file ("main.glade",
				       "vbox_jabber_settings",
				       NULL,
				       "vbox_jabber_settings", &settings->vbox_settings,
				       "label_id", &label_id,
				       "label_password", &label_password,
				       "label_resource", &label_resource,
				       "label_server", &label_server,
				       "label_port", &label_port,
				       "entry_id", &settings->entry_id,
				       "entry_resource", &settings->entry_resource,
				       "entry_server", &settings->entry_server,
				       "entry_password", &settings->entry_password,
				       "entry_port", &settings->entry_port,
				       "checkbutton_ssl", &settings->checkbutton_ssl,
				       "button_forget", &settings->button_forget,
				       NULL);

	account_widget_jabber_setup (settings);

	gossip_glade_connect (glade, 
			      settings,
			      "vbox_jabber_settings", "destroy", account_widget_jabber_destroy_cb,
			      "entry_id", "changed", account_widget_jabber_entry_changed_cb,
			      "entry_password", "changed", account_widget_jabber_entry_changed_cb,
			      "entry_resource", "changed", account_widget_jabber_entry_changed_cb,
			      "entry_server", "changed", account_widget_jabber_entry_changed_cb,
			      "entry_port", "changed", account_widget_jabber_entry_changed_cb,
			      "entry_id", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_password", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_resource", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_server", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_port", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_port", "insert_text", account_widget_jabber_entry_port_insert_text_cb,
			      "checkbutton_ssl", "toggled", account_widget_jabber_checkbutton_toggled_cb,
			      "button_forget", "clicked", account_widget_jabber_button_forget_clicked_cb,
			      NULL);

	g_object_unref (glade);

	/* Set up remaining widgets */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, label_id);
	gtk_size_group_add_widget (size_group, label_password);

	g_object_unref (size_group);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, label_resource);
	gtk_size_group_add_widget (size_group, label_server);
	gtk_size_group_add_widget (size_group, label_port);

	if (label_name) {
		gtk_size_group_add_widget (size_group, label_name);
	}

	g_object_unref (size_group);

	return settings->vbox_settings;
}
