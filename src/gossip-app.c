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
#include <libgnomeui/libgnomeui.h>
#include "eggtrayicon.h"
#include "eel-ellipsizing-label.h"
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
#include "gossip-account-dialog.h"
#include "gossip-app.h"
#include "gossip-stock.h"


#define DEFAULT_RESOURCE N_("Home")
#define ADD_CONTACT_RESPONSE_ADD 1
#define REQUEST_RESPONSE_DECIDE_LATER 1

#define d(x)

extern GConfClient *gconf_client;

struct _GossipAppPriv {
	LmConnection        *connection;
	GtkWidget           *window;

	EggTrayIcon         *tray_icon;
	GtkWidget           *tray_event_box;
	GtkWidget           *tray_image;
	GtkTooltips         *tray_tooltips;
	GList               *tray_flash_icons;
	guint                tray_flash_timeout_id;
	GtkWidget           *popup_menu;
	GtkWidget           *show_popup_item;
	GtkWidget           *hide_popup_item;
	
	GossipRosterOld     *roster;

	GossipAccount       *account;
	gchar               *overridden_resource;

	GossipJID           *jid;

	/* Dialogs that we only have one of at a time. */
	GossipJoinDialog    *join_dialog;
	GtkWidget           *about;
	GtkWidget           *preferences_dialog;

	/* Widgets that are enabled when we're connected. */
	GList               *enabled_connected_widgets;

	/* Widgets that are enabled when we're disconnected. */
	GList               *enabled_disconnected_widgets;

	/* Widgets for the status popup. */
	GtkWidget           *status_button;
	GtkWidget           *status_popup;
	GtkWidget           *status_entry;
	GtkWidget           *status_busy_checkbutton;
	GtkWidget           *status_label;
	GtkWidget           *status_image;

	/* Current status. */
	gchar               *status_text;
	GossipShow           explicit_show;
	GossipShow           auto_show;
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
static void     app_user_activated_cb                (GossipRosterOld       *roster,
						      GossipJID          *jid,
						      GossipApp          *app);
static gboolean app_idle_check_cb                    (GossipApp          *app);
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
static void     app_account_information_cb           (GtkWidget          *widget,
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


static LmHandlerResult
app_message_handler                                  (LmMessageHandler   *handler,
						      LmConnection       *connection,
						      LmMessage          *m,
						      GossipApp          *app);
static LmHandlerResult
app_show_handler                                 (LmMessageHandler   *handler,
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
static void     app_tray_icon_destroy_cb             (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_create_tray_icon                 (void);
static void     app_create_connection                (void);
static void     app_disconnect                       (void);
static void     app_setup_conn_dependent_menu_items  (GladeXML           *glade);
static void     app_update_conn_dependent_menu_items (void);
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
static void     app_push_message                     (LmMessage          *m);
static gboolean app_pop_message                      (GossipJID          *jid);
static void     app_tray_icon_update_tooltip         (void);


static gboolean status_popup_key_press_event_cb   (GtkWidget      *widget,
						   GdkEventKey    *event,
						   gpointer        data);

static gboolean status_popup_button_release_event_cb (GtkWidget      *widget,
						    GdkEventButton *event,
						    gpointer        user_data);
static void     show_popup                         (void);
static void     hide_popup                         (gboolean        apply);
static void     status_button_toggled_cb           (GtkWidget      *widget,
						    gpointer        user_data);

static GossipShow app_get_explicit_show  (void);
static GossipShow app_get_effective_show (void);
static void       app_update_show        (void);


static const gchar * app_get_current_status_icon (void);


static GObjectClass *parent_class;
static GossipApp *app;
static GdkPixbuf *message_pixbuf;

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

	message_pixbuf = gdk_pixbuf_new_from_file (IMAGEDIR "/message.png", NULL);
	
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
app_init (GossipApp *singleton_app)
{
        GossipAppPriv *priv;
	GladeXML      *glade;
	GtkWidget     *sw;
	GtkWidget     *item;
	GtkWidget     *status_label_hbox;

	app = singleton_app;
	
        priv = g_new0 (GossipAppPriv, 1);
        app->priv = priv;

	priv->explicit_show = GOSSIP_SHOW_AVAILABLE;
	priv->auto_show = GOSSIP_SHOW_AVAILABLE;
	priv->status_text = g_strdup (_("Available"));
	
	glade = gossip_glade_get_file (GLADEDIR "/main.glade",
				       "main_window",
				       NULL,
				       "main_window", &priv->window,
				       "roster_scrolledwindow", &sw,
				       "status_button", &priv->status_button,
				       "status_label_hbox", &status_label_hbox,
				       "status_image", &priv->status_image,
				       NULL);

	priv->status_label = eel_ellipsizing_label_new (_("Available"));
	gtk_misc_set_alignment (GTK_MISC (priv->status_label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (status_label_hbox), priv->status_label, TRUE, TRUE, 0);
	gtk_widget_show (priv->status_label);
	
	app_setup_conn_dependent_menu_items (glade);
	
	gossip_glade_connect (glade,
			      app,
			      "file_quit", "activate", app_quit_cb,
			      "actions_connect", "activate", app_connect_cb,
			      "actions_disconnect", "activate", app_disconnect_cb,
			      "actions_join_group_chat", "activate", app_join_group_chat_cb,
			      "actions_send_chat_message", "activate", app_send_chat_message_cb,
			      "actions_add_contact", "activate", app_add_contact_cb,
			      "edit_preferences", "activate", app_preferences_cb,
			      "edit_account_information", "activate", app_account_information_cb,
			      "help_about", "activate", app_about_cb,
			      "main_window", "destroy", app_main_window_destroy_cb,
			      NULL);

	g_object_unref (glade);

	if (gossip_startup_druid_is_needed ()) {
		gossip_startup_druid_run ();
	}

	priv->account = gossip_account_get_default ();

	/* If we still don't have an account, create an empty one, we don't save
	 * it until it's edited though, so the druid will come up again.
	 */
	if (!priv->account) {
		priv->account = gossip_account_new ("Default",
						    NULL, NULL,
						    _(DEFAULT_RESOURCE),
						    NULL,
						    LM_CONNECTION_DEFAULT_PORT,
						    FALSE);
	}

	app_create_connection ();
	
	priv->roster = gossip_roster_old_new (app);

	gtk_widget_show (GTK_WIDGET (priv->roster));
	gtk_container_add (GTK_CONTAINER (sw),
			   GTK_WIDGET (priv->roster));

	g_signal_connect (priv->roster,
			  "user_activated",
			  G_CALLBACK (app_user_activated_cb),
			  app);

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

	app_create_tray_icon ();
	app_update_show ();
	app_update_conn_dependent_menu_items ();

	g_signal_connect (priv->status_button,
			  "toggled",
			  G_CALLBACK (status_button_toggled_cb),
			  app);

	/* Start the idle time checker. */
	g_timeout_add (5000, (GSourceFunc) app_idle_check_cb, app);
	
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
	LmConnection *connection;

	connection = app->priv->connection;
	
	if (lm_connection_is_open (connection)) {
		lm_connection_close (connection, NULL);
	}
	
	gtk_main_quit ();
}

static void
app_quit_cb (GtkWidget *widget,
	     GossipApp *app)
{
	gtk_widget_destroy (app->priv->window);
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
	/* Note to translators: put here your name and email so it
	 * will shop up in the "about" box
	 */
	gchar         *translator_credits = _("translator_credits");
	GtkWidget     *href;

	priv = app->priv;

	if (priv->about) {
		gtk_window_present (GTK_WINDOW (priv->about));
		return;
	}
	
	priv->about = gnome_about_new ("Gossip", VERSION,
				       "Copyright \xc2\xa9 2003 Imendio HB",
				       _("A Jabber Client for GNOME"),
				       authors,
				       NULL,
				       strcmp (translator_credits, "translator_credits") != 0 ?
				       translator_credits : NULL,
				       NULL);

	href = gnome_href_new ("http://www.imendio.com/projects/gossip/", _("Gossip Website"));
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (priv->about)->vbox), href, FALSE, FALSE, 5);
	gtk_widget_show (href);
	
	g_object_add_weak_pointer (G_OBJECT (priv->about), (gpointer) &priv->about);

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

	app_disconnect ();
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
		       gboolean      success,
		       GossipApp    *app)
{
	GossipAppPriv *priv;
	
	priv = app->priv;

	if (success) {
		g_signal_emit (app, signals[CONNECTED], 0);
	}

	app_update_conn_dependent_menu_items ();
	app_update_show ();
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
		jid = gossip_roster_old_get_selected_jid (priv->roster);
		if (jid) {
			gossip_jid_ref (jid);
		}
	} else {
		jid = NULL;
	}

	data->jid_strings = NULL;
	data->jids = gossip_roster_old_get_jids (priv->roster);
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
app_account_information_cb (GtkWidget *widget, GossipApp *app)
{
	gossip_account_dialog_show ();
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
	case REQUEST_RESPONSE_DECIDE_LATER: /* Decide later. */
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
	subscribed = gossip_roster_old_have_jid (priv->roster, jid);

	if (add_user && !subscribed && sub_type == LM_MESSAGE_SUB_TYPE_SUBSCRIBED) {
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
	gchar       *str, *tmp;
	const gchar *from;
	GossipJID   *jid;
	
	gossip_glade_get_file_simple (GLADEDIR "/main.glade",
				      "subscription_request_dialog",
				      NULL,
				      "subscription_request_dialog", &dialog,
				      "jid_label", &jid_label,
				      "add_check_button", &add_check_button,
				      NULL);
	
	from = lm_message_node_get_attribute (m->node, "from");


	tmp = g_strdup_printf (_("%s wants to be notified of your presence."), from);
	str = g_strdup_printf ("<span weight='bold' size='larger'>%s</span>", tmp);
	g_free (tmp);

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
	if (gossip_roster_old_have_jid (app->priv->roster, jid)) {
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

	type = lm_message_get_sub_type (m);
	from = lm_message_node_get_attribute (m->node, "from");
	
	switch (type) {
	
	case LM_MESSAGE_SUB_TYPE_NOT_SET:
	case LM_MESSAGE_SUB_TYPE_NORMAL:
	case LM_MESSAGE_SUB_TYPE_CHAT:
	case LM_MESSAGE_SUB_TYPE_HEADLINE: /* For now, fixes #120009 */
		app_push_message (m);
		return gossip_chat_handle_message (m);

	case LM_MESSAGE_SUB_TYPE_GROUPCHAT:
		g_warning ("Hmm .. looks like an unhandled group chat message "
			   "from %s, this needs to be taken care of.", from);
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;

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
app_show_handler (LmMessageHandler *handler,
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
	
	d(g_print ("Default show handler: %s from %s\n",
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

	if (node) {
		const gchar *namespace;
		
		namespace = lm_message_node_get_attribute (node, "xmlns");
		
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
			lm_message_node_add_child (node, "name", "Gossip");
			lm_message_node_add_child (node, "version", VERSION);
			
			uname (&osinfo);
			lm_message_node_add_child (node, "os", osinfo.sysname);

			lm_connection_send (connection, v, NULL);
			lm_message_unref (v);
		}
	}

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
app_connection_open_cb (LmConnection *connection,
			gboolean      success,
			GossipApp    *app)
{
	GossipAppPriv *priv;
	gchar         *password;
	GossipAccount *account;
	const gchar   *resource = NULL;

	priv = app->priv;

	if (!success) {
		/* FIXME: Handle this better... */
		app_disconnect ();
		return;
	}

	account = priv->account;
	
	if (!account->password || !account->password[0]) {
		password = gossip_password_dialog_run (account,
						       GTK_WINDOW (priv->window));
		if (!password) {
			app_disconnect ();
			return;
 		}
	} else {
		password = g_strdup (account->password);
	}

	/* Try overridden resource, account resource, default resource in that
	 * order.
	 */
	if (priv->overridden_resource) {
		resource = priv->overridden_resource;
	}
	if (!resource || !resource[0]) {
		resource = account->resource;
	}
	if (!resource || !resource[0]) {
		resource = _(DEFAULT_RESOURCE);
	}

	if (priv->jid) {
		gossip_jid_unref (priv->jid);
	}
	priv->jid = gossip_account_get_jid (account);

	lm_connection_authenticate (connection, 
				    account->username, 
				    password,
				    resource,
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
	gint           response;

	priv = app->priv;

	g_signal_emit (app, signals[DISCONNECTED], 0);

	if (reason != LM_DISCONNECT_REASON_OK) {
		/* FIXME: Remove this dialog, it's stupid to ask. Just keep
		 * trying maybe once a minute until it works.
		 */
		dialog = gtk_message_dialog_new (GTK_WINDOW (priv->window),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_INFO,
						 GTK_BUTTONS_YES_NO,
						 _("You were disconnected from the server. "
						   "Do you want to reconnect?"));
		
		response = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		if (response == GTK_RESPONSE_YES) {
			gossip_app_connect ();
		}
	}

	app_update_conn_dependent_menu_items ();
	app_update_show ();
}

static void
app_user_activated_cb (GossipRosterOld *roster,
		       GossipJID    *jid,
		       GossipApp    *app)
{
	GossipChat *chat;
	GtkWidget  *widget;

	chat = gossip_chat_get_for_jid (jid);
	widget = gossip_chat_get_dialog (chat);
	gtk_window_present (GTK_WINDOW (widget));

	app_pop_message (jid);
}

void
gossip_app_join_group_chat (const gchar *room,
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
	GossipAppPriv *priv;
	gboolean       enabled;
	gint           idle;
	gint           auto_away = 5*60;
	gint           auto_ext_away = 30*60;
	GossipShow     show;

	priv = app->priv;

	enabled =  gconf_client_get_bool (gconf_client,
					  "/apps/gossip/auto_away/enabled",
					  NULL);
	
	/* Make sure we don't get stuck in autoaway if it's turned off. */
	if (!enabled) {
		priv->auto_show = GOSSIP_SHOW_AVAILABLE;
		return TRUE;
	}

	/* auto_away = gconf_client_get_int (gconf_client,
	   "/apps/gossip/auto_away/idle_time",
	   NULL); */

	/* auto_ext_away = gconf_client_get_int (gconf_client,
	   "/apps/gossip/auto_away/extended_idle_time",
	   NULL); */

	idle = base + gossip_idle_get_seconds ();
	show = app_get_effective_show ();

	if (show != GOSSIP_SHOW_EXT_AWAY && idle >= auto_ext_away) {
		priv->auto_show = GOSSIP_SHOW_EXT_AWAY;
	}
	else if (show != GOSSIP_SHOW_AWAY && show != GOSSIP_SHOW_EXT_AWAY && 
		 idle >= auto_away) {
		priv->auto_show = GOSSIP_SHOW_AWAY;
	}
	else if (idle < auto_away) {
		priv->auto_show = GOSSIP_SHOW_AVAILABLE;
	}
	
	if (show != app_get_effective_show ()) {
		app_update_show ();
	}

	return TRUE;
}

void
gossip_app_connect (void)
{
	GossipAppPriv *priv;
	GossipAccount *account;
	GError        *error = NULL;

	priv = app->priv;

	if (priv->account) {
		gossip_account_unref (priv->account);
	}
	
	priv->account = gossip_account_get_default ();
	
	if (!priv->account) {
		return;
	}
	
	app_disconnect ();
	
	account = priv->account;
	
	lm_connection_set_server (priv->connection, account->server);
	lm_connection_set_port (priv->connection, account->port);
	lm_connection_set_use_ssl (priv->connection, account->use_ssl);

 	if (!lm_connection_open (priv->connection,
 				 (LmResultFunction) app_connection_open_cb,
 				 app, NULL, &error)) {
 		
 		if (error) {
 			GtkWidget *dialog;
			
 			d(g_print ("Failed to connect, error:%d, %s\n", error->code, error->reason));

			dialog = gossip_hig_dialog_new (GTK_WINDOW (priv->window),
							GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
							GTK_MESSAGE_ERROR,
							GTK_BUTTONS_OK,
							"Unable to connect",
 							 "%s\n%s",
							_("Make sure that your account information is correct."),
							_("The server may currently be unavailable."));

 			
 			gtk_dialog_run (GTK_DIALOG (dialog));
 			gtk_widget_destroy (dialog);

			g_error_free (error);
 		}
 	}
}

void
gossip_app_set_overridden_resource (const gchar *resource)
{
	g_free (app->priv->overridden_resource);
	app->priv->overridden_resource = g_strdup (resource);
}

void
gossip_app_create (void)
{
	/* Create singleton "app". */
	g_object_new (GOSSIP_TYPE_APP, NULL);
}

GossipApp *
gossip_app_get (void)
{
	g_assert (app != NULL);
	
	return app;
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

GossipRosterOld *
gossip_app_get_roster (void)
{
	return app->priv->roster;
}

static void
app_create_connection (void)
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

	handler = lm_message_handler_new ((LmHandleMessageFunction) app_show_handler, app, NULL);
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
app_disconnect (void)
{
	GossipAppPriv *priv = app->priv;

	if (lm_connection_is_open (priv->connection)) {
		lm_connection_close (priv->connection, NULL);
	}
}

LmConnection *
gossip_app_get_connection (void)
{
	return app->priv->connection;
}

static void
app_setup_conn_dependent_menu_items (GladeXML *glade)
{
	const gchar *connect_widgets[] = {
		"actions_disconnect",
		"actions_join_group_chat",
		"actions_send_chat_message",
		"actions_add_contact"
	};
	
	const gchar *disconnect_widgets[] = {
		"actions_connect"
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
app_update_conn_dependent_menu_items (void)
{
	GossipAppPriv *priv;
	GList         *l;
	gboolean       connected;

	priv = app->priv;
	
	connected = lm_connection_is_open (priv->connection);

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
			gtk_window_present (GTK_WINDOW (widget));
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
	gossip_roster_old_free_jid_list (data->jids);
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

GossipShow 
gossip_app_get_show (void)
{
	return app_get_effective_show ();
}

gboolean
gossip_app_is_connected (void)
{
	return lm_connection_is_open (app->priv->connection);
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
	
	priv = app->priv;

	gtk_widget_destroy (GTK_WIDGET (priv->tray_icon));
	app_create_tray_icon ();

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
		if (app_pop_message (NULL)) {
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
app_create_tray_icon (void)
{
	GossipAppPriv *priv;

	priv = app->priv;

	priv->tray_icon = egg_tray_icon_new (_("Gossip, Jabber Client"));
		
	priv->tray_event_box = gtk_event_box_new ();
	priv->tray_image = gtk_image_new ();
	
	gtk_image_set_from_stock (GTK_IMAGE (priv->tray_image),
				  app_get_current_status_icon (),
				  GTK_ICON_SIZE_MENU);
	
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

static gboolean
flash_timeout_func (gpointer data)
{
	GossipAppPriv   *priv;
	static gboolean  on = FALSE;
	const gchar     *icon;

	priv = app->priv;
	
	if (on) {
		icon = GOSSIP_STOCK_MESSAGE;
	} else {
		icon = app_get_current_status_icon ();
	}
	
	gtk_image_set_from_stock (GTK_IMAGE (priv->tray_image),
				  icon,
				  GTK_ICON_SIZE_MENU);
	
	on = !on;

	return TRUE;
}

static void
app_push_message (LmMessage *m)
{
	GossipAppPriv *priv;
	const gchar   *from;
	GossipJID     *jid;
	const gchar   *without_resource;
	GList         *l;
		
	priv = app->priv;
	
	from = lm_message_node_get_attribute (m->node, "from");
	jid = gossip_jid_new (from);

	without_resource = gossip_jid_get_without_resource (jid);
	
	l = g_list_find_custom (priv->tray_flash_icons,
				without_resource,
				(GCompareFunc) g_ascii_strcasecmp);
	if (l) {
		return;
	}

	priv->tray_flash_icons = g_list_append (priv->tray_flash_icons,
						g_strdup (without_resource));

	if (!priv->tray_flash_timeout_id) {
		priv->tray_flash_timeout_id = g_timeout_add (350,
							     flash_timeout_func,
							     NULL);

		app_tray_icon_update_tooltip ();
	}

	gossip_roster_old_flash_jid (priv->roster, jid, TRUE);

	gossip_jid_unref (jid);
}

static gboolean
app_pop_message (GossipJID *jid)
{
	GossipAppPriv *priv;
	const gchar   *without_resource;
	GossipChat    *chat;
	GtkWidget     *widget;
	GList         *l;

	priv = app->priv;

	if (!priv->tray_flash_icons) {
		return FALSE;
	}

	if (jid) {
		gossip_jid_ref (jid);
	} else {
		jid = gossip_jid_new (priv->tray_flash_icons->data);
	}
	
	chat = gossip_chat_get_for_jid (jid);
	if (!chat) {
		gossip_jid_unref (jid);
		return FALSE;
	}

	widget = gossip_chat_get_dialog (chat);
	gtk_window_present (GTK_WINDOW (widget));

	without_resource = gossip_jid_get_without_resource (jid);
	
	l = g_list_find_custom (priv->tray_flash_icons,
				without_resource,
				(GCompareFunc) g_ascii_strcasecmp);

	if (!l) {
		gossip_jid_unref (jid);
		return FALSE;
	}
	
	g_free (l->data);
	priv->tray_flash_icons = g_list_delete_link (priv->tray_flash_icons, l);

	if (!priv->tray_flash_icons && priv->tray_flash_timeout_id) {
		g_source_remove (priv->tray_flash_timeout_id);
		priv->tray_flash_timeout_id = 0;
		
		gtk_image_set_from_stock (GTK_IMAGE (priv->tray_image),
					  app_get_current_status_icon (),
					  GTK_ICON_SIZE_MENU);
	}

	app_tray_icon_update_tooltip ();

	gossip_roster_old_flash_jid (priv->roster, jid, FALSE);

	gossip_jid_unref (jid);

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
	
	name = gossip_roster_old_get_nick_from_jid (priv->roster, jid);
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

/******************** Test ********************/

static gboolean
grab_on_window (GdkWindow *window)
{
	guint32 activate_time = gtk_get_current_event_time ();
	
	if ((gdk_pointer_grab (window, TRUE,
			       GDK_BUTTON_PRESS_MASK |
			       GDK_BUTTON_RELEASE_MASK |
			       GDK_POINTER_MOTION_MASK,
			       NULL, NULL, activate_time) == 0)) {
		if (gdk_keyboard_grab (window, TRUE, activate_time) == 0) {
			return TRUE;
		} else {
			gdk_pointer_ungrab (activate_time);
			return FALSE;
		}
	}
	
	return FALSE;
}

static GossipShow
app_get_explicit_show (void)
{
	GossipAppPriv *priv;

	if (0) app_get_explicit_show ();
	
	priv = app->priv;

	return priv->explicit_show;
}

static GossipShow
app_get_effective_show (void)
{
	GossipAppPriv *priv;

	priv = app->priv;

	if (priv->auto_show != GOSSIP_SHOW_AVAILABLE) {
		return priv->auto_show;
	}

	return priv->explicit_show;
}

static const gchar *
app_get_current_status_icon (void)
{
	GossipAppPriv *priv;

	priv = app->priv;

	if (!lm_connection_is_open (priv->connection)) {
		return GOSSIP_STOCK_OFFLINE;
	}
	
	switch (app_get_effective_show ()) {
	case GOSSIP_SHOW_BUSY:
		return GOSSIP_STOCK_BUSY;
	case GOSSIP_SHOW_AWAY:
		return GOSSIP_STOCK_AWAY;
	case GOSSIP_SHOW_EXT_AWAY:
		return GOSSIP_STOCK_EXT_AWAY;
	case GOSSIP_SHOW_AVAILABLE:
	default:
		return GOSSIP_STOCK_AVAILABLE;
	}

	return NULL;
}

/* Updates the GUI bits and pieces to match the current show/connected
 * status. Also sends the correct show to the server.
 */
static void
app_update_show (void)
{
	GossipAppPriv *priv;
	const gchar   *icon;
	LmMessage     *m;
	
	priv = app->priv;
	
	if (priv->status_text) {
		eel_ellipsizing_label_set_text (EEL_ELLIPSIZING_LABEL (priv->status_label),
						priv->status_text);
	}
	
	icon = app_get_current_status_icon ();
	
	gtk_image_set_from_stock (GTK_IMAGE (priv->tray_image),
				  icon,
				  GTK_ICON_SIZE_MENU);
	gtk_image_set_from_stock (GTK_IMAGE (priv->status_image),
				  icon,
				  GTK_ICON_SIZE_MENU);
	
	if (!lm_connection_is_open (priv->connection)) {
		return;
	}
	
	m = lm_message_new_with_sub_type (NULL, 
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_AVAILABLE);
	
	if (app_get_effective_show () == GOSSIP_SHOW_BUSY) {
		lm_message_node_add_child (m->node, "show", "dnd");
	}
	
	lm_message_node_add_child (m->node, "status", priv->status_text);
	lm_connection_send (app->priv->connection, m, NULL);
	lm_message_unref (m);
}

static gboolean
status_popup_key_press_event_cb (GtkWidget   *widget,
				 GdkEventKey *event,
				 gpointer     data)
{
	GossipAppPriv *priv;

	priv = app->priv;
	
	switch (event->keyval) {
	case GDK_Escape:
		hide_popup (FALSE);
		return TRUE;
      
	case GDK_KP_Enter:
	case GDK_ISO_Enter:
	case GDK_3270_Enter:
	case GDK_Return:
		hide_popup (TRUE);
		return TRUE;
		
	default:
		break;
	}
	
	return FALSE;
}

static gboolean
status_popup_button_release_event_cb (GtkWidget      *widget,
				      GdkEventButton *event,
				      gpointer        user_data)
{
	GtkAllocation alloc;
	gdouble       x, y;
	gint          xoffset, yoffset;
	gint          x1, y1;
	gint          x2, y2;

	/* Only cancel for button 1 releases outside the popup window. */

	if (event->button != 1) {
		return FALSE;
	}
	
	/* Note, we could use gdk_event_get_root_coords but that's broken in
	 * older GTK+ versions so let's not do that for now.
	 */
	x = event->x_root;
	y = event->y_root;
	
	gdk_window_get_root_origin (widget->window,
				    &xoffset,
				    &yoffset);

	xoffset += widget->allocation.x;
	yoffset += widget->allocation.y;
	
	alloc = app->priv->status_popup->allocation;

	x1 = alloc.x + xoffset;
	y1 = alloc.y + yoffset;
	x2 = x1 + alloc.width;
	y2 = y1 + alloc.height;

	if (x > x1 && x < x2 && y > y1 && y < y2) {
		return FALSE;
	}

	hide_popup (TRUE);

	return FALSE;
}

static void
busy_toggled_cb (GtkToggleButton *button,
		 gpointer         user_data)
{
	grab_on_window (app->priv->status_popup->window);
}

static void
show_popup (void)
{
	GossipAppPriv  *priv;
	GdkScreen      *screen;
	GtkWidget      *frame;
	GtkWidget      *box;
	GtkWidget      *alignment;
	GtkWidget      *label;
	GtkRequisition  req;
	int             x, y;
	int             width, height;
	int             screen_width, screen_height;

	g_return_if_fail (app->priv->status_popup == NULL);

	priv = app->priv;

	screen =  gtk_widget_get_screen (GTK_WIDGET (priv->status_button));
	
	priv->status_popup = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_window_set_screen (GTK_WINDOW (priv->status_popup), screen);
	
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
	gtk_widget_show (frame);

	gtk_container_add (GTK_CONTAINER (priv->status_popup), frame);
	
	box = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (box), 4);
	gtk_widget_show (box);

	gtk_container_add (GTK_CONTAINER (frame), box);

	alignment = gtk_alignment_new (0, 0.5, 0, 1);
	gtk_widget_show (alignment);
	gtk_box_pack_start (GTK_BOX (box), alignment, FALSE, TRUE, 0);

	label = gtk_label_new_with_mnemonic (_("Set your status:"));
	gtk_widget_show (label);
	gtk_container_add (GTK_CONTAINER (alignment), label);
	
	priv->status_entry = gtk_entry_new ();
	gtk_widget_show (priv->status_entry);
	gtk_box_pack_start (GTK_BOX (box), priv->status_entry, FALSE, TRUE, 0);

	priv->status_busy_checkbutton = gtk_check_button_new_with_mnemonic (_("_Busy"));
	gtk_widget_show (priv->status_busy_checkbutton);
	gtk_box_pack_start (GTK_BOX (box), priv->status_busy_checkbutton, FALSE, TRUE, 0);
	
	gtk_entry_set_text (GTK_ENTRY (priv->status_entry), priv->status_text);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->status_busy_checkbutton),
				      priv->explicit_show == GOSSIP_SHOW_BUSY);
	
	g_signal_connect (priv->status_popup,
			  "button_release_event",
			  G_CALLBACK (status_popup_button_release_event_cb),
			  NULL);

	g_signal_connect (priv->status_popup,
			  "key_press_event",
			  G_CALLBACK (status_popup_key_press_event_cb),
			  NULL);

	g_signal_connect (priv->status_busy_checkbutton,
			  "toggled",
			  G_CALLBACK (busy_toggled_cb),
			  NULL);

	/* Align the popup with the button. */
	gtk_widget_size_request (priv->status_popup, &req);
  
	gdk_window_get_origin (GTK_BUTTON (priv->status_button)->event_window, &x, &y);
	gdk_drawable_get_size (GTK_BUTTON (priv->status_button)->event_window, &width, &height);

	x -= req.width - width;
	y += height;
  
	/* Clamp to screen size. */
	screen_height = gdk_screen_get_height (screen) - y;
	screen_width = gdk_screen_get_width (screen);

	if (req.height > screen_height) {
		/* It doesn't fit, so we see if we have the minimum space needed. */
		if (req.height > screen_height && y - height > screen_height) {
			/* Put the popup above the button instead. */
			screen_height = y - height;
			y -= (req.height + height);
			if (y < 0) {
				y = 0;
			}
		}
	}

	x = CLAMP (x, 0, screen_width - req.width);
	y = MAX (0, y);

	gtk_widget_set_size_request (priv->status_popup, req.width, -1);
	gtk_window_move (GTK_WINDOW (priv->status_popup), x, y);

	gtk_grab_add (priv->status_popup);
	gtk_widget_show (priv->status_popup);
	gtk_widget_grab_focus (priv->status_entry);

	if (!grab_on_window (priv->status_popup->window)) {
		gtk_grab_remove (priv->status_popup);
		gtk_widget_destroy (priv->status_popup);
		priv->status_popup = NULL;
		priv->status_entry = NULL;
	}
}

static void
hide_popup (gboolean apply)
{
	GossipAppPriv   *priv;
	const gchar     *str;
	GtkToggleButton *toggle;
	GossipShow       show;
	gboolean         changed = FALSE;
	guint32          event_time;

	priv = app->priv;
	
	if (apply && priv->status_popup) {
		str = gtk_entry_get_text (GTK_ENTRY (priv->status_entry));

		if (strcmp (priv->status_text, str) != 0) {
			g_free (priv->status_text);
			priv->status_text = g_strdup (str);

			changed = TRUE;
		}
		
		show = app_get_effective_show ();
		
		toggle = GTK_TOGGLE_BUTTON (priv->status_busy_checkbutton);
		if (gtk_toggle_button_get_active (toggle)) {
			priv->explicit_show = GOSSIP_SHOW_BUSY;
		} else {
			priv->explicit_show = GOSSIP_SHOW_AVAILABLE;
		}

		if (show != app_get_effective_show ()) {
			changed = TRUE;
		}
	}

	if (changed) {
		app_update_show ();
	}
	
	event_time = gtk_get_current_event_time ();

	if (priv->status_popup) {
		gtk_grab_remove (priv->status_popup);
		gdk_pointer_ungrab (event_time);		
		gdk_keyboard_ungrab (event_time);

		gtk_widget_destroy (priv->status_popup);

		priv->status_popup = NULL;
		priv->status_entry = NULL;
		priv->status_busy_checkbutton = NULL;
	}
	
	toggle = GTK_TOGGLE_BUTTON (priv->status_button);
	if (gtk_toggle_button_get_active (toggle)) {
		gtk_toggle_button_set_active (toggle, FALSE);
	}
}

static void
status_button_toggled_cb (GtkWidget *widget,
			  gpointer   user_data)
{
	GtkToggleButton *toggle;
	
	toggle = GTK_TOGGLE_BUTTON (widget);
	if (gtk_toggle_button_get_active (toggle)) {
		show_popup ();
	} else {
		hide_popup (TRUE);
	}
}

/*
// FIXME
const gchar *
gossip_status_to_string (GossipStatus status)
{
	switch (status) {
	case GOSSIP_STATUS_AVAILABLE:
		return NULL;
		break;
	case GOSSIP_STATUS_AWAY:
		return "away";
		break;
	case GOSSIP_STATUS_EXT_AWAY:
		return "xa";
		break;
	case GOSSIP_STATUS_BUSY:
		return "dnd";
		break;
	default:
		return NULL;
	}
	
	return NULL;
}

const gchar *gossip_status_to_icon_filename          (GossipStatus      status);
const gchar *gossip_status_to_string                 (GossipStatus      status);

*/

