/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio AB
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
#include <libgnomeui/gnome-druid.h>
#include <libgnome/gnome-config.h>
#include <glib/gi18n.h>
#include <loudmouth/loudmouth.h>

#include "gossip-app.h"
#include "gossip-register.h"
#include "gossip-new-account-window.h"


typedef struct {
	gchar  *label;
	gchar  *address;
} ServerEntry;


/* default servers to fall back on */
static ServerEntry servers[] = {
	{ "Jabber.com", "jabber.com" },
	{ "Jabber.cn", "jabber.cn" },
	{ "Jabber.cz", "jabber.cz" },
	{ "Jabber.dk", "jabber.dk" },
	{ "Jabber.fr", "jabber.fr" },
	{ "Jabber.hu", "jabber.hu" },
	{ "Jabber.no", "jabber.no" },
	{ "Jabber.org", "jabber.org" },
	{ "Jabber.org.uk", "jabber.org.uk" },
	{ "Jabber.ru", "jabber.ru" },
	{ "Jabber.sk", "jabber.sk" }
};


typedef struct {
	gboolean      gtk_main_started;
	
	GtkWidget    *window;
	GtkWidget    *druid;
	
	/* Page one */
	GtkWidget    *one_page;

	/* Page two */
	GtkWidget    *two_page;
	GtkWidget    *two_yes_radiobutton;
	GtkWidget    *two_no_radiobutton;
	
	/* Page three */
	GtkWidget    *three_page;
	GtkWidget    *three_nick_entry;
	GtkWidget    *three_name_label;
	GtkWidget    *three_name_entry;
	GtkWidget    *three_account_label;
	GtkWidget    *three_no_account_label;

	/* Page four */
	GtkWidget    *four_page;
	GtkWidget    *four_no_account_label;
	GtkWidget    *four_account_label;
	GtkWidget    *four_server_optionmenu;
	GtkWidget    *four_different_radiobutton;
	GtkWidget    *four_server_entry;

	/* Last page */
	GtkWidget    *last_page;
	GtkWidget    *last_action_label;
} GossipNewAccountWindow;


static void     new_account_window_destroyed          (GtkWidget              *widget,
						       GossipNewAccountWindow *window);
static void     new_account_window_cancel             (GtkWidget              *widget,
						       GossipNewAccountWindow *window);
static void     new_account_window_prepare_page_1     (GnomeDruidPage         *page,
						       GnomeDruid             *druid,
						       GossipNewAccountWindow *window);
static void     new_account_window_prepare_page_3     (GnomeDruidPage         *page,
						       GnomeDruid             *druid,
						       GossipNewAccountWindow *window);
static void     new_account_window_prepare_page_last  (GnomeDruidPage         *page,
						       GnomeDruid             *druid,
						       GossipNewAccountWindow *window);
static void     new_account_window_last_page_finished (GnomeDruidPage         *page,
						       GnomeDruid             *druid,
						       GossipNewAccountWindow *window);
static void     new_account_window_3_entry_changed    (GtkEntry               *entry,
						       GossipNewAccountWindow *window);
static gboolean new_account_window_setup_account      (GossipNewAccountWindow *druid);


static void
new_account_window_destroyed (GtkWidget              *widget,
			      GossipNewAccountWindow *window)
{
	if (window->gtk_main_started) {
		gtk_main_quit ();
	}

	g_free (window);
}

static void
new_account_window_cancel (GtkWidget              *widget,
			   GossipNewAccountWindow *window)
{
	gtk_widget_destroy (window->window);
}

static void
new_account_window_prepare_page_1 (GnomeDruidPage         *page, 
				   GnomeDruid             *druid, 
				   GossipNewAccountWindow *window)
{
	gnome_druid_set_buttons_sensitive (druid, FALSE, TRUE, TRUE, FALSE);
}

static void
new_account_window_prepare_page_3 (GnomeDruidPage         *page,
				   GnomeDruid             *druid, 
				   GossipNewAccountWindow *window)
{
	gboolean         ok = TRUE;
	const gchar     *str;
	GtkToggleButton *toggle;
	gboolean         has_account;
	
	str = gtk_entry_get_text (GTK_ENTRY (window->three_nick_entry));
	ok &= str && str[0];

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid),
					   TRUE,
					   ok,
					   TRUE,
					   FALSE);

	toggle = GTK_TOGGLE_BUTTON (window->two_yes_radiobutton);
	has_account = gtk_toggle_button_get_active (toggle);
	if (has_account) {
		gtk_widget_show (window->three_account_label);
		gtk_widget_hide (window->three_no_account_label);
		gtk_widget_hide (window->three_name_entry);
		gtk_widget_hide (window->three_name_label);
	} else {
		gtk_widget_hide (window->three_account_label);
		gtk_widget_show (window->three_no_account_label);
		gtk_widget_show (window->three_name_label);
		gtk_widget_show (window->three_name_entry);
	}

	gtk_widget_grab_focus (window->three_nick_entry);
}

static void
new_account_window_prepare_page_4 (GnomeDruidPage         *page,
				   GnomeDruid             *druid,
				   GossipNewAccountWindow *window) 
{
	GtkToggleButton *toggle;
	gboolean         has_account;

	toggle = GTK_TOGGLE_BUTTON (window->two_yes_radiobutton);
	has_account = gtk_toggle_button_get_active (toggle);
	if (has_account) {
		gtk_widget_show (window->four_account_label);
		gtk_widget_hide (window->four_no_account_label);
	} else {
		gtk_widget_show (window->four_no_account_label);
		gtk_widget_hide (window->four_account_label);
	}
}

static gboolean
new_account_window_get_account_info (GossipNewAccountWindow  *window,
				     GossipAccount          **account)
{
	GtkToggleButton *toggle;
	gboolean         has_account;
	gboolean         predefined_server;
	GtkOptionMenu   *option_menu;
	
	toggle = GTK_TOGGLE_BUTTON (window->two_yes_radiobutton);
	has_account = gtk_toggle_button_get_active (toggle);
	toggle = GTK_TOGGLE_BUTTON (window->four_different_radiobutton);
	predefined_server = !gtk_toggle_button_get_active (toggle);

	if (account) {
		const gchar *username;
		const gchar *server = "";
		gchar       *name;
		gchar       *id;

		*account = NULL;

		username = gtk_entry_get_text (GTK_ENTRY (window->three_nick_entry));
		if (predefined_server) {
			ServerEntry *server_entry;
			
			option_menu = GTK_OPTION_MENU (window->four_server_optionmenu);
			server_entry = gossip_option_menu_get_history (option_menu);

			server = server_entry->address;
		} else {
			server = gtk_entry_get_text (GTK_ENTRY (window->four_server_entry));
		}
		
		name = g_strdup_printf ("%s at %s", 
				       username, server);

		/* FIXME: Jabber specific... */
		id = g_strdup_printf ("%s@%s/%s",
				      username, server, _("Home"));

		*account = g_object_new (GOSSIP_TYPE_ACCOUNT,
					 "name", name,
					 "id", id,
					 "server", server, 
					 "port", 5222,
					 "use_ssl", FALSE,
					 "use_proxy", FALSE,
					 NULL);

		g_free (name);
		g_free (id);
	}

	/* FIXME: Set this in some settings-thingy... */
	/* *realname = gtk_entry_get_text (GTK_ENTRY (window->three_name_entry)); */

	return has_account;
}
				
static void
new_account_window_prepare_page_last (GnomeDruidPage         *page,
				      GnomeDruid             *druid,
				      GossipNewAccountWindow *window)
{
	GossipAccount *account;
	gchar         *str;
	gboolean       has_account;
	
  	gnome_druid_set_show_finish (GNOME_DRUID (window->druid), TRUE);

	has_account = new_account_window_get_account_info (window, &account);

	if (has_account) {
		str = g_strdup_printf ("%s\n<b>%s</b>.",
				       _("Gossip will now try to use your account:"),
				       gossip_account_get_id (account));
	} else {
		str = g_strdup_printf ("%s\n<b>%s</b>.",
				       _("Gossip will now try to register the account:"),
				       gossip_account_get_id (account));
	}

	gtk_label_set_markup (GTK_LABEL (window->last_action_label), str);

	g_free (str);

	g_object_unref (account);
}

static void
new_account_window_last_page_finished (GnomeDruidPage         *page,
				       GnomeDruid             *druid,
				       GossipNewAccountWindow *window)
{
	g_print ("last page finished\n");

	if (new_account_window_setup_account (window)) {
		gtk_widget_destroy (window->window);
	}
}

static void
new_account_window_3_entry_changed (GtkEntry               *entry,
				    GossipNewAccountWindow *window)
{
	gboolean     ok = TRUE;
	const gchar *str;
	
	str = gtk_entry_get_text (GTK_ENTRY (window->three_nick_entry));
	ok &= str && str[0];

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid),
					   TRUE,
					   ok,
					   TRUE,
					   FALSE);
}

static void
new_account_window_4_entry_changed (GtkEntry               *entry, 
				    GossipNewAccountWindow *window) 
{
	gboolean other;
	gboolean ok = TRUE;

	other = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (window->four_different_radiobutton));

	if (other) {
		const gchar *str;

		str = gtk_entry_get_text (GTK_ENTRY (window->four_server_entry));
		
		if (!str || strcmp (str, "") == 0) {
			ok = FALSE;
		}
	} 
	
	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid),
					   TRUE,
					   ok,
					   TRUE,
					   FALSE);
}

static void
new_account_window_4_different_toggled (GtkToggleButton        *button, 
					GossipNewAccountWindow *window) 
{
	gboolean ok = TRUE;
	
	if (gtk_toggle_button_get_active (button)) {
		const gchar *str;

		gtk_widget_set_sensitive (window->four_server_optionmenu,
					  FALSE);
		gtk_widget_set_sensitive (window->four_server_entry,
					  TRUE);
		
		str = gtk_entry_get_text (GTK_ENTRY (window->four_server_entry));
		if (!str || strcmp (str, "") == 0) {
			ok = FALSE;
		}
	} else {
		gtk_widget_set_sensitive (window->four_server_optionmenu,
					  TRUE);
		gtk_widget_set_sensitive (window->four_server_entry,
					  FALSE);
	}

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid),
					   TRUE,
					   ok,
					   TRUE,
					   FALSE);
}

static gboolean
new_account_window_setup_account (GossipNewAccountWindow *druid)
{
	GossipSession        *session;
	GossipAccountManager *manager;
	GossipAccount        *account;
	gboolean              has_account;

	session = gossip_app_get_session ();
	manager = gossip_session_get_account_manager (session);

	has_account = new_account_window_get_account_info (druid, &account); 
	if (!has_account) {
		if (!gossip_register_account (account, GTK_WINDOW (druid->window))) {
			return FALSE;
		}
	}
	
	if (gossip_account_manager_get_count (manager) < 1) {
		gossip_account_manager_set_default (manager, account);
	}

	gossip_account_manager_add (manager, account);
	gossip_account_manager_store (manager);

	g_object_unref (account);
	
	return TRUE;
}

gboolean
gossip_new_account_window_is_needed (void)
{
	GossipSession          *session;
	GossipAccountManager   *manager;

	if (g_getenv ("GOSSIP_FORCE_DRUID")) {
		return TRUE;
	}

	session = gossip_app_get_session ();
	if (!session) {
		return FALSE;
	}

	manager = gossip_session_get_account_manager (session);
	if (!manager) {
		return FALSE;
	}

	if (gossip_account_manager_get_count (manager) < 1) {
		return TRUE;
	}

	return FALSE;
}

void
gossip_new_account_window_show (void)
{
	GossipNewAccountWindow *window;
	GladeXML               *glade;
	GnomeDruid             *druid;

	window = g_new0 (GossipNewAccountWindow, 1);
	
	glade = gossip_glade_get_file (
		GLADEDIR "/main.glade",
		"new_account_window",
		NULL,
		"new_account_window", &window->window,
		"druid", &window->druid,
		"1_page", &window->one_page,
		"2_page", &window->two_page,
		"2_yes_radiobutton", &window->two_yes_radiobutton,
		"2_no_radiobutton", &window->two_no_radiobutton,
		"3_page", &window->three_page,
		"3_account_label", &window->three_account_label,
		"3_no_account_label", &window->three_no_account_label,
		"3_nick_entry", &window->three_nick_entry,
		"3_name_label", &window->three_name_label,
		"3_name_entry", &window->three_name_entry,
		"4_page", &window->four_page,
		"4_no_account_label", &window->four_no_account_label,
		"4_account_label", &window->four_account_label,
		"4_server_optionmenu", &window->four_server_optionmenu,
		"4_different_radiobutton", &window->four_different_radiobutton,
		"4_server_entry", &window->four_server_entry,
		"last_page", &window->last_page,
		"last_action_label", &window->last_action_label,
		NULL);
	
	gossip_glade_connect (
		glade, window,
		"new_account_window", "destroy", new_account_window_destroyed,
		"druid", "cancel", new_account_window_cancel,
		"3_nick_entry", "changed", new_account_window_3_entry_changed,
		"4_server_entry", "changed", new_account_window_4_entry_changed,
		"4_different_radiobutton", "toggled", new_account_window_4_different_toggled,
		"last_page", "finish", new_account_window_last_page_finished,
		NULL);
	
	gossip_option_menu_setup (window->four_server_optionmenu,
				  NULL, NULL,
				  servers[0].label, &servers[0], 
				  servers[1].label, &servers[1], 
				  servers[2].label, &servers[2], 
				  servers[3].label, &servers[3], 
				  servers[4].label, &servers[4], 
				  servers[5].label, &servers[5], 
				  servers[6].label, &servers[6], 
				  servers[7].label, &servers[7], 
				  servers[8].label, &servers[8], 
				  servers[9].label, &servers[9], 
				  servers[10].label, &servers[10], 
				  NULL);
		
	g_object_unref (glade);
	
	g_signal_connect_after (window->one_page, "prepare",
				G_CALLBACK (new_account_window_prepare_page_1),
				window);
	g_signal_connect_after (window->three_page, "prepare",
				G_CALLBACK (new_account_window_prepare_page_3),
				window);
	g_signal_connect_after (window->four_page, "prepare",
				G_CALLBACK (new_account_window_prepare_page_4),
				window);
	g_signal_connect_after (window->last_page, "prepare",
				G_CALLBACK (new_account_window_prepare_page_last),
				window);

	new_account_window_prepare_page_1 (GNOME_DRUID_PAGE (window->one_page),
				      GNOME_DRUID (window->druid),
				      window);

	druid = GNOME_DRUID (window->druid);
	
	g_object_set (G_OBJECT (druid->next), 
		      "can-default", TRUE,
		      "has-default", TRUE,
		      NULL);

	gtk_widget_show (window->window);


	if (!gossip_new_account_window_is_needed ()) {
		/* skip the first page */
		gnome_druid_set_page (GNOME_DRUID (window->druid),
				      GNOME_DRUID_PAGE (window->two_page));
	} else {
		window->gtk_main_started = TRUE;
		gtk_main ();
	}
}

