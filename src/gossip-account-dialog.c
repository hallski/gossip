/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2004 Imendio AB
 * Copyright (C) 2005      Martyn Russell <mr@gnome.org>
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
#include <glib/gi18n.h>
#include <loudmouth/loudmouth.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-utils.h>

#include "gossip-ui-utils.h"
#include "gossip-app.h"
#include "gossip-register.h"
#include "gossip-account-dialog.h"


typedef struct {
	GtkWidget     *dialog;
	GtkWidget     *account_name_entry;
	GtkWidget     *jid_entry;
	GtkWidget     *server_entry;
	GtkWidget     *resource_entry;
	GtkWidget     *password_entry;
	GtkWidget     *port_entry;
	GtkWidget     *ssl_checkbutton;
	GtkWidget     *proxy_checkbutton;
	GtkWidget     *register_button;

	gboolean       account_changed;

	GossipAccount *account;
} GossipAccountDialog;


static void     account_dialog_destroy_cb          (GtkWidget            *widget,
						    GossipAccountDialog **dialog);
static void     account_dialog_response_cb         (GtkWidget            *widget,
						    gint                  response,
						    GossipAccountDialog  *dialog);
static void           account_dialog_register_clicked        (GtkWidget            *button,
							      GossipAccountDialog  *dialog);
static void           account_dialog_changed_cb              (GtkWidget            *widget,
							      GossipAccountDialog  *dialog);
static gboolean       account_dialog_focus_out_event         (GtkWidget            *widget,
						    GdkEventFocus        *event,
						    GossipAccountDialog  *dialog);
static void     account_dialog_port_insert_text_cb (GtkEditable          *editable,
						    gchar                *new_text,
						    gint                  len,
						    gint                 *position,
						    GossipAccountDialog  *dialog);
static void           account_dialog_toggled_cb              (GtkWidget            *widget,
						    GossipAccountDialog  *dialog);
static void           account_dialog_save                    (GossipAccountDialog  *dialog);
static void           account_dialog_setup                   (GossipAccountDialog  *dialog,
							      GossipAccount        *account);



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
 	if (dialog->account_changed) {
 		account_dialog_save (dialog);
	}
		
	gtk_widget_destroy (widget);
}

static void
account_dialog_register_clicked (GtkWidget           *button,
 				 GossipAccountDialog *dialog)
{
 	gossip_register_account (dialog->account, 
 				 GTK_WINDOW (dialog->dialog));
}

static void
account_dialog_changed_cb (GtkWidget           *widget,
				   GossipAccountDialog *dialog)
{
	GossipAccount *account;
 	const gchar   *str = NULL;
 
 	dialog->account_changed = TRUE;

	account = dialog->account;

 	g_return_if_fail (account != NULL);
  
 	if (GTK_IS_ENTRY (widget)) {
 		str = gtk_entry_get_text (GTK_ENTRY (widget));
	}
	
 	if (widget == dialog->jid_entry) {
		gchar       *username;
		gchar       *host;

		username = gossip_utils_jid_str_get_part_name (str);
		host = gossip_utils_jid_str_get_part_host (str);

		gossip_account_set_username (account, username);
		gossip_account_set_host (account, host);
	
		g_free (username);
		g_free (host);

 	} else if (widget == dialog->password_entry) {
		gossip_account_set_password (account, str);
 	} else if (widget == dialog->resource_entry) {
		gossip_account_set_resource (account, str);
 	} else if (widget == dialog->server_entry) {
		g_free (account->server);
 		account->server = g_strdup (str);
 	} else if (widget == dialog->port_entry) {
		gint pnr;
		
 		pnr = strtol (str, NULL, 10);
		if (pnr > 0 && pnr < 65556) {
			account->port = pnr;
		} else {
			gchar *port_str;
		
			port_str = g_strdup_printf ("%d", account->port);
 			g_signal_handlers_block_by_func (dialog->port_entry, 
 							 account_dialog_changed_cb, 
						   dialog);
 			gtk_entry_set_text (GTK_ENTRY (widget), port_str);
 			g_signal_handlers_unblock_by_func (dialog->port_entry, 
 							   account_dialog_changed_cb, 
 							   dialog);
			g_free (port_str);
	}
	}

	gossip_account_store (account, NULL);
	gossip_account_set_default (account);
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
account_dialog_toggled_cb (GtkWidget           *widget,
			    GossipAccountDialog *dialog)
{
	gboolean       active;
	gboolean       changed = FALSE;
	GossipAccount *account;
	
	account = dialog->account;
	
	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	
	if (widget == dialog->ssl_checkbutton) {
		account->use_ssl = active;
	
	if (active && (account->port == LM_CONNECTION_DEFAULT_PORT)) {
		account->port = LM_CONNECTION_DEFAULT_PORT_SSL;
		changed = TRUE;
		} else if (!active && (account->port == LM_CONNECTION_DEFAULT_PORT_SSL)) {
		account->port = LM_CONNECTION_DEFAULT_PORT;
		changed = TRUE;
	}

	if (changed) {
		gchar *port_str = g_strdup_printf ("%d", account->port);
		gtk_entry_set_text (GTK_ENTRY (dialog->port_entry), port_str);
		g_free (port_str);
	}
	} else if (widget == dialog->proxy_checkbutton) {
		account->use_proxy = active;
	}

	account_dialog_save (dialog);
}

static gboolean
account_dialog_focus_out_event (GtkWidget           *widget,
				GdkEventFocus       *event,
				GossipAccountDialog *dialog)
{
	if (!dialog->account_changed) {
		return FALSE;
	}

	account_dialog_save (dialog);

	return FALSE;
}

static void
account_dialog_save (GossipAccountDialog *dialog) 
{
	gossip_account_store (dialog->account, NULL);
	gossip_account_set_default (dialog->account);

	dialog->account_changed = FALSE;
}

static void
account_dialog_setup (GossipAccountDialog *dialog,
		      GossipAccount       *account_to_use)
{
	GossipAccount *account;
	gchar         *jid_str;
	gchar         *port_str; 

	account = account_to_use;
	if (!account) {
		account = gossip_account_get_default ();
	}

	if (!account) {
		account = gossip_account_create_empty ();
	}

	g_return_if_fail (account != NULL);

	if (dialog->account) {
		gossip_account_unref (dialog->account);
	}

	dialog->account = gossip_account_ref (account);

	gtk_widget_set_sensitive (dialog->register_button, TRUE);
	
	/* block signals first */
	g_signal_handlers_block_by_func (dialog->jid_entry, account_dialog_changed_cb, dialog);
	g_signal_handlers_block_by_func (dialog->password_entry, account_dialog_changed_cb, dialog);
	g_signal_handlers_block_by_func (dialog->resource_entry, account_dialog_changed_cb, dialog);
	g_signal_handlers_block_by_func (dialog->server_entry, account_dialog_changed_cb, dialog);
	g_signal_handlers_block_by_func (dialog->port_entry, account_dialog_changed_cb, dialog);
	g_signal_handlers_block_by_func (dialog->ssl_checkbutton, account_dialog_toggled_cb, dialog);
	g_signal_handlers_block_by_func (dialog->proxy_checkbutton, account_dialog_toggled_cb, dialog);

	if (lm_ssl_is_supported ()) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->ssl_checkbutton),
					      account->use_ssl);
	} else {
		gtk_widget_set_sensitive (dialog->ssl_checkbutton, FALSE);
	}

	if (account->username && strlen (account->username) > 0 && 
	    account->host && strlen (account->host) > 0)  {
		jid_str = g_strdup_printf ("%s@%s", 
					   account->username,
					   account->host);
		gtk_entry_set_text (GTK_ENTRY (dialog->jid_entry), jid_str);
		g_free (jid_str);
	}

	gtk_entry_set_text (GTK_ENTRY (dialog->password_entry), 
			    account->password);
	gtk_entry_set_text (GTK_ENTRY (dialog->resource_entry), 
			    account->resource);
	gtk_entry_set_text (GTK_ENTRY (dialog->server_entry), 
			    account->server);

	port_str = g_strdup_printf ("%d", account->port);
	gtk_entry_set_text (GTK_ENTRY (dialog->port_entry), 
			    port_str);
	g_free (port_str);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->proxy_checkbutton),
				      account->use_proxy);

	/* unblock signals */
	g_signal_handlers_unblock_by_func (dialog->jid_entry, account_dialog_changed_cb, dialog);
	g_signal_handlers_unblock_by_func (dialog->password_entry, account_dialog_changed_cb, dialog);
	g_signal_handlers_unblock_by_func (dialog->resource_entry, account_dialog_changed_cb, dialog);
	g_signal_handlers_unblock_by_func (dialog->server_entry, account_dialog_changed_cb, dialog);
	g_signal_handlers_unblock_by_func (dialog->port_entry, account_dialog_changed_cb, dialog);
	g_signal_handlers_unblock_by_func (dialog->ssl_checkbutton, account_dialog_toggled_cb, dialog);
	g_signal_handlers_unblock_by_func (dialog->proxy_checkbutton, account_dialog_toggled_cb, dialog);

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

	gossip_glade_connect (gui, 
			      dialog,
			      "account_dialog", "response", account_dialog_response_cb,
			      "jid_entry", "focus-out-event", account_dialog_focus_out_event,
			      "resource_entry", "focus-out-event", account_dialog_focus_out_event,
			      "server_entry", "focus-out-event", account_dialog_focus_out_event,
			      "port_entry", "focus-out-event", account_dialog_focus_out_event,
			      "jid_entry", "changed", account_dialog_changed_cb,
			      "resource_entry", "changed", account_dialog_changed_cb,
			      "server_entry", "changed", account_dialog_changed_cb,
			      "port_entry", "changed", account_dialog_changed_cb,
			      "password_entry", "changed", account_dialog_changed_cb,
			      "port_entry", "insert_text", account_dialog_port_insert_text_cb,
			      "proxy_checkbutton", "toggled", account_dialog_toggled_cb,
			      "ssl_checkbutton", "toggled", account_dialog_toggled_cb,
			      "register_button", "clicked", account_dialog_register_clicked,
				       NULL);
	
	g_signal_connect (dialog->dialog,
			  "destroy",
			  G_CALLBACK (account_dialog_destroy_cb),
			  &dialog);
	
	g_object_unref (gui);

	account_dialog_setup (dialog, NULL);
	
	gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog), 
				      GTK_WINDOW (gossip_app_get_window ()));
	gtk_widget_show (dialog->dialog);

	return dialog->dialog;
}

