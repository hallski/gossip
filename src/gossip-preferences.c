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
#include <glib/gi18n.h>

#include <libgossip/gossip-utils.h>

#include "gossip-app.h"
#include "gossip-stock.h"
#include "gossip-ui-utils.h"
#include "gossip-preferences.h"
#include "gossip-spell.h"

typedef struct {
	GossipApp *app;
 	
	GtkWidget *dialog;
	GtkWidget *notebook;
	GtkWidget *sound_checkbutton;
	GtkWidget *silent_busy_checkbutton;
	GtkWidget *silent_away_checkbutton;
	GtkWidget *smileys_checkbutton;
        GtkWidget *separate_windows_checkbutton;
	GtkWidget *compact_checkbutton;
	GtkWidget *leaving_entry;
	GtkWidget *away_entry;
	GtkWidget *spell_checker_vbox;
	GtkWidget *spell_checker_checkbutton;
	GtkWidget *spell_checker_treeview;

	GtkWidget *theme_combobox;
	
	GList     *ids;
} GossipPreferences;

enum {
	COL_LANG_ENABLED,
	COL_LANG_CODE,
	COL_LANG_NAME,
	COL_LANG_COUNT
};

enum {
	COL_COMBO_VISIBLE_NAME,
	COL_COMBO_NAME,
	COL_COMBO_COUNT
};

static void     preferences_setup_widgets                (GossipPreferences      *preferences);
static void     preferences_languages_setup              (GossipPreferences      *preferences);
static void     preferences_languages_add                (GossipPreferences      *preferences);
static void     preferences_languages_save               (GossipPreferences      *preferences);
static gboolean preferences_languages_save_foreach       (GtkTreeModel           *model,
							  GtkTreePath            *path,
							  GtkTreeIter            *iter,
							  gchar                 **languages);
static void     preferences_languages_load               (GossipPreferences      *preferences);
static gboolean preferences_languages_load_foreach       (GtkTreeModel           *model,
							  GtkTreePath            *path,
							  GtkTreeIter            *iter,
							  gchar                 **languages);
static void     preferences_languages_cell_toggled_cb    (GtkCellRendererToggle  *cell,
							  gchar                  *path_string,
							  GossipPreferences      *preferences);
static void     preferences_themes_setup                 (GossipPreferences      *preferences);
static void     preferences_set_boolean_from_gconf       (const gchar            *key,
							  GtkWidget              *widget);
static void     preferences_set_int_from_gconf           (const gchar            *key,
							  GtkWidget              *widget);
static void     preferences_set_string_from_gconf        (const gchar            *key,
							  GtkWidget              *widget);
static void     preferences_set_string_combo_from_gconf  (const gchar            *key,
							  GtkWidget              *widget);
static void     preferences_notify_int_cb                (GConfClient            *client,
							  guint                   cnxn_id,
							  GConfEntry             *entry,
							  gpointer                user_data);
static void     preferences_notify_string_cb             (GConfClient            *client,
							  guint                   cnxn_id,
							  GConfEntry             *entry,
							  gpointer                user_data);
static void     preferences_notify_string_combo_cb       (GConfClient            *client,
							  guint                   cnxn_id,
							  GConfEntry             *entry,
							  gpointer                user_data);
static void     preferences_notify_bool_cb               (GConfClient            *client,
							  guint                   cnxn_id,
							  GConfEntry             *entry,
							  gpointer                user_data);
static void     preferences_notify_sensitivity_cb        (GConfClient            *client,
							  guint                   cnxn_id,
							  GConfEntry             *entry,
							  gpointer                user_data);
static void     preferences_hookup_spin_button           (GossipPreferences      *preferences,
							  const gchar            *key,
							  GtkWidget              *widget);
static void     preferences_hookup_entry                 (GossipPreferences      *preferences,
							  const gchar            *key,
							  GtkWidget              *widget);
static void     preferences_hookup_toggle_button         (GossipPreferences      *preferences,
							  const gchar            *key,
							  GtkWidget              *widget);
static void     preferences_hookup_string_combo          (GossipPreferences      *preferences,
							  const gchar            *key,
							  GtkWidget              *widget);
static void     preferences_hookup_sensitivity           (GossipPreferences      *preferences,
							  const gchar            *key,
							  GtkWidget              *widget);
static void     preferences_spin_button_value_changed_cb (GtkWidget              *button,
							  gpointer                user_data);
static void     preferences_entry_value_changed_cb       (GtkWidget              *entry,
							  gpointer                user_data);
static void     preferences_toggle_button_toggled_cb     (GtkWidget              *button,
							  gpointer                user_data);
static void     preferences_string_combo_changed_cb      (GtkWidget *button,
							  gpointer                user_data);
static void     preferences_destroy_cb                   (GtkWidget              *widget,
							  GossipPreferences      *preferences);
static void     preferences_response_cb                  (GtkWidget              *widget,
							  gint                    response,
							  GossipPreferences      *preferences);

static void
preferences_setup_widgets (GossipPreferences *preferences)
{
	preferences_hookup_toggle_button (preferences,
					  GCONF_PATH "/sound/play_sounds",
					  preferences->sound_checkbutton);
	preferences_hookup_toggle_button (preferences,
					  GCONF_PATH "/sound/silent_away",
					  preferences->silent_away_checkbutton);
	preferences_hookup_toggle_button (preferences,
					  GCONF_PATH "/sound/silent_busy",
					  preferences->silent_busy_checkbutton);
	preferences_hookup_sensitivity (preferences,
					GCONF_PATH "/sound/play_sounds",
					preferences->silent_away_checkbutton);
	preferences_hookup_sensitivity (preferences,
					GCONF_PATH "/sound/play_sounds",
					preferences->silent_busy_checkbutton);

        preferences_hookup_toggle_button (preferences,
					  GCONF_PATH "/conversation/graphical_smileys",
					  preferences->smileys_checkbutton);

        preferences_hookup_toggle_button (preferences,
					  GCONF_PATH "/ui/separate_chat_windows",
					  preferences->separate_windows_checkbutton);

	preferences_hookup_toggle_button (preferences,
					  GCONF_PATH "/conversation/enable_spell_checker",
					  preferences->spell_checker_checkbutton);

	preferences_hookup_string_combo (preferences,
					 GCONF_PATH "/conversation/theme",
					 preferences->theme_combobox);
}

static void 
preferences_languages_setup (GossipPreferences *preferences)
{
	GtkTreeView       *view;
	GtkListStore      *store;
	GtkTreeSelection  *selection;
	GtkTreeModel      *model;
	GtkTreeViewColumn *column; 
	GtkCellRenderer   *renderer;
	guint              col_offset;

	view = GTK_TREE_VIEW (preferences->spell_checker_treeview);

	store = gtk_list_store_new (COL_LANG_COUNT,
				    G_TYPE_BOOLEAN,  /* enabled */
				    G_TYPE_STRING,   /* code */
				    G_TYPE_STRING);  /* name */
	
	gtk_tree_view_set_model (view, GTK_TREE_MODEL (store));

	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	
	model = GTK_TREE_MODEL (store);
	
	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled",  
 			  G_CALLBACK (preferences_languages_cell_toggled_cb),  
 			  preferences);
	
	column = gtk_tree_view_column_new_with_attributes (NULL, renderer,
							   "active", COL_LANG_ENABLED,
							   NULL);
	
	gtk_tree_view_append_column (view, column);
	
	renderer = gtk_cell_renderer_text_new ();
	col_offset = gtk_tree_view_insert_column_with_attributes (view,
								  -1, _("Language"),
								  renderer, 
								  "text", COL_LANG_NAME,
								  NULL);
	
	g_object_set_data (G_OBJECT (renderer),
			   "column", GINT_TO_POINTER (COL_LANG_NAME));
	
	column = gtk_tree_view_get_column (view, col_offset - 1);
	gtk_tree_view_column_set_sort_column_id (column, COL_LANG_NAME);
	gtk_tree_view_column_set_resizable (column, FALSE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	g_object_unref (store);
}

static void 
preferences_languages_add (GossipPreferences *preferences)
{
	GtkTreeView  *view;
	GtkListStore *store;
	GossipSpell  *spell;
	GList        *codes, *l;

	view = GTK_TREE_VIEW (preferences->spell_checker_treeview);
	store = GTK_LIST_STORE (gtk_tree_view_get_model (view));

	spell = gossip_spell_new (NULL);
	codes = gossip_spell_get_language_codes (spell);

	for (l = codes; l; l = l->next) {
		GtkTreeIter  iter;
		const gchar *code;
		const gchar *language;

		code = l->data; 
		language = gossip_spell_get_language_name (spell, code);
		
		if (!language || strlen (language) < 1) {
			continue;
		}

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 
				    COL_LANG_CODE, code,
				    COL_LANG_NAME, language,
				    -1);
	}

	g_list_foreach (codes, (GFunc)g_free, NULL);
	g_list_free (codes);

	if (spell) {
		gossip_spell_unref (spell);
	}
}

static void
preferences_languages_save (GossipPreferences *preferences)
{
	GtkTreeView       *view;
	GtkTreeModel      *model;

	gchar             *languages = NULL;

	view = GTK_TREE_VIEW (preferences->spell_checker_treeview);
	model = gtk_tree_view_get_model (view);
	
	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) preferences_languages_save_foreach,
				&languages);

	if (!languages) {
		/* default to english */
		languages = g_strdup ("en");
	}
	
	gconf_client_set_string (gossip_app_get_gconf_client (),
				 GCONF_PATH "/conversation/spell_checker_languages", 
				 languages,
				 NULL);

	g_free (languages);
}

static gboolean 
preferences_languages_save_foreach (GtkTreeModel  *model, 
				    GtkTreePath   *path, 
				    GtkTreeIter   *iter, 
				    gchar        **languages) 
{
	gboolean  enabled;
	gchar    *code;

	if (!languages) {
		return TRUE;
	}

	gtk_tree_model_get (model, iter, COL_LANG_ENABLED, &enabled, -1);
	if (!enabled) {
		return FALSE;
	}

	gtk_tree_model_get (model, iter, COL_LANG_CODE, &code, -1);
	if (!code) {
		return FALSE;
	}
	
	if (!(*languages)) {
		*languages = g_strdup (code);
	} else {
		gchar *str = *languages;
		*languages = g_strdup_printf ("%s,%s", str, code);
		g_free (str);
	}

	g_free (code);
	
	return FALSE;
}

static void 
preferences_languages_load (GossipPreferences *preferences)
{
	GtkTreeView    *view;
	GtkTreeModel   *model;

	gchar          *value;
	gchar         **vlanguages;

	value = gconf_client_get_string (
		gossip_app_get_gconf_client (),
		GCONF_PATH "/conversation/spell_checker_languages", 
		NULL);
	
	if (!value) {
		return;
	}

	vlanguages = g_strsplit (value, ",", -1);
	g_free (value);

	view = GTK_TREE_VIEW (preferences->spell_checker_treeview);
	model = gtk_tree_view_get_model (view);

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) preferences_languages_load_foreach,
				vlanguages);

	g_strfreev (vlanguages);
}

static gboolean 
preferences_languages_load_foreach (GtkTreeModel  *model, 
				    GtkTreePath   *path, 
				    GtkTreeIter   *iter, 
				    gchar        **languages) 
{
	gchar    *code;
	gchar    *lang;
	gint      i;
	gboolean  found = FALSE;

	if (!languages) {
		return TRUE;
	}


	gtk_tree_model_get (model, iter, COL_LANG_CODE, &code, -1);
	if (!code) {
		return FALSE;
	}

	for (i = 0, lang = languages[i]; lang; lang = languages[++i]) {
		if (strcmp (lang, code) == 0) {
			found = TRUE;
		}
	}
	
	gtk_list_store_set (GTK_LIST_STORE (model), iter, COL_LANG_ENABLED, found, -1);
	return FALSE;
}

static void 
preferences_languages_cell_toggled_cb (GtkCellRendererToggle *cell, 
				       gchar                 *path_string, 
				       GossipPreferences     *preferences)
{
	GtkTreeView  *view;
	GtkTreeModel *model;
	GtkListStore *store;
	GtkTreePath  *path;
	GtkTreeIter   iter;
	gboolean      enabled;

	view = GTK_TREE_VIEW (preferences->spell_checker_treeview);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);
	
	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, COL_LANG_ENABLED, &enabled, -1);

	enabled ^= 1;

	gtk_list_store_set (store, &iter, COL_LANG_ENABLED, enabled, -1);
	gtk_tree_path_free (path);

	preferences_languages_save (preferences);
}

static void
preferences_themes_setup (GossipPreferences *preferences)
{
	GtkComboBox     *combo;
	GtkListStore    *model;
	GtkTreeIter      iter;

	combo = GTK_COMBO_BOX (preferences->theme_combobox);

	model = gtk_list_store_new (COL_COMBO_COUNT,
				    G_TYPE_STRING,
				    G_TYPE_STRING);

	gtk_list_store_append (model, &iter);
	gtk_list_store_set (model, &iter,
			    COL_COMBO_VISIBLE_NAME, _("Classic"),
			    COL_COMBO_NAME, "classic",
			    -1);
	
	gtk_list_store_append (model, &iter);
	gtk_list_store_set (model, &iter,
			    COL_COMBO_VISIBLE_NAME, _("Blue"),
			    COL_COMBO_NAME, "blue",
			    -1);
	
	gtk_combo_box_set_model (combo, GTK_TREE_MODEL (model));
	g_object_unref (model);
}	

static void
preferences_set_boolean_from_gconf (const gchar *key, GtkWidget *widget)
{
	gboolean value;
	
	value = gconf_client_get_bool (gossip_app_get_gconf_client (), key, NULL);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);
}

static void
preferences_set_int_from_gconf (const gchar *key, GtkWidget *widget)
{
	gint value;
	
	value = gconf_client_get_int (gossip_app_get_gconf_client (), key, NULL);
	
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);
}

static void
preferences_set_string_from_gconf (const gchar *key, GtkWidget *widget)
{
	gchar *value;
	
	value = gconf_client_get_string (gossip_app_get_gconf_client (), key, NULL);
	
	gtk_entry_set_text (GTK_ENTRY (widget), value);

	g_free (value);
}

static void
preferences_set_string_combo_from_gconf (const gchar *key, GtkWidget *widget)
{
	gchar        *value;
	GtkTreeModel *model; 
	GtkTreeIter   iter;
	gboolean      found;
	
	value = gconf_client_get_string (gossip_app_get_gconf_client (), key, NULL);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));

	found = FALSE;
	if (value && gtk_tree_model_get_iter_first (model, &iter)) {
		gchar *name;
		
		do {
			gtk_tree_model_get (model, &iter,
					    COL_COMBO_NAME, &name, 
					    -1);
			
			if (strcmp (name, value) == 0) {
				found = TRUE;
				gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &iter);
				break;
			} else {
				found = FALSE;
			}

			g_free (name);
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	/* Fallback to classic. */
	if (!found) {
		if (gtk_tree_model_get_iter_first (model, &iter)) {
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &iter);
		}
	}

	g_free (value);
}

static void
preferences_notify_int_cb (GConfClient *client,
			   guint        cnxn_id,
			   GConfEntry  *entry,
			      gpointer   user_data)
{
	GConfValue *value;

	value = gconf_entry_get_value (entry);
	
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (user_data),
				   gconf_value_get_int (value));
}

static void
preferences_notify_string_cb (GConfClient *client,
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
preferences_notify_string_combo_cb (GConfClient *client,
				    guint        cnxn_id,
				    GConfEntry  *entry,
				    gpointer     user_data)
{
	preferences_set_string_combo_from_gconf (gconf_entry_get_key (entry), user_data);
}

static void
preferences_notify_bool_cb (GConfClient *client,
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
preferences_notify_sensitivity_cb (GConfClient *client,
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
preferences_hookup_spin_button (GossipPreferences *preferences,
		    const gchar       *key,
		    GtkWidget         *widget)
{
	guint id;

	/* Silence warning. */
	if (0) {
		preferences_hookup_spin_button (preferences, key, widget);
	}
	
	preferences_set_int_from_gconf (key, widget);

	g_object_set_data_full (G_OBJECT (widget), "key",
				g_strdup (key), g_free); 
	
	g_signal_connect (widget,
			  "value_changed",
			  G_CALLBACK (preferences_spin_button_value_changed_cb),
			  NULL);

	id = gconf_client_notify_add (gossip_app_get_gconf_client (),
				      key,
				      preferences_notify_int_cb,
				      widget,
				      NULL,
				      NULL);
	
	preferences->ids = g_list_prepend (preferences->ids,
					   GUINT_TO_POINTER (id));
}

static void
preferences_hookup_entry (GossipPreferences *preferences,
	      const gchar       *key,
	      GtkWidget         *widget)
{
	guint id;

	if (0) {  /* Silent warning before we use this function. */
		preferences_hookup_entry (preferences, key, widget);
	}
	
	preferences_set_string_from_gconf (key, widget);

	g_object_set_data_full (G_OBJECT (widget), "key",
				g_strdup (key), g_free); 
	
	g_signal_connect (widget,
			  "changed",
			  G_CALLBACK (preferences_entry_value_changed_cb),
			  NULL);

	id = gconf_client_notify_add (gossip_app_get_gconf_client (),
				      key,
				      preferences_notify_string_cb,
				      widget,
				      NULL,
				      NULL);
	
	preferences->ids = g_list_prepend (preferences->ids,
					   GUINT_TO_POINTER (id));
}

static void
preferences_hookup_toggle_button (GossipPreferences *preferences,
		      const gchar       *key,
		      GtkWidget         *widget)
{
	guint id;
	
	preferences_set_boolean_from_gconf (key, widget);
	
	g_object_set_data_full (G_OBJECT (widget), "key",
				g_strdup (key), g_free); 
	
	g_signal_connect (widget,
			  "toggled",
			  G_CALLBACK (preferences_toggle_button_toggled_cb),
			  NULL);

	id = gconf_client_notify_add (gossip_app_get_gconf_client (),
				      key,
				      preferences_notify_bool_cb,
				      widget,
				      NULL,
				      NULL);
	
	preferences->ids = g_list_prepend (preferences->ids,
					   GUINT_TO_POINTER (id));
}

static void
preferences_hookup_string_combo (GossipPreferences *preferences,
				 const gchar       *key,
				 GtkWidget         *widget)
{
	guint id;

	preferences_set_string_combo_from_gconf (key, widget);
	
	g_object_set_data_full (G_OBJECT (widget), "key",
				g_strdup (key), g_free); 
	
	g_signal_connect (widget,
			  "changed",
			  G_CALLBACK (preferences_string_combo_changed_cb),
			  NULL);

	id = gconf_client_notify_add (gossip_app_get_gconf_client (),
				      key,
				      preferences_notify_string_combo_cb,
				      widget,
				      NULL,
				      NULL);
	
	preferences->ids = g_list_prepend (preferences->ids,
					   GUINT_TO_POINTER (id));
}

static void
preferences_hookup_sensitivity (GossipPreferences *preferences,
			       const gchar       *key,
			       GtkWidget         *widget)
{
	gboolean value;
	guint    id;

	value = gconf_client_get_bool (gossip_app_get_gconf_client (), key, NULL);
	gtk_widget_set_sensitive (widget, value);
	
	id = gconf_client_notify_add (gossip_app_get_gconf_client (),
				      key,
				      preferences_notify_sensitivity_cb,
				      widget,
				      NULL,
				      NULL);

	preferences->ids = g_list_prepend (preferences->ids,
					   GUINT_TO_POINTER (id));
}

static void
preferences_spin_button_value_changed_cb (GtkWidget *button,
					  gpointer   user_data)
{
	const gchar *key;

	key = g_object_get_data (G_OBJECT (button), "key");

	gconf_client_set_int (gossip_app_get_gconf_client (),
			      key,
			      gtk_spin_button_get_value (GTK_SPIN_BUTTON (button)),
			      NULL);
}

static void
preferences_entry_value_changed_cb (GtkWidget *entry,
				    gpointer   user_data)
{
	const gchar *key;

	key = g_object_get_data (G_OBJECT (entry), "key");
	
	gconf_client_set_string (gossip_app_get_gconf_client (),
				 key,
				 gtk_entry_get_text (GTK_ENTRY (entry)),
				 NULL);
}

static void
preferences_toggle_button_toggled_cb (GtkWidget *button,
				      gpointer   user_data)
{
	const gchar *key;

	key = g_object_get_data (G_OBJECT (button), "key");

	gconf_client_set_bool (gossip_app_get_gconf_client (),
			       key,
			       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)),
			       NULL);
}

static void
preferences_string_combo_changed_cb (GtkWidget *combo,
				     gpointer   user_data)
{
	const gchar  *key;
	GtkTreeModel *model;
	GtkTreeIter   iter;
	gchar        *name;

	key = g_object_get_data (G_OBJECT (combo), "key");

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter)) {
		model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
		
		gtk_tree_model_get (model, &iter,
				    COL_COMBO_NAME, &name,
				    -1);
		
		gconf_client_set_string (gossip_app_get_gconf_client (),
					 key,
					 name,
					 NULL);

		g_free (name);
	}
}

static void
preferences_response_cb (GtkWidget         *widget,
			 gint               response,
			 GossipPreferences *preferences)
{
	gtk_widget_destroy (widget);
}

static void
preferences_destroy_cb (GtkWidget         *widget,
			GossipPreferences *preferences)
{
	GList *l;

	for (l = preferences->ids; l; l = l->next) {
		guint id = GPOINTER_TO_UINT (l->data);

		gconf_client_notify_remove (gossip_app_get_gconf_client (), id);
	}

	g_list_free (preferences->ids);
	g_free (preferences);
}

void
gossip_preferences_show (void)
{
	static GossipPreferences *preferences;
	GladeXML                 *glade;

	if (preferences) {
		gtk_window_present (GTK_WINDOW (preferences->dialog));
		return;
	}
	
        preferences = g_new0 (GossipPreferences, 1);
	preferences->app = gossip_app_get ();

	glade = gossip_glade_get_file (
		GLADEDIR "/main.glade",
		"preferences_dialog",
		NULL,
		"preferences_dialog", &preferences->dialog,
		"notebook", &preferences->notebook,
		"sound_checkbutton", &preferences->sound_checkbutton,
		"silent_busy_checkbutton", &preferences->silent_busy_checkbutton,
		"silent_away_checkbutton", &preferences->silent_away_checkbutton,
		"smileys_checkbutton", &preferences->smileys_checkbutton,
		"separate_windows_checkbutton", &preferences->separate_windows_checkbutton,
		"spell_checker_vbox", &preferences->spell_checker_vbox,
		"spell_checker_checkbutton", &preferences->spell_checker_checkbutton,
		"spell_checker_treeview", &preferences->spell_checker_treeview,
		"theme_combobox", &preferences->theme_combobox,
		NULL);

	g_object_add_weak_pointer (G_OBJECT (preferences->dialog),
				   (gpointer) &preferences);
	
	preferences_themes_setup (preferences);

	preferences_setup_widgets (preferences);
	
	preferences_languages_setup (preferences);
	preferences_languages_add (preferences);
	preferences_languages_load (preferences);
	
	gossip_glade_connect (glade, 
			      preferences,
			      "preferences_dialog", "destroy", preferences_destroy_cb,
			      "preferences_dialog", "response", preferences_response_cb,
			      NULL);
	
	if (gossip_spell_supported ()) {
		GtkWidget *spell_checking_page;

		spell_checking_page = gtk_notebook_get_nth_page (
			GTK_NOTEBOOK (preferences->notebook), 1);
		gtk_widget_show (spell_checking_page);
	}

	gtk_window_set_transient_for (GTK_WINDOW (preferences->dialog),
				      GTK_WINDOW (gossip_app_get_window ()));
	gtk_widget_show (preferences->dialog);
}
