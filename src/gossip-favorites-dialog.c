/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002 Richard Hult <richard@imendio.com>
 * Copyright (C) 2002 Mikael Hallendal <micke@imendio.com>
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
#include "gossip-utils.h"
#include "gossip-app.h"
#include "gossip-favorites-dialog.h"
#include "gossip-favorite.h"

typedef struct {
	GossipApp    *app;
	
	GtkWidget    *dialog;
	GtkWidget    *chats_list;
	GtkEntry     *name_entry;
	GtkEntry     *room_entry;
	GtkEntry     *nick_entry;
	GtkEntry     *server_entry;
	GtkWidget    *add_button;
	GtkWidget    *remove_button;

	GtkListStore *model;
} GossipFavoritesDialog;

enum {
	COL_NAME,
	COL_FAVORITE,
	NUM_COLS
};

static void favorites_dialog_destroy_cb            (GtkWidget             *widget,
						    GossipFavoritesDialog  *dialog);
static void favorites_dialog_response_cb           (GtkWidget             *widget,
						    gint                   response,
						    GossipFavoritesDialog  *dialog);
static void favorites_dialog_add_favorite_cb        (GtkWidget             *widget,
						     GossipFavoritesDialog  *dialog);
static void favorites_dialog_remove_favorite_cb     (GtkWidget             *widget,
						     GossipFavoritesDialog  *dialog);
static gboolean favorites_dialog_update_favorite_cb (GtkWidget             *widget,
						     GdkEventFocus         *event,
						     GossipFavoritesDialog  *dialog);
static void favorites_dialog_set_entries            (GossipFavoritesDialog  *dialog,
						     const gchar            *favorite_name,
						     const gchar            *nick,
						     const gchar            *room,
						     const gchar            *server);
static void favorites_dialog_rebuild_list           (GossipFavoritesDialog  *dialog);
static void favorites_dialog_selection_changed_cb   (GtkTreeSelection     *selection,
						     GossipFavoritesDialog *dialog);

static void
favorites_dialog_destroy_cb (GtkWidget            *widget,
			       GossipFavoritesDialog *dialog)
{
	g_free (dialog);
}

static void
favorites_dialog_response_cb (GtkWidget            *widget,
				gint                  response,
				GossipFavoritesDialog *dialog)
{
	switch (response) {
	default:
		break;
	}

	gtk_widget_destroy (widget);
}

static void
favorites_dialog_set_favorite_information (GossipFavoritesDialog *dialog,
					   GossipFavorite        *favorite,
					   gchar                 *old_name)
{
	gchar *path;
	gchar *key;

	if (old_name) {
		gchar *old_path = g_strdup_printf ("%s/Favorite: %s",
						   GOSSIP_FAVORITES_PATH,
						   old_name);
		gnome_config_clean_section (old_path);
		g_free (old_path);
	}

	path = g_strdup_printf ("%s/Favorite: %s", 
				GOSSIP_FAVORITES_PATH, favorite->name);
	
	key = g_strdup_printf ("%s/nick", path);
	gnome_config_set_string (key, favorite->nick);
	g_free (key);
	
	key = g_strdup_printf ("%s/room", path);
	gnome_config_set_string (key, favorite->room);
	g_free (key);

	key = g_strdup_printf ("%s/server", path);
	gnome_config_set_string (key, favorite->server);
	g_free (key);
	
	g_free (path);

 	gnome_config_sync_file (GOSSIP_FAVORITES_PATH);
}

static void
favorites_dialog_add_favorite_cb (GtkWidget            *widget,
				GossipFavoritesDialog *dialog)
{
	GtkTreeIter       iter;
	GtkTreeSelection *selection;
	GossipFavorite    *favorite;

	gtk_list_store_append (GTK_LIST_STORE (dialog->model), &iter);

	favorite = gossip_favorite_new ("New favorite", NULL, NULL, NULL);
	
	gtk_list_store_set (GTK_LIST_STORE (dialog->model),
			    &iter,
			    COL_NAME, favorite->name,
			    COL_FAVORITE, favorite,
			    -1);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->chats_list));
	
	gtk_tree_selection_select_iter (selection, &iter);

	favorites_dialog_set_favorite_information (dialog, favorite, NULL);
}

static void
favorites_dialog_remove_favorite_cb (GtkWidget            *widget,
				       GossipFavoritesDialog *dialog)
{
	GossipFavorite    *favorite;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	gchar            *path;
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->chats_list));
	gtk_tree_selection_get_selected (selection, NULL, &iter);
	
	gtk_tree_model_get (GTK_TREE_MODEL (dialog->model),
			    &iter,
			    COL_FAVORITE, &favorite,
			    -1);

	path = g_strdup_printf ("%s/Favorite: %s", 
				GOSSIP_FAVORITES_PATH, favorite->name);
	gnome_config_clean_section (path);
	g_free (path);

	favorites_dialog_rebuild_list (dialog);
	/* Move all favorites that is after the removed one! */
	gnome_config_sync_file (GOSSIP_FAVORITES_PATH);
}

static gboolean
favorites_dialog_update_favorite_cb (GtkWidget             *widget,
				     GdkEventFocus         *event,
				     GossipFavoritesDialog *dialog)
{
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	GossipFavorite    *favorite;
	gchar            *old_name = NULL;
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->chats_list));
	
	gtk_tree_selection_get_selected (selection, NULL, &iter);
	
	gtk_tree_model_get (GTK_TREE_MODEL (dialog->model),
			    &iter,
			    COL_FAVORITE, &favorite,
			    -1);
	
	if (widget == GTK_WIDGET (dialog->name_entry)) {
		old_name = favorite->name;
		favorite->name = g_strdup (gtk_entry_get_text (dialog->name_entry));
		gtk_list_store_set (GTK_LIST_STORE (dialog->model),
				    &iter,
				    COL_NAME, favorite->name,
				    COL_FAVORITE, favorite,
				    -1);
	}
	else if (widget == GTK_WIDGET (dialog->nick_entry)) {
		g_free (favorite->nick);
		favorite->nick = g_strdup (gtk_entry_get_text (dialog->nick_entry));
	}
	else if (widget == GTK_WIDGET (dialog->room_entry)) {
		g_free (favorite->room);
		favorite->room = g_strdup (gtk_entry_get_text (dialog->room_entry));
	}
	else if (widget == GTK_WIDGET (dialog->server_entry)) {
		g_free (favorite->server);
		favorite->server = g_strdup (gtk_entry_get_text (dialog->server_entry));
	}

	favorites_dialog_set_favorite_information (dialog, favorite, old_name);
	g_free (old_name);
	
	return FALSE;
}

static void
favorites_dialog_set_entries (GossipFavoritesDialog *dialog,
			      const gchar           *favorite_name,
			      const gchar           *nick,
			      const gchar           *room,
			      const gchar           *server) 
{
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->name_entry), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->nick_entry), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->room_entry), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->server_entry), TRUE);
	gtk_entry_set_text (dialog->name_entry, favorite_name);
	gtk_entry_set_text (dialog->nick_entry, nick);
	gtk_entry_set_text (dialog->room_entry, room);
	gtk_entry_set_text (dialog->server_entry, server);
}
 
static void
favorites_dialog_rebuild_list (GossipFavoritesDialog *dialog)
{
	GSList      *favorites, *l;
	GtkTreeIter  iter;
	
	favorites = gossip_favorite_get_all ();
	
	gtk_list_store_clear (GTK_LIST_STORE (dialog->model));

	for (l = favorites; l; l = l->next) {
		GossipFavorite *favorite = (GossipFavorite *) l->data;
	
		gtk_list_store_append (GTK_LIST_STORE (dialog->model), &iter);
		
		gtk_list_store_set (GTK_LIST_STORE (dialog->model),
				    &iter,
				    COL_NAME, favorite->name,
				    COL_FAVORITE, favorite,
				    -1);
	}

	g_slist_free (favorites);
}

static void
favorites_dialog_selection_changed_cb (GtkTreeSelection     *selection,
				      GossipFavoritesDialog *dialog)
{
	GtkTreeIter    iter;
	GossipFavorite *favorite;
	
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		gtk_widget_set_sensitive (dialog->remove_button, FALSE);
		favorites_dialog_set_entries (dialog, "", "", "", "");
		return;
 	}
	
	gtk_widget_set_sensitive (dialog->remove_button, TRUE);
		
	gtk_tree_model_get (GTK_TREE_MODEL (dialog->model),
			    &iter,
			    COL_FAVORITE, &favorite, 
			    -1);

	favorites_dialog_set_entries (dialog, 
				     favorite->name, favorite->nick, 
				     favorite->room, favorite->server);
}

GtkWidget *
gossip_favorites_dialog_show (GossipApp *app)
{
	GossipFavoritesDialog *dialog;
	GtkTreeViewColumn     *column;
	GtkCellRenderer       *cell;
	GtkTreeSelection      *selection;
	
        dialog = g_new0 (GossipFavoritesDialog, 1);
	dialog->app = app;

	gossip_glade_get_file_simple (GLADEDIR "/group-chat.glade",
				      "favorites_dialog",
				      NULL,
				      "favorites_dialog", &dialog->dialog,
				      "chats_list", &dialog->chats_list,
				      "name_entry", &dialog->name_entry,
				      "nick_entry", &dialog->nick_entry,
				      "room_entry", &dialog->room_entry,
				      "server_entry", &dialog->server_entry,
				      "add_button", &dialog->add_button,
				      "remove_button", &dialog->remove_button,
				      NULL);
	
	g_signal_connect (dialog->dialog,
			  "destroy",
			  G_CALLBACK (favorites_dialog_destroy_cb),
			  dialog);

	g_signal_connect (dialog->dialog,
			  "response",
			  G_CALLBACK (favorites_dialog_response_cb),
			  dialog);

	g_signal_connect (dialog->add_button,
			  "clicked",
			  G_CALLBACK (favorites_dialog_add_favorite_cb),
			  dialog);

	g_signal_connect (dialog->remove_button,
			  "clicked",
			  G_CALLBACK (favorites_dialog_remove_favorite_cb),
			  dialog);

	g_signal_connect (dialog->name_entry,
			  "focus-out-event",
			  G_CALLBACK (favorites_dialog_update_favorite_cb),
			  dialog);
	
  	g_signal_connect (dialog->nick_entry,
			  "focus-out-event",
			  G_CALLBACK (favorites_dialog_update_favorite_cb),
			  dialog);
	
	g_signal_connect (dialog->room_entry,
			  "focus-out-event",
			  G_CALLBACK (favorites_dialog_update_favorite_cb),
			  dialog);

	g_signal_connect (dialog->server_entry,
			  "focus-out-event",
			  G_CALLBACK (favorites_dialog_update_favorite_cb),
			  dialog);
	
	dialog->model = gtk_list_store_new (NUM_COLS,
					    G_TYPE_STRING,
					    G_TYPE_POINTER);

	gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->chats_list),
				 GTK_TREE_MODEL (dialog->model));
 	/*g_object_unref (dialog->model);*/
	
	cell = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (NULL,
							   cell,
							   "text", COL_NAME,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->chats_list),
				     column);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->chats_list));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (favorites_dialog_selection_changed_cb),
			  dialog);
		
	favorites_dialog_rebuild_list (dialog);

	gtk_widget_set_sensitive (dialog->remove_button, FALSE);

  	gtk_window_set_modal (GTK_WINDOW (dialog->dialog), TRUE);
	gtk_widget_show (dialog->dialog);

 	return dialog->dialog;
}

