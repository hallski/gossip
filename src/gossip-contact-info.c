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
#include <glib/gi18n.h>
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
	GtkWidget     *id_label;
	GtkWidget     *name_label;
	GtkWidget     *client_label;
	GtkWidget     *version_label;
	GtkWidget     *os_label;
	GtkWidget     *personal_table;
	GtkWidget     *description_vbox;
	GtkWidget     *description_textview;
	GtkWidget     *client_not_avail_label;
	GtkWidget     *client_table;
	GtkWidget     *stub_id_label;
	GtkWidget     *stub_name_label;
	GtkWidget     *stub_email_label;
	GtkWidget     *stub_web_label;
	GtkWidget     *stub_client_label;
	GtkWidget     *stub_version_label;
	GtkWidget     *stub_os_label;
	GtkWidget     *subscription_hbox;
	GtkWidget     *subscription_label;
	GtkWidget     *subscribe_button;

	GtkWidget     *personal_status_label;
	GtkWidget     *personal_status_hbox;
	GtkWidget     *client_status_label;
	GtkWidget     *client_status_hbox;
	GtkWidget     *client_vbox;

	gulong         presence_signal_handler;
};


static void contact_info_dialog_destroy_cb      (GtkWidget         *widget,
						 GossipContactInfo *info);
static void contact_info_dialog_response_cb     (GtkWidget         *widget,
						 gint               response,
						 GossipContactInfo *info);
static void contact_info_get_vcard_cb           (GossipAsyncResult  result,
						 GossipVCard       *vcard,
						 GossipContactInfo *info);
static void contact_info_get_version_cb         (GossipAsyncResult  result,
						 GossipVersionInfo *version_info,
						 GossipContactInfo *info);
static void contact_info_subscribe_cb           (GtkWidget         *widget,
						 GossipContactInfo *info);
static void contact_info_update_subscription_ui (GossipContactInfo *info,
						 GossipContact     *contact);
static void contact_info_contact_updated_cb     (GossipSession     *session,
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
contact_info_dialog_destroy_cb (GtkWidget         *widget,
				GossipContactInfo *info)
{
	GossipContactInfoPriv *priv;

	priv = GET_PRIV (info);

	if (priv->presence_signal_handler) {
		g_signal_handler_disconnect (gossip_app_get_session (), 
					     priv->presence_signal_handler);
	}

	g_object_unref (priv->contact);

	priv->dialog = NULL;

	g_object_unref (info);
}

static void
contact_info_dialog_response_cb (GtkWidget         *widget, 
				 gint               response,
				 GossipContactInfo *info)
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
	GossipContactInfoPriv *priv;
	GtkTextBuffer         *buffer;
	gboolean               show_personal = FALSE;
	const gchar           *str;
	
	priv = GET_PRIV (info);

	if (result != GOSSIP_ASYNC_OK || !priv->dialog) {
/* 		gchar *status; */

/* 		status = g_strdup_printf ("<i>%s</i>",  */
/* 				       _("Information Not Available")); */
/* 		gtk_label_set_markup (GTK_LABEL (priv->personal_status_label), status); */
/* 		g_free (status); */
		
		if (priv->dialog) {
			gtk_widget_hide (priv->personal_status_hbox);
		}

		g_object_unref (info);
		return;
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description_textview));
	str = gossip_vcard_get_description (vcard);

	if (str && strlen (str) > 0) {
		gtk_text_buffer_set_text (buffer, str, -1);
		gtk_widget_show (priv->description_vbox);
	} else {
		gtk_widget_hide (priv->description_vbox);
	}
	
	str = gossip_vcard_get_name (vcard);
	if (str && strlen (str) > 0) {
		gtk_label_set_text (GTK_LABEL (priv->name_label), str);

		gtk_label_set_text (GTK_LABEL (priv->stub_name_label),
				    _("Name:"));
	} else {
		gtk_label_set_text (GTK_LABEL (priv->stub_name_label),
				    _("Alias:"));
	}
	
	str = gossip_vcard_get_email (vcard);
	if (str && strlen (str) > 0) {
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
				  0, 1,
				  GTK_FILL, GTK_FILL,
				  0, 0);

		g_free (link);

		gtk_widget_show_all (alignment);
		gtk_widget_show (priv->stub_email_label);
	} else {
		gtk_widget_hide (priv->stub_email_label);
	}

	str = gossip_vcard_get_url (vcard);
	if (str && strlen (str) > 0) {
		GtkWidget *href, *alignment;

		show_personal = TRUE;

		href = gnome_href_new (str, str);

		alignment = gtk_alignment_new (0, 1, 0, 0.5);
		gtk_container_add (GTK_CONTAINER (alignment), href);

		gtk_table_attach (GTK_TABLE (priv->personal_table),
				  alignment, 
				  1, 2,
				  1, 2,
				  GTK_FILL, GTK_FILL,
				  0, 0);

		gtk_widget_show_all (alignment);
		gtk_widget_show (priv->stub_web_label);
	} else {
		gtk_widget_hide (priv->stub_web_label);
	}

	if (show_personal) {
		GtkSizeGroup *size_group;

		gtk_widget_hide (priv->personal_status_hbox);
		gtk_widget_show (priv->personal_table);

		size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
		gtk_size_group_add_widget (size_group, priv->stub_id_label);
		gtk_size_group_add_widget (size_group, priv->stub_email_label);
		g_object_unref (size_group);
	}
	
	g_object_unref (info);
}

static void
contact_info_get_version_cb (GossipAsyncResult  result,
			     GossipVersionInfo *version_info,
			     GossipContactInfo *info)
{
	GossipContactInfoPriv *priv;
	gboolean               show_client_info = FALSE;
	const gchar           *str;

	priv = GET_PRIV (info);

	if (result != GOSSIP_ASYNC_OK || !priv->dialog) {
/* 		gchar *status; */

/* 		status = g_strdup_printf ("<i>%s</i>",  */
/* 				       _("Information Not Available")); */
/* 		gtk_label_set_markup (GTK_LABEL (priv->client_status_label), status); */
/* 		g_free (status); */

		if (priv->dialog) {
			gtk_widget_hide (priv->client_status_hbox);
			gtk_widget_hide (priv->client_vbox);
		}

		g_object_unref (info);
		return;
	}

	str = gossip_version_info_get_name (version_info);
	if (str && strcmp (str,  "") != 0) {
		show_client_info = TRUE;

		gtk_label_set_text (GTK_LABEL (priv->client_label), str);
		gtk_widget_show (priv->stub_client_label);
	} else {
		gtk_widget_hide (priv->stub_client_label);
	}

	str = gossip_version_info_get_version (version_info);
	if (str && strcmp (str, "") != 0) {
		show_client_info = TRUE;

		gtk_label_set_text (GTK_LABEL (priv->version_label), str);
		gtk_widget_show (priv->stub_version_label);
	} else {
		gtk_widget_hide (priv->stub_version_label);
	}

	str = gossip_version_info_get_os (version_info);
	if (str && strcmp (str, "") != 0) {
		show_client_info = TRUE;

		gtk_label_set_text (GTK_LABEL (priv->os_label), str);
		gtk_widget_show (priv->stub_os_label);
	} else {
		gtk_widget_hide (priv->stub_os_label);
	}

	if (show_client_info) {
		GtkSizeGroup *size_group;

		gtk_widget_hide (priv->client_status_hbox);
		gtk_widget_show (priv->client_table);

		size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
		gtk_size_group_add_widget (size_group, priv->stub_id_label);
		gtk_size_group_add_widget (size_group, priv->stub_client_label);
		g_object_unref (size_group);
	} 

	g_object_unref (info);
}

static void
contact_info_subscribe_cb (GtkWidget         *widget, 
			   GossipContactInfo *info)
{
	GossipContactInfoPriv *priv;
	const gchar           *message;

	g_return_if_fail (info != NULL);

	priv = GET_PRIV (info);

	message = _("I would like to add you to my contact list.");

        gossip_session_add_contact (gossip_app_get_session (),
                                    gossip_contact_get_id (priv->contact), 
				    gossip_contact_get_name (priv->contact),
				    NULL, /* group */
				    message);
}

static void
contact_info_update_subscription_ui (GossipContactInfo *info,
				     GossipContact     *contact)
{
	GossipSubscription subscription;
	GossipContactInfoPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (info != NULL);

	priv = GET_PRIV (info);

	subscription = gossip_contact_get_subscription (contact);

	if (subscription == GOSSIP_SUBSCRIPTION_NONE ||
	    subscription == GOSSIP_SUBSCRIPTION_FROM) {
		gtk_widget_show_all (priv->subscription_hbox);
	
		g_signal_connect (priv->subscribe_button,
				  "clicked",
				  G_CALLBACK (contact_info_subscribe_cb),
				  info);
	} else {
		gtk_widget_hide (priv->subscription_hbox);
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
	
	priv->contact = g_object_ref (contact);

	gui = gossip_glade_get_file (GLADEDIR "/main.glade",
				     "contact_information_dialog",
				     NULL,
				     "contact_information_dialog", &priv->dialog,
				     "id_label", &priv->id_label,
				     "name_label", &priv->name_label,
				     "client_label", &priv->client_label,
				     "version_label", &priv->version_label,
				     "os_label", &priv->os_label,
				     "personal_table", &priv->personal_table,
				     "description_vbox", &priv->description_vbox,
				     "description_textview", &priv->description_textview,
				     "client_table", &priv->client_table,
				     "stub_id_label", &priv->stub_id_label,
				     "stub_name_label", &priv->stub_name_label,
				     "stub_email_label", &priv->stub_email_label,
				     "stub_web_label", &priv->stub_web_label,
				     "stub_client_label", &priv->stub_client_label,
				     "stub_version_label", &priv->stub_version_label,
				     "stub_os_label", &priv->stub_os_label,
				     "personal_status_label", &priv->personal_status_label,
				     "personal_status_hbox", &priv->personal_status_hbox,
				     "client_status_label", &priv->client_status_label,
				     "client_status_hbox", &priv->client_status_hbox,
				     "client_vbox", &priv->client_vbox,
				     "subscription_hbox", &priv->subscription_hbox,
				     "subscription_label", &priv->subscription_label,
				     "subscribe_button", &priv->subscribe_button,
				     NULL);

	g_signal_connect (priv->dialog,
			  "destroy",
			  G_CALLBACK (contact_info_dialog_destroy_cb),
			  info);

	g_signal_connect (priv->dialog,
			  "response",
			  G_CALLBACK (contact_info_dialog_response_cb),
			  info);

	/* A bit ugly, but the result is nice. Align the labels in the
	   two different tables. */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, priv->stub_name_label);
	gtk_size_group_add_widget (size_group, priv->stub_email_label);
	gtk_size_group_add_widget (size_group, priv->stub_web_label);
	gtk_size_group_add_widget (size_group, priv->stub_client_label);
	gtk_size_group_add_widget (size_group, priv->stub_version_label);
	gtk_size_group_add_widget (size_group, priv->stub_os_label);

	g_object_unref (size_group);
	
	/* set labels */
	tmp_str = g_strdup_printf (_("Contact Information for %s"), 
				   gossip_contact_get_name (contact));

	str = g_markup_escape_text (tmp_str, -1);
	g_free (tmp_str);
	
	gtk_label_set_text (GTK_LABEL (priv->id_label), 
			    gossip_contact_get_id (contact));

	gtk_label_set_text (GTK_LABEL (priv->stub_name_label),
			    _("Alias:"));
	gtk_label_set_text (GTK_LABEL (priv->name_label), 
			    gossip_contact_get_name (contact));

	/* subscription listener */
	contact_info_update_subscription_ui (info, contact);
		
	priv->presence_signal_handler = g_signal_connect (gossip_app_get_session (),
							  "contact-updated",
							  G_CALLBACK (contact_info_contact_updated_cb), 
							  info);

	/* get vcard and version info */
	str = g_strdup_printf ("<i>%s</i>", 
			       _("Requested Information"));
	gtk_label_set_markup (GTK_LABEL (priv->personal_status_label), str);
	gtk_label_set_markup (GTK_LABEL (priv->client_status_label), str);
	g_free (str);

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
