/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2004 Imendio HB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
 * Copyright (C) 2002-2003 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2003-2004 Geert-Jan Van den Bogaerde <gvdbogaerde@pandora.be>
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
#include "gossip-log.h"
#include "gossip-roster.h"
#include "gossip-chat.h"

#define d(x)
#define IS_ENTER(v) (v == GDK_Return || v == GDK_ISO_Enter || v == GDK_KP_Enter)
#define COMPOSING_STOP_TIMEOUT 5

struct _GossipChatPriv {
        LmMessageHandler *message_handler;

        GossipChatWindow *window;

        GtkWidget        *widget;
	GtkWidget	 *text_view_sw;
        GtkWidget        *input_text_view;
        GtkWidget        *single_hbox;
        GtkWidget        *subject_entry;

	GossipChatView   *view;

        GtkTooltips      *tooltips;

        GossipRosterItem *item;
	gchar            *name;
	
	gchar            *locked_resource;
	gchar            *roster_resource;

        guint             composing_stop_timeout_id;
        gboolean          request_composing_events;
        gboolean          send_composing_events;
        gchar            *last_composing_id;
	gchar            *composing_resource; 
	
	gboolean          is_online;
};

static void            gossip_chat_class_init            (GossipChatClass  *klass);
static void            gossip_chat_init                  (GossipChat       *chat);
static void            gossip_chat_finalize              (GObject          *object);
static void            chats_init                        (void);
static void            chat_create_gui                   (GossipChat       *chat);
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
static void            chat_item_presence_updated        (gpointer          not_used,
							  GossipRosterItem *item,
							  GossipChat       *chat);
static void            chat_item_updated                 (gpointer          not_used,
							  GossipRosterItem *item,
							  GossipChat       *chat);
static void            chat_item_removed                 (gpointer          not_used,
							  GossipRosterItem *item,
							  GossipChat       *chat);
static void            chat_item_added			 (gpointer	    not_used,
							  GossipRosterItem *item,
							  GossipChat       *chat);
static void            chat_connected_cb		 (GossipApp	   *app,
							  GossipChat	   *chat);
static void            chat_disconnected_cb		 (GossipApp	   *app,
							  GossipChat	   *chat);
static gboolean        chat_handle_composing_event       (GossipChat       *chat,
                                                          LmMessage        *m);
static void            chat_error_dialog                 (GossipChat       *chat,
                                                          const gchar      *msg);
static gboolean        chat_input_key_press_event_cb     (GtkWidget        *widget,
                                                          GdkEventKey      *event,
                                                          GossipChat       *chat);
static void            chat_input_text_buffer_changed_cb (GtkTextBuffer    *buffer,
                                                          GossipChat       *chat);
static gboolean	       chat_text_view_focus_in_event_cb  (GtkWidget        *widget,
							  GdkEvent	   *event,
							  GossipChat       *chat);
static gboolean	       chat_delete_event_cb		 (GtkWidget	   *widget,
							  GdkEvent	   *event,
							  GossipChat       *chat);

enum {
        COMPOSING,
        NEW_MESSAGE,
	NAME_CHANGED,
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

        object_class->finalize = gossip_chat_finalize;

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

	chat_signals[NAME_CHANGED]=
		g_signal_new ("name-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GossipChatClass, name_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
}

static void
gossip_chat_init (GossipChat *chat)
{
        GossipChatPriv   *priv;
	LmConnection     *connection;
	GossipRoster     *roster;
	LmMessageHandler *handler;
	
	priv = g_new0 (GossipChatPriv, 1);
	priv->window = NULL;
	priv->request_composing_events = TRUE;
	priv->is_online = FALSE;
	
	chat->priv = priv;
	
        chat_create_gui (chat);

	connection = gossip_app_get_connection ();
	roster = gossip_app_get_roster ();

	handler = lm_message_handler_new (chat_message_handler, chat, NULL);
	chat->priv->message_handler = handler;
	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_MESSAGE,
						LM_HANDLER_PRIORITY_NORMAL);

	g_signal_connect (roster,
			  "item_presence_updated",
			  G_CALLBACK (chat_item_presence_updated),
			  chat);

	g_signal_connect (roster,
			  "item_updated",
			  G_CALLBACK (chat_item_updated),
			  chat);

	g_signal_connect (roster,
			  "item_removed",
			  G_CALLBACK (chat_item_removed),
			  chat);

	g_signal_connect (roster,
			  "item_added",
			  G_CALLBACK (chat_item_added),
			  chat);

	g_signal_connect (gossip_app_get (), 
			  "connected",
			  G_CALLBACK (chat_connected_cb),
			  chat);

	g_signal_connect (gossip_app_get (), 
			  "disconnected",
			  G_CALLBACK (chat_disconnected_cb),
			  chat);
}

static void
gossip_chat_finalize (GObject *object)
{
        GossipChat       *chat = GOSSIP_CHAT (object);
	GossipChatPriv   *priv;
	LmConnection     *connection;
        LmMessageHandler *handler;

	priv = chat->priv;
        connection = gossip_app_get_connection ();

        handler = priv->message_handler;
        if (handler) {
                lm_connection_unregister_message_handler (
                                connection, handler, LM_MESSAGE_TYPE_MESSAGE);
                lm_message_handler_unref (handler);
        }

	gossip_roster_item_unref (priv->item);

	chat_composing_remove_timeout (chat);
	
	g_free (priv->composing_resource);
	g_free (priv->last_composing_id);
	
        g_free (priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
chats_init (void)
{
        static gboolean inited = FALSE;

	if (inited) {
		return;
	}

        inited = TRUE;

        chats = g_hash_table_new_full (gossip_jid_hash,
				       gossip_jid_equal,
                                       (GDestroyNotify) gossip_jid_unref,
                                       (GDestroyNotify) g_object_unref);
}

static void
chat_create_gui (GossipChat *chat)
{
	GossipChatPriv *priv;
        GossipRoster   *roster;
        GtkTextBuffer  *buffer;

	priv = chat->priv;

        gossip_glade_get_file_simple (GLADEDIR "/chat.glade",
                                      "chat_widget",
                                      NULL,
                                      "chat_widget", &priv->widget,
                                      "chat_view_sw", &priv->text_view_sw,
                                      "input_textview", &priv->input_text_view,
                                      NULL);

	priv->view = gossip_chat_view_new ();
	gtk_container_add (GTK_CONTAINER (priv->text_view_sw),
			   GTK_WIDGET (priv->view));
	gtk_widget_show (GTK_WIDGET (priv->view));

	g_object_ref (priv->widget);

	g_object_set_data (G_OBJECT (priv->widget), "chat", chat);

        roster = gossip_app_get_roster ();

	priv->tooltips = gtk_tooltips_new ();

	g_signal_connect (priv->input_text_view,
			  "key_press_event",
			  G_CALLBACK (chat_input_key_press_event_cb),
			  chat);

        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->input_text_view));
        g_signal_connect (buffer,
                          "changed",
                          G_CALLBACK (chat_input_text_buffer_changed_cb),
                          chat);
	g_signal_connect (priv->view,
			  "focus_in_event",
			  G_CALLBACK (chat_text_view_focus_in_event_cb),
			  chat);
	g_signal_connect (priv->widget,
			  "delete_event",
			  G_CALLBACK (chat_delete_event_cb),
			  NULL);

	gossip_chat_view_set_margin (priv->view, 3);

	gtk_widget_grab_focus (priv->input_text_view);
}

static void
chat_update_locked_resource (GossipChat *chat)
{
	GossipChatPriv *priv = chat->priv;
	GossipJID      *jid;
	const gchar    *roster_resource;

	if (gossip_roster_item_is_offline (priv->item)) {
		g_free (priv->roster_resource);
		priv->roster_resource = NULL;

		g_free (priv->locked_resource);
		priv->locked_resource = NULL;

		return;
	}

	jid = gossip_roster_item_get_jid (priv->item);

	roster_resource = gossip_jid_get_resource (jid);

	/* It seems like some agents don't set a resource sometimes (ICQ for
	 * example). I don't know if it's a bug or not, but we need to handle
	 * those cases either way.
	 */
	if (!roster_resource) {
		g_free (priv->roster_resource);
		priv->roster_resource = NULL;

		g_free (priv->locked_resource);
		priv->locked_resource = NULL;

		/* Make sure we don't try to send composing events if the
		 * resource somehow got lost.
		 */
		priv->send_composing_events = FALSE;

		return;
	}
	
	if (priv->roster_resource &&
	    g_ascii_strcasecmp (priv->roster_resource, roster_resource) == 0) {
		d(g_print ("Roster unchanged\n"));

		if (!priv->locked_resource) {
			priv->locked_resource = g_strdup (roster_resource);
		}
		
		return;
	}
	
	d(g_print ("New roster resource: %s\n", roster_resource));
	
	g_free (priv->roster_resource);
	priv->roster_resource = g_strdup (roster_resource);

	g_free (priv->locked_resource);
	priv->locked_resource = g_strdup (roster_resource);

	/* Stop sending compose events since the resource changed. */
	priv->send_composing_events = FALSE;
}

static gchar *
chat_get_jid_with_resource (GossipChat *chat, const gchar *resource)
{
	GossipChatPriv *priv = chat->priv;
	GossipJID      *jid;

	jid = gossip_roster_item_get_jid (priv->item);

	if (resource) {
		return g_strconcat (gossip_jid_get_without_resource (jid),
				    "/", resource, NULL);
	}

	return g_strdup (gossip_jid_get_without_resource (jid));
}

static void
chat_send (GossipChat  *chat,
           const gchar *msg)
{
	GossipChatPriv *priv;
        LmMessage      *m;
        gchar          *nick;
	gchar          *jid_string;

	priv = chat->priv;

        if (msg == NULL || msg[0] == '\0') {
                return;
        }

        if (g_ascii_strcasecmp (msg, "/clear") == 0) {
		gossip_chat_view_clear (priv->view);
                return;
        }

        chat_composing_remove_timeout (chat);

        nick = gossip_jid_get_part_name (gossip_app_get_jid ());

        gossip_chat_view_append_chat_message (priv->view,
                                              NULL,
					      gossip_app_get_username (),
                                              nick,
                                              msg);

        g_free (nick);

	chat_update_locked_resource (chat);
	
	jid_string = chat_get_jid_with_resource (chat, priv->locked_resource);
        m = lm_message_new_with_sub_type (jid_string,
                                          LM_MESSAGE_TYPE_MESSAGE,
                                          LM_MESSAGE_SUB_TYPE_CHAT);

	d(g_print ("to: %s (roster: %s)\n", jid_string, priv->roster_resource));
	g_free (jid_string);
	
	lm_message_node_add_child (m->node, "body", msg);
        
        if (!gossip_roster_item_is_offline (priv->item) &&
	    priv->request_composing_events) {
                chat_request_composing (m);
        }
	
	gossip_log_message (m, FALSE);

        lm_connection_send (gossip_app_get_connection (), m, NULL);
        lm_message_unref (m);
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
	GossipChatPriv *priv;
        LmConnection   *connection;
        LmMessage      *m;
        LmMessageNode  *x;
	gchar          *jid_string;

	priv = chat->priv;

        connection = gossip_app_get_connection ();
        if (!lm_connection_is_open (connection)) {
                return;
        }

	if (gossip_roster_item_is_offline (priv->item)) {
		return;
	}
	
	jid_string = chat_get_jid_with_resource (chat, priv->composing_resource);
		
        m = lm_message_new_with_sub_type (jid_string,
                                          LM_MESSAGE_TYPE_MESSAGE,
                                          LM_MESSAGE_SUB_TYPE_CHAT);
        x = lm_message_node_add_child (m->node, "x", NULL);
        lm_message_node_set_attribute (x, "xmlns", "jabber:x:event");
        lm_message_node_add_child (x, "id", priv->last_composing_id);
        lm_message_node_add_child (x, "composing", NULL);

        lm_connection_send (connection, m, NULL);
        lm_message_unref (m);
	g_free (jid_string);
}

static void
chat_composing_send_stop_event (GossipChat *chat)
{
	GossipChatPriv *priv;
	LmMessage      *m;
        LmMessageNode  *x;
        LmConnection   *connection;
	gchar          *jid_string;
	
	priv = chat->priv;
	
        connection = gossip_app_get_connection ();
        if (!lm_connection_is_open (connection)) {
                return;
        }

	jid_string = chat_get_jid_with_resource (chat, priv->composing_resource);

        m = lm_message_new_with_sub_type (jid_string,
                                          LM_MESSAGE_TYPE_MESSAGE,
                                          LM_MESSAGE_SUB_TYPE_CHAT);
        x = lm_message_node_add_child (m->node, "x", NULL);
        lm_message_node_set_attribute (x, "xmlns", "jabber:x:event");
        lm_message_node_add_child (x, "id", priv->last_composing_id);

        lm_connection_send (connection, m, NULL);
        lm_message_unref (m);
	g_free (jid_string);
}

static void
chat_composing_remove_timeout (GossipChat *chat)
{
	GossipChatPriv *priv;

	priv = chat->priv;

        if (priv->composing_stop_timeout_id) {
                g_source_remove (chat->priv->composing_stop_timeout_id);
                chat->priv->composing_stop_timeout_id = 0;
        }
}

static gboolean
chat_composing_stop_timeout_cb (GossipChat *chat)
{
	GossipChatPriv *priv;

	priv = chat->priv;
	
        priv->composing_stop_timeout_id = 0;
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
	GossipChatPriv   *priv;
        const gchar      *from;
        LmMessageSubType  type;
        GossipJID        *from_jid;
        const gchar      *from_resource;
	const gchar      *timestamp = NULL;
        LmMessageNode    *node;
        const gchar      *body = "";
        const gchar      *thread = "";
	GossipJID        *jid;

	priv = chat->priv;

        from = lm_message_node_get_attribute (m->node, "from");

        from_jid = gossip_jid_new (from);
	jid = gossip_roster_item_get_jid (priv->item);

        d(g_print ("Incoming message:: '%s' ?= '%s'\n",
                   gossip_jid_get_without_resource (from_jid),
                   gossip_jid_get_without_resource (jid)));
	
        if (!gossip_jid_equals_without_resource (from_jid, jid)) {
                gossip_jid_unref (from_jid);
                return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
        }

        type = lm_message_get_sub_type (m);

        if (type == LM_MESSAGE_SUB_TYPE_ERROR) {
                gchar *tmp, *str, *msg;

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

                return LM_HANDLER_RESULT_REMOVE_MESSAGE;
        }

	from_resource = gossip_jid_get_resource (from_jid);
	
	if (from_resource &&
	    (!priv->locked_resource ||
	     g_ascii_strcasecmp (from_resource, priv->locked_resource) != 0)) {

		d(g_print ("New resource, relock\n"));
		g_free (priv->locked_resource);
		priv->locked_resource = g_strdup (from_resource);
	}
	
	gossip_jid_unref (from_jid);

        if (chat_handle_composing_event (chat, m)) {
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

	gossip_log_message (m, TRUE);

        gossip_chat_view_append_chat_message (chat->priv->view,
                                              timestamp,
					      NULL,
                                              gossip_roster_item_get_name (priv->item),
                                              body);

	g_signal_emit (chat, chat_signals[NEW_MESSAGE], 0);
	
	if (priv->window == NULL) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

        return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
chat_item_presence_updated (gpointer          not_used,
			    GossipRosterItem *item,
			    GossipChat       *chat)

{
	GossipChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT (chat));
	g_return_if_fail (item != NULL);
	
	priv = chat->priv;

	if (item != priv->item) {
		return;
	}

	if (gossip_roster_item_is_offline (item)) {
		chat_composing_remove_timeout (chat);
		priv->send_composing_events = FALSE;

		g_free (priv->composing_resource);
		priv->composing_resource = NULL;
		
		if (priv->is_online) {
			gchar *msg;
			
			msg = g_strdup_printf (_("%s went offline"),
					       gossip_roster_item_get_name (priv->item));
			gossip_chat_view_append_event_msg (priv->view,
							   msg, TRUE);
			g_free (msg);
		}
		priv->is_online = FALSE;

		g_signal_emit (chat, chat_signals[COMPOSING], 0, FALSE);
				
	} else {
		if (!priv->is_online) {
			gchar *msg;
		
			msg = g_strdup_printf (_("%s comes online"),
					       gossip_roster_item_get_name (priv->item));
			gossip_chat_view_append_event_msg (priv->view,
							   msg, TRUE);
			g_free (msg);
		}
		priv->is_online = TRUE;
	}
}

static void
chat_item_updated (gpointer          not_used,
		   GossipRosterItem *item,
		   GossipChat       *chat)
{
	GossipChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT (chat));
	g_return_if_fail (item != NULL);
	
	priv = chat->priv;

	if (item != priv->item) {
		return;
	}

	if (strcmp (priv->name, gossip_roster_item_get_name (item)) != 0) {
		g_free (priv->name);
		priv->name = g_strdup (gossip_roster_item_get_name (item));
		g_signal_emit (chat, chat_signals[NAME_CHANGED], 0, priv->name);
	}
}

static void 
chat_item_removed (gpointer          not_used,
		   GossipRosterItem *item,
		   GossipChat       *chat)
{
	GossipChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT (chat));
	g_return_if_fail (item != NULL);
	
	priv = chat->priv;

	if (item != priv->item) {
		return;
	}
}

static void
chat_item_added (gpointer          not_user,
		 GossipRosterItem *item,
		 GossipChat       *chat)
{
	GossipChatPriv *priv;
	GossipJID      *new_jid;
	GossipJID      *old_jid;

	g_return_if_fail (GOSSIP_IS_CHAT (chat));
	g_return_if_fail (item != NULL);

	priv = chat->priv;
	
	new_jid = gossip_roster_item_get_jid (item);
	old_jid = gossip_roster_item_get_jid (priv->item);

	if (!gossip_jid_equals_without_resource (old_jid, new_jid))
		return;

	gossip_roster_item_unref (priv->item);
	priv->item = item;
	gossip_roster_item_ref (item);
}

static void
chat_connected_cb (GossipApp  *app,
		   GossipChat *chat)
{
	GossipChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	priv = chat->priv;

	gtk_widget_set_sensitive (priv->input_text_view, TRUE);

	gossip_chat_view_append_event_msg (priv->view, _("Connected"), TRUE);
}

static void
chat_disconnected_cb (GossipApp  *app,
		      GossipChat *chat)
{
	GossipChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	priv = chat->priv;

	gtk_widget_set_sensitive (priv->input_text_view, FALSE);

	gossip_chat_view_append_event_msg (priv->view, _("Disconnected"), TRUE);

	priv->send_composing_events = FALSE;
	chat_composing_remove_timeout (chat);

	g_free (priv->composing_resource);
	priv->composing_resource = NULL;
}

static gboolean
chat_handle_composing_event (GossipChat *chat, LmMessage *m)
{
	GossipChatPriv *priv = chat->priv;
        LmMessageNode  *x;
        const gchar    *xmlns;
        const gchar    *new_id;
        const gchar    *from;
	GossipJID      *jid;

        x = lm_message_node_get_child (m->node, "x");
        if (!x) {
                return FALSE;
        }

        xmlns = lm_message_node_get_attribute (x, "xmlns");
        if (strcmp (xmlns, "jabber:x:event") != 0) {
                return FALSE;
        }

        if (lm_message_node_get_child (m->node, "body")) {
                if (priv->is_online && lm_message_node_get_child (x, "composing")) {
                        /* Handle request for composing events. */
			priv->send_composing_events = TRUE;

			from = lm_message_node_get_attribute (m->node, "from");
			jid = gossip_jid_new (from);

			g_free (priv->composing_resource);
			priv->composing_resource =
				g_strdup (gossip_jid_get_resource (jid));
			
			gossip_jid_unref (jid);
			
			g_free (priv->last_composing_id);
			new_id = lm_message_node_get_attribute (m->node, "id");
			if (new_id) {
				priv->last_composing_id = g_strdup (new_id);
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
        GtkWindow *window = NULL;
        GtkWidget *dialog;

        g_return_if_fail (chat != NULL);

        if (chat->priv->window) {
                window = GTK_WINDOW (gossip_chat_window_get_dialog (chat->priv->window));
        }

        dialog = gtk_message_dialog_new (window,
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
chat_input_text_view_send (GossipChat *chat)
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

static gboolean
chat_input_key_press_event_cb (GtkWidget   *widget,
                               GdkEventKey *event,
                               GossipChat  *chat)
{
	GossipChatPriv *priv = chat->priv;
	GtkAdjustment  *adj;
	gdouble         val;
		
	/* Catch enter but not ctrl-enter */
	if (IS_ENTER (event->keyval) && !(event->state & GDK_CONTROL_MASK)) {
		chat_input_text_view_send (chat);

		return TRUE;
	}
	else if (event->keyval == GDK_Page_Up) {
		adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->text_view_sw));
		gtk_adjustment_set_value (adj, adj->value - adj->page_size);
		
		return TRUE;
	}
	else if (event->keyval == GDK_Page_Down) {
		adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->text_view_sw));
		val = MIN (adj->value + adj->page_size, adj->upper - adj->page_size);  
		gtk_adjustment_set_value (adj, val);

		return TRUE;
	}
		
	return FALSE;
}

static void
chat_input_text_buffer_changed_cb (GtkTextBuffer *buffer,
                                   GossipChat    *chat)
{
        if (chat->priv->send_composing_events) {
                if (gtk_text_buffer_get_char_count (buffer) == 0) {
                        chat_composing_stop (chat);
                } else {
                        chat_composing_start (chat);
                }
        }
}

static gboolean
chat_text_view_focus_in_event_cb (GtkWidget  *widget,
				  GdkEvent   *event,
				  GossipChat *chat)
{
	GossipChatPriv *priv = chat->priv;
/*
	gint pos;

	pos = gtk_editable_get_position (GTK_EDITABLE (chat->priv->input_entry));

	gtk_widget_grab_focus (chat->priv->input_entry);
	gtk_editable_select_region (GTK_EDITABLE (chat->priv->input_entry), 0, 0);

	gtk_editable_set_position (GTK_EDITABLE (chat->priv->input_entry), pos);
*/
	gtk_widget_grab_focus (priv->input_text_view);
	
	return TRUE;
}

static gboolean
chat_delete_event_cb (GtkWidget  *widget,
		      GdkEvent   *event,
		      GossipChat *chat)
{
	return TRUE;
}

GossipChat *
gossip_chat_get_for_item (GossipRosterItem *item)
{
	GossipChat     *chat;
	GossipChatPriv *priv;
	GossipJID      *jid;

	chats_init ();

	jid = gossip_roster_item_get_jid (item);
	chat = g_hash_table_lookup (chats, jid);

	if (chat) {
		return chat;
	}
	
	chat = g_object_new (GOSSIP_TYPE_CHAT, NULL);
	g_hash_table_insert (chats, gossip_jid_ref (jid), chat);
	
	priv = chat->priv;
	priv->item = gossip_roster_item_ref (item);
	priv->name = g_strdup (gossip_roster_item_get_name (item));

	if (gossip_roster_item_is_offline (priv->item)) {
		priv->is_online = FALSE;
	} else {
		priv->is_online = TRUE;
	}

	return chat;
}

GossipChat *
gossip_chat_get_for_group_chat (GossipRosterItem *item)
{
	GossipChat     *chat;
	GossipChatPriv *priv;
	GossipJID      *jid;

	chats_init ();

	jid = gossip_roster_item_get_jid (item);
	chat = g_hash_table_lookup (chats, jid);
	if (chat) {
		return chat;
	}

	chat = g_object_new (GOSSIP_TYPE_CHAT, NULL);
	priv = chat->priv;
	priv->item = gossip_roster_item_ref (item);
	
	if (gossip_roster_item_is_offline (priv->item)) {
		priv->is_online = FALSE;
	} else {
		priv->is_online = TRUE;
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
        const gchar      *from;
        GossipJID        *jid;
        GossipChat       *chat;
        LmHandlerResult   result;
	GossipRosterItem *item;

	chats_init ();

        from = lm_message_node_get_attribute (m->node, "from");
        jid = gossip_jid_new (from);

	item = gossip_roster_get_item (gossip_app_get_roster (), jid);
	if (!item) {
		item = gossip_roster_item_new (jid);
	}
	chat = (GossipChat *) g_hash_table_lookup (chats, jid);

	if (chat) {
		/* The existing message handler will catch it. */
		result = LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	} else {
		chat = gossip_chat_get_for_item (item);
		/* chat = g_object_new (GOSSIP_TYPE_CHAT,
				     "jid", jid, NULL); */
		gossip_chat_append_message (chat, m);
		result = LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

        gossip_jid_unref (jid);

        return result;
}

void
gossip_chat_present (GossipChat *chat)
{
	GossipChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	priv = chat->priv;
	
	if (priv->window == NULL) {
		gossip_chat_window_add_chat (
			gossip_chat_window_new (), chat);
        }

        gossip_chat_window_switch_to_chat (priv->window, chat);
        gtk_window_present (GTK_WINDOW (gossip_chat_window_get_dialog (priv->window)));

	gtk_widget_grab_focus (priv->input_text_view);
}

GtkWidget *
gossip_chat_get_widget (GossipChat *chat)
{
        return chat->priv->widget;
}

GossipRosterItem *
gossip_chat_get_item (GossipChat *chat)
{
	GossipChatPriv *priv;
	
	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), NULL);
	
	priv = chat->priv;

	return priv->item;
}

void
gossip_chat_set_window (GossipChat *chat, GossipChatWindow *window)
{
	chat->priv->window = window;
}

GossipChatWindow *
gossip_chat_get_window (GossipChat *chat)
{
	return chat->priv->window;
}

void
gossip_chat_clear (GossipChat *chat)
{
	GossipChatPriv *priv;
	
	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	priv = chat->priv;
	
	gossip_chat_view_clear (priv->view);
}

