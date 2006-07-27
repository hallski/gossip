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

#include <string.h>
#include <config.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>

#include <libgossip/gossip-utils.h> 

#include "gossip-ui-utils.h"
#include "gossip-status-presets-dialog.h"


typedef struct {
	GtkWidget *dialog;
	GtkWidget *treeview;
	GtkWidget *status_combobox;
	GtkWidget *message_entry;
	GtkWidget *remove_button;
	GtkWidget *add_button;
} GossipStatusPresetsDialog;


enum {
	COL_TYPE,
	COL_STRING,
	COL_STATE,
	NUM_COLS
};


static void status_presets_setup                     (GossipStatusPresetsDialog *dialog);
static void status_presets_selection_changed_cb      (GtkTreeSelection          *selection,
						      GossipStatusPresetsDialog *dialog);
static void status_presets_remove_button_clicked_cb  (GtkWidget                 *button,
						      GossipStatusPresetsDialog *dialog);
static void status_presets_message_entry_changed_cb  (GtkWidget                 *widget,
						      GossipStatusPresetsDialog *dialog);
static void status_presets_message_entry_activate_cb (GtkWidget                 *widget,
						      GossipStatusPresetsDialog *dialog);
static void status_presets_response_cb               (GtkWidget                 *widget,
						      gint                       response,
						      gpointer                   user_data);
static void status_presets_destroy_cb                (GtkWidget                 *widget,
						      GossipStatusPresetsDialog *editor);


static void 
status_presets_setup (GossipStatusPresetsDialog *dialog)
{
	GtkComboBox         *combobox;
	GtkTreeView         *view;
	GtkTreeViewColumn   *column;
	GtkTreeSelection    *selection;
	GtkListStore        *store;
	GtkTreeIter          iter;
	GtkCellRenderer     *renderer;
 	GList               *list, *l; 
	GdkPixbuf           *pixbuf;
	GossipPresenceState  state;

	combobox = GTK_COMBO_BOX (dialog->status_combobox);

  	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combobox));  

	store = gtk_list_store_new (3, 
				    GDK_TYPE_PIXBUF, 
				    G_TYPE_STRING, 
				    G_TYPE_INT);

	gtk_combo_box_set_model (combobox, GTK_TREE_MODEL (store));

	/* set up status' */
	state = GOSSIP_PRESENCE_STATE_AVAILABLE;
	pixbuf = gossip_ui_utils_presence_state_get_pixbuf (state);
	
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 
			    0, pixbuf,
			    1, _("Available"),
			    2, state,
			    -1);

	g_object_unref (pixbuf);

	state = GOSSIP_PRESENCE_STATE_BUSY;
	pixbuf = gossip_ui_utils_presence_state_get_pixbuf (state);
	
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 
			    0, pixbuf,
			    1, _("Busy"),
			    2, state,
			    -1);

	g_object_unref (pixbuf);

	state = GOSSIP_PRESENCE_STATE_AWAY;
	pixbuf = gossip_ui_utils_presence_state_get_pixbuf (state);
	
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 
			    0, pixbuf,
			    1, _("Away"),
			    2, state,
			    -1);

	g_object_unref (pixbuf);
	
	/* use Away as the default */
	gtk_combo_box_set_active_iter (combobox, &iter);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
					"pixbuf", 0,
					NULL);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
					"text", 1,
					NULL);

	g_object_unref (store);

	view = GTK_TREE_VIEW (dialog->treeview);

	selection = gtk_tree_view_get_selection (view);
	g_signal_connect (selection,
			  "changed", G_CALLBACK (status_presets_selection_changed_cb),
			  dialog);

	store = gtk_list_store_new (NUM_COLS, 
				    GDK_TYPE_PIXBUF, 
				    G_TYPE_STRING,
				    G_TYPE_INT);

	column = gtk_tree_view_column_new ();

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "pixbuf", COL_TYPE,
					     NULL);
	
	renderer = gtk_cell_renderer_text_new (); 
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "text", COL_STRING, 
					     NULL);

	gtk_tree_view_insert_column (view, column, 0);

	/* insert data */
	list = gossip_utils_get_status_messages ();

	for (l = list; l; l = l->next) {
		GossipStatusEntry *entry;
		GdkPixbuf         *pixbuf;

		entry = l->data;

		pixbuf = gossip_ui_utils_presence_state_get_pixbuf (entry->state);
		
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_TYPE, pixbuf,
				    COL_STRING, entry->string,
				    COL_STATE, entry->state,
				    -1);

		g_object_unref (pixbuf);
	}

	gossip_utils_free_status_messages (list);

	gtk_tree_view_set_model (view, GTK_TREE_MODEL (store));

	g_object_unref (store);
}

static void
status_presets_selection_changed_cb (GtkTreeSelection          *selection,
				     GossipStatusPresetsDialog *dialog)
{
	if (!gtk_tree_selection_get_selected (selection, NULL, NULL)) {
		gtk_widget_set_sensitive (dialog->remove_button, FALSE);
		return;
	}
	
	gtk_widget_set_sensitive (dialog->remove_button, TRUE);
}

static void
status_presets_add_button_clicked_cb (GtkWidget                 *button,
				      GossipStatusPresetsDialog *dialog)
{
	GtkTreeModel        *model;
	GtkListStore        *store;
	GtkTreeViewColumn   *column;
	GtkTreePath         *path;
	GtkTreeIter          iter;
	GossipPresenceState  state;
	GdkPixbuf           *pixbuf;
	const gchar         *string;
	GList               *list;
	GossipStatusEntry   *entry;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->status_combobox));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->status_combobox), &iter);
	
	gtk_tree_model_get (model, &iter, 2, &state, -1);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->treeview));
	store = GTK_LIST_STORE (model);

	pixbuf = gossip_ui_utils_presence_state_get_pixbuf (state);

	string = gtk_entry_get_text (GTK_ENTRY (dialog->message_entry));

	/* save preset */
	list = gossip_utils_get_status_messages ();

	entry = g_new (GossipStatusEntry, 1);
	entry->string = g_strdup (string);
	entry->state = state;
	
	list = g_list_prepend (list, entry);

	gossip_utils_set_status_messages (list);
	gossip_utils_free_status_messages (list);

	/* set model */
	gtk_list_store_prepend (store, &iter);
	gtk_list_store_set (store, &iter,
			    COL_TYPE, pixbuf,
			    COL_STRING, string,
			    COL_STATE, state,
			    -1);

	path = gtk_tree_model_get_path (model, &iter);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (dialog->treeview), 0);
	gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (dialog->treeview),
				      path,
                                      column,
				      TRUE,
                                      0.5,
				      0);

	gtk_entry_set_text (GTK_ENTRY (dialog->message_entry), "");

	gtk_tree_path_free (path);
	g_object_unref (pixbuf);
}

static void
status_presets_remove_button_clicked_cb (GtkWidget                 *button,
					 GossipStatusPresetsDialog *dialog)
{
	GtkTreeSelection    *selection;
	GtkTreeModel        *model;
	GtkTreeIter          iter;
	gchar               *str;
	GList               *list, *l;
	GossipPresenceState  state;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->treeview));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return;
	}

	gtk_tree_model_get (model, &iter,
			    COL_STRING, &str,
			    COL_STATE, &state,
			    -1);

	list = gossip_utils_get_status_messages ();

	for (l = list; l; l = l->next) {
		GossipStatusEntry *entry = l->data;
		
		if (state == entry->state && !strcmp (str, entry->string)) {
			break;
		}
	}

	if (l) {
		g_free (l->data);
		list = g_list_delete_link (list, l);
	}

	gossip_utils_set_status_messages (list);
	gossip_utils_free_status_messages (list);
	
	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

	g_free (str);
}

static void
status_presets_message_entry_changed_cb (GtkWidget                 *widget,
					 GossipStatusPresetsDialog *dialog)
{
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (widget));
 	gtk_widget_set_sensitive (dialog->add_button, (str && strlen (str) > 0));
}

static void
status_presets_message_entry_activate_cb (GtkWidget                 *widget,
					  GossipStatusPresetsDialog *dialog)
{
	status_presets_add_button_clicked_cb (dialog->add_button, dialog);	
}

static void
status_presets_response_cb (GtkWidget *widget,
			    gint       response,
			    gpointer   user_data)
{
	gtk_widget_destroy (widget);
}

static void
status_presets_destroy_cb (GtkWidget                 *widget,
			   GossipStatusPresetsDialog *dialog)
{
	g_free (dialog);
}

void
gossip_status_presets_dialog_show (void)
{
	static GossipStatusPresetsDialog *dialog = NULL;
	GladeXML                         *gui;
 	GtkSizeGroup                     *sizegroup; 

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->dialog));
		return;
	}
	
	dialog = g_new0 (GossipStatusPresetsDialog, 1);
	
	gui = gossip_glade_get_file ("main.glade",
				     "status_presets_dialog",
				     NULL,
				     "status_presets_dialog", &dialog->dialog,
				     "treeview", &dialog->treeview,
				     "status_combobox", &dialog->status_combobox,
				     "message_entry", &dialog->message_entry,
				     "remove_button", &dialog->remove_button,
 				     "add_button", &dialog->add_button,
				     NULL);
	
	gossip_glade_connect (gui,
			      dialog,
			      "status_presets_dialog", "response", status_presets_response_cb,
			      "status_presets_dialog", "destroy", status_presets_destroy_cb,
			      "message_entry", "changed", status_presets_message_entry_changed_cb,
			      "message_entry", "activate", status_presets_message_entry_activate_cb,
 			      "add_button", "clicked", status_presets_add_button_clicked_cb, 
 			      "remove_button", "clicked", status_presets_remove_button_clicked_cb, 
			      NULL);

	g_object_unref (gui);

	g_object_add_weak_pointer (G_OBJECT (dialog->dialog), (gpointer) &dialog);

 	sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL); 
 	gtk_size_group_add_widget (sizegroup, dialog->add_button); 
 	gtk_size_group_add_widget (sizegroup, dialog->remove_button); 

	status_presets_setup (dialog);
}
