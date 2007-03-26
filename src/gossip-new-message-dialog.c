/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-utils.h>

#include "gossip-account-chooser.h"
#include "gossip-app.h"
#include "gossip-new-message-dialog.h"
#include "gossip-ui-utils.h"

typedef struct {
	GtkWidget    *dialog;

	GtkWidget    *accounts_vbox;
	GtkWidget    *accounts_chooser;
	GtkWidget    *name_entry;
	GtkWidget    *treeview;
	GtkWidget    *chat_button;

	GtkListStore *store;
	GtkTreeModel *filter;
} GossipNewMessageDialog;

static void     new_message_dialog_update_buttons     (GossipNewMessageDialog *dialog);
static void     new_message_dialog_pixbuf_data_func   (GtkCellLayout          *cell_layout,
						       GtkCellRenderer        *cell,
						       GtkTreeModel           *tree_model,
						       GtkTreeIter            *iter,
						       GossipNewMessageDialog *dialog);
static void     new_message_dialog_text_data_func     (GtkCellLayout          *cell_layout,
						       GtkCellRenderer        *cell,
						       GtkTreeModel           *tree_model,
						       GtkTreeIter            *iter,
						       GossipNewMessageDialog *dialog);
static gboolean new_message_dialog_filter_func        (GtkTreeModel           *model,
						       GtkTreeIter            *iter,
						       GossipNewMessageDialog *dialog);
static void     new_message_dialog_row_activated      (GtkTreeView            *view,
						       GtkTreePath            *path,
						       GtkTreeViewColumn      *column,
						       GossipNewMessageDialog *dialog);
static void     new_message_dialog_selection_changed  (GtkTreeSelection       *selection,
						       GossipNewMessageDialog *dialog);
static void     new_message_dialog_setup_contacts     (GossipNewMessageDialog *dialog);
static void     new_message_dialog_setup_view         (GossipNewMessageDialog *dialog);
static void     new_message_dialog_name_entry_changed (GtkEntry               *entry,
						       GossipNewMessageDialog *dialog);
static void     new_message_dialog_destroy            (GtkWidget              *widget,
						       GossipNewMessageDialog *dialog);
static void     new_message_dialog_response           (GtkWidget              *widget,
						       gint                    response,
						       GossipNewMessageDialog *dialog);

enum {
	COL_STATUS,
	COL_NAME,
	COL_POINTER,
	COL_COUNT
};

static void
new_message_dialog_update_buttons (GossipNewMessageDialog *dialog)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	gboolean          can_chat = FALSE;
	const gchar      *text;

	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);

	text = gtk_entry_get_text (GTK_ENTRY (dialog->name_entry));

	can_chat |= !G_STR_EMPTY (text);
	can_chat |= gtk_tree_selection_get_selected (selection, &model, &iter);

	gtk_widget_set_sensitive (dialog->chat_button, can_chat);
}

static void
new_message_dialog_pixbuf_data_func (GtkCellLayout          *cell_layout,
				     GtkCellRenderer        *cell,
				     GtkTreeModel           *tree_model,
				     GtkTreeIter            *iter,
				     GossipNewMessageDialog *dialog)
{
	GossipContact *contact;
	GdkPixbuf     *pixbuf;

	gtk_tree_model_get (tree_model, iter, COL_POINTER, &contact, -1);

	pixbuf = gossip_pixbuf_for_contact (contact);

	g_object_set (cell, "pixbuf", pixbuf, NULL);
	g_object_unref (pixbuf);
	g_object_unref (contact);
}

static void
new_message_dialog_text_data_func (GtkCellLayout          *cell_layout,
				   GtkCellRenderer        *cell,
				   GtkTreeModel           *tree_model,
				   GtkTreeIter            *iter,
				   GossipNewMessageDialog *dialog)
{
	GossipContact *contact;

	gtk_tree_model_get (tree_model, iter, COL_POINTER, &contact, -1);

	g_object_set (cell, "text", gossip_contact_get_name (contact), NULL);
	g_object_unref (contact);
}

static gboolean
new_message_dialog_filter_func (GtkTreeModel           *model,
				GtkTreeIter            *iter,
				GossipNewMessageDialog *dialog)
{
	GossipContact *contact;
	const gchar   *id;
	const gchar   *name;
	const gchar   *text;
	gchar         *id_nocase;
	gchar         *name_nocase;
	gchar         *text_nocase;
	gboolean       found = FALSE;

	gtk_tree_model_get (model, iter, COL_POINTER, &contact, -1);

	if (!contact) {
		return TRUE;
	}

	id = gossip_contact_get_id (contact);
	name = gossip_contact_get_name (contact);

	text = gtk_entry_get_text (GTK_ENTRY (dialog->name_entry));

	/* casefold */
	id_nocase = g_utf8_casefold (id, -1);
	name_nocase = g_utf8_casefold (name, -1);
	text_nocase = g_utf8_casefold (text, -1);

	/* compare */
	if (G_STR_EMPTY (text_nocase) ||
	    strstr (id_nocase, text_nocase) ||
	    strstr (name_nocase, text_nocase)) {
		found = TRUE;
	}

	g_object_unref (contact);

	g_free (id_nocase);
	g_free (name_nocase);
	g_free (text_nocase);

	return found;
}

static void
new_message_dialog_row_activated (GtkTreeView            *view,
				  GtkTreePath            *path,
				  GtkTreeViewColumn      *column,
				  GossipNewMessageDialog *dialog)
{
	new_message_dialog_response (dialog->dialog,
				     GTK_RESPONSE_OK,
				     dialog);
}

static void
new_message_dialog_selection_changed (GtkTreeSelection       *selection,
				      GossipNewMessageDialog *dialog)
{
	new_message_dialog_update_buttons (dialog);
}

static void
new_message_dialog_setup_contacts (GossipNewMessageDialog *dialog)
{
	GossipSession *session;
	const GList   *contacts, *l;

	session = gossip_app_get_session ();
	contacts = gossip_session_get_contacts (session);

	for (l = contacts; l; l = l->next) {
		GossipContact *contact;
		GtkTreeIter    iter;

		contact = l->data;

		gtk_list_store_append (dialog->store, &iter);
		gtk_list_store_set (dialog->store, &iter,
				    COL_NAME, gossip_contact_get_name (contact),
				    COL_POINTER, contact,
				    -1);
	}
}

static void
new_message_dialog_setup_view (GossipNewMessageDialog *dialog)
{
	GtkTreeView       *view;
	GtkTreeModel      *model, *filter;
	GtkTreeSelection  *selection;
	GtkTreeSortable   *sortable;
	GtkTreeViewColumn *column;
	GtkListStore      *store;
	GtkCellRenderer   *cell;

	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);

	/* new store */
	store = gtk_list_store_new (COL_COUNT,
				    GDK_TYPE_PIXBUF,   /* status */
				    G_TYPE_STRING,     /* name */
				    GOSSIP_TYPE_CONTACT);

	model = GTK_TREE_MODEL (store);
	sortable = GTK_TREE_SORTABLE (store);

	/* set up filter */
	filter = gtk_tree_model_filter_new (model, NULL);

	gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
						(GtkTreeModelFilterVisibleFunc)new_message_dialog_filter_func,
						dialog,
						NULL);

	gtk_tree_view_set_model (view, filter);

	g_object_unref (model);
	g_object_unref (filter);

	/* new column */
	column = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc)
						 new_message_dialog_pixbuf_data_func,
						 dialog,
						 NULL);

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc)
						 new_message_dialog_text_data_func,
						 dialog,
						 NULL);

	gtk_tree_view_append_column (view, column);

	/* set up treeview properties */
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	gtk_tree_sortable_set_sort_column_id (sortable,
					      COL_NAME,
					      GTK_SORT_ASCENDING);

	/* set up signals */
	g_signal_connect (view, "row-activated",
			  G_CALLBACK (new_message_dialog_row_activated),
			  dialog);

	g_signal_connect (selection, "changed",
			  G_CALLBACK (new_message_dialog_selection_changed),
			  dialog);

	dialog->store = store;
	dialog->filter = filter;
}

static void
new_message_dialog_name_entry_changed (GtkEntry               *entry,
				       GossipNewMessageDialog *dialog)
{
	GtkTreeView  *view;
	GtkTreeModel *model;

	gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (dialog->filter));

	view = GTK_TREE_VIEW (dialog->treeview);
	model = gtk_tree_view_get_model (view);

	if (gtk_tree_model_iter_n_children (model, NULL) > 0) {
		GtkTreeSelection *selection;
		GtkTreeIter       iter;

		selection = gtk_tree_view_get_selection (view);
		gtk_tree_model_get_iter_first (model, &iter);
		gtk_tree_selection_select_iter (selection, &iter);
	}

	new_message_dialog_update_buttons (dialog);
}

static void
new_message_dialog_destroy (GtkWidget              *widget,
			    GossipNewMessageDialog *dialog)
{
	g_free (dialog);
}

static void
new_message_dialog_response (GtkWidget              *widget,
			     gint                    response,
			     GossipNewMessageDialog *dialog)
{
	GtkTreeView       *view;
	GtkTreeModel      *model;
	GtkTreeSelection  *selection;
	GtkTreeIter        iter;
	GossipContact     *contact;
	GossipChatManager *chat_manager;

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog->dialog);
		return;
	}

	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);

	contact = NULL;

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		GossipAccount        *account;
		GossipAccountChooser *accounts_chooser;
		const gchar          *text;

		accounts_chooser = GOSSIP_ACCOUNT_CHOOSER (dialog->accounts_chooser);
		account = gossip_account_chooser_get_account (accounts_chooser);

		text = gtk_entry_get_text (GTK_ENTRY (dialog->name_entry));

		contact = gossip_contact_new (GOSSIP_CONTACT_TYPE_TEMPORARY, account);
		if (contact) {
			gossip_contact_set_id (contact, text);
		}
	} else {
		gtk_tree_model_get (model, &iter,
				    COL_POINTER, &contact,
				    -1);
	}

	gtk_widget_destroy (dialog->dialog);

	if (contact) {
		chat_manager = gossip_app_get_chat_manager ();
		gossip_chat_manager_show_chat (chat_manager, contact);

		g_object_unref (contact);
	}
}

void
gossip_new_message_dialog_show (GtkWindow *parent)
{
	static GossipNewMessageDialog *dialog = NULL;
	GossipSession                 *session;
	GList                         *accounts;
	GladeXML                      *glade;

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->dialog));
		return;
	}

	dialog = g_new0 (GossipNewMessageDialog, 1);

	glade = gossip_glade_get_file ("main.glade",
				       "new_message_dialog",
				       NULL,
				       "new_message_dialog", &dialog->dialog,
				       "accounts_vbox", &dialog->accounts_vbox,
				       "name_entry", &dialog->name_entry,
				       "chat_button", &dialog->chat_button,
				       "treeview", &dialog->treeview,
				       NULL);

	gossip_glade_connect (glade,
			      dialog,
			      "new_message_dialog", "response", new_message_dialog_response,
			      "new_message_dialog", "destroy", new_message_dialog_destroy,
			      "name_entry", "changed", new_message_dialog_name_entry_changed,
			      NULL);

	g_object_unref (glade);

	g_object_add_weak_pointer (G_OBJECT (dialog->dialog), (gpointer) &dialog);

	new_message_dialog_setup_view (dialog);
	new_message_dialog_setup_contacts (dialog);

	/* set up account chooser */
	session = gossip_app_get_session ();

	dialog->accounts_chooser = gossip_account_chooser_new (session);
	gtk_box_pack_start (GTK_BOX (dialog->accounts_vbox),
			    dialog->accounts_chooser,
			    TRUE, TRUE, 0);
	gtk_widget_show (dialog->accounts_chooser);

	accounts = gossip_session_get_accounts (session);
	if (g_list_length (accounts) > 1) {
		gtk_widget_show (dialog->accounts_vbox);
	} else {
		/* show no accounts combo box */
		gtk_widget_hide (dialog->accounts_vbox);
	}

	g_list_foreach (accounts, (GFunc) g_object_unref, NULL);
	g_list_free (accounts);

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog), parent);
	}

	gtk_widget_show (dialog->dialog);
}

