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
#include <libgnome/gnome-i18n.h>

#include "gossip-account.h"
#include "gossip-app.h"
#include "gossip-session.h"
#include "gossip-utils.h"
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
	data->dialog = NULL;

	/*g_free (data); We leak this until we can cancel pending replies. */
}

static void
register_registration_done_cb (GossipAsyncResult   result, 
                               const gchar         *err_message,
                               RegisterAccountData *data)
{
	switch (result) {
        case GOSSIP_ASYNC_OK:
                data->success = TRUE;

		if (data->dialog) {
                        gtk_dialog_response (GTK_DIALOG (data->dialog),
                                             GTK_RESPONSE_NONE);
                }
		
                break;
        case GOSSIP_ASYNC_ERROR_REGISTRATION:
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
	gint                 response;
	gboolean             retval;
        const gchar         *id;

	if (!account->password || !account->password[0]) {
                password = gossip_password_dialog_run (account, parent);

		if (!password) {
			return FALSE;
		}
	} else {
                password = g_strdup (account->password);
	}

	data = g_new0 (RegisterAccountData, 1);

        id = gossip_jid_get_without_resource (account->jid);
	data->account = account;
	
	data->dialog = gtk_message_dialog_new (parent,
					       GTK_DIALOG_MODAL |
					       GTK_DIALOG_DESTROY_WITH_PARENT,
					       GTK_MESSAGE_INFO,
					       GTK_BUTTONS_CANCEL,
					       "%s\n<b>%s</b>",
					       _("Registering account"),
                                               id);

	g_object_set (GTK_MESSAGE_DIALOG (data->dialog)->label,
		      "use-markup", TRUE,
		      "wrap", FALSE,
		      NULL);
	
	g_signal_connect (data->dialog,
			  "destroy",
			  G_CALLBACK (register_dialog_destroy_cb),
			  data);
	
        gossip_session_async_register (gossip_app_get_session (),
                                       GOSSIP_ACCOUNT_TYPE_JABBER,
                                       account,
                                       (GossipAsyncRegisterCallback) register_registration_done_cb,
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
                                                 id);

		g_object_set (GTK_MESSAGE_DIALOG (dialog)->label,
			      "use-markup", TRUE,
			      "wrap", FALSE,
			      NULL);
		
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	
		/* Add account information */
		gossip_account_store (data->account, NULL);

		retval = TRUE;
	} else {
		GtkWidget *dialog;
		gchar     *str;

		if (data->error_message) {
			str = g_strdup_printf ("%s\n<b>%s</b>\n\n%s\n%s",
					       _("Failed registering the account"),
                                               id,
					       _("Reason:"),
					       data->error_message);
		} else {
			str = g_strdup_printf ("%s\n<b>%s</b>",
					       _("Failed registering the account"),
                                               id);
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
