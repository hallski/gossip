/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2004 Imendio HB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
 * Copyright (C) 2002-2003 Mikael Hallendal <micke@imendio.com>
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
#include <gdk/gdkx.h>
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
#include "gossip-sound.h"
#include "gossip-idle.h"
#include "gossip-startup-druid.h"
#include "gossip-preferences.h"
#include "gossip-account-dialog.h"
#include "gossip-stock.h"
#include "gossip-roster-view.h"
#include "gossip-app.h"

#define DEFAULT_RESOURCE _("Home")

#define ADD_CONTACT_RESPONSE_ADD 1
#define REQUEST_RESPONSE_DECIDE_LATER 1

/* Number of seconds before entering autoaway and extended autoaway. */
#define	AWAY_TIME (5*60)
#define	EXT_AWAY_TIME (30*60)

/* Number of seconds to flash the icon when explicitly entering away status
 * (activity is also allowed during this period).
 */
#define	LEAVE_SLACK 15

/* Number of seconds of slack when returning from autoaway. The _NOFLASH value
 * is the number of seconds to wait before starting to flash (to get a less
 * annoying effect).
 */
#define	BACK_SLACK 15
#define	BACK_SLACK_NOFLASH 5

/* Flashing delay for icons (milliseconds). */
#define FLASH_TIMEOUT 400

/* Minimum width of roster window if something goes wrong. */
#define MIN_WIDTH 50

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
	GtkWidget           *popup_menu_status_item;
	GtkWidget           *show_popup_item;
	GtkWidget           *hide_popup_item;
	
	GossipRoster        *roster;
	GossipRosterView    *roster_view;

	GossipAccount       *account;
	gchar               *overridden_resource;

	GossipJID           *jid;

	/* Widgets that are enabled when we're connected/disconnected. */
	GList               *enabled_connected_widgets;
	GList               *enabled_disconnected_widgets;

	/* Status popup. */
	GtkWidget           *status_button_hbox;
	GtkWidget           *status_button;
	GtkWidget           *status_popup;
	GtkWidget           *status_label;
	GtkWidget           *status_image;

	guint                status_flash_timeout_id;
	time_t               leave_time;

	/* Current status/overridden away message. */
	gchar               *status_text;
	gchar               *away_message;

	/* Set by the user (available/busy). */
	GossipShow           explicit_show;

	/* Autostatus (away/xa), overrides explicit_show. */
	GossipShow           auto_show;     

	guint                size_timeout_id;
};

typedef struct {
	GCompletion *completion;
	GList       *names;

	guint        complete_idle_id;
	gboolean     touched;
	
	GtkWidget   *combo;
	GtkWidget   *entry;
	GtkWidget   *dialog;
} CompleteNameData;

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
static void     app_item_activated_cb                (GossipRosterView   *roster_view,
						      GossipRosterItem   *item,
						      GossipApp          *app);
static gboolean app_idle_check_cb                    (GossipApp          *app);
static void     app_quit_cb                          (GtkWidget          *window,
						      GossipApp          *app);
static void     app_new_message_cb                   (GtkWidget          *widget,
						      gpointer            user_data);
static void     app_popup_new_message_cb             (GtkWidget          *widget,
						      gpointer            user_data);
static void     app_add_contact_cb                   (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_show_offline_cb                  (GtkCheckMenuItem   *item,
						      GossipApp          *app);
static void     app_show_offline_key_changed_cb      (GConfClient        *client,
						      guint               cnxn_id,
						      GConfEntry         *entry,
						      gpointer            check_menu_item);
static void     app_preferences_cb                   (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_account_information_cb           (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_status_messages_cb               (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_about_cb                         (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_join_group_chat_cb               (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_connection_open_cb               (LmConnection       *connection,
						      gboolean            result,
						      GossipApp          *app);
static LmSSLResponse
app_connection_ssl_func                              (LmSSL              *ssl,
						      LmSSLStatus         status,
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
app_show_handler                                     (LmMessageHandler   *handler,
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
static gboolean app_tray_destroy_event_cb            (GtkWidget          *widget,
						      GdkEvent           *event,
						      gpointer            user_data);
static void     app_tray_create_menu                 (void);
static void     app_tray_create                      (void);
static gboolean app_is_visible                       (void);
static void     app_create_connection                (void);
static void     app_disconnect                       (void);
static void     app_setup_conn_dependent_menu_items  (GladeXML           *glade);
static void     app_update_conn_dependent_menu_items (void);
static void     app_complete_name_response_cb         (GtkWidget          *dialog,
						      gint                response,
						      CompleteNameData    *data);
static gboolean app_complete_name_idle                (CompleteNameData    *data);
static void     app_complete_name_insert_text_cb      (GtkEntry           *entry,
						      const gchar        *text,
						      gint                length,
						      gint               *position,
						      CompleteNameData    *data);
static gboolean app_complete_name_key_press_event_cb  (GtkEntry           *entry,
						      GdkEventKey        *event,
						      CompleteNameData    *data);
static void     app_complete_name_activate_cb         (GtkEntry           *entry,
						      CompleteNameData    *data);
static gchar *  app_complete_item_to_string          (gpointer            data);
static void     app_toggle_visibility                (void);
static void     app_tray_push_message                (LmMessage          *m);
static gboolean app_tray_pop_message                 (GossipRosterItem   *item);
static void     app_tray_update_tooltip              (void);
static GtkWidget *
app_create_status_menu                               (void);
static gboolean app_status_button_press_event_cb     (GtkButton          *button,
						      GdkEventButton     *event,
						      gpointer            user_data);
static void     app_status_button_clicked_cb         (GtkButton          *button,
						      GdkEventButton     *event,
						      gpointer            user_data);
static void     app_status_show_status_dialog        (GossipShow          show,
						      const gchar        *str);
static GossipShow app_get_explicit_show              (void);
static GossipShow app_get_effective_show             (void);
static void       app_update_show                    (void);
static void       app_status_clear_away              (void);
static void       app_status_flash_start             (void);
static void       app_status_flash_stop              (void);
static const gchar * app_get_current_status_icon     (void);
static gboolean      app_window_configure_event_cb   (GtkWidget          *widget,
						      GdkEventConfigure  *event,
						      gpointer            data);
static gboolean      app_have_tray                   (void);


static GObjectClass *parent_class;
static GossipApp    *app;


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
app_init (GossipApp *singleton_app)
{
        GossipAppPriv  *priv;
	GladeXML       *glade;
	GtkWidget      *sw;
	GtkWidget      *status_label_hbox;
	gint            width, height;
	GtkRequisition  req;
	gboolean        show_offline;
	GtkWidget      *show_offline_widget;
	gint            x, y;
	gboolean        hidden;

	app = singleton_app;
	
        priv = g_new0 (GossipAppPriv, 1);
        app->priv = priv;

	priv->explicit_show = GOSSIP_SHOW_AVAILABLE;
	priv->auto_show = GOSSIP_SHOW_AVAILABLE;

	glade = gossip_glade_get_file (GLADEDIR "/main.glade",
				       "main_window",
				       NULL,
				       "main_window", &priv->window,
				       "roster_scrolledwindow", &sw,
				       "status_button_hbox", &priv->status_button_hbox,
				       "status_button", &priv->status_button,
				       "status_label_hbox", &status_label_hbox,
				       "status_image", &priv->status_image,
				       "actions_show_offline", &show_offline_widget,
				       NULL);

	width = gconf_client_get_int (gconf_client,
				      GCONF_PATH "/ui/main_window_width",
				      NULL);

	height = gconf_client_get_int (gconf_client,
				       GCONF_PATH "/ui/main_window_height",
				       NULL);

	width = MAX (width, MIN_WIDTH);
	gtk_window_set_default_size (GTK_WINDOW (priv->window), width, height);
	
	priv->status_label = eel_ellipsizing_label_new (NULL);
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
			      "actions_send_chat_message", "activate", app_new_message_cb,
			      "actions_add_contact", "activate", app_add_contact_cb,
			      "actions_show_offline", "toggled", app_show_offline_cb,
			      "edit_preferences", "activate", app_preferences_cb,
			      "edit_account_information", "activate", app_account_information_cb,
			      "edit_status_messages", "activate", app_status_messages_cb,
			      "help_about", "activate", app_about_cb,
			      "main_window", "destroy", app_main_window_destroy_cb,
			      "main_window", "configure_event", app_window_configure_event_cb,
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
						    DEFAULT_RESOURCE,
						    NULL,
						    LM_CONNECTION_DEFAULT_PORT,
						    FALSE);
	}

	app_create_connection ();
	
	priv->roster = gossip_roster_new ();
	priv->roster_view = gossip_roster_view_new (priv->roster);

	show_offline = gconf_client_get_bool (gconf_client,
					      "/apps/gossip/contact/show_offline",
					      NULL);

	gconf_client_notify_add (gconf_client,
				 "/apps/gossip/contact/show_offline",
				 app_show_offline_key_changed_cb,
				 show_offline_widget,
				 NULL, NULL);

	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (show_offline_widget),
					show_offline);
	
	gossip_roster_view_set_show_offline (GOSSIP_ROSTER_VIEW (priv->roster_view),
					     show_offline);

	gtk_widget_show (GTK_WIDGET (priv->roster_view));
	gtk_container_add (GTK_CONTAINER (sw),
			   GTK_WIDGET (priv->roster_view));

	g_signal_connect (priv->roster_view,
			  "item_activated",
			  G_CALLBACK (app_item_activated_cb),
			  app);

	app_tray_create_menu ();
	app_tray_create ();

	app_update_show ();

	app_update_conn_dependent_menu_items ();

	g_signal_connect (priv->status_button,
			  "button_press_event",
			  G_CALLBACK (app_status_button_press_event_cb),
			  NULL);

	g_signal_connect (priv->status_button,
			  "clicked",
			  G_CALLBACK (app_status_button_clicked_cb),
			  NULL);

	/* Start the idle time checker. */
	g_timeout_add (2 * 1000, (GSourceFunc) app_idle_check_cb, app);

	/* Set window position */
 	x = gconf_client_get_int (gconf_client, 
				  GCONF_PATH "/ui/main_window_position_x",
				  NULL);

	y = gconf_client_get_int (gconf_client, 
				  GCONF_PATH "/ui/main_window_position_y", 
				  NULL);
 
 	if (x >= 0 && y >= 0) {
 		gtk_window_move (GTK_WINDOW (priv->window), x, y);
	}

	hidden = gconf_client_get_bool (gconf_client, 
					GCONF_PATH "/ui/main_window_hidden", 
					NULL);

 	/* FIXME: See bug #132632, if (!hidden || !app_have_tray ()) {*/
 	if (!hidden) {
		gtk_widget_show (priv->window);
	}

	/* Note: this is a hack that sets the minimal size of the window so it
	 * doesn't allow resizing to a smaller width than the menubar takes
	 * up. We must set a minimal size, otherwise the window won't shrink
	 * beyond the longest string in the roster, which will cause the
	 * ellipsizing cell renderer not to work. FIXME: needs to update this on
	 * theme/font change.
	 */
	gtk_widget_size_request (priv->window, &req);
	req.width = MAX (req.width, MIN_WIDTH);
	gtk_widget_set_size_request (priv->window, req.width, -1);
}

static void
app_finalize (GObject *object)
{
        GossipApp     *app;
        GossipAppPriv *priv;
	
        app = GOSSIP_APP (object);
        priv = app->priv;

	if (priv->size_timeout_id) {
		g_source_remove (priv->size_timeout_id);
	}

	if (priv->tray_flash_timeout_id) {
		g_source_remove (priv->tray_flash_timeout_id);
	}

	if (priv->status_flash_timeout_id) {
		g_source_remove (priv->status_flash_timeout_id);
	}

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

	if (lm_connection_get_state (connection) == LM_CONNECTION_STATE_CONNECTING) {
		lm_connection_cancel_open (connection);
	} else if (lm_connection_is_open (connection)) {
		lm_connection_close (connection, NULL);
	}
	
	exit (0);
	/* gtk_main_quit (); */
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
	GossipAppPriv    *priv;
	static GtkWidget *about;
	const gchar *authors[] = { 
		"Richard Hult <richard@imendio.com>",
		"Mikael Hallendal <micke@imendio.com>",
		"",
		_("Artwork by:"),
		"Daniel Taylor <dantaylor@web.de>",
		NULL
	};
	/* Note to translators: put here your name and email so it
	 * will shop up in the "about" box
	 */
	gchar     *translator_credits = _("translator_credits");
	GtkWidget *href;
	GdkPixbuf *pixbuf;

	priv = app->priv;

	if (about) {
		gtk_window_present (GTK_WINDOW (about));
		return;
	}

	pixbuf = gdk_pixbuf_new_from_file (IMAGEDIR "/gossip-logo.png", NULL);
	
	about = gnome_about_new ("Gossip", VERSION,
				  "Copyright \xc2\xa9 2003-2004 Imendio HB",
				  _("An Instant Messaging Client for GNOME"),
				  authors,
				  NULL,
				  strcmp (translator_credits, "translator_credits") != 0 ?
				  translator_credits : NULL,
				  pixbuf);

	g_object_unref (pixbuf);
	
	href = gnome_href_new ("http://gossip.imendio.org/", _("Gossip Website"));
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (about)->vbox), href, FALSE, FALSE, 5);
	gtk_widget_show (href);
	
	g_object_add_weak_pointer (G_OBJECT (about), (gpointer) &about);

	gtk_window_set_transient_for (GTK_WINDOW (about), GTK_WINDOW (priv->window));
	gtk_widget_show (about);
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
app_join_group_chat_cb (GtkWidget *window,
			GossipApp *app)
{
	gossip_join_dialog_show ();
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

static gint
app_name_complete_cmp_func (const gchar *s1, const gchar *s2, gsize n)
{
	gchar *u1, *u2;
	gint   ret_val;
	
	u1 = g_utf8_casefold (s1, n);
	u2 = g_utf8_casefold (s2, n);

	ret_val = g_utf8_collate (u1, u2);
	g_free (u1);
	g_free (u2);

	return ret_val;
}

static void
app_new_message (gboolean use_roster_selection)
{
	GossipAppPriv    *priv;
	const gchar      *selected_name = NULL;
	GList            *l;
	GList            *items;
	GossipRosterItem *item = NULL;
	GtkWidget        *frame;
	CompleteNameData  *data;
	
	priv = app->priv;

	data = g_new0 (CompleteNameData, 1);

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
				G_CALLBACK (app_complete_name_insert_text_cb),
				data);
	
	g_signal_connect (data->entry,
			  "key_press_event",
			  G_CALLBACK (app_complete_name_key_press_event_cb),
			  data);

	g_signal_connect (data->entry,
			  "activate",
			  G_CALLBACK (app_complete_name_activate_cb),
			  data);

	g_signal_connect (data->dialog,
			  "response",
			  G_CALLBACK (app_complete_name_response_cb),
			  data);
	
	data->completion = g_completion_new (app_complete_item_to_string);
	g_completion_set_compare (data->completion, 
				  app_name_complete_cmp_func);
	if (use_roster_selection) {
		item = gossip_roster_view_get_selected_item (priv->roster_view);
	} else {
		item = NULL;
	}

	data->names = NULL;

	items = gossip_roster_get_all_items (priv->roster);
	for (l = items; l; l = l->next) {
		GossipRosterItem *roster_item = (GossipRosterItem *) l->data;
		
		data->names = g_list_prepend (data->names, 
					      g_strdup (gossip_roster_item_get_name (roster_item)));

		if (item == roster_item) {
			/* Got the selected one, select it in the combo. */
			selected_name = gossip_roster_item_get_name (roster_item);
		}
	}
	
	if (data->names) {
		gtk_combo_set_popdown_strings (GTK_COMBO (data->combo),
					       data->names);
	}

	if (items) {
		g_completion_add_items (data->completion, items);
	}

	if (selected_name) {
		gtk_entry_set_text (GTK_ENTRY (data->entry), selected_name);
		gtk_editable_select_region (GTK_EDITABLE (data->entry), 0, -1);
	} else {
		gtk_entry_set_text (GTK_ENTRY (data->entry), "");
	}

	gtk_widget_show (data->dialog);
}

static void
app_new_message_cb (GtkWidget *widget,
			  gpointer   user_data)
{
	app_new_message (TRUE);
}

static void
app_popup_new_message_cb (GtkWidget *widget,
			  gpointer   user_data)
{
	app_new_message (FALSE);
}

static void
app_add_contact_cb (GtkWidget *widget, GossipApp *app)
{
	gossip_add_contact_new (app->priv->connection, NULL);
}

static void
app_show_offline_cb (GtkCheckMenuItem *item, GossipApp *app)
{
	GossipAppPriv *priv;
	gboolean       current;
	
	priv = app->priv;

	current = gtk_check_menu_item_get_active (item);

	gconf_client_set_bool (gconf_client,
			       "/apps/gossip/contact/show_offline",
			       current,
			       NULL);

	gossip_roster_view_set_show_offline (GOSSIP_ROSTER_VIEW (priv->roster_view),
					     current);
}

static void
app_show_offline_key_changed_cb (GConfClient *client,
				 guint        cnxn_id,
				 GConfEntry  *entry,
				 gpointer     check_menu_item)
{
	gboolean show_offline;

	show_offline = gconf_value_get_bool (gconf_entry_get_value (entry));

	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (check_menu_item),
					show_offline);
}

static void
app_preferences_cb (GtkWidget *widget, GossipApp *app)
{
	gossip_preferences_show ();
}

static void
app_account_information_cb (GtkWidget *widget, GossipApp *app)
{
	gossip_account_dialog_show ();
}

static void
app_status_messages_cb (GtkWidget *widget, GossipApp *app)
{
	gossip_preferences_show_status_editor ();
}

static void
app_subscription_request_dialog_response_cb (GtkWidget *dialog,
					     gint       response,
					     GossipApp *app)
{
	GossipAppPriv    *priv;
	LmMessage        *m;
	LmMessage        *reply = NULL;
	gboolean          add_user;
	GtkWidget        *add_check_button;
	LmMessageSubType  sub_type = LM_MESSAGE_SUB_TYPE_NOT_SET;
	const gchar      *from;
	GossipRosterItem *item;
	GossipJID        *jid;

	g_return_if_fail (GTK_IS_DIALOG (dialog));
	g_return_if_fail (GOSSIP_IS_APP (app));
  
	priv = app->priv;
	
	m = g_object_get_data (G_OBJECT (dialog), "message");

	if (!m) {
		g_warning ("Message not set on subscription request dialog\n");
		return;
	}

	add_check_button = g_object_get_data (G_OBJECT (dialog), 
					      "add_check_button");
	add_user = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (add_check_button));

	gtk_widget_destroy (dialog);
	
	switch (response) {
	case REQUEST_RESPONSE_DECIDE_LATER:
		/* Decide later. */
		return;
		break;
	case GTK_RESPONSE_DELETE_EVENT:
		/* Do nothing */
		return;
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
	
	from = lm_message_node_get_attribute (m->node, "from");
	
	reply = lm_message_new_with_sub_type (from, 
					      LM_MESSAGE_TYPE_PRESENCE,
					      sub_type);

	lm_connection_send (priv->connection, reply, NULL);
	lm_message_unref (reply);
	
	jid = gossip_jid_new (from);
	item = gossip_roster_get_item (priv->roster, jid);

	if (add_user && !item && sub_type == LM_MESSAGE_SUB_TYPE_SUBSCRIBED) {
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
	if (gossip_roster_get_item (app->priv->roster, jid)) {
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
		app_tray_push_message (m);
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
		resource = DEFAULT_RESOURCE;
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

static LmSSLResponse
app_connection_ssl_func (LmSSL *ssl, LmSSLStatus status, GossipApp *app)
{
	return LM_SSL_RESPONSE_CONTINUE;
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
		dialog = gtk_message_dialog_new (NULL,
						 0,
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
app_item_activated_cb (GossipRosterView *roster,
		       GossipRosterItem *item,
		       GossipApp        *app)
{
	GossipChat *chat;

	chat = gossip_chat_get_for_item (item, TRUE);
	gossip_chat_present (chat);

	app_tray_pop_message (item);
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
	gint32         idle;
	time_t         now;
	GossipShow     show;

	priv = app->priv;

	idle = gossip_idle_get_seconds ();
	show = app_get_effective_show ();

	if (priv->leave_time > 0) {
		/* We're going away, allow some slack. */
		now = time (NULL);

		if (now - priv->leave_time > LEAVE_SLACK) {
			priv->leave_time = 0;
			app_status_flash_stop ();

			gossip_idle_reset ();

			d(g_print ("OK, away now.\n"));
		}
		
		return TRUE;
	}
	else if (show != GOSSIP_SHOW_EXT_AWAY && idle > EXT_AWAY_TIME) {
		priv->auto_show = GOSSIP_SHOW_EXT_AWAY;
	}
	else if (show != GOSSIP_SHOW_AWAY && show != GOSSIP_SHOW_EXT_AWAY && 
		 idle > AWAY_TIME) {
		priv->auto_show = GOSSIP_SHOW_AWAY;
	}
	else if (show == GOSSIP_SHOW_AWAY || show == GOSSIP_SHOW_EXT_AWAY) {
		if (idle >= -BACK_SLACK && idle <= -BACK_SLACK_NOFLASH) {
			/* Allow some slack before returning from away. */
			d(g_print ("Slack, do nothing.\n"));
			app_status_flash_start ();
		}
		else if (idle < -(BACK_SLACK + BACK_SLACK_NOFLASH)) {
			d(g_print ("No more slack, break away.\n"));

			priv->auto_show = GOSSIP_SHOW_AVAILABLE;
			
			g_free (priv->away_message);
			priv->away_message = NULL;

			app_status_flash_stop ();
		}
		else if (idle > BACK_SLACK) {
			d(g_print ("OK, don't go back.\n"));
			app_status_flash_stop ();
		}
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
	gboolean       result;

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
	
	if (account->use_ssl) {
		LmSSL *ssl = lm_ssl_new (NULL,  
					 (LmSSLFunction) app_connection_ssl_func,
					 app,
					 NULL);
		
		lm_connection_set_ssl (priv->connection, ssl);
		lm_ssl_unref (ssl);
	}
	
	result = lm_connection_open (priv->connection,
				     (LmResultFunction) app_connection_open_cb,
				     app, NULL, &error);

	if (result == FALSE && error) {
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

GossipRoster *
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
		"actions_add_contact",
		"status_button"
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
app_complete_name_response_cb (GtkWidget        *dialog,
			       gint              response,
			       CompleteNameData *data)
{
	const gchar *str;
	GossipChat  *chat;
	GList       *l;

	if (response == GTK_RESPONSE_OK) {
		GossipRosterItem *item = NULL;
		str = gtk_entry_get_text (GTK_ENTRY (data->entry));
		
		item = gossip_roster_find_item (gossip_app_get_roster (), str);
		if (!item) {
			/* FIXME: Display error dialog... */
			g_warning ("'%s' is not a valid JID or nick name.", str);
		}

		chat = gossip_chat_get_for_item (item, TRUE);
				
		gossip_chat_present (chat);
	}

	for (l = data->names; l; l = l->next) {
		g_free (l->data);
	}
	g_list_free (data->names);
	g_free (data);
	
	gtk_widget_destroy (dialog);	
}

static gboolean 
app_complete_name_idle (CompleteNameData *data)
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
						 app_complete_name_insert_text_cb,
						 data);
		
  		gtk_entry_set_text (GTK_ENTRY (data->entry), new_prefix); 
					  
		g_signal_handlers_unblock_by_func (data->entry, 
						   app_complete_name_insert_text_cb,
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
app_complete_name_insert_text_cb (GtkEntry        *entry, 
				  const gchar     *text,
				  gint             length,
				  gint            *position,
				  CompleteNameData *data)
{
	if (!data->complete_idle_id) {
		data->complete_idle_id = g_idle_add ((GSourceFunc) app_complete_name_idle, data);
	}
}

static gboolean
app_complete_name_key_press_event_cb (GtkEntry        *entry,
				      GdkEventKey     *event,
				      CompleteNameData *data)
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
app_complete_name_activate_cb (GtkEntry        *entry,
			       CompleteNameData *data)
{
	gtk_dialog_response (GTK_DIALOG (data->dialog), GTK_RESPONSE_OK);
}

static gchar *
app_complete_item_to_string (gpointer data)
{ 
	GossipRosterItem *item = data;

	return (gchar *) gossip_roster_item_get_name (item);
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

static gboolean
app_is_visible (void)
{
	GossipAppPriv *priv;
	gboolean       visible;

	priv = app->priv;
	
	g_object_get (priv->window,
		      "visible", &visible,
		      NULL);
	
	if (priv->window->window) {
		GdkWindow *window;
		
		window = priv->window->window;
		
		visible = visible && !(gdk_window_get_state (window) & GDK_WINDOW_STATE_ICONIFIED);
	}
	
	return visible;
}

static void
app_toggle_visibility (void)
{
	GossipAppPriv *priv;
	gboolean       visible;

	priv = app->priv;

	visible = app_is_visible ();

	if (visible) {
		gtk_widget_hide (priv->window);

		gconf_client_set_bool (gconf_client, 
				       GCONF_PATH "/ui/main_window_hidden", TRUE,
				       NULL);
	} else {
		gint x, y;

		x = gconf_client_get_int (gconf_client, 
					  GCONF_PATH "/ui/main_window_position_x",
					  NULL);
		y = gconf_client_get_int (gconf_client, 
					  GCONF_PATH "/ui/main_window_position_y",
					  NULL);
		
		if (x >= 0 && y >= 0) {
			gtk_window_move (GTK_WINDOW (priv->window), x, y);
		}

		gtk_window_present (GTK_WINDOW (priv->window));

		gconf_client_set_bool (gconf_client, 
				       GCONF_PATH "/ui/main_window_hidden", FALSE,
				       NULL);
	}
}

static gboolean
app_tray_destroy_event_cb (GtkWidget *widget,
			   GdkEvent  *event,
			   gpointer   user_data)
{
	GossipAppPriv *priv = app->priv;
	
	gtk_widget_destroy (GTK_WIDGET (priv->tray_icon));
	priv->tray_icon = NULL;
	priv->tray_event_box = NULL;
	priv->tray_image = NULL;
	priv->tray_tooltips = NULL;

	if (priv->tray_flash_timeout_id) {
		g_source_remove (priv->tray_flash_timeout_id);
		priv->tray_flash_timeout_id = 0;
	}
	
	app_tray_create ();
	
	/* Show the window in case the notification area was removed. */
	if (!app_have_tray ()) {
		gtk_widget_show (app->priv->window);
	}

	return TRUE;
}

static gboolean
app_tray_button_press_cb (GtkWidget      *widget, 
			  GdkEventButton *event, 
			  GossipApp      *app)
{
	GossipAppPriv *priv;

	priv = app->priv;

	if (event->type == GDK_2BUTTON_PRESS ||
	    event->type == GDK_3BUTTON_PRESS) {
		return FALSE;
	}
	
	switch (event->button) {
	case 1:
		if (app_tray_pop_message (NULL)) {
			break;
		}

		app_toggle_visibility ();
		break;

	case 3:
		if (app_is_visible ()) {
			gtk_widget_show (priv->hide_popup_item);
			gtk_widget_hide (priv->show_popup_item);
		} else {
			gtk_widget_hide (priv->hide_popup_item);
			gtk_widget_show (priv->show_popup_item);
		}

		{
			GtkWidget *submenu;
			
			submenu = app_create_status_menu ();

			gtk_menu_item_set_submenu (GTK_MENU_ITEM (priv->popup_menu_status_item), submenu);
		}
		
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
app_tray_create_menu (void)
{
	GossipAppPriv *priv;
	GladeXML      *glade;
	GtkWidget     *message_item;

	priv = app->priv;

	glade = gossip_glade_get_file (GLADEDIR "/main.glade",
				       "tray_menu",
				       NULL,
				       "tray_menu", &priv->popup_menu,
				       "tray_show_list", &priv->show_popup_item,
				       "tray_hide_list", &priv->hide_popup_item,
				       "tray_new_message", &message_item,
				       "tray_status", &priv->popup_menu_status_item,
				       NULL);

	gossip_glade_connect (glade,
			      app,
			      "tray_show_list", "activate", app_show_hide_activate_cb,
			      "tray_hide_list", "activate", app_show_hide_activate_cb,
			      "tray_new_message", "activate", app_popup_new_message_cb,
			      "tray_quit", "activate", app_quit_cb,
			      NULL);
	
	priv->enabled_connected_widgets = g_list_prepend (priv->enabled_connected_widgets,
							  priv->popup_menu_status_item);

	priv->enabled_connected_widgets = g_list_prepend (priv->enabled_connected_widgets,
							  message_item);

	g_object_unref (glade);
}

static void
app_tray_create (void)
{
	GossipAppPriv *priv;

	priv = app->priv;

	priv->tray_icon = egg_tray_icon_new (_("Gossip, Instant Messaging Client"));
		
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
			  G_CALLBACK (app_tray_button_press_cb),
			  app);
	
	g_signal_connect (priv->tray_icon,
			  "destroy_event",
			  G_CALLBACK (app_tray_destroy_event_cb),
			  priv->tray_event_box);
}

static gboolean
app_tray_flash_timeout_func (gpointer data)
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
app_tray_push_message (LmMessage *m)
{
	GossipAppPriv    *priv;
	const gchar      *from;
	GossipJID        *jid; 
	GossipRosterItem *item;
	GList            *l;
		
	priv = app->priv;
	
	from = lm_message_node_get_attribute (m->node, "from");
	jid = gossip_jid_new (from);

	item = gossip_roster_get_item (priv->roster, jid);
	if (!item) {
		item = gossip_roster_item_new (jid);
	} else {
		gossip_roster_item_ref (item);
	}

	gossip_jid_unref (jid);
	
	l = g_list_find_custom (priv->tray_flash_icons, item,
				(GCompareFunc) gossip_roster_item_compare);
	if (l) {
		return;
	}

	priv->tray_flash_icons = g_list_append (priv->tray_flash_icons, 
						gossip_roster_item_ref (item));

	if (!priv->tray_flash_timeout_id) {
		priv->tray_flash_timeout_id = g_timeout_add (FLASH_TIMEOUT,
							     app_tray_flash_timeout_func,
							     NULL);

		app_tray_update_tooltip ();
	}

	gossip_roster_view_flash_item (priv->roster_view, item, TRUE);
	gossip_roster_item_unref (item);
}

static gboolean
app_tray_pop_message (GossipRosterItem *item)
{
	GossipAppPriv *priv;
	GossipChat    *chat;
	GList         *l;

	priv = app->priv;

	if (!priv->tray_flash_icons) {
		return FALSE;
	}

	if (!item) {
		item = priv->tray_flash_icons->data;
	}
	
	chat = gossip_chat_get_for_item (item, TRUE);
	if (!chat) {
		return FALSE;
	}

	gossip_chat_present (chat);

	l = g_list_find_custom (priv->tray_flash_icons, item,
				(GCompareFunc) gossip_roster_item_compare);

	if (!l) {
		return FALSE;
	}

	priv->tray_flash_icons = g_list_delete_link (priv->tray_flash_icons, l);

	if (!priv->tray_flash_icons && priv->tray_flash_timeout_id) {
		g_source_remove (priv->tray_flash_timeout_id);
		priv->tray_flash_timeout_id = 0;
		
		gtk_image_set_from_stock (GTK_IMAGE (priv->tray_image),
					  app_get_current_status_icon (),
					  GTK_ICON_SIZE_MENU);
	}

	app_tray_update_tooltip ();

	if (item) {
		gossip_roster_view_flash_item (priv->roster_view, item, FALSE);
	}
	
	gossip_roster_item_unref (item);

	return TRUE;
}

static void
app_tray_update_tooltip (void)
{
	GossipAppPriv    *priv;
	const gchar      *name;
	gchar            *str;
	GossipRosterItem *item;

	priv = app->priv;

	if (!priv->tray_flash_icons) {
		gtk_tooltips_set_tip (GTK_TOOLTIPS (priv->tray_tooltips),
				      priv->tray_event_box,
				      NULL, NULL);
		return;
	}

	item = priv->tray_flash_icons->data;
	
	name = gossip_roster_item_get_name (item);
	
	str = g_strdup_printf (_("New message from %s"), name);
	
	gtk_tooltips_set_tip (GTK_TOOLTIPS (priv->tray_tooltips),
			      priv->tray_event_box,
			      str, str);
	
	g_free (str);
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
	GossipShow     effective_show;
	const gchar   *icon;
	LmMessage     *m;
	const gchar   *show = NULL;
	gchar         *priority = "50"; /* Default priority when available. */
	gchar         *status_text;     /* Text to send. */
	const gchar   *status_label;    /* Text to display in the UI. */
	
	priv = app->priv;

	icon = app_get_current_status_icon ();
	
	gtk_image_set_from_stock (GTK_IMAGE (priv->tray_image),
				  icon,
				  GTK_ICON_SIZE_MENU);
	gtk_image_set_from_stock (GTK_IMAGE (priv->status_image),
				  icon,
				  GTK_ICON_SIZE_MENU);
	
	if (!lm_connection_is_open (priv->connection)) {
		eel_ellipsizing_label_set_text (EEL_ELLIPSIZING_LABEL (priv->status_label),
						_("Offline"));
		return;
	}
	
	m = lm_message_new_with_sub_type (NULL, 
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_AVAILABLE);

	effective_show = app_get_effective_show ();
	
	if (effective_show == GOSSIP_SHOW_AWAY ||
	    effective_show == GOSSIP_SHOW_EXT_AWAY) {
		status_text = g_strdup (priv->away_message);
	} else {
		status_text = g_strdup (priv->status_text);
		if (status_text) {
			status_label = g_strdup (priv->status_text);
		} else {
			status_label = g_strdup (gossip_utils_get_default_status (effective_show));
		}
	}

	if (status_text && status_text[0]) {
		status_label = status_text;
	} else {
		status_label = gossip_utils_get_default_status (effective_show);
	}

	eel_ellipsizing_label_set_text (EEL_ELLIPSIZING_LABEL (priv->status_label),
					status_label);

	show = gossip_utils_show_to_string (effective_show);

	switch (effective_show) {
	case GOSSIP_SHOW_BUSY:
		priority = "40";
		break;
	case GOSSIP_SHOW_AWAY:
		priority = "30";
		break;
	case GOSSIP_SHOW_EXT_AWAY:
		priority = "0";
		break;
	default:
		break;
	}
	
	if (show) {
		lm_message_node_add_child (m->node, "show", show);
	}
	
	lm_message_node_add_child (m->node, "priority", priority);
	if (status_text) {
		lm_message_node_add_child (m->node, "status", status_text);
	}
	lm_connection_send (app->priv->connection, m, NULL);
	lm_message_unref (m);

	g_free (status_text);
}

/* Clears status data from autoaway mode. */ 
static void
app_status_clear_away (void)
{
	GossipAppPriv *priv = app->priv;
	
	g_free (priv->away_message);
	priv->away_message = NULL;

	priv->leave_time = 0;

	priv->auto_show = GOSSIP_SHOW_AVAILABLE;

	/* Force this so we don't get a delay in the display. */
	app_update_show ();
}

static gboolean
app_status_flash_timeout_func (gpointer data)
{
	GossipAppPriv   *priv = app->priv;
	static gboolean  on = FALSE;
	const gchar     *icon;

	if (on) {
		switch (priv->explicit_show) {
		case GOSSIP_SHOW_BUSY:
			icon = GOSSIP_STOCK_BUSY;
			break;
		case GOSSIP_SHOW_AVAILABLE:
			icon = GOSSIP_STOCK_AVAILABLE;
			break;
		default:
			icon = GOSSIP_STOCK_AVAILABLE;
			g_assert_not_reached ();
			break;
		}
	} else {
		icon = app_get_current_status_icon ();
	}
	
	gtk_image_set_from_stock (GTK_IMAGE (priv->status_image),
				  icon,
				  GTK_ICON_SIZE_MENU);
	
	on = !on;

	return TRUE;
}

static void
app_status_flash_start (void)
{
	GossipAppPriv *priv = app->priv;

	if (!priv->status_flash_timeout_id) {
		priv->status_flash_timeout_id = g_timeout_add (FLASH_TIMEOUT,
							       app_status_flash_timeout_func,
							       NULL);
	}
}

static void
app_status_flash_stop (void)
{
	GossipAppPriv *priv = app->priv;
	const gchar   *icon;

	icon = app_get_current_status_icon ();
	
	gtk_image_set_from_stock (GTK_IMAGE (priv->status_image),
				  icon,
				  GTK_ICON_SIZE_MENU);

	if (priv->status_flash_timeout_id) {
		g_source_remove (priv->status_flash_timeout_id);
		priv->status_flash_timeout_id = 0;
	}
}

void
gossip_app_status_force_nonaway (void)
{
	GossipAppPriv *priv = app->priv;

	/* If we just left, allow some slack. */
	if (priv->leave_time) {
		return;
	}
	
	if (priv->auto_show == GOSSIP_SHOW_AWAY || priv->auto_show == GOSSIP_SHOW_EXT_AWAY) {
		app_status_clear_away ();
	}
}

static void
app_status_align_menu (GtkMenu  *menu,
		       gint     *x,
		       gint     *y,
		       gboolean *push_in,
		       gpointer  user_data)
{
	GossipAppPriv  *priv = app->priv;
	GtkWidget      *button;
	GtkRequisition  req;
	GdkScreen      *screen;
	gint            width, height;
	gint            screen_height;

	button = priv->status_button;
	
	gtk_widget_size_request (GTK_WIDGET (menu), &req);
  
	gdk_window_get_origin (GTK_BUTTON (button)->event_window, x, y);
	gdk_drawable_get_size (GTK_BUTTON (button)->event_window, &width, &height);

	*x -= req.width - width;
	*y += height;

	screen = gtk_widget_get_screen (GTK_WIDGET (menu));
	
	/* Clamp to screen size. */
	screen_height = gdk_screen_get_height (screen) - *y;	
	if (req.height > screen_height) {
		/* It doesn't fit, so we see if we have the minimum space needed. */
		if (req.height > screen_height && *y - height > screen_height) {
			/* Put the menu above the button instead. */
			screen_height = *y - height;
			*y -= (req.height + height);
			if (*y < 0) {
				*y = 0;
			}
		}
	}
}

static void
app_status_available_activate_cb (GtkWidget *item,
				  gpointer   user_data)
{
	GossipAppPriv *priv = app->priv;
	gchar         *str;

	app_status_flash_stop ();

	str = g_object_get_data (G_OBJECT (item), "status");
	g_free (priv->status_text);
	priv->status_text = g_strdup (str);

	priv->explicit_show = GOSSIP_SHOW_AVAILABLE;
	
	app_status_clear_away ();
}

static void
app_status_busy_activate_cb (GtkWidget *item,
			     gpointer   user_data)
{
	GossipAppPriv *priv = app->priv;
	gchar         *str;

	app_status_flash_stop ();
	
	str = g_object_get_data (G_OBJECT (item), "status");
	g_free (priv->status_text);
	priv->status_text = g_strdup (str);

	priv->explicit_show = GOSSIP_SHOW_BUSY;
	
	app_status_clear_away ();
}

static void
app_status_away_activate_cb (GtkWidget *item,
			     gpointer   user_data)
{
	GossipAppPriv *priv = app->priv;
	gchar         *str;

	/* If we are already away, don't go back to available, just change the
	 * message.
	 */
	if (priv->auto_show != GOSSIP_SHOW_AWAY &&
	    priv->auto_show != GOSSIP_SHOW_EXT_AWAY) {
		priv->leave_time = time (NULL);
		app_status_flash_start ();
	}

	gossip_idle_reset ();
	priv->auto_show = GOSSIP_SHOW_AWAY;

	str = g_object_get_data (G_OBJECT (item), "status");
	g_free (priv->away_message);
	priv->away_message = g_strdup (str);
	
	app_update_show ();
}

static void
app_status_show_status_dialog (GossipShow show, const gchar *str)
{
        GossipAppPriv  *priv;
	GladeXML       *glade;
	GtkWidget      *dialog;
	GtkWidget      *image;
	GtkWidget      *entry;
	gint            response;

        priv = app->priv;

	glade = gossip_glade_get_file (GLADEDIR "/main.glade",
				       "status_message_dialog",
				       NULL,
				       "status_message_dialog", &dialog,
				       "status_entry", &entry,
				       "status_image", &image,
				       NULL);

	gtk_entry_set_text (GTK_ENTRY (entry), str);

	gtk_image_set_from_stock (GTK_IMAGE (image),
				  gossip_utils_get_stock_from_show (show),
				  GTK_ICON_SIZE_MENU);

	gtk_window_set_transient_for (GTK_WINDOW (dialog),
				      GTK_WINDOW (priv->window));

	gtk_widget_grab_focus (entry);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_hide (dialog);

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}
		
	str = gtk_entry_get_text (GTK_ENTRY (entry));

	if (strcmp (str, gossip_utils_get_default_status (show)) == 0) {
		str = NULL;
	}

	if (show != GOSSIP_SHOW_AWAY) {
		app_status_flash_stop ();
		
		g_free (priv->status_text);
		priv->status_text = g_strdup (str);

		priv->explicit_show = show;

		app_status_clear_away ();
	} else {
		/* If we are already away, don't go back to available, just
		 * change the message.
		 */
		if (priv->auto_show != GOSSIP_SHOW_AWAY &&
		    priv->auto_show != GOSSIP_SHOW_EXT_AWAY) {
			priv->leave_time = time (NULL);
			app_status_flash_start ();
		}

		gossip_idle_reset ();
		priv->auto_show = GOSSIP_SHOW_AWAY;

		g_free (priv->away_message);
		priv->away_message = g_strdup (str);

		app_update_show ();
	}
	
	gtk_widget_destroy (dialog);
}

static void
app_status_custom_available_activate_cb (GtkWidget *item,
					 gpointer   user_data)
{
	app_status_show_status_dialog (GOSSIP_SHOW_AVAILABLE, _("Available"));
}

static void
app_status_custom_busy_activate_cb (GtkWidget *item,
				    gpointer   user_data)
{
	app_status_show_status_dialog (GOSSIP_SHOW_BUSY, _("Busy"));
}

static void
app_status_custom_away_activate_cb (GtkWidget *item,
				    gpointer   user_data)
{
	app_status_show_status_dialog (GOSSIP_SHOW_AWAY, _("Away"));
}

static void
app_status_edit_activate_cb (GtkWidget *item,
			     gpointer   user_data)
{
	gossip_preferences_show_status_editor ();
}

static gchar *
ellipsize_string (const gchar *str, gint len)
{
	gchar *copy;
	gchar *tmp;

	copy = g_strdup (str);
	
	if (g_utf8_strlen (copy, -1) > len + 4) {
		tmp = g_utf8_offset_to_pointer (copy, len);
		
		tmp[0] = '.';
		tmp[1] = '.';
		tmp[2] = '.';
		tmp[3] = '\0';
	}

	return copy;
}

static void
add_status_image_menu_item (GtkWidget   *menu,
			    const gchar *str,
			    GossipShow   show,
			    gboolean     custom)
{
	gchar       *shortened;
	GtkWidget   *item;
	GtkWidget   *image;
	const gchar *stock;

	g_return_if_fail (show == GOSSIP_SHOW_AVAILABLE ||
			  show == GOSSIP_SHOW_BUSY ||
			  show == GOSSIP_SHOW_AWAY);

	shortened = ellipsize_string (str, 25);
	
	item = gtk_image_menu_item_new_with_label (shortened);

	switch (show) {
	case GOSSIP_SHOW_AVAILABLE:
		stock = GOSSIP_STOCK_AVAILABLE;

		if (custom) {
			g_signal_connect (item,
					  "activate",
					  G_CALLBACK (app_status_custom_available_activate_cb),
					  NULL);
		} else {
			g_signal_connect (item,
					  "activate",
					  G_CALLBACK (app_status_available_activate_cb),
					  NULL);
		}
		break;
		
	case GOSSIP_SHOW_BUSY:
		stock = GOSSIP_STOCK_BUSY;

		if (custom) {
			g_signal_connect (item,
					  "activate",
					  G_CALLBACK (app_status_custom_busy_activate_cb),
					  NULL);
		} else {
			g_signal_connect (item,
					  "activate",
					  G_CALLBACK (app_status_busy_activate_cb),
					  NULL);
		}
		break;

	case GOSSIP_SHOW_AWAY:
		stock = GOSSIP_STOCK_AWAY;

		if (custom) {
			g_signal_connect (item,
					  "activate",
					  G_CALLBACK (app_status_custom_away_activate_cb),
					  NULL);
		} else {
			g_signal_connect (item,
					  "activate",
					  G_CALLBACK (app_status_away_activate_cb),
					  NULL);
		}
		break;

	default:
		g_assert_not_reached ();
		stock = NULL;
		break;
	}
	
	image = gtk_image_new_from_stock (stock, GTK_ICON_SIZE_MENU);
	gtk_widget_show (image);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	gtk_widget_show (item);

	g_object_set_data_full (G_OBJECT (item),
				"status", g_strdup (str),
				(GDestroyNotify) g_free);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	g_free (shortened);
}
	
static GtkWidget *
app_create_status_menu (void)
{
	GtkWidget *menu;
	GtkWidget *item;
	GList     *list, *l;

	menu = gtk_menu_new ();

	add_status_image_menu_item (menu, _("Available..."),
				    GOSSIP_SHOW_AVAILABLE,
				    TRUE);

	add_status_image_menu_item (menu, _("Busy..."),
				    GOSSIP_SHOW_BUSY,
				    TRUE);

	add_status_image_menu_item (menu, _("Away..."),
				    GOSSIP_SHOW_AWAY,
				    TRUE);
	
	/* Separator. */
	item = gtk_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	/* Preset messages. */
	list = gossip_utils_get_status_messages ();
	for (l = list; l; l = l->next) {
		GossipStatusEntry *entry = l->data;
		
		add_status_image_menu_item (menu, entry->string, entry->show, FALSE);
	}

	/* Separator again if there are preset messages. */
	if (list) {
		item = gtk_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);
	}

	gossip_utils_free_status_messages (list);

	item = gtk_menu_item_new_with_label (_("Edit List..."));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
	
	g_signal_connect (item,
			  "activate",
			  G_CALLBACK (app_status_edit_activate_cb),
			  NULL);

	return menu;
}

static void
app_status_popup_show (void)
{
	GossipAppPriv *priv;
	GtkWidget     *menu;

	priv = app->priv;
	
	menu = app_create_status_menu ();

	gtk_widget_set_size_request (menu, priv->status_button->allocation.width, -1);
	
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, app_status_align_menu,
			NULL, 1, gtk_get_current_event_time ());
}

static gboolean
app_status_button_press_event_cb (GtkButton      *button,
				  GdkEventButton *event,
				  gpointer        user_data)
{
	if (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS) {
		return FALSE;
	}
	
	if (event->button != 1) {
		return FALSE;
	}
	
	app_status_popup_show ();

	return TRUE;
}

static void
app_status_button_clicked_cb (GtkButton      *button,
			      GdkEventButton *event,
			      gpointer        user_data)
{
	app_status_popup_show ();
}

static gboolean
configure_event_idle_cb (GtkWidget *widget)
{
	GossipAppPriv *priv = app->priv;
	gint           width, height;
	gint           x, y;
	
	gtk_window_get_size (GTK_WINDOW (widget), &width, &height);
	gtk_window_get_position (GTK_WINDOW(app->priv->window), &x, &y);

	gconf_client_set_int (gconf_client,
			      GCONF_PATH "/ui/main_window_width",
			      width,
			      NULL);

	gconf_client_set_int (gconf_client,
			      GCONF_PATH "/ui/main_window_height",
			      height,
			      NULL);
	
	gconf_client_set_int (gconf_client,
 			      GCONF_PATH "/ui/main_window_position_x",
 			      x,
 			      NULL);
 
 	gconf_client_set_int (gconf_client,
 			      GCONF_PATH "/ui/main_window_position_y",
 			      y,
 			      NULL);

	priv->size_timeout_id = 0;
	
	return FALSE;
}

static gboolean
app_window_configure_event_cb (GtkWidget         *widget,
			       GdkEventConfigure *event,
			       gpointer           data)
{
	GossipAppPriv *priv = app->priv;

	if (priv->size_timeout_id) {
		g_source_remove (priv->size_timeout_id);
	}
	
	priv->size_timeout_id = g_timeout_add (1000,
					       (GSourceFunc) configure_event_idle_cb,
					       widget);

	return FALSE;
}

GtkWidget *
gossip_app_get_window (void)
{
	return app->priv->window;
}


static gboolean
app_have_tray (void)
{
	Screen *xscreen = DefaultScreenOfDisplay (gdk_display);
	Atom    selection_atom;
	char   *selection_atom_name;
	
	selection_atom_name = g_strdup_printf ("_NET_SYSTEM_TRAY_S%d",
					       XScreenNumberOfScreen (xscreen));
	selection_atom = XInternAtom (DisplayOfScreen (xscreen), selection_atom_name, False);
	g_free (selection_atom_name);
	
	if (XGetSelectionOwner (DisplayOfScreen (xscreen), selection_atom)) {
		return TRUE;
	} else {
		return FALSE;
	}
}

