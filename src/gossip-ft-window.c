/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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
 *
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#ifdef HAVE_GNOME
#include <libgnomevfs/gnome-vfs.h>
#endif

#include <libgossip/gossip.h>

#include "gossip-ft-window.h"
#include "gossip-ui-utils.h"
#include "gossip-app.h"

#define DEBUG_DOMAIN "FileTransferWindow"

typedef struct {
	GossipProtocol *protocol;
	GossipFT       *ft;
	GossipVCard    *vcard;
} FTData;

#ifdef HAVE_GNOME

static void ft_window_protocol_connected_cb    (GossipSession      *session,
						GossipAccount      *account,
						GossipProtocol     *protocol,
						gpointer            user_data);
static void ft_window_protocol_disconnected_cb (GossipSession      *session,
						GossipAccount      *account,
						GossipProtocol     *protocol,
						gint                reason,
						gpointer            user_data);
static void ft_window_request_cb               (GossipProtocol     *protocol,
						GossipFT           *ft,
						gpointer            user_data);
static void ft_window_error_cb                 (GossipProtocol     *protocol,
						GossipFT           *ft,
						GError             *error,
						gpointer            user_data);
static void ft_window_event_activated_cb       (GossipEventManager *event_manager,
						GossipEvent        *event,
						GossipProtocol     *protocol);
static void ft_window_vcard_cb                 (GossipResult        result,
						GossipVCard        *vcard,
						FTData             *data);
static void ft_window_request_dialog_cb        (GtkWidget          *dialog,
						gint                response,
						FTData             *data);
static void ft_window_filechooser_create       (GossipContact      *contact);
static void ft_window_filechooser_response_cb  (GtkDialog          *dialog,
						gint                response_id,
						GossipContact      *contact);

void
gossip_ft_window_init (GossipSession *session)
{
	g_object_ref (session);

	g_signal_connect (session,
			  "protocol-connected",
			  G_CALLBACK (ft_window_protocol_connected_cb),
			  NULL);

	g_signal_connect (session,
			  "protocol-disconnected",
			  G_CALLBACK (ft_window_protocol_disconnected_cb),
			  NULL);
}

void
gossip_ft_window_finalize (GossipSession *session)
{
	g_signal_handlers_disconnect_by_func (session,
					      ft_window_protocol_connected_cb,
					      NULL);
	g_signal_handlers_disconnect_by_func (session,
					      ft_window_protocol_disconnected_cb,
					      NULL);

	g_object_unref (session);
}

/*
 * receiving from another contact
 */

static void
ft_window_protocol_connected_cb (GossipSession  *session,
				 GossipAccount  *account,
				 GossipProtocol *protocol,
				 gpointer        user_data)
{
	g_signal_connect (protocol,
			  "file-transfer-request",
			  G_CALLBACK (ft_window_request_cb),
			  session);

	g_signal_connect (protocol,
			  "file-transfer-error",
			  G_CALLBACK (ft_window_error_cb),
			  session);
}

static void
ft_window_protocol_disconnected_cb (GossipSession  *session,
				    GossipAccount  *account,
				    GossipProtocol *protocol,
				    gint            reason,
				    gpointer        user_data)
{
	g_signal_handlers_disconnect_by_func (protocol,
					      ft_window_request_cb,
					      session);
	g_signal_handlers_disconnect_by_func (protocol,
					      ft_window_error_cb,
					      session);
}

static void
ft_window_request_cb (GossipProtocol *protocol,
		      GossipFT       *ft,
		      gpointer        user_data)
{
	GossipEvent   *event;
	GossipContact *contact;
	gchar         *str;

	event = gossip_event_new (GOSSIP_EVENT_FILE_TRANSFER_REQUEST);
	contact = gossip_ft_get_contact (ft);

	str = g_strdup_printf (_("New file transfer request from %s"),
			       gossip_contact_get_name (contact));

	g_object_set (event,
		      "message", str,
		      "data", ft,
		      NULL);
	g_free (str);

	gossip_event_manager_add (gossip_app_get_event_manager (),
				  event,
				  (GossipEventActivateFunction) ft_window_event_activated_cb,
				  G_OBJECT (protocol));
}

static void
ft_window_error_cb (GossipProtocol *protocol,
		    GossipFT       *ft,
		    GError         *error,
		    gpointer        user_data)
{
	GtkWidget      *dialog;
	GtkMessageType  type;
	const gchar    *str1, *str2;

	type = GTK_MESSAGE_ERROR;

	str2 = error->message;

	switch (error->code) {
	case GOSSIP_FT_ERROR_UNSUPPORTED:
		type = GTK_MESSAGE_INFO;
		str1 = _("File transfer is not supported by both parties.");
		break;
	case GOSSIP_FT_ERROR_DECLINED:
		type = GTK_MESSAGE_INFO;
		str1 = _("Your file transfer offer declined.");
		str2 = _("The other user decided not to continue.");
		break;

	case GOSSIP_FT_ERROR_UNKNOWN:
	default:
		str1 = _("Unknown error occurred during file transfer.");
		break;
	}

	dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (gossip_app_get_window ()),
						     GTK_DIALOG_DESTROY_WITH_PARENT,
						     type,
						     GTK_BUTTONS_CLOSE,
						     "<b>%s</b>\n\n%s",
						     str1,
						     str2);

	g_signal_connect_swapped (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  dialog);

	gtk_widget_show (dialog);
}

static void
ft_window_event_activated_cb (GossipEventManager *event_manager,
			      GossipEvent        *event,
			      GossipProtocol     *protocol)
{
	GossipFT *ft;
	FTData   *data;

	ft = GOSSIP_FT (gossip_event_get_data (event));

	data = g_new0 (FTData, 1);

	data->protocol = g_object_ref (protocol);
	data->ft = g_object_ref (ft);

	gossip_session_get_vcard (gossip_app_get_session (),
				  NULL,
				  gossip_ft_get_contact (ft),
				  (GossipVCardCallback) ft_window_vcard_cb,
				  data,
				  NULL);
}

static void
ft_window_vcard_cb (GossipResult  result,
		    GossipVCard  *vcard,
		    FTData       *data)
{
	GossipContact *contact;
	GtkWidget     *dialog;
	GtkWidget     *who_label;
	GtkWidget     *id_label;
	GtkWidget     *website_label;
	GtkWidget     *personal_table;
	GtkWidget     *file_name_label;
	GtkWidget     *file_size_label;
	const gchar   *name = NULL;
	const gchar   *url = NULL;
	gchar         *who;
	gchar         *str;
	gint           num_matches = 0;
	gchar         *file_size;

	if (GOSSIP_IS_VCARD (vcard)) {
		data->vcard = g_object_ref (vcard);

		name = gossip_vcard_get_name (vcard);
		url = gossip_vcard_get_url (vcard);
	}

	contact = gossip_ft_get_contact (data->ft);

	gossip_glade_get_file_simple ("file-transfer.glade",
				      "file_transfer_dialog",
				      NULL,
				      "file_transfer_dialog", &dialog,
				      "who_label", &who_label,
				      "id_label", &id_label,
				      "website_label", &website_label,
				      "personal_table", &personal_table,
				      "file_name_label", &file_name_label,
				      "file_size_label", &file_size_label,
				      NULL);

	if (name) {
		who = g_strdup_printf (_("%s would like to send you a file."),
				       name);
	} else {
		who = g_strdup (_("Someone would like to send you a file."));
	}

	str = g_strdup_printf ("<span weight='bold' size='larger'>%s</span>", who);
	gtk_label_set_markup (GTK_LABEL (who_label), str);
	gtk_label_set_use_markup (GTK_LABEL (who_label), TRUE);
	g_free (str);
	g_free (who);

	gtk_label_set_text (GTK_LABEL (file_name_label), gossip_ft_get_file_name (data->ft));

	file_size = gnome_vfs_format_file_size_for_display (gossip_ft_get_file_size (data->ft));
	gtk_label_set_text (GTK_LABEL (file_size_label), file_size);
	g_free (file_size);

	gtk_label_set_text (GTK_LABEL (id_label), gossip_contact_get_id (contact));

	if (!G_STR_EMPTY (url)) {
		GArray *start, *end;

		start = g_array_new (FALSE, FALSE, sizeof (gint));
		end = g_array_new (FALSE, FALSE, sizeof (gint));

		num_matches = gossip_regex_match (GOSSIP_REGEX_ALL, url, start, end);

		g_array_free (start, TRUE);
		g_array_free (end, TRUE);
	}

	if (num_matches > 0) {
		GtkWidget *href;
		GtkWidget *alignment;

		href = gossip_link_button_new (url, url);

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
			  G_CALLBACK (ft_window_request_dialog_cb),
			  data);

	gtk_widget_show (dialog);
}

static void
ft_window_request_dialog_cb (GtkWidget *dialog,
			     gint       response,
			     FTData    *data)
{
	g_return_if_fail (GTK_IS_DIALOG (dialog));

	g_return_if_fail (GOSSIP_IS_PROTOCOL (data->protocol));
	g_return_if_fail (GOSSIP_IS_FT (data->ft));

	gtk_widget_destroy (dialog);

/* 	g_return_if_fail (GOSSIP_IS_CONTACT (contact)); */
/* 	g_return_if_fail (!G_STR_EMPTY (file)); */

	if (response == GTK_RESPONSE_YES) {
		gossip_ft_provider_accept (GOSSIP_FT_PROVIDER (data->protocol),
					   gossip_ft_get_id (data->ft));
	} else {
		gossip_ft_provider_decline (GOSSIP_FT_PROVIDER (data->protocol),
					    gossip_ft_get_id (data->ft));
	}

	g_object_unref (data->protocol);
	g_object_unref (data->ft);

	if (data->vcard) {
		g_object_unref (data->vcard);
	}

	g_free (data);
}

/*
 * sending with the file chooser
 */

void
gossip_ft_window_send_file (GossipContact *contact)
{
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	ft_window_filechooser_create (contact);
}

void
gossip_ft_window_send_file_from_uri (GossipContact *contact,
				     const gchar   *file)
{
	GossipSession    *session;
	GossipAccount    *account;
	GossipFTProvider *provider;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (!G_STR_EMPTY (file));

	account = gossip_contact_get_account (contact);

	session = gossip_app_get_session ();
	provider = gossip_session_get_ft_provider (session, account);

	gossip_ft_provider_send (provider, contact, file);
}

static void
ft_window_filechooser_create (GossipContact *contact)
{
	GtkWidget     *dialog;
	GtkFileFilter *filter;

	gossip_debug (DEBUG_DOMAIN, "Creating filechooser...");

	dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
			       "action", GTK_FILE_CHOOSER_ACTION_OPEN,
			       "select-multiple", FALSE,
			       NULL);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Select a file"));
	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OPEN, GTK_RESPONSE_OK,
				NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 GTK_RESPONSE_OK);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (ft_window_filechooser_response_cb),
			  g_object_ref (contact));

	/* filters */
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, "All Files");
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

	gtk_widget_show (dialog);
}

static void
ft_window_filechooser_response_cb (GtkDialog     *dialog,
				   gint           response_id,
				   GossipContact *contact)
{
	if (response_id == GTK_RESPONSE_OK) {
		GSList *list;

		list = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (dialog));

		if (list) {
			GSList *l;

			gossip_debug (DEBUG_DOMAIN, "File chooser selected files:");

			for (l = list; l; l = l->next) {
				gchar *file;

				file = l->data;

				gossip_debug (DEBUG_DOMAIN, "\t%s", file);
				gossip_ft_window_send_file_from_uri (contact, file);

				g_free (file);
			}

			g_slist_free (list);
		} else {
			gossip_debug (DEBUG_DOMAIN, "File chooser had no files selected");
		}
	}

	g_object_unref (contact);
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

#else

void
gossip_ft_window_init               (GossipSession *session)
{
}

void
gossip_ft_window_finalize           (GossipSession *session)
{
}

void
gossip_ft_window_send_file          (GossipContact *account)
{
}

void
gossip_ft_window_send_file_from_uri (GossipContact *contact,
				     const gchar   *file)
{
}

#endif
