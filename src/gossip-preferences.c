/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Richard Hult <richard@imendio.com>
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
#include <libgnome/gnome-i18n.h>
#include <gconf/gconf-client.h>
#include "gossip-utils.h"
#include "gossip-app.h"
#include "gossip-preferences.h"

extern GConfClient *gconf_client;

typedef struct {
	GossipApp    *app;
 	
	GtkWidget    *dialog;
	GtkWidget    *sound_checkbutton;
	GtkWidget    *silent_busy_checkbutton;
	GtkWidget    *silent_away_checkbutton;
	GtkWidget    *smileys_checkbutton;
	GtkWidget    *list_checkbutton;
	GtkWidget    *compact_checkbutton;
	GtkWidget    *leaving_entry;
	GtkWidget    *away_entry;
	
	GList        *ids;
} GossipPreferences;

static void preferences_destroy_cb           (GtkWidget         *widget,
					      GossipPreferences *preferences);
static void preferences_response_cb          (GtkWidget         *widget,
					      gint               response,
					      GossipPreferences *preferences);

static void
preferences_destroy_cb (GtkWidget         *widget,
			GossipPreferences *preferences)
{
	GList *l;

	for (l = preferences->ids; l; l = l->next) {
		guint id = GPOINTER_TO_UINT (l->data);

		gconf_client_notify_remove (gconf_client, id);
	}

	g_list_free (preferences->ids);
	g_free (preferences);
}

static void
preferences_response_cb (GtkWidget         *widget,
			 gint               response,
			 GossipPreferences *preferences)
{
	gtk_widget_destroy (widget);
}

static void
set_boolean_from_gconf (const gchar *key, GtkWidget *widget)
{
	gboolean value;
	
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));

	value = gconf_client_get_bool (gconf_client, key, NULL);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);
}

static void
set_int_from_gconf (const gchar *key, GtkWidget *widget)
{
	gint value;
	
	g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));

	value = gconf_client_get_int (gconf_client, key, NULL);
	
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);
}

static void
set_string_from_gconf (const gchar *key, GtkWidget *widget)
{
	gchar *value;
	
	g_return_if_fail (GTK_IS_ENTRY (widget));

	value = gconf_client_get_string (gconf_client, key, NULL);
	
	gtk_entry_set_text (GTK_ENTRY (widget), value);

	g_free (value);
}

static void
spin_button_value_changed_cb (GtkWidget *button,
			      gpointer   user_data)
{
	const gchar *key;

	key = g_object_get_data (G_OBJECT (button), "key");
	
	gconf_client_set_int (gconf_client,
			      key,
			      gtk_spin_button_get_value (GTK_SPIN_BUTTON (button)),
			      NULL);
}

static void
int_notify_func (GConfClient *client,
		 guint        cnxn_id,
		 GConfEntry  *entry,
		 gpointer     user_data)
{
	GConfValue *value;
	
	value = gconf_entry_get_value (entry);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (user_data),
				   gconf_value_get_int (value));
}

static void
hookup_spin_button (GossipPreferences *preferences,
		    const gchar       *key,
		    GtkWidget         *widget)
{
	guint id;

	g_return_if_fail (GTK_IS_SPIN_BUTTON (widget));

	/* Silence warning. */
	if (0) {
		hookup_spin_button (preferences, key, widget);
	}
	
	set_int_from_gconf (key, widget);

	g_object_set_data_full (G_OBJECT (widget), "key",
				g_strdup (key), g_free); 
	
	g_signal_connect (widget,
			  "value_changed",
			  G_CALLBACK (spin_button_value_changed_cb),
			  NULL);

	id = gconf_client_notify_add (gconf_client,
				      key,
				      int_notify_func,
				      widget,
				      NULL,
				      NULL);
	
	preferences->ids = g_list_prepend (preferences->ids,
					   GUINT_TO_POINTER (id));
}

static void
entry_value_changed_cb (GtkWidget *entry,
			gpointer   user_data)
{
	const gchar *key;

	key = g_object_get_data (G_OBJECT (entry), "key");
	
	gconf_client_set_string (gconf_client,
				 key,
				 gtk_entry_get_text (GTK_ENTRY (entry)),
				 NULL);
}

static void
string_notify_func (GConfClient *client,
		    guint        cnxn_id,
		    GConfEntry  *entry,
		    gpointer     user_data)
{
	GConfValue *value;
	
	value = gconf_entry_get_value (entry);

	gtk_entry_set_text (GTK_ENTRY (user_data),
			    gconf_value_get_string (value));
}

static void
hookup_entry (GossipPreferences *preferences,
	      const gchar       *key,
	      GtkWidget         *widget)
{
	guint id;

	g_return_if_fail (GTK_ENTRY (widget));

	if (0) {  /* Silent warning before we use this function. */
		hookup_entry (preferences, key, widget);
	}
	
	set_string_from_gconf (key, widget);

	g_object_set_data_full (G_OBJECT (widget), "key",
				g_strdup (key), g_free); 
	
	g_signal_connect (widget,
			  "changed",
			  G_CALLBACK (entry_value_changed_cb),
			  NULL);

	id = gconf_client_notify_add (gconf_client,
				      key,
				      string_notify_func,
				      widget,
				      NULL,
				      NULL);
	
	preferences->ids = g_list_prepend (preferences->ids,
					   GUINT_TO_POINTER (id));
}

static void
toggle_button_toggled_cb (GtkWidget *button,
			  gpointer   user_data)
{
	const gchar *key;

	key = g_object_get_data (G_OBJECT (button), "key");

	gconf_client_set_bool (gconf_client,
			       key,
			       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)),
			       NULL);
}

static void
bool_notify_func (GConfClient *client,
		  guint        cnxn_id,
		  GConfEntry  *entry,
		  gpointer     user_data)
{
	GConfValue *value;
	
	value = gconf_entry_get_value (entry);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (user_data),
				      gconf_value_get_bool (value));
}

static void
hookup_toggle_button (GossipPreferences *preferences,
		      const gchar       *key,
		      GtkWidget         *widget)
{
	guint id;
	
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));

	set_boolean_from_gconf (key, widget);
	
	g_object_set_data_full (G_OBJECT (widget), "key",
				g_strdup (key), g_free); 
	
	g_signal_connect (widget,
			  "toggled",
			  G_CALLBACK (toggle_button_toggled_cb),
			  NULL);

	id = gconf_client_notify_add (gconf_client,
				      key,
				      bool_notify_func,
				      widget,
				      NULL,
				      NULL);
	
	preferences->ids = g_list_prepend (preferences->ids,
					   GUINT_TO_POINTER (id));
}

static void
sensitivity_func (GConfClient *client,
		  guint        cnxn_id,
		  GConfEntry  *entry,
		  gpointer     user_data)
{
	GConfValue *value;
	
	value = gconf_entry_get_value (entry);

	gtk_widget_set_sensitive (GTK_WIDGET (user_data),
				  gconf_value_get_bool (value));
}

static void
hookup_sensitivity_controller (GossipPreferences *preferences,
			       const gchar       *key,
			       GtkWidget         *widget)
{
	gboolean value;
	guint    id;

	value = gconf_client_get_bool (gconf_client, key, NULL);
	gtk_widget_set_sensitive (widget, value);
	
	id = gconf_client_notify_add (gconf_client,
				      key,
				      sensitivity_func,
				      widget,
				      NULL,
				      NULL);

	preferences->ids = g_list_prepend (preferences->ids,
					   GUINT_TO_POINTER (id));
}

static void
preferences_setup_widgets (GossipPreferences *preferences)
{
	hookup_toggle_button (preferences,
			      GCONF_PATH "/sound/play_sounds",
			      preferences->sound_checkbutton);
	hookup_toggle_button (preferences,
			      GCONF_PATH "/sound/silent_away",
			      preferences->silent_away_checkbutton);
	hookup_toggle_button (preferences,
			      GCONF_PATH "/sound/silent_busy",
			      preferences->silent_busy_checkbutton);
	hookup_sensitivity_controller (preferences,
				       GCONF_PATH "/sound/play_sounds",
				       preferences->silent_away_checkbutton);
	hookup_sensitivity_controller (preferences,
				       GCONF_PATH "/sound/play_sounds",
				       preferences->silent_busy_checkbutton);

	hookup_toggle_button (preferences,
			      GCONF_PATH "/conversation/graphical_smileys",
			      preferences->smileys_checkbutton);
	hookup_toggle_button (preferences,
			      GCONF_PATH "/conversation/open_in_list",
			      preferences->list_checkbutton);
}

GtkWidget *
gossip_preferences_show (GossipApp *app)
{
	GossipPreferences *preferences;
	GladeXML          *gui;
	
        preferences = g_new0 (GossipPreferences, 1);
	preferences->app = app;

	gui = gossip_glade_get_file (GLADEDIR "/main.glade",
				     "preferences_dialog",
				     NULL,
				     "preferences_dialog", &preferences->dialog,
				     "sound_checkbutton", &preferences->sound_checkbutton,
				     "silent_busy_checkbutton", &preferences->silent_busy_checkbutton,
				     "silent_away_checkbutton", &preferences->silent_away_checkbutton,
				     "smileys_checkbutton", &preferences->smileys_checkbutton,
				     "list_checkbutton", &preferences->list_checkbutton,
				     NULL);

	preferences_setup_widgets (preferences);
	
	gossip_glade_connect (gui, preferences,
			      "preferences_dialog", "destroy", preferences_destroy_cb,
			      "preferences_dialog", "response", preferences_response_cb,
			      NULL);
	
	gtk_window_set_transient_for (GTK_WINDOW (preferences->dialog), GTK_WINDOW (gossip_app_get_window ()));
	gtk_widget_show (preferences->dialog);

 	return preferences->dialog;
}


/* ------------------------------
 * Status message editor
 */

typedef struct {
	GtkWidget    *dialog;

	GtkWidget    *busy_treeview;
	GtkWidget    *away_treeview;

	GtkWidget    *remove_busy_button;
	GtkWidget    *remove_away_button;
} GossipStatusEditor;

static gchar *
get_new_message (GtkWidget *parent, const gchar *title)
{
	GtkWidget   *dialog;
	GtkWidget   *entry;
	gint         response;
	gchar       *str;

	gossip_glade_get_file_simple (GLADEDIR "/main.glade",
				      "add_status_message_dialog",
				      NULL,
				      "add_status_message_dialog", &dialog,
				      "status_entry", &entry,
				      NULL);

	gtk_window_set_title (GTK_WINDOW (dialog), title);
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return NULL;
	}

	str = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
	gtk_widget_destroy (dialog);
	
	if (strlen (str) == 0) {
		g_free (str);
		return NULL;
	}

	return str;
}

static void
status_destroy_cb (GtkWidget          *widget,
		   GossipStatusEditor *editor)
{
	g_free (editor);
}

static void
status_add_busy_clicked_cb (GtkWidget          *button,
			    GossipStatusEditor *editor)
{
	gchar        *str;
	GSList       *list;
	GtkListStore *store;
	GtkTreeIter   iter;
	
	str = get_new_message (editor->dialog, _("New Busy Message"));
	if (!str) {
		return;
	}

	list = gossip_utils_get_busy_messages ();

	list = g_slist_append (list, g_strdup (str));
	
	gconf_client_set_list (gconf_client,
			       "/apps/gossip/status/custom_busy_messages",
			       GCONF_VALUE_STRING,
			       list,
			       NULL);

	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (editor->busy_treeview)));
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    0, str,
			    -1);

	g_slist_foreach (list, (GFunc)g_free, NULL);
	g_slist_free (list);
}

static void
status_add_away_clicked_cb (GtkWidget          *button,
			    GossipStatusEditor *editor)
{
	gchar        *str;
	GSList       *list;
	GtkListStore *store;
	GtkTreeIter   iter;

	str = get_new_message (editor->dialog, _("New Away Message"));

	list = gossip_utils_get_away_messages ();

	list = g_slist_append (list, g_strdup (str));
	
	gconf_client_set_list (gconf_client,
			       "/apps/gossip/status/custom_away_messages",
			       GCONF_VALUE_STRING,
			       list,
			       NULL);

	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (editor->away_treeview)));
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    0, str,
			    -1);
	
	g_slist_foreach (list, (GFunc)g_free, NULL);
	g_slist_free (list);
}

static void
status_remove_busy_clicked_cb (GtkWidget          *button,
			       GossipStatusEditor *editor)
{
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	gchar            *str;
	GSList           *list, *l;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->busy_treeview));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return;
 	}

	gtk_tree_model_get (model, &iter,
			    0, &str,
			    -1);

	list = gossip_utils_get_busy_messages ();

	for (l = list; l; l = l->next) {
		if (strcmp (str, l->data) == 0) {
			break;
		}
	}

	if (l) {
		g_free (l->data);
		list = g_slist_delete_link (list, l);
	}
	
	gconf_client_set_list (gconf_client,
			       "/apps/gossip/status/custom_busy_messages",
			       GCONF_VALUE_STRING,
			       list,
			       NULL);

	g_slist_foreach (list, (GFunc)g_free, NULL);
	g_slist_free (list);

	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static void
status_remove_away_clicked_cb (GtkWidget          *button,
			       GossipStatusEditor *editor)
{
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	gchar            *str;
	GSList           *list, *l;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->away_treeview));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return;
 	}

	gtk_tree_model_get (model, &iter,
			    0, &str,
			    -1);

	list = gossip_utils_get_away_messages ();

	for (l = list; l; l = l->next) {
		if (strcmp (str, l->data) == 0) {
			break;
		}
	}

	if (l) {
		g_free (l->data);
		list = g_slist_delete_link (list, l);
	}
	
	gconf_client_set_list (gconf_client,
			       "/apps/gossip/status/custom_away_messages",
			       GCONF_VALUE_STRING,
			       list,
			       NULL);

	g_slist_foreach (list, (GFunc)g_free, NULL);
	g_slist_free (list);

	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static void
status_response_cb (GtkWidget *widget,
		    gint       response,
		    gpointer user_data)
{
	gtk_widget_destroy (widget);
}

static void
status_busy_selection_changed_cb (GtkTreeSelection   *selection,
				  GossipStatusEditor *editor)
{
	if (!gtk_tree_selection_get_selected (selection, NULL, NULL)) {
		gtk_widget_set_sensitive (editor->remove_busy_button, FALSE);
		return;
 	}
	
	gtk_widget_set_sensitive (editor->remove_busy_button, TRUE);
}

static void
status_away_selection_changed_cb (GtkTreeSelection   *selection,
				  GossipStatusEditor *editor)
{
	if (!gtk_tree_selection_get_selected (selection, NULL, NULL)) {
		gtk_widget_set_sensitive (editor->remove_away_button, FALSE);
		return;
 	}
	
	gtk_widget_set_sensitive (editor->remove_away_button, TRUE);
}

GtkWidget *
gossip_preferences_show_status_editor (void)
{
	GossipStatusEditor *editor;
	GladeXML           *glade;
	GSList             *list, *l;
	GtkListStore       *store;
	GtkTreeIter         iter;
	GtkCellRenderer    *cell;
	GtkTreeViewColumn  *col;
	GtkTreeSelection   *selection;

	editor = g_new0 (GossipStatusEditor, 1);
	
	glade = gossip_glade_get_file (GLADEDIR "/main.glade",
				       "status_editor_dialog",
				       NULL,
				       "status_editor_dialog", &editor->dialog,
				       "busy_treeview", &editor->busy_treeview,
				       "away_treeview", &editor->away_treeview,
				       "busy_remove_button", &editor->remove_busy_button,
				       "away_remove_button", &editor->remove_away_button,
				       NULL);

	gossip_glade_connect (glade,
			      editor,
			      "status_editor_dialog", "response", status_response_cb,
			      "status_editor_dialog", "destroy", status_destroy_cb,
			      "busy_add_button", "clicked", status_add_busy_clicked_cb,
			      "away_add_button", "clicked", status_add_away_clicked_cb,
			      "busy_remove_button", "clicked", status_remove_busy_clicked_cb,
			      "away_remove_button", "clicked", status_remove_away_clicked_cb,
			      NULL);

	/* Fill in busy messages. */
	list = gossip_utils_get_busy_messages ();

	store = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_tree_view_set_model (GTK_TREE_VIEW (editor->busy_treeview),
				 GTK_TREE_MODEL (store));

	col = gtk_tree_view_column_new ();
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	gtk_tree_view_column_add_attribute (col, cell,  "text", 0);
	
	gtk_tree_view_append_column (GTK_TREE_VIEW (editor->busy_treeview), col);
	
	for (l = list; l; l = l->next) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    0, l->data,
				    -1);
	}

	g_slist_free (list);
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->busy_treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (status_busy_selection_changed_cb),
			  editor);
	
	gtk_widget_set_sensitive (editor->remove_busy_button, FALSE);

	/* Fill in away messages. */
	list = gossip_utils_get_away_messages ();

	store = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_tree_view_set_model (GTK_TREE_VIEW (editor->away_treeview),
				 GTK_TREE_MODEL (store));
	
	col = gtk_tree_view_column_new ();
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	gtk_tree_view_column_add_attribute (col, cell,  "text", 0);
	
	gtk_tree_view_append_column (GTK_TREE_VIEW (editor->away_treeview), col);
	
	for (l = list; l; l = l->next) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    0, l->data,
				    -1);
	}
	
	g_slist_free (list);
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->away_treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (status_away_selection_changed_cb),
			  editor);
	
	gtk_widget_set_sensitive (editor->remove_away_button, FALSE);

	gtk_widget_show (editor->dialog);
	return editor->dialog;
}
