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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <loudmouth/loudmouth.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-protocol.h>
#include <libgossip/gossip-utils.h>

#include "gossip-accounts-dialog.h"
#include "gossip-ui-utils.h"
#include "gossip-app.h"
#include "gossip-new-account-window.h"

#define STRING_EMPTY(x) ((x) == NULL || (x)[0] == '\0')


typedef struct {
	GtkWidget *window;

	GtkWidget *treeview;
	GtkWidget *notebook;

	GtkWidget *button_remove;
	GtkWidget *button_connect;

	GtkWidget *entry_name;
	GtkWidget *entry_id;
	GtkWidget *entry_server;
	GtkWidget *entry_resource;
	GtkWidget *entry_password;
	GtkWidget *entry_port;

	GtkWidget *checkbutton_ssl;
	GtkWidget *checkbutton_proxy;
	GtkWidget *checkbutton_connect;

	gboolean   account_changed;

} GossipAccountsDialog;


typedef struct {
	GossipAccount *account;
	GtkComboBox   *combobox;
} SetAccountData;


static void           accounts_dialog_setup                     (GossipAccountsDialog  *dialog);
static void           accounts_dialog_update_connect_button     (GossipAccountsDialog  *dialog);
static void           accounts_dialog_update_account            (GossipAccountsDialog  *dialog,
								 GossipAccount         *account);
static void           accounts_dialog_block_widgets             (GossipAccountsDialog  *dialog,
								 gboolean               block);
static void           accounts_dialog_save                      (GossipAccountsDialog  *dialog,
								 GossipAccount         *account);
static void           accounts_dialog_model_setup               (GossipAccountsDialog  *dialog);
static void           accounts_dialog_model_add_columns         (GossipAccountsDialog  *dialog);
static void           accounts_dialog_model_cell_toggled        (GtkCellRendererToggle *cell,
								 gchar                 *path_string,
								 GossipAccountsDialog  *dialog);
static void           accounts_dialog_model_pixbuf_data_func    (GtkTreeViewColumn     *tree_column,
								 GtkCellRenderer       *cell,
								 GtkTreeModel          *model,
								 GtkTreeIter           *iter,
								 GossipAccountsDialog  *dialog);
static GossipAccount *accounts_dialog_model_get_selected        (GossipAccountsDialog  *dialog);
static void           accounts_dialog_model_set_selected        (GossipAccountsDialog  *dialog,
								 GossipAccount         *account);
static void           accounts_dialog_model_select_first        (GossipAccountsDialog  *dialog);

static gboolean       accounts_dialog_model_remove_selected     (GossipAccountsDialog  *dialog);
static void           accounts_dialog_model_selection_changed   (GtkTreeSelection      *selection,
								 GossipAccountsDialog  *dialog);
static void           accounts_dialog_model_cell_edited         (GtkCellRendererText   *cell,
								 const gchar           *path_string,
								 const gchar           *new_text,
								 GossipAccountsDialog  *dialog);
static void           accounts_dialog_account_added_cb          (GossipAccountManager  *manager,
								 GossipAccount         *account,
								 GossipAccountsDialog  *dialog);
static void           accounts_dialog_account_removed_cb        (GossipAccountManager  *manager,
								 GossipAccount         *account,
								 GossipAccountsDialog  *dialog);
static void           accounts_dialog_account_name_changed_cb   (GossipAccount         *account,
								 GParamSpec            *param,
								 GossipAccountsDialog  *dialog);
static void           accounts_dialog_protocol_connected_cb     (GossipSession         *session,
								 GossipAccount         *account,
								 GossipProtocol        *protocol,
								 GossipAccountsDialog  *dialog);
static void           accounts_dialog_protocol_disconnected_cb  (GossipSession         *session,
								 GossipAccount         *account,
								 GossipProtocol        *protocol,
								 GossipAccountsDialog  *dialog);
static void           accounts_dialog_entry_changed_cb          (GtkWidget             *widget,
								 GossipAccountsDialog  *dialog);
static gboolean       accounts_dialog_entry_focus_cb            (GtkWidget             *widget,
								 GdkEventFocus         *event,
								 GossipAccountsDialog  *dialog);
static void           accounts_dialog_checkbutton_toggled_cb    (GtkWidget             *widget,
								 GossipAccountsDialog  *dialog);
static void           accounts_dialog_entry_port_insert_text_cb (GtkEditable           *editable,
								 gchar                 *new_text,
								 gint                   len,
								 gint                  *position,
								 GossipAccountsDialog  *dialog);
static void           accounts_dialog_button_add_clicked_cb     (GtkWidget             *button,
								 GossipAccountsDialog  *dialog);
static void           accounts_dialog_button_remove_clicked_cb  (GtkWidget             *button,
								 GossipAccountsDialog  *dialog);
static void           accounts_dialog_button_close_clicked_cb   (GtkWidget             *button,
								 GossipAccountsDialog  *dialog);
static gboolean       accounts_dialog_foreach                   (GtkTreeModel          *model,
								 GtkTreePath           *path,
								 GtkTreeIter           *iter,
								 GossipAccountsDialog  *dialog);
static void           accounts_dialog_destroy_cb                (GtkWidget             *widget,
								 GossipAccountsDialog  *dialog);


enum {
	COL_NAME,
	COL_EDITABLE,
	COL_ENABLED,
	COL_DEFAULT,
	COL_CONNECTED, 
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
				    COL_ENABLED, gossip_account_get_enabled (account),
				    COL_AUTO_CONNECT, gossip_account_get_auto_connect (account), 
				    COL_ACCOUNT_POINTER, g_object_ref (account),
				    -1);

		g_signal_connect (account, "notify::name", 
				  G_CALLBACK (accounts_dialog_account_name_changed_cb), dialog);
	}

	g_list_free (accounts);
}

static void
accounts_dialog_update_connect_button (GossipAccountsDialog *dialog)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	GtkWidget        *image;
	const gchar      *stock_id;
	const gchar      *label;
	gboolean          is_connected;

	view = GTK_TREE_VIEW (dialog->treeview);
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

	gtk_button_set_label (GTK_BUTTON (dialog->button_connect), label);
	gtk_button_set_image (GTK_BUTTON (dialog->button_connect), image);
}

static void
accounts_dialog_update_account (GossipAccountsDialog *dialog,
				GossipAccount        *account)
{
	gchar         *port_str; 
	const gchar   *id;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	/* block signals first */
	accounts_dialog_block_widgets (dialog, TRUE);

	/* set account details */
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_name), 
			    gossip_account_get_name (account));

	if (lm_ssl_is_supported ()) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->checkbutton_ssl),
					      gossip_account_get_use_ssl (account));
	} else {
		gtk_widget_set_sensitive (dialog->checkbutton_ssl, FALSE);
	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->checkbutton_proxy),
				      gossip_account_get_use_proxy (account));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->checkbutton_connect),
				      gossip_account_get_auto_connect (account));


	id = gossip_account_get_id (account);
	if (!STRING_EMPTY (id)) {
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

	if (gossip_account_get_password (account)) {
		gtk_entry_set_text (GTK_ENTRY (dialog->entry_password), 
				    gossip_account_get_password (account));
	}

	gtk_entry_set_text (GTK_ENTRY (dialog->entry_server), 
			    gossip_account_get_server (account));

	port_str = g_strdup_printf ("%d", gossip_account_get_port (account));
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_port), 
			    port_str);
	g_free (port_str);

	/* unblock signals */
	accounts_dialog_block_widgets (dialog, FALSE);
}

static void
accounts_dialog_block_widgets (GossipAccountsDialog *dialog,
			       gboolean              block)
{
	/* FIXME: need to do this better */
	if (block) {
		g_signal_handlers_block_by_func (dialog->entry_name,
						 accounts_dialog_entry_changed_cb, 
						 dialog);
		g_signal_handlers_block_by_func (dialog->entry_id,
						 accounts_dialog_entry_changed_cb, 
						 dialog);
		g_signal_handlers_block_by_func (dialog->entry_password,
						 accounts_dialog_entry_changed_cb, 
						 dialog);
		g_signal_handlers_block_by_func (dialog->entry_server,
						 accounts_dialog_entry_changed_cb, 
						 dialog);
		g_signal_handlers_block_by_func (dialog->entry_port,
						 accounts_dialog_entry_changed_cb, 
						 dialog);

		g_signal_handlers_block_by_func (dialog->checkbutton_proxy,
						 accounts_dialog_checkbutton_toggled_cb, 
						 dialog);
		g_signal_handlers_block_by_func (dialog->checkbutton_ssl,
						 accounts_dialog_checkbutton_toggled_cb, 
						 dialog);
		g_signal_handlers_block_by_func (dialog->checkbutton_connect,
						 accounts_dialog_checkbutton_toggled_cb, 
						 dialog);
	} else {
		g_signal_handlers_unblock_by_func (dialog->entry_name,
						   accounts_dialog_entry_changed_cb, 
						   dialog);
		g_signal_handlers_unblock_by_func (dialog->entry_id,
						   accounts_dialog_entry_changed_cb, 
						   dialog);
		g_signal_handlers_unblock_by_func (dialog->entry_password,
						   accounts_dialog_entry_changed_cb, 
						   dialog);
		g_signal_handlers_unblock_by_func (dialog->entry_server,
						   accounts_dialog_entry_changed_cb, 
						   dialog);
		g_signal_handlers_unblock_by_func (dialog->entry_port,
						   accounts_dialog_entry_changed_cb, 
						   dialog);

		g_signal_handlers_unblock_by_func (dialog->checkbutton_proxy,
						   accounts_dialog_checkbutton_toggled_cb, 
						   dialog);
		g_signal_handlers_unblock_by_func (dialog->checkbutton_ssl,
						   accounts_dialog_checkbutton_toggled_cb, 
						   dialog);
		g_signal_handlers_unblock_by_func (dialog->checkbutton_connect,
						   accounts_dialog_checkbutton_toggled_cb, 
						   dialog);
	}
}

static void
accounts_dialog_save (GossipAccountsDialog *dialog,
		      GossipAccount        *account) 
{
	GossipSession        *session;
	GossipAccountManager *manager;
 	const gchar          *str;
	gchar                *id_to_save;
	const gchar          *resource;
	gint                  pnr;
	gboolean              bool;

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	/* set name */
	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_name));
 	gossip_account_set_name (account, str); 

	/* set id */
	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_id));
	resource = gtk_entry_get_text (GTK_ENTRY (dialog->entry_resource));
	if (STRING_EMPTY (resource)) {
		resource = _("Home");
	}
	
	id_to_save = g_strdup_printf ("%s/%s", str, resource);
	gossip_account_set_id (account, id_to_save);
	g_free (id_to_save);

	/* set password */
	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_password));
	gossip_account_set_password (account, str);

	/* set server */
	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_server));
	gossip_account_set_server (account, str);

	/* set port */
	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_port));
	pnr = strtol (str, NULL, 10);
	if (pnr > 0 && pnr < 65556) {
		gossip_account_set_port (account, pnr);
	}

	/* set auto connect, proxy, ssl */
	bool = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->checkbutton_connect));
	gossip_account_set_auto_connect (account, bool);

	bool = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->checkbutton_proxy));
	gossip_account_set_use_proxy (account, bool);

	bool = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->checkbutton_ssl));
	gossip_account_set_use_ssl (account, bool);
	
	/* save */
	gossip_account_manager_store (manager);

	/* reset flags */
	dialog->account_changed = FALSE;
}

static void 
accounts_dialog_model_setup (GossipAccountsDialog *dialog)
{
	GtkListStore     *store;
	GtkTreeSelection *selection;

	store = gtk_list_store_new (COL_COUNT,
				    G_TYPE_STRING,   /* name */
				    G_TYPE_BOOLEAN,  /* editable */
				    G_TYPE_BOOLEAN,  /* enabled */
				    G_TYPE_BOOLEAN,  /* default */
				    G_TYPE_BOOLEAN,  /* connected */
				    G_TYPE_BOOLEAN,  /* auto start */
				    G_TYPE_POINTER); /* account */ 
	
	gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->treeview), 
				 GTK_TREE_MODEL (store));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect (selection, "changed", 
			  G_CALLBACK (accounts_dialog_model_selection_changed), dialog);

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
	gtk_tree_view_set_headers_visible (view, TRUE);

	/* account enabled */
	cell = gtk_cell_renderer_toggle_new ();
 	column = gtk_tree_view_column_new_with_attributes (NULL, cell,  
 							   "active", COL_ENABLED,  
 							   NULL); 
	gtk_tree_view_append_column (view, column);

 	g_signal_connect (cell, "toggled",  
 			  G_CALLBACK (accounts_dialog_model_cell_toggled),  
 			  dialog); 

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
	
	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		gtk_widget_set_sensitive (dialog->notebook, FALSE);
	} else {
		gtk_widget_set_sensitive (dialog->notebook, TRUE);
		
		selection = gtk_tree_view_get_selection (view);
		gtk_tree_selection_select_iter (selection, &iter);
	}
}

static void 
accounts_dialog_model_cell_toggled (GtkCellRendererToggle *cell, 
				    gchar                 *path_string, 
				    GossipAccountsDialog  *dialog)
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

	view = GTK_TREE_VIEW (dialog->treeview);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);
	
	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, 
			    COL_ENABLED, &enabled, 
			    COL_ACCOUNT_POINTER, &account,
			    -1);

	enabled = !enabled;

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	gossip_account_set_enabled (account, enabled);

	dialog->account_changed = TRUE;
 	gossip_account_manager_store (manager); 

	gtk_list_store_set (store, &iter, COL_ENABLED, enabled, -1);
	gtk_tree_path_free (path);
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

	gtk_tree_model_get (model, iter, 
			    COL_CONNECTED, &is_connected,
			    COL_ACCOUNT_POINTER, &account, 
			    -1);

	pixbuf = gossip_ui_utils_get_pixbuf_from_account (account, GTK_ICON_SIZE_BUTTON);
	g_return_if_fail (pixbuf != NULL);

	if (!is_connected) {
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

	g_object_set (cell, 
		      "visible", TRUE,
		      "pixbuf", pixbuf,
		      NULL); 

	g_object_unref (pixbuf);
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
	GtkTreeModel         *model;
	GtkTreeIter           iter;
	gboolean              is_selection;

	static GossipAccount *account = NULL;

	if (account && dialog->account_changed) {
 		accounts_dialog_save (dialog, account); 
	}

	is_selection = gtk_tree_selection_get_selected (selection, &model, &iter);

	gtk_widget_set_sensitive (dialog->notebook, is_selection);
	gtk_widget_set_sensitive (dialog->button_remove, is_selection);
	gtk_widget_set_sensitive (dialog->button_connect, is_selection);

	accounts_dialog_update_connect_button (dialog);

	if (is_selection) {
		account = accounts_dialog_model_get_selected (dialog);
		accounts_dialog_update_account (dialog, account);
	}
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
			    COL_ENABLED, gossip_account_get_enabled (account),
			    COL_AUTO_CONNECT, gossip_account_get_auto_connect (account), 
			    COL_ACCOUNT_POINTER, g_object_ref (account),
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
}

static gboolean
accounts_dialog_entry_focus_cb (GtkWidget            *widget,
				GdkEventFocus        *event,
				GossipAccountsDialog *dialog)
{
	if (widget == dialog->entry_id) {
		const gchar *server;
		
		server = gtk_entry_get_text (GTK_ENTRY (dialog->entry_server));

		if (STRING_EMPTY (server)) {
			const gchar *str;
			gchar       *host;

			str = gtk_entry_get_text (GTK_ENTRY (widget));
			host = gossip_utils_jid_str_get_part_host (str);
			gtk_entry_set_text (GTK_ENTRY (dialog->entry_server), host);

			g_free (host);
		}
	}

	if (dialog->account_changed) {
		GossipAccount *account;

		account = accounts_dialog_model_get_selected (dialog);
 		accounts_dialog_save (dialog, account); 
	}

	return FALSE;
}

static void
accounts_dialog_entry_changed_cb (GtkWidget            *widget,
				  GossipAccountsDialog *dialog)
{
	if (widget == dialog->entry_port) {
		const gchar *str;
		gint         pnr;

		str = gtk_entry_get_text (GTK_ENTRY (widget));
		pnr = strtol (str, NULL, 10);
		
		if (pnr <= 0 || pnr >= 65556) {
			GossipAccount *account;
			gchar         *port_str;
			
			account = accounts_dialog_model_get_selected (dialog);
			port_str = g_strdup_printf ("%d", gossip_account_get_port (account));
			g_signal_handlers_block_by_func (dialog->entry_port, 
							 accounts_dialog_entry_changed_cb, 
							 dialog);
			gtk_entry_set_text (GTK_ENTRY (widget), port_str);
			g_signal_handlers_unblock_by_func (dialog->entry_port, 
							   accounts_dialog_entry_changed_cb, 
							   dialog);
			g_free (port_str);

			return;
		}
	}

	/* save */
	dialog->account_changed = TRUE;
}

static void  
accounts_dialog_checkbutton_toggled_cb (GtkWidget            *widget,
					GossipAccountsDialog *dialog)
{
	GossipAccount *account;
	gboolean       active;
	gboolean       changed = FALSE;

	account = accounts_dialog_model_get_selected (dialog);

	if (widget == dialog->checkbutton_ssl) {
		active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
		
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
	}

	/* save */
 	accounts_dialog_save (dialog, account); 
}

static void  
accounts_dialog_entry_port_insert_text_cb (GtkEditable          *editable,
					   gchar                *new_text,
					   gint                  len,
					   gint                 *position,
					   GossipAccountsDialog *dialog)
{
	gint i;
	
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
accounts_dialog_protocol_connected_cb (GossipSession        *session,
				       GossipAccount        *account,
				       GossipProtocol       *protocol,
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

	accounts_dialog_update_connect_button (dialog);
}

static void
accounts_dialog_protocol_disconnected_cb (GossipSession        *session,
					  GossipAccount        *account,
					  GossipProtocol       *protocol,
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

	accounts_dialog_update_connect_button (dialog);
}

static void
accounts_dialog_button_add_clicked_cb (GtkWidget            *button,
				       GossipAccountsDialog *dialog)
{
	gossip_new_account_window_show ();
}

static void
accounts_dialog_button_remove_clicked_cb (GtkWidget            *button,
					  GossipAccountsDialog *window)
{
	GossipSession        *session;
	GossipAccountManager *manager;
	GossipAccount        *account;

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	account = accounts_dialog_model_get_selected (window);
	gossip_account_manager_remove (manager, account);
	gossip_account_manager_store (manager);
}

static void
accounts_dialog_button_connect_clicked_cb (GtkWidget            *button,
					   GossipAccountsDialog *window)
{
	GossipSession        *session;
	GossipAccountManager *manager;
	GossipAccount        *account;

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	account = accounts_dialog_model_get_selected (window);

	if (gossip_session_is_connected (session, account)) {
		gossip_session_disconnect (session, account);
	} else {
		gossip_session_connect (session, account, FALSE);
	}
}

static void
accounts_dialog_button_close_clicked_cb (GtkWidget            *button,
					 GossipAccountsDialog *dialog)
{
	gtk_widget_destroy (dialog->window);
}

static gboolean
accounts_dialog_foreach (GtkTreeModel         *model,
			 GtkTreePath          *path,
			 GtkTreeIter          *iter,
			 GossipAccountsDialog *dialog)
{
	GossipAccount *account;

	gtk_tree_model_get (model, iter, COL_ACCOUNT_POINTER, &account, -1);
	g_object_unref (account);

	g_signal_handlers_disconnect_by_func (account,
					      accounts_dialog_account_name_changed_cb, 
					      dialog);

	return FALSE;
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
					      accounts_dialog_protocol_connected_cb, 
					      dialog);

	g_signal_handlers_disconnect_by_func (session,
					      accounts_dialog_protocol_disconnected_cb, 
					      dialog);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->treeview));
	gtk_tree_model_foreach (model, 
				(GtkTreeModelForeachFunc) accounts_dialog_foreach, 
				dialog);

	if (dialog->account_changed) {
		GossipAccount *account;

		account = accounts_dialog_model_get_selected (dialog);
 		accounts_dialog_save (dialog, account); 
	}

	g_free (dialog);
}

void
gossip_accounts_dialog_show (GossipAccount *optional_account)
{
	GossipSession               *session;
	GossipAccountManager        *manager;
	static GossipAccountsDialog *dialog = NULL;
	GladeXML                    *glade;
	GtkSizeGroup                *size_group;
	GtkWidget                   *label_name, *label_id, *label_password;
	GtkWidget                   *label_server, *label_port, *label_resource; 
	GtkWidget                   *bbox, *button_close;

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->window));
		return;
	}
	
	dialog = g_new0 (GossipAccountsDialog, 1);

	glade = gossip_glade_get_file (GLADEDIR "/connect.glade",
				       "accounts_dialog",
				       NULL,
				       "accounts_dialog", &dialog->window,
				       "dialog-action_area", &bbox,
				       "treeview", &dialog->treeview,
				       "notebook", &dialog->notebook,
				       "label_name", &label_name,
				       "label_id", &label_id,
				       "label_password", &label_password,
				       "label_server", &label_server,
				       "label_port", &label_port,
				       "label_resource", &label_resource,
				       "entry_name", &dialog->entry_name,
				       "entry_id", &dialog->entry_id,
				       "entry_resource", &dialog->entry_resource,
				       "entry_server", &dialog->entry_server,
				       "entry_password", &dialog->entry_password,
				       "entry_port", &dialog->entry_port,
				       "checkbutton_ssl", &dialog->checkbutton_ssl,
				       "checkbutton_proxy", &dialog->checkbutton_proxy,
				       "checkbutton_connect", &dialog->checkbutton_connect,
				       "button_remove", &dialog->button_remove,
				       "button_connect", &dialog->button_connect,
				       "button_close", &button_close,
				       NULL);

	gossip_glade_connect (glade, 
			      dialog,
			      "accounts_dialog", "destroy", accounts_dialog_destroy_cb,
			      "entry_name", "changed", accounts_dialog_entry_changed_cb,
			      "entry_id", "changed", accounts_dialog_entry_changed_cb,
			      "entry_password", "changed", accounts_dialog_entry_changed_cb,
			      "entry_server", "changed", accounts_dialog_entry_changed_cb,
			      "entry_port", "changed", accounts_dialog_entry_changed_cb,
			      "entry_resource", "changed", accounts_dialog_entry_changed_cb,
			      "entry_name", "focus-out-event", accounts_dialog_entry_focus_cb,
			      "entry_id", "focus-out-event", accounts_dialog_entry_focus_cb,
			      "entry_password", "focus-out-event", accounts_dialog_entry_focus_cb,
			      "entry_server", "focus-out-event", accounts_dialog_entry_focus_cb,
			      "entry_port", "focus-out-event", accounts_dialog_entry_focus_cb,
			      "entry_resource", "focus-out-event", accounts_dialog_entry_focus_cb,
			      "entry_port", "insert_text", accounts_dialog_entry_port_insert_text_cb,
			      "checkbutton_proxy", "toggled", accounts_dialog_checkbutton_toggled_cb,
			      "checkbutton_ssl", "toggled", accounts_dialog_checkbutton_toggled_cb,
			      "checkbutton_connect", "toggled", accounts_dialog_checkbutton_toggled_cb,
			      "button_add", "clicked", accounts_dialog_button_add_clicked_cb,
			      "button_remove", "clicked", accounts_dialog_button_remove_clicked_cb,
			      "button_connect", "clicked", accounts_dialog_button_connect_clicked_cb,
			      "button_close", "clicked", accounts_dialog_button_close_clicked_cb,
			      NULL);

	g_object_add_weak_pointer (G_OBJECT (dialog->window), (gpointer) &dialog);
	
	g_object_unref (glade);

	session = gossip_app_get_session ();
	manager = gossip_session_get_account_manager (session);

	g_signal_connect (manager, "account_added",
			  G_CALLBACK (accounts_dialog_account_added_cb), 
			  dialog);

	g_signal_connect (manager, "account_removed",
			  G_CALLBACK (accounts_dialog_account_removed_cb), 
			  dialog);

	g_signal_connect (session, "protocol-connected",
			  G_CALLBACK (accounts_dialog_protocol_connected_cb), 
			  dialog);

	g_signal_connect (session, "protocol-disconnected",
			  G_CALLBACK (accounts_dialog_protocol_disconnected_cb), 
			  dialog);

	accounts_dialog_model_setup (dialog);
	accounts_dialog_setup (dialog);

	/* set up remaining widgets */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, label_name);
	gtk_size_group_add_widget (size_group, label_id);
	gtk_size_group_add_widget (size_group, label_password);

	g_object_unref (size_group);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, label_server);
	gtk_size_group_add_widget (size_group, label_port);

	g_object_unref (size_group);

	gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (bbox), 
					    button_close, TRUE);
	
	gtk_window_set_transient_for (GTK_WINDOW (dialog->window), 
				      GTK_WINDOW (gossip_app_get_window ()));
	gtk_widget_show (dialog->window);

	gtk_widget_set_sensitive (dialog->notebook, FALSE);

	if (GOSSIP_IS_ACCOUNT (optional_account)) {
		/* if account was specified then we select it */
		accounts_dialog_model_set_selected (dialog, optional_account);
	} else {
		accounts_dialog_model_select_first (dialog);
	}
}
