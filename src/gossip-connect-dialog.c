/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2003 Imendio HB
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
#include <libgnome/gnome-config.h>
#include <loudmouth/loudmouth.h>
#include "gossip-utils.h"
#include "gossip-app.h"
#include "gossip-connect-dialog.h"
#include "gossip-account-dialog.h"
#include "gossip-account.h"

#define RESPONSE_CONNECT 1
#define RESPONSE_EDIT 2

typedef struct {
	GtkWidget *dialog;
	GtkLabel  *server_label;
	GtkEntry  *resource_entry;

	GtkWidget *account_dialog;
} GossipConnectDialog;


static void connect_dialog_destroy_cb  (GtkWidget            *widget,
					GossipConnectDialog **dialog);
static void connect_dialog_response_cb (GtkWidget            *widget,
					gint                  response,
					GossipConnectDialog  *dialog);


static void
account_dialog_destroy_cb (GtkWidget           *widget,
			   GossipConnectDialog *dialog)
{
	GossipAccount *account;

	dialog->account_dialog = NULL;

	account = gossip_account_get_default ();
	gtk_label_set_text (GTK_LABEL (dialog->server_label), account->server);
	gtk_entry_set_text (GTK_ENTRY (dialog->resource_entry), 
			    gossip_jid_get_resource (account->jid));
	gossip_account_unref (account);
}

static void
connect_dialog_destroy_cb (GtkWidget            *widget,
			   GossipConnectDialog **dialog_ptr)
{
	GossipConnectDialog *dialog = *dialog_ptr;

	if (dialog->account_dialog) {
		g_signal_handlers_disconnect_by_func (dialog->account_dialog,
						      account_dialog_destroy_cb,
						      dialog);
		dialog->account_dialog = NULL;
	}
	
	g_free (dialog);
	*dialog_ptr = NULL;
}

static void
connect_dialog_response_cb (GtkWidget           *widget,
			    gint                 response,
			    GossipConnectDialog *dialog)
{
	const gchar *resource;
	GtkWidget   *window;

	switch (response) {
	case RESPONSE_CONNECT:
		resource = gtk_entry_get_text (dialog->resource_entry);
		gossip_app_set_overridden_resource (resource);
		gossip_app_connect ();
		break;

	case RESPONSE_EDIT:
		window = gossip_account_dialog_show ();

		if (!dialog->account_dialog) {
			dialog->account_dialog = window;

			g_signal_connect (window,
					  "destroy",
					  G_CALLBACK (account_dialog_destroy_cb),
					  dialog);
		}
		return;
		
	default:
		break;
	}

	gtk_widget_destroy (widget);
}

void
gossip_connect_dialog_show (GossipApp *app)
{
	static GossipConnectDialog *dialog = NULL;
	GladeXML                   *gui;
	GossipAccount              *account;

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->dialog));
		return;
	}
	
        dialog = g_new0 (GossipConnectDialog, 1);

	gui = gossip_glade_get_file (GLADEDIR "/connect.glade",
				     "connect_dialog",
				     NULL,
				     "connect_dialog", &dialog->dialog,
				     "server_label", &dialog->server_label,
				     "resource_entry", &dialog->resource_entry,
				     NULL);

	account = gossip_account_get_default ();
	gtk_label_set_text (GTK_LABEL (dialog->server_label), account->server);
	gtk_entry_set_text (GTK_ENTRY (dialog->resource_entry), 
			    gossip_jid_get_resource (account->jid));
	gossip_account_unref (account);
	
	g_signal_connect (dialog->dialog,
			  "response",
			  G_CALLBACK (connect_dialog_response_cb),
			  dialog);
	
	g_signal_connect (dialog->dialog,
			  "destroy",
			  G_CALLBACK (connect_dialog_destroy_cb),
			  &dialog);
	
	gossip_glade_setup_size_group (gui,
				       GTK_SIZE_GROUP_HORIZONTAL,
				       "server_label_label",
				       "resource_label",
				       /*"priority_label",*/
				       NULL);

	gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog), GTK_WINDOW (gossip_app_get_window ()));
	gtk_widget_show (dialog->dialog);
}



