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

#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-session.h>

#include "gossip-app.h"
#include "gossip-contact-info.h"

#define d(x) 

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CONTACT_INFO, GossipContactInfoPriv))

typedef struct _GossipContactInfoPriv GossipContactInfoPriv;
struct _GossipContactInfoPriv {
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

G_DEFINE_TYPE (GossipContactInfo, gossip_contact_info, G_TYPE_OBJECT);
static gpointer parent_class = NULL;

static void
gossip_contact_info_class_init (GossipContactInfoClass *class)
{
	parent_class = g_type_class_peek_parent (class);

	g_type_class_add_private (class, sizeof (GossipContactInfoPriv));
}

static void
gossip_contact_info_init (GossipContactInfo *info)
{
	/* Do nothing */
}

static void
contact_info_dialog_destroy_cb (GtkWidget *widget, GossipContactInfo *info)
{
	GossipContactInfoPriv *priv;

	priv = GET_PRIV (info);

	if (priv->presence_signal_handler) {
		g_signal_handler_disconnect (gossip_app_get_session (), 
					     priv->presence_signal_handler);
	}

	priv->dialog = NULL;

	g_object_unref (info);
}

static void
contact_info_dialog_close_cb (GtkWidget *widget, GossipContactInfo *info)
{
	GossipContactInfoPriv *priv;

	priv = GET_PRIV (info);
	gtk_widget_destroy (priv->dialog);
}

static void
contact_info_get_vcard_cb (GossipAsyncResult  result,
			   GossipVCard       *vcard,
			   GossipContactInfo *info)
{
	GtkTextBuffer *buffer;
	gboolean       show_personal = FALSE;
	const gchar   *str;
	GossipContactInfoPriv *priv;

	priv = GET_PRIV (info);
	
	if (result != GOSSIP_ASYNC_OK || !priv->dialog) {
		g_object_unref (info);
		return;
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description_textview));
	str = gossip_vcard_get_description (vcard);

	if (!str) {
		str = "";
	}
	
	gtk_text_buffer_set_text (buffer, str, -1);

	str = gossip_vcard_get_name (vcard);
	if (str && strcmp (str, "") != 0) {
		show_personal = TRUE;

		gtk_label_set_text (GTK_LABEL (priv->name_label), str);
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
		
		gtk_table_attach (GTK_TABLE (priv->personal_table),
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

		gtk_table_attach (GTK_TABLE (priv->personal_table),
				  alignment, 
				  1, 2,
				  2, 3,
				  GTK_FILL, GTK_FILL,
				  0, 0);
	}

	if (show_personal) {
		gtk_widget_hide (priv->personal_not_avail_label);
		gtk_widget_show_all (priv->personal_table);
	}
	
	g_object_unref (info);
}

static void
contact_info_get_version_cb (GossipAsyncResult  result,
			     GossipVersionInfo *version_info,
			     GossipContactInfo *info)
{
	const gchar           *str;
	gboolean               show_client_info = FALSE;
	GossipContactInfoPriv *priv;

	priv = GET_PRIV (info);
	
	if (result != GOSSIP_ASYNC_OK || !priv->dialog) {
		g_object_unref (info);
		return;
	}

	str = gossip_version_info_get_name (version_info);
	if (str && strcmp (str,  "") != 0) {
		show_client_info = TRUE;

		gtk_label_set_text (GTK_LABEL (priv->client_name_label), str);
	}

	str = gossip_version_info_get_version (version_info);
	if (str && strcmp (str, "") != 0) {
		show_client_info = TRUE;

		gtk_label_set_text (GTK_LABEL (priv->version_label), str);
	}

	str = gossip_version_info_get_os (version_info);
	if (str && strcmp (str, "") != 0) {
		show_client_info = TRUE;

		gtk_label_set_text (GTK_LABEL (priv->os_label), str);
	}

	if (show_client_info) {
		gtk_widget_hide (priv->client_not_avail_label);
	
		gtk_widget_show_all (priv->client_table);
	}

	g_object_unref (info);
}

static void
contact_info_resubscribe_cb (GtkWidget *widget, GossipContactInfo *info)
{
	/* FIXME (session): Readd */
#if 0
	LmMessage *m;
	GError *error = NULL;

	m = lm_message_new (gossip_jid_get_without_resource (priv->jid), 
			    LM_MESSAGE_TYPE_PRESENCE);
	lm_message_node_set_attribute (m->node, "type", "subscribe");

	if (!lm_connection_send (priv->connection, m, &error)) {
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
	GossipContactInfoPriv *priv;

	priv = GET_PRIV (info);

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (info != NULL);

	subscription = gossip_contact_get_subscription (contact);

	if (subscription == GOSSIP_SUBSCRIPTION_NONE ||
	    subscription == GOSSIP_SUBSCRIPTION_FROM) {
		gtk_widget_show_all (priv->subscription_box);
	
		g_signal_connect (priv->resubscribe_button,
				  "clicked",
				  G_CALLBACK (contact_info_resubscribe_cb),
				  info);
	} else {
		gtk_widget_hide (priv->subscription_box);
	}
}

static void
contact_info_contact_updated_cb (GossipSession     *session,
				 GossipContact     *contact,
				 GossipContactInfo *info)
{
	GossipContactInfoPriv *priv;

	priv = GET_PRIV (info);
	if (!gossip_contact_equal (contact, priv->contact)) {
		return;
	}
	
	contact_info_update_subscription_ui (info, contact);
}

GossipContactInfo *
gossip_contact_info_new (GossipContact *contact)
{
	GossipContactInfo     *info;
	GossipContactInfoPriv *priv;
	GladeXML              *gui;
	gchar                 *str, *tmp_str;
	GtkSizeGroup          *size_group;

	info = g_object_new (GOSSIP_TYPE_CONTACT_INFO, NULL);

	priv = GET_PRIV (info);
	
	priv->contact = contact;

	gui = gossip_glade_get_file (GLADEDIR "/main.glade",
				     "contact_information_dialog",
				     NULL,
				     "contact_information_dialog", &priv->dialog,
				     "title_label", &priv->title_label,
				     "jid_label", &priv->jid_label,
				     "personal_not_avail_label", &priv->personal_not_avail_label,
				     "personal_table", &priv->personal_table,
				     "name_label", &priv->name_label,
				     "client_not_avail_label", &priv->client_not_avail_label,
				     "client_table", &priv->client_table,
				     "client_name_label", &priv->client_name_label,
				     "version_label", &priv->version_label,
				     "os_label", &priv->os_label,
				     "close_button", &priv->close_button,
				     "description_textview", &priv->description_textview,
				     "subscription_box", &priv->subscription_box,
				     "subscription_label", &priv->subscription_label,
				     "resubscribe_button", &priv->resubscribe_button,
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
	
	g_signal_connect (priv->dialog,
			  "destroy",
			  G_CALLBACK (contact_info_dialog_destroy_cb),
			  info);

	tmp_str = g_strdup_printf (_("Contact Information for %s"), 
				   gossip_contact_get_name (contact));

	gtk_window_set_title (GTK_WINDOW (priv->dialog), tmp_str);

	str = g_markup_escape_text (tmp_str, -1);
	g_free (tmp_str);
	
	tmp_str = g_strdup_printf ("<b>%s</b>", str);
	g_free (str);
	gtk_label_set_markup (GTK_LABEL (priv->title_label), tmp_str);
	g_free (tmp_str);

	gtk_label_set_text (GTK_LABEL (priv->jid_label), 
			    gossip_contact_get_id (contact));
	
	contact_info_update_subscription_ui (info, contact);
		
	priv->presence_signal_handler = g_signal_connect (gossip_app_get_session (),
							  "contact-updated",
							  G_CALLBACK (contact_info_contact_updated_cb), 
							  info);

	g_signal_connect (priv->close_button,
			  "clicked",
			  G_CALLBACK (contact_info_dialog_close_cb),
			  info);

	gossip_session_async_get_vcard (gossip_app_get_session (),
					contact,
					(GossipAsyncVCardCallback) contact_info_get_vcard_cb,
					g_object_ref (info), NULL);

	gossip_session_async_get_version (gossip_app_get_session (),
					  contact,
					  (GossipAsyncVersionCallback) contact_info_get_version_cb,
					  g_object_ref (info), NULL);
	
	gtk_widget_show (priv->dialog);

	return info;
}

GtkWidget *
gossip_contact_info_get_dialog (GossipContactInfo *info)
{
	GossipContactInfoPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_INFO (info), NULL);
	
	priv = GET_PRIV (info);
	
	return priv->dialog;
}
