/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2006 Imendio AB
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

#include "gossip-account-chooser.h"
#include "gossip-app.h"
#include "gossip-chatrooms-window.h"
#include "gossip-group-chat.h"
#include "gossip-stock.h"


/* FIXME: We do this because GtkNotebook is broken, even though the
 * page has been switched, on the callback "switch-page", if you call
 * gtk_notebook_get_current_page() it returns the old page not the new
 * page.  What is worse, is that the documentation is out of date and
 * the signal "change-current-page" is never emitted in the GTK+ code.
 * So we keep track of things here to remain consistent. */ 
#define BROKEN_NOTEBOOK_API 1

/* This is turned off for now, but to configure the auto connect in
 * the list instead of from the edit dialog, define this variable: */
#undef CHATROOM_AUTOCONNECT_IN_LIST


typedef struct {
	GtkWidget        *window;

	GtkWidget        *notebook;
	
	GtkWidget        *hbox_account_chatroom;
	GtkWidget        *label_account_chatroom;
	GtkWidget        *account_chooser_chatroom;

	GtkWidget        *treeview;

	GtkWidget        *button_edit;
	GtkWidget        *button_delete;

	GtkWidget        *label_preamble_custom;

	GtkWidget        *hbox_account_custom;
	GtkWidget        *label_account_custom;
	GtkWidget        *account_chooser_custom;

	GtkWidget        *table_details;
	GtkWidget        *label_nickname;
	GtkWidget        *entry_nickname;
	GtkWidget        *label_server;
	GtkWidget        *entry_server;
	GtkWidget        *label_room;
	GtkWidget        *entry_room;
	GtkWidget        *table_progress;
	GtkWidget        *image_progress;
	GtkWidget        *label_progress;
	GtkWidget        *label_progress_detail;
	GtkWidget        *progressbar;

	GtkWidget        *checkbutton_add;

	GtkWidget        *button_join;
	GtkWidget        *button_close;

	GossipChatroom   *joining_chatroom;
	guint             joining_chatroom_pulse_id;
	guint             joining_chatroom_change_id;

	GossipChatroomId  last_selected_id;

	guint             page;
} GossipChatroomsWindow;


typedef struct {
	GtkWidget      *dialog;
	GtkWidget      *entry_name;
	GtkWidget      *entry_nickname;
	GtkWidget      *entry_server;
	GtkWidget      *entry_room;
	GtkWidget      *checkbutton_auto_connect;
	GtkWidget      *button_save;

	GossipChatroom *chatroom;
} ChatroomEditDialog;


static void       chatrooms_window_model_add_columns           (GossipChatroomsWindow    *window);
#ifdef CHATROOM_AUTOCONNECT_IN_LIST
static void       chatrooms_window_model_cell_toggled          (GtkCellRendererToggle    *cell,
								gchar                    *path_string,
								GossipChatroomsWindow    *window);
#endif
static void       chatrooms_window_model_pixbuf_cell_data_func (GtkTreeViewColumn        *tree_column,
								GtkCellRenderer          *cell,
								GtkTreeModel             *model,
								GtkTreeIter              *iter,
								GossipChatroomsWindow    *window);
static void       chatrooms_window_model_text_cell_data_func   (GtkTreeViewColumn        *tree_column,
								GtkCellRenderer          *cell,
								GtkTreeModel             *model,
								GtkTreeIter              *iter,
								GossipChatroomsWindow    *window);

static GossipChatroom *
                  chatrooms_window_model_get_selected          (GossipChatroomsWindow    *window);
static GossipChatroomStatus
                  chatrooms_window_model_status_selected       (GossipChatroomsWindow    *window);
static void       chatrooms_window_model_join_selected         (GossipChatroomsWindow    *window);
static void       chatrooms_window_model_cancel_selected       (GossipChatroomsWindow    *window);
static void       chatrooms_window_model_selection_changed     (GtkTreeSelection         *selection,
								GossipChatroomsWindow    *window);
static void       chatrooms_window_model_refresh_data          (GossipChatroomsWindow    *window,
								gboolean                  first_time);
static void       chatrooms_window_model_add                   (GossipChatroomsWindow    *window,
								GossipChatroom           *chatroom,
								gboolean                  set_active,
								gboolean                  first_time);
static void       chatrooms_window_model_remove                (GossipChatroomsWindow    *window,
								GossipChatroom           *chatroom);
static void       chatrooms_window_model_setup                 (GossipChatroomsWindow    *window);

static void       chatrooms_window_update_join_button          (GossipChatroomsWindow    *window);
static void       chatrooms_window_join_custom                 (GossipChatroomsWindow    *window);
static void       chatrooms_window_join_stop                   (GossipChatroomsWindow    *window);
static void       chatrooms_window_join_cancel                 (GossipChatroomsWindow    *window);

static gboolean   chatrooms_window_progress_pulse_cb           (GtkWidget                *progressbar);
static void       chatrooms_window_join_cb                     (GossipChatroomProvider   *provider,
								GossipChatroomJoinResult  result,
								GossipChatroomId          id,
								GossipChatroomsWindow    *window);
static void       chatrooms_window_notebook_switch_page_cb     (GtkNotebook              *notebook,
								GtkNotebookPage          *new_page,
								gint                      old_page,
								GossipChatroomsWindow    *window);
static void       chatrooms_window_edit_clicked_cb             (GtkWidget                *widget,
								GossipChatroomsWindow    *window);
static void       chatrooms_window_delete_clicked_cb           (GtkWidget                *widget,
								GossipChatroomsWindow    *window);
static gboolean   chatrooms_window_delete_foreach              (GtkTreeModel             *model,
								GtkTreePath              *path,
								GtkTreeIter              *iter,
								GossipChatroom           *chatroom);
static void       chatrooms_window_join_clicked_cb             (GtkWidget                *widget,
								GossipChatroomsWindow    *window);
static void       chatrooms_window_close_clicked_cb            (GtkWidget                *widget,
								GossipChatroomsWindow    *window);
static void       chatrooms_window_entry_changed_cb            (GtkWidget                *widget,
								GossipChatroomsWindow    *window);
static void       chatrooms_window_chatroom_changed_cb         (GossipChatroom           *chatroom,
								GParamSpec               *param,
								GossipChatroomsWindow    *window);
static gboolean   chatrooms_window_chatroom_changed_foreach    (GtkTreeModel             *model,
								GtkTreePath              *path,
								GtkTreeIter              *iter,
								GossipChatroom           *chatroom);
static void       chatrooms_window_account_chatroom_changed_cb (GtkWidget                *combo_box,
								GossipChatroomsWindow    *window);

/* edit dialog */
static GtkWidget *chatroom_edit_dialog_show                    (GossipChatroom           *chatroom);
static void       chatroom_edit_dialog_set                     (ChatroomEditDialog       *dialog);
static void       chatroom_edit_dialog_entry_changed_cb        (GtkEntry                 *entry,
								ChatroomEditDialog       *dialog);
static void       chatroom_edit_dialog_response_cb             (GtkWidget                *widget,
								gint                      response,
								ChatroomEditDialog       *dialog);

static void       chatroom_edit_dialog_destroy_cb              (GtkWidget                *widget,
								ChatroomEditDialog       *dialog);

enum {
	COL_IMAGE,
	COL_NAME,
	COL_AUTO_CONNECT,
	COL_POINTER,
	COL_COUNT
};


enum {
	PAGE_CUSTOM,
	PAGE_CHATROOM
};


static void 
chatrooms_window_model_add_columns (GossipChatroomsWindow *window)
{
	GtkTreeView       *view;
	GtkTreeViewColumn *column; 
	GtkCellRenderer   *cell;
	
	view = GTK_TREE_VIEW (window->treeview);
	gtk_tree_view_set_headers_visible (view, FALSE);

#ifdef CHATROOM_AUTOCONNECT_IN_LIST
	/* chatroom auto connect */
	cell = gtk_cell_renderer_toggle_new ();
 	column = gtk_tree_view_column_new_with_attributes (NULL, cell,  
 							   "active", COL_AUTO_CONNECT,  
 							   NULL); 
	gtk_tree_view_append_column (view, column);

 	g_signal_connect (cell, "toggled",  
 			  G_CALLBACK (chatrooms_window_model_cell_toggled),  
 			  window); 
#endif /* CHATROOM_AUTOCONNECT_IN_LIST */

	/* chatroom pointer */
	column = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, cell, 
						 (GtkTreeCellDataFunc) 
						 chatrooms_window_model_pixbuf_cell_data_func,
						 window, 
						 NULL);

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell,
		      "xpad", (guint) 4, 
		      "ypad", (guint) 1,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);

	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, cell, 
						 (GtkTreeCellDataFunc) 
						 chatrooms_window_model_text_cell_data_func,
						 window, 
						 NULL);

	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_append_column (view, column);
}

#ifdef CHATROOM_AUTOCONNECT_IN_LIST
static void 
chatrooms_window_model_cell_toggled (GtkCellRendererToggle  *cell, 
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

	/* store */
	gossip_chatroom_set_auto_connect (chatroom, enabled);

	manager = gossip_app_get_chatroom_manager ();
	gossip_chatroom_manager_store (manager);
	
	gtk_list_store_set (store, &iter, COL_AUTO_CONNECT, enabled, -1);
	gtk_tree_path_free (path);

	g_object_unref (chatroom);
}
#endif /* CHATROOM_AUTOCONNECT_IN_LIST */

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

	/* if a pixbuf, use it */
	if (pixbuf) {
		g_object_set (cell, 
			      "visible", TRUE,
			      "pixbuf", pixbuf,
			      NULL); 

		g_object_unref (pixbuf);
		return;
	}

	gtk_tree_model_get (model, iter, 
			    COL_POINTER, &chatroom,
			    -1);

	status = gossip_chatroom_get_status (chatroom);
	last_error = gossip_chatroom_get_last_error (chatroom);

	if (status == GOSSIP_CHATROOM_ERROR && !last_error) {
		status = GOSSIP_CHATROOM_CLOSED;
	}

	pixbuf = gossip_pixbuf_for_chatroom_status (chatroom, GTK_ICON_SIZE_MENU);
	g_object_unref (chatroom);
	
	g_object_set (cell, 
		      "visible", TRUE,
		      "pixbuf", pixbuf,
		      NULL); 

 	g_object_unref (pixbuf); 
}

static void  
chatrooms_window_model_text_cell_data_func (GtkTreeViewColumn     *tree_column,
					    GtkCellRenderer       *cell,
					    GtkTreeModel          *model,
					    GtkTreeIter           *iter,
					    GossipChatroomsWindow *window)
{
	GtkTreeView          *view;
	GtkTreeSelection     *selection;
	PangoAttrList        *attr_list;
	PangoAttribute       *attr_color, *attr_style, *attr_size;
	GtkStyle             *style;
	GdkColor              color;
	gchar                *str;
	const gchar          *last_error;
	gchar                *name;
	GossipChatroom       *chatroom;
	GossipChatroomStatus  status;
 	const gchar          *status_str;
	gboolean              selected = FALSE;

	attr_color = NULL;

	gtk_tree_model_get (model, iter, 
			    COL_POINTER, &chatroom, 
			    -1);
	name = g_strdup_printf ("%s (%s@%s)",
				gossip_chatroom_get_name (chatroom),
				gossip_chatroom_get_room (chatroom),
				gossip_chatroom_get_server (chatroom));

	status = gossip_chatroom_get_status (chatroom);
	last_error = gossip_chatroom_get_last_error (chatroom);
	g_object_unref (chatroom);

	if ((status == GOSSIP_CHATROOM_UNKNOWN) ||
	    (status == GOSSIP_CHATROOM_ERROR && !last_error)) {
		status = GOSSIP_CHATROOM_CLOSED;
	}

	if (status == GOSSIP_CHATROOM_ERROR) {
		status_str = last_error;
	} else {
		status_str = gossip_chatroom_get_status_as_str (status);
	}

	str = g_strdup_printf ("%s\n%s", name, status_str);

	/* get: is_selected */
	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);
	selected = gtk_tree_selection_iter_is_selected (selection, iter);

	/* make text look flashy */
 	style = gtk_widget_get_style (GTK_WIDGET (view));
	color = style->text_aa[GTK_STATE_NORMAL];

	attr_list = pango_attr_list_new ();

	attr_style = pango_attr_style_new (PANGO_STYLE_ITALIC);
	attr_style->start_index = strlen (name) + 1;
	attr_style->end_index = -1;
	pango_attr_list_insert (attr_list, attr_style);

  	if (!selected) {  
   		attr_color = pango_attr_foreground_new (color.red, color.green, color.blue);   
   		attr_color->start_index = attr_style->start_index;   
   		attr_color->end_index = -1;   
   		pango_attr_list_insert (attr_list, attr_color);   
   	}   

	attr_size = pango_attr_size_new (pango_font_description_get_size (style->font_desc) / 1.2);
	attr_size->start_index = attr_style->start_index;
	attr_size->end_index = -1;
	pango_attr_list_insert (attr_list, attr_size);

	g_object_set (cell,
		      "weight", PANGO_WEIGHT_NORMAL,
		      "text", str,
		      "attributes", attr_list,
		      NULL);
      
	pango_attr_list_unref (attr_list);

	g_free (str);
	g_free (name);
}

static GossipChatroom *
chatrooms_window_model_get_selected (GossipChatroomsWindow *window)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	GossipChatroom   *chatroom;

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter, COL_POINTER, &chatroom, -1);
	return chatroom;
}

static GossipChatroomStatus
chatrooms_window_model_status_selected (GossipChatroomsWindow *window)
{
	GtkTreeView          *view;
	GtkTreeModel         *model;
	GtkTreeSelection     *selection;
	GtkTreeIter           iter;
	GossipChatroom       *chatroom;
	GossipChatroomStatus  status;

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return FALSE;
	}

	gtk_tree_model_get (model, &iter, COL_POINTER, &chatroom, -1);
	status = gossip_chatroom_get_status (chatroom);
	g_object_unref (chatroom);

	return status;
}

static void
chatrooms_window_model_join_selected (GossipChatroomsWindow *window)
{
	GossipSession          *session;
	GossipAccount          *account;
	GossipAccountChooser   *account_chooser_chatroom;
	GossipChatroomProvider *provider;
	GossipChatroom         *chatroom;
	GossipChatroomId        id;
	GossipChatroomStatus    status;

	status = chatrooms_window_model_status_selected (window);
	if (status == GOSSIP_CHATROOM_CONNECTING) {
		chatrooms_window_update_join_button (window);
		return;
	}

	session = gossip_app_get_session ();

	account_chooser_chatroom = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser_chatroom);
	account = gossip_account_chooser_get_account (account_chooser_chatroom);

	provider = gossip_session_get_chatroom_provider (session, account);
	
	g_object_unref (account);

	/* get chatroom */
	chatroom = chatrooms_window_model_get_selected (window);	
	
	id = gossip_chatroom_provider_join (provider,
					    chatroom,
					    (GossipChatroomJoinCb)chatrooms_window_join_cb,
					    window);
	
	/* set model with data we need to remember */
	chatrooms_window_update_join_button (window);
	
	g_object_unref (chatroom);	

}

static void
chatrooms_window_model_cancel_selected (GossipChatroomsWindow *window)
{
	GossipSession          *session;
	GossipAccount          *account;
	GossipAccountChooser   *account_chooser;
	GossipChatroomProvider *provider;
	GossipChatroom         *chatroom;
	GossipChatroomId        id;
	
	GtkTreeView            *view;
	GtkTreeSelection       *selection;
	GtkTreeModel           *model;
	GtkTreeIter             iter;

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return;
	}

	gtk_tree_model_get (model, &iter, COL_POINTER, &chatroom, -1);

	session = gossip_app_get_session ();
	
	account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser_custom);
	account = gossip_account_chooser_get_account (account_chooser);
	
	provider = gossip_session_get_chatroom_provider (session, account);
	g_object_unref (account);
	
	id = gossip_chatroom_get_id (chatroom);
	gossip_chatroom_provider_cancel (provider, id);
	
	g_object_unref (chatroom);
}

static void
chatrooms_window_model_selection_changed (GtkTreeSelection      *selection,
					  GossipChatroomsWindow *window)
{
	GtkTreeModel *model;
	GtkTreeIter   iter;
	gboolean      is_selection;
		
	is_selection = gtk_tree_selection_get_selected (selection, &model, &iter);

	gtk_widget_set_sensitive (window->button_join, is_selection);
	gtk_widget_set_sensitive (window->button_edit, is_selection);
	gtk_widget_set_sensitive (window->button_delete, is_selection);

	chatrooms_window_update_join_button (window);

	/* set default chatroom */
	if (is_selection) {
		GossipChatroomManager *manager;
		GossipChatroom        *chatroom;

		manager = gossip_app_get_chatroom_manager ();
		chatroom = chatrooms_window_model_get_selected (window);
		
		gossip_chatroom_manager_set_default (manager, chatroom);
		gossip_chatroom_manager_store (manager);
	}
}

static void
chatrooms_window_model_refresh_data (GossipChatroomsWindow *window,
				     gboolean               first_time)
{
	GtkTreeView           *view;
	GtkTreeSelection      *selection;
	GtkTreeModel          *model;
	GtkListStore          *store;

	GossipSession         *session;
	GossipAccountChooser  *account_chooser_chatroom;
	GossipAccount         *account;
	GossipChatroomManager *manager;
	GossipChatroom        *default_chatroom;
	GList                 *chatrooms, *l;

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);

	/* look up chatrooms */
	session = gossip_app_get_session ();

	account_chooser_chatroom = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser_chatroom);
	account = gossip_account_chooser_get_account (account_chooser_chatroom);

	manager = gossip_app_get_chatroom_manager ();
	chatrooms = gossip_chatroom_manager_get_chatrooms (manager, account);

	default_chatroom = gossip_chatroom_manager_get_default (manager);
	if (default_chatroom) {
		window->last_selected_id = gossip_chatroom_get_id (default_chatroom);
	}

	/* clear store */
	gtk_list_store_clear (store);

	/* populate with chatroom list */
	for (l = chatrooms; l; l = l->next) {
		GossipChatroom *chatroom;
		gboolean        set_active = FALSE;

		chatroom = l->data;

		if (l == chatrooms) {
			set_active = TRUE;
		}
		
		if (gossip_chatroom_equal (chatroom, default_chatroom)) {
			set_active = TRUE;
		}

		chatrooms_window_model_add (window, chatroom, 
					       set_active, first_time);
	}

	if (first_time) {
		/* if no accounts set tab to custom */
	}

	g_object_unref (account);
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

	/* add to model */
 	view = GTK_TREE_VIEW (window->treeview); 
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);
	
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    COL_NAME, gossip_chatroom_get_name (chatroom),
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
chatrooms_window_model_remove (GossipChatroomsWindow *window,
				  GossipChatroom        *chatroom)
{
	GossipChatroomManager *manager;
	GtkTreeView           *view;
	GtkTreeModel          *model;

	/* remove from config */
	manager = gossip_app_get_chatroom_manager ();

	gossip_chatroom_manager_remove (manager, chatroom);
	gossip_chatroom_manager_store (manager);

	/* remove from treeview */
 	view = GTK_TREE_VIEW (window->treeview); 
	model = gtk_tree_view_get_model (view);

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc)chatrooms_window_delete_foreach,
				chatroom);

	/* select next iter */
}

static void
chatrooms_window_model_setup (GossipChatroomsWindow *window)
{
	GtkListStore     *store;
	GtkTreeSelection *selection;

	store = gtk_list_store_new (COL_COUNT,
				    GDK_TYPE_PIXBUF,       /* image */
				    G_TYPE_STRING,         /* text */
				    G_TYPE_BOOLEAN,        /* auto start */
				    GOSSIP_TYPE_CHATROOM); /* chatroom */ 
	
	gtk_tree_view_set_model (GTK_TREE_VIEW (window->treeview), 
				 GTK_TREE_MODEL (store));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect (selection, "changed", 
			  G_CALLBACK (chatrooms_window_model_selection_changed), window);

 	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),  
 					      COL_NAME, GTK_SORT_ASCENDING); 
	 
	chatrooms_window_model_add_columns (window);

	g_object_unref (store);

	chatrooms_window_model_refresh_data (window, TRUE);
}

static void
chatrooms_window_update_join_button (GossipChatroomsWindow *window)
{
	GossipChatroomManager *manager;
	GList                 *chatrooms;
	const gchar           *nickname;
	const gchar           *server;
	const gchar           *room;
	gboolean               disabled = FALSE;

	GtkButton             *button;
	GtkWidget             *image;
	GossipChatroomStatus   status;

	gint                   page;

	/* first get button and icon */
	button = GTK_BUTTON (window->button_join);

	image = gtk_button_get_image (button);
	if (!image) {
		image = gtk_image_new ();
		gtk_button_set_image (button, image);
	}

#ifndef BROKEN_NOTEBOOK_API
	/* FIXME: this API is not very reliable, see switch-page callback */
	page = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));
#else
	page = window->page;
#endif
	
	/* now sort out button name and sensitivity */
	switch (page) {
	case PAGE_CHATROOM: 
		status = chatrooms_window_model_status_selected (window);

		switch (status) {
		case GOSSIP_CHATROOM_CONNECTING:
			gtk_button_set_use_stock (button, TRUE);
			gtk_button_set_label (button, GTK_STOCK_CANCEL);
			
			gtk_image_set_from_stock (GTK_IMAGE (image), 
						  GTK_STOCK_CANCEL,
						  GTK_ICON_SIZE_BUTTON);
			break;
		case GOSSIP_CHATROOM_OPEN:
		case GOSSIP_CHATROOM_CLOSED:
		case GOSSIP_CHATROOM_ERROR:
		case GOSSIP_CHATROOM_UNKNOWN:
			gtk_button_set_use_stock (button, FALSE);
			gtk_button_set_label (button, _("Join"));
			gtk_image_set_from_stock (GTK_IMAGE (image), 
						  GTK_STOCK_EXECUTE,
						  GTK_ICON_SIZE_BUTTON);
			break;
		}

		gtk_widget_set_sensitive (GTK_WIDGET (button), 
					  (status != GOSSIP_CHATROOM_OPEN));

		break;

	case PAGE_CUSTOM:
		if (window->joining_chatroom) {
			gtk_button_set_use_stock (button, TRUE);
			gtk_button_set_label (button, GTK_STOCK_CANCEL);
			
			gtk_image_set_from_stock (GTK_IMAGE (image), 
						  GTK_STOCK_CANCEL,
						  GTK_ICON_SIZE_BUTTON);
			
			disabled = FALSE;
		} else {
			gtk_button_set_use_stock (button, FALSE);
			gtk_button_set_label (button, _("Join"));
			gtk_image_set_from_stock (GTK_IMAGE (image), 
						  GTK_STOCK_EXECUTE,
						  GTK_ICON_SIZE_BUTTON);

			nickname = gtk_entry_get_text (GTK_ENTRY (window->entry_nickname));
			disabled |= !nickname || nickname[0] == 0;
			
			server = gtk_entry_get_text (GTK_ENTRY (window->entry_server));
			disabled |= !server || server[0] == 0;
			
			room = gtk_entry_get_text (GTK_ENTRY (window->entry_room));
			disabled |= !room || room[0] == 0;

			if (!disabled) {
				manager = gossip_app_get_chatroom_manager ();
				chatrooms = gossip_chatroom_manager_find_extended (manager, nickname, server, room);
				
				if (chatrooms) {
					gtk_widget_set_sensitive (window->checkbutton_add, FALSE);
				} else {
					gtk_widget_set_sensitive (window->checkbutton_add, TRUE);
				}
				
				g_list_foreach (chatrooms, (GFunc)g_object_unref, NULL);
				g_list_free (chatrooms);
			}
		}

		gtk_widget_set_sensitive (window->button_join, !disabled);
		break;
	}
}

static void
chatrooms_window_join_custom (GossipChatroomsWindow *window)
{
	GossipSession          *session;

	GossipAccount          *account;
	GossipAccountChooser   *account_chooser_custom;
	
	GossipChatroomManager  *manager;
	GossipChatroomProvider *provider;
	GossipChatroom         *chatroom;

	gchar                  *name;

	const gchar            *room;
	const gchar            *server;
	const gchar            *nickname;

	session = gossip_app_get_session ();

	account_chooser_custom = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser_custom);
	account = gossip_account_chooser_get_account (account_chooser_custom);
	
	manager = gossip_app_get_chatroom_manager ();
	provider = gossip_session_get_chatroom_provider (session, account);

	nickname = gtk_entry_get_text (GTK_ENTRY (window->entry_nickname));
	server = gtk_entry_get_text (GTK_ENTRY (window->entry_server));
	room   = gtk_entry_get_text (GTK_ENTRY (window->entry_room));

	name = g_strdup_printf ("%s@%s", room, server);
			
	chatroom = g_object_new (GOSSIP_TYPE_CHATROOM,
				 "name", name, 
				 "nick", nickname, 
				 "server", server,
				 "room", room, 
				 "account", account,
				 NULL);	

	g_free (name);

	g_object_unref (account);

	/* should we save the chatroom? */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (window->checkbutton_add))) {
		gossip_chatroom_manager_add (manager, chatroom);
		chatrooms_window_model_add (window, chatroom, FALSE, TRUE);
	}

	/* remember this chatroom */
	window->joining_chatroom = chatroom;

	/* change widgets so they are unsensitive */
	gtk_widget_set_sensitive (window->label_preamble_custom, FALSE);
	gtk_widget_set_sensitive (window->hbox_account_custom, FALSE);
	gtk_widget_set_sensitive (window->table_details, FALSE);

 	gtk_widget_show (window->table_progress); 

	chatrooms_window_update_join_button (window);

	/* connect change signal to update the progress label */
	window->joining_chatroom_pulse_id = 
		g_timeout_add (50,
			       (GSourceFunc) chatrooms_window_progress_pulse_cb,
			       window->progressbar);

	window->joining_chatroom_change_id = 
		g_signal_connect (chatroom, "notify",
				  G_CALLBACK (chatrooms_window_chatroom_changed_cb), 
				  window);

	/* now do the join */
	gossip_chatroom_provider_join (provider,
				       chatroom,
				       (GossipChatroomJoinCb)chatrooms_window_join_cb,
				       window);
}

static void
chatrooms_window_join_stop (GossipChatroomsWindow *window)
{
	const gchar *last_error;

	gtk_widget_set_sensitive (window->label_preamble_custom, TRUE);
	gtk_widget_set_sensitive (window->hbox_account_custom, TRUE);
	gtk_widget_set_sensitive (window->table_details, TRUE);


	last_error = gossip_chatroom_get_last_error (window->joining_chatroom);
	if (!last_error) {
		gtk_widget_hide (window->table_progress);
	} else {
		GdkPixbuf *pixbuf;

		gtk_label_set_text (GTK_LABEL (window->label_progress_detail),
				    last_error);	

		pixbuf = gossip_pixbuf_for_chatroom_status (window->joining_chatroom,
							    GTK_ICON_SIZE_BUTTON);
		gtk_image_set_from_pixbuf (GTK_IMAGE (window->image_progress),
					   pixbuf);
		g_object_unref (pixbuf);

		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (window->progressbar), 0);
	}

	if (window->joining_chatroom_pulse_id != 0) {
		g_source_remove (window->joining_chatroom_pulse_id);
		window->joining_chatroom_pulse_id = 0;
	}

	if (window->joining_chatroom_change_id != 0) {
		g_signal_handler_disconnect (window->joining_chatroom,
					     window->joining_chatroom_change_id);
		window->joining_chatroom_change_id = 0;
	}

	if (window->joining_chatroom) {
		g_object_unref (window->joining_chatroom);
		window->joining_chatroom = NULL;
	}
}

static void
chatrooms_window_join_cancel (GossipChatroomsWindow *window)
{
	if (window->joining_chatroom) {
		GossipSession          *session;
		GossipAccount          *account;
		GossipAccountChooser   *account_chooser;
		GossipChatroomProvider *provider;
		GossipChatroomId        id;

		session = gossip_app_get_session ();
		
		account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser_custom);
		account = gossip_account_chooser_get_account (account_chooser);

		provider = gossip_session_get_chatroom_provider (session, account);
		g_object_unref (account);

		id = gossip_chatroom_get_id (window->joining_chatroom);
		gossip_chatroom_provider_cancel (provider, id);
	}

	chatrooms_window_join_stop (window);
	chatrooms_window_update_join_button (window);
}

static gboolean 
chatrooms_window_progress_pulse_cb (GtkWidget *progressbar)
{
	g_return_val_if_fail (progressbar != NULL, FALSE);
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (progressbar));

	return TRUE;
}

static void
chatrooms_window_join_cb (GossipChatroomProvider   *provider,
			  GossipChatroomJoinResult  result,
			  GossipChatroomId          id,
			  GossipChatroomsWindow    *window)
{
	g_return_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider));
	g_return_if_fail (window != NULL);
	
	if (result == GOSSIP_CHATROOM_JOIN_OK ||
	    result == GOSSIP_CHATROOM_JOIN_ALREADY_OPEN) {
		gossip_group_chat_show (provider, id);
	} 

	if (window->joining_chatroom && 
	    gossip_chatroom_get_id (window->joining_chatroom) == id) {
		chatrooms_window_join_stop (window);
	}

	chatrooms_window_update_join_button (window);
}

static void
chatrooms_window_notebook_switch_page_cb (GtkNotebook           *notebook,
					  GtkNotebookPage       *new_page,
					  gint                   old_page,
					  GossipChatroomsWindow *window)
{
#ifdef BROKEN_NOTEBOOK_API
	if (window->page == PAGE_CHATROOM) {
		window->page = PAGE_CUSTOM;
	} else {
		window->page = PAGE_CHATROOM;
	}
#endif

	chatrooms_window_update_join_button (window);
}

static void
chatrooms_window_edit_clicked_cb (GtkWidget             *widget,
				  GossipChatroomsWindow *window)
{
	GossipChatroom *chatroom;
	GtkWidget      *child;

	chatroom = chatrooms_window_model_get_selected (window);
	child = chatroom_edit_dialog_show (chatroom);

	gtk_window_set_transient_for (GTK_WINDOW (child), 
				      GTK_WINDOW (window->window));

	g_object_unref (chatroom);
}

static void
chatrooms_window_delete_clicked_cb (GtkWidget             *widget,
				    GossipChatroomsWindow *window)
{
	GossipChatroom *chatroom;

	chatroom = chatrooms_window_model_get_selected (window);
	chatrooms_window_model_remove (window, chatroom);
	
	g_object_unref (chatroom);	
}

static gboolean 
chatrooms_window_delete_foreach (GtkTreeModel   *model,
				 GtkTreePath    *path,
				 GtkTreeIter    *iter,
				 GossipChatroom *chatroom_to_delete)
{
	GossipChatroom *chatroom;
	gboolean        equal = FALSE;

	gtk_tree_model_get (model, iter, COL_POINTER, &chatroom, -1);
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
chatrooms_window_join_clicked_cb (GtkWidget             *widget,
				  GossipChatroomsWindow *window)
{
	GossipChatroomStatus status;
	gint                 page;

#ifndef BROKEN_NOTEBOOK_API
	/* FIXME: this API is not very reliable, see switch-page callback */
	page = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));
#else
	page = window->page;
#endif

	switch (page) {
	case PAGE_CHATROOM:
		status = chatrooms_window_model_status_selected (window);

		if (status == GOSSIP_CHATROOM_CONNECTING) {
			chatrooms_window_model_cancel_selected (window);
		} else {
			chatrooms_window_model_join_selected (window);
		}
		break;

	case PAGE_CUSTOM:
		if (window->joining_chatroom) {
			chatrooms_window_join_cancel (window);
		} else {
			chatrooms_window_join_custom (window);
		}
		break;
	}
}

static void
chatrooms_window_close_clicked_cb (GtkWidget             *widget,
				   GossipChatroomsWindow *window)
{
	gtk_widget_hide (window->window);
}

static void
chatrooms_window_entry_changed_cb (GtkWidget             *widget,
				   GossipChatroomsWindow *window)
{
	chatrooms_window_update_join_button (window);
}

static void
chatrooms_window_chatroom_changed_cb (GossipChatroom        *chatroom,
				      GParamSpec            *param,
				      GossipChatroomsWindow *window)
{
	switch (window->page) {
	case PAGE_CHATROOM: {
		GtkTreeModel *model = NULL;

		model = gtk_tree_view_get_model (GTK_TREE_VIEW (window->treeview)); 
		
		gtk_tree_model_foreach (model,
					(GtkTreeModelForeachFunc) 
					chatrooms_window_chatroom_changed_foreach,
					chatroom);
		break;
	}	

	case PAGE_CUSTOM: {
		GossipChatroomStatus  status;
		GdkPixbuf            *pixbuf;
		const gchar          *status_str;

		status = gossip_chatroom_get_status (chatroom);
		status_str = gossip_chatroom_get_status_as_str (status);

		gtk_label_set_text (GTK_LABEL (window->label_progress_detail),
				    status_str);

		pixbuf = gossip_pixbuf_for_chatroom_status (window->joining_chatroom,
							    GTK_ICON_SIZE_BUTTON);
		gtk_image_set_from_pixbuf (GTK_IMAGE (window->image_progress),
					   pixbuf);
		g_object_unref (pixbuf);

		break;
	}
	}

	chatrooms_window_update_join_button (window);
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
		gtk_tree_model_row_changed (model, path, iter);
	}

	g_object_unref (chatroom);
	return equal;
}

static void
chatrooms_window_account_chatroom_changed_cb (GtkWidget             *combo_box,
					      GossipChatroomsWindow *window)
{
	chatrooms_window_model_refresh_data (window, FALSE);
}

void
gossip_chatrooms_window_show (gboolean show_chatrooms)
{
	static GossipChatroomsWindow *window = NULL;
	GladeXML                     *glade;
	GossipSession                *session;
	GList                        *accounts;
	gint                          account_num;
	GossipAccountChooser         *account_chooser_chatroom;
	GossipAccount                *account;
	GossipChatroomManager        *manager;
	GList                        *chatrooms;
	gint                          chatroom_num;
	GtkSizeGroup                 *size_group;
	guint                         connected;

	if (window) {
		gtk_window_present (GTK_WINDOW (window->window));
		return;
	}		
	
        window = g_new0 (GossipChatroomsWindow, 1);

	glade = gossip_glade_get_file (GLADEDIR "/group-chat.glade",
				       "chatrooms_window",
				       NULL,
				       "chatrooms_window", &window->window,
				       "notebook", &window->notebook,
				       "hbox_account_chatroom", &window->hbox_account_chatroom,
				       "label_account_chatroom", &window->label_account_chatroom,
				       "treeview", &window->treeview,
				       "button_edit", &window->button_edit,
				       "button_delete", &window->button_delete,
				       "label_preamble_custom", &window->label_preamble_custom,
				       "hbox_account_custom", &window->hbox_account_custom,
				       "label_account_custom", &window->label_account_custom,
				       "table_details", &window->table_details,
 				       "label_nickname", &window->label_nickname, 
 				       "entry_nickname", &window->entry_nickname, 
				       "label_server", &window->label_server,
				       "entry_server", &window->entry_server,
				       "label_room", &window->label_room,
				       "entry_room", &window->entry_room,
				       "checkbutton_add", &window->checkbutton_add,
				       "table_progress", &window->table_progress,
				       "label_progress", &window->label_progress,
				       "image_progress", &window->image_progress,
				       "label_progress_detail", &window->label_progress_detail,
				       "progressbar", &window->progressbar,
				       "button_close", &window->button_close,
				       "button_join", &window->button_join,
				       NULL);
	
	gossip_glade_connect (glade, 
			      window,
			      "notebook", "switch-page", chatrooms_window_notebook_switch_page_cb,
			      "button_edit", "clicked", chatrooms_window_edit_clicked_cb,
			      "button_delete", "clicked", chatrooms_window_delete_clicked_cb,
			      "button_join", "clicked", chatrooms_window_join_clicked_cb,
			      "button_close", "clicked", chatrooms_window_close_clicked_cb,
			      "entry_nickname", "changed", chatrooms_window_entry_changed_cb,
			      "entry_server", "changed", chatrooms_window_entry_changed_cb,
			      "entry_room", "changed", chatrooms_window_entry_changed_cb,
			      NULL);

	g_signal_connect_swapped (window->window, "delete_event",
				  G_CALLBACK (gtk_widget_hide_on_delete), 
				  window->window);

	/* look and feel - aligning... */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, window->label_account_chatroom);
	gtk_size_group_add_widget (size_group, window->label_account_custom);
	gtk_size_group_add_widget (size_group, window->label_nickname);
	gtk_size_group_add_widget (size_group, window->label_server);
	gtk_size_group_add_widget (size_group, window->label_room);
	gtk_size_group_add_widget (size_group, window->label_progress);

	g_object_unref (size_group);

	/* get the session and chat room manager */
	session = gossip_app_get_session ();
	manager = gossip_app_get_chatroom_manager ();

	/* account chooser for chat rooms */
	window->account_chooser_chatroom = gossip_account_chooser_new (session);
	gtk_box_pack_start (GTK_BOX (window->hbox_account_chatroom), 
			    window->account_chooser_chatroom,
			    TRUE, TRUE, 0);

	g_signal_connect (window->account_chooser_chatroom, "changed",
			  G_CALLBACK (chatrooms_window_account_chatroom_changed_cb),
			  window);

	gtk_widget_show (window->account_chooser_chatroom);

	/* account chooser for custom */
	window->account_chooser_custom = gossip_account_chooser_new (session);
	gtk_box_pack_start (GTK_BOX (window->hbox_account_custom), 
			    window->account_chooser_custom,
			    TRUE, TRUE, 0);

	gtk_widget_show (window->account_chooser_custom);

	/* populate */
	accounts = gossip_session_get_accounts (session);
	account_num = g_list_length (accounts);

	g_list_foreach (accounts, (GFunc)g_object_unref, NULL);
	g_list_free (accounts);

	if (account_num > 1) {
		gtk_widget_show (window->hbox_account_chatroom);
		gtk_widget_show (window->hbox_account_custom);
	} else {
		/* show no accounts combo box */	
		gtk_widget_hide (window->hbox_account_chatroom);
		gtk_widget_hide (window->hbox_account_custom);
	}

	/* get connected accounts */
	gossip_session_count_accounts (session, NULL, NULL, &connected);

	/* get number of chatrooms */
	account_chooser_chatroom = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser_chatroom);
	account = gossip_account_chooser_get_account (account_chooser_chatroom);

	chatrooms = gossip_chatroom_manager_get_chatrooms (manager, account);
	chatroom_num = g_list_length (chatrooms);

	g_list_free (chatrooms);

	/* do some logic for which page to show */
	if (connected == 1) {
		if (chatroom_num == 0) {
			/* show customise tab */
			gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), 
						       PAGE_CUSTOM);
			gtk_widget_grab_focus (window->entry_nickname);
		} else {
			gtk_widget_grab_focus (window->treeview);
		}
	}

	if (show_chatrooms) {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), 
					       PAGE_CHATROOM);
	}

	/* set up chatrooms */
	chatrooms_window_model_setup (window);

	/* last touches */
	gtk_window_set_transient_for (GTK_WINDOW (window->window), 
				      GTK_WINDOW (gossip_app_get_window ()));

	gtk_widget_show (window->window);
}

/*
 * Edit chatroom
 */

static void
chatroom_edit_dialog_set (ChatroomEditDialog *dialog)
{
	GossipChatroomManager *manager;
	GtkToggleButton       *togglebutton;
	const gchar           *str;

	manager = gossip_app_get_chatroom_manager ();

	/* set chatroom information */
	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_name));
	g_object_set (dialog->chatroom, "name", str, NULL);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_nickname));
	g_object_set (dialog->chatroom, "nick", str, NULL);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_server));
	g_object_set (dialog->chatroom, "server", str, NULL);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_room));
	g_object_set (dialog->chatroom, "room", str, NULL);

	togglebutton = GTK_TOGGLE_BUTTON (dialog->checkbutton_auto_connect);
	g_object_set (dialog->chatroom, "auto_connect", 
		      gtk_toggle_button_get_active (togglebutton), NULL);

	gossip_chatroom_manager_store (manager);
}

static void
chatroom_edit_dialog_entry_changed_cb (GtkEntry           *entry,
				       ChatroomEditDialog *dialog)
{
	const gchar *str;
	gboolean     disabled = FALSE;

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_nickname));
	disabled |= !str || str[0] == 0;

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_server));
	disabled |= !str || str[0] == 0;

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_room));
	disabled |= !str || str[0] == 0;

	gtk_widget_set_sensitive (dialog->button_save, !disabled);
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
				     "entry_name", &dialog->entry_name,
				     "entry_nickname", &dialog->entry_nickname,
				     "entry_server", &dialog->entry_server,
				     "entry_room", &dialog->entry_room,
				     "checkbutton_auto_connect", &dialog->checkbutton_auto_connect,
				     "button_save", &dialog->button_save,
				     NULL);
	
	gossip_glade_connect (gui, 
			      dialog,
			      "chatroom_edit_dialog", "destroy", chatroom_edit_dialog_destroy_cb,
			      "chatroom_edit_dialog", "response", chatroom_edit_dialog_response_cb,
			      "entry_name", "changed", chatroom_edit_dialog_entry_changed_cb,
			      "entry_nickname", "changed", chatroom_edit_dialog_entry_changed_cb,
			      "entry_server", "changed", chatroom_edit_dialog_entry_changed_cb,
			      "entry_room", "changed", chatroom_edit_dialog_entry_changed_cb,
			      NULL);

	gtk_entry_set_text (GTK_ENTRY (dialog->entry_name), 
			    gossip_chatroom_get_name (chatroom));
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_nickname), 
			    gossip_chatroom_get_nick (chatroom));
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_server),
			    gossip_chatroom_get_server (chatroom));
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_room), 
			    gossip_chatroom_get_room (chatroom));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->checkbutton_auto_connect),
		gossip_chatroom_get_auto_connect (chatroom));
				      
 	return dialog->dialog;
}
