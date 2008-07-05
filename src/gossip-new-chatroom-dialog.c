/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006-2007 Imendio AB
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
#include <libgossip/gossip-jabber.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-chatroom-provider.h>
#include <libgossip/gossip-utils.h>
#include <libgossip/gossip-chatroom-manager.h>
#include <libgossip/gossip-stock.h>

#include "gossip-account-chooser.h"
#include "gossip-app.h"
#include "gossip-chatrooms-window.h"
#include "gossip-glade.h"
#include "gossip-new-chatroom-dialog.h"
#include "gossip-ui-utils.h"
#include "ephy-spinner.h"

#define DEBUG_DOMAIN "NewChatroomDialog"

typedef struct {
	GtkWidget        *window;

	GtkWidget        *vbox_widgets;

	GtkWidget        *hbox_account;
	GtkWidget        *label_account;
	GtkWidget        *account_chooser;

	GtkWidget        *hbox_server;
	GtkWidget        *label_server;
	GtkWidget        *entry_server;
	GtkWidget        *togglebutton_refresh;
	
	GtkWidget        *hbox_room;
	GtkWidget        *label_room;
	GtkWidget        *entry_room;

	GtkWidget        *hbox_nick;
	GtkWidget        *label_nick;
	GtkWidget        *entry_nick;

	GtkWidget        *hbox_password;
	GtkWidget        *label_password;
	GtkWidget        *entry_password;

	GtkWidget        *image_status;
	GtkWidget        *label_status;
	GtkWidget        *hbox_status;
	GtkWidget        *button_size;
	GtkWidget        *throbber;
	GtkWidget        *treeview;
	GtkTreeModel     *model;
	GtkTreeModel     *filter_model;
	GtkTreeModel     *sort_model;

	GtkWidget        *button_join;
	GtkWidget        *button_close;

	GossipChatroomId  last_selected_id;
} GossipNewChatroomDialog;

static void     new_chatroom_dialog_update_buttons                  (GossipNewChatroomDialog *dialog);
static void     new_chatroom_dialog_update_widgets                  (GossipNewChatroomDialog *dialog);
static void     new_chatroom_dialog_model_add                       (GossipNewChatroomDialog *dialog,
								     GossipChatroom          *chatroom,
								     gboolean                 prepend);
static void     new_chatroom_dialog_model_clear                     (GossipNewChatroomDialog *dialog);
static GList *  new_chatroom_dialog_model_get_selected              (GossipNewChatroomDialog *dialog);
static gboolean new_chatroom_dialog_model_filter_func               (GtkTreeModel            *model,
								     GtkTreeIter             *iter,
								     GossipNewChatroomDialog *dialog);
static void     new_chatroom_dialog_model_row_activated_cb          (GtkTreeView             *tree_view,
								     GtkTreePath             *path,
								     GtkTreeViewColumn       *column,
								     GossipNewChatroomDialog *dialog);
static void     new_chatroom_dialog_model_row_inserted_cb           (GtkTreeModel            *model,
								     GtkTreePath             *path,
								     GtkTreeIter             *iter,
								     GossipNewChatroomDialog *dialog);
static void     new_chatroom_dialog_model_row_deleted_cb            (GtkTreeModel            *model,
								     GtkTreePath             *path,
								     GossipNewChatroomDialog *dialog);
static void     new_chatroom_dialog_model_selection_changed         (GtkTreeSelection        *selection,
								     GossipNewChatroomDialog *dialog);
static void     new_chatroom_dialog_model_pixbuf_cell_data_func     (GtkTreeViewColumn       *tree_column,
								     GtkCellRenderer         *cell,
								     GtkTreeModel            *model,
								     GtkTreeIter             *iter,
								     GossipNewChatroomDialog *dialog);
static void     new_chatroom_dialog_model_text_cell_data_func       (GtkTreeViewColumn       *tree_column,
								     GtkCellRenderer         *cell,
								     GtkTreeModel            *model,
								     GtkTreeIter             *iter,
								     GossipNewChatroomDialog *dialog);
static void     new_chatroom_dialog_model_setup                     (GossipNewChatroomDialog *dialog);
static void     new_chatroom_dialog_set_defaults                    (GossipNewChatroomDialog *dialog);
static void     new_chatroom_dialog_join                            (GossipNewChatroomDialog *window);
static void     new_chatroom_dialog_entry_changed_cb                (GtkWidget               *widget,
								     GossipNewChatroomDialog *window);
static void     new_chatroom_dialog_browse_cb                       (GossipChatroomProvider  *provider,
								     const gchar             *server,
								     GList                   *rooms,
								     GError                  *error,
								     GossipNewChatroomDialog *dialog);
static void     new_chatroom_dialog_browse_start                    (GossipNewChatroomDialog *dialog);
static void     new_chatroom_dialog_browse_stop                     (GossipNewChatroomDialog *dialog);
static void     new_chatroom_dialog_entry_server_activate_cb        (GtkWidget               *widget,
								     GossipNewChatroomDialog *window);
static void     new_chatroom_dialog_togglebutton_refresh_toggled_cb (GtkWidget               *widget,
								     GossipNewChatroomDialog *dialog);
static void     new_chatroom_dialog_response_cb                     (GtkWidget               *widget,
								     gint                     response,
								     GossipNewChatroomDialog *dialog);
static void     new_chatroom_dialog_destroy_cb                      (GtkWidget               *widget,
								     GossipNewChatroomDialog *dialog);

enum {
	COL_IMAGE,
	COL_NAME,
	COL_OCCUPANTS,
	COL_PASSWORD_PROTECTED,
	COL_DESCRIPTION,
	COL_POINTER,
	COL_COUNT
};

static GossipNewChatroomDialog *dialog_p = NULL;

static void
new_chatroom_dialog_update_buttons (GossipNewChatroomDialog *dialog)
{
	GtkButton            *button;
	GtkWidget            *image;
	GList                *chatrooms;
	GossipChatroom       *chatroom;
	GossipChatroomStatus  status = GOSSIP_CHATROOM_STATUS_UNKNOWN;
	gboolean              sensitive = TRUE;

	GtkTreeView          *view;
	GtkTreeModel         *model;
	guint                 items;

	const gchar          *server;
	const gchar          *room;
	const gchar          *nick;

	/* Sort out Join button. */
	button = GTK_BUTTON (dialog->button_join);
	
	image = gtk_button_get_image (button);
	if (!image) {
		image = gtk_image_new ();
		gtk_button_set_image (button, image);
	}

	server = gtk_entry_get_text (GTK_ENTRY (dialog->entry_server));
	room = gtk_entry_get_text (GTK_ENTRY (dialog->entry_room));
	nick = gtk_entry_get_text (GTK_ENTRY (dialog->entry_nick));

	/* Collect necessary information first. */
	view = GTK_TREE_VIEW (dialog->treeview);
	model = gtk_tree_view_get_model (view);
	items = gtk_tree_model_iter_n_children (model, NULL);
	
	chatrooms = new_chatroom_dialog_model_get_selected (dialog);
	chatroom = g_list_nth_data (chatrooms, 0);
	if (chatroom) {
		status = gossip_chatroom_get_status (chatroom);
		gtk_button_set_use_stock (button, FALSE);
		gtk_button_set_label (button, _("Join"));
		gtk_image_set_from_stock (GTK_IMAGE (image),
					  GTK_STOCK_EXECUTE,
					  GTK_ICON_SIZE_BUTTON);
	} else {
		if (items < 1 && !G_STR_EMPTY (room)) {
			gtk_button_set_use_stock (button, FALSE);
			gtk_button_set_label (button, _("Create"));
			gtk_image_set_from_stock (GTK_IMAGE (image),
						  GTK_STOCK_NEW,
						  GTK_ICON_SIZE_BUTTON);
		} else {
			gtk_button_set_use_stock (button, FALSE);
			gtk_button_set_label (button, _("Join"));
			gtk_image_set_from_stock (GTK_IMAGE (image),
						  GTK_STOCK_EXECUTE,
						  GTK_ICON_SIZE_BUTTON);
		}
	}
	
	/* Only allow browsing if we have a server */
	gtk_widget_set_sensitive (dialog->togglebutton_refresh, !G_STR_EMPTY (server));

	/* Only join if:
	 * 1. If we are not ALREADY in the room 
	 * 2. If we are creating the room
	 * 3. If we have items in the list and one or more is selected
	 */
	sensitive &= status != GOSSIP_CHATROOM_STATUS_ACTIVE;
	sensitive &= ((items < 1 && !G_STR_EMPTY (room)) || 
		      (items > 0 && chatroom) || 
		      (!G_STR_EMPTY (server) && !G_STR_EMPTY (nick) && !G_STR_EMPTY (room)));
	
	gtk_widget_set_sensitive (dialog->button_join, sensitive);

	g_list_foreach (chatrooms, (GFunc) g_object_unref, NULL);
	g_list_free (chatrooms);
}

static void
new_chatroom_dialog_update_widgets (GossipNewChatroomDialog *dialog)
{
	new_chatroom_dialog_set_defaults (dialog);
	new_chatroom_dialog_update_buttons (dialog);

	/* Final set up of the dialog */
	gtk_widget_grab_focus (dialog->entry_room);
}

static void
new_chatroom_dialog_account_changed_cb (GtkComboBox             *combobox,
					GossipNewChatroomDialog *dialog)
{
	new_chatroom_dialog_update_widgets (dialog);
}


static void
new_chatroom_dialog_model_add (GossipNewChatroomDialog *dialog,
			       GossipChatroom          *chatroom,
			       gboolean                 prepend)
{
	GtkTreeView      *view;
	GtkTreeSelection *selection;
	GtkListStore     *store;
	GtkTreeIter       iter;

	/* Add to model */
	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);
	store = GTK_LIST_STORE (dialog->model);

	if (prepend) {
		gtk_list_store_prepend (store, &iter);
	} else {
		gtk_list_store_append (store, &iter);
	}

	if (chatroom) {
		GossipChatroomFeature  features;
		const gchar           *stock_id = NULL;
		
		features = gossip_chatroom_get_features (chatroom);
		if (features & GOSSIP_CHATROOM_FEATURE_PASSWORD_PROTECTED) {
			stock_id = GTK_STOCK_DIALOG_AUTHENTICATION;
		}

		gtk_list_store_set (store, &iter,
				    COL_NAME, gossip_chatroom_get_name (chatroom),
				    COL_OCCUPANTS, gossip_chatroom_get_occupants (chatroom),
				    COL_PASSWORD_PROTECTED, stock_id,
				    COL_DESCRIPTION, gossip_chatroom_get_description (chatroom),
				    COL_POINTER, chatroom,
				    -1);
	}
}

static void
new_chatroom_dialog_model_clear (GossipNewChatroomDialog *dialog)
{
	GtkListStore *store;

	store = GTK_LIST_STORE (dialog->model);
	gtk_list_store_clear (store);
}

static GList *
new_chatroom_dialog_model_get_selected (GossipNewChatroomDialog *dialog)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	GossipChatroom   *chatroom;
	GList            *chatrooms = NULL;
	GList            *rows;
	GList            *l;

	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);

	rows = gtk_tree_selection_get_selected_rows (selection, NULL);
	for (l = rows; l; l = l->next) {
		if (!gtk_tree_model_get_iter (model, &iter, l->data)) {
			continue;
		}

		gtk_tree_model_get (model, &iter, COL_POINTER, &chatroom, -1);
		chatrooms = g_list_append (chatrooms, chatroom);
	}

	g_list_foreach (rows, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (rows);

	return chatrooms;
}

static gboolean
new_chatroom_dialog_model_filter_func (GtkTreeModel            *model,
				       GtkTreeIter             *iter,
				       GossipNewChatroomDialog *dialog)
{
	GossipChatroom *chatroom;
	const gchar    *room;
	const gchar    *text;
	gchar          *room_nocase;
	gchar          *text_nocase;
	gboolean        found = FALSE;

	gtk_tree_model_get (model, iter, COL_POINTER, &chatroom, -1);

	if (!chatroom) {
		return TRUE;
	}

	room = gossip_chatroom_get_room (chatroom);
	text = gtk_entry_get_text (GTK_ENTRY (dialog->entry_room));

	/* Casefold */
	room_nocase = g_utf8_casefold (room, -1);
	text_nocase = g_utf8_casefold (text, -1);

	/* Compare */
	if (g_utf8_strlen (text_nocase, -1) < 1 ||
	    strstr (room_nocase, text_nocase)) {
		found = TRUE;
	}

	g_object_unref (chatroom);

	g_free (room_nocase);
	g_free (text_nocase);

	return found;
}

static void
new_chatroom_dialog_model_row_activated_cb (GtkTreeView             *tree_view,
					    GtkTreePath             *path,
					    GtkTreeViewColumn       *column,
					    GossipNewChatroomDialog *dialog)
{
	gtk_widget_activate (dialog->button_join);
}

static void
new_chatroom_dialog_model_row_inserted_cb (GtkTreeModel            *model,
					   GtkTreePath             *path,
					   GtkTreeIter             *iter,
					   GossipNewChatroomDialog *dialog)
{
	new_chatroom_dialog_update_buttons (dialog);
}

static void
new_chatroom_dialog_model_row_deleted_cb (GtkTreeModel            *model,
					  GtkTreePath             *path,
					  GossipNewChatroomDialog *dialog)
{
	new_chatroom_dialog_update_buttons (dialog);
}

static void
new_chatroom_dialog_model_selection_changed (GtkTreeSelection      *selection,
					     GossipNewChatroomDialog *dialog)
{
	new_chatroom_dialog_update_buttons (dialog);
}

static void
new_chatroom_dialog_model_pixbuf_cell_data_func (GtkTreeViewColumn       *tree_column,
						 GtkCellRenderer         *cell,
						 GtkTreeModel            *model,
						 GtkTreeIter             *iter,
					      GossipNewChatroomDialog *dialog)
{
	GossipChatroom       *chatroom;
	GossipChatroomStatus  status;
	GossipChatroomError   last_error;
	GdkPixbuf            *pixbuf = NULL;

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

	if (!chatroom) {
		return;
	}

	status = gossip_chatroom_get_status (chatroom);
	last_error = gossip_chatroom_get_last_error (chatroom);

	if (status == GOSSIP_CHATROOM_STATUS_ERROR && 
	    last_error == GOSSIP_CHATROOM_ERROR_NONE) {
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

static void
new_chatroom_dialog_model_text_cell_data_func (GtkTreeViewColumn       *tree_column,
					       GtkCellRenderer         *cell,
					       GtkTreeModel            *model,
					       GtkTreeIter             *iter,
					       GossipNewChatroomDialog *dialog)
{
	gchar *name;

	gtk_tree_model_get (model, iter, COL_NAME, &name, -1);
	g_object_set (cell, "text", name, NULL);
	g_free (name);
}

static void
new_chatroom_dialog_model_setup (GossipNewChatroomDialog *dialog)
{
	GtkTreeView       *view;
	GtkListStore      *store;
	GtkTreeModel      *model;
	GtkTreeModel      *sort_model;
	GtkTreeModel      *filter_model;
	GtkTreeSortable   *sortable;
	GtkTreeSelection  *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer   *cell;

	if (dialog->filter_model) {
		g_object_unref (dialog->filter_model);
	}

	if (dialog->sort_model) {
		g_object_unref (dialog->sort_model);
	}

	/* View */
	view = GTK_TREE_VIEW (dialog->treeview);

	g_signal_connect (view, "row-activated",
			  G_CALLBACK (new_chatroom_dialog_model_row_activated_cb),
			  dialog);
	/* Create store */
	store = gtk_list_store_new (COL_COUNT,
				    GDK_TYPE_PIXBUF,       /* Image */
				    G_TYPE_STRING,         /* Name */
				    G_TYPE_UINT,           /* Occupants */
				    G_TYPE_STRING,         /* Password Protected */
				    G_TYPE_STRING,         /* Description */
				    GOSSIP_TYPE_CHATROOM); /* Chatroom */

	model = GTK_TREE_MODEL (store);
	dialog->model = model;

	/* Create filter model */
	filter_model = gtk_tree_model_filter_new (model, NULL);
	gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter_model),
						(GtkTreeModelFilterVisibleFunc)
						new_chatroom_dialog_model_filter_func,
						dialog,
						NULL);
	g_signal_connect (filter_model, "row-inserted",
			  G_CALLBACK (new_chatroom_dialog_model_row_inserted_cb),
			  dialog);
	g_signal_connect (filter_model, "row-deleted",
			  G_CALLBACK (new_chatroom_dialog_model_row_deleted_cb),
			  dialog);

	dialog->filter_model = filter_model;

	/* Create sort model */
	sort_model = gtk_tree_model_sort_new_with_model (filter_model);
	dialog->sort_model = sort_model;

	sortable = GTK_TREE_SORTABLE (sort_model);

	/* Set model to use in tree view */
	gtk_tree_view_set_model (view, sort_model);

	/* Selection */
	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	g_signal_connect (selection, "changed",
			  G_CALLBACK (new_chatroom_dialog_model_selection_changed), 
			  dialog);

	/* Columns */
	gtk_tree_view_set_headers_visible (view, FALSE);

	/* Columns: Rooms */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Rooms"));

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc)
						 new_chatroom_dialog_model_pixbuf_cell_data_func,
						 dialog,
						 NULL);

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell,
		      "xpad", (guint) 4,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);

	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc)
						 new_chatroom_dialog_model_text_cell_data_func,
						 dialog,
						 NULL);

  	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);  
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_resizable (column, TRUE);
 	gtk_tree_view_column_set_min_width (column, 120); 
	gtk_tree_view_column_set_visible (column, TRUE);
	gtk_tree_view_column_set_sort_column_id (column, COL_NAME);
	gtk_tree_view_append_column (view, column);

	/* Columns: Occupants */
	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell,
		      "xpad", (guint) 4,
		      NULL);

	column = gtk_tree_view_column_new_with_attributes (_("Users"), cell, 
							   "text", COL_OCCUPANTS, 
							   NULL);
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_resizable (column, FALSE);
	gtk_tree_view_column_set_visible (column, FALSE);
	gtk_tree_view_column_set_sort_column_id (column, COL_OCCUPANTS);
	gtk_tree_view_append_column (view, column);

	/* Columns: Password Protected */
	cell = gtk_cell_renderer_pixbuf_new ();
	g_object_set (cell,
		      "xpad", (guint) 4,
		      "stock-size", GTK_ICON_SIZE_SMALL_TOOLBAR,
		      NULL);

	column = gtk_tree_view_column_new_with_attributes (_("Locked"), cell, 
							   "stock-id", COL_PASSWORD_PROTECTED, 
							   NULL);
	gtk_tree_view_column_set_alignment (column, 0.5);
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_resizable (column, FALSE);
	gtk_tree_view_column_set_visible (column, TRUE);
	gtk_tree_view_column_set_sort_column_id (column, COL_PASSWORD_PROTECTED);
	gtk_tree_view_append_column (view, column);

	/* Columns: Description */
	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell,
		      "xpad", (guint) 4,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);

	column = gtk_tree_view_column_new_with_attributes (_("Description"), cell, 
							   "text", COL_DESCRIPTION, 
							   NULL);
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_resizable (column, FALSE);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_visible (column, FALSE);
	gtk_tree_view_column_set_sort_column_id (column, COL_DESCRIPTION);
	gtk_tree_view_append_column (view, column);

	/* Set default sorting */
	gtk_tree_sortable_set_sort_column_id (sortable, COL_NAME, GTK_SORT_ASCENDING);
}

static void
new_chatroom_dialog_set_defaults (GossipNewChatroomDialog *dialog)
{
	GossipSession        *session;
	GossipAccount        *account;
	GossipAccountChooser *account_chooser;
	gchar                *server;
	const gchar          *nick;
	const gchar          *id;

	session = gossip_app_get_session ();

	account_chooser = GOSSIP_ACCOUNT_CHOOSER (dialog->account_chooser);
	account = gossip_account_chooser_get_account (account_chooser);

	if (!account) {
		return;
	}
	
	nick = gossip_session_get_nickname (session, account);
	if (nick) {
		gtk_entry_set_text (GTK_ENTRY (dialog->entry_nick), nick);
	}

	id = gossip_account_get_id (account);
	server = gossip_jabber_get_default_server (id);
	if (server) {
		gchar *conference_server;
		
		conference_server = g_strconcat ("conference.", server, NULL);
		gtk_entry_set_text (GTK_ENTRY (dialog->entry_server),
				    conference_server);
		g_free (conference_server);
		g_free (server);
	}
}

static void
new_chatroom_dialog_join (GossipNewChatroomDialog *dialog)
{
	GossipSession          *session;

	GossipAccount          *account;
	GossipAccountChooser   *account_chooser;

	GossipChatroomProvider *provider;
	GossipChatroom         *chatroom = NULL;
	GossipGroupChat        *chat;

	gboolean                new_chatroom;

	const gchar            *room;
	const gchar            *server;
	const gchar            *nick;
	const gchar            *password;

	GtkTreeView            *view;
	GtkTreeModel           *model;
	
	GList                  *chatrooms;
	guint                   items_selected;

	/* Collect necessary information first. */
	view = GTK_TREE_VIEW (dialog->treeview);
	model = gtk_tree_view_get_model (view);

	server = gtk_entry_get_text (GTK_ENTRY (dialog->entry_server));
	room = gtk_entry_get_text (GTK_ENTRY (dialog->entry_room));
	password = gtk_entry_get_text (GTK_ENTRY (dialog->entry_password));

	/* Account information */
	session = gossip_app_get_session ();

	account_chooser = GOSSIP_ACCOUNT_CHOOSER (dialog->account_chooser);
	account = gossip_account_chooser_get_account (account_chooser);

	if (!account) {
		return;
	}

	provider = gossip_session_get_chatroom_provider (session, account);

	/* Options */
	chatrooms = new_chatroom_dialog_model_get_selected (dialog);
	items_selected = g_list_length (chatrooms);

	g_list_foreach (chatrooms, (GFunc) g_object_unref, NULL);
	g_list_free (chatrooms);

	chatroom = g_list_nth_data (chatrooms, 0);
	new_chatroom = items_selected < 1;
	
	nick = gtk_entry_get_text (GTK_ENTRY (dialog->entry_nick));
	if (G_STR_EMPTY (nick)) {
		nick = gossip_session_get_nickname (session, account);
	}

	/* Set widget sensitivity */
	gtk_widget_set_sensitive (dialog->vbox_widgets, FALSE);
	
	/* New or existing? */
	if (new_chatroom) {
		GossipChatroomManager *chatroom_manager;

		chatroom_manager = gossip_session_get_chatroom_manager (session);
		chatroom = gossip_chatroom_manager_find_or_create (chatroom_manager, 
								   account, 
								   server, 
								   room,
								   NULL);

		gossip_chatroom_set_nick (chatroom, nick);

		if (!G_STR_EMPTY (password)) {
			gossip_chatroom_set_password (chatroom, password);
		}

		/* Now do the join */
		chat = gossip_group_chat_new (provider, chatroom);
		g_object_unref (chatroom);
	} else {
		GList *chatrooms, *l;

		chatrooms = new_chatroom_dialog_model_get_selected (dialog);
		for (l = chatrooms; l; l = l->next) {
			chatroom = l->data;

			/* Make sure we set the nick */
			gossip_chatroom_set_nick (chatroom, nick);

			if (!G_STR_EMPTY (password)) {
				gossip_chatroom_set_password (chatroom, password);
			}

			chat = gossip_group_chat_new (provider, chatroom);
		}
	}

	g_object_unref (account);
}

static void
new_chatroom_dialog_entry_changed_cb (GtkWidget               *entry,
				      GossipNewChatroomDialog *dialog)
{
	if (entry == dialog->entry_room) {
		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (dialog->filter_model));
	} 

	new_chatroom_dialog_update_buttons (dialog);
}

static void
new_chatroom_dialog_browse_cb (GossipChatroomProvider  *provider,
			       const gchar             *server,
			       GList                   *rooms,
			       GError                  *error,
			       GossipNewChatroomDialog *dialog)
{
	GList *l;
	gchar *str;

	if (!dialog_p) {
		return;
	}

	gossip_toggle_button_set_state_quietly (dialog->togglebutton_refresh, 
						G_CALLBACK (new_chatroom_dialog_togglebutton_refresh_toggled_cb),
						dialog,
						FALSE);

	ephy_spinner_stop (EPHY_SPINNER (dialog->throbber));

	gtk_widget_hide (dialog->throbber);
	gtk_widget_show (dialog->button_size);

	new_chatroom_dialog_model_clear (dialog);
	gtk_widget_set_sensitive (dialog->treeview, TRUE);

	if (error) {
		gtk_image_set_from_icon_name (GTK_IMAGE (dialog->image_status), 
					      GTK_STOCK_DIALOG_ERROR,
					      GTK_ICON_SIZE_BUTTON); 
		
		gtk_label_set_text (GTK_LABEL (dialog->label_status), error->message);
		
		return;
	}

 	gtk_image_set_from_icon_name (GTK_IMAGE (dialog->image_status), 
				      GTK_STOCK_FIND,
				      GTK_ICON_SIZE_BUTTON); 

	str = g_strdup_printf (ngettext ("Found %d conference room", 
					 "Found %d conference rooms", 
					 g_list_length (rooms)), 
			       g_list_length (rooms));
	gtk_label_set_text (GTK_LABEL (dialog->label_status), str);
	g_free (str);

	for (l = rooms; l; l = l->next) {
		new_chatroom_dialog_model_add (dialog, l->data, FALSE);
	}
}

static void
new_chatroom_dialog_browse_start (GossipNewChatroomDialog *dialog)
{
	GossipSession          *session;

	GossipAccount          *account;
	GossipAccountChooser   *account_chooser;

	GossipChatroomProvider *provider;
	const gchar            *server;

	server = gtk_entry_get_text (GTK_ENTRY (dialog->entry_server));
	if (G_STR_EMPTY (server)) {
		return;
	}

	/* Set UI */
	gossip_toggle_button_set_state_quietly (dialog->togglebutton_refresh, 
						G_CALLBACK (new_chatroom_dialog_togglebutton_refresh_toggled_cb),
						dialog,
						TRUE);

 	gtk_image_set_from_icon_name (GTK_IMAGE (dialog->image_status), 
				      GTK_STOCK_FIND,
				      GTK_ICON_SIZE_BUTTON); 

	gtk_label_set_text (GTK_LABEL (dialog->label_status), 
			    _("Browsing for conference rooms, please wait..."));

	gtk_widget_set_sensitive (dialog->treeview, FALSE);
	gtk_widget_show (dialog->throbber);
	gtk_widget_hide (dialog->button_size);

	ephy_spinner_start (EPHY_SPINNER (dialog->throbber));

	/* Fire off request */
	session = gossip_app_get_session ();

	account_chooser = GOSSIP_ACCOUNT_CHOOSER (dialog->account_chooser);
	account = gossip_account_chooser_get_account (account_chooser);
	
	if (!account) {
		return;
	}

	provider = gossip_session_get_chatroom_provider (session, account);

	gossip_chatroom_provider_browse_rooms (provider,
					       server,
					       (GossipChatroomBrowseCb) 
					       new_chatroom_dialog_browse_cb,
					       dialog);
}

static void
new_chatroom_dialog_browse_stop (GossipNewChatroomDialog *dialog)
{
	GossipSession          *session;

	GossipAccount          *account;
	GossipAccountChooser   *account_chooser;

	GossipChatroomProvider *provider;

	/* Set UI */
	gossip_toggle_button_set_state_quietly (dialog->togglebutton_refresh, 
						G_CALLBACK (new_chatroom_dialog_togglebutton_refresh_toggled_cb),
						dialog,
						FALSE);
	
 	gtk_image_set_from_icon_name (GTK_IMAGE (dialog->image_status), 
				      GTK_STOCK_FIND,
				      GTK_ICON_SIZE_BUTTON); 

	gtk_label_set_text (GTK_LABEL (dialog->label_status), 
			    _("Browsing cancelled!"));
	
	gtk_widget_set_sensitive (dialog->treeview, TRUE);
	gtk_widget_hide (dialog->throbber);
	gtk_widget_show (dialog->button_size);

	ephy_spinner_stop (EPHY_SPINNER (dialog->throbber));

	/* Fire off cancellation */
	session = gossip_app_get_session ();

	account_chooser = GOSSIP_ACCOUNT_CHOOSER (dialog->account_chooser);
	account = gossip_account_chooser_get_account (account_chooser);

	if (!account) {
		return;
	}

	provider = gossip_session_get_chatroom_provider (session, account);
	
	/* FIXME: NEED API Here */
}

static void
new_chatroom_dialog_entry_server_activate_cb (GtkWidget                *widget,
					      GossipNewChatroomDialog  *dialog)
{
	new_chatroom_dialog_browse_stop (dialog);
	
	gtk_widget_grab_focus (dialog->entry_room);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->togglebutton_refresh), TRUE);
}

static void
new_chatroom_dialog_button_size_clicked_cb (GtkWidget               *widget,
					    GossipNewChatroomDialog *dialog)
{
	GtkWidget         *image;
	GtkTreeViewColumn *column;
	gchar             *name;
	GtkIconSize        size;

	image = gtk_bin_get_child (GTK_BIN (widget));
	gtk_image_get_stock (GTK_IMAGE (image), &name, &size);

	if (!name) {
		return;
	}

	if (strcmp (name, GTK_STOCK_FULLSCREEN) == 0) {
		gtk_image_set_from_stock (GTK_IMAGE (image), GTK_STOCK_LEAVE_FULLSCREEN, size);
 		gtk_window_set_resizable (GTK_WINDOW (dialog->window), TRUE);

		/* Show certain columns */
		gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (dialog->treeview), TRUE);

		column = gtk_tree_view_get_column (GTK_TREE_VIEW (dialog->treeview), 0);
		gtk_tree_view_column_set_expand (column, FALSE);

		column = gtk_tree_view_get_column (GTK_TREE_VIEW (dialog->treeview), 1);
		gtk_tree_view_column_set_visible (column, TRUE);

		column = gtk_tree_view_get_column (GTK_TREE_VIEW (dialog->treeview), 3);
		gtk_tree_view_column_set_visible (column, TRUE);
	} else {
		gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (dialog->treeview), FALSE);

		gtk_image_set_from_stock (GTK_IMAGE (image), GTK_STOCK_FULLSCREEN, size);
 		gtk_window_set_resizable (GTK_WINDOW (dialog->window), FALSE);

		/* Hide certain columns */
		column = gtk_tree_view_get_column (GTK_TREE_VIEW (dialog->treeview), 0);
		gtk_tree_view_column_set_expand (column, TRUE);

		column = gtk_tree_view_get_column (GTK_TREE_VIEW (dialog->treeview), 1);
		gtk_tree_view_column_set_visible (column, FALSE);

		column = gtk_tree_view_get_column (GTK_TREE_VIEW (dialog->treeview), 3);
		gtk_tree_view_column_set_visible (column, FALSE);
	}
}

static void
new_chatroom_dialog_togglebutton_refresh_toggled_cb (GtkWidget               *widget,
						     GossipNewChatroomDialog *dialog)
{
	gboolean toggled;

	toggled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	
	if (toggled) {
		new_chatroom_dialog_browse_start (dialog);
	} else {
		new_chatroom_dialog_browse_stop (dialog);
	}
}

static void
new_chatroom_dialog_response_cb (GtkWidget               *widget,
				 gint                     response,
				 GossipNewChatroomDialog *dialog)
{
	if (response == GTK_RESPONSE_OK) {
		new_chatroom_dialog_join (dialog);
	}

	gtk_widget_destroy (widget);
}

static void
new_chatroom_dialog_destroy_cb (GtkWidget               *widget,
				GossipNewChatroomDialog *dialog)
{
	g_object_unref (dialog->model);
	g_object_unref (dialog->filter_model);
	g_object_unref (dialog->sort_model);
	
	g_free (dialog);
}

void
gossip_new_chatroom_dialog_show (GtkWindow *parent)
{
	GossipNewChatroomDialog *dialog;
	GladeXML                *glade;
	GossipSession           *session;
	GossipChatroomManager   *manager;
	GtkSizeGroup            *size_group;

	if (dialog_p) {
		gtk_window_present (GTK_WINDOW (dialog_p->window));
		return;
	}

	dialog_p = dialog = g_new0 (GossipNewChatroomDialog, 1);

	glade = gossip_glade_get_file ("group-chat.glade",
				       "new_chatroom_dialog",
				       NULL,
				       "new_chatroom_dialog", &dialog->window,
				       "hbox_account", &dialog->hbox_account,
				       "label_account", &dialog->label_account,
				       "vbox_widgets", &dialog->vbox_widgets,
				       "label_server", &dialog->label_server,
				       "label_room", &dialog->label_room,
				       "label_nick", &dialog->label_nick,
				       "label_password", &dialog->label_password,
				       "hbox_server", &dialog->hbox_server,
				       "hbox_room", &dialog->hbox_room,
				       "hbox_nick", &dialog->hbox_nick,
				       "hbox_password", &dialog->hbox_password,
				       "entry_server", &dialog->entry_server,
				       "entry_room", &dialog->entry_room,
				       "entry_nick", &dialog->entry_nick,
				       "entry_password", &dialog->entry_password,
				       "togglebutton_refresh", &dialog->togglebutton_refresh,
				       "image_status", &dialog->image_status,
				       "label_status", &dialog->label_status,
				       "hbox_status", &dialog->hbox_status,
				       "button_size", &dialog->button_size,
				       "treeview", &dialog->treeview,
				       "button_join", &dialog->button_join,
				       NULL);

	gossip_glade_connect (glade,
			      dialog,
			      "new_chatroom_dialog", "response", new_chatroom_dialog_response_cb,
			      "new_chatroom_dialog", "destroy", new_chatroom_dialog_destroy_cb,
			      "entry_nick", "changed", new_chatroom_dialog_entry_changed_cb,
			      "entry_server", "changed", new_chatroom_dialog_entry_changed_cb,
			      "entry_server", "activate", new_chatroom_dialog_entry_server_activate_cb,
			      "entry_room", "changed", new_chatroom_dialog_entry_changed_cb,
			      "button_size", "clicked", new_chatroom_dialog_button_size_clicked_cb,
			      "togglebutton_refresh", "toggled", new_chatroom_dialog_togglebutton_refresh_toggled_cb,
			      NULL);

	g_object_unref (glade);

	g_object_add_weak_pointer (G_OBJECT (dialog->window), (gpointer) &dialog_p);

	/* Label alignment */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, dialog->label_account);
	gtk_size_group_add_widget (size_group, dialog->label_server);
	gtk_size_group_add_widget (size_group, dialog->label_room);
	gtk_size_group_add_widget (size_group, dialog->label_nick);
	gtk_size_group_add_widget (size_group, dialog->label_password);

	g_object_unref (size_group);

	/* Get the session and chat room manager */
	session = gossip_app_get_session ();
	manager = gossip_app_get_chatroom_manager ();

	/* Account chooser for custom */
	dialog->account_chooser = gossip_account_chooser_new (session);
	g_signal_connect (GTK_COMBO_BOX (dialog->account_chooser), "changed",
			  G_CALLBACK (new_chatroom_dialog_account_changed_cb),
			  dialog);
	gtk_box_pack_start (GTK_BOX (dialog->hbox_account),
			    dialog->account_chooser,
			    TRUE, TRUE, 0);
	gtk_widget_show (dialog->account_chooser);

	/* Populate */
	if (gossip_account_chooser_get_count (GOSSIP_ACCOUNT_CHOOSER (dialog->account_chooser)) > 1) {
		gtk_widget_show (dialog->hbox_account);
	} else {
		/* Show no accounts combo box */
		gtk_widget_hide (dialog->hbox_account);
	}

	/* Add throbber */
	dialog->throbber = ephy_spinner_new ();
	ephy_spinner_set_size (EPHY_SPINNER (dialog->throbber), GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_widget_show (dialog->throbber);

	gtk_box_pack_start (GTK_BOX (dialog->hbox_status), dialog->throbber, 
			    FALSE, FALSE, 0);

	/* Set up chatrooms treeview */
	new_chatroom_dialog_model_setup (dialog);

	/* Set things up according to the account type */
	new_chatroom_dialog_update_widgets (dialog);

#if 0
	/* Populate rooms on current server */
 	new_chatroom_dialog_browse_start (dialog); 
#endif

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog->window),
					      GTK_WINDOW (parent));
	}

	gtk_widget_show (dialog->window);
}
