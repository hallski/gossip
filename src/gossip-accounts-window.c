/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Martyn Russell <mr@gnome.org>
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
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-protocol.h>

#include "gossip-accounts-window.h"
#include "gossip-ui-utils.h"
#include "gossip-app.h"
#include "gossip-new-account-window.h"
#include "gossip-account-info-dialog.h"


typedef struct {
	GtkWidget *window;

	GtkWidget *treeview;
	GtkWidget *button_add;
	GtkWidget *button_edit;
	GtkWidget *button_remove;
	GtkWidget *button_connect;
} GossipAccountsWindow;


typedef struct {
	GossipAccount *account;
	GtkComboBox   *combobox;
} SetAccountData;


static void           accounts_window_setup                     (GossipAccountsWindow  *window);
static void           accounts_window_update_connect_button     (GossipAccountsWindow  *window);
static void           accounts_window_model_setup               (GossipAccountsWindow  *window);
static void           accounts_window_model_add_columns         (GossipAccountsWindow  *window);
static void           accounts_window_model_pixbuf_data_func    (GtkTreeViewColumn     *tree_column,
								 GtkCellRenderer       *cell,
								 GtkTreeModel          *model,
								 GtkTreeIter           *iter,
								 GossipAccountsWindow  *window);
static GossipAccount *accounts_window_model_get_selected        (GossipAccountsWindow  *window);
static void           accounts_window_model_set_selected        (GossipAccountsWindow  *window,
								 GossipAccount         *account);
static gboolean       accounts_window_model_remove_selected     (GossipAccountsWindow  *window);
static void           accounts_window_model_selection_changed   (GtkTreeSelection      *selection,
								 GossipAccountsWindow  *window);
static void           accounts_window_model_default_toggled     (GtkCellRendererToggle *cell,
								 gchar                 *path_string,
								 GossipAccountsWindow  *window);
static void           accounts_window_model_auto_toggled        (GtkCellRendererToggle *cell,
								 gchar                 *path_string,
								 GossipAccountsWindow  *window);
static void           accounts_window_model_cell_edited         (GtkCellRendererText   *cell,
								 const gchar           *path_string,
								 const gchar           *new_text,
								 GossipAccountsWindow  *window);
static void           accounts_window_account_added_cb          (GossipAccountManager  *manager,
								 GossipAccount         *account,
								 GossipAccountsWindow  *window);
static void           accounts_window_account_removed_cb        (GossipAccountManager  *manager,
								 GossipAccount         *account,
								 GossipAccountsWindow  *window);
static void           accounts_window_account_name_changed_cb   (GossipAccount         *account,
								 GParamSpec            *param,
								 GossipAccountsWindow  *window);
static void           accounts_window_protocol_connected_cb     (GossipSession         *session,
								 GossipAccount         *account,
								 GossipProtocol        *protocol,
								 GossipAccountsWindow  *window);
static void           accounts_window_protocol_disconnected_cb  (GossipSession         *session,
								 GossipAccount         *account,
								 GossipProtocol        *protocol,
								 GossipAccountsWindow  *window);
static void           accounts_window_button_add_clicked_cb     (GtkWidget             *button,
								 GossipAccountsWindow  *window);
static void           accounts_window_button_edit_clicked_cb    (GtkWidget             *button,
								 GossipAccountsWindow  *window);
static void           accounts_window_button_remove_clicked_cb  (GtkWidget             *button,
								 GossipAccountsWindow  *window);
static void           accounts_window_button_close_clicked_cb   (GtkWidget             *button,
								 GossipAccountsWindow  *window);
static gboolean       accounts_window_foreach                   (GtkTreeModel          *model,
								 GtkTreePath           *path,
								 GtkTreeIter           *iter,
								 GossipAccountsWindow  *window);

static void           accounts_window_destroy_cb                (GtkWidget             *widget,
								 GossipAccountsWindow  *window);


enum {
	COL_NAME,
	COL_EDITABLE,
	COL_DEFAULT,
	COL_CONNECTED, 
	COL_AUTO_CONNECT,
	COL_ACCOUNT_POINTER,
	COL_COUNT
};


static void
accounts_window_setup (GossipAccountsWindow *window)
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

	view = GTK_TREE_VIEW (window->treeview);
	store = GTK_LIST_STORE (gtk_tree_view_get_model (view));
	selection = gtk_tree_view_get_selection (view);

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);
	accounts = gossip_account_manager_get_accounts (manager); 

	default_account = gossip_account_manager_get_default (manager);

	for (l = accounts; l; l = l->next) {
 		GossipAccount *account; 
		const gchar   *name;
		gboolean       is_default = FALSE;
		gboolean       is_connected = FALSE;

		account = l->data;

		name = gossip_account_get_name (account);
		if (!name) {
			continue;
		}

		is_default = (gossip_account_equal (account, default_account));
		is_connected = 	(gossip_session_is_connected (session, account));

		gtk_list_store_append (store, &iter); 
		gtk_list_store_set (store, &iter, 
				    COL_NAME, name,
				    COL_EDITABLE, is_editable,
				    COL_DEFAULT, is_default,
				    COL_CONNECTED, is_connected,
				    COL_AUTO_CONNECT, gossip_account_get_auto_connect (account), 
				    COL_ACCOUNT_POINTER, g_object_ref (account),
				    -1);
	}

	g_list_free (accounts);
}

static void
accounts_window_update_connect_button (GossipAccountsWindow *window)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	GtkWidget        *image;
	const gchar      *stock_id;
	const gchar      *label;
	gboolean          is_connected;

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return;
	}

	gtk_tree_model_get (model, &iter, COL_CONNECTED, &is_connected, -1);
	
	/* The stock items are not defined correctly in GTK+ so we do it
	 * ourselves (#318939). 
	 */
	if (is_connected) {
		label = _("Disconnect");
		stock_id = "gtk-disconnect";
	} else {
		label = _("Connect");
		stock_id = "gtk-connect";
	}

	image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON);

	gtk_button_set_label (GTK_BUTTON (window->button_connect), label);
	gtk_button_set_image (GTK_BUTTON (window->button_connect), image);
}

static void 
accounts_window_model_setup (GossipAccountsWindow *window)
{
	GtkListStore     *store;
	GtkTreeSelection *selection;

	store = gtk_list_store_new (COL_COUNT,
				    G_TYPE_STRING,   /* name */
				    G_TYPE_BOOLEAN,  /* editable */
				    G_TYPE_BOOLEAN,  /* default */
				    G_TYPE_BOOLEAN,  /* connected */
				    G_TYPE_BOOLEAN,  /* auto start */
				    G_TYPE_POINTER); /* account */ 
	
	gtk_tree_view_set_model (GTK_TREE_VIEW (window->treeview), 
				 GTK_TREE_MODEL (store));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect (selection, "changed", 
			  G_CALLBACK (accounts_window_model_selection_changed), window);


	accounts_window_model_add_columns (window);

	g_object_unref (store);
}

static void 
accounts_window_model_add_columns (GossipAccountsWindow *window)
{
	GtkTreeView       *view;
	GtkTreeViewColumn *column; 
	GtkCellRenderer   *cell;
	
	view = GTK_TREE_VIEW (window->treeview);
	gtk_tree_view_set_headers_visible (view, TRUE);

	/* Account name/status */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Account"));

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, cell, 
						 (GtkTreeCellDataFunc) 
						 accounts_window_model_pixbuf_data_func,
						 window, 
						 NULL);

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_add_attribute (column,
					    cell,
					    "text", COL_NAME);
	g_signal_connect (cell, "edited",
			  G_CALLBACK (accounts_window_model_cell_edited), 
			  window);

 	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
	gtk_tree_view_column_set_expand (column, TRUE);

	gtk_tree_view_column_set_sort_column_id (column, COL_NAME);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	gtk_tree_view_append_column (GTK_TREE_VIEW (window->treeview), column);

	/* Default account. */
	cell = gtk_cell_renderer_toggle_new ();
	g_object_set (cell, 
		      "radio", TRUE, 
		      "xalign", 0.0,
		      NULL);

	column = gtk_tree_view_column_new_with_attributes (_("Default"), cell, 
							   "active", COL_DEFAULT, 
							   NULL);
	
	g_signal_connect (cell, 
			  "toggled",
			  G_CALLBACK (accounts_window_model_default_toggled), 
			  window);

	gtk_tree_view_column_set_sort_column_id (column, COL_DEFAULT);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	gtk_tree_view_append_column (GTK_TREE_VIEW (window->treeview), column);

	/* Auto connect */
	cell = gtk_cell_renderer_toggle_new ();
	g_object_set (cell, 
		      "xalign", 0.0,
		      NULL);
	g_signal_connect (cell, "toggled", 
			  G_CALLBACK (accounts_window_model_auto_toggled), 
			  window);
	
	column = gtk_tree_view_column_new_with_attributes (_("Auto Connect"), cell,
							   "active", COL_AUTO_CONNECT,
							   NULL);
	
 	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE); 
	gtk_tree_view_column_set_expand (column, FALSE);

	gtk_tree_view_column_set_sort_column_id (column, COL_AUTO_CONNECT);
	gtk_tree_view_column_set_resizable (column, FALSE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	gtk_tree_view_append_column (view, column);
}

static void  
accounts_window_model_pixbuf_data_func (GtkTreeViewColumn    *tree_column,
					GtkCellRenderer      *cell,
					GtkTreeModel         *model,
					GtkTreeIter          *iter,
					GossipAccountsWindow *window)
{
	GossipAccount *account;
	GError        *error = NULL;
	GtkIconTheme  *theme;
	GdkPixbuf     *pixbuf;
	gint           w, h;
	gint           size;
	const gchar   *icon_id;
	gboolean       is_connected;

	gtk_tree_model_get (model, iter, 
			    COL_CONNECTED, &is_connected,
			    COL_ACCOUNT_POINTER, &account, 
			    -1);

	/* get theme and size details */
	theme = gtk_icon_theme_get_default ();

	if (!gtk_icon_size_lookup (GTK_ICON_SIZE_BUTTON, &w, &h)) {
		size = 48;
	} else {
		size = (w + h) / 2; 
	}

	switch (gossip_account_get_type (account)) {
	case GOSSIP_ACCOUNT_TYPE_JABBER:
		icon_id = "im-jabber";
		break;
	case GOSSIP_ACCOUNT_TYPE_AIM:
		icon_id = "im-aim";
		break;
	case GOSSIP_ACCOUNT_TYPE_ICQ:
		icon_id = "im-icq";
		break;
	case GOSSIP_ACCOUNT_TYPE_MSN:
		icon_id = "im-msn";
		break;
	case GOSSIP_ACCOUNT_TYPE_YAHOO:
		icon_id = "im-yahoo";
		break;
	default:
		icon_id = NULL;
		g_assert_not_reached ();
	}

	pixbuf = gtk_icon_theme_load_icon (theme,
					   icon_id,     /* icon name */
					   size,        /* size */
					   0,           /* flags */
					   &error);

	g_return_if_fail (pixbuf != NULL);

	if (!is_connected) {
		GdkPixbuf *modded_pixbuf;

		modded_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
						TRUE,
						8,
						size, size);

		gdk_pixbuf_saturate_and_pixelate (pixbuf,
						  modded_pixbuf,
						  1.0,
						  TRUE);
		g_object_unref (pixbuf);
		pixbuf = modded_pixbuf;
	}

	g_object_set (cell, 
		      "visible", TRUE,
		      "pixbuf", pixbuf,
		      NULL); 

	g_object_unref (pixbuf);
}

static GossipAccount *
accounts_window_model_get_selected (GossipAccountsWindow *window)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	GossipAccount    *account;

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter, COL_ACCOUNT_POINTER, &account, -1);
	return account;
}

static void
accounts_window_model_set_selected (GossipAccountsWindow *window,
				    GossipAccount        *account) 
{
	GtkTreeView      *view;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);

	/* Nothing in list. */
	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		return;
	}

	while (gtk_tree_model_iter_next (model, &iter)) {
		GossipAccount *this_account;

		gtk_tree_model_get (model, &iter, 
				    COL_ACCOUNT_POINTER, &this_account, 
				    -1);

		if (gossip_account_equal (this_account, account)) {
			gtk_tree_selection_select_iter (selection, &iter);
			return;
		}
	}
}

static gboolean 
accounts_window_model_remove_selected (GossipAccountsWindow *window)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return FALSE;
	}

	return gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static void
accounts_window_model_selection_changed (GtkTreeSelection    *selection,
					 GossipAccountsWindow *window)
{
	GtkTreeModel  *model;
	GtkTreeIter    iter;
	gboolean       is_selection;

	is_selection = gtk_tree_selection_get_selected (selection, &model, &iter);

	gtk_widget_set_sensitive (window->button_edit, is_selection);
	gtk_widget_set_sensitive (window->button_remove, is_selection);
	gtk_widget_set_sensitive (window->button_connect, is_selection);

	accounts_window_update_connect_button (window);
}

static void 
accounts_window_model_default_toggled (GtkCellRendererToggle *cell, 
				       gchar                 *path_string, 
				       GossipAccountsWindow  *window)
{
	GossipSession        *session;
	GossipAccountManager *manager;
	GossipAccount        *account;
	GtkTreeView          *view;
	GtkTreeModel         *model;
	GtkListStore         *store;
	GtkTreePath          *path;
	GtkTreeIter           iter;

	view = GTK_TREE_VIEW (window->treeview);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);

	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gtk_list_store_set (store, &iter, 
					    COL_DEFAULT, FALSE,
					    -1);
		} while (gtk_tree_model_iter_next (model, &iter));
	}
	
	path = gtk_tree_path_new_from_string (path_string);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	gtk_tree_model_get (model, &iter, 
			    COL_ACCOUNT_POINTER, &account,
			    -1);

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	gossip_account_manager_set_default (manager, account);
	gossip_account_manager_store (manager);

	gtk_list_store_set (store, &iter, 
			    COL_DEFAULT, TRUE,
			    -1);
}

static void 
accounts_window_model_auto_toggled (GtkCellRendererToggle *cell, 
				    gchar                 *path_string, 
				    GossipAccountsWindow  *window)
{
	GossipSession        *session;
	GossipAccountManager *manager;
	GossipAccount        *account;
	gboolean              enabled;
	GtkTreeView          *view;
	GtkTreeModel         *model;
	GtkListStore         *store;
	GtkTreePath          *path;
	GtkTreeIter           iter;

	view = GTK_TREE_VIEW (window->treeview);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);
	
	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, 
			    COL_AUTO_CONNECT, &enabled, 
			    COL_ACCOUNT_POINTER, &account,
			    -1);

	enabled = !enabled;

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	gossip_account_set_auto_connect (account, enabled);
	gossip_account_manager_store (manager);

	gtk_list_store_set (store, &iter, COL_AUTO_CONNECT, enabled, -1);
	gtk_tree_path_free (path);
}

static void
accounts_window_model_cell_edited (GtkCellRendererText *cell,
				  const gchar         *path_string,
				  const gchar         *new_text,
				  GossipAccountsWindow *window)
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

	view = GTK_TREE_VIEW (window->treeview);
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
accounts_window_account_added_cb (GossipAccountManager *manager,
				  GossipAccount        *account,
				  GossipAccountsWindow *window)
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

	view = GTK_TREE_VIEW (window->treeview);
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
			    COL_ACCOUNT_POINTER, g_object_ref (account),
			    -1);
}

static void
accounts_window_account_removed_cb (GossipAccountManager *manager,
				    GossipAccount        *account,
				    GossipAccountsWindow *window)
{
	accounts_window_model_set_selected (window, account);
	accounts_window_model_remove_selected (window);
}

static void
accounts_window_account_name_changed_cb (GossipAccount        *account,
					 GParamSpec           *param,
					 GossipAccountsWindow *window)
{
#if 0
	GtkTreeView      *view;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	gboolean          ok;

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);

	for (ok = gtk_tree_model_get_iter_first (model, &iter);
	     ok;
	     ok = gtk_tree_model_iter_next (model, &iter)) {
		GossipAccount *this_account;
		
		gtk_tree_model_get (model, &iter, 
				    COL_ACCOUNT_POINTER, &this_account, 
				    -1);
		
		if (gossip_account_equal (this_account, account)) {
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    COL_NAME, gossip_account_get_name (account),
					    -1);
			return;
		}
	}
#endif	
}

static void
accounts_window_protocol_connected_cb (GossipSession        *session,
				       GossipAccount        *account,
				       GossipProtocol       *protocol,
				       GossipAccountsWindow *window)
{
	GtkTreeView      *view;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	gboolean          ok;

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);

	for (ok = gtk_tree_model_get_iter_first (model, &iter);
	     ok;
	     ok = gtk_tree_model_iter_next (model, &iter)) {
		GossipAccount *this_account;
		
		gtk_tree_model_get (model, &iter, 
				    COL_ACCOUNT_POINTER, &this_account, 
				    -1);
		
		if (gossip_account_equal (this_account, account)) {
			GtkTreePath *path;

			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    COL_CONNECTED, TRUE,
					    -1);

			path = gtk_tree_model_get_path (model, &iter);
			gtk_tree_model_row_changed (model, path, &iter);
			gtk_tree_path_free (path);

			break;
		}
	}

	accounts_window_update_connect_button (window);
}

static void
accounts_window_protocol_disconnected_cb (GossipSession        *session,
					  GossipAccount        *account,
					  GossipProtocol       *protocol,
					  GossipAccountsWindow *window)
{
	GtkTreeView      *view;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	gboolean          ok;

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);

	for (ok = gtk_tree_model_get_iter_first (model, &iter);
	     ok;
	     ok = gtk_tree_model_iter_next (model, &iter)) {
		GossipAccount *this_account;
		
		gtk_tree_model_get (model, &iter, 
				    COL_ACCOUNT_POINTER, &this_account, 
				    -1);
		
		if (gossip_account_equal (this_account, account)) {
			GtkTreePath *path;

			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    COL_CONNECTED, FALSE,
					    -1);

			path = gtk_tree_model_get_path (model, &iter);
			gtk_tree_model_row_changed (model, path, &iter);
			gtk_tree_path_free (path);

			break;
		}
	}

	accounts_window_update_connect_button (window);
}

static void
accounts_window_button_add_clicked_cb (GtkWidget            *button,
				       GossipAccountsWindow *window)
{
	gossip_new_account_window_show ();
}

static void
accounts_window_button_edit_clicked_cb (GtkWidget            *button,
					GossipAccountsWindow *window)
{
	GossipAccount *account;

	account = accounts_window_model_get_selected (window);
	gossip_account_info_dialog_show (account);
}

static void
accounts_window_button_remove_clicked_cb (GtkWidget            *button,
					  GossipAccountsWindow *window)
{
	GossipSession        *session;
	GossipAccountManager *manager;
	GossipAccount        *account;

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	account = accounts_window_model_get_selected (window);
	gossip_account_manager_remove (manager, account);
	gossip_account_manager_store (manager);
}

static void
accounts_window_button_connect_clicked_cb (GtkWidget            *button,
					   GossipAccountsWindow *window)
{
	GossipSession        *session;
	GossipAccountManager *manager;
	GossipAccount        *account;

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	account = accounts_window_model_get_selected (window);

	if (gossip_session_is_connected (session, account)) {
		gossip_session_disconnect (session, account);
	} else {
		gossip_session_connect (session, account, FALSE);
	}
}

static void
accounts_window_button_close_clicked_cb (GtkWidget            *button,
					 GossipAccountsWindow *window)
{
	gtk_widget_destroy (window->window);
}

static gboolean
accounts_window_foreach (GtkTreeModel         *model,
			 GtkTreePath          *path,
			 GtkTreeIter          *iter,
			 GossipAccountsWindow *window)
{
	GossipAccount *account;

	gtk_tree_model_get (model, iter, COL_ACCOUNT_POINTER, &account, -1);
	g_object_unref (account);

	g_signal_handlers_disconnect_by_func (account,
					      accounts_window_account_name_changed_cb, 
					      window);

	return FALSE;
}

static void
accounts_window_destroy_cb (GtkWidget            *widget,
			    GossipAccountsWindow *window)
{
	GossipSession        *session;
	GossipAccountManager *manager;
	GtkTreeModel         *model;

	session = gossip_app_get_session ();
	manager = gossip_session_get_account_manager (session);

	g_signal_handlers_disconnect_by_func (manager,
					      accounts_window_account_added_cb, 
					      window);

	g_signal_handlers_disconnect_by_func (manager,
					      accounts_window_account_removed_cb, 
					      window);

	g_signal_handlers_disconnect_by_func (session,
					      accounts_window_protocol_connected_cb, 
					      window);

	g_signal_handlers_disconnect_by_func (session,
					      accounts_window_protocol_disconnected_cb, 
					      window);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (window->treeview));
	gtk_tree_model_foreach (model, 
				(GtkTreeModelForeachFunc) accounts_window_foreach, 
				window);

	g_free (window);
}

void
gossip_accounts_window_show (void)
{
	GossipSession               *session;
	GossipAccountManager        *manager;
	static GossipAccountsWindow *window = NULL;
	GladeXML                    *glade;
	GtkWidget                   *bbox, *button_close;

	if (window) {
		gtk_window_present (GTK_WINDOW (window->window));
		return;
	}
	
	window = g_new0 (GossipAccountsWindow, 1);

	glade = gossip_glade_get_file (GLADEDIR "/connect.glade",
				       "accounts_window",
				       NULL,
				       "accounts_window", &window->window,
				       "treeview", &window->treeview,
				       "bbox", &bbox,
				       "button_add", &window->button_add,
				       "button_edit", &window->button_edit,
				       "button_remove", &window->button_remove,
				       "button_connect", &window->button_connect,
				       "button_close", &button_close,
				       NULL);

	gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (bbox), 
					    button_close, TRUE);

	gossip_glade_connect (glade, 
			      window,
			      "accounts_window", "destroy", accounts_window_destroy_cb,
			      "button_add", "clicked", accounts_window_button_add_clicked_cb,
			      "button_edit", "clicked", accounts_window_button_edit_clicked_cb,
			      "button_remove", "clicked", accounts_window_button_remove_clicked_cb,
			      "button_connect", "clicked", accounts_window_button_connect_clicked_cb,
			      "button_close", "clicked", accounts_window_button_close_clicked_cb,
			      NULL);

	g_object_add_weak_pointer (G_OBJECT (window->window), (gpointer) &window);
	
	g_object_unref (glade);

	session = gossip_app_get_session ();
	manager = gossip_session_get_account_manager (session);

	g_signal_connect (manager, "account_added",
			  G_CALLBACK (accounts_window_account_added_cb), 
			  window);

	g_signal_connect (manager, "account_removed",
			  G_CALLBACK (accounts_window_account_removed_cb), 
			  window);

	g_signal_connect (session, "protocol-connected",
			  G_CALLBACK (accounts_window_protocol_connected_cb), 
			  window);

	g_signal_connect (session, "protocol-disconnected",
			  G_CALLBACK (accounts_window_protocol_disconnected_cb), 
			  window);

	accounts_window_model_setup (window);
	accounts_window_setup (window);
	
	gtk_window_set_transient_for (GTK_WINDOW (window->window), 
				      GTK_WINDOW (gossip_app_get_window ()));
	gtk_widget_show (window->window);
}

