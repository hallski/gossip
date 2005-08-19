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

#include "gossip-account-info-dialog.h"
#include "gossip-ui-utils.h"
#include "gossip-app.h"
#include "gossip-register.h"


typedef struct {
	GossipAccount *account;

	GtkWidget     *dialog;

	GtkWidget     *entry_name;
	GtkWidget     *entry_id;
	GtkWidget     *entry_server;
	GtkWidget     *entry_resource;
	GtkWidget     *entry_password;
	GtkWidget     *entry_port;

	GtkWidget     *checkbutton_ssl;
	GtkWidget     *checkbutton_proxy;
	GtkWidget     *checkbutton_connect;
} GossipAccountInfoDialog;


static void account_info_dialog_setup               (GossipAccountInfoDialog *dialog);
static void account_info_dialog_save                (GossipAccountInfoDialog *dialog);
static void account_info_dialog_toggled_cb          (GtkWidget               *widget,
						     GossipAccountInfoDialog *dialog);
static void account_info_dialog_port_insert_text_cb (GtkEditable             *editable,
						     gchar                   *new_text,
						     gint                     len,
						     gint                    *position,
						     GossipAccountInfoDialog *dialog);
static void account_info_dialog_port_changed_cb     (GtkWidget               *widget,
						     GossipAccountInfoDialog *dialog);
static void account_info_dialog_response_cb         (GtkWidget               *widget,
						     gint                     response,
						     GossipAccountInfoDialog *dialog);
static void account_info_dialog_destroy_cb          (GtkWidget               *widget,
						     GossipAccountInfoDialog *dialog);
static void account_info_dialog_port_insert_text_cb (GtkEditable             *editable,
						     gchar                   *new_text,
						     gint                     len,
						     gint                    *position,
						     GossipAccountInfoDialog *dialog);

static GHashTable *dialogs = NULL;


static void
account_info_dialog_setup (GossipAccountInfoDialog *dialog)
{
	gchar         *port_str; 
	const gchar   *id;
	
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_name), 
			    gossip_account_get_name (dialog->account));

	if (lm_ssl_is_supported ()) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->checkbutton_ssl),
					      gossip_account_get_use_ssl (dialog->account));
	} else {
		gtk_widget_set_sensitive (dialog->checkbutton_ssl, FALSE);
	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->checkbutton_proxy),
				      gossip_account_get_use_proxy (dialog->account));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->checkbutton_connect),
				      gossip_account_get_auto_connect (dialog->account));


	id = gossip_account_get_id (dialog->account);
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

		gtk_entry_set_text (GTK_ENTRY (dialog->entry_resource), resource);
		gtk_entry_set_text (GTK_ENTRY (dialog->entry_id), id_no_resource);

		g_free (name);
		g_free (host);
		g_free (id_no_resource);
	}

	if (gossip_account_get_password (dialog->account)) {
		gtk_entry_set_text (GTK_ENTRY (dialog->entry_password), 
				    gossip_account_get_password (dialog->account));
	}

	gtk_entry_set_text (GTK_ENTRY (dialog->entry_server), 
			    gossip_account_get_server (dialog->account));

	port_str = g_strdup_printf ("%d", gossip_account_get_port (dialog->account));
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_port), 
			    port_str);
	g_free (port_str);
}

static void
account_info_dialog_save (GossipAccountInfoDialog *dialog) 
{
 	const gchar   *str;
	gchar         *id_to_save;
	const gchar   *resource;
	gint           pnr;
	gboolean       bool;

	/* set name */
	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_name));
	gossip_account_set_name (dialog->account, str);

	/* set id */
	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_id));
	resource = gtk_entry_get_text (GTK_ENTRY (dialog->entry_resource));
	if (!resource || strlen (resource) < 1) {
		resource = _("Home");
	}
	
	id_to_save = g_strdup_printf ("%s/%s", str, resource);
	gossip_account_set_id (dialog->account, id_to_save);
	g_free (id_to_save);

	/* set password */
	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_password));
	gossip_account_set_password (dialog->account, str);

	/* set server */
	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_server));
	gossip_account_set_server (dialog->account, str);

	/* set port */
	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_port));
	pnr = strtol (str, NULL, 10);
	if (pnr > 0 && pnr < 65556) {
		gossip_account_set_port (dialog->account, pnr);
	}

	/* set auto connect, proxy, ssl */
	bool = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->checkbutton_connect));
	gossip_account_set_auto_connect (dialog->account, bool);

	bool = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->checkbutton_proxy));
	gossip_account_set_use_proxy (dialog->account, bool);

	bool = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->checkbutton_ssl));
	gossip_account_set_use_ssl (dialog->account, bool);
	
	/* save */
	gossip_accounts_store ();
}

static void  
account_info_dialog_toggled_cb (GtkWidget               *widget,
				GossipAccountInfoDialog *dialog)
{
	gboolean       active;
	gboolean       changed = FALSE;
	GossipAccount *account;
	
	account = dialog->account;
	
	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	
	if (widget == dialog->checkbutton_ssl) {
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
			gtk_entry_set_text (GTK_ENTRY (dialog->entry_port), port_str);
			g_free (port_str);
		}
	} else if (widget == dialog->checkbutton_proxy) {
		gossip_account_set_use_proxy (account, active);
	} else if (widget == dialog->checkbutton_connect) {
		gossip_account_set_auto_connect (account, active);
	}
}

static void  
account_info_dialog_port_insert_text_cb (GtkEditable             *editable,
					 gchar                   *new_text,
					 gint                     len,
					 gint                    *position,
					 GossipAccountInfoDialog *dialog)
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
account_info_dialog_port_changed_cb (GtkWidget               *widget,
				     GossipAccountInfoDialog *dialog)
{
	const gchar *str;
	gint         pnr;
	
	str = gtk_entry_get_text (GTK_ENTRY (widget));
	pnr = strtol (str, NULL, 10);

	if (pnr <= 0 || pnr >= 65556) {
		gchar *port_str;
		
		port_str = g_strdup_printf ("%d", gossip_account_get_port (dialog->account));
		g_signal_handlers_block_by_func (dialog->entry_port, 
						 account_info_dialog_port_changed_cb, 
						 dialog);
		gtk_entry_set_text (GTK_ENTRY (widget), port_str);
		g_signal_handlers_unblock_by_func (dialog->entry_port, 
						   account_info_dialog_port_changed_cb, 
						   dialog);
		g_free (port_str);
	}
}

static void
account_info_dialog_response_cb (GtkWidget           *widget,
				 gint                 response,
				 GossipAccountInfoDialog *dialog)
{
 	if (GTK_RESPONSE_OK) {
 		account_info_dialog_save (dialog);
	}
		
	gtk_widget_destroy (widget);
}

static void
account_info_dialog_destroy_cb (GtkWidget                *widget,
				GossipAccountInfoDialog  *dialog)
{
	g_hash_table_remove (dialogs, dialog->account);
	g_free (dialog);
}

void
gossip_account_info_dialog_show (GossipAccount *account)
{
	GossipAccountInfoDialog *dialog;
	GladeXML                *glade;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
	
	if (!dialogs) {
		dialogs = g_hash_table_new_full (g_direct_hash,
						 g_direct_equal,
						 g_object_unref,
						 NULL);
	} else {
		dialog = g_hash_table_lookup (dialogs, account);
		if (dialog) {
			gtk_window_present (GTK_WINDOW (dialog->dialog));
			return;
		}
	}

	dialog = g_new0 (GossipAccountInfoDialog, 1);
	
	dialog->account = g_object_ref (account);

	g_hash_table_insert (dialogs, dialog->account, dialog);
	
	glade = gossip_glade_get_file (GLADEDIR "/connect.glade",
				       "account_info_dialog",
				       NULL,
				       "account_info_dialog", &dialog->dialog,
				       "entry_name", &dialog->entry_name,
				       "entry_id", &dialog->entry_id,
				       "entry_resource", &dialog->entry_resource,
				       "entry_server", &dialog->entry_server,
				       "entry_password", &dialog->entry_password,
				       "entry_port", &dialog->entry_port,
				       "checkbutton_ssl", &dialog->checkbutton_ssl,
				       "checkbutton_proxy", &dialog->checkbutton_proxy,
				       "checkbutton_connect", &dialog->checkbutton_connect,
				       NULL);

	gossip_glade_connect (glade, 
			      dialog,
			      "account_info_dialog", "response", account_info_dialog_response_cb,
			      "account_info_dialog", "destroy", account_info_dialog_destroy_cb,
			      "entry_port", "changed", account_info_dialog_port_changed_cb,
			      "entry_port", "insert_text", account_info_dialog_port_insert_text_cb,
			      "checkbutton_proxy", "toggled", account_info_dialog_toggled_cb,
			      "checkbutton_ssl", "toggled", account_info_dialog_toggled_cb,
			      "checkbutton_connect", "toggled", account_info_dialog_toggled_cb,
			      NULL);
	
	g_object_unref (glade);

	account_info_dialog_setup (dialog);
	
	gtk_widget_show (dialog->dialog);
}

