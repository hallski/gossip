/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio AB
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
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-utils.h>

#include "gossip-account-chooser.h"
#include "gossip-add-contact-dialog.h"
#include "gossip-app.h"
#include "gossip-avatar-image.h"
#include "gossip-ui-utils.h"

#define DEBUG_DOMAIN "AddContact"

#define RESPONSE_ADD 1

typedef struct {
	GtkWidget   *dialog;

	GtkWidget   *account_chooser;
	GtkWidget   *avatar_image;

	GtkWidget   *table_who;
	GtkWidget   *label_account;
	GtkWidget   *label_id;
	GtkWidget   *entry_id;
	GtkWidget   *hbox_information;
	GtkWidget   *vbox_information;
	GtkWidget   *table_information;
	GtkWidget   *label_information;
	GtkWidget   *label_name_stub;
	GtkWidget   *label_email_stub;
	GtkWidget   *label_country_stub;
	GtkWidget   *label_name;
	GtkWidget   *label_email;
	GtkWidget   *label_country;
	GtkWidget   *label_alias;
	GtkWidget   *entry_alias;
	GtkWidget   *label_group;
	GtkWidget   *combo_group;
	GtkWidget   *entry_group;
	GtkWidget   *button_cancel;
	GtkWidget   *button_add;

	GCompletion *group_completion;
	guint        idle_complete;

	gchar       *last_id;
} GossipAddContactDialog;

static void     add_contact_dialog_vcard_cb                   (GossipResult            result,
							       GossipVCard            *vcard,
							       GossipAddContactDialog *dialog);
static gboolean add_contact_dialog_id_entry_focus_cb          (GtkWidget              *widget,
							       GdkEventFocus          *event,
							       GossipAddContactDialog *dialog);
static void     add_contact_dialog_account_chooser_changed_cb (GtkWidget              *account_chooser,
							       GossipAddContactDialog *dialog);
static void     add_contact_dialog_id_entry_changed_cb        (GtkEntry               *entry,
							       GossipAddContactDialog *dialog);
static gboolean add_contact_dialog_complete_group_idle        (GossipAddContactDialog *dialog);
static void     add_contact_dialog_entry_group_insert_text_cb (GtkEntry               *entry,
							       const gchar            *text,
							       gint                    length,
							       gint                   *position,
							       GossipAddContactDialog *dialog);
static void     add_contact_dialog_response_cb                (GtkDialog              *widget,
							       gint                    response,
							       GossipAddContactDialog *dialog);
static void     add_contact_dialog_destroy_cb                 (GtkWidget              *widget,
							       GossipAddContactDialog *dialog);

static GossipAddContactDialog *p = NULL;

static void
add_contact_dialog_vcard_cb (GossipResult            result,
			     GossipVCard            *vcard,
			     GossipAddContactDialog *dialog)
{
	gossip_debug (DEBUG_DOMAIN, "VCard response");

	/* If we get a callback for an old dialog, ignore it. */
	if (p != dialog) {
		return;
	}

	if (result != GOSSIP_RESULT_OK || 
	    (!gossip_vcard_get_name (vcard) &&
	     !gossip_vcard_get_email (vcard) &&
	     !gossip_vcard_get_country (vcard))) {
		gchar *str;

	    	gtk_widget_show (dialog->label_information);

		str = g_strdup_printf ("<b>%s</b>",
				       _("No information is available for this contact."));
		gtk_label_set_markup (GTK_LABEL (dialog->label_information), str);
		g_free (str);

	} else {
		GdkPixbuf   *pixbuf;
		const gchar *value = NULL;

		gtk_widget_hide (dialog->label_information);

		/* Name */
		value = gossip_vcard_get_name (vcard);
		if (value && strlen (value) > 0) {
			gtk_widget_show (dialog->label_name);
			gtk_widget_show (dialog->label_name_stub);
			gtk_label_set_text (GTK_LABEL (dialog->label_name), value);
			gtk_entry_set_text (GTK_ENTRY (dialog->entry_alias), value);
		}
		
		/* Email */
		value = gossip_vcard_get_email (vcard);
		if (value && strlen (value) > 0) {
			gtk_widget_show (dialog->label_email);
			gtk_widget_show (dialog->label_email_stub);
			gtk_label_set_text (GTK_LABEL (dialog->label_email), value);
		}
		
		/* Country */
		value = gossip_vcard_get_country (vcard);
		if (value && strlen (value) > 0) {
			gtk_widget_show (dialog->label_country);
			gtk_widget_show (dialog->label_country_stub);
			gtk_label_set_text (GTK_LABEL (dialog->label_country), value);
		}
		
		/* Avatar */
		pixbuf = gossip_pixbuf_avatar_from_vcard (vcard);
		if (pixbuf != NULL) {
			gossip_avatar_image_set_pixbuf (GOSSIP_AVATAR_IMAGE (dialog->avatar_image), 
							pixbuf);
			gtk_widget_show (dialog->avatar_image);
			g_object_unref (pixbuf);
		}
		
		gtk_widget_show (dialog->hbox_information);
		gtk_widget_show (dialog->table_information);
	}

	/* Select the alias entry */
	gtk_entry_select_region (GTK_ENTRY (dialog->entry_alias), 0, -1);
	gtk_widget_grab_focus (dialog->entry_alias);
}

static gboolean
add_contact_dialog_id_entry_focus_cb (GtkWidget              *widget,
				      GdkEventFocus          *event,
				      GossipAddContactDialog *dialog)
{
	GossipAccountChooser *account_chooser;
	GossipAccount        *account;
	GossipProtocol       *protocol;
	GossipContact        *contact;
	const gchar          *id;
	const gchar          *example;
	gchar                *str;
	gboolean              lookup = TRUE;

	/* We don't care about focus in, just focus out */
	if (event->in) {
		return FALSE;
	}

	/* Get protocol for example */
	account_chooser = GOSSIP_ACCOUNT_CHOOSER (dialog->account_chooser);
	account = gossip_account_chooser_get_account (account_chooser);
	protocol = gossip_session_get_protocol (gossip_app_get_session (), 
						account);

	/* Make sure we aren't looking up the same ID or the example */
	id = gtk_entry_get_text (GTK_ENTRY (dialog->entry_id));
	example = gossip_protocol_get_example_username (protocol);

	lookup &= strlen (id) > 0;

	if (example && strlen (example) > 0) {
		lookup &= strcmp (id, example) != 0;
	}

	if (dialog->last_id) {
		lookup &= strcmp (dialog->last_id, id) != 0;
	}

	if (!lookup) {
		return FALSE;
	}	

	/* Remember so we don't keep lookup the same ID */
	dialog->last_id = g_strdup (id);


	contact = gossip_contact_new (GOSSIP_CONTACT_TYPE_TEMPORARY, account);
	gossip_contact_set_id (contact, id);

	gossip_session_get_vcard (gossip_app_get_session (),
				  account,
				  contact,
				  (GossipVCardCallback) add_contact_dialog_vcard_cb,
				  dialog,
				  NULL);

	gtk_widget_hide_all (dialog->vbox_information);
	gtk_widget_show (dialog->vbox_information);
	gtk_widget_show (dialog->label_information);

	str = g_strdup_printf ("<b>%s</b>",
			       _("Information requested, please wait..."));
	gtk_label_set_markup (GTK_LABEL (dialog->label_information), str);
	g_free (str);

	g_object_unref (account);
	
	return FALSE;
}

static void
add_contact_dialog_account_chooser_changed_cb (GtkWidget              *account_chooser,
					       GossipAddContactDialog *dialog)
{
	GossipAccount  *account;
	GossipProtocol *protocol;
	const gchar    *example;

	account = gossip_account_chooser_get_account (GOSSIP_ACCOUNT_CHOOSER (account_chooser));
	protocol = gossip_session_get_protocol (gossip_app_get_session (), account);

	example = gossip_protocol_get_example_username (protocol);
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_id), example);
	
	g_object_unref (account);
}

static void
add_contact_dialog_id_entry_changed_cb (GtkEntry               *entry,
					GossipAddContactDialog *dialog)
{
	const gchar *id;

	id = gtk_entry_get_text (GTK_ENTRY (entry));
	gtk_widget_set_sensitive (dialog->button_add, id && strlen (id) > 0);
}

static gboolean
add_contact_dialog_complete_group_idle (GossipAddContactDialog *dialog)
{
	GtkEntry    *entry;
	const gchar *prefix;
	gchar       *new_prefix;
	gint         len;

	entry = GTK_ENTRY (dialog->entry_group);
	prefix = gtk_entry_get_text (entry);
	len = strlen (prefix);

	g_completion_complete (dialog->group_completion,
			       (gchar*) prefix,
			       &new_prefix);

	if (new_prefix) {
		g_signal_handlers_block_by_func (entry,
						 add_contact_dialog_entry_group_insert_text_cb,
						 dialog);

		gtk_entry_set_text (entry, new_prefix);

		g_signal_handlers_unblock_by_func (entry,
						   add_contact_dialog_entry_group_insert_text_cb,
						   dialog);

		gtk_editable_set_position (GTK_EDITABLE (entry), len);
		gtk_editable_select_region (GTK_EDITABLE (entry),
					    len, -1);
		g_free (new_prefix);
	}

	dialog->idle_complete = 0;

	return FALSE;
}

static void
add_contact_dialog_entry_group_insert_text_cb (GtkEntry               *entry,
					       const gchar            *text,
					       gint                    length,
					       gint                   *position,
					       GossipAddContactDialog *dialog)
{
	if (!dialog->idle_complete) {
		dialog->idle_complete =
			g_idle_add ((GSourceFunc) add_contact_dialog_complete_group_idle,
				    dialog);
	}
}

static void
add_contact_dialog_response_cb (GtkDialog              *widget,
				gint                    response,
				GossipAddContactDialog *dialog)
{
	if (response == RESPONSE_ADD) {
		GossipAccount        *account;
		GossipAccountChooser *account_chooser;
		const gchar          *id;
		const gchar          *name;
		const gchar          *group;
		const gchar          *message;

		message = _("I would like to add you to my contact list.");

		account_chooser = GOSSIP_ACCOUNT_CHOOSER (dialog->account_chooser);
		account = gossip_account_chooser_get_account (account_chooser);

		id = gtk_entry_get_text (GTK_ENTRY (dialog->entry_id));
		name = gtk_entry_get_text (GTK_ENTRY (dialog->entry_alias));
		group = gtk_entry_get_text (GTK_ENTRY (dialog->entry_group));

		gossip_session_add_contact (gossip_app_get_session (),
					    account,
					    id, name, group, message);

		g_object_unref (account);
	}

	gtk_widget_destroy (dialog->dialog);
}

static void
add_contact_dialog_destroy_cb (GtkWidget              *widget,
			       GossipAddContactDialog *dialog)
{
	if (dialog->idle_complete) {
		g_source_remove (dialog->idle_complete);
	}
	
	g_completion_free (dialog->group_completion);

	g_free (dialog);
}

void
gossip_add_contact_dialog_show (GtkWindow     *parent,
				GossipContact *contact)
{
	GossipAddContactDialog *dialog;
	GossipSession          *session;
	GladeXML               *glade;
	GList                  *accounts;
	GList                  *all_groups;
	GtkSizeGroup           *size_group;

	if (p) {
		gtk_window_present (GTK_WINDOW (p->dialog));
		return;
	}

	dialog = p = g_new0 (GossipAddContactDialog, 1);

	dialog->group_completion = g_completion_new (NULL);

	glade = gossip_glade_get_file (
		"main.glade",
		"add_contact_dialog",
		NULL,
		"add_contact_dialog", &dialog->dialog,
		"table_who", &dialog->table_who,
		"label_account", &dialog->label_account,
		"label_id", &dialog->label_id,
		"entry_id", &dialog->entry_id,
		"hbox_information", &dialog->hbox_information,
		"vbox_information", &dialog->vbox_information,
		"table_information", &dialog->table_information,
		"label_information", &dialog->label_information,
		"label_name_stub", &dialog->label_name_stub,
		"label_email_stub", &dialog->label_email_stub,
		"label_country_stub", &dialog->label_country_stub,
		"label_name", &dialog->label_name,
		"label_email", &dialog->label_email,
		"label_country", &dialog->label_country,
		"label_alias", &dialog->label_alias,
		"entry_alias", &dialog->entry_alias,
		"label_group", &dialog->label_group,
		"combo_group", &dialog->combo_group,
		"entry_group", &dialog->entry_group,
		"button_cancel", &dialog->button_cancel,
		"button_add", &dialog->button_add,
		NULL);

	gossip_glade_connect (
		glade, dialog,
		"add_contact_dialog", "destroy", add_contact_dialog_destroy_cb,
		"add_contact_dialog", "response", add_contact_dialog_response_cb,
		"entry_id", "focus-out-event", add_contact_dialog_id_entry_focus_cb,
		"entry_id", "changed", add_contact_dialog_id_entry_changed_cb,
		NULL);

	g_object_add_weak_pointer (G_OBJECT (dialog->dialog), (gpointer) &p);
	g_object_unref (glade);

	g_signal_connect_after (dialog->entry_group, "insert_text",
				G_CALLBACK (add_contact_dialog_entry_group_insert_text_cb),
				dialog);

	session = gossip_app_get_session ();

	/* Make the UI look puurrty :) */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_set_ignore_hidden (size_group, FALSE);
	gtk_size_group_add_widget (size_group, dialog->label_account);
	gtk_size_group_add_widget (size_group, dialog->label_id);
	gtk_size_group_add_widget (size_group, dialog->label_name_stub);
	gtk_size_group_add_widget (size_group, dialog->label_email_stub);
	gtk_size_group_add_widget (size_group, dialog->label_country_stub);
	gtk_size_group_add_widget (size_group, dialog->label_alias);
	gtk_size_group_add_widget (size_group, dialog->label_group);
	g_object_unref (size_group);

	/* Add our own customary widgets */
	dialog->account_chooser = gossip_account_chooser_new (session);
	gtk_table_attach_defaults (GTK_TABLE (dialog->table_who),
				   dialog->account_chooser,
				   1, 2, 0, 1);
	g_signal_connect (dialog->account_chooser, "changed",
			  G_CALLBACK (add_contact_dialog_account_chooser_changed_cb),
			  dialog);

	accounts = gossip_session_get_accounts (session);
	if (g_list_length (accounts) > 1) {
		gtk_widget_show (dialog->account_chooser);
	} else {
		gtk_widget_hide (dialog->label_account);
	}

	g_list_foreach (accounts, (GFunc)g_object_unref, NULL);
	g_list_free (accounts);

	dialog->avatar_image = gossip_avatar_image_new (NULL);
	gtk_box_pack_end (GTK_BOX (dialog->hbox_information),
			  dialog->avatar_image,
			  FALSE, TRUE, 0);

	/* Set up the contact if provided */
	if (contact) {
		GossipAccount        *account;
		GossipAccountChooser *account_chooser;

		account = gossip_contact_get_account (contact);
		account_chooser = GOSSIP_ACCOUNT_CHOOSER (dialog->account_chooser);
		gossip_account_chooser_set_account (account_chooser, account);

		gtk_entry_set_text (GTK_ENTRY (dialog->entry_id),
				    gossip_contact_get_id (contact));
	}

	/* Set up the groups already used */
	all_groups = gossip_session_get_groups (session);
	all_groups = g_list_sort (all_groups, (GCompareFunc) strcmp);

	if (all_groups) {
		gtk_combo_set_popdown_strings (GTK_COMBO (dialog->combo_group),
					       all_groups);
		g_completion_add_items (dialog->group_completion, all_groups);
	}

	/* Set focus to the entry */
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_group), "");
	gtk_entry_select_region (GTK_ENTRY (dialog->entry_id), 0, -1);

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog), parent);
	}

	gtk_widget_show (dialog->dialog);
}
