/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2004 Imendio AB
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

#include "gossip-app.h"
#include "gossip-contact.h"
#include "gossip-session.h"
#include "gossip-utils.h"
#include "gossip-contact-info.h"

#define d(x) 

struct _GossipContactInfo {
	GossipContact *contact;

	GtkWidget     *dialog;
	GtkWidget     *title_label;
	GtkWidget     *jid_label;
	GtkWidget     *personal_not_avail_label;
	GtkWidget     *personal_table;
	GtkWidget     *name_label;
	GtkWidget     *client_not_avail_label;
	GtkWidget     *client_table;
	GtkWidget     *client_name_label;
	GtkWidget     *version_label;
	GtkWidget     *os_label;
	GtkWidget     *description_textview;
	GtkWidget     *subscription_box;
	GtkWidget     *subscription_label;
	GtkWidget     *resubscribe_button;
	GtkWidget     *close_button;

	gulong         presence_signal_handler;
};

static void contact_info_dialog_destroy_cb   (GtkWidget         *widget,
					      GossipContactInfo *info);
static void contact_info_dialog_close_cb     (GtkWidget         *widget, 
					      GossipContactInfo *info);
static void contact_info_get_vcard_cb        (GossipAsyncResult  result,
					      GossipVCard       *vcard,
					      GossipContactInfo *info);
static void contact_info_get_version_cb      (GossipAsyncResult  result,
					      GossipVersionInfo *version_info,
					      GossipContactInfo *info);
static void contact_info_resubscribe_cb      (GtkWidget         *widget,
					      GossipContactInfo *info);
static void 
contact_info_update_subscription_ui          (GossipContactInfo *info,
					      GossipContact     *contact);
static void contact_info_contact_updated_cb  (GossipSession     *session,
					      GossipContact     *contact,
					      GossipContactInfo *info);

static void
contact_info_dialog_destroy_cb (GtkWidget *widget, GossipContactInfo *info)
{
	if (info->presence_signal_handler) {
		g_signal_handler_disconnect (gossip_app_get_session (), 
					     info->presence_signal_handler);
	}

	g_free (info);
}

static void
contact_info_dialog_close_cb (GtkWidget *widget, GossipContactInfo *info)
{
	gtk_widget_destroy (info->dialog);
}

static void
contact_info_get_vcard_cb (GossipAsyncResult  result,
			   GossipVCard       *vcard,
			   GossipContactInfo *info)
{
	GtkTextBuffer *buffer;
	gboolean       show_personal = FALSE;
	const gchar   *str;

	g_print ("vcard callback ()\n");
	
	if (result != GOSSIP_ASYNC_OK) {
		return;
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (info->description_textview));
	str = gossip_vcard_get_description (vcard);

	if (!str) {
		str = "";
	}
	
	gtk_text_buffer_set_text (buffer, str, -1);

	str = gossip_vcard_get_name (vcard);
	if (str && strcmp (str, "") != 0) {
		show_personal = TRUE;

		gtk_label_set_text (GTK_LABEL (info->name_label), str);
	}
	
	str = gossip_vcard_get_email (vcard);
	if (str && strcmp (str, "") != 0) {
		GtkWidget *href, *alignment;
		gchar     *link;

		show_personal = TRUE;

		link = g_strdup_printf ("mailto:%s", str);
		
		href = gnome_href_new (link, str);

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

	str = gossip_vcard_get_url (vcard);
	if (str && strcmp (str, "") != 0) {
		GtkWidget *href, *alignment;

		show_personal = TRUE;

		href = gnome_href_new (str, str);

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
}

static void
contact_info_get_version_cb (GossipAsyncResult  result,
			     GossipVersionInfo *version_info,
			     GossipContactInfo *info)
{
	const gchar *str;
	gboolean     show_client_info = FALSE;

	g_print ("version callback ()\n");

	if (result != GOSSIP_ASYNC_OK) {
		return;
	}

	str = gossip_version_info_get_name (version_info);
	if (str && strcmp (str,  "") != 0) {
		show_client_info = TRUE;

		gtk_label_set_text (GTK_LABEL (info->client_name_label), str);
	}

	str = gossip_version_info_get_version (version_info);
	if (str && strcmp (str, "") != 0) {
		show_client_info = TRUE;

		gtk_label_set_text (GTK_LABEL (info->version_label), str);
	}

	str = gossip_version_info_get_os (version_info);
	if (str && strcmp (str, "") != 0) {
		show_client_info = TRUE;

		gtk_label_set_text (GTK_LABEL (info->os_label), str);
	}

	if (show_client_info) {
		gtk_widget_hide (info->client_not_avail_label);
	
		gtk_widget_show_all (info->client_table);
	}
}

static void
contact_info_resubscribe_cb (GtkWidget *widget, GossipContactInfo *info)
{
	/* FIXME (session): Readd */
#if 0
	LmMessage *m;
	GError *error = NULL;

	m = lm_message_new (gossip_jid_get_without_resource (info->jid), 
			    LM_MESSAGE_TYPE_PRESENCE);
	lm_message_node_set_attribute (m->node, "type", "subscribe");

	if (!lm_connection_send (info->connection, m, &error)) {
		d(g_print ("Error while sending: %s\n", error->message));
		lm_message_unref (m);
		return;
	}

	lm_message_unref (m);
#endif
}

static void
contact_info_update_subscription_ui (GossipContactInfo *info,
				     GossipContact     *contact)
{
	GossipSubscription subscription;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (info != NULL);

	subscription = gossip_contact_get_subscription (contact);

	if (subscription == GOSSIP_SUBSCRIPTION_NONE ||
	    subscription == GOSSIP_SUBSCRIPTION_FROM) {
		gtk_widget_show_all (info->subscription_box);
	
		g_signal_connect (info->resubscribe_button,
				  "clicked",
				  G_CALLBACK (contact_info_resubscribe_cb),
				  info);
	} else {
		gtk_widget_hide (info->subscription_box);
	}
}

static void
contact_info_contact_updated_cb (GossipSession     *session,
				 GossipContact     *contact,
				 GossipContactInfo *info)
{
	if (!gossip_contact_equal (contact, info->contact)) {
		return;
	}
	
	contact_info_update_subscription_ui (info, contact);
}

GossipContactInfo *
gossip_contact_info_new (GossipContact *contact)
{
	GossipContactInfo *info;
	GladeXML          *gui;
	gchar             *str, *tmp_str;
	GtkSizeGroup      *size_group;

	info = g_new0 (GossipContactInfo, 1);

	info->contact = contact;

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
				     "subscription_box", &info->subscription_box,
				     "subscription_label", &info->subscription_label,
				     "resubscribe_button", &info->resubscribe_button,
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

	tmp_str = g_strdup_printf (_("Contact Information for %s"), 
				   gossip_contact_get_name (contact));

	gtk_window_set_title (GTK_WINDOW (info->dialog), tmp_str);

	str = g_markup_escape_text (tmp_str, -1);
	g_free (tmp_str);
	
	tmp_str = g_strdup_printf ("<b>%s</b>", str);
	g_free (str);
	gtk_label_set_markup (GTK_LABEL (info->title_label), tmp_str);
	g_free (tmp_str);

	gtk_label_set_text (GTK_LABEL (info->jid_label), 
			    gossip_contact_get_id (contact));
	
	contact_info_update_subscription_ui (info, contact);
		
	info->presence_signal_handler = g_signal_connect (gossip_app_get_session (),
							  "contact-updated",
							  G_CALLBACK (contact_info_contact_updated_cb), 
							  info);

	g_signal_connect (info->close_button,
			  "clicked",
			  G_CALLBACK (contact_info_dialog_close_cb),
			  info);

	gossip_session_async_get_vcard (gossip_app_get_session (),
					contact,
					(GossipAsyncVCardCallback) contact_info_get_vcard_cb,
					info, NULL);

	gossip_session_async_get_version (gossip_app_get_session (),
					  contact,
					  (GossipAsyncVersionCallback) contact_info_get_version_cb,
					  info, NULL);

	gtk_widget_show (info->dialog);

	return info;
}

GtkWidget *
gossip_contact_info_get_dialog (GossipContactInfo *info)
{
	return info->dialog;
}
