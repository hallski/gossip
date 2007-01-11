/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Xavier Claessens <xclaesse@gmail.com>
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

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-utils.h>

#include "gossip-account-widget-generic.h"
#include "gossip-app.h"

typedef struct {
	GossipAccount *account;

	GList         *widgets;

	GtkWidget     *table_settings;
	GtkSizeGroup  *size_group;

	guint          n_rows;
	gboolean       account_changed;
} GossipAccountWidgetGeneric;

static void     account_widget_generic_save_foreach           (GtkWidget                  *widget,
							       GossipAccountWidgetGeneric *settings);
static void     account_widget_generic_save                   (GossipAccountWidgetGeneric *settings);
static gboolean account_widget_generic_entry_focus_cb         (GtkWidget                  *widget,
							       GdkEventFocus              *event,
							       GossipAccountWidgetGeneric *settings);
static void     account_widget_generic_entry_changed_cb       (GtkWidget                  *widget,
							       GossipAccountWidgetGeneric *settings);
static void     account_widget_generic_checkbutton_toggled_cb (GtkWidget                  *widget,
							       GossipAccountWidgetGeneric *settings);
static gchar *  account_widget_generic_format_param_name      (const gchar                *param_name);
static void     account_widget_generic_setup_foreach          (gchar                      *param_name,
							       GossipAccountWidgetGeneric *settings);
static void     account_widget_generic_destroy_cb             (GtkWidget                  *widget,
							       GossipAccountWidgetGeneric *settings);

static void
account_widget_generic_save_foreach (GtkWidget                  *widget,
				     GossipAccountWidgetGeneric *settings)
{
	const gchar *param_name;
	GValue      *g_value = NULL;

	param_name = g_object_get_data (G_OBJECT (widget), "param_name");

	if (GTK_IS_CHECK_BUTTON (widget)) {
		gboolean active;

		active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

		g_value = g_new0 (GValue, 1);
		g_value_init (g_value, G_TYPE_BOOLEAN);
		g_value_set_boolean (g_value, active);
	}
	else if (GTK_IS_ENTRY (widget)){
		const gchar *str;
		GType        g_type;

		str = gtk_entry_get_text (GTK_ENTRY (widget));

		g_type = G_VALUE_TYPE (gossip_account_param_get_g_value (settings->account, param_name));
		g_value = gossip_string_to_g_value (str, g_type);
	} else {
		return;
	}

	gossip_account_param_set_g_value (settings->account, param_name, g_value);
	g_value_unset (g_value);
	g_free (g_value);
}

static void
account_widget_generic_save (GossipAccountWidgetGeneric *settings)
{
	GossipSession        *session;
	GossipAccountManager *manager;

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	g_list_foreach (settings->widgets,
			(GFunc) account_widget_generic_save_foreach,
			settings);

	gossip_account_manager_store (manager);

	settings->account_changed = FALSE;
}

static gboolean 
account_widget_generic_entry_focus_cb (GtkWidget                  *widget,
				       GdkEventFocus              *event,
				       GossipAccountWidgetGeneric *settings)
{
	if (settings->account_changed) {
 		account_widget_generic_save (settings); 
	}

	return FALSE;
}

static void
account_widget_generic_entry_changed_cb (GtkWidget                  *widget,
					 GossipAccountWidgetGeneric *settings)
{
	settings->account_changed = TRUE;
}

static void  
account_widget_generic_checkbutton_toggled_cb (GtkWidget                  *widget,
					       GossipAccountWidgetGeneric *settings)
{
 	account_widget_generic_save (settings); 
}

static gchar *
account_widget_generic_format_param_name (const gchar *param_name)
{
	gchar *str;
	gchar *p;

	str = g_strdup (param_name);
	
	if (str && g_ascii_isalpha (str[0])) {
		str[0] = g_ascii_toupper (str[0]);
	}
	
	while ((p = strchr (str, '-')) != NULL) {
		if (p[1] != '\0' && g_ascii_isalpha (p[1])) {
			p[0] = ' ';
			p[1] = g_ascii_toupper (p[1]);
		}

		p++;
	}
	
	return str;
}

static void
account_widget_generic_setup_foreach (gchar                      *param_name,
				      GossipAccountWidgetGeneric *settings)
{
	GtkWidget          *widget;
	GossipAccountParam *param;

	param = gossip_account_param_get_param (settings->account, param_name);
		
	gtk_table_resize (GTK_TABLE (settings->table_settings),
			  ++settings->n_rows,
			  2);

	if (G_VALUE_TYPE (&param->g_value) == G_TYPE_BOOLEAN) {
		widget = gtk_check_button_new_with_label (param_name);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
					      g_value_get_boolean (&param->g_value));

		g_signal_connect (widget, "toggled",
				  G_CALLBACK (account_widget_generic_checkbutton_toggled_cb),
				  settings);

		gtk_table_attach (GTK_TABLE (settings->table_settings),
				  widget,
				  0, 2,
				  settings->n_rows - 1, settings->n_rows,
				  GTK_FILL | GTK_EXPAND, 0,
				  0, 0);

		g_object_set_data (G_OBJECT (widget), "param_name", param_name);
		settings->widgets = g_list_prepend (settings->widgets, widget);
	} else {
		gchar *param_name_formatted;
		gchar *str;

		param_name_formatted = account_widget_generic_format_param_name (param_name);
		str = g_strdup_printf (_("%s:"), param_name_formatted);
		widget = gtk_label_new (str);
		g_free (param_name_formatted);
		g_free (str);

		gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
		gtk_size_group_add_widget (settings->size_group, widget);
		gtk_table_attach (GTK_TABLE (settings->table_settings),
				  widget,
				  0, 1,
				  settings->n_rows - 1, settings->n_rows,
				  GTK_FILL, 0,
				  0, 0);

		widget = gtk_entry_new ();
		str = gossip_g_value_to_string (&param->g_value);
		if (str) {
			gtk_entry_set_text (GTK_ENTRY (widget), str);
			g_free (str);
		}

		if (g_ascii_strncasecmp (param_name, "password", 8) == 0) {
			gtk_entry_set_visibility (GTK_ENTRY (widget), FALSE);
		}

		g_signal_connect (widget, "changed",
				  G_CALLBACK (account_widget_generic_entry_changed_cb),
				  settings);
		g_signal_connect (widget, "focus-out-event",
				  G_CALLBACK (account_widget_generic_entry_focus_cb),
				  settings);

		gtk_table_attach (GTK_TABLE (settings->table_settings),
				  widget,
				  1, 2,
				  settings->n_rows - 1, settings->n_rows,
				  GTK_FILL | GTK_EXPAND, 0,
				  0, 0);

		g_object_set_data (G_OBJECT (widget), "param_name", param_name);
		settings->widgets = g_list_prepend (settings->widgets, widget);
	}
}

static void
account_widget_generic_destroy_cb (GtkWidget                  *widget,
				   GossipAccountWidgetGeneric *settings)
{
	if (settings->account_changed) {
 		account_widget_generic_save (settings); 
	}

	g_list_free (settings->widgets);
	g_object_unref (settings->account);


	g_object_unref (settings->size_group);

	g_free (settings);
}

GtkWidget *
gossip_account_widget_generic_new (GossipAccount *account,
				   GtkWidget     *label_name)
{
	GossipAccountWidgetGeneric *settings;
	GList                      *params;

	settings = g_new0 (GossipAccountWidgetGeneric, 1);

	settings->account = g_object_ref (account);

	settings->table_settings = gtk_table_new (0, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (settings->table_settings), 6);
	gtk_table_set_col_spacings (GTK_TABLE (settings->table_settings), 6);

	settings->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	if (label_name) {
		gtk_size_group_add_widget (settings->size_group, label_name);
	}
	
	params = gossip_account_param_get_all (settings->account);
	g_list_foreach (params, (GFunc) account_widget_generic_setup_foreach, settings);
	g_list_foreach (params, (GFunc) g_free, NULL);
	g_list_free (params);

/* 	gossip_account_param_foreach (settings->account, */
/* 				      (GossipAccountParamFunc) account_widget_generic_setup_foreach, */
/* 				      settings); */

	g_signal_connect (settings->table_settings, "destroy",
			  G_CALLBACK (account_widget_generic_destroy_cb),
			  settings);

	gtk_widget_show_all (settings->table_settings);

	return settings->table_settings;
}
