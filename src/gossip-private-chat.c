/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2004 Imendio HB
 * Copyright (C) 2003-2004 Geert-Jan Van den Bogaerde <geertjan@gnome.org>
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
#include "gossip-private-chat.h"

#define d(x)

#define IS_ENTER(v) (v == GDK_Return || v == GDK_ISO_Enter || v == GDK_KP_Enter)
#define COMPOSING_STOP_TIMEOUT 5

struct _GossipPrivateChatPriv {
        LmMessageHandler *message_handler;

        GtkWidget        *widget;
	GtkWidget	 *text_view_sw;
        GtkWidget        *single_hbox;
        GtkWidget        *subject_entry;

        GossipContact    *contact;
	gchar            *name;
	
	gchar            *locked_resource;
	gchar            *roster_resource;

        guint             composing_stop_timeout_id;
        gboolean          request_composing_events;
        gboolean          send_composing_events;
        gchar            *last_composing_id;
	gchar            *composing_resource; 
	
	gboolean          is_online;
	gboolean          groupchat_priv;
};

static GObjectClass *parent_class  = NULL;
static GHashTable   *private_chats = NULL;

static void            private_chat_class_init                   (GossipPrivateChatClass  *klass);
static void            private_chat_init                         (GossipPrivateChat       *chat);
static void            private_chat_finalize                     (GObject                 *object);
static void            private_chats_init                        (void);
static void            private_chat_create_gui                   (GossipPrivateChat       *chat);
static void            private_chat_send                         (GossipPrivateChat       *chat,
                                                                  const gchar      *msg);
static void            private_chat_request_composing            (LmMessage        *m);
static void            private_chat_composing_start              (GossipPrivateChat       *chat);
static void            private_chat_composing_stop               (GossipPrivateChat       *chat);
static void            private_chat_composing_send_start_event   (GossipPrivateChat       *chat);
static void            private_chat_composing_send_stop_event    (GossipPrivateChat       *chat);
static void            private_chat_composing_remove_timeout     (GossipPrivateChat       *chat);
static gboolean        private_chat_composing_stop_timeout_cb    (GossipPrivateChat       *chat);
static LmHandlerResult private_chat_message_handler              (LmMessageHandler        *handler,
                                                                  LmConnection            *connection,
                                                                  LmMessage               *m,
                                                                  gpointer                 user_data);
static void            private_chat_contact_presence_updated     (gpointer                 not_used,
			   				          GossipContact           *contact,
							          GossipPrivateChat       *chat);
static void            private_chat_contact_updated              (gpointer                 not_used,
							          GossipContact           *contact,
							          GossipPrivateChat       *chat);
static void            private_chat_contact_removed              (gpointer                 not_used,
							          GossipContact           *contact,
							          GossipPrivateChat       *chat);
static void            private_chat_contact_added		 (gpointer	           not_used,
							          GossipContact           *contact,
							          GossipPrivateChat       *chat);
static void            private_chat_connected_cb		 (GossipApp	          *app,
							          GossipPrivateChat	  *chat);
static void            private_chat_disconnected_cb		 (GossipApp	          *app,
							          GossipPrivateChat	  *chat);
static gboolean        private_chat_handle_composing_event       (GossipPrivateChat       *chat,
                                                                  LmMessage               *m);
static void            private_chat_error_dialog                 (GossipPrivateChat       *chat,
                                                                  const gchar             *msg);
static gboolean        private_chat_input_key_press_event_cb     (GtkWidget               *widget,
                                                                  GdkEventKey             *event,
                                                                  GossipPrivateChat       *chat);
static void            private_chat_input_text_buffer_changed_cb (GtkTextBuffer           *buffer,
                                                                  GossipPrivateChat       *chat);
static gboolean	       private_chat_text_view_focus_in_event_cb  (GtkWidget               *widget,
							          GdkEvent	          *event,
							          GossipPrivateChat       *chat);
static gboolean	       private_chat_delete_event_cb		 (GtkWidget	          *widget,
							          GdkEvent	          *event,
							          GossipPrivateChat       *chat);
static const gchar *   private_chat_get_name                     (GossipChat              *chat);
static gchar *         private_chat_get_tooltip                  (GossipChat              *chat);
static GdkPixbuf *     private_chat_get_status_pixbuf            (GossipChat              *chat);
static void            private_chat_get_geometry                 (GossipChat              *chat,
		                                                  int                     *width,
								  int                     *height); 
static GtkWidget *     private_chat_get_widget		         (GossipChat              *chat);
static GossipContact * private_chat_get_contact                  (GossipChat              *chat);
                      
GType
gossip_private_chat_get_type (void)
{
        static GType type_id = 0;

        if (type_id == 0) {
                const GTypeInfo type_info = {
                        sizeof (GossipPrivateChatClass),
                        NULL,
                        NULL,
                        (GClassInitFunc) private_chat_class_init,
                        NULL,
                        NULL,
                        sizeof (GossipPrivateChat),
                        0,
                        (GInstanceInitFunc) private_chat_init
                };

                type_id = g_type_register_static (GOSSIP_TYPE_CHAT,
                                                  "GossipPrivateChat",
                                                  &type_info,
                                                  0);
        }

        return type_id;
}

static void
private_chat_class_init (GossipPrivateChatClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GossipChatClass *chat_class = GOSSIP_CHAT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = private_chat_finalize;
	
	chat_class->get_name          = private_chat_get_name;
	chat_class->get_tooltip       = private_chat_get_tooltip;
	chat_class->get_status_pixbuf = private_chat_get_status_pixbuf;
	chat_class->get_contact       = private_chat_get_contact;
	chat_class->get_geometry      = private_chat_get_geometry;
	chat_class->get_widget        = private_chat_get_widget;
}

static void
private_chat_init (GossipPrivateChat *chat)
{
        GossipPrivateChatPriv *priv;
	LmConnection          *connection;
	LmMessageHandler      *handler;
	
	priv = g_new0 (GossipPrivateChatPriv, 1);
	priv->request_composing_events = TRUE;
	priv->is_online = FALSE;

	chat->priv = priv;
	
        private_chat_create_gui (chat);

	connection = gossip_app_get_connection ();

	handler = lm_message_handler_new (private_chat_message_handler, chat, NULL);
	chat->priv->message_handler = handler;
	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_MESSAGE,
						LM_HANDLER_PRIORITY_NORMAL);

	g_signal_connect_object (gossip_app_get (), 
				 "connected",
				 G_CALLBACK (private_chat_connected_cb),
				 chat, 0);
	
	g_signal_connect_object (gossip_app_get (), 
				 "disconnected",
				 G_CALLBACK (private_chat_disconnected_cb),
				 chat, 0);
}

static void
private_chat_finalize (GObject *object)
{
        GossipPrivateChat     *chat = GOSSIP_PRIVATE_CHAT (object);
	GossipPrivateChatPriv *priv;
	LmConnection          *connection;
        LmMessageHandler      *handler;

	priv = chat->priv;
        connection = gossip_app_get_connection ();

        handler = priv->message_handler;
        if (handler) {
                lm_connection_unregister_message_handler (
                                connection, handler, LM_MESSAGE_TYPE_MESSAGE);
                lm_message_handler_unref (handler);
        }

	gossip_contact_unref (priv->contact);

	private_chat_composing_remove_timeout (chat);
	
	g_free (priv->composing_resource);
	g_free (priv->last_composing_id);
	
        g_free (priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
private_chats_init (void)
{
        static gboolean inited = FALSE;

	if (inited) {
		return;
	}

        inited = TRUE;

        private_chats = g_hash_table_new_full (gossip_contact_hash,
				               gossip_contact_equal,
                                               (GDestroyNotify) gossip_contact_unref,
                                               (GDestroyNotify) g_object_unref);
}

static void
private_chat_create_gui (GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;
        GossipRoster          *roster;
        GtkTextBuffer         *buffer;
	GtkWidget             *input_text_view_sw;

	priv = chat->priv;

        gossip_glade_get_file_simple (GLADEDIR "/chat.glade",
                                      "chat_widget",
                                      NULL,
                                      "chat_widget", &priv->widget,
                                      "chat_view_sw", &priv->text_view_sw,
				      "input_text_view_sw", &input_text_view_sw,
                                      NULL);

	gtk_container_add (GTK_CONTAINER (priv->text_view_sw),
			   GTK_WIDGET (GOSSIP_CHAT (chat)->view));
	gtk_widget_show (GTK_WIDGET (GOSSIP_CHAT (chat)->view));

	gtk_container_add (GTK_CONTAINER (input_text_view_sw),
			   GOSSIP_CHAT (chat)->input_text_view);
	gtk_widget_show (GOSSIP_CHAT (chat)->input_text_view);

	g_object_ref (priv->widget);

	g_object_set_data (G_OBJECT (priv->widget), "chat", chat);

        roster = gossip_app_get_roster ();

	g_signal_connect (GOSSIP_CHAT (chat)->input_text_view,
			  "key_press_event",
			  G_CALLBACK (private_chat_input_key_press_event_cb),
			  chat);

        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->input_text_view));
        g_signal_connect (buffer,
                          "changed",
                          G_CALLBACK (private_chat_input_text_buffer_changed_cb),
                          chat);
	g_signal_connect (GOSSIP_CHAT (chat)->view,
			  "focus_in_event",
			  G_CALLBACK (private_chat_text_view_focus_in_event_cb),
			  chat);
	g_signal_connect (priv->widget,
			  "delete_event",
			  G_CALLBACK (private_chat_delete_event_cb),
			  NULL);

	gossip_chat_view_set_margin (GOSSIP_CHAT (chat)->view, 3);

	gtk_widget_grab_focus (GOSSIP_CHAT (chat)->input_text_view);
}

static void
private_chat_update_locked_resource (GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv = chat->priv;
	const gchar           *roster_resource;

	if (priv->groupchat_priv) {
		return;
	}
	
	if (!gossip_contact_is_online (priv->contact)) {
		g_free (priv->roster_resource);
		priv->roster_resource = NULL;

		g_free (priv->locked_resource);
		priv->locked_resource = NULL;

		return;
	}

	roster_resource = gossip_roster_get_active_resource (gossip_app_get_roster (),
							     priv->contact);

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
private_chat_get_jid_with_resource (GossipPrivateChat *chat, 
		                    const gchar       *resource)
{
	GossipPrivateChatPriv *priv = chat->priv;
	GossipJID             *jid;

	jid = gossip_contact_get_jid (priv->contact);

	d(g_print ("Getting jid with resource: %s\n", resource));
	
	if (resource) {
		return g_strconcat (gossip_jid_get_without_resource (jid),
				    "/", resource, NULL);
	}

	return g_strdup (gossip_jid_get_without_resource (jid));
}

static void
private_chat_send (GossipPrivateChat *chat,
                   const gchar       *msg)
{
	GossipPrivateChatPriv *priv;
        LmMessage             *m;
        gchar                 *nick;
	gchar                 *jid_string;
	gchar                 *username;

	priv = chat->priv;

	gossip_app_force_non_away ();

        if (msg == NULL || msg[0] == '\0') {
                return;
        }

        if (g_ascii_strcasecmp (msg, "/clear") == 0) {
		gossip_chat_view_clear (GOSSIP_CHAT (chat)->view);
                return;
        }

        private_chat_composing_remove_timeout (chat);

        nick = gossip_jid_get_part_name (gossip_app_get_jid ());

	username = gossip_jid_get_part_name (gossip_app_get_jid ());

        gossip_chat_view_append_chat_message (GOSSIP_CHAT (chat)->view,
                                              NULL,
					      username,
                                              nick,
                                              msg);

	g_free (username);
        g_free (nick);

	private_chat_update_locked_resource (chat);
	
	jid_string = private_chat_get_jid_with_resource (chat, priv->locked_resource);
        m = lm_message_new_with_sub_type (jid_string,
                                          LM_MESSAGE_TYPE_MESSAGE,
                                          LM_MESSAGE_SUB_TYPE_CHAT);

	d(g_print ("to: %s (roster: %s)\n", jid_string, priv->roster_resource));
	g_free (jid_string);
	
	lm_message_node_add_child (m->node, "body", msg);
        
        if (gossip_contact_is_online (priv->contact) &&
	    priv->request_composing_events) {
                private_chat_request_composing (m);
        }
	
	gossip_log_message (m, FALSE);

        lm_connection_send (gossip_app_get_connection (), m, NULL);
        lm_message_unref (m);
}

static void
private_chat_request_composing (LmMessage *m)
{
        LmMessageNode *x;

        x = lm_message_node_add_child (m->node, "x", NULL);

        lm_message_node_set_attribute (x,
                                       "xmlns",
                                       "jabber:x:event");
        lm_message_node_add_child (x, "composing", NULL);
}

static void
private_chat_composing_start (GossipPrivateChat *chat)
{
        if (!chat->priv->send_composing_events) {
                return;
        }

        if (chat->priv->composing_stop_timeout_id) {
                /* Just restart the timeout */
                private_chat_composing_remove_timeout (chat);
        } else {
                private_chat_composing_send_start_event (chat);
        }

        chat->priv->composing_stop_timeout_id = g_timeout_add (
                        1000 * COMPOSING_STOP_TIMEOUT,
                        (GSourceFunc) private_chat_composing_stop_timeout_cb,
                        chat);
}

static void
private_chat_composing_stop (GossipPrivateChat *chat)
{
	if (!chat->priv->send_composing_events) {
		return;
	}

	private_chat_composing_remove_timeout (chat);
	private_chat_composing_send_stop_event (chat);
}

static void
private_chat_composing_send_start_event (GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;
        LmConnection          *connection;
        LmMessage             *m;
        LmMessageNode         *x;
	gchar                 *jid_string;

	priv = chat->priv;

        connection = gossip_app_get_connection ();
        if (!lm_connection_is_open (connection)) {
                return;
        }

	if (!gossip_contact_is_online (priv->contact)) {
		return;
	}
	
	jid_string = private_chat_get_jid_with_resource (chat, priv->composing_resource);
		
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
private_chat_composing_send_stop_event (GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;
	LmMessage             *m;
        LmMessageNode         *x;
        LmConnection          *connection;
	gchar                 *jid_string;
	
	priv = chat->priv;
	
        connection = gossip_app_get_connection ();
        if (!lm_connection_is_open (connection)) {
                return;
        }

	jid_string = private_chat_get_jid_with_resource (chat, priv->composing_resource);

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
private_chat_composing_remove_timeout (GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = chat->priv;

        if (priv->composing_stop_timeout_id) {
                g_source_remove (priv->composing_stop_timeout_id);
                priv->composing_stop_timeout_id = 0;
        }
}

static gboolean
private_chat_composing_stop_timeout_cb (GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = chat->priv;
	
        priv->composing_stop_timeout_id = 0;
        private_chat_composing_send_stop_event (chat);

        return FALSE;
}

static gboolean
private_chat_should_play_sound (GossipPrivateChat *chat)
{
	GossipChatWindow      *window;
	GtkWidget             *toplevel;
	gboolean               play = TRUE;

	/* Play sounds if the window is not focused. */

	window = gossip_chat_get_window (GOSSIP_CHAT (chat));
	
	if (!window) {
		return TRUE;
	}

	toplevel = gossip_chat_window_get_dialog (window);

	/* The has-toplevel-focus property is new in GTK 2.2 so if we don't find it, we
	 * pretend that the window doesn't have focus => always play sound.
	 */
	if (g_object_class_find_property (G_OBJECT_GET_CLASS (toplevel),
					  "has-toplevel-focus")) {
		g_object_get (toplevel, "has-toplevel-focus", &play, NULL);
		play = !play;
	}

	return play;
}

static LmHandlerResult
private_chat_message_handler (LmMessageHandler *handler,
                              LmConnection     *connection,
                              LmMessage        *m,
                              gpointer          user_data)
{
        GossipPrivateChat     *chat = GOSSIP_PRIVATE_CHAT (user_data);
	GossipPrivateChatPriv *priv;
        const gchar           *from;
        LmMessageSubType       type;
        GossipJID             *from_jid;
        const gchar           *from_resource;
	const gchar           *timestamp = NULL;
        LmMessageNode         *node;
        const gchar           *body = "";
        const gchar           *thread = "";
	GossipJID             *jid;

	priv = chat->priv;

        from = lm_message_node_get_attribute (m->node, "from");

	if (!from) {
		g_print ("Received message without from attribute");
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

        from_jid = gossip_jid_new (from);
	jid = gossip_contact_get_jid (priv->contact);

	if (lm_message_get_sub_type (m) == LM_MESSAGE_SUB_TYPE_GROUPCHAT) {
		d(g_print ("GROUP CHAT!\n"));
		gossip_jid_unref (from_jid);
		
                return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}
	
        d(g_print ("Incoming message:: '%s' ?= '%s'\n",
                   gossip_jid_get_full (from_jid),
                   gossip_jid_get_full (jid)));

        if ((!priv->groupchat_priv && !gossip_jid_equals_without_resource (from_jid, jid)) ||

	    (priv->groupchat_priv && !gossip_jid_equals (from_jid, jid))) {
		d(g_print ("GROUP CHAT2!\n"));
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

                private_chat_error_dialog (chat, msg);

                g_free (msg);

                return LM_HANDLER_RESULT_REMOVE_MESSAGE;
        }

	from_resource = gossip_jid_get_resource (from_jid);
	
	if (!priv->groupchat_priv &&
	    from_resource &&
	    (!priv->locked_resource ||
	     g_ascii_strcasecmp (from_resource, priv->locked_resource) != 0)) {

		d(g_print ("New resource, relock\n"));
		g_free (priv->locked_resource);
		priv->locked_resource = g_strdup (from_resource);
	}
	
	gossip_jid_unref (from_jid);

        if (private_chat_handle_composing_event (chat, m)) {
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

        gossip_chat_view_append_chat_message (GOSSIP_CHAT (chat)->view,
                                              timestamp,
					      NULL,
                                              gossip_contact_get_name (priv->contact),
                                              body);

	g_signal_emit_by_name (chat, "new-message");
	
	if (private_chat_should_play_sound (chat)) {
		gossip_sound_play (GOSSIP_SOUND_CHAT);
	}

	if (gossip_chat_get_window (GOSSIP_CHAT (chat)) == NULL) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

        return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
private_chat_contact_presence_updated (gpointer           not_used,
			               GossipContact     *contact,
			               GossipPrivateChat *chat)

{
	GossipPrivateChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat));
	g_return_if_fail (contact != NULL);
	
	priv = chat->priv;

	if (!gossip_contact_equal (contact, priv->contact)) {
		return;
	}

	g_signal_emit_by_name (chat, "status-changed");

	if (!gossip_contact_is_online (contact)) {
		private_chat_composing_remove_timeout (chat);
		priv->send_composing_events = FALSE;

		g_free (priv->composing_resource);
		priv->composing_resource = NULL;
		
		if (priv->is_online) {
			gchar *msg;
			
			msg = g_strdup_printf (_("%s went offline"),
					       gossip_contact_get_name (priv->contact));
			gossip_chat_view_append_event_msg (GOSSIP_CHAT (chat)->view, 
							   msg, TRUE);
			g_free (msg);
		}
		priv->is_online = FALSE;

		g_signal_emit_by_name (chat, "composing", FALSE);
				
	} else {
		if (!priv->is_online) {
			gchar *msg;
		
			msg = g_strdup_printf (_("%s comes online"),
					       gossip_contact_get_name (priv->contact));
			gossip_chat_view_append_event_msg (GOSSIP_CHAT (chat)->view,
							   msg, TRUE);
			g_free (msg);
		}
		priv->is_online = TRUE;
	}
}

static void
private_chat_contact_updated (gpointer              not_used, 
		              GossipContact        *contact, 
		              GossipPrivateChat    *chat)
{
	GossipPrivateChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat));
	g_return_if_fail (contact != NULL);
	
	priv = chat->priv;

	if (!gossip_contact_equal (contact, priv->contact)) {
		return;
	}

	if (strcmp (priv->name, gossip_contact_get_name (contact)) != 0) {
		g_free (priv->name);
		priv->name = g_strdup (gossip_contact_get_name (contact));
		g_signal_emit_by_name (chat, "name-changed", priv->name);
	}
}

static void 
private_chat_contact_removed (gpointer           not_used,
		              GossipContact     *contact,
		              GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat));
	g_return_if_fail (contact != NULL);
	
	priv = chat->priv;

	if (!gossip_contact_equal (contact, priv->contact)) {
		return;
	}
}

static void
private_chat_contact_added (gpointer           not_user,
		            GossipContact     *contact,
		            GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat));
	g_return_if_fail (contact != NULL);

	priv = chat->priv;
	
	if (!gossip_contact_equal (contact, priv->contact)) {
		return;
	}
	
	gossip_contact_unref (priv->contact);
	priv->contact = gossip_contact_ref (contact);
}

static void
private_chat_connected_cb (GossipApp *app, GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat));

	priv = chat->priv;

	gtk_widget_set_sensitive (GOSSIP_CHAT (chat)->input_text_view, TRUE);

	gossip_chat_view_append_event_msg (GOSSIP_CHAT (chat)->view, _("Connected"), TRUE);
}

static void
private_chat_disconnected_cb (GossipApp *app, GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat));

	priv = chat->priv;

	gtk_widget_set_sensitive (GOSSIP_CHAT (chat)->input_text_view, FALSE);

	gossip_chat_view_append_event_msg (GOSSIP_CHAT (chat)->view, _("Disconnected"), TRUE);

	priv->send_composing_events = FALSE;
	private_chat_composing_remove_timeout (chat);

	g_free (priv->composing_resource);
	priv->composing_resource = NULL;
}

static gboolean
private_chat_handle_composing_event (GossipPrivateChat *chat, LmMessage *m)
{
	GossipPrivateChatPriv *priv = chat->priv;
        LmMessageNode         *x;
        const gchar           *xmlns;
        const gchar           *new_id;
        const gchar           *from;
	GossipJID             *jid;

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

                g_signal_emit_by_name (chat, "composing", FALSE);

                return FALSE;
        }

        if (lm_message_node_get_child (x, "composing")) {
                g_signal_emit_by_name (chat, "composing", TRUE);
        } else {
                g_signal_emit_by_name (chat, "composing", FALSE);
        }

        return TRUE;
}

static void
private_chat_error_dialog (GossipPrivateChat  *chat, 
                           const gchar        *msg)
{
	GossipChatWindow *chat_window;
        GtkWindow        *window = NULL;
        GtkWidget        *dialog;

        g_return_if_fail (chat != NULL);

	chat_window = gossip_chat_get_window (GOSSIP_CHAT (chat));

        if (chat_window) {
                window = GTK_WINDOW (gossip_chat_window_get_dialog (chat_window));
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
private_chat_input_text_view_send (GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;
	GtkTextBuffer         *buffer;
	GtkTextIter            start, end;
	gchar	              *msg;
	gboolean               send_normal_value;

	priv = chat->priv;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->input_text_view));

	gtk_text_buffer_get_bounds (buffer, &start, &end);
	msg = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	/* Clear the input field */
	send_normal_value = priv->send_composing_events;
	priv->send_composing_events = FALSE;
	gtk_text_buffer_set_text (buffer, "", -1);
	priv->send_composing_events = send_normal_value;

	private_chat_send (chat, msg);

	g_free (msg);

	GOSSIP_CHAT (chat)->is_first_char = TRUE;
}

static gboolean
private_chat_input_key_press_event_cb (GtkWidget         *widget,
                                       GdkEventKey       *event,
                                       GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;
	GtkAdjustment         *adj;
	gdouble                val;
	
	priv = chat->priv;

	/* Catch enter but not ctrl/shift-enter */
	if (IS_ENTER (event->keyval) && !(event->state & GDK_SHIFT_MASK)) {

		/* This is to make sure that kinput2 gets the enter. And if 
		 * it's handled there we shouldn't send on it. This is because
		 * kinput2 uses Enter to commit letters. See:
		 * http://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=104299
		 */
		if (gtk_im_context_filter_keypress (GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->input_text_view)->im_context, event)) {
			GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->input_text_view)->need_im_reset = TRUE;
			return TRUE;
		}
		
		private_chat_input_text_view_send (chat);
		return TRUE;
	}
	
	if (IS_ENTER (event->keyval) && (event->state & GDK_SHIFT_MASK)) {
		/* Newline for shift-enter. */
		return FALSE;
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
private_chat_input_text_buffer_changed_cb (GtkTextBuffer     *buffer, 
		                           GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = chat->priv;

        if (priv->send_composing_events) {
                if (gtk_text_buffer_get_char_count (buffer) == 0) {
			private_chat_composing_stop (chat);
                } else {
                        private_chat_composing_start (chat);
                }
        }
}

static gboolean
private_chat_text_view_focus_in_event_cb (GtkWidget         *widget,
				          GdkEvent          *event,
				          GossipPrivateChat *chat)
{
	gtk_widget_grab_focus (GOSSIP_CHAT (chat)->input_text_view);
	
	return TRUE;
}

static gboolean
private_chat_delete_event_cb (GtkWidget         *widget,
		              GdkEvent          *event,
		              GossipPrivateChat *chat)
{
	return TRUE;
}

static const gchar *
private_chat_get_name (GossipChat *chat)
{
	GossipPrivateChat     *p_chat;
	GossipPrivateChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	p_chat = GOSSIP_PRIVATE_CHAT (chat);
	priv   = p_chat->priv;

	return priv->name;
}
		
static gchar *
private_chat_get_tooltip (GossipChat *chat)
{
	GossipPrivateChat     *p_chat;
	GossipPrivateChatPriv *priv;
	GossipContact         *contact;
	GossipJID             *jid;
	gchar                 *str;
	const gchar           *status;
	GossipPresence        *presence;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	p_chat = GOSSIP_PRIVATE_CHAT (chat);
	priv   = p_chat->priv;

	contact = gossip_chat_get_contact (chat);
	jid = gossip_contact_get_jid (contact);

	presence = gossip_contact_get_presence (contact);
	status = gossip_presence_get_status (presence);

	if (!status || strcmp(status, "") == 0) {
		if (gossip_contact_is_online (contact)) {
			GossipPresenceType p_type;

			p_type = gossip_presence_get_type (presence);
			status = gossip_utils_get_default_status (p_type);
		} else {
			status = _("Offline");
		}
	}

	str = g_strdup_printf ("%s\n%s",
			       gossip_jid_get_without_resource (jid),
			       status);

	return str;
}

GdkPixbuf *
private_chat_get_status_pixbuf (GossipChat *chat)
{
	GossipPrivateChat     *p_chat;
	GossipPrivateChatPriv *priv;
	GossipContact         *contact;
	GossipPresence        *presence;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	p_chat = GOSSIP_PRIVATE_CHAT (chat);
	priv   = p_chat->priv;

	contact = gossip_chat_get_contact (chat);
	presence = gossip_contact_get_presence (contact);
	
	return gossip_presence_get_pixbuf (presence);
}

static GossipContact *
private_chat_get_contact (GossipChat *chat)
{
	GossipPrivateChat     *p_chat;
	GossipPrivateChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	p_chat = GOSSIP_PRIVATE_CHAT (chat);
	priv   = p_chat->priv;

	return priv->contact;
}

static void
private_chat_get_geometry (GossipChat *chat,
		           int        *width,
			   int        *height)
{
	*width  = 350;
	*height = 250;
}

static GtkWidget *
private_chat_get_widget (GossipChat *chat)
{
	GossipPrivateChat     *p_chat;
	GossipPrivateChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	p_chat = GOSSIP_PRIVATE_CHAT (chat);
	priv   = p_chat->priv;

	return priv->widget;
}

GossipPrivateChat *
gossip_private_chat_get_for_contact (GossipContact *contact, gboolean create)
{
	GossipPrivateChat     *chat;
	GossipPrivateChatPriv *priv;
	GossipRoster          *roster;

	private_chats_init ();

	chat = g_hash_table_lookup (private_chats, contact);

	if (chat) {
		return chat;
	}

	if (!create) {
		return NULL;
	}
	
	chat = g_object_new (GOSSIP_TYPE_PRIVATE_CHAT, NULL);
	g_hash_table_insert (private_chats, gossip_contact_ref (contact), chat);
	
	priv = chat->priv;
	priv->contact = gossip_contact_ref (contact);
	priv->name = g_strdup (gossip_contact_get_name (contact));

	priv->groupchat_priv = FALSE;

	roster = gossip_app_get_roster ();

	g_signal_connect_object (roster,
				 "contact_presence_updated",
				 G_CALLBACK (private_chat_contact_presence_updated),
				 chat, 0);

	g_signal_connect_object (roster,
				 "contact_updated",
				 G_CALLBACK (private_chat_contact_updated),
				 chat, 0);

	g_signal_connect_object (roster,
				 "contact_removed",
				 G_CALLBACK (private_chat_contact_removed),
				 chat, 0);
	
	g_signal_connect_object (roster,
				 "contact_added",
				 G_CALLBACK (private_chat_contact_added),
				 chat, 0);

	if (gossip_contact_is_online (priv->contact)) {
		priv->is_online = TRUE;
	} else {
		priv->is_online = FALSE;
	}

	return chat;
}

GossipPrivateChat *
gossip_private_chat_get_for_group_chat (GossipContact   *contact, 
				        GossipGroupChat *g_chat)
{
	GossipPrivateChat     *chat;
	GossipPrivateChatPriv *priv;
	GossipJID             *jid;

	private_chats_init ();

	chat = g_hash_table_lookup (private_chats, contact);
	if (chat) {
		return chat;
	}
	
	jid = gossip_contact_get_jid (contact);
	
	chat = g_object_new (GOSSIP_TYPE_PRIVATE_CHAT, NULL);
	g_hash_table_insert (private_chats, gossip_contact_ref (contact), chat);

	priv = chat->priv;
	priv->contact = gossip_contact_ref (contact);
	priv->name = g_strdup (gossip_contact_get_name (contact));
	priv->groupchat_priv = TRUE;

	priv->locked_resource = g_strdup (gossip_jid_get_resource (jid));
	
	if (gossip_contact_is_online (priv->contact)) {
		priv->is_online = TRUE;
	} else {
		priv->is_online = FALSE;
	}

	g_signal_connect_object (g_chat,
				 "contact_presence_updated",
				 G_CALLBACK (private_chat_contact_presence_updated),
				 chat, 0);

	g_signal_connect_object (g_chat,
				 "contact_updated",
				 G_CALLBACK (private_chat_contact_updated),
				 chat, 0);

	g_signal_connect_object (g_chat,
				 "contact_removed",
				 G_CALLBACK (private_chat_contact_removed),
				 chat, 0);
	
	g_signal_connect_object (g_chat,
				 "contact_added",
				 G_CALLBACK (private_chat_contact_added),
				 chat, 0);
	
	return chat;
}

gchar *
gossip_private_chat_get_history (GossipPrivateChat *chat, gint lines)
{
	GossipPrivateChatPriv *priv;
	GtkTextBuffer         *buffer;
	GtkTextIter            start, end;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	priv = chat->priv;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->view));

	gtk_text_buffer_get_bounds (buffer, &start, &end);

	if (lines != -1) {
		GtkTextIter tmp;

		tmp = end;
		if (gtk_text_iter_backward_lines (&tmp, lines)) {
			start = tmp;
		}
	}

	return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

void
gossip_private_chat_append_message (GossipPrivateChat *chat, LmMessage *m)
{
        LmConnection *connection;

        g_return_if_fail (chat != NULL);
        g_return_if_fail (m != NULL);

        connection = gossip_app_get_connection ();

        private_chat_message_handler (chat->priv->message_handler,
                                      connection, m, chat);
}

LmHandlerResult
gossip_private_chat_handle_message (LmMessage *m)
{
        const gchar       *from;
        GossipJID         *jid;
        GossipPrivateChat *chat;
        LmHandlerResult    result;
	GossipRosterItem  *item;
	GossipContact     *contact;

	private_chats_init ();

        from = lm_message_node_get_attribute (m->node, "from");
        jid = gossip_jid_new (from);

	item = gossip_roster_get_item (gossip_app_get_roster (), jid);
	if (item) {
		contact = gossip_roster_get_contact_from_item (gossip_app_get_roster (),
							       item);
		gossip_contact_ref (contact);
	} else {
		contact = gossip_contact_new (GOSSIP_CONTACT_TYPE_TEMPORARY);
		gossip_contact_set_jid (contact, jid);
	}

	gossip_jid_unref (jid);

	chat = (GossipPrivateChat *) g_hash_table_lookup (private_chats, contact);

	if (chat) {
		/* The existing message handler will catch it. */
		result = LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	} else {
		chat = gossip_private_chat_get_for_contact (contact, TRUE);
		
		gossip_private_chat_append_message (chat, m);
		result = LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

	gossip_contact_unref (contact);

        return result;
}

