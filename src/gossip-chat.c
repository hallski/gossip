/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
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
#include <gconf/gconf-client.h>
#include <loudmouth/loudmouth.h>
#include "gossip-utils.h"
#include "gossip-sound.h"
#include "gossip-chat.h"
#include "gossip-app.h"
#include "disclosure-widget.h"

#define d(x) 

struct _GossipChat {
	GossipApp        *app;

	LmConnection     *connection;
	LmMessageHandler *presence_handler;
	LmMessageHandler *message_handler;

	GtkWidget        *dialog;
	GtkWidget        *text_view;
	GtkWidget        *input_entry;
	GtkWidget        *input_text_view;
	GtkWidget        *single_hbox;
	GtkWidget        *multi_vbox;
	GtkWidget        *send_multi_button;
	GtkWidget        *subject_entry;
	GtkWidget        *status_image;
	GtkWidget        *disclosure;

	GossipJID        *jid;
	gchar            *nick;
};

static GossipChat *
chat_create                                       (GossipApp        *app,
						   GossipJID        *jid);
static void     chat_input_activate_cb            (GtkWidget        *entry,
						   GossipChat       *chat);
static gboolean chat_input_key_press_event_cb     (GtkWidget        *widget,
						   GdkEventKey      *event,
						   GossipChat       *chat);
static void     chat_input_text_buffer_changed_cb (GtkTextBuffer    *buffer,
						   GossipChat       *chat);
static void     chat_dialog_destroy_cb            (GtkWidget        *widget,
						   GossipChat       *chat);
static void     chat_dialog_send                  (GossipChat       *chat,
						   const gchar      *msg);
static void     chat_dialog_send_multi_clicked_cb (GtkWidget        *unused,
						   GossipChat       *chat);
static gboolean chat_focus_in_event_cb            (GtkWidget        *widget,
						   GdkEvent         *event,
						   GossipChat       *chat);
static LmHandlerResult
chat_message_handler                              (LmMessageHandler *handler,
						   LmConnection     *connection,
						   LmMessage        *m,
						   GossipChat       *chat);
static LmHandlerResult
chat_presence_handler                             (LmMessageHandler *handler,
						   LmConnection     *connection,
						   LmMessage        *m,
						   GossipChat       *chat);
GtkWidget *     chat_dialog_create_disclosure     (gpointer          data);


GtkWidget *
chat_dialog_create_disclosure (gpointer data)
{
	GtkWidget *widget;
	
	widget = cddb_disclosure_new (NULL, NULL);

	gtk_widget_show (widget);

	return widget;
}

static void
chat_disclosure_toggled_cb (GtkToggleButton *disclosure,
			    GossipChat      *chat)
{
	GtkTextBuffer *buffer;
	GtkTextIter    start, end;
	const gchar   *const_str; 
	gchar         *str;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	
	if (gtk_toggle_button_get_active (disclosure)) {
		gtk_widget_show (chat->multi_vbox);
		gtk_widget_hide (chat->single_hbox);

		const_str = gtk_entry_get_text (GTK_ENTRY (chat->input_entry));
		gtk_text_buffer_set_text (buffer, const_str, -1);
	} else {
		gtk_widget_show (chat->single_hbox);
		gtk_widget_hide (chat->multi_vbox);

		gtk_text_buffer_get_bounds (buffer, &start, &end);
		str = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
		gtk_entry_set_text (GTK_ENTRY (chat->input_entry), str);
		g_free (str);
	}
}

static GossipChat *
chat_create (GossipApp *app, GossipJID *jid) 
{
	GossipChat    *chat;
	GtkWidget     *from_label;
	GossipRoster  *roster;
	const gchar   *name;
	GdkPixbuf     *pixbuf;
	gchar         *from;
	GtkTextBuffer *buffer;
	
	g_return_val_if_fail (GOSSIP_IS_APP (app), NULL);

	chat = g_new0 (GossipChat, 1);

	chat->app = app;
	chat->connection = lm_connection_ref (gossip_app_get_connection (app));
	chat->jid = gossip_jid_ref (jid);
	chat->nick = NULL;
	
	gossip_glade_get_file_simple (GLADEDIR "/chat.glade",
				      "chat_window",
				      NULL,
				      "chat_window", &chat->dialog,
				      "chat_textview", &chat->text_view,
				      "input_entry", &chat->input_entry,
				      "input_textview", &chat->input_text_view,
				      "single_hbox", &chat->single_hbox,
				      "multi_vbox", &chat->multi_vbox,
				      "from_label", &from_label,
				      "status_image", &chat->status_image,
				      "disclosure", &chat->disclosure,
				      "send_multi_button", &chat->send_multi_button,
				      NULL);

	roster = gossip_app_get_roster ();

	pixbuf = gossip_roster_get_status_pixbuf_for_jid (roster, jid);
	if (pixbuf) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (chat->status_image),
					   pixbuf);
	}

	name = gossip_roster_get_nick_from_jid (roster, jid);
	if (name) {
		from = g_strdup_printf ("%s <%s>", name, gossip_jid_get_full (jid));
	} else {
		from = (gchar *) gossip_jid_get_full (jid);
	}

	gtk_label_set_text (GTK_LABEL (from_label), from);
	if (from != gossip_jid_get_full (jid)) {
		g_free (from);
	}
		
	g_signal_connect (chat->disclosure,
			  "toggled",
			  G_CALLBACK (chat_disclosure_toggled_cb),
			  chat);
	
	g_signal_connect (chat->send_multi_button,
			  "clicked",
			  G_CALLBACK (chat_dialog_send_multi_clicked_cb),
			  chat);

	g_signal_connect (chat->dialog,
			  "destroy",
			  G_CALLBACK (chat_dialog_destroy_cb),
			  chat);
		
	g_signal_connect (chat->input_entry,
			  "activate",
			  G_CALLBACK (chat_input_activate_cb),
			  chat);

	g_signal_connect (chat->input_text_view,
			  "key_press_event",
			  G_CALLBACK (chat_input_key_press_event_cb),
			  chat);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	g_signal_connect (buffer,
			  "changed",
			  G_CALLBACK (chat_input_text_buffer_changed_cb),
			  chat);

	g_signal_connect (chat->text_view,
			  "focus_in_event",
			  G_CALLBACK (chat_focus_in_event_cb),
			  chat);

	gtk_widget_show (chat->dialog);

	gossip_text_view_set_margin (GTK_TEXT_VIEW (chat->text_view), 3);
	gossip_text_view_setup_tags (GTK_TEXT_VIEW (chat->text_view));
	
	gtk_widget_grab_focus (chat->input_entry);

	chat->presence_handler = lm_message_handler_new (
		(LmHandleMessageFunction) chat_presence_handler, chat,NULL);

	lm_connection_register_message_handler (chat->connection,
						chat->presence_handler,
						LM_MESSAGE_TYPE_PRESENCE,
						LM_HANDLER_PRIORITY_NORMAL);

	return chat;
}

static void
chat_input_activate_cb (GtkWidget *entry, GossipChat *chat)
{
	gchar *msg;

	msg = gtk_editable_get_chars (GTK_EDITABLE (chat->input_entry), 0, -1);

	/* Clear the input field. */
	gtk_entry_set_text (GTK_ENTRY (chat->input_entry), "");

	chat_dialog_send (chat, msg);

	g_free (msg);
}

static gboolean
chat_input_key_press_event_cb (GtkWidget   *widget,
			       GdkEventKey *event,
			       GossipChat  *chat)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chat->disclosure))) {
		/* Multi line entry. */
		if ((event->state & GDK_CONTROL_MASK) &&
		    (event->keyval == GDK_Return ||
		     event->keyval == GDK_ISO_Enter ||
		     event->keyval == GDK_KP_Enter)) {
			gtk_widget_activate (chat->send_multi_button);
			return TRUE;
		}
		
		return FALSE;
	}

	return FALSE;
}

static void
chat_input_text_buffer_changed_cb (GtkTextBuffer *buffer,
				   GossipChat    *chat)
{
	if (gtk_text_buffer_get_line_count (buffer) > 1) {
		gtk_widget_set_sensitive (chat->disclosure, FALSE);
	} else {
		gtk_widget_set_sensitive (chat->disclosure, TRUE);
	}
}

static void
chat_dialog_destroy_cb (GtkWidget *widget, GossipChat *chat)
{
	if (chat->presence_handler) {
		lm_connection_unregister_message_handler (chat->connection,
							  chat->presence_handler,
							  LM_MESSAGE_TYPE_PRESENCE);
		lm_message_handler_unref (chat->presence_handler);
	}
	if (chat->message_handler) {
		lm_connection_unregister_message_handler (chat->connection,
							  chat->message_handler,
							  LM_MESSAGE_TYPE_MESSAGE);
		lm_message_handler_unref (chat->message_handler);
	}
	
	lm_connection_unref (chat->connection);

	gossip_jid_unref (chat->jid);
	g_free (chat);
}

static void
chat_dialog_send (GossipChat *chat, const gchar *msg)
{
	LmMessage *m;
	gchar     *nick;

	if (g_ascii_strcasecmp (msg, "/clear") == 0) {
		GtkTextBuffer *buffer;
		
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->text_view));
		gtk_text_buffer_set_text (buffer, "", -1);
		
		return;
	}
	
	nick = gossip_jid_get_part_name (gossip_app_get_jid (chat->app));	
	
	gossip_text_view_append_chat_message (GTK_TEXT_VIEW (chat->text_view),
					      NULL,
					      gossip_app_get_username (chat->app),
					      nick,
					      msg);

	g_free (nick);

	m = lm_message_new_with_sub_type (gossip_jid_get_without_resource (chat->jid),
					  LM_MESSAGE_TYPE_MESSAGE,
					  LM_MESSAGE_SUB_TYPE_CHAT);
	lm_message_node_add_child (m->node, "body", msg);
	
	lm_connection_send (chat->connection, m, NULL);
	lm_message_unref (m);
}

static void
chat_dialog_send_multi_clicked_cb (GtkWidget *unused, GossipChat *chat)
{
	GtkTextBuffer *buffer;
	GtkTextIter    start, end;
	gchar         *msg;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	
	gtk_text_buffer_get_bounds (buffer, &start, &end);
	msg = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	/* Clear the input field. */
	gtk_text_buffer_set_text (buffer, "", -1);

	chat_dialog_send (chat, msg);

	g_free (msg);
}

static gboolean
chat_focus_in_event_cb (GtkWidget  *widget,
			GdkEvent   *event,
			GossipChat *chat)
{
	gint pos;

	pos = gtk_editable_get_position (GTK_EDITABLE (chat->input_entry));

	gtk_widget_grab_focus (chat->input_entry);
	gtk_editable_select_region (GTK_EDITABLE (chat->input_entry), 0, 0);

	gtk_editable_set_position (GTK_EDITABLE (chat->input_entry), pos);
	
	return TRUE;
}

static LmHandlerResult
chat_message_handler (LmMessageHandler *handler,
		      LmConnection     *connection,
		      LmMessage        *m,
		      GossipChat       *chat)
{
	const gchar   *from;
	GossipJID     *jid;
	const gchar   *timestamp = NULL;
	LmMessageNode *node;
	const gchar   *body = "";
	const gchar   *thread = "";
	gboolean       focus;
	gchar         *nick;

	from = lm_message_node_get_attribute (m->node, "from");
	jid = gossip_jid_new (from);

	d(g_print ("Incoming message:: '%s' ?= '%s'", 
		   gossip_jid_get_without_resource (jid),
		   gossip_jid_get_without_resource (chat->jid)));
	
	if (!gossip_jid_equals_without_resource (jid, chat->jid)) {
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	/* The has-toplevel-focus is new in gtk 2.2 so if we don't find it, we
	 * pretend that the window doesn't have focus (i.e. always play sounds.
	 */
	if (g_object_class_find_property (G_OBJECT_GET_CLASS (chat->dialog),
					  "has-toplevel-focus")) {
		g_object_get (chat->dialog, "has-toplevel-focus", &focus, NULL);
	} else {
		focus = FALSE;
	}

	if (!focus) {
		gossip_sound_play (GOSSIP_SOUND_CHAT);
	}

	timestamp = gossip_utils_get_timestamp_from_message (m);
	
	node = lm_message_node_get_child (m->node, "body");
	if (node) {
		body = node->value;
	} 

	node = lm_message_node_get_child (m->node, "thread");
	if (node) {
		/*g_print ("Thread set to: %s\n", node->value);*/
		thread = node->value;
	} 

	if (chat->nick) {
		nick = g_strdup (chat->nick);
	}
	else {
		nick = g_strdup (gossip_roster_get_nick_from_jid (gossip_app_get_roster (), jid));
	}
	if (!nick) {
		nick = gossip_jid_get_part_name (jid);
	}

	gossip_text_view_append_chat_message (GTK_TEXT_VIEW (chat->text_view),
					      timestamp,
					      gossip_app_get_username (chat->app),
					      nick,
					      body);

	g_free (nick);

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult
chat_presence_handler (LmMessageHandler *handler,
		       LmConnection     *connection,
		       LmMessage        *m,
		       GossipChat       *chat)
{
	const gchar   *type;
	const gchar   *show = NULL;
	const gchar   *from;
	GossipJID     *jid;
	const gchar   *filename = NULL; 
	LmMessageNode *node;
	
	g_return_val_if_fail (chat != NULL, 
			      LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS);
	g_return_val_if_fail (m != NULL, 
			      LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS);

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
	
	if (!gossip_jid_equals_without_resource (jid, chat->jid)) {
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	gossip_jid_unref (jid);
	
	if (strcmp (type, "unavailable") == 0 || 
	    strcmp (type, "error") == 0) {
		filename = gossip_status_to_icon_filename (GOSSIP_STATUS_OFFLINE);
	}
	else if (strcmp (type, "available") == 0) {
		filename = gossip_utils_get_show_filename (show);
	}

	gtk_image_set_from_file (GTK_IMAGE (chat->status_image), filename);
	
	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

GossipChat *
gossip_chat_new (GossipApp *app, GossipJID *jid)
{
	GossipChat *chat;
	
	chat = chat_create (app, jid);
	
	chat->message_handler = lm_message_handler_new ((LmHandleMessageFunction) chat_message_handler,
							chat, NULL);
	lm_connection_register_message_handler (chat->connection,
						chat->message_handler,
						LM_MESSAGE_TYPE_MESSAGE,
						LM_HANDLER_PRIORITY_NORMAL);
	
	return chat;
}

GossipChat *
gossip_chat_new_from_group_chat (GossipApp   *app, 
				 GossipJID   *jid,
				 const gchar *nick)
{
	GossipChat *chat;
	
	chat = chat_create (app, jid);
	chat->nick = g_strdup (nick);
	
	return chat;
}

void
gossip_chat_append_message (GossipChat *chat, LmMessage *m)
{
	g_return_if_fail (chat != NULL);
	g_return_if_fail (m != NULL);
	
	chat_message_handler (chat->message_handler, chat->connection,
			      m, chat);
}


GtkWidget *
gossip_chat_get_dialog (GossipChat *chat)
{
	return chat->dialog;
}
			      
