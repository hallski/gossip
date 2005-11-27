/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Imendio AB
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

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libgnomeui/libgnomeui.h>

#include <libgossip/gossip-protocol.h>
#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-event-manager.h>
#include <libgossip/gossip-vcard.h>
#include <libgossip/gossip-utils.h>

#include "gossip-subscription-dialog.h"
#include "gossip-app.h"


typedef struct {
	GossipProtocol *protocol;
	GossipContact  *contact;
	GossipVCard    *vcard;
} SubscriptionData;


static void subscription_dialog_protocol_connected_cb    (GossipSession      *session,
							  GossipAccount      *account,
							  GossipProtocol     *protocol, 
							  gpointer            user_data);
static void subscription_dialog_protocol_disconnected_cb (GossipSession      *session,
							  GossipAccount      *account,
							  GossipProtocol     *protocol,
							  gpointer            user_data);
static void subscription_dialog_request_cb               (GossipProtocol     *protocol,
							  GossipContact      *contact,
							  gpointer            user_data);
static void subscription_dialog_event_activated_cb       (GossipEventManager *event_manager,
							  GossipEvent        *event,
							  GossipProtocol     *protocol);
static void subscription_dialog_vcard_cb                 (GossipResult        result,
							  GossipVCard        *vcard,
							  SubscriptionData   *data);
static void subscription_dialog_request_dialog_cb        (GtkWidget          *dialog,
							  gint                response,
							  SubscriptionData   *data);


void
gossip_subscription_dialog_init (GossipSession *session)
{
	g_signal_connect (session, 
			  "protocol-connected",
			  G_CALLBACK (subscription_dialog_protocol_connected_cb),
			  NULL);

	g_signal_connect (session, 
			  "protocol-disconnected",
			  G_CALLBACK (subscription_dialog_protocol_disconnected_cb),
			  NULL);
}

void
gossip_subscription_dialog_finalize (GossipSession *session)
{
 	g_signal_handlers_disconnect_by_func (session, 
					      subscription_dialog_protocol_connected_cb, 
					      NULL);
 	g_signal_handlers_disconnect_by_func (session, 
					      subscription_dialog_protocol_disconnected_cb, 
					      NULL);
}

static void
subscription_dialog_protocol_connected_cb (GossipSession  *session,
					   GossipAccount  *account,
					   GossipProtocol *protocol,
					   gpointer        user_data)
{
	g_signal_connect (protocol,
                          "subscription-request",
                          G_CALLBACK (subscription_dialog_request_cb),
                          session);
}

static void
subscription_dialog_protocol_disconnected_cb (GossipSession  *session,
					      GossipAccount  *account,
					      GossipProtocol *protocol,
					      gpointer        user_data)
{
 	g_signal_handlers_disconnect_by_func (protocol, 
					      subscription_dialog_request_cb, 
					      session);
}

static void
subscription_dialog_request_cb (GossipProtocol *protocol,
				GossipContact  *contact,
				gpointer        user_data)
{
	GossipEvent      *event;
	gchar            *str;

	event = gossip_event_new (GOSSIP_EVENT_SUBSCRIPTION_REQUEST);

	str = g_strdup_printf (_("New subscription request from %s"), 
			       gossip_contact_get_name (contact));

	g_object_set (event, 
		      "message", str, 
		      "data", contact,
		      NULL);
	g_free (str);

	gossip_event_manager_add (gossip_app_get_event_manager (),
				  event, 
				  (GossipEventActivatedFunction)subscription_dialog_event_activated_cb,
				  G_OBJECT (protocol));
}

static void
subscription_dialog_event_activated_cb (GossipEventManager *event_manager,
					GossipEvent        *event,
					GossipProtocol     *protocol)
{
	GossipContact    *contact;
	SubscriptionData *data;

	contact = GOSSIP_CONTACT (gossip_event_get_data (event));

	data = g_new0 (SubscriptionData, 1);

	data->protocol = g_object_ref (protocol);
	data->contact = g_object_ref (contact);

	gossip_session_get_vcard (gossip_app_get_session (),
				  NULL,
				  data->contact,
				  (GossipVCardCallback) subscription_dialog_vcard_cb,
				  data, 
				  NULL);
}

static void
subscription_dialog_vcard_cb (GossipResult      result,
			      GossipVCard       *vcard,
			      SubscriptionData  *data)
{
	GtkWidget   *dialog;
	GtkWidget   *who_label;
	GtkWidget   *question_label;
	GtkWidget   *id_label;
 	GtkWidget   *website_label;
 	GtkWidget   *personal_table;
	const gchar *name = NULL;
	const gchar *url = NULL;
	gchar       *who;
	gchar       *question;
	gchar       *str;
	gint         num_matches = 0;

	if (GOSSIP_IS_VCARD (vcard)) {
		data->vcard = g_object_ref (vcard);

		name = gossip_vcard_get_name (vcard);
		url = gossip_vcard_get_url (vcard);
	}

	gossip_glade_get_file_simple (GLADEDIR "/main.glade",
				      "subscription_request_dialog",
				      NULL,
				      "subscription_request_dialog", &dialog,
				      "who_label", &who_label,
				      "question_label", &question_label,
				      "id_label", &id_label,
				      "website_label", &website_label,
				      "personal_table", &personal_table,
				      NULL);

	if (name) {
		who = g_strdup_printf (_("%s wants to be added to your contact list."), 
				       name);
		question = g_strdup_printf (_("Do you want to add %s to your contact list?"),
					    name);
	} else {
		who = g_strdup (_("Someone wants to be added to your contact list."));
		question = g_strdup (_("Do you want to add this person to your contact list?"));
	}

	str = g_strdup_printf ("<span weight='bold' size='larger'>%s</span>", who);
	gtk_label_set_markup (GTK_LABEL (who_label), str);
	gtk_label_set_use_markup (GTK_LABEL (who_label), TRUE);
	g_free (str);
	g_free (who);

	gtk_label_set_text (GTK_LABEL (question_label), question);
	g_free (question);

	gtk_label_set_text (GTK_LABEL (id_label), gossip_contact_get_id (data->contact));

	if (url && strlen (url) > 0) {
		GArray *start, *end;

		start = g_array_new (FALSE, FALSE, sizeof (gint));
		end = g_array_new (FALSE, FALSE, sizeof (gint));
		
		num_matches = gossip_utils_url_regex_match (url, start, end);
	}

	if (num_matches > 0) {
		GtkWidget *href;
		GtkWidget *alignment;

		href = gnome_href_new (url, url);

		alignment = gtk_alignment_new (0, 1, 0, 0.5);
		gtk_container_add (GTK_CONTAINER (alignment), href);

		gtk_table_attach (GTK_TABLE (personal_table),
				  alignment, 
				  1, 2,
				  1, 2,
				  GTK_FILL, GTK_FILL,
				  0, 0);
		
		gtk_widget_show_all (personal_table);
	} else {
		gtk_widget_hide (website_label);
	}

	g_signal_connect (dialog,
			  "response",
			  G_CALLBACK (subscription_dialog_request_dialog_cb),
			  data);

	gtk_widget_show (dialog);
}

static void
subscription_dialog_request_dialog_cb (GtkWidget        *dialog,
				       gint              response,
				       SubscriptionData *data)
{
	gboolean add_user;

	g_return_if_fail (GTK_IS_DIALOG (dialog));

	g_return_if_fail (GOSSIP_IS_PROTOCOL (data->protocol));
	g_return_if_fail (GOSSIP_IS_CONTACT (data->contact));

	add_user = (gossip_contact_get_type (data->contact) == GOSSIP_CONTACT_TYPE_TEMPORARY);

	gtk_widget_destroy (dialog);
	
	if (response == GTK_RESPONSE_YES ||
	    response == GTK_RESPONSE_NO) {
		gboolean subscribe;

		subscribe = (response == GTK_RESPONSE_YES);
		gossip_protocol_set_subscription (data->protocol, 
						  data->contact, 
						  subscribe);

		if (subscribe && add_user) {
			const gchar *id, *name, *message;
			
			id = gossip_contact_get_id (data->contact);
			if (data->vcard) {
				name = gossip_vcard_get_name (data->vcard);
			} else {
				name = id;
			}
			
			message = _("I would like to add you to my contact list.");
					
			/* FIXME: how is session related to an IM account? */
			/* micke, hmm .. this feels a bit wrong, should be
			 * signalled from the protocol when we do 
			 * set_subscribed
			 */
			gossip_protocol_add_contact (data->protocol,
						     id, name, NULL,
						     message);
		}
	}
	
	g_object_unref (data->protocol);
	g_object_unref (data->contact);

	if (data->vcard) {
		g_object_unref (data->vcard);
	}
	
	g_free (data);
}
