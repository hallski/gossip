/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003      Imendio HB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
 * Copyright (C) 2002-2003 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2002-2003 CodeFactory AB
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
#include <string.h>
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
#include "gossip-app.h"

#include "gossip-startup-druid.h"

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
	
	GossipRoster        *roster;

	GHashTable          *one2one_chats;
	GHashTable          *group_chats;

	GossipAccount       *account;

	GossipJID           *jid;

	/* The status to set when we get a connection. */
	GossipStatus         status_to_set_on_connect;
	
	/* Dialogs that we only have one of at a time. */
	GossipJoinDialog    *join_dialog;
	GtkWidget           *about;
};

enum {
	CONNECTED,
	DISCONNECTED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void      app_class_init                     (GossipAppClass *klass);
static void      app_init                           (GossipApp      *app);
static void      app_finalize                       (GObject        *object);
static void      app_main_window_destroy_cb         (GtkWidget      *window,
						     GossipApp      *app);
static GossipGroupChat *
app_get_group_chat                                  (GossipApp      *app,
						     const gchar    *room,
						     const gchar    *server,
						     const gchar    *nick,
						     gboolean        create);
static GossipChat *
app_get_chat_for_jid                                (GossipApp      *app,
						     GossipJID      *jid);
static void      app_user_activated_cb              (GossipRoster   *roster,
						     GossipJID      *jid,
						     GossipApp      *app);
static void      app_set_status                     (GossipApp      *app,
						     GossipStatus    status);
static void      app_quit_cb                        (GtkWidget      *window,
						     GossipApp      *app);
static void      app_send_chat_message_cb           (GtkWidget      *widget,
						     GossipApp      *app);
static void      app_add_contact_cb                 (GtkWidget      *widget,
						     GossipApp      *app);
static void      app_about_cb                       (GtkWidget      *widget,
						     GossipApp      *app);
static void      app_join_group_chat_cb             (GtkWidget      *widget,
						     GossipApp      *app);
static void      app_connection_open_cb             (LmConnection   *connection,
						     gboolean        result,
						     GossipApp      *app);
static void      app_authentication_cb              (LmConnection   *connection,
						     gboolean        result,
						     GossipApp      *app);
static void      app_client_disconnected_cb         (LmConnection *connection,
						     LmDisconnectReason  reason, 
						     GossipApp    *app);
static void      app_connect_cb                     (GtkWidget      *widget,
						     GossipApp      *app);

static void      app_status_item_activated_cb       (GtkMenuItem    *item,
						     GossipApp      *app);
static LmHandlerResult
app_message_handler                                 (LmMessageHandler *handler,
						     LmConnection   *connection,
						     LmMessage      *m,
						     GossipApp      *app);
static LmHandlerResult
app_presence_handler                                (LmMessageHandler *handler,
						     LmConnection     *connection,
		 				     LmMessage        *m,
						     GossipApp        *app);
static LmHandlerResult
app_iq_handler                                      (LmMessageHandler *handler,
						     LmConnection     *connection,
						     LmMessage        *m,
						     GossipApp        *app);

static void      app_handle_subscription_request    (GossipApp      *app,
						     LmMessage      *m);
static void      app_set_status_indicator           (GossipApp      *app,
						     GossipStatus    status);
static void      app_tray_icon_destroy_cb           (GtkWidget      *widget,
						     GossipApp      *app);
static void      app_create_tray_icon               (GossipApp      *app);
static void      app_tray_icon_set_status           (GossipApp      *app,
						     GossipStatus    status);
static void      app_create_connection              (GossipApp      *app);

static void      app_disconnect                     (GossipApp      *app);

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
        GossipAppPriv    *priv;
	GladeXML         *glade;
	GtkWidget        *sw;

        priv = g_new0 (GossipAppPriv, 1);
        app->priv = priv;

	priv->account = gossip_account_get_default ();
	priv->status_to_set_on_connect = GOSSIP_STATUS_AVAILABLE;

	priv->one2one_chats = g_hash_table_new_full (g_str_hash,
						     g_str_equal,
						     g_free, NULL);

	priv->group_chats = g_hash_table_new_full (g_str_hash,
						   g_str_equal,
						   g_free,
						   NULL);

	glade = gossip_glade_get_file (GLADEDIR "/main.glade",
				       "main_window",
				       NULL,
				       "main_window", &priv->window,
				       "status_optionmenu", &priv->option_menu,
				       "roster_scrolledwindow", &sw,
				       NULL);

	gossip_glade_connect (glade,
			      app,
			      "file_quit", "activate", app_quit_cb,
			      "actions_connect", "activate", app_connect_cb,
			      "actions_join_group_chat", "activate", app_join_group_chat_cb,
			      "actions_send_chat_message", "activate", app_send_chat_message_cb,
			      "actions_add_contact", "activate", app_add_contact_cb,
			      "help_about", "activate", app_about_cb,
			      "main_window", "destroy", app_main_window_destroy_cb,
			      NULL);

	g_object_unref (glade);

	if (!priv->account) {
		gossip_startup_druid_run (priv->connection);
		priv->account = gossip_account_get_default ();
	}
	
	if (!priv->account) {
		g_error ("Get your act together dude!");
	}
		
	app_create_connection (app);

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
	
	app_create_tray_icon (app);
	app_set_status_indicator (app, GOSSIP_STATUS_OFFLINE);

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

	g_hash_table_destroy (priv->one2one_chats);

	gossip_account_unref (priv->account);
	
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
				       strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
				       NULL);
	
	g_object_add_weak_pointer (G_OBJECT (priv->about), (gpointer *) &priv->about);

	gtk_widget_show (priv->about);
}

static void
app_connect_cb (GtkWidget *window,
		 GossipApp *app)
{
	g_return_if_fail (GOSSIP_IS_APP (app));

	gossip_connect_dialog_show (app);
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
app_send_chat_message_cb (GtkWidget *widget,
			  GossipApp *app)
{
	/* FIXME: Start chat instead. */

	/*gossip_message_send_dialog_new (app, NULL, FALSE, NULL);*/
}

static void
app_add_contact_cb (GtkWidget *widget, GossipApp *app)
{
	gossip_add_contact_new (app->priv->connection, NULL);
	return;
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

typedef struct {
	GossipApp *app;
	GossipJID *jid;
} DestroyData;

static void
app_chat_dialog_destroy_cb (GtkWidget   *dialog,
			    DestroyData *data)
{
	g_hash_table_remove (data->app->priv->one2one_chats,
			     gossip_jid_get_without_resource (data->jid));
	gossip_jid_unref (data->jid);
	g_free (data);
}

static GossipChat *
app_get_chat_for_jid (GossipApp *app, GossipJID *jid)
{
	GossipAppPriv *priv;
	GossipChat    *chat;
	GtkWidget     *dialog;
	DestroyData   *data;
	
	g_return_val_if_fail (GOSSIP_IS_APP (app), NULL);

	priv = app->priv;

	chat = g_hash_table_lookup (priv->one2one_chats, 
				    gossip_jid_get_without_resource (jid));
	if (!chat) {
		/* FIXME: flash the tray icon and the roster icon for this user */
		chat = gossip_chat_new (app, jid);

		dialog = gossip_chat_get_dialog (chat);

		data = g_new (DestroyData, 1);
		data->app = app;
		data->jid = gossip_jid_ref (jid);
		
		g_signal_connect (dialog,
				  "destroy",
				  G_CALLBACK (app_chat_dialog_destroy_cb),
				  data);

		g_hash_table_insert (priv->one2one_chats,
				     g_strdup (gossip_jid_get_without_resource (jid)),
				     chat);
	}

	return chat;
}

static void
app_group_chat_destroy_cb (GtkWidget   *dialog,
			   DestroyData *data)
{
	g_hash_table_remove (data->app->priv->group_chats, 
			     gossip_jid_get_without_resource (data->jid));
	g_free (data);
}

static GossipGroupChat *
app_get_group_chat (GossipApp   *app,
		    const gchar *room,
		    const gchar *server,
		    const gchar *nick,
		    gboolean     create)
{
	GossipAppPriv   *priv;
	GossipGroupChat *chat;
	GtkWidget       *window;
	gchar           *str;
	DestroyData     *data;
	GossipJID       *jid;
	
	g_return_val_if_fail (GOSSIP_IS_APP (app), NULL);
	g_return_val_if_fail (room != NULL, NULL);
	g_return_val_if_fail (server != NULL, NULL);
	g_return_val_if_fail (!create || nick != NULL, NULL);

	priv = app->priv;

	str = g_strdup_printf ("%s@%s", room, server);
	jid = gossip_jid_new (str);
	g_free (str);
	
	chat = g_hash_table_lookup (priv->group_chats, 
				    gossip_jid_get_without_resource (jid));
	
	if (create && !chat) {
		data = g_new (DestroyData, 1);
		data->app = app;
		data->jid = jid;
		
		chat = gossip_group_chat_new (app, jid, nick);
		window = gossip_group_chat_get_window (chat);

		g_signal_connect (window,
				  "destroy",
				  G_CALLBACK (app_group_chat_destroy_cb),
				  data);
		
		g_hash_table_insert (priv->group_chats,
				     g_strdup (gossip_jid_get_without_resource (jid)),
				     chat);
	}

	return chat;
}

static LmHandlerResult
app_message_handler (LmMessageHandler *handler,
		     LmConnection     *connection,
		     LmMessage        *m,
		     GossipApp        *app)
{
	GossipAppPriv    *priv;
	const gchar      *from;
	GossipChat       *chat;
	LmMessageSubType  type;
	GossipJID        *jid;
	
	g_return_val_if_fail (connection != NULL,
			      LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS);
	g_return_val_if_fail (GOSSIP_IS_APP (app),
			      LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS);

	priv = app->priv;

	d(g_print ("App handle message\n"));

	type = lm_message_get_sub_type (m);
	from = lm_message_node_get_attribute (m->node, "from");
	
	switch (type) {
	case LM_MESSAGE_SUB_TYPE_AVAILABLE:
		gossip_message_handle_message (app, m);
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
		break;
	case LM_MESSAGE_SUB_TYPE_CHAT:
		jid = gossip_jid_new (from);
		chat = app_get_chat_for_jid (app, jid);
		gossip_jid_unref (jid);
		gossip_chat_append_message (chat, m);
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
		break;
	case LM_MESSAGE_SUB_TYPE_GROUPCHAT:
		g_print ("Hmm .. looks like an unhandled group chat message from %s, "
			 "this needs to be taken care of\n", from);
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
		break;
	case LM_MESSAGE_SUB_TYPE_HEADLINE:
		g_print ("Unhandled headline!\n");
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
		break;
	case LM_MESSAGE_SUB_TYPE_ERROR:
		g_warning ("Unhandled error from: %s", from);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
		break;
	default: 
		g_assert_not_reached ();
	}

	d(g_print ("Unhandled message of type: %d\n", lm_message_get_type (m)));

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
	
	g_return_val_if_fail (connection != NULL,
			      LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS);
	g_return_val_if_fail (GOSSIP_IS_APP (app),
			      LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS);

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
			gchar          *os;

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
			os = g_strdup_printf ("%s %s %s",
					      osinfo.sysname,
					      osinfo.release,
					      osinfo.machine);
			lm_message_node_add_child (node, "os", os);
			g_free (os);

			lm_connection_send (connection, v, NULL);
			lm_message_unref (v);
		}
	}

	d(g_print ("\n"));

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
app_password_dialog_activate_cb (GtkWidget *entry, gpointer dialog)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static void
app_connection_open_cb (LmConnection *connection,
			gboolean      result,
			GossipApp    *app)
{
	GossipAppPriv *priv;
	GtkWidget     *dialog;
	GtkWidget     *hbox;
	GtkWidget     *entry;

	g_return_if_fail (connection != NULL);
	g_return_if_fail (GOSSIP_IS_APP (app));

	priv = app->priv;

	if (result != TRUE) {
		g_warning ("Connection failed, handle this!");
	}

	if (!priv->account->password || 
	    strlen (priv->account->password) == 0) {
		dialog = gtk_message_dialog_new (GTK_WINDOW (priv->window),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_QUESTION,
						 GTK_BUTTONS_OK_CANCEL,
						 _("Please enter your password:"));

		entry = gtk_entry_new ();
		gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE); 
		gtk_widget_show (entry);

		g_signal_connect (entry,
				  "activate",
				  G_CALLBACK (app_password_dialog_activate_cb),
				  dialog);
		
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (hbox);
		
		gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 4);
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, FALSE, TRUE, 4);
		
		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
			priv->account->password = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
		} else {
			gtk_widget_destroy (dialog);
		}
		
		gtk_widget_destroy (dialog);
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
				    priv->account->password,
				    priv->account->resource,
				    (LmResultFunction) app_authentication_cb,
				    app, NULL, NULL);
}

static void
app_client_disconnected_cb (LmConnection *connection,
			    LmDisconnectReason  reason, 
			    GossipApp    *app)
{
	GossipAppPriv *priv;
	GtkWidget     *dialog;
	GtkWidget     *menu, *item;
	GossipStatus   old_status;

	priv = app->priv;

	/* Keep the old status if we are going to reconnect. */
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->option_menu));
	item = gtk_menu_get_active (GTK_MENU (menu));
	old_status = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "data"));

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
			app_set_status (app, old_status);
		}
	
		gtk_widget_destroy (dialog);
	}
}

static void
app_user_activated_cb (GossipRoster *roster,
		       GossipJID    *jid,
		       GossipApp    *app)
{
	app_get_chat_for_jid (app, jid);
}

void
gossip_app_join_group_chat (GossipApp   *app,
			    const gchar *room,
			    const gchar *server,
			    const gchar *nick)
{
	app_get_group_chat (app, room, server, nick, TRUE);
}

void
gossip_app_connect (GossipAccount *account)
{
	GossipAppPriv *priv;

	if (!account) {
		/* Handle this better */
		return;
	}
	
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
	
	lm_connection_open (priv->connection,
			    (LmResultFunction) app_connection_open_cb,
			    app, NULL, NULL);
}

void
gossip_app_connect_default ()
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

typedef struct {
	GossipApp *app;
	LmMessage *m;
} ForeachSetStatusData;

static void
app_set_status_group_chat_foreach (gchar                *key,
				   gpointer              value,
				   ForeachSetStatusData *data)
{
	GossipAppPriv *priv = data->app->priv;
	LmMessage     *m = data->m;
	
	lm_message_node_set_attributes (m->node, "to", key, NULL);
	lm_connection_send (priv->connection, m, NULL);
};

static void
app_set_status (GossipApp *app, GossipStatus status)
{
	GossipAppPriv        *priv;
	LmMessage            *m;
	ForeachSetStatusData *data;
	
	priv = app->priv;

	d(g_print ("app_set_status\n"));

	switch (status) {
	case GOSSIP_STATUS_OFFLINE:
	case GOSSIP_STATUS_EXT_AWAY:
	case GOSSIP_STATUS_AWAY:
	case GOSSIP_STATUS_BUSY:
		gossip_sound_set_silent (TRUE);
		break;
	default:
		gossip_sound_set_silent (FALSE);
	}
	
	/* Disconnect. */
	if (status == GOSSIP_STATUS_OFFLINE) {
		if (lm_connection_is_open (priv->connection)) {
			app_disconnect (app);
			app_set_status_indicator (app, GOSSIP_STATUS_OFFLINE);
		}
					     
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
	
	lm_message_node_add_child (m->node, 
				   "show", gossip_status_to_string (status));
	lm_message_node_add_child (m->node,
				   "status", "Online");
	lm_connection_send (priv->connection, m, NULL);
	
	data = g_new0 (ForeachSetStatusData, 1);
	data->app = app;
	data->m = m;

	g_hash_table_foreach (priv->group_chats,
			      (GHFunc) app_set_status_group_chat_foreach,
			      data);
	g_free (data);
	lm_message_unref (m);
	
	app_set_status_indicator (app, status);
}

const gchar *
gossip_app_get_username (GossipApp *app)
{
	g_return_val_if_fail (GOSSIP_IS_APP (app), NULL);
	
	return app->priv->account->username;
}

GossipJID *
gossip_app_get_jid (GossipApp *app)
{
	g_return_val_if_fail (GOSSIP_IS_APP (app), NULL);
	
	return app->priv->jid;
}

GossipRoster *
gossip_app_get_roster ()
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

static void
app_tray_icon_button_press_cb (GtkWidget      *widget, 
			       GdkEventButton *event, 
			       GossipApp      *app)
{
	GossipAppPriv *priv;
	gboolean       visible;
	static gint    x, y;

	priv = app->priv;

	g_object_get (priv->window,
		      "visible", &visible,
		      NULL);

	if (visible) {
		gtk_window_get_position (GTK_WINDOW(priv->window), &x, &y);
		gtk_widget_hide (priv->window);
	} else {
		gtk_widget_show (priv->window);
		gtk_window_move (GTK_WINDOW(priv->window), x, y);
	}
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
		
	gtk_container_add (GTK_CONTAINER (priv->tray_event_box), priv->tray_image);

	gtk_widget_show (priv->tray_event_box);
	gtk_widget_show (priv->tray_image);

	gtk_container_add (GTK_CONTAINER (priv->tray_icon), priv->tray_event_box);
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

static void
app_create_connection (GossipApp *app)
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
	
	if (priv->connection &&
	    lm_connection_is_open (priv->connection)) {
		lm_connection_close (priv->connection, NULL);
	}
}

	LmConnection *
gossip_app_get_connection (GossipApp *app)
{
	g_return_val_if_fail (GOSSIP_IS_APP (app), NULL);
	
	return app->priv->connection;
}

#if 0
static void app_tray_icon_push_notification (GossipApp   *app,
					     const gchar *label);
static void app_tray_icon_pop_notification (GossipApp   *app);

static void
app_tray_icon_push_notification (GossipApp   *app,
				 const gchar *label)
{

}

static void
app_tray_icon_pop_notification (GossipApp   *app)
{

}

#endif

