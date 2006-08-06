/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2006 Imendio AB
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
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#ifdef HAVE_GNOME
#include <libgnomeui/libgnomeui.h>
#include "eggtrayicon.h"
#endif

#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-ft.h>
#include <libgossip/gossip-presence.h>
#include <libgossip/gossip-protocol.h>
#include <libgossip/gossip-utils.h>
#include <libgossip/gossip-conf.h>

#include "gossip-about-dialog.h"
#include "gossip-account-button.h"
#include "gossip-accounts-dialog.h"
#include "gossip-add-contact-window.h"
#include "gossip-app.h"
#include "gossip-chat.h"
#include "gossip-chatrooms-window.h"
#include "gossip-contact-list.h"
#include "gossip-ft-window.h"
#include "gossip-geometry.h"
#include "gossip-group-chat.h"
#include "gossip-idle.h"
#include "gossip-log-window.h"
#include "gossip-marshal.h"
#include "gossip-new-account-window.h"
#include "gossip-new-chatroom-dialog.h"
#include "gossip-new-message-dialog.h"
#include "gossip-preferences.h"
#include "gossip-presence-chooser.h"
#include "gossip-private-chat.h"
#include "gossip-sound.h"
#include "gossip-status-presets.h"
#include "gossip-stock.h"
#include "gossip-subscription-dialog.h"
#include "gossip-throbber.h"
#include "gossip-ui-utils.h"
#include "gossip-vcard-dialog.h"

#ifdef HAVE_DBUS
#include "gossip-dbus.h"
#endif

#ifdef HAVE_GALAGO
#include "gossip-galago.h"
#endif

#ifdef HAVE_LIBNOTIFY
#include "gossip-notify.h"
#endif


#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_APP, GossipAppPriv))

#define DEBUG_DOMAIN_SETUP     "AppSetup"
#define DEBUG_DOMAIN_ACCELS    "AppAccels"
#define DEBUG_DOMAIN_IDLE      "AppIdle"
#define DEBUG_DOMAIN_ACCOUNTS  "AppAccounts"
#define DEBUG_DOMAIN_CHATROOMS "AppChatrooms"
#define DEBUG_DOMAIN_TRAY      "AppTray"
#define DEBUG_DOMAIN_SESSION   "AppSession"

#define DEBUG_QUIT

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

/* Accels (menu shortcuts) can be configured and saved */
#define ACCELS_FILENAME "accels.txt"

struct _GossipAppPriv {
	GossipSession         *session;

	GossipChatroomManager *chatroom_manager;

	GossipChatManager     *chat_manager;
        GossipEventManager    *event_manager;
	
	GossipContactList     *contact_list;

	/* Main widgets */
	GtkWidget             *window;
	GtkWidget             *main_vbox;
	GtkWidget             *errors_vbox;
	GHashTable            *errors;

	/* Tooltips for all widgets */
	GtkTooltips           *tooltips;

	/* Menu widgets */
	GtkWidget             *actions_connect;
	GtkWidget             *actions_disconnect;
	GtkWidget             *actions_hide_list;
	GtkWidget             *actions_group_chat_join;   

	/* Tray */
	/*EggTrayIcon*/ GtkWidget           *tray_icon;
	GtkWidget             *tray_event_box;
	GtkWidget             *tray_image;
	GList                 *tray_flash_icons;
	guint                  tray_flash_timeout_id;

	GtkWidget             *popup_menu;
	GtkWidget             *popup_menu_status_item;
	GtkWidget             *popup_menu_show_list_item;
	 
	/* Throbber */
	GtkWidget             *throbber;

	/* Widgets that are enabled when we're connected/disconnected */
	GList                 *widgets_connected;
	GList                 *widgets_disconnected;

	/* Status popup */
	GtkWidget             *presence_toolbar;
	GtkWidget             *presence_chooser;

	time_t                 leave_time;

	/* Presence set by the user (available/busy) */
	GossipPresence        *presence;

	/* Away presence (away/xa), overrides priv->presence */
	GossipPresence        *away_presence;
	
	/* Misc */
	guint                  size_timeout_id;
};

static void            gossip_app_class_init                (GossipAppClass           *klass);
static void            gossip_app_init                      (GossipApp                *singleton_app);
static void            app_finalize                         (GObject                  *object);
static void            app_setup                            (GossipSession            *session,
							     GossipAccountManager     *manager);
static gboolean        app_main_window_quit_confirm         (GossipApp                *app,
							     GtkWidget                *window);
static void            app_main_window_quit_confirm_cb      (GtkWidget                *dialog,
							     gint                      response,
							     GossipApp                *app);
static void            app_main_window_destroy_cb           (GtkWidget                *window,
							     GossipApp                *app);
static gboolean        app_main_window_delete_event_cb      (GtkWidget                *window,
							     GdkEvent                 *event,
							     GossipApp                *app);
static gboolean        app_main_window_key_press_event_cb   (GtkWidget                *window,
							     GdkEventKey              *event,
							     GossipApp                *app);
static void            app_quit_cb                          (GtkWidget                *window,
							     GossipApp                *app);
static void            app_connect_cb                       (GtkWidget                *window,
							     GossipApp                *app);
static void            app_disconnect_cb                    (GtkWidget                *window,
							     GossipApp                *app);
static void            app_new_message_cb                   (GtkWidget                *widget,
							     GossipApp                *app);
static void            app_history_cb                       (GtkWidget                *window,
							     GossipApp                *app);
static void            app_group_chat_new_cb                (GtkWidget                *window,
							     GossipApp                *app);
static void            app_group_chat_join_cb               (GtkWidget                *window,
							     GossipApp                *app);
static void            app_group_chat_rooms_cb              (GtkWidget                *window,
							     GossipApp                *app);
static void            app_add_contact_cb                   (GtkWidget                *widget,
							     GossipApp                *app);
static void            app_show_offline_cb                  (GtkCheckMenuItem         *item,
							     GossipApp                *app);
static void            app_accounts_cb                      (GtkWidget                *widget,
							     GossipApp                *app);
static void            app_personal_information_cb          (GtkWidget                *widget,
							     GossipApp                *app);
static void            app_preferences_cb                   (GtkWidget                *widget,
							     GossipApp                *app);
static void            app_about_cb                         (GtkWidget                *window,
							     GossipApp                *app);
static void            app_help_cb                          (GtkWidget                *window,
							     GossipApp                *app);
static gboolean        app_throbber_button_press_event_cb   (GtkWidget                *throbber,
							     GdkEventButton           *event,
							     gpointer                  user_data);
static void            app_session_protocol_connecting_cb   (GossipSession            *session,
							     GossipAccount            *account,
							     GossipProtocol           *protocol,
							     gpointer                  user_data);
static void            app_session_protocol_connected_cb    (GossipSession            *session,
							     GossipAccount            *account,
							     GossipProtocol           *protocol,
							     gpointer                  user_data);
static void            app_session_protocol_disconnected_cb (GossipSession            *session,
							     GossipAccount            *account,
							     GossipProtocol           *protocol,
							     gpointer                  user_data);
static void            app_session_protocol_error_cb        (GossipSession            *session,
							     GossipProtocol           *protocol,
							     GossipAccount            *account,
							     GError                   *error,
							     gpointer                  user_data);
static gchar *         app_session_get_password_cb          (GossipSession            *session,
							     GossipAccount            *account,
							     gpointer                  user_data);
static void            app_accels_load                      (void);
static void            app_accels_save                      (void);
static void            app_show_hide_list_cb                (GtkWidget                *widget,
							     GossipApp                *app);
static void            app_popup_new_message_cb             (GtkWidget                *widget,
							     gpointer                  user_data);
static gboolean        app_tray_destroy_cb                  (GtkWidget                *widget,
							     gpointer                  user_data);
static gboolean        app_tray_button_press_cb             (GtkWidget                *widget,
							     GdkEventButton           *event,
							     GossipApp                *app);
static void            app_tray_create_menu                 (void);
static void            app_tray_create                      (void);
static void            app_tray_update_tooltip              (void);
static void            app_status_flash_start               (void);
static void            app_status_flash_stop                (void);
static void            app_notify_show_offline_cb           (GossipConf               *conf,
							     const gchar              *key,
 							     gpointer                  check_menu_item);
static void            app_notify_show_avatars_cb           (GossipConf               *conf,
							     const gchar              *key,
							     gpointer                  user_data);
static void            app_connect_no_accounts_response_cb  (GtkWidget                *widget,
							     gint                      response,
							     gpointer                  user_data);
static gboolean        app_idle_check_cb                    (GossipApp                *app);
static void            app_disconnect                       (void);
static void            app_connection_items_setup           (GladeXML                 *glade);
static void            app_connection_items_update          (void);
static void            app_accounts_error_edit_clicked_cb   (GtkButton                *button,
							     GossipAccount            *account);
static void            app_accounts_error_clear_clicked_cb  (GtkButton                *button,
							     GossipAccount            *account);
static void            app_accounts_error_display           (GossipAccount            *account,
							     GError                   *error);
static GossipPresence *app_get_effective_presence           (void);
static void            app_set_away                         (const gchar              *status);
static GdkPixbuf *     app_get_current_status_pixbuf        (void);
static GossipPresenceState
                       app_get_current_state                (void);
static GossipPresenceState
                       app_get_previous_state               (void);
static void            app_presence_updated                 (void);
static void            app_status_clear_away                (void);
static void            app_presence_chooser_changed_cb      (GtkWidget                *chooser,
							     GossipPresenceState       state,
							     const gchar              *status,
							     gpointer                  user_data);
static gboolean        configure_event_timeout_cb           (GtkWidget                *widget);
static gboolean        app_window_configure_event_cb        (GtkWidget                *widget,
							     GdkEventConfigure        *event,
							     GossipApp                *app);
static gboolean        app_tray_flash_timeout_func          (gpointer                  data);
static void            app_tray_flash_start                 (void);
static void            app_tray_flash_maybe_stop            (void);
static void            app_chatroom_auto_connect_update_cb  (GossipChatroomManager    *manager,
							     GossipChatroomProvider   *provider,
							     GossipChatroom           *chatroom,
							     GossipChatroomJoinResult  result,
							     gpointer                  user_data);
static void            app_chatroom_favourite_update        (void);
static void            app_chatroom_favourite_update_cb     (GossipChatroomManager    *manager,
							     GossipChatroom           *chatroom,
							     gpointer                  user_data);
static void            app_event_added_cb                   (GossipEventManager       *manager,
							     GossipEvent              *event,
							     gpointer                  user_data);
static void            app_event_removed_cb                 (GossipEventManager       *manager,
							     GossipEvent              *event,
							     gpointer                  user_data);
static void            app_contact_activated_cb             (GossipContactList        *contact_list,
							     GossipContact            *contact,
							     GossipEventId             event_id,
							     gpointer                  user_data);

static GossipApp    *app;

G_DEFINE_TYPE (GossipApp, gossip_app, G_TYPE_OBJECT);

static void
gossip_app_class_init (GossipAppClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);
	
        object_class->finalize = app_finalize;

	g_type_class_add_private (object_class, sizeof (GossipAppPriv));
}

static void
gossip_app_init (GossipApp *singleton_app)
{
        GossipAppPriv *priv;

	app = singleton_app;

	priv = GET_PRIV (app);

	priv->errors = g_hash_table_new_full (gossip_account_hash,
					      gossip_account_equal,
					      g_object_unref,
					      (GDestroyNotify) gtk_widget_destroy);
	priv->tooltips = g_object_ref (gtk_tooltips_new ());
	gtk_object_sink (GTK_OBJECT (priv->tooltips));
}

static void
app_finalize (GObject *object)
{
        GossipApp            *app;
        GossipAppPriv        *priv;
	GossipAccountManager *manager;
	
        app = GOSSIP_APP (object);
        priv = GET_PRIV (app);

	if (priv->size_timeout_id) {
		g_source_remove (priv->size_timeout_id);
	}

	if (priv->tray_flash_timeout_id) {
		g_source_remove (priv->tray_flash_timeout_id);
	}

	g_list_free (priv->widgets_connected);
	g_list_free (priv->widgets_disconnected);

	if (priv->errors) {
		/* FIXME: We are leaking this on exit. We need to
		 * connect to the error widgets' "destroy" signals and
		 * remove them from the hash table, otherwise this
		 * call will crash, since the widgets are gone
		 * already.
		 */
		/*g_hash_table_destroy (priv->errors);*/
	}

	gtk_widget_destroy (priv->popup_menu);
	
	g_object_unref (priv->tooltips);

	if (priv->tray_icon) {
		/* Disconnect so it doesn't get recreated when we destroy it. */
		g_signal_handlers_disconnect_by_func (priv->tray_icon,
						      app_tray_destroy_cb,
						      priv->tray_event_box);
		
		gtk_widget_destroy (GTK_WIDGET (priv->tray_icon));
	}

	gossip_conf_shutdown ();

#ifdef HAVE_LIBNOTIFY
	gossip_notify_finalize ();
#endif

	gossip_sound_finalize ();

	manager = gossip_session_get_account_manager (priv->session);

	g_signal_handlers_disconnect_by_func (priv->session,
					      app_session_protocol_connecting_cb, 
					      NULL);
	g_signal_handlers_disconnect_by_func (priv->session,
					      app_session_protocol_connected_cb, 
					      NULL);
	g_signal_handlers_disconnect_by_func (priv->session,
					      app_session_protocol_disconnected_cb, 
					      NULL);
	g_signal_handlers_disconnect_by_func (priv->session,
					      app_session_protocol_error_cb, 
					      NULL);
	g_signal_handlers_disconnect_by_func (priv->session,
					      app_session_get_password_cb, 
					      NULL);

	g_signal_handlers_disconnect_by_func (priv->chatroom_manager,
					      app_chatroom_auto_connect_update_cb, 
					      NULL);

	g_signal_handlers_disconnect_by_func (priv->event_manager,
					      app_event_added_cb, 
					      NULL);
	g_signal_handlers_disconnect_by_func (priv->event_manager,
					      app_event_removed_cb, 
					      NULL);

	if (priv->presence) {
		g_object_unref (priv->presence);
	}

	if (priv->away_presence) {
		g_object_unref (priv->away_presence);
	}

	g_object_unref (priv->event_manager);
	g_object_unref (priv->chat_manager);
	g_object_unref (priv->chatroom_manager);
	g_object_unref (priv->session);

	gossip_ft_window_finalize (priv->session);
	gossip_subscription_dialog_finalize (priv->session);

	G_OBJECT_CLASS (gossip_app_parent_class)->finalize (object);
}

static void
app_setup (GossipSession        *session,
	   GossipAccountManager *manager)
{
        GossipAppPriv  *priv;
	GossipConf     *conf;
	GladeXML       *glade;
	GtkWidget      *sw;
	GtkWidget      *image;
	GtkToolItem    *item;
	GtkWidget      *show_offline_widget;
	GtkRequisition  req;
	gchar          *str;
	gboolean        show_offline;
	gboolean        show_avatars;
	gboolean        hidden;
	gint            x, y, w, h;

	gossip_debug (DEBUG_DOMAIN_SETUP, "Beginning...");

	priv = GET_PRIV (app);

	conf = gossip_conf_get ();
	
	priv->session = g_object_ref (session);

	gossip_debug (DEBUG_DOMAIN_SETUP,
		      "Initialising session listeners "
		      "(subscription, file transfer, etc)");
	gossip_subscription_dialog_init (priv->session);
	gossip_ft_window_init (priv->session);

	if (gossip_new_account_window_is_needed ()) {
		gossip_debug (DEBUG_DOMAIN_SETUP,
			      "Showing new account window "
			      "for first time run");
		gossip_new_account_window_show (NULL);
	}

	gossip_debug (DEBUG_DOMAIN_SETUP,
		      "Initialising managers "
		      "(chatroom, chat, event)");

	priv->chatroom_manager = gossip_chatroom_manager_new (manager, 
							      priv->session, 
							      NULL);

	priv->chat_manager = gossip_chat_manager_new ();
 	priv->event_manager = gossip_event_manager_new (); 

	gossip_debug (DEBUG_DOMAIN_SETUP,
		      "Initialising notification "
		      "handlers (libnotify, sound)");

#ifdef HAVE_LIBNOTIFY
	gossip_notify_init (priv->session, priv->event_manager);
#endif

	gossip_sound_init (priv->session);

	g_signal_connect (priv->session, "protocol-connecting",
			  G_CALLBACK (app_session_protocol_connecting_cb),
			  NULL);

	g_signal_connect (priv->session, "protocol-connected",
			  G_CALLBACK (app_session_protocol_connected_cb),
			  NULL);

	g_signal_connect (priv->session, "protocol-disconnected",
			  G_CALLBACK (app_session_protocol_disconnected_cb),
			  NULL);

	g_signal_connect (priv->session, "protocol-error",
			  G_CALLBACK (app_session_protocol_error_cb),
			  NULL);

	g_signal_connect (priv->session, "get-password",
			  G_CALLBACK (app_session_get_password_cb),
			  NULL);

	g_signal_connect (priv->chatroom_manager, "chatroom-auto-connect-update",
			  G_CALLBACK (app_chatroom_auto_connect_update_cb),
			  NULL);

	g_signal_connect (priv->chatroom_manager, "chatroom-favourite-update",
			  G_CALLBACK (app_chatroom_favourite_update_cb),
			  NULL);

	g_signal_connect (priv->event_manager, "event-added",
			  G_CALLBACK (app_event_added_cb),
			  NULL);
	g_signal_connect (priv->event_manager, "event-removed",
			  G_CALLBACK (app_event_removed_cb),
			  NULL);

	/* set up interface */
	gossip_debug (DEBUG_DOMAIN_SETUP, "Initialising interface");
	glade = gossip_glade_get_file ("main.glade",
				       "main_window",
				       NULL,
				       "main_window", &priv->window,
				       "main_vbox", &priv->main_vbox,
				       "errors_vbox", &priv->errors_vbox,
				       "actions_connect", &priv->actions_connect,
				       "actions_disconnect", &priv->actions_disconnect,
				       "actions_show_offline", &show_offline_widget,
				       "actions_hide_list", &priv->actions_hide_list,
				       "actions_group_chat_join", &priv->actions_group_chat_join,
				       "presence_toolbar", &priv->presence_toolbar,
				       "roster_scrolledwindow", &sw,
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
			      "actions_new_message", "activate", app_new_message_cb,
			      "actions_history", "activate", app_history_cb,
			      "actions_group_chat_new", "activate", app_group_chat_new_cb,
			      "actions_group_chat_join", "activate", app_group_chat_join_cb,
			      "actions_group_chat_rooms", "activate", app_group_chat_rooms_cb,
			      "actions_add_contact", "activate", app_add_contact_cb,
			      "actions_show_offline", "toggled", app_show_offline_cb,
			      "edit_accounts", "activate", app_accounts_cb,
			      "edit_personal_information", "activate", app_personal_information_cb,
			      "edit_preferences", "activate", app_preferences_cb,
			      "help_about", "activate", app_about_cb,
			      "help_contents", "activate", app_help_cb,
			      NULL);

	/* Set up menu */
	gossip_debug (DEBUG_DOMAIN_SETUP, "Configuring menu");
	g_object_get (GTK_IMAGE_MENU_ITEM (priv->actions_connect), "image", &image, NULL);
	gtk_image_set_from_stock (GTK_IMAGE (image), GTK_STOCK_CONNECT, GTK_ICON_SIZE_MENU);
	gtk_label_set_text_with_mnemonic (GTK_LABEL (GTK_BIN (priv->actions_connect)->child), 
					  _("_Connect"));

	g_object_get (GTK_IMAGE_MENU_ITEM (priv->actions_disconnect), "image", &image, NULL);
	gtk_image_set_from_stock (GTK_IMAGE (image), GTK_STOCK_DISCONNECT, GTK_ICON_SIZE_MENU);
	gtk_label_set_text_with_mnemonic (GTK_LABEL (GTK_BIN (priv->actions_disconnect)->child), 
					  _("_Disconnect"));

	g_signal_connect (priv->actions_hide_list, "activate", 
			  G_CALLBACK (app_show_hide_list_cb), app);

	/* Set up connection related widgets. */
	app_connection_items_setup (glade);
	g_object_unref (glade);

	/* Set up presence chooser */
	gossip_debug (DEBUG_DOMAIN_SETUP, "Configuring specialised widgets");
	priv->presence_chooser = gossip_presence_chooser_new ();
	gtk_widget_show (priv->presence_chooser);
	gossip_presence_chooser_set_flash_interval (GOSSIP_PRESENCE_CHOOSER (priv->presence_chooser), 
						    FLASH_TIMEOUT);

	item = gtk_tool_item_new ();
	gtk_widget_show (GTK_WIDGET (item));
 	gtk_container_add (GTK_CONTAINER (item), priv->presence_chooser); 
	gtk_tool_item_set_is_important (item, TRUE);
 	gtk_tool_item_set_expand (item, TRUE); 
	gtk_toolbar_insert (GTK_TOOLBAR (priv->presence_toolbar), item, -1);

	g_signal_connect (priv->presence_chooser,
			  "changed",
			  G_CALLBACK (app_presence_chooser_changed_cb),
			  NULL);

	priv->widgets_connected = g_list_prepend (priv->widgets_connected,
						  priv->presence_chooser);

	/* Set up the throbber */
	priv->throbber = gossip_throbber_new ();
	gossip_throbber_set_small_mode (GOSSIP_THROBBER (priv->throbber), TRUE);
        gtk_widget_show (priv->throbber);

        item = gtk_tool_item_new ();
	gtk_container_add (GTK_CONTAINER (item), priv->throbber);
        gtk_widget_show (GTK_WIDGET (item));

        gtk_toolbar_insert (GTK_TOOLBAR (priv->presence_toolbar), item, -1);

	str = _("Show accounts and pending connections");
	gtk_tooltips_set_tip (GTK_TOOLTIPS (priv->tooltips),
			      priv->throbber, str, str);

	g_signal_connect (priv->throbber, 
			  "button-press-event",
			  G_CALLBACK (app_throbber_button_press_event_cb),
			  NULL);

	/* Set up contact list. */
	priv->contact_list = gossip_contact_list_new ();

	g_signal_connect (priv->contact_list, "contact-activated",
			  G_CALLBACK (app_contact_activated_cb),
			  NULL);

	/* Get saved presence presets. */
	gossip_debug (DEBUG_DOMAIN_SETUP, "Configuring presets");
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
	gossip_debug (DEBUG_DOMAIN_SETUP, "Configuring notification area widgets");
	app_tray_create_menu ();
	app_tray_create ();

	/* Load user-defined accelerators. */
	gossip_debug (DEBUG_DOMAIN_SETUP, "Configuring accels");
	app_accels_load ();

	/* Set the idle time checker. */
	g_timeout_add (2 * 1000, (GSourceFunc) app_idle_check_cb, app);

	/* Set window size. */
	gossip_debug (DEBUG_DOMAIN_SETUP, "Configuring window geometry...");
	gossip_geometry_load_for_main_window (&x, &y, &w, &h);

	if (w >= 1 && h >= 1) {
		/* Use the defaults from the glade file if we
		 * don't have good w, h geometry.  
		 */
		gossip_debug (DEBUG_DOMAIN_SETUP, "Configuring window default size w:%d, h:%d", w, h); 
		gtk_window_set_default_size (GTK_WINDOW (priv->window), w, h);
	}

	if (x >= 1 && y >= 1) {
		/* Let the window manager position it if we
		 * don't have good x, y coordinates.  
		 */
		gossip_debug (DEBUG_DOMAIN_SETUP, "Configuring window default position x:%d, y:%d", x, y); 
		gtk_window_move (GTK_WINDOW (priv->window), x, y);
	}

	/* Set up current presence. */
	app_presence_updated ();

	/* Set up 'show_offline' config hooks to know when it changes. */
	gossip_debug (DEBUG_DOMAIN_SETUP, "Configuring miscellaneous settings");
	gossip_conf_get_bool (conf,
			      GOSSIP_PREFS_CONTACTS_SHOW_OFFLINE,
			      &show_offline);
	gossip_conf_notify_add (conf,
				GOSSIP_PREFS_CONTACTS_SHOW_OFFLINE,
				app_notify_show_offline_cb,
				show_offline_widget);

	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (show_offline_widget),
					show_offline);

	/* Show avatars? */
	gossip_conf_get_bool (conf,
			      GOSSIP_PREFS_UI_SHOW_AVATARS,
			      &show_avatars);
	gossip_conf_notify_add (conf,
				GOSSIP_PREFS_UI_SHOW_AVATARS,
				app_notify_show_avatars_cb,
				NULL);

	g_object_set (priv->contact_list, "show-avatars", show_avatars, NULL);

	/* Set window to be hidden. If doesn't have tray, show window and mask
	 * "actions_hide_list".
	 */
	if (!gossip_have_tray ()) {
		hidden = FALSE;
		gtk_widget_hide (GTK_WIDGET (priv->actions_hide_list));
	} else {
		gossip_conf_get_bool (conf,
				      GOSSIP_PREFS_UI_MAIN_WINDOW_HIDDEN,
				      &hidden);
	}

	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (show_offline_widget),
					show_offline);

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

	gossip_debug (DEBUG_DOMAIN_SETUP, "Complete!");
}

static gboolean 
app_main_window_quit_confirm (GossipApp *app,
			      GtkWidget *window)
{
	GossipAppPriv *priv;
	GList         *events;
	
	priv = GET_PRIV (app);

	events = gossip_event_manager_get_events (priv->event_manager);

	if (g_list_length ((GList*)events) > 0) {
		GList     *l;
		gint       i;
		gint       count[GOSSIP_EVENT_ERROR];

		GtkWidget *dialog;
		gchar     *str;
		gchar     *oldstr;

		str = g_strconcat (_("To summarize:"), "\n", NULL);

		for (i = 0; i < GOSSIP_EVENT_ERROR; i++) {
			count[i] = 0;
		}

		for (l = events; l; l = l->next) {
			GossipEvent     *event;
			GossipEventType  type; 

			event = l->data;
			type = gossip_event_get_type (event);

			count[type]++;
		}

		str = g_strdup ("");
		for (i = 0; i < GOSSIP_EVENT_ERROR; i++) {
			gchar *info;
			gchar *format;

			if (count[i] < 1) {
				continue;
			}
			
			switch (i) {
			case GOSSIP_EVENT_NEW_MESSAGE: 
				format = ngettext ("%d new message",
						   "%d new messages",
						   count[i]);
				break;
			case GOSSIP_EVENT_SUBSCRIPTION_REQUEST: 
				format = ngettext ("%d subscription request",
						   "%d subscription requests",
						   count[i]);
				break;
			case GOSSIP_EVENT_FILE_TRANSFER_REQUEST: 
				format = ngettext ("%d file transfer request",
						   "%d file transfer requests",
						   count[i]);
				break;
			case GOSSIP_EVENT_SERVER_MESSAGE: 
				format = ngettext ("%d server message",
						   "%d server messages",
						   count[i]);
				break;
			case GOSSIP_EVENT_ERROR: 
				format = ngettext ("%d error",
						   "%d errors",
						   count[i]);
				break;
			default:
				format = "";
				break;
			}

			info = g_strdup_printf (format, count[i]);

			oldstr = str;
			str = g_strconcat (str, info, "\n",NULL);
			g_free (info);
			g_free (oldstr);
		}

		g_list_free (events);

		dialog = gtk_message_dialog_new_with_markup 
			(GTK_WINDOW (gossip_app_get_window ()),
			 0,
			 GTK_MESSAGE_WARNING,
			 GTK_BUTTONS_OK_CANCEL,
			 "<b>%s</b>\n\n%s",
			 _("If you quit, you will lose all unread information."), 
			 str);
	
		g_signal_connect (dialog, "response",
				  G_CALLBACK (app_main_window_quit_confirm_cb),
				  app);
		
		gtk_widget_show (dialog);

		g_free (str);
		
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
	
	priv = GET_PRIV (app);

	gtk_widget_destroy (dialog);

	if (response == GTK_RESPONSE_OK) {
		gtk_widget_destroy (priv->window);
	}
}

static void
app_main_window_destroy_cb (GtkWidget *window,
			    GossipApp *app)
{
	GossipAppPriv *priv;
	
	priv = GET_PRIV (app);

	if (gossip_session_is_connected (priv->session, NULL)) {
		gossip_session_disconnect (priv->session, NULL);
	}
	
	/* Save user-defined accelerators. */
	app_accels_save ();

#ifdef DEBUG_QUIT
	gtk_main_quit ();
#else	
	exit (EXIT_SUCCESS);
#endif
}

static gboolean
app_main_window_delete_event_cb (GtkWidget *window,
				 GdkEvent  *event,
				 GossipApp *app)
{
	return app_main_window_quit_confirm (app, window);
}

static gboolean
app_main_window_key_press_event_cb (GtkWidget   *window, 
				    GdkEventKey *event,
				    GossipApp   *app) 
{
	if (event->keyval == GDK_Escape) {
		gossip_app_toggle_visibility ();
	}

	return FALSE;
}

static void 
app_quit_cb (GtkWidget *window,
	     GossipApp *app)
{
	GossipAppPriv *priv;
	
	priv = GET_PRIV (app);

	if (!app_main_window_quit_confirm (app, priv->window)) {
		gtk_widget_destroy (priv->window);
	}
}

static void
app_connect_cb (GtkWidget *window,
		GossipApp *app)
{
	g_return_if_fail (GOSSIP_IS_APP (app));

	gossip_app_connect (NULL, FALSE);
}

static void
app_disconnect_cb (GtkWidget *window,
		   GossipApp *app)
{
	g_return_if_fail (GOSSIP_IS_APP (app));

	app_disconnect ();
}

static void
app_new_message_cb (GtkWidget *widget,
		    GossipApp *app)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_new_message_dialog_show (GTK_WINDOW (priv->window));
}

static void
app_history_cb (GtkWidget *widget,
		GossipApp *app)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_log_window_show (NULL, NULL);
}

static void
app_group_chat_new_cb (GtkWidget *window,
		       GossipApp *app)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);
	
	gossip_new_chatroom_dialog_show (GTK_WINDOW (priv->window));
}

static void
app_group_chat_join_cb (GtkWidget *window,
			GossipApp *app)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);
	
	gossip_chatroom_manager_join_favourites (priv->chatroom_manager);
}

static void
app_group_chat_rooms_cb (GtkWidget *window,
			 GossipApp *app)
{
	gossip_chatrooms_window_show (NULL, FALSE);
}

static void
app_add_contact_cb (GtkWidget *widget, 
		    GossipApp *app)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_add_contact_window_show (GTK_WINDOW (priv->window), NULL);
}

static void
app_show_offline_cb (GtkCheckMenuItem *item, 
		     GossipApp        *app)
{
	GossipAppPriv *priv;
	gboolean       current;
	
	priv = GET_PRIV (app);

	current = gtk_check_menu_item_get_active (item);

	gossip_conf_set_bool (gossip_conf_get (),
			       GOSSIP_PREFS_CONTACTS_SHOW_OFFLINE,
			       current);
	
	/* Turn off sound just while we alter the contact list. */
	gossip_sound_set_enabled (FALSE);
	g_object_set (priv->contact_list, "show_offline", current, NULL);
	gossip_sound_set_enabled (TRUE);
}

static void
app_accounts_cb (GtkWidget *widget, 
		 GossipApp *app)
{
	gossip_accounts_dialog_show (NULL);
}

static void
app_personal_information_cb (GtkWidget *widget, 
			     GossipApp *app)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);
	
	gossip_vcard_dialog_show (GTK_WINDOW (priv->window));
}

static void
app_preferences_cb (GtkWidget *widget, 
		    GossipApp *app)
{
	gossip_preferences_show ();
}

static void
app_about_cb (GtkWidget *window,
	      GossipApp *app)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);
	
	gossip_about_dialog_new (GTK_WINDOW (priv->window));
}

static void
app_help_cb (GtkWidget *window,
	     GossipApp *app)
{
	gossip_help_show ();
}

static gboolean
app_throbber_button_press_event_cb (GtkWidget      *throbber,
				    GdkEventButton *event,
				    gpointer        user_data)
{
	if (event->type != GDK_BUTTON_PRESS || 
	    event->button != 1) {
		return FALSE;
	}

	gossip_accounts_dialog_show (NULL);

	return FALSE;
}

static void
app_session_protocol_connecting_cb (GossipSession  *session,
				    GossipAccount  *account,
				    GossipProtocol *protocol,
				    gpointer        user_data)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_debug (DEBUG_DOMAIN_SESSION, "Connecting account:'%s'", 
		      gossip_account_get_id (account));

	gossip_throbber_start (GOSSIP_THROBBER (priv->throbber));
}

static void
app_session_protocol_connected_cb (GossipSession  *session,
				   GossipAccount  *account,
				   GossipProtocol *protocol,
				   gpointer        user_data)
{
	GossipAppPriv *priv;
	gboolean       connecting;

	priv = GET_PRIV (app);

	gossip_debug (DEBUG_DOMAIN_SESSION, "Connected account:'%s'", 
		      gossip_account_get_id (account));

	gossip_session_count_accounts (priv->session,
				       NULL, 
				       &connecting,
				       NULL);

	if (connecting < 1) {
		gossip_throbber_stop (GOSSIP_THROBBER (priv->throbber));
	}

	g_hash_table_remove (priv->errors, account);

	app_connection_items_update ();
	app_chatroom_favourite_update ();

	/* Use saved presence */
	gossip_app_set_presence (gossip_status_presets_get_default_state (), 
				 gossip_status_presets_get_default_status());

	app_presence_updated ();
}

static void
app_session_protocol_disconnected_cb (GossipSession  *session,
				      GossipAccount  *account,
				      GossipProtocol *protocol,
				      gpointer        user_data)
{
	GossipAppPriv *priv;
	gboolean       connecting;

	priv = GET_PRIV (app);

	gossip_debug (DEBUG_DOMAIN_SESSION, "Disconnected account:'%s'", 
		      gossip_account_get_id (account));

	gossip_session_count_accounts (priv->session,
				       NULL, 
				       &connecting,
				       NULL);

	if (connecting < 1) {
		gossip_throbber_stop (GOSSIP_THROBBER (priv->throbber));
	}

 	app_connection_items_update ();
	app_chatroom_favourite_update ();
	app_presence_updated ();
}

static void
app_session_protocol_error_cb (GossipSession  *session,
			       GossipProtocol *protocol,
			       GossipAccount  *account,
			       GError         *error,
			       gpointer        user_data)
{
	GossipAppPriv *priv;
	gboolean       connecting;

	priv = GET_PRIV (app);

	gossip_debug (DEBUG_DOMAIN_SESSION, "Error for account:'%s'", 
		      gossip_account_get_id (account));

	gossip_session_count_accounts (priv->session,
				       NULL, 
				       &connecting,
				       NULL);

	if (connecting < 1) {
		gossip_throbber_stop (GOSSIP_THROBBER (priv->throbber));
	}

	app_accounts_error_display (account, error);	
}

static gchar * 
app_session_get_password_cb (GossipSession *session,
			     GossipAccount *account,
			     gpointer       user_data)
{
	GossipAppPriv *priv;
	gchar         *password;

	priv = GET_PRIV (app);

	password = gossip_password_dialog_run (account,
					       GTK_WINDOW (priv->window));

	return password;
}

/*
 * Accels
 */
static void
app_accels_load (void)
{
	gchar *filename;

	filename = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, ACCELS_FILENAME, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		gossip_debug (DEBUG_DOMAIN_ACCELS, "Loading from:'%s'", filename);
		gtk_accel_map_load (filename);
	}
	
	g_free (filename);
}

static void
app_accels_save (void)
{
	gchar *dir;
	gchar *file_with_path;

	dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
	g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	file_with_path = g_build_filename (dir, ACCELS_FILENAME, NULL);
	g_free (dir);

	gossip_debug (DEBUG_DOMAIN_ACCELS, "Saving to:'%s'", file_with_path);
	gtk_accel_map_save (file_with_path);
	
	g_free (file_with_path);
}

gboolean
gossip_app_is_window_visible (void)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	return gossip_window_get_is_visible (GTK_WINDOW (priv->window));
}

void
gossip_app_toggle_visibility (void)
{
	GossipAppPriv *priv;
	gboolean       visible;

	priv = GET_PRIV (app);

	visible = gossip_window_get_is_visible (GTK_WINDOW (priv->window));

	if (visible && gossip_have_tray ()) {
		gint x, y, w, h;

		gtk_window_get_size (GTK_WINDOW (priv->window), &w, &h);
		gtk_window_get_position (GTK_WINDOW (priv->window), &x, &y);
		
		gossip_geometry_save_for_main_window (x, y, w, h);

		if (priv->size_timeout_id) {
			g_source_remove (priv->size_timeout_id);
			priv->size_timeout_id = 0;
		}

		gtk_widget_hide (priv->window);
		
		gossip_conf_set_bool (gossip_conf_get (),
				       GOSSIP_PREFS_UI_MAIN_WINDOW_HIDDEN, TRUE);
	} else {
		gint x, y, w, h;

		gossip_geometry_load_for_main_window (&x, &y, &w, &h);

		if (w >= 1 && h >= 1) {
			/* Use the defaults from the glade file if we
			 * don't have good w, h geometry.  
			 */
			gtk_window_set_default_size (GTK_WINDOW (priv->window), w, h);
		}
		
		if (x >= 1 && y >= 1) {
			/* Let the window manager position it if we
			 * don't have good x, y coordinates.  
			 */
			gtk_window_move (GTK_WINDOW (priv->window), x, y);
		}

		gossip_window_present (GTK_WINDOW (priv->window));

		gossip_conf_set_bool (gossip_conf_get (),
				       GOSSIP_PREFS_UI_MAIN_WINDOW_HIDDEN, FALSE);
	}
}

void
gossip_app_set_visibility (gboolean visible)
{
	GtkWidget *window;

	window = gossip_app_get_window ();
	
	if (visible) {
		gossip_window_present (GTK_WINDOW (window));
	} else {
		gtk_widget_hide (window);
	}
}

static void
app_show_hide_list_cb (GtkWidget *widget,
		       GossipApp *app)
{
	gossip_app_toggle_visibility ();
}

static void
app_popup_new_message_cb (GtkWidget *widget,
			  gpointer   user_data)
{
	gossip_new_message_dialog_show (NULL);
}

static gboolean
app_tray_destroy_cb (GtkWidget *widget,
		     gpointer   user_data)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);
	
	gtk_widget_destroy (GTK_WIDGET (priv->tray_icon));
	priv->tray_icon = NULL;
	priv->tray_event_box = NULL;
	priv->tray_image = NULL;

	if (priv->tray_flash_timeout_id) {
		g_source_remove (priv->tray_flash_timeout_id);
		priv->tray_flash_timeout_id = 0;
	}
	
	app_tray_create ();
	
	/* Show the window in case the notification area was removed */
	if (!gossip_have_tray ()) {
		gtk_widget_show (priv->window);
	}

	return TRUE;
}

static gboolean
app_tray_button_press_cb (GtkWidget      *widget, 
			  GdkEventButton *button_event, 
			  GossipApp      *app)
{
	GossipAppPriv *priv;
	GtkWidget     *submenu;

	priv = GET_PRIV (app);

	if (button_event->type == GDK_2BUTTON_PRESS ||
	    button_event->type == GDK_3BUTTON_PRESS) {
		return FALSE;
	}
	
	if (button_event->button != 1 && 
	    button_event->button != 3) {
		return FALSE;
	}

	if (button_event->button == 1) {
		GossipEvent *event;

		if (!priv->tray_flash_icons) {
			gossip_app_toggle_visibility ();
			return TRUE;
		}
		
		event = priv->tray_flash_icons->data;
		gossip_event_manager_activate (priv->event_manager, event);
	} else if (button_event->button == 3) {
		gboolean show;

		show = gossip_window_get_is_visible (GTK_WINDOW (priv->window));
		
		g_signal_handlers_block_by_func (priv->popup_menu_show_list_item, 
						 app_show_hide_list_cb, app);
		gtk_check_menu_item_set_active 
			(GTK_CHECK_MENU_ITEM (priv->popup_menu_show_list_item), show);
		g_signal_handlers_unblock_by_func (priv->popup_menu_show_list_item, 
						   app_show_hide_list_cb, app);

		submenu = gossip_presence_chooser_create_menu (
			GOSSIP_PRESENCE_CHOOSER (priv->presence_chooser));
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (priv->popup_menu_status_item),
					   submenu);
		
                gtk_menu_popup (GTK_MENU (priv->popup_menu), 
				NULL, NULL, NULL, NULL, 
				button_event->button, 
				button_event->time);
	}

	return TRUE;
}

static void
app_tray_create_menu (void)
{
	GossipAppPriv *priv;
	GladeXML      *glade;
	GtkWidget     *message_item;

	priv = GET_PRIV (app);

	glade = gossip_glade_get_file ("main.glade",
				       "tray_menu",
				       NULL,
				       "tray_menu", &priv->popup_menu,
				       "tray_show_list", &priv->popup_menu_show_list_item,
				       "tray_new_message", &message_item,
				       "tray_status", &priv->popup_menu_status_item,
				       NULL);

	gossip_glade_connect (glade,
			      app,
			      "tray_new_message", "activate", app_popup_new_message_cb,
			      "tray_quit", "activate", app_quit_cb,
			      NULL);

	g_signal_connect (priv->popup_menu_show_list_item, "toggled", 
			  G_CALLBACK (app_show_hide_list_cb), app);

	priv->widgets_connected = g_list_prepend (priv->widgets_connected,
						  priv->popup_menu_status_item);
	
	priv->widgets_connected = g_list_prepend (priv->widgets_connected,
						  message_item);
	
	g_object_unref (glade);
}

static void
app_tray_create (void)
{
#ifdef HAVE_GNOME
	GossipAppPriv *priv;
	GdkPixbuf     *pixbuf;
	gchar         *name;

	priv = GET_PRIV (app);

	name = g_strdup_printf ("%s, Instant Messaging Client", PACKAGE_NAME);
	priv->tray_icon = GTK_WIDGET (egg_tray_icon_new (name));
	g_free (name);

	priv->tray_event_box = gtk_event_box_new ();
	priv->tray_image = gtk_image_new ();
	
	pixbuf = app_get_current_status_pixbuf ();
	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->tray_image), pixbuf);
	g_object_unref (pixbuf);
	
	gtk_container_add (GTK_CONTAINER (priv->tray_event_box),
			   priv->tray_image);

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

#ifdef HAVE_LIBNOTIFY
	gossip_notify_set_attach_widget (GTK_WIDGET (priv->tray_icon));
#endif

	app_tray_update_tooltip ();
#else
	if (0) {
		app_tray_button_press_cb (NULL, NULL, NULL);
	}
#endif
}

static void
app_tray_update_tooltip (void)
{
	GossipAppPriv *priv;
	GossipEvent   *event;

	priv = GET_PRIV (app);

	if (!priv->tray_flash_icons) {
		const gchar *status;

		if (gossip_app_is_connected ()) {
			GossipPresence      *presence;
			GossipPresenceState  state;
			
			presence = app_get_effective_presence ();
			state = gossip_presence_get_state (presence);
			status = gossip_presence_get_status (presence);
			
			if (!status) {
				status = gossip_presence_state_get_default_status (state);
			}
		} else {
			status = _("Disconnected");
		}

		if (priv->tray_event_box) {
			gtk_tooltips_set_tip (GTK_TOOLTIPS (priv->tooltips),
					      priv->tray_event_box, status, status);
		}
		
		return;
	}

	event = priv->tray_flash_icons->data;

	if (priv->tray_event_box) {
		gtk_tooltips_set_tip (GTK_TOOLTIPS (priv->tooltips),
				      priv->tray_event_box,
				      gossip_event_get_message (event),
				      gossip_event_get_message (event));
	}	
}

static void
app_status_flash_start (void)
{
	GossipAppPriv *priv;
	
	priv = GET_PRIV (app);

	gossip_presence_chooser_flash_start (GOSSIP_PRESENCE_CHOOSER (priv->presence_chooser),
					     app_get_current_state (),
					     app_get_previous_state ());
	
	app_tray_flash_start ();
}

static void
app_status_flash_stop (void)
{
	GossipAppPriv *priv;
	
	priv = GET_PRIV (app);
	
	gossip_presence_chooser_flash_stop (GOSSIP_PRESENCE_CHOOSER (priv->presence_chooser),
					    app_get_current_state ());
	
	app_tray_flash_maybe_stop ();
}

static void
app_notify_show_offline_cb (GossipConf  *conf,
			    const gchar *key,
			    gpointer     check_menu_item)
{
	gboolean show_offline;

	if (gossip_conf_get_bool (conf, key, &show_offline)) {
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (check_menu_item),
						show_offline);
	}
}

static void
app_notify_show_avatars_cb (GossipConf  *conf,
			    const gchar *key,
			    gpointer     user_data)
{
	GossipAppPriv *priv;
	gboolean       show_avatars;

	priv = GET_PRIV (app);

	if (gossip_conf_get_bool (conf, key, &show_avatars)) {
		gossip_contact_list_set_show_avatars (priv->contact_list, show_avatars);
	}
}

static gboolean
app_idle_check_cb (GossipApp *app)
{
	GossipAppPriv       *priv;
	gint32               idle;
	GossipPresence      *presence;
	GossipPresenceState  state;
	gboolean             presence_changed = FALSE;

	priv = GET_PRIV (app);

	if (!gossip_app_is_connected ()) {
		return TRUE;
	}
	
	idle = gossip_idle_get_seconds ();
	presence = app_get_effective_presence ();
	state = gossip_presence_get_state (presence);

	/* gossip_debug (DEBUG_DOMAIN_IDLE, "Idle for:%d", idle); */

	/* We're going away, allow some slack. */
	if (priv->leave_time > 0) {
		if (time (NULL) - priv->leave_time > LEAVE_SLACK) {
			priv->leave_time = 0;
			app_status_flash_stop ();

			gossip_idle_reset ();
			gossip_debug (DEBUG_DOMAIN_IDLE, "OK, away now.");
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
		gossip_debug (DEBUG_DOMAIN_IDLE, "Going to ext away...");
		gossip_presence_set_state (priv->away_presence, 
					   GOSSIP_PRESENCE_STATE_EXT_AWAY);
		presence_changed = TRUE;
	}
	else if (state != GOSSIP_PRESENCE_STATE_AWAY && 
		 state != GOSSIP_PRESENCE_STATE_EXT_AWAY &&
		 idle > AWAY_TIME) {
		gossip_debug (DEBUG_DOMAIN_IDLE, "Going to away...");
		app_set_away (NULL);
		presence_changed = TRUE;
	}
	else if (state == GOSSIP_PRESENCE_STATE_AWAY ||
		 state == GOSSIP_PRESENCE_STATE_EXT_AWAY) {
		/* Allow some slack before returning from away. */
		if (idle >= -BACK_SLACK && idle <= 0) {
			gossip_debug (DEBUG_DOMAIN_IDLE, "Slack, do nothing.");
			app_status_flash_start ();
		}
		else if (idle < -BACK_SLACK) {
			gossip_debug (DEBUG_DOMAIN_IDLE, "No more slack, break interrupted.");
			app_status_clear_away ();
			return TRUE;
		}
		else if (idle > BACK_SLACK) {
			gossip_debug (DEBUG_DOMAIN_IDLE, "Don't interrupt break.");
			app_status_flash_stop ();
		}
	}

	if (presence_changed) {
		app_presence_updated ();
	}

	return TRUE;
}

/*
 * App 
 */

static void
app_connect_no_accounts_response_cb (GtkWidget *widget,
				     gint       response,
				     gpointer   user_data)
{
	gtk_widget_destroy (widget);
	gossip_accounts_dialog_show (NULL);
}

void
gossip_app_connect (GossipAccount *account,
		    gboolean       startup)
{
	GossipAppPriv        *priv;
	GossipAccountManager *manager;

	priv = GET_PRIV (app);

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

		g_signal_connect (dialog, "response", 
				  G_CALLBACK (app_connect_no_accounts_response_cb),
				  NULL);
		
		gtk_widget_show (dialog);
		return;
	}

#ifdef HAVE_DBUS
	if (startup) {
		gboolean connected = TRUE;

		/* Don't try to automatically connect if we have Network
		 * Manager state and we are NOT connected. 
		 */

		if (gossip_dbus_nm_get_state (&connected) && !connected) {
			return;
		}
	}
#endif

	gossip_session_connect (priv->session, account, startup);
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

	priv = GET_PRIV (app);

	/* Disconnect all and store a list. */
	accounts = gossip_session_get_accounts (priv->session);
	for (l = accounts; l; l = l->next) {
		GossipAccount *account = l->data;

		if (!gossip_session_is_connected (priv->session, account)) {
			continue;
		}

		tmp_account_list = g_slist_prepend (tmp_account_list,
						    g_object_ref (account));

		gossip_session_disconnect (priv->session, account);
	}

	g_list_foreach (accounts, (GFunc) g_object_unref, NULL);
	g_list_free (accounts);
}

void
gossip_app_net_up (void)
{
	GossipAppPriv *priv;
	GSList        *l;

	priv = GET_PRIV (app);

	/* Connect all that went down before. */
	if (!tmp_account_list) {
		/* If no previous disconnect then we just 
		 * connect the default connect on startup accounts.
		 */
		gossip_app_connect (NULL, TRUE);
		return;
	}

	for (l = tmp_account_list; l; l = l->next) {
		GossipAccount *account = l->data;
		
		gossip_session_connect (priv->session, account, FALSE);
		g_object_unref (account);
	}

	g_slist_free (tmp_account_list);
	tmp_account_list = NULL;
}

void
gossip_app_create (GossipSession        *session,
		   GossipAccountManager *manager)
		   
{
	g_return_if_fail (GOSSIP_IS_SESSION (session));
	g_return_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager));
	
	g_object_new (GOSSIP_TYPE_APP, NULL);
	
	app_setup (session, manager);
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
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_session_disconnect (priv->session, NULL);
}

static void
app_connection_items_setup (GladeXML *glade)
{
	GossipAppPriv *priv;

	const gchar   *widgets_connected[] = {
		"actions_disconnect",
		"actions_group_chat",
		"actions_new_message",
		"actions_add_contact",
		"edit_personal_information"
	};

	const gchar   *widgets_disconnected[] = {
		"actions_connect"
	};

	GList         *list;
	GtkWidget     *w;
	gint           i;

	priv = GET_PRIV (app);

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
	guint          connected = 0;
	guint          disconnected = 0;

	priv = GET_PRIV (app);

	/* Get account count for: 
	 *  - connected and disabled, 
	 *  - connected and enabled 
	 *  - disabled and enabled 
	 */
	gossip_session_count_accounts (priv->session,
				       &connected, 
				       NULL,
				       &disconnected);

	for (l = priv->widgets_connected; l; l = l->next) {
		gtk_widget_set_sensitive (l->data, (connected > 0));
	}

	for (l = priv->widgets_disconnected; l; l = l->next) {
		gtk_widget_set_sensitive (l->data, (disconnected > 0));
	}

	if (connected == 0) {
		app_status_flash_stop ();
	}
}

gboolean
gossip_app_is_connected (void)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	return gossip_session_is_connected (priv->session, NULL);
}

/*
 * Toolbar for accounts
 */

static void
app_accounts_error_edit_clicked_cb (GtkButton     *button,
				    GossipAccount *account)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_accounts_dialog_show (account);
	g_hash_table_remove (priv->errors, account);
}

static void
app_accounts_error_clear_clicked_cb (GtkButton     *button,
				     GossipAccount *account)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	g_hash_table_remove (priv->errors, account);
}

static void
app_accounts_error_display (GossipAccount *account,
			    GError        *error)
{
	GossipAppPriv *priv;
	GtkWidget     *child;
	GtkWidget     *table;
	GtkWidget     *image;
	GtkWidget     *button_edit;
	GtkWidget     *alignment;
	GtkWidget     *hbox;
	GtkWidget     *label;
	GtkWidget     *fixed;
	GtkWidget     *vbox;
	GtkWidget     *button_close;
	gchar         *str;

	priv = GET_PRIV (app);

	child = g_hash_table_lookup (priv->errors, account);
	if (child) {
		label = g_object_get_data (G_OBJECT (child), "label");

		/* Just set the latest error and return */
		str = g_markup_printf_escaped ("<b>%s</b>\n%s", 
					       gossip_account_get_name (account), 
					       error->message);
		gtk_label_set_markup (GTK_LABEL (label), str);
		g_free (str);
	
		return;
	}
	
	child = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (priv->errors_vbox), child, FALSE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (child), 6);
	gtk_widget_show (child);

	table = gtk_table_new (2, 4, FALSE);
	gtk_widget_show (table);
	gtk_box_pack_start (GTK_BOX (child), table, TRUE, TRUE, 0);
	gtk_table_set_row_spacings (GTK_TABLE (table), 12);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);
	
	image = gtk_image_new_from_stock (GTK_STOCK_DISCONNECT, GTK_ICON_SIZE_MENU);
	gtk_widget_show (image);
	gtk_table_attach (GTK_TABLE (table), image, 0, 1, 0, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0);
	
	button_edit = gtk_button_new ();
	gtk_widget_show (button_edit);
	gtk_table_attach (GTK_TABLE (table), button_edit, 1, 2, 1, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	
	alignment = gtk_alignment_new (0.5, 0.5, 0, 0);
	gtk_widget_show (alignment);
	gtk_container_add (GTK_CONTAINER (button_edit), alignment);
	
	hbox = gtk_hbox_new (FALSE, 2);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (alignment), hbox);
	
	image = gtk_image_new_from_stock (GTK_STOCK_EDIT, GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
	
	label = gtk_label_new_with_mnemonic (_("Edit Account _Details"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	
	fixed = gtk_fixed_new ();
	gtk_widget_show (fixed);
	gtk_table_attach (GTK_TABLE (table), fixed, 2, 3, 1, 2,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);
	
	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_table_attach (GTK_TABLE (table), vbox, 3, 4, 0, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);
	
	button_close = gtk_button_new ();
	gtk_widget_show (button_close);
	gtk_box_pack_start (GTK_BOX (vbox), button_close, FALSE, FALSE, 0);
	gtk_button_set_relief (GTK_BUTTON (button_close), GTK_RELIEF_NONE);

	
	image = gtk_image_new_from_stock ("gtk-close", GTK_ICON_SIZE_MENU);
	gtk_widget_show (image);
	gtk_container_add (GTK_CONTAINER (button_close), image);
	
	label = gtk_label_new ("");
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 1, 3, 0, 1,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
			  (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL), 0, 0);
	gtk_widget_set_size_request (label, 175, -1);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0);

	str = g_markup_printf_escaped ("<b>%s</b>\n%s", 
				       gossip_account_get_name (account), 
				       error->message);
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);

	g_object_set_data (G_OBJECT (child), "label", label);

	g_signal_connect_object (button_edit, "clicked", 
				 G_CALLBACK (app_accounts_error_edit_clicked_cb),
				 account, 0);

	g_signal_connect_object (button_close, "clicked", 
				 G_CALLBACK (app_accounts_error_clear_clicked_cb),
				 account, 0);

	gtk_widget_show (priv->errors_vbox);

	g_hash_table_insert (priv->errors, g_object_ref (account), child);
}

static GossipPresence *
app_get_effective_presence (void)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	if (priv->away_presence) {
		return priv->away_presence;
	}

	return priv->presence;
}

static void
app_set_away (const gchar *status)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

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

static GossipPresenceState
app_get_current_state (void)
{
        GossipAppPriv  *priv;
        GossipPresence *presence;

	priv = GET_PRIV (app);
	
	if (!gossip_session_is_connected (priv->session, NULL)) {
		return GOSSIP_PRESENCE_STATE_UNAVAILABLE;
	}
	
	presence = app_get_effective_presence ();
	return gossip_presence_get_state (presence);
}

static GossipPresenceState
app_get_previous_state (void)
{
	GossipAppPriv *priv;
	
	priv = GET_PRIV (app);

	if (!gossip_session_is_connected (priv->session, NULL)) {
		return GOSSIP_PRESENCE_STATE_UNAVAILABLE;
	}
	
	return gossip_presence_get_state (priv->presence);
}

static GdkPixbuf *
app_get_current_status_pixbuf (void)
{
	GossipAppPriv  *priv;
	GossipPresence *presence;

	priv = GET_PRIV (app);

	if (!gossip_session_is_connected (priv->session, NULL)) {
		return gossip_pixbuf_from_stock (GOSSIP_STOCK_OFFLINE, 
						 GTK_ICON_SIZE_MENU);
	}
	
	presence = app_get_effective_presence ();
	return gossip_pixbuf_for_presence (presence);
}

static void
app_presence_updated (void)
{
	GossipAppPriv       *priv;
	GdkPixbuf           *pixbuf;
	GossipPresence      *presence;
	GossipPresenceState  state;
	const gchar         *status;
	
	priv = GET_PRIV (app);

	if (priv->tray_image) {
		pixbuf = app_get_current_status_pixbuf ();
		gtk_image_set_from_pixbuf (GTK_IMAGE (priv->tray_image), pixbuf);
		g_object_unref (pixbuf);
	}
	
	if (!gossip_session_is_connected (priv->session, NULL)) {
		gossip_presence_chooser_set_status (
			GOSSIP_PRESENCE_CHOOSER (priv->presence_chooser),
			_("Offline"));
		return;
	}

	presence = app_get_effective_presence ();
	state = gossip_presence_get_state (presence);
	status = gossip_presence_get_status (presence);

	if (!status) {
		status = gossip_presence_state_get_default_status (state);
	}

	gossip_presence_chooser_set_state 
		(GOSSIP_PRESENCE_CHOOSER (priv->presence_chooser), state);
	gossip_presence_chooser_set_status
		(GOSSIP_PRESENCE_CHOOSER (priv->presence_chooser), status);
		
	gossip_session_set_presence (priv->session, presence);

	app_tray_update_tooltip ();
}

/* clears status data from autoaway mode */ 
static void
app_status_clear_away (void)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);
	
	if (priv->away_presence) {
		g_object_unref (priv->away_presence);
		priv->away_presence = NULL;
	}

	priv->leave_time = 0;
	app_status_flash_stop ();
	
	/* Force this so we don't get a delay in the display */
	app_presence_updated ();
}

void
gossip_app_set_not_away (void)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	/* If we just left, allow some slack. */
	if (priv->leave_time) {
		return;
	}
	
	if (priv->away_presence) {
		app_status_clear_away ();
	}
}

void
gossip_app_set_presence (GossipPresenceState  state,
			 const gchar         *status)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	if (state != GOSSIP_PRESENCE_STATE_AWAY) {
		const gchar *default_status;

		/* Send NULL if it's not changed from default status string. We
		 * do this so that the translated default strings will work
		 * across two Gossips.
		 */
		default_status = gossip_presence_state_get_default_status (state);

		if (status && strcmp (status, default_status) == 0) {
			g_object_set (priv->presence, "status", NULL, NULL);
		} else {
			g_object_set (priv->presence, "status", status, NULL);
		}
		
		g_object_set (priv->presence, "state", state, NULL);

		app_status_flash_stop ();
		app_status_clear_away ();
	} else {
		app_status_flash_start ();
		app_set_away (status);
		app_presence_updated ();
	}
}

static void
app_presence_chooser_changed_cb (GtkWidget           *chooser,
				 GossipPresenceState  state,
				 const gchar         *status,
				 gpointer             user_data)
{
	gossip_app_set_presence (state, status);
	gossip_status_presets_set_default (state, status);
}

static gboolean
configure_event_timeout_cb (GtkWidget *widget)
{
	GossipAppPriv *priv;
	gint           x, y, w, h;
	
	priv = GET_PRIV (app);

	gtk_window_get_size (GTK_WINDOW (widget), &w, &h);
	gtk_window_get_position (GTK_WINDOW (widget), &x, &y);

	gossip_geometry_save_for_main_window (x, y, w, h);

	priv->size_timeout_id = 0;

	return FALSE;
}

static gboolean
app_window_configure_event_cb (GtkWidget         *widget,
			       GdkEventConfigure *event,
			       GossipApp         *app)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	if (priv->size_timeout_id) {
		g_source_remove (priv->size_timeout_id);
	}
	
	priv->size_timeout_id = g_timeout_add (500,
					       (GSourceFunc) configure_event_timeout_cb,
					       widget);

	return FALSE;
}

GtkWidget *
gossip_app_get_window (void)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	return priv->window;
}

static gboolean
app_tray_flash_timeout_func (gpointer data)
{
	GossipAppPriv   *priv;
	GdkPixbuf       *pixbuf = NULL;
	gboolean         is_flashing;
	static gboolean  on = FALSE;

	priv = GET_PRIV (app);

	if (!priv->tray_image) {
		return TRUE;
	}
	
	is_flashing = gossip_presence_chooser_is_flashing
		(GOSSIP_PRESENCE_CHOOSER (priv->presence_chooser));

	if (on) {
		if (is_flashing) {
			pixbuf = gossip_pixbuf_for_presence (priv->presence);
		}
		else if (priv->tray_flash_icons != NULL) {
			GossipEvent *event;
			const gchar *stock_id = NULL;

			event = priv->tray_flash_icons->data;
			switch (gossip_event_get_type (event)) {
			case GOSSIP_EVENT_NEW_MESSAGE:
				stock_id = GOSSIP_STOCK_MESSAGE;
				break;
			case GOSSIP_EVENT_SUBSCRIPTION_REQUEST:
				stock_id = GTK_STOCK_DIALOG_QUESTION;
				break;

			default:
				break;
			}
			
			if (stock_id) {
				pixbuf = gossip_pixbuf_from_stock (stock_id, 
								   GTK_ICON_SIZE_MENU);
			}
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
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	if (!priv->tray_flash_timeout_id) {
		priv->tray_flash_timeout_id = g_timeout_add (FLASH_TIMEOUT,
							     app_tray_flash_timeout_func,
							     NULL);
	}
}

/* Stop if there are no flashing messages or status change. */
static void
app_tray_flash_maybe_stop (void)
{
	GossipAppPriv *priv;
	GdkPixbuf     *pixbuf;

	priv = GET_PRIV (app);

	if (priv->tray_flash_icons != NULL || priv->leave_time > 0) {
		return;
	}

	if (priv->tray_image) {
		pixbuf = app_get_current_status_pixbuf ();
		gtk_image_set_from_pixbuf (GTK_IMAGE (priv->tray_image), pixbuf);
		g_object_unref (pixbuf);
	}
	
	if (priv->tray_flash_timeout_id) {
		g_source_remove (priv->tray_flash_timeout_id);
		priv->tray_flash_timeout_id = 0;
	}
}

static void
app_chatroom_auto_connect_update_cb (GossipChatroomManager    *manager,
				     GossipChatroomProvider   *provider,
				     GossipChatroom           *chatroom,
				     GossipChatroomJoinResult  result,
				     gpointer                  user_data)
{
	const gchar *name;

	name = gossip_chatroom_get_name (chatroom);

	switch (result) {
	case GOSSIP_CHATROOM_JOIN_NICK_IN_USE:
	case GOSSIP_CHATROOM_JOIN_NEED_PASSWORD:
	case GOSSIP_CHATROOM_JOIN_TIMED_OUT:
	case GOSSIP_CHATROOM_JOIN_UNKNOWN_HOST:
	case GOSSIP_CHATROOM_JOIN_UNKNOWN_ERROR:
		gossip_debug (DEBUG_DOMAIN_CHATROOMS, "Auto connect update: failed for room:'%s'", name);
		gossip_chatrooms_window_show (NULL, TRUE);
		break;

	case GOSSIP_CHATROOM_JOIN_OK:
	case GOSSIP_CHATROOM_JOIN_ALREADY_OPEN:
		gossip_debug (DEBUG_DOMAIN_CHATROOMS, "Auto connect update: success for room:'%s'", name);
		gossip_group_chat_new (provider, 
				       gossip_chatroom_get_id (chatroom));
		break;

 	case GOSSIP_CHATROOM_JOIN_CANCELED:
		break;
	}
}

static void
app_chatroom_favourite_update (void)
{
	GossipAppPriv *priv;
	GList         *accounts;
	GList         *chatrooms, *l;
	gboolean       found;
		
	priv = GET_PRIV (app);

	accounts = gossip_session_get_accounts (priv->session);
	if (!accounts) {
		gtk_widget_set_sensitive (priv->actions_group_chat_join, FALSE);
		return;
	}

	g_list_foreach (accounts, (GFunc) g_object_unref, NULL);
	g_list_free (accounts);

	chatrooms = gossip_chatroom_manager_get_chatrooms (priv->chatroom_manager, NULL);
	found = FALSE;
	
	for (l = chatrooms; l; l = l->next) {
		GossipChatroom *chatroom;
		GossipAccount  *account;

		chatroom = l->data;
		if (!gossip_chatroom_get_is_favourite (chatroom)) {
 			continue;
		}

		account = gossip_chatroom_get_account (chatroom);
		if (!gossip_session_is_connected (priv->session, account)) {
			continue;
		}
		
		/* If we find at least ONE chatroom that is a
		 * favourite on an account that _IS_ connected, we
		 * make the join favourites option available.
		 */
		found = TRUE;
		break;
	}

	gtk_widget_set_sensitive (priv->actions_group_chat_join, found);

	g_list_free (chatrooms);
}

static void
app_chatroom_favourite_update_cb (GossipChatroomManager *manager,
				  GossipChatroom        *chatroom,
				  gpointer               user_data)
{
	app_chatroom_favourite_update ();
}

static void
app_event_added_cb (GossipEventManager *manager,
		    GossipEvent        *event,
		    gpointer            user_data)
{
	GossipAppPriv *priv;
	GList         *l;
		
	priv = GET_PRIV (app);
	
	gossip_request_user_attention ();
	
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
		      gpointer            user_data)
{
	GossipAppPriv *priv;
	GList         *l;

	priv = GET_PRIV (app);

	l = g_list_find_custom (priv->tray_flash_icons, event,
				gossip_event_compare);

	if (!l) {
		/* Not flashing this event */
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
			  GossipEventId      event_id,
			  gpointer           user_data)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	if (event_id > 0) {
		gossip_event_manager_activate_by_id (priv->event_manager, event_id);
		return;
	}

	gossip_chat_manager_show_chat (priv->chat_manager, contact);
}

GossipSession *
gossip_app_get_session (void)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	return priv->session;
}

GossipChatroomManager *
gossip_app_get_chatroom_manager (void)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	return priv->chatroom_manager;
}

GossipChatManager *
gossip_app_get_chat_manager (void)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	return priv->chat_manager;
}

GossipEventManager *
gossip_app_get_event_manager (void)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	return priv->event_manager;
}

