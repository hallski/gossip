/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2006 Imendio AB
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
#include <libgnomeui/gnome-href.h>
#include <loudmouth/loudmouth.h>
#include <unistd.h>

#include <libgossip/gossip-session.h>
#include <libgossip/gossip-protocol.h>
#include <libgossip/gossip-vcard.h>

#include "gossip-account-chooser.h"
#include "gossip-app.h"
#include "gossip-vcard-dialog.h"

/* #define DEBUG_MSG(x)   */
#define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n"); 

#define STRING_EMPTY(x) ((x) == NULL || (x)[0] == '\0')

#define VCARD_TIMEOUT    20000
#define SAVED_TIMEOUT    10000

#define RESPONSE_SAVE    1

typedef struct {
	GtkWidget *dialog;

	GtkWidget *hbox_account;
	GtkWidget *label_account;
	GtkWidget *account_chooser;

	GtkWidget *table_vcard;

	GtkWidget *label_name;
	GtkWidget *label_nickname;
	GtkWidget *label_web_site;
	GtkWidget *label_email;
	GtkWidget *label_description;

	GtkWidget *entry_name;
	GtkWidget *entry_nickname;
	GtkWidget *entry_web_site;
	GtkWidget *entry_email;

	GtkWidget *textview_description;

	GtkWidget *button_cancel;
	GtkWidget *button_save;

	guint      timeout_id; 
	guint      saved_id;

	gboolean   requesting_vcard;

	gint       last_account_selected;
} GossipVCardDialog;

static void     vcard_dialog_set_account_to_last (GossipVCardDialog *dialog);
static void     vcard_dialog_lookup_start        (GossipVCardDialog *dialog);
static void     vcard_dialog_lookup_stop         (GossipVCardDialog *dialog);
static void     vcard_dialog_get_vcard_cb        (GossipResult       result,
						  GossipVCard       *vcard,
						  GossipVCardDialog *dialog);
static void     vcard_dialog_set_vcard           (GossipVCardDialog *dialog);
static void     vcard_dialog_set_vcard_cb        (GossipResult       result,
						  GossipVCardDialog *dialog);
static gboolean vcard_dialog_timeout_cb          (GossipVCardDialog *dialog);
static void     vcard_dialog_account_changed_cb  (GtkWidget         *combo_box,
						  GossipVCardDialog *dialog);
static void     vcard_dialog_response_cb         (GtkDialog         *widget,
						  gint               response,
						  GossipVCardDialog *dialog);
static void     vcard_dialog_destroy_cb          (GtkWidget         *widget,
						  GossipVCardDialog *dialog);

static void
vcard_dialog_set_account_to_last (GossipVCardDialog *dialog) 
{
	g_signal_handlers_block_by_func (dialog->account_chooser, 
					 vcard_dialog_account_changed_cb, 
					 dialog);
		
	gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->account_chooser), 
				  dialog->last_account_selected);

	g_signal_handlers_unblock_by_func (dialog->account_chooser, 
					   vcard_dialog_account_changed_cb, 
					   dialog);
}

static void
vcard_dialog_lookup_start (GossipVCardDialog *dialog) 
{
	GossipSession        *session;
	GossipAccount        *account;
	GossipAccountChooser *account_chooser;

	gtk_widget_set_sensitive (dialog->table_vcard, FALSE);
	gtk_widget_set_sensitive (dialog->account_chooser, FALSE);
	gtk_widget_set_sensitive (dialog->button_save, FALSE);

	dialog->timeout_id = g_timeout_add (VCARD_TIMEOUT, 
					    (GSourceFunc)vcard_dialog_timeout_cb,
					    dialog);

	session = gossip_app_get_session ();
	account_chooser = GOSSIP_ACCOUNT_CHOOSER (dialog->account_chooser);
	account = gossip_account_chooser_get_account (account_chooser);

	gossip_session_get_vcard (session,
				  account,
				  NULL,
				  (GossipVCardCallback) vcard_dialog_get_vcard_cb,
				  dialog,
				  NULL);

	dialog->requesting_vcard = TRUE;

	g_object_unref (account);
}

static void  
vcard_dialog_lookup_stop (GossipVCardDialog *dialog)
{
	dialog->requesting_vcard = FALSE;

	gtk_widget_set_sensitive (dialog->table_vcard, TRUE);
	gtk_widget_set_sensitive (dialog->account_chooser, TRUE);
	gtk_widget_set_sensitive (dialog->button_save, TRUE);

	if (dialog->timeout_id != 0) {
		g_source_remove (dialog->timeout_id);
		dialog->timeout_id = 0;
	}

	if (dialog->saved_id != 0) {
		g_source_remove (dialog->saved_id);
		dialog->saved_id = 0;
	}
}

static void
vcard_dialog_get_vcard_cb (GossipResult       result,
			   GossipVCard       *vcard,
			   GossipVCardDialog *dialog)
{
	GtkComboBox   *combo_box;
	GtkTextBuffer *buffer;
	const gchar   *str;

	DEBUG_MSG (("VCardDialog: Got a VCard response"));

	vcard_dialog_lookup_stop (dialog);

	if (result != GOSSIP_RESULT_OK) {
		DEBUG_MSG (("VCardDialog: VCard result was not good"));
		return;
	}

	str = gossip_vcard_get_name (vcard);
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_name), STRING_EMPTY (str) ? "" : str);
		
	str = gossip_vcard_get_nickname (vcard);
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_nickname), STRING_EMPTY (str) ? "" : str);

	str = gossip_vcard_get_email (vcard);
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_email), STRING_EMPTY (str) ? "" : str);

	str = gossip_vcard_get_url (vcard);
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_web_site), STRING_EMPTY (str) ? "" : str);

	str = gossip_vcard_get_description (vcard);
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->textview_description));
	gtk_text_buffer_set_text (buffer, STRING_EMPTY (str) ? "" : str, -1);

	/* Save position incase the next lookup fails. */
	combo_box = GTK_COMBO_BOX (dialog->account_chooser);
	dialog->last_account_selected = gtk_combo_box_get_active (combo_box);
}

static void
vcard_dialog_set_vcard (GossipVCardDialog *dialog)
{
	GossipVCard          *vcard;
	GossipAccount        *account;
	GossipAccountChooser *account_chooser;
	GError               *error = NULL;
	GtkTextBuffer        *buffer;
	GtkTextIter           iter_begin, iter_end;
	gchar                *description;
	const gchar          *str;

	if (!gossip_app_is_connected ()) {
		DEBUG_MSG (("VCardDialog: Not connected, not setting VCard"));
		return;
	}

	vcard = gossip_vcard_new ();

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_name));
	gossip_vcard_set_name (vcard, str);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_nickname));
	gossip_vcard_set_nickname (vcard, str);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_web_site));
	gossip_vcard_set_url (vcard, str);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_email));
	gossip_vcard_set_email (vcard, str);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->textview_description));
	gtk_text_buffer_get_bounds (buffer, &iter_begin, &iter_end);
	description = gtk_text_buffer_get_text (buffer, &iter_begin, &iter_end, FALSE);
	gossip_vcard_set_description (vcard, description);
	g_free (description);

	/* NOTE: if account is NULL, all accounts will get the same vcard */
	account_chooser = GOSSIP_ACCOUNT_CHOOSER (dialog->account_chooser);
	account = gossip_account_chooser_get_account (account_chooser);

	gossip_session_set_vcard (gossip_app_get_session (),
				  account,
				  vcard, 
				  (GossipResultCallback) vcard_dialog_set_vcard_cb,
				  dialog, &error);

	g_object_unref (account);
}

static void
vcard_dialog_set_vcard_cb (GossipResult       result, 
			   GossipVCardDialog *dialog)
{
	DEBUG_MSG (("VCardDialog: Got a VCard response"));
  
	gtk_widget_destroy (dialog->dialog);
}

static gboolean
vcard_dialog_timeout_cb (GossipVCardDialog *dialog)
{
	GtkWidget *md;

	vcard_dialog_lookup_stop (dialog);

	/* select last successfull account */
	vcard_dialog_set_account_to_last (dialog);

	/* show message dialog and the account dialog */
	md = gtk_message_dialog_new_with_markup (GTK_WINDOW (dialog->dialog),
						 GTK_DIALOG_MODAL |
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_INFO,
						 GTK_BUTTONS_CLOSE,
						 "<b>%s</b>\n\n%s",
						 _("The server does not seem to be responding."),
						 _("Try again later."));
	
	g_signal_connect_swapped (md, "response",
				  G_CALLBACK (gtk_widget_destroy), md);
	gtk_widget_show (md);
	
	return FALSE;
}

static void
vcard_dialog_account_changed_cb (GtkWidget         *combo_box,
				 GossipVCardDialog *dialog)
{
	vcard_dialog_lookup_start (dialog);
}

static void
vcard_dialog_response_cb (GtkDialog         *widget, 
			  gint               response, 
			  GossipVCardDialog *dialog)
{
	switch (response) {
	case RESPONSE_SAVE:
		vcard_dialog_set_vcard (dialog);
		return;

	case GTK_RESPONSE_CANCEL:
		if (dialog->requesting_vcard) {
			/* change widgets so they are unsensitive */
			vcard_dialog_lookup_stop (dialog);
			
			/* select last successfull account */
			vcard_dialog_set_account_to_last (dialog);
			return;
		}

	default:
		break;
	}

	gtk_widget_destroy (dialog->dialog);
}

static void
vcard_dialog_destroy_cb (GtkWidget         *widget, 
			 GossipVCardDialog *dialog)
{
	vcard_dialog_lookup_stop (dialog);

	g_free (dialog);
}

void
gossip_vcard_dialog_show (GtkWindow *parent)
{
	GossipSession     *session;
	GossipVCardDialog *dialog;
	GladeXML          *glade;
	GList             *accounts;
	GtkSizeGroup      *size_group;

	dialog = g_new0 (GossipVCardDialog, 1);

	glade = gossip_glade_get_file (GLADEDIR "/main.glade",
				       "vcard_dialog",
				       NULL,
				       "vcard_dialog", &dialog->dialog,
				       "hbox_account", &dialog->hbox_account,
				       "label_account", &dialog->label_account,
				       "table_vcard", &dialog->table_vcard,
				       "label_name", &dialog->label_name,
				       "label_nickname", &dialog->label_nickname,
				       "label_web_site", &dialog->label_web_site,
				       "label_email", &dialog->label_email,
				       "label_description", &dialog->label_description,
				       "entry_name", &dialog->entry_name,
				       "entry_nickname", &dialog->entry_nickname,
				       "entry_web_site", &dialog->entry_web_site,
				       "entry_email", &dialog->entry_email,
				       "textview_description", &dialog->textview_description,
				       "button_cancel", &dialog->button_cancel,
				       "button_save", &dialog->button_save,
				       NULL);

	gossip_glade_connect (glade, 
			      dialog,
			      "vcard_dialog", "destroy", vcard_dialog_destroy_cb,
			      "vcard_dialog", "response", vcard_dialog_response_cb,
			      NULL);

	g_object_unref (glade);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, dialog->label_account);
	gtk_size_group_add_widget (size_group, dialog->label_name);
	gtk_size_group_add_widget (size_group, dialog->label_nickname);
	gtk_size_group_add_widget (size_group, dialog->label_email);
	gtk_size_group_add_widget (size_group, dialog->label_web_site);
	gtk_size_group_add_widget (size_group, dialog->label_description);

	g_object_unref (size_group);

	/* sort out accounts */
	session = gossip_app_get_session ();

	dialog->account_chooser = gossip_account_chooser_new (session);
	gtk_box_pack_start (GTK_BOX (dialog->hbox_account), 
			    dialog->account_chooser,
			    TRUE, TRUE, 0);

	g_signal_connect (dialog->account_chooser, "changed",
			  G_CALLBACK (vcard_dialog_account_changed_cb),
			  dialog);

	gtk_widget_show (dialog->account_chooser);

	/* select first */
	accounts = gossip_session_get_accounts (session);
	if (g_list_length (accounts) > 1) {
		gtk_widget_show (dialog->hbox_account);
	} else {
		/* show no accounts combo box */	
		gtk_widget_hide (dialog->hbox_account);
	}

	g_list_foreach (accounts, (GFunc)g_object_unref, NULL);
	g_list_free (accounts);

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog), parent); 
	}

	gtk_widget_show (dialog->dialog);
	
	vcard_dialog_lookup_start (dialog);
}
