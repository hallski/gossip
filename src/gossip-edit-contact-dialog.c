/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
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

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libgossip/gossip.h>

#include "gossip-app.h"
#include "gossip-edit-contact-dialog.h"
#include "gossip-glade.h"
#include "gossip-ui-utils.h"

typedef struct {
	GtkWidget       *dialog;
	GtkWidget       *label_name;
	GtkWidget       *entry_name;
	GtkWidget       *button_retrieve;
	GtkWidget       *entry_group;
	GtkWidget       *button_add;
	GtkWidget       *treeview;
	GtkWidget       *button_ok;
	GtkWidget       *frame_subscription;
	GtkWidget       *label_subscription;
	GtkWidget       *button_subscribe;

	GossipContact   *contact;
	GtkCellRenderer *renderer;
	gboolean         changes_made;
} GossipEditContactDialog;

typedef struct {
	GossipEditContactDialog *dialog;

	const gchar             *name;

	gboolean                 found;
	GtkTreeIter              found_iter;
} FindName;

typedef struct {
	GossipEditContactDialog *dialog;
	GList                   *list;
} FindSelected;

enum {
	COL_NAME,
	COL_ENABLED,
	COL_EDITABLE,
	COL_COUNT
};

static void     edit_contact_dialog_init                         (void);
static void     edit_contact_dialog_save_name                    (GossipEditContactDialog *dialog);
static void     edit_contact_dialog_save_groups                  (GossipEditContactDialog *dialog);
static gboolean edit_contact_dialog_can_save                     (GossipEditContactDialog *dialog);
static void     edit_contact_dialog_model_setup                  (GossipEditContactDialog *dialog);
static void     edit_contact_dialog_model_populate_columns       (GossipEditContactDialog *dialog);
static void     edit_contact_dialog_model_populate_data          (GossipEditContactDialog *dialog,
								  GList                   *groups);
static gboolean edit_contact_dialog_model_find_name              (GossipEditContactDialog *dialog,
								  const gchar             *name,
								  GtkTreeIter             *iter);
static gboolean edit_contact_dialog_model_find_name_foreach      (GtkTreeModel            *model,
								  GtkTreePath             *path,
								  GtkTreeIter             *iter,
								  FindName                *data);
static GList *  edit_contact_dialog_model_find_selected          (GossipEditContactDialog *dialog);
static gboolean edit_contact_dialog_model_find_selected_foreach  (GtkTreeModel            *model,
								  GtkTreePath             *path,
								  GtkTreeIter             *iter,
								  FindSelected            *data);
static void     edit_contact_dialog_update_widgets               (GossipContact           *contact);
static void     edit_contact_dialog_contact_updated_cb           (GossipContact           *contact,
								  gpointer                 user_data);
static void     edit_contact_dialog_cell_toggled                 (GtkCellRendererToggle   *cell,
								  gchar                   *path_string,
								  GossipEditContactDialog *dialog);
static void     edit_contact_dialog_entry_name_changed_cb        (GtkEditable             *editable,
								  GossipEditContactDialog *dialog);
static void     edit_contact_dialog_button_add_clicked_cb        (GtkButton               *button,
								  GossipEditContactDialog *dialog);
static void     edit_contact_dialog_button_retrieve_get_vcard_cb (GossipResult             result,
								  GossipVCard             *vcard,
								  GossipContact           *contact);
static void     edit_contact_dialog_button_retrieve_clicked_cb   (GtkButton               *button,
								  GossipEditContactDialog *dialog);
static void     edit_contact_dialog_button_subscribe_clicked_cb  (GtkWidget               *widget,
								  GossipEditContactDialog *dialog);
static void     edit_contact_dialog_destroy_cb                   (GtkWidget               *widget,
								  GossipEditContactDialog *dialog);
static void     edit_contact_dialog_response_cb                  (GtkWidget               *widget,
								  gint                     response,
								  GossipEditContactDialog *dialog);

static GHashTable *dialogs = NULL;

static void
edit_contact_dialog_init (void)
{
	if (dialogs) {
		return;
	}

	dialogs = g_hash_table_new_full (gossip_contact_hash,
					 gossip_contact_equal,
					 g_object_unref,
					 NULL);
}

static void
edit_contact_dialog_save_name (GossipEditContactDialog *dialog)
{
	const gchar *name;

	name = gtk_entry_get_text (GTK_ENTRY (dialog->entry_name));

	gossip_session_rename_contact (gossip_app_get_session (),
				       dialog->contact,
				       name);
}

static void
edit_contact_dialog_save_groups (GossipEditContactDialog *dialog)
{
	GList *groups;

	groups = edit_contact_dialog_model_find_selected (dialog);

	gossip_contact_set_groups (dialog->contact, groups);
	gossip_session_update_contact (gossip_app_get_session (),
				       dialog->contact);

	g_list_foreach (groups, (GFunc)g_free, NULL);
	g_list_free (groups);
}

static gboolean
edit_contact_dialog_can_save (GossipEditContactDialog *dialog)
{
	const gchar *name;
	gboolean    ok = TRUE;

	name = gtk_entry_get_text (GTK_ENTRY (dialog->entry_name));

	ok &= dialog->changes_made == TRUE;
	ok &= !G_STR_EMPTY (name);

	return ok;
}

static void
edit_contact_dialog_model_setup (GossipEditContactDialog *dialog)
{
	GtkTreeView      *view;
	GtkListStore     *store;
	GtkTreeSelection *selection;

	view = GTK_TREE_VIEW (dialog->treeview);

	store = gtk_list_store_new (COL_COUNT,
				    G_TYPE_STRING,   /* name */
				    G_TYPE_BOOLEAN,  /* enabled */
				    G_TYPE_BOOLEAN); /* editable */

	gtk_tree_view_set_model (view, GTK_TREE_MODEL (store));

	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	edit_contact_dialog_model_populate_columns (dialog);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      COL_NAME, GTK_SORT_ASCENDING);

	g_object_unref (store);
}

static void
edit_contact_dialog_model_populate_columns (GossipEditContactDialog *dialog)
{
	GtkTreeView       *view;
	GtkTreeModel      *model;
	GtkTreeViewColumn *column;
	GtkCellRenderer   *renderer;
	guint              col_offset;

	view = GTK_TREE_VIEW (dialog->treeview);
	model = gtk_tree_view_get_model (view);

	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled",
			  G_CALLBACK (edit_contact_dialog_cell_toggled),
			  dialog);

	column = gtk_tree_view_column_new_with_attributes (_("Select"), renderer,
							   "active", COL_ENABLED,
							   NULL);

	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width (column, 50);
	gtk_tree_view_append_column (view, column);

	renderer = gtk_cell_renderer_text_new ();
	col_offset = gtk_tree_view_insert_column_with_attributes (view,
								  -1, _("Group"),
								  renderer,
								  "text", COL_NAME,
/* 								  "editable", COL_EDITABLE, */
								  NULL);

	g_object_set_data (G_OBJECT (renderer),
			   "column", GINT_TO_POINTER (COL_NAME));

	column = gtk_tree_view_get_column (view, col_offset - 1);
	gtk_tree_view_column_set_sort_column_id (column, COL_NAME);
	gtk_tree_view_column_set_resizable (column,FALSE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	if (dialog->renderer) {
		g_object_unref (dialog->renderer);
	}

	dialog->renderer = g_object_ref (renderer);
}

static void
edit_contact_dialog_model_populate_data (GossipEditContactDialog *dialog,
					 GList                   *groups)
{
	GtkTreeView  *view;
	GtkListStore *store;
	GtkTreeIter   iter;
	GList        *l;
	GList        *my_groups = NULL;
	GList        *all_groups;

	view = GTK_TREE_VIEW (dialog->treeview);
	store = GTK_LIST_STORE (gtk_tree_view_get_model (view));

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
			if (edit_contact_dialog_model_find_name (dialog, group_str, &iter)) {
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
edit_contact_dialog_model_find_name (GossipEditContactDialog *dialog,
				     const gchar             *name,
				     GtkTreeIter             *iter)
{
	GtkTreeView  *view;
	GtkTreeModel *model;
	FindName      data;

	if (G_STR_EMPTY (name)) {
		return FALSE;
	}

	data.dialog = dialog;
	data.name = name;
	data.found = FALSE;

	view = GTK_TREE_VIEW (dialog->treeview);
	model = gtk_tree_view_get_model (view);

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) edit_contact_dialog_model_find_name_foreach,
				&data);

	if (data.found == TRUE) {
		*iter = data.found_iter;
		return TRUE;
	}

	return FALSE;
}

static gboolean
edit_contact_dialog_model_find_selected_foreach (GtkTreeModel *model,
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

static gboolean
edit_contact_dialog_model_find_name_foreach (GtkTreeModel *model,
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
edit_contact_dialog_model_find_selected (GossipEditContactDialog *dialog)
{
	GtkTreeView  *view;
	GtkTreeModel *model;
	FindSelected  data;

	data.dialog = dialog;
	data.list = NULL;

	view = GTK_TREE_VIEW (dialog->treeview);
	model = gtk_tree_view_get_model (view);

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) edit_contact_dialog_model_find_selected_foreach,
				&data);

	return data.list;
}

static void
edit_contact_dialog_update_widgets (GossipContact *contact)
{
	GossipEditContactDialog *dialog;
	GossipSubscription       subscription;

	dialog = g_hash_table_lookup (dialogs, contact);

	if (!dialog) {
		return;
	}

	if (!gossip_contact_equal (contact, dialog->contact)) {
		return;
	}

	subscription = gossip_contact_get_subscription (dialog->contact);

	if (subscription == GOSSIP_SUBSCRIPTION_NONE ||
	    subscription == GOSSIP_SUBSCRIPTION_FROM) {
		gtk_widget_show_all (dialog->frame_subscription);
	} else {
		gtk_widget_hide (dialog->frame_subscription);
	}
}

static void
edit_contact_dialog_contact_updated_cb (GossipContact           *contact,
					gpointer                 user_data)
{
	edit_contact_dialog_update_widgets (contact);	
}

static void
edit_contact_dialog_cell_toggled (GtkCellRendererToggle   *cell,
				  gchar                   *path_string,
				  GossipEditContactDialog *dialog)
{
	GtkTreeView  *view;
	GtkTreeModel *model;
	GtkListStore *store;
	GtkTreePath  *path;
	GtkTreeIter   iter;

	gboolean      enabled;

	g_return_if_fail(dialog != NULL);

	view = GTK_TREE_VIEW (dialog->treeview);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);

	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, COL_ENABLED, &enabled, -1);

	enabled ^= 1;

	gtk_list_store_set (store, &iter, COL_ENABLED, enabled, -1);
	gtk_tree_path_free (path);

	dialog->changes_made = TRUE;

	gtk_widget_set_sensitive (dialog->button_ok,
				  edit_contact_dialog_can_save (dialog));
}

static void
edit_contact_dialog_entry_name_changed_cb (GtkEditable             *editable,
					   GossipEditContactDialog *dialog)
{
	const gchar *name;

	name = gtk_entry_get_text (GTK_ENTRY (dialog->entry_name));

	dialog->changes_made = TRUE;

	gtk_widget_set_sensitive (dialog->button_ok,
				  edit_contact_dialog_can_save (dialog));
}

static void
edit_contact_dialog_entry_group_changed_cb (GtkEditable             *editable,
					    GossipEditContactDialog *dialog)
{
	GtkTreeIter  iter;
	const gchar *group;

	group = gtk_entry_get_text (GTK_ENTRY (dialog->entry_group));

	if (edit_contact_dialog_model_find_name (dialog, group, &iter)) {
		gtk_widget_set_sensitive (GTK_WIDGET (dialog->button_add), FALSE);

	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (dialog->button_add),
					  !G_STR_EMPTY (group));
	}
}

static void
edit_contact_dialog_entry_group_activate_cb (GtkEntry                *entry,
					     GossipEditContactDialog *dialog)
{
	gtk_widget_activate (GTK_WIDGET (dialog->button_add));
}

static void
edit_contact_dialog_button_add_clicked_cb (GtkButton               *button,
					   GossipEditContactDialog *dialog)
{
	GtkTreeView  *view;
	GtkListStore *store;
	GtkTreeIter   iter;
	const gchar  *group;

	view = GTK_TREE_VIEW (dialog->treeview);
	store = GTK_LIST_STORE (gtk_tree_view_get_model (view));

	group = gtk_entry_get_text (GTK_ENTRY (dialog->entry_group));

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    COL_NAME, group,
			    COL_ENABLED, TRUE,
			    -1);

	dialog->changes_made = TRUE;

	gtk_widget_set_sensitive (dialog->button_ok,
				  edit_contact_dialog_can_save (dialog));
}

static void
edit_contact_dialog_button_retrieve_get_vcard_cb (GossipResult   result,
						  GossipVCard   *vcard,
						  GossipContact *contact)
{
	GossipEditContactDialog *dialog;
	const gchar             *name;

	dialog = g_hash_table_lookup (dialogs, contact);
	g_object_unref (contact);

	if (!dialog) {
		return;
	}

	gtk_widget_set_sensitive (dialog->entry_name, TRUE);
	gtk_widget_set_sensitive (dialog->button_retrieve, TRUE);

	if (result != GOSSIP_RESULT_OK) {
		return;
	}

	name = gossip_vcard_get_nickname (vcard);
	if (!G_STR_EMPTY (name)) {
		gtk_entry_set_text (GTK_ENTRY (dialog->entry_name), name);
		return;
	}

	name = gossip_vcard_get_name (vcard);
	if (!G_STR_EMPTY (name)) {
		gtk_entry_set_text (GTK_ENTRY (dialog->entry_name), name);
		return;
	}
}

static void
edit_contact_dialog_button_retrieve_clicked_cb (GtkButton               *button,
						GossipEditContactDialog *dialog)
{
	GossipSession *session;

	gtk_widget_set_sensitive (dialog->entry_name, FALSE);
	gtk_widget_set_sensitive (dialog->button_retrieve, FALSE);

	session = gossip_app_get_session ();

	gossip_session_get_vcard (session,
				  NULL,
				  dialog->contact,
				  (GossipVCardCallback)
				  edit_contact_dialog_button_retrieve_get_vcard_cb,
				  g_object_ref (dialog->contact),
				  NULL);
}

static void
edit_contact_dialog_button_subscribe_clicked_cb (GtkWidget               *widget,
						 GossipEditContactDialog *dialog)
{
	GossipSession *session;
	GossipAccount *account;
	const gchar   *message;

	message = _("I would like to add you to my contact list.");

	session = gossip_app_get_session ();
	account = gossip_contact_get_account (dialog->contact);

	gossip_session_add_contact (session,
				    account,
				    gossip_contact_get_id (dialog->contact),
				    gossip_contact_get_name (dialog->contact),
				    NULL, /* group */
				    message);

	g_object_unref (account);
}

static void
edit_contact_dialog_response_cb (GtkWidget               *widget,
				 gint                     response,
				 GossipEditContactDialog *dialog)
{
	if (dialog->changes_made && response == GTK_RESPONSE_OK) {
		edit_contact_dialog_save_groups (dialog);
		edit_contact_dialog_save_name (dialog);
	}

	gtk_widget_destroy (dialog->dialog);
}

static void
edit_contact_dialog_destroy_cb (GtkWidget               *widget,
				GossipEditContactDialog *dialog)
{
	if (dialog->renderer) {
		g_object_unref (dialog->renderer);
	}

	g_signal_handlers_disconnect_by_func (dialog->contact,
					      edit_contact_dialog_contact_updated_cb,
					      NULL);

	g_hash_table_remove (dialogs, dialog->contact);
	g_free (dialog);
}

void
gossip_edit_contact_dialog_show (GossipContact *contact,
				 GtkWindow     *parent)
{
	GossipEditContactDialog *dialog;
	GladeXML                *glade;
	GList                   *groups;
	GtkSizeGroup            *size_group;
	gchar                   *str;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	edit_contact_dialog_init ();

	dialog = g_hash_table_lookup (dialogs, contact);
	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->dialog));
		return;
	}

	dialog = g_new0 (GossipEditContactDialog, 1);

	dialog->contact = g_object_ref (contact);

	g_hash_table_insert (dialogs, dialog->contact, dialog);

	glade = gossip_glade_get_file ("main.glade",
				       "edit_contact_dialog",
				       NULL,
				       "edit_contact_dialog", &dialog->dialog,
				       "label_name", &dialog->label_name,
				       "entry_name", &dialog->entry_name,
				       "button_retrieve", &dialog->button_retrieve,
				       "entry_group", &dialog->entry_group,
				       "button_add", &dialog->button_add,
				       "treeview", &dialog->treeview,
				       "button_ok", &dialog->button_ok,
				       "frame_subscription", &dialog->frame_subscription,
				       "label_subscription", &dialog->label_subscription,
				       "button_subscribe", &dialog->button_subscribe,
				       NULL);

	gossip_glade_connect (glade,
			      dialog,
			      "edit_contact_dialog", "response", edit_contact_dialog_response_cb,
			      "edit_contact_dialog", "destroy", edit_contact_dialog_destroy_cb,
			      "entry_name", "changed", edit_contact_dialog_entry_name_changed_cb,
			      "entry_group", "changed", edit_contact_dialog_entry_group_changed_cb,
			      "entry_group", "activate", edit_contact_dialog_entry_group_activate_cb,
			      "button_add", "clicked", edit_contact_dialog_button_add_clicked_cb,
			      "button_retrieve", "clicked", edit_contact_dialog_button_retrieve_clicked_cb,
			      "button_subscribe", "clicked", edit_contact_dialog_button_subscribe_clicked_cb,
			      NULL);

	g_object_unref (glade);

	/* Set up name */
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_name),
			    gossip_contact_get_name (dialog->contact));
	gtk_editable_select_region (GTK_EDITABLE (dialog->entry_name), 0, -1);

	str = g_markup_printf_escaped (_("Set the alias you want to use for:\n"
					 "<b>%s</b>\n"
					 "\n"
					 "You can retrieve contact information from the server."),
				       gossip_contact_get_display_id (contact));
	gtk_label_set_markup (GTK_LABEL (dialog->label_name), str);
	g_free (str);

	/* Subscription listener */
	g_signal_connect (contact, "notify",
			  G_CALLBACK (edit_contact_dialog_contact_updated_cb),
			  NULL);

	/* Set up contact subscription widgets */
	edit_contact_dialog_update_widgets (contact);	

	/* Set up groups */
	groups = gossip_contact_get_groups (contact);

	edit_contact_dialog_model_setup (dialog);
	edit_contact_dialog_model_populate_data (dialog, groups);

	/* Line up buttons */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, dialog->button_retrieve);
	gtk_size_group_add_widget (size_group, dialog->button_subscribe);
	gtk_size_group_add_widget (size_group, dialog->button_add);
	g_object_unref (size_group);

	/* Set up transient parent */
	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog), parent);
	}

	gtk_widget_show (dialog->dialog);
}
