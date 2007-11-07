/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2007 Imendio AB
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

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libgossip/gossip-chatroom.h>
#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-event.h>
#include <libgossip/gossip-presence.h>
#include <libgossip/gossip-chatroom-provider.h>
#include <libgossip/gossip-account-manager.h>
#include <libgossip/gossip-conf.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-stock.h>
#include <libgossip/gossip-jabber.h>
#include <libgossip/gossip-jabber-utils.h>

#include "gossip-about-dialog.h"
#include "gossip-accounts-dialog.h"
#include "gossip-add-contact-dialog.h"
#include "gossip-app.h"
#include "gossip-chat.h"
#include "gossip-chatrooms-window.h"
#include "gossip-contact-list.h"
#include "gossip-glade.h"
#include "gossip-self-presence.h"
#include "gossip-ft-dialog.h"
#include "gossip-geometry.h"
#include "gossip-group-chat.h"
#include "gossip-hint.h"
#include "gossip-idle.h"
#include "gossip-log-window.h"
#include "gossip-new-chatroom-dialog.h"
#include "gossip-new-message-dialog.h"
#include "gossip-preferences.h"
#include "gossip-presence-chooser.h"
#include "gossip-private-chat.h"
#include "gossip-sound.h"
#include "gossip-status-icon.h"
#include "gossip-status-presets.h"
#include "gossip-subscription-dialog.h"
#include "gossip-ui-utils.h"
#include "gossip-vcard-dialog.h"
#include "ephy-spinner.h"

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
#define DEBUG_DOMAIN_ACCOUNTS  "AppAccounts"
#define DEBUG_DOMAIN_CHATROOMS "AppChatrooms"
#define DEBUG_DOMAIN_TRAY      "AppTray"
#define DEBUG_DOMAIN_SESSION   "AppSession"

#define DEBUG_QUIT

/* Flashing delay for icons (milliseconds). */
#define FLASH_TIMEOUT 500

/* Minimum width of roster window if something goes wrong. */
#define MIN_WIDTH 50

/* Delay for retry to connect when disconnected (seconds) */
#define RETRY_CONNECT_TIMEOUT 20

struct _GossipAppPriv {
	GossipSession         *session;

	GossipChatroomManager *chatroom_manager;

	GossipChatManager     *chat_manager;
	GossipEventManager    *event_manager;

	GossipContactList     *contact_list;

	/* Main widgets */
	GtkWidget             *window;
	GtkWidget             *main_vbox;
	GtkWidget             *find_hbox;
	GtkWidget             *find_entry;
	GtkWidget             *errors_vbox;
	GHashTable            *errors;
	GHashTable            *reconnects;

	/* Tooltips for all widgets */
	GtkTooltips           *tooltips;

	/* Menu widgets */
	GtkWidget             *chat_connect;
	GtkWidget             *chat_disconnect;
	GtkWidget             *chat_search;
	GtkWidget             *chat_context;
	GtkWidget             *chat_context_separator;
	GtkWidget             *room;
	GtkWidget             *room_menu;
	GtkWidget             *room_sep;
	GtkWidget             *room_join_favorites;

	/* Status Icon */
	GtkStatusIcon         *status_icon;

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

	GossipSelfPresence    *self_presence;
	GossipHeartbeat       *flash_heartbeat;

	/* Misc */
	guint                  size_timeout_id;
};

static void     app_finalize                 (GObject               *object);
static void     app_setup                    (GossipSession         *session);
static gboolean app_main_window_quit_confirm (GossipApp             *app,
					      GtkWidget             *window);
static void     
app_main_window_quit_confirm_cb              (GtkWidget             *dialog,
					      gint                   response,
					      GossipApp             *app);
static void     app_main_window_destroy_cb   (GtkWidget             *window,
					      GossipApp             *app);
static gboolean 
app_main_window_delete_event_cb              (GtkWidget             *window,
					      GdkEvent              *event,
					      GossipApp             *app);
static gboolean 
app_main_window_key_press_event_cb           (GtkWidget             *window,
					      GdkEventKey           *event,
					      GossipApp             *app);
static void     app_find_entry_changed_cb    (GtkEditable           *editable,
					      GossipApp             *app);
static void     
app_favorite_chatroom_menu_activate_cb       (GtkMenuItem           *menu_item,
					      GossipChatroom        *chatroom);
static void     
app_favorite_chatroom_menu_update            (void);
static gboolean 
app_favorite_chatroom_menu_add               (GossipChatroom        *chatroom);
static void     
app_favorite_chatroom_menu_update_cb         (GossipChatroomManager *manager,
					      GossipChatroom        *chatroom,
					      gpointer               user_data);
static void
app_favorite_chatroom_menu_name_cb           (GossipChatroom        *chatroom,
					      GParamSpec            *spec,
					      GtkWidget             *menu_item);
static void     
app_favorite_chatroom_menu_added_cb          (GossipChatroomManager *manager,
					      GossipChatroom        *chatroom,
					      gpointer               user_data);
static void     
app_favorite_chatroom_menu_removed_cb        (GossipChatroomManager *manager,
					      GossipChatroom        *chatroom,
					      gpointer               user_data);
static void     
app_favorite_chatroom_menu_setup             (void);
static void     app_chat_quit_cb             (GtkWidget             *window,
					      GossipApp             *app);
static void     app_chat_connect_cb          (GtkWidget             *window,
					      GossipApp             *app);
static void     app_chat_disconnect_cb       (GtkWidget             *window,
					      GossipApp             *app);
static void     app_chat_search_cb           (GtkWidget             *window,
					      GossipApp             *app);
static void     app_chat_new_message_cb      (GtkWidget             *widget,
					      GossipApp             *app);
static void     app_chat_history_cb          (GtkWidget             *window,
					      GossipApp             *app);
static void     app_room_join_new_cb         (GtkWidget             *window,
					      GossipApp             *app);
static void     app_room_join_favorites_cb   (GtkWidget             *window,
					      GossipApp             *app);
static void     app_room_manage_favorites_cb (GtkWidget             *window,
					      GossipApp             *app);
static void     app_chat_add_contact_cb      (GtkWidget             *widget,
					      GossipApp             *app);
static void     app_chat_show_offline_cb     (GtkCheckMenuItem      *item,
					      GossipApp             *app);
static gboolean 
app_chat_button_press_event_cb               (GtkWidget             *widget,
					      GdkEventButton        *event,
					      GossipApp             *app);
static void     app_edit_accounts_cb         (GtkWidget             *widget,
					      GossipApp             *app);
static void     
app_edit_personal_information_cb             (GtkWidget             *widget,
					      GossipApp             *app);
static void     app_edit_preferences_cb      (GtkWidget             *widget,
					      GossipApp             *app);
static void     app_help_about_cb            (GtkWidget             *window,
					      GossipApp             *app);
static void     app_help_contents_cb         (GtkWidget             *window,
					      GossipApp             *app);
static gboolean 
app_throbber_button_press_event_cb           (GtkWidget             *throbber,
					      GdkEventButton        *event,
					      gpointer               user_data);
static void     
app_session_protocol_connecting_cb           (GossipSession         *session,
					      GossipAccount         *account,
					      GossipJabber          *jabber,
					      gpointer               user_data);
static void     
app_session_protocol_connected_cb            (GossipSession         *session,
					      GossipAccount         *account,
					      GossipJabber          *jabber,
					      gpointer               user_data);
static void     app_reconnect_remove         (gpointer               p);
static gboolean app_reconnect_cb             (GossipAccount         *account);
static void     
app_session_protocol_disconnected_cb         (GossipSession         *session,
					      GossipAccount         *account,
					      GossipJabber          *jabber,
					      gint                   reason,
					      gpointer               user_data);
static void     
app_session_protocol_error_cb                (GossipSession         *session,
					      GossipJabber          *jabber,
					      GossipAccount         *account,
					      GError                *error,
					      gpointer               user_data);
static void app_account_password_dialog_show (GossipAccount         *account);
static void     app_restore_user_accels      (void);
static void     app_save_user_accels         (void);
static void     app_show_hide_list_cb        (GtkWidget             *widget,
					      GossipApp             *app);
static void     app_popup_new_message_cb     (GtkWidget             *widget,
					      gpointer               user_data);
static void     
app_status_icon_popup_menu_cb                (GtkStatusIcon         *status_icon,
					      guint                  button,
					      guint                  activate_time,
					      GossipApp             *app);
static void     app_status_icon_create_menu  (void);
static void     app_status_icon_create       (void);
static gboolean 
app_status_icon_check_embedded_cb            (gpointer               user_data);
static void     app_notify_show_offline_cb   (GossipConf            *conf,
					      const gchar           *key,
					      gpointer               check_menu_item);
static void     app_notify_show_avatars_cb   (GossipConf            *conf,
					      const gchar           *key,
					      gpointer               user_data);
static void     app_notify_sort_criterium_cb (GossipConf            *conf,
					      const gchar           *key,
					      gpointer               user_data);
static void     
app_notify_compact_contact_list_cb           (GossipConf            *conf,
					      const gchar           *key,
					      gpointer               user_data);
static void     app_disconnect               (void);
static void     app_connection_items_setup   (GladeXML              *glade);
static void     app_connection_items_update  (void);
static void     
app_accounts_error_edit_clicked_cb           (GtkButton             *button,
					      GossipAccount         *account);
static void     
app_accounts_error_clear_clicked_cb          (GtkButton             *button,
					      GossipAccount         *account);
static void     app_accounts_error_display   (GossipAccount         *account,
					      GError                *error);
static void     app_presence_updated_cb      (GossipSelfPresence    *self_presence,
					      gpointer               user_data);

static void     
app_presence_chooser_changed_cb              (GtkWidget             *chooser,
					      GossipPresenceState    state,
					      const gchar           *status,
					      gpointer               user_data);
static gboolean configure_event_timeout_cb   (GtkWidget             *widget);
static gboolean 
app_window_configure_event_cb                (GtkWidget             *widget,
					      GdkEventConfigure     *event,
					      GossipApp             *app);
static void     
app_session_chatroom_auto_connect_cb         (GossipSession         *session,
					      GossipChatroomProvider *provider,
					      GossipChatroom        *chatroom,
					      gpointer               user_data);
static void     app_event_added_cb           (GossipEventManager    *manager,
					      GossipEvent           *event,
					      gpointer               user_data);
static void     app_contact_activated_cb     (GossipContactList     *contact_list,
					      GossipContact         *contact,
					      GossipEventId          event_id,
					      gpointer               user_data);

static GossipApp *app = NULL;

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

	priv->flash_heartbeat = g_object_new (GOSSIP_TYPE_HEARTBEAT,
					      "interval", FLASH_TIMEOUT,
					      NULL);

	priv->errors = g_hash_table_new_full (gossip_account_hash,
					      gossip_account_equal,
					      g_object_unref,
					      (GDestroyNotify) gtk_widget_destroy);

	priv->reconnects = g_hash_table_new_full (gossip_account_hash,
						  gossip_account_equal,
						  g_object_unref,
						  (GDestroyNotify) app_reconnect_remove);

	priv->self_presence = g_object_new (GOSSIP_TYPE_SELF_PRESENCE, NULL);

	g_signal_connect (priv->self_presence, "updated",
			  G_CALLBACK (app_presence_updated_cb),
			  NULL);

	priv->tooltips = g_object_ref (gtk_tooltips_new ());
	gtk_object_sink (GTK_OBJECT (priv->tooltips));
}

static void
app_finalize (GObject *object)
{
	GossipApp     *app;
	GossipAppPriv *priv;

	app = GOSSIP_APP (object);
	priv = GET_PRIV (app);

	if (priv->size_timeout_id) {
		g_source_remove (priv->size_timeout_id);
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

	if (priv->reconnects) {
		g_hash_table_destroy (priv->reconnects);
	}

	gtk_widget_destroy (priv->popup_menu);

	g_object_unref (priv->tooltips);

	gossip_conf_shutdown ();

#ifdef HAVE_LIBNOTIFY
	gossip_notify_finalize ();
#endif

	gossip_sound_finalize ();

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
					      app_session_chatroom_auto_connect_cb,
					      NULL);

	g_signal_handlers_disconnect_by_func (priv->event_manager,
					      app_event_added_cb,
					      NULL);

	gossip_ft_dialog_finalize (priv->session);
	gossip_subscription_dialog_finalize (priv->session);
	
	g_object_unref (priv->self_presence);

	g_object_unref (priv->event_manager);
	g_object_unref (priv->chat_manager);
	g_object_unref (priv->chatroom_manager);
	g_object_unref (priv->session);
	g_object_unref (priv->flash_heartbeat);

	G_OBJECT_CLASS (gossip_app_parent_class)->finalize (object);
}

static void
app_setup_presence_chooser ()
{
	GossipAppPriv *priv;
	GtkToolItem   *item;

	priv = GET_PRIV (app);

	priv->presence_chooser = gossip_presence_chooser_new ();
	gtk_widget_show (priv->presence_chooser);

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
}

static void
app_setup_throbber (void)
{
	GossipAppPriv *priv;
	GtkWidget     *ebox;
	GtkToolItem   *item;
	gchar         *str;

	priv = GET_PRIV (app);

	ebox = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (ebox), FALSE);

	priv->throbber = ephy_spinner_new ();
	ephy_spinner_set_size (EPHY_SPINNER (priv->throbber), GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_container_add (GTK_CONTAINER (ebox), priv->throbber);

	item = gtk_tool_item_new ();
	gtk_container_add (GTK_CONTAINER (item), ebox);
	gtk_widget_show_all (GTK_WIDGET (item));

	gtk_toolbar_insert (GTK_TOOLBAR (priv->presence_toolbar), item, -1);

	str = _("Show and edit accounts");
	gtk_tooltips_set_tip (GTK_TOOLTIPS (priv->tooltips),
			      ebox, str, str);

	g_signal_connect (ebox,
			  "button-press-event",
			  G_CALLBACK (app_throbber_button_press_event_cb),
			  NULL);
}

static void
app_setup_presences (void)
{
	GossipAppPriv  *priv;

	priv = GET_PRIV (app);

	/* Get saved presence presets. */
	gossip_debug (DEBUG_DOMAIN_SETUP, "Configuring presets");
	gossip_status_presets_get_all ();

	/* Set up current presence. */
	gossip_self_presence_updated (gossip_app_get_self_presence ());
}

static void
app_restore_main_window_geometry (GtkWidget *main_window)
{
	GossipAppPriv *priv;
	gint           x, y, w, h;

	priv = GET_PRIV (app);

	gossip_debug (DEBUG_DOMAIN_SETUP, "Configuring window geometry...");
	gossip_geometry_load_for_main_window (&x, &y, &w, &h);

	if (w >= 1 && h >= 1) {
		/* Use the defaults from the glade file if we
		 * don't have good w, h geometry.
		 */
		gossip_debug (DEBUG_DOMAIN_SETUP, "Configuring window default size w:%d, h:%d", w, h);
		gtk_window_set_default_size (GTK_WINDOW (main_window), w, h);
	}

	if (x >= 0 && y >= 0) {
		/* Let the window manager position it if we
		 * don't have good x, y coordinates.
		 */
		gossip_debug (DEBUG_DOMAIN_SETUP, "Configuring window default position x:%d, y:%d", x, y);
		gtk_window_move (GTK_WINDOW (main_window), x, y);
	}
}

static void
app_setup_contact_list_show_offline (GtkWidget *show_offline_widget)
{
	GossipAppPriv *priv;
	gboolean       show_offline;

	priv = GET_PRIV (app);
	/* Set up 'show_offline' config hooks to know when it changes. */
	gossip_conf_get_bool (gossip_conf_get (),
			      GOSSIP_PREFS_CONTACTS_SHOW_OFFLINE,
			      &show_offline);
	gossip_conf_notify_add (gossip_conf_get (),
				GOSSIP_PREFS_CONTACTS_SHOW_OFFLINE,
				app_notify_show_offline_cb,
				show_offline_widget);

	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (show_offline_widget),
					show_offline);
}

static void
app_setup_contact_list_compact (void)
{
	GossipAppPriv *priv;
	gboolean       compact_contact_list;

	priv = GET_PRIV (app);

	/* Compact contact list? */
	gossip_conf_get_bool (gossip_conf_get (),
			      GOSSIP_PREFS_UI_COMPACT_CONTACT_LIST,
			      &compact_contact_list);

	gossip_conf_notify_add (gossip_conf_get (),
				GOSSIP_PREFS_UI_COMPACT_CONTACT_LIST,
				app_notify_compact_contact_list_cb,
				NULL);

	g_object_set (priv->contact_list,
		      "is-compact", compact_contact_list,
		      NULL);
}

static void
app_setup_contact_list_show_avatars (void)
{
	GossipAppPriv *priv;
	gboolean       show_avatars;

	priv = GET_PRIV (app);

	/* Show avatars? */
	gossip_conf_get_bool (gossip_conf_get (),
			      GOSSIP_PREFS_UI_SHOW_AVATARS,
			      &show_avatars);
	gossip_conf_notify_add (gossip_conf_get (),
				GOSSIP_PREFS_UI_SHOW_AVATARS,
				app_notify_show_avatars_cb,
				NULL);

	g_object_set (priv->contact_list,
		      "show-avatars", show_avatars,
		      NULL);
}

static void
app_setup_contact_list_sort_criterium (void)
{
	GossipAppPriv *priv;
	gchar         *str;

	priv = GET_PRIV (app);

	str = NULL;

	gossip_conf_get_string (gossip_conf_get (),
				GOSSIP_PREFS_CONTACTS_SORT_CRITERIUM,
				&str);
	gossip_conf_notify_add (gossip_conf_get (),
				GOSSIP_PREFS_CONTACTS_SORT_CRITERIUM,
				app_notify_sort_criterium_cb,
				NULL);

	if (str) {
		GType       type;
		GEnumClass *enum_class;
		GEnumValue *enum_value;
		
		type = gossip_contact_list_sort_get_type ();
		enum_class = G_ENUM_CLASS (g_type_class_peek (type));
		enum_value = g_enum_get_value_by_nick (enum_class, str);

		if (enum_value) {
			g_object_set (priv->contact_list,
				      "sort-criterium", enum_value->value,
				      NULL);
		}

		g_free (str);
	} 
}

static void
app_setup_contact_list (GtkWidget *scrolled_window,
			GtkWidget *show_offline_widget)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	priv->contact_list = gossip_contact_list_new ();

	g_signal_connect (priv->contact_list, "contact-activated",
			  G_CALLBACK (app_contact_activated_cb),
			  NULL);

	/* Finish setting up contact list. */
	gtk_widget_show (GTK_WIDGET (priv->contact_list));
	gtk_container_add (GTK_CONTAINER (scrolled_window),
			   GTK_WIDGET (priv->contact_list));

	gossip_debug (DEBUG_DOMAIN_SETUP, "Configuring contact list settings");
	app_setup_contact_list_show_offline (show_offline_widget);
	app_setup_contact_list_compact ();
	app_setup_contact_list_show_avatars ();
	app_setup_contact_list_sort_criterium ();
}

static void
app_setup (GossipSession *session)
{
	GossipAppPriv *priv;
	GossipConf    *conf;
	GladeXML      *glade;
	GtkWidget     *sw;
	GtkWidget     *show_offline_widget;
	GtkWidget     *help_contents;

	gossip_debug (DEBUG_DOMAIN_SETUP, "Beginning...");

	priv = GET_PRIV (app);

	conf = gossip_conf_get ();

	priv->session = g_object_ref (session);

	gossip_debug (DEBUG_DOMAIN_SETUP,
		      "Initialising session listeners "
		      "(subscription, file transfer, etc)");
	gossip_subscription_dialog_init (priv->session);
	gossip_ft_dialog_init (priv->session);

	if (gossip_accounts_dialog_is_needed ()) {
		gossip_debug (DEBUG_DOMAIN_SETUP,
			      "Showing new account window "
			      "for first time run");
		gossip_accounts_dialog_show (NULL);
	}

	gossip_debug (DEBUG_DOMAIN_SETUP,
		      "Initialising managers "
		      "(chatroom, chat, event)");

	priv->chatroom_manager = gossip_session_get_chatroom_manager (priv->session);

	priv->chat_manager = gossip_chat_manager_new ();
	priv->event_manager = gossip_event_manager_new ();

	gossip_debug (DEBUG_DOMAIN_SETUP,
		      "Initialising notification "
		      "handlers (libnotify, sound)");

#ifdef HAVE_LIBNOTIFY
	gossip_notify_init (priv->session, priv->event_manager);
#endif

	gossip_sound_init (priv->session);

	/* Set up signals */
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

	g_signal_connect (priv->session, "chatroom-auto-connect",
			  G_CALLBACK (app_session_chatroom_auto_connect_cb),
			  NULL);

	g_signal_connect (priv->event_manager, "event-added",
			  G_CALLBACK (app_event_added_cb),
			  NULL);

	/* Set up interface */
	gossip_debug (DEBUG_DOMAIN_SETUP, "Initialising interface");
	glade = gossip_glade_get_file ("main.glade",
				       "main_window",
				       NULL,
				       "main_window", &priv->window,
				       "main_vbox", &priv->main_vbox,
				       "find_hbox", &priv->find_hbox,
				       "find_entry", &priv->find_entry,
				       "errors_vbox", &priv->errors_vbox,
				       "chat_connect", &priv->chat_connect,
				       "chat_disconnect", &priv->chat_disconnect,
				       "chat_search", &priv->chat_search,
				       "chat_context", &priv->chat_context,
				       "chat_context_separator", &priv->chat_context_separator,
				       "chat_show_offline", &show_offline_widget,
				       "room", &priv->room,
				       "room_sep", &priv->room_sep,
				       "room_join_favorites", &priv->room_join_favorites,
				       "help_contents", &help_contents,
				       "presence_toolbar", &priv->presence_toolbar,
				       "roster_scrolledwindow", &sw,
				       NULL);

	gossip_glade_connect (glade,
			      app,
			      "main_window", "destroy", app_main_window_destroy_cb,
			      "main_window", "delete_event", app_main_window_delete_event_cb,
			      "main_window", "configure_event", app_window_configure_event_cb,
			      "main_window", "key_press_event", app_main_window_key_press_event_cb,
			      "find_entry", "changed", app_find_entry_changed_cb, 
			      "chat", "button-press-event", app_chat_button_press_event_cb,
			      "chat_quit", "activate", app_chat_quit_cb,
			      "chat_connect", "activate", app_chat_connect_cb,
			      "chat_disconnect", "activate", app_chat_disconnect_cb,
			      "chat_search", "activate", app_chat_search_cb,
			      "chat_new_message", "activate", app_chat_new_message_cb,
			      "chat_history", "activate", app_chat_history_cb,
			      "room_join_new", "activate", app_room_join_new_cb,
			      "room_join_favorites", "activate", app_room_join_favorites_cb,
			      "room_manage_favorites", "activate", app_room_manage_favorites_cb,
			      "chat_add_contact", "activate", app_chat_add_contact_cb,
			      "chat_show_offline", "toggled", app_chat_show_offline_cb,
			      "edit_accounts", "activate", app_edit_accounts_cb,
			      "edit_personal_information", "activate", app_edit_personal_information_cb,
			      "edit_preferences", "activate", app_edit_preferences_cb,
			      "help_about", "activate", app_help_about_cb,
			      "help_contents", "activate", app_help_contents_cb,
			      NULL);

	/* Set up menu */
	app_favorite_chatroom_menu_setup ();

	gtk_widget_hide (priv->chat_context);
	gtk_widget_hide (priv->chat_context_separator);

#ifndef HAVE_HELP
	gtk_widget_hide (help_contents);
#endif

	/* Set up connection related widgets. */
	app_connection_items_setup (glade);
	g_object_unref (glade);

	/* Setup specialised widgets */
	gossip_debug (DEBUG_DOMAIN_SETUP, "Configuring specialised widgets");

	app_setup_presence_chooser ();
	
	app_setup_throbber ();


	/* Set up notification area / tray. */
	gossip_debug (DEBUG_DOMAIN_SETUP, "Configuring notification area widgets");
	app_status_icon_create_menu ();
	app_status_icon_create ();

	app_setup_presences ();

	app_restore_user_accels ();

	app_restore_main_window_geometry (priv->window);

	/* Setup the contact list */
	app_setup_contact_list (sw, show_offline_widget);

	/* Sort criterium */
	/* Set window to be hidden. If doesn't have status icon, show window
	 * and mask "chat_hide_list".
	 */
	g_timeout_add (200, app_status_icon_check_embedded_cb, NULL);
}

static gboolean
app_main_window_quit_confirm (GossipApp *app,
			      GtkWidget *window)
{
	GossipAppPriv *priv;
	GList         *events;

	priv = GET_PRIV (app);

	events = gossip_event_manager_get_events (priv->event_manager);

	if (g_list_length ((GList*) events) > 0) {
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

	app_save_user_accels ();

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
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	if (gtk_status_icon_is_embedded (priv->status_icon)) {
		gossip_hint_show (GOSSIP_PREFS_HINTS_CLOSE_MAIN_WINDOW,
				  _("Gossip is still running, it is just hidden."),
				  _("Click on the notification area icon to show Gossip."),
				  GTK_WINDOW (gossip_app_get_window ()),
				  NULL, NULL);

		gossip_app_set_visibility (FALSE);

		return TRUE;
	}

	if (app_main_window_quit_confirm (app, window)) {
		/* Don't quit if we have messages open */
		return TRUE;
	}

	if (gossip_hint_dialog_show (GOSSIP_PREFS_HINTS_CLOSE_MAIN_WINDOW,
				     _("You were about to quit!"),
				     _("Since no system or notification tray has been "
				       "found, this action would normally quit Gossip.\n\n"
				       "This is just a reminder, from now on, Gossip will "
				       "quit when performing this action unless you uncheck "
				       "the option below."),
				     GTK_WINDOW (gossip_app_get_window ()),
				     NULL, NULL)) {
		/* Shown, we don't quit because the callback will
		 * decide that based on the YES|NO response from the
		 * question we are about to ask, since this behaviour
		 * is new.
		 */
		return TRUE;
	}

	/* At this point, we have checked we have:
	 *   - No tray
	 *   - No pending messages
	 *   - Have NOT shown the hint
	 *
	 * So we just quit.
	 */

	return FALSE;
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
app_find_entry_changed_cb (GtkEditable *editable,
		     	   GossipApp   *app)
{
	GossipAppPriv *priv;
	const gchar   *filter;

	priv = GET_PRIV (app);

	filter = gtk_entry_get_text (GTK_ENTRY (priv->find_entry));
	gossip_contact_list_set_filter (priv->contact_list, filter);
}

static void
app_favorite_chatroom_menu_activate_cb (GtkMenuItem    *menu_item,
					GossipChatroom *chatroom)
{
	GossipSession          *session;
	GossipAccount          *account;
	GossipChatroomProvider *provider;

	session = gossip_app_get_session ();
	account = gossip_chatroom_get_account (chatroom);
	provider = gossip_session_get_chatroom_provider (session, account);

	gossip_group_chat_new (provider, chatroom);
}

static void
app_favorite_chatroom_menu_update (void)
{
	GossipAppPriv *priv;
	GList         *chatrooms, *l;
	gboolean       found = FALSE;

	priv = GET_PRIV (app);

	chatrooms = gossip_chatroom_manager_get_chatrooms (priv->chatroom_manager, NULL);
	for (l = chatrooms; l; l = l->next) {
		GossipChatroom *chatroom;
		GossipAccount  *account;
		GtkWidget      *menu_item;
		gboolean        visible;

		chatroom = l->data;

		account = gossip_chatroom_get_account (chatroom);
		menu_item = g_object_get_data (G_OBJECT (chatroom), "menu_item");

		visible = gossip_chatroom_get_favourite (chatroom);
		visible &= gossip_session_is_connected (priv->session, account);

		if (visible) {
			gtk_widget_show (menu_item);
			found = TRUE;
		} else {
			gtk_widget_hide (menu_item);
		}
	}

	g_list_free (chatrooms);

	if (found) {
		gtk_widget_show (priv->room_sep);
	} else {
		gtk_widget_hide (priv->room_sep);
	}

	gtk_widget_set_sensitive (priv->room_join_favorites, found);
}

static gboolean
app_favorite_chatroom_menu_add (GossipChatroom *chatroom)
{
	GossipAppPriv *priv;
	GossipAccount *account;
	GtkWidget     *menu_item;
	const gchar   *name;
	gboolean       visible;

	priv = GET_PRIV (app);

	account = gossip_chatroom_get_account (chatroom);
	menu_item = g_object_get_data (G_OBJECT (chatroom), "menu_item");

	visible = gossip_chatroom_get_favourite (chatroom);
	visible &= gossip_session_is_connected (priv->session, account);

	if (menu_item) {
		return visible;
	}

	name = gossip_chatroom_get_name (chatroom);
	menu_item = gtk_menu_item_new_with_label (name);

	g_object_set_data (G_OBJECT (chatroom), "menu_item", menu_item);
	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (app_favorite_chatroom_menu_activate_cb),
			  chatroom);
	g_signal_connect (chatroom, "notify::name",
			  G_CALLBACK (app_favorite_chatroom_menu_name_cb),
			  menu_item);

	gtk_menu_shell_insert (GTK_MENU_SHELL (priv->room_menu),
			       menu_item, 3);

	if (visible) {
		gtk_widget_show (menu_item);
	}

	return visible;
}

static void
app_favorite_chatroom_menu_update_cb (GossipChatroomManager *manager,
				      GossipChatroom        *chatroom,
				      gpointer               user_data)
{
	app_favorite_chatroom_menu_update ();
}

static void
app_favorite_chatroom_menu_name_cb (GossipChatroom *chatroom,
				    GParamSpec     *spec,
				    GtkWidget      *menu_item)
{
	GtkWidget *label;

	label = gtk_bin_get_child (GTK_BIN (menu_item));
	gtk_label_set_text (GTK_LABEL (label), gossip_chatroom_get_name (chatroom));
}

static void
app_favorite_chatroom_menu_added_cb (GossipChatroomManager *manager,
				     GossipChatroom        *chatroom,
				     gpointer               user_data)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	if (app_favorite_chatroom_menu_add (chatroom)) {
		gtk_widget_show (priv->room_sep);
		gtk_widget_set_sensitive (priv->room_join_favorites, TRUE);
	}
}

static void
app_favorite_chatroom_menu_removed_cb (GossipChatroomManager *manager,
				       GossipChatroom        *chatroom,
				       gpointer               user_data)
{
	GossipAppPriv *priv;
	GossipAccount *account;
	GtkWidget     *menu_item;
	gboolean       visible;

	priv = GET_PRIV (app);

	account = gossip_chatroom_get_account (chatroom);
	menu_item = g_object_get_data (G_OBJECT (chatroom), "menu_item");

	visible = gossip_chatroom_get_favourite (chatroom);
	visible &= gossip_session_is_connected (priv->session, account);

	g_signal_handlers_disconnect_by_func (chatroom, 
					      G_CALLBACK (app_favorite_chatroom_menu_name_cb),
					      menu_item);

	g_object_set_data (G_OBJECT (chatroom), "menu_item", NULL);
	gtk_widget_destroy (menu_item);

	if (visible) {
		app_favorite_chatroom_menu_update ();
	}
}

static void
app_favorite_chatroom_menu_setup (void)
{
	GossipAppPriv *priv;
	GList         *chatrooms, *l;
	gboolean       found = FALSE;

	priv = GET_PRIV (app);

	chatrooms = gossip_chatroom_manager_get_chatrooms (priv->chatroom_manager, NULL);
	priv->room_menu =
		gtk_menu_item_get_submenu (GTK_MENU_ITEM (priv->room));

	for (l = chatrooms; l; l = l->next) {
		if (app_favorite_chatroom_menu_add (l->data)) {
			found = TRUE;
		}
	}

	g_list_free (chatrooms);

	if (!found) {
		gtk_widget_hide (priv->room_sep);
	}

	gtk_widget_set_sensitive (priv->room_join_favorites, found);

	g_signal_connect (priv->chatroom_manager, "chatroom-favourite-update",
			  G_CALLBACK (app_favorite_chatroom_menu_update_cb),
			  NULL);
	g_signal_connect (priv->chatroom_manager, "chatroom-added",
			  G_CALLBACK (app_favorite_chatroom_menu_added_cb),
			  NULL);
	g_signal_connect (priv->chatroom_manager, "chatroom-removed",
			  G_CALLBACK (app_favorite_chatroom_menu_removed_cb),
			  NULL);
}

static void
app_chat_quit_cb (GtkWidget *window,
		  GossipApp *app)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	if (!app_main_window_quit_confirm (app, priv->window)) {
		gtk_widget_destroy (priv->window);
	}
}

static void
app_chat_connect_cb (GtkWidget *window,
		     GossipApp *app)
{
	gossip_app_connect (NULL, FALSE);
}

static void
app_chat_disconnect_cb (GtkWidget *window,
			GossipApp *app)
{
	app_disconnect ();
}

static void
app_chat_search_cb (GtkWidget *window,
		    GossipApp *app)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);
	
	if (GTK_WIDGET_VISIBLE (priv->find_hbox)) {
		gtk_widget_hide (priv->find_hbox);
	} else {
		gtk_widget_show (priv->find_hbox);
		gtk_widget_grab_focus (priv->find_entry);
	}
}

static void
app_chat_new_message_cb (GtkWidget *widget,
			 GossipApp *app)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_new_message_dialog_show (GTK_WINDOW (priv->window));
}

static void
app_chat_history_cb (GtkWidget *widget,
		     GossipApp *app)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_log_window_show (NULL, NULL);
}

static void
app_room_join_new_cb (GtkWidget *window,
		      GossipApp *app)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_new_chatroom_dialog_show (GTK_WINDOW (priv->window));
}

static void
app_room_join_favorites_cb (GtkWidget *window,
			    GossipApp *app)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_session_chatroom_join_favorites (priv->session);
}

static void
app_room_manage_favorites_cb (GtkWidget *window,
			      GossipApp *app)
{
	gossip_chatrooms_window_show (NULL, FALSE);
}

static void
app_chat_add_contact_cb (GtkWidget *widget,
			 GossipApp *app)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_add_contact_dialog_show (GTK_WINDOW (priv->window), NULL);
}

static void
app_chat_show_offline_cb (GtkCheckMenuItem *item,
			  GossipApp        *app)
{
	GossipAppPriv *priv;
	gboolean       current;

	priv = GET_PRIV (app);

	current = gtk_check_menu_item_get_active (item);

	gossip_conf_set_bool (gossip_conf_get (),
			      GOSSIP_PREFS_CONTACTS_SHOW_OFFLINE,
			      current);

	g_object_set (priv->contact_list, "show_offline", current, NULL);
}

static gboolean
app_chat_button_press_event_cb (GtkWidget      *widget,
				GdkEventButton *event,
				GossipApp      *app)
{
	GossipAppPriv *priv;
	GossipContact *contact;
	gchar         *group;

	if (!event->button == 1) {
		return FALSE;
	}

	priv = GET_PRIV (app);

	group = gossip_contact_list_get_selected_group (priv->contact_list);
	if (group) {
		GtkMenuItem *item;
		GtkWidget   *label;
		GtkWidget   *submenu;

		item = GTK_MENU_ITEM (priv->chat_context);
		label = gtk_bin_get_child (GTK_BIN (item));
		gtk_label_set_text (GTK_LABEL (label), _("Group"));

		gtk_widget_show (priv->chat_context);
		gtk_widget_show (priv->chat_context_separator);

		submenu = gossip_contact_list_get_group_menu (priv->contact_list);
		gtk_menu_item_set_submenu (item, submenu);

		g_free (group);

		return FALSE;
	}

	contact = gossip_contact_list_get_selected (priv->contact_list);
	if (contact) {
		GtkMenuItem *item;
		GtkWidget   *label;
		GtkWidget   *submenu;

		item = GTK_MENU_ITEM (priv->chat_context);
		label = gtk_bin_get_child (GTK_BIN (item));
		gtk_label_set_text (GTK_LABEL (label), _("Contact"));

		gtk_widget_show (priv->chat_context);
		gtk_widget_show (priv->chat_context_separator);

		submenu = gossip_contact_list_get_contact_menu (priv->contact_list,
								contact);
		gtk_menu_item_set_submenu (item, submenu);

		g_object_unref (contact);

		return FALSE;
	}

	gtk_widget_hide (priv->chat_context);
	gtk_widget_hide (priv->chat_context_separator);

	return FALSE;
}

static void
app_edit_accounts_cb (GtkWidget *widget,
		      GossipApp *app)
{
	gossip_accounts_dialog_show (NULL);
}

static void
app_edit_personal_information_cb (GtkWidget *widget,
				  GossipApp *app)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_vcard_dialog_show (GTK_WINDOW (priv->window));
}

static void
app_edit_preferences_cb (GtkWidget *widget,
			 GossipApp *app)
{
	gossip_preferences_show ();
}

static void
app_help_about_cb (GtkWidget *window,
		   GossipApp *app)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_about_dialog_new (GTK_WINDOW (priv->window));
}

static void
app_help_contents_cb (GtkWidget *window,
		      GossipApp *app)
{
	gossip_help_show ();
}

static gboolean
app_throbber_button_press_event_cb (GtkWidget      *throbber_ebox,
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
				    GossipJabber   *jabber,
				    gpointer        user_data)
{
	GossipAppPriv *priv;
	const gchar   *name;

	priv = GET_PRIV (app);

	name = gossip_account_get_name (account);
	gossip_debug (DEBUG_DOMAIN_SESSION, "Connecting account:'%s'", name);

	ephy_spinner_start (EPHY_SPINNER (priv->throbber));
}

static void
app_restore_saved_presence (void)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_app_set_presence (gossip_status_presets_get_default_state (),
				 gossip_status_presets_get_default_status());

	gossip_self_presence_updated (gossip_app_get_self_presence ());
}

static void
app_session_protocol_connected_cb (GossipSession *session,
				   GossipAccount *account,
				   GossipJabber  *jabber,
				   gpointer       user_data)
{
	GossipAppPriv *priv;
	gboolean       connecting;
	const gchar   *name;

	priv = GET_PRIV (app);

	name = gossip_account_get_name (account);
	gossip_debug (DEBUG_DOMAIN_SESSION, "Connected account:'%s'", name);

	gossip_session_count_accounts (priv->session,
				       NULL,
				       &connecting,
				       NULL);

	if (connecting < 1) {
		ephy_spinner_stop (EPHY_SPINNER (priv->throbber));
	}

	g_hash_table_remove (priv->errors, account);
	g_hash_table_remove (priv->reconnects, account);

	app_connection_items_update ();
	app_favorite_chatroom_menu_update ();

	app_restore_saved_presence ();
}

static void
app_reconnect_remove (gpointer data)
{
	guint *id;

	id = (guint*) data;
	g_source_remove (*id);
}

static gboolean
app_reconnect_cb (GossipAccount *account)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_app_connect (account, TRUE);
	g_hash_table_remove (priv->reconnects, account);

	return FALSE;
}

static void
app_session_protocol_disconnected_cb (GossipSession *session,
				      GossipAccount *account,
				      GossipJabber  *jabber,
				      gint           reason,
				      gpointer       user_data)
{
	GossipAppPriv *priv;
	gboolean       connecting;
	gboolean       should_reconnect;
	const gchar   *name;
#ifdef HAVE_DBUS
	gboolean       nm_connected;
#endif

	priv = GET_PRIV (app);

	name = gossip_account_get_name (account);
	gossip_debug (DEBUG_DOMAIN_SESSION, "Disconnected account:'%s'", name);

	gossip_session_count_accounts (priv->session,
				       NULL,
				       &connecting,
				       NULL);

	if (connecting < 1) {
		ephy_spinner_stop (EPHY_SPINNER (priv->throbber));
	}

	app_connection_items_update ();
	app_favorite_chatroom_menu_update ();
	gossip_self_presence_updated (gossip_app_get_self_presence ());

	should_reconnect = reason != GOSSIP_JABBER_DISCONNECT_ASKED;

#ifdef HAVE_DBUS
	/* If NM says we are offline that's useless to retry to connect,
	 * NM will tell us when network is up again. 
	 */
	if (gossip_dbus_nm_get_state (&nm_connected)) {
		should_reconnect &= nm_connected;
	}
#endif

	should_reconnect &= !g_hash_table_lookup (priv->reconnects, account);

	if (should_reconnect) {
		guint id;

		/* Unexpected disconnection, try to reconnect */
		id = g_timeout_add (RETRY_CONNECT_TIMEOUT * 1000,
				    (GSourceFunc) app_reconnect_cb,
				    account);
		g_hash_table_insert (priv->reconnects,
				     g_object_ref (account),
				     &id);
	}
}

static void
app_session_protocol_error_cb (GossipSession  *session,
			       GossipJabber   *jabber,
			       GossipAccount  *account,
			       GError         *error,
			       gpointer        user_data)
{
	GossipAppPriv *priv;
	gboolean       connecting;
	const gchar   *name;

	priv = GET_PRIV (app);

	name = gossip_account_get_name (account);
	gossip_debug (DEBUG_DOMAIN_SESSION, "Error for account:'%s'", name);

	gossip_session_count_accounts (priv->session,
				       NULL,
				       &connecting,
				       NULL);

	if (connecting < 1) {
		ephy_spinner_stop (EPHY_SPINNER (priv->throbber));
	}

	if (error && error->code == GOSSIP_JABBER_NO_PASSWORD) {
		/* Handle this less generally. */
		app_account_password_dialog_show (account);
		return;
	}

	app_accounts_error_display (account, error);
}

static void
app_account_password_dialog_activate_cb (GtkWidget *entry, 
					 GtkDialog *dialog)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static void
app_account_password_dialog_response_cb (GtkWidget *dialog,
					 gint       response,
					 gpointer   user_data)
{
	if (response == GTK_RESPONSE_OK) {
		GossipSession *session;
		GossipAccount *account;
		GtkWidget     *checkbutton;
		GtkWidget     *entry;
		const gchar   *password;

		session = gossip_app_get_session ();
		account = g_object_get_data (G_OBJECT (dialog), "account");

		checkbutton = g_object_get_data (G_OBJECT (dialog), "checkbutton");
		entry = g_object_get_data (G_OBJECT (dialog), "entry");

		password = gtk_entry_get_text (GTK_ENTRY (entry));

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton))) {
			GossipAccountManager *manager;

			manager = gossip_session_get_account_manager (session);

			gossip_account_set_password (account, password);
			gossip_account_manager_store (manager);
		} else {
			gossip_account_set_password_tmp (account, password);
		}

		/* Retry the connect */
		gossip_session_connect (session, account, FALSE);
	}

	gtk_widget_destroy (dialog);
}

static void
app_account_password_dialog_show (GossipAccount *account)
{
	GtkWidget   *dialog;
	GtkWidget   *checkbutton;
	GtkWidget   *entry;
	GtkWidget   *vbox;
	gchar       *str;
	const gchar *name;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	str = g_strdup_printf (_("Please enter your %s account password"),
			       gossip_account_get_name (account));
	dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (gossip_app_get_window ()),
						     GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_MESSAGE_QUESTION,
						     GTK_BUTTONS_OK_CANCEL,
						     "<b>%s</b>",
						     str);
	g_free (str);

	name = gossip_account_get_name (account);
	str = g_strdup_printf (_("Logging in to account '%s'"), name);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", str);
	g_free (str);

	checkbutton = gtk_check_button_new_with_label (_("Remember Password?"));

	entry = gtk_entry_new ();
	gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);

	g_signal_connect (entry, "activate",
			  G_CALLBACK (app_account_password_dialog_activate_cb),
			  dialog);

	vbox = gtk_vbox_new (FALSE, 6);

	gtk_container_set_border_width  (GTK_CONTAINER (vbox), 6);

	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), checkbutton, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, FALSE, FALSE, 0);

	g_object_set_data (G_OBJECT (dialog), "entry", entry);
	g_object_set_data (G_OBJECT (dialog), "checkbutton", checkbutton);
	g_object_set_data_full (G_OBJECT (dialog), "account", 
				g_object_ref (account), 
				g_object_unref);

	g_signal_connect (dialog, "response", 
			  G_CALLBACK (app_account_password_dialog_response_cb),
			  NULL);

	gtk_widget_show_all (dialog);
}

/*
 * Accels
 */
static void
app_restore_user_accels (void)
{
	gchar *filename;

	gossip_debug (DEBUG_DOMAIN_SETUP, "Configuring accels");
	filename = g_build_filename (g_get_home_dir (), ".gnome2", "accels", PACKAGE_TARNAME, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		gossip_debug (DEBUG_DOMAIN_ACCELS, "Loading from:'%s'", filename);
		gtk_accel_map_load (filename);
	}

	g_free (filename);
}

static void
app_save_user_accels (void)
{
	gchar *dir;
	gchar *file_with_path;

	dir = g_build_filename (g_get_home_dir (), ".gnome2", "accels", NULL);
	g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	file_with_path = g_build_filename (dir, PACKAGE_TARNAME, NULL);
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

	if (visible && gtk_status_icon_is_embedded (priv->status_icon)) {
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

		if (x >= 0 && y >= 0) {
			/* Let the window manager position it if we
			 * don't have good x, y coordinates.
			 */
			gtk_window_move (GTK_WINDOW (priv->window), x, y);
		}

		gossip_window_present (GTK_WINDOW (priv->window), TRUE);

		gossip_conf_set_bool (gossip_conf_get (),
				      GOSSIP_PREFS_UI_MAIN_WINDOW_HIDDEN, FALSE);
	}
}

void
gossip_app_set_visibility (gboolean visible)
{
	GtkWidget *window;

	window = gossip_app_get_window ();

	gossip_conf_set_bool (gossip_conf_get (),
			      GOSSIP_PREFS_UI_MAIN_WINDOW_HIDDEN,
			      !visible);

	if (visible) {
		gossip_window_present (GTK_WINDOW (window), TRUE);
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

static void
app_status_icon_popup_menu_cb (GtkStatusIcon  *status_icon,
			       guint          button,
			       guint          activate_time,
			       GossipApp      *app)
{
	GossipAppPriv *priv;
	GtkWidget     *submenu;
	gboolean       show;

	priv = GET_PRIV (app);

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
			NULL, NULL,
			gtk_status_icon_position_menu,
			priv->status_icon,
			button,
			activate_time);
}

static void
app_status_icon_create_menu (void)
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
			      "tray_quit", "activate", app_chat_quit_cb,
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
app_status_icon_create (void)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	priv->status_icon = gossip_status_icon_get ();

	g_signal_connect (priv->status_icon,
			  "popup_menu",
			  G_CALLBACK (app_status_icon_popup_menu_cb),
			  app);

	gtk_status_icon_set_visible (priv->status_icon, TRUE);

#ifdef HAVE_LIBNOTIFY
	gossip_notify_set_attach_status_icon (priv->status_icon);
#endif

	gossip_status_icon_update_tooltip (GOSSIP_STATUS_ICON (priv->status_icon));
}

#define MAX_CHECKS 5

static gboolean
app_status_icon_check_embedded_cb (gpointer user_data)
{
	static guint    checks = 1;
	GossipAppPriv  *priv;
	GtkRequisition  req;
	gboolean        hidden;
	gboolean        embedded;

	priv = GET_PRIV (app);

	embedded = gtk_status_icon_is_embedded (gossip_status_icon_get ());

	if (!embedded && checks < MAX_CHECKS) {
		gossip_debug (DEBUG_DOMAIN_SETUP, 
			      "Configuring window (status icon not embedded, check %d/%d)", 
			      checks, MAX_CHECKS);
		checks++;
		return TRUE;
	}
	
	if (!embedded) {
		gossip_debug (DEBUG_DOMAIN_SETUP, 
			      "Configuring window (status icon not embedded, showing)");
		hidden = FALSE;
	} else {
		GossipConf *conf;

		conf = gossip_conf_get ();
		gossip_conf_get_bool (conf,
				      GOSSIP_PREFS_UI_MAIN_WINDOW_HIDDEN,
				      &hidden);
		gossip_debug (DEBUG_DOMAIN_SETUP, 
			      "Configuring window visibility (config says %s)", 
			      hidden ? "hide it" : "show it");
	}

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

	return FALSE;
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
		gossip_contact_list_set_show_avatars (priv->contact_list,
						      show_avatars);
	}
}

static void
app_notify_sort_criterium_cb (GossipConf  *conf,
			      const gchar *key,
			      gpointer     user_data)
{
	GossipAppPriv *priv;
	gchar         *str = NULL;

	priv = GET_PRIV (app);

	if (gossip_conf_get_string (conf, key, &str)) {
		GType       type;
		GEnumClass *enum_class;
		GEnumValue *enum_value;

		type = gossip_contact_list_sort_get_type ();
		enum_class = G_ENUM_CLASS (g_type_class_peek (type));
		enum_value = g_enum_get_value_by_nick (enum_class, str);

		if (enum_value) {
			gossip_contact_list_set_sort_criterium (priv->contact_list, 
								enum_value->value);
		}
		
		g_free (str);
	}
}

static void
app_notify_compact_contact_list_cb (GossipConf  *conf,
				    const gchar *key,
				    gpointer     user_data)
{
	GossipAppPriv *priv;
	gboolean       compact_contact_list;

	priv = GET_PRIV (app);

	if (gossip_conf_get_bool (conf, key, &compact_contact_list)) {
		gossip_contact_list_set_is_compact (priv->contact_list,
						    compact_contact_list);
	}
}

/*
 * App
 */

void
gossip_app_connect (GossipAccount *account, gboolean startup)
{
	GossipAppPriv        *priv;
	GossipAccountManager *manager;

	priv = GET_PRIV (app);

	manager = gossip_session_get_account_manager (priv->session);
	if (gossip_account_manager_get_count (manager) < 1) {
		/* Show the accounts dialog instead */
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
gossip_app_create (GossipSession *session)

{
	g_return_if_fail (GOSSIP_IS_SESSION (session));

	g_object_new (GOSSIP_TYPE_APP, NULL);

	app_setup (session);
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
		"chat_disconnect",
		"room",
		"chat_new_message",
		"chat_add_contact",
		"edit_personal_information"
	};

	const gchar   *widgets_disconnected[] = {
		"chat_connect"
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
		gossip_self_presence_stop_flash (gossip_app_get_self_presence ());
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
app_accounts_error_edit_clicked_cb (GtkButton *button, GossipAccount *account)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_accounts_dialog_show (account);
	g_hash_table_remove (priv->errors, account);
}

static void
app_accounts_error_clear_clicked_cb (GtkButton *button, GossipAccount *account)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	g_hash_table_remove (priv->errors, account);
}

static void
app_accounts_error_display (GossipAccount *account, GError *error)
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

static void
app_presence_updated_cb (GossipSelfPresence *self_presence, gpointer user_data)
{
	GossipAppPriv       *priv;
	GdkPixbuf           *pixbuf;
	GossipPresence      *presence;
	GossipPresenceState  state;
	const gchar         *status;

	priv = GET_PRIV (app);

	pixbuf = gossip_self_presence_get_current_pixbuf (gossip_app_get_self_presence ());
	gtk_status_icon_set_from_pixbuf (priv->status_icon, pixbuf);
	g_object_unref (pixbuf);

	if (!gossip_session_is_connected (priv->session, NULL)) {
		gossip_presence_chooser_set_status (
			GOSSIP_PRESENCE_CHOOSER (priv->presence_chooser),
			_("Offline"));
		gossip_status_icon_update_tooltip (GOSSIP_STATUS_ICON (priv->status_icon));
		return;
	}

	presence = gossip_self_presence_get_effective (gossip_app_get_self_presence ());
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

	gossip_status_icon_update_tooltip (GOSSIP_STATUS_ICON (priv->status_icon));
}

void
gossip_app_set_not_away (void)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_self_presence_set_not_away (gossip_app_get_self_presence ());
}

void
gossip_app_set_presence (GossipPresenceState state, const gchar *status)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_self_presence_set_state_status (gossip_app_get_self_presence (), 
					       state, status);
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

static void
app_session_chatroom_auto_connect_cb (GossipSession          *session,
				      GossipChatroomProvider *provider,
				      GossipChatroom         *chatroom,
				      gpointer                user_data)
{
	GossipGroupChat *chat;

	gossip_debug (DEBUG_DOMAIN_CHATROOMS, 
		      "Auto connect for room:'%s'", 
		      gossip_chatroom_get_name (chatroom));

	chat = gossip_group_chat_new (provider, chatroom);
}

static void
app_event_added_cb (GossipEventManager *manager,
		    GossipEvent        *event,
		    gpointer            user_data)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	gossip_request_user_attention ();
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

GossipSelfPresence *
gossip_app_get_self_presence (void)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	return priv->self_presence;
}

GossipHeartbeat *
gossip_app_get_flash_heartbeat (void)
{
	GossipAppPriv *priv;

	priv = GET_PRIV (app);

	return priv->flash_heartbeat;
}

