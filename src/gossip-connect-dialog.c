/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002 Richard Hult <richard@imendio.com>
 * Copyright (C) 2002 Mikeal Hallendal <micke@imendio.com> 
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
#include <gtk/gtk.h>
#include <libgnome/gnome-config.h>
#include <loudmouth/loudmouth.h>
#include "gossip-utils.h"
#include "gossip-app.h"
#include "gossip-connect-dialog.h"
#include "gossip-accounts-dialog.h"
#include "gossip-account.h"

typedef struct {
	GossipApp *app;
	
	GtkWidget *dialog;
	GtkLabel  *user_label;
	GtkEntry  *resource_entry;
	GtkWidget *option_menu;
} GossipConnectDialog;

static void connect_dialog_destroy_cb         (GtkWidget            *widget,
					       GossipConnectDialog **dialog);
static void connect_dialog_response_cb        (GtkWidget            *widget,
					       gint                  response,
					       GossipConnectDialog  *dialog);
static void connect_dialog_activate_cb        (GtkWidget            *widget,
					       GossipConnectDialog  *dialog);
static void connect_dialog_edit_clicked_cb    (GtkWidget            *unused,
					       GossipConnectDialog  *dialog);
static void connect_dialog_edit_accounts      (GossipConnectDialog  *dialog);
static void connect_dialog_account_activated  (GtkMenuItem  *item,
					       GossipConnectDialog  *dialog);
static void connect_dialog_update_option_menu (GtkOptionMenu        *option_menu,
					       GCallback             callback,
					       gpointer              user_data);
static const gchar * 
connect_dialog_option_menu_get_active         (GtkOptionMenu        *option_menu);


static void
connect_dialog_destroy_cb (GtkWidget            *widget,
			   GossipConnectDialog **dialog)
{
	g_free (*dialog);
	
	*dialog = NULL;
}

static void
connect_dialog_response_cb (GtkWidget           *widget,
			    gint                 response,
			    GossipConnectDialog *dialog)
{
	GossipAccount *account;
	const gchar   *name;
	const gchar   *resource;

	switch (response) {
	case GTK_RESPONSE_OK:
		name = connect_dialog_option_menu_get_active (GTK_OPTION_MENU (dialog->option_menu));

		account = gossip_account_get (name);
		resource = gtk_entry_get_text (dialog->resource_entry);;
		if (strcmp (resource, "") != 0) {
			g_free (account->resource);
			account->resource = g_strdup (resource);
		}

		gossip_app_connect (account);
		gossip_account_unref (account);
		break;
	default:
		break;
	}

	gtk_widget_destroy (widget);
}

static void
connect_dialog_activate_cb (GtkWidget           *widget,
			    GossipConnectDialog *dialog)
{
	const gchar *name;

	connect_dialog_account_activated (GTK_MENU_ITEM (widget), dialog);

	name = connect_dialog_option_menu_get_active (GTK_OPTION_MENU (dialog->option_menu));
	
	/* Set default to the selected group. */
	if (name) {
		gnome_config_set_string (GOSSIP_ACCOUNTS_PATH "/Accounts/Default",
					 name);
		gnome_config_sync_file (GOSSIP_ACCOUNTS_PATH);
	}
}

static void
connect_dialog_edit_clicked_cb (GtkWidget           *unused,
				GossipConnectDialog *dialog)
{
	connect_dialog_edit_accounts (dialog);	
}

static void 
connect_dialog_accounts_dialog_destroyed (GtkWidget           *unused, 
					  GossipConnectDialog *dialog)
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (GTK_IS_OPTION_MENU (dialog->option_menu));
	
	connect_dialog_update_option_menu (GTK_OPTION_MENU (dialog->option_menu),
					   G_CALLBACK (connect_dialog_activate_cb),
					   dialog);
}

static void
connect_dialog_edit_accounts (GossipConnectDialog *dialog)
{
	GtkWidget *window;
	
	window = gossip_accounts_dialog_show (dialog->app);

 	g_signal_connect (window, "destroy",
 			  G_CALLBACK (connect_dialog_accounts_dialog_destroyed),
 			  dialog);
}

static void
connect_dialog_account_activated (GtkMenuItem         *item,
				  GossipConnectDialog *dialog) 
{
	GossipAccount *account;
	const char    *name;
	gchar         *user;

	name = g_object_get_data (G_OBJECT (item), "name");
	account = gossip_account_get (name);

	user = g_strdup_printf ("%s@%s", account->username, account->server);
	
	gtk_label_set_text (dialog->user_label, user);
	gtk_entry_set_text (dialog->resource_entry, account->resource);
}

static void
connect_dialog_update_option_menu (GtkOptionMenu *option_menu,
				   GCallback      callback,
				   gpointer       user_data)
{
	GSList    *accounts, *l;
	gchar     *default_name;
	GtkWidget *menu;
	GtkWidget *item;
	GtkWidget *default_item = NULL;
	gint       i;
	gint       default_index = 0;
	
	default_name = gnome_config_get_string (GOSSIP_ACCOUNTS_PATH "/Accounts/Default");

	menu = gtk_option_menu_get_menu (option_menu);
	if (menu) {
		gtk_widget_destroy (menu);
	}
	
	menu = gtk_menu_new ();

	accounts = gossip_account_get_all ();
	
	for (i = 0, l = accounts; l; i++, l = l->next) {
		GossipAccount *account = (GossipAccount *) l->data;

		item = gtk_menu_item_new_with_label (account->name);
		gtk_widget_show (item);

		if (default_name && !strcmp (default_name, account->name)) {
			default_index = i;
			default_item = item;
		}
		else if (!default_item) {
			/* Use the first item as default if we don't find one. */
			default_item = item;
		}

		g_signal_connect (item, "activate", callback, user_data);
		
		gtk_menu_append (GTK_MENU (menu), item);

		g_object_set_data_full (G_OBJECT (item),
					"username",
					g_strdup (account->username),
					g_free);

		g_object_set_data_full (G_OBJECT (item),
					"resource", 
					g_strdup (account->resource),
					g_free);

		g_object_set_data_full (G_OBJECT (item),
					"password",
					g_strdup (account->password),
					g_free);

		g_object_set_data_full (G_OBJECT (item),
					"server", g_strdup (account->server),
					g_free);

		g_object_set_data_full (G_OBJECT (item), 
					"name", g_strdup (account->name),
					g_free);
		
		gossip_account_unref (account);
	}
	g_slist_free (accounts);

	gtk_widget_show (menu);
	gtk_option_menu_set_menu (option_menu, menu);
	gtk_option_menu_set_history (option_menu, default_index);

	if (default_item) {
		gtk_widget_activate (default_item);
	}
}

static const gchar * 
connect_dialog_option_menu_get_active (GtkOptionMenu *option_menu)
{
	GtkWidget *menu;
	GtkWidget *item;

	menu = gtk_option_menu_get_menu (option_menu);
	if (!menu) {
		return NULL;
	}
	
	item = gtk_menu_get_active (GTK_MENU (menu));

	return g_object_get_data (G_OBJECT (item), "name");
}

void
gossip_connect_dialog_show (GossipApp *app)
{
	static GossipConnectDialog *dialog = NULL;
	GtkWidget                  *edit_button;

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->dialog));
		return;
	}

        dialog = g_new0 (GossipConnectDialog, 1);

	dialog->app = app;
	
	gossip_glade_get_file_simple (GLADEDIR "/connect.glade",
				      "connect_dialog",
				      NULL,
				      "connect_dialog", &dialog->dialog,
				      "account_optionmenu", &dialog->option_menu,
				      "user_label", &dialog->user_label,
				      "resource_entry", &dialog->resource_entry,
				      "edit_button", &edit_button,
				      NULL);

	g_signal_connect (dialog->dialog,
			  "destroy",
			  G_CALLBACK (connect_dialog_destroy_cb),
			  &dialog);
	
	g_signal_connect (dialog->dialog,
			  "response",
			  G_CALLBACK (connect_dialog_response_cb),
			  dialog);

	g_signal_connect (edit_button,
			  "clicked",
			  G_CALLBACK (connect_dialog_edit_clicked_cb),
			  dialog);
	
	connect_dialog_update_option_menu (GTK_OPTION_MENU (dialog->option_menu),
					   G_CALLBACK (connect_dialog_activate_cb),
					   dialog);

	gtk_widget_show (dialog->dialog);
}



