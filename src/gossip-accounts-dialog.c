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
#include "gossip-accounts-dialog.h"

#define RESPONSE_REGISTER 1


typedef struct {
	GtkWidget     *dialog;
	GtkWidget     *account_name_entry;
	GtkWidget     *id_entry;
	GtkWidget     *server_entry;
	GtkWidget     *resource_entry;
	GtkWidget     *password_entry;
	GtkWidget     *port_entry;
	GtkWidget     *ssl_checkbutton;
	GtkWidget     *proxy_checkbutton;
	GtkWidget     *auto_connect_checkbutton;

	gboolean       account_changed;

	GossipAccount *account;
} GossipAccountDialog;


static void     accounts_dialog_destroy_cb          (GtkWidget            *widget,
						     GossipAccountDialog **dialog);
static void     accounts_dialog_response_cb         (GtkWidget            *widget,
						     gint                  response,
						     GossipAccountDialog  *dialog);
static void     accounts_dialog_changed_cb          (GtkWidget            *widget,
						     GossipAccountDialog  *dialog);
static gboolean accounts_dialog_focus_out_event     (GtkWidget            *widget,
						     GdkEventFocus        *event,
						     GossipAccountDialog  *dialog);
static void     accounts_dialog_port_insert_text_cb (GtkEditable          *editable,
						     gchar                *new_text,
						     gint                  len,
						     gint                 *position,
						     GossipAccountDialog  *dialog);
static void     accounts_dialog_toggled_cb          (GtkWidget            *widget,
						     GossipAccountDialog  *dialog);
static void     accounts_dialog_save                (GossipAccountDialog  *dialog);
static void     accounts_dialog_setup               (GossipAccountDialog  *dialog,
						     GossipAccount        *account);


static void
accounts_dialog_destroy_cb (GtkWidget            *widget,
			   GossipAccountDialog **dialog)
{
	g_object_unref ((*dialog)->account);
	g_free (*dialog);

	*dialog = NULL;
}

static void
accounts_dialog_response_cb (GtkWidget           *widget,
			    gint                 response,
			    GossipAccountDialog *dialog)
{
	if (response == RESPONSE_REGISTER) {
		gossip_register_account (dialog->account, 
					 GTK_WINDOW (dialog->dialog));
		return;
	}

 	if (dialog->account_changed) {
 		accounts_dialog_save (dialog);
	}
		
	gtk_widget_destroy (widget);
}

static void
accounts_dialog_changed_cb (GtkWidget           *widget,
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
	
 	if (widget == dialog->id_entry) {
		gchar       *id_to_save;
		const gchar *resource;

		resource = gtk_entry_get_text (GTK_ENTRY (dialog->resource_entry));
		if (!resource || strlen (resource) < 1) {
			resource = _("Home");
		}

		id_to_save = g_strdup_printf ("%s/%s", str, resource);
		gossip_account_set_id (account, id_to_save);

		g_free (id_to_save);
 	} else if (widget == dialog->password_entry) {
		gossip_account_set_password (account, str);
 	} else if (widget == dialog->resource_entry) {
		const gchar *id;

		id = gtk_entry_get_text (GTK_ENTRY (dialog->id_entry));
		if (id && strlen (id) > 0) {
			gchar *id_to_save;

			id_to_save = g_strdup_printf ("%s/%s", id, str);
			gossip_account_set_id (account, id_to_save);

			g_free (id_to_save);
		}
 	} else if (widget == dialog->server_entry) {
 		gossip_account_set_server (account, str);
 	} else if (widget == dialog->port_entry) {
		gint pnr;
		
 		pnr = strtol (str, NULL, 10);
		if (pnr > 0 && pnr < 65556) {
			gossip_account_set_port (account, pnr);
		} else {
			gchar *port_str;
		
			port_str = g_strdup_printf ("%d", gossip_account_get_port (account));
 			g_signal_handlers_block_by_func (dialog->port_entry, 
 							 accounts_dialog_changed_cb, 
							 dialog);
 			gtk_entry_set_text (GTK_ENTRY (widget), port_str);
 			g_signal_handlers_unblock_by_func (dialog->port_entry, 
 							   accounts_dialog_changed_cb, 
 							   dialog);
			g_free (port_str);
		}
	}

	gossip_accounts_set_default (account);
	gossip_accounts_store (account);
}

static void
accounts_dialog_port_insert_text_cb (GtkEditable          *editable,
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
accounts_dialog_toggled_cb (GtkWidget           *widget,
			   GossipAccountDialog *dialog)
{
	gboolean       active;
	gboolean       changed = FALSE;
	GossipAccount *account;
	
	account = dialog->account;
	
	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	
	if (widget == dialog->ssl_checkbutton) {
		gossip_account_set_use_ssl (account, active);
	
		if (active && 
		    (gossip_account_get_port (account) == LM_CONNECTION_DEFAULT_PORT)) {
			gossip_account_set_port (account, LM_CONNECTION_DEFAULT_PORT_SSL);
			changed = TRUE;
		} else if (!active && 
			   (gossip_account_get_port (account) == LM_CONNECTION_DEFAULT_PORT_SSL)) {
			gossip_account_set_port (account, LM_CONNECTION_DEFAULT_PORT);
			changed = TRUE;
		}

		if (changed) {
			gchar *port_str = g_strdup_printf ("%d", gossip_account_get_port (account));
			gtk_entry_set_text (GTK_ENTRY (dialog->port_entry), port_str);
			g_free (port_str);
		}
	} else if (widget == dialog->proxy_checkbutton) {
		gossip_account_set_use_proxy (account, active);
	} else if (widget == dialog->auto_connect_checkbutton) {
		gossip_account_set_auto_connect (account, active);
	}

	accounts_dialog_save (dialog);
}

static gboolean
accounts_dialog_focus_out_event (GtkWidget           *widget,
				GdkEventFocus       *event,
				GossipAccountDialog *dialog)
{
	if (!dialog->account_changed) {
		return FALSE;
	}

	accounts_dialog_save (dialog);

	return FALSE;
}

static void
accounts_dialog_save (GossipAccountDialog *dialog) 
{
	gossip_accounts_set_default (dialog->account);
	gossip_accounts_store (dialog->account);

	dialog->account_changed = FALSE;
}

static void
accounts_dialog_setup (GossipAccountDialog *dialog,
		      GossipAccount       *account_to_use)
{
	GossipAccount *account;
	gchar         *port_str; 
	const gchar   *id;
	
	account = account_to_use;
	if (!account) {
		account = gossip_accounts_get_default ();
	}

	if (!account) {
		account = g_object_new (GOSSIP_TYPE_ACCOUNT, NULL);
	}

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	if (dialog->account) {
		g_object_unref (dialog->account);
	}

	dialog->account = g_object_ref (account);

	/* block signals first */
	g_signal_handlers_block_by_func (dialog->id_entry, 
					 accounts_dialog_changed_cb, dialog);
	g_signal_handlers_block_by_func (dialog->password_entry, 
					 accounts_dialog_changed_cb, dialog);
	g_signal_handlers_block_by_func (dialog->resource_entry, 
					 accounts_dialog_changed_cb, dialog);
	g_signal_handlers_block_by_func (dialog->server_entry, 
					 accounts_dialog_changed_cb, dialog);
	g_signal_handlers_block_by_func (dialog->port_entry, 
					 accounts_dialog_changed_cb, dialog);
	g_signal_handlers_block_by_func (dialog->ssl_checkbutton, 
					 accounts_dialog_toggled_cb, dialog);
	g_signal_handlers_block_by_func (dialog->proxy_checkbutton, 
					 accounts_dialog_toggled_cb, dialog);
	g_signal_handlers_block_by_func (dialog->auto_connect_checkbutton, 
					 accounts_dialog_toggled_cb, dialog);

	if (lm_ssl_is_supported ()) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->ssl_checkbutton),
					      gossip_account_get_use_ssl (account));
	} else {
		gtk_widget_set_sensitive (dialog->ssl_checkbutton, FALSE);
	}

	id = gossip_account_get_id (account);
	if (id && strlen (id) > 0) {
		const gchar *resource;
		gchar       *name, *host, *id_no_resource;
		
		/* FIXME: we have to know about Jabber stuff here... */
		resource = gossip_utils_jid_str_locate_resource (id);
		if (!resource) {
			resource = _("Home");
		}

		name = gossip_utils_jid_str_get_part_name (id);
		host = gossip_utils_jid_str_get_part_host (id);

		id_no_resource = g_strdup_printf ("%s@%s", name, host);

		gtk_entry_set_text (GTK_ENTRY (dialog->resource_entry), resource);
		gtk_entry_set_text (GTK_ENTRY (dialog->id_entry), id_no_resource);

		g_free (name);
		g_free (host);
		g_free (id_no_resource);
	}

	if (gossip_account_get_password (account)) {
		gtk_entry_set_text (GTK_ENTRY (dialog->password_entry), 
				    gossip_account_get_password (account));
	}

	gtk_entry_set_text (GTK_ENTRY (dialog->server_entry), 
			    gossip_account_get_server (account));

	port_str = g_strdup_printf ("%d", gossip_account_get_port (account));
	gtk_entry_set_text (GTK_ENTRY (dialog->port_entry), 
			    port_str);
	g_free (port_str);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->proxy_checkbutton),
				      gossip_account_get_use_proxy (account));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->auto_connect_checkbutton),
				      gossip_account_get_auto_connect (account));

	/* unblock signals */
	g_signal_handlers_unblock_by_func (dialog->id_entry, 
					   accounts_dialog_changed_cb, dialog);
	g_signal_handlers_unblock_by_func (dialog->password_entry,
					   accounts_dialog_changed_cb, dialog);
	g_signal_handlers_unblock_by_func (dialog->resource_entry, 
					   accounts_dialog_changed_cb, dialog);
	g_signal_handlers_unblock_by_func (dialog->server_entry, 
					   accounts_dialog_changed_cb, dialog);
	g_signal_handlers_unblock_by_func (dialog->port_entry, 
					   accounts_dialog_changed_cb, dialog);
	g_signal_handlers_unblock_by_func (dialog->ssl_checkbutton, 
					   accounts_dialog_toggled_cb, dialog);
	g_signal_handlers_unblock_by_func (dialog->proxy_checkbutton, 
					   accounts_dialog_toggled_cb, dialog);
	g_signal_handlers_unblock_by_func (dialog->auto_connect_checkbutton, 
					   accounts_dialog_toggled_cb, dialog);
}

GtkWidget *
gossip_accounts_dialog_show (void)
{
	static GossipAccountDialog *dialog = NULL;
	GladeXML                   *gui;

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->dialog));
		return dialog->dialog;
	}
	
	dialog = g_new0 (GossipAccountDialog, 1);

	gui = gossip_glade_get_file (GLADEDIR "/connect.glade",
				     "accounts_dialog",
				     NULL,
				     "accounts_dialog", &dialog->dialog,
				     "id_entry", &dialog->id_entry,
				     "resource_entry", &dialog->resource_entry,
				     "server_entry", &dialog->server_entry,
				     "password_entry", &dialog->password_entry,
				     "port_entry", &dialog->port_entry,
				     "ssl_checkbutton", &dialog->ssl_checkbutton,
				     "proxy_checkbutton", &dialog->proxy_checkbutton,
				     "auto_connect_checkbutton", &dialog->auto_connect_checkbutton,
				     NULL);

	gossip_glade_connect (gui, 
			      dialog,
			      "accounts_dialog", "response", accounts_dialog_response_cb,
			      "id_entry", "focus-out-event", accounts_dialog_focus_out_event,
			      "resource_entry", "focus-out-event", accounts_dialog_focus_out_event,
			      "server_entry", "focus-out-event", accounts_dialog_focus_out_event,
			      "port_entry", "focus-out-event", accounts_dialog_focus_out_event,
			      "id_entry", "changed", accounts_dialog_changed_cb,
			      "resource_entry", "changed", accounts_dialog_changed_cb,
			      "server_entry", "changed", accounts_dialog_changed_cb,
			      "port_entry", "changed", accounts_dialog_changed_cb,
			      "password_entry", "changed", accounts_dialog_changed_cb,
			      "port_entry", "insert_text", accounts_dialog_port_insert_text_cb,
			      "proxy_checkbutton", "toggled", accounts_dialog_toggled_cb,
			      "ssl_checkbutton", "toggled", accounts_dialog_toggled_cb,
			      "auto_connect_checkbutton", "toggled", accounts_dialog_toggled_cb,
			      NULL);
	
	g_signal_connect (dialog->dialog,
			  "destroy",
			  G_CALLBACK (accounts_dialog_destroy_cb),
			  &dialog);
	
	g_object_unref (gui);

	accounts_dialog_setup (dialog, NULL);
	
	gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog), 
				      GTK_WINDOW (gossip_app_get_window ()));
	gtk_widget_show (dialog->dialog);

	return dialog->dialog;
}

