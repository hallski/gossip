/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
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

#include "gossip-chat.h"
#include "gossip-glade.h"
#include "gossip-spell-dialog.h"
#include "gossip-ui-utils.h"

typedef struct {
	GtkWidget   *window;

	GtkWidget   *vbox_language;
	GtkWidget   *combobox_language;

	GtkWidget   *label_word;
	GtkWidget   *treeview_words;

	GtkWidget   *button_replace;

	GossipChat  *chat;

	gchar       *word;
	GtkTextIter  start;
	GtkTextIter  end;
} GossipSpellDialog;

enum {
	COL_SPELL_WORD,
	COL_SPELL_COUNT
};

enum {
	COL_LANG_CODE,
	COL_LANG_NAME,
	COL_LANG_COUNT
};

static void
model_populate_languages (GossipSpellDialog *dialog)
{
	GtkComboBox  *combobox;
	GtkTreeModel *model;
	GtkListStore *store;
	GtkTreeIter   iter;
	GList        *codes, *l;

 	/* Words treeview */
	combobox = GTK_COMBO_BOX (dialog->combobox_language);

	model = gtk_combo_box_get_model (combobox);
	store = GTK_LIST_STORE (model);

	codes = gossip_spell_get_language_codes ();
	for (l = codes; l; l = l->next) {
		const gchar *code;
		const gchar *name;

		code = l->data;
		name = gossip_spell_get_language_name (code);
		if (!name) {
			continue;
		}

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_LANG_CODE, code,
				    COL_LANG_NAME, name,
				    -1);
	}
	
	/* Only show the combo if we have enough languages */
	if (g_list_length (codes) != 1) {
		gtk_widget_show (dialog->vbox_language);
	} else {
		gtk_widget_hide (dialog->vbox_language);
	}

	g_list_foreach (codes, (GFunc) g_free, NULL);
	g_list_free (codes);

	/* Insert the 'All' option and separator. */
	gtk_list_store_prepend (store, &iter);
	gtk_list_store_set (store, &iter,
			    COL_LANG_CODE, "-",
			    COL_LANG_NAME, _("All"),
			    -1);

	gtk_list_store_prepend (store, &iter);
	gtk_list_store_set (store, &iter,
			    COL_LANG_CODE, NULL,
			    COL_LANG_NAME, _("All"),
			    -1);

	/* Select 'ALL' by default */
	gtk_combo_box_set_active_iter (combobox, &iter);
}

static void
model_populate_suggestions (GossipSpellDialog *dialog)
{
	GtkTreeModel *model;
	GtkListStore *store;
	GtkTreeIter   iter;
	GtkComboBox  *combobox;
	GList        *suggestions, *l;
	gchar        *code;

	/* Get combobox model */
	combobox = GTK_COMBO_BOX (dialog->combobox_language);
	model = gtk_combo_box_get_model (combobox);

	if (!gtk_combo_box_get_active_iter (combobox, &iter)) {
		return;
	}

	/* Get current code */
	gtk_tree_model_get (model, &iter, COL_LANG_CODE, &code, -1);

	/* Get list model */ 
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->treeview_words));
	store = GTK_LIST_STORE (model);

	/* Clear list */
	gtk_list_store_clear (store);
	
	suggestions = gossip_spell_get_suggestions (dialog->word, code);
	for (l = suggestions; l; l = l->next) {
		const gchar *word;

		word = l->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_SPELL_WORD, word,
				    -1);
	}

	g_free (code);
	g_list_foreach (suggestions, (GFunc) g_free, NULL);
	g_list_free (suggestions);
	
	/* Select first by default */
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		GtkTreeView      *view;
		GtkTreeSelection *selection;

		view = GTK_TREE_VIEW (dialog->treeview_words);
		selection = gtk_tree_view_get_selection (view);
		gtk_tree_selection_select_iter (selection, &iter);
	}
}

static void
dialog_response_cb (GtkWidget         *widget,
		    gint               response,
		    GossipSpellDialog *dialog)
{
	if (response == GTK_RESPONSE_OK) {
		GtkTreeView      *view;
		GtkTreeModel     *model;
		GtkTreeSelection *selection;
		GtkTreeIter       iter;

		gchar            *new_word;

		view = GTK_TREE_VIEW (dialog->treeview_words);
		selection = gtk_tree_view_get_selection (view);

		if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
			return;
		}

		gtk_tree_model_get (model, &iter, COL_SPELL_WORD, &new_word, -1);

		gossip_chat_correct_word (dialog->chat,
					  dialog->start,
					  dialog->end,
					  new_word);

		g_free (new_word);
	}

	gtk_widget_destroy (dialog->window);
}

static void
model_row_activated_cb (GtkTreeView       *tree_view,
			GtkTreePath       *path,
			GtkTreeViewColumn *column,
			GossipSpellDialog *dialog)
{
	dialog_response_cb (dialog->window, GTK_RESPONSE_OK, dialog);
}

static void
model_selection_changed_cb (GtkTreeSelection  *treeselection,
			    GossipSpellDialog *dialog)
{
	gint count;

	count = gtk_tree_selection_count_selected_rows (treeselection);
	gtk_widget_set_sensitive (dialog->button_replace, (count == 1));
}

static void
combobox_selection_changed_cb (GtkComboBox       *combobox,
			       GossipSpellDialog *dialog)
{
	model_populate_suggestions (dialog);
}

static gboolean
combobox_separator_func (GtkTreeModel *model,
			  GtkTreeIter  *iter,
			  gpointer      user_data)
{
	gchar    *code;
	gboolean  is_separator;

	gtk_tree_model_get (model, iter, COL_LANG_CODE, &code, -1);
	is_separator = code && strcmp (code, "-") == 0;
	g_free (code);

	return is_separator;
}

static void
model_setup (GossipSpellDialog *dialog)
{
	GtkTreeView       *view;
	GtkTreeViewColumn *column;
	GtkTreeModel      *model;
	GtkListStore      *store;
	GtkTreeSelection  *selection;
	GtkCellRenderer   *renderer;
	GtkComboBox       *combobox;
	guint              col_offset;

	view = GTK_TREE_VIEW (dialog->treeview_words);

	g_signal_connect (view, "row-activated",
			  G_CALLBACK (model_row_activated_cb),
			  dialog);

	store = gtk_list_store_new (COL_SPELL_COUNT,
				    G_TYPE_STRING);   /* word */

	gtk_tree_view_set_model (view, GTK_TREE_MODEL (store));
	g_object_unref (store);

	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect (selection, "changed",
			  G_CALLBACK (model_selection_changed_cb),
			  dialog);

	model = gtk_tree_view_get_model (view);

	renderer = gtk_cell_renderer_text_new ();
	col_offset = gtk_tree_view_insert_column_with_attributes (view,
								  -1, _("Word"),
								  renderer,
								  "text", COL_SPELL_WORD,
								  NULL);

	g_object_set_data (G_OBJECT (renderer), "column", 
			   GINT_TO_POINTER (COL_SPELL_WORD));

	column = gtk_tree_view_get_column (view, col_offset - 1);
	gtk_tree_view_column_set_sort_column_id (column, COL_SPELL_WORD);
	gtk_tree_view_column_set_resizable (column, FALSE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	/* Languages combobox */
	combobox = GTK_COMBO_BOX (dialog->combobox_language);

	store = gtk_list_store_new (COL_LANG_COUNT,
				    G_TYPE_STRING,    /* code */
				    G_TYPE_STRING);   /* language */

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combobox));

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
					"text", COL_LANG_NAME,
					NULL);

	gtk_combo_box_set_model (combobox, GTK_TREE_MODEL (store));
	g_object_unref (store);

	g_signal_connect (combobox, "changed",
			  G_CALLBACK (combobox_selection_changed_cb),
			  dialog);

	gtk_combo_box_set_row_separator_func (combobox, 
					      combobox_separator_func,
					      NULL, 
					      NULL);
}

static void
dialog_destroy_cb (GtkWidget         *widget,
		   GossipSpellDialog *dialog)
{
	g_object_unref (dialog->chat);
	g_free (dialog->word);

	g_free (dialog);
}

void
gossip_spell_dialog_show (GossipChat  *chat,
			  GtkTextIter  start,
			  GtkTextIter  end,
			  const gchar *word)
{
	GossipSpellDialog *dialog;
	GladeXML          *gui;
	gchar             *str;

	g_return_if_fail (chat != NULL);
	g_return_if_fail (word != NULL);

	dialog = g_new0 (GossipSpellDialog, 1);

	dialog->chat = g_object_ref (chat);

	dialog->word = g_strdup (word);

	dialog->start = start;
	dialog->end = end;

	gui = gossip_glade_get_file ("main.glade",
				     "spell_dialog",
				     NULL,
				     "spell_dialog", &dialog->window,
				     "vbox_language", &dialog->vbox_language,
				     "combobox_language", &dialog->combobox_language,
				     "label_word", &dialog->label_word,
				     "treeview_words", &dialog->treeview_words,
				     "button_replace", &dialog->button_replace,
				     NULL);

	gossip_glade_connect (gui,
			      dialog,
			      "spell_dialog", "response", dialog_response_cb,
			      "spell_dialog", "destroy", dialog_destroy_cb,
			      NULL);

	g_object_unref (gui);

	str = g_strdup_printf ("%s: <b>%s</b>",
			       _("Suggestions for the word"),
			       word);

	gtk_label_set_markup (GTK_LABEL (dialog->label_word), str);
	g_free (str);

	model_setup (dialog);
	model_populate_languages (dialog);
	model_populate_suggestions (dialog);

	gtk_widget_show (dialog->window);
}
