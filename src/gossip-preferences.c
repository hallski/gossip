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
	GtkWidget    *auto_away_checkbutton;
	GtkWidget    *away_spinbutton;
	GtkWidget    *extended_away_spinbutton;
	GtkWidget    *sound_checkbutton;
	GtkWidget    *silent_checkbutton;
	GtkWidget    *smileys_checkbutton;
	GtkWidget    *timestamp_checkbutton;
	GtkWidget    *compact_checkbutton;

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
			      GCONF_PATH "/auto_away/enabled",
			      preferences->auto_away_checkbutton);
	
	hookup_spin_button (preferences,
			    GCONF_PATH "/auto_away/idle_time",
			    preferences->away_spinbutton);
	hookup_sensitivity_controller (preferences,
				       GCONF_PATH "/auto_away/enabled",
				       preferences->away_spinbutton);
	
	hookup_spin_button (preferences,
			    GCONF_PATH "/auto_away/extended_idle_time",
			    preferences->extended_away_spinbutton);
	hookup_sensitivity_controller (preferences,
				       GCONF_PATH "/auto_away/enabled",
				       preferences->extended_away_spinbutton);
	
	hookup_toggle_button (preferences,
			      GCONF_PATH "/sound/play_sounds",
			      preferences->sound_checkbutton);
	hookup_toggle_button (preferences,
			      GCONF_PATH "/sound/silent_away",
			      preferences->silent_checkbutton);
	hookup_sensitivity_controller (preferences,
				       GCONF_PATH "/sound/play_sounds",
				       preferences->silent_checkbutton);

	hookup_toggle_button (preferences,
			      GCONF_PATH "/conversation/graphical_smileys",
			      preferences->smileys_checkbutton);
	hookup_toggle_button (preferences,
			      GCONF_PATH "/conversation/compact_mode",
			      preferences->compact_checkbutton);
	hookup_toggle_button (preferences,
			      GCONF_PATH "/conversation/timestamp_messages",
			      preferences->timestamp_checkbutton);
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
				     "auto_away_checkbutton", &preferences->auto_away_checkbutton,
				     "away_spinbutton", &preferences->away_spinbutton,
				     "extended_away_spinbutton", &preferences->extended_away_spinbutton,
				     "sound_checkbutton", &preferences->sound_checkbutton,
				     "silent_checkbutton", &preferences->silent_checkbutton,
				     "smileys_checkbutton", &preferences->smileys_checkbutton,
				     "compact_checkbutton", &preferences->compact_checkbutton,
				     "timestamp_checkbutton", &preferences->timestamp_checkbutton,
				     NULL);

	preferences_setup_widgets (preferences);
	
	gossip_glade_connect (gui, preferences,
			      "preferences_dialog", "destroy", preferences_destroy_cb,
			      "preferences_dialog", "response", preferences_response_cb,
			      NULL);
	
	gtk_widget_show (preferences->dialog);

 	return preferences->dialog;
}

