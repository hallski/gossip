/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio HB
 * Copyright (C) 2003 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2003 Richard Hult <richard@imendio.com>
 * Copyright (C) 2003 CodeFactory AB
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
#include <libgnome/gnome-i18n.h>
#include <loudmouth/loudmouth.h>
#include "gossip-utils.h"
#include "gossip-app.h"
#include "gossip-startup-druid.h"

typedef enum {
	SERVER_JABBER_ORG,
	SERVER_JABBER_COM,
	SERVER_IMENDIO_COM
} Server;

typedef struct {
	gchar  *label;
	gchar  *address;
	Server  server;
} ServerEntry;

static ServerEntry servers[] = {
	{ "Jabber.org", "jabber.org", SERVER_JABBER_ORG },
	{ "Jabber.com", "jabber.com", SERVER_JABBER_COM },
	{ "Imendio.com", "imendio.com", SERVER_IMENDIO_COM }
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
		*account = gossip_account_new (_("Default"), username, NULL, 
					       "Gossip", server, 
					       LM_CONNECTION_DEFAULT_PORT);
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
	gchar         *label_text;
	GossipAccount *account;
	
  	gnome_druid_set_show_finish (GNOME_DRUID (startup_druid->druid), TRUE);

	has_account = startup_druid_get_account_info (startup_druid, &account);
	
	jid = gossip_jid_get_without_resource (gossip_account_get_jid (account));

	if (has_account) {
		label_text = g_strdup_printf (_("Gossip will now try to use your account:\n<b>%s</b>"), jid);
	} else {
		label_text = g_strdup_printf (_("Gossip will now try to register the account:\n<b>%s</b>."), jid);
	}

	gtk_label_set_markup (GTK_LABEL (startup_druid->last_action_label),
			      label_text);
	
	g_free (label_text);
	gossip_account_store (account, NULL);
	gossip_account_unref (account);
}

static void
startup_druid_last_page_finished (GnomeDruidPage     *page,
				  GnomeDruid         *druid,
				  GossipStartupDruid *startup_druid)
{
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

typedef struct {
	LmConnection  *connection;
	GossipAccount *account;
	GtkWidget     *dialog;

	gboolean       success;
	gchar         *error_message;
} RegisterAccountData;

static void
startup_druid_progress_dialog_destroy_cb (GtkWidget           *widget,
					  RegisterAccountData *data)
{
	data->dialog = NULL;
}
	
static LmHandlerResult
startup_druid_register_handler (LmMessageHandler    *handler,
				LmConnection        *connection,
				LmMessage           *msg,
				RegisterAccountData *data)
{
	LmMessageSubType  sub_type;
	LmMessageNode    *node;

	sub_type = lm_message_get_sub_type (msg);
	switch (sub_type) {
	case LM_MESSAGE_SUB_TYPE_RESULT:
		data->success = TRUE;

		if (data->dialog) {
			gtk_dialog_response (GTK_DIALOG (data->dialog),
					     GTK_RESPONSE_NONE);
		}
		
		break;

	case LM_MESSAGE_SUB_TYPE_ERROR:
	default:
		node = lm_message_node_find_child (msg->node, "error");
		if (node) {
			data->error_message = g_strdup (lm_message_node_get_value (node));
		} else {
			data->error_message = g_strdup (_("Unknown error"));
		}
		
		if (data->dialog) {
			gtk_dialog_response (GTK_DIALOG (data->dialog),
					     GTK_RESPONSE_NONE);
		}
		
		break;
	}

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}	

static void
startup_druid_connection_open_cb (LmConnection        *connection,
				  gboolean             result,
				  RegisterAccountData *data)
{
	GossipJID        *jid;
	LmMessage        *msg;
	LmMessageNode    *node;
	LmMessageHandler *handler;
	
	if (result != TRUE) {
		data->error_message = g_strdup ("Could not connect to the server.");

		if (data->dialog) {
			gtk_dialog_response (GTK_DIALOG (data->dialog),
					     GTK_RESPONSE_NONE);
		}
		return;
	}

	jid = gossip_account_get_jid (data->account);
	
	msg = lm_message_new_with_sub_type (gossip_jid_get_without_resource (jid),
					    LM_MESSAGE_TYPE_IQ,
					    LM_MESSAGE_SUB_TYPE_SET);
	
	node = lm_message_node_add_child (msg->node, "query", NULL);
	lm_message_node_set_attribute (node, "xmlns", "jabber:iq:register");
	
	lm_message_node_add_child (node, "username", gossip_jid_get_part_name (jid));
	lm_message_node_add_child (node, "password", "foofoo");

	handler = lm_message_handler_new ((LmHandleMessageFunction) startup_druid_register_handler,
					  data, NULL);

	lm_connection_send_with_reply (data->connection, msg, handler, NULL);
	lm_message_unref (msg);
	
	gossip_jid_unref (jid);
}

static gboolean
startup_druid_register_account (GossipStartupDruid *druid)
{
	gboolean             has_account;
	GossipJID           *jid;
	RegisterAccountData *data;
	GossipAccount       *account;
	gint                 response;
	gboolean             retval;

	has_account = startup_druid_get_account_info (druid, &account); 
	if (has_account) {
		/* Don't need to register. */
		gossip_account_store (account, NULL);

		/* FIXME: We could try and connect just to see if the
		 * server/username is correct.
		 */

		gossip_account_unref (account);
		
		return TRUE;
	}

	jid = gossip_account_get_jid (account);
	
	data = g_new0 (RegisterAccountData, 1);

	data->account = account;
	data->connection = lm_connection_new (account->server);
	
	data->dialog = gtk_message_dialog_new (GTK_WINDOW (druid->window),
					       GTK_DIALOG_MODAL |
					       GTK_DIALOG_DESTROY_WITH_PARENT,
					       GTK_MESSAGE_INFO,
					       GTK_BUTTONS_CANCEL,
					       "%s\n<b>%s</b>",
					       _("Registering account"),
					       gossip_jid_get_without_resource (jid));

	g_object_set (GTK_MESSAGE_DIALOG (data->dialog)->label,
		      "use-markup", TRUE,
		      "wrap", FALSE,
		      NULL);
	
	g_signal_connect (data->dialog,
			  "destroy",
			  G_CALLBACK (startup_druid_progress_dialog_destroy_cb),
			  data);
	
	lm_connection_open (data->connection,
			    (LmResultFunction) startup_druid_connection_open_cb,
			    data, NULL, NULL);

	response = gtk_dialog_run (GTK_DIALOG (data->dialog));
	switch (response) {
	case GTK_RESPONSE_CANCEL:
		/* FIXME: cancel pending replies... */
		break;

	default:
		break;
	}

 	if (data->dialog) {
		gtk_widget_hide (data->dialog);
	}
	
	if (data->success) {
		GtkWidget *dialog;
		
		dialog = gtk_message_dialog_new (GTK_WINDOW (druid->window),
						 GTK_DIALOG_MODAL |
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_INFO,
						 GTK_BUTTONS_CLOSE,
						 "%s\n<b>%s</b>",
						 _("Successfully registered the account"),
						 gossip_jid_get_without_resource (jid));

		g_object_set (GTK_MESSAGE_DIALOG (dialog)->label,
			      "use-markup", TRUE,
			      "wrap", FALSE,
			      NULL);
		
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	
		/* Add account information */
		gossip_account_store (data->account, NULL);

		retval = TRUE;
	} else {
		GtkWidget *dialog;
		gchar     *str;

		if (data->error_message) {
			str = g_strdup_printf ("%s\n<b>%s</b>\n\n%s\n%s",
					       _("Failed registering the account"),
					       gossip_jid_get_without_resource (jid),
					       _("Reason:"),
					       data->error_message);
		} else {
			str = g_strdup_printf ("%s\n<b>%s</b>",
					       _("Failed registering the account"),
					       gossip_jid_get_without_resource (jid));
		}

		dialog = gtk_message_dialog_new (GTK_WINDOW (druid->window),
						 GTK_DIALOG_MODAL |
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_INFO,
						 GTK_BUTTONS_CLOSE,
						 str);
		g_free (str);

		g_object_set (GTK_MESSAGE_DIALOG (dialog)->label,
			      "use-markup", TRUE,
			      "wrap", FALSE,
			      NULL);
		
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_free (data->error_message);

		retval = FALSE;
	}		

	gossip_jid_unref (jid);
	gossip_account_unref (account);
	
	gtk_widget_destroy (data->dialog);
	g_free (data);

	return retval;
}
