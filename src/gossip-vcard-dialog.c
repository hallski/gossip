/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Martyn Russell <ginxd@btopenworld.com>
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
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-href.h>
#include <loudmouth/loudmouth.h>

#include "gossip-app.h"
#include "gossip-vcard-dialog.h"

#define d(x) 


struct _GossipVCardDialog {
	LmConnection *connection;

	LmMessageHandler *vcard_get_handler;
	LmMessageHandler *vcard_set_handler;

	GtkWidget *dialog;

	GtkWidget *label_status;

	GtkWidget *vbox_personal_information;
	GtkWidget *vbox_description;

	GtkWidget *entry_name;
	GtkWidget *entry_nickname;
	GtkWidget *entry_web_site;
	GtkWidget *entry_email;

	GtkWidget *textview_description;
};


typedef struct _GossipVCardDialog GossipVCardDialog;


static void            vcard_dialog_get_vcard    (GossipVCardDialog *dialog);

static LmHandlerResult vcard_dialog_get_vcard_cb (LmMessageHandler  *handler,
						  LmConnection      *connection,
						  LmMessage         *message,
						  GossipVCardDialog *dialog);

static void            vcard_dialog_set_vcard    (GossipVCardDialog *dialog);

static LmHandlerResult vcard_dialog_set_vcard_cb (LmMessageHandler  *handler,
						  LmConnection      *connection,
						  LmMessage         *message,
						  GossipVCardDialog *dialog);

static void            vcard_dialog_response_cb  (GtkDialog         *widget,
						  gint               response,
						  GossipVCardDialog *dialog);

static void            vcard_dialog_destroy_cb   (GtkWidget         *widget,
						  GossipVCardDialog *dialog);

void
gossip_vcard_dialog_show ()
{
	GossipVCardDialog *dialog;
	GladeXML          *gui;

	dialog = g_new0 (GossipVCardDialog, 1);

	dialog->connection = gossip_app_get_connection ();

	gui = gossip_glade_get_file (GLADEDIR "/main.glade",
				     "vcard_dialog",
				     NULL,
				     "vcard_dialog", &dialog->dialog,
				     "label_status", &dialog->label_status,
				     "vbox_personal_information", &dialog->vbox_personal_information,
				     "vbox_description", &dialog->vbox_description,
				     "entry_name", &dialog->entry_name,
				     "entry_nickname", &dialog->entry_nickname,
				     "entry_web_site", &dialog->entry_web_site,
				     "entry_email", &dialog->entry_email,
				     "textview_description", &dialog->textview_description,
				     NULL);

	gossip_glade_connect (gui, 
			      dialog,
			      "vcard_dialog", "destroy", vcard_dialog_destroy_cb,
			      "vcard_dialog", "response", vcard_dialog_response_cb,
			      NULL);

	g_object_unref (gui);
	
	/* request current vCard */
	vcard_dialog_get_vcard (dialog);
}

static void
vcard_dialog_get_vcard (GossipVCardDialog *dialog)
{
	LmMessage     *m;
	LmMessageNode *node;
	GError        *error = NULL;
	GossipJID     *jid;
	gchar         *str;
	
	str = g_strdup_printf ("<b>%s</b>", _("Requesting Personal Details, Please Wait..."));
	gtk_label_set_markup (GTK_LABEL (dialog->label_status), str);
	gtk_widget_show (dialog->label_status);
	g_free (str);

	jid = gossip_app_get_jid ();

	m = lm_message_new (gossip_jid_get_full (jid),
			    LM_MESSAGE_TYPE_IQ);

	node = lm_message_node_add_child (m->node, "vCard", NULL);
	lm_message_node_set_attribute (node, "xmlns", "vcard-temp");

	dialog->vcard_get_handler = lm_message_handler_new ((LmHandleMessageFunction) vcard_dialog_get_vcard_cb,
							    dialog, NULL);
					  
	if (!lm_connection_send_with_reply (dialog->connection, m,
					    dialog->vcard_get_handler, &error)) {
		d(g_print ("Error while sending: %s\n", error->message));

		lm_message_unref (m);
		lm_message_handler_unref (dialog->vcard_get_handler);

		dialog->vcard_get_handler = NULL;

		return;
	}

	lm_message_unref (m);
}

static LmHandlerResult
vcard_dialog_get_vcard_cb (LmMessageHandler  *handler,
			   LmConnection      *connection,
			   LmMessage         *m,
			   GossipVCardDialog *dialog)
{
	LmMessageNode *vCard, *node;

	d(g_print ("Got a vCard response\n"));

	lm_message_handler_unref (dialog->vcard_get_handler);
	dialog->vcard_get_handler = NULL;
	
	gtk_widget_hide (dialog->label_status);
	gtk_widget_set_sensitive (dialog->vbox_personal_information, TRUE);
	gtk_widget_set_sensitive (dialog->vbox_description, TRUE);

	vCard = lm_message_node_get_child (m->node, "vCard");
	if (!vCard) {
		d(g_print ("No vCard node\n"));
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

	node = lm_message_node_get_child (vCard, "FN");
	if (node) {
		gtk_entry_set_text (GTK_ENTRY (dialog->entry_name),
				    lm_message_node_get_value (node)); 
		
		d(g_print ("Found the 'FN' tag\n"));
	}

	node = lm_message_node_get_child (vCard, "NICKNAME");
	if (node) {
		gtk_entry_set_text (GTK_ENTRY (dialog->entry_nickname),
				    lm_message_node_get_value (node)); 
		
		d(g_print ("Found the 'NICKNAME' tag\n"));
	}

	node = lm_message_node_get_child (vCard, "EMAIL");
	if (node) {
		gtk_entry_set_text (GTK_ENTRY (dialog->entry_email),
				    lm_message_node_get_value (node)); 
		
		d(g_print ("Found the 'EMAIL' tag\n"));
	}

	node = lm_message_node_get_child (vCard, "URL");
	if (node) {
		gtk_entry_set_text (GTK_ENTRY (dialog->entry_web_site),
				    lm_message_node_get_value (node)); 
		
		d(g_print ("Found the 'URL' tag\n"));
	}

	node = lm_message_node_get_child (vCard, "DESC");
	if (node) {
		GtkTextBuffer *buffer;

		if (node->value) {
			buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->textview_description));
			gtk_text_buffer_set_text (buffer, node->value, -1);
		}
	}

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
vcard_dialog_set_vcard (GossipVCardDialog *dialog)
{
	LmMessage            *m;
	LmMessageNode        *node;
	GError               *error = NULL;

	G_CONST_RETURN gchar *name;
	G_CONST_RETURN gchar *nickname;
	G_CONST_RETURN gchar *web_site;
	G_CONST_RETURN gchar *email;
	gchar                *description;

	gchar                *str;

	GtkTextBuffer        *buffer;
	GtkTextIter           iter_begin, iter_end;

	if (!gossip_app_is_connected ()) {
		d(g_print ("Not connected, not setting vCard\n"));
		return;
	}

	name = gtk_entry_get_text (GTK_ENTRY (dialog->entry_name));
	nickname = gtk_entry_get_text (GTK_ENTRY (dialog->entry_nickname));
	web_site = gtk_entry_get_text (GTK_ENTRY (dialog->entry_web_site));
	email = gtk_entry_get_text (GTK_ENTRY (dialog->entry_email));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->textview_description));
	gtk_text_buffer_get_bounds (buffer, &iter_begin, &iter_end);
	description = gtk_text_buffer_get_text (buffer, &iter_begin, &iter_end, FALSE);

	str = g_strdup_printf ("<b>%s</b>", _("Saving Personal Details, Please Wait..."));
	gtk_label_set_markup (GTK_LABEL (dialog->label_status), str);
	gtk_widget_show (dialog->label_status);
	g_free (str);

	gtk_widget_set_sensitive (dialog->vbox_personal_information, FALSE);
	gtk_widget_set_sensitive (dialog->vbox_description, FALSE);

	m = lm_message_new_with_sub_type (NULL,
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_SET);

	node = lm_message_node_add_child (m->node, "vCard", NULL);
	lm_message_node_set_attribute (node, "xmlns", "vcard-temp");

	lm_message_node_add_child (node, "FN", name);
	lm_message_node_add_child (node, "NICKNAME", nickname);
	lm_message_node_add_child (node, "URL", web_site);
	lm_message_node_add_child (node, "EMAIL", email);
	lm_message_node_add_child (node, "DESC", description);

	g_free (description);

	dialog->vcard_set_handler = lm_message_handler_new ((LmHandleMessageFunction) vcard_dialog_set_vcard_cb,
							    dialog, NULL);
					  
	if (!lm_connection_send_with_reply (dialog->connection, m,
					    dialog->vcard_set_handler, &error)) {
		d(g_print ("Error while sending: %s\n", error->message));

		lm_message_unref (m);
		lm_message_handler_unref (dialog->vcard_set_handler);

		dialog->vcard_set_handler = NULL;

		return;
	}

	lm_message_unref (m);
}

static LmHandlerResult
vcard_dialog_set_vcard_cb (LmMessageHandler  *handler,
			   LmConnection      *connection,
			   LmMessage         *m,
			   GossipVCardDialog *dialog)
{
/* 	LmMessageNode *vCard, *node; */

	d(g_print ("Got a vCard response\n"));

	lm_message_handler_unref (dialog->vcard_set_handler);
	dialog->vcard_set_handler = NULL;

/* 	vCard = lm_message_node_get_child (m->node, "vCard"); */
/* 	if (!vCard) { */
/* 		d(g_print ("No vCard node\n")); */
/* 		return LM_HANDLER_RESULT_REMOVE_MESSAGE; */
/* 	} */

	/* !!! need to put some sort of error checking in here */

	gtk_widget_destroy (dialog->dialog);

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
vcard_dialog_response_cb (GtkDialog *widget, gint response, GossipVCardDialog *dialog)
{
	/* save vcard */
	if (response == GTK_RESPONSE_OK) {
		vcard_dialog_set_vcard (dialog);
		return;
	}

	gtk_widget_destroy (dialog->dialog);
}

static void
vcard_dialog_destroy_cb (GtkWidget *widget, GossipVCardDialog *dialog)
{
	if (dialog->vcard_get_handler) {
		lm_message_handler_invalidate (dialog->vcard_get_handler);
		lm_message_handler_unref (dialog->vcard_get_handler);
	}

	if (dialog->vcard_set_handler) {
		lm_message_handler_invalidate (dialog->vcard_set_handler);
		lm_message_handler_unref (dialog->vcard_set_handler);
	}

	g_free (dialog);
}
