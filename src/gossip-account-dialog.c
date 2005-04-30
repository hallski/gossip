/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2004 Imendio AB
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <libgnome/gnome-config.h>
#include <libgnome/gnome-i18n.h>
#include <loudmouth/loudmouth.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-utils.h>

#include "gossip-ui-utils.h"
#include "gossip-app.h"
#include "gossip-register.h"
#include "gossip-account-dialog.h"

#define RESPONSE_REGISTER 1

typedef struct {
	GtkWidget     *dialog;

	GossipAccount *account;
	
	GtkEntry      *account_name_entry;
	GtkEntry      *jid_entry;
	GtkEntry      *server_entry;
	GtkEntry      *resource_entry;
	GtkEntry      *password_entry;
	GtkEntry      *port_entry;
	GtkWidget     *ssl_checkbutton;
	GtkWidget     *proxy_checkbutton;
	GtkWidget     *register_button;
} GossipAccountDialog ;


static void     account_dialog_destroy_cb          (GtkWidget            *widget,
						    GossipAccountDialog **dialog);
static void     account_dialog_response_cb         (GtkWidget            *widget,
						    gint                  response,
						    GossipAccountDialog  *dialog);
static gboolean account_dialog_focus_out_event_cb  (GtkWidget            *widget,
						    GdkEventFocus        *event,
						    GossipAccountDialog  *dialog);
static void     account_dialog_setup               (GossipAccountDialog  *dialog);
static void     account_dialog_port_insert_text_cb (GtkEditable          *editable,
						    gchar                *new_text,
						    gint                  len,
						    gint                 *position,
						    GossipAccountDialog  *dialog);
static void     account_dialog_ssl_toggled         (GtkToggleButton      *button,
						    GossipAccountDialog  *dialog);


static void
account_dialog_destroy_cb (GtkWidget            *widget,
			   GossipAccountDialog **dialog)
{
	gossip_account_unref ((*dialog)->account);
	g_free (*dialog);

	*dialog = NULL;
}

static void
account_dialog_response_cb (GtkWidget            *widget,
			    gint                  response,
			    GossipAccountDialog *dialog)
{
	if (response == RESPONSE_REGISTER) {
		gossip_register_account (dialog->account, GTK_WINDOW (dialog->dialog));
	} else {
		gtk_widget_destroy (widget);
	}
}

static gboolean
account_dialog_focus_out_event_cb (GtkWidget            *widget,
				   GdkEventFocus        *event,
				   GossipAccountDialog *dialog)
{
	GossipAccount *account;

	account = dialog->account;

	if (widget == GTK_WIDGET (dialog->jid_entry)) {
		const gchar *str;
		gchar       *username;
		gchar       *host;

		str = gtk_entry_get_text (dialog->jid_entry);

		username = gossip_utils_jid_str_get_part_name (str);
		host = gossip_utils_jid_str_get_part_host (str);

		gossip_account_set_username (account, username);
		gossip_account_set_host (account, host);
	
		g_free (username);
		g_free (host);
	}
	else if (widget == GTK_WIDGET (dialog->password_entry)) {
		gossip_account_set_password (account,
					     gtk_entry_get_text (dialog->password_entry));
	}
	else if (widget == GTK_WIDGET (dialog->resource_entry)) {
		gossip_account_set_resource (account,
					     gtk_entry_get_text (dialog->resource_entry));
	}
	else if (widget == GTK_WIDGET (dialog->server_entry)) {
		g_free (account->server);
		account->server = g_strdup (gtk_entry_get_text (dialog->server_entry));
	}
	else if (widget == GTK_WIDGET (dialog->port_entry)) {
		gint pnr;
		
		pnr = strtol (gtk_entry_get_text (dialog->port_entry), NULL, 10);
		if (pnr > 0 && pnr < 65556) {
			account->port = pnr;
		} else {
			gchar *str = g_strdup_printf ("%d", account->port);
			gtk_entry_set_text (dialog->port_entry, str);
			g_free (str);
		}
	}
	else if (widget == GTK_WIDGET (dialog->ssl_checkbutton)) {
		account->use_ssl = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
		
		/* Update the port as well. */
		account_dialog_focus_out_event_cb (GTK_WIDGET (dialog->port_entry), 
						   event,
						   dialog);
	}
	else if (widget == GTK_WIDGET (dialog->proxy_checkbutton)) {
		account->use_proxy = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	}

	gossip_account_store (account, NULL);
	gossip_account_set_default (account);

	return FALSE;
}

static void
account_dialog_setup (GossipAccountDialog *dialog)
{
	GossipAccount *account;
	gchar         *port_str;
	gchar         *jid_str;

	account = dialog->account;
	
	if (lm_ssl_is_supported ()) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->ssl_checkbutton),
					      account->use_ssl);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (dialog->ssl_checkbutton),
					  FALSE);
	}

	jid_str = g_strdup_printf ("%s@%s", account->username, account->host);
	gtk_entry_set_text (dialog->jid_entry, jid_str);
	g_free (jid_str);
	gtk_entry_set_text (dialog->password_entry, account->password);
	gtk_entry_set_text (dialog->resource_entry, account->resource);
	gtk_entry_set_text (dialog->server_entry, account->server);

	port_str = g_strdup_printf ("%d", account->port);
	gtk_entry_set_text (dialog->port_entry, port_str);
	g_free (port_str);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->proxy_checkbutton),
				      account->use_proxy);

}

static void
account_dialog_port_insert_text_cb (GtkEditable          *editable,
				    gchar                *new_text,
				    gint                  len,
				    gint                 *position,
				    GossipAccountDialog *dialog)
{
	gint  i;
	
	for (i = 0; i < len; ++i) {
		gchar *ch = new_text + i;
		if (!isdigit (*ch)) {
			g_signal_stop_emission_by_name (editable,
							"insert-text");
			return;
		}
	}
}

static void  
account_dialog_ssl_toggled (GtkToggleButton      *button,
			    GossipAccountDialog *dialog)
{
	gboolean       active;
	gboolean       changed = FALSE;
	GossipAccount *account;
	
	account = dialog->account;
	
	active = gtk_toggle_button_get_active (button);
	
	if (active && (account->port == LM_CONNECTION_DEFAULT_PORT)) {
		account->port = LM_CONNECTION_DEFAULT_PORT_SSL;
		changed = TRUE;
	} 
	else if (!active && (account->port == LM_CONNECTION_DEFAULT_PORT_SSL)) {
		account->port = LM_CONNECTION_DEFAULT_PORT;
		changed = TRUE;
	}

	if (changed) {
		gchar *str = g_strdup_printf ("%d", account->port);
		gtk_entry_set_text (dialog->port_entry, str);
		g_free (str);
	}
}

GtkWidget *
gossip_account_dialog_show (void)
{
	static GossipAccountDialog *dialog = NULL;
	GladeXML                   *gui;

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->dialog));
		return dialog->dialog;
	}
	
	dialog = g_new0 (GossipAccountDialog, 1);

	gui = gossip_glade_get_file (GLADEDIR "/connect.glade",
				     "account_dialog",
				     NULL,
				     "account_dialog", &dialog->dialog,
				     "jid_entry", &dialog->jid_entry,
				     "resource_entry", &dialog->resource_entry,
				     "server_entry", &dialog->server_entry,
				     "password_entry", &dialog->password_entry,
				     "port_entry", &dialog->port_entry,
				     "ssl_checkbutton", &dialog->ssl_checkbutton,
				     "proxy_checkbutton", &dialog->proxy_checkbutton,
				     "register_button", &dialog->register_button,
				     NULL);

/*	gossip_glade_setup_size_group (gui,
				       GTK_SIZE_GROUP_HORIZONTAL,
				       "server_label",
				       "jid_label",
				       "password_label",
				       "resource_label",
				       "port_label",
				       NULL);
*/
	g_signal_connect (dialog->dialog,
			  "destroy",
			  G_CALLBACK (account_dialog_destroy_cb),
			  &dialog);
	
	gossip_glade_connect (gui, dialog,
			      "account_dialog", "response",
			      G_CALLBACK (account_dialog_response_cb),
			      
			      "jid_entry", "focus_out_event",
			      G_CALLBACK (account_dialog_focus_out_event_cb),
			      
			      "resource_entry", "focus_out_event",
			      G_CALLBACK (account_dialog_focus_out_event_cb),
			      
			      "server_entry", "focus_out_event",
			      G_CALLBACK (account_dialog_focus_out_event_cb),
			      
			      "port_entry", "focus_out_event",
			      G_CALLBACK (account_dialog_focus_out_event_cb),
			      
			      "password_entry", "focus_out_event",
			      G_CALLBACK (account_dialog_focus_out_event_cb),
			      
			      "ssl_checkbutton", "focus_out_event",
			      G_CALLBACK (account_dialog_focus_out_event_cb),

			      "proxy_checkbutton", "focus_out_event",
			      G_CALLBACK (account_dialog_focus_out_event_cb),
			      
			      "port_entry", "insert_text",
			      G_CALLBACK (account_dialog_port_insert_text_cb),
			      
			      "ssl_checkbutton", "toggled",
			      G_CALLBACK (account_dialog_ssl_toggled),
			      
			      NULL);	
	
	g_object_unref (gui);

	dialog->account = gossip_account_get_default ();
	if (!dialog->account) {
		dialog->account = gossip_account_create_empty ();
	}
	
	account_dialog_setup (dialog);
	
	gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog), GTK_WINDOW (gossip_app_get_window ()));
	gtk_widget_show (dialog->dialog);

	return dialog->dialog;
}

