/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2004 Imendio AB
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

/*#include "gossip-app.h" 
  #include "gossip-stock.h"
  #include "gossip-preferences.h"
*/

#include "gossip-ui-utils.h"
#include "gossip-status-presets-dialog.h"


typedef struct {
	GtkWidget *dialog;
	GtkWidget *treeview;
	GtkWidget *remove_button;
	GtkWidget *add_button;
} GossipStatusPresetsDialog;


enum {
	COL_TYPE,
	COL_TYPE_STR,
	COL_STRING,
	COL_STATE,
	COL_NEW,
	NUM_COLS
};

static const gchar *types[] = { N_("Available"), 
				N_("Busy"), 
				N_("Away") };


static void     status_presets_setup                    (GossipStatusPresetsDialog *dialog);
static void     status_presets_type_edited_cb           (GtkCellRendererText       *cell,
							 const gchar               *path,
							 const gchar               *value,
							 GtkListStore              *store);
static void     status_presets_type_edit_started_cb     (GtkCellRenderer           *renderer,
							 GtkCellEditable           *editable,
							 gchar                     *path,
							 gpointer                   user_data);
static void     status_presets_string_edited_cb         (GtkCellRendererText       *cell,
							 const gchar               *path,
							 const gchar               *value,
							 GtkListStore              *store);
static void     status_presets_string_edit_canceled_cb  (GtkCellRendererText       *cell,
							 GtkListStore              *store);
static gboolean status_presets_delete_canceled_cb       (GtkTreeModel              *model,
							 GtkTreePath               *path,
							 GtkTreeIter               *iter,
							 gpointer                   data);
static void     status_presets_selection_changed_cb     (GtkTreeSelection          *selection,
							 GossipStatusPresetsDialog *dialog);
static void     status_presets_remove_button_clicked_cb (GtkWidget                 *button,
							 GossipStatusPresetsDialog *dialog);
static void     status_presets_response_cb              (GtkWidget                 *widget,
							 gint                       response,
							 gpointer                   user_data);
static void     status_presets_destroy_cb               (GtkWidget                 *widget,
							 GossipStatusPresetsDialog *editor);


static void 
status_presets_setup (GossipStatusPresetsDialog *dialog)
{
	GtkTreeView       *view;
	GtkTreeViewColumn *column;
	GtkTreeSelection  *selection;
	GtkListStore      *store, *store_types;
	GtkTreeIter        iter;
	GtkCellRenderer   *renderer;
 	GList             *list, *l; 
	gint               i = 0;

	view = GTK_TREE_VIEW (dialog->treeview);

	selection = gtk_tree_view_get_selection (view);
	g_signal_connect (selection,
			  "changed", G_CALLBACK (status_presets_selection_changed_cb),
			  dialog);

	store = gtk_list_store_new (NUM_COLS, 
				    GDK_TYPE_PIXBUF, 
				    G_TYPE_STRING,
				    G_TYPE_STRING, 
				    G_TYPE_INT,
				    G_TYPE_BOOLEAN);

	/* create the combo box cell renderer */
	renderer = gtk_cell_renderer_combo_new ();
	store_types = gtk_list_store_new (2, 
					  GDK_TYPE_PIXBUF, 
					  G_TYPE_STRING, 
					  G_TYPE_STRING);
	while (i < G_N_ELEMENTS (types)) {
		GdkPixbuf *pixbuf;

		pixbuf = gossip_ui_utils_presence_state_get_pixbuf (i);

		gtk_list_store_append (store_types, &iter);
		gtk_list_store_set (store_types, &iter, 
				    0, pixbuf,
				    1, types[i], 
				    -1);
		i++;

		g_object_unref (pixbuf);
	}
	
	g_object_set (renderer, 
		      "model", store_types,
		      "has-entry", FALSE,
		      "text-column", 1,
		      "editable", TRUE,
		      NULL);
 	g_signal_connect (renderer, "edited", 
 			  G_CALLBACK (status_presets_type_edited_cb), store); 
 	g_signal_connect (renderer, "editing-started", 
 			  G_CALLBACK (status_presets_type_edit_started_cb), store); 

	gtk_tree_view_insert_column_with_attributes (view,
						     COL_TYPE_STR, "",
						     renderer,
						     NULL);
	g_object_unref (store_types);

	/* create editable column for the actual statu text */
	renderer = gtk_cell_renderer_text_new (); 
	g_object_set (renderer, 
		      "editable", TRUE,
		      NULL);
	g_signal_connect (renderer, "edited", 
			  G_CALLBACK (status_presets_string_edited_cb), store); 
 	g_signal_connect (renderer, "editing-canceled", 
 			  G_CALLBACK (status_presets_string_edit_canceled_cb), store); 

	gtk_tree_view_insert_column_with_attributes (view,
						     COL_STRING, "",
						     renderer,
						     "text", COL_STRING, 
						     NULL);

	/* add pixbuf to the combo column, that way, the both the
	   image and the empty text column will create the popup menu */
	column = gtk_tree_view_get_column (view, 0);
	
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "pixbuf", 0,
					     NULL);

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
				    COL_TYPE_STR, types[entry->state],
				    COL_STRING, entry->string,
				    COL_STATE, entry->state,
				    COL_NEW, FALSE,
				    -1);

		g_object_unref (pixbuf);
	}

	gossip_utils_free_status_messages (list);

	gtk_tree_view_set_model (view, GTK_TREE_MODEL (store));

	g_object_unref (store);
}

static void
status_presets_type_edited_cb (GtkCellRendererText *cell, 
			       const gchar         *path,
			       const gchar         *value, 
			       GtkListStore        *store)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	guint       i = -1;
	
	model = GTK_TREE_MODEL (store);
	
	if (!gtk_tree_model_get_iter_from_string (model, &iter, path)) {
		return;
	}
	
	while (++i < G_N_ELEMENTS (types)) {
		GList               *list, *l;
		GdkPixbuf           *pixbuf;
		gchar               *string;
		GossipPresenceState  old_state;

		if (g_utf8_collate (value, types[i]) != 0) {
			continue;
		}

		gtk_tree_model_get (model, &iter,
				    COL_STRING, &string,
				    COL_STATE, &old_state,
				    -1);

		/* save presets */
		list = gossip_utils_get_status_messages ();
		for (l = list; l; l = l->next) {
			GossipStatusEntry *entry = l->data;
			
			if (old_state == entry->state && 
			    !strcmp (string, entry->string)) {
				entry->state = i;
				break;
			}
		}

		g_free (string);

		gossip_utils_set_status_messages (list);
		gossip_utils_free_status_messages (list);
		
		/* set model */
		pixbuf = gossip_ui_utils_presence_state_get_pixbuf (i);
		
		gtk_list_store_set (store, &iter, 
				    COL_TYPE, pixbuf, 
				    COL_STATE, i,
				    -1);
		
		g_object_unref (pixbuf);
	}
}

static void 
status_presets_type_edit_started_cb (GtkCellRenderer *renderer,
				     GtkCellEditable *editable,
				     gchar           *path,
				     gpointer         user_data)
{
	GtkComboBox *combobox;

	if (!GTK_IS_COMBO_BOX (editable)) {
		return;
	}

	combobox = GTK_COMBO_BOX (editable);

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combobox));  
	
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
}

static void
status_presets_string_edited_cb (GtkCellRendererText *cell, 
				 const gchar         *path,
				 const gchar         *value, 
				 GtkListStore        *store)
{
	GtkTreeModel        *model;
	GtkTreeIter          iter;
	GList               *list;
	GossipStatusEntry   *entry;
	GossipPresenceState  state;
	gboolean             is_new;
	gchar               *old_string;
	
	model = GTK_TREE_MODEL (store);

	if (!gtk_tree_model_get_iter_from_string (model, &iter, path)) {
		return;
	}

	gtk_tree_model_get (model, &iter,
			    COL_NEW, &is_new,
			    COL_STRING, &old_string,
			    COL_STATE, &state, 
			    -1);
	
	list = gossip_utils_get_status_messages ();

	if (is_new) {
		entry = g_new (GossipStatusEntry, 1);
		entry->string = g_strdup (value);
		entry->state = state;
		
		list = g_list_prepend (list, entry);
	} else {
		GList *l;

		for (l = list; l; l = l->next) {
			GossipStatusEntry *entry = l->data;
			
			if (state == entry->state && 
			    !strcmp (old_string, entry->string)) {
				g_free (entry->string);
				entry->string = g_strdup (value);
				break;
			}
		}
	}

	g_free (old_string);

	gossip_utils_set_status_messages (list);
	gossip_utils_free_status_messages (list);

	gtk_list_store_set (store, &iter, 
			    COL_STRING, value, 
			    COL_NEW, FALSE,
			    -1);
}

static void
status_presets_string_edit_canceled_cb (GtkCellRendererText *cell, 
					GtkListStore        *store)
{
	gtk_tree_model_foreach (GTK_TREE_MODEL (store),
				(GtkTreeModelForeachFunc) status_presets_delete_canceled_cb,
				NULL);
}

static gboolean
status_presets_delete_canceled_cb (GtkTreeModel *model,
				   GtkTreePath  *path,
				   GtkTreeIter  *iter,
				   gpointer      data)
{
	gboolean is_new;

	gtk_tree_model_get (model, iter, COL_NEW, &is_new, -1);
	if (is_new) {
		gtk_list_store_remove (GTK_LIST_STORE (model), iter);
	}

	return TRUE;
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
	GtkTreeViewColumn   *column;
	GtkListStore        *store;
	GtkTreePath         *path;
	GtkTreeIter          iter;
	GossipPresenceState  state;
	GdkPixbuf           *pixbuf;

	state = GOSSIP_PRESENCE_STATE_AWAY;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->treeview));
	store = GTK_LIST_STORE (model);

	pixbuf = gossip_ui_utils_presence_state_get_pixbuf (state);

	gtk_list_store_prepend (store, &iter);
	gtk_list_store_set (store, &iter,
			    COL_TYPE, pixbuf,
			    COL_TYPE_STR, types[state],
			    COL_STRING, "",
			    COL_STATE, state,
			    COL_NEW, TRUE,
			    -1);

	g_object_unref (pixbuf);

	/* set it to be editable */
	path = gtk_tree_path_new_from_string ("0");
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (dialog->treeview), 1);
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (dialog->treeview),
				  path, column, TRUE);
	
	gtk_tree_path_free (path);
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
}

static void
status_presets_response_cb (GtkWidget *widget,
			    gint       response,
			    gpointer   user_data)
{
	gtk_widget_destroy (widget);
}

static void
status_presets_destroy_cb (GtkWidget          *widget,
			   GossipStatusPresetsDialog *dialog)
{
	g_free (dialog);
}

void
gossip_status_presets_dialog_show (void)
{
	static GossipStatusPresetsDialog *dialog = NULL;
	GladeXML                         *gui;

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->dialog));
		return;
	}
	
	dialog = g_new0 (GossipStatusPresetsDialog, 1);
	
	gui = gossip_glade_get_file (GLADEDIR "/main.glade",
				     "status_presets_dialog",
				     NULL,
				     "status_presets_dialog", &dialog->dialog,
				     "treeview", &dialog->treeview,
				     "remove_button", &dialog->remove_button,
				     "add_button", &dialog->add_button,
				     NULL);
	
	gossip_glade_connect (gui,
			      dialog,
			      "status_presets_dialog", "response", status_presets_response_cb,
			      "status_presets_dialog", "destroy", status_presets_destroy_cb,
 			      "add_button", "clicked", status_presets_add_button_clicked_cb, 
 			      "remove_button", "clicked", status_presets_remove_button_clicked_cb, 
			      NULL);

	g_object_add_weak_pointer (G_OBJECT (dialog->dialog), (gpointer) &dialog);

	status_presets_setup (dialog);
}
