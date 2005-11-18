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


/* default servers to fall back on */
static const gchar *servers[] = {
	"jabber.com",
	"jabber.cn",
	"jabber.cz",
	"jabber.dk",
	"jabber.fr",
	"jabber.hu",
	"jabber.no",
	"jabber.org",
	"jabber.org.uk",
	"jabber.ru",
	"jabber.sk",
	NULL
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
	GtkWidget    *four_server_comboboxentry;
	GtkWidget    *four_server_entry;
	GtkWidget    *four_ssl_checkbutton;
	GtkWidget    *four_proxy_checkbutton;

	/* Last page */
	GtkWidget    *last_page;
	GtkWidget    *last_action_label;
} GossipNewAccountWindow;


static void     new_account_window_setup_servers      (GossipNewAccountWindow  *window);
static gboolean new_account_window_setup_account      (GossipNewAccountWindow  *window);
static gboolean new_account_window_get_account_info   (GossipNewAccountWindow  *window,
						       GossipAccount          **account);
static void     new_account_window_1_prepare          (GnomeDruidPage          *page,
						       GnomeDruid              *druid,
						       GossipNewAccountWindow  *window);
static void     new_account_window_2_prepare          (GnomeDruidPage          *page,
						       GnomeDruid              *druid,
						       GossipNewAccountWindow  *window);
static void     new_account_window_3_prepare          (GnomeDruidPage          *page,
						       GnomeDruid              *druid,
						       GossipNewAccountWindow  *window);
static void     new_account_window_3_entry_changed    (GtkEntry                *entry,
						       GossipNewAccountWindow  *window);
static void     new_account_window_4_entry_changed    (GtkEntry                *entry,
						       GossipNewAccountWindow  *window);
static void     new_account_window_last_page_prepare  (GnomeDruidPage          *page,
						       GnomeDruid              *druid,
						       GossipNewAccountWindow  *window);
static void     new_account_window_last_page_finished (GnomeDruidPage          *page,
						       GnomeDruid              *druid,
						       GossipNewAccountWindow  *window);
static void     new_account_window_cancel             (GtkWidget               *widget,
						       GossipNewAccountWindow  *window);
static void     new_account_window_destroy            (GtkWidget               *widget,
						       GossipNewAccountWindow  *window);



static void
new_account_window_setup_servers (GossipNewAccountWindow *window)
{
	GtkComboBox  *combobox;
	GtkListStore *store;
 	GtkTreeIter   iter; 
 	gint          i = 0; 

	combobox = GTK_COMBO_BOX (window->four_server_comboboxentry);

	store = gtk_list_store_new (1, G_TYPE_STRING);
	
	gtk_combo_box_set_model (combobox,
				 GTK_TREE_MODEL (store));

	gtk_combo_box_entry_set_text_column (GTK_COMBO_BOX_ENTRY (combobox), 0);

	while (servers[i]) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, servers[i++], -1);
	}

	g_object_unref (store);
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

static gboolean
new_account_window_get_account_info (GossipNewAccountWindow  *window,
				     GossipAccount          **account)
{
	GtkToggleButton *toggle;
	gboolean         has_account;

	toggle = GTK_TOGGLE_BUTTON (window->two_yes_radiobutton);
	has_account = gtk_toggle_button_get_active (toggle);

	if (account) {
		const gchar *username;
		const gchar *server = "";
		gchar       *name;
		gchar       *id;
		guint16      port;
		gboolean     ssl;
		gboolean     proxy;

		*account = NULL;

		username = gtk_entry_get_text (GTK_ENTRY (window->three_nick_entry));
		server = gtk_entry_get_text (GTK_ENTRY (window->four_server_entry));
		
		name = g_strdup_printf ("%s at %s", 
				       username, server);

		toggle = GTK_TOGGLE_BUTTON (window->four_ssl_checkbutton);
		ssl = gtk_toggle_button_get_active (toggle);

		toggle = GTK_TOGGLE_BUTTON (window->four_proxy_checkbutton);
		proxy = gtk_toggle_button_get_active (toggle);

		/* FIXME: Jabber specific - start */
		id = g_strdup_printf ("%s@%s/%s",
				      username, server, _("Home"));
		port = ssl ? 5223 : 5222;
		/* FIXME: Jabber specific - end */
		
		*account = g_object_new (GOSSIP_TYPE_ACCOUNT,
					 "name", name,
					 "id", id,
					 "server", server, 
					 "port", port,
					 "use_ssl", ssl,
					 "use_proxy", proxy,
					 NULL);

		g_free (name);
		g_free (id);
	}

	/* FIXME: Set this in some settings-thingy... */
	/* *realname = gtk_entry_get_text (GTK_ENTRY (window->three_name_entry)); */

	return has_account;
}

static void
new_account_window_1_prepare (GnomeDruidPage         *page, 
			      GnomeDruid             *druid, 
			      GossipNewAccountWindow *window)
{
	gnome_druid_set_buttons_sensitive (druid, FALSE, TRUE, TRUE, FALSE);
}

static void
new_account_window_2_prepare (GnomeDruidPage         *page,
			      GnomeDruid             *druid, 
			      GossipNewAccountWindow *window)
{
	gboolean first_time;

	first_time = gossip_new_account_window_is_needed ();
	gnome_druid_set_buttons_sensitive (druid, first_time, TRUE, TRUE, FALSE);
}

static void
new_account_window_3_prepare (GnomeDruidPage         *page,
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
new_account_window_4_prepare (GnomeDruidPage         *page,
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

	gtk_widget_grab_focus (window->four_server_entry);
}

static void
new_account_window_4_entry_changed (GtkEntry               *entry, 
				    GossipNewAccountWindow *window) 
{
	const gchar *str;
	gboolean     ok = TRUE;

	str = gtk_entry_get_text (GTK_ENTRY (window->four_server_entry));

	ok &= (str != NULL);
	ok &= (strlen (str) > 0);
	
	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid),
					   TRUE,
					   ok,
					   TRUE,
					   FALSE);
}

static void
new_account_window_last_page_prepare (GnomeDruidPage         *page,
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
	if (new_account_window_setup_account (window)) {
		gtk_widget_destroy (window->window);
	}
}

static void
new_account_window_cancel (GtkWidget              *widget,
			   GossipNewAccountWindow *window)
{
	gtk_widget_destroy (window->window);
}

static void
new_account_window_destroy (GtkWidget              *widget,
			    GossipNewAccountWindow *window)
{
	if (window->gtk_main_started) {
		gtk_main_quit ();
	}

	g_free (window);
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
gossip_new_account_window_show (GtkWindow *parent)
{
	GossipNewAccountWindow *window;
	GladeXML               *glade;
	GnomeDruid             *druid;

	window = g_new0 (GossipNewAccountWindow, 1);
	
	glade = gossip_glade_get_file (GLADEDIR "/main.glade",
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
				       "4_server_comboboxentry", &window->four_server_comboboxentry,
				       "4_ssl_checkbutton", &window->four_ssl_checkbutton,
				       "4_proxy_checkbutton", &window->four_proxy_checkbutton,
				       "last_page", &window->last_page,
				       "last_action_label", &window->last_action_label,
				       NULL);

	gossip_glade_connect (glade, 
			      window,
			      "new_account_window", "destroy", new_account_window_destroy,
			      "druid", "cancel", new_account_window_cancel,
			      "3_nick_entry", "changed", new_account_window_3_entry_changed,
			      "last_page", "finish", new_account_window_last_page_finished,
			      NULL);

	g_object_unref (glade);

	window->four_server_entry = GTK_BIN (window->four_server_comboboxentry)->child;

	g_signal_connect (window->four_server_entry, "changed",
			  G_CALLBACK (new_account_window_4_entry_changed), window);

	g_signal_connect_after (window->one_page, "prepare",
				G_CALLBACK (new_account_window_1_prepare),
				window);
	g_signal_connect_after (window->two_page, "prepare",
				G_CALLBACK (new_account_window_2_prepare),
				window);
	g_signal_connect_after (window->three_page, "prepare",
				G_CALLBACK (new_account_window_3_prepare),
				window);
	g_signal_connect_after (window->four_page, "prepare",
				G_CALLBACK (new_account_window_4_prepare),
				window);
	g_signal_connect_after (window->last_page, "prepare",
				G_CALLBACK (new_account_window_last_page_prepare),
				window);

	new_account_window_1_prepare (GNOME_DRUID_PAGE (window->one_page),
				      GNOME_DRUID (window->druid),
				      window);

	druid = GNOME_DRUID (window->druid);
	
	g_object_set (G_OBJECT (druid->next), 
		      "can-default", TRUE,
		      "has-default", TRUE,
		      NULL);

	/* set up list */
	new_account_window_setup_servers (window);
	gtk_combo_box_set_active (GTK_COMBO_BOX (window->four_server_comboboxentry), 0);

	/* can we use ssl */
	gtk_widget_set_sensitive (window->four_ssl_checkbutton, lm_ssl_is_supported ());

	/* set the position, either center on screen on startup, or on the parent window */
	if (parent != NULL) {
		gtk_window_set_transient_for (GTK_WINDOW (window->window), 
					      parent);
	} else {
		gtk_window_set_position (GTK_WINDOW (window->window), 
					 GTK_WIN_POS_CENTER);
	}

	/* show window */
	gtk_widget_show (window->window);

	if (!gossip_new_account_window_is_needed ()) {
		/* skip the first page */
		gnome_druid_set_page (GNOME_DRUID (window->druid),
				      GNOME_DRUID_PAGE (window->two_page));
	} else {
		/* FIXME: disable the back button on the first page, there is
		   a bug here where it doesn't get set right if we call this
		   function before we set the combo box index active??? */
		gnome_druid_set_buttons_sensitive (druid, FALSE, TRUE, TRUE, FALSE);

		window->gtk_main_started = TRUE;
		gtk_main ();
	}
}
