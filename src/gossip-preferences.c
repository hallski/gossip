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

#include <config.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <libgnome/gnome-i18n.h>
#include <gconf/gconf-client.h>
#include "gossip-utils.h"
#include "gossip-app.h"
#include "gossip-stock.h"
#include "gossip-preferences.h"

enum {
	COL_TYPE,
	COL_STRING,
	COL_STATE,
	NUM_COLS
};

extern GConfClient *gconf_client;

typedef struct {
	GossipApp    *app;
 	
	GtkWidget    *dialog;
	GtkWidget    *sound_checkbutton;
	GtkWidget    *silent_busy_checkbutton;
	GtkWidget    *silent_away_checkbutton;
	GtkWidget    *smileys_checkbutton;
	GtkWidget    *compact_checkbutton;
	GtkWidget    *leaving_entry;
	GtkWidget    *away_entry;
	GtkWidget    *spell_checker_checkbutton;
	
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
			      GCONF_PATH "/conversation/enable_spell_checker",
			      preferences->spell_checker_checkbutton);
}

void
gossip_preferences_show (void)
{
	static GossipPreferences *preferences;
	GladeXML                 *gui;

	if (preferences) {
		gtk_window_present (GTK_WINDOW (preferences->dialog));
		return;
	}
	
        preferences = g_new0 (GossipPreferences, 1);
	preferences->app = gossip_app_get ();

	gui = gossip_glade_get_file (GLADEDIR "/main.glade",
				     "preferences_dialog",
				     NULL,
				     "preferences_dialog", &preferences->dialog,
				     "sound_checkbutton", &preferences->sound_checkbutton,
				     "silent_busy_checkbutton", &preferences->silent_busy_checkbutton,
				     "silent_away_checkbutton", &preferences->silent_away_checkbutton,
				     "smileys_checkbutton", &preferences->smileys_checkbutton,
				     "spell_checker_checkbutton", &preferences->spell_checker_checkbutton,
				     NULL);

	g_object_add_weak_pointer (G_OBJECT (preferences->dialog),
				   (gpointer) &preferences);
	
	preferences_setup_widgets (preferences);
	
	gossip_glade_connect (gui, preferences,
			      "preferences_dialog", "destroy", preferences_destroy_cb,
			      "preferences_dialog", "response", preferences_response_cb,
			      NULL);
	
	gtk_window_set_transient_for (GTK_WINDOW (preferences->dialog), GTK_WINDOW (gossip_app_get_window ()));
	gtk_widget_show (preferences->dialog);
}


/* ------------------------------
 * Status message editor
 */

typedef struct {
	GtkWidget *dialog;
	GtkWidget *treeview;
	GtkWidget *remove_button;
	GtkWidget *add_button;
	GtkWidget *type_menu;
	GtkWidget *status_entry;
} GossipStatusEditor;

static void
status_destroy_cb (GtkWidget          *widget,
		   GossipStatusEditor *editor)
{
	g_free (editor);
}

static void
status_add_clicked_cb (GtkWidget          *button,
		       GossipStatusEditor *editor)
{
	GossipPresenceState  state;
	const gchar         *str;
	GList               *list;
	GtkListStore        *store;
	GtkTreeIter          iter;
	GdkPixbuf           *pixbuf;
	GossipStatusEntry   *entry;

	state = GPOINTER_TO_INT (gossip_option_menu_get_history (GTK_OPTION_MENU (editor->type_menu)));
	
	/*str = get_new_message (editor->dialog, show);
	if (!str) {
		return;
		}*/

	str = gtk_entry_get_text (GTK_ENTRY (editor->status_entry));
	if (strlen (str) == 0) {
		return;
	}
	
	list = gossip_utils_get_status_messages ();

	entry = g_new (GossipStatusEntry, 1);
	entry->string = g_strdup (str);
	entry->state = state;

	list = g_list_append (list, entry);

	gossip_utils_set_status_messages (list);

	pixbuf = gossip_presence_state_get_pixbuf (state);

	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (editor->treeview)));
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    COL_TYPE, pixbuf,
			    COL_STRING, str,
			    COL_STATE, state,
			    -1);

	g_object_unref (pixbuf);

	gossip_utils_free_status_messages (list);

	gtk_entry_set_text (GTK_ENTRY (editor->status_entry), "");
}

static void
status_remove_clicked_cb (GtkWidget          *button,
			  GossipStatusEditor *editor)
{
	GtkTreeSelection    *selection;
	GtkTreeModel        *model;
	GtkTreeIter          iter;
	gchar               *str;
	GList               *list, *l;
	GossipPresenceState  state;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->treeview));
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
		
		if (state == entry->state && strcmp (str, entry->string) == 0) {
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
status_response_cb (GtkWidget *widget,
		    gint       response,
		    gpointer   user_data)
{
	gtk_widget_destroy (widget);
}

static void
status_selection_changed_cb (GtkTreeSelection   *selection,
			     GossipStatusEditor *editor)
{
	if (!gtk_tree_selection_get_selected (selection, NULL, NULL)) {
		gtk_widget_set_sensitive (editor->remove_button, FALSE);
		return;
 	}
	
	gtk_widget_set_sensitive (editor->remove_button, TRUE);
}

static void
status_dnd_row_deleted_cb (GtkTreeModel *model,
			   GtkTreePath  *arg1,
			   GtkTreeIter  *arg2,
			   gpointer      user_data)
{
	GList               *list = NULL;
	GtkTreeIter          iter;
	gboolean             valid;
	gint                 row_count = 0;
	gchar               *str;
	GossipPresenceState  state;
	GossipStatusEntry   *entry;

	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		gtk_tree_model_get (model, &iter,
				    COL_STRING, &str,
				    COL_STATE, &state,
				    -1);

		entry = g_new (GossipStatusEntry, 1);
		entry->string = str;
		entry->state = state;
		
		list = g_list_append (list, entry);
		
		row_count++;
		
		valid = gtk_tree_model_iter_next (model, &iter);
	}

	gossip_utils_set_status_messages (list);
	gossip_utils_free_status_messages (list);
}

static void
status_entry_changed_cb (GtkWidget *widget, GossipStatusEditor *editor)
{
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (widget));
	gtk_widget_set_sensitive (editor->add_button, strlen (str) > 0);
}

static void
status_entry_activate_cb (GtkWidget *widget, GossipStatusEditor *editor)
{
	gtk_widget_activate (editor->add_button);
}

void
gossip_preferences_show_status_editor (void)
{
	static GossipStatusEditor *editor;
	GladeXML                  *glade;
	GList                     *list, *l;
	GtkListStore              *store;
	GtkTreeIter                iter;
	GtkCellRenderer           *cell;
	GtkTreeViewColumn         *col;
	GtkTreeSelection          *selection;

	if (editor) {
		gtk_window_present (GTK_WINDOW (editor->dialog));
		return;
	}
	
	editor = g_new0 (GossipStatusEditor, 1);
	
	glade = gossip_glade_get_file (GLADEDIR "/main.glade",
				       "status_editor_dialog",
				       NULL,
				       "status_editor_dialog", &editor->dialog,
				       "treeview", &editor->treeview,
				       "remove_button", &editor->remove_button,
				       "add_button", &editor->add_button,
				       "optionmenu", &editor->type_menu,
				       "entry", &editor->status_entry,
				       NULL);

	gossip_glade_connect (glade,
			      editor,
			      "status_editor_dialog", "response", status_response_cb,
			      "status_editor_dialog", "destroy", status_destroy_cb,
			      "add_button", "clicked", status_add_clicked_cb,
			      "remove_button", "clicked", status_remove_clicked_cb,
			      "entry", "changed", status_entry_changed_cb,
			      "entry", "activate", status_entry_activate_cb,
			      NULL);

	g_object_add_weak_pointer (G_OBJECT (editor->dialog), (gpointer) &editor);

	gossip_option_image_menu_setup (editor->type_menu,
					NULL,
					NULL,
					_("Available"), GOSSIP_STOCK_AVAILABLE, GOSSIP_PRESENCE_STATE_AVAILABLE,
					_("Busy"), GOSSIP_STOCK_BUSY, GOSSIP_PRESENCE_STATE_BUSY,
					_("Away"), GOSSIP_STOCK_AWAY, GOSSIP_PRESENCE_STATE_AWAY,
					NULL);
	
	store = gtk_list_store_new (NUM_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_INT);
	gtk_tree_view_set_model (GTK_TREE_VIEW (editor->treeview),
				 GTK_TREE_MODEL (store));
	
	col = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_add_attribute (col, cell, "pixbuf", COL_TYPE);
	
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	gtk_tree_view_column_add_attribute (col, cell, "text", COL_STRING);
	
	gtk_tree_view_append_column (GTK_TREE_VIEW (editor->treeview), col);

	/* Fill in status messages. */
	list = gossip_utils_get_status_messages ();
	for (l = list; l; l = l->next) {
		GossipStatusEntry *entry;
		GdkPixbuf         *pixbuf;

		entry = l->data;

		pixbuf = gossip_presence_state_get_pixbuf (entry->state);
		
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_TYPE, pixbuf,
				    COL_STRING, entry->string,
				    COL_STATE, entry->state,
				    -1);

		g_object_unref (pixbuf);
	}

	gossip_utils_free_status_messages (list);
	
	g_signal_connect (GTK_TREE_MODEL (store),
			  "row_deleted",
			  G_CALLBACK (status_dnd_row_deleted_cb),
			  NULL);
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (status_selection_changed_cb),
			  editor);
	
	gtk_widget_set_sensitive (editor->remove_button, FALSE);

	gtk_widget_show (editor->dialog);
}
