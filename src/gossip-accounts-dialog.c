/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
 * Copyright (C) 2002-2003 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2002 CodeFactory AB
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
#include <loudmouth/loudmouth.h>
#include "gossip-utils.h"
#include "gossip-app.h"
#include "gossip-accounts-dialog.h"
#include "gossip-account.h"

typedef struct {
	GossipApp *app;
	
	GtkWidget *dialog;
	GtkWidget *accounts_list;
	GtkEntry  *account_entry;
	GtkEntry  *username_entry;
	GtkEntry  *resource_entry;
	GtkEntry  *server_entry;
	GtkEntry  *port_entry;
	GtkEntry  *password_entry;
	GtkWidget *add_button;
	GtkWidget *remove_button;

	GtkListStore *model;
} GossipAccountsDialog;

enum {
	COL_NAME,
	COL_ACCOUNT,
	NUM_COLS
};

static void accounts_dialog_destroy_cb         (GtkWidget             *widget,
						GossipAccountsDialog  *dialog);
static void accounts_dialog_response_cb        (GtkWidget             *widget,
						gint                   response,
						GossipAccountsDialog  *dialog);
static void accounts_dialog_add_account_cb     (GtkWidget             *widget,
						GossipAccountsDialog  *dialog);
static void accounts_dialog_remove_account_cb  (GtkWidget             *widget,
						GossipAccountsDialog  *dialog);
static gboolean accounts_dialog_update_account_cb  (GtkWidget             *widget,
						    GdkEventFocus         *event,
						    GossipAccountsDialog  *dialog);
static void accounts_dialog_set_entries        (GossipAccountsDialog *dialog,
						const gchar          *account_name,
						const gchar          *username,
						const gchar          *password,
						const gchar          *resource,
						const gchar          *server,
						guint                 port);
static void accounts_dialog_rebuild_list       (GossipAccountsDialog  *dialog);
static void accounts_dialog_selection_changed_cb (GtkTreeSelection    *selection,
						  GossipAccountsDialog *dialog);
static void accounts_dialog_passwd_entry_insert_text_cb (GtkEditable *editable,
							 gchar       *new_text,
							 gint         len,
							 gint        *position,
							 GossipAccountsDialog *dialog);

static void
accounts_dialog_destroy_cb (GtkWidget            *widget,
			    GossipAccountsDialog *dialog)
{
	g_free (dialog);
}

static void
accounts_dialog_response_cb (GtkWidget            *widget,
			     gint                  response,
			     GossipAccountsDialog *dialog)
{
	switch (response) {
	default:
		break;
	}

	gtk_widget_destroy (widget);
}

static void
accounts_dialog_add_account_cb (GtkWidget            *widget,
				GossipAccountsDialog *dialog)
{
	GtkTreeIter       iter;
	GtkTreeSelection *selection;
	GossipAccount    *account;

	gtk_list_store_append (GTK_LIST_STORE (dialog->model), &iter);

	account = gossip_account_new ("New account", NULL, NULL, NULL, NULL, 
				      LM_CONNECTION_DEFAULT_PORT);
	
	gtk_list_store_set (GTK_LIST_STORE (dialog->model),
			    &iter,
			    COL_NAME, account->name,
			    COL_ACCOUNT, account,
			    -1);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->accounts_list));
	
	gtk_tree_selection_select_iter (selection, &iter);

	gossip_account_store (account, NULL);
}

static void
accounts_dialog_remove_account_cb (GtkWidget            *widget,
				   GossipAccountsDialog *dialog)
{
	GossipAccount    *account;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	gchar            *path;
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->accounts_list));
	gtk_tree_selection_get_selected (selection, NULL, &iter);
	
	gtk_tree_model_get (GTK_TREE_MODEL (dialog->model),
			    &iter,
			    COL_ACCOUNT, &account,
			    -1);

	path = g_strdup_printf ("%s/Account: %s", 
				GOSSIP_ACCOUNTS_PATH, account->name);
	gnome_config_clean_section (path);
	g_free (path);

	accounts_dialog_rebuild_list (dialog);
	/* Move all accounts that is after the removed one! */
	gnome_config_sync_file (GOSSIP_ACCOUNTS_PATH);
}

static gboolean
accounts_dialog_update_account_cb (GtkWidget            *widget,
				   GdkEventFocus        *event,
				   GossipAccountsDialog *dialog)
{
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	GossipAccount    *account;
	gchar            *old_name = NULL;
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->accounts_list));
	
	gtk_tree_selection_get_selected (selection, NULL, &iter);
	
	gtk_tree_model_get (GTK_TREE_MODEL (dialog->model),
			    &iter,
			    COL_ACCOUNT, &account,
			    -1);
	
	if (widget == GTK_WIDGET (dialog->account_entry)) {
		old_name = account->name;
		account->name = g_strdup (gtk_entry_get_text (dialog->account_entry));
		gtk_list_store_set (GTK_LIST_STORE (dialog->model),
				    &iter,
				    COL_NAME, account->name,
				    COL_ACCOUNT, account,
				    -1);
	}
	else if (widget == GTK_WIDGET (dialog->username_entry)) {
		g_free (account->username);
		account->username = g_strdup (gtk_entry_get_text (dialog->username_entry));
	}
	else if (widget == GTK_WIDGET (dialog->password_entry)) {
		g_free (account->password);
		account->password = g_strdup (gtk_entry_get_text (dialog->password_entry));
	}
	else if (widget == GTK_WIDGET (dialog->resource_entry)) {
		g_free (account->resource);
		account->resource = g_strdup (gtk_entry_get_text (dialog->resource_entry));
	}
	else if (widget == GTK_WIDGET (dialog->server_entry)) {
		g_free (account->server);
		account->server = g_strdup (gtk_entry_get_text (dialog->server_entry));
	}
	else if (widget == GTK_WIDGET (dialog->port_entry)) {
		gint pnr;
		
		pnr = strtol (gtk_entry_get_text (dialog->port_entry), 
			      NULL, 10);
		if (pnr > 0 && pnr < 65556) {
			account->port = pnr;
		} else {
			gchar *str = g_strdup_printf ("%d", account->port);;
			gtk_entry_set_text (dialog->port_entry, str);
			g_free (str);
		}
	}

	gossip_account_store (account, old_name);
	g_free (old_name);
	
	return FALSE;
}

static void
accounts_dialog_set_entries (GossipAccountsDialog *dialog,
			     const gchar          *account_name,
			     const gchar          *username,
			     const gchar          *password,
			     const gchar          *resource,
			     const gchar          *server,
			     guint                 port) 
{
	gchar *port_str;
	
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->account_entry), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->username_entry), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->password_entry), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->resource_entry), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->server_entry), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->port_entry), TRUE);
	gtk_entry_set_text (dialog->account_entry, account_name);
	gtk_entry_set_text (dialog->username_entry, username);
	gtk_entry_set_text (dialog->password_entry, password);
	gtk_entry_set_text (dialog->resource_entry, resource);
	gtk_entry_set_text (dialog->server_entry, server);

	port_str = g_strdup_printf ("%d", port);
	gtk_entry_set_text (dialog->port_entry, port_str);
	g_free (port_str);
}
 
static void
accounts_dialog_rebuild_list (GossipAccountsDialog *dialog)
{
	GSList      *accounts, *l;
	GtkTreeIter  iter;
	
	accounts = gossip_account_get_all ();
	
	gtk_list_store_clear (GTK_LIST_STORE (dialog->model));

	for (l = accounts; l; l = l->next) {
		GossipAccount *account = (GossipAccount *) l->data;
	
		gtk_list_store_append (GTK_LIST_STORE (dialog->model), &iter);
		
		gtk_list_store_set (GTK_LIST_STORE (dialog->model),
				    &iter,
				    COL_NAME, account->name,
				    COL_ACCOUNT, account,
				    -1);
	}

	g_slist_free (accounts);
}

static void
accounts_dialog_selection_changed_cb (GtkTreeSelection     *selection,
				      GossipAccountsDialog *dialog)
{
	GtkTreeIter    iter;
	GossipAccount *account;
	
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		gtk_widget_set_sensitive (dialog->remove_button, FALSE);
		accounts_dialog_set_entries (dialog, "", "", "", "", "", LM_CONNECTION_DEFAULT_PORT);
		return;
 	}
	
	gtk_widget_set_sensitive (dialog->remove_button, TRUE);
		
	gtk_tree_model_get (GTK_TREE_MODEL (dialog->model),
			    &iter,
			    COL_ACCOUNT, &account, 
			    -1);

	accounts_dialog_set_entries (dialog,
				     account->name, account->username, 
				     account->password, account->resource,
				     account->server, account->port);
}

static void
accounts_dialog_passwd_entry_insert_text_cb (GtkEditable *editable,
					     gchar       *new_text,
					     gint         len,
					     gint        *position,
					     GossipAccountsDialog *dialog)
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

GtkWidget *
gossip_accounts_dialog_show (GossipApp *app)
{
	GossipAccountsDialog *dialog;
	GtkTreeViewColumn    *column;
	GtkCellRenderer      *cell;
	GtkTreeSelection     *selection;
	
        dialog = g_new0 (GossipAccountsDialog, 1);
	dialog->app = app;

	gossip_glade_get_file_simple (GLADEDIR "/connect.glade",
				      "accounts_dialog",
				      NULL,
				      "accounts_dialog", &dialog->dialog,
				      "accounts_list", &dialog->accounts_list,
				      "username_entry", &dialog->username_entry,
				      "resource_entry", &dialog->resource_entry,
				      "server_entry", &dialog->server_entry,
				      "port_entry", &dialog->port_entry,
				      "password_entry", &dialog->password_entry,
				      "account_entry", &dialog->account_entry,
				      "add_button", &dialog->add_button,
				      "remove_button", &dialog->remove_button,
				      NULL);
	
	g_signal_connect (dialog->dialog,
			  "destroy",
			  G_CALLBACK (accounts_dialog_destroy_cb),
			  dialog);

	g_signal_connect (dialog->dialog,
			  "response",
			  G_CALLBACK (accounts_dialog_response_cb),
			  dialog);

	g_signal_connect (dialog->add_button,
			  "clicked",
			  G_CALLBACK (accounts_dialog_add_account_cb),
			  dialog);

	g_signal_connect (dialog->remove_button,
			  "clicked",
			  G_CALLBACK (accounts_dialog_remove_account_cb),
			  dialog);

	g_signal_connect (dialog->account_entry,
			  "focus-out-event",
			  G_CALLBACK (accounts_dialog_update_account_cb),
			  dialog);
	
  	g_signal_connect (dialog->username_entry,
			  "focus-out-event",
			  G_CALLBACK (accounts_dialog_update_account_cb),
			  dialog);
	
	g_signal_connect (dialog->resource_entry,
			  "focus-out-event",
			  G_CALLBACK (accounts_dialog_update_account_cb),
			  dialog);

	g_signal_connect (dialog->server_entry,
			  "focus-out-event",
			  G_CALLBACK (accounts_dialog_update_account_cb),
			  dialog);

	g_signal_connect (dialog->port_entry,
			  "focus-out-event",
			  G_CALLBACK (accounts_dialog_update_account_cb),
			  dialog);
	
	g_signal_connect (dialog->password_entry,
			  "focus-out-event",
			  G_CALLBACK (accounts_dialog_update_account_cb),
			  dialog);

	g_signal_connect (dialog->port_entry,
			  "insert-text",
			  G_CALLBACK (accounts_dialog_passwd_entry_insert_text_cb),
			  dialog);
	
	dialog->model = gtk_list_store_new (NUM_COLS,
					    G_TYPE_STRING,
					    G_TYPE_POINTER);

	gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->accounts_list),
				 GTK_TREE_MODEL (dialog->model));
/* 	g_object_unref (dialog->model); */
	
	cell = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (NULL,
							   cell,
							   "text", COL_NAME,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->accounts_list),
				     column);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->accounts_list));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (accounts_dialog_selection_changed_cb),
			  dialog);
		
	accounts_dialog_rebuild_list (dialog);

	gtk_widget_set_sensitive (dialog->remove_button, FALSE);

  	gtk_window_set_modal (GTK_WINDOW (dialog->dialog), TRUE);
	gtk_widget_show (dialog->dialog);

 	return dialog->dialog;
}

