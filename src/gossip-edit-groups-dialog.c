/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Martyn Russell <ginxd@btopenworld.com>
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
#include <glib/gi18n.h>
#include <loudmouth/loudmouth.h>

#include "gossip-app.h"
#include "gossip-edit-groups-dialog.h"


struct _GossipEditGroups {
	GtkWidget   *dialog;
	GtkWidget   *contact_label;
	GtkWidget   *id_label;
	GtkWidget   *add_entry;
	GtkWidget   *add_button;

	GtkWidget   *ok_button;

	GtkTreeView *treeview;

        GossipContact *contact;

	gboolean     changes_made;
};


typedef struct {
	GossipEditGroups *info;
	
	const gchar      *name;
	
	gboolean          found;
	GtkTreeIter       found_iter;
} FindName;


typedef struct {
	GossipEditGroups *info;
	GList            *list;
} FindSelected;


enum {
	COL_NAME,
	COL_ENABLED,
	COL_EDITABLE,
	COL_COUNT
};

static void     edit_groups_dialog_destroy_cb     (GtkWidget             *widget,
						   GossipEditGroups      *info);
static void     edit_groups_dialog_response_cb    (GtkWidget             *widget,
						   gint                   response,
						   GossipEditGroups      *info);
static void     edit_groups_setup                 (GossipEditGroups      *info);
static void     edit_groups_populate_columns      (GossipEditGroups      *info);
static void     edit_groups_add_groups            (GossipEditGroups      *info,
						   GList                 *groups);
static void     edit_groups_set                   (GossipEditGroups      *info,
						   GList                 *groups);
static void     edit_groups_save                  (GossipEditGroups      *info);
static gboolean edit_groups_find_name             (GossipEditGroups      *info,
						   const gchar           *name,
						   GtkTreeIter           *iter);
static gboolean edit_groups_find_name_foreach     (GtkTreeModel          *model,
						   GtkTreePath           *path,
						   GtkTreeIter           *iter,
						   FindName              *data);
static GList *  edit_groups_find_selected         (GossipEditGroups      *info);
static gboolean edit_groups_find_selected_foreach (GtkTreeModel          *model,
						   GtkTreePath           *path,
						   GtkTreeIter           *iter,
						   FindSelected          *data);
static void     edit_groups_cell_toggled          (GtkCellRendererToggle *cell,
						   gchar                 *path_string,
						   GossipEditGroups      *info);
static void     edit_groups_add_entry_changed     (GtkEditable           *editable,
						   GossipEditGroups      *info);
static void     edit_groups_add_entry_activated   (GtkWidget             *widget,
						   GossipEditGroups      *info);
static void     edit_groups_add_button_clicked    (GtkButton             *button,
						   GossipEditGroups      *info);


static void
edit_groups_dialog_destroy_cb (GtkWidget *widget, GossipEditGroups *info)
{
        g_object_unref (info->contact);

 	g_free (info); 
}

static void
edit_groups_dialog_response_cb (GtkWidget *widget, gint response, GossipEditGroups *info)
{
	if (info->changes_made && response == GTK_RESPONSE_OK) {
		edit_groups_save (info);
	}
	
	gtk_widget_destroy (info->dialog);
}

GossipEditGroups *
gossip_edit_groups_new (GossipContact *contact)
{
	GossipEditGroups *info;
	GladeXML         *gui;
	gchar            *str, *tmp;
        GList            *groups;

	info = g_new0 (GossipEditGroups, 1);

        info->contact = g_object_ref (contact);
        groups = gossip_contact_get_groups (contact);
        
	gui = gossip_glade_get_file (GLADEDIR "/main.glade",
				     "edit_groups_dialog",
				     NULL,
				     "edit_groups_dialog", &info->dialog,
				     "contact_label", &info->contact_label,
				     "jid_label", &info->id_label,
				     "add_entry", &info->add_entry,
				     "add_button", &info->add_button,
				     "ok_button", &info->ok_button,
				     "groups_treeview", &info->treeview,
				     NULL);

	gossip_glade_connect (gui,
			      info,
			      "edit_groups_dialog", "response", edit_groups_dialog_response_cb,
			      "edit_groups_dialog", "destroy", edit_groups_dialog_destroy_cb,
			      "add_entry", "changed", edit_groups_add_entry_changed,
			      "add_entry", "activate", edit_groups_add_entry_activated,
			      "add_button", "clicked", edit_groups_add_button_clicked,
			      NULL);

	g_object_unref (gui);

	str = g_markup_escape_text (gossip_contact_get_name (contact), -1);
	tmp = g_strdup_printf (_("Edit groups for %s"), str);
	g_free (str);
	
	str = g_strdup_printf ("<b>%s</b>", tmp);
	gtk_label_set_markup (GTK_LABEL (info->contact_label), str);
	g_free (str);
	g_free (tmp);

	if (strcmp (gossip_contact_get_name (contact),
                    gossip_contact_get_id (contact)) != 0) {
		gtk_label_set_text (GTK_LABEL (info->id_label), 
                                    gossip_contact_get_id (info->contact));
	} else {
		gtk_widget_hide (GTK_WIDGET (info->id_label));
	}

	edit_groups_setup (info);
	edit_groups_add_groups (info, groups);

	gtk_widget_show (info->dialog);

	return info;
}

GtkWidget *
gossip_edit_groups_get_dialog (GossipEditGroups *info)
{
	return info->dialog;
}

static void 
edit_groups_setup (GossipEditGroups *info)
{
	GtkListStore     *store;
	GtkTreeSelection *selection;

	store = gtk_list_store_new (COL_COUNT,
				    G_TYPE_STRING,   /* name */
				    G_TYPE_BOOLEAN,  /* enabled */
				    G_TYPE_BOOLEAN); /* editable */
	
	gtk_tree_view_set_model (info->treeview, GTK_TREE_MODEL (store));

	selection = gtk_tree_view_get_selection (info->treeview);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	
	edit_groups_populate_columns (info);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), 
					      COL_NAME, GTK_SORT_ASCENDING);

	g_object_unref (store);
}

static void 
edit_groups_populate_columns (GossipEditGroups *info)
{
	GtkTreeModel      *model;
	GtkTreeViewColumn *column; 
	GtkCellRenderer   *renderer;
	guint              col_offset;
	
	model = gtk_tree_view_get_model (info->treeview);
	
	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled", 
			  G_CALLBACK (edit_groups_cell_toggled), 
			  info);
	
	column = gtk_tree_view_column_new_with_attributes (_("Select"), renderer,
							   "active", COL_ENABLED,
							   NULL);
	
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width (column, 50);
	gtk_tree_view_append_column (info->treeview, column);
	
	renderer = gtk_cell_renderer_text_new ();
	col_offset = gtk_tree_view_insert_column_with_attributes (
		info->treeview,
		-1, _("Group"),
		renderer, 
		"text", COL_NAME,
		NULL);
	
	g_object_set_data (G_OBJECT (renderer),
			   "column", GINT_TO_POINTER (COL_NAME));
	
	column = gtk_tree_view_get_column (info->treeview, col_offset - 1);
	gtk_tree_view_column_set_sort_column_id (column, COL_NAME);
	gtk_tree_view_column_set_resizable (column,FALSE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);
}

static void
edit_groups_set (GossipEditGroups *info, GList *groups)
{
        gossip_contact_set_groups (info->contact, groups);
        gossip_session_update_contact (gossip_app_get_session (),
                                       info->contact);
}

static void
edit_groups_save (GossipEditGroups *info)
{
	GList *groups;

	groups = edit_groups_find_selected (info); 

	edit_groups_set (info, groups);
	
	g_list_foreach (groups, (GFunc)g_free, NULL); 
	g_list_free (groups);
}

static void
edit_groups_add_groups (GossipEditGroups *info, GList *groups)
{
	GtkListStore *store;
	GtkTreeIter   iter;	
	GList        *l;
	GList        *my_groups = NULL;
	GList        *all_groups;

	store = GTK_LIST_STORE (gtk_tree_view_get_model (info->treeview));

	all_groups = gossip_session_get_groups (gossip_app_get_session ());

	for (l = groups; l; l = l->next) {
		const gchar *group_str = (const gchar *) l->data;

		if (strcmp (group_str, _("Unsorted")) == 0) {
			continue;
		}
		
		my_groups = g_list_append (my_groups, g_strdup (group_str));
	}
	
	for (l = all_groups; l; l = l->next) {
		const gchar *group_str = (const gchar *) l->data;

		if (strcmp (group_str, _("Unsorted")) == 0) {
			continue;
		}
	
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,    
				    COL_NAME, group_str,
				    COL_EDITABLE, TRUE,
				    -1);

		if (g_list_find_custom (my_groups, group_str, (GCompareFunc)strcmp)) {
			if (edit_groups_find_name (info, group_str, &iter)) {
				gtk_list_store_set (store, &iter,    
						    COL_ENABLED, TRUE,
						    -1);
			}
		}
	}

	g_list_foreach (my_groups, (GFunc) g_free, NULL); 
	g_list_free (my_groups);

	g_list_free (all_groups);
}

static gboolean 
edit_groups_find_name (GossipEditGroups *info, 
		       const gchar      *name,
		       GtkTreeIter      *iter)
{
	GtkTreeModel *model;
	FindName      data;
	
	if (!name || strlen (name) < 1) {
		return FALSE;
	}
	
	data.info = info;
	data.name = name;
	data.found = FALSE;
	
	model = gtk_tree_view_get_model (info->treeview);
	
	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) edit_groups_find_name_foreach,
				&data);
	
	if (data.found == TRUE) {
		*iter = data.found_iter;
		return TRUE;
	}
	
	return FALSE;
}

static gboolean 
edit_groups_find_name_foreach (GtkTreeModel *model, 
			       GtkTreePath  *path, 
			       GtkTreeIter  *iter, 
			       FindName     *data) 
{
	gchar *name;

	gtk_tree_model_get (model, iter, 
			    COL_NAME, &name, 
			    -1);
  
	if (!name) {
		return FALSE;
	}
	
	if (data->name && strcmp (data->name, name) == 0) {
		data->found = TRUE;
		data->found_iter = *iter;

		g_free (name);
		
		return TRUE;
	}

	g_free (name);
	
	return FALSE;
}

static GList * 
edit_groups_find_selected (GossipEditGroups  *info)
{
	GtkTreeModel *model;
	FindSelected  data;
	
	data.info = info;
	data.list = NULL;

	model = gtk_tree_view_get_model (info->treeview);
	
	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) edit_groups_find_selected_foreach,
				&data);
	
	return data.list;
}

static gboolean 
edit_groups_find_selected_foreach (GtkTreeModel *model, 
				   GtkTreePath  *path, 
				   GtkTreeIter  *iter, 
				   FindSelected *data) 
{
	gchar    *name;
	gboolean  selected;

	gtk_tree_model_get (model, iter, 
			    COL_NAME, &name, 
			    COL_ENABLED, &selected, 
			    -1);
  
	if (!name) {
		return FALSE;
	}
	
	if (selected) {
		data->list = g_list_append (data->list, name);
		return FALSE;
	}
	
	g_free (name);
  
	return FALSE;
}

static void 
edit_groups_cell_toggled (GtkCellRendererToggle *cell, 
			  gchar                 *path_string, 
			  GossipEditGroups      *info)
{
	GtkTreeModel *model;
	GtkListStore *store;
	GtkTreePath  *path;
	GtkTreeIter   iter;

	gboolean      enabled;

	g_return_if_fail(info != NULL);

	model = gtk_tree_view_get_model (info->treeview);
	store = GTK_LIST_STORE (model);
	
	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, COL_ENABLED, &enabled, -1);

	enabled ^= 1;

	gtk_list_store_set (store, &iter, COL_ENABLED, enabled, -1);
	gtk_tree_path_free (path);

	info->changes_made = TRUE;
}

static void 
edit_groups_add_entry_changed (GtkEditable *editable, GossipEditGroups *info)
{
	GtkTreeIter  iter;
	gchar       *group = NULL;
	
	g_return_if_fail (info != NULL);
	
	group = gtk_editable_get_chars (editable, 0, -1); 
	
	if (edit_groups_find_name (info, group, &iter)) {
		gtk_widget_set_sensitive (GTK_WIDGET (info->add_button),  
					  FALSE);
	
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (info->add_button), 
					  !(group == NULL || strlen(group) < 1));
	}

	g_free (group);
}

static void 
edit_groups_add_entry_activated (GtkWidget *widget, GossipEditGroups *info)
{
	gtk_widget_activate (GTK_WIDGET (info->add_button));
}

static void 
edit_groups_add_button_clicked (GtkButton *button, GossipEditGroups *info)
{
	GtkListStore *store;
	GtkTreeIter   iter;
	const gchar  *group;
	
	store = GTK_LIST_STORE (gtk_tree_view_get_model (info->treeview));
	
	group = gtk_entry_get_text (GTK_ENTRY (info->add_entry));
	
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,    
			    COL_NAME, group,
			    COL_ENABLED, TRUE,
			    COL_EDITABLE, TRUE,
			    -1);
	
	gtk_entry_set_text (GTK_ENTRY (info->add_entry), "");
	
	info->changes_made = TRUE;
}

