/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003      Imendio HB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
 * Copyright (C) 2002-2003 Mikael Hallendal <micke@imendio.com>
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-href.h>
#include <gconf/gconf-client.h>
#include <loudmouth/loudmouth.h>
#include "gossip-chat-window.h"
#include "gossip-utils.h"
#include "gossip-sound.h"
#include "gossip-chat-view.h"
#include "gossip-app.h"
#include "gossip-contact-info.h"
#include "gossip-stock.h"
#include "disclosure-widget.h"
#include "gossip-log.h"
#include "gossip-chat.h"

#define d(x)
#define IS_ENTER(v) (v == GDK_Return || v == GDK_ISO_Enter || v == GDK_KP_Enter)
#define COMPOSING_STOP_TIMEOUT 5

static void            gossip_chat_class_init            (GossipChatClass  *klass);
static GObject *       gossip_chat_constructor		 (GType		         type,
							  guint		         n_construct_params,
							  GObjectConstructParam *construct_params);
static void            gossip_chat_init                  (GossipChat       *chat);
static void            gossip_chat_finalize              (GObject          *object);
static void            gossip_chat_set_property          (GObject          *object,  
                                                          guint             prop_id,
                                                          const GValue     *value,
                                                          GParamSpec       *pspec);
static void            gossip_chat_get_property          (GObject          *object,
                                                          guint             prop_id,
                                                          GValue           *value,
                                                          GParamSpec       *pspec);

static void            chats_init                        (void);
static void            chat_create_gui                   (GossipChat       *chat);
static GossipChat *    chat_get_for_jid                  (GossipJID        *jid);
static void            chat_send                         (GossipChat       *chat,
                                                          const gchar      *msg);
static void            chat_request_composing            (LmMessage        *m);
static void            chat_composing_start              (GossipChat       *chat);
static void            chat_composing_stop               (GossipChat       *chat);
static void            chat_composing_send_start_event   (GossipChat       *chat);
static void            chat_composing_send_stop_event    (GossipChat       *chat);
static void            chat_composing_remove_timeout     (GossipChat       *chat);
static gboolean        chat_composing_stop_timeout_cb    (GossipChat       *chat);
static LmHandlerResult chat_message_handler              (LmMessageHandler *handler,
                                                          LmConnection     *connection,
                                                          LmMessage        *m,
                                                          gpointer          user_data);
static LmHandlerResult chat_presence_handler             (LmMessageHandler *handler,
                                                          LmConnection     *connection,
                                                          LmMessage        *m,
                                                          gpointer          user_data);
static gboolean        chat_event_handler                (GossipChat       *chat,
                                                          LmMessage        *m);
static void            chat_error_dialog                 (GossipChat       *chat,
                                                          const gchar      *msg);
static void            chat_set_window                   (GossipChat       *chat,
                                                          GossipChatWindow *window);

static void            chat_disclosure_toggled_cb        (GtkToggleButton  *disclosure,
                                                          GossipChat       *chat);
static void	       chat_send_multi_clicked_cb	 (GtkWidget	   *button,
							  GossipChat	   *chat);
static void            chat_input_activate_cb            (GtkWidget        *entry,
                                                          GossipChat       *chat);
static gboolean        chat_input_key_press_event_cb     (GtkWidget        *widget,
                                                          GdkEventKey      *event,
                                                          GossipChat       *chat);
static void            chat_input_entry_changed_cb       (GtkWidget        *widget,
                                                          GossipChat       *chat);
static void            chat_input_text_buffer_changed_cb (GtkTextBuffer    *buffer,
                                                          GossipChat       *chat);
static void	       chat_presence_changed_cb		 (GossipChat       *chat);
static void	       chat_window_layout_changed_cb	 (GossipChatWindow       *window,
							  GossipChatWindowLayout  layout,
							  GossipChat		 *chat);
static gboolean	       chat_text_view_focus_in_event_cb  (GtkWidget        *widget,
							  GdkEvent	   *event,
							  GossipChat       *chat);
static void	       chat_composing_cb		 (GossipChat	   *chat,
							  gboolean	    composing);
static gboolean	       chat_delete_event_cb		 (GtkWidget	   *widget,
							  GdkEvent	   *event,
							  GossipChat       *chat);

GtkWidget *            chat_create_disclosure            (gpointer          data);

struct _GossipChatPriv {
        LmMessageHandler *presence_handler;
        LmMessageHandler *message_handler;

        GossipChatWindow *window;

        GtkWidget        *widget;
	GtkWidget	 *text_view_sw;
	GtkWidget	 *status_box;
        GtkWidget        *input_entry;
        GtkWidget        *input_text_view;
        GtkWidget        *single_hbox;
        GtkWidget        *multi_vbox;
        GtkWidget        *send_multi_button;
        GtkWidget        *subject_entry;
        GtkWidget        *status_image;
        GtkWidget        *disclosure;
        GtkWidget        *from_label;

	GossipChatView   *view;

        GtkTooltips      *tooltips;
        GtkWidget        *from_eventbox;

        GossipJID        *jid;
        gchar            *nick;
	gboolean	  priv_group_chat;

        guint             composing_stop_timeout_id;
        gboolean          request_composing_events;
        gboolean          send_composing_events;
        gchar            *last_composing_id;

	gboolean	  other_offline;
	GossipShow	  other_show;	
};

enum {
        PROP_0,
        PROP_JID,
        PROP_PRIV_GROUP_CHAT,
        PROP_WINDOW,
	PROP_OTHER_SHOW,
	PROP_OTHER_OFFLINE
};

enum {
        COMPOSING,
        NEW_MESSAGE,
        PRESENCE_CHANGED,
        LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;
static guint         chat_signals[LAST_SIGNAL] = { 0 };
static GHashTable   *chats = NULL;

GType
gossip_chat_get_type (void)
{
        static GType type_id = 0;

        if (type_id == 0) {
                const GTypeInfo type_info = {
                        sizeof (GossipChatClass),
                        NULL,
                        NULL,
                        (GClassInitFunc) gossip_chat_class_init,
                        NULL,
                        NULL,
                        sizeof (GossipChat),
                        0,
                        (GInstanceInitFunc) gossip_chat_init
                };

                type_id = g_type_register_static (G_TYPE_OBJECT,
                                                  "GossipChat",
                                                  &type_info,
                                                  0);
        }

        return type_id;
}

static void
gossip_chat_class_init (GossipChatClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        parent_class = g_type_class_peek_parent (klass);

	object_class->constructor = gossip_chat_constructor;
        object_class->finalize = gossip_chat_finalize;
        object_class->set_property = gossip_chat_set_property;
        object_class->get_property = gossip_chat_get_property;

        g_object_class_install_property (object_class,
                                         PROP_JID,
                                         g_param_spec_pointer ("jid",
                                                               "jid",
                                                               "jid",
                                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property (object_class,
                                         PROP_PRIV_GROUP_CHAT,
                                         g_param_spec_boolean ("priv-group-chat",
                                                               "priv-group-chat",
                                                               "priv-group-chat",
                                                               FALSE,
                                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property (object_class,
                                         PROP_WINDOW,
                                         g_param_spec_object ("window",
                                                              "window",
                                                              "window",
                                                              GOSSIP_TYPE_CHAT_WINDOW,
                                                              G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_OTHER_SHOW,
					 g_param_spec_int ("other-show",
							   "other-show",
							   "other-show",
							   0,
							   G_MAXINT,
							   GOSSIP_SHOW_AVAILABLE,
							   G_PARAM_READABLE));
	
	g_object_class_install_property (object_class,
					 PROP_OTHER_OFFLINE,
					 g_param_spec_boolean ("other-offline",
							       "other-offline",
							       "other-offline",
							       TRUE,
							       G_PARAM_READABLE));

        chat_signals[COMPOSING] =
                g_signal_new ("composing",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GossipChatClass, composing),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__BOOLEAN,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_BOOLEAN);

	chat_signals[NEW_MESSAGE] =
		g_signal_new ("new-message",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GossipChatClass, new_message),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	chat_signals[PRESENCE_CHANGED] =
		g_signal_new ("presence-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GossipChatClass, presence_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
}

static void
gossip_chat_init (GossipChat *chat)
{
        chat->priv = g_new0 (GossipChatPriv, 1);

	chat->priv->window = NULL;

	chat->priv->request_composing_events = TRUE;

        chat_create_gui (chat);

	g_signal_connect (chat,
			  "presence-changed",
			  G_CALLBACK (chat_presence_changed_cb),
			  NULL);
}

static GObject *
gossip_chat_constructor (GType                  type,
			 guint                  n_construct_params,
			 GObjectConstructParam *construct_params)
{
	GossipRosterOld  *roster;
	GdkPixbuf	 *pixbuf;
	gchar		 *name;
	GossipChat       *chat;
	GObject          *object;
	LmMessageHandler *handler;
	LmConnection     *connection;
	const gchar	 *without_resource;

	/* call parent class constructor */
	object = (*G_OBJECT_CLASS (parent_class)->constructor)
		(type, n_construct_params, construct_params);

	chat = GOSSIP_CHAT (object);

	gossip_jid_ref (chat->priv->jid);

	chats_init ();
	without_resource = gossip_jid_get_without_resource (chat->priv->jid);
        g_hash_table_insert (chats, g_strdup (without_resource), chat);

	roster = gossip_app_get_roster ();
	chat->priv->other_offline = 
		gossip_roster_old_get_is_offline (roster, chat->priv->jid);
	chat->priv->other_show = gossip_roster_old_get_show (roster, 
							     chat->priv->jid);

	pixbuf = gossip_roster_old_get_status_pixbuf_for_jid (roster, 
							      chat->priv->jid);

	if (pixbuf) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (chat->priv->status_image),
					   pixbuf);
		g_object_unref (pixbuf);
	}

	name = g_strdup (gossip_roster_old_get_nick_from_jid (roster, 
							      chat->priv->jid));
	if (!name) {
		name = gossip_jid_get_part_name (chat->priv->jid);
	}

	gtk_label_set_text (GTK_LABEL (chat->priv->from_label), name);
	chat->priv->nick = name;

	gtk_tooltips_set_tip (chat->priv->tooltips,
			      chat->priv->from_eventbox,
			      gossip_jid_get_full (chat->priv->jid),
			      gossip_jid_get_full (chat->priv->jid));

        connection = gossip_app_get_connection ();

        handler = lm_message_handler_new (chat_presence_handler, chat, NULL);
        chat->priv->presence_handler = handler; 
        lm_connection_register_message_handler (connection,
                                                handler,
                                                LM_MESSAGE_TYPE_PRESENCE,
                                                LM_HANDLER_PRIORITY_NORMAL);

	if (!chat->priv->priv_group_chat) {
		handler = lm_message_handler_new (chat_message_handler, chat,
						  NULL);
		chat->priv->message_handler = handler;
		lm_connection_register_message_handler (connection,
							handler,
							LM_MESSAGE_TYPE_MESSAGE,
							LM_HANDLER_PRIORITY_NORMAL);
	}

	return object;
}

static void
gossip_chat_finalize (GObject *object)
{
        GossipChat       *chat = GOSSIP_CHAT (object);
        LmConnection     *connection;
        LmMessageHandler *handler;

        connection = gossip_app_get_connection ();

        handler = chat->priv->presence_handler;
        if (handler) {
                lm_connection_unregister_message_handler (
                                connection, handler, LM_MESSAGE_TYPE_PRESENCE);
                lm_message_handler_unref (handler);
        }

        handler = chat->priv->message_handler;
        if (handler) {
                lm_connection_unregister_message_handler (
                                connection, handler, LM_MESSAGE_TYPE_MESSAGE);
                lm_message_handler_unref (handler);
        }

        gossip_jid_unref (chat->priv->jid);
	g_object_unref (chat->priv->widget);
        
        g_free (chat->priv);
        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gossip_chat_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
        GossipChat *chat = GOSSIP_CHAT (object);

        switch (prop_id) {
        case PROP_JID:
		chat->priv->jid = g_value_get_pointer (value);
                break;
        case PROP_PRIV_GROUP_CHAT:
		chat->priv->priv_group_chat = g_value_get_boolean (value);
                break;
        case PROP_WINDOW:
                chat_set_window (chat, g_value_get_object (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gossip_chat_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
        GossipChat *chat = GOSSIP_CHAT (object);

        switch (prop_id) {
        case PROP_JID: 
                g_value_set_pointer (value, chat->priv->jid);
                break;
        case PROP_PRIV_GROUP_CHAT:
		g_value_set_boolean (value, chat->priv->priv_group_chat);
                break;
        case PROP_WINDOW:
                g_value_set_object (value, chat->priv->window);
                break;
	case PROP_OTHER_SHOW:
		g_value_set_int (value, chat->priv->other_show);
		break;
	case PROP_OTHER_OFFLINE:
		g_value_set_boolean (value, chat->priv->other_offline);
		break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
chats_init (void)
{
        static gboolean inited = FALSE;

	if (inited == TRUE) {
		return;
	}

        inited = TRUE;

        chats = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       (GDestroyNotify) g_free,
                                       (GDestroyNotify) g_object_unref);
}

static void
chat_create_gui (GossipChat *chat)
{
        GossipRosterOld *roster;
        GtkTextBuffer   *buffer;

        gossip_glade_get_file_simple (GLADEDIR "/chat.glade",
                                      "chat_widget",
                                      NULL,
                                      "chat_widget", &chat->priv->widget,
				      "status_box", &chat->priv->status_box,
                                      "chat_view_sw", &chat->priv->text_view_sw,
                                      "input_entry", &chat->priv->input_entry,
                                      "input_textview", &chat->priv->input_text_view,
                                      "single_hbox", &chat->priv->single_hbox,
                                      "multi_vbox", &chat->priv->multi_vbox,
                                      "status_image", &chat->priv->status_image,
                                      "from_eventbox", &chat->priv->from_eventbox,
                                      "from_label", &chat->priv->from_label,
                                      "disclosure", &chat->priv->disclosure,
                                      "send_multi_button", &chat->priv->send_multi_button,
                                      NULL);

	chat->priv->view = gossip_chat_view_new ();
	gtk_container_add (GTK_CONTAINER (chat->priv->text_view_sw),
			   GTK_WIDGET (chat->priv->view));
	gtk_widget_show (GTK_WIDGET (chat->priv->view));

	g_object_ref (chat->priv->widget);

	g_object_set_data (G_OBJECT (chat->priv->widget), "chat", chat);

        roster = gossip_app_get_roster ();

	chat->priv->tooltips = gtk_tooltips_new ();

        g_signal_connect (chat->priv->disclosure,
                          "toggled",
                          G_CALLBACK (chat_disclosure_toggled_cb),
                          chat);
	g_signal_connect (chat->priv->send_multi_button,
			  "clicked",
			  G_CALLBACK (chat_send_multi_clicked_cb),
			  chat);
        g_signal_connect (chat->priv->input_entry,
                          "activate",
                          G_CALLBACK (chat_input_activate_cb),
                          chat);
        g_signal_connect (chat->priv->input_entry,
                          "changed",
                          G_CALLBACK (chat_input_entry_changed_cb),
                          chat);
	g_signal_connect (chat->priv->input_text_view,
			  "key_press_event",
			  G_CALLBACK (chat_input_key_press_event_cb),
			  chat);
        g_signal_connect (chat->priv->input_entry,
                          "key_press_event",
                          G_CALLBACK (chat_input_key_press_event_cb),
                          chat);

        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->priv->input_text_view));
        g_signal_connect (buffer,
                          "changed",
                          G_CALLBACK (chat_input_text_buffer_changed_cb),
                          chat);
	g_signal_connect (chat->priv->view,
			  "focus_in_event",
			  G_CALLBACK (chat_text_view_focus_in_event_cb),
			  chat);
	g_signal_connect (chat,
			  "composing",
			  G_CALLBACK (chat_composing_cb),
			  NULL);
	g_signal_connect (chat->priv->widget,
			  "delete_event",
			  G_CALLBACK (chat_delete_event_cb),
			  NULL);

	gossip_chat_view_set_margin (chat->priv->view, 3);

	gtk_widget_grab_focus (chat->priv->input_entry);
}

static void
chat_send (GossipChat  *chat,
           const gchar *msg)
{
        LmMessage *m;
        gchar     *nick;

        if (msg == NULL || msg[0] == '\0') {
                return;
        }

        if (g_ascii_strcasecmp (msg, "/clear") == 0) {
                GtkTextBuffer *buffer;

                buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->priv->view));
                gtk_text_buffer_set_text (buffer, "", 1);

                return;
        }

        chat_composing_remove_timeout (chat);

        nick = gossip_jid_get_part_name (gossip_app_get_jid ());

        gossip_chat_view_append_chat_message (chat->priv->view,
                                              NULL,
                                              gossip_app_get_username (),
                                              nick,
                                              msg);

        g_free (nick);

        m = lm_message_new_with_sub_type (gossip_jid_get_full (chat->priv->jid),
                                          LM_MESSAGE_TYPE_MESSAGE,
                                          LM_MESSAGE_SUB_TYPE_CHAT);
        lm_message_node_add_child (m->node, "body", msg);
        
        if (chat->priv->request_composing_events) {
                chat_request_composing (m);
        }

	gossip_log_message (m, FALSE);

        lm_connection_send (gossip_app_get_connection (), m, NULL);
        lm_message_unref (m);
}

static GossipChat *
chat_get_for_jid (GossipJID *jid)
{
        GossipChat       *chat;
        const gchar      *without_resource;

        chats_init ();

        without_resource = gossip_jid_get_without_resource (jid);
        chat = g_hash_table_lookup (chats, without_resource);

        if (chat) {
                return chat;
        } else {
		return NULL;
	}
}

static void
chat_request_composing (LmMessage *m)
{
        LmMessageNode *x;

        x = lm_message_node_add_child (m->node, "x", NULL);

        lm_message_node_set_attribute (x,
                                       "xmlns",
                                       "jabber:x:event");
        lm_message_node_add_child (x, "composing", NULL);
}

static void
chat_composing_start (GossipChat *chat)
{
        if (!chat->priv->send_composing_events) {
                return;
        }

        if (chat->priv->composing_stop_timeout_id) {
                /* Just restart the timeout */
                chat_composing_remove_timeout (chat);
        } else {
                chat_composing_send_start_event (chat);
        }

        chat->priv->composing_stop_timeout_id = g_timeout_add (
                        1000 * COMPOSING_STOP_TIMEOUT,
                        (GSourceFunc) chat_composing_stop_timeout_cb,
                        chat);
}

static void
chat_composing_stop (GossipChat *chat)
{
	if (!chat->priv->send_composing_events) {
		return;
	}

	chat_composing_remove_timeout (chat);
	chat_composing_send_stop_event (chat);
}

static void
chat_composing_send_start_event (GossipChat *chat)
{
        LmConnection  *connection;
        LmMessage     *m;
        LmMessageNode *x;

        connection = gossip_app_get_connection ();
        if (!lm_connection_is_open (connection)) {
                return;
        }

        m = lm_message_new_with_sub_type (gossip_jid_get_full (chat->priv->jid),
                                          LM_MESSAGE_TYPE_MESSAGE,
                                          LM_MESSAGE_SUB_TYPE_CHAT);
        x = lm_message_node_add_child (m->node, "x", NULL);
        lm_message_node_set_attribute (x, "xmlns", "jabber:x:event");
        lm_message_node_add_child (x, "id", chat->priv->last_composing_id);
        lm_message_node_add_child (x, "composing", NULL);

        lm_connection_send (connection, m, NULL);
        lm_message_unref (m);
}

static void
chat_composing_send_stop_event (GossipChat *chat)
{
        LmMessage     *m;
        LmMessageNode *x;
        LmConnection  *connection;

        connection = gossip_app_get_connection ();
        if (!lm_connection_is_open (connection)) {
                return;
        }

        m = lm_message_new_with_sub_type (gossip_jid_get_full (chat->priv->jid),
                                          LM_MESSAGE_TYPE_MESSAGE,
                                          LM_MESSAGE_SUB_TYPE_CHAT);
        x = lm_message_node_add_child (m->node, "x", NULL);
        lm_message_node_set_attribute (x, "xmlns", "jabber:x:event");
        lm_message_node_add_child (x, "id", chat->priv->last_composing_id);

        lm_connection_send (connection, m, NULL);
        lm_message_unref (m);
}

static void
chat_composing_remove_timeout (GossipChat *chat)
{
        if (chat->priv->composing_stop_timeout_id) {
                g_source_remove (chat->priv->composing_stop_timeout_id);
                chat->priv->composing_stop_timeout_id = 0;
        }
}

static gboolean
chat_composing_stop_timeout_cb (GossipChat *chat)
{
        chat->priv->composing_stop_timeout_id = 0;
        chat_composing_send_stop_event (chat);

        return FALSE;
}

static LmHandlerResult
chat_message_handler (LmMessageHandler *handler,
                      LmConnection     *connection,
                      LmMessage        *m,
                      gpointer          user_data)
{
        GossipChat       *chat = GOSSIP_CHAT (user_data);
        const gchar      *from;
        LmMessageSubType  type;
        GossipJID        *jid;
        const gchar      *timestamp = NULL;
        LmMessageNode    *node;
        const gchar      *body = "";
        const gchar      *thread = "";
        gchar            *nick;

        from = lm_message_node_get_attribute (m->node, "from");

        jid = gossip_jid_new (from);

        d(g_print ("Incoming message:: '%s' ?= '%s'",
                   gossip_jid_get_without_resource (jid),
                   gossip_jid_get_without_resource (chat->priv->jid)));

        if (!gossip_jid_equals_without_resource (jid, chat->priv->jid)) {
                gossip_jid_unref (jid);
                return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
        }

        type = lm_message_get_sub_type (m);

        if (type == LM_MESSAGE_SUB_TYPE_ERROR) {
                gchar     *tmp, *str, *msg;

                tmp = g_strdup_printf ("<b>%s</b>", from);
                str = g_strdup_printf (_("An error occurred when chatting with %s."), tmp);
                g_free (tmp);

                node = lm_message_node_get_child (m->node, "error");
                if (node && node->value && node->value[0]) {
                        msg = g_strconcat (str, "\n\n", _("Details:"), " ", node->value, NULL);
                        g_free (str);
                } else {
                        msg = str;
                }

                chat_error_dialog (chat, msg);

                g_free (msg);
                gossip_jid_unref (jid);

                return LM_HANDLER_RESULT_REMOVE_MESSAGE;
        }

        if (chat_event_handler (chat, m)) {
                gossip_jid_unref (jid);
                return LM_HANDLER_RESULT_REMOVE_MESSAGE;
        }

        timestamp = gossip_utils_get_timestamp_from_message (m);

        node = lm_message_node_get_child (m->node, "body");
        if (node) {
                body = node->value;
        }

        node = lm_message_node_get_child (m->node, "thread");
        if (node) {
                thread = node->value;
        }

        if (chat->priv->nick) {
                nick = g_strdup (chat->priv->nick);
        } else {
                nick = g_strdup (gossip_roster_old_get_nick_from_jid (gossip_app_get_roster (), jid));
        }
        if (!nick) {
                nick = gossip_jid_get_part_name (jid);
        }

	gossip_log_message (m, TRUE);

        gossip_chat_view_append_chat_message (chat->priv->view,
                                              timestamp,
                                              gossip_app_get_username (),
                                              nick,
                                              body);

        g_free (nick);
        gossip_jid_unref (jid);

	g_signal_emit (chat, chat_signals[NEW_MESSAGE], 0);
	
	if (chat->priv->window == NULL) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

        return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult
chat_presence_handler (LmMessageHandler *handler,
                       LmConnection     *connection,
                       LmMessage        *m,
                       gpointer          user_data)
{
        GossipChat    *chat = GOSSIP_CHAT (user_data);
        const gchar   *type;
        const gchar   *show = NULL;
        const gchar   *from;
        GossipJID     *jid;
        LmMessageNode *node;

        type = lm_message_node_get_attribute (m->node, "type");
        if (!type) {
                type = "available";
        }

        node = lm_message_node_get_child (m->node, "show");
        if (node) {
                show = node->value;
        }

        from = lm_message_node_get_attribute (m->node, "from");
        jid = gossip_jid_new (from);

        if (!gossip_jid_equals_without_resource (jid, chat->priv->jid)) {
                gossip_jid_unref (jid);
                return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
        }

	if (strcmp (type, "unavailable") == 0 || strcmp (type, "error") == 0) {
		gchar *event_msg;

		chat_composing_remove_timeout (chat);
		chat->priv->send_composing_events = FALSE;
		g_signal_emit (chat, chat_signals[COMPOSING], 0, FALSE);
		
		if (chat->priv->window != NULL && !chat->priv->other_offline) {
			event_msg = g_strdup_printf (_("%s went offline"),
						     chat->priv->nick);
			gossip_chat_view_append_event_msg (chat->priv->view,
							   event_msg, TRUE);
			g_free (event_msg);
		}

		chat->priv->other_offline = TRUE;
	} else {
		GossipRosterOld *roster;
		gchar		*event_msg;

		roster = gossip_app_get_roster ();

		if (chat->priv->window != NULL && !chat->priv->other_offline) {
			event_msg = g_strdup_printf (_("%s comes online"),
						     chat->priv->nick);
			gossip_chat_view_append_event_msg (chat->priv->view,
							   event_msg, TRUE);
			g_free (event_msg);
		}

		chat->priv->other_show = gossip_show_from_string (show);
		chat->priv->other_offline = FALSE;
	}

	g_signal_emit (chat, chat_signals[PRESENCE_CHANGED], 0);

        gossip_jid_unref (jid);

        return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static gboolean
chat_event_handler (GossipChat *chat,
                    LmMessage  *m)
{
        LmMessageNode *x;
        const gchar   *xmlns;
        const gchar   *new_id;

        x = lm_message_node_get_child (m->node, "x");
        if (!x) {
                return FALSE;
        }

        xmlns = lm_message_node_get_attribute (x, "xmlns");
        if (strcmp (xmlns, "jabber:x:event") != 0) {
                return FALSE;
        }

        if (lm_message_node_get_child (m->node, "body")) {
                if (lm_message_node_get_child (x, "composing")) {
                        /* Handle request for composing events. */
                        chat->priv->send_composing_events = TRUE;
                        
                        g_free (chat->priv->last_composing_id);
                        new_id = lm_message_node_get_attribute (m->node, "id");
                        if (new_id) {
                                chat->priv->last_composing_id = g_strdup (new_id);
                        }
                }

                g_signal_emit (chat, chat_signals[COMPOSING], 0, FALSE);

                return FALSE;
        }

        if (lm_message_node_get_child (x, "composing")) {
                g_signal_emit (chat, chat_signals[COMPOSING], 0, TRUE);
        } else {
                g_signal_emit (chat, chat_signals[COMPOSING], 0, FALSE);
        }

        return TRUE;
}

static void
chat_error_dialog (GossipChat  *chat, 
                   const gchar *msg)
{
        GtkWidget *window;
        GtkWidget *dialog;

        window = gossip_chat_window_get_dialog (chat->priv->window);
        
        dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         msg);

        gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

        g_object_set (GTK_MESSAGE_DIALOG (dialog)->label,
                      "use-markup", TRUE,
                      NULL);

        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}

static void
chat_set_window (GossipChat       *chat,
                 GossipChatWindow *window)
{
        if (window == chat->priv->window) {
                return;
        }

	if (chat->priv->window != NULL) {
		g_signal_handlers_disconnect_by_func (chat->priv->window,
						      G_CALLBACK (chat_window_layout_changed_cb),
						      chat);
	}

	if (window != NULL) {
		g_signal_connect (window,
				  "layout-changed",
				  G_CALLBACK (chat_window_layout_changed_cb),
				  chat);

		chat_window_layout_changed_cb (window,
					       gossip_chat_window_get_layout (window),
					       chat);
	}

        chat->priv->window = window;
}

static void
chat_disclosure_toggled_cb (GtkToggleButton *disclosure,
                            GossipChat      *chat)
{
        GtkTextBuffer *buffer;
        GtkTextIter    start, end;
        const gchar   *const_str;
        gchar         *str;

        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->priv->input_text_view));

        if (gtk_toggle_button_get_active (disclosure)) {
                gtk_widget_show (chat->priv->multi_vbox);
                gtk_widget_hide (chat->priv->single_hbox);

                const_str = gtk_entry_get_text (GTK_ENTRY (chat->priv->input_entry));
                gtk_text_buffer_set_text (buffer, const_str, -1);
        } else {
                gtk_widget_show (chat->priv->single_hbox);
                gtk_widget_hide (chat->priv->multi_vbox);

                gtk_text_buffer_get_bounds (buffer, &start, &end);
                str = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
                gtk_entry_set_text (GTK_ENTRY (chat->priv->input_entry), str);
                g_free (str);
        }
}

static void
chat_send_multi_clicked_cb (GtkWidget  *button,
			    GossipChat *chat)
{
	GtkTextBuffer *buffer;
	GtkTextIter    start, end;
	gchar	      *msg;
	gboolean       send_normal_value;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->priv->input_text_view));

	gtk_text_buffer_get_bounds (buffer, &start, &end);
	msg = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	/* Clear the input field */
	send_normal_value = chat->priv->send_composing_events;
	chat->priv->send_composing_events = FALSE;
	gtk_text_buffer_set_text (buffer, "", -1);
	chat->priv->send_composing_events = send_normal_value;

	chat_send (chat, msg);

	g_free (msg);
}

static void
chat_input_activate_cb (GtkWidget  *entry,
                        GossipChat *chat)
{
        gchar *msg;

        msg = gtk_editable_get_chars (GTK_EDITABLE (chat->priv->input_entry), 0, -1);

        /* Clear the input field */
        gtk_entry_set_text (GTK_ENTRY (chat->priv->input_entry), "");

        chat_send (chat, msg);

        g_free (msg);
}

static gboolean
chat_input_key_press_event_cb (GtkWidget   *widget,
                               GdkEventKey *event,
                               GossipChat  *chat)
{
        /* Catch ctrl-enter */
        if ((event->state && GDK_CONTROL_MASK) && IS_ENTER (event->keyval)) {
                if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chat->priv->disclosure))) {
                        gtk_widget_activate (chat->priv->send_multi_button);
                } else {
                        gtk_widget_activate (chat->priv->input_entry);
                }

                return TRUE;
        }

        return FALSE;
}

static void
chat_input_entry_changed_cb (GtkWidget  *widget,
                             GossipChat *chat)
{
        const gchar *str;

        if (chat->priv->send_composing_events) {
                str = gtk_entry_get_text (GTK_ENTRY (widget));

                if (strlen (str) == 0) {
                        chat_composing_stop (chat);
                } else {
                        chat_composing_start (chat);
                }
        }
}

static void
chat_input_text_buffer_changed_cb (GtkTextBuffer *buffer,
                                   GossipChat    *chat)
{
        if (gtk_text_buffer_get_line_count (buffer) > 1) {
                gtk_widget_set_sensitive (chat->priv->disclosure, FALSE);
        } else {
                gtk_widget_set_sensitive (chat->priv->disclosure, TRUE);
        }

        if (chat->priv->send_composing_events) {
                if (gtk_text_buffer_get_char_count (buffer) == 0) {
                        chat_composing_stop (chat);
                } else {
                        chat_composing_start (chat);
                }
        }
}

static void
chat_presence_changed_cb (GossipChat  *chat)
{
	GossipRosterOld *roster;
	GdkPixbuf       *pixbuf;

	roster = gossip_app_get_roster ();
	pixbuf = gossip_roster_old_get_status_pixbuf_for_jid (roster, chat->priv->jid);
	
	gtk_image_set_from_pixbuf (GTK_IMAGE (chat->priv->status_image),
				   pixbuf);
}

static void
chat_window_layout_changed_cb (GossipChatWindow       *window,
			       GossipChatWindowLayout  layout,
			       GossipChat	      *chat)
{
	if (layout == GOSSIP_CHAT_WINDOW_LAYOUT_WINDOW) {
		gtk_widget_show (chat->priv->status_box);
	} else {
		gtk_widget_hide (chat->priv->status_box);
	}
}

static gboolean
chat_text_view_focus_in_event_cb (GtkWidget  *widget,
				  GdkEvent   *event,
				  GossipChat *chat)
{
	gint pos;

	pos = gtk_editable_get_position (GTK_EDITABLE (chat->priv->input_entry));

	gtk_widget_grab_focus (chat->priv->input_entry);
	gtk_editable_select_region (GTK_EDITABLE (chat->priv->input_entry), 0, 0);

	gtk_editable_set_position (GTK_EDITABLE (chat->priv->input_entry), pos);

	return TRUE;
}

static void
chat_composing_cb (GossipChat *chat,
		   gboolean    composing)
{
	GossipRosterOld *roster;
	GdkPixbuf       *pixbuf;

	roster = gossip_app_get_roster ();

	if (composing) {
		gtk_image_set_from_stock (GTK_IMAGE (chat->priv->status_image),
					  GOSSIP_STOCK_TYPING,
					  GTK_ICON_SIZE_MENU);
	} else {
		pixbuf = gossip_roster_old_get_status_pixbuf_for_jid (roster, 
								      chat->priv->jid);
		
		gtk_image_set_from_pixbuf (GTK_IMAGE (chat->priv->status_image),
					   pixbuf);
		g_object_unref (pixbuf);
	}
}

static gboolean
chat_delete_event_cb (GtkWidget  *widget,
		      GdkEvent   *event,
		      GossipChat *chat)
{
	return TRUE;
}

GtkWidget *
chat_create_disclosure (gpointer data)
{
	GtkWidget *widget;

	widget = cddb_disclosure_new (NULL, NULL);

	gtk_widget_show (widget);

	return widget;
}

GossipChat *
gossip_chat_get_for_jid (GossipJID *jid)
{
	GossipChat *chat;

	chat = chat_get_for_jid (jid);

	if (chat == NULL) {
		chat = g_object_new (GOSSIP_TYPE_CHAT,
				     "jid", jid,
				     "priv_group_chat", FALSE,
				     NULL);
	}

	return chat;
}

GossipChat *
gossip_chat_get_for_group_chat (GossipJID *jid)
{
	GossipChat *chat;

	chat = chat_get_for_jid (jid);

	if (chat == NULL) {
		chat = g_object_new (GOSSIP_TYPE_CHAT,
				     "jid", jid,
				     "priv_group_chat", TRUE,
				     NULL);
	}

	return chat;
}

void
gossip_chat_append_message (GossipChat *chat, LmMessage *m)
{
        LmConnection *connection;

        g_return_if_fail (chat != NULL);
        g_return_if_fail (m != NULL);

        connection = gossip_app_get_connection ();

        chat_message_handler (chat->priv->message_handler,
                              connection, m, chat);
}

LmHandlerResult
gossip_chat_handle_message (LmMessage *m)
{
        const gchar     *from;
        GossipJID       *jid;
        GossipChat      *chat;
        LmHandlerResult  result;

        from = lm_message_node_get_attribute (m->node, "from");
        jid = gossip_jid_new (from);

	chat = chat_get_for_jid (jid);

	if (chat) {
		/* The existing message handler will catch it. */
		result = LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	} else {
		chat = g_object_new (GOSSIP_TYPE_CHAT,
				     "jid", jid, NULL);
		gossip_chat_append_message (chat, m);
		result = LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

        gossip_jid_unref (jid);

        return result;
}

void
gossip_chat_present (GossipChat *chat)
{

        if (chat->priv->window == NULL) {
		gossip_chat_window_add_chat (gossip_chat_window_get_default (),
					     chat);
        }

        gossip_chat_window_switch_to_chat (chat->priv->window, chat);
        gtk_window_present (GTK_WINDOW (gossip_chat_window_get_dialog (chat->priv->window)));
}

GtkWidget *
gossip_chat_get_widget (GossipChat *chat)
{
        return chat->priv->widget;
}
