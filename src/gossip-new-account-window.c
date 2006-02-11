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
#include <ctype.h>

#include <libgnomeui/gnome-druid.h>
#include <libgnome/gnome-config.h>
#include <glib/gi18n.h>
#include <loudmouth/loudmouth.h>

#include <libgossip/gossip-protocol.h>

#include "gossip-app.h"
#include "gossip-new-account-window.h"


typedef struct {
	GtkWidget      *window;
	GtkWidget      *druid;
	
	/* Page one */
	GtkWidget      *one_page;

	/* Page two */
	GtkWidget      *two_page;
	GtkWidget      *two_yes_radiobutton;
	GtkWidget      *two_no_radiobutton;
	
	/* Page three */
	GtkWidget      *three_page;
	GtkWidget      *three_username_label;
	GtkWidget      *three_username_new_label;
	GtkWidget      *three_username_entry;
	GtkWidget      *three_example_label;
	GtkWidget      *three_password_label;
	GtkWidget      *three_password_new_label;
	GtkWidget      *three_password_entry;
	GtkWidget      *three_name_label;
	GtkWidget      *three_name_entry;

	/* Page four */
	GtkWidget      *four_page;
	GtkWidget      *four_server_entry;
	GtkWidget      *four_port_entry;
	GtkWidget      *four_ssl_checkbutton;
	GtkWidget      *four_proxy_checkbutton;

	/* Page five */
	GtkWidget      *five_page;
	GtkWidget      *five_name_entry;

	/* Page six */
	GtkWidget      *six_page;
	GtkWidget      *six_progress_image;
	GtkWidget      *six_progress_label;
	GtkWidget      *six_progress_progressbar;
	GtkWidget      *six_error_label;

	/* Last page */
	GtkWidget      *last_page;
	GtkWidget      *last_action_label;

	/* Misc */
	GossipProtocol *selected_protocol;

	GossipAccount  *account;

	guint           register_pulse_id;

	gulong          port_changed_signal_id;
	gboolean        port_changed; 

	gboolean        gtk_main_started;

} GossipNewAccountWindow;


static gboolean new_account_window_progress_pulse_cb   (GtkWidget               *progressbar);
static void     new_account_window_register_cb         (GossipResult             result, 
							const gchar             *err_message,
							GossipNewAccountWindow  *window);
static void     new_account_window_register            (GossipNewAccountWindow  *window,
							GossipAccount           *account);
static gboolean new_account_window_get_account_info    (GossipNewAccountWindow  *window,
							GossipAccount          **account);
static GossipAccountType  
                new_account_window_get_account_type    (GossipNewAccountWindow  *window); 

static void     new_account_window_1_prepare           (GnomeDruidPage          *page,
							GnomeDruid              *druid,
							GossipNewAccountWindow  *window);
static void     new_account_window_2_prepare           (GnomeDruidPage          *page,
							GnomeDruid              *druid,
							GossipNewAccountWindow  *window);
static void     new_account_window_3_prepare           (GnomeDruidPage          *page,
							GnomeDruid              *druid,
							GossipNewAccountWindow  *window);
static void     new_account_window_4_prepare           (GnomeDruidPage          *page,
							GnomeDruid              *druid,
							GossipNewAccountWindow  *window);
static void     new_account_window_5_prepare           (GnomeDruidPage          *page,
							GnomeDruid              *druid,
							GossipNewAccountWindow  *window);
static void     new_account_window_last_page_prepare   (GnomeDruidPage          *page,
							GnomeDruid              *druid,
							GossipNewAccountWindow  *window);
static void     new_account_window_last_page_finished  (GnomeDruidPage          *page,
							GnomeDruid              *druid,
							GossipNewAccountWindow  *window);
static void     new_account_window_port_insert_text    (GtkEditable             *editable,
							gchar                   *new_text,
							gint                     len,
							gint                    *position,
							GossipNewAccountWindow  *window);
static void     new_account_window_port_changed        (GtkEntry                *entry,
							GossipNewAccountWindow  *window);
static void     new_account_window_ssl_toggled         (GtkToggleButton         *button,
							GossipNewAccountWindow  *window);
static void     new_account_window_entry_changed       (GtkEntry                *entry,
							GossipNewAccountWindow  *window);
static void     new_account_window_cancel              (GtkWidget               *widget,
							GossipNewAccountWindow  *window);
static void     new_account_window_destroy             (GtkWidget               *widget,
							GossipNewAccountWindow  *window);



static gboolean 
new_account_window_progress_pulse_cb (GtkWidget *progressbar)
{
	g_return_val_if_fail (progressbar != NULL, FALSE);
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (progressbar));

	return TRUE;
}

static void
new_account_window_register_cb (GossipResult            result, 
				const gchar            *error_message,
				GossipNewAccountWindow *window)
{
	if (window->register_pulse_id != 0) {
		g_source_remove (window->register_pulse_id);
		window->register_pulse_id = 0;
	}

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (window->six_progress_progressbar), 1);

	if (result == GOSSIP_RESULT_OK) {
		gnome_druid_set_page (GNOME_DRUID (window->druid),
				      GNOME_DRUID_PAGE (window->last_page));
		return;
	}
		
	gtk_image_set_from_stock (GTK_IMAGE (window->six_progress_image),
				  GTK_STOCK_DIALOG_ERROR,
				  GTK_ICON_SIZE_BUTTON);
	
	gtk_label_set_markup (GTK_LABEL (window->six_progress_label), 
			      _("Failed to register your new account settings"));
	gtk_label_set_markup (GTK_LABEL (window->six_error_label), error_message);
	
	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   TRUE, FALSE, TRUE, FALSE);	
}	

static void
new_account_window_register (GossipNewAccountWindow *window,
			     GossipAccount          *account)
{
	GossipSession *session;
	GossipVCard   *vcard;
	const gchar   *name;

	gtk_image_set_from_stock (GTK_IMAGE (window->six_progress_image),
				  GTK_STOCK_INFO,
				  GTK_ICON_SIZE_BUTTON);

	gtk_label_set_text (GTK_LABEL (window->six_progress_label),
			    _("Registering account"));
	gtk_label_set_text (GTK_LABEL (window->six_error_label), "");

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (window->six_progress_progressbar), 0);

	window->register_pulse_id = 
		g_timeout_add (50,
			       (GSourceFunc) new_account_window_progress_pulse_cb,
			       window->six_progress_progressbar);

	session = gossip_app_get_session ();

	vcard = gossip_vcard_new ();

	name = gtk_entry_get_text (GTK_ENTRY (window->three_name_entry));
	gossip_vcard_set_name (vcard, name);

        gossip_session_register_account (session,
					 GOSSIP_ACCOUNT_TYPE_JABBER,
					 account,
					 vcard,
					 (GossipRegisterCallback) new_account_window_register_cb,
					 window, 
					 NULL);
}

static gboolean
new_account_window_get_account_info (GossipNewAccountWindow  *window,
				     GossipAccount          **account)
{
	GtkToggleButton *toggle;
	gboolean         has_account;

	const gchar     *username;
	const gchar     *password;
	const gchar     *server;
	const gchar     *port;
	guint16          port_int;
	gboolean         use_ssl;
	gboolean         use_proxy;
	const gchar     *name;
	 
	toggle = GTK_TOGGLE_BUTTON (window->two_yes_radiobutton);
	has_account = gtk_toggle_button_get_active (toggle);

	if (!account) {
		return has_account;
	}

	*account = NULL;

	/* get widget values */
	username = gtk_entry_get_text (GTK_ENTRY (window->three_username_entry));
	password = gtk_entry_get_text (GTK_ENTRY (window->three_password_entry));
	server = gtk_entry_get_text (GTK_ENTRY (window->four_server_entry));
	port = gtk_entry_get_text (GTK_ENTRY (window->four_port_entry));

	name = gtk_entry_get_text (GTK_ENTRY (window->five_name_entry));

	toggle = GTK_TOGGLE_BUTTON (window->four_ssl_checkbutton);
	use_ssl = gtk_toggle_button_get_active (toggle);
	
	toggle = GTK_TOGGLE_BUTTON (window->four_proxy_checkbutton);
	use_proxy = gtk_toggle_button_get_active (toggle);
	
	/* create account */
	port_int = atoi (port);

	*account = g_object_new (GOSSIP_TYPE_ACCOUNT,
				 "name", name,
				 "id", username,
				 "server", server,
				 "password", password,
				 "port", port_int,
				 "use_ssl", use_ssl,
				 "use_proxy", use_proxy,
				 NULL);

	return has_account;
}

static GossipAccountType
new_account_window_get_account_type (GossipNewAccountWindow *window)
{
	/* FIXME: in future we would have an account selector which we
	 * would get the account type from, but for now, we just
	 * support Jabber */
	return GOSSIP_ACCOUNT_TYPE_JABBER;
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
	const gchar     *example;
	
	str = gtk_entry_get_text (GTK_ENTRY (window->three_username_entry));
	ok &= str && strlen (str) > 0;

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid),
					   TRUE,
					   ok,
					   TRUE,
					   FALSE);

	toggle = GTK_TOGGLE_BUTTON (window->two_yes_radiobutton);
	has_account = gtk_toggle_button_get_active (toggle);
	if (has_account) {
		gtk_widget_show (window->three_username_label);
		gtk_widget_hide (window->three_username_new_label);
		gtk_widget_show (window->three_password_label);
		gtk_widget_hide (window->three_password_new_label);
		gtk_widget_hide (window->three_name_entry);
		gtk_widget_hide (window->three_name_label);
	} else {
		gtk_widget_hide (window->three_username_label);
		gtk_widget_show (window->three_username_new_label);
		gtk_widget_hide (window->three_password_label);
		gtk_widget_show (window->three_password_new_label);
		gtk_widget_show (window->three_name_label);
		gtk_widget_show (window->three_name_entry);
	}

	/* update example label based on the type */
	example = gossip_protocol_get_example_username (window->selected_protocol);
	if (example) {
		gchar *str;

		str = g_strdup_printf ("<span size=\"smaller\">%s: %s</span>", 
				       _("Example"), example);
		gtk_label_set_markup (GTK_LABEL (window->three_example_label), str);
		g_free (str);

		gtk_widget_show (window->three_example_label);
	} else {
		gtk_widget_hide (window->three_example_label);
	}

	gtk_widget_grab_focus (window->three_username_entry);
}

static void
new_account_window_4_prepare (GnomeDruidPage         *page,
			      GnomeDruid             *druid,
			      GossipNewAccountWindow *window) 
{
	GtkToggleButton *toggle;
	gboolean         use_ssl;
	guint16          port;
	gchar           *port_str;
	const gchar     *username;
	gchar           *server;

	username = gtk_entry_get_text (GTK_ENTRY (window->three_username_entry));

	server = gossip_protocol_get_default_server (window->selected_protocol, username);
	gtk_entry_set_text (GTK_ENTRY (window->four_server_entry), server);
	g_free (server);


	toggle = GTK_TOGGLE_BUTTON (window->four_ssl_checkbutton);
	use_ssl = gtk_toggle_button_get_active (toggle);
	port = gossip_protocol_get_default_port (window->selected_protocol, 
						 use_ssl);

	port_str = g_strdup_printf ("%d", port);
	g_signal_handler_block (window->four_port_entry, 
				window->port_changed_signal_id);
	gtk_entry_set_text (GTK_ENTRY (window->four_port_entry), port_str);
	g_signal_handler_unblock (window->four_port_entry, 
				  window->port_changed_signal_id);
	g_free (port_str);

	window->port_changed = FALSE;

	gtk_widget_grab_focus (window->four_server_entry);
}

static void
new_account_window_5_prepare (GnomeDruidPage         *page,
			      GnomeDruid             *druid,
			      GossipNewAccountWindow *window) 
{
	gtk_widget_grab_focus (window->five_name_entry);
}

static void
new_account_window_6_prepare (GnomeDruidPage         *page,
			      GnomeDruid             *druid,
			      GossipNewAccountWindow *window) 
{
	GossipAccount *account;
	gboolean       has_account;

	has_account = new_account_window_get_account_info (window, &account); 

	if (window->account) {
		g_object_unref (window->account);
	}

	window->account = account;

	if (!has_account) {
		new_account_window_register (window, account);
	} else {
		gnome_druid_set_page (GNOME_DRUID (window->druid),
				      GNOME_DRUID_PAGE (window->last_page));
/* 		gnome_druid_set_show_finish (GNOME_DRUID (window->druid), TRUE); */
/* 		gnome_druid_set_buttons_sensitive (druid, FALSE, TRUE, FALSE, FALSE); */
	}
}

static void
new_account_window_last_page_prepare (GnomeDruidPage         *page,
				      GnomeDruid             *druid,
				      GossipNewAccountWindow *window)
{
	gnome_druid_set_show_finish (GNOME_DRUID (window->druid), TRUE);
	gnome_druid_set_buttons_sensitive (druid, FALSE, TRUE, FALSE, FALSE);
}

static void
new_account_window_last_page_finished (GnomeDruidPage         *page,
				       GnomeDruid             *druid,
				       GossipNewAccountWindow *window)
{
	GossipSession        *session;
	GossipAccountManager *manager;

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	gossip_account_manager_add (manager, window->account);
	gossip_account_manager_store (manager);

	if (gossip_account_manager_get_count (manager) < 1) {
		gossip_account_manager_set_default (manager, window->account);
	}

	gtk_widget_destroy (window->window);
}

static void  
new_account_window_port_insert_text (GtkEditable            *editable,
				     gchar                  *new_text,
				     gint                    len,
				     gint                   *position,
				     GossipNewAccountWindow *window)
{
	gchar *ch; 
	gint   i;
	
	for (i = 0; i < len; ++i) {
		ch = new_text + i;

		if (!isdigit (*ch)) {
			g_signal_stop_emission_by_name (editable, "insert-text");
			return;
		}
	}
}

static void
new_account_window_port_changed (GtkEntry               *entry,
				 GossipNewAccountWindow *window)
{
	window->port_changed = TRUE;
}

static void
new_account_window_ssl_toggled (GtkToggleButton        *button,
				GossipNewAccountWindow *window)
{
	guint16   port;
	gchar    *port_str;
	gboolean  use_ssl;

	if (window->port_changed) {
		return;
	}

	use_ssl = gtk_toggle_button_get_active (button);

	port = gossip_protocol_get_default_port (window->selected_protocol, 
						 use_ssl);
	
	port_str = g_strdup_printf ("%d", port);
	g_signal_handler_block (window->four_port_entry, 
				window->port_changed_signal_id);
	gtk_entry_set_text (GTK_ENTRY (window->four_port_entry), port_str);
	g_signal_handler_unblock (window->four_port_entry, 
				  window->port_changed_signal_id);
	g_free (port_str);
}

static void
new_account_window_entry_changed (GtkEntry               *entry,
				  GossipNewAccountWindow *window)
{
	const gchar *str;
	gboolean     ok = TRUE;
	
	str = gtk_entry_get_text (entry);
	ok &= str && strlen (str) > 0;

	if (entry == GTK_ENTRY (window->three_username_entry)) {
		ok &= gossip_protocol_is_valid_username (window->selected_protocol, str);
	}

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid),
					   TRUE,
					   ok,
					   TRUE,
					   FALSE);
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

	if (window->selected_protocol) {
		g_object_unref (window->selected_protocol);
	}

	if (window->register_pulse_id != 0) {
		g_source_remove (window->register_pulse_id);
		window->register_pulse_id = 0;
	}

	if (window->account) {
		g_object_unref (window->account);
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
	GossipAccountType       type;
	GladeXML               *glade;
	GnomeDruid             *druid;
	gboolean                ssl_supported;

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
				       "3_username_label", &window->three_username_label,
				       "3_username_new_label", &window->three_username_new_label,
				       "3_username_entry", &window->three_username_entry,
				       "3_password_label", &window->three_password_label,
				       "3_password_new_label", &window->three_password_new_label,
				       "3_password_entry", &window->three_password_entry,
				       "3_example_label", &window->three_example_label,
				       "3_name_label", &window->three_name_label,
				       "3_name_entry", &window->three_name_entry,
				       "4_page", &window->four_page,
				       "4_server_entry", &window->four_server_entry,
				       "4_port_entry", &window->four_port_entry,
				       "4_ssl_checkbutton", &window->four_ssl_checkbutton,
				       "4_proxy_checkbutton", &window->four_proxy_checkbutton,
				       "5_page", &window->five_page,
				       "5_name_entry", &window->five_name_entry,
				       "6_page", &window->six_page,
				       "6_progress_image", &window->six_progress_image,
				       "6_progress_label", &window->six_progress_label,
				       "6_progress_progressbar", &window->six_progress_progressbar,
				       "6_error_label", &window->six_error_label,
				       "last_page", &window->last_page,
				       "last_action_label", &window->last_action_label,
				       NULL);

	gossip_glade_connect (glade, 
			      window,
			      "new_account_window", "destroy", new_account_window_destroy,
			      "druid", "cancel", new_account_window_cancel,
			      "3_username_entry", "changed", new_account_window_entry_changed,
			      "3_password_entry", "changed", new_account_window_entry_changed,
			      "4_server_entry", "changed", new_account_window_entry_changed,
			      "5_name_entry", "changed", new_account_window_entry_changed,
			      "4_port_entry", "insert_text", new_account_window_port_insert_text,
			      "4_ssl_checkbutton", "toggled", new_account_window_ssl_toggled,
			      "last_page", "finish", new_account_window_last_page_finished,
			      NULL);

	g_object_unref (glade);

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
	g_signal_connect_after (window->five_page, "prepare",
				G_CALLBACK (new_account_window_5_prepare),
				window);
	g_signal_connect_after (window->six_page, "prepare",
				G_CALLBACK (new_account_window_6_prepare),
				window);
	g_signal_connect_after (window->last_page, "prepare",
				G_CALLBACK (new_account_window_last_page_prepare),
				window);

	window->port_changed_signal_id = 
		g_signal_connect (window->four_port_entry, "changed", 
				  G_CALLBACK (new_account_window_port_changed),
				  window);

	new_account_window_1_prepare (GNOME_DRUID_PAGE (window->one_page),
				      GNOME_DRUID (window->druid),
				      window);

	druid = GNOME_DRUID (window->druid);
	
	g_object_set (G_OBJECT (druid->next), 
		      "can-default", TRUE,
		      "has-default", TRUE,
		      NULL);

	/* set default account type */
	type = new_account_window_get_account_type (window);
	window->selected_protocol = gossip_protocol_new_from_account_type (type);

	/* can we use ssl */
	ssl_supported = gossip_protocol_is_ssl_supported (window->selected_protocol);
	gtk_widget_set_sensitive (window->four_ssl_checkbutton, ssl_supported);

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
