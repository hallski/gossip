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
#include <gconf/gconf-client.h>

#include <libgossip/gossip-utils.h>

#include "gossip-app.h"
#include "gossip-stock.h"
#include "gossip-ui-utils.h"
#include "gossip-preferences.h"
#include "gossip-spell.h"


extern GConfClient *gconf_client;


typedef struct {
	GossipApp *app;
 	
	GtkWidget *dialog;
	GtkWidget *sound_checkbutton;
	GtkWidget *silent_busy_checkbutton;
	GtkWidget *silent_away_checkbutton;
	GtkWidget *smileys_checkbutton;
	GtkWidget *compact_checkbutton;
	GtkWidget *leaving_entry;
	GtkWidget *away_entry;
	GtkWidget *spell_checker_vbox;
	GtkWidget *spell_checker_checkbutton;
	
	GList     *ids;
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
				     "spell_checker_vbox", &preferences->spell_checker_vbox,
				     "spell_checker_checkbutton", &preferences->spell_checker_checkbutton,
				     NULL);

	g_object_add_weak_pointer (G_OBJECT (preferences->dialog),
				   (gpointer) &preferences);
	
	preferences_setup_widgets (preferences);
	
	gossip_glade_connect (gui, preferences,
			      "preferences_dialog", "destroy", preferences_destroy_cb,
			      "preferences_dialog", "response", preferences_response_cb,
			      NULL);
	
	if (!gossip_spell_supported ()) {
		gtk_widget_hide (preferences->spell_checker_vbox);
	}

	gtk_window_set_transient_for (GTK_WINDOW (preferences->dialog), GTK_WINDOW (gossip_app_get_window ()));
	gtk_widget_show (preferences->dialog);
}
