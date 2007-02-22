/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Imendio AB
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
 * 
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-chatroom.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-utils.h>
#include <libgossip/gossip-chatroom-provider.h>

#include "gossip-app.h"
#include "gossip-chat-invite.h"
#include "gossip-ui-utils.h"

typedef struct {
	GossipContact    *contact;
	GossipChatroomId  chatroom_id;

	GtkWidget        *window;
	GtkWidget        *treeview;
	GtkWidget        *label;
	GtkWidget        *entry;
	GtkWidget        *button;
} GossipChatInviteDialog;

enum {
	COL_NAME,
	COL_CHATROOM_ID,
	COL_CONTACT,
	COL_COUNT
};

static void chat_invite_dialog_model_populate_columns     (GossipChatInviteDialog *dialog);
static void chat_invite_dialog_model_populate_suggestions (GossipChatInviteDialog *dialog);
static void chat_invite_dialog_model_row_activated_cb     (GtkTreeView            *tree_view,
							   GtkTreePath            *path,
							   GtkTreeViewColumn      *column,
							   GossipChatInviteDialog *dialog);
static void chat_invite_dialog_model_selection_changed_cb (GtkTreeSelection       *treeselection,
							   GossipChatInviteDialog *dialog);
static void chat_invite_dialog_model_setup                (GossipChatInviteDialog *dialog);
static void chat_invite_dialog_response_cb                (GtkWidget              *widget,
							   gint                    response,
							   GossipChatInviteDialog *dialog);
static void chat_invite_dialog_destroy_cb                 (GtkWidget              *widget,
							   GossipChatInviteDialog *dialog);

static void
chat_invite_dialog_model_populate_columns (GossipChatInviteDialog *dialog)
{
	GtkTreeView       *view;
	GtkTreeModel      *model;
	GtkTreeViewColumn *column;
	GtkCellRenderer   *renderer;
	guint              col_offset;

	view = GTK_TREE_VIEW (dialog->treeview);
	model = gtk_tree_view_get_model (view);

	renderer = gtk_cell_renderer_text_new ();
	col_offset = gtk_tree_view_insert_column_with_attributes (view,
								  -1, _("Word"),
								  renderer,
								  "text", COL_NAME,
								  NULL);

	g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (COL_NAME));

	column = gtk_tree_view_get_column (GTK_TREE_VIEW (dialog->treeview), col_offset - 1);
	gtk_tree_view_column_set_sort_column_id (column, COL_NAME);
	gtk_tree_view_column_set_resizable (column, FALSE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);
}

static void
chat_invite_dialog_model_populate_suggestions (GossipChatInviteDialog *dialog)
{
	GtkTreeView            *view;
	GtkListStore           *store;
	GtkTreeIter             iter;

	GossipSession          *session;
	GossipAccount          *account;
	GossipChatroomProvider *provider;
	GList                  *list = NULL;
	GList                  *l;

	session = gossip_app_get_session ();
	account = gossip_contact_get_account (dialog->contact);
	provider = gossip_session_get_chatroom_provider (session, account);

	if (dialog->chatroom_id != 0) {
		list = gossip_session_get_contacts_by_account (session, account);

		if (!list || g_list_length (list) < 1) {
			g_list_free (list);
			return;
		}

		view = GTK_TREE_VIEW (dialog->treeview);
		store = GTK_LIST_STORE (gtk_tree_view_get_model (view));

		for (l = list; l; l = l->next) {
			GossipContact *contact = l->data;
			const gchar   *name;

			if (!gossip_contact_is_online (contact)) {
				continue;
			}

			name = gossip_contact_get_name (contact);
			if (G_STR_EMPTY (name)) {
				continue;
			}

			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter,
					    COL_NAME, name,
					    COL_CONTACT, contact,
					    -1);
		}
	} else {
		list = gossip_chatroom_provider_get_rooms (provider);

		if (!list || g_list_length (list) < 1) {
			g_list_free (list);
			return;
		}

		view = GTK_TREE_VIEW (dialog->treeview);
		store = GTK_LIST_STORE (gtk_tree_view_get_model (view));

		for (l = list; l; l = l->next) {
			GossipChatroomId  id;
			GossipChatroom   *chatroom;
			const gchar      *name;

			id = GPOINTER_TO_INT(l->data);
			chatroom = gossip_chatroom_provider_find_by_id (provider, id);
			name = gossip_chatroom_get_name (chatroom);

			if (G_STR_EMPTY (name)) {
				continue;
			}

			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter,
					    COL_NAME, name,
					    COL_CHATROOM_ID, id,
					    -1);
		}
	}

	g_list_free (list);

}

static void
chat_invite_dialog_model_row_activated_cb (GtkTreeView            *tree_view,
					   GtkTreePath            *path,
					   GtkTreeViewColumn      *column,
					   GossipChatInviteDialog *dialog)
{
	chat_invite_dialog_response_cb (dialog->window, GTK_RESPONSE_OK, dialog);
}

static void
chat_invite_dialog_model_selection_changed_cb (GtkTreeSelection      *treeselection,
					       GossipChatInviteDialog *dialog)
{
	gint count;

	count = gtk_tree_selection_count_selected_rows (treeselection);
	gtk_widget_set_sensitive (dialog->button, (count == 1));
}

static void
chat_invite_dialog_model_setup (GossipChatInviteDialog *dialog)
{
	GtkTreeView      *view;
	GtkListStore     *store;
	GtkTreeSelection *selection;

	view = GTK_TREE_VIEW (dialog->treeview);

	g_signal_connect (view, "row-activated",
			  G_CALLBACK (chat_invite_dialog_model_row_activated_cb),
			  dialog);

	store = gtk_list_store_new (COL_COUNT,
				    G_TYPE_STRING,         /* name */
				    G_TYPE_INT,            /* chatroom id */
				    GOSSIP_TYPE_CONTACT);  /* contact */

	gtk_tree_view_set_model (view, GTK_TREE_MODEL (store));

	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect (selection, "changed",
			  G_CALLBACK (chat_invite_dialog_model_selection_changed_cb),
			  dialog);

	chat_invite_dialog_model_populate_columns (dialog);
	chat_invite_dialog_model_populate_suggestions (dialog);

	g_object_unref (store);
}

static void
chat_invite_dialog_destroy_cb (GtkWidget              *widget,
			       GossipChatInviteDialog *dialog)
{
	if (dialog->contact) {
		g_object_unref (dialog->contact);
	}

	g_free (dialog);
}

static void
chat_invite_dialog_response_cb (GtkWidget              *widget,
				gint                    response,
				GossipChatInviteDialog *dialog)
{
	if (response == GTK_RESPONSE_OK) {
		GtkTreeView            *view;
		GtkTreeModel           *model;
		GtkTreeSelection       *selection;
		GtkTreeIter             iter;
		GossipSession          *session;
		GossipAccount          *account;
		GossipChatroomProvider *provider;
		GossipChatroomId        id;
		GossipContact          *contact;
		const gchar            *message;

		view = GTK_TREE_VIEW (dialog->treeview);
		selection = gtk_tree_view_get_selection (view);

		if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
			return;
		}

		if (dialog->chatroom_id != 0) {
			id = dialog->chatroom_id;
			gtk_tree_model_get (model, &iter, COL_CONTACT, &contact, -1);
		} else {
			contact = g_object_ref (dialog->contact);
			gtk_tree_model_get (model, &iter, COL_CHATROOM_ID, &id, -1);
		}

		/* If NULL is used, it means the invite is translated
		 * and shown in the contact's language.
		 */
		message = gtk_entry_get_text (GTK_ENTRY (dialog->entry));
		message = (strlen (message) > 0) ? message : NULL;

		session = gossip_app_get_session ();
		account = gossip_contact_get_account (dialog->contact);
		provider = gossip_session_get_chatroom_provider (session, account);

		gossip_chatroom_provider_invite (provider, id, contact, message);

		g_object_unref (contact);
	}

	gtk_widget_destroy (dialog->window);
}

void
gossip_chat_invite_dialog_show (GossipContact    *contact,
				GossipChatroomId  id)
{
	GossipChatInviteDialog *dialog;
	GladeXML               *gui;
	gchar                  *name;
	gchar                  *str;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	dialog = g_new0 (GossipChatInviteDialog, 1);

	dialog->contact = g_object_ref (contact);
	dialog->chatroom_id = id;

	gui = gossip_glade_get_file ("chat.glade",
				     "chat_invite_dialog",
				     NULL,
				     "chat_invite_dialog", &dialog->window,
				     "treeview", &dialog->treeview,
				     "label", &dialog->label,
				     "entry", &dialog->entry,
				     "button_invite", &dialog->button,
				     NULL);

	gossip_glade_connect (gui,
			      dialog,
			      "chat_invite_dialog", "response", chat_invite_dialog_response_cb,
			      "chat_invite_dialog", "destroy", chat_invite_dialog_destroy_cb,
			      NULL);

	g_object_unref (gui);

	if (dialog->chatroom_id != 0) {
		GossipSession          *session;
		GossipAccount          *account;
		GossipChatroomProvider *provider;
		GossipChatroom         *chatroom;

		/* Show a list of contacts */
		session = gossip_app_get_session ();
		account = gossip_contact_get_account (dialog->contact);
		provider = gossip_session_get_chatroom_provider (session, account);
		chatroom = gossip_chatroom_provider_find_by_id (provider, dialog->chatroom_id);

		name = g_markup_escape_text (gossip_chatroom_get_name (chatroom), -1);
		str = g_strdup_printf ("%s\n<b>%s</b>",
				       _("Select who would you like to invite to room:"),
				       name);
	} else {
		/* Show a list of rooms */
		name = g_markup_escape_text (gossip_contact_get_name (dialog->contact), -1);
		str = g_strdup_printf ("%s\n<b>%s</b>",
				       _("Select which room you would like to invite:"),
				       name);
	}

	gtk_label_set_markup (GTK_LABEL (dialog->label), str);

	g_free (name);
	g_free (str);

	chat_invite_dialog_model_setup (dialog);

	gtk_widget_show (dialog->window);
}

