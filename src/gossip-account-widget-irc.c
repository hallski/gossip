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

#include "gossip-account-widget-irc.h"
#include "gossip-app.h"
#include "gossip-glade.h"
#include "gossip-marshal.h"
#include "gossip-ui-utils.h"

typedef struct {
	GossipAccount *account;
	gboolean       account_changed;

	GtkWidget     *vbox_settings;

	GtkWidget     *button_forget;

	GtkWidget     *entry_nick_name;
	GtkWidget     *entry_full_name;
	GtkWidget     *entry_password;
	GtkWidget     *entry_server;
	GtkWidget     *entry_port;
	GtkWidget     *entry_quit_message;

	GtkWidget     *checkbutton_use_ssl;
} GossipAccountWidgetIRC;

static void     account_widget_irc_save                      (GossipAccountWidgetIRC *settings);
static void     account_widget_irc_protocol_connected_cb     (GossipSession          *session,
							      GossipAccount          *account,
							      GossipProtocol         *protocol,
							      GossipAccountWidgetIRC *settings);
static void     account_widget_irc_protocol_disconnected_cb  (GossipSession          *session,
							      GossipAccount          *account,
							      GossipProtocol         *protocol,
							      gint                    reason,
							      GossipAccountWidgetIRC *settings);
static gboolean account_widget_irc_entry_focus_cb            (GtkWidget              *widget,
							      GdkEventFocus          *event,
							      GossipAccountWidgetIRC *settings);
static void     account_widget_irc_entry_changed_cb          (GtkWidget              *widget,
							      GossipAccountWidgetIRC *settings);
static void     account_widget_irc_entry_port_insert_text_cb (GtkEditable            *editable,
							      gchar                  *new_text,
							      gint                    len,
							      gint                   *position,
							      GossipAccountWidgetIRC *settings);
static void     account_widget_irc_button_forget_clicked_cb  (GtkWidget              *button,
							      GossipAccountWidgetIRC *settings);
static void     account_widget_irc_destroy_cb                (GtkWidget              *widget,
							      GossipAccountWidgetIRC *settings);
static void     account_widget_irc_setup                     (GossipAccountWidgetIRC *settings);

static void
account_widget_irc_save (GossipAccountWidgetIRC *settings)
{
	GossipSession        *session;
	GossipAccountManager *manager;
 	const gchar          *nick_name;
 	const gchar          *full_name;
 	const gchar          *password;
 	const gchar          *server;
 	const gchar          *port_str;
	guint                 port;
 	const gchar          *quit_message;

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	nick_name = gtk_entry_get_text (GTK_ENTRY (settings->entry_nick_name));
	full_name = gtk_entry_get_text (GTK_ENTRY (settings->entry_full_name));
	password = gtk_entry_get_text (GTK_ENTRY (settings->entry_password));
	server = gtk_entry_get_text (GTK_ENTRY (settings->entry_server));
	port_str = gtk_entry_get_text (GTK_ENTRY (settings->entry_port));
	port = strtol (port_str, NULL, 10);
	quit_message = gtk_entry_get_text (GTK_ENTRY (settings->entry_quit_message));

	gossip_account_set_param (settings->account,
				  "account", nick_name,
				  "fullname", full_name,
				  "password", password,
				  "server", server,
				  "port", port,
				  "quit-message", quit_message,
				  NULL);

	gossip_account_manager_store (manager);

	settings->account_changed = FALSE;
}

static void
account_widget_irc_protocol_connected_cb (GossipSession             *session,
					  GossipAccount             *account,
					  GossipProtocol            *protocol,
					  GossipAccountWidgetIRC *settings)
{
	if (gossip_account_equal (account, settings->account)) {
		/* FIXME: IMPLEMENT */
	}
}

static void
account_widget_irc_protocol_disconnected_cb (GossipSession             *session,
					     GossipAccount             *account,
					     GossipProtocol            *protocol,
					     gint                       reason,
					     GossipAccountWidgetIRC *settings)
{
	if (gossip_account_equal (account, settings->account)) {
		/* FIXME: IMPLEMENT */
	}
}

static void
account_widget_irc_protocol_error_cb (GossipSession             *session,
				      GossipProtocol            *protocol,
				      GossipAccount             *account,
				      GError                    *error,
				      GossipAccountWidgetIRC *settings)
{
	if (gossip_account_equal (account, settings->account)) {
		/* FIXME: HANDLE ERRORS */
	}	
}

static gboolean
account_widget_irc_entry_focus_cb (GtkWidget                 *widget,
				   GdkEventFocus             *event,
				   GossipAccountWidgetIRC *settings)
{
	if (widget == settings->entry_nick_name ||
	    widget == settings->entry_full_name || 
	    widget == settings->entry_password ||
	    widget == settings->entry_server) {
		const gchar *str;

		str = gtk_entry_get_text (GTK_ENTRY (widget));
		if (G_STR_EMPTY (str)) {
			if (widget == settings->entry_nick_name) {
				gossip_account_get_param (settings->account, "account", &str, NULL);
			} else if (widget == settings->entry_full_name) {
				gossip_account_get_param (settings->account, "fullname", &str, NULL);
			} else if (widget == settings->entry_password) {
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
 		account_widget_irc_save (settings); 
	}

	return FALSE;
}

static void
account_widget_irc_entry_changed_cb (GtkWidget                 *widget,
				     GossipAccountWidgetIRC *settings)
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
							 account_widget_irc_entry_changed_cb, 
							 settings);
			gtk_entry_set_text (GTK_ENTRY (widget), port_str);
			g_signal_handlers_unblock_by_func (settings->entry_port, 
							   account_widget_irc_entry_changed_cb, 
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
account_widget_irc_entry_port_insert_text_cb (GtkEditable               *editable,
					      gchar                     *new_text,
					      gint                       len,
					      gint                      *position,
					      GossipAccountWidgetIRC *settings)
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
account_widget_irc_button_forget_clicked_cb (GtkWidget                 *button,
					     GossipAccountWidgetIRC *settings)
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
account_widget_irc_destroy_cb (GtkWidget                 *widget,
			       GossipAccountWidgetIRC *settings)
{
	GossipSession *session;

	if (settings->account_changed) {
 		account_widget_irc_save (settings); 
	}

	session = gossip_app_get_session ();

	g_signal_handlers_disconnect_by_func (session,
					      account_widget_irc_protocol_connected_cb,
					      settings);

	g_signal_handlers_disconnect_by_func (session,
					      account_widget_irc_protocol_disconnected_cb,
					      settings);

	g_signal_handlers_disconnect_by_func (session,
					      account_widget_irc_protocol_error_cb,
					      settings);

	g_object_unref (settings->account);
	g_free (settings);
}

static void
account_widget_irc_setup (GossipAccountWidgetIRC *settings)
{
	GossipSession      *session;
	GossipProtocol     *protocol;
	GossipAccountParam *param;
	const gchar        *nick_name;
	const gchar        *full_name;
	const gchar        *password;
	const gchar        *server;
	guint               port;
	gchar              *port_str; 
	const gchar        *quit_message;
	gboolean            is_connected;

	session = gossip_app_get_session ();
	protocol = gossip_session_get_protocol (session, settings->account);

	gossip_account_get_param (settings->account,
				  "account", &nick_name,
				  "fullname", &full_name,
				  "password", &password,
				  "server", &server,
				  "port", &port,
				  "quit-message", &quit_message,
				  NULL);

	/* Don't use Telepathy defaults here */
	param = gossip_account_get_param_param (settings->account, "fullname");
	if (param->flags & GOSSIP_ACCOUNT_PARAM_FLAG_HAS_DEFAULT && 
	    param->modified == FALSE) {
		full_name = "";
	}

	/* Don't use Telepathy defaults here */
	param = gossip_account_get_param_param (settings->account, "quit-message");
	if (param->flags & GOSSIP_ACCOUNT_PARAM_FLAG_HAS_DEFAULT && 
	    param->modified == FALSE) {
		quit_message = _("Bye bye");
	}

	gtk_entry_set_text (GTK_ENTRY (settings->entry_nick_name), nick_name ? nick_name : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_full_name), full_name ? full_name : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_password), password ? password : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_server), server ? server : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_quit_message), quit_message ? quit_message : "");

	port_str = g_strdup_printf ("%d", port);
	gtk_entry_set_text (GTK_ENTRY (settings->entry_port), port_str);
	g_free (port_str);

	gtk_widget_set_sensitive (settings->button_forget, !G_STR_EMPTY (password));

	/* Set up connection specific buttons */
	is_connected = gossip_session_is_connected (session, settings->account);

	/* Set up protocol signals */
	g_signal_connect (session, "protocol-connected",
			  G_CALLBACK (account_widget_irc_protocol_connected_cb),
			  settings);

	g_signal_connect (session, "protocol-disconnected",
			  G_CALLBACK (account_widget_irc_protocol_disconnected_cb),
			  settings);

	g_signal_connect (session, "protocol-error",
			  G_CALLBACK (account_widget_irc_protocol_error_cb),
			  settings);
}

GtkWidget *
gossip_account_widget_irc_new (GossipAccount *account)
{
	GossipAccountWidgetIRC *settings;
	GladeXML               *glade;
	GtkSizeGroup           *size_group;
	GtkWidget              *label_nick_name;
	GtkWidget              *label_full_name;
	GtkWidget              *label_password;
	GtkWidget              *label_server;
	GtkWidget              *label_port; 
	GtkWidget              *label_quit_message;

	settings = g_new0 (GossipAccountWidgetIRC, 1);
	settings->account = g_object_ref (account);

	glade = gossip_glade_get_file ("main.glade",
				       "vbox_irc_settings",
				       NULL,
				       "vbox_irc_settings", &settings->vbox_settings,
				       "button_forget", &settings->button_forget,
				       "label_nick_name", &label_nick_name,
				       "label_full_name", &label_full_name,
				       "label_password", &label_password,
				       "label_server", &label_server,
				       "label_port", &label_port,
				       "label_quit_message", &label_quit_message,
				       "entry_nick_name", &settings->entry_nick_name,
				       "entry_full_name", &settings->entry_full_name,
				       "entry_server", &settings->entry_server,
				       "entry_password", &settings->entry_password,
				       "entry_port", &settings->entry_port,
				       "entry_quit_message", &settings->entry_quit_message,
				       "checkbutton_use_ssl", &settings->checkbutton_use_ssl,
				       NULL);

	account_widget_irc_setup (settings);

	gossip_glade_connect (glade, 
			      settings,
			      "vbox_irc_settings", "destroy", account_widget_irc_destroy_cb,
			      "button_forget", "clicked", account_widget_irc_button_forget_clicked_cb,
			      "entry_nick_name", "changed", account_widget_irc_entry_changed_cb,
			      "entry_full_name", "changed", account_widget_irc_entry_changed_cb,
			      "entry_password", "changed", account_widget_irc_entry_changed_cb,
			      "entry_server", "changed", account_widget_irc_entry_changed_cb,
			      "entry_port", "changed", account_widget_irc_entry_changed_cb,
			      "entry_nick_name", "focus-out-event", account_widget_irc_entry_focus_cb,
			      "entry_full_name", "focus-out-event", account_widget_irc_entry_focus_cb,
			      "entry_password", "focus-out-event", account_widget_irc_entry_focus_cb,
			      "entry_server", "focus-out-event", account_widget_irc_entry_focus_cb,
			      "entry_port", "focus-out-event", account_widget_irc_entry_focus_cb,
			      "entry_port", "insert_text", account_widget_irc_entry_port_insert_text_cb,
			      "entry_quit_message", "focus-out-event", account_widget_irc_entry_focus_cb,
			      NULL);

	g_object_unref (glade);

	/* Set up remaining widgets */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, label_nick_name);
	gtk_size_group_add_widget (size_group, label_full_name);
	gtk_size_group_add_widget (size_group, label_password);
	gtk_size_group_add_widget (size_group, label_server);
	gtk_size_group_add_widget (size_group, label_port);
	gtk_size_group_add_widget (size_group, label_quit_message);

	g_object_unref (size_group);

	return settings->vbox_settings;
}
