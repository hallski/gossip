/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio HB
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
#include <libgnome/gnome-i18n.h>
#include <loudmouth/loudmouth.h>
#include "gossip-utils.h"
#include "gossip-app.h"
#include "gossip-register.h"
#include "gossip-startup-druid.h"

typedef struct {
	gchar  *label;
	gchar  *address;
} ServerEntry;

static ServerEntry servers[] = {
	{ "Jabber.org", "jabber.org" },
	{ "Jabber.com", "jabber.com" }
};

typedef struct {
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
} GossipStartupDruid;

static void     startup_druid_destroyed                (GtkWidget          *unused,
							GossipStartupDruid *startup_druid);
static void     startup_druid_cancel                   (GtkWidget          *unused,
							GossipStartupDruid *startup_druid);
static void     startup_druid_prepare_page_1           (GnomeDruidPage     *page,
							GnomeDruid         *druid,
							GossipStartupDruid *startup_druid);
static void     startup_druid_prepare_page_2           (GnomeDruidPage     *page,
							GnomeDruid         *druid,
							GossipStartupDruid *startup_druid);
static void     startup_druid_prepare_page_3           (GnomeDruidPage     *page,
							GnomeDruid         *druid,
							GossipStartupDruid *startup_druid);
static void     startup_druid_prepare_page_last        (GnomeDruidPage     *page,
							GnomeDruid         *druid,
							GossipStartupDruid *startup_druid);
static void     startup_druid_last_page_finished       (GnomeDruidPage     *page,
							GnomeDruid         *druid,
							GossipStartupDruid *startup_druid);
static void     startup_druid_3_entry_changed          (GtkEntry           *entry,
							GossipStartupDruid *startup_druid);
static gboolean startup_druid_register_account         (GossipStartupDruid *druid);


static void
startup_druid_destroyed (GtkWidget          *unused,
			 GossipStartupDruid *startup_druid)
{
	g_free (startup_druid);
	gtk_main_quit ();
}

static void
startup_druid_cancel (GtkWidget          *widget,
		      GossipStartupDruid *startup_druid)
{
	gtk_widget_destroy (startup_druid->window);
}

static void
startup_druid_prepare_page_1 (GnomeDruidPage     *page, 
			      GnomeDruid         *druid, 
			      GossipStartupDruid *startup_druid)
{
	gnome_druid_set_buttons_sensitive (druid, FALSE, TRUE, TRUE, FALSE);
}

static void
startup_druid_prepare_page_2 (GnomeDruidPage     *page,
			      GnomeDruid         *druid, 
			      GossipStartupDruid *startup_druid)
{

}

static void
startup_druid_prepare_page_3 (GnomeDruidPage     *page,
			      GnomeDruid         *druid, 
			      GossipStartupDruid *startup_druid)
{
	gboolean         ok = TRUE;
	const gchar     *str;
	GtkToggleButton *toggle;
	gboolean         has_account;
	
	str = gtk_entry_get_text (GTK_ENTRY (startup_druid->three_nick_entry));
	ok &= str && str[0];

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (startup_druid->druid),
					   TRUE,
					   ok,
					   TRUE,
					   FALSE);

	toggle = GTK_TOGGLE_BUTTON (startup_druid->two_yes_radiobutton);
	has_account = gtk_toggle_button_get_active (toggle);
	if (has_account) {
		gtk_widget_show (startup_druid->three_account_label);
		gtk_widget_hide (startup_druid->three_no_account_label);
		gtk_widget_hide (startup_druid->three_name_entry);
		gtk_widget_hide (startup_druid->three_name_label);
	} else {
		gtk_widget_hide (startup_druid->three_account_label);
		gtk_widget_show (startup_druid->three_no_account_label);
		gtk_widget_show (startup_druid->three_name_label);
		gtk_widget_show (startup_druid->three_name_entry);
	}

	gtk_widget_grab_focus (startup_druid->three_nick_entry);
}

static void
startup_druid_prepare_page_4 (GnomeDruidPage     *page,
			      GnomeDruid         *druid,
			      GossipStartupDruid *startup_druid) 
{
	GtkToggleButton *toggle;
	gboolean         has_account;

	toggle = GTK_TOGGLE_BUTTON (startup_druid->two_yes_radiobutton);
	has_account = gtk_toggle_button_get_active (toggle);
	if (has_account) {
		gtk_widget_show (startup_druid->four_account_label);
		gtk_widget_hide (startup_druid->four_no_account_label);
	} else {
		gtk_widget_show (startup_druid->four_no_account_label);
		gtk_widget_hide (startup_druid->four_account_label);
	}
}

static gboolean
startup_druid_get_account_info (GossipStartupDruid  *startup_druid,
				GossipAccount      **account)
{
	GtkToggleButton *toggle;
	gboolean         has_account;
	gboolean         predefined_server;
	GtkOptionMenu   *option_menu;
	
	toggle = GTK_TOGGLE_BUTTON (startup_druid->two_yes_radiobutton);
	has_account = gtk_toggle_button_get_active (toggle);
	toggle = GTK_TOGGLE_BUTTON (startup_druid->four_different_radiobutton);
	predefined_server = !gtk_toggle_button_get_active (toggle);

	if (account) {
		const gchar *username;
		const gchar *server = "";

		username = gtk_entry_get_text (GTK_ENTRY (startup_druid->three_nick_entry));
		if (predefined_server) {
			ServerEntry *server_entry;
			
			option_menu = GTK_OPTION_MENU (startup_druid->four_server_optionmenu);
			server_entry = gossip_option_menu_get_history (option_menu);

			server = server_entry->address;
		} else {
			server = gtk_entry_get_text (GTK_ENTRY (startup_druid->four_server_entry));
		}
		
		/* Should user be able to set resource, account name and
		 * port?
		 */
		*account = gossip_account_new ("Default",
					       username, NULL, 
					       "Gossip",
					       server, 
					       LM_CONNECTION_DEFAULT_PORT, 
					       FALSE);
	}

	/* FIXME: Set this in some settings-thingy... */
	/* *realname = gtk_entry_get_text (GTK_ENTRY (startup_druid->three_name_entry)); */

	return has_account;
}
				
static void
startup_druid_prepare_page_last (GnomeDruidPage     *page,
				 GnomeDruid         *druid,
				 GossipStartupDruid *startup_druid)
{
	gboolean       has_account;
	const gchar   *jid;
	gchar         *str;
	GossipAccount *account;
	
  	gnome_druid_set_show_finish (GNOME_DRUID (startup_druid->druid), TRUE);

	has_account = startup_druid_get_account_info (startup_druid, &account);
	
	jid = gossip_jid_get_without_resource (gossip_account_get_jid (account));

	if (has_account) {
		str = g_strdup_printf ("%s\n<b>%s</b>.",
				       _("Gossip will now try to use your account:"),
				       jid);
	} else {
		str = g_strdup_printf ("%s\n<b>%s</b>.",
				       _("Gossip will now try to register the account:"),
				       jid);
	}

	gtk_label_set_markup (GTK_LABEL (startup_druid->last_action_label), str);
	
	g_free (str);

	gossip_account_unref (account);
}

static void
startup_druid_last_page_finished (GnomeDruidPage     *page,
				  GnomeDruid         *druid,
				  GossipStartupDruid *startup_druid)
{
	g_print ("last page finished\n");

	if (startup_druid_register_account (startup_druid)) {
		gtk_widget_destroy (startup_druid->window);
	}
}

static void
startup_druid_3_entry_changed (GtkEntry           *entry,
			       GossipStartupDruid *startup_druid)
{
	gboolean     ok = TRUE;
	const gchar *str;
	
	str = gtk_entry_get_text (GTK_ENTRY (startup_druid->three_nick_entry));
	ok &= str && str[0];

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (startup_druid->druid),
					   TRUE,
					   ok,
					   TRUE,
					   FALSE);
}

static void
startup_druid_4_entry_changed (GtkEntry           *entry, 
			       GossipStartupDruid *startup_druid) 
{
	gboolean other;
	gboolean ok = TRUE;

	other = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (startup_druid->four_different_radiobutton));

	if (other) {
		const gchar *str;

		str = gtk_entry_get_text (GTK_ENTRY (startup_druid->four_server_entry));
		
		if (!str || strcmp (str, "") == 0) {
			ok = FALSE;
		}
	} 
	
	gnome_druid_set_buttons_sensitive (GNOME_DRUID (startup_druid->druid),
					   TRUE,
					   ok,
					   TRUE,
					   FALSE);
}

static void
startup_druid_4_different_toggled (GtkToggleButton    *button, 
				   GossipStartupDruid *startup_druid) 
{
	gboolean ok = TRUE;
	
	if (gtk_toggle_button_get_active (button)) {
		const gchar *str;

		gtk_widget_set_sensitive (startup_druid->four_server_optionmenu,
					  FALSE);
		gtk_widget_set_sensitive (startup_druid->four_server_entry,
					  TRUE);
		
		str = gtk_entry_get_text (GTK_ENTRY (startup_druid->four_server_entry));
		if (!str || strcmp (str, "") == 0) {
			ok = FALSE;
		}
	} else {
		gtk_widget_set_sensitive (startup_druid->four_server_optionmenu,
					  TRUE);
		gtk_widget_set_sensitive (startup_druid->four_server_entry,
					  FALSE);
	}

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (startup_druid->druid),
					   TRUE,
					   ok,
					   TRUE,
					   FALSE);
}

void
gossip_startup_druid_run (void)
{
	GossipStartupDruid *startup_druid;
	GladeXML           *glade;

	startup_druid = g_new0 (GossipStartupDruid, 1);
	
	glade = gossip_glade_get_file (
		GLADEDIR "/main.glade",
		"startup_druid_window",
		NULL,
		"startup_druid_window", &startup_druid->window,
		"startup_druid", &startup_druid->druid,
		"1_page", &startup_druid->one_page,
		"2_page", &startup_druid->two_page,
		"2_yes_radiobutton", &startup_druid->two_yes_radiobutton,
		"2_no_radiobutton", &startup_druid->two_no_radiobutton,
		"3_page", &startup_druid->three_page,
		"3_account_label", &startup_druid->three_account_label,
		"3_no_account_label", &startup_druid->three_no_account_label,
		"3_nick_entry", &startup_druid->three_nick_entry,
		"3_name_label", &startup_druid->three_name_label,
		"3_name_entry", &startup_druid->three_name_entry,
		"4_page", &startup_druid->four_page,
		"4_no_account_label", &startup_druid->four_no_account_label,
		"4_account_label", &startup_druid->four_account_label,
		"4_server_optionmenu", &startup_druid->four_server_optionmenu,
		"4_different_radiobutton", &startup_druid->four_different_radiobutton,
		"4_server_entry", &startup_druid->four_server_entry,
		"last_page", &startup_druid->last_page,
		"last_action_label", &startup_druid->last_action_label,
		NULL);
	
	gossip_glade_connect (
		glade, startup_druid,
		"startup_druid_window", "destroy", startup_druid_destroyed,
		"startup_druid", "cancel", startup_druid_cancel,
		"3_nick_entry", "changed", startup_druid_3_entry_changed,
		"4_server_entry", "changed", startup_druid_4_entry_changed,
		"4_different_radiobutton", "toggled", startup_druid_4_different_toggled,
		"last_page", "finish", startup_druid_last_page_finished,
		NULL);
	
	gossip_option_menu_setup (startup_druid->four_server_optionmenu,
				  NULL, NULL,
				  servers[0].label, &servers[0], 
				  servers[1].label, &servers[1], 
				  servers[2].label, &servers[2], 
				  NULL);
		
	g_object_unref (glade);
	
	g_signal_connect_after (startup_druid->one_page, "prepare",
				G_CALLBACK (startup_druid_prepare_page_1),
				startup_druid);
	g_signal_connect_after (startup_druid->two_page, "prepare",
				G_CALLBACK (startup_druid_prepare_page_2),
				startup_druid);
	g_signal_connect_after (startup_druid->three_page, "prepare",
				G_CALLBACK (startup_druid_prepare_page_3),
				startup_druid);
	g_signal_connect_after (startup_druid->four_page, "prepare",
				G_CALLBACK (startup_druid_prepare_page_4),
				startup_druid);
	g_signal_connect_after (startup_druid->last_page, "prepare",
				G_CALLBACK (startup_druid_prepare_page_last),
				startup_druid);

	startup_druid_prepare_page_1 (GNOME_DRUID_PAGE (startup_druid->one_page),
				      GNOME_DRUID (startup_druid->druid),
				      startup_druid);

	gtk_widget_show (startup_druid->window);
	gtk_main ();
}

static gboolean
startup_druid_register_account (GossipStartupDruid *druid)
{
	gboolean       has_account;
	GossipAccount *account;

	has_account = startup_druid_get_account_info (druid, &account); 
	if (!has_account) {
		if (!gossip_register_account (account, GTK_WINDOW (druid->window))) {
			return FALSE;
		}
	}
	
	gossip_account_store (account, NULL);
	gossip_account_set_default (account);

	/* FIXME: We could try and connect just to see if the server/username is
	 * correct.
	 */

	gossip_account_unref (account);
	
	return TRUE;
}

gboolean
gossip_startup_druid_is_needed (void)
{
	GossipAccount *account;

	if (g_getenv ("GOSSIP_FORCE_DRUID")) {
		return TRUE;
	}

	account = gossip_account_get_default ();

	if (!account) {
		return TRUE;
	}

	gossip_account_unref (account);

	return FALSE;
}
