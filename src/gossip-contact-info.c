/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2003 Richard Hult <richard@imendio.com>
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
#include <glade/glade.h>
#include <gtk/gtklabel.h>
#include <gtk/gtktable.h>
#include <libgnome/gnome-i18n.h>
#include <loudmouth/loudmouth.h>
#include "gossip-utils.h"
#include "gossip-contact-info.h"

#define d(x) 

struct _GossipContactInfo {
	GossipApp    *app;
	LmConnection *connection;

	GtkWidget *dialog;
	GtkWidget *title_label;
	GtkWidget *personal_not_avail_label;
	GtkWidget *personal_table;
	GtkWidget *address_not_avail_label;
	GtkWidget *address_table;
	GtkWidget *additional_not_avail_label;
	GtkWidget *additional_table;
	GtkWidget *client_not_avail_label;
	GtkWidget *client_table;
	GtkWidget *description_textview;
	GtkWidget *close_button;
};

static void contact_info_request_information (GossipContactInfo *info,
					      GossipJID         *jid);
static LmHandlerResult
contact_info_vcard_reply_cb                (LmMessageHandler  *handler,
					    LmConnection      *connection,
					    LmMessage         *message,
					    GossipContactInfo *info);
static LmHandlerResult
contact_info_version_reply_cb              (LmMessageHandler  *handler,
					    LmConnection      *connection,
					    LmMessage         *message,
					    GossipContactInfo *info);

static void contact_info_dialog_close_cb     (GtkWidget         *widget,
					      GossipContactInfo *info);

static void
contact_info_dialog_destroy_cb (GtkWidget *widget, GossipContactInfo *info)
{
	/* FIXME: Need a way to cancel pending replies from loudmouth here...
	 */
#if 0
	if (info->vcard_handler) {
		g_print ("Cancelled vcard: %d\n",
			 lm_connection_cancel_... (info->connection, info->vcard_handler, NULL));
	}
	if (info->version_handler) {
		g_print ("Cancelled version: %d\n",
			 lm_connection_cancel_... (info->connection, info->version_handler, NULL));
	}
#endif	
	g_free (info);
}

static void
contact_info_request_information (GossipContactInfo *info, GossipJID *jid)
{
	LmMessage        *m;
	LmMessageNode    *node;
	LmMessageHandler *handler;
	GError           *error = NULL;
	
	m = lm_message_new (gossip_jid_get_without_resource (jid),
			    LM_MESSAGE_TYPE_IQ);
	node = lm_message_node_add_child (m->node, "vCard", NULL);
	lm_message_node_set_attribute (node, "xmlns", "vcard-temp");

	handler = lm_message_handler_new ((LmHandleMessageFunction) contact_info_vcard_reply_cb,
					  info, NULL);
					  
	if (!lm_connection_send_with_reply (info->connection, m,
					    handler, &error)) {
		d(g_print ("Error while sending: %s\n", error->message));
		lm_message_unref (m);
		lm_message_handler_unref (handler);
		return;
	}

	lm_message_unref (m);
	lm_message_handler_unref (handler);
	
	m = lm_message_new (gossip_jid_get_full (jid), LM_MESSAGE_TYPE_IQ);
	node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attribute (node, "xmlns", "jabber:iq:version");

	handler = lm_message_handler_new ((LmHandleMessageFunction) contact_info_version_reply_cb,
					  info, NULL);

	if (!lm_connection_send_with_reply (info->connection, m,
					    handler, &error)) {
		d(g_print ("Error while sending: %s\n", error->message));
		lm_message_unref (m);
		lm_message_handler_unref (handler);
		return;
	}

	lm_message_unref (m);
	lm_message_handler_unref (handler);
}

static void
contact_info_dialog_close_cb (GtkWidget *widget, GossipContactInfo *info)
{
	/* FIXME: use this instead when we can cancel pending replies in
	 * loudmouth: gtk_widget_destroy (info->dialog); */

	gtk_widget_hide (info->dialog);
}

static LmHandlerResult
contact_info_vcard_reply_cb (LmMessageHandler  *handler,
			     LmConnection      *connection,
			     LmMessage         *m,
			     GossipContactInfo *info)
{
	LmMessageNode *vCard, *node;

	d(g_print ("Got a vcard response\n"));

	vCard = lm_message_node_get_child (m->node, "vCard");
	if (!vCard) {
		d(g_print ("No vCard node\n"));
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

	node = lm_message_node_get_child (vCard, "DESC");
	if (node) {
		GtkTextBuffer *buffer;

		if (node->value) {
			buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (info->description_textview));
			gtk_text_buffer_set_text (buffer, node->value, -1);
		}
	} else {
		gtk_widget_set_sensitive (info->description_textview, FALSE);
	}

	node = lm_message_node_get_child (vCard, "N");
	if (node) {
		d(g_print ("Found the 'N' tag\n"));
	}

	node = lm_message_node_get_child (vCard, "ADR");
	if (node) {
		d(g_print ("Found the 'ADR' tag\n"));
	}

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult
contact_info_version_reply_cb (LmMessageHandler  *handler,
			       LmConnection      *connection,
			       LmMessage         *m,
			       GossipContactInfo *info)
{
	LmMessageNode *query, *node;
	GtkWidget     *name_label, *value_label;

	d(g_print ("Version reply\n"));
	query = lm_message_node_get_child (m->node, "query");
	if (!query) {
		d(g_print ("No query node\n"));
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

	gtk_widget_hide (info->client_not_avail_label);

	node = lm_message_node_get_child (query, "name");
	if (node) {
		name_label = gtk_label_new (_("Name:"));
		value_label = gtk_label_new (node->value);

		gtk_misc_set_alignment (GTK_MISC (name_label), 0, 0.5);
		gtk_misc_set_alignment (GTK_MISC (value_label), 0, 0.5);
		
		gtk_table_attach_defaults (GTK_TABLE (info->client_table),
					   name_label,
					   0, 1, 
					   0, 1);
		
		gtk_table_attach_defaults (GTK_TABLE (info->client_table),
					   value_label,
					   1, 2, 
					   0, 1);
	}
	else {
		d(g_print ("No name\n"));
	}

	node = lm_message_node_get_child (query, "version");
	if (node) {
		name_label = gtk_label_new (_("Version:"));
		value_label = gtk_label_new (node->value);
	
		gtk_misc_set_alignment (GTK_MISC (name_label), 0, 0.5);
		gtk_misc_set_alignment (GTK_MISC (value_label), 0, 0.5);
		
		gtk_table_attach_defaults (GTK_TABLE (info->client_table),
					   name_label,
					   0, 1,
					   1, 2);
		gtk_table_attach_defaults (GTK_TABLE (info->client_table),
					   value_label,
					   1, 2, 
					   1, 2);
	}

	node = lm_message_node_get_child (query, "os");
	if (node) {
		name_label = gtk_label_new (_("Operating system:"));
		value_label = gtk_label_new (node->value);
		
		gtk_misc_set_alignment (GTK_MISC (name_label), 0, 0.5);
		gtk_misc_set_alignment (GTK_MISC (value_label), 0, 0.5);
		
		gtk_table_attach_defaults (GTK_TABLE (info->client_table),
					   name_label,
					   0, 1, 
					   2, 3);
		gtk_table_attach_defaults (GTK_TABLE (info->client_table),
					   value_label,
					   1, 2, 
					   2, 3);
	}

	gtk_widget_show_all (info->client_table);

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

GossipContactInfo *
gossip_contact_info_new (GossipApp *app, GossipJID *jid, const gchar *name)
{
	GossipContactInfo *info;
	gchar             *str, *tmp_str;

	info = g_new0 (GossipContactInfo, 1);

	info->app = app;
	info->connection = gossip_app_get_connection (app);
	
	gossip_glade_get_file_simple (GLADEDIR "/main.glade",
				      "contact_information_dialog",
				      NULL,
				      "contact_information_dialog", &info->dialog,
				      "title_label", &info->title_label,
				      "personal_not_avail_label", &info->personal_not_avail_label,
				      "personal_table", &info->personal_table,
				      "address_not_avail_label", &info->address_not_avail_label,
				      "address_table", &info->address_table,
				      "additional_not_avail_label", &info->additional_not_avail_label,
				      "additional_table", &info->additional_table,
				      "client_not_avail_label", &info->client_not_avail_label,
				      "client_table", &info->client_table,
				      "close_button", &info->close_button,
				      "description_textview", &info->description_textview,
				      NULL);

	g_signal_connect (info->dialog,
			  "destroy",
			  G_CALLBACK (contact_info_dialog_destroy_cb),
			  info);

	tmp_str = g_strdup_printf (_("Information about %s"), name);
	str = g_strdup_printf ("<b>%s</b>", tmp_str);
	g_free (tmp_str);
	gtk_label_set_markup (GTK_LABEL (info->title_label), str);
	g_free (str);
	
	g_signal_connect (info->close_button,
			  "clicked",
			  G_CALLBACK (contact_info_dialog_close_cb),
			  info);
	
	contact_info_request_information (info, jid);

	gtk_widget_show (info->dialog);

	return info;
}

GtkWidget *
gossip_contact_info_get_dialog (GossipContactInfo *info)
{
	return info->dialog;
}

