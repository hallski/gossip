/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2008 Imendio AB
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

#include <string.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include "gossip-app.h"
#include "gossip-account-widget-jabber.h"
#include "gossip-accounts-dialog.h"
#include "gossip-glade.h"
#include "gossip-ui-utils.h"

#define DEBUG_DOMAIN "AccountsDialog"

/* Flashing delay for icons (milliseconds). */
#define FLASH_TIMEOUT 500

typedef struct {
	GtkWidget *window;

	GtkWidget *alignment_settings;

	GtkWidget *frame_details;
	GtkWidget *frame_no_account;
	GtkWidget *label_no_account;
	GtkWidget *label_no_account_blurb;

	GtkWidget *treeview;

	GtkWidget *button_remove;
	GtkWidget *button_connect;

	GtkWidget *frame_new_account;
	GtkWidget *entry_name;
	GtkWidget *button_create;
	GtkWidget *button_back;

	GtkWidget *settings_widget;

	gboolean   connecting_show;
	guint      connecting_id;
	gboolean   registering;
	gboolean   account_changed;
} GossipAccountsDialog;

typedef struct {
	GossipAccount *account;
	GtkComboBox   *combobox;
} SetAccountData;

static void           accounts_dialog_setup                     (GossipAccountsDialog *dialog);
static void           accounts_dialog_set_connecting            (GossipAccountsDialog *dialog,
								 GossipAccount        *account);
static void           accounts_dialog_update_connect_button     (GossipAccountsDialog *dialog);
static void           accounts_dialog_update_account            (GossipAccountsDialog *dialog,
								 GossipAccount        *account);
static void           accounts_dialog_model_setup               (GossipAccountsDialog *dialog);
static void           accounts_dialog_model_add_columns         (GossipAccountsDialog *dialog);
static void           accounts_dialog_model_pixbuf_data_func    (GtkTreeViewColumn    *tree_column,
								 GtkCellRenderer      *cell,
								 GtkTreeModel         *model,
								 GtkTreeIter          *iter,
								 GossipAccountsDialog *dialog);
static GossipAccount *accounts_dialog_model_get_selected        (GossipAccountsDialog *dialog);
static void           accounts_dialog_model_set_selected        (GossipAccountsDialog *dialog,
								 GossipAccount        *account);
static void           accounts_dialog_model_select_first        (GossipAccountsDialog *dialog);
static gboolean       accounts_dialog_model_remove_selected     (GossipAccountsDialog *dialog);
static void           accounts_dialog_model_selection_changed   (GtkTreeSelection     *selection,
								 GossipAccountsDialog *dialog);
static void           accounts_dialog_model_cell_edited         (GtkCellRendererText  *cell,
								 const gchar          *path_string,
								 const gchar          *new_text,
								 GossipAccountsDialog *dialog);
static void           accounts_dialog_account_added_cb          (GossipAccountManager *manager,
								 GossipAccount        *account,
								 GossipAccountsDialog *dialog);
static void           accounts_dialog_account_removed_cb        (GossipAccountManager *manager,
								 GossipAccount        *account,
								 GossipAccountsDialog *dialog);
static void           accounts_dialog_account_name_changed_cb   (GossipAccount        *account,
								 GParamSpec           *param,
								 GossipAccountsDialog *dialog);
static gboolean       accounts_dialog_flash_connecting_foreach  (GtkTreeModel         *model,
								 GtkTreePath          *path,
								 GtkTreeIter          *iter,
								 gpointer              user_data);
static gboolean       accounts_dialog_flash_connecting_cb       (GossipAccountsDialog *dialog);
static void           accounts_dialog_protocol_connecting_cb    (GossipSession        *session,
								 GossipAccount        *account,
								 GossipJabber         *jabber,
								 GossipAccountsDialog *dialog);
static void           accounts_dialog_protocol_connected_cb     (GossipSession        *session,
								 GossipAccount        *account,
								 GossipJabber         *jabber,
								 GossipAccountsDialog *dialog);
static void           accounts_dialog_protocol_disconnected_cb  (GossipSession        *session,
								 GossipAccount        *account,
								 GossipJabber         *jabber,
								 gint                  reason,
								 GossipAccountsDialog *dialog);
static void           accounts_dialog_protocol_error_cb         (GossipSession        *session,
								 GossipJabber         *jabber,
								 GossipAccount        *account,
								 GError               *error,
								 GossipAccountsDialog *dialog);
static void           accounts_dialog_entry_name_changed_cb     (GtkWidget            *widget,
								 GossipAccountsDialog *dialog);
static void           accounts_dialog_button_create_clicked_cb  (GtkWidget            *button,
								 GossipAccountsDialog *dialog);
static void           accounts_dialog_button_back_clicked_cb    (GtkWidget            *button,
								 GossipAccountsDialog *dialog);
static void           accounts_dialog_button_connect_clicked_cb (GtkWidget            *button,
								 GossipAccountsDialog *dialog);
static void           accounts_dialog_button_add_clicked_cb     (GtkWidget            *button,
								 GossipAccountsDialog *dialog);
static void           accounts_dialog_remove_response_cb        (GtkWidget            *dialog,
								 gint                  response,
								 GossipAccount        *account);
static void           accounts_dialog_button_remove_clicked_cb  (GtkWidget            *button,
								 GossipAccountsDialog *dialog);
static void           accounts_dialog_treeview_row_activated_cb (GtkTreeView          *tree_view,
								 GtkTreePath          *path,
								 GtkTreeViewColumn    *column,
								 gpointer             *dialog);
static void           accounts_dialog_save                      (GossipAccountsDialog *dialog);
static gboolean       accounts_dialog_foreach                   (GtkTreeModel         *model,
								 GtkTreePath          *path,
								 GtkTreeIter          *iter,
								 GossipAccountsDialog *dialog);
static void           accounts_dialog_response_cb               (GtkWidget            *widget,
								 gint                  response,
								 GossipAccountsDialog *dialog);
static void           accounts_dialog_destroy_cb                (GtkWidget            *widget,
								 GossipAccountsDialog *dialog);

enum {
	COL_NAME,
	COL_EDITABLE,
	COL_DEFAULT,
	COL_CONNECTED,
	COL_CONNECTING,
	COL_AUTO_CONNECT,
	COL_ACCOUNT_POINTER,
	COL_COUNT
};

static void
accounts_dialog_setup (GossipAccountsDialog *dialog)
{
	GossipSession        *session;
	GossipAccountManager *manager;
	GtkTreeView          *view;
	GtkListStore         *store;
	GtkTreeSelection     *selection;
	GtkTreeIter           iter;
	GList                *accounts, *l;
	GossipAccount        *default_account = NULL;
	gboolean              is_editable = FALSE;

	view = GTK_TREE_VIEW (dialog->treeview);
	store = GTK_LIST_STORE (gtk_tree_view_get_model (view));
	selection = gtk_tree_view_get_selection (view);

	session = gossip_app_get_session ();
	manager = gossip_session_get_account_manager (session);
	accounts = gossip_account_manager_get_accounts (manager);

	default_account = gossip_account_manager_get_default (manager);

	for (l = accounts; l; l = l->next) {
		GossipAccount *account;
		const gchar   *name;
		gboolean       is_default;
		gboolean       is_connected;
		gboolean       is_connecting;

		account = l->data;

		name = gossip_account_get_name (account);
		if (!name) {
			continue;
		}

		is_default = gossip_account_equal (account, default_account);
		is_connected = gossip_session_is_connected (session, account);
		is_connecting =	gossip_session_is_connecting (session, account);

		gossip_debug (DEBUG_DOMAIN,
			      "Adding account:'%s', connected:%s, connecting:%s",
			      name, 
			      is_connected ? "YES" : "NO",
			      is_connecting ? "YES" : "NO");

		gtk_list_store_insert_with_values (store, &iter,
						   -1,
						   COL_NAME, name,
						   COL_EDITABLE, is_editable,
						   COL_DEFAULT, is_default,
						   COL_CONNECTED, is_connected,
						   COL_CONNECTING, is_connecting,
						   COL_AUTO_CONNECT, gossip_account_get_auto_connect (account),
						   COL_ACCOUNT_POINTER, account,
						   -1);

		g_signal_connect (account, "notify::name",
				  G_CALLBACK (accounts_dialog_account_name_changed_cb), 
				  dialog);

		if (is_connecting) {
			accounts_dialog_set_connecting (dialog, account);
		}
	}

	g_list_foreach (accounts, (GFunc) g_object_unref, NULL);
	g_list_free (accounts);
}

static void
accounts_dialog_set_connecting (GossipAccountsDialog *dialog,
				GossipAccount        *account)
{
	GtkTreeView      *view;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	gboolean          ok;
	GossipAccount    *selected_account;

	if (!dialog->connecting_id) {
		dialog->connecting_id = g_timeout_add (FLASH_TIMEOUT,
						       (GSourceFunc) accounts_dialog_flash_connecting_cb,
						       dialog);
	}

	selected_account = accounts_dialog_model_get_selected (dialog);
	if (selected_account) {
		if (gossip_account_equal (selected_account, account)) {
			gtk_widget_set_sensitive (dialog->button_remove, FALSE);
		}

		g_object_unref (selected_account);
	}

	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);

	for (ok = gtk_tree_model_get_iter_first (model, &iter);
	     ok;
	     ok = gtk_tree_model_iter_next (model, &iter)) {
		GossipAccount *this_account;
		gboolean       equal;

		gtk_tree_model_get (model, &iter,
				    COL_ACCOUNT_POINTER, &this_account,
				    -1);

		equal = gossip_account_equal (this_account, account);
		g_object_unref (this_account);

		if (equal) {
			GtkTreePath *path;

			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    COL_CONNECTING, TRUE,
					    -1);

			path = gtk_tree_model_get_path (model, &iter);
			gtk_tree_model_row_changed (model, path, &iter);
			gtk_tree_path_free (path);

			break;
		}
	}

	accounts_dialog_update_connect_button (dialog);
}

static void
accounts_dialog_update_connect_button (GossipAccountsDialog *dialog)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	const gchar      *stock_id;
	gboolean          is_connected;
	gboolean          is_connecting;
	GtkWidget        *image;

	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_widget_set_sensitive (dialog->button_connect, FALSE);
		return;
	}

	gtk_tree_model_get (model, &iter,
			    COL_CONNECTED, &is_connected,
			    COL_CONNECTING, &is_connecting,
			    -1);

	if (is_connecting) {
		stock_id = GTK_STOCK_STOP;
	} else {
		if (is_connected) {
			stock_id = GTK_STOCK_DISCONNECT;
		} else {
			stock_id = GTK_STOCK_CONNECT;
		}
	}

	image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);
	gtk_button_set_image (GTK_BUTTON (dialog->button_connect), image);
	gtk_widget_show (image); /* override gtk-button-images setting */
}

static void
accounts_dialog_update_account (GossipAccountsDialog *dialog,
				GossipAccount        *account)
{
	if (dialog->settings_widget) {
		gtk_widget_destroy (dialog->settings_widget);
		dialog->settings_widget = NULL;
	}

	if (!account) {
		GtkTreeView  *view;
		GtkTreeModel *model;

		gtk_widget_show (dialog->frame_no_account);
		gtk_widget_hide (dialog->frame_details);

		gtk_widget_set_sensitive (dialog->button_connect, FALSE);
		gtk_widget_set_sensitive (dialog->button_remove, FALSE);

		view = GTK_TREE_VIEW (dialog->treeview);
		model = gtk_tree_view_get_model (view);

		if (gtk_tree_model_iter_n_children (model, NULL) > 0) {
			gtk_label_set_markup (GTK_LABEL (dialog->label_no_account),
					      _("<b>No Account Selected</b>"));
			gtk_label_set_markup (GTK_LABEL (dialog->label_no_account_blurb),
					      _("To add a new account, you can click on the "
						"'Add' button and a new entry will be created "
						"for you to start configuring.\n"
						"\n"
						"If you do not want to add an account, simply "
						"click on the account you want to configure in "
						"the list on the left."));
		} else {
			gtk_label_set_markup (GTK_LABEL (dialog->label_no_account),
					      _("<b>No Accounts Configured</b>"));
			gtk_label_set_markup (GTK_LABEL (dialog->label_no_account_blurb),
					      _("To add a new account, you can click on the "
						"'Add' button and a new entry will be created "
						"for you to start configuring."));
		}
	} else {
		gtk_widget_hide (dialog->frame_no_account);
		gtk_widget_show (dialog->frame_details);

		dialog->settings_widget = 
			gossip_account_widget_jabber_new (account);
	}

	if (dialog->settings_widget) {
		gtk_container_add (GTK_CONTAINER (dialog->alignment_settings),
				   dialog->settings_widget);

		gtk_widget_show (dialog->settings_widget);

		/* Set default again, this seems to get changed after
		 * resetting the settings widget.
		 */
		g_object_set (dialog->button_create, 
			      "can-default", TRUE,
			      "has-default", TRUE,
			      NULL);
	}
}

static void
accounts_dialog_model_setup (GossipAccountsDialog *dialog)
{
	GtkListStore     *store;
	GtkTreeSelection *selection;

	store = gtk_list_store_new (COL_COUNT,
				    G_TYPE_STRING,        /* name */
				    G_TYPE_BOOLEAN,       /* editable */
				    G_TYPE_BOOLEAN,       /* default */
				    G_TYPE_BOOLEAN,       /* connected */
				    G_TYPE_BOOLEAN,       /* connecting */
				    G_TYPE_BOOLEAN,       /* auto start */
				    GOSSIP_TYPE_ACCOUNT); /* account */

	gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->treeview),
				 GTK_TREE_MODEL (store));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect (selection, "changed",
			  G_CALLBACK (accounts_dialog_model_selection_changed),
			  dialog);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      COL_NAME, GTK_SORT_ASCENDING);

	accounts_dialog_model_add_columns (dialog);

	g_object_unref (store);
}

static void
accounts_dialog_model_add_columns (GossipAccountsDialog *dialog)
{
	GtkTreeView       *view;
	GtkTreeViewColumn *column;
	GtkCellRenderer   *cell;

	view = GTK_TREE_VIEW (dialog->treeview);
	/* gtk_tree_view_set_headers_visible (view, TRUE); */

	/* account name/status */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Accounts"));

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc)
						 accounts_dialog_model_pixbuf_data_func,
						 dialog,
						 NULL);

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_add_attribute (column,
					    cell,
					    "text", COL_NAME);
	g_signal_connect (cell, "edited",
			  G_CALLBACK (accounts_dialog_model_cell_edited),
			  dialog);

	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_append_column (view, column);
}

static void
accounts_dialog_model_select_first (GossipAccountsDialog *dialog)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;

	/* select first */
	view = GTK_TREE_VIEW (dialog->treeview);
	model = gtk_tree_view_get_model (view);
	
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		selection = gtk_tree_view_get_selection (view);
		gtk_tree_selection_select_iter (selection, &iter);
	} else {
		accounts_dialog_update_account (dialog, NULL);
	}
}

static void
accounts_dialog_model_pixbuf_data_func (GtkTreeViewColumn    *tree_column,
					GtkCellRenderer      *cell,
					GtkTreeModel         *model,
					GtkTreeIter          *iter,
					GossipAccountsDialog *dialog)
{
	GossipAccount *account;
	GdkPixbuf     *pixbuf;
	gboolean       is_connected;
	gboolean       is_connecting;

	gtk_tree_model_get (model, iter,
			    COL_CONNECTED, &is_connected,
			    COL_CONNECTING, &is_connecting,
			    COL_ACCOUNT_POINTER, &account,
			    -1);

	pixbuf = gossip_account_create_pixbuf (account, GTK_ICON_SIZE_BUTTON);

	if (pixbuf) {
		if ((!is_connecting && !is_connected) ||
		    (is_connecting && !dialog->connecting_show)) {
			GdkPixbuf *modded_pixbuf;

			modded_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
							TRUE,
							8,
							gdk_pixbuf_get_width (pixbuf),
							gdk_pixbuf_get_height (pixbuf));

			gdk_pixbuf_saturate_and_pixelate (pixbuf,
							  modded_pixbuf,
							  1.0,
							  TRUE);
			g_object_unref (pixbuf);
			pixbuf = modded_pixbuf;
		}
	}

	g_object_set (cell,
		      "visible", TRUE,
		      "pixbuf", pixbuf,
		      NULL);

	g_object_unref (account);

	if (pixbuf) {
		g_object_unref (pixbuf);
	}
}

static GossipAccount *
accounts_dialog_model_get_selected (GossipAccountsDialog *dialog)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	GossipAccount    *account;

	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter, COL_ACCOUNT_POINTER, &account, -1);
	return account;
}

static void
accounts_dialog_model_set_selected (GossipAccountsDialog *dialog,
				    GossipAccount        *account)
{
	GtkTreeView      *view;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	gboolean          ok;

	view = GTK_TREE_VIEW (dialog->treeview);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);

	for (ok = gtk_tree_model_get_iter_first (model, &iter);
	     ok;
	     ok = gtk_tree_model_iter_next (model, &iter)) {
		GossipAccount *this_account;
		gboolean       equal;

		gtk_tree_model_get (model, &iter,
				    COL_ACCOUNT_POINTER, &this_account,
				    -1);

		equal = gossip_account_equal (this_account, account);
		g_object_unref (this_account);

		if (equal) {
			gtk_tree_selection_select_iter (selection, &iter);
			break;
		}
	}
}

static gboolean
accounts_dialog_model_remove_selected (GossipAccountsDialog *dialog)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;

	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return FALSE;
	}

	return gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static void
accounts_dialog_model_selection_changed (GtkTreeSelection     *selection,
					 GossipAccountsDialog *dialog)
{
	GossipSession *session;
	GossipAccount *account;
	GtkTreeModel  *model;
	GtkTreeIter    iter;
	gboolean       is_selection;

	is_selection = gtk_tree_selection_get_selected (selection, &model, &iter);

	gtk_widget_set_sensitive (dialog->button_remove, is_selection);
	gtk_widget_set_sensitive (dialog->button_connect, is_selection);

	accounts_dialog_update_connect_button (dialog);

	session = gossip_app_get_session ();
	account = accounts_dialog_model_get_selected (dialog);
	accounts_dialog_update_account (dialog, account);

	if (!account) {
		return;
	}

	if (gossip_session_is_connected (session, account)) {
		gtk_widget_set_sensitive (dialog->button_remove, FALSE);
	}

	g_object_unref (account);
}

static void
accounts_dialog_model_cell_edited (GtkCellRendererText  *cell,
				   const gchar          *path_string,
				   const gchar          *new_text,
				   GossipAccountsDialog *dialog)
{
	GossipSession        *session;
	GossipAccountManager *manager;
	GossipAccount        *account;

	GtkTreeView          *view;
	GtkTreeModel         *model;
	GtkTreePath          *path;
	GtkTreeIter           iter;
	gint                  column;
	gchar                *old_text;

	view = GTK_TREE_VIEW (dialog->treeview);
	model = gtk_tree_view_get_model (view);

	path = gtk_tree_path_new_from_string (path_string);
	gtk_tree_model_get_iter (model, &iter, path);

	column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column"));

	gtk_tree_model_get (model, &iter, column, &old_text, -1);

	session = gossip_app_get_session ();
	manager = gossip_session_get_account_manager (session);

	account = gossip_account_manager_find (manager, old_text);
	if (account) {
		gossip_account_set_name (account, new_text);
		gossip_account_manager_store (manager);
	}

	g_free (old_text);

	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    column, new_text, -1);

	/* update account info */

	gtk_tree_path_free (path);
}

static void
accounts_dialog_account_added_cb (GossipAccountManager *manager,
				  GossipAccount        *account,
				  GossipAccountsDialog *dialog)
{
	GossipSession *session;
	GossipAccount *default_account;
	const gchar   *name;
	gboolean       is_default;
	gboolean       is_connected;
	GtkTreeView   *view;
	GtkTreeModel  *model;
	GtkListStore  *store;
	GtkTreeIter    iter;

	view = GTK_TREE_VIEW (dialog->treeview);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);

	session = gossip_app_get_session ();

	default_account = gossip_account_manager_get_default (manager);
	is_default = (gossip_account_equal (account, default_account));
	is_connected = 	(gossip_session_is_connected (session, account));

	name = gossip_account_get_name (account);

	g_return_if_fail (name != NULL);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    COL_NAME, name,
			    COL_EDITABLE, FALSE,
			    COL_DEFAULT, is_default,
			    COL_CONNECTED, is_connected,
			    COL_AUTO_CONNECT, gossip_account_get_auto_connect (account),
			    COL_ACCOUNT_POINTER, account,
			    -1);

	g_signal_connect (account, "notify::name",
			  G_CALLBACK (accounts_dialog_account_name_changed_cb),
			  dialog);
}

static void
accounts_dialog_account_removed_cb (GossipAccountManager *manager,
				    GossipAccount        *account,
				    GossipAccountsDialog *dialog)
{
	g_signal_handlers_disconnect_by_func (account,
					      accounts_dialog_account_name_changed_cb,
					      dialog);

	accounts_dialog_model_set_selected (dialog, account);
	accounts_dialog_model_remove_selected (dialog);
}

static void
accounts_dialog_account_name_changed_cb (GossipAccount        *account,
					 GParamSpec           *param,
					 GossipAccountsDialog *dialog)
{
	GtkTreeView      *view;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	gboolean          ok;

	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);

	for (ok = gtk_tree_model_get_iter_first (model, &iter);
	     ok;
	     ok = gtk_tree_model_iter_next (model, &iter)) {
		GossipAccount *this_account;
		gboolean       equal;

		gtk_tree_model_get (model, &iter,
				    COL_ACCOUNT_POINTER, &this_account,
				    -1);

		equal = gossip_account_equal (this_account, account);
		g_object_unref (this_account);

		if (equal) {
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    COL_NAME, gossip_account_get_name (account),
					    -1);
			break;
		}
	}
}

static gboolean
accounts_dialog_flash_connecting_foreach (GtkTreeModel *model,
					  GtkTreePath  *path,
					  GtkTreeIter  *iter,
					  gpointer      user_data)
{
	gboolean is_connecting;

	gtk_tree_model_get (model, iter, COL_CONNECTING, &is_connecting, -1);
	gtk_tree_model_row_changed (model, path, iter);

	return FALSE;
}

static gboolean
accounts_dialog_flash_connecting_cb (GossipAccountsDialog *dialog)
{
	GtkTreeView  *view;
	GtkTreeModel *model;

	dialog->connecting_show = !dialog->connecting_show;

	view = GTK_TREE_VIEW (dialog->treeview);
	model = gtk_tree_view_get_model (view);

	gtk_tree_model_foreach (model, accounts_dialog_flash_connecting_foreach, NULL);

	return TRUE;
}

static void
accounts_dialog_protocol_connecting_cb (GossipSession        *session,
					GossipAccount        *account,
					GossipJabber         *jabber,
					GossipAccountsDialog *dialog)
{
	accounts_dialog_set_connecting (dialog, account);
}

static void
accounts_dialog_protocol_connected_cb (GossipSession        *session,
				       GossipAccount        *account,
				       GossipJabber         *jabber,
				       GossipAccountsDialog *dialog)
{
	GtkTreeView      *view;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	gboolean          ok;
	GossipAccount    *selected_account;
	guint             connecting;

	gossip_session_count_accounts (session, NULL, &connecting, NULL);
	if (connecting < 1 && dialog->connecting_id) {
		g_source_remove (dialog->connecting_id);
		dialog->connecting_id = 0;
	}

	selected_account = accounts_dialog_model_get_selected (dialog);
	if (selected_account) {
		if (gossip_account_equal (selected_account, account)) {
			gtk_widget_set_sensitive (dialog->button_remove, FALSE);
		}

		g_object_unref (selected_account);
	}

	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);

	for (ok = gtk_tree_model_get_iter_first (model, &iter);
	     ok;
	     ok = gtk_tree_model_iter_next (model, &iter)) {
		GossipAccount *this_account;
		gboolean       equal;

		gtk_tree_model_get (model, &iter,
				    COL_ACCOUNT_POINTER, &this_account,
				    -1);

		equal = gossip_account_equal (this_account, account);
		g_object_unref (this_account);

		if (equal) {
			GtkTreePath *path;

			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    COL_CONNECTED, TRUE,
					    COL_CONNECTING, FALSE,
					    -1);

			path = gtk_tree_model_get_path (model, &iter);
			gtk_tree_model_row_changed (model, path, &iter);
			gtk_tree_path_free (path);

			break;
		}
	}

	accounts_dialog_update_connect_button (dialog);
}

static void
accounts_dialog_protocol_disconnected_cb (GossipSession        *session,
					  GossipAccount        *account,
					  GossipJabber         *jabber,
					  gint                  reason,
					  GossipAccountsDialog *dialog)
{
	GtkTreeView      *view;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	gboolean          ok;
	GossipAccount    *selected_account;
	guint             connecting;

	gossip_session_count_accounts (session, NULL, &connecting, NULL);
	if (connecting < 1 && dialog->connecting_id) {
		g_source_remove (dialog->connecting_id);
		dialog->connecting_id = 0;
	}

	selected_account = accounts_dialog_model_get_selected (dialog);
	if (selected_account) {
		if (gossip_account_equal (selected_account, account)) {
			gtk_widget_set_sensitive (dialog->button_remove, TRUE);
		}

		g_object_unref (selected_account);
	}

	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);

	for (ok = gtk_tree_model_get_iter_first (model, &iter);
	     ok;
	     ok = gtk_tree_model_iter_next (model, &iter)) {
		GossipAccount *this_account;
		gboolean       equal;

		gtk_tree_model_get (model, &iter,
				    COL_ACCOUNT_POINTER, &this_account,
				    -1);

		equal = gossip_account_equal (this_account, account);
		g_object_unref (this_account);

		if (equal) {
			GtkTreePath *path;

			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    COL_CONNECTED, FALSE,
					    COL_CONNECTING, FALSE,
					    -1);

			path = gtk_tree_model_get_path (model, &iter);
			gtk_tree_model_row_changed (model, path, &iter);
			gtk_tree_path_free (path);

			break;
		}
	}

	accounts_dialog_update_connect_button (dialog);
}

static void
accounts_dialog_protocol_error_cb (GossipSession        *session,
				   GossipJabber         *jabber,
				   GossipAccount        *account,
				   GError               *error,
				   GossipAccountsDialog *dialog)
{
	GtkTreeView      *view;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	gboolean          ok;
	GossipAccount    *selected_account;
	guint             connecting;

	gossip_session_count_accounts (session, NULL, &connecting, NULL);
	if (connecting < 1 && dialog->connecting_id) {
		g_source_remove (dialog->connecting_id);
		dialog->connecting_id = 0;
	}

	selected_account = accounts_dialog_model_get_selected (dialog);
	if (selected_account) {
		if (gossip_account_equal (selected_account, account)) {
			gtk_widget_set_sensitive (dialog->button_remove, TRUE);
		}

		g_object_unref (selected_account);
	}

	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);

	for (ok = gtk_tree_model_get_iter_first (model, &iter);
	     ok;
	     ok = gtk_tree_model_iter_next (model, &iter)) {
		GossipAccount *this_account;
		gboolean       equal;

		gtk_tree_model_get (model, &iter,
				    COL_ACCOUNT_POINTER, &this_account,
				    -1);

		equal = gossip_account_equal (this_account, account);
		g_object_unref (this_account);

		if (equal) {
			GtkTreePath *path;

			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    COL_CONNECTED, FALSE,
					    COL_CONNECTING, FALSE,
					    -1);

			path = gtk_tree_model_get_path (model, &iter);
			gtk_tree_model_row_changed (model, path, &iter);
			gtk_tree_path_free (path);

			break;
		}
	}

	accounts_dialog_update_connect_button (dialog);
}

static void          
accounts_dialog_entry_name_changed_cb (GtkWidget             *widget,
				       GossipAccountsDialog  *dialog)
{
	const gchar *str;
	
	str = gtk_entry_get_text (GTK_ENTRY (widget));
	gtk_widget_set_sensitive (dialog->button_create, !G_STR_EMPTY (str));
}

static void
accounts_dialog_button_create_clicked_cb (GtkWidget             *button,
					  GossipAccountsDialog  *dialog)
{
	GossipSession        *session;
	GossipAccountManager *manager;
	GossipAccount        *account;
	const gchar          *str;

	/* Update widgets */
	gtk_widget_show (dialog->frame_details);
	gtk_widget_hide (dialog->frame_no_account);
	gtk_widget_hide (dialog->frame_new_account);

	/* Create account */
	session = gossip_app_get_session ();
	manager = gossip_session_get_account_manager (session);
	account = gossip_session_new_account (session);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_name));
	gossip_account_set_name (account, str);

	gossip_account_manager_add (manager, account);
	accounts_dialog_model_set_selected (dialog, account);
	g_object_unref (account);
}

static void
accounts_dialog_button_back_clicked_cb (GtkWidget             *button,
					GossipAccountsDialog  *dialog)
{
	GossipAccount *account;

	gtk_widget_hide (dialog->frame_details);
	gtk_widget_hide (dialog->frame_no_account);
	gtk_widget_hide (dialog->frame_new_account);

	account = accounts_dialog_model_get_selected (dialog);
	accounts_dialog_update_account (dialog, account);
}

static void
accounts_dialog_button_connect_clicked_cb (GtkWidget            *button,
					   GossipAccountsDialog *dialog)
{
	GtkTreeView          *view;
	GtkTreeModel         *model;
	GtkTreeSelection     *selection;
	GtkTreeIter           iter;
	GossipSession        *session;
	GossipAccountManager *manager;
	GossipAccount        *account;
	gboolean              is_connected;
	gboolean              is_connecting;
	gboolean              should_connect;

	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		g_warning ("No selection but connect button clicked?");
		return;
	}

	session = gossip_app_get_session ();
	manager = gossip_session_get_account_manager (session);

	account = accounts_dialog_model_get_selected (dialog);

	is_connected = gossip_session_is_connected (session, account);
	is_connecting = gossip_session_is_connecting (session, account);

	/* Make sure we update our information */
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    COL_CONNECTED, is_connected,
			    COL_CONNECTING, is_connecting,
			    -1);

	if (!is_connected) {
		if (is_connecting) {
			should_connect = FALSE;
		} else {
			should_connect = TRUE;
		}
	} else {
		should_connect = FALSE;
	}

	if (should_connect) {
		gossip_session_connect (session, account, FALSE);
	} else {
		gossip_session_disconnect (session, account);
	}

	g_object_unref (account);
}

static void
accounts_dialog_button_add_clicked_cb (GtkWidget            *button,
				       GossipAccountsDialog *dialog)
{
	gtk_widget_hide (dialog->frame_details);
	gtk_widget_hide (dialog->frame_no_account);
	gtk_widget_show (dialog->frame_new_account);

	gtk_entry_set_text (GTK_ENTRY (dialog->entry_name), "");
	gtk_widget_grab_focus (dialog->entry_name);
}

static void
accounts_dialog_remove_response_cb (GtkWidget     *dialog,
				    gint           response,
				    GossipAccount *account)
{
	gtk_widget_destroy (dialog);

	if (response == GTK_RESPONSE_YES) {
		GossipSession        *session;
		GossipAccountManager *manager;

		session = gossip_app_get_session ();
		manager = gossip_session_get_account_manager (session);

		gossip_account_manager_remove (manager, account);
		gossip_account_manager_store (manager);
	}

	g_object_unref (account);
}

static void
accounts_dialog_button_remove_clicked_cb (GtkWidget            *button,
					  GossipAccountsDialog *dialog)
{
	GossipAccount *account;
	GtkWidget     *message_dialog;

	account = accounts_dialog_model_get_selected (dialog);

	message_dialog = gtk_message_dialog_new
		(GTK_WINDOW (dialog->window),
		 GTK_DIALOG_DESTROY_WITH_PARENT,
		 GTK_MESSAGE_QUESTION,
		 GTK_BUTTONS_NONE,
		 _("You are about to remove your %s account!\n"
		   "Are you sure you want to proceed?"),
		 gossip_account_get_name (account));

	gtk_message_dialog_format_secondary_text
		(GTK_MESSAGE_DIALOG (message_dialog),
		 _("Any associated conversations and chat rooms will NOT be "
		   "removed if you decide to proceed.\n"
		   "\n"
		   "Should you decide to add the account back at a later time, "
		   "they will still be available."));

	gtk_dialog_add_button (GTK_DIALOG (message_dialog),
			       GTK_STOCK_CANCEL, 
			       GTK_RESPONSE_NO);
	gtk_dialog_add_button (GTK_DIALOG (message_dialog),
			       GTK_STOCK_REMOVE, 
			       GTK_RESPONSE_YES);

	g_signal_connect (message_dialog, "response",
			  G_CALLBACK (accounts_dialog_remove_response_cb),
			  account);

	gtk_widget_show (message_dialog);
}

static void
accounts_dialog_treeview_row_activated_cb (GtkTreeView           *tree_view,
					   GtkTreePath           *path,
					   GtkTreeViewColumn     *column,
					   gpointer              *data)
{
	GossipAccountsDialog *dialog = (GossipAccountsDialog *) data;

	accounts_dialog_button_connect_clicked_cb (dialog->button_connect,
						   dialog);
}

static void
accounts_dialog_save (GossipAccountsDialog *dialog)
{
	GossipSession        *session;
	GossipAccount        *account;
	GossipAccountManager *manager;

	dialog->account_changed = FALSE;

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);
 	account = accounts_dialog_model_get_selected (dialog);

	if (!account) {
		return;
	}

	gossip_account_manager_store (manager);

	g_object_unref (account);
}

static gboolean
accounts_dialog_foreach (GtkTreeModel         *model,
			 GtkTreePath          *path,
			 GtkTreeIter          *iter,
			 GossipAccountsDialog *dialog)
{
	GossipAccount *account;

	gtk_tree_model_get (model, iter, COL_ACCOUNT_POINTER, &account, -1);

	g_signal_handlers_disconnect_by_func (account,
					      accounts_dialog_account_name_changed_cb,
					      dialog);

	g_object_unref (account);

	return FALSE;
}

static void
accounts_dialog_response_cb (GtkWidget            *widget,
			     gint                  response,
			     GossipAccountsDialog *dialog)
{
	gtk_widget_destroy (widget);
}

static void
accounts_dialog_destroy_cb (GtkWidget            *widget,
			    GossipAccountsDialog *dialog)
{
	GossipSession        *session;
	GossipAccountManager *manager;
	GtkTreeModel         *model;

	session = gossip_app_get_session ();
	manager = gossip_session_get_account_manager (session);

	g_signal_handlers_disconnect_by_func (manager,
					      accounts_dialog_account_added_cb,
					      dialog);

	g_signal_handlers_disconnect_by_func (manager,
					      accounts_dialog_account_removed_cb,
					      dialog);

	g_signal_handlers_disconnect_by_func (session,
					      accounts_dialog_protocol_connecting_cb,
					      dialog);

	g_signal_handlers_disconnect_by_func (session,
					      accounts_dialog_protocol_connected_cb,
					      dialog);

	g_signal_handlers_disconnect_by_func (session,
					      accounts_dialog_protocol_disconnected_cb,
					      dialog);

	g_signal_handlers_disconnect_by_func (session,
					      accounts_dialog_protocol_error_cb,
					      dialog);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->treeview));
	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) accounts_dialog_foreach,
				dialog);

	if (dialog->account_changed) {
 		accounts_dialog_save (dialog); 
	}

	if (dialog->connecting_id) {
		g_source_remove (dialog->connecting_id);
	}

	g_free (dialog);
}

void
gossip_accounts_dialog_show (GossipAccount *account)
{
	GossipSession               *session;
	GossipAccountManager        *manager;
	static GossipAccountsDialog *dialog = NULL;
	GladeXML                    *glade;
	GtkWidget                   *bbox;
	GtkWidget                   *button_close;

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->window));
		return;
	}

	dialog = g_new0 (GossipAccountsDialog, 1);

	glade = gossip_glade_get_file ("main.glade",
				       "accounts_dialog",
				       NULL,
				       "accounts_dialog", &dialog->window,
				       "frame_details", &dialog->frame_details,
				       "frame_no_account", &dialog->frame_no_account,
				       "label_no_account", &dialog->label_no_account,
				       "label_no_account_blurb", &dialog->label_no_account_blurb,
				       "alignment_settings", &dialog->alignment_settings,
				       "dialog-action_area", &bbox,
				       "treeview", &dialog->treeview,
				       "frame_new_account", &dialog->frame_new_account,
				       "entry_name", &dialog->entry_name,
				       "button_create", &dialog->button_create,
				       "button_back", &dialog->button_back,
				       "button_remove", &dialog->button_remove,
				       "button_connect", &dialog->button_connect,
				       "button_close", &button_close,
				       NULL);

	gossip_glade_connect (glade,
			      dialog,
			      "accounts_dialog", "destroy", accounts_dialog_destroy_cb,
			      "accounts_dialog", "response", accounts_dialog_response_cb,
			      "button_create", "clicked", accounts_dialog_button_create_clicked_cb,
			      "button_back", "clicked", accounts_dialog_button_back_clicked_cb,
			      "entry_name", "changed", accounts_dialog_entry_name_changed_cb,
			      "treeview", "row-activated", accounts_dialog_treeview_row_activated_cb,
			      "button_connect", "clicked", accounts_dialog_button_connect_clicked_cb,
			      "button_add", "clicked", accounts_dialog_button_add_clicked_cb,
			      "button_remove", "clicked", accounts_dialog_button_remove_clicked_cb,
			      NULL);

	g_object_add_weak_pointer (G_OBJECT (dialog->window), (gpointer) &dialog);

	g_object_unref (glade);

	/* Set up signalling */
	session = gossip_app_get_session ();
	manager = gossip_session_get_account_manager (session);

	g_signal_connect (manager, "account_added",
			  G_CALLBACK (accounts_dialog_account_added_cb),
			  dialog);

	g_signal_connect (manager, "account_removed",
			  G_CALLBACK (accounts_dialog_account_removed_cb),
			  dialog);

	g_signal_connect (session, "protocol-connecting",
			  G_CALLBACK (accounts_dialog_protocol_connecting_cb),
			  dialog);

	g_signal_connect (session, "protocol-connected",
			  G_CALLBACK (accounts_dialog_protocol_connected_cb),
			  dialog);

	g_signal_connect (session, "protocol-disconnected",
			  G_CALLBACK (accounts_dialog_protocol_disconnected_cb),
			  dialog);

	g_signal_connect (session, "protocol-error",
			  G_CALLBACK (accounts_dialog_protocol_error_cb),
			  dialog);

	accounts_dialog_model_setup (dialog);
	accounts_dialog_setup (dialog);

	gtk_window_set_transient_for (GTK_WINDOW (dialog->window),
				      GTK_WINDOW (gossip_app_get_window ()));

	if (GOSSIP_IS_ACCOUNT (account)) {
		/* If account was specified then we select it */
		accounts_dialog_model_set_selected (dialog, account);
	} else {
		accounts_dialog_model_select_first (dialog);
	}

	gtk_widget_show (dialog->window);
}

gboolean
gossip_accounts_dialog_is_needed (void)
{
	GossipSession          *session;
	GossipAccountManager   *manager;

	if (g_getenv ("GOSSIP_FORCE_SHOW_ACCOUNTS")) {
		return TRUE;
	}

	session = gossip_app_get_session ();
	if (!session) {
		return FALSE;
	}

	manager = gossip_session_get_account_manager (session);
	if (!manager) {
		return FALSE;
	}

	if (gossip_account_manager_get_count (manager) < 1) {
		return TRUE;
	}

	return FALSE;
}
