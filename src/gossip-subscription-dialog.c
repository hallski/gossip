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

#include <libgossip/gossip.h>

#include "gossip-app.h"
#include "gossip-glade.h"
#include "gossip-subscription-dialog.h"
#include "gossip-ui-utils.h"

#define DEBUG_DOMAIN "SubscriptionDialog"

typedef struct {
	GtkWidget      *dialog;
	GtkWidget      *who_label;
	GtkWidget      *question_label;
	GtkWidget      *id_label;
	GtkWidget      *id_label_value;
	GtkWidget      *website_label;
	GtkWidget      *name_label;
	GtkWidget      *name_entry;
	GtkWidget      *group_label;
	GtkWidget      *group_comboboxentry;
	GtkWidget      *personal_table;
	GtkWidget      *info_requested_hbox;

	GossipJabber   *jabber;
	GossipContact  *contact;
	GossipVCard    *vcard;
} GossipSubscriptionDialog;

static void subscription_dialog_protocol_connected_cb    (GossipSession            *session,
							  GossipAccount            *account,
							  GossipJabber             *jabber,
							  gpointer                  user_data);
static void subscription_dialog_protocol_disconnected_cb (GossipSession            *session,
							  GossipAccount            *account,
							  GossipJabber             *jabber,
							  gint                      reason,
							  gpointer                  user_data);
static void subscription_dialog_request_cb               (GossipJabber             *jabber,
							  GossipContact            *contact,
							  gpointer                  user_data);
static void subscription_dialog_event_activated_cb       (GossipEventManager       *event_manager,
							  GossipEvent              *event,
							  GossipJabber             *jabber);
static void subscription_dialog_show                     (GossipSubscriptionDialog *dialog);
static void subscription_dialog_vcard_cb                 (GossipResult              result,
							  GossipVCard              *vcard,
							  GossipContact            *contact);
static void subscription_dialog_setup_groups             (GtkComboBoxEntry         *comboboxentry);
static void subscription_dialog_request_dialog_cb        (GtkWidget                *dialog,
							  gint                      response,
							  GossipSubscriptionDialog *data);

static GHashTable *dialogs = NULL;

void
gossip_subscription_dialog_init (GossipSession *session)
{
	if (dialogs) {
		return;
	}

	dialogs = g_hash_table_new_full (gossip_contact_hash,
					 gossip_contact_equal,
					 g_object_unref,
					 NULL);

	g_object_ref (session);

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

	g_object_unref (session);

	g_hash_table_destroy (dialogs);
	dialogs = NULL;
}

static void
subscription_dialog_protocol_connected_cb (GossipSession  *session,
					   GossipAccount  *account,
					   GossipJabber   *jabber,
					   gpointer        user_data)
{
	g_signal_connect (jabber,
			  "subscription-request",
			  G_CALLBACK (subscription_dialog_request_cb),
			  session);
}

static void
subscription_dialog_protocol_disconnected_cb (GossipSession  *session,
					      GossipAccount  *account,
					      GossipJabber   *jabber,
					      gint            reason,
					      gpointer        user_data)
{
	g_signal_handlers_disconnect_by_func (jabber,
					      subscription_dialog_request_cb,
					      session);
}

static void
subscription_dialog_request_cb (GossipJabber *jabber,
				GossipContact  *contact,
				gpointer        user_data)
{
	GossipEvent        *event;
	GossipContactType   type;
	GossipSubscription  subscription;
	gchar              *str;

	gossip_debug (DEBUG_DOMAIN, "New request from:'%s'",
		      gossip_contact_get_id (contact));

	type = gossip_contact_get_type (contact);
	subscription = gossip_contact_get_subscription (contact);

	/* If the contact is on our contact list and we get a
	 * subscription request, send back subscribed because
	 * we obviously want them on our roster, there is no need to
	 * show a dialog and confirm it with the user.
	 */
	if (type == GOSSIP_CONTACT_TYPE_CONTACTLIST) {
		gossip_debug (DEBUG_DOMAIN, "Silently accepting request");
		gossip_jabber_set_subscription (jabber, contact, TRUE);
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "Adding request to event manager");

	event = gossip_event_new (GOSSIP_EVENT_SUBSCRIPTION_REQUEST);

	str = g_strdup_printf (_("New subscription request from %s"),
			       gossip_contact_get_name (contact));

	g_object_set (event,
		      "message", str,
		      "data", contact,
		      NULL);
	g_free (str);

	gossip_event_manager_add
		(gossip_app_get_event_manager (),
		 event,
		 (GossipEventActivateFunction) subscription_dialog_event_activated_cb,
		 G_OBJECT (jabber));
}

static void
subscription_dialog_event_activated_cb (GossipEventManager *event_manager,
					GossipEvent        *event,
					GossipJabber       *jabber)
{
	GossipContact            *contact;
	GossipSubscriptionDialog *dialog;

	contact = GOSSIP_CONTACT (gossip_event_get_data (event));

	dialog = g_hash_table_lookup (dialogs, contact);
	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->dialog));
		return;
	}

	dialog = g_new0 (GossipSubscriptionDialog, 1);

	dialog->jabber = g_object_ref (jabber);
	dialog->contact = g_object_ref (contact);

	g_hash_table_insert (dialogs, dialog->contact, dialog);

	subscription_dialog_show (dialog);
}

static void
subscription_dialog_show (GossipSubscriptionDialog *dialog)
{
	GossipSession        *session;
	GossipAccountManager *account_manager;
	guint                 n;
	GtkSizeGroup         *size_group;
	gchar                *who;
	gchar                *question;
	gchar                *str;

	gossip_session_get_vcard (gossip_app_get_session (),
				  NULL,
				  dialog->contact,
				  (GossipVCardCallback) subscription_dialog_vcard_cb,
				  g_object_ref (dialog->contact),
				  NULL);

	gossip_glade_get_file_simple ("main.glade",
				      "subscription_request_dialog",
				      NULL,
				      "subscription_request_dialog", &dialog->dialog,
				      "who_label", &dialog->who_label,
				      "question_label", &dialog->question_label,
				      "id_label", &dialog->id_label,
				      "id_label_value", &dialog->id_label_value,
				      "website_label", &dialog->website_label,
				      "name_label", &dialog->name_label,
				      "name_entry", &dialog->name_entry,
				      "group_label", &dialog->group_label,
				      "group_comboboxentry", &dialog->group_comboboxentry,
				      "personal_table", &dialog->personal_table,
				      "info_requested_hbox", &dialog->info_requested_hbox,
				      NULL);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, dialog->id_label);
	gtk_size_group_add_widget (size_group, dialog->website_label);
	gtk_size_group_add_widget (size_group, dialog->name_label);
	gtk_size_group_add_widget (size_group, dialog->group_label);

	g_object_unref (size_group);

	gtk_entry_set_text (GTK_ENTRY (dialog->name_entry),
			    gossip_contact_get_id (dialog->contact));

	session = gossip_app_get_session ();
	account_manager = gossip_session_get_account_manager (session);
	n = gossip_account_manager_get_count (account_manager);

	if (n > 1) {
		GossipAccount *account;
		const gchar   *account_name;

		account = gossip_contact_get_account (dialog->contact);
		account_name = gossip_account_get_name (account);

		who = g_strdup_printf (
			_("Someone wants to be added to your contact list for your '%s' account."),
			account_name);
	} else { 
		who = g_strdup_printf (
			_("Someone wants to be added to your contact list."));
	}

	question = g_strdup (_("Do you want to add this person to your contact list?"));

	gtk_widget_grab_focus (dialog->name_entry);

	subscription_dialog_setup_groups (GTK_COMBO_BOX_ENTRY (dialog->group_comboboxentry));

	str = g_strdup_printf ("<span weight='bold' size='larger'>%s</span>", who);
	gtk_label_set_markup (GTK_LABEL (dialog->who_label), str);
	gtk_label_set_use_markup (GTK_LABEL (dialog->who_label), TRUE);
	g_free (str);
	g_free (who);

	gtk_label_set_text (GTK_LABEL (dialog->question_label),
			    question);
	g_free (question);

	gtk_label_set_text (GTK_LABEL (dialog->id_label_value),
			    gossip_contact_get_id (dialog->contact));

	gtk_widget_hide (dialog->website_label);

	g_signal_connect (dialog->dialog,
			  "response",
			  G_CALLBACK (subscription_dialog_request_dialog_cb),
			  dialog);

	gtk_widget_show (dialog->dialog);
}

static void
subscription_dialog_vcard_cb (GossipResult   result,
			      GossipVCard   *vcard,
			      GossipContact *contact)
{
	GossipSubscriptionDialog *dialog;
	const gchar              *contact_name = NULL;
	const gchar              *url = NULL;
	gint                      num_matches = 0;

	dialog = g_hash_table_lookup (dialogs, contact);
	g_object_unref (contact);

	if (!dialog) {
		return;
	}

	if (GOSSIP_IS_VCARD (vcard)) {
		dialog->vcard = g_object_ref (vcard);

		contact_name = gossip_vcard_get_name (vcard);
		url = gossip_vcard_get_url (vcard);
	}

	gtk_widget_hide (dialog->info_requested_hbox);

	if (contact_name) {
		GossipSession        *session;
		GossipAccountManager *account_manager;
		gchar                *who;
		gchar                *question;
		gchar                *str;
		guint                 n;

		session = gossip_app_get_session ();
		account_manager = gossip_session_get_account_manager (session);
		n = gossip_account_manager_get_count (account_manager);

		if (n > 1) {
			GossipAccount *account;
			const gchar   *account_name;

			account = gossip_contact_get_account (contact);
			account_name = gossip_account_get_name (account);

			who = g_strdup_printf (
				_("%s wants to be added to your contact list for your '%s' account."), 
				contact_name, account_name);
		} else { 
			who = g_strdup_printf (
				_("%s wants to be added to your contact list."), 
				contact_name);
		}

		gtk_entry_set_text (GTK_ENTRY (dialog->name_entry), contact_name);

		question = g_strdup_printf (_("Do you want to add %s to your contact list?"),
					    contact_name);

		str = g_strdup_printf ("<span weight='bold' size='larger'>%s</span>", who);
		gtk_label_set_markup (GTK_LABEL (dialog->who_label), str);
		gtk_label_set_use_markup (GTK_LABEL (dialog->who_label), TRUE);
		gtk_label_set_text (GTK_LABEL (dialog->question_label),
				    question);
		g_free (str);
		g_free (who);
		g_free (question);
	}

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

		gtk_table_attach (GTK_TABLE (dialog->personal_table),
				  alignment,
				  1, 2,
				  1, 2,
				  GTK_FILL, GTK_FILL,
				  0, 0);

		gtk_widget_show_all (dialog->personal_table);
	}
}

static void
subscription_dialog_setup_groups (GtkComboBoxEntry *comboboxentry)
{
	GtkListStore    *store;
	GtkCellRenderer *renderer;
	GtkTreeIter      iter;
	GList           *l;
	GList           *all_groups;

	store = gtk_list_store_new (1, G_TYPE_STRING);

	all_groups = gossip_session_get_groups (gossip_app_get_session ());

	for (l = all_groups; l; l = l->next) {
		const gchar *group;

		group = l->data;

		if (strcmp (group, _("Unsorted")) == 0) {
			continue;
		}

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, group, -1);
	}

	gtk_combo_box_set_model (GTK_COMBO_BOX (comboboxentry),
				 GTK_TREE_MODEL (store));
	gtk_combo_box_entry_set_text_column (comboboxentry, 0);

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (comboboxentry));

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (comboboxentry), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (comboboxentry), renderer,
					"text", 0,
					NULL);

	g_object_unref (store);

	g_list_free (all_groups);
}

static void
subscription_dialog_request_dialog_cb (GtkWidget                *widget,
				       gint                      response,
				       GossipSubscriptionDialog *dialog)
{
	g_return_if_fail (GOSSIP_IS_JABBER (dialog->jabber));
	g_return_if_fail (GOSSIP_IS_CONTACT (dialog->contact));

	if (response == GTK_RESPONSE_YES ||
	    response == GTK_RESPONSE_NO) {
		gboolean subscribed;

		gossip_debug (DEBUG_DOMAIN, "Sending subscribed");

		subscribed = (response == GTK_RESPONSE_YES);
		gossip_jabber_set_subscription (dialog->jabber,
						dialog->contact,
						subscribed);

		if (subscribed) {
			GtkWidget   *group_entry;
			const gchar *name;
			const gchar *group;
			const gchar *message;

			group_entry = GTK_BIN (dialog->group_comboboxentry)->child;

			name = gtk_entry_get_text (GTK_ENTRY (dialog->name_entry));
			group = gtk_entry_get_text (GTK_ENTRY (group_entry));

			message = _("I would like to add you to my contact list.");

			gossip_debug (DEBUG_DOMAIN, "Adding contact");

			gossip_jabber_add_contact (dialog->jabber,
						   gossip_contact_get_id (dialog->contact),
						   name, group,
						   message);
		}
	}

	g_hash_table_remove (dialogs, dialog->contact);
	gtk_widget_destroy (widget);

	g_object_unref (dialog->jabber);

	if (dialog->vcard) {
		g_object_unref (dialog->vcard);
	}

	g_free (dialog);
}
