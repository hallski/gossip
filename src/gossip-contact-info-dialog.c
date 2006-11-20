/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2006 Imendio AB
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
#include <glib/gi18n.h>

#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-paths.h>
#include <libgossip/gossip-utils.h>

#include "gossip-app.h"
#include "gossip-avatar-image.h"
#include "gossip-ui-utils.h"
#include "gossip-contact-info-dialog.h"

typedef struct {
	GossipContact *contact;

	GtkWidget     *dialog;

	GtkWidget     *id_label;
	GtkWidget     *name_label;
	GtkWidget     *stub_email_label;
	GtkWidget     *stub_web_label;
	GtkWidget     *personal_status_label;
	GtkWidget     *personal_status_hbox;
	GtkWidget     *personal_vbox;
	GtkWidget     *personal_table;

	GtkWidget     *client_label;
	GtkWidget     *version_label;
	GtkWidget     *os_label;
	GtkWidget     *stub_client_label;
	GtkWidget     *stub_version_label;
	GtkWidget     *stub_os_label;

	GtkWidget     *client_status_label;
	GtkWidget     *client_status_hbox;
	GtkWidget     *client_vbox;
	GtkWidget     *client_not_avail_label;
	GtkWidget     *client_table;

	GtkWidget     *subscription_hbox;
	GtkWidget     *subscription_label;
	GtkWidget     *presence_list_vbox;
	GtkWidget     *presence_table;

	GtkWidget     *description_vbox;
	GtkWidget     *description_label;
	GtkWidget     *avatar_image;

	gulong         contact_signal_handler;
	gulong         presence_signal_handler;

	guint          show_timeout_id;
} GossipContactInfoDialog;

static void contact_info_dialog_init                 (void);
static void contact_info_dialog_update_subscription  (GossipContactInfoDialog *dialog);
static void contact_info_dialog_update_presences     (GossipContactInfoDialog *dialog);
static void contact_info_dialog_get_vcard_cb         (GossipResult             result,
						      GossipVCard             *vcard,
						      GossipContact           *contact);
static void contact_info_dialog_get_version_cb       (GossipResult             result,
						      GossipVersionInfo       *version_info,
						      GossipContact           *contact);
static void contact_info_dialog_contact_updated_cb   (GossipSession           *session,
						      GossipContact           *contact,
						      gpointer                 user_data);
static void contact_info_dialog_presence_updated_cb  (GossipSession           *session,
						      GossipContact           *contact,
						      gpointer                 user_data);
static void contact_info_dialog_subscribe_clicked_cb (GtkWidget               *widget,
						      GossipContactInfoDialog *dialog);
static void contact_info_dialog_destroy_cb           (GtkWidget               *widget,
						      GossipContactInfoDialog *dialog);
static void contact_info_dialog_response_cb          (GtkWidget               *widget,
						      gint                     response,
						      GossipContactInfoDialog *dialog);

static GHashTable *dialogs = NULL;

static void
contact_info_dialog_init (void)
{
	if (dialogs) {
		return;
	}

	dialogs = g_hash_table_new_full (gossip_contact_hash,
					 gossip_contact_equal,
					 g_object_unref,
					 NULL);
}

static void
contact_info_dialog_update_subscription (GossipContactInfoDialog *dialog)
{
	GossipSubscription subscription;

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (GOSSIP_IS_CONTACT (dialog->contact));

	subscription = gossip_contact_get_subscription (dialog->contact);

	if (subscription == GOSSIP_SUBSCRIPTION_NONE ||
	    subscription == GOSSIP_SUBSCRIPTION_FROM) {
		gtk_widget_show_all (dialog->subscription_hbox);
	} else {
		gtk_widget_hide (dialog->subscription_hbox);
	}
}

static void
contact_info_dialog_update_presences (GossipContactInfoDialog *dialog)
{
	GdkPixbuf   *pixbuf = NULL;
	const gchar *status = NULL;
	const gchar *resource = NULL;
	GList       *presences, *l;
	gint         i = 0;
	gint         cols = 4;

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (GOSSIP_IS_CONTACT (dialog->contact));

	/* first clean up anything already in there... */
	gtk_widget_destroy (dialog->presence_table);

	/* now create an object for each presence the contact has (for
	   each resource if in the case of Jabber). */
	dialog->presence_table = gtk_table_new (1, cols, FALSE);
	gtk_widget_show (dialog->presence_table);
	gtk_box_pack_start (GTK_BOX (dialog->presence_list_vbox),
			    dialog->presence_table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings (GTK_TABLE (dialog->presence_table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (dialog->presence_table), 6);

	presences = gossip_contact_get_presence_list (dialog->contact);

	if (!presences) {
		GtkWidget *widget;

		pixbuf = gossip_pixbuf_offline ();
		status = _("Offline");

		widget = gtk_image_new_from_pixbuf (pixbuf);
		g_object_unref (pixbuf);
		gtk_table_attach (GTK_TABLE (dialog->presence_table),
				  widget,
				  0, 1,
				  i, i + 1,
				  0, 0,
				  0, 0);

		widget = gtk_label_new (status);
		gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
		gtk_table_attach (GTK_TABLE (dialog->presence_table),
				  widget,
				  1, 2,
				  i, i + 1,
				  0, 0,
				  0, 0);

		gtk_widget_show_all (dialog->presence_table);
		return;
	}

	for (l = presences, i = 0; l; l = l->next, i++) {
		GossipPresence *presence;
		GtkWidget      *widget;

		presence = l->data;

		if (presence) {
			pixbuf = gossip_pixbuf_for_presence (presence);

			status = gossip_presence_get_status (presence);
			if (!status) {
				GossipPresenceState state;

				state = gossip_presence_get_state (presence);
				status = gossip_presence_state_get_default_status (state);
			}

			resource = gossip_presence_get_resource (presence);
		}

		if (i > 1) {
			gtk_table_resize (GTK_TABLE (dialog->presence_table), i, cols);
		}

		widget = gtk_image_new_from_pixbuf (pixbuf);
		g_object_unref (pixbuf);
		gtk_table_attach (GTK_TABLE (dialog->presence_table),
				  widget,
				  0, 1,
				  i, i + 1,
				  0, 0,
				  0, 0);

		widget = gtk_label_new (status);
		gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
		gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
		gtk_table_attach (GTK_TABLE (dialog->presence_table),
				  widget,
				  1, 2,
				  i, i + 1,
				  GTK_FILL,
				  GTK_FILL,
				  0, 0);

		if (!resource) {
			continue;
		}

		widget = gtk_label_new ("-");
		gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
		gtk_table_attach (GTK_TABLE (dialog->presence_table), widget,
				  2, 3,
				  i, i + 1,
				  0, 0,
				  0, 0.5);

		widget = gtk_label_new (resource);
		gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
		gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
		gtk_table_attach (GTK_TABLE (dialog->presence_table),
				  widget,
				  3, 4,
				  i, i + 1,
				  GTK_EXPAND | GTK_FILL,
				  GTK_EXPAND | GTK_FILL,
				  0, 0);
	}

	gtk_widget_show_all (dialog->presence_table);
}

static void
contact_info_dialog_get_vcard_cb (GossipResult   result,
				  GossipVCard   *vcard,
				  GossipContact *contact)
{
	GossipContactInfoDialog *dialog;
	gboolean                 show_personal = FALSE;
	const gchar             *str;
	GdkPixbuf               *pixbuf;

	dialog = g_hash_table_lookup (dialogs, contact);
	g_object_unref (contact);

	if (!dialog) {
		return;
	}

	if (result != GOSSIP_RESULT_OK) {
		if (dialog->dialog) {
			gtk_widget_hide (dialog->personal_status_hbox);
			gtk_widget_hide (dialog->personal_vbox);
		}

		return;
	}

	pixbuf = gossip_pixbuf_avatar_from_vcard (vcard);
	if (pixbuf != NULL) {
		gossip_avatar_image_set_pixbuf (GOSSIP_AVATAR_IMAGE (dialog->avatar_image), pixbuf);
		g_object_unref (pixbuf);
	}

	str = gossip_vcard_get_description (vcard);
	if (!G_STR_EMPTY (str)) {
		gtk_label_set_text (GTK_LABEL (dialog->description_label), str);
		gtk_widget_show (dialog->description_vbox);
	} else {
		gtk_widget_hide (dialog->description_vbox);
	}

	str = gossip_vcard_get_name (vcard);
	if (!G_STR_EMPTY (str)) {
		gtk_label_set_text (GTK_LABEL (dialog->name_label), str);
	}

	str = gossip_vcard_get_email (vcard);
	if (!G_STR_EMPTY (str)) {
		GtkWidget *href, *alignment;
		gchar     *link;

		show_personal = TRUE;

		link = g_strdup_printf ("mailto:%s", str);
		href = gossip_link_button_new (link, str);
		g_free (link);

		alignment = gtk_alignment_new (0, 1, 0, 0.5);
		gtk_container_add (GTK_CONTAINER (alignment), href);

		gtk_table_attach (GTK_TABLE (dialog->personal_table),
				  alignment,
				  1, 2,
				  0, 1,
				  GTK_FILL, GTK_FILL,
				  0, 0);

		gtk_widget_show_all (alignment);
		gtk_widget_show (dialog->stub_email_label);
	} else {
		gtk_widget_hide (dialog->stub_email_label);
	}

	str = gossip_vcard_get_url (vcard);
	if (!G_STR_EMPTY (str)) {
		GtkWidget *href, *alignment;

		show_personal = TRUE;

		href = gossip_link_button_new (str, str);

		alignment = gtk_alignment_new (0, 1, 0, 0.5);
		gtk_container_add (GTK_CONTAINER (alignment), href);

		gtk_table_attach (GTK_TABLE (dialog->personal_table),
				  alignment,
				  1, 2,
				  1, 2,
				  GTK_FILL, GTK_FILL,
				  0, 0);

		gtk_widget_show_all (alignment);
		gtk_widget_show (dialog->stub_web_label);
	} else {
		gtk_widget_hide (dialog->stub_web_label);
	}

	gtk_widget_hide (dialog->personal_status_hbox);

	if (show_personal) {
		gtk_widget_show (dialog->personal_vbox);
	} else {
		gtk_widget_hide (dialog->personal_vbox);
	}
}

static void
contact_info_dialog_get_version_cb (GossipResult       result,
				    GossipVersionInfo *version_info,
				    GossipContact     *contact)
{
	GossipContactInfoDialog *dialog;
	const gchar             *str;
	gboolean                 show_client_info = FALSE;

	dialog = g_hash_table_lookup (dialogs, contact);
	g_object_unref (contact);

	if (!dialog) {
		return;
	}

	if (result != GOSSIP_RESULT_OK) {
		if (dialog->dialog) {
			gtk_widget_hide (dialog->client_status_hbox);
			gtk_widget_hide (dialog->client_vbox);
		}

		return;
	}

	str = gossip_version_info_get_name (version_info);
	if (str && strcmp (str,  "") != 0) {
		show_client_info = TRUE;

		gtk_label_set_text (GTK_LABEL (dialog->client_label), str);
		gtk_widget_show (dialog->client_label);
		gtk_widget_show (dialog->stub_client_label);
	} else {
		gtk_widget_hide (dialog->client_label);
		gtk_widget_hide (dialog->stub_client_label);
	}

	str = gossip_version_info_get_version (version_info);
	if (str && strcmp (str, "") != 0) {
		show_client_info = TRUE;

		gtk_label_set_text (GTK_LABEL (dialog->version_label), str);
		gtk_widget_show (dialog->version_label);
		gtk_widget_show (dialog->stub_version_label);
	} else {
		gtk_widget_hide (dialog->version_label);
		gtk_widget_hide (dialog->stub_version_label);
	}

	str = gossip_version_info_get_os (version_info);
	if (str && strcmp (str, "") != 0) {
		show_client_info = TRUE;

		gtk_label_set_text (GTK_LABEL (dialog->os_label), str);
		gtk_widget_show (dialog->os_label);
		gtk_widget_show (dialog->stub_os_label);
	} else {
		gtk_widget_hide (dialog->os_label);
		gtk_widget_hide (dialog->stub_os_label);
	}

	gtk_widget_hide (dialog->client_status_hbox);

	if (show_client_info) {
		gtk_widget_show (dialog->client_vbox);
		gtk_widget_show (dialog->client_table);
	} else {
		gtk_widget_hide (dialog->client_vbox);
	}
}

static void
contact_info_dialog_contact_updated_cb (GossipSession           *session,
					GossipContact           *contact,
					gpointer                 user_data)
{
	GossipContactInfoDialog *dialog;

	dialog = g_hash_table_lookup (dialogs, contact);

	if (!dialog) {
		return;
	}

	if (!gossip_contact_equal (contact, dialog->contact)) {
		return;
	}

	contact_info_dialog_update_subscription (dialog);
	contact_info_dialog_update_presences (dialog);
}

static void
contact_info_dialog_presence_updated_cb (GossipSession *session,
					 GossipContact *contact,
					 gpointer       user_data)
{
	GossipContactInfoDialog *dialog;

	dialog = g_hash_table_lookup (dialogs, contact);

	if (!dialog) {
		return;
	}

	if (!gossip_contact_equal (contact, dialog->contact)) {
		return;
	}

	contact_info_dialog_update_presences (dialog);
}

static void
contact_info_dialog_subscribe_clicked_cb (GtkWidget               *widget,
					  GossipContactInfoDialog *dialog)
{
	GossipSession *session;
	GossipAccount *account;
	const gchar   *message;

	message = _("I would like to add you to my contact list.");

	session = gossip_app_get_session ();
	account = gossip_session_find_account (session, dialog->contact);

	gossip_session_add_contact (session,
				    account,
				    gossip_contact_get_id (dialog->contact),
				    gossip_contact_get_name (dialog->contact),
				    NULL, /* group */
				    message);

	g_object_unref (account);
}

static void
contact_info_dialog_destroy_cb (GtkWidget               *widget,
				GossipContactInfoDialog *dialog)
{
	if (dialog->contact_signal_handler) {
		g_signal_handler_disconnect (gossip_app_get_session (),
					     dialog->contact_signal_handler);
	}

	if (dialog->presence_signal_handler) {
		g_signal_handler_disconnect (gossip_app_get_session (),
					     dialog->presence_signal_handler);
	}

	if (dialog->show_timeout_id) {
		g_source_remove (dialog->show_timeout_id);
	}

	g_hash_table_remove (dialogs, dialog->contact);

	g_free (dialog);
}

static void
contact_info_dialog_response_cb (GtkWidget               *widget,
				 gint                     response,
				 GossipContactInfoDialog *dialog)
{
	gtk_widget_destroy (dialog->dialog);
}

static gboolean
contact_info_dialog_show_timeout_cb (GossipContactInfoDialog *dialog)
{
	gtk_widget_show (dialog->dialog);

	dialog->show_timeout_id = 0;

	return FALSE;
}

void
gossip_contact_info_dialog_show (GossipContact *contact,
				 GtkWindow     *parent)
{
	GossipContactInfoDialog *dialog;
	GossipSession           *session;
	GossipAccount           *account;
	GladeXML                *glade;
	gchar                   *str;
	GtkSizeGroup            *size_group;
	guint                    id;
	GtkWidget               *avatar_image_placeholder;
	GdkPixbuf               *pixbuf;
	gchar                   *path;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	contact_info_dialog_init ();

	dialog = g_hash_table_lookup (dialogs, contact);
	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->dialog));
		return;
	}

	session = gossip_app_get_session ();
	account = gossip_session_find_account (session, contact);

	dialog = g_new0 (GossipContactInfoDialog, 1);

	dialog->contact = g_object_ref (contact);

	g_hash_table_insert (dialogs, dialog->contact, dialog);

	glade = gossip_glade_get_file ("main.glade",
				       "contact_information_dialog",
				       NULL,
				       "contact_information_dialog", &dialog->dialog,
				       "id_label", &dialog->id_label,
				       "name_label", &dialog->name_label,
				       "client_label", &dialog->client_label,
				       "version_label", &dialog->version_label,
				       "os_label", &dialog->os_label,
				       "personal_vbox", &dialog->personal_vbox,
				       "personal_table", &dialog->personal_table,
				       "description_vbox", &dialog->description_vbox,
				       "description_label", &dialog->description_label,
				       "client_table", &dialog->client_table,
				       "stub_email_label", &dialog->stub_email_label,
				       "stub_web_label", &dialog->stub_web_label,
				       "stub_client_label", &dialog->stub_client_label,
				       "stub_version_label", &dialog->stub_version_label,
				       "stub_os_label", &dialog->stub_os_label,
				       "personal_status_label", &dialog->personal_status_label,
				       "personal_status_hbox", &dialog->personal_status_hbox,
				       "client_status_label", &dialog->client_status_label,
				       "client_status_hbox", &dialog->client_status_hbox,
				       "client_vbox", &dialog->client_vbox,
				       "subscription_hbox", &dialog->subscription_hbox,
				       "subscription_label", &dialog->subscription_label,
				       "presence_list_vbox", &dialog->presence_list_vbox,
				       "presence_table", &dialog->presence_table,
				       "avatar_image_placeholder", &avatar_image_placeholder,
				       NULL);

	dialog->avatar_image = gossip_avatar_image_new (NULL);
	gtk_container_add (GTK_CONTAINER (avatar_image_placeholder),
			   dialog->avatar_image);

	path = gossip_paths_get_image_path ("vcard_48.png");
	pixbuf = gdk_pixbuf_new_from_file (path, NULL);
	g_free (path);
	if (pixbuf) {
		gossip_avatar_image_set_pixbuf (
			GOSSIP_AVATAR_IMAGE (dialog->avatar_image), pixbuf);
		g_object_unref (pixbuf);
	}
	gtk_widget_show (dialog->avatar_image);

	gossip_glade_connect (glade,
			      dialog,
			      "contact_information_dialog", "destroy", contact_info_dialog_destroy_cb,
			      "contact_information_dialog", "response", contact_info_dialog_response_cb,
			      "subscribe_button", "clicked", contact_info_dialog_subscribe_clicked_cb,
			      NULL);

	g_object_unref (glade);

	/* Align widgets */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, dialog->stub_email_label);
	gtk_size_group_add_widget (size_group, dialog->stub_web_label);
	gtk_size_group_add_widget (size_group, dialog->stub_client_label);
	gtk_size_group_add_widget (size_group, dialog->stub_version_label);
	gtk_size_group_add_widget (size_group, dialog->stub_os_label);

	g_object_unref (size_group);

	/* Set labels */
	gtk_label_set_text (GTK_LABEL (dialog->id_label),
			    gossip_contact_get_id (contact));

	gtk_label_set_text (GTK_LABEL (dialog->name_label),
			    gossip_contact_get_name (contact));

	/* Set up transient parent */
	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog), parent);
	}

	/* Set details */
	contact_info_dialog_update_subscription (dialog);
	contact_info_dialog_update_presences (dialog);

	/* Subscription listener */
	id = g_signal_connect (session,
			       "contact-updated",
			       G_CALLBACK (contact_info_dialog_contact_updated_cb),
			       NULL);
	dialog->contact_signal_handler = id;

	/* Presence listener */
	id = g_signal_connect (session,
			       "contact-presence-updated",
			       G_CALLBACK (contact_info_dialog_presence_updated_cb),
			       NULL);
	dialog->presence_signal_handler = id;

	/* Get vcard and version info */
	str = g_strdup_printf ("<i>%s</i>",
			       _("Information requested..."));
	gtk_label_set_markup (GTK_LABEL (dialog->personal_status_label), str);
	gtk_label_set_markup (GTK_LABEL (dialog->client_status_label), str);
	g_free (str);

	gtk_widget_show (dialog->personal_vbox);
	gtk_widget_show (dialog->client_vbox);

	gossip_session_get_vcard (session,
				  account,
				  contact,
				  (GossipVCardCallback) contact_info_dialog_get_vcard_cb,
				  g_object_ref (contact),
				  NULL);

	gossip_session_get_version (session,
				    contact,
				    (GossipVersionCallback) contact_info_dialog_get_version_cb,
				    g_object_ref (contact),
				    NULL);

	dialog->show_timeout_id =
		g_timeout_add (500,
			       (GSourceFunc) contact_info_dialog_show_timeout_cb,
			       dialog);

	g_object_unref (account);
}

