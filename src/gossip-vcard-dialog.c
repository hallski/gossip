/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Martyn Russell <ginxd@btopenworld.com>
 * Copyright (C) 2004 Imendio AB
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
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-href.h>
#include <loudmouth/loudmouth.h>
#include <unistd.h>

#include "gossip-app.h"
#include "gossip-session.h"
#include "gossip-vcard.h"
#include "gossip-vcard-dialog.h"
#include "gossip-transport-accounts.h"
#include "gossip-transport-register.h"

#define d(x) x

struct _GossipVCardDialog {
	GtkWidget        *dialog;

	GtkWidget        *label_status;

	GtkWidget        *vbox_personal_information;
	GtkWidget        *vbox_description;

	GtkWidget        *entry_name;
	GtkWidget        *entry_nickname;
	GtkWidget        *entry_web_site;
	GtkWidget        *entry_email;

	GtkWidget        *textview_description;

	gboolean          set_vcard;
	gboolean          set_msn_nick;
};

typedef struct _GossipVCardDialog GossipVCardDialog;


static void vcard_dialog_get_vcard_cb            (GossipAsyncResult  result,
						  GossipVCard       *vcard,
						  GossipVCardDialog *dialog);
static void vcard_dialog_set_vcard               (GossipVCardDialog *dialog);
static void vcard_dialog_set_vcard_cb            (GossipAsyncResult  result,
						  GossipVCardDialog *dialog);
static void vcard_dialog_set_msn_nick            (GossipVCardDialog *dialog);
static void vcard_dialog_set_msn_nick_details_cb (GossipJID         *jid,
						  const gchar       *key,
						  const gchar       *username,
						  const gchar       *password,
						  const gchar       *nick,
						  const gchar       *email,
						  gboolean           require_username,
						  gboolean           require_password,
						  gboolean           require_nick,
						  gboolean           require_email,
						  gboolean           is_registered,
						  const gchar       *error_code,
						  const gchar       *error_reason,
						  GossipVCardDialog *dialog);
static void vcard_dialog_set_msn_nick_done_cb    (const gchar       *error_code,
						  const gchar       *error_reason,
						  GossipVCardDialog *dialog);
static void vcard_dialog_check_all_set           (GossipVCardDialog *dialog);
static void vcard_dialog_response_cb             (GtkDialog         *widget,
						  gint               response,
						  GossipVCardDialog *dialog);
static void vcard_dialog_destroy_cb              (GtkWidget         *widget,
						  GossipVCardDialog *dialog);


void
gossip_vcard_dialog_show (void)
{
	GossipVCardDialog *dialog;
	GladeXML          *gui;

	dialog = g_new0 (GossipVCardDialog, 1);

	gui = gossip_glade_get_file (GLADEDIR "/main.glade",
				     "vcard_dialog",
				     NULL,
				     "vcard_dialog", &dialog->dialog,
				     "label_status", &dialog->label_status,
				     "vbox_personal_information", &dialog->vbox_personal_information,
				     "vbox_description", &dialog->vbox_description,
				     "entry_name", &dialog->entry_name,
				     "entry_nickname", &dialog->entry_nickname,
				     "entry_web_site", &dialog->entry_web_site,
				     "entry_email", &dialog->entry_email,
				     "textview_description", &dialog->textview_description,
				     NULL);

	gossip_glade_connect (gui, 
			      dialog,
			      "vcard_dialog", "destroy", vcard_dialog_destroy_cb,
			      "vcard_dialog", "response", vcard_dialog_response_cb,
			      NULL);

	g_object_unref (gui);
	
	/* request current vCard */
	gossip_session_async_get_vcard (gossip_app_get_session (),
					NULL,
					(GossipAsyncVCardCallback) vcard_dialog_get_vcard_cb,
					dialog,
					NULL);
}

static void
vcard_dialog_get_vcard_cb (GossipAsyncResult  result,
			   GossipVCard       *vcard,
			   GossipVCardDialog *dialog)
{
	GtkTextBuffer *buffer;

	d(g_print ("Got a vCard response\n"));

	gtk_widget_hide (dialog->label_status);
	gtk_widget_set_sensitive (dialog->vbox_personal_information, TRUE);
	gtk_widget_set_sensitive (dialog->vbox_description, TRUE);

	if (result != GOSSIP_ASYNC_OK) {
		d(g_print ("vCard result != GOSSIP_ASYNC_OK\n"));
		return;
	}

	gtk_entry_set_text (GTK_ENTRY (dialog->entry_name),
			    gossip_vcard_get_name (vcard));
		
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_nickname),
			    gossip_vcard_get_nickname (vcard));

	gtk_entry_set_text (GTK_ENTRY (dialog->entry_email),
			    gossip_vcard_get_email (vcard));
		
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_web_site),
			    gossip_vcard_get_url (vcard));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->textview_description));
	gtk_text_buffer_set_text (buffer,
				  gossip_vcard_get_description (vcard),
				  -1);
	g_object_unref (vcard);
}

static void
vcard_dialog_set_msn_nick (GossipVCardDialog *dialog)
{
	GossipTransportAccount *account;
	GossipJID              *jid;

	const gchar            *nickname;

	nickname = gtk_entry_get_text (GTK_ENTRY (dialog->entry_nickname));

	if (!nickname || g_utf8_strlen (nickname, -1) < 1) {
		d(g_print ("Nickname not set, no need to configure an the MSN nickname\n"));
		dialog->set_msn_nick = TRUE;
		return;	
	}

#ifdef FIXME_MJR
	account = gossip_transport_account_find_by_disco_type (al, "msn");
	if (!account) {
		d(g_print ("No MSN account, no need to configure an the MSN nickname\n"));
		dialog->set_msn_nick = TRUE;
		return;
	}

	jid = gossip_transport_account_get_jid (account);
#else
	account = NULL;
	jid = NULL;
#endif

	gossip_transport_requirements (NULL, 
				       jid,
				       (GossipTransportRequirementsFunc) vcard_dialog_set_msn_nick_details_cb,
				       dialog);
}

static void  
vcard_dialog_set_msn_nick_details_cb (GossipJID         *jid,
				      const gchar       *key,
				      const gchar       *username,
				      const gchar       *password,
				      const gchar       *nick,
				      const gchar       *email,
				      gboolean           require_username,
				      gboolean           require_password,
				      gboolean           require_nick,
				      gboolean           require_email,
				      gboolean           is_registered,
				      const gchar       *error_code,
				      const gchar       *error_reason,
				      GossipVCardDialog *dialog)
{
	const gchar *nickname;

	nickname = gtk_entry_get_text (GTK_ENTRY (dialog->entry_nickname));

	d(g_print ("Setting MSN nickname to %s, waiting for response...\n", nickname));

	gossip_transport_register (NULL,
				   jid, 
				   key, 
				   username,
				   password, 
				   nickname, 
				   email,
				   (GossipTransportRegisterFunc) vcard_dialog_set_msn_nick_done_cb,
				   dialog);
}

static void
vcard_dialog_set_msn_nick_done_cb (const gchar       *error_code,
				   const gchar       *error_reason,
				   GossipVCardDialog *dialog)
{
	d(g_print ("Setting MSN nickname complete.\n"));

	dialog->set_msn_nick = TRUE;
	vcard_dialog_check_all_set (dialog);
}

static void
vcard_dialog_set_vcard (GossipVCardDialog *dialog)
{
	GError        *error = NULL;
	gchar         *description;
	gchar         *str;
	GtkTextBuffer *buffer;
	GtkTextIter    iter_begin, iter_end;
	GossipVCard   *vcard;

	if (!gossip_app_is_connected ()) {
		d(g_print ("Not connected, not setting vCard\n"));
		return;
	}

	vcard = gossip_vcard_new ();

	gossip_vcard_set_name (vcard,
			       gtk_entry_get_text (GTK_ENTRY (dialog->entry_name)));
	gossip_vcard_set_nickname (vcard,
				   gtk_entry_get_text (GTK_ENTRY (dialog->entry_nickname)));
	gossip_vcard_set_url (vcard,
			      gtk_entry_get_text (GTK_ENTRY (dialog->entry_web_site)));
	gossip_vcard_set_email (vcard,
				gtk_entry_get_text (GTK_ENTRY (dialog->entry_email)));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->textview_description));
	gtk_text_buffer_get_bounds (buffer, &iter_begin, &iter_end);
	description = gtk_text_buffer_get_text (buffer, &iter_begin, &iter_end, FALSE);
	gossip_vcard_set_description (vcard, description);
	g_free (description);

	str = g_strdup_printf ("<b>%s</b>", _("Saving personal details, please wait..."));
	gtk_label_set_markup (GTK_LABEL (dialog->label_status), str);
	gtk_widget_show (dialog->label_status);
	g_free (str);

	gtk_widget_set_sensitive (dialog->vbox_personal_information, FALSE);
	gtk_widget_set_sensitive (dialog->vbox_description, FALSE);

	gossip_session_async_set_vcard (gossip_app_get_session (),
					vcard, 
					(GossipAsyncResultCallback) vcard_dialog_set_vcard_cb,
					dialog, &error);
}

static void
vcard_dialog_set_vcard_cb (GossipAsyncResult result, GossipVCardDialog *dialog)
{
  
  d(g_print ("Got a vCard response\n"));
  
  /* FIXME: need to put some sort of error checking in here */

	dialog->set_vcard = TRUE;
	vcard_dialog_check_all_set (dialog);
}

static void
vcard_dialog_check_all_set (GossipVCardDialog *dialog)
{
	g_return_if_fail (dialog != NULL);

	if (dialog->set_vcard &&
	    dialog->set_msn_nick) {
		gtk_widget_destroy (dialog->dialog);
	}
}

static void
vcard_dialog_response_cb (GtkDialog *widget, gint response, GossipVCardDialog *dialog)
{
	/* save vcard */
	if (response == GTK_RESPONSE_OK) {
		vcard_dialog_set_msn_nick (dialog);
		vcard_dialog_set_vcard (dialog);
		return;
	}

	gtk_widget_destroy (dialog->dialog);
}

static void
vcard_dialog_destroy_cb (GtkWidget *widget, GossipVCardDialog *dialog)
{
	g_free (dialog);
}
