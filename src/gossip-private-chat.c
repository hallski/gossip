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
 *
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 *          Geert-Jan Van den Bogaerde <geertjan@gnome.org>
 */

#include "config.h"

#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-chatroom.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-protocol.h>
#include <libgossip/gossip-log.h>

#include "gossip-app.h"
#include "gossip-chat-window.h"
#include "gossip-chat-view.h"
#include "gossip-chat.h"
#include "gossip-preferences.h"
#include "gossip-private-chat.h"
#include "gossip-sound.h"
#include "gossip-stock.h"
#include "gossip-ui-utils.h"

#define DEBUG_DOMAIN "PrivateChat"

#define COMPOSING_STOP_TIMEOUT 5

#define IS_ENTER(v) (v == GDK_Return || v == GDK_ISO_Enter || v == GDK_KP_Enter)

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_PRIVATE_CHAT, GossipPrivateChatPriv))

struct _GossipPrivateChatPriv {
	GossipContact    *contact;
	GossipContact    *own_contact;
	gchar            *name;

	GdkPixbuf        *own_avatar;
	GdkPixbuf        *other_avatar;

	gchar            *locked_resource;
	gchar            *roster_resource;

	guint             composing_stop_timeout_id;
	guint             scroll_idle_id;

	gboolean          is_online;

	GtkWidget        *widget;
	GtkWidget	 *text_view_sw;
};

static void           gossip_private_chat_class_init            (GossipPrivateChatClass *klass);
static void           gossip_private_chat_init                  (GossipPrivateChat      *chat);
static void           private_chat_finalize                     (GObject                *object);
static void           private_chat_create_ui                    (GossipPrivateChat      *chat);
static void           private_chat_send                         (GossipPrivateChat      *chat,
								 const gchar            *msg);
static void           private_chat_composing_start              (GossipPrivateChat      *chat);
static void           private_chat_composing_stop               (GossipPrivateChat      *chat);
static void           private_chat_composing_remove_timeout     (GossipPrivateChat      *chat);
static gboolean       private_chat_composing_stop_timeout_cb    (GossipPrivateChat      *chat);
static void           private_chat_contact_presence_updated_cb  (GossipContact          *contact,
								 GParamSpec             *param,
								 GossipPrivateChat      *chat);
static void           private_chat_contact_updated_cb           (GossipContact          *contact,
								 GParamSpec             *param,
								 GossipPrivateChat      *chat);
static void           private_chat_contact_added_cb             (GossipSession          *session,
								 GossipContact          *contact,
								 GossipPrivateChat      *chat);
static void           private_chat_contact_removed_cb           (GossipSession          *session,
								 GossipContact          *contact,
								 GossipPrivateChat      *chat);
static void           private_chat_protocol_connected_cb        (GossipSession          *session,
								 GossipAccount          *account,
								 GossipProtocol         *protocol,
								 GossipPrivateChat      *chat);
static void           private_chat_protocol_disconnected_cb     (GossipSession          *session,
								 GossipAccount          *account,
								 GossipProtocol         *protocol,
								 gint                    reason,
								 GossipPrivateChat      *chat);
static void           private_chat_composing_cb                 (GossipSession          *session,
								 GossipContact          *contact,
								 gboolean                composing,
								 GossipChat             *chat);
static gboolean       private_chat_input_key_press_event_cb     (GtkWidget              *widget,
								 GdkEventKey            *event,
								 GossipPrivateChat      *chat);
static void           private_chat_input_text_buffer_changed_cb (GtkTextBuffer          *buffer,
								 GossipPrivateChat      *chat);
static gboolean       private_chat_text_view_focus_in_event_cb  (GtkWidget              *widget,
								 GdkEvent               *event,
								 GossipPrivateChat      *chat);
static void           private_chat_widget_destroy_cb            (GtkWidget              *widget,
								 GossipPrivateChat      *chat);
static void           private_chat_own_avatar_notify_cb         (GossipContact          *contact,
								 GParamSpec             *pspec,
								 GossipPrivateChat      *chat);
static void           private_chat_other_avatar_notify_cb       (GossipContact          *contact,
								 GParamSpec             *pspec,
								 GossipPrivateChat      *chat);
static const gchar *  private_chat_get_name                     (GossipChat             *chat);
static gchar *        private_chat_get_tooltip                  (GossipChat             *chat);
static GdkPixbuf *    private_chat_get_status_pixbuf            (GossipChat             *chat);
static GossipContact *private_chat_get_contact                  (GossipChat             *chat);
static GossipContact *private_chat_get_own_contact              (GossipChat             *chat);
static GtkWidget *    private_chat_get_widget                   (GossipChat             *chat);
static gboolean       private_chat_is_connected                 (GossipChat             *chat);
static GdkPixbuf *    private_chat_pad_to_size                  (GdkPixbuf              *pixbuf,
								 gint                    width,
								 gint                    height,
								 gint                    extra_padding_right);

G_DEFINE_TYPE (GossipPrivateChat, gossip_private_chat, GOSSIP_TYPE_CHAT);

static void
gossip_private_chat_class_init (GossipPrivateChatClass *klass)
{
	GObjectClass    *object_class = G_OBJECT_CLASS (klass);
	GossipChatClass *chat_class = GOSSIP_CHAT_CLASS (klass);

	object_class->finalize = private_chat_finalize;

	chat_class->get_name          = private_chat_get_name;
	chat_class->get_tooltip       = private_chat_get_tooltip;
	chat_class->get_status_pixbuf = private_chat_get_status_pixbuf;
	chat_class->get_contact       = private_chat_get_contact;
	chat_class->get_own_contact   = private_chat_get_own_contact;
	chat_class->get_widget        = private_chat_get_widget;
	chat_class->get_show_contacts = NULL;
	chat_class->set_show_contacts = NULL;
	chat_class->is_group_chat     = NULL;
	chat_class->is_connected      = private_chat_is_connected;

	g_type_class_add_private (object_class, sizeof (GossipPrivateChatPriv));
}

static void
gossip_private_chat_init (GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

	priv->is_online = FALSE;

	private_chat_create_ui (chat);

	gossip_debug (DEBUG_DOMAIN, "Connecting");

	g_signal_connect (gossip_app_get_session (),
			  "protocol-connected",
			  G_CALLBACK (private_chat_protocol_connected_cb),
			  chat);

	g_signal_connect (gossip_app_get_session (),
			  "protocol-disconnected",
			  G_CALLBACK (private_chat_protocol_disconnected_cb),
			  chat);

	g_signal_connect (gossip_app_get_session (),
			  "composing",
			  G_CALLBACK (private_chat_composing_cb),
			  chat);
}

static void
private_chat_finalize (GObject *object)
{
	GossipPrivateChat     *chat;
	GossipPrivateChatPriv *priv;

	gossip_debug (DEBUG_DOMAIN, "Finalized:%p", object);
	
	chat = GOSSIP_PRIVATE_CHAT (object);
	priv = GET_PRIV (chat);

	g_signal_handlers_disconnect_by_func (gossip_app_get_session (),
					      private_chat_protocol_connected_cb,
					      chat);
	g_signal_handlers_disconnect_by_func (gossip_app_get_session (),
					      private_chat_protocol_disconnected_cb,
					      chat);
	g_signal_handlers_disconnect_by_func (gossip_app_get_session (),
					      private_chat_composing_cb,
					      chat);

	g_signal_handlers_disconnect_by_func (gossip_app_get_session (), 
					      private_chat_contact_added_cb,
					      chat);
	g_signal_handlers_disconnect_by_func (gossip_app_get_session (), 
					      private_chat_contact_removed_cb,
					      chat);

	g_signal_handlers_disconnect_by_func (priv->own_contact,
					      private_chat_own_avatar_notify_cb,
					      chat);
	g_signal_handlers_disconnect_by_func (priv->contact,
					      private_chat_other_avatar_notify_cb,
					      chat);
	g_signal_handlers_disconnect_by_func (priv->contact,
					      private_chat_contact_updated_cb,
					      chat);
	g_signal_handlers_disconnect_by_func (priv->contact,
					      private_chat_contact_presence_updated_cb,
					      chat);

	if (priv->contact) {
		g_object_unref (priv->contact);
	}

	if (priv->own_contact) {
		g_object_unref (priv->own_contact);
	}

	if (priv->own_avatar) {
		g_object_unref (priv->own_avatar);
	}

	if (priv->other_avatar) {
		g_object_unref (priv->other_avatar);
	}

	if (priv->scroll_idle_id) {
		g_source_remove (priv->scroll_idle_id);
	}

	g_free (priv->name);
	g_free (priv->locked_resource);
	g_free (priv->roster_resource);

	private_chat_composing_remove_timeout (chat);

	G_OBJECT_CLASS (gossip_private_chat_parent_class)->finalize (object);
}

static void
private_chat_create_ui (GossipPrivateChat *chat)
{
	GladeXML              *glade;
	GossipPrivateChatPriv *priv;
	GtkTextBuffer         *buffer;
	GtkWidget             *input_text_view_sw;

	priv = GET_PRIV (chat);

	glade = gossip_glade_get_file ("chat.glade",
				       "chat_widget",
				       NULL,
				      "chat_widget", &priv->widget,
				      "chat_view_sw", &priv->text_view_sw,
				      "input_text_view_sw", &input_text_view_sw,
				       NULL);

	gossip_glade_connect (glade,
			      chat,
			      "chat_widget", "destroy", private_chat_widget_destroy_cb,
			      NULL);

	g_object_unref (glade);

	g_object_set_data (G_OBJECT (priv->widget), "chat", chat);

	gtk_container_add (GTK_CONTAINER (priv->text_view_sw),
			   GTK_WIDGET (GOSSIP_CHAT (chat)->view));
	gtk_widget_show (GTK_WIDGET (GOSSIP_CHAT (chat)->view));

	gtk_container_add (GTK_CONTAINER (input_text_view_sw),
			   GOSSIP_CHAT (chat)->input_text_view);
	gtk_widget_show (GOSSIP_CHAT (chat)->input_text_view);

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
}

static void
private_chat_update_locked_resource (GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;
	const gchar           *roster_resource;

	priv = GET_PRIV (chat);

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
		gossip_debug (DEBUG_DOMAIN, "Roster unchanged");

		if (!priv->locked_resource) {
			priv->locked_resource = g_strdup (roster_resource);
		}

		return;
	}

	gossip_debug (DEBUG_DOMAIN, "New roster resource: %s", roster_resource);

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
	GossipLogManager      *log_manager;
	GossipMessage         *message;

	priv = GET_PRIV (chat);

	if (msg == NULL || msg[0] == '\0') {
		return;
	}

	if (g_ascii_strcasecmp (msg, "/clear") == 0) {
		gossip_chat_view_clear (GOSSIP_CHAT (chat)->view);
		return;
	}

	gossip_app_set_not_away ();

	private_chat_composing_remove_timeout (chat);

	private_chat_update_locked_resource (chat);

	message = gossip_message_new (GOSSIP_MESSAGE_TYPE_NORMAL, priv->contact);

	if (priv->locked_resource) {
		gossip_message_set_explicit_resource (message, priv->locked_resource);
	}

	gossip_message_set_body (message, msg);
	gossip_message_request_composing (message);
	gossip_message_set_sender (message, priv->own_contact);

	log_manager = gossip_session_get_log_manager (gossip_app_get_session ());
	gossip_log_message_for_contact (log_manager, message, FALSE);

	gossip_chat_view_append_message_from_self (GOSSIP_CHAT (chat)->view,
						   message,
						   priv->own_contact,
						   priv->own_avatar);

	gossip_session_send_message (gossip_app_get_session (), message);

	g_object_unref (message);
}

static void
private_chat_composing_start (GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

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

	priv = GET_PRIV (chat);

	private_chat_composing_remove_timeout (chat);
	gossip_session_send_composing (gossip_app_get_session (),
				       priv->contact, FALSE);
}

static void
private_chat_composing_remove_timeout (GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

	if (priv->composing_stop_timeout_id) {
		g_source_remove (priv->composing_stop_timeout_id);
		priv->composing_stop_timeout_id = 0;
	}
}

static gboolean
private_chat_composing_stop_timeout_cb (GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

	priv->composing_stop_timeout_id = 0;
	gossip_session_send_composing (gossip_app_get_session (),
				       priv->contact, FALSE);

	return FALSE;
}

static void
private_chat_contact_presence_updated_cb (GossipContact     *contact,
					  GParamSpec        *param,
					  GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

	gossip_debug (DEBUG_DOMAIN, "Presence update for contact: %s",
		      gossip_contact_get_id (contact));

	if (!gossip_contact_is_online (contact)) {
		private_chat_composing_remove_timeout (chat);

		if (priv->is_online) {
			gchar *msg;

			msg = g_strdup_printf (_("%s went offline"),
					       gossip_contact_get_name (priv->contact));
			gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, msg);
			g_free (msg);
		}

		priv->is_online = FALSE;

		g_signal_emit_by_name (chat, "composing", FALSE);

	} else {
		if (!priv->is_online) {
			gchar *msg;

			msg = g_strdup_printf (_("%s has come online"),
					       gossip_contact_get_name (priv->contact));
			gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, msg);
			g_free (msg);
		}

		priv->is_online = TRUE;
	}

	g_signal_emit_by_name (chat, "status-changed");
}

static void
private_chat_contact_updated_cb (GossipContact     *contact,
				 GParamSpec        *param,
				 GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

	if (strcmp (priv->name, gossip_contact_get_name (contact)) != 0) {
		g_free (priv->name);
		priv->name = g_strdup (gossip_contact_get_name (contact));
		g_signal_emit_by_name (chat, "name-changed", priv->name);
	}
}

static void
private_chat_contact_added_cb (GossipSession     *session,
			       GossipContact     *contact,
			       GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

	if (!gossip_contact_equal (contact, priv->contact)) {
		return;
	}

	g_signal_connect (priv->contact, 
			  "notify::avatar",
			  G_CALLBACK (private_chat_other_avatar_notify_cb),
			  chat);
	g_signal_connect (priv->contact, 
			  "notify::name",
			  G_CALLBACK (private_chat_contact_updated_cb),
			  chat);
	g_signal_connect (priv->contact, 
			  "notify::presences",
			  G_CALLBACK (private_chat_contact_presence_updated_cb),
			  chat);
}

static void
private_chat_contact_removed_cb (GossipSession     *session,
				 GossipContact     *contact,
				 GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

	if (!gossip_contact_equal (contact, priv->contact)) {
		return;
	}

	g_signal_handlers_disconnect_by_func (priv->contact,
					      private_chat_other_avatar_notify_cb,
					      chat);
	g_signal_handlers_disconnect_by_func (priv->contact,
					      private_chat_contact_updated_cb,
					      chat);
	g_signal_handlers_disconnect_by_func (priv->contact,
					      private_chat_contact_presence_updated_cb,
					      chat);

	gtk_widget_set_sensitive (GOSSIP_CHAT (chat)->input_text_view, TRUE);

	/* i18n: An event, as in "has now been connected". */
	gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, _("Contact has been removed"));

}

static void
private_chat_protocol_connected_cb (GossipSession     *session,
				    GossipAccount     *account,
				    GossipProtocol    *protocol,
				    GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;
	GossipAccount         *this_account;

	priv = GET_PRIV (chat);

	this_account = gossip_contact_get_account (priv->own_contact);
	if (!gossip_account_equal (this_account, account)) {
		return;
	}

	gtk_widget_set_sensitive (GOSSIP_CHAT (chat)->input_text_view, TRUE);

	/* i18n: An event, as in "has now been connected". */
	gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, _("Connected"));

	g_signal_emit_by_name (chat, "status-changed");
}

static void
private_chat_protocol_disconnected_cb (GossipSession     *session,
				       GossipAccount     *account,
				       GossipProtocol    *protocol,
				       gint               reason,
				       GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;
	GossipAccount         *this_account;

	priv = GET_PRIV (chat);

	this_account = gossip_contact_get_account (priv->own_contact);
	if (!gossip_account_equal (this_account, account)) {
		return;
	}

	gtk_widget_set_sensitive (GOSSIP_CHAT (chat)->input_text_view, FALSE);

	/* i18n: An event, as in "has now been disconnected". */
	gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view,
				       _("Disconnected"));

	g_signal_emit_by_name (chat, "status-changed");

	private_chat_composing_remove_timeout (chat);
}

static void
private_chat_composing_cb (GossipSession *session,
			   GossipContact *contact,
			   gboolean       composing,
			   GossipChat    *chat)
{
	GossipPrivateChat     *p_chat;
	GossipPrivateChatPriv *priv;

	p_chat = GOSSIP_PRIVATE_CHAT (chat);
	priv = GET_PRIV (p_chat);

	if (gossip_contact_equal (contact, priv->contact)) {
		gossip_debug (DEBUG_DOMAIN, "Contact: '%s' %s typing",
			      gossip_contact_get_name (contact),
			      composing ? "is" : "is not");

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

	priv = GET_PRIV (chat);

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

	priv = GET_PRIV (chat);

	if (event->keyval == GDK_Tab && !(event->state & GDK_CONTROL_MASK)) {
		return TRUE;
	}

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
	else if ((event->state & GDK_CONTROL_MASK) != GDK_CONTROL_MASK &&
		 event->keyval == GDK_Page_Up) {
		adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->text_view_sw));
		gtk_adjustment_set_value (adj, adj->value - adj->page_size);

		return TRUE;
	}
	else if ((event->state & GDK_CONTROL_MASK) != GDK_CONTROL_MASK &&
		 event->keyval == GDK_Page_Down) {
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

	priv = GET_PRIV (chat);

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

static void
private_chat_widget_destroy_cb (GtkWidget         *widget,
				GossipPrivateChat *chat)
{
	gossip_debug (DEBUG_DOMAIN, "Destroyed");

	g_object_unref (chat);
}

static void
private_chat_own_avatar_notify_cb (GossipContact     *contact,
				   GParamSpec        *pspec,
				   GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;
	GdkPixbuf             *pixbuf;

	priv = GET_PRIV (chat);

	if (priv->own_avatar) {
		g_object_unref (priv->own_avatar);
	}

	pixbuf = gossip_pixbuf_avatar_from_contact_scaled (priv->own_contact, 32, 32);
	if (pixbuf) {
		priv->own_avatar = private_chat_pad_to_size (pixbuf, 32, 32, 6);
		g_object_unref (pixbuf);
	} else {
		priv->own_avatar = NULL;
	}
}

static void
private_chat_other_avatar_notify_cb (GossipContact     *contact,
				     GParamSpec        *pspec,
				     GossipPrivateChat *chat)
{
	GossipPrivateChatPriv *priv;
	GdkPixbuf             *pixbuf;

	priv = GET_PRIV (chat);

	if (priv->other_avatar) {
		g_object_unref (priv->other_avatar);
	}

	pixbuf = gossip_pixbuf_avatar_from_contact_scaled (priv->contact, 32, 32);
	if (pixbuf) {
		priv->other_avatar = private_chat_pad_to_size (pixbuf, 32, 32, 6);
		g_object_unref (pixbuf);
	} else {
		priv->other_avatar = NULL;
	}
}

static const gchar *
private_chat_get_name (GossipChat *chat)
{
	GossipPrivateChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	return priv->name;
}

static gchar *
private_chat_get_tooltip (GossipChat *chat)
{
	GossipPrivateChatPriv *priv;
	GossipContact         *contact;
	const gchar           *status;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	contact = gossip_chat_get_contact (chat);
	status = gossip_contact_get_status (contact);

	return g_strdup_printf ("%s\n%s",
				gossip_contact_get_id (contact),
				status);
}

static GdkPixbuf *
private_chat_get_status_pixbuf (GossipChat *chat)
{
	GossipPrivateChatPriv *priv;
	GossipContact         *contact;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	contact = gossip_chat_get_contact (chat);

	return gossip_pixbuf_for_contact (contact);
}

static GossipContact *
private_chat_get_contact (GossipChat *chat)
{
	GossipPrivateChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	return priv->contact;
}

static GossipContact *
private_chat_get_own_contact (GossipChat *chat)
{
	GossipPrivateChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	return priv->own_contact;
}

static GtkWidget *
private_chat_get_widget (GossipChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

	return priv->widget;
}

static gboolean
private_chat_is_connected (GossipChat *chat)
{
	GossipPrivateChatPriv *priv;
	GossipAccount         *account;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), FALSE);
	
	priv = GET_PRIV (chat);

	if (!priv->own_contact) {
		return FALSE;
	}
	
	account = gossip_contact_get_account (priv->own_contact);
	if (!account) {
		return FALSE;
	}

	return gossip_session_is_connected (gossip_app_get_session (), account);
}

/* Scroll down after the back-log has been received. */
static gboolean
private_chat_scroll_down_idle_func (GossipChat *chat)
{
	GossipPrivateChatPriv *priv;

	priv = GET_PRIV (chat);

	gossip_chat_scroll_down (chat);
	g_object_unref (chat);

	priv->scroll_idle_id = 0;

	return FALSE;
}

GossipPrivateChat *
gossip_private_chat_new (GossipContact *own_contact,
			 GossipContact *contact)
{
	GossipPrivateChatPriv *priv;
	GossipPrivateChat     *chat;
	GossipLogManager      *log_manager;
	GossipChatView        *view;
	GossipContact         *sender;
	GossipMessage         *message;
	GList                 *messages, *l;
	gint                   num_messages, i;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (own_contact), NULL);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	chat = g_object_new (GOSSIP_TYPE_PRIVATE_CHAT, NULL);

	priv = GET_PRIV (chat);

	priv->contact = g_object_ref (contact);
	priv->own_contact = g_object_ref (own_contact);

	priv->name = g_strdup (gossip_contact_get_name (contact));

	g_signal_connect (priv->own_contact, 
			  "notify::avatar",
			  G_CALLBACK (private_chat_own_avatar_notify_cb),
			  chat);
	g_signal_connect (priv->contact, 
			  "notify::avatar",
			  G_CALLBACK (private_chat_other_avatar_notify_cb),
			  chat);
	g_signal_connect (priv->contact, 
			  "notify::name",
			  G_CALLBACK (private_chat_contact_updated_cb),
			  chat);
	g_signal_connect (priv->contact, 
			  "notify::presences",
			  G_CALLBACK (private_chat_contact_presence_updated_cb),
			  chat);

	g_signal_connect (gossip_app_get_session (), 
			  "contact_added",
			  G_CALLBACK (private_chat_contact_added_cb),
			  chat);

	g_signal_connect (gossip_app_get_session (), 
			  "contact_removed",
			  G_CALLBACK (private_chat_contact_removed_cb),
			  chat);

	private_chat_own_avatar_notify_cb (priv->own_contact, NULL, chat);
	private_chat_other_avatar_notify_cb (priv->contact, NULL, chat);

	view = GOSSIP_CHAT (chat)->view;

	/* Turn off scrolling temporarily */
	gossip_chat_view_scroll (view, FALSE);

	/* Add messages from last conversation */
	log_manager = gossip_session_get_log_manager (gossip_app_get_session ());
	messages = gossip_log_get_last_for_contact (log_manager, priv->contact);
	num_messages  = g_list_length (messages);

	for (l = messages, i = 0; l; l = l->next, i++) {
		message = l->data;

		if (num_messages - i > 10) {
			continue;
		}

		sender = gossip_message_get_sender (message);
		if (gossip_contact_equal (priv->own_contact, sender)) {
			gossip_chat_view_append_message_from_self (view,
								   message,
								   priv->own_contact,
								   priv->own_avatar);
		} else {
			gossip_chat_view_append_message_from_other (view,
								    message,
								    sender,
								    priv->other_avatar);
		}
	}

	g_list_foreach (messages, (GFunc) g_object_unref, NULL);
	g_list_free (messages);

	/* Turn back on scrolling */
	gossip_chat_view_scroll (view, TRUE);

	/* Scroll to the most recent messages, we reference the chat
	 * for the duration of the scroll func.
	 */
	priv->scroll_idle_id = g_idle_add ((GSourceFunc) 
					   private_chat_scroll_down_idle_func, 
					   g_object_ref (chat));

	priv->is_online = gossip_contact_is_online (priv->contact);

	return chat;
}

gchar *
gossip_private_chat_get_history (GossipPrivateChat *chat, gint lines)
{
	GossipPrivateChatPriv *priv;
	GtkTextBuffer         *buffer;
	GtkTextIter            start, end;

	g_return_val_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

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
				    GossipMessage     *message)
{
	GossipPrivateChatPriv *priv;
	GossipLogManager      *log_manager;
	GossipContact         *sender;
	GossipChatroomInvite  *invite;
	const gchar           *resource;
	const gchar           *subject;

	g_return_if_fail (GOSSIP_IS_PRIVATE_CHAT (chat));
	g_return_if_fail (GOSSIP_IS_MESSAGE (message));

	priv = GET_PRIV (chat);

	gossip_debug (DEBUG_DOMAIN, "Appending message ('%s')",
		   gossip_contact_get_name (gossip_message_get_sender (message)));

	sender = gossip_message_get_sender (message);
	if (!gossip_contact_equal (priv->contact, sender)) {
		return;
	}

	resource = gossip_message_get_explicit_resource (message);
	if (resource) {
		gboolean is_different;

		if (priv->locked_resource) {
			is_different = g_ascii_strcasecmp (resource, priv->locked_resource) != 0;
		} else {
			is_different = TRUE;
		}

		if (!priv->locked_resource || is_different) {
			g_free (priv->locked_resource);
			priv->locked_resource = g_strdup (resource);
		}
	}

	log_manager = gossip_session_get_log_manager (gossip_app_get_session ());
	gossip_log_message_for_contact (log_manager, message, TRUE);

	subject = gossip_message_get_subject (message);
	if (subject) {
		gchar *str;

		str = g_strdup_printf (_("Subject: %s"), subject);
		gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, str);
		g_free (str);
	}

	invite = gossip_message_get_invite (message);
	if (invite) {
		gossip_chat_view_append_invite (GOSSIP_CHAT (chat)->view,
						message);
	} else {
		gossip_chat_view_append_message_from_other (GOSSIP_CHAT (chat)->view,
							    message,
							    priv->own_contact,
							    priv->other_avatar);
	}

	if (gossip_chat_should_play_sound (GOSSIP_CHAT (chat))) {
		gossip_sound_play (GOSSIP_SOUND_CHAT);
	}

	g_signal_emit_by_name (chat, "new-message", message);
}

/* Pads a pixbuf to the specified size, by centering it in a larger transparent
 * pixbuf. Returns a new ref.
 */
static GdkPixbuf *
private_chat_pad_to_size (GdkPixbuf *pixbuf,
			  gint       width,
			  gint       height,
			  gint       extra_padding_right)
{
	gint       src_width, src_height;
	GdkPixbuf *padded;
	gint       x_offset, y_offset;

	src_width = gdk_pixbuf_get_width (pixbuf);
	src_height = gdk_pixbuf_get_height (pixbuf);

	x_offset = (width - src_width) / 2;
	y_offset = (height - src_height) / 2;

	padded = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (pixbuf),
				 TRUE, /* alpha */
				 gdk_pixbuf_get_bits_per_sample (pixbuf),
				 width + extra_padding_right,
				 height);

	gdk_pixbuf_fill (padded, 0);

	gdk_pixbuf_copy_area (pixbuf,
			      0, /* source coords */
			      0,
			      src_width,
			      src_height,
			      padded,
			      x_offset, /* dest coords */
			      y_offset);

	return padded;
}

