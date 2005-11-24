/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2005 Imendio AB
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
#include <stdio.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <libgnome/gnome-config.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <libgossip/gossip-chatroom.h>
#include <libgossip/gossip-chatroom-provider.h>

#include "gossip-app.h"
#include "gossip-chatrooms-dialog.h"
#include "gossip-group-chat.h"

#define JOIN_TIMEOUT   20000


typedef struct _GossipChatroomsDialog GossipChatroomsDialog;

struct _GossipChatroomsDialog {
	gpointer         *p;

	GtkWidget        *dialog;
	GtkWidget        *preamble_label;

	GtkWidget        *account_table;
	GtkWidget        *account_label;
	GtkWidget        *account_combobox;

	GtkWidget        *expander;

	GtkWidget        *details_table;
	GtkWidget        *chatroom_label;
	GtkWidget        *chatroom_combobox;
	GtkWidget        *nickname_label;
	GtkWidget        *nickname_entry;
	GtkWidget        *server_label;
	GtkWidget        *server_entry;
	GtkWidget        *room_label;
	GtkWidget        *room_entry;

	GtkWidget        *edit_button;
	GtkWidget        *delete_button;
	GtkWidget        *add_checkbutton;
	GtkWidget        *joining_vbox;
	GtkWidget        *joining_progressbar;
	GtkWidget        *join_button;

	guint             wait_id;
	guint             pulse_id;
	guint             timeout_id; 

	gboolean          changed;
	gboolean          joining;

	GossipChatroomId  joining_id;
	GossipChatroomId  last_selected_id;
};


typedef struct {
	GossipChatroomsDialog *dialog;
	GossipChatroom        *chatroom;
	gpointer               user_data;
} ChatroomData;


typedef struct {
	GtkWidget      *dialog;
	GtkWidget      *name_entry;
	GtkWidget      *nickname_entry;
	GtkWidget      *server_entry;
	GtkWidget      *room_entry;
	GtkWidget      *save_button;

	GossipChatroom *chatroom;
} ChatroomEditDialog;


enum {
	COL_ACCOUNT_IMAGE,
	COL_ACCOUNT_TEXT,
	COL_ACCOUNT_CONNECTED,
	COL_ACCOUNT_POINTER,
	COL_ACCOUNT_COUNT
};


enum {
	COL_CHATROOM_TEXT,
	COL_CHATROOM_POINTER,
	COL_CHATROOM_COUNT
};


static void           chatrooms_dialog_setup_accounts               (GList                    *accounts,
								     GossipChatroomsDialog    *dialog);
static GossipAccount *chatrooms_dialog_get_account_selected         (GossipChatroomsDialog    *dialog);
static void           chatrooms_dialog_setup_chatrooms              (GossipChatroomsDialog    *dialog);
static void           chatrooms_dialog_cancel                       (GossipChatroomsDialog    *dialog,
								     gboolean                  cleanup_ui,
								     gboolean                  cancel_join);
static void           chatrooms_dialog_set_next_chatroom            (GossipChatroomsDialog    *dialog);
static void           chatrooms_dialog_set_default                  (GossipChatroom           *chatroom);
static gboolean       chatrooms_dialog_set_default_timeout_cb       (ChatroomData             *crd);
static gboolean       chatrooms_dialog_progress_pulse_cb            (GtkWidget                *progressbar);
static gboolean       chatrooms_dialog_wait_cb                      (GossipChatroomsDialog    *dialog);
static gboolean       chatrooms_dialog_timeout_cb                   (GossipChatroomsDialog    *dialog);
static gboolean       chatrooms_dialog_is_separator_cb              (GtkTreeModel             *model,
								     GtkTreeIter              *iter,
								     gpointer                  data);
static void           chatrooms_dialog_join_cb                      (GossipChatroomProvider   *provider,
								     GossipChatroomJoinResult  result,
								     GossipChatroomId          id,
								     GossipChatroomsDialog    *dialog);
static void           chatrooms_dialog_edit_clicked_cb              (GtkWidget                *widget,
								     GossipChatroomsDialog    *dialog);
static void           chatrooms_dialog_delete_clicked_cb            (GtkWidget                *widget,
								     GossipChatroomsDialog    *dialog);
static gboolean       chatrooms_dialog_delete_foreach               (GtkTreeModel             *model,
								     GtkTreePath              *path,
								     GtkTreeIter              *iter,
								     GossipChatroom           *chatroom);
static void           chatrooms_dialog_entry_changed_cb             (GtkWidget                *widget,
								     GossipChatroomsDialog    *dialog);
static void           chatrooms_dialog_entry_changed_combo_cb       (GtkWidget                *widget,
								     GossipChatroomsDialog    *dialog);
static void           chatrooms_dialog_chatroom_combobox_changed_cb (GtkWidget                *widget,
								     GossipChatroomsDialog    *dialog);
static gboolean       chatrooms_dialog_chatroom_select_foreach      (GtkTreeModel             *model,
								     GtkTreePath              *path,
								     GtkTreeIter              *iter,
								     ChatroomData             *crd);
static void           chatrooms_dialog_chatroom_name_changed_cb     (GossipChatroom           *chatroom,
								     GParamSpec               *param,
								     GossipChatroomsDialog    *dialog);
static gboolean       chatrooms_dialog_chatroom_name_foreach        (GtkTreeModel             *model,
								     GtkTreePath              *path,
								     GtkTreeIter              *iter,
								     GossipChatroom           *chatroom);
static gboolean       chatrooms_dialog_chatroom_foreach             (GtkTreeModel             *model,
								     GtkTreePath              *path,
								     GtkTreeIter              *iter,
								     GossipChatroomsDialog    *dialog);
static void           chatrooms_dialog_response_cb                  (GtkWidget                *widget,
								     gint                      response,
								     GossipChatroomsDialog    *dialog);
static void           chatrooms_dialog_destroy_cb                   (GtkWidget                *unused,
								     GossipChatroomsDialog    *dialog);


/* favourites dialog */
static GtkWidget *    chatroom_edit_dialog_show                     (GossipChatroom           *chatroom);
static void           chatroom_edit_dialog_set                      (ChatroomEditDialog       *dialog);
static void           chatroom_edit_dialog_entry_changed_cb         (GtkWidget                *widget,
								     ChatroomEditDialog       *dialog);
static void           chatroom_edit_dialog_response_cb              (GtkWidget                *widget,
								     gint                      response,
								     ChatroomEditDialog       *dialog);
static void           chatroom_edit_dialog_destroy_cb               (GtkWidget                *widget,
								     ChatroomEditDialog       *dialog);



static void
chatrooms_dialog_setup_accounts (GList                 *accounts,
				 GossipChatroomsDialog *dialog)
{
	GossipSession   *session;

	GtkListStore    *store;
	GtkTreeIter      iter;
	GtkCellRenderer *renderer;
	GtkComboBox     *combo_box;

	GList           *l;
	GError          *error = NULL;
	GdkPixbuf       *pixbuf;

	gboolean         active_item_set = FALSE;

	session = gossip_app_get_session ();

	/* set up combo box with new store */
	combo_box = GTK_COMBO_BOX (dialog->account_combobox);

  	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo_box));  

	store = gtk_list_store_new (COL_ACCOUNT_COUNT,
				    GDK_TYPE_PIXBUF, 
				    G_TYPE_STRING,   /* name */
				    G_TYPE_BOOLEAN,
				    GOSSIP_TYPE_ACCOUNT);    

	gtk_combo_box_set_model (combo_box, GTK_TREE_MODEL (store));
		
	/* populate accounts */
	for (l = accounts; l; l = l->next) {
		GossipAccount *account;
		gboolean       is_connected;

		account = l->data;

		error = NULL; 
		pixbuf = NULL;

		is_connected = gossip_session_is_connected (session, account);
		pixbuf = gossip_ui_utils_get_pixbuf_from_account_status (account, 
									 GTK_ICON_SIZE_MENU,
									 is_connected);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 
				    COL_ACCOUNT_IMAGE, pixbuf,
				    COL_ACCOUNT_TEXT, gossip_account_get_name (account), 
				    COL_ACCOUNT_CONNECTED, is_connected,
				    COL_ACCOUNT_POINTER, account,
				    -1);

		g_object_unref (pixbuf);

		/* set first connected account as active account */
		if (!active_item_set && is_connected) {
			active_item_set = TRUE;
			gtk_combo_box_set_active_iter (combo_box, &iter); 
		}	
	}
	
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"pixbuf", COL_ACCOUNT_IMAGE,
					"sensitive", COL_ACCOUNT_CONNECTED,
					NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"text", COL_ACCOUNT_TEXT,
					"sensitive", COL_ACCOUNT_CONNECTED,
					NULL);

	g_object_unref (store);
}

static GossipAccount *
chatrooms_dialog_get_account_selected (GossipChatroomsDialog *dialog) 
{
	GossipAccount *account;
	GtkTreeModel  *model;
	GtkTreeIter    iter;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->account_combobox));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->account_combobox), &iter);

	gtk_tree_model_get (model, &iter, COL_ACCOUNT_POINTER, &account, -1);
	g_object_unref (account);

	return account;
}

static void
chatrooms_dialog_setup_chatrooms (GossipChatroomsDialog *dialog)
{
	GossipChatroomManager *manager;
	GossipChatroom        *default_chatroom;
	GList                 *chatrooms, *l;

	GtkListStore          *store;
 	GtkTreeIter            iter; 

        GtkCellRenderer       *renderer;
	GtkComboBox           *combobox;

	manager = gossip_app_get_chatroom_manager ();

	combobox = GTK_COMBO_BOX (dialog->chatroom_combobox);

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combobox));  
	
	store = gtk_list_store_new (2, G_TYPE_STRING, GOSSIP_TYPE_CHATROOM);
	
	gtk_combo_box_set_model (combobox,
				 GTK_TREE_MODEL (store));
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
					"text", COL_CHATROOM_TEXT,
					NULL);
	
	gtk_combo_box_set_row_separator_func (combobox, 
					      chatrooms_dialog_is_separator_cb, 
					      NULL, NULL);
	
	/* add custom entry */
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            COL_CHATROOM_TEXT, _("Custom"),
                            -1);

	g_signal_handlers_block_by_func (dialog->chatroom_combobox, 
					 chatrooms_dialog_chatroom_combobox_changed_cb, 
					 dialog);

	gtk_combo_box_set_active_iter (combobox, &iter);

	g_signal_handlers_unblock_by_func (dialog->chatroom_combobox, 
					   chatrooms_dialog_chatroom_combobox_changed_cb, 
					   dialog);

	/* look up chatrooms */
	chatrooms = gossip_chatroom_manager_get_chatrooms (manager);

	default_chatroom = gossip_chatroom_manager_get_default (manager);
	dialog->last_selected_id = gossip_chatroom_get_id (default_chatroom);

	/* add separator if we have chatrooms */
	if (chatrooms) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_CHATROOM_TEXT, "separator",
				    -1);
	}

	for (l = chatrooms; l; l = l->next) {
		GossipChatroom *chatroom;

		chatroom = l->data;
		
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 
				    COL_CHATROOM_TEXT, gossip_chatroom_get_name (chatroom), 
				    COL_CHATROOM_POINTER, chatroom,
				    -1);

		if (l == chatrooms) {
			/* use first item as default if there is none */
			g_signal_handlers_block_by_func (dialog->chatroom_combobox, 
							 chatrooms_dialog_chatroom_combobox_changed_cb, 
							 dialog);

			gtk_combo_box_set_active_iter (combobox, &iter);

			g_signal_handlers_unblock_by_func (dialog->chatroom_combobox, 
							   chatrooms_dialog_chatroom_combobox_changed_cb, 
							   dialog);
		}

		if (gossip_chatroom_equal (chatroom, default_chatroom)) {
			/* if set use the default item as first selection */
			gtk_combo_box_set_active_iter (combobox, &iter);
		}

		g_signal_connect (chatroom, "notify::name",
				  G_CALLBACK (chatrooms_dialog_chatroom_name_changed_cb), 
				  dialog);
	}

	/* show custom details if we have no chatrooms */
	if (!chatrooms) {
		gtk_expander_set_expanded (GTK_EXPANDER (dialog->expander), TRUE);
		gtk_widget_grab_focus (dialog->nickname_entry);
	}

	g_object_unref (store);
	g_list_free (chatrooms);
}

static void
chatrooms_dialog_cancel (GossipChatroomsDialog *dialog, 
			 gboolean               cleanup_ui,
			 gboolean               cancel_join)
{
	if (cleanup_ui) {
		gtk_widget_set_sensitive (dialog->account_table, TRUE);
		gtk_widget_set_sensitive (dialog->preamble_label, TRUE);
		gtk_widget_set_sensitive (dialog->details_table, TRUE);
		gtk_widget_set_sensitive (dialog->join_button, TRUE);
	
		gtk_widget_hide (dialog->joining_vbox);
	}
	
	if (dialog->wait_id != 0) {
		g_source_remove (dialog->wait_id);
		dialog->wait_id = 0;
	}
	
	if (dialog->pulse_id != 0) {
		g_source_remove (dialog->pulse_id);
		dialog->pulse_id = 0;
	}

	if (dialog->timeout_id != 0) {
		g_source_remove (dialog->timeout_id);
		dialog->timeout_id = 0;
	}

	if (dialog->joining_id > 0) {
		if (cancel_join) {
			GossipSession          *session;
			GossipAccount          *account;
			GossipChatroomProvider *provider;
			
			session = gossip_app_get_session ();
			account = chatrooms_dialog_get_account_selected (dialog);
			provider = gossip_session_get_chatroom_provider (session, account);
			
			gossip_chatroom_provider_cancel (provider, dialog->joining_id);
		}

		dialog->joining_id = 0;
		dialog->joining = FALSE;
	}
}

static void
chatrooms_dialog_set_next_chatroom (GossipChatroomsDialog *dialog)
{
	GtkComboBox  *combobox;
	GtkTreeModel *model;
	GtkTreeIter   iter;
	gboolean      ok;

	combobox = GTK_COMBO_BOX (dialog->chatroom_combobox);
	model = gtk_combo_box_get_model (combobox);

	for (ok = gtk_tree_model_get_iter_first (model, &iter);
	     ok;
	     ok = gtk_tree_model_iter_next (model, &iter)) {
		GossipChatroom *chatroom;
		
		gtk_tree_model_get (model, &iter, 
				    COL_CHATROOM_POINTER, &chatroom, 
				    -1);

		if (chatroom) {
			gtk_combo_box_set_active_iter (combobox, &iter);
			g_object_unref (chatroom);
			return;
		}
		
	}
}

static void
chatrooms_dialog_set_default (GossipChatroom *chatroom)
{
	ChatroomData *crd;
	static guint  timeout_id = 0;

	if (timeout_id != 0) {
		g_source_remove (timeout_id);
	}

	crd = g_new0 (ChatroomData, 1);
	
	crd->chatroom = g_object_ref (chatroom);
	
	timeout_id = g_timeout_add (100, 
				    (GSourceFunc)chatrooms_dialog_set_default_timeout_cb, 
				    crd);

	crd->user_data = &timeout_id;
}

static gboolean
chatrooms_dialog_set_default_timeout_cb (ChatroomData *crd)
{
	GossipChatroomManager *manager;

	manager = gossip_app_get_chatroom_manager ();

	gossip_chatroom_manager_set_default (manager, crd->chatroom);
	gossip_chatroom_manager_store (manager);

	g_object_unref (crd->chatroom);
	g_free (crd);

	if (crd->user_data) {
		*(guint*)(crd->user_data) = 0;
	}

	return FALSE;
}

static gboolean 
chatrooms_dialog_progress_pulse_cb (GtkWidget *progressbar)
{
	g_return_val_if_fail (progressbar != NULL, FALSE);
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (progressbar));

	return TRUE;
}

static gboolean
chatrooms_dialog_wait_cb (GossipChatroomsDialog *dialog)
{
	gtk_widget_show (dialog->joining_vbox);

	dialog->pulse_id = g_timeout_add (50, 
					  (GSourceFunc)chatrooms_dialog_progress_pulse_cb, 
					  dialog->joining_progressbar);

	return FALSE;
}

static gboolean
chatrooms_dialog_timeout_cb (GossipChatroomsDialog *dialog)
{
	GtkWidget *md;

	chatrooms_dialog_cancel (dialog, TRUE, TRUE);
	
	/* show message dialog and the account dialog */
	md = gtk_message_dialog_new_with_markup (GTK_WINDOW (dialog->dialog),
						 GTK_DIALOG_MODAL |
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_INFO,
						 GTK_BUTTONS_CLOSE,
						 "<b>%s</b>\n\n%s",
						 _("The chat room you are trying is not responding."),
						 _("Check your details and try again."));
	
	g_signal_connect_swapped (md, "response",
				  G_CALLBACK (gtk_widget_destroy), md);
	gtk_widget_show (md);
	
	return FALSE;
}

static gboolean
chatrooms_dialog_is_separator_cb (GtkTreeModel *model,
				  GtkTreeIter  *iter,
				  gpointer      data)
{
	GossipChatroomManager *manager;
	gint                   count;
	GtkTreePath           *path;
	gboolean               result;

	manager = gossip_app_get_chatroom_manager ();
	count = gossip_chatroom_manager_get_count (manager);

	if (count < 1) {
		return FALSE;
	}

	path = gtk_tree_model_get_path (model, iter);
	result = (gtk_tree_path_get_indices (path)[0] == 1);
	gtk_tree_path_free (path);
	
	return result;
}
	
static void
chatrooms_dialog_join_cb (GossipChatroomProvider   *provider,
			  GossipChatroomJoinResult  result,
			  GossipChatroomId          id,
			  GossipChatroomsDialog    *dialog)
{
	GtkWidget        *md;
	const gchar      *str1 = NULL;
	const gchar      *str2 = NULL;

	g_return_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider));
	g_return_if_fail (dialog != NULL);

	switch (result) {
		/* FIXME: show dialog */
	case GOSSIP_CHATROOM_JOIN_NICK_IN_USE:
		str1 = _("The nickname you have chosen is already in use.");
		str2 = _("Select another and try again");
		break;

	case GOSSIP_CHATROOM_JOIN_NEED_PASSWORD:
		str1 = _("The chat room you tried to join requires a password.");
		str2 = _("This is currently unsupported.");
		break;

	case GOSSIP_CHATROOM_JOIN_TIMED_OUT:
		str1 = _("The remote conference server did not respond in a sensible time.");
		str2 = _("Perhaps the conference server is busy, try again later.");
		break;

	case GOSSIP_CHATROOM_JOIN_UNKNOWN_HOST:
		str1 = _("The conference server you tried to join could not be found.");
		str2 = _("Check the server host name is correct and is available.");
		break;

	case GOSSIP_CHATROOM_JOIN_UNKNOWN_ERROR:
		str1 = _("An unknown error occured.");
		str2 = _("Check your details are correct.");
		break;

	case GOSSIP_CHATROOM_JOIN_OK:
	case GOSSIP_CHATROOM_JOIN_ALREADY_OPEN:
		gossip_group_chat_show (provider, id);
		gtk_widget_destroy (dialog->dialog);

		break;
	}

	if (str1 && str2) {
		chatrooms_dialog_cancel (dialog, TRUE, FALSE);
		
		/* show message dialog and the account dialog */
		md = gtk_message_dialog_new_with_markup (GTK_WINDOW (dialog->dialog),
							 GTK_DIALOG_MODAL |
							 GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_MESSAGE_INFO,
							 GTK_BUTTONS_CLOSE,
							 "<b>%s</b>\n\n%s",
							 str1,
							 str2);
		
		g_signal_connect_swapped (md, "response",
					  G_CALLBACK (gtk_widget_destroy), md);
		gtk_widget_show (md);
	}
}

static void
chatrooms_dialog_edit_clicked_cb (GtkWidget             *widget,
				  GossipChatroomsDialog *dialog)
{
	GossipChatroom *chatroom;
	GtkTreeModel   *model;
	GtkTreeIter     iter;
	GtkWidget      *window;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->chatroom_combobox));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->chatroom_combobox), &iter);

	gtk_tree_model_get (model, &iter, COL_CHATROOM_POINTER, &chatroom, -1);

	window = chatroom_edit_dialog_show (chatroom);

	gtk_window_set_transient_for (GTK_WINDOW (window), 
				      GTK_WINDOW (dialog->dialog));

	g_object_unref (chatroom);
}

static void
chatrooms_dialog_delete_clicked_cb (GtkWidget             *widget,
				    GossipChatroomsDialog *dialog)
{
	GossipChatroomManager *manager;
	GossipChatroom        *chatroom;
	GtkTreeModel          *model;
	GtkTreeIter            iter;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->chatroom_combobox));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->chatroom_combobox), &iter);

	gtk_tree_model_get (model, &iter, COL_CHATROOM_POINTER, &chatroom, -1);

	manager = gossip_app_get_chatroom_manager ();

	gossip_chatroom_manager_remove (manager, chatroom);
	gossip_chatroom_manager_store (manager);

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc)chatrooms_dialog_delete_foreach,
				chatroom);
	
	g_signal_handlers_block_by_func (chatroom, 
					 chatrooms_dialog_chatroom_name_changed_cb,
					 dialog);

	g_object_unref (chatroom);

	/* set the next chatroom */
	chatrooms_dialog_set_next_chatroom (dialog);
}

static gboolean 
chatrooms_dialog_delete_foreach (GtkTreeModel   *model,
				 GtkTreePath    *path,
				 GtkTreeIter    *iter,
				 GossipChatroom *chatroom_to_delete)
{
	GossipChatroom *chatroom;
	gboolean        equal = FALSE;

	gtk_tree_model_get (model, iter, COL_CHATROOM_POINTER, &chatroom, -1);
	if (!chatroom) {
		return equal;
	}

	equal = gossip_chatroom_equal (chatroom, chatroom_to_delete);
	if (equal) {
		gtk_list_store_remove (GTK_LIST_STORE (model), iter);
	}

	g_object_unref (chatroom);
	return equal;
}

static void
chatrooms_dialog_entry_changed_cb (GtkWidget             *widget,
				   GossipChatroomsDialog *dialog)
{
	const gchar *nickname;
	const gchar *server;
	const gchar *room;
	gboolean     disabled = FALSE;

	dialog->changed = TRUE;

	nickname = gtk_entry_get_text (GTK_ENTRY (dialog->nickname_entry));
	disabled |= !nickname || nickname[0] == 0;

	server = gtk_entry_get_text (GTK_ENTRY (dialog->server_entry));
	disabled |= !server || server[0] == 0;

	room = gtk_entry_get_text (GTK_ENTRY (dialog->room_entry));
	disabled |= !room || room[0] == 0;
	
	gtk_widget_set_sensitive (dialog->join_button, !disabled);
}

static void
chatrooms_dialog_entry_changed_combo_cb (GtkWidget             *widget,
					 GossipChatroomsDialog *dialog)
{
	GossipChatroomManager *manager;
	GList                 *chatrooms;
	const gchar           *nickname;
	const gchar           *server;
	const gchar           *room;

	nickname = gtk_entry_get_text (GTK_ENTRY (dialog->nickname_entry));
	server = gtk_entry_get_text (GTK_ENTRY (dialog->server_entry));
	room = gtk_entry_get_text (GTK_ENTRY (dialog->room_entry));
	
	manager = gossip_app_get_chatroom_manager ();
	chatrooms = gossip_chatroom_manager_find_extended (manager, nickname, server, room);

	if (chatrooms) {
		GtkTreeModel   *model;
		GossipChatroom *chatroom;
		ChatroomData   *crd;

		chatroom = g_list_nth_data (chatrooms, 0);

		crd = g_new0 (ChatroomData, 1);

		crd->dialog = dialog;
		crd->chatroom = g_object_ref (chatroom);

		model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->chatroom_combobox));
		gtk_tree_model_foreach (model,
					(GtkTreeModelForeachFunc)chatrooms_dialog_chatroom_select_foreach,
					crd);

		g_object_unref (crd->chatroom);
		g_free (crd);

		g_list_foreach (chatrooms, (GFunc)g_object_unref, NULL);
		g_list_free (chatrooms);

		return;
	} else {
		gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->chatroom_combobox), 0);
	}
}

static void
chatrooms_dialog_chatroom_combobox_changed_cb (GtkWidget             *widget,
					       GossipChatroomsDialog *dialog)
{
	GossipChatroom *chatroom;
	GtkTreeModel   *model;
	GtkTreeIter     iter;
	gchar          *name;
	gboolean        custom = FALSE;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->chatroom_combobox));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->chatroom_combobox), &iter);

	gtk_tree_model_get (model, &iter, 
			    COL_CHATROOM_TEXT, &name, 
			    COL_CHATROOM_POINTER, &chatroom, 
			    -1);

	if (name && !strcmp (name, _("Custom"))) {
		custom = TRUE;
	}

	g_free (name);

	gtk_widget_set_sensitive (dialog->add_checkbutton, custom);

	if (!custom) {
		gtk_widget_set_sensitive (dialog->join_button, TRUE);
		gtk_widget_set_sensitive (dialog->edit_button, TRUE);
		gtk_widget_set_sensitive (dialog->delete_button, TRUE);
	} else {
		gtk_widget_set_sensitive (dialog->edit_button, FALSE);
		gtk_widget_set_sensitive (dialog->delete_button, FALSE);

		gtk_expander_set_expanded (GTK_EXPANDER (dialog->expander), TRUE);
	}

	if (chatroom && (!custom || !dialog->changed)) {
		GossipChatroomId id;

		dialog->changed = FALSE;
		
		g_signal_handlers_block_by_func (dialog->nickname_entry, 
						 chatrooms_dialog_entry_changed_combo_cb,
						 dialog);
		g_signal_handlers_block_by_func (dialog->server_entry, 
						 chatrooms_dialog_entry_changed_combo_cb,
						 dialog);
		g_signal_handlers_block_by_func (dialog->room_entry, 
						 chatrooms_dialog_entry_changed_combo_cb,
						 dialog);
		
		gtk_entry_set_text (GTK_ENTRY (dialog->nickname_entry), 
				    gossip_chatroom_get_nick (chatroom));
		gtk_entry_set_text (GTK_ENTRY (dialog->server_entry), 
				    gossip_chatroom_get_server (chatroom));
		gtk_entry_set_text (GTK_ENTRY (dialog->room_entry), 
				    gossip_chatroom_get_room (chatroom));
		
		g_signal_handlers_unblock_by_func (dialog->nickname_entry, 
						   chatrooms_dialog_entry_changed_combo_cb,
						   dialog);
		g_signal_handlers_unblock_by_func (dialog->server_entry, 
						   chatrooms_dialog_entry_changed_combo_cb,
						   dialog);
		g_signal_handlers_unblock_by_func (dialog->room_entry, 
						   chatrooms_dialog_entry_changed_combo_cb,
						   dialog);

		id = gossip_chatroom_get_id (chatroom);
		if (id != dialog->last_selected_id) {
			dialog->last_selected_id = id;

			/* save the new default */
			chatrooms_dialog_set_default (chatroom);
		}
	}

	g_object_unref (chatroom);
}

static gboolean 
chatrooms_dialog_chatroom_select_foreach (GtkTreeModel *model,
					  GtkTreePath  *path,
					  GtkTreeIter  *iter,
					  ChatroomData *crd)
{
	GossipChatroomsDialog *dialog;
	GossipChatroom       *chatroom;
	GtkComboBox          *combobox;
	gboolean              equal = FALSE;

	gtk_tree_model_get (model, iter, COL_CHATROOM_POINTER, &chatroom, -1);
	if (!chatroom) {
		return equal;
	}

 	equal = gossip_chatroom_equal (chatroom, crd->chatroom);
 	if (equal) {
		dialog = crd->dialog;
		combobox = GTK_COMBO_BOX (dialog->chatroom_combobox);
		gtk_combo_box_set_active_iter (combobox, iter);
	}

	g_object_unref (chatroom);
	return equal;
}

static void
chatrooms_dialog_chatroom_name_changed_cb (GossipChatroom        *chatroom,
					   GParamSpec            *param,
					   GossipChatroomsDialog *dialog)
{
	GtkTreeModel *model;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->chatroom_combobox));

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc)chatrooms_dialog_chatroom_name_foreach,
				chatroom);

	chatrooms_dialog_set_default (chatroom);
}

static gboolean 
chatrooms_dialog_chatroom_name_foreach (GtkTreeModel   *model,
					GtkTreePath    *path,
					GtkTreeIter    *iter,
					GossipChatroom *chatroom_to_update)
{
	GossipChatroom *chatroom;
	gboolean        equal = FALSE;

	gtk_tree_model_get (model, iter, COL_CHATROOM_POINTER, &chatroom, -1);
	if (!chatroom) {
		return equal;
	}

	equal = gossip_chatroom_equal (chatroom, chatroom_to_update);
	if (equal) {
		gtk_list_store_set (GTK_LIST_STORE (model), iter,
				    COL_CHATROOM_TEXT, gossip_chatroom_get_name (chatroom),
				    -1);
	}

	g_object_unref (chatroom);
	return equal;
}

static gboolean
chatrooms_dialog_chatroom_foreach (GtkTreeModel          *model,
				   GtkTreePath           *path,
				   GtkTreeIter           *iter,
				   GossipChatroomsDialog *dialog)
{
	GossipChatroom *chatroom;

	gtk_tree_model_get (model, iter, COL_CHATROOM_POINTER, &chatroom, -1);
	if (!chatroom) {
		return FALSE;
	}

	g_signal_handlers_block_by_func (chatroom, 
					 chatrooms_dialog_chatroom_name_changed_cb,
					 dialog);

	g_object_unref (chatroom);

	return FALSE;
}

static void
chatrooms_dialog_response_cb (GtkWidget             *widget,
			      gint                   response,
			      GossipChatroomsDialog *dialog)
{
	if (response == GTK_RESPONSE_OK) {
		GossipSession          *session;
		GossipAccount          *account;
		GossipChatroomProvider *provider;
		GossipChatroomId        id;
		const gchar            *room;
		const gchar            *server;
		const gchar            *nickname;
	
		nickname = gtk_entry_get_text (GTK_ENTRY (dialog->nickname_entry));
		server = gtk_entry_get_text (GTK_ENTRY (dialog->server_entry));
		room   = gtk_entry_get_text (GTK_ENTRY (dialog->room_entry));

		/* should we save the chatroom? */
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->add_checkbutton))) {
			GossipChatroomManager *manager;
			GossipChatroom        *chatroom;
			GtkComboBox           *combobox;
			GtkListStore          *store;
			GtkTreeIter            iter;
			gchar                 *tmpname;

			tmpname = g_strdup_printf ("%s@%s as %s", room, server, nickname);
			chatroom = g_object_new (GOSSIP_TYPE_CHATROOM,
						 "name", tmpname, 
						 "nick", nickname, 
						 "server", server,
						 "room", room, 
						 NULL);
			g_free (tmpname);

			manager = gossip_app_get_chatroom_manager ();

			gossip_chatroom_manager_add (manager, chatroom);

			/* add to model and select in combo */
			combobox = GTK_COMBO_BOX (dialog->chatroom_combobox);
			store = GTK_LIST_STORE (gtk_combo_box_get_model (combobox));

			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter, 
					    COL_CHATROOM_TEXT, gossip_chatroom_get_name (chatroom), 
					    COL_CHATROOM_POINTER, chatroom,
					    -1);

			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox),
						       &iter);

			g_signal_connect (chatroom, "notify::name",
					  G_CALLBACK (chatrooms_dialog_chatroom_name_changed_cb), 
					  dialog);
						
			g_object_unref (chatroom);
		}

		/* change widgets so they are unsensitive */
		gtk_widget_set_sensitive (dialog->account_table, FALSE);
		gtk_widget_set_sensitive (dialog->preamble_label, FALSE);
		gtk_widget_set_sensitive (dialog->details_table, FALSE);
		gtk_widget_set_sensitive (dialog->join_button, FALSE);

		dialog->wait_id = g_timeout_add (2000, 
						 (GSourceFunc)chatrooms_dialog_wait_cb,
						 dialog);

		dialog->timeout_id = g_timeout_add (JOIN_TIMEOUT, 
						    (GSourceFunc)chatrooms_dialog_timeout_cb,
						    dialog);

		/* now do the join */
		session = gossip_app_get_session ();
		account = chatrooms_dialog_get_account_selected (dialog);

		provider = gossip_session_get_chatroom_provider (session, account);

		id = gossip_chatroom_provider_join (provider,
						    room, server, nickname, NULL,
						    (GossipChatroomJoinCb)chatrooms_dialog_join_cb,
						    dialog);

		dialog->joining = TRUE;
		dialog->joining_id = id;

		return;
	}

	if (response == GTK_RESPONSE_CANCEL && dialog->joining) {
		/* change widgets so they are unsensitive */
		chatrooms_dialog_cancel (dialog, TRUE, TRUE);

		return;
	}

	gtk_widget_destroy (widget);
}

static void 
chatrooms_dialog_destroy_cb (GtkWidget             *widget, 
			     GossipChatroomsDialog *dialog)
{
	GtkTreeModel *model;

	chatrooms_dialog_cancel (dialog, FALSE, FALSE);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->chatroom_combobox));
	gtk_tree_model_foreach (model, 
				(GtkTreeModelForeachFunc) chatrooms_dialog_chatroom_foreach, 
				dialog);

	*dialog->p = NULL;

	g_free (dialog);
}

void
gossip_chatrooms_dialog_show (void)
{
	static GossipChatroomsDialog *dialog = NULL;
	GladeXML                     *glade;
	GossipSession                *session;
	GList                        *accounts;
	GtkSizeGroup                 *size_group;

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->dialog));
		return;
	}		
	
        dialog = g_new0 (GossipChatroomsDialog, 1);

	dialog->p = (gpointer) &dialog;

	glade = gossip_glade_get_file (GLADEDIR "/group-chat.glade",
				       "chatrooms_dialog",
				       NULL,
				       "chatrooms_dialog", &dialog->dialog,
				       "account_table", &dialog->account_table,
				       "account_label", &dialog->account_label,
				       "account_combobox", &dialog->account_combobox,
				       "preamble_label", &dialog->preamble_label,
				       "details_table", &dialog->details_table,
				       "chatroom_label", &dialog->chatroom_label,
				       "chatroom_combobox", &dialog->chatroom_combobox,
				       "expander", &dialog->expander,
 				       "nickname_label", &dialog->nickname_label, 
 				       "nickname_entry", &dialog->nickname_entry, 
				       "server_label", &dialog->server_label,
				       "server_entry", &dialog->server_entry,
				       "room_label", &dialog->room_label,
				       "room_entry", &dialog->room_entry,
				       "edit_button", &dialog->edit_button,
				       "delete_button", &dialog->delete_button,
				       "add_checkbutton", &dialog->add_checkbutton,
				       "joining_vbox", &dialog->joining_vbox,
				       "joining_progressbar", &dialog->joining_progressbar,
				       "join_button", &dialog->join_button,
				       NULL);
	
	gossip_glade_connect (glade, 
			      dialog,
			      "chatrooms_dialog", "destroy", chatrooms_dialog_destroy_cb,
			      "chatrooms_dialog", "response", chatrooms_dialog_response_cb,
			      "edit_button", "clicked", chatrooms_dialog_edit_clicked_cb,
			      "delete_button", "clicked", chatrooms_dialog_delete_clicked_cb,
			      "nickname_entry", "changed", chatrooms_dialog_entry_changed_cb,
			      "server_entry", "changed", chatrooms_dialog_entry_changed_cb,
			      "room_entry", "changed", chatrooms_dialog_entry_changed_cb,
			      NULL);

	/* special cases which need to be disabled in some instances */
	g_signal_connect (dialog->chatroom_combobox, "changed", 
			  G_CALLBACK (chatrooms_dialog_chatroom_combobox_changed_cb), 
			  dialog);

	g_signal_connect (dialog->nickname_entry, "changed", 
			  G_CALLBACK (chatrooms_dialog_entry_changed_combo_cb), 
			  dialog);
	g_signal_connect (dialog->server_entry, "changed", 
			  G_CALLBACK (chatrooms_dialog_entry_changed_combo_cb), 
			  dialog);
	g_signal_connect (dialog->room_entry, "changed", 
			  G_CALLBACK (chatrooms_dialog_entry_changed_combo_cb), 
			  dialog);

	/* look and feel - aligning... */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, dialog->account_label);
	gtk_size_group_add_widget (size_group, dialog->chatroom_label);
	gtk_size_group_add_widget (size_group, dialog->nickname_label);
	gtk_size_group_add_widget (size_group, dialog->server_label);
	gtk_size_group_add_widget (size_group, dialog->room_label);

	g_object_unref (size_group);

	/* sort out accounts */
	session = gossip_app_get_session ();
	accounts = gossip_session_get_accounts (session);

	/* populate */
	chatrooms_dialog_setup_accounts (accounts, dialog);

	if (g_list_length (accounts) > 1) {
		gtk_widget_show (dialog->account_table);
	} else {
		/* show no accounts combo box */	
		gtk_widget_hide (dialog->account_table);
	}

	/* set FALSE _BEFORE_ setting favs */
	gtk_widget_set_sensitive (dialog->join_button, FALSE);

	/* set up chatrooms */
	chatrooms_dialog_setup_chatrooms (dialog);

	/* last touches */
	gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog), 
				      GTK_WINDOW (gossip_app_get_window ()));

	gtk_widget_show (dialog->dialog);
}

/*
 * Edit chatroom
 */

static void
chatroom_edit_dialog_set (ChatroomEditDialog *dialog)
{
	GossipChatroomManager *manager;
	const gchar           *str;

	manager = gossip_app_get_chatroom_manager ();

	/* set chatroom information */
	str = gtk_entry_get_text (GTK_ENTRY (dialog->name_entry));
	g_object_set (dialog->chatroom, "name", str, NULL);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->nickname_entry));
	g_object_set (dialog->chatroom, "nick", str, NULL);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->server_entry));
	g_object_set (dialog->chatroom, "server", str, NULL);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->room_entry));
	g_object_set (dialog->chatroom, "room", str, NULL);
	
	gossip_chatroom_manager_store (manager);
}

static void
chatroom_edit_dialog_entry_changed_cb (GtkWidget          *widget,
				       ChatroomEditDialog *dialog)
{
	const gchar *str;
	gboolean     disabled = FALSE;

	str = gtk_entry_get_text (GTK_ENTRY (dialog->nickname_entry));
	disabled |= !str || str[0] == 0;

	str = gtk_entry_get_text (GTK_ENTRY (dialog->server_entry));
	disabled |= !str || str[0] == 0;

	str = gtk_entry_get_text (GTK_ENTRY (dialog->room_entry));
	disabled |= !str || str[0] == 0;

	gtk_widget_set_sensitive (dialog->save_button, !disabled);
}

static void
chatroom_edit_dialog_response_cb (GtkWidget          *widget,
				  gint                response,
				  ChatroomEditDialog *dialog)
{
	switch (response) {
	case GTK_RESPONSE_OK:
		chatroom_edit_dialog_set (dialog);
		break;
	}

	gtk_widget_destroy (widget);
}

static void
chatroom_edit_dialog_destroy_cb (GtkWidget          *widget,
				 ChatroomEditDialog *dialog)
{
	g_object_unref (dialog->chatroom);
	g_free (dialog);
}

static GtkWidget *
chatroom_edit_dialog_show (GossipChatroom *chatroom)
{
	ChatroomEditDialog *dialog;
	GladeXML           *gui;

	g_return_val_if_fail (chatroom != NULL, NULL);
	
        dialog = g_new0 (ChatroomEditDialog, 1);

	dialog->chatroom = g_object_ref (chatroom);

	gui = gossip_glade_get_file (GLADEDIR "/group-chat.glade",
				     "chatroom_edit_dialog",
				     NULL,
				     "chatroom_edit_dialog", &dialog->dialog,
				     "name_entry", &dialog->name_entry,
				     "nickname_entry", &dialog->nickname_entry,
				     "server_entry", &dialog->server_entry,
				     "room_entry", &dialog->room_entry,
				     "save_button", &dialog->save_button,
				     NULL);
	
	gossip_glade_connect (gui, 
			      dialog,
			      "chatroom_edit_dialog", "destroy", chatroom_edit_dialog_destroy_cb,
			      "chatroom_edit_dialog", "response", chatroom_edit_dialog_response_cb,
			      "name_entry", "changed", chatroom_edit_dialog_entry_changed_cb,
			      "nickname_entry", "changed", chatroom_edit_dialog_entry_changed_cb,
			      "server_entry", "changed", chatroom_edit_dialog_entry_changed_cb,
			      "room_entry", "changed", chatroom_edit_dialog_entry_changed_cb,
			      NULL);

	gtk_entry_set_text (GTK_ENTRY (dialog->name_entry), 
			    gossip_chatroom_get_name (chatroom));
	gtk_entry_set_text (GTK_ENTRY (dialog->nickname_entry), 
			    gossip_chatroom_get_nick (chatroom));
	gtk_entry_set_text (GTK_ENTRY (dialog->server_entry),
			    gossip_chatroom_get_server (chatroom));
	gtk_entry_set_text (GTK_ENTRY (dialog->room_entry), 
			    gossip_chatroom_get_room (chatroom));

 	return dialog->dialog;
}

