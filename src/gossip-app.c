/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003      Imendio HB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
 * Copyright (C) 2002-2003 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2002-2003 CodeFactory AB
 * Copyright (C) 2003      Kevin Dougherty <gossip@kdough.net>
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
#include <sys/utsname.h>
#include <stdlib.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <loudmouth/loudmouth.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-about.h>
#include "eggtrayicon.h"
#include "gossip-add-contact.h"
#include "gossip-jid.h"
#include "gossip-marshal.h"
#include "gossip-utils.h"
#include "gossip-group-chat.h"
#include "gossip-chat.h"
#include "gossip-account.h"
#include "gossip-connect-dialog.h"
#include "gossip-join-dialog.h"
#include "gossip-message.h"
#include "gossip-sound.h"
#include "gossip-idle.h"
#include "gossip-startup-druid.h"
#include "gossip-preferences.h"
#include "gossip-app.h"

#define DEFAULT_RESOURCE "Gossip"
#define ADD_CONTACT_RESPONSE_ADD 1

#define d(x)

extern GConfClient *gconf_client;

struct _GossipAppPriv {
	LmConnection        *connection;
	GtkWidget           *window;
	GtkWidget           *option_menu;

	EggTrayIcon         *tray_icon;
	GtkWidget           *tray_event_box;
	GtkWidget           *tray_image;
	GtkTooltips         *tray_tooltips;
	GList               *tray_flash_icons;
	guint                tray_flash_timeout_id;
	GtkWidget           *popup_menu;
	GtkWidget           *show_popup_item;
	GtkWidget           *hide_popup_item;
	
	GossipRoster        *roster;

	GossipAccount       *account;

	GossipJID           *jid;

	/* The status to set when we get a connection. */
	GossipStatus         status_to_set_on_connect;

	gboolean             is_auto_away;
	
	/* Dialogs that we only have one of at a time. */
	GossipJoinDialog    *join_dialog;
	GtkWidget           *about;
	GtkWidget           *preferences_dialog;

	/* Widges that are enabled when we're connected. */
	GList               *enabled_connected_widgets;

	/* Widges that are enabled when we're disconnected. */
	GList               *enabled_disconnected_widgets;
};

typedef struct {
	GossipApp   *app;
	GCompletion *completion;
	GList       *jids;
	GList       *jid_strings;

	guint        complete_idle_id;
	gboolean     touched;
	
	GtkWidget   *combo;
	GtkWidget   *entry;
	GtkWidget   *dialog;
} CompleteJIDData;

enum {
	CONNECTED,
	DISCONNECTED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void     app_class_init                       (GossipAppClass     *klass);
static void     app_init                             (GossipApp          *app);
static void     app_finalize                         (GObject            *object);
static void     app_main_window_destroy_cb           (GtkWidget          *window,
						      GossipApp          *app);
static void     app_user_activated_cb                (GossipRoster       *roster,
						      GossipJID          *jid,
						      GossipApp          *app);
static gboolean app_idle_check_cb                    (GossipApp          *app);
static void     app_set_status                       (GossipApp          *app,
						      GossipStatus        status);
static void     app_quit_cb                          (GtkWidget          *window,
						      GossipApp          *app);
static void     app_send_chat_message_cb             (GtkWidget          *widget,
						      gpointer            user_data);
static void     app_popup_send_chat_message_cb       (GtkWidget          *widget,
						      gpointer            user_data);
static void     app_add_contact_cb                   (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_preferences_cb                   (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_about_cb                         (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_join_group_chat_cb               (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_connection_open_cb               (LmConnection       *connection,
						      gboolean            result,
						      GossipApp          *app);
static void     app_authentication_cb                (LmConnection       *connection,
						      gboolean            result,
						      GossipApp          *app);
static void     app_client_disconnected_cb           (LmConnection       *connection,
						      LmDisconnectReason  reason,
						      GossipApp          *app);
static void     app_show_hide_activate_cb            (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_connect_cb                       (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_disconnect_cb                    (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_status_item_activated_cb         (GtkMenuItem        *item,
						      GossipApp          *app);


static LmHandlerResult
app_message_handler                                  (LmMessageHandler   *handler,
						      LmConnection       *connection,
						      LmMessage          *m,
						      GossipApp          *app);
static LmHandlerResult
app_presence_handler                                 (LmMessageHandler   *handler,
						      LmConnection       *connection,
						      LmMessage          *m,
						      GossipApp          *app);
static LmHandlerResult
app_iq_handler                                       (LmMessageHandler   *handler,
						      LmConnection       *connection,
						      LmMessage          *m,
						      GossipApp          *app);
static void     app_handle_subscription_request      (GossipApp          *app,
						      LmMessage          *m);
static void     app_set_status_indicator             (GossipApp          *app,
						      GossipStatus        status);
static void     app_tray_icon_destroy_cb             (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_create_tray_icon                 (GossipApp          *app);
static void     app_tray_icon_set_status             (GossipApp          *app,
						      GossipStatus        status);
static void     app_create_connection                (GossipApp          *app);
static void     app_disconnect                       (GossipApp          *app);
static void     app_setup_conn_dependent_menu_items  (GossipApp          *app,
						      GladeXML           *glade);
static void     app_update_conn_dependent_menu_items (GossipApp          *app);
static void     app_complete_jid_response_cb         (GtkWidget          *dialog,
						      gint                response,
						      CompleteJIDData    *data);
static gboolean app_complete_jid_idle                (CompleteJIDData    *data);
static void     app_complete_jid_insert_text_cb      (GtkEntry           *entry,
						      const gchar        *text,
						      gint                length,
						      gint               *position,
						      CompleteJIDData    *data);
static gboolean app_complete_jid_key_press_event_cb  (GtkEntry           *entry,
						      GdkEventKey        *event,
						      CompleteJIDData    *data);
static void     app_complete_jid_activate_cb         (GtkEntry           *entry,
						      CompleteJIDData    *data);
static gchar *  app_complete_jid_to_string           (gpointer            data);
static void     app_toggle_visibility                (void);
static void     app_tray_icon_push_message           (LmMessage          *m);
static gboolean app_tray_icon_pop_message            (void);
static void     app_tray_icon_update_tooltip         (void);


static GObjectClass *parent_class;
static GossipApp *app;


GType
gossip_app_get_type (void)
{
	static GType object_type = 0;
	
	if (!object_type) {
		static const GTypeInfo object_info = {
			sizeof (GossipAppClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) app_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GossipApp),
			0,              /* n_preallocs */
			(GInstanceInitFunc) app_init,
		};

		object_type = g_type_register_static (G_TYPE_OBJECT,
                                                      "GossipApp", 
                                                      &object_info, 0);
	}

	return object_type;
}

static void
app_class_init (GossipAppClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);
	
        parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
        
        object_class->finalize = app_finalize;

	signals[CONNECTED] = 
		g_signal_new ("connected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0, 
			      NULL, NULL,
			      gossip_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[DISCONNECTED] = 
		g_signal_new ("disconnected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0, 
			      NULL, NULL,
			      gossip_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
app_init (GossipApp *app)
{
        GossipAppPriv *priv;
	GladeXML      *glade;
	GtkWidget     *sw;
	GtkWidget     *item;

        priv = g_new0 (GossipAppPriv, 1);
        app->priv = priv;

	priv->account = gossip_account_get_default ();
	priv->status_to_set_on_connect = GOSSIP_STATUS_AVAILABLE;

	glade = gossip_glade_get_file (GLADEDIR "/main.glade",
				       "main_window",
				       NULL,
				       "main_window", &priv->window,
				       "status_optionmenu", &priv->option_menu,
				       "roster_scrolledwindow", &sw,
				       NULL);

	app_setup_conn_dependent_menu_items (app, glade);
	
	gossip_glade_connect (glade,
			      app,
			      "file_quit", "activate", app_quit_cb,
			      "actions_connect", "activate", app_connect_cb,
			      "actions_disconnect", "activate", app_disconnect_cb,
			      "actions_join_group_chat", "activate", app_join_group_chat_cb,
			      "actions_send_chat_message", "activate", app_send_chat_message_cb,
			      "actions_add_contact", "activate", app_add_contact_cb,
			      "edit_preferences", "activate", app_preferences_cb,
			      "help_about", "activate", app_about_cb,
			      "main_window", "destroy", app_main_window_destroy_cb,
			      NULL);

	g_object_unref (glade);

	if (!priv->account || g_getenv ("GOSSIP_FORCE_DRUID")) {
		gossip_startup_druid_run ();
		priv->account = gossip_account_get_default ();
	}
	
	if (priv->account) {
		app_create_connection (app);
	}
	
	priv->roster = gossip_roster_new (app);

	gtk_widget_show (GTK_WIDGET (priv->roster));
	gtk_container_add (GTK_CONTAINER (sw),
			   GTK_WIDGET (priv->roster));

	g_signal_connect (priv->roster,
			  "user_activated",
			  G_CALLBACK (app_user_activated_cb),
			  app);

	gossip_status_menu_setup (priv->option_menu,
				  G_CALLBACK (app_status_item_activated_cb),
				  app,

				  _("Available"),
				  gossip_status_to_icon_filename (GOSSIP_STATUS_AVAILABLE),
				  GOSSIP_STATUS_AVAILABLE,
				  
				  _("Free to chat"),
				  gossip_status_to_icon_filename (GOSSIP_STATUS_FREE),
				  GOSSIP_STATUS_FREE,
				  
				  _("Busy"),
				  gossip_status_to_icon_filename (GOSSIP_STATUS_BUSY),
				  GOSSIP_STATUS_BUSY,
				  
				  _("Away"),
				  gossip_status_to_icon_filename (GOSSIP_STATUS_AWAY),
				  GOSSIP_STATUS_AWAY,
				  
				  _("Extended away"),
				  gossip_status_to_icon_filename (GOSSIP_STATUS_EXT_AWAY),
				  GOSSIP_STATUS_EXT_AWAY,
				  
				  "", /* Separator */
				  NULL,
				  0,
				  
				  _("Offline"),
				  gossip_status_to_icon_filename (GOSSIP_STATUS_OFFLINE),
				  GOSSIP_STATUS_OFFLINE,
				  
				  NULL);

	/* Popup menu. */
        priv->popup_menu = gtk_menu_new ();

        item = gtk_image_menu_item_new_with_mnemonic (_("_Show Contact List"));
	priv->show_popup_item = item;
        g_signal_connect (item,
			  "activate",
			  G_CALLBACK (app_show_hide_activate_cb),
			  app);
        gtk_widget_hide (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);

	item = gtk_image_menu_item_new_with_mnemonic (_("_Hide Contact List"));
	priv->hide_popup_item = item;
        g_signal_connect (item,
			  "activate",
			  G_CALLBACK (app_show_hide_activate_cb),
			  app);
        gtk_widget_show (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);

	item = gtk_separator_menu_item_new ();
        gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);

	item = gtk_image_menu_item_new_with_mnemonic (_("Send _Chat message..."));
        g_signal_connect (item,
			  "activate",
			  G_CALLBACK (app_popup_send_chat_message_cb),
			  app);
        gtk_widget_show (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);

	priv->enabled_connected_widgets = g_list_prepend (priv->enabled_connected_widgets,
							  item);
	
	item = gtk_separator_menu_item_new ();
        gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);
	
	item = gtk_image_menu_item_new_with_mnemonic (_("_Quit"));
        g_signal_connect (item,
			  "activate",
			  G_CALLBACK (app_quit_cb),
			  app);
        gtk_widget_show (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);

	app_create_tray_icon (app);
	app_set_status_indicator (app, GOSSIP_STATUS_OFFLINE);

	app_update_conn_dependent_menu_items (app);

	/* Start the idle time checker. */
	g_timeout_add (5000, (GSourceFunc) app_idle_check_cb, app);
	
	priv->about = NULL;
	gtk_widget_show (priv->window);
}

static void
app_finalize (GObject *object)
{
        GossipApp     *app;
        GossipAppPriv *priv;
	
        app = GOSSIP_APP (object);
        priv = app->priv;

	gossip_account_unref (priv->account);

	g_list_free (priv->enabled_connected_widgets);
	g_list_free (priv->enabled_disconnected_widgets);
	
	g_free (priv);
	app->priv = NULL;

        if (G_OBJECT_CLASS (parent_class)->finalize) {
                (* G_OBJECT_CLASS (parent_class)->finalize) (object);
        }
}

static void
app_main_window_destroy_cb (GtkWidget *window,
			    GossipApp *app)
{
	gtk_main_quit ();
}

static void
app_quit_cb (GtkWidget *window,
	     GossipApp *app)
{
	gtk_main_quit ();
}

static void
app_about_cb (GtkWidget *window,
	      GossipApp *app)
{
	GossipAppPriv *priv;
	const gchar   *authors[] = { 
		"Richard Hult <richard@imendio.com>",
		"Mikael Hallendal <micke@imendio.com>",
		NULL
	};
	/* Note to translators: put here your name (and address) so it
	 * will shop up in the "about" box */
	gchar         *translator_credits = _("translator_credits");

	priv = app->priv;

	if (priv->about) {
		gtk_window_present (GTK_WINDOW (priv->about));
		return;
	}
	
	priv->about = gnome_about_new ("Gossip", VERSION,
				       "",
				       _("A Jabber Client for GNOME"),
				       authors,
				       NULL,
				       strcmp (translator_credits, "translator_credits") != 0 ?
				       translator_credits : NULL,
				       NULL);
	
	g_object_add_weak_pointer (G_OBJECT (priv->about),
				   (gpointer) &priv->about);

	gtk_widget_show (priv->about);
}

static void
app_show_hide_activate_cb (GtkWidget *widget,
			   GossipApp *app)
{
	app_toggle_visibility ();
}

static void
app_connect_cb (GtkWidget *window,
		 GossipApp *app)
{
	g_return_if_fail (GOSSIP_IS_APP (app));

	gossip_connect_dialog_show (app);
}

static void
app_disconnect_cb (GtkWidget *window,
		   GossipApp *app)
{
	g_return_if_fail (GOSSIP_IS_APP (app));

	app_set_status (app, GOSSIP_STATUS_OFFLINE);
}

static void
app_join_dialog_destroy_cb (GtkWidget *widget,
			    GossipApp *app)
{
	app->priv->join_dialog = NULL;
}

static void
app_join_group_chat_cb (GtkWidget *window,
			GossipApp *app)
{
	GossipAppPriv *priv;
	GtkWidget     *dialog;

	g_return_if_fail (GOSSIP_IS_APP (app));

	priv = app->priv;

	if (priv->join_dialog) {
		dialog = gossip_join_dialog_get_dialog (priv->join_dialog);
		gtk_window_present (GTK_WINDOW (dialog));
		return;
	}
		
	priv->join_dialog = gossip_join_dialog_new (app);

	g_signal_connect (gossip_join_dialog_get_dialog (priv->join_dialog),
			  "destroy",
			  G_CALLBACK (app_join_dialog_destroy_cb),
			  app);
}

static void
app_authentication_cb (LmConnection *connection,
		       gboolean      result,
		       GossipApp    *app)
{
	GossipAppPriv *priv;
	
	g_return_if_fail (connection != NULL);
	g_return_if_fail (GOSSIP_IS_APP (app));

	priv = app->priv;

	if (result == TRUE) {
		g_signal_emit (app, signals[CONNECTED], 0);
		app_set_status (app, priv->status_to_set_on_connect);

		app_update_conn_dependent_menu_items (app);
	} else {
		app_set_status_indicator (app, GOSSIP_STATUS_OFFLINE);
	}
}

static void
app_status_item_activated_cb (GtkMenuItem *item, GossipApp *app)
{
	GossipAppPriv  *priv;
	GossipStatus    status;
	
	g_return_if_fail (GTK_IS_MENU_ITEM (item));
	g_return_if_fail (GOSSIP_IS_APP (app));
	
	priv = app->priv;
	
	status = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "data"));
	
	app_set_status (app, status);
}

static void
app_send_chat_message (gboolean use_roster_selection)
{
	GossipAppPriv   *priv;
	GossipJID       *jid;
	GtkWidget       *frame;
	CompleteJIDData *data;
	const gchar     *selected_jid = NULL;
	GList           *l;

	priv = app->priv;

	data = g_new0 (CompleteJIDData, 1);
	data->app = app;

	data->dialog = gtk_message_dialog_new (GTK_WINDOW (priv->window),
					       0,
					       GTK_MESSAGE_QUESTION,
					       GTK_BUTTONS_NONE,
					       _("Enter the user ID of the person you would "
						 "like to send a chat message to."));
	
	gtk_dialog_add_buttons (GTK_DIALOG (data->dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				_("_Chat"), GTK_RESPONSE_OK,
				NULL);
	
	gtk_dialog_set_has_separator (GTK_DIALOG (data->dialog), FALSE);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (data->dialog)->vbox),
			    frame, TRUE, TRUE, 8);
	gtk_widget_show (frame);
	
	data->combo = gtk_combo_new ();
	gtk_combo_set_use_arrows (GTK_COMBO (data->combo), FALSE);
	gtk_combo_disable_activate (GTK_COMBO (data->combo));
	gtk_container_add (GTK_CONTAINER (frame), data->combo);
	gtk_widget_show (data->combo);

	data->entry = GTK_COMBO (data->combo)->entry;

	g_signal_connect_after (data->entry,
				"insert_text",
				G_CALLBACK (app_complete_jid_insert_text_cb),
				data);
	
	g_signal_connect (data->entry,
			  "key_press_event",
			  G_CALLBACK (app_complete_jid_key_press_event_cb),
			  data);

	g_signal_connect (data->entry,
			  "activate",
			  G_CALLBACK (app_complete_jid_activate_cb),
			  data);

	g_signal_connect (data->dialog,
			  "response",
			  G_CALLBACK (app_complete_jid_response_cb),
			  data);
	
	data->completion = g_completion_new (app_complete_jid_to_string);

	if (use_roster_selection) {
		jid = gossip_roster_get_selected_jid (priv->roster);
		if (jid) {
			gossip_jid_ref (jid);
		}
	} else {
		jid = NULL;
	}

	data->jid_strings = NULL;
	data->jids = gossip_roster_get_jids (priv->roster);
	for (l = data->jids; l; l = l->next) {
		const gchar *str;
				
		str = gossip_jid_get_without_resource (l->data);
		data->jid_strings = g_list_append (data->jid_strings, g_strdup (str));

		if (jid && gossip_jid_equals_without_resource (jid, l->data)) {
			/* Got the selected one, select it in the combo. */
			selected_jid = str;
		}
	}
	
	if (data->jid_strings) {
		gtk_combo_set_popdown_strings (GTK_COMBO (data->combo),
					       data->jid_strings);
	}

	if (data->jids) {
		g_completion_add_items (data->completion, data->jids);
	}

	if (selected_jid) {
		gtk_entry_set_text (GTK_ENTRY (data->entry), selected_jid);
		gtk_editable_select_region (GTK_EDITABLE (data->entry), 0, -1);
	} else {
		gtk_entry_set_text (GTK_ENTRY (data->entry), "");
	}

	if (jid) {
		gossip_jid_unref (jid);
	}
	
	gtk_widget_show (data->dialog);
}

static void
app_send_chat_message_cb (GtkWidget *widget,
			  gpointer   user_data)
{
	app_send_chat_message (TRUE);
}

static void
app_popup_send_chat_message_cb (GtkWidget *widget,
				gpointer   user_data)
{
	app_send_chat_message (FALSE);
}

static void
app_add_contact_cb (GtkWidget *widget, GossipApp *app)
{
	gossip_add_contact_new (app->priv->connection, NULL);
}

static void
app_preferences_cb (GtkWidget *widget, GossipApp *app)
{
	GossipAppPriv *priv;

	priv = app->priv;

	if (priv->preferences_dialog) {
		gtk_window_present (GTK_WINDOW (priv->preferences_dialog));
		return;
	}

	priv->preferences_dialog = gossip_preferences_show (app);

	g_object_add_weak_pointer (G_OBJECT (priv->preferences_dialog),
				   (gpointer) &priv->preferences_dialog);
}

static void
app_subscription_request_dialog_response_cb (GtkWidget *dialog,
					     gint       response,
					     GossipApp *app)
{
	GossipAppPriv *priv;
	LmMessage   *m;
	LmMessage   *reply = NULL;
	gboolean     add_user;
	GtkWidget   *add_check_button;
	LmMessageSubType sub_type = LM_MESSAGE_SUB_TYPE_NOT_SET;
	const gchar *from;
	gboolean     subscribed = FALSE;
	GossipJID   *jid;

	g_return_if_fail (GTK_IS_DIALOG (dialog));
	g_return_if_fail (GOSSIP_IS_APP (app));
  
	priv = app->priv;
	
	m = (LmMessage *) (g_object_get_data (G_OBJECT (dialog), "message"));

	if (!m) {
		g_warning ("Message not set on subscription request dialog\n");
		return;
	}
	
	switch (response) {
	case 1:
	case GTK_RESPONSE_DELETE_EVENT:
		/* Do nothing */
		break;
	case GTK_RESPONSE_YES:
		sub_type = LM_MESSAGE_SUB_TYPE_SUBSCRIBED;
		break;
	case GTK_RESPONSE_NO:
		sub_type = LM_MESSAGE_SUB_TYPE_UNSUBSCRIBED;
		break;
	default:
		g_assert_not_reached ();
		break;
	};
	
	add_check_button = g_object_get_data (G_OBJECT (dialog), 
					      "add_check_button");
	add_user = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (add_check_button));

	gtk_widget_destroy (dialog);

	from = lm_message_node_get_attribute (m->node, "from");
	
	reply = lm_message_new_with_sub_type (from, 
					      LM_MESSAGE_TYPE_PRESENCE,
					      sub_type);

	lm_connection_send (priv->connection, reply, NULL);
	lm_message_unref (reply);
	
	jid = gossip_jid_new (from);
	subscribed = gossip_roster_have_jid (priv->roster, jid);

	if (add_user && !subscribed) {
		gossip_add_contact_new (app->priv->connection, jid);
	}
	
	gossip_jid_unref (jid);
	lm_message_unref (m);
}

static void
app_handle_subscription_request (GossipApp *app, LmMessage *m)
{
	GtkWidget   *dialog;
	GtkWidget   *jid_label;
	GtkWidget   *add_check_button;
	gchar       *str;
	const gchar *from;
	GossipJID   *jid;
	
	d(g_print ("Handle subscription request\n"));

	gossip_glade_get_file_simple (GLADEDIR "/main.glade",
				      "subscription_request_dialog",
				      NULL,
				      "subscription_request_dialog", &dialog,
				      "jid_label", &jid_label,
				      "add_check_button", &add_check_button,
				      NULL);
	
	from = lm_message_node_get_attribute (m->node, "from");
	
	str = g_strdup_printf ("<b>%s</b>", from);

	gtk_label_set_text (GTK_LABEL (jid_label), str);
	gtk_label_set_use_markup (GTK_LABEL (jid_label), TRUE);
	g_free (str);

	g_signal_connect (dialog,
			  "response",
			  G_CALLBACK (app_subscription_request_dialog_response_cb),
			  app);

	g_object_set_data (G_OBJECT (dialog), 
			   "message", lm_message_ref (m));

	g_object_set_data (G_OBJECT (dialog),
			   "add_check_button", add_check_button);

	jid = gossip_jid_new (from);
	if (gossip_roster_have_jid (app->priv->roster, jid)) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (add_check_button),
					      FALSE);
		gtk_widget_set_sensitive (add_check_button, FALSE);
	}
	gossip_jid_unref (jid);

	gtk_widget_show (dialog);
}

static LmHandlerResult
app_message_handler (LmMessageHandler *handler,
		     LmConnection     *connection,
		     LmMessage        *m,
		     GossipApp        *app)
{
	GossipAppPriv    *priv;
	const gchar      *from;
	LmMessageSubType  type;

	priv = app->priv;

	d(g_print ("App handle message\n"));

	type = lm_message_get_sub_type (m);
	from = lm_message_node_get_attribute (m->node, "from");
	
	switch (type) {
	case LM_MESSAGE_SUB_TYPE_AVAILABLE:
		/* FIXME: Hm, what is this for? Doesn't look right. */
		gossip_message_handle_message (app, m);
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;

		/* Is "not set" right? Gabber sends messages like that...  */
	case LM_MESSAGE_SUB_TYPE_NOT_SET:
	case LM_MESSAGE_SUB_TYPE_NORMAL:
	case LM_MESSAGE_SUB_TYPE_CHAT:
		app_tray_icon_push_message (m);
		return gossip_chat_handle_message (m);

	case LM_MESSAGE_SUB_TYPE_GROUPCHAT:
		g_warning ("Hmm .. looks like an unhandled group chat message "
			   "from %s, this needs to be taken care of.", from);
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;

	case LM_MESSAGE_SUB_TYPE_HEADLINE:
		g_warning ("Unhandled headline.");
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

	case LM_MESSAGE_SUB_TYPE_ERROR:
		g_warning ("Unhandled error from: %s.", from);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	
	default: 
		g_warning ("Unhandled subtype %d from: %s.", type, from);
		break;
	}

	d(g_print ("Unhandled message of type: %d.\n",
		   lm_message_get_type (m)));

	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static LmHandlerResult
app_presence_handler (LmMessageHandler *handler,
		      LmConnection     *connection,
		      LmMessage        *m,
		      GossipApp        *app)
{
	GossipAppPriv *priv;
	const gchar   *type;
	
	priv = app->priv;

	type = lm_message_node_get_attribute (m->node, "type");
	if (!type) {
		type = "available";
	}

	if (strcmp (type, "subscribe") == 0) {
		app_handle_subscription_request (app, m);
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}
	
	d(g_print ("Default presence handler: %s from %s\n",
		   type,
		   lm_message_node_get_attribute (m->node, "from")));

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult
app_iq_handler (LmMessageHandler *handler,
		LmConnection     *connection,
		LmMessage        *m,
		GossipApp        *app)
{
	LmMessageNode *node;

	g_return_val_if_fail (connection != NULL,
			      LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS);
	g_return_val_if_fail (GOSSIP_IS_APP (app),
			      LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS);
	
	node = lm_message_node_get_child (m->node, "query");

	d(g_print ("Default IQ handler... "));
	
	if (node) {
		const gchar *namespace;
		
		namespace = lm_message_node_get_attribute (node, "xmlns");
		
		d(g_print ("with namespace %s", namespace));
		
		if (strcmp (namespace, "jabber:iq:version") == 0) {
			LmMessage      *v;
			const gchar    *from, *id;
			struct utsname  osinfo;

			from = lm_message_node_get_attribute (m->node, "from");
			id = lm_message_node_get_attribute (m->node, "id");

			v = lm_message_new_with_sub_type (from, 
							  LM_MESSAGE_TYPE_IQ,
							  LM_MESSAGE_SUB_TYPE_RESULT);
			lm_message_node_set_attributes (v->node, 
							"id", id, NULL);
			
			node = lm_message_node_add_child (v->node,
							  "query", NULL);
			lm_message_node_set_attributes (node,
							"xmlns", namespace,
							NULL);
			lm_message_node_add_child (node, "name", PACKAGE);
			lm_message_node_add_child (node, "version", VERSION);
			
			uname (&osinfo);
			lm_message_node_add_child (node, "os", osinfo.sysname);

			lm_connection_send (connection, v, NULL);
			lm_message_unref (v);
		}
	}

	d(g_print ("\n"));

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
app_connection_open_cb (LmConnection *connection,
			gboolean      result,
			GossipApp    *app)
{
	GossipAppPriv *priv;
	gchar         *password;

	g_return_if_fail (connection != NULL);
	g_return_if_fail (GOSSIP_IS_APP (app));

	priv = app->priv;

	if (result != TRUE) {
		/* FIXME: Handle this better... */

		app_disconnect (app);
		return;
	}

	if (!priv->account->password || !priv->account->password[0]) {
		password = gossip_password_dialog_run (priv->account, GTK_WINDOW (priv->window));
		if (!password) {
			app_disconnect (app);
			return;
 		}
	} else {
		password = g_strdup (priv->account->password);
	}

	if (!priv->account->resource || !priv->account->resource[0]) {
		g_free (priv->account->resource);
		priv->account->resource = g_strdup (DEFAULT_RESOURCE);
	}

	if (priv->jid) {
		gossip_jid_unref (priv->jid);
	}
	priv->jid = gossip_account_get_jid (priv->account);

	lm_connection_authenticate (connection, 
				    priv->account->username, 
				    password,
				    priv->account->resource,
				    (LmResultFunction) app_authentication_cb,
				    app, NULL, NULL);

	g_free (password);
}

static void
app_client_disconnected_cb (LmConnection       *connection,
			    LmDisconnectReason  reason, 
			    GossipApp          *app)
{
	GossipAppPriv *priv;
	GtkWidget     *dialog;
	GossipStatus   status;

	priv = app->priv;

	/* Keep the old status if we are going to reconnect. */
	status = gossip_status_menu_get_status (priv->option_menu);
	
	app_set_status_indicator (app, GOSSIP_STATUS_OFFLINE);

	g_signal_emit (app, signals[DISCONNECTED], 0);

	if (reason != LM_DISCONNECT_REASON_OK) {
		dialog = gtk_message_dialog_new (GTK_WINDOW (priv->window),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_INFO,
						 GTK_BUTTONS_YES_NO,
						 _("You were disconnected from the server. "
						   "Do you want to reconnect?"));
		
		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES) {
			app_set_status (app, status);
		} else {
			app_set_status (app, GOSSIP_STATUS_OFFLINE);
		}
	
		gtk_widget_destroy (dialog);
	}
}

static void
app_user_activated_cb (GossipRoster *roster,
		       GossipJID    *jid,
		       GossipApp    *app)
{
	GossipChat *chat;
	GtkWidget  *widget;

	chat = gossip_chat_get_for_jid (jid);
	widget = gossip_chat_get_dialog (chat);
	gtk_widget_show (widget);
}

void
gossip_app_join_group_chat (GossipApp   *app,
			    const gchar *room,
			    const gchar *server,
			    const gchar *nick)
{
	gchar     *tmp;
	GossipJID *jid;

	tmp = g_strdup_printf ("%s@%s", room, server);
	jid = gossip_jid_new (tmp);
	g_free (tmp);
	gossip_group_chat_new (jid, nick);
	gossip_jid_unref (jid);
}

static gboolean
app_idle_check_cb (GossipApp *app)
{
	GossipStatus   status;
	GossipAppPriv *priv;
	gint           idle;
	gint           auto_away;
	gint           auto_ext_away;
	gboolean       is_away;

	priv = app->priv;

	if (!gconf_client_get_bool (gconf_client,
				    "/apps/gossip/auto_away/enabled",
				    NULL)) {
		return TRUE;
	}

	status = gossip_status_menu_get_status (priv->option_menu);

	switch (status) {
	case GOSSIP_STATUS_OFFLINE:
		return TRUE;

	case GOSSIP_STATUS_AWAY:
	case GOSSIP_STATUS_EXT_AWAY:
		is_away = TRUE;
		break;

	default:
		is_away = FALSE;
		break;
	}

	auto_away = 60 * gconf_client_get_int (gconf_client,
					       "/apps/gossip/auto_away/idle_time",
					       NULL);
	auto_ext_away = 60 * gconf_client_get_int (gconf_client,
						   "/apps/gossip/auto_away/extended_idle_time",
						   NULL);
	
	idle = gossip_idle_get_seconds ();

	if (!is_away && idle > (auto_away)) {
		d(g_print ("Going auto-away...\n"));

		app_set_status (app, GOSSIP_STATUS_AWAY); /*, "Autoaway");*/
		/*gossip_app_statusbar_push_notification (app, "Going auto-away");*/
		priv->is_auto_away = TRUE;
	}
	else if (priv->is_auto_away &&
		 status != GOSSIP_STATUS_EXT_AWAY && idle > auto_ext_away) {
		d(g_print ("Going extended auto-away...\n"));
		app_set_status (app, GOSSIP_STATUS_EXT_AWAY); /*, "Auto-Away (Ext)");*/
		/*gossip_app_statusbar_push_notification (app, "Going extended auto-away");*/
	}
	else if (priv->is_auto_away && idle < auto_away) {
		d(g_print ("Returning from auto-away...\n"));
		app_set_status (app, GOSSIP_STATUS_AVAILABLE); /*, "Back from idle");*/
		/*gossip_app_statusbar_push_notification (app, "Returning from auto-away");*/
		priv->is_auto_away = FALSE;
	}

	return TRUE;
}

void
gossip_app_connect (GossipAccount *account)
{
	GossipAppPriv *priv;

	g_return_if_fail (account != NULL);
	
	priv = app->priv;

	app_disconnect (app);
	
	gossip_account_ref (account);

	d(g_print ("Connecting to: %s\n", account->server));
	
	if (priv->account) {
		gossip_account_unref (priv->account);
	}
	priv->account = account;

	if (!priv->connection) {
		app_create_connection (app);
	}

	lm_connection_set_server (priv->connection, account->server);
	lm_connection_set_port (priv->connection, account->port);
	lm_connection_set_use_ssl (priv->connection, account->use_ssl);
	
	lm_connection_open (priv->connection,
			    (LmResultFunction) app_connection_open_cb,
			    app, NULL, NULL);
}

void
gossip_app_connect_default (void)
{
	GossipAppPriv *priv;

	priv = app->priv;

	gossip_app_connect (priv->account);
}

void
gossip_app_create (void)
{
	app = g_object_new (GOSSIP_TYPE_APP, NULL);
}

GossipApp *
gossip_app_get (void)
{
	g_assert (app != NULL);
	
	return app;
}

static void
app_set_status (GossipApp *app, GossipStatus status)
{
	GossipAppPriv        *priv;
	LmMessage            *m;
	
	priv = app->priv;

	d(g_print ("app_set_status\n"));

	/* Disconnect. */
	if (status == GOSSIP_STATUS_OFFLINE) {
		if (lm_connection_is_open (priv->connection)) {
			app_disconnect (app);
			app_set_status_indicator (app, GOSSIP_STATUS_OFFLINE);
		}

		app_update_conn_dependent_menu_items (app);
		return;
	}

	/* Connect. */
	if (!lm_connection_is_open (priv->connection)) {
		priv->status_to_set_on_connect = status;

		gossip_app_connect (priv->account);
		return;
	}

	/* Change status. */
	m = lm_message_new_with_sub_type (NULL, 
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_AVAILABLE);
	
	lm_message_node_add_child (m->node, "show", gossip_status_to_string (status));
	/*lm_message_node_add_child (m->node, "status", "Online");*/
	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);

	gossip_group_chat_set_status (status);
	
	app_set_status_indicator (app, status);
}

const gchar *
gossip_app_get_username (void)
{
	return app->priv->account->username;
}

GossipJID *
gossip_app_get_jid (void)
{
	return app->priv->jid;
}

GossipRoster *
gossip_app_get_roster (void)
{
	return app->priv->roster;
}

static void
app_set_status_indicator (GossipApp *app,
			  GossipStatus status)
{
	GossipAppPriv *priv;
	const gchar   *filename;
	GdkPixbuf     *pixbuf;

	priv = app->priv;

	g_signal_handlers_block_by_func (priv->option_menu,
					 app_status_item_activated_cb,
					 app);
	
	gossip_option_menu_set_history (GTK_OPTION_MENU (priv->option_menu),
					GINT_TO_POINTER (status));

	g_signal_handlers_unblock_by_func (priv->option_menu,
					   app_status_item_activated_cb,
					   app);

	filename = gossip_status_to_icon_filename (status);
	pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
	gtk_window_set_icon (GTK_WINDOW (priv->window), pixbuf);
	g_object_unref (pixbuf);

	app_tray_icon_set_status (app, status);
}

static void
app_create_connection (GossipApp *app)
{
	GossipAppPriv    *priv;
	LmMessageHandler *handler;

	priv = app->priv;

	priv->connection = lm_connection_new (priv->account->server);
	lm_connection_set_port (priv->connection, priv->account->port);
	lm_connection_set_use_ssl (priv->connection, priv->account->use_ssl);

	handler = lm_message_handler_new ((LmHandleMessageFunction) app_message_handler, 
					  app, NULL);
	lm_connection_register_message_handler (priv->connection,
						handler,
						LM_MESSAGE_TYPE_MESSAGE,
						LM_HANDLER_PRIORITY_LAST);
	lm_message_handler_unref (handler);

	handler = lm_message_handler_new ((LmHandleMessageFunction) app_presence_handler, app, NULL);
	lm_connection_register_message_handler (priv->connection,
						handler,
						LM_MESSAGE_TYPE_PRESENCE,
						LM_HANDLER_PRIORITY_LAST);
	lm_message_handler_unref (handler);

	handler = lm_message_handler_new ((LmHandleMessageFunction) app_iq_handler, app, NULL);
	lm_connection_register_message_handler (priv->connection,
						handler,
						LM_MESSAGE_TYPE_IQ,
						LM_HANDLER_PRIORITY_LAST);
	lm_message_handler_unref (handler);

	lm_connection_set_disconnect_function (priv->connection, 
			(LmDisconnectFunction) app_client_disconnected_cb, 
			app, NULL);
			

}

static void
app_disconnect (GossipApp *app)
{
	GossipAppPriv *priv = app->priv;

	if (priv->connection && lm_connection_is_open (priv->connection)) {
		lm_connection_close (priv->connection, NULL);
	}
}

LmConnection *
gossip_app_get_connection (void)
{
	return app->priv->connection;
}

static void
app_setup_conn_dependent_menu_items (GossipApp *app,
				     GladeXML  *glade)
{
	const gchar *connect_widgets[] = { "actions_disconnect",
					   "actions_join_group_chat",
					   "actions_send_chat_message",
					   "actions_add_contact"
	};
	
	const gchar *disconnect_widgets[] = { "actions_connect"
	};
	
	GList     *list;
	GtkWidget *w;
	gint       i;

	list = NULL;
	for (i = 0; i < G_N_ELEMENTS (connect_widgets); i++) {
		w = glade_xml_get_widget (glade, connect_widgets[i]);
		list = g_list_prepend (list, w);
	}
	app->priv->enabled_connected_widgets = list;

	list = NULL;
	for (i = 0; i < G_N_ELEMENTS (disconnect_widgets); i++) {
		w = glade_xml_get_widget (glade, disconnect_widgets[i]);
		list = g_list_prepend (list, w);
	}
	app->priv->enabled_disconnected_widgets = list;
}

static void
app_update_conn_dependent_menu_items (GossipApp *app)
{
	GossipAppPriv *priv;
	GList         *l;
	GossipStatus   status;
	gboolean       connected;

	priv = app->priv;
	
	status = gossip_status_menu_get_status (priv->option_menu);
	connected = (status != GOSSIP_STATUS_OFFLINE);

	for (l = priv->enabled_connected_widgets; l; l = l->next) {
		gtk_widget_set_sensitive (l->data, connected);
	}
	for (l = priv->enabled_disconnected_widgets; l; l = l->next) {
		gtk_widget_set_sensitive (l->data, !connected);
	}
}

static void
app_complete_jid_response_cb (GtkWidget       *dialog,
			      gint             response,
			      CompleteJIDData *data)
{
	const gchar *str;
	GossipJID   *jid;
	GossipChat  *chat;
	GtkWidget   *widget;
	GList       *l;

	if (response == GTK_RESPONSE_OK) {
		str = gtk_entry_get_text (GTK_ENTRY (data->entry));
		if (gossip_jid_string_is_valid_jid (str)) {
			jid = gossip_jid_new (str);
			chat = gossip_chat_get_for_jid (jid);
			widget = gossip_chat_get_dialog (chat);
			gtk_widget_show (widget);
			gossip_jid_unref (jid);
		} else {
			/* FIXME: Display error dialog... */
			g_warning ("'%s' is not a valid JID.", str);
		}
	}

	for (l = data->jid_strings; l; l = l->next) {
		g_free (l->data);
	}
	
	g_list_free (data->jid_strings);
	gossip_roster_free_jid_list (data->jids);
	g_free (data);
	
	gtk_widget_destroy (dialog);	
}

static gboolean 
app_complete_jid_idle (CompleteJIDData *data)
{
	const gchar *prefix;
	gsize        len;
	gchar       *new_prefix;

	prefix = gtk_entry_get_text (GTK_ENTRY (data->entry));
	len = strlen (prefix);
	
	g_completion_complete (data->completion, 
			       (gchar *) prefix, 
			       &new_prefix);

	if (new_prefix) {
		g_signal_handlers_block_by_func (data->entry,
						 app_complete_jid_insert_text_cb,
						 data);
		
  		gtk_entry_set_text (GTK_ENTRY (data->entry), new_prefix); 
					  
		g_signal_handlers_unblock_by_func (data->entry, 
						   app_complete_jid_insert_text_cb,
						   data);

		if (data->touched) {
			gtk_editable_set_position (GTK_EDITABLE (data->entry), len);
			gtk_editable_select_region (GTK_EDITABLE (data->entry), len, -1);
		} else {
			/* We want to leave the whole thing selected at first. */
			gtk_editable_set_position (GTK_EDITABLE (data->entry), -1);
			gtk_editable_select_region (GTK_EDITABLE (data->entry), 0, -1);
		}
		
		g_free (new_prefix);
	}

	data->complete_idle_id = 0;

	return FALSE;
}

static void
app_complete_jid_insert_text_cb (GtkEntry        *entry, 
				 const gchar     *text,
				 gint             length,
				 gint            *position,
				 CompleteJIDData *data)
{
	if (!data->complete_idle_id) {
		data->complete_idle_id = g_idle_add ((GSourceFunc) app_complete_jid_idle, data);
	}
}

static gboolean
app_complete_jid_key_press_event_cb (GtkEntry        *entry,
				     GdkEventKey     *event,
				     CompleteJIDData *data)
{
	data->touched = TRUE;
	
	if ((event->state & GDK_CONTROL_MASK) == 0 &&
	    (event->state & GDK_SHIFT_MASK) == 0 && event->keyval == GDK_Tab) {
		gtk_editable_set_position (GTK_EDITABLE (entry), -1);
		gtk_editable_select_region (GTK_EDITABLE (entry), -1, -1);
	
		return TRUE;
	}
	
	return FALSE;
}

static void
app_complete_jid_activate_cb (GtkEntry        *entry,
			      CompleteJIDData *data)
{
	gtk_dialog_response (GTK_DIALOG (data->dialog), GTK_RESPONSE_OK);
}

static gchar *
app_complete_jid_to_string (gpointer data)
{
	GossipJID *jid = data;

	return (gchar *) gossip_jid_get_without_resource (jid);
}

GossipStatus 
gossip_app_get_status (void)
{
	return gossip_status_menu_get_status (app->priv->option_menu);
}

static void
app_toggle_visibility (void)
{
	GossipAppPriv *priv;
	static gint    x = 0, y = 0;
	gboolean       visible;

	priv = app->priv;
	
	g_object_get (priv->window,
		      "visible", &visible,
		      NULL);
	
	if (visible) {
		gtk_window_get_position (GTK_WINDOW(priv->window), &x, &y);
		gtk_widget_hide (priv->window);
		
		gtk_widget_hide (priv->hide_popup_item);
		gtk_widget_show (priv->show_popup_item);
	} else {
		gtk_widget_show (priv->window);
		gtk_window_move (GTK_WINDOW (priv->window), x, y);
		
		gtk_widget_show (priv->hide_popup_item);
		gtk_widget_hide (priv->show_popup_item);
	}
}

static void
app_tray_icon_destroy_cb (GtkWidget *widget,
			  GossipApp *app)
{
	GossipAppPriv *priv;
	GossipStatus   status;
	
	priv = app->priv;

	gtk_widget_destroy (GTK_WIDGET (app->priv->tray_icon));
	app_create_tray_icon (app);

	status = gossip_status_menu_get_status (priv->option_menu);
	app_tray_icon_set_status (app, status);

	/* Show the window in case the notification area was removed. */
	gtk_widget_show (priv->window);	
}

static gboolean
app_tray_icon_button_press_cb (GtkWidget      *widget, 
			       GdkEventButton *event, 
			       GossipApp      *app)
{
	GossipAppPriv *priv;

	priv = app->priv;

	switch (event->button) {
	case 1:
		if (app_tray_icon_pop_message ()) {
			break;
		}

		app_toggle_visibility ();
		break;

	case 3:
                gtk_menu_popup (GTK_MENU (priv->popup_menu), NULL, NULL, NULL,
				NULL, event->button, event->time);
                return TRUE;
		
		break;

	default:
		return FALSE;
	}

	return TRUE;
}
	
static void
app_create_tray_icon (GossipApp *app)
{
	GossipAppPriv *priv;

	priv = app->priv;

	priv->tray_icon = egg_tray_icon_new (_("Gossip, Jabber Client"));
		
	priv->tray_event_box = gtk_event_box_new ();
	priv->tray_image = gtk_image_new_from_file (
		gossip_status_to_icon_filename (GOSSIP_STATUS_OFFLINE));
		
	gtk_container_add (GTK_CONTAINER (priv->tray_event_box),
			   priv->tray_image);

	priv->tray_tooltips = gtk_tooltips_new ();
	
	gtk_widget_show (priv->tray_event_box);
	gtk_widget_show (priv->tray_image);

	gtk_container_add (GTK_CONTAINER (priv->tray_icon),
			   priv->tray_event_box);
	gtk_widget_show (GTK_WIDGET (priv->tray_icon));

	gtk_widget_add_events (GTK_WIDGET (priv->tray_icon),
			       GDK_BUTTON_PRESS_MASK);
	
	g_signal_connect (priv->tray_icon,
			  "button_press_event",
			  G_CALLBACK (app_tray_icon_button_press_cb),
			  app);
	
	g_signal_connect (priv->tray_event_box,
			  "destroy",
			  G_CALLBACK (app_tray_icon_destroy_cb),
			  app);
}

static void
app_tray_icon_set_status (GossipApp *app, GossipStatus status)
{
	GossipAppPriv *priv;
	const gchar   *filename;
	GdkPixbuf     *pixbuf;

	priv = app->priv;

	filename = gossip_status_to_icon_filename (status);
	pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
	
	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->tray_image), pixbuf);
		
	g_object_unref (pixbuf);
}

static gboolean
flash_timeout_func (gpointer data)
{
	GossipAppPriv   *priv;
	static gboolean  on = FALSE;
	GossipStatus     status;
	const gchar     *filename;

	priv = app->priv;
	
	if (on) {
		filename = IMAGEDIR "/message.png";
	} else {
		status = gossip_status_menu_get_status (priv->option_menu);
		filename = gossip_status_to_icon_filename (status);
	}
		
	gtk_image_set_from_file (GTK_IMAGE (priv->tray_image), filename);
	
	on = !on;

	return TRUE;
}

static void
app_tray_icon_push_message (LmMessage *m)
{
	GossipAppPriv *priv;
	const gchar   *from;

	priv = app->priv;

	from = lm_message_node_get_attribute (m->node, "from");
	
	priv->tray_flash_icons = g_list_append (priv->tray_flash_icons,
						g_strdup (from));

	if (!priv->tray_flash_timeout_id) {
		priv->tray_flash_timeout_id = g_timeout_add (350,
							     flash_timeout_func,
							     NULL);

		app_tray_icon_update_tooltip ();
	}
}

static gboolean
app_tray_icon_pop_message (void)
{
	GossipAppPriv *priv;
	gchar         *from;
	GossipJID     *jid;
	GossipChat    *chat;
	GtkWidget     *widget;
	GossipStatus   status;
	const gchar   *filename;

	priv = app->priv;

	if (!priv->tray_flash_icons)
		return FALSE;

	from = priv->tray_flash_icons->data;
	jid = gossip_jid_new (from);
	chat = gossip_chat_get_for_jid (jid);
	gossip_jid_unref (jid);

	widget = gossip_chat_get_dialog (chat);

	gtk_window_present (GTK_WINDOW (widget));
	
	priv->tray_flash_icons = g_list_delete_link (priv->tray_flash_icons,
						     priv->tray_flash_icons);

	if (!priv->tray_flash_icons && priv->tray_flash_timeout_id) {
		g_source_remove (priv->tray_flash_timeout_id);
		priv->tray_flash_timeout_id = 0;

		status = gossip_status_menu_get_status (priv->option_menu);
		filename = gossip_status_to_icon_filename (status);
		gtk_image_set_from_file (GTK_IMAGE (priv->tray_image), filename);
	}

	app_tray_icon_update_tooltip ();

	return TRUE;
}

static void
app_tray_icon_update_tooltip (void)
{
	GossipAppPriv *priv;
	const gchar   *from;
	GossipJID     *jid;
	const gchar   *name;
	gchar         *str;

	priv = app->priv;

	if (!priv->tray_flash_icons) {
		gtk_tooltips_set_tip (GTK_TOOLTIPS (priv->tray_tooltips),
				      priv->tray_event_box,
				      NULL, NULL);
		return;
	}

	from = priv->tray_flash_icons->data;
	jid = gossip_jid_new (from);
	
	name = gossip_roster_get_nick_from_jid (priv->roster, jid);
	if (!name) {
		name = from;
	}
	
	str = g_strdup_printf (_("New message from %s"), name);
	
	gtk_tooltips_set_tip (GTK_TOOLTIPS (priv->tray_tooltips),
			      priv->tray_event_box,
			      str, str);
	
	g_free (str);
	gossip_jid_unref (jid);
}	

