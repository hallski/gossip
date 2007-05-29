/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2007 Imendio AB
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
#include <stdio.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-chatroom.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-chatroom-manager.h>
#include <libgossip/gossip-chatroom-provider.h>

#include "gossip-account-chooser.h"
#include "gossip-app.h"
#include "gossip-chatrooms-window.h"
#include "gossip-edit-chatroom-dialog.h"
#include "gossip-glade.h"
#include "gossip-new-chatroom-dialog.h"
#include "gossip-ui-utils.h"

typedef struct {
	GtkWidget *window;

	GtkWidget *hbox_account;
	GtkWidget *label_account;
	GtkWidget *account_chooser;

	GtkWidget *treeview;

	GtkWidget *button_remove;
	GtkWidget *button_edit;
	GtkWidget *button_close;

	gint       server_column;
	gint       room_column;
} GossipChatroomsWindow;

static void            chatrooms_window_model_add_columns               (GossipChatroomsWindow *window);
static void            chatrooms_window_model_cell_auto_connect_toggled (GtkCellRendererToggle *cell,
									 gchar                 *path_string,
									 GossipChatroomsWindow *window);
static void            chatrooms_window_model_pixbuf_cell_data_func     (GtkTreeViewColumn     *tree_column,
									 GtkCellRenderer       *cell,
									 GtkTreeModel          *model,
									 GtkTreeIter           *iter,
									 GossipChatroomsWindow *window);
static GossipChatroom *chatrooms_window_model_get_selected              (GossipChatroomsWindow *window);
static void            chatrooms_window_model_action_selected           (GossipChatroomsWindow *window);
static void            chatrooms_window_model_selection_changed         (GtkTreeSelection      *selection,
									 GossipChatroomsWindow *window);
static void            chatrooms_window_model_refresh_data              (GossipChatroomsWindow *window,
									 gboolean               first_time);
static void            chatrooms_window_model_add                       (GossipChatroomsWindow *window,
									 GossipChatroom        *chatroom,
									 gboolean               set_active,
									 gboolean               first_time);
static void            chatrooms_window_row_activated_cb                (GtkTreeView           *tree_view,
									 GtkTreePath           *path,
									 GtkTreeViewColumn     *column,
									 GossipChatroomsWindow *window);
static void            chatrooms_window_model_setup                     (GossipChatroomsWindow *window);
static void            chatrooms_window_update_buttons                  (GossipChatroomsWindow *window);
static void            chatrooms_window_button_remove_clicked_cb        (GtkWidget             *widget,
									 GossipChatroomsWindow *window);
static void            chatrooms_window_button_edit_clicked_cb          (GtkWidget             *widget,
									 GossipChatroomsWindow *window);
static void            chatrooms_window_button_close_clicked_cb         (GtkWidget             *widget,
									 GossipChatroomsWindow *window);
static gboolean        chatrooms_window_chatroom_changed_foreach        (GtkTreeModel          *model,
									 GtkTreePath           *path,
									 GtkTreeIter           *iter,
									 GossipChatroom        *chatroom);
static void            chatrooms_window_chatroom_changed_cb             (GossipChatroom        *chatroom,
									 GParamSpec            *param,
									 GossipChatroomsWindow *window);
static void            chatrooms_window_chatroom_added_cb               (GossipChatroomManager *manager,
									 GossipChatroom        *chatroom,
									 GossipChatroomsWindow *window);
static void            chatrooms_window_account_changed_cb              (GtkWidget             *combo_box,
									 GossipChatroomsWindow *window);

enum {
	COL_IMAGE,
	COL_NAME,
	COL_ROOM,
	COL_SERVER,
	COL_AUTO_CONNECT,
	COL_POINTER,
	COL_COUNT
};

static void
chatrooms_window_model_add_columns (GossipChatroomsWindow *window)
{
	GtkTreeView       *view;
	GtkTreeModel      *model;
	GtkTreeViewColumn *column;
	GtkCellRenderer   *cell;
	gint               count;

	view = GTK_TREE_VIEW (window->treeview);
	model = gtk_tree_view_get_model (view);

	gtk_tree_view_set_headers_visible (view, TRUE);
	gtk_tree_view_set_headers_clickable (view, TRUE);

	/* Name & Status */
	column = gtk_tree_view_column_new ();
	count = gtk_tree_view_append_column (view, column);

	gtk_tree_view_column_set_title (column, _("Name"));
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_sort_column_id (column, count - 1);

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc)
						 chatrooms_window_model_pixbuf_cell_data_func,
						 window,
						 NULL);

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell,
		      "xpad", 4,
		      "ypad", 1,
		      NULL);
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_add_attribute (column, cell, "text", COL_NAME);

	/* Room */
	cell = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Room"), cell, 
							   "text", COL_ROOM, 
							   NULL);
	count = gtk_tree_view_append_column (view, column);
	gtk_tree_view_column_set_sort_column_id (column, count - 1);
	window->room_column = count - 1;

	/* Server */
	cell = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Server"), cell, 
							   "text", COL_SERVER, 
							   NULL);
	count = gtk_tree_view_append_column (view, column);
	gtk_tree_view_column_set_sort_column_id (column, count - 1);
	window->server_column = count - 1;

	/* Chatroom auto connect */
	cell = gtk_cell_renderer_toggle_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Auto Connect"), cell,
							   "active", COL_AUTO_CONNECT,
							   NULL);
	count = gtk_tree_view_append_column (view, column);
	gtk_tree_view_column_set_sort_column_id (column, count - 1);

	g_signal_connect (cell, "toggled",
			  G_CALLBACK (chatrooms_window_model_cell_auto_connect_toggled),
			  window);

	/* Sort model */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model), 0, 
					      GTK_SORT_ASCENDING);
}

static void
chatrooms_window_model_cell_auto_connect_toggled (GtkCellRendererToggle  *cell,
						  gchar                  *path_string,
						  GossipChatroomsWindow  *window)
{
	GossipChatroomManager *manager;
	GossipChatroom        *chatroom;
	gboolean               enabled;
	GtkTreeView           *view;
	GtkTreeModel          *model;
	GtkListStore          *store;
	GtkTreePath           *path;
	GtkTreeIter            iter;

	view = GTK_TREE_VIEW (window->treeview);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);

	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    COL_AUTO_CONNECT, &enabled,
			    COL_POINTER, &chatroom,
			    -1);

	enabled = !enabled;

	/* Store */
	gossip_chatroom_set_auto_connect (chatroom, enabled);

	manager = gossip_app_get_chatroom_manager ();
	gossip_chatroom_manager_store (manager);

	gtk_list_store_set (store, &iter, COL_AUTO_CONNECT, enabled, -1);
	gtk_tree_path_free (path);

	g_object_unref (chatroom);
}

static void
chatrooms_window_model_pixbuf_cell_data_func (GtkTreeViewColumn     *tree_column,
					      GtkCellRenderer       *cell,
					      GtkTreeModel          *model,
					      GtkTreeIter           *iter,
					      GossipChatroomsWindow *window)
{
	GossipChatroom       *chatroom;
	GossipChatroomStatus  status;
	GdkPixbuf            *pixbuf = NULL;
	const gchar          *last_error;

	gtk_tree_model_get (model, iter,
			    COL_IMAGE, &pixbuf,
			    -1);

	/* If a pixbuf, use it */
	if (pixbuf) {
		g_object_set (cell,
			      "visible", TRUE,
			      "pixbuf", pixbuf,
			      NULL);

		g_object_unref (pixbuf);
		return;
	}

	gtk_tree_model_get (model, iter, COL_POINTER, &chatroom, -1);

	status = gossip_chatroom_get_status (chatroom);
	last_error = gossip_chatroom_get_last_error (chatroom);

	if (status == GOSSIP_CHATROOM_STATUS_ERROR && !last_error) {
		status = GOSSIP_CHATROOM_STATUS_INACTIVE;
	}

	pixbuf = gossip_pixbuf_for_chatroom_status (chatroom, GTK_ICON_SIZE_MENU);
	g_object_unref (chatroom);

	g_object_set (cell,
		      "visible", TRUE,
		      "pixbuf", pixbuf,
		      NULL);

	g_object_unref (pixbuf);
}

static GossipChatroom *
chatrooms_window_model_get_selected (GossipChatroomsWindow *window)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	GossipChatroom   *chatroom = NULL;

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, COL_POINTER, &chatroom, -1);
	}

	return chatroom;
}

static void
chatrooms_window_model_action_selected (GossipChatroomsWindow *window)
{
	GossipSession          *session;
	GossipAccount          *account;
	GossipAccountChooser   *account_chooser;
	GossipChatroomProvider *provider;
	GossipChatroom         *chatroom;
	GossipChatroomStatus    status;
	GtkTreeView            *view;
	GtkTreeModel           *model;

	session = gossip_app_get_session ();

	account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser);
	account = gossip_account_chooser_get_account (account_chooser);

	provider = gossip_session_get_chatroom_provider (session, account);

	if (account) {
		g_object_unref (account);
	}

	view = GTK_TREE_VIEW (window->treeview);
	model = gtk_tree_view_get_model (view);

	chatroom = chatrooms_window_model_get_selected (window);
	if (!chatroom) {
		return;
	}

	status = gossip_chatroom_get_status (chatroom);
	
	if (status != GOSSIP_CHATROOM_STATUS_JOINING &&
	    status != GOSSIP_CHATROOM_STATUS_ACTIVE) {
		gossip_edit_chatroom_dialog_show (GTK_WINDOW (window->window), chatroom);
	}
	
	chatrooms_window_update_buttons (window);
	
	g_object_unref (chatroom);
}

static void
chatrooms_window_model_selection_changed (GtkTreeSelection      *selection,
					  GossipChatroomsWindow *window)
{
 	chatrooms_window_update_buttons (window); 
}

static void
chatrooms_window_model_refresh_data (GossipChatroomsWindow *window,
				     gboolean               first_time)
{
	GtkTreeView           *view;
	GtkTreeSelection      *selection;
	GtkTreeModel          *model;
	GtkListStore          *store;
	GtkTreeIter            iter;

	GossipSession         *session;
	GossipAccountChooser  *account_chooser;
	GossipAccount         *account;
	GossipChatroomManager *manager;
	GtkTreeViewColumn     *column;
	GList                 *chatrooms, *l;

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);

	/* Look up chatrooms */
	session = gossip_app_get_session ();

	account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser);
	account = gossip_account_chooser_get_account (account_chooser);

	manager = gossip_app_get_chatroom_manager ();
	chatrooms = gossip_chatroom_manager_get_chatrooms (manager, account);

	/* Sort out columns, we only show the server column for
	 * selected protocol types, such as Jabber. 
	 */
	if (account) {
		GossipAccountType type;
		
		type = gossip_account_get_type (account);

		if (type == GOSSIP_ACCOUNT_TYPE_JABBER_LEGACY ||
		    type == GOSSIP_ACCOUNT_TYPE_JABBER) {
			column = gtk_tree_view_get_column (view, window->room_column);
			gtk_tree_view_column_set_visible (column, TRUE);

			column = gtk_tree_view_get_column (view, window->server_column);
			gtk_tree_view_column_set_visible (column, TRUE);
		} else {
			column = gtk_tree_view_get_column (view, window->room_column);
			gtk_tree_view_column_set_visible (column, FALSE);

			column = gtk_tree_view_get_column (view, window->server_column);
			gtk_tree_view_column_set_visible (column, FALSE);
		}
	} else {
		column = gtk_tree_view_get_column (view, window->room_column);
		gtk_tree_view_column_set_visible (column, FALSE);

		column = gtk_tree_view_get_column (view, window->server_column);
		gtk_tree_view_column_set_visible (column, FALSE);
	}

	/* Clean out the store */
	gtk_list_store_clear (store);

	/* Populate with chatroom list. */
	for (l = chatrooms; l; l = l->next) {
		chatrooms_window_model_add (window, l->data,  FALSE, first_time);
	}

	if (gtk_tree_model_get_iter_first (model, &iter)) {
		gtk_tree_selection_select_iter (selection, &iter);
	}

 	chatrooms_window_update_buttons (window); 

	if (account) {
		g_object_unref (account);
	}

	g_list_free (chatrooms);
}

static void
chatrooms_window_model_add (GossipChatroomsWindow *window,
			    GossipChatroom        *chatroom,
			    gboolean               set_active,
			    gboolean               first_time)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkListStore     *store;
	GtkTreeIter       iter;

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    COL_NAME, gossip_chatroom_get_name (chatroom),
			    COL_ROOM, gossip_chatroom_get_room (chatroom),
			    COL_SERVER, gossip_chatroom_get_server (chatroom),
			    COL_AUTO_CONNECT, gossip_chatroom_get_auto_connect (chatroom),
			    COL_POINTER, chatroom,
			    -1);

	if (set_active) {
		gtk_tree_selection_select_iter (selection, &iter);
	}

	if (first_time) {
		g_signal_connect (chatroom, "notify",
				  G_CALLBACK (chatrooms_window_chatroom_changed_cb),
				  window);
	}
}

static void
chatrooms_window_row_activated_cb (GtkTreeView           *tree_view,
				   GtkTreePath           *path,
				   GtkTreeViewColumn     *column,
				   GossipChatroomsWindow *window)
{
	if (GTK_WIDGET_IS_SENSITIVE (window->button_edit)) {
		chatrooms_window_model_action_selected (window);
	}
}

static void
chatrooms_window_model_setup (GossipChatroomsWindow *window)
{
	GtkTreeView      *view;
	GtkListStore     *store;
	GtkTreeSelection *selection;

	/* View */
	view = GTK_TREE_VIEW (window->treeview);

	g_signal_connect (view, "row-activated",
			  G_CALLBACK (chatrooms_window_row_activated_cb),
			  window);

	/* Store */
	store = gtk_list_store_new (COL_COUNT,
				    GDK_TYPE_PIXBUF,       /* Image */
				    G_TYPE_STRING,         /* Name */
				    G_TYPE_STRING,         /* Room */
				    G_TYPE_STRING,         /* Server */
				    G_TYPE_BOOLEAN,        /* Auto start */
				    GOSSIP_TYPE_CHATROOM); /* Chatroom */

	gtk_tree_view_set_model (view, GTK_TREE_MODEL (store));

	/* Selection */ 
	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect (selection, "changed",
			  G_CALLBACK (chatrooms_window_model_selection_changed), 
			  window);

	/* Columns */
	chatrooms_window_model_add_columns (window);

	/* Add data */
	chatrooms_window_model_refresh_data (window, TRUE);

	/* Clean up */
	g_object_unref (store);
}

static void
chatrooms_window_update_buttons (GossipChatroomsWindow *window)
{
	GossipChatroom *chatroom;

	chatroom = chatrooms_window_model_get_selected (window);
	if (chatroom) {
		GossipChatroomStatus status;
		gboolean             sensitive = TRUE;

		status = gossip_chatroom_get_status (chatroom);
		sensitive &= status != GOSSIP_CHATROOM_STATUS_JOINING;
		sensitive &= status != GOSSIP_CHATROOM_STATUS_ACTIVE;

		gtk_widget_set_sensitive (window->button_remove, sensitive);
		gtk_widget_set_sensitive (window->button_edit, sensitive);
		
		g_object_unref (chatroom);
	} else {
		gtk_widget_set_sensitive (window->button_remove, FALSE);
		gtk_widget_set_sensitive (window->button_edit, FALSE);
	}
}

static void
chatrooms_window_button_remove_clicked_cb (GtkWidget             *widget,
					   GossipChatroomsWindow *window)
{
	GossipChatroomManager *manager;
	GossipChatroom        *chatroom;
	GtkTreeView           *view;
	GtkTreeModel          *model;
	GtkTreeSelection      *selection;
	GtkTreeIter            iter;

	/* Remove from treeview */
	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return;
	}

	gtk_tree_model_get (model, &iter, COL_POINTER, &chatroom, -1);
	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

	/* Remove from config */
	manager = gossip_app_get_chatroom_manager ();
	gossip_chatroom_manager_remove (manager, chatroom);
	gossip_chatroom_manager_store (manager);
	
	g_object_unref (chatroom);
}

static void
chatrooms_window_button_edit_clicked_cb (GtkWidget             *widget,
					 GossipChatroomsWindow *window)
{
	GossipChatroom *chatroom;

	chatroom = chatrooms_window_model_get_selected (window);
	if (!chatroom) {
		return;
	}

	gossip_edit_chatroom_dialog_show (GTK_WINDOW (window->window), chatroom);

	g_object_unref (chatroom);
}

static void
chatrooms_window_button_close_clicked_cb (GtkWidget             *widget,
					  GossipChatroomsWindow *window)
{
	gtk_widget_hide (window->window);
}

static gboolean
chatrooms_window_chatroom_changed_foreach (GtkTreeModel   *model,
					   GtkTreePath    *path,
					   GtkTreeIter    *iter,
					   GossipChatroom *chatroom_to_update)
{
	GossipChatroom *chatroom;
	gboolean        equal = FALSE;

	gtk_tree_model_get (model, iter, COL_POINTER, &chatroom, -1);
	if (!chatroom) {
		return equal;
	}

	equal = gossip_chatroom_equal (chatroom, chatroom_to_update);
	if (equal) {
		/* We have to set the model's copy of the favourite
		 * information because we don't use cell data
		 * functions to work it out on update.
		 */
		gtk_list_store_set (GTK_LIST_STORE (model), iter,
				    COL_NAME, gossip_chatroom_get_name (chatroom),
				    COL_ROOM, gossip_chatroom_get_room (chatroom),
				    COL_SERVER, gossip_chatroom_get_server (chatroom),
				    COL_AUTO_CONNECT, gossip_chatroom_get_auto_connect (chatroom),
				    -1);


		gtk_tree_model_row_changed (model, path, iter);
	}

	g_object_unref (chatroom);

	return equal;
}

static void
chatrooms_window_chatroom_changed_cb (GossipChatroom        *chatroom,
				      GParamSpec            *param,
				      GossipChatroomsWindow *window)
{
	GtkTreeModel *model = NULL;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (window->treeview));

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc)
				chatrooms_window_chatroom_changed_foreach,
				chatroom);

	chatrooms_window_update_buttons (window);
}

static void
chatrooms_window_chatroom_added_cb (GossipChatroomManager *manager,
				    GossipChatroom        *chatroom,
				    GossipChatroomsWindow *window)
{
	GossipAccount        *account;
	GossipAccount        *account_selected;
	GossipAccountChooser *account_chooser;

	account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser);
	account_selected = gossip_account_chooser_get_account (account_chooser);

	if (!account_selected) {
		chatrooms_window_model_add (window, chatroom, FALSE, TRUE);
	} else {
		account = gossip_chatroom_get_account (chatroom);

		if (gossip_account_equal (account_selected, account)) {
			chatrooms_window_model_add (window, chatroom, FALSE, TRUE);
		}

		g_object_unref (account_selected);
	}
}

static void
chatrooms_window_account_changed_cb (GtkWidget             *combo_box,
				     GossipChatroomsWindow *window)
{
	chatrooms_window_model_refresh_data (window, FALSE);
}

void
gossip_chatrooms_window_show (GtkWindow *parent,
			      gboolean   show_chatrooms)
{
	static GossipChatroomsWindow *window = NULL;
	GladeXML                     *glade;
	GossipSession                *session;
	GossipChatroomManager        *manager;
	GList                        *accounts;
	guint                         connected;

	if (window) {
		gtk_window_present (GTK_WINDOW (window->window));
		return;
	}

	window = g_new0 (GossipChatroomsWindow, 1);

	glade = gossip_glade_get_file ("group-chat.glade",
				       "chatrooms_window",
				       NULL,
				       "chatrooms_window", &window->window,
				       "hbox_account", &window->hbox_account,
				       "label_account", &window->label_account,
				       "treeview", &window->treeview,
				       "button_edit", &window->button_edit,
				       "button_remove", &window->button_remove,
				       "button_close", &window->button_close,
				       NULL);

	gossip_glade_connect (glade,
			      window,
			      "button_remove", "clicked", chatrooms_window_button_remove_clicked_cb,
			      "button_edit", "clicked", chatrooms_window_button_edit_clicked_cb,
			      "button_close", "clicked", chatrooms_window_button_close_clicked_cb,
			      NULL);

	g_object_unref (glade);

	g_signal_connect_swapped (window->window, "delete_event",
				  G_CALLBACK (gtk_widget_hide_on_delete),
				  window->window);

	/* Get the session and chat room manager */
	session = gossip_app_get_session ();
	manager = gossip_app_get_chatroom_manager ();

	g_signal_connect (manager, "chatroom-added",
			  G_CALLBACK (chatrooms_window_chatroom_added_cb),
			  window);

	/* Account chooser for chat rooms */
	window->account_chooser = gossip_account_chooser_new (session);
	g_object_set (window->account_chooser, 
		      "can-select-all", TRUE,
		      "has-all-option", TRUE,
		      NULL);

	gtk_box_pack_start (GTK_BOX (window->hbox_account),
			    window->account_chooser,
			    TRUE, TRUE, 0);

	g_signal_connect (window->account_chooser, "changed",
			  G_CALLBACK (chatrooms_window_account_changed_cb),
			  window);

	gtk_widget_show (window->account_chooser);

	/* Set up chatrooms */
	chatrooms_window_model_setup (window);

	/* Populate */
	accounts = gossip_session_get_accounts (session);

	if (g_list_length (accounts) > 1) {
		gtk_widget_show (window->hbox_account);
	} else {
		/* Show no accounts combo box */
		gtk_widget_hide (window->hbox_account);
	}

	g_list_foreach (accounts, (GFunc) g_object_unref, NULL);
	g_list_free (accounts);

        gossip_session_count_accounts (session, &connected, NULL, NULL);
	if (connected != 1) {
		GossipAccountChooser *account_chooser;

		account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser);
		gossip_account_chooser_set_account (account_chooser, NULL);
	}

	/* Set focus */
	gtk_widget_grab_focus (window->treeview);

	/* Last touches */
	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (window->window),
					      GTK_WINDOW (parent));
	}

	gtk_widget_show (window->window);
}
