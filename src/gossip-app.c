/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2005 Imendio AB
 * Copyright (C) 2004-2005 Martyn Russell <mr@gnome.org>
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
#include <loudmouth/loudmouth.h>
#include <glib/gi18n.h>
#include <libgnomeui/libgnomeui.h>

#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-presence.h>
#include <libgossip/gossip-protocol.h>
#include <libgossip/gossip-utils.h>

#include "eggtrayicon.h"

#include "gossip-about.h"
#include "gossip-account-button.h"
#include "gossip-accounts-dialog.h"
#include "gossip-add-contact-window.h"
#include "gossip-app.h"
#include "gossip-chat.h"
#include "gossip-chatrooms-dialog.h"
#include "gossip-contact-list.h"
#include "gossip-group-chat.h"
#include "gossip-idle.h"
#include "gossip-marshal.h"
#include "gossip-new-account-window.h"
#include "gossip-preferences.h"
#include "gossip-presence-chooser.h"
#include "gossip-private-chat.h"
#include "gossip-sound.h"
#include "gossip-status-presets.h"
#include "gossip-stock.h"
#include "gossip-ui-utils.h"
#include "gossip-vcard-dialog.h"

#ifdef HAVE_DBUS
#include "gossip-dbus.h"
#endif

#ifdef HAVE_GALAGO
#include "gossip-galago.h"
#endif

/* Number of seconds before entering autoaway and extended autoaway. */
#define	AWAY_TIME (5*60) 
#define	EXT_AWAY_TIME (30*60)

/* Number of seconds to flash the icon when explicitly entering away status
 * (activity is also allowed during this period).
 */
#define	LEAVE_SLACK 15

/* Number of seconds of slack when returning from autoaway. */
#define	BACK_SLACK 15

/* Flashing delay for icons (milliseconds). */
#define FLASH_TIMEOUT 500

/* Minimum width of roster window if something goes wrong. */
#define MIN_WIDTH 50

/* Debugging */
#define d(x) 


struct _GossipAppPriv {
	GossipSession         *session;

	GossipChatroomManager *chatroom_manager;

	GossipChatManager     *chat_manager;
        GossipEventManager    *event_manager;
	
	GossipContactList     *contact_list;

	GConfClient           *gconf_client;

	/* main widgets */
	GtkWidget             *window;
	GtkWidget             *main_vbox;

	/* menu widgets */
	GtkWidget             *actions_connect;
	GtkWidget             *actions_disconnect;

	/* accounts toolbar */
	GtkWidget             *accounts_toolbar;
	GHashTable            *accounts;

	/* tray */
	EggTrayIcon           *tray_icon;
	GtkWidget             *tray_event_box;
	GtkWidget             *tray_image;
	GtkTooltips           *tray_tooltips;
	GList                 *tray_flash_icons;
	guint                  tray_flash_timeout_id;

	GtkWidget             *popup_menu;
	GtkWidget             *popup_menu_status_item;
	GtkWidget             *show_popup_item;
	GtkWidget             *hide_popup_item;
	 
	/* widgets that are enabled when we're connected/disconnected */
	GList                 *widgets_connected_all;
	GList                 *widgets_connected;
	GList                 *widgets_disconnected;

	/* status popup */
	GtkWidget             *status_button_hbox;
	GtkWidget             *status_image;
	GtkWidget             *presence_chooser;

	guint                  status_flash_timeout_id;
	time_t                 leave_time;

	/* presence set by the user (available/busy) */
	GossipPresence        *presence;

	/* away presence (away/xa), overrides priv->presence */
	GossipPresence        *away_presence;
	
	/* misc */
	guint                  size_timeout_id;
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


typedef struct {
	GossipProtocol *protocol;
	GossipContact  *contact;
	GossipVCard    *vcard;
} SubscriptionData;


enum {
	CONNECTED,
	DISCONNECTED,
	LAST_SIGNAL
};


static void            gossip_app_class_init                (GossipAppClass       *klass);
static void            gossip_app_init                      (GossipApp            *app);
static void            app_finalize                         (GObject              *object);
static void            app_setup                            (GossipAccountManager *manager);
static gboolean        app_main_window_quit_confirm         (GossipApp            *app,
							     GtkWidget            *window);
static void            app_main_window_quit_confirm_cb      (GtkWidget            *dialog,
							     gint                  response,
							     GossipApp            *app);
static gboolean        app_main_window_key_press_event_cb   (GtkWidget            *window,
					                     GdkEventKey          *event);
static gboolean        app_main_window_delete_event_cb      (GtkWidget            *window,
							     GdkEvent             *event,
							     GossipApp            *app);
static void            app_main_window_destroy_cb           (GtkWidget            *window,
							     GossipApp            *app);
static gboolean        app_idle_check_cb                    (GossipApp            *app);
static void            app_quit_cb                          (GtkWidget            *window,
							     GossipApp            *app);
static void            app_session_protocol_connected_cb    (GossipSession        *session,
							     GossipAccount        *account,
							     GossipProtocol       *protocol,
							     gpointer              unused);
static void            app_session_protocol_disconnected_cb (GossipSession        *session,
							     GossipAccount        *account,
							     GossipProtocol       *protocol,
							     gpointer              unused);
static gchar *         app_session_get_password_cb          (GossipSession        *session,
							     GossipAccount        *account,
							     gpointer              unused);
static void            app_new_message_cb                   (GtkWidget            *widget,
							     gpointer              user_data);
static void            app_popup_new_message_cb             (GtkWidget            *widget,
							     gpointer              user_data);
static void            app_add_contact_cb                   (GtkWidget            *widget,
							     GossipApp            *app);
static void            app_show_offline_cb                  (GtkCheckMenuItem     *item,
							     GossipApp            *app);
static void            app_show_offline_key_changed_cb      (GConfClient          *client,
							     guint                 cnxn_id,
							     GConfEntry           *entry,
							     gpointer              check_menu_item);
static void            app_accounts_cb                      (GtkWidget            *widget,
							     GossipApp            *app);
static void            app_personal_information_cb          (GtkWidget            *widget,
							     GossipApp            *app);
static void            app_preferences_cb                   (GtkWidget            *widget,
							     GossipApp            *app);
static void            app_configure_transports_cb          (GtkWidget            *widget,
							     GossipApp            *app);
static void            app_about_cb                         (GtkWidget            *widget,
							     GossipApp            *app);
static void	       app_help_cb			    (GtkWidget		  *widget,
							     GossipApp		  *app);
static void            app_join_group_chat_cb               (GtkWidget            *widget,
							     GossipApp            *app);
static void            app_show_hide_activate_cb            (GtkWidget            *widget,
							     GossipApp            *app);
static void            app_connect_cb                       (GtkWidget            *widget,
							     GossipApp            *app);
static void            app_disconnect_cb                    (GtkWidget            *widget,
							     GossipApp            *app);
static gboolean        app_tray_destroy_cb                  (GtkWidget            *widget,
							     gpointer              user_data);
static void            app_tray_create_menu                 (void);
static void            app_tray_create                      (void);
static void            app_disconnect                       (void);
static void            app_connection_items_setup           (GladeXML             *glade);
static void            app_connection_items_update          (void);
static void            app_complete_name_response_cb        (GtkWidget            *dialog,
							     gint                  response,
							     CompleteNameData     *data);
static gboolean        app_complete_name_idle               (CompleteNameData     *data);
static void            app_complete_name_insert_text_cb     (GtkEntry             *entry,
							     const gchar          *text,
							     gint                  length,
							     gint                 *position,
							     CompleteNameData     *data);
static gboolean        app_complete_name_key_press_event_cb (GtkEntry             *entry,
							     GdkEventKey          *event,
							     CompleteNameData     *data);
static void            app_complete_name_activate_cb        (GtkEntry             *entry,
							     CompleteNameData     *data);
static void            app_toggle_visibility                (void);
static gboolean        app_tray_pop_event                   (void);
static void            app_tray_update_tooltip              (void);
static void            app_accounts_create                  (void);
static void            app_accounts_add                     (GossipAccount        *account);
static void            app_accounts_remove                  (GossipAccount        *account);
static void            app_accounts_update_toolbar          (void);
static void            app_accounts_account_added_cb        (GossipAccountManager *manager,
							     GossipAccount        *account,
							     gpointer              user_data);
static void            app_accounts_account_removed_cb      (GossipAccountManager *manager,
							     GossipAccount        *account,
							     gpointer              user_data);
static void            app_accounts_account_enabled_cb      (GossipAccountManager *manager,
							     GossipAccount        *account,
							     gpointer              user_data);
static GossipPresence *app_get_effective_presence           (void);
static void            app_set_away                         (const gchar          *status);
static void            app_presence_updated                 (void);
static void            app_status_clear_away                (void);
static void            app_status_flash_start               (void);
static void            app_status_flash_stop                (void);
static GdkPixbuf *     app_get_current_status_pixbuf        (void);
static void            app_presence_chooser_changed_cb      (GtkWidget            *chooser,
							     GossipPresenceState   state,
							     const gchar          *status,
							     gpointer              data);
static gboolean        app_window_configure_event_cb        (GtkWidget            *widget,
							     GdkEventConfigure    *event,
							     gpointer              data);
static gboolean        app_have_tray                        (void);
static void            app_tray_flash_start                 (void);
static void            app_tray_flash_maybe_stop            (void);
static void            app_event_added_cb                   (GossipEventManager   *manager,
							     GossipEvent          *event,
							     gpointer              unused);
static void            app_event_removed_cb                 (GossipEventManager   *manager,
							     GossipEvent          *event,
							     gpointer              unused);
static void            app_contact_activated_cb             (GossipContactList    *contact_list,
							     GossipContact        *contact,
							     gpointer              unused);
static void            app_subscription_request_cb          (GossipProtocol       *protocol,
							     GossipContact        *contact,
							     gpointer              user_data);
static void            app_subscription_event_activated_cb  (GossipEventManager   *event_manager,
							     GossipEvent          *event,
							     GossipProtocol       *protocol);
static void            app_subscription_vcard_cb            (GossipResult          result,
							     GossipVCard          *vcard,
							     SubscriptionData     *data);
static void            app_subscription_request_dialog_cb   (GtkWidget            *dialog,
							     gint                  response,
							     SubscriptionData     *data);


static guint         signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class;
static GossipApp    *app;


G_DEFINE_TYPE (GossipApp, gossip_app, G_TYPE_OBJECT);


static void
gossip_app_class_init (GossipAppClass *klass)
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
gossip_app_init (GossipApp *singleton_app)
{
        GossipAppPriv *priv;

	app = singleton_app;
	
        priv = g_new0 (GossipAppPriv, 1);

 	priv->accounts = g_hash_table_new_full (gossip_account_hash,
						gossip_account_equal,
						g_object_unref,
						(GDestroyNotify)gtk_widget_destroy);

        app->priv = priv;
}

static void
app_finalize (GObject *object)
{
        GossipApp            *app;
        GossipAppPriv        *priv;
	GossipAccountManager *manager;
	
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

	if (priv->chatroom_manager) {
		g_object_unref (priv->chatroom_manager);
	}

	manager = gossip_session_get_account_manager (priv->session);
	
	g_signal_handlers_disconnect_by_func (manager,
					      app_accounts_account_added_cb, 
					      NULL);

	g_signal_handlers_disconnect_by_func (manager,
					      app_accounts_account_removed_cb, 
					      NULL);


	g_list_free (priv->widgets_connected_all);
	g_list_free (priv->widgets_connected);
	g_list_free (priv->widgets_disconnected);
	
	g_free (priv);
	app->priv = NULL;

        if (G_OBJECT_CLASS (parent_class)->finalize) {
                (* G_OBJECT_CLASS (parent_class)->finalize) (object);
        }
}

static void
app_setup (GossipAccountManager *manager) 
{
        GossipAppPriv  *priv;
	GladeXML       *glade;
	GtkWidget      *sw;
	gint            width, height;
	GtkRequisition  req;
	gboolean        show_offline;
	GtkWidget      *show_offline_widget;
	gint            x, y;
	gboolean        hidden;
	GtkWidget      *image;

	priv = app->priv;

	priv->gconf_client = gconf_client_get_default ();

	gconf_client_add_dir (priv->gconf_client,
			      GCONF_PATH,
			      GCONF_CLIENT_PRELOAD_ONELEVEL,
			      NULL);
	
	priv->session = gossip_session_new (manager);

	/* is this the best place for this, 
	   perhaps in gossip-main.c? */
#ifdef HAVE_DBUS
        gossip_dbus_init (priv->session);
#endif

#ifdef HAVE_GALAGO
	gossip_galago_init (priv->session);
#endif

	/* do we need first time start up druid? */
	if (gossip_new_account_window_is_needed ()) {
		gossip_new_account_window_show (NULL);
	}

	priv->chatroom_manager = gossip_chatroom_manager_new (NULL);

	priv->chat_manager = gossip_chat_manager_new ();
 	priv->event_manager = gossip_event_manager_new (); 

	g_signal_connect (manager, "account_added",
			  G_CALLBACK (app_accounts_account_added_cb), 
			  NULL);

	g_signal_connect (manager, "account_removed",
			  G_CALLBACK (app_accounts_account_removed_cb), 
			  NULL);

	g_signal_connect (manager, "account_enabled",
			  G_CALLBACK (app_accounts_account_enabled_cb), 
			  NULL);

	g_signal_connect (priv->session, "protocol-connected",
			  G_CALLBACK (app_session_protocol_connected_cb),
			  NULL);

	g_signal_connect (priv->session, "protocol-disconnected",
			  G_CALLBACK (app_session_protocol_disconnected_cb),
			  NULL);

	g_signal_connect (priv->session, "get-password",
			  G_CALLBACK (app_session_get_password_cb),
			  NULL);

	g_signal_connect (priv->event_manager, "event-added",
			  G_CALLBACK (app_event_added_cb),
			  NULL);
	g_signal_connect (priv->event_manager, "event-removed",
			  G_CALLBACK (app_event_removed_cb),
			  NULL);

	/* set up interface */
	glade = gossip_glade_get_file (GLADEDIR "/main.glade",
				       "main_window",
				       NULL,
				       "main_window", &priv->window,
				       "main_vbox", &priv->main_vbox,
				       "actions_connect", &priv->actions_connect,
				       "actions_disconnect", &priv->actions_disconnect,
				       "accounts_toolbar", &priv->accounts_toolbar,
				       "roster_scrolledwindow", &sw,
				       "status_button_hbox", &priv->status_button_hbox,
				       "status_image", &priv->status_image,
				       "actions_show_offline", &show_offline_widget,
				       NULL);

	gossip_glade_connect (glade,
			      app,
			      "main_window", "destroy", app_main_window_destroy_cb,
			      "main_window", "delete_event", app_main_window_delete_event_cb,
			      "main_window", "configure_event", app_window_configure_event_cb,
			      "main_window", "key_press_event", app_main_window_key_press_event_cb,
			      "file_quit", "activate", app_quit_cb,
			      "actions_connect", "activate", app_connect_cb,
 			      "actions_disconnect", "activate", app_disconnect_cb, 
			      "actions_join_group_chat", "activate", app_join_group_chat_cb,
			      "actions_send_chat_message", "activate", app_new_message_cb,
			      "actions_add_contact", "activate", app_add_contact_cb,
			      "actions_show_offline", "toggled", app_show_offline_cb,
			      "actions_configure_transports", "activate", app_configure_transports_cb,
			      "edit_accounts", "activate", app_accounts_cb,
			      "edit_personal_information", "activate", app_personal_information_cb,
			      "edit_preferences", "activate", app_preferences_cb,
			      "help_about", "activate", app_about_cb,
			      "help_contents", "activate", app_help_cb,
			      NULL);

	/* Set up menu */
	g_object_get (GTK_IMAGE_MENU_ITEM (priv->actions_connect), "image", &image, NULL);
	gtk_image_set_from_stock (GTK_IMAGE (image), GTK_STOCK_CONNECT, GTK_ICON_SIZE_MENU);
	gtk_label_set_text_with_mnemonic (GTK_LABEL (GTK_BIN (priv->actions_connect)->child), 
					  _("_Connect"));

	g_object_get (GTK_IMAGE_MENU_ITEM (priv->actions_disconnect), "image", &image, NULL);
	gtk_image_set_from_stock (GTK_IMAGE (image), GTK_STOCK_DISCONNECT, GTK_ICON_SIZE_MENU);
	gtk_label_set_text_with_mnemonic (GTK_LABEL (GTK_BIN (priv->actions_disconnect)->child), 
					  _("_Disconnect"));

	/* Set up connection related widgets. */
	app_connection_items_setup (glade);
	g_object_unref (glade);

	/* Set up presence chooser */
	priv->presence_chooser = gossip_presence_chooser_new ();
	gtk_box_pack_start (GTK_BOX (priv->status_button_hbox), priv->presence_chooser,
			    TRUE, TRUE, 0);
	g_signal_connect (priv->presence_chooser,
			  "changed",
			  G_CALLBACK (app_presence_chooser_changed_cb),
			  NULL);
	gtk_widget_show (priv->presence_chooser);

	priv->widgets_connected_all = g_list_prepend (priv->widgets_connected_all,
						      priv->presence_chooser);

	/* Set up contact list. */
	priv->contact_list = gossip_contact_list_new ();

	g_signal_connect (priv->contact_list, "contact-activated",
			  G_CALLBACK (app_contact_activated_cb),
			  NULL);

	/* Get saved presence presets. */
	gossip_status_presets_get_all ();

	/* Set up saved presence information. */
	priv->presence = gossip_presence_new ();
	gossip_presence_set_state (priv->presence, 
				   GOSSIP_PRESENCE_STATE_AVAILABLE);
	priv->away_presence = NULL;
	
	/* Finish setting up contact list. */
	gtk_widget_show (GTK_WIDGET (priv->contact_list));
	gtk_container_add (GTK_CONTAINER (sw),
			   GTK_WIDGET (priv->contact_list));

	/* Set up notification area / tray. */
	app_tray_create_menu ();
	app_tray_create ();

	/* Set up accounts area. */
	app_accounts_create ();

	/* Set the idle time checker. */
	g_timeout_add (2 * 1000, (GSourceFunc) app_idle_check_cb, app);

	/* Set window position. */
 	x = gconf_client_get_int (priv->gconf_client, 
				  GCONF_PATH "/ui/main_window_position_x",
				  NULL);

	y = gconf_client_get_int (priv->gconf_client, 
				  GCONF_PATH "/ui/main_window_position_y", 
				  NULL);
 
 	if (x >= 0 && y >= 0) {
 		gtk_window_move (GTK_WINDOW (priv->window), x, y);
	}

	/* Set window size. */
	width = gconf_client_get_int (priv->gconf_client,
				      GCONF_PATH "/ui/main_window_width",
				      NULL);

	height = gconf_client_get_int (priv->gconf_client,
				       GCONF_PATH "/ui/main_window_height",
				       NULL);

	width = MAX (width, MIN_WIDTH);
	gtk_window_set_default_size (GTK_WINDOW (priv->window), width, height);
	
	/* Set up current presence. */
	app_presence_updated ();

	/* Set up 'show_offline' config hooks to know when it changes. */
	show_offline = gconf_client_get_bool (priv->gconf_client,
					      GCONF_PATH "/contacts/show_offline",
					      NULL);

	gconf_client_notify_add (priv->gconf_client,
				 GCONF_PATH "/contacts/show_offline",
				 app_show_offline_key_changed_cb,
				 show_offline_widget,
				 NULL, NULL);

	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (show_offline_widget),
					show_offline);

	/* Set window to be hidden / shown. */
	hidden = gconf_client_get_bool (priv->gconf_client, 
					GCONF_PATH "/ui/main_window_hidden", 
					NULL);

 	/* FIXME: See bug #132632, if (!hidden || !app_have_tray ()) */
 	if (!hidden) {
		gtk_widget_show (priv->window);
	} else {
		gdk_notify_startup_complete ();
	}

	/* This is a hack that sets the minimal size of the window so it doesn't
	 * allow resizing to a smaller width than the menubar takes up. We must
	 * set a minimal size, otherwise the window won't shrink beyond the
	 * longest string in the roster, which will cause the ellipsizing cell
	 * renderer not to work. FIXME: needs to update this on theme/font
	 * change.
	 */
	gtk_widget_size_request (priv->window, &req);
	req.width = MAX (req.width, MIN_WIDTH);
	gtk_widget_set_size_request (priv->window, req.width, -1);

	app_connection_items_update ();
}

static gboolean 
app_main_window_quit_confirm (GossipApp *app,
			      GtkWidget *window)
{
	GossipAppPriv *priv;
	GList         *events;
	
	priv = app->priv;

	events = gossip_event_manager_get_events (priv->event_manager);

	if (g_list_length ((GList*)events) > 0) {
		GList     *l;
		gint       i;
		gint       count[GOSSIP_EVENT_ERROR];

		GtkWidget *dialog;
		gchar     *str;
		gchar     *oldstr = NULL;

		str = g_strconcat (_("To summarize:"), "\n", NULL);

		for (i = 0; i < GOSSIP_EVENT_ERROR; i++) {
			count[i] = 0;
		}

		for (l = events; l; l = l->next) {
			GossipEvent     *event;
			GossipEventType  type; 

			event = l->data;
			type = gossip_event_get_event_type (event);

			count[type]++;
		}
		
		for (i = 0; i < GOSSIP_EVENT_ERROR; i++) {
			gchar *str_single = NULL;
			gchar *str_plural = NULL;
			gchar *info;

			if (count[i] < 1) {
				continue;
			}
			
			if (i == GOSSIP_EVENT_TYPING) {
				continue;
			}

			switch (i) {
			case GOSSIP_EVENT_NEW_MESSAGE: 
				str_single = "%d New message";
				str_plural = "%d New messages";
				break;
			case GOSSIP_EVENT_UNFOCUSED_MESSAGE: 
				str_single = "%d Unfocused message";
				str_plural = "%d Unfocused messages";
				break;
			case GOSSIP_EVENT_SUBSCRIPTION_REQUEST: 
				str_single = "%d Subscription request";
				str_plural = "%d Subscription requests";
				break;
			case GOSSIP_EVENT_SERVER_MESSAGE: 
				str_single = "%d Server message";
				str_plural = "%d Server messages";
				break;
			case GOSSIP_EVENT_ERROR: 
				str_single = "%d Error";
				str_plural = "%d Errors";
				break;
			}

			info = g_strdup_printf (ngettext (str_single,
							  str_plural,
							  count[i]),
						count[i]);

			if (oldstr) {
				g_free (oldstr);
			}

			oldstr = str;

			str = g_strconcat (str, "\n", "\t", info, NULL);
			g_free (info);
		}

		g_list_free (events);

		dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (gossip_app_get_window ()),
							     0,
							     GTK_MESSAGE_WARNING,
							     GTK_BUTTONS_OK_CANCEL,
							     "<b>%s</b>\n\n%s",
							     _("If you quit, you will loose all unread information."), 
							     str);
	
		g_signal_connect (dialog, "response",
				  G_CALLBACK (app_main_window_quit_confirm_cb),
				  app);
		
		gtk_widget_show (dialog);

		return TRUE;
	}

	return FALSE;
}

static void
app_main_window_quit_confirm_cb (GtkWidget *dialog,
				 gint       response,
				 GossipApp *app) 
{
	GossipAppPriv *priv;
	
	priv = app->priv;

	gtk_widget_destroy (dialog);

	if (response == GTK_RESPONSE_OK) {
		gtk_widget_destroy (priv->window);
	}
}

static gboolean
app_main_window_key_press_event_cb (GtkWidget   *window, 
				    GdkEventKey *event) 
{
	if (event->keyval == GDK_Escape) {
		gtk_widget_hide (window);
	}

	return FALSE;
}

static gboolean
app_main_window_delete_event_cb (GtkWidget *window,
				 GdkEvent  *event,
				 GossipApp *app)
{
	return app_main_window_quit_confirm (app, window);
}

static void
app_main_window_destroy_cb (GtkWidget *window,
			    GossipApp *app)
{
	GossipAppPriv *priv;
	
	priv = app->priv;

	if (gossip_session_is_connected (priv->session, NULL)) {
		gossip_session_disconnect (priv->session, NULL);
	}
	
	exit (EXIT_SUCCESS);
}

static void 
app_quit_cb (GtkWidget *window,
	     GossipApp *app)
{
	GossipAppPriv *priv;
	
	priv = app->priv;

	if (!app_main_window_quit_confirm (app, priv->window)) {
		gtk_widget_destroy (priv->window);
	}
}

static void
app_session_protocol_connected_cb (GossipSession  *session,
				   GossipAccount  *account,
				   GossipProtocol *protocol,
				   gpointer        unused)
{
	GossipAppPriv  *priv;

	priv = app->priv;

	app_connection_items_update ();
	app_presence_updated ();

	/* is this the right place for setting up protocol signals? */
	g_signal_connect (protocol,
                          "subscription-request",
                          G_CALLBACK (app_subscription_request_cb),
                          session);
}

static void
app_session_protocol_disconnected_cb (GossipSession  *session,
				      GossipAccount  *account,
				      GossipProtocol *protocol,
				      gpointer        unused)
{
	GossipAppPriv *priv;
	
	priv = app->priv;
	
 	app_connection_items_update ();
	app_presence_updated ();

	/* FIXME: implement */
	g_signal_handlers_disconnect_by_func (protocol, 
					      app_subscription_request_cb, 
					      session);
}

static gchar * 
app_session_get_password_cb (GossipSession *session,
			     GossipAccount *account,
			     gpointer       unused)
{
	GossipAppPriv *priv;
	gchar         *password;

	priv = app->priv;

	password = gossip_password_dialog_run (account,
					       GTK_WINDOW (priv->window));

	return password;
}

static void
app_about_cb (GtkWidget *window,
	      GossipApp *app)
{
	static GtkWidget *about;

	if (about) {
		gtk_window_present (GTK_WINDOW (about));
		return;
	}
	
	about = gossip_about_new ();
	g_object_add_weak_pointer (G_OBJECT (about), (gpointer) &about);
	gtk_widget_show (about);
}

static void
app_help_cb (GtkWidget *window,
	     GossipApp *app)
{
	gboolean   ok;

	GtkWidget *dialog;
	GError    *err = NULL;

	ok = gnome_help_display ("gossip.xml", NULL, &err);
	if (ok) {
		return;
	}

	dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (gossip_app_get_window ()),
						     GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_MESSAGE_ERROR,
						     GTK_BUTTONS_CLOSE,
						     "<b>%s</b>\n\n%s",
						     _("Could not display the help contents."),
						     err->message);
	
	g_signal_connect_swapped (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  dialog);

	gtk_widget_show (dialog);
		
	g_error_free (err);
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

	/* currently we pass TRUE even though this is not a startup
	   call to connect, but if we pass FALSE then *ALL* accounts
	   will be connected. */
	gossip_app_connect (FALSE);
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
	gossip_chatrooms_dialog_show ();
}

static void
app_new_message_presence_data_func (GtkCellLayout   *cell_layout,
				    GtkCellRenderer *cell,
				    GtkTreeModel    *tree_model,
				    GtkTreeIter     *iter,
				    gpointer         unused)
{
	GossipContact  *contact;
	GdkPixbuf      *pixbuf;

	gtk_tree_model_get (tree_model, iter, 0, &contact, -1);

	pixbuf = gossip_ui_utils_get_pixbuf_for_contact (contact);

	g_object_set (cell, "pixbuf", pixbuf, NULL);
	g_object_unref (pixbuf);
	g_object_unref (contact);
}

static void
app_new_message (gboolean use_roster_selection,
		 gboolean be_transient)
{
	GossipAppPriv    *priv;
	const gchar      *selected_name = NULL;
	const GList      *l;
	GList            *contacts = NULL;
	GossipContact    *contact = NULL;
	GtkWidget        *frame;
	CompleteNameData *data;
	const GList      *all_contacts;
	GtkWindow        *parent;
	GtkListStore     *model;
	GtkCellRenderer  *cell;
	
	priv = app->priv;

	data = g_new0 (CompleteNameData, 1);

	if (be_transient) {
		parent = GTK_WINDOW (priv->window);
	} else {
		parent = NULL;
	}
	
	data->dialog = gtk_message_dialog_new (parent,
					       0,
					       GTK_MESSAGE_QUESTION,
					       GTK_BUTTONS_NONE,
					       _("Enter the user ID of the person you would "
						 "like to send a chat message to."));
	gtk_label_set_selectable (GTK_LABEL (GTK_MESSAGE_DIALOG (data->dialog)->label), 
				  FALSE);
	
	gtk_dialog_add_buttons (GTK_DIALOG (data->dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				_("_Chat"), GTK_RESPONSE_OK,
				NULL);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (data->dialog)->vbox),
			    frame, TRUE, TRUE, 8);
	gtk_widget_show (frame);

	model = gtk_list_store_new (2, G_TYPE_POINTER, G_TYPE_STRING);
	data->combo = gtk_combo_box_entry_new_with_model (GTK_TREE_MODEL (model), 1);

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (data->combo), cell, FALSE);

	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (data->combo),
					    cell, 
					    app_new_message_presence_data_func,
					    NULL, NULL);
					    
	gtk_cell_layout_reorder (GTK_CELL_LAYOUT (data->combo), cell, 0);
	
	gtk_container_add (GTK_CONTAINER (frame), data->combo);
	gtk_widget_show (data->combo);

	data->entry = GTK_BIN (data->combo)->child;

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
	
	data->completion = g_completion_new ((GCompletionFunc) gossip_contact_get_name);
	g_completion_set_compare (data->completion, 
				  gossip_utils_str_n_case_cmp);

	if (use_roster_selection) {
		GossipContact *contact;

		contact = gossip_contact_list_get_selected (priv->contact_list);
	} 

	all_contacts = gossip_session_get_contacts (priv->session);
	for (l = all_contacts; l; l = l->next) {
		GossipContact *c;

		c = GOSSIP_CONTACT (l->data);
		
		if (contact && gossip_contact_equal (contact, c)) {
			/* Got the selected one, select it in the combo. */
			selected_name = gossip_contact_get_name (c);
		}

		contacts = g_list_prepend (contacts, g_object_ref (c));
	}

	contacts = g_list_sort (contacts, gossip_contact_name_case_compare);
	for (l = contacts; l; l = l->next) {
		GtkTreeIter     iter;
		GossipContact  *c;
		
		c = GOSSIP_CONTACT (l->data);

		gtk_list_store_append (model, &iter);

		gtk_list_store_set (model, &iter,
				    0, c,
				    1, gossip_contact_get_name (c),
				    -1);
	}
#if 0	
	data->names = g_list_sort (data->names, 
				   (GCompareFunc) gossip_utils_str_case_cmp);
	if (data->names) {
		gtk_combo_set_popdown_strings (GTK_COMBO (data->combo),
					       data->names);
	}
#endif
	/* contacts = g_list_sort (contacts,
	   gossip_contact_name_case_compare);*/
	if (contacts) {
		g_completion_add_items (data->completion, contacts);
	}

#if 0
	if (selected_name) {
		gtk_entry_set_text (GTK_ENTRY (data->entry), selected_name);
		gtk_editable_select_region (GTK_EDITABLE (data->entry), 0, -1);
	} else {
		gtk_entry_set_text (GTK_ENTRY (data->entry), "");
	}

	gtk_widget_grab_focus (data->entry);
#endif	
	gtk_widget_show (data->dialog);
}

static void
app_new_message_cb (GtkWidget *widget,
		    gpointer   user_data)
{
	app_new_message (TRUE, TRUE);
}

static void
app_popup_new_message_cb (GtkWidget *widget,
			  gpointer   user_data)
{
	app_new_message (FALSE, FALSE);
}

static void
app_add_contact_cb (GtkWidget *widget, 
		    GossipApp *app)
{
	GossipAppPriv *priv;

	priv = app->priv;

	gossip_add_contact_window_show (GTK_WINDOW (priv->window), NULL);
}

static void
app_show_offline_cb (GtkCheckMenuItem *item, 
		     GossipApp        *app)
{
	GossipAppPriv *priv;
	gboolean       current;
	
	priv = app->priv;

	current = gtk_check_menu_item_get_active (item);

	gconf_client_set_bool (priv->gconf_client,
			       GCONF_PATH "/contacts/show_offline",
			       current,
			       NULL);

	/* turn off while we alter the contact list */
	gossip_sound_toggle (FALSE);

	g_object_set (priv->contact_list, "show_offline", current, NULL);

	/* turn on back to normal */
	gossip_sound_toggle (TRUE);
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
app_personal_information_cb (GtkWidget *widget, GossipApp *app)
{
	GossipAppPriv *priv;

	priv = app->priv;
	
	gossip_vcard_dialog_show (GTK_WINDOW (priv->window));
}

static void
app_accounts_cb (GtkWidget *widget, GossipApp *app)
{
	gossip_accounts_dialog_show (NULL);
}

static void
app_preferences_cb (GtkWidget *widget, GossipApp *app)
{
	gossip_preferences_show ();
}

static void
app_configure_transports_cb (GtkWidget *widget, GossipApp *app)
{
#if 0 /* TRANSPORTS */
	gossip_transport_accounts_window_show ();
#endif
}

static gboolean
app_idle_check_cb (GossipApp *app)
{
	GossipAppPriv       *priv;
	gint32               idle;
	GossipPresence      *presence;
	GossipPresenceState  state;
	gboolean             presence_changed = FALSE;

	priv = app->priv;

	if (!gossip_app_is_connected ()) {
		return TRUE;
	}
	
	idle = gossip_idle_get_seconds ();
	presence = app_get_effective_presence ();
	state = gossip_presence_get_state (presence);

	/* d(g_print ("Idle for:%d\n", idle)); */

	/* We're going away, allow some slack. */
	if (priv->leave_time > 0) {
		if (time (NULL) - priv->leave_time > LEAVE_SLACK) {
			priv->leave_time = 0;
			app_status_flash_stop ();

			gossip_idle_reset ();
			d(g_print ("OK, away now.\n"));
		}
		
		return TRUE;
	}
	else if (state != GOSSIP_PRESENCE_STATE_EXT_AWAY && 
		 idle > EXT_AWAY_TIME) {
 		/* Presence may be idle if the screensaver has been started and
		 * hence no away_presence set.
		 */
		if (!priv->away_presence) {
			priv->away_presence = gossip_presence_new ();
		}

		/* Presence will already be away. */
		d(g_print ("Going to ext away...\n"));
		gossip_presence_set_state (priv->away_presence, 
					   GOSSIP_PRESENCE_STATE_EXT_AWAY);
		presence_changed = TRUE;
	}
	else if (state != GOSSIP_PRESENCE_STATE_AWAY && 
		 state != GOSSIP_PRESENCE_STATE_EXT_AWAY &&
		 idle > AWAY_TIME) {
		d(g_print ("Going to away...\n"));
		app_set_away (NULL);
		presence_changed = TRUE;
	}
	else if (state == GOSSIP_PRESENCE_STATE_AWAY ||
		 state == GOSSIP_PRESENCE_STATE_EXT_AWAY) {
		/* Allow some slack before returning from away. */
		if (idle >= -BACK_SLACK && idle <= 0) {
			d(g_print ("Slack, do nothing.\n"));
			app_status_flash_start ();
		}
		else if (idle < -BACK_SLACK) {
			d(g_print ("No more slack, break interrupted.\n"));
			app_status_clear_away ();
			return TRUE;
		}
		else if (idle > BACK_SLACK) {
			d(g_print ("Don't interrupt break.\n"));
			app_status_flash_stop ();
		}
	}

	if (presence_changed) {
		app_presence_updated ();
	}

	return TRUE;
}

void
gossip_app_connect (gboolean startup)
{
	GossipAppPriv        *priv;
	GossipAccountManager *manager;

	priv = app->priv;

	manager = gossip_session_get_account_manager (priv->session);
	if (gossip_account_manager_get_count (manager) < 1) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new_with_markup (
			GTK_WINDOW (gossip_app_get_window ()),
			GTK_DIALOG_MODAL |
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_CLOSE,
			"<b>%s</b>\n\n%s",
			_("You have no Instant Messaging accounts "
			  "configured!"),
			_("Next you will be presented with the "
			  "Account Information dialog to set your "
			  "details up.")); 
		
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		gossip_accounts_dialog_show (NULL);
		return;
	}
	
	gossip_session_connect (priv->session, NULL, startup);
}

/* Test hack to add support for disconnecting all accounts and then connect 
 * them again due to if the net goes up and down.
 */
static GSList *tmp_account_list = NULL;
void 
gossip_app_net_down (void)
{
	GossipAppPriv *priv;
	GList         *accounts, *l;

	priv = app->priv;

	/* Disconnect all and store a list */
	accounts = gossip_session_get_accounts (priv->session);
	for (l = accounts; l; l = l->next) {
		GossipAccount *account = GOSSIP_ACCOUNT (l->data);

		if (!gossip_session_is_connected (priv->session, account)) {
			continue;
		}

		tmp_account_list = g_slist_prepend (tmp_account_list,
						    g_object_ref (account));

		gossip_session_disconnect (priv->session, account);
	}

	g_list_free (accounts);
}

void
gossip_app_net_up (void)
{
	GossipAppPriv *priv;
	GSList        *l;

	priv = app->priv;

	/* Connect all that went down before */
	if (!tmp_account_list) {
		/* Nothing to connect */
		return;
	}

	for (l = tmp_account_list; l; l = l->next) {
		GossipAccount *account = GOSSIP_ACCOUNT (l->data);
		
		gossip_session_connect (priv->session, account, FALSE);
		g_object_unref (account);
	}

	g_slist_free (tmp_account_list);
	tmp_account_list = NULL;
}

void
gossip_app_create (GossipAccountManager *manager)
{
	g_return_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager));
	
	g_object_new (GOSSIP_TYPE_APP, NULL);
	
	app_setup (manager);
}

GossipApp *
gossip_app_get (void)
{
	g_assert (app != NULL);
	
	return app;
}

static void
app_disconnect (void)
{
	GossipAppPriv *priv = app->priv;

	gossip_session_disconnect (priv->session, NULL);
	app_status_flash_stop ();
}

static void
app_connection_items_setup (GladeXML *glade)
{
	GossipAppPriv *priv;

	const gchar   *widgets_connected_all[] = {
		"actions_join_group_chat",
		"actions_send_chat_message",
		"actions_add_contact",
		"edit_personal_information"
	};

	const gchar   *widgets_connected[] = {
		"actions_disconnect"
	};

	const gchar   *widgets_disconnected[] = {
		"actions_connect"
	};

	GList         *list;
	GtkWidget     *w;
	gint           i;

	priv = app->priv;

	for (i = 0, list = NULL; i < G_N_ELEMENTS (widgets_connected_all); i++) {
		w = glade_xml_get_widget (glade, widgets_connected_all[i]);
		list = g_list_prepend (list, w);
	}

	priv->widgets_connected_all = list;

	for (i = 0, list = NULL; i < G_N_ELEMENTS (widgets_connected); i++) {
		w = glade_xml_get_widget (glade, widgets_connected[i]);
		list = g_list_prepend (list, w);
	}

	priv->widgets_connected = list;

	for (i = 0, list = NULL; i < G_N_ELEMENTS (widgets_disconnected); i++) {
		w = glade_xml_get_widget (glade, widgets_disconnected[i]);
		list = g_list_prepend (list, w);
	}

	priv->widgets_disconnected = list;
}

static void
app_connection_items_update (void)
{
	GossipAppPriv *priv;
	GList         *l;
	guint          connected_all;
	guint          connected;
	guint          disconnected;

	priv = app->priv;

	/* get account count for: 
	   - connected and disabled, 
	   - connected and enabled 
	   - disabled and enabled 
	*/
	connected_all = gossip_session_count_all_connected (priv->session);
	connected = gossip_session_count_connected (priv->session);
	disconnected = gossip_session_count_disconnected (priv->session);

	for (l = priv->widgets_connected_all; l; l = l->next) {
		gtk_widget_set_sensitive (l->data, (connected_all > 0));
	}

	for (l = priv->widgets_connected; l; l = l->next) {
		gtk_widget_set_sensitive (l->data, (connected > 0));
	}

	for (l = priv->widgets_disconnected; l; l = l->next) {
		gtk_widget_set_sensitive (l->data, (disconnected > 0));
	}
}

static void
app_complete_name_response_cb (GtkWidget        *dialog,
			       gint              response,
			       CompleteNameData *data)
{
	GossipAppPriv *priv;
	const gchar   *str;
	GList         *l;

	priv = app->priv;
	
	if (response == GTK_RESPONSE_OK) {
		GossipContact *contact;
		
		str = gtk_entry_get_text (GTK_ENTRY (data->entry));

		if (str && strcmp (str, "") != 0) {
			contact = gossip_session_find_contact (priv->session,
							       str);

			if (!contact) {
				/* FIXME: need to ask which account we should be
				 * sending from if the contact is not known.
				 */
				contact = gossip_contact_new (
					GOSSIP_CONTACT_TYPE_TEMPORARY, NULL);
				gossip_contact_set_id (contact, str);
			}
			
			gossip_chat_manager_show_chat (priv->chat_manager, 
						       contact);
		}
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

gboolean
gossip_app_is_connected (void)
{
	GossipAppPriv *priv;

	priv = app->priv;

	return gossip_session_is_connected (priv->session, NULL);
}

static void
app_toggle_visibility (void)
{
	GossipAppPriv *priv;
	gboolean       visible;

	priv = app->priv;

	visible = gossip_ui_utils_window_get_is_visible (
							 GTK_WINDOW (priv->window));

	if (visible) {
		gtk_widget_hide (priv->window);
		
		gconf_client_set_bool (priv->gconf_client, 
				       GCONF_PATH "/ui/main_window_hidden", TRUE,
				       NULL);
	} else {
		gint x, y;

		x = gconf_client_get_int (priv->gconf_client, 
					  GCONF_PATH "/ui/main_window_position_x",
					  NULL);
		y = gconf_client_get_int (priv->gconf_client, 
					  GCONF_PATH "/ui/main_window_position_y",
					  NULL);
		
		if (x >= 0 && y >= 0) {
			gtk_window_move (GTK_WINDOW (priv->window), x, y);
		}

		gossip_ui_utils_window_present (GTK_WINDOW (priv->window));

		gconf_client_set_bool (priv->gconf_client, 
				       GCONF_PATH "/ui/main_window_hidden", FALSE,
				       NULL);
	}
}

static gboolean
app_tray_destroy_cb (GtkWidget *widget,
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
	
	/* show the window in case the notification area was removed */
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
	GtkWidget     *submenu;

	priv = app->priv;

	if (event->type == GDK_2BUTTON_PRESS ||
	    event->type == GDK_3BUTTON_PRESS) {
		return FALSE;
	}
	
	if (event->button != 1 && 
	    event->button != 3) {
		return FALSE;
	}

	if (event->button == 1) {
		if (!app_tray_pop_event ()) {
			app_toggle_visibility ();
		}
	} else if (event->button == 3) {
		if (gossip_ui_utils_window_get_is_visible (GTK_WINDOW (priv->window))) {
			gtk_widget_show (priv->hide_popup_item);
			gtk_widget_hide (priv->show_popup_item);
		} else {
			gtk_widget_hide (priv->hide_popup_item);
			gtk_widget_show (priv->show_popup_item);
		}

		submenu = gossip_presence_chooser_create_menu (
			GOSSIP_PRESENCE_CHOOSER (priv->presence_chooser));
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (priv->popup_menu_status_item),
					   submenu);
		
                gtk_menu_popup (GTK_MENU (priv->popup_menu), NULL, NULL, NULL,
				NULL, event->button, event->time);
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

	priv->widgets_connected_all = g_list_prepend (priv->widgets_connected_all,
						      priv->popup_menu_status_item);
	
	priv->widgets_connected_all = g_list_prepend (priv->widgets_connected_all,
						      message_item);
	
	g_object_unref (glade);
}

static void
app_tray_create (void)
{
	GossipAppPriv *priv;
	GdkPixbuf     *pixbuf;

	priv = app->priv;

	priv->tray_icon = egg_tray_icon_new (_("Gossip, Instant Messaging Client"));
		
	priv->tray_event_box = gtk_event_box_new ();
	priv->tray_image = gtk_image_new ();
	
	pixbuf = app_get_current_status_pixbuf ();
	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->tray_image), pixbuf);
	g_object_unref (pixbuf);
	
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

	/* Handles when the area is removed from the panel. */
	g_signal_connect (priv->tray_icon,
			  "destroy",
			  G_CALLBACK (app_tray_destroy_cb),
			  priv->tray_event_box);
}

static gboolean
app_tray_pop_event (void)
{
	GossipAppPriv *priv;
	GossipEvent   *event;

	priv = app->priv;

	if (!priv->tray_flash_icons) {
		return FALSE;
	}

	event = priv->tray_flash_icons->data;
	gossip_event_manager_activate (priv->event_manager, event);

	return TRUE;
}

static void
app_tray_update_tooltip (void)
{
	GossipAppPriv *priv;
	GossipEvent   *event;

	priv = app->priv;

	if (!priv->tray_flash_icons) {
		gtk_tooltips_set_tip (GTK_TOOLTIPS (priv->tray_tooltips),
				      priv->tray_event_box,
				      NULL, NULL);
		return;
	}

	event = priv->tray_flash_icons->data;

	gtk_tooltips_set_tip (GTK_TOOLTIPS (priv->tray_tooltips),
			      priv->tray_event_box,
			      gossip_event_get_message (event),
			      gossip_event_get_message (event));
}	

/*
 * toolbar for accounts
 */

static void
app_accounts_create (void)
{
	GossipAppPriv        *priv;
	GossipAccountManager *manager;
	GList                *accounts;
	GList                *l;

	priv = app->priv;

	manager = gossip_session_get_account_manager (priv->session);
	accounts = gossip_account_manager_get_accounts (manager);

	for (l = accounts; l; l = l->next) {
		GossipAccount *account = l->data;

		app_accounts_add (account);
	}

	g_list_free (accounts);

	/* show/hide toolbar */
	app_accounts_update_toolbar ();
}

static void 
app_accounts_update_toolbar (void)
{
	GossipAppPriv        *priv;
	GossipAccountManager *manager;
	gint                  count;

	priv = app->priv;

	manager = gossip_session_get_account_manager (priv->session);
	count = gossip_account_manager_get_count (manager);

	/* show accounts if we have more than one */
	if (count < 2) {
		gtk_widget_hide (priv->accounts_toolbar);
	} else {
		gtk_widget_show (priv->accounts_toolbar);
	}
}

static void
app_accounts_add (GossipAccount *account)
{
	GossipAppPriv *priv;
	GtkWidget     *account_button;

	GtkWidget     *toolitem;

	priv = app->priv;

	account_button = gossip_account_button_new ();
	gossip_account_button_set_account (GOSSIP_ACCOUNT_BUTTON (account_button), 
					   account);

	g_hash_table_insert (priv->accounts, 
			     g_object_ref (account), 
			     account_button);
	
	/* add to toolbar */
	toolitem = (GtkWidget*) gtk_tool_item_new ();
	gtk_container_add (GTK_CONTAINER (priv->accounts_toolbar), toolitem);

	gtk_container_add (GTK_CONTAINER (toolitem), account_button);
	gtk_widget_show_all (toolitem);

	/* show/hide toolbar */
	app_accounts_update_toolbar ();
}

static void
app_accounts_remove (GossipAccount *account)
{
	GossipAppPriv *priv;

	priv = app->priv;

	g_hash_table_remove (priv->accounts, account);

	/* show/hide toolbar */
	app_accounts_update_toolbar ();
}

static void
app_accounts_account_added_cb (GossipAccountManager *manager,
			       GossipAccount        *account,
			       gpointer              user_data)
{
 	app_accounts_add (account); 
}

static void
app_accounts_account_removed_cb (GossipAccountManager *manager,
				 GossipAccount        *account,
				 gpointer              user_data)
{
	app_accounts_remove (account);
}

static void
app_accounts_account_enabled_cb (GossipAccountManager *manager,
				 GossipAccount        *account,
				 gpointer              user_data)
{
	app_connection_items_update ();
}

static GossipPresence *
app_get_effective_presence (void)
{
	GossipAppPriv *priv;

	priv = app->priv;

	if (priv->away_presence) {
		return priv->away_presence;
	}

	return priv->presence;
}

static void
app_set_away (const gchar *status)
{
	GossipAppPriv *priv;

	priv = app->priv;

	if (!priv->away_presence) {
		priv->away_presence = gossip_presence_new ();
		gossip_presence_set_state (priv->away_presence, 
					   GOSSIP_PRESENCE_STATE_AWAY);
	}
	
	priv->leave_time = time (NULL);
	gossip_idle_reset ();

	if (status) {
		gossip_presence_set_status (priv->away_presence, status);
	}
}

static GdkPixbuf *
app_get_current_status_pixbuf (void)
{
	GossipAppPriv  *priv;
	GossipPresence *presence;

	priv = app->priv;

	if (!gossip_session_is_connected (priv->session, NULL)) {
		return gossip_ui_utils_get_pixbuf_from_stock (GOSSIP_STOCK_OFFLINE);
	}
	
	presence = app_get_effective_presence ();
	return gossip_ui_utils_get_pixbuf_for_presence (presence);
}

static void
app_presence_updated (void)
{
	GossipAppPriv  *priv;
	GdkPixbuf      *pixbuf;
	GossipPresence *presence;
	const gchar    *status;
	
	priv = app->priv;

	pixbuf = app_get_current_status_pixbuf ();

	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->tray_image), pixbuf);
	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->status_image), pixbuf);
	
	g_object_unref (pixbuf);
	
	if (!gossip_session_is_connected (priv->session, NULL)) {
		gossip_presence_chooser_set_status (
			GOSSIP_PRESENCE_CHOOSER (priv->presence_chooser),
			_("Offline"));
		return;
	}

	presence = app_get_effective_presence ();

	status = gossip_presence_get_status (presence);
	if (!status) {
		status = gossip_presence_state_get_default_status (
			gossip_presence_get_state (presence));
	}

	gossip_presence_chooser_set_status (
		GOSSIP_PRESENCE_CHOOSER (priv->presence_chooser),
		status);
		
	gossip_session_set_presence (priv->session, presence);
}

/* clears status data from autoaway mode */ 
static void
app_status_clear_away (void)
{
	GossipAppPriv *priv = app->priv;
	
	if (priv->away_presence) {
		g_object_unref (priv->away_presence);
		priv->away_presence = NULL;
	}

	priv->leave_time = 0;
	app_status_flash_stop ();
	
	/* force this so we don't get a delay in the display */
	app_presence_updated ();
}

static gboolean
app_status_flash_timeout_func (gpointer data)
{
	GossipAppPriv   *priv = app->priv;
	static gboolean  on = FALSE;
	GdkPixbuf       *pixbuf;

	if (on) {
		pixbuf = gossip_ui_utils_get_pixbuf_for_presence (priv->presence);
	} else {
		pixbuf = app_get_current_status_pixbuf ();
	}
	
	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->status_image), pixbuf);
	g_object_unref (pixbuf);
	
	on = !on;

	return TRUE;
}

static void
app_status_flash_start (void)
{
	GossipAppPriv *priv = app->priv;

	if (!priv->status_flash_timeout_id) {
		priv->status_flash_timeout_id = g_timeout_add (
			FLASH_TIMEOUT,
			app_status_flash_timeout_func,
			NULL);
	}

	app_tray_flash_start ();
}

static void
app_status_flash_stop (void)
{
	GossipAppPriv  *priv = app->priv;
	GdkPixbuf      *pixbuf;
	
	pixbuf = app_get_current_status_pixbuf ();
	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->status_image), pixbuf);
	g_object_unref (pixbuf);

	if (priv->status_flash_timeout_id) {
		g_source_remove (priv->status_flash_timeout_id);
		priv->status_flash_timeout_id = 0;
	}
	
	app_tray_flash_maybe_stop ();
}

void
gossip_app_force_non_away (void)
{
	GossipAppPriv *priv = app->priv;

	/* If we just left, allow some slack. */
	if (priv->leave_time) {
		return;
	}
	
	if (priv->away_presence) {
		app_status_clear_away ();
	}
}

static void
app_presence_chooser_changed_cb (GtkWidget           *chooser,
				 GossipPresenceState  state,
				 const gchar         *status,
				 gpointer             data)
{
	GossipAppPriv *priv;

	priv = app->priv;

	if (state != GOSSIP_PRESENCE_STATE_AWAY) {
		const gchar *default_status;

		/* Send NULL if it's not changed from default status string. We
		 * do this so that the translated default strings will work
		 * across two Gossips.
		 */
		default_status = gossip_presence_state_get_default_status (state);
		if (strcmp (status, default_status) == 0) {
			status = NULL;
		}
		
		g_object_set (priv->presence,
			      "state", state,
			      "status", status,
			      NULL);

		app_status_flash_stop ();
		app_status_clear_away ();
	} else {
		app_status_flash_start ();
		app_set_away (status);
	}

	app_presence_updated ();
}

static gboolean
configure_event_idle_cb (GtkWidget *widget)
{
	GossipAppPriv *priv = app->priv;
	gint           width, height;
	gint           x, y;
	
	gtk_window_get_size (GTK_WINDOW (widget), &width, &height);
	gtk_window_get_position (GTK_WINDOW(app->priv->window), &x, &y);

	gconf_client_set_int (priv->gconf_client,
			      GCONF_PATH "/ui/main_window_width",
			      width,
			      NULL);

	gconf_client_set_int (priv->gconf_client,
			      GCONF_PATH "/ui/main_window_height",
			      height,
			      NULL);
	
	gconf_client_set_int (priv->gconf_client,
 			      GCONF_PATH "/ui/main_window_position_x",
 			      x,
 			      NULL);
 
 	gconf_client_set_int (priv->gconf_client,
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

GConfClient *
gossip_app_get_gconf_client (void)
{
	return app->priv->gconf_client;
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

static gboolean
app_tray_flash_timeout_func (gpointer data)
{
	GossipAppPriv   *priv;
	static gboolean  on = FALSE;
	GdkPixbuf       *pixbuf = NULL;

	priv = app->priv;

#if 0
	/* Debug code */
	if (priv->tray_flash_icons == NULL && priv->status_flash_timeout_id == 0) {
		g_print ("no flash\n");
	}

	if (priv->status_flash_timeout_id != 0 && priv->explicit_show == priv->auto_show) {
		g_print ("expl == auto, flashing\n");
	}
#endif
	
	if (priv->status_flash_timeout_id != 0) {
		if (on) {
			pixbuf = gossip_ui_utils_get_pixbuf_for_presence (priv->presence);
		}
	}
	else if (priv->tray_flash_icons != NULL) {
		if (on) {
			pixbuf = gossip_ui_utils_get_pixbuf_from_stock (GOSSIP_STOCK_MESSAGE);
		}
	}

	if (pixbuf == NULL) {
		pixbuf = app_get_current_status_pixbuf ();
	}

	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->tray_image), pixbuf);
	g_object_unref (pixbuf);
	
	on = !on;

	return TRUE;
}

static void
app_tray_flash_start (void)
{
	GossipAppPriv *priv = app->priv;

	if (!priv->tray_flash_timeout_id) {
		priv->tray_flash_timeout_id = g_timeout_add (FLASH_TIMEOUT,
							     app_tray_flash_timeout_func,
							     NULL);
	}
}

/* stop if there are no flashing messages or status change. */
static void
app_tray_flash_maybe_stop (void)
{
	GossipAppPriv *priv = app->priv;
	GdkPixbuf     *pixbuf;

	if (priv->tray_flash_icons != NULL || priv->leave_time > 0) {
		return;
	}

	pixbuf = app_get_current_status_pixbuf ();
	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->tray_image), pixbuf);
	g_object_unref (pixbuf);
	
	if (priv->tray_flash_timeout_id) {
		g_source_remove (priv->tray_flash_timeout_id);
		priv->tray_flash_timeout_id = 0;
	}
}

static void
app_event_added_cb (GossipEventManager *manager,
		    GossipEvent        *event,
		    gpointer            unused)
{
	GossipAppPriv *priv;
	GList         *l;
		
	d(g_print ("Tray start blink\n"));

	priv = app->priv;
	
	l = g_list_find_custom (priv->tray_flash_icons, 
				event, gossip_event_compare);
	if (l) {
		/* Already in list */
		return;
	}

	priv->tray_flash_icons = g_list_append (priv->tray_flash_icons, 
						g_object_ref (event));

	app_tray_flash_start ();
	app_tray_update_tooltip ();
}

static void
app_event_removed_cb (GossipEventManager *manager,
		      GossipEvent        *event,
		      gpointer            unused)
{
	GossipAppPriv *priv;
	GList         *l;

	priv = app->priv;

	d(g_print ("Tray stop blink\n"));
	l = g_list_find_custom (priv->tray_flash_icons, event,
				gossip_event_compare);

	if (!l) {
		/* Not flashing this event */
		g_print ("Couldn't find event\n");
		return;
	}

	priv->tray_flash_icons = g_list_delete_link (priv->tray_flash_icons, l);

	app_tray_flash_maybe_stop ();
	app_tray_update_tooltip ();

	g_object_unref (event);
}

static void
app_contact_activated_cb (GossipContactList *contact_list,
			  GossipContact     *contact,
			  gpointer           unused)
{
	GossipAppPriv *priv;

	priv = app->priv;

	gossip_chat_manager_show_chat (priv->chat_manager, contact);
}

static void
app_subscription_request_cb (GossipProtocol *protocol,
			     GossipContact  *contact,
			     gpointer        user_data)
{
	GossipEvent      *event;
	gchar            *str;

	event = gossip_event_new (GOSSIP_EVENT_SUBSCRIPTION_REQUEST);

	str = g_strdup_printf (_("New subscription request from %s"), 
			       gossip_contact_get_name (contact));

	g_object_set (event, 
		      "message", str, 
		      "data", contact,
		      NULL);
	g_free (str);

	gossip_event_manager_add (gossip_app_get_event_manager (),
				  event, 
				  (GossipEventActivatedFunction)app_subscription_event_activated_cb,
				  G_OBJECT (protocol));
}

static void
app_subscription_event_activated_cb (GossipEventManager *event_manager,
				     GossipEvent        *event,
				     GossipProtocol     *protocol)
{
	GossipContact    *contact;
	SubscriptionData *data;

	contact = GOSSIP_CONTACT (gossip_event_get_data (event));

	data = g_new0 (SubscriptionData, 1);

	data->protocol = g_object_ref (protocol);
	data->contact = g_object_ref (contact);

	gossip_session_get_vcard (gossip_app_get_session (),
				  NULL,
				  data->contact,
				  (GossipVCardCallback) app_subscription_vcard_cb,
				  data, 
				  NULL);
}

static void
app_subscription_vcard_cb (GossipResult      result,
			   GossipVCard       *vcard,
			   SubscriptionData  *data)
{
	GtkWidget   *dialog;
	GtkWidget   *who_label;
	GtkWidget   *question_label;
	GtkWidget   *jid_label;
 	GtkWidget   *website_label;
 	GtkWidget   *personal_table;
	const gchar *name = NULL;
	const gchar *url = NULL;
	gchar       *who;
	gchar       *question;
	gchar       *str;
	gint         num_matches = 0;

	if (GOSSIP_IS_VCARD (vcard)) {
		data->vcard = g_object_ref (vcard);

		name = gossip_vcard_get_name (vcard);
		url = gossip_vcard_get_url (vcard);
	}

	gossip_glade_get_file_simple (GLADEDIR "/main.glade",
				      "subscription_request_dialog",
				      NULL,
				      "subscription_request_dialog", &dialog,
				      "who_label", &who_label,
				      "question_label", &question_label,
				      "jid_label", &jid_label,
				      "website_label", &website_label,
				      "personal_table", &personal_table,
				      NULL);

	if (name) {
		who = g_strdup_printf (_("%s wants to be added to your contact list."), 
				       name);
		question = g_strdup_printf (_("Do you want to add %s to your contact list?"),
					    name);
	} else {
		who = g_strdup (_("Someone wants to be added to your contact list."));
		question = g_strdup (_("Do you want to add this person to your contact list?"));
	}

	str = g_strdup_printf ("<span weight='bold' size='larger'>%s</span>", who);
	gtk_label_set_markup (GTK_LABEL (who_label), str);
	gtk_label_set_use_markup (GTK_LABEL (who_label), TRUE);
	g_free (str);
	g_free (who);

	gtk_label_set_text (GTK_LABEL (question_label), question);
	g_free (question);

	gtk_label_set_text (GTK_LABEL (jid_label), gossip_contact_get_id (data->contact));

	if (url && strlen (url) > 0) {
		GArray *start, *end;

		start = g_array_new (FALSE, FALSE, sizeof (gint));
		end = g_array_new (FALSE, FALSE, sizeof (gint));
		
		num_matches = gossip_utils_url_regex_match (url, start, end);
	}

	if (num_matches > 0) {
		GtkWidget *href;
		GtkWidget *alignment;

		href = gnome_href_new (url, url);

		alignment = gtk_alignment_new (0, 1, 0, 0.5);
		gtk_container_add (GTK_CONTAINER (alignment), href);

		gtk_table_attach (GTK_TABLE (personal_table),
				  alignment, 
				  1, 2,
				  1, 2,
				  GTK_FILL, GTK_FILL,
				  0, 0);
		
		gtk_widget_show_all (personal_table);
	} else {
		gtk_widget_hide (website_label);
	}

	g_signal_connect (dialog,
			  "response",
			  G_CALLBACK (app_subscription_request_dialog_cb),
			  data);

	gtk_widget_show (dialog);
}

static void
app_subscription_request_dialog_cb (GtkWidget        *dialog,
				    gint              response,
				    SubscriptionData *data)
{
	gboolean add_user;

	g_return_if_fail (GTK_IS_DIALOG (dialog));

	g_return_if_fail (GOSSIP_IS_PROTOCOL (data->protocol));
	g_return_if_fail (GOSSIP_IS_CONTACT (data->contact));

	add_user = (gossip_contact_get_type (data->contact) == GOSSIP_CONTACT_TYPE_TEMPORARY);

	gtk_widget_destroy (dialog);
	
	if (response == GTK_RESPONSE_YES ||
	    response == GTK_RESPONSE_NO) {
		gboolean subscribe;

		subscribe = (response == GTK_RESPONSE_YES);
		gossip_protocol_set_subscription (data->protocol, 
						  data->contact, 
						  subscribe);

		if (subscribe && add_user) {
			const gchar *id, *name, *message;
			
			id = gossip_contact_get_id (data->contact);
			if (data->vcard) {
				name = gossip_vcard_get_name (data->vcard);
			} else {
				name = id;
			}
			
			message = _("I would like to add you to my contact list.");
					
			/* FIXME: how is session related to an IM account? */
			/* micke, hmm .. this feels a bit wrong, should be
			 * signalled from the protocol when we do 
			 * set_subscribed
			 */
			gossip_protocol_add_contact (data->protocol,
						     id, name, NULL,
						     message);
		}
	}
	
	g_object_unref (data->protocol);
	g_object_unref (data->contact);

	if (data->vcard) {
		g_object_unref (data->vcard);
	}
	
	g_free (data);
}

GossipSession *
gossip_app_get_session (void)
{
	return app->priv->session;
}

GossipChatroomManager *
gossip_app_get_chatroom_manager (void)
{
	return app->priv->chatroom_manager;
}

GossipChatManager *
gossip_app_get_chat_manager (void)
{
	return app->priv->chat_manager;
}

GossipEventManager *
gossip_app_get_event_manager (void)
{
	return app->priv->event_manager;
}

