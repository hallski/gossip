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
#include "gossip-utils.h"
#include "gossip-sound.h"
#include "gossip-chat.h"
#include "gossip-app.h"
#include "gossip-contact-info.h"
#include "disclosure-widget.h"

#define d(x) 
#define IS_ENTER(v) (v == GDK_Return || v == GDK_ISO_Enter || v == GDK_KP_Enter)
#define COMPOSING_STOP_TIMEOUT 5


struct _GossipChat {
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
	GtkWidget        *info_button;
	GtkWidget        *from_label;
	GtkWidget        *composing_image;

	GossipJID        *jid;
	gchar            *nick;

	guint             composing_stop_timeout_id;
	gboolean          request_composing_events;
	gboolean          send_composing_events;
	gchar            *last_composing_id;
	
	/* Chat exists but has been hidden by the user. */
	gboolean          hidden;
};


static void        destroy_notify_cb                 (GossipChat      *chat);
static void        chat_init                         (void);
static GossipChat *chat_get_for_jid                  (GossipJID       *jid,
						      gboolean         priv_group_chat);
static void        chat_create_gui                   (GossipChat      *chat);
GtkWidget *        chat_create_disclosure            (gpointer         data);
static void        chat_disclosure_toggled_cb        (GtkToggleButton *disclosure,
						      GossipChat      *chat);
static void        chat_input_activate_cb            (GtkWidget       *entry,
						      GossipChat      *chat);
static gboolean    chat_input_key_press_event_cb     (GtkWidget       *widget,
						      GdkEventKey     *event,
						      GossipChat      *chat);
static void        chat_input_entry_changed_cb       (GtkWidget       *widget,
						      GossipChat      *chat);
static void        chat_input_text_buffer_changed_cb (GtkTextBuffer   *buffer,
						      GossipChat      *chat);
static void        chat_dialog_destroy_cb            (GtkWidget       *widget,
						      GossipChat      *chat); 
static gboolean    chat_dialog_delete_event_cb       (GtkWidget       *widget,
						      GdkEvent        *event,
						      GossipChat      *chat);
static void        chat_dialog_notify_visible_cb     (GtkWidget       *dialog,
						      GParamSpec      *spec,
						      GossipChat      *chat);
static gboolean    chat_key_press_event_cb           (GtkWidget       *dialog,
						      GdkEventKey     *event,
						      GossipChat      *chat);
static void        chat_info_clicked_cb              (GtkWidget       *widget,
						      GossipChat      *chat);
static void        chat_send                         (GossipChat      *chat,
						      const gchar     *msg);
static void        chat_send_multi_clicked_cb        (GtkWidget       *unused,
						      GossipChat      *chat);
static gboolean    chat_text_view_focus_in_event_cb  (GtkWidget       *widget,
						      GdkEvent        *event,
						      GossipChat      *chat);
static gboolean    chat_focus_in_event_cb            (GtkWidget       *widget,
						      GdkEvent        *event,
						      GossipChat      *chat);

static LmHandlerResult
chat_message_handler                                 (LmMessageHandler *handler,
						      LmConnection     *connection,
						      LmMessage        *m,
						      gpointer          user_data);
static LmHandlerResult
chat_presence_handler                                (LmMessageHandler *handler,
						      LmConnection     *connection,
						      LmMessage        *m,
						      gpointer          user_data);
static void        chat_update_title                 (GossipChat       *chat,
						      gboolean          new_message);
static void        chat_hide                         (GossipChat       *chat);
static void        chat_request_composing            (LmMessage        *m);
static gboolean    chat_event_handler                (GossipChat       *chat,
						      LmMessage        *m);
static void        chat_composing_start              (GossipChat       *chat);
static void        chat_composing_send_start_event   (GossipChat       *chat);
static void        chat_composing_stop               (GossipChat       *chat);
static void        chat_composing_send_stop_event    (GossipChat       *chat);
static void        chat_composing_remove_timeout     (GossipChat       *chat);
static gboolean    chat_composing_stop_timeout_cb    (GossipChat       *chat);
static void        chat_show_composing_icon          (GossipChat       *chat,
						      gboolean          is_composing);


static GHashTable *chats = NULL;


static void
destroy_notify_cb (GossipChat *chat)
{
	LmConnection     *connection;
	LmMessageHandler *handler;

	connection = gossip_app_get_connection ();
	
	handler = chat->presence_handler;
	if (handler) {
		lm_connection_unregister_message_handler (
			connection, handler, LM_MESSAGE_TYPE_PRESENCE);
		lm_message_handler_unref (handler);
	}
	
	handler = chat->message_handler;
	if (handler) {
		lm_connection_unregister_message_handler (
			connection, handler, LM_MESSAGE_TYPE_MESSAGE);
		lm_message_handler_unref (handler);
	}

	gossip_jid_unref (chat->jid);
	g_free (chat);
}

static void
chat_dialog_destroy_cb (GtkWidget *widget, GossipChat *chat)
{
	g_hash_table_remove (chats,
			     gossip_jid_get_without_resource (chat->jid));
}

static gboolean
chat_dialog_delete_event_cb (GtkWidget  *widget,
			     GdkEvent   *event,
			     GossipChat *chat)
{
	chat_hide (chat);

	return TRUE;
}

static void
chat_dialog_notify_visible_cb (GtkWidget  *window,
			       GParamSpec *spec,
			       GossipChat *chat)
{
	gboolean visible;
	
	g_object_get (window, "visible", &visible, NULL);

	if (visible) {
		chat->hidden = FALSE;
	} else {
		chat_composing_stop (chat);
	}
}
	
static void
chat_init (void)
{
	static gboolean inited = FALSE;

	if (inited) {
		return;
	}

	inited = TRUE;

	chats = g_hash_table_new_full (g_str_hash,
				       g_str_equal,
				       NULL,
				       (GDestroyNotify) destroy_notify_cb);
}

static GossipChat *
chat_get_for_jid (GossipJID  *jid,
		  gboolean    priv_group_chat)
{
	GossipChat       *chat;
	const gchar      *without_resource;
	LmMessageHandler *handler;
	LmConnection     *connection;
	
	chat_init ();
	
	without_resource = gossip_jid_get_without_resource (jid);
	chat = g_hash_table_lookup (chats, without_resource);
	if (chat) {
		return chat;
	}

	chat = g_new0 (GossipChat, 1);
	
	chat->jid = gossip_jid_ref (jid);
	chat->hidden = FALSE;
		
	if (priv_group_chat) {
		chat->nick = g_strdup (gossip_jid_get_resource (jid));
	}
		
	chat_create_gui (chat);

	g_hash_table_insert (chats, (gchar *)without_resource, chat);

	connection = gossip_app_get_connection ();
	
	handler = lm_message_handler_new (chat_presence_handler, chat, NULL);
	chat->presence_handler = handler;
	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_PRESENCE,
						LM_HANDLER_PRIORITY_NORMAL);

	if (!priv_group_chat) {
		handler = lm_message_handler_new (chat_message_handler, chat, NULL);
		chat->message_handler = handler;
		lm_connection_register_message_handler (connection,
							handler,
							LM_MESSAGE_TYPE_MESSAGE,
							LM_HANDLER_PRIORITY_NORMAL);
	}
	
	/* Event stuff */
	chat->request_composing_events = TRUE;

	return chat;
}

static void
chat_create_gui (GossipChat *chat) 
{
	GossipRoster  *roster;
	gchar         *name;
	GtkTextBuffer *buffer;
	GtkTooltips   *tooltips;
	GtkWidget     *from_eventbox;
	GdkPixbuf     *pixbuf;
	
	gossip_glade_get_file_simple (GLADEDIR "/chat.glade",
				      "chat_window",
				      NULL,
				      "chat_window", &chat->dialog,
				      "chat_textview", &chat->text_view,
				      "input_entry", &chat->input_entry,
				      "input_textview", &chat->input_text_view,
				      "single_hbox", &chat->single_hbox,
				      "multi_vbox", &chat->multi_vbox,
				      "status_image", &chat->status_image,
				      "from_eventbox", &from_eventbox,
				      "info_button", &chat->info_button,
				      "from_label", &chat->from_label,
				      "disclosure", &chat->disclosure,
				      "send_multi_button", &chat->send_multi_button,
				      "composing_image", &chat->composing_image,
				      NULL);

	roster = gossip_app_get_roster ();

	gtk_image_set_from_file (GTK_IMAGE (chat->composing_image),
				 IMAGEDIR "/typing.png");
	
	pixbuf = gossip_roster_get_status_pixbuf_for_jid (roster, chat->jid);
	if (pixbuf) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (chat->status_image),
					   pixbuf);
	}

	if (chat->nick) {
		gtk_label_set_text (GTK_LABEL (chat->from_label), chat->nick);
	} else {
		name = g_strdup (gossip_roster_get_nick_from_jid (roster, chat->jid));
		if (!name) {
			name = gossip_jid_get_part_name (chat->jid);
		}

		gtk_label_set_text (GTK_LABEL (chat->from_label), name);
		g_free (name);
	}
	
	tooltips = gtk_tooltips_new ();

	gtk_tooltips_set_tip (tooltips,
			      from_eventbox,
			      gossip_jid_get_full (chat->jid),
			      gossip_jid_get_full (chat->jid));
	
	g_signal_connect (chat->dialog,
			  "destroy",
			  G_CALLBACK (chat_dialog_destroy_cb),
			  chat);

	g_signal_connect (chat->dialog,
			  "delete_event",
			  G_CALLBACK (chat_dialog_delete_event_cb),
			  chat);

	g_signal_connect (chat->dialog,
			  "notify::visible",
			  G_CALLBACK (chat_dialog_notify_visible_cb),
			  chat);
	
	g_signal_connect (chat->info_button,
			  "clicked",
			  G_CALLBACK (chat_info_clicked_cb),
			  chat);

	g_signal_connect (chat->disclosure,
			  "toggled",
			  G_CALLBACK (chat_disclosure_toggled_cb),
			  chat);
	
	g_signal_connect (chat->send_multi_button,
			  "clicked",
			  G_CALLBACK (chat_send_multi_clicked_cb),
			  chat);

	g_signal_connect (chat->dialog,
			  "key_press_event",
			  G_CALLBACK (chat_key_press_event_cb),
			  chat);

	g_signal_connect (chat->input_entry,
			  "activate",
			  G_CALLBACK (chat_input_activate_cb),
			  chat);

	g_signal_connect (chat->input_text_view,
			  "key_press_event",
			  G_CALLBACK (chat_input_key_press_event_cb),
			  chat);

	g_signal_connect (chat->input_entry,
			  "key_press_event",
			  G_CALLBACK (chat_input_key_press_event_cb),
			  chat);

	g_signal_connect (chat->input_entry,
			  "changed",
			  G_CALLBACK (chat_input_entry_changed_cb),
			  chat);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	g_signal_connect (buffer,
			  "changed",
			  G_CALLBACK (chat_input_text_buffer_changed_cb),
			  chat);

	g_signal_connect (chat->text_view,
			  "focus_in_event",
			  G_CALLBACK (chat_text_view_focus_in_event_cb),
			  chat);

	g_signal_connect (chat->dialog,
			  "focus_in_event",
			  G_CALLBACK (chat_focus_in_event_cb),
			  chat);

	gossip_text_view_set_margin (GTK_TEXT_VIEW (chat->text_view), 3);
	gossip_text_view_setup_tags (GTK_TEXT_VIEW (chat->text_view));

	chat_update_title (chat, FALSE);
		
	gtk_widget_grab_focus (chat->input_entry);
}

GtkWidget *
chat_create_disclosure (gpointer data)
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

static void
chat_input_activate_cb (GtkWidget *entry, GossipChat *chat)
{
	gchar *msg;

	msg = gtk_editable_get_chars (GTK_EDITABLE (chat->input_entry), 0, -1);

	/* Clear the input field. */
	gtk_entry_set_text (GTK_ENTRY (chat->input_entry), "");

	chat_send (chat, msg);

	g_free (msg);
}

static gboolean
chat_input_key_press_event_cb (GtkWidget   *widget,
			       GdkEventKey *event,
			       GossipChat  *chat)
{
	/* Catch ctrl-enter. */
	if ((event->state & GDK_CONTROL_MASK) && IS_ENTER (event->keyval)) {
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chat->disclosure))) {
			gtk_widget_activate (chat->send_multi_button);
		} else {
			gtk_widget_activate (chat->input_entry);
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

	if (chat->send_composing_events) {
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
		gtk_widget_set_sensitive (chat->disclosure, FALSE);
	} else {
		gtk_widget_set_sensitive (chat->disclosure, TRUE);
	}

	if (chat->send_composing_events) {
		if (gtk_text_buffer_get_char_count (buffer) == 0) {
			chat_composing_stop (chat);
		} else {
			chat_composing_start (chat);
		}
	}
}

static gboolean
chat_key_press_event_cb (GtkWidget   *dialog,
			 GdkEventKey *event,
			 GossipChat  *chat)
{
	/* C-w closes (a bit hacky...) */
	if ((event->state & GDK_CONTROL_MASK) && (event->keyval == GDK_w)) {
		chat_hide (chat);
		return TRUE;
	}

	return FALSE;
}
	
static void
chat_info_clicked_cb (GtkWidget  *widget,
		      GossipChat *chat)
{
	GossipRoster *roster;
	const gchar  *name;

	roster = gossip_app_get_roster ();
	
	name = gossip_roster_get_nick_from_jid (roster, chat->jid);
	if (name && name[0]) {
		gossip_contact_info_new (gossip_app_get (), chat->jid, name);
	}
}

static void
chat_send (GossipChat *chat, const gchar *msg)
{
	LmMessage *m;
	gchar     *nick;
	
	if (msg == NULL || msg[0] == '\0') {
		return;
	}
	
	if (g_ascii_strcasecmp (msg, "/clear") == 0) {
		GtkTextBuffer *buffer;
		
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->text_view));
		gtk_text_buffer_set_text (buffer, "", -1);
		
		return;
	}

	chat_composing_remove_timeout (chat);

	nick = gossip_jid_get_part_name (gossip_app_get_jid ());	
	
	gossip_text_view_append_chat_message (GTK_TEXT_VIEW (chat->text_view),
					      NULL,
					      gossip_app_get_username (),
					      nick,
					      msg);

	g_free (nick);

	m = lm_message_new_with_sub_type (gossip_jid_get_full (chat->jid),
					  LM_MESSAGE_TYPE_MESSAGE,
					  LM_MESSAGE_SUB_TYPE_CHAT);
	lm_message_node_add_child (m->node, "body", msg);

	if (chat->request_composing_events) {
		chat_request_composing (m);
	}
	
	lm_connection_send (gossip_app_get_connection (), m, NULL);
	lm_message_unref (m);
}

static void
chat_send_multi_clicked_cb (GtkWidget *unused, GossipChat *chat)
{
	GtkTextBuffer *buffer;
	GtkTextIter    start, end;
	gchar         *msg;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	
	gtk_text_buffer_get_bounds (buffer, &start, &end);
	msg = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	/* Clear the input field. */
	gtk_text_buffer_set_text (buffer, "", -1);

	chat_send (chat, msg);

	g_free (msg);
}

static gboolean
chat_text_view_focus_in_event_cb (GtkWidget  *widget,
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

static gboolean
chat_focus_in_event_cb (GtkWidget  *widget,
			GdkEvent   *event,
			GossipChat *chat)
{
	chat_update_title (chat, FALSE);
	
	return FALSE;
}

static LmHandlerResult
chat_message_handler (LmMessageHandler *handler,
		      LmConnection     *connection,
		      LmMessage        *m,
		      gpointer          user_data)
{
	GossipChat       *chat = user_data;
	const gchar      *from;
	LmMessageSubType  type;
	GossipJID        *jid;
	const gchar      *timestamp = NULL;
	LmMessageNode    *node;
	const gchar      *body = "";
	const gchar      *thread = "";
	gboolean          focus;
	gchar            *nick;

	from = lm_message_node_get_attribute (m->node, "from");

	jid = gossip_jid_new (from);

	d(g_print ("Incoming message:: '%s' ?= '%s'", 
		   gossip_jid_get_without_resource (jid),
		   gossip_jid_get_without_resource (chat->jid)));
	
	if (!gossip_jid_equals_without_resource (jid, chat->jid)) {
		gossip_jid_unref (jid);

		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}
	
	type = lm_message_get_sub_type (m);

	if (type == LM_MESSAGE_SUB_TYPE_ERROR) {
		GtkWidget *dialog;
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

		dialog = gtk_message_dialog_new (GTK_WINDOW (chat->dialog),
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

		g_free (msg);
		gossip_jid_unref (jid);

		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

	if (chat_event_handler (chat, m)) {
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

	/* The has-toplevel-focus is new in gtk 2.2 so if we don't find it, we
	 * pretend that the window doesn't have focus (i.e. always play sounds).
	 */
	if (g_object_class_find_property (G_OBJECT_GET_CLASS (chat->dialog),
					  "has-toplevel-focus")) {
		g_object_get (chat->dialog, "has-toplevel-focus", &focus, NULL);
	} else {
		focus = FALSE;
	}

	if (!focus) {
		gossip_sound_play (GOSSIP_SOUND_CHAT);
		chat_update_title (chat, TRUE);
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
	} else {
		nick = g_strdup (gossip_roster_get_nick_from_jid (gossip_app_get_roster (), jid));
	}
	if (!nick) {
		nick = gossip_jid_get_part_name (jid);
	}

	gossip_text_view_append_chat_message (GTK_TEXT_VIEW (chat->text_view),
					      timestamp,
					      gossip_app_get_username (),
					      nick,
					      body);

	g_free (nick);
	gossip_jid_unref (jid);

	if (chat->hidden) {
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
	GossipChat    *chat = user_data;
	const gchar   *type;
	const gchar   *show = NULL;
	const gchar   *from;
	GossipJID     *jid;
	const gchar   *filename = NULL; 
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
	
	if (!gossip_jid_equals_without_resource (jid, chat->jid)) {
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	gossip_jid_unref (jid);
	
	if (strcmp (type, "unavailable") == 0 || strcmp (type, "error") == 0) {
		filename = gossip_status_to_icon_filename (GOSSIP_STATUS_OFFLINE);

		chat_composing_remove_timeout (chat);
		chat->send_composing_events = FALSE;
		chat_show_composing_icon (chat, FALSE);
	}
	else if (strcmp (type, "available") == 0) {
		filename = gossip_utils_get_show_filename (show);
	}

	gtk_image_set_from_file (GTK_IMAGE (chat->status_image), filename);
	
	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static void
chat_update_title (GossipChat *chat, gboolean new_message)
{
	gchar        *nick = NULL, *title;
	GossipRoster *roster;
	
	if (chat->nick) {
		nick = g_strdup (chat->nick);
	}
	
	if (!nick) {
		roster = gossip_app_get_roster ();
		nick = g_strdup (gossip_roster_get_nick_from_jid (roster, chat->jid));
	}
	
	if (!nick) {
		nick = gossip_jid_get_part_name (chat->jid);
	}
	
	if (nick && nick[0]) {
		/*
		Translators: This is for the title of the chat window. The
		first %s is an "* " that gets displayed if the chat window has
		new messages in it. (Please complain if this doesn't work well
		in your locale.)
		*/
		title = g_strdup_printf (_("%sChat - %s"), new_message ? "* " : "", nick);
	} else {
		/*
		Translators: See comment for "%sChat - %s".
		*/
		title = g_strdup_printf (_("%sChat"), new_message ? "* " : "");
	}
	g_free (nick);
	
	gtk_window_set_title (GTK_WINDOW (chat->dialog), title);
	g_free (title);
}

static void
chat_hide (GossipChat *chat) 
{
	/* g_print ("Remove all but the last 10 rows in the text area.\n"); */

	gtk_widget_hide (chat->dialog);
	chat->hidden = TRUE;
}

GossipChat *
gossip_chat_get_for_jid (GossipJID *jid)
{
	return chat_get_for_jid (jid, FALSE);
}

GossipChat *
gossip_chat_get_for_group_chat (GossipJID *jid)
{
	return chat_get_for_jid (jid, TRUE);
}

void
gossip_chat_append_message (GossipChat *chat, LmMessage *m)
{
	LmConnection *connection;
	
	g_return_if_fail (chat != NULL);
	g_return_if_fail (m != NULL);

	connection = gossip_app_get_connection ();
	
	/* If the chat exists but has been hidden by the user we have handlers 
	 * that has already appended the message, this is something of a 
	 * work-around before we have logging that can be used to show last
	 * couple of lines in a chat window.
	 */
	if (!chat->hidden) {
		chat_message_handler (chat->message_handler,
				      connection, m, chat);
	}
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

	chat = chat_get_for_jid (jid, FALSE);
	if (chat) {
		gossip_chat_append_message (chat, m);
		result = LM_HANDLER_RESULT_REMOVE_MESSAGE;
	} else {
		result = LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	gossip_jid_unref (jid);

	return result;
}
			      
GtkWidget *
gossip_chat_get_dialog (GossipChat *chat)
{
	g_return_val_if_fail (chat != NULL, NULL);
	
	return chat->dialog;
}

static void
chat_request_composing (LmMessage  *m)
{
	static guint id = 0;
	static gchar str_id[16];
	LmMessageNode *x;

	x = lm_message_node_add_child (m->node, "x", NULL);

	lm_message_node_set_attribute (x,
				       "xmlns",
				       "jabber:x:event");
	lm_message_node_add_child (x, "composing", NULL);
	
	g_snprintf (str_id, 16, "m_%d", id++);

	lm_message_node_set_attribute (m->node, "id", str_id);
}

static gboolean
chat_event_handler (GossipChat *chat, LmMessage *m)
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

			chat->send_composing_events = TRUE;

			g_free (chat->last_composing_id);
			new_id = lm_message_node_get_attribute (m->node, "id");
			if (new_id) {
				chat->last_composing_id = g_strdup (new_id);
			}
		}

		chat_show_composing_icon (chat, FALSE);

		return FALSE;
	}
	
	if (lm_message_node_get_child (x, "composing")) {
		chat_show_composing_icon (chat, TRUE);
	} else {
		chat_show_composing_icon (chat, FALSE);
	}
	
	return TRUE;
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
	
	m = lm_message_new_with_sub_type (gossip_jid_get_full (chat->jid),
					  LM_MESSAGE_TYPE_MESSAGE,
					  LM_MESSAGE_SUB_TYPE_CHAT);
	x = lm_message_node_add_child (m->node, "x", NULL);
	lm_message_node_set_attribute (x, "xmlns", "jabber:x:event");
	lm_message_node_add_child (x, "id", chat->last_composing_id);
	lm_message_node_add_child (x, "composing", NULL);
	
	lm_connection_send (connection, m, NULL);
	lm_message_unref (m);
}

static void
chat_composing_start (GossipChat *chat)
{
	if (!chat->send_composing_events) {
		return;
	}

	if (chat->composing_stop_timeout_id) {
		/* Just restart the timeout. */
		chat_composing_remove_timeout (chat);
	} else {
		chat_composing_send_start_event (chat);
	}
	
	chat->composing_stop_timeout_id = g_timeout_add (
		1000 * COMPOSING_STOP_TIMEOUT,
		(GSourceFunc) chat_composing_stop_timeout_cb,
		chat);
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

	m = lm_message_new_with_sub_type (gossip_jid_get_full (chat->jid),
					  LM_MESSAGE_TYPE_MESSAGE,
					  LM_MESSAGE_SUB_TYPE_CHAT);
	x = lm_message_node_add_child (m->node, "x", NULL);
	lm_message_node_set_attribute (x, "xmlns", "jabber:x:event");
	lm_message_node_add_child (x, "id", chat->last_composing_id);
	
	lm_connection_send (connection, m, NULL);
	lm_message_unref (m);
}

static void
chat_composing_stop (GossipChat *chat)
{
	if (!chat->send_composing_events) {
		return;
	}

	chat_composing_remove_timeout (chat);
	chat_composing_send_stop_event (chat);
}

static void
chat_composing_remove_timeout (GossipChat *chat)
{
	if (chat->composing_stop_timeout_id) {
		g_source_remove (chat->composing_stop_timeout_id);
		chat->composing_stop_timeout_id = 0;
	}
}

static gboolean
chat_composing_stop_timeout_cb (GossipChat *chat)
{
	chat->composing_stop_timeout_id = 0;
	chat_composing_send_stop_event (chat);
	
	return FALSE;
}

static void
chat_show_composing_icon (GossipChat *chat, gboolean is_composing)
{
	if (is_composing) {
		gtk_widget_show (chat->composing_image);
	} else {
		gtk_widget_hide (chat->composing_image);
	}
}

