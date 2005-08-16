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
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-session.h>

#include "gossip-app.h"
#include "gossip-register.h"

typedef struct {
	GossipAccount *account;
	GtkWidget     *dialog;

	gboolean       success;
	gchar         *error_message;
} RegisterAccountData;

static void
register_dialog_destroy_cb (GtkWidget           *widget,
			    RegisterAccountData *data)
{
	g_object_unref (data->account);
	
	data->dialog = NULL;

	/*g_free (data); We leak this until we can cancel pending replies. */
}

static void
register_registration_done_cb (GossipResult   result, 
                               const gchar         *err_message,
                               RegisterAccountData *data)
{
	switch (result) {
        case GOSSIP_RESULT_OK:
                data->success = TRUE;

		if (data->dialog) {
                        gtk_dialog_response (GTK_DIALOG (data->dialog),
                                             GTK_RESPONSE_NONE);
                }
		
                break;
        case GOSSIP_RESULT_ERROR_REGISTRATION:
	default:
                data->error_message = g_strdup (err_message);
		
                if (data->dialog) {
			gtk_dialog_response (GTK_DIALOG (data->dialog),
					     GTK_RESPONSE_NONE);
		}
		
		break;
	}
}	

gboolean
gossip_register_account (GossipAccount *account,
			 GtkWindow     *parent)
{
	RegisterAccountData *data;
	gchar               *password;
	const gchar         *account_password;
	gint                 response;
	gboolean             retval;

	account_password = gossip_account_get_password (account);

	if (!account_password || !strlen (account_password) < 1) {
                password = gossip_password_dialog_run (account, parent);

		if (!password) {
			return FALSE;
		}
	} else {
                password = g_strdup (account_password);
	}

	data = g_new0 (RegisterAccountData, 1);

	data->account = g_object_ref (account);
	
	data->dialog = gtk_message_dialog_new (parent,
					       GTK_DIALOG_MODAL |
					       GTK_DIALOG_DESTROY_WITH_PARENT,
					       GTK_MESSAGE_INFO,
					       GTK_BUTTONS_CANCEL,
					       "%s\n<b>%s</b>",
					       _("Registering account"),
                                               gossip_account_get_id (account));

	g_object_set (GTK_MESSAGE_DIALOG (data->dialog)->label,
		      "use-markup", TRUE,
		      "wrap", FALSE,
		      NULL);
	
	g_signal_connect (data->dialog,
			  "destroy",
			  G_CALLBACK (register_dialog_destroy_cb),
			  data);
	
        gossip_session_register_account (gossip_app_get_session (),
                                       GOSSIP_ACCOUNT_TYPE_JABBER,
                                       account,
					 (GossipRegisterCallback) register_registration_done_cb,
                                       data, NULL);
        g_free (password);

	response = gtk_dialog_run (GTK_DIALOG (data->dialog));
	switch (response) {
	case GTK_RESPONSE_CANCEL:
		/* FIXME: cancel pending replies... */
		break;

	default:
		break;
	}

 	if (data->dialog) {
		gtk_widget_hide (data->dialog);
	}
	
	if (data->success) {
		GtkWidget *dialog;
		
		dialog = gtk_message_dialog_new (parent,
						 GTK_DIALOG_MODAL |
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_INFO,
						 GTK_BUTTONS_CLOSE,
						 "%s\n<b>%s</b>",
						 _("Successfully registered the account"),
                                                 gossip_account_get_id (account));

		g_object_set (GTK_MESSAGE_DIALOG (dialog)->label,
			      "use-markup", TRUE,
			      "wrap", FALSE,
			      NULL);
		
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	
		/* Add account information */
		gossip_accounts_store (data->account);

		retval = TRUE;
	} else {
		GtkWidget *dialog;
		gchar     *str;

		if (data->error_message) {
			str = g_strdup_printf ("%s\n<b>%s</b>\n\n%s\n%s",
					       _("Failed registering the account"),
					       gossip_account_get_id (account),
					       _("Reason:"),
					       data->error_message);
		} else {
			str = g_strdup_printf ("%s\n<b>%s</b>",
					       _("Failed registering the account"),
                                               gossip_account_get_id (account));
		}

		dialog = gtk_message_dialog_new (parent,
						 GTK_DIALOG_MODAL |
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 str);
		g_free (str);

		g_object_set (GTK_MESSAGE_DIALOG (dialog)->label,
			      "use-markup", TRUE,
			      "wrap", FALSE,
			      NULL);
		
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_free (data->error_message);

		retval = FALSE;
	}		

	gtk_widget_destroy (data->dialog);

	return retval;
}
