/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio HB
 * Copyright (C) 2003 Richard Hult <richard@imendio.com>
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
#include <loudmouth/loudmouth.h>
#include "gossip-account.h"
#include "gossip-utils.h"
#include "gossip-register.h"

typedef struct {
	LmConnection  *connection;
	GossipAccount *account;
	GtkWidget     *dialog;

	gchar         *password;
	
	gboolean       success;
	gchar         *error_message;
} RegisterAccountData;

static void
register_dialog_destroy_cb (GtkWidget           *widget,
			    RegisterAccountData *data)
{
	g_free (data->password);
	data->dialog = NULL;

	/*g_free (data); We leak this until we can cancel pending replies. */
}
	
static LmHandlerResult
register_register_handler (LmMessageHandler    *handler,
			   LmConnection        *connection,
			   LmMessage           *msg,
			   RegisterAccountData *data)
{
	LmMessageSubType  sub_type;
	LmMessageNode    *node;

	sub_type = lm_message_get_sub_type (msg);
	switch (sub_type) {
	case LM_MESSAGE_SUB_TYPE_RESULT:
		data->success = TRUE;

		if (data->dialog) {
			gtk_dialog_response (GTK_DIALOG (data->dialog),
					     GTK_RESPONSE_NONE);
		}
		
		break;

	case LM_MESSAGE_SUB_TYPE_ERROR:
	default:
		node = lm_message_node_find_child (msg->node, "error");
		if (node) {
			data->error_message = g_strdup (lm_message_node_get_value (node));
		} else {
			data->error_message = g_strdup (_("Unknown error"));
		}
		
		if (data->dialog) {
			gtk_dialog_response (GTK_DIALOG (data->dialog),
					     GTK_RESPONSE_NONE);
		}
		
		break;
	}

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}	

static void
register_connection_open_cb (LmConnection        *connection,
			     gboolean             result,
			     RegisterAccountData *data)
{
	GossipJID        *jid;
	LmMessage        *msg;
	LmMessageNode    *node;
	LmMessageHandler *handler;
	
	if (result != TRUE) {
		data->error_message = g_strdup ("Could not connect to the server.");

		if (data->dialog) {
			gtk_dialog_response (GTK_DIALOG (data->dialog),
					     GTK_RESPONSE_NONE);
		}
		return;
	}

	jid = gossip_account_get_jid (data->account);
	
	msg = lm_message_new_with_sub_type (gossip_jid_get_without_resource (jid),
					    LM_MESSAGE_TYPE_IQ,
					    LM_MESSAGE_SUB_TYPE_SET);
	
	node = lm_message_node_add_child (msg->node, "query", NULL);
	lm_message_node_set_attribute (node, "xmlns", "jabber:iq:register");
	
	lm_message_node_add_child (node, "username", gossip_jid_get_part_name (jid));
	lm_message_node_add_child (node, "password", data->password);

	handler = lm_message_handler_new ((LmHandleMessageFunction) register_register_handler,
					  data, NULL);

	lm_connection_send_with_reply (data->connection, msg, handler, NULL);
	lm_message_unref (msg);
	
	gossip_jid_unref (jid);
}

gboolean
gossip_register_account (GossipAccount *account,
			 GtkWindow     *parent)
{
	gchar               *password;
	GossipJID           *jid;
	RegisterAccountData *data;
	gint                 response;
	gboolean             retval;

	if (!account->password || !account->password[0]) {
		password = gossip_password_dialog_run (account, parent);

		if (!password) {
			return FALSE;
		}
	} else {
		password = g_strdup (account->password);
	}
	
	jid = gossip_account_get_jid (account);

	data = g_new0 (RegisterAccountData, 1);

	data->account = account;
	data->connection = lm_connection_new (account->server);
	data->password = password;
	
	data->dialog = gtk_message_dialog_new (parent,
					       GTK_DIALOG_MODAL |
					       GTK_DIALOG_DESTROY_WITH_PARENT,
					       GTK_MESSAGE_INFO,
					       GTK_BUTTONS_CANCEL,
					       "%s\n<b>%s</b>",
					       _("Registering account"),
					       gossip_jid_get_without_resource (jid));

	g_object_set (GTK_MESSAGE_DIALOG (data->dialog)->label,
		      "use-markup", TRUE,
		      "wrap", FALSE,
		      NULL);
	
	g_signal_connect (data->dialog,
			  "destroy",
			  G_CALLBACK (register_dialog_destroy_cb),
			  data);
	
	lm_connection_open (data->connection,
			    (LmResultFunction) register_connection_open_cb,
			    data, NULL, NULL);

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
						 gossip_jid_get_without_resource (jid));

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
					       gossip_jid_get_without_resource (jid),
					       _("Reason:"),
					       data->error_message);
		} else {
			str = g_strdup_printf ("%s\n<b>%s</b>",
					       _("Failed registering the account"),
					       gossip_jid_get_without_resource (jid));
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

	gossip_jid_unref (jid);
	
	gtk_widget_destroy (data->dialog);

	return retval;
}
