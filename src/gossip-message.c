/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2003 CodeFactory AB
 * Copyright (C) 2002      Richard Hult <rhult@imendo.com>
 * Copyright (C) 2003      Mikael Hallendal <micke@imendo.com>
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
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <loudmouth/loudmouth.h>
#include "gossip-utils.h"
#include "gossip-message.h"
#include "gossip-roster.h"
#include "gossip-app.h"

struct _GossipMessage {
	GossipApp    *app;
	LmConnection *connection;
	GtkWidget    *dialog;
	GtkWidget    *textview;
	GtkWidget    *input_entry;

	GtkWidget    *to_entry;
	GtkWidget    *replyto_textview;
	GtkWidget    *replyto_vbox;
	
	GossipJID    *jid;

	LmMessage    *lm_message;
};

static void
message_dialog_destroy_cb (GtkWidget     *widget,
			   GossipMessage *message)
{
	if (message->connection) {
		lm_connection_unref (message->connection);
	}
	
	if (message->lm_message) {
		lm_message_unref (message->lm_message);
	}
		
	gossip_jid_unref (message->jid);
	g_free (message);
}

static void
message_send_dialog_response_cb (GtkWidget     *dialog,
				 gint           response,
				 GossipMessage *message)
{
	GtkTextBuffer *buffer;
	gchar         *msg;
	const gchar   *to;
	GtkTextIter    start, end;
	LmMessage     *m;
	
	switch (response) {
	case GTK_RESPONSE_OK:
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (message->textview));

		gtk_text_buffer_get_bounds (buffer, &start, &end);
		
		msg = gtk_text_buffer_get_text (buffer,
						&start,
						&end,
						FALSE); 

		to = gtk_entry_get_text (GTK_ENTRY (message->to_entry));

		/* FIXME: don't allow pressing send without address. */
		if (!to || strlen (to) == 0 || strstr (to, "@") == 0) {
			g_free (msg);
			return;
		}
		
		m = lm_message_new (to, LM_MESSAGE_TYPE_MESSAGE);
		lm_message_node_add_child (m->node, "subject", "test");
		lm_message_node_add_child (m->node, "body", msg);
		g_free (msg);

		lm_connection_send (message->connection, m, NULL);
		lm_message_unref (m);
		break;

	case GTK_RESPONSE_DELETE_EVENT:
	case GTK_RESPONSE_NONE:
	default:
		break;
	}
	
	gtk_widget_destroy (dialog);
}

GossipMessage *
gossip_message_send_dialog_new (GossipApp    *app,
				GossipJID    *jid,
				gboolean      is_reply,
				const gchar  *reply_msg)
{
	GossipMessage *message;
	GtkTextBuffer *buffer;
	GtkWidget     *padding;

	g_return_val_if_fail (GOSSIP_IS_APP (app), NULL);
	
	message = g_new0 (GossipMessage, 1);
	
	message->app = app;
	message->connection = lm_connection_ref (gossip_app_get_connection (app));
	
	gossip_glade_get_file_simple (GLADEDIR "/chat.glade",
				      "send_message_dialog",
				      NULL,
				      "send_message_dialog", &message->dialog,
				      "message_textview", &message->textview,
				      "replyto_textview", &message->replyto_textview,
				      "replyto_vbox", &message->replyto_vbox,
				      "padding_label", &padding,
				      "to_entry", &message->to_entry,
				      NULL);

	if (jid) {
		gtk_entry_set_text (GTK_ENTRY (message->to_entry), 
				    gossip_jid_get_full (jid));
	}
	
	if (!is_reply) {
		gtk_widget_hide (message->replyto_vbox);
	} else {
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (message->replyto_textview));
		gtk_text_buffer_set_text (buffer, reply_msg, -1);
		gtk_widget_hide (padding);
	}

	gtk_widget_grab_focus (message->textview);
	
	g_signal_connect (message->dialog,
			  "response",
			  G_CALLBACK (message_send_dialog_response_cb),
			  message);

	gtk_widget_show (message->dialog);

	return message;
}

static void
message_recv_dialog_response_cb (GtkWidget     *dialog,
				 gint           response,
				 GossipMessage *message)
{
	const gchar   *from;
	GossipJID     *jid;
	const gchar   *body = "";
	LmMessageNode *node;
	
	g_return_if_fail (message->lm_message != NULL);
	
	from = lm_message_node_get_attribute (message->lm_message->node, 
					      "from");
	jid = gossip_jid_new (from);
	node = lm_message_node_get_child (message->lm_message->node, "body");
	if (node) {
		body = node->value;
	}

	switch (response) {
	case GTK_RESPONSE_OK:
		gossip_message_send_dialog_new (message->app, jid, TRUE, body);
		break;
		
	case GTK_RESPONSE_DELETE_EVENT:
	case GTK_RESPONSE_NONE:
	default:
		break;
	}

	gossip_jid_unref (jid);
	gtk_widget_destroy (dialog);
}

static GossipMessage *
message_new (GossipApp *app, LmMessage *m)
{
	GossipMessage *message;
	GtkTextBuffer *buffer;
	GtkWidget     *from_label;
	gchar         *from;
	const gchar   *name;
	GossipRoster  *roster;
	LmMessageNode *node;
	
	g_return_val_if_fail (GOSSIP_IS_APP (app), NULL);
	g_return_val_if_fail (m != NULL, NULL);

	message = g_new0 (GossipMessage, 1);

	message->app = app;
	message->connection = lm_connection_ref (gossip_app_get_connection (app));
	message->jid = gossip_jid_new (lm_message_node_get_attribute (m->node, 
								      "from"));
	message->lm_message = lm_message_ref (m);

	gossip_glade_get_file_simple (GLADEDIR "/chat.glade",
				      "recv_message_dialog",
				      NULL,
				      "recv_message_dialog", &message->dialog,
				      "message_textview", &message->textview,
				      "from_label", &from_label,
				      NULL);

	roster = gossip_app_get_roster (app);
	name = gossip_roster_get_nick_from_jid (roster, message->jid);
	if (name) {
		from = g_strdup_printf ("%s <%s>", name, 
					gossip_jid_get_without_resource (message->jid));
	} else {
		from = (gchar *) gossip_jid_get_without_resource (message->jid);
	}
	gtk_label_set_text (GTK_LABEL (from_label), from);
	if (from != gossip_jid_get_without_resource (message->jid)) {
		g_free (from);
	}
	
	g_signal_connect (message->dialog,
			  "response",
			  G_CALLBACK (message_recv_dialog_response_cb),
			  message);

	g_signal_connect (message->dialog,
			  "destroy",
			  G_CALLBACK (message_dialog_destroy_cb),
			  message);
		
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (message->textview));
		
	gtk_text_buffer_create_tag (buffer,
				    "bracket",
				    "foreground", "blue",
				    NULL);	
	
	/* No highlighting in one to one message. */
	gtk_text_buffer_create_tag (buffer,
				    "highlight",
				    NULL);

	node = lm_message_node_get_child (m->node, "body");
	
	gossip_text_view_append_normal_message (GTK_TEXT_VIEW (message->textview),
						node->value);

	gtk_widget_show (message->dialog);
	gossip_text_view_set_margin (GTK_TEXT_VIEW (message->textview), 3);

	return message;
}

void
gossip_message_handle_message (GossipApp *app, LmMessage *message)
{
	message_new (app, message);
}
			      


