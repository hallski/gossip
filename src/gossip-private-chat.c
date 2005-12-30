/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2004 Imendio AB
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
#include <gconf/gconf-client.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-href.h>

#include <libgossip/gossip-message.h>

#include "gossip-app.h"
#include "gossip-chat-window.h"
#include "gossip-chat-view.h"
#include "gossip-chat.h"
#include "gossip-log.h"
#include "gossip-private-chat.h"
#include "gossip-sound.h"
#include "gossip-stock.h"
#include "gossip-ui-utils.h"

#define d(x)

#define IS_ENTER(v) (v == GDK_Return || v == GDK_ISO_Enter || v == GDK_KP_Enter)

#define COMPOSING_STOP_TIMEOUT 5


struct _GossipPrivateChatPriv {
        GtkWidget        *widget;
	GtkWidget	 *text_view_sw;
        GtkWidget        *single_hbox;
        GtkWidget        *subject_entry;

        GossipContact    *contact;
        GossipContact    *own_contact;
	gchar            *name;
	
	gchar            *locked_resource;
	gchar            *roster_resource;

        guint             composing_stop_timeout_id;
	
	gboolean          is_online;
};


static GObjectClass *parent_class  = NULL;
 

static void            gossip_private_chat_class_init            (GossipPrivateChatClass  *klass);
static void            gossip_private_chat_init                  (GossipPrivateChat       *chat);
static void            private_chat_finalize                     (GObject                 *object);
static void            private_chat_create_gui                   (GossipPrivateChat       *chat);
static void            private_chat_send                         (GossipPrivateChat       *chat,
                                                                  const gchar      *msg);
static void            private_chat_composing_start              (GossipPrivateChat       *chat);
static void            private_chat_composing_stop               (GossipPrivateChat       *chat);
static void            private_chat_composing_remove_timeout     (GossipPrivateChat       *chat);
static gboolean        private_chat_composing_stop_timeout_cb    (GossipPrivateChat       *chat);

static void            private_chat_contact_presence_updated     (gpointer                 not_used,
			   				          GossipContact           *contact,
							          GossipPrivateChat       *chat);
static void            private_chat_contact_updated              (gpointer                 not_used,
							          GossipContact           *contact,
							          GossipPrivateChat       *chat);
static void            private_chat_contact_added		 (gpointer	           not_used,
							          GossipContact           *contact,
							          GossipPrivateChat       *chat);
static void            private_chat_connected_cb		 (GossipSession	          *session,
							          GossipPrivateChat	  *chat);
static void            private_chat_disconnected_cb		 (GossipSession	          *session,
							          GossipPrivateChat	  *chat);
static void            private_chat_composing_event_cb          (GossipSession *session,
								 GossipContact *contact,
								 gboolean       composing,
								 GossipChat    *chat);

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
static GossipContact * private_chat_get_own_contact              (GossipChat              *chat);
        

G_DEFINE_TYPE (GossipPrivateChat, gossip_private_chat, GOSSIP_TYPE_CHAT);


static void
gossip_private_chat_class_init (GossipPrivateChatClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GossipChatClass *chat_class = GOSSIP_CHAT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = private_chat_finalize;
	
	chat_class->get_name          = private_chat_get_name;
	chat_class->get_tooltip       = private_chat_get_tooltip;
	chat_class->get_status_pixbuf = private_chat_get_status_pixbuf;
	chat_class->get_contact       = private_chat_get_contact;
	chat_class->get_own_contact   = private_chat_get_own_contact;
	chat_class->get_geometry      = private_chat_get_geometry;
	chat_class->get_widget        = private_chat_get_widget;
}

static void
gossip_private_chat_init (GossipPrivateChat *chat)
{
        GossipPrivateChatPriv *priv;
	
	priv = g_new0 (GossipPrivateChatPriv, 1);
	priv->is_online = FALSE;

	chat->priv = priv;
	
        private_chat_create_gui (chat);

	d(g_print ("Private Chat: Connecting\n"));

	g_signal_connect_object (gossip_app_get_session (),
				 "connected",
				 G_CALLBACK (private_chat_connected_cb),
				 chat, 0);

	g_signal_connect_object (gossip_app_get_session (),
				 "disconnected",
				 G_CALLBACK (private_chat_disconnected_cb),
				 chat, 0);
	
	g_signal_connect_object (gossip_app_get_session (),
				 "composing-event",
				 G_CALLBACK (private_chat_composing_event_cb),
				 chat, 0);
}

static void
private_chat_finalize (GObject *object)
{
        GossipPrivateChat     *chat = GOSSIP_PRIVATE_CHAT (object);
	GossipPrivateChatPriv *priv;

	priv = chat->priv;

	g_object_unref (priv->contact);

	if (priv->own_contact) {
		g_object_unref (priv->own_contact);
	}

	private_chat_composing_remove_timeout (chat);
	
        g_free (priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
private_chat_create_gui (GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;
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

	gtk_widget_grab_focus (GOSSIP_CHAT (chat)->input_text_view);
}

static void
private_chat_update_locked_resource (GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv = chat->priv;
	const gchar           *roster_resource;

	if (!gossip_contact_is_online (priv->contact)) {
		g_free (priv->roster_resource);
		priv->roster_resource = NULL;

		g_free (priv->locked_resource);
		priv->locked_resource = NULL;

		return;
	}

	roster_resource = gossip_session_get_active_resource (gossip_app_get_session (), 
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
		return;
	}
	
	if (priv->roster_resource &&
	    g_ascii_strcasecmp (priv->roster_resource, roster_resource) == 0) {
		d(g_print ("Private Chat: Roster unchanged\n"));

		if (!priv->locked_resource) {
			priv->locked_resource = g_strdup (roster_resource);
		}
		
		return;
	}
	
	d(g_print ("Private Chat: New roster resource: %s\n", roster_resource));
	
	g_free (priv->roster_resource);
	priv->roster_resource = g_strdup (roster_resource);

	g_free (priv->locked_resource);
	priv->locked_resource = g_strdup (roster_resource);
}

static void
private_chat_send (GossipPrivateChat *chat,
                   const gchar       *msg)
{
	GossipPrivateChatPriv *priv;
	GossipMessage         *m;

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

	private_chat_update_locked_resource (chat);
	
	m = gossip_message_new (GOSSIP_MESSAGE_TYPE_NORMAL, priv->contact);

	if (priv->locked_resource) {
		gossip_message_set_explicit_resource (m, priv->locked_resource);
	}
	
	gossip_message_set_body (m, msg);
	gossip_message_request_composing (m);

	gossip_log_message (m, FALSE);

	gossip_chat_view_append_message_from_self (GOSSIP_CHAT (chat)->view, m, NULL);

	gossip_session_send_message (gossip_app_get_session (), m);
	
	g_object_unref (m);
}

static void
private_chat_composing_start (GossipPrivateChat *chat)
{
        GossipPrivateChatPriv *priv;

        priv = chat->priv;

        if (priv->composing_stop_timeout_id) {
                /* Just restart the timeout */
                private_chat_composing_remove_timeout (chat);
        } else {
                gossip_session_send_composing (gossip_app_get_session (),
                                               priv->contact, TRUE);
        }

        priv->composing_stop_timeout_id = g_timeout_add (
                        1000 * COMPOSING_STOP_TIMEOUT,
                        (GSourceFunc) private_chat_composing_stop_timeout_cb,
                        chat);
}

static void
private_chat_composing_stop (GossipPrivateChat *chat)
{
        GossipPrivateChatPriv *priv;

        priv = chat->priv;

	private_chat_composing_remove_timeout (chat);
        gossip_session_send_composing (gossip_app_get_session (),
                                       priv->contact, FALSE);
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
        gossip_session_send_composing (gossip_app_get_session (),
                                       priv->contact, FALSE);

        return FALSE;
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

		if (priv->is_online) {
			gchar *msg;
			
			msg = g_strdup_printf (_("%s went offline"),
					       gossip_contact_get_name (priv->contact));
			gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, 
						       msg);
			g_free (msg);
		}
		priv->is_online = FALSE;

		g_signal_emit_by_name (chat, "composing", FALSE);
				
	} else {
		if (!priv->is_online) {
			gchar *msg;
		
			msg = g_strdup_printf (_("%s comes online"),
					       gossip_contact_get_name (priv->contact));
			gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view,
						       msg);
			g_free (msg);
		}
		priv->is_online = TRUE;
	}
}

static void
private_chat_contact_updated (gpointer           not_used,
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

	if (strcmp (priv->name, gossip_contact_get_name (contact)) != 0) {
		g_free (priv->name);
		priv->name = g_strdup (gossip_contact_get_name (contact));
		g_signal_emit_by_name (chat, "name-changed", priv->name);
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
	
	g_object_unref (priv->contact);
	priv->contact = g_object_ref (contact);
}

static void
private_chat_connected_cb (GossipSession     *session, 
			   GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat));

	priv = chat->priv;

	gtk_widget_set_sensitive (GOSSIP_CHAT (chat)->input_text_view, TRUE);

	gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, _("Connected"));
}

static void
private_chat_disconnected_cb (GossipSession     *session, 
			      GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat));

	priv = chat->priv;

	gtk_widget_set_sensitive (GOSSIP_CHAT (chat)->input_text_view, FALSE);

	gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, _("Disconnected"));

	private_chat_composing_remove_timeout (chat);
}

static void
private_chat_composing_event_cb (GossipSession *session,
				 GossipContact *contact,
				 gboolean       composing,
				 GossipChat    *chat)
{
	GossipPrivateChat     *p_chat;
	GossipPrivateChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat));

	p_chat = GOSSIP_PRIVATE_CHAT (chat);
	priv   = p_chat->priv;

	if (gossip_contact_equal (contact, priv->contact)) {
		d(g_print ("Private Chat: Contact:'%s' %s typing\n",
			   gossip_contact_get_name (contact),
			   composing ? "is" : "is not"));

		g_signal_emit_by_name (chat, "composing", composing);
	}
}

static void
private_chat_input_text_view_send (GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;
	GtkTextBuffer         *buffer;
	GtkTextIter            start, end;
	gchar	              *msg;

	priv = chat->priv;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->input_text_view));

	gtk_text_buffer_get_bounds (buffer, &start, &end);
	msg = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	/* clear the input field */
	gtk_text_buffer_set_text (buffer, "", -1);

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
		GtkTextView *view;

		/* This is to make sure that kinput2 gets the enter. And if 
		 * it's handled there we shouldn't send on it. This is because
		 * kinput2 uses Enter to commit letters. See:
		 * http://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=104299
		 */

		view = GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->input_text_view);
		if (gtk_im_context_filter_keypress (view->im_context, event)) {
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

	if (gtk_text_buffer_get_char_count (buffer) == 0) {
		private_chat_composing_stop (chat);
	} else {
		private_chat_composing_start (chat);
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
	gchar                 *str;
	const gchar           *status;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	p_chat = GOSSIP_PRIVATE_CHAT (chat);
	priv   = p_chat->priv;

	contact = gossip_chat_get_contact (chat);
	status = gossip_contact_get_status (contact);

	str = g_strdup_printf ("%s\n%s",
			       gossip_contact_get_id (contact),
			       status);

	return str;
}

GdkPixbuf *
private_chat_get_status_pixbuf (GossipChat *chat)
{
	GossipPrivateChat     *p_chat;
	GossipPrivateChatPriv *priv;
	GossipContact         *contact;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	p_chat = GOSSIP_PRIVATE_CHAT (chat);
	priv   = p_chat->priv;

	contact = gossip_chat_get_contact (chat);

	return gossip_ui_utils_get_pixbuf_for_contact (contact);
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

static GossipContact *
private_chat_get_own_contact (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	return NULL;
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
gossip_private_chat_new (GossipContact *contact)
{
	GossipPrivateChat     *chat;
	GossipPrivateChatPriv *priv;

	chat = g_object_new (GOSSIP_TYPE_PRIVATE_CHAT, NULL);

	priv = chat->priv;
	priv->contact = g_object_ref (contact);
	priv->name = g_strdup (gossip_contact_get_name (contact));

	g_signal_connect_object (gossip_app_get_session (),
				 "contact_presence_updated",
				 G_CALLBACK (private_chat_contact_presence_updated),
				 chat, 0);

	g_signal_connect_object (gossip_app_get_session (),
				 "contact_updated",
				 G_CALLBACK (private_chat_contact_updated),
				 chat, 0);

	g_signal_connect_object (gossip_app_get_session (),
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
gossip_private_chat_append_message (GossipPrivateChat *chat, 
				    GossipMessage     *m)
{
	GossipPrivateChatPriv *priv;
	GossipContact         *sender;
	const gchar           *resource;
	const gchar           *invite;
	
        g_return_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat));
        g_return_if_fail (GOSSIP_IS_MESSAGE (m));

	priv = chat->priv;
	
	d(g_print ("Private Chat: Appending message ('%s')\n",
		   gossip_contact_get_name (gossip_message_get_sender (m))));

	sender = gossip_message_get_sender (m);
	if (!gossip_contact_equal (priv->contact, sender)) {
		return;
	}

	resource = gossip_message_get_explicit_resource (m);

	if (resource) {
		gboolean is_different;

		is_different = g_ascii_strcasecmp (resource, priv->locked_resource) != 0;

		if (!priv->locked_resource || is_different) {
			g_free (priv->locked_resource);
			priv->locked_resource = g_strdup (resource);
		}
	}

	gossip_log_message (m, TRUE);

	invite = gossip_message_get_invite (m);
	if (invite) {
		gossip_chat_view_append_invite (GOSSIP_CHAT (chat)->view,
					m);
	} else {
		gossip_chat_view_append_message_from_other (GOSSIP_CHAT (chat)->view,
							    m,
							    FALSE);
	}

	if (gossip_chat_should_play_sound (GOSSIP_CHAT (chat))) {
		gossip_sound_play (GOSSIP_SOUND_CHAT);
	}

	g_signal_emit_by_name (chat, "new-message");
}
