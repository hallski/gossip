/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio HB
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
#include <string.h>
#include <glade/glade.h>
#include <gtk/gtklabel.h>
#include <gtk/gtktable.h>
#include <gtk/gtksizegroup.h>
#include <gtk/gtkalignment.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-href.h>
#include <loudmouth/loudmouth.h>

#include "gossip-app.h"
#include "gossip-utils.h"
#include "gossip-contact-info.h"

#define d(x) 

struct _GossipContactInfo {
	LmConnection *connection;

	GtkWidget *dialog;
	GtkWidget *title_label;
	GtkWidget *jid_label;
	GtkWidget *personal_not_avail_label;
	GtkWidget *personal_table;
	GtkWidget *name_label;
	GtkWidget *client_not_avail_label;
	GtkWidget *client_table;
	GtkWidget *client_name_label;
	GtkWidget *version_label;
	GtkWidget *os_label;
	GtkWidget *description_textview;
	GtkWidget *close_button;

	LmMessageHandler *vcard_handler;
	LmMessageHandler *version_handler;
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
	if (info->vcard_handler) {
		lm_message_handler_invalidate (info->vcard_handler);
		lm_message_handler_unref (info->vcard_handler);
	}

	if (info->version_handler) {
		lm_message_handler_invalidate (info->version_handler);
		lm_message_handler_unref (info->version_handler);
	}

	g_free (info);
}

static void
contact_info_request_information (GossipContactInfo *info, GossipJID *jid)
{
	LmMessage        *m;
	LmMessageNode    *node;
	GError           *error = NULL;
	
	m = lm_message_new (gossip_jid_get_without_resource (jid),
			    LM_MESSAGE_TYPE_IQ);
	node = lm_message_node_add_child (m->node, "vCard", NULL);
	lm_message_node_set_attribute (node, "xmlns", "vcard-temp");

	info->vcard_handler = lm_message_handler_new ((LmHandleMessageFunction) contact_info_vcard_reply_cb,
					  info, NULL);
					  
	if (!lm_connection_send_with_reply (info->connection, m,
					    info->vcard_handler, &error)) {
		d(g_print ("Error while sending: %s\n", error->message));
		lm_message_unref (m);
		lm_message_handler_unref (info->vcard_handler);
		info->vcard_handler = NULL;
		return;
	}

	lm_message_unref (m);
	
	m = lm_message_new (gossip_jid_get_full (jid), LM_MESSAGE_TYPE_IQ);
	node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attribute (node, "xmlns", "jabber:iq:version");

	info->version_handler = lm_message_handler_new ((LmHandleMessageFunction) contact_info_version_reply_cb,
					  info, NULL);

	if (!lm_connection_send_with_reply (info->connection, m,
					    info->version_handler, &error)) {
		d(g_print ("Error while sending: %s\n", error->message));
		lm_message_unref (m);
		lm_message_handler_unref (info->version_handler);
		info->version_handler = NULL;
		return;
	}

	lm_message_unref (m);
}

static void
contact_info_dialog_close_cb (GtkWidget *widget, GossipContactInfo *info)
{
	gtk_widget_destroy (info->dialog);
}

static LmHandlerResult
contact_info_vcard_reply_cb (LmMessageHandler  *handler,
			     LmConnection      *connection,
			     LmMessage         *m,
			     GossipContactInfo *info)
{
	LmMessageNode *vCard, *node;
	gboolean       show_personal = FALSE;

	d(g_print ("Got a vcard response\n"));

	lm_message_handler_unref (info->vcard_handler);
	info->vcard_handler = NULL;
	
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

	node = lm_message_node_get_child (vCard, "FN");
	if (node) {
		show_personal = TRUE;
		
		gtk_label_set_text (GTK_LABEL (info->name_label),
				    lm_message_node_get_value (node)); 
		
		d(g_print ("Found the 'FN' tag\n"));
	}

	node = lm_message_node_get_child (vCard, "EMAIL");
	if (node && lm_message_node_get_value (node) &&
	    strcmp (lm_message_node_get_value (node), "") != 0) {
		GtkWidget *href, *alignment;
		gchar     *link;

		show_personal = TRUE;

		link = g_strdup_printf ("mailto:%s", 
					lm_message_node_get_value (node));
		
		href = gnome_href_new (link,
				       lm_message_node_get_value (node));

		alignment = gtk_alignment_new (0, 1, 0, 0.5);
		gtk_container_add (GTK_CONTAINER (alignment), href);
		
		gtk_table_attach (GTK_TABLE (info->personal_table),
				  alignment,
				  1, 2,
				  1, 2,
				  GTK_FILL, GTK_FILL,
				  0, 0);

		g_free (link);
	}
	
	node = lm_message_node_get_child (vCard, "URL");
	if (node && lm_message_node_get_value (node) &&
	    strcmp (lm_message_node_get_value (node), "") != 0) {
		GtkWidget *href, *alignment;

		show_personal = TRUE;

		href = gnome_href_new (lm_message_node_get_value (node),
				       lm_message_node_get_value (node));

		alignment = gtk_alignment_new (0, 1, 0, 0.5);
		gtk_container_add (GTK_CONTAINER (alignment), href);

		gtk_table_attach (GTK_TABLE (info->personal_table),
				  alignment, 
				  1, 2,
				  2, 3,
				  GTK_FILL, GTK_FILL,
				  0, 0);
	}

	

	if (show_personal) {
		gtk_widget_hide (info->personal_not_avail_label);
		gtk_widget_show_all (info->personal_table);
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
	gboolean       show_client_info = FALSE;

	d(g_print ("Version reply\n"));

	lm_message_handler_unref (info->version_handler);
	info->version_handler = NULL;

	query = lm_message_node_get_child (m->node, "query");
	if (!query) {
		d(g_print ("No query node\n"));
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

	node = lm_message_node_get_child (query, "name");
	if (node) {
		show_client_info = TRUE;

		gtk_label_set_text (GTK_LABEL (info->client_name_label),
				    lm_message_node_get_value (node));
	}

	node = lm_message_node_get_child (query, "version");
	if (node) {
		show_client_info = TRUE;

		gtk_label_set_text (GTK_LABEL (info->version_label),
				    lm_message_node_get_value (node));
	}

	node = lm_message_node_get_child (query, "os");
	if (node) {
		show_client_info = TRUE;

		gtk_label_set_text (GTK_LABEL (info->os_label),
				    lm_message_node_get_value (node));
	}

	if (show_client_info) {
		gtk_widget_hide (info->client_not_avail_label);
	
		gtk_widget_show_all (info->client_table);
	}

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

GossipContactInfo *
gossip_contact_info_new (GossipJID *jid, const gchar *name)
{
	GossipContactInfo *info;
	GladeXML          *gui;
	gchar             *str, *tmp_str;
	GtkSizeGroup      *size_group;

	info = g_new0 (GossipContactInfo, 1);

	info->connection = gossip_app_get_connection ();
	
	gui = gossip_glade_get_file (GLADEDIR "/main.glade",
				     "contact_information_dialog",
				     NULL,
				     "contact_information_dialog", &info->dialog,
				     "title_label", &info->title_label,
				     "jid_label", &info->jid_label,
				     "personal_not_avail_label", &info->personal_not_avail_label,
				     "personal_table", &info->personal_table,
				     "name_label", &info->name_label,
				     "client_not_avail_label", &info->client_not_avail_label,
				     "client_table", &info->client_table,
				     "client_name_label", &info->client_name_label,
				     "version_label", &info->version_label,
				     "os_label", &info->os_label,
				     "close_button", &info->close_button,
				     "description_textview", &info->description_textview,
				     NULL);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	/* A bit ugly, but the result is nice. Align the labels in the two
	 * different tables.
	 */
	gtk_size_group_add_widget (
		size_group, glade_xml_get_widget (gui, "personal_name_label"));
	gtk_size_group_add_widget (
		size_group, glade_xml_get_widget (gui, "personal_email_label"));
	gtk_size_group_add_widget (
		size_group, glade_xml_get_widget (gui, "personal_web_label"));
	gtk_size_group_add_widget (
		size_group, glade_xml_get_widget (gui, "client_client_label"));
	gtk_size_group_add_widget (
		size_group, glade_xml_get_widget (gui, "client_version_label"));
	gtk_size_group_add_widget (
		size_group, glade_xml_get_widget (gui, "client_os_label"));

	g_object_unref (size_group);
	
	g_signal_connect (info->dialog,
			  "destroy",
			  G_CALLBACK (contact_info_dialog_destroy_cb),
			  info);

	tmp_str = g_strdup_printf (_("Contact Information for %s"), name);

	gtk_window_set_title (GTK_WINDOW (info->dialog), tmp_str);

	str = g_markup_escape_text (tmp_str, -1);
	g_free (tmp_str);
	
	tmp_str = g_strdup_printf ("<b>%s</b>", str);
	g_free (str);
	gtk_label_set_markup (GTK_LABEL (info->title_label), tmp_str);
	g_free (tmp_str);

	gtk_label_set_text (GTK_LABEL (info->jid_label), gossip_jid_get_without_resource (jid));
	
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
