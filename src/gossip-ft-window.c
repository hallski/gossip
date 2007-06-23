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
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#ifdef HAVE_GNOME
#include <libgnomevfs/gnome-vfs.h>
#endif

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-event.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-event-manager.h>
#include <libgossip/gossip-utils.h>
#include <libgossip/gossip-vcard.h>
#include <libgossip/gossip-conf.h>

#include "gossip-ft-window.h"
#include "gossip-glade.h"
#include "gossip-ui-utils.h"
#include "gossip-app.h"
#include "gossip-preferences.h"

#define DEBUG_DOMAIN "FileTransferDialog"

typedef struct {
	GtkWidget    *dialog;

	GtkWidget    *label_title;
	GtkWidget    *image_action;
	GtkWidget    *label_action;
	GtkWidget    *filechooserbutton_location;
	GtkWidget    *progressbar;
	GtkWidget    *button_yes;
	GtkWidget    *button_no;
	GtkWidget    *button_open_folder;
	GtkWidget    *button_open;
	GtkWidget    *button_close;
	
	gboolean      complete;

	GossipJabber *jabber;
	GossipFT     *ft;
} GossipFTDialog;

#ifdef HAVE_GNOME

static void ft_dialog_protocol_connected_cb          (GossipSession      *session,
						      GossipAccount      *account,
						      GossipJabber       *jabber,
						      gpointer            user_data);
static void ft_dialog_protocol_disconnected_cb       (GossipSession      *session,
						      GossipAccount      *account,
						      GossipJabber       *jabber,
						      gint                reason,
						      gpointer            user_data);
static void ft_dialog_request_cb                     (GossipJabber       *jabber,
						      GossipFT           *ft,
						      GossipSession      *session);
static void ft_dialog_initiated_cb                   (GossipJabber       *jabber,
						      GossipFT           *ft,
						      GossipSession      *session);
static void ft_dialog_complete_cb                    (GossipJabber       *jabber,
						      GossipFT           *ft,
						      GossipSession      *session);
static void ft_dialog_progress_cb                    (GossipJabber       *jabber,
						      GossipFT           *ft,
						      gdouble             progress,
						      GossipSession      *session);
static void ft_dialog_error_cb                       (GossipJabber       *jabber,
						      GossipFT           *ft,
						      GError             *error,
						      GossipSession      *session);
static void ft_dialog_event_activated_cb             (GossipEventManager *event_manager,
						      GossipEvent        *event,
						      GossipJabber       *jabber);
static void ft_dialog_response_cb                    (GtkWidget          *dialog,
						      gint                response,
						      GossipFTDialog     *data);
static void ft_dialog_destroy_cb                     (GtkDialog          *widget,
						      GossipFTDialog     *dialog);
static void ft_dialog_response_cb                    (GtkWidget          *widget,
						      gint                response,
						      GossipFTDialog     *dialog);
static void ft_dialog_show_complete                  (GossipFTDialog     *dialog);
static void ft_dialog_show_transferring              (GossipFTDialog     *dialog);
static void ft_dialog_show                           (GossipJabber       *jabber,
						      GossipFT           *ft);
static void ft_dialog_select_filechooser_response_cb (GtkDialog          *widget,
						      gint                response_id,
						      GossipContact      *contact);
static void ft_dialog_select_filechooser_create      (GossipContact      *contact);


static GHashTable *dialogs = NULL;

void
gossip_ft_dialog_init (GossipSession *session)
{
	g_object_ref (session);

	if (!dialogs) {
		dialogs = g_hash_table_new_full (g_direct_hash, 
						 g_direct_equal,
						 (GDestroyNotify) g_object_unref,
						 NULL);
	}

	g_signal_connect (session,
			  "protocol-connected",
			  G_CALLBACK (ft_dialog_protocol_connected_cb),
			  NULL);

	g_signal_connect (session,
			  "protocol-disconnected",
			  G_CALLBACK (ft_dialog_protocol_disconnected_cb),
			  NULL);
}

void
gossip_ft_dialog_finalize (GossipSession *session)
{
	g_signal_handlers_disconnect_by_func (session,
					      ft_dialog_protocol_connected_cb,
					      NULL);
	g_signal_handlers_disconnect_by_func (session,
					      ft_dialog_protocol_disconnected_cb,
					      NULL);

	if (dialogs) {
		g_hash_table_destroy (dialogs);
		dialogs = NULL;
	}

	g_object_unref (session);
}

/*
 * Receiving from another contact
 */
static void
ft_dialog_protocol_connected_cb (GossipSession  *session,
				 GossipAccount  *account,
				 GossipJabber   *jabber,
				 gpointer        user_data)
{
	g_signal_connect (jabber,
			  "file-transfer-request",
			  G_CALLBACK (ft_dialog_request_cb),
			  session);

	g_signal_connect (jabber,
			  "file-transfer-initiated",
			  G_CALLBACK (ft_dialog_initiated_cb),
			  session);

	g_signal_connect (jabber,
			  "file-transfer-complete",
			  G_CALLBACK (ft_dialog_complete_cb),
			  session);

	g_signal_connect (jabber,
			  "file-transfer-progress",
			  G_CALLBACK (ft_dialog_progress_cb),
			  session);

	g_signal_connect (jabber,
			  "file-transfer-error",
			  G_CALLBACK (ft_dialog_error_cb),
			  session);
}

static void
ft_dialog_protocol_disconnected_cb (GossipSession  *session,
				    GossipAccount  *account,
				    GossipJabber   *jabber,
				    gint            reason,
				    gpointer        user_data)
{
	g_signal_handlers_disconnect_by_func (jabber,
					      ft_dialog_request_cb,
					      session);
	g_signal_handlers_disconnect_by_func (jabber,
					      ft_dialog_complete_cb,
					      session);
	g_signal_handlers_disconnect_by_func (jabber,
					      ft_dialog_progress_cb,
					      session);
	g_signal_handlers_disconnect_by_func (jabber,
					      ft_dialog_error_cb,
					      session);
}

static void
ft_dialog_request_cb (GossipJabber  *jabber,
		      GossipFT      *ft,
		      GossipSession *session)
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
				  (GossipEventActivateFunction) ft_dialog_event_activated_cb,
				  G_OBJECT (jabber));
}

static void
ft_dialog_initiated_cb (GossipJabber  *jabber,
			 GossipFT      *ft,
			 GossipSession *session)
{
	GossipFTDialog *dialog;

	dialog = g_hash_table_lookup (dialogs, ft);
	if (!dialog) {
		return;
	}	      
	
	ft_dialog_show_transferring (dialog);
}

static void
ft_dialog_complete_cb (GossipJabber  *jabber,
		       GossipFT      *ft,
		       GossipSession *session)
{
	GossipFTDialog *dialog;

	dialog = g_hash_table_lookup (dialogs, ft);
	if (!dialog) {
		return;
	}	      
	
	ft_dialog_show_complete (dialog);
}

static void
ft_dialog_progress_cb (GossipJabber  *jabber,
		       GossipFT      *ft,
		       gdouble        progress,
		       GossipSession *session)
{
	GossipFTDialog *dialog;
	gchar          *progress_str;

	dialog = g_hash_table_lookup (dialogs, ft);
	if (!dialog) {
		return;
	}	      

	progress_str = g_strdup_printf ("%.2f %%", progress * 100);
	gossip_debug (DEBUG_DOMAIN, "Progress %s", progress_str);

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (dialog->progressbar), progress);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (dialog->progressbar), progress_str);
	g_free (progress_str);
}

static void
ft_dialog_error_cb (GossipJabber  *jabber,
		    GossipFT      *ft,
		    GError        *error,
		    GossipSession *session)
{
	GtkWidget      *dialog;
	GtkMessageType  type;
	const gchar    *str1, *str2;

	type = GTK_MESSAGE_ERROR;

	str2 = error->message;

	switch (error->code) {
	case GOSSIP_FT_ERROR_DECLINED:
		type = GTK_MESSAGE_INFO;
		str1 = _("Your file transfer offer declined.");
		str2 = _("The other user decided not to continue.");
		break;

	case GOSSIP_FT_ERROR_CLIENT_DISCONNECTED:
		type = GTK_MESSAGE_ERROR;
		str1 = _("Unable to complete the file transfer.");
		break;

	case GOSSIP_FT_ERROR_UNABLE_TO_CONNECT:
		type = GTK_MESSAGE_ERROR;
		str1 = _("Unable to start the file transfer.");
		break;

	case GOSSIP_FT_ERROR_UNSUPPORTED:
		type = GTK_MESSAGE_INFO;
		str1 = _("File transfer is not supported by both parties.");
		break;

	case GOSSIP_FT_ERROR_UNKNOWN:
		str1 = _("An unknown error occurred during file transfer.");
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
ft_dialog_event_activated_cb (GossipEventManager *event_manager,
			      GossipEvent        *event,
			      GossipJabber       *jabber)
{
	GossipFT *ft;

	ft = GOSSIP_FT (gossip_event_get_data (event));
	ft_dialog_show (jabber, ft);
}

/* 
 * Receiving dialog 
 */
static void
ft_dialog_destroy_cb (GtkDialog      *widget,
		      GossipFTDialog *dialog)
{
	g_hash_table_remove (dialogs, dialog->ft);

	if (dialog->jabber) {
		g_object_unref (dialog->jabber);
	}

	if (dialog->ft) {
		g_object_unref (dialog->ft);
	}

	g_free (dialog);
}

static void
ft_dialog_accept (GossipFTDialog *dialog) 
{
	GtkFileChooser *chooser;
	gchar          *folder;
	gchar          *uri;

	gtk_widget_set_sensitive (dialog->button_yes, FALSE);
	gtk_widget_set_sensitive (dialog->button_no, FALSE);
	
	chooser = GTK_FILE_CHOOSER (dialog->filechooserbutton_location);
	folder = gtk_file_chooser_get_uri (chooser);

	if (folder == NULL) {
		g_warning ("Folder was NULL, file wants to be saved non-locally, aborting...");
		return;
	}

	uri = g_build_path (G_DIR_SEPARATOR_S, 
			    folder, 
			    gossip_ft_get_file_name (dialog->ft), 
			    NULL);
	gossip_debug (DEBUG_DOMAIN, "Saving file in location:'%s', target uri will be:'%s'",
		      folder, uri);
	gossip_conf_set_string (gossip_conf_get (),
				GOSSIP_PREFS_FILE_TRANSFER_DEFAULT_FOLDER,
				folder);

	/* Send acceptance response */
	gossip_ft_set_location (dialog->ft, uri);
	gossip_ft_provider_accept (GOSSIP_FT_PROVIDER (dialog->jabber),
				   gossip_ft_get_id (dialog->ft));
	
	g_free (uri);
	g_free (folder);
}

static void
ft_dialog_response_cb (GtkWidget      *widget,
		       gint            response,
		       GossipFTDialog *dialog)
{
	const gchar *location;
	gchar       *dirname;

	switch (response) {
	case GTK_RESPONSE_YES:
		ft_dialog_accept (dialog);
		return;

	case GTK_RESPONSE_NO:
	case GTK_RESPONSE_DELETE_EVENT:
		gossip_ft_provider_decline (GOSSIP_FT_PROVIDER (dialog->jabber),
					    gossip_ft_get_id (dialog->ft));
		break;

	case GTK_RESPONSE_CANCEL:
		if (!dialog->complete) {
			gossip_ft_provider_cancel (GOSSIP_FT_PROVIDER (dialog->jabber),
						   gossip_ft_get_id (dialog->ft));
		}

		break;

	case 1:
		/* Open folder */
	        location = gossip_ft_get_location (dialog->ft);
		dirname = g_path_get_dirname (location);
		gossip_debug (DEBUG_DOMAIN, "Showing location:'%s'", dirname);
		gossip_url_show (dirname);
		g_free (dirname);
		return;

	case 2:
		/* Open */
	        location = gossip_ft_get_location (dialog->ft);
		gossip_debug (DEBUG_DOMAIN, "Opening uri:'%s'", location);
		gossip_url_show (location);
		return;

	default:
		break;
	}

	gtk_widget_destroy (widget);
}

static void
ft_dialog_show_complete (GossipFTDialog *dialog)
{
	gchar *str;

	dialog->complete = TRUE;

	gtk_widget_hide (dialog->button_yes);
	gtk_widget_hide (dialog->button_no);
 	gtk_widget_show (dialog->button_close); 

	if (gossip_ft_get_type (dialog->ft) == GOSSIP_FT_TYPE_RECEIVING) {
		gtk_widget_show (dialog->button_open_folder); 
		gtk_widget_show (dialog->button_open); 
		
		gtk_widget_set_sensitive (dialog->button_open_folder, TRUE);
		gtk_widget_set_sensitive (dialog->button_open, TRUE);
	} else {
		gtk_widget_hide (dialog->button_open_folder); 
		gtk_widget_hide (dialog->button_open); 
	}

	gtk_button_set_use_stock (GTK_BUTTON (dialog->button_close), TRUE);
	gtk_button_set_label (GTK_BUTTON (dialog->button_close), 
			      GTK_STOCK_CLOSE);

	str = g_strdup_printf ("<span weight='bold' size='larger'>%s</span>", 
			       _("Transfer complete"));
	gtk_label_set_markup (GTK_LABEL (dialog->label_title), str);
	gtk_label_set_use_markup (GTK_LABEL (dialog->label_title), TRUE);
	g_free (str);
	
	gtk_label_set_text (GTK_LABEL (dialog->label_action), 
			    _("The file has been transfered successfully."));

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (dialog->progressbar), 1.0);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (dialog->progressbar), "100 %");
}

static void
ft_dialog_show_transferring (GossipFTDialog *dialog)
{
	gchar *str;

	gtk_widget_hide (dialog->button_yes);
	gtk_widget_hide (dialog->button_no);
	gtk_widget_show (dialog->button_close);

	if (gossip_ft_get_type (dialog->ft) == GOSSIP_FT_TYPE_RECEIVING) {
		gtk_widget_show (dialog->button_open_folder); 
		gtk_widget_show (dialog->button_open); 

		gtk_widget_set_sensitive (dialog->button_open_folder, FALSE);
		gtk_widget_set_sensitive (dialog->button_open, FALSE);
	} else {
		gtk_widget_hide (dialog->button_open_folder); 
		gtk_widget_hide (dialog->button_open); 
	}

	gtk_widget_show (dialog->progressbar);

	gtk_widget_set_sensitive (dialog->filechooserbutton_location, FALSE);
	
	gtk_button_set_use_stock (GTK_BUTTON (dialog->button_close), TRUE);
	gtk_button_set_label (GTK_BUTTON (dialog->button_close), 
			      GTK_STOCK_CANCEL);
	
	str = g_strdup_printf ("<span weight='bold' size='larger'>%s</span>", 
			       _("Transferring file"));
	gtk_label_set_markup (GTK_LABEL (dialog->label_title), str);
	gtk_label_set_use_markup (GTK_LABEL (dialog->label_title), TRUE);
	g_free (str);
	
	gtk_label_set_text (GTK_LABEL (dialog->label_action),
			    _("Please wait while the file is transferred"));
}

static void
ft_dialog_show (GossipJabber *jabber,
		GossipFT     *ft)
{
	GossipFTDialog *dialog;
	GossipContact  *contact;
	GtkSizeGroup   *size_group;
	GtkFileChooser *chooser;
	GtkWidget      *label_id_stub;
	GtkWidget      *label_file_name_stub;
	GtkWidget      *label_file_size_stub;
	GtkWidget      *label_location_stub;
	GtkWidget      *label_id;
	GtkWidget      *label_file_name;
	GtkWidget      *label_file_size;
	const gchar    *name = NULL;
	gchar          *who;
	const gchar    *action;
	gchar          *str;
	gchar          *file_size;
	gchar          *default_folder;

	dialog = g_new0 (GossipFTDialog, 1);

	dialog->ft = g_object_ref (ft);
	dialog->jabber = g_object_ref (jabber);

	g_hash_table_insert (dialogs, g_object_ref (dialog->ft), dialog);

	gossip_glade_get_file_simple ("file-transfer.glade",
				      "file_transfer_dialog",
				      NULL,
				      "file_transfer_dialog", &dialog->dialog,
				      "image_action", &dialog->image_action,
				      "label_id_stub", &label_id_stub,
				      "label_file_name_stub", &label_file_name_stub,
				      "label_file_size_stub", &label_file_size_stub,
				      "label_location_stub", &label_location_stub,
				      "label_action", &dialog->label_action,
				      "label_title", &dialog->label_title,
				      "label_id", &label_id,
				      "label_file_name", &label_file_name,
				      "label_file_size", &label_file_size,
				      "filechooserbutton_location", &dialog->filechooserbutton_location,
				      "progressbar", &dialog->progressbar,
				      "button_yes", &dialog->button_yes,
				      "button_no", &dialog->button_no,
				      "button_open_folder", &dialog->button_open_folder,
				      "button_open", &dialog->button_open,
				      "button_close", &dialog->button_close,
				      NULL);

	contact = gossip_ft_get_contact (ft);
	name = gossip_contact_get_name (contact);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, label_id_stub);
	gtk_size_group_add_widget (size_group, label_file_name_stub);
	gtk_size_group_add_widget (size_group, label_file_size_stub);
	gtk_size_group_add_widget (size_group, label_location_stub);
	g_object_unref (size_group);

	gtk_label_set_text (GTK_LABEL (label_id), gossip_contact_get_id (contact));


	if (gossip_ft_get_type (dialog->ft) == GOSSIP_FT_TYPE_RECEIVING) {
		if (name) {
			who = g_strdup_printf (_("%s would like to send you a file"), name);
		} else {
			who = g_strdup (_("Someone would like to send you a file"));
		}

		action = _("Do you want to accept this file?");
	} else {
		if (name) {
			who = g_strdup_printf (_("Attempting to send file to %s"), name);
		} else {
			who = g_strdup (_("Attempting to send file"));
		}

		action = _("Please wait while the other participant responds");
	}
	
	str = g_strdup_printf ("<span weight='bold' size='larger'>%s</span>", who);
	gtk_label_set_markup (GTK_LABEL (dialog->label_title), str);
	gtk_label_set_use_markup (GTK_LABEL (dialog->label_title), TRUE);
	g_free (str);
	g_free (who);

	gtk_label_set_text (GTK_LABEL (dialog->label_action), action);
	gtk_label_set_text (GTK_LABEL (label_file_name), gossip_ft_get_file_name (ft));

	file_size = gnome_vfs_format_file_size_for_display (gossip_ft_get_file_size (ft));
	gtk_label_set_text (GTK_LABEL (label_file_size), file_size);
	g_free (file_size);

	if (gossip_ft_get_type (dialog->ft) == GOSSIP_FT_TYPE_RECEIVING) {
		/* Set location */
		if (!gossip_conf_get_string (gossip_conf_get (),
					     GOSSIP_PREFS_FILE_TRANSFER_DEFAULT_FOLDER,
					     &default_folder) || !default_folder) {
			default_folder = g_build_path (G_DIR_SEPARATOR_S, 
						       g_get_home_dir(),
						       "Desktop",
						       NULL);
		}

		chooser = GTK_FILE_CHOOSER (dialog->filechooserbutton_location);
		gtk_file_chooser_set_uri (chooser, default_folder);
		g_free (default_folder);

		gtk_widget_show (label_location_stub);
		gtk_widget_show (dialog->filechooserbutton_location);

		/* Hide progress until later */
		gtk_widget_hide (dialog->progressbar);

		/* Show/hide buttons */
		gtk_widget_show (dialog->button_yes);
		gtk_widget_show (dialog->button_no);
		gtk_widget_hide (dialog->button_close);
	} else {
		gtk_widget_hide (label_location_stub);
		gtk_widget_hide (dialog->filechooserbutton_location);

		gtk_widget_show (dialog->progressbar);

		/* Show/hide buttons */
		gtk_widget_hide (dialog->button_yes);
		gtk_widget_hide (dialog->button_no);
		gtk_widget_show (dialog->button_close);
	}

	gtk_widget_hide (dialog->button_open_folder);
	gtk_widget_hide (dialog->button_open);

	gtk_button_set_use_stock (GTK_BUTTON (dialog->button_close), TRUE);
	gtk_button_set_label (GTK_BUTTON (dialog->button_close), 
			      GTK_STOCK_CANCEL);

	/* Set up signalling */
	g_signal_connect (dialog->dialog, "response",
			  G_CALLBACK (ft_dialog_response_cb),
			  dialog);
	g_signal_connect (dialog->dialog, "destroy",
			  G_CALLBACK (ft_dialog_destroy_cb),
			  dialog);

	gtk_widget_show (dialog->dialog);
}

/*
 * Sending with the file chooser
 */
static void
ft_dialog_select_filechooser_response_cb (GtkDialog     *widget,
					  gint           response_id,
					  GossipContact *contact)
{
	if (response_id == GTK_RESPONSE_OK) {
		GSList *list;

		list = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (widget));

		if (list) {
			GSList *l;

			gossip_debug (DEBUG_DOMAIN, "File chooser selected files:");

			for (l = list; l; l = l->next) {
				gchar *file;

				file = l->data;

				gossip_debug (DEBUG_DOMAIN, "\t%s", file);
				gossip_ft_dialog_send_file_from_uri (contact, file);

				g_free (file);
			}

			g_slist_free (list);
		} else {
			gossip_debug (DEBUG_DOMAIN, "File chooser had no files selected");
		}
	}

	g_object_unref (contact);
	gtk_widget_destroy (GTK_WIDGET (widget));
}

static void
ft_dialog_select_filechooser_create (GossipContact *contact)
{
	GtkWidget     *widget;
	GtkFileFilter *filter;

	gossip_debug (DEBUG_DOMAIN, "Creating selection filechooser...");

	widget = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
			       "action", GTK_FILE_CHOOSER_ACTION_OPEN,
			       "select-multiple", FALSE,
			       NULL);

	gtk_window_set_title (GTK_WINDOW (widget), _("Select a file"));
	gtk_dialog_add_buttons (GTK_DIALOG (widget),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OPEN, GTK_RESPONSE_OK,
				NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (widget),
					 GTK_RESPONSE_OK);

	g_signal_connect (widget, "response",
			  G_CALLBACK (ft_dialog_select_filechooser_response_cb),
			  g_object_ref (contact));

	/* filters */
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, "All Files");
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (widget), filter);

	gtk_widget_show (widget);
}

void
gossip_ft_dialog_send_file (GossipContact *contact)
{
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	ft_dialog_select_filechooser_create (contact);
}

void
gossip_ft_dialog_send_file_from_uri (GossipContact *contact,
				     const gchar   *file)
{
	GossipSession    *session;
	GossipAccount    *account;
	GossipJabber     *jabber;
	GossipFTProvider *provider;
	GossipFT         *ft;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (!G_STR_EMPTY (file));

	account = gossip_contact_get_account (contact);

	session = gossip_app_get_session ();
	jabber = gossip_session_get_protocol (session, account);	
	provider = gossip_session_get_ft_provider (session, account);

	ft = gossip_ft_provider_send (provider, contact, file);
 	ft_dialog_show (jabber, ft);
}

#else

void
gossip_ft_dialog_init (GossipSession *session)
{
}

void
gossip_ft_dialog_finalize (GossipSession *session)
{
}

void
gossip_ft_dialog_send_file (GossipContact *account)
{
}

void
gossip_ft_dialog_send_file_from_uri (GossipContact *contact,
				     const gchar   *file)
{
}

#endif
