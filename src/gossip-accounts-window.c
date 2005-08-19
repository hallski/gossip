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

#include "gossip-accounts-window.h"
#include "gossip-ui-utils.h"
#include "gossip-app.h"
#include "gossip-new-account-window.h"
#include "gossip-account-info-dialog.h"


typedef struct {
	gpointer  *p;

	GtkWidget *window;

	GtkWidget *treeview;
	GtkWidget *button_add;
	GtkWidget *button_edit;
	GtkWidget *button_remove;
	GtkWidget *button_default;
} GossipAccountsWindow;


static void           accounts_window_setup                     (GossipAccountsWindow *window);
static void           accounts_window_model_setup               (GossipAccountsWindow *window);
static void           accounts_window_model_add_columns         (GossipAccountsWindow *window);
static GossipAccount *accounts_window_model_get_selected        (GossipAccountsWindow *window);
static gboolean       accounts_window_model_remove_selected     (GossipAccountsWindow *window);
static void           accounts_window_model_selection_changed   (GtkTreeSelection     *selection,
								 GossipAccountsWindow *window);
static void           accounts_window_model_cell_edited         (GtkCellRendererText  *cell,
								 const gchar          *path_string,
								 const gchar          *new_text,
								 GossipAccountsWindow *window);
static void           accounts_window_button_add_clicked_cb     (GtkWidget            *button,
								 GossipAccountsWindow *window);
static void           accounts_window_button_edit_clicked_cb    (GtkWidget            *button,
								 GossipAccountsWindow *window);
static void           accounts_window_button_remove_clicked_cb  (GtkWidget            *button,
								 GossipAccountsWindow *window);
static void           accounts_window_button_default_clicked_cb (GtkWidget            *button,
								 GossipAccountsWindow *window);
static gboolean       accounts_window_foreach                   (GtkTreeModel         *model,
								 GtkTreePath          *path,
								 GtkTreeIter          *iter,
								 gpointer              user_data);
static void           accounts_window_destroy_cb                (GtkWidget            *widget,
								 GossipAccountsWindow *window);


enum {
	COL_NAME,
	COL_EDITABLE,
	COL_ACCOUNT_POINTER,
	COL_COUNT
};


static void
accounts_window_setup (GossipAccountsWindow *window)
{

	GtkTreeView      *view;
	GtkListStore     *store;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;

	const GList      *accounts, *l;
	GossipAccount    *default_account = NULL;
	const gchar      *default_name = NULL;

	gboolean          editable = TRUE;
	gboolean          selected = FALSE;

	view = GTK_TREE_VIEW (window->treeview);
	store = GTK_LIST_STORE (gtk_tree_view_get_model (view));
	selection = gtk_tree_view_get_selection (view);

 	accounts = gossip_accounts_get_all (NULL); 
	if (accounts) {
		default_account = gossip_accounts_get_default (); 
		if (default_account) {
			default_name = gossip_account_get_name (default_account);
		}
	}

	for (l = accounts; l; l = l->next) {
 		GossipAccount *account; 
		const gchar   *name;

		account = l->data;

		name = gossip_account_get_name (account);
		if (!name) {
			continue;
		}

		gtk_list_store_append (store, &iter); 
		gtk_list_store_set (store, &iter, 
				    COL_NAME, name,
				    COL_EDITABLE, editable,
				    COL_ACCOUNT_POINTER, g_object_ref (account),
				    -1);

		if (!default_account && selected == FALSE) {
			gtk_tree_selection_select_iter (selection, &iter);
			selected = TRUE;
		} else {
			if (default_name && strcmp (name, default_name) == 0) {
				gtk_tree_selection_select_iter (selection, &iter);
			} 
		}
	}
}

static void 
accounts_window_model_setup (GossipAccountsWindow *window)
{
	GtkListStore     *store;
	GtkTreeSelection *selection;

	store = gtk_list_store_new (COL_COUNT,
				    G_TYPE_STRING,   /* name */
				    G_TYPE_BOOLEAN,  /* editable */
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
	GtkTreeModel      *model;
	GtkTreeViewColumn *column; 
	GtkCellRenderer   *renderer;
	guint              col_offset;
	
	view = GTK_TREE_VIEW (window->treeview);
	model = gtk_tree_view_get_model (view);
	
	renderer = gtk_cell_renderer_text_new ();

	col_offset = gtk_tree_view_insert_column_with_attributes (view,
								  -1, _("Account"),
								  renderer, 
								  "text", COL_NAME,
								  "editable", COL_EDITABLE,
								  NULL);

	g_signal_connect (renderer, "edited",
			  G_CALLBACK (accounts_window_model_cell_edited), 
			  window);
	g_object_set_data (G_OBJECT (renderer),
			   "column", GINT_TO_POINTER (COL_NAME));
	
	column = gtk_tree_view_get_column (view, col_offset - 1);
	gtk_tree_view_column_set_sort_column_id (column, COL_NAME);
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
	gtk_widget_set_sensitive (window->button_default, is_selection);
}

static void
accounts_window_model_cell_edited (GtkCellRendererText *cell,
				  const gchar         *path_string,
				  const gchar         *new_text,
				  GossipAccountsWindow *window)
{
	GtkTreeView   *view;
	GtkTreeModel  *model;
	GtkTreePath   *path;
	GtkTreeIter    iter;
	gint           column;
	gchar         *old_text;
	GossipAccount *account;

	view = GTK_TREE_VIEW (window->treeview);
	model = gtk_tree_view_get_model (view);

	path = gtk_tree_path_new_from_string (path_string);
	gtk_tree_model_get_iter (model, &iter, path);

 	column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column")); 

	gtk_tree_model_get (model, &iter, column, &old_text, -1);
	account = gossip_accounts_get_by_name (old_text);
	if (account) {
		gossip_account_set_name (account, new_text);
		gossip_accounts_store ();
	}

	g_free (old_text);

	gtk_list_store_set (GTK_LIST_STORE (model), &iter, 
			    column, new_text, -1);

	/* update account info */
	
	gtk_tree_path_free (path);
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
	GossipAccount *account;

	account = accounts_window_model_get_selected (window);
	gossip_accounts_remove (account);
	gossip_accounts_store ();

	/* should do this on a signal */
	accounts_window_model_remove_selected (window);
}

static void
accounts_window_button_default_clicked_cb (GtkWidget            *button,
					   GossipAccountsWindow *window)
{
	GossipAccount *account;

	account = accounts_window_model_get_selected (window);
	gossip_accounts_set_default (account);
	gossip_accounts_store ();
}

static gboolean
accounts_window_foreach (GtkTreeModel *model,
			 GtkTreePath  *path,
			 GtkTreeIter  *iter,
			 gpointer      user_data)
{
	GossipAccount *account;

	gtk_tree_model_get (model, iter, COL_ACCOUNT_POINTER, &account, -1);
	g_object_unref (account);

	return FALSE;
}

static void
accounts_window_destroy_cb (GtkWidget            *widget,
			    GossipAccountsWindow *window)
{
	GtkTreeModel *model;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (window->treeview));
	gtk_tree_model_foreach (model, 
				(GtkTreeModelForeachFunc) accounts_window_foreach, 
				NULL);

	*window->p = NULL;
	g_free (window);
}

void
gossip_accounts_window_show (void)
{
	static GossipAccountsWindow *window = NULL;
	GladeXML                    *glade;

	if (window) {
		gtk_window_present (GTK_WINDOW (window->window));
		return;
	}
	
	window = g_new0 (GossipAccountsWindow, 1);

	window->p = (gpointer) &window;

	glade = gossip_glade_get_file (GLADEDIR "/connect.glade",
				       "accounts_window",
				       NULL,
				       "accounts_window", &window->window,
				       "treeview", &window->treeview,
				       "button_add", &window->button_add,
				       "button_edit", &window->button_edit,
				       "button_remove", &window->button_remove,
				       "button_default", &window->button_default,
				       NULL);

	gossip_glade_connect (glade, 
			      window,
			      "accounts_window", "destroy", accounts_window_destroy_cb,
			      "button_add", "clicked", accounts_window_button_add_clicked_cb,
			      "button_edit", "clicked", accounts_window_button_edit_clicked_cb,
			      "button_remove", "clicked", accounts_window_button_remove_clicked_cb,
			      "button_default", "clicked", accounts_window_button_default_clicked_cb,
			      NULL);
	
	g_object_unref (glade);

	accounts_window_model_setup (window);
	accounts_window_setup (window);
	
	gtk_window_set_transient_for (GTK_WINDOW (window->window), 
				      GTK_WINDOW (gossip_app_get_window ()));
	gtk_widget_show (window->window);
}

