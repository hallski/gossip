/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2004 Imendio AB
 * Copyright (C) 2003      Kevin Dougherty <gossip@kdough.net>
 * Copyright (C) 2004      Martyn Russell <mr@gnome.org>
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
#include "gossip-transport-accounts.h"
#include "gossip-transport-accounts-window.h"
#include "gossip-transport-discover.h"
#include "gossip-vcard-dialog.h"
#include "gossip-add-contact.h"
#include "gossip-event-manager.h"
#include "gossip-contact.h"
#include "gossip-marshal.h"
#include "gossip-utils.h"
#include "gossip-group-chat.h"
#include "gossip-chat.h"
#include "gossip-private-chat.h"
#include "gossip-account.h"
#include "gossip-connect-dialog.h"
#include "gossip-contact-list.h"
#include "gossip-join-dialog.h"
#include "gossip-sound.h"
#include "gossip-idle.h"
#include "gossip-jabber.h"
#include "gossip-protocol.h"
#include "gossip-startup-druid.h"
#include "gossip-preferences.h"
#include "gossip-presence.h"
#include "gossip-private-chat.h"
#include "gossip-account-dialog.h"
#include "gossip-stock.h"
#include "gossip-about.h"
#include "gossip-app.h"

#ifdef HAVE_DBUS
#include "gossip-dbus.h"
#endif

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

/* Number of seconds of slack when returning from autoaway. */
#define	BACK_SLACK 15

/* Flashing delay for icons (milliseconds). */
#define FLASH_TIMEOUT 500

/* Minimum width of roster window if something goes wrong. */
#define MIN_WIDTH 50


#define d(x)

extern GConfClient *gconf_client;

struct _GossipAppPriv {
	GossipSession     *session;
	GossipChatManager *chat_manager;

        GossipEventManager *event_manager;

	GossipContactList *contact_list;

	/* FIXME: Shouldn't have an LmConnection here */
	LmConnection      *connection;
	GtkWidget         *window;

	EggTrayIcon       *tray_icon;
	GtkWidget         *tray_event_box;
	GtkWidget         *tray_image;
	GtkTooltips       *tray_tooltips;
	GList             *tray_flash_icons;
	guint              tray_flash_timeout_id;

	GtkWidget         *popup_menu;
	GtkWidget         *popup_menu_status_item;
	GtkWidget         *show_popup_item;
	GtkWidget         *hide_popup_item;
	
	GossipAccount     *account;
	gchar             *overridden_resource;

	/* Widgets that are enabled when we're connected/disconnected. */
	GList             *enabled_connected_widgets;
	GList             *enabled_disconnected_widgets;

	/* Status popup. */
	GtkWidget         *status_button_hbox;
	GtkWidget         *status_button;
	GtkWidget         *status_popup;
	GtkWidget         *status_label;
	GtkWidget         *status_image;

	guint              status_flash_timeout_id;
	time_t             leave_time;

	/* Current status/overridden away message. */
	gchar             *status_text;
	gchar             *away_message;

	/* Set by the user (available/busy). */
	GossipShow         explicit_show;

	/* Autostatus (away/xa), overrides explicit_show. */
	GossipShow         auto_show;     
	
	guint              size_timeout_id;
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

static void     gossip_app_class_init                (GossipAppClass     *klass);
static void     gossip_app_init                      (GossipApp          *app);
static void     app_finalize                         (GObject            *object);
static void     app_main_window_destroy_cb           (GtkWidget          *window,
						      GossipApp          *app);
static gboolean app_idle_check_cb                    (GossipApp          *app);
static void     app_quit_cb                          (GtkWidget          *window,
						      GossipApp          *app);
static void     app_session_protocol_connected_cb    (GossipSession      *session,
						      GossipProtocol     *protocol,
						      gpointer            unused);
static void     app_session_protocol_disconnected_cb (GossipSession      *session,
						      GossipProtocol     *protocol,
						      gpointer            unused);
static gchar *  app_session_get_password_cb          (GossipSession      *session,
						      GossipAccount      *account,
						      gpointer            unused);

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
static void     app_personal_details_cb              (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_account_information_cb           (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_status_messages_cb               (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_configure_transports_cb          (GtkWidget          *widget, 
						      GossipApp          *app);
static void     app_about_cb                         (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_join_group_chat_cb               (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_show_hide_activate_cb            (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_connect_cb                       (GtkWidget          *widget,
						      GossipApp          *app);
static void     app_disconnect_cb                    (GtkWidget          *widget,
						      GossipApp          *app);
static gboolean app_tray_destroy_cb                  (GtkWidget          *widget,
						      gpointer            user_data);
static void     app_tray_create_menu                 (void);
static void     app_tray_create                      (void);
static gboolean app_is_visible                       (void);
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
static void     app_toggle_visibility                (void);
static gboolean app_tray_pop_event                   (void);
static void     app_tray_update_tooltip              (void);
static GtkWidget *
app_create_status_menu                               (gboolean            from_window);
static gboolean app_status_button_press_event_cb     (GtkButton          *button,
						      GdkEventButton     *event,
						      gpointer            user_data);
static void     app_status_button_clicked_cb         (GtkButton          *button,
						      GdkEventButton     *event,
						      gpointer            user_data);
static void     app_status_show_status_dialog        (GossipShow          show,
						      const gchar        *str,
						      gboolean            transient);
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
static gboolean   app_have_tray                      (void);
static void       app_tray_flash_start               (void);
static void       app_tray_flash_maybe_stop          (void);

static void       app_event_added_cb                 (GossipEventManager *manager,
						      GossipEvent        *event,
						      gpointer            unused);

static void       app_event_removed_cb               (GossipEventManager *manager,
						      GossipEvent        *event,
						      gpointer            unused);
static void       app_contact_activated_cb           (GossipContactList  *contact_list,
						      GossipContact      *contact,
						      gpointer            unused);

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

	priv->session = gossip_session_new ();
	priv->chat_manager = gossip_chat_manager_new ();
	priv->event_manager = gossip_event_manager_new ();

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

	priv->explicit_show = GOSSIP_SHOW_AVAILABLE;
	priv->auto_show = GOSSIP_SHOW_AVAILABLE;

#ifdef HAVE_DBUS
	gossip_dbus_init ();
#endif
	
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
			      "actions_configure_transports", "activate", app_configure_transports_cb,
			      "edit_preferences", "activate", app_preferences_cb,
			      "edit_personal_details", "activate", app_personal_details_cb,
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
		priv->account = gossip_account_create_empty ();
	}

#if 0
	app_create_connection ();
#endif	
	priv->contact_list = gossip_contact_list_new ();

	g_signal_connect (priv->contact_list, "contact-activated",
			  G_CALLBACK (app_contact_activated_cb),
			  NULL);
	
	show_offline = gconf_client_get_bool (gconf_client,
					      "/apps/gossip/contacts/show_offline",
					      NULL);

	gconf_client_notify_add (gconf_client,
				 "/apps/gossip/contacts/show_offline",
				 app_show_offline_key_changed_cb,
				 show_offline_widget,
				 NULL, NULL);

	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (show_offline_widget),
					show_offline);

#if 0
	gossip_roster_view_set_show_offline (GOSSIP_ROSTER_VIEW (priv->roster_view),
					     show_offline);
#endif
	

	gtk_widget_show (GTK_WIDGET (priv->contact_list));
	gtk_container_add (GTK_CONTAINER (sw),
			   GTK_WIDGET (priv->contact_list));
	/*
	gtk_widget_show (GTK_WIDGET (priv->roster_view));
	gtk_container_add (GTK_CONTAINER (sw),
			   GTK_WIDGET (priv->roster_view));
			   */

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

	if (lm_connection_get_state (connection) == LM_CONNECTION_STATE_OPENING) {
		lm_connection_cancel_open (connection);
	} else if (lm_connection_is_open (connection)) {
		lm_connection_close (connection, NULL);
	}
	
	exit (0);
	/* gtk_main_quit (); */
}

static void
app_quit_cb (GtkWidget *widget, GossipApp *app)
{
	gtk_widget_destroy (app->priv->window);
}

static void
app_session_protocol_connected_cb (GossipSession  *session,
				   GossipProtocol *protocol,
				   gpointer        unused)
{
	GossipAppPriv *priv;
	
	priv = app->priv;

	/* FIXME: implement */

	priv->connection = gossip_jabber_get_connection (GOSSIP_JABBER (protocol));
	app_update_conn_dependent_menu_items ();
	app_update_show ();
}

static void
app_session_protocol_disconnected_cb (GossipSession  *session,
				      GossipProtocol *protocol,
				      gpointer        unused)
{
	GossipAppPriv *priv;
	
	priv = app->priv;
	
	app_update_conn_dependent_menu_items ();
	app_update_show ();
/* FIXME: implement */
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
app_new_message_presence_data_func (GtkCellLayout   *cell_layout,
				    GtkCellRenderer *cell,
				    GtkTreeModel    *tree_model,
				    GtkTreeIter     *iter,
				    gpointer         unused)
{
	GossipContact  *c;
	GdkPixbuf      *pixbuf;
	GossipPresence *p;

	gtk_tree_model_get (tree_model, iter, 0, &c, -1);

	p = gossip_contact_get_presence (c);
	pixbuf = gossip_presence_get_pixbuf (p);

	g_object_set (cell, "pixbuf", pixbuf, NULL);
	g_object_unref (pixbuf);
}

static void
app_new_message (gboolean use_roster_selection, gboolean be_transient)
{
	GossipAppPriv    *priv;
	const gchar      *selected_name = NULL;
	GList            *l;
	GList            *contacts = NULL;
	GossipContact    *contact = NULL;
	GtkWidget        *frame;
	CompleteNameData *data;
	GList            *all_contacts;
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

	/* Results in warnings on GTK+ 2.3.x */
	/*gtk_dialog_set_has_separator (GTK_DIALOG (data->dialog), FALSE);*/

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
	/*gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (data->combo), cell,
				       "pixbuf", 0); */
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
app_add_contact_cb (GtkWidget *widget, GossipApp *app)
{
	gossip_add_contact_new (NULL);
}

static void
app_show_offline_cb (GtkCheckMenuItem *item, GossipApp *app)
{
	GossipAppPriv *priv;
	gboolean       current;
	
	priv = app->priv;

	current = gtk_check_menu_item_get_active (item);

	gconf_client_set_bool (gconf_client,
			       "/apps/gossip/contacts/show_offline",
			       current,
			       NULL);

	g_object_set (priv->contact_list, "show_offline", current, NULL);
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
app_personal_details_cb (GtkWidget *widget, GossipApp *app)
{
	gossip_vcard_dialog_show ();
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
app_configure_transports_cb (GtkWidget *widget, GossipApp *app)
{
	gossip_transport_accounts_window_show ();
}

static gboolean
app_idle_check_cb (GossipApp *app)
{
	GossipAppPriv *priv;
	gint32         idle;
	GossipShow     show;

	priv = app->priv;

	if (!gossip_app_is_connected ()) {
		return TRUE;
	}
	
	idle = gossip_idle_get_seconds ();
	show = app_get_effective_show ();

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
	else if (show != GOSSIP_SHOW_EXT_AWAY && idle > EXT_AWAY_TIME) {
		priv->auto_show = GOSSIP_SHOW_EXT_AWAY;
	}
	else if (show != GOSSIP_SHOW_AWAY && show != GOSSIP_SHOW_EXT_AWAY && 
		 idle > AWAY_TIME) {
		priv->auto_show = GOSSIP_SHOW_AWAY;
	}
	else if (show == GOSSIP_SHOW_AWAY || show == GOSSIP_SHOW_EXT_AWAY) {
		/* Allow some slack before returning from away. */
		if (idle >= -BACK_SLACK && idle <= 0) {
			d(g_print ("Slack, do nothing.\n"));
			app_status_flash_start ();
		}
		else if (idle < -BACK_SLACK) {
			d(g_print ("No more slack, break interrupted.\n"));
			priv->auto_show = GOSSIP_SHOW_AVAILABLE;
			
			g_free (priv->away_message);
			priv->away_message = NULL;

			app_status_flash_stop ();
		}
		else if (idle > BACK_SLACK) {
			d(g_print ("Don't interrupt break.\n"));
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
#if 0
	GError        *error = NULL;
	gboolean       result;
#endif

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
	
	gossip_session_connect (priv->session);
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

static void
app_disconnect (void)
{
	GossipAppPriv *priv = app->priv;

	gossip_session_disconnect (priv->session);
	app_status_flash_stop ();
}

static void
app_setup_conn_dependent_menu_items (GladeXML *glade)
{
	const gchar *connect_widgets[] = {
		"actions_disconnect",
		"actions_join_group_chat",
		"actions_send_chat_message",
		"actions_add_contact",
		"status_button",
		"actions_configure_transports",
		"edit_personal_details"
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
	
	connected = gossip_session_is_connected (priv->session);

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
				/* FIXME: Display error dialog... */
				g_warning ("'%s' is not a valid ID or nick name.", str);
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
	return gossip_session_is_connected (app->priv->session);
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
	GtkWidget     *submenu;

	priv = app->priv;

	if (event->type == GDK_2BUTTON_PRESS ||
	    event->type == GDK_3BUTTON_PRESS) {
		return FALSE;
	}
	
	switch (event->button) {
	case 1:
		if (app_tray_pop_event ()) {
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

		submenu = app_create_status_menu (FALSE);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (priv->popup_menu_status_item),
					   submenu);
		
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

	if (!gossip_session_is_connected (priv->session)) {
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
	
	if (!gossip_session_is_connected (priv->session)) {
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
			status_label = g_strdup (gossip_utils_get_default_status_show (effective_show));
		}
	}

	if (status_text && status_text[0]) {
		status_label = status_text;
	} else {
		status_label = gossip_utils_get_default_status_show (effective_show);
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

	priv->auto_show = GOSSIP_SHOW_AVAILABLE;

	priv->leave_time = 0;
	app_status_flash_stop ();
	
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

	app_tray_flash_start ();
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
	
	if (priv->auto_show == GOSSIP_SHOW_AWAY || priv->auto_show == GOSSIP_SHOW_EXT_AWAY) {
		app_status_clear_away ();
	}
}

/* Note: test function for the dbus stuff. */
void
gossip_app_set_presence (GossipShow show, const gchar *status)
{
	GossipAppPriv *priv = app->priv;

	switch (show) {
	case GOSSIP_SHOW_AVAILABLE:
		priv->auto_show = GOSSIP_SHOW_AVAILABLE;
		priv->explicit_show = GOSSIP_SHOW_AVAILABLE;

		g_free (priv->status_text);
		priv->status_text = g_strdup (status);
		
		g_free (priv->away_message);
		priv->away_message = NULL;
		
		app_status_clear_away ();
		break;

	case GOSSIP_SHOW_BUSY:
		priv->auto_show = GOSSIP_SHOW_AVAILABLE;
		priv->explicit_show = GOSSIP_SHOW_BUSY;

		g_free (priv->status_text);
		priv->status_text = g_strdup (status);
		
		g_free (priv->away_message);
		priv->away_message = NULL;

		app_status_clear_away ();
		break;

	case GOSSIP_SHOW_AWAY:
	case GOSSIP_SHOW_EXT_AWAY:
		gossip_idle_reset ();
		priv->auto_show = GOSSIP_SHOW_AWAY;
		g_free (priv->away_message);
		priv->away_message = g_strdup (status);
		break;
	}		
	
	app_update_show ();
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
app_status_show_status_dialog (GossipShow   show,
			       const gchar *str,
			       gboolean     transient)
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

	if (transient) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog),
					      GTK_WINDOW (priv->window));
	}
	
	gtk_widget_grab_focus (entry);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_hide (dialog);

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}
		
	str = gtk_entry_get_text (GTK_ENTRY (entry));

	if (strcmp (str, gossip_utils_get_default_status_show (show)) == 0) {
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
	gboolean transient = GPOINTER_TO_INT (user_data);
	
	app_status_show_status_dialog (GOSSIP_SHOW_AVAILABLE,
				       _("Available"),
				       transient);
}

static void
app_status_custom_busy_activate_cb (GtkWidget *item,
				    gpointer   user_data)
{
	gboolean transient = GPOINTER_TO_INT (user_data);

	app_status_show_status_dialog (GOSSIP_SHOW_BUSY,
				       _("Busy"),
				       transient);
}

static void
app_status_custom_away_activate_cb (GtkWidget *item,
				    gpointer   user_data)
{
	gboolean transient = GPOINTER_TO_INT (user_data);

	app_status_show_status_dialog (GOSSIP_SHOW_AWAY,
				       _("Away"),
				       transient);
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
			    gboolean     custom,
			    gpointer     user_data)
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
					  user_data);
		} else {
			g_signal_connect (item,
					  "activate",
					  G_CALLBACK (app_status_available_activate_cb),
					  user_data);
		}
		break;
		
	case GOSSIP_SHOW_BUSY:
		stock = GOSSIP_STOCK_BUSY;

		if (custom) {
			g_signal_connect (item,
					  "activate",
					  G_CALLBACK (app_status_custom_busy_activate_cb),
					  user_data);
		} else {
			g_signal_connect (item,
					  "activate",
					  G_CALLBACK (app_status_busy_activate_cb),
					  user_data);
		}
		break;

	case GOSSIP_SHOW_AWAY:
		stock = GOSSIP_STOCK_AWAY;

		if (custom) {
			g_signal_connect (item,
					  "activate",
					  G_CALLBACK (app_status_custom_away_activate_cb),
					  user_data);
		} else {
			g_signal_connect (item,
					  "activate",
					  G_CALLBACK (app_status_away_activate_cb),
					  user_data);
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
app_create_status_menu (gboolean from_window)
{
	GtkWidget *menu;
	GtkWidget *item;
	GList     *list, *l;
	gpointer   transient;

	transient = GINT_TO_POINTER (from_window);
	
	menu = gtk_menu_new ();

	add_status_image_menu_item (menu, _("Available..."),
				    GOSSIP_SHOW_AVAILABLE,
				    TRUE,
				    transient);
	
	add_status_image_menu_item (menu, _("Busy..."),
				    GOSSIP_SHOW_BUSY,
				    TRUE,
				    transient);

	add_status_image_menu_item (menu, _("Away..."),
				    GOSSIP_SHOW_AWAY,
				    TRUE,
				    transient);
	
	/* Separator. */
	item = gtk_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	/* Preset messages. */
	list = gossip_utils_get_status_messages ();
	for (l = list; l; l = l->next) {
		GossipStatusEntry *entry = l->data;
		
		add_status_image_menu_item (menu, entry->string,
					    entry->show, FALSE,
					    NULL);
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
	
	menu = app_create_status_menu (TRUE);

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

static gboolean
app_tray_flash_timeout_func (gpointer data)
{
	GossipAppPriv   *priv;
	static gboolean  on = FALSE;
	const gchar     *icon = NULL;

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
			switch (priv->explicit_show) {
			case GOSSIP_SHOW_BUSY:
				icon = GOSSIP_STOCK_BUSY;
				break;
			case GOSSIP_SHOW_AVAILABLE:
				icon = GOSSIP_STOCK_AVAILABLE;
				break;
			default:
				g_assert_not_reached ();
				break;
			}
		}
	}
	else if (priv->tray_flash_icons != NULL) {
		if (on) {
			icon = GOSSIP_STOCK_MESSAGE;
		}
	}

	if (icon == NULL) {
		icon = app_get_current_status_icon ();
	}

	gtk_image_set_from_stock (GTK_IMAGE (priv->tray_image),
				  icon,
				  GTK_ICON_SIZE_MENU);
	
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

/* Stop if there are no flashing messages or status change. */
static void
app_tray_flash_maybe_stop (void)
{
	GossipAppPriv *priv = app->priv;
	
	if (priv->tray_flash_icons != NULL || priv->leave_time > 0) {
		return;
	}
	
	gtk_image_set_from_stock (GTK_IMAGE (priv->tray_image),
				  app_get_current_status_icon (),
				  GTK_ICON_SIZE_MENU);
	
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
		
	g_print ("Tray start blink\n");

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

	g_print ("Tray stop blink\n");
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

GossipSession *
gossip_app_get_session (void)
{
	return app->priv->session;
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

