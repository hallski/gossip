/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2004 Imendio AB
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
#include <glib/gi18n.h>

#include <libgossip/gossip-chatroom-provider.h>
#include "gossip-group-chat.h"
#include "gossip-app.h"
#include "gossip-favorite.h"
#include "gossip-edit-favorite-dialog.h"
#include "gossip-join-dialog.h"

#define FAVORITES_PATH "/apps/gossip/group_chat_favorites"

#define JOIN_TIMEOUT   20000

typedef struct _GossipJoinDialog GossipJoinDialog;

struct _GossipJoinDialog {
	GtkWidget        *dialog;
	GtkWidget        *preamble_label;
	GtkWidget        *details_table;
	GtkWidget        *favorite_combobox;
	GtkWidget        *nickname_entry;
	GtkWidget        *server_entry;
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
};


typedef struct {
	GossipJoinDialog *dialog;
	GossipFavorite   *favorite;
} FindFavorite;


static void     join_dialog_setup_favorites       (GossipJoinDialog         *dialog,
						   gboolean                  reload);
static void     join_dialog_cancel                (GossipJoinDialog         *dialog,
						   gboolean                  cleanup_ui,
						   gboolean                  cancel_join);
static gboolean join_dialog_progress_pulse_cb     (GtkWidget                *progressbar);
static gboolean join_dialog_wait_cb               (GossipJoinDialog         *dialog);
static gboolean join_dialog_timeout_cb            (GossipJoinDialog         *dialog);
static gboolean join_dialog_select_favorite_cb    (GtkTreeModel             *model,
						   GtkTreePath              *path,
						   GtkTreeIter              *iter,
						   FindFavorite             *ff);
static gboolean join_dialog_is_separator_cb       (GtkTreeModel             *model,
						   GtkTreeIter              *iter,
						   gpointer                  data);
static void     join_dialog_join_cb               (GossipChatroomProvider   *provider,
						   GossipChatroomJoinResult  result,
						   GossipChatroomId          id,
						   gpointer                  user_data);
static void     join_dialog_edit_clicked_cb       (GtkWidget                *widget,
						   GossipJoinDialog         *dialog);
static void     join_dialog_delete_clicked_cb     (GtkWidget                *widget,
						   GossipJoinDialog         *dialog);
static void     join_dialog_entry_changed_cb      (GtkWidget                *widget,
						   GossipJoinDialog         *dialog);
static void     join_dialog_reload_config_cb      (GtkWidget                *widget,
						   GossipJoinDialog         *dialog);
static void     join_dialog_response_cb           (GtkWidget                *widget,
						   gint                      response,
						   GossipJoinDialog         *dialog);
static void     join_dialog_destroy_cb            (GtkWidget                *unused,
						   GossipJoinDialog         *dialog);


static GossipJoinDialog *current_dialog = NULL;


static void
join_dialog_setup_favorites (GossipJoinDialog *dialog, 
			     gboolean          reload)
{
	GtkListStore   *store;
 	GtkTreeIter     iter; 
 	GSList         *favorites, *l; 
 	gchar          *default_name; 
 	gint            i; 

        GtkCellRenderer *renderer;
	GtkComboBox     *combobox;

	combobox = GTK_COMBO_BOX (dialog->favorite_combobox);

	if (!reload) {
		gtk_cell_layout_clear (GTK_CELL_LAYOUT (combobox));  
		
		store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
		
		gtk_combo_box_set_model (combobox,
					 GTK_TREE_MODEL (store));

		renderer = gtk_cell_renderer_pixbuf_new ();
		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, FALSE);
		gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
						"stock-id", 0,
						NULL);
		
		renderer = gtk_cell_renderer_text_new ();
		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
		gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
						"text", 1,
						NULL);
		
		gtk_combo_box_set_row_separator_func (combobox, 
						      join_dialog_is_separator_cb, 
						      NULL, NULL);
	} else {
		store = GTK_LIST_STORE (gtk_combo_box_get_model (combobox));

		/* clear current entries */
		gtk_list_store_clear (store);
	}

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, "gtk-stock-new",
                            1, _("Custom"),
                            -1);

	gtk_combo_box_set_active_iter (GTK_COMBO_BOX (dialog->favorite_combobox), 
				       &iter);

	favorites = gossip_favorite_get_all ();
	if (favorites) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    0, NULL,
				    1, "separator",
				    -1);
	}

	if (!reload) {
		g_object_unref (store);
	}

	default_name = gnome_config_get_string (GOSSIP_FAVORITES_PATH 
						"/Favorites/Default");

	for (i = 0, l = favorites; l; i++, l = l->next) {
		GossipFavorite *favorite;

		favorite = l->data;
		
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 1, favorite->name, -1);

		if (i == 0) {
			/* use first item as default if there is none */
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (dialog->favorite_combobox),
						       &iter);
		}

		if (default_name && !strcmp (default_name, favorite->name)) {
			/* if set use the default item as first selection */
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (dialog->favorite_combobox), 
						       &iter);
		}
	}
}

static void
join_dialog_cancel (GossipJoinDialog *dialog, 
		    gboolean          cleanup_ui,
		    gboolean          cancel_join)
{
	if (cleanup_ui) {
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
		GossipChatroomProvider *provider;
			
		if (cancel_join) {
			provider = gossip_session_get_chatroom_provider (gossip_app_get_session ());
			gossip_chatroom_provider_cancel (provider, dialog->joining_id);
		}

		dialog->joining_id = 0;
		dialog->joining = FALSE;
	}
}

static gboolean 
join_dialog_progress_pulse_cb (GtkWidget *progressbar)
{
	g_return_val_if_fail (progressbar != NULL, FALSE);
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (progressbar));

	return TRUE;
}

static gboolean
join_dialog_wait_cb (GossipJoinDialog *dialog)
{
	gtk_widget_show (dialog->joining_vbox);

	dialog->pulse_id = g_timeout_add (50, 
					  (GSourceFunc)join_dialog_progress_pulse_cb, 
					  dialog->joining_progressbar);

	return FALSE;
}

static gboolean
join_dialog_timeout_cb (GossipJoinDialog *dialog)
{
	GtkWidget *md;

	join_dialog_cancel (dialog, TRUE, TRUE);
	
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
join_dialog_select_favorite_cb (GtkTreeModel *model,
				GtkTreePath  *path,
				GtkTreeIter  *iter,
				FindFavorite *ff)
{
	GossipJoinDialog *dialog;
	GossipFavorite   *favorite;
	gchar            *name;
	gboolean          found = FALSE; 

	dialog = ff->dialog;
	favorite = ff->favorite;

	gtk_tree_model_get (model, iter, 1, &name, -1);
	if (!strcmp (name, favorite->name)) {
		found = TRUE;
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (dialog->favorite_combobox), 
					       iter);
	}
	
	g_free (name);
	return found;
}

static void
join_dialog_join_cb (GossipChatroomProvider   *provider,
		     GossipChatroomJoinResult  result,
		     GossipChatroomId          id,
		     gpointer                  user_data)
{
	GossipJoinDialog *dialog;
	GtkWidget        *md;
	const gchar      *str1 = NULL;
	const gchar      *str2 = NULL;

	/* FIXME: should be better */
	dialog = current_dialog;

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
		join_dialog_cancel (dialog, TRUE, FALSE);
		
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

static gboolean
join_dialog_is_separator_cb (GtkTreeModel *model,
			     GtkTreeIter  *iter,
			     gpointer      data)
{
	GtkTreePath *path;
	gboolean     result;

	if (!gossip_favorite_get_all ()) {
		return FALSE;
	}

	path = gtk_tree_model_get_path (model, iter);
	result = (gtk_tree_path_get_indices (path)[0] == 1);
	gtk_tree_path_free (path);
	
	return result;
}

static void
join_dialog_edit_clicked_cb (GtkWidget        *widget,
			     GossipJoinDialog *dialog)
{
	GtkTreeModel   *model;
	GtkTreeIter     iter;
	GtkWidget      *window;
	GossipFavorite *favorite;
	gchar          *name;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->favorite_combobox));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->favorite_combobox), &iter);

	gtk_tree_model_get (model, &iter, 1, &name, -1);

	favorite = gossip_favorite_get (name);
	g_free (name);

	window = gossip_edit_favorite_dialog_show (favorite);

	g_signal_connect (GTK_WIDGET (window),
			  "destroy",
			  G_CALLBACK (join_dialog_reload_config_cb),
			  dialog);

	gtk_window_set_transient_for (GTK_WINDOW (window), 
				      GTK_WINDOW (dialog->dialog));

}

static void
join_dialog_delete_clicked_cb (GtkWidget        *widget,
			       GossipJoinDialog *dialog)
{
	GtkTreeModel   *model;
	GtkTreeIter     iter;
	GossipFavorite *favorite;
	gchar          *name;
	gchar          *path;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->favorite_combobox));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->favorite_combobox), &iter);

	gtk_tree_model_get (model, &iter, 1, &name, -1);

	favorite = gossip_favorite_get (name);
	g_free (name);

	path = g_strdup_printf ("%s/Favorite: %s", 
				GOSSIP_FAVORITES_PATH, favorite->name);
	gnome_config_clean_section (path);
	g_free (path);

	gnome_config_sync_file (GOSSIP_FAVORITES_PATH);

	join_dialog_setup_favorites (dialog, TRUE);	
}

static void
join_dialog_favorite_combobox_changed_cb (GtkWidget        *widget,
					  GossipJoinDialog *dialog)
{
	GtkTreeModel   *model;
	GtkTreeIter     iter;
	gchar          *name = NULL;
	GSList         *favorites, *l;
	GossipFavorite *favorite = NULL;
	gboolean        custom = FALSE;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->favorite_combobox));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->favorite_combobox), &iter);

	gtk_tree_model_get (model, &iter, 1, &name, -1);

	if (!strcmp (name, _("Custom"))) {
		custom = TRUE;
	}

	gtk_widget_set_sensitive (dialog->add_checkbutton, custom);

	if (!custom) {
		gtk_widget_set_sensitive (dialog->join_button, TRUE);
		gtk_widget_set_sensitive (dialog->edit_button, TRUE);
		gtk_widget_set_sensitive (dialog->delete_button, TRUE);
	} else {
		gtk_widget_set_sensitive (dialog->edit_button, FALSE);
		gtk_widget_set_sensitive (dialog->delete_button, FALSE);
	}

	if (custom && dialog->changed) {
		return;
	}

	favorites = gossip_favorite_get_all ();
	for (l = favorites; l; l = l->next) {
		favorite = l->data;
		if (favorite->name && name &&
		    !strcmp (favorite->name, name)) {
			break;
		} else {
			favorite = NULL;
		}
	}
	
	g_free (name);
	
	if (!favorite) {
		return;
	}

	dialog->changed = FALSE;
		
	gtk_entry_set_text (GTK_ENTRY (dialog->nickname_entry), favorite->nick);
	gtk_entry_set_text (GTK_ENTRY (dialog->server_entry), favorite->server);
	gtk_entry_set_text (GTK_ENTRY (dialog->room_entry), favorite->room);

	gnome_config_set_string (GOSSIP_FAVORITES_PATH "/Favorites/Default",
				 favorite->name);
	gnome_config_sync_file (GOSSIP_FAVORITES_PATH);
}

static void
join_dialog_entry_changed_cb (GtkWidget        *widget,
			      GossipJoinDialog *dialog)
{
	GossipFavorite *favorite = NULL;
	const gchar    *nickname;
	const gchar    *server;
	const gchar    *room;
	gboolean        disabled = FALSE;

	dialog->changed = TRUE;

	nickname = gtk_entry_get_text (GTK_ENTRY (dialog->nickname_entry));
	disabled |= !nickname || nickname[0] == 0;

	server = gtk_entry_get_text (GTK_ENTRY (dialog->server_entry));
	disabled |= !server || server[0] == 0;

	room = gtk_entry_get_text (GTK_ENTRY (dialog->room_entry));
	disabled |= !room || room[0] == 0;

	favorite = gossip_favorite_find (nickname, server, room);
	if (favorite) {
		GtkTreeModel *model;
		FindFavorite *ff;

		ff = g_new0 (FindFavorite, 1);

		ff->dialog = dialog;
		ff->favorite = gossip_favorite_ref (favorite);

		model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->favorite_combobox));
		gtk_tree_model_foreach (model,
					(GtkTreeModelForeachFunc)join_dialog_select_favorite_cb,
					ff);

		gossip_favorite_unref (ff->favorite);
		g_free (ff);

		return;
	} else {
		gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->favorite_combobox), 0);
	}

	gtk_widget_set_sensitive (dialog->join_button, !disabled);
}

static void
join_dialog_response_cb (GtkWidget        *widget,
			 gint              response,
			 GossipJoinDialog *dialog)
{
	if (response == GTK_RESPONSE_OK) {
		GossipChatroomProvider *provider;
		GossipChatroomId        id;
		const gchar            *room;
		const gchar            *server;
		const gchar            *nickname;
	
		nickname = gtk_entry_get_text (GTK_ENTRY (dialog->nickname_entry));
		server = gtk_entry_get_text (GTK_ENTRY (dialog->server_entry));
		room   = gtk_entry_get_text (GTK_ENTRY (dialog->room_entry));

		/* should we add to favourites? */
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->add_checkbutton))) {
			GossipFavorite *favorite;
			gchar          *path;
			gchar          *key;
			gchar          *tmpname;

			tmpname = g_strdup_printf ("%s@%s as %s", room, server, nickname);
			favorite = gossip_favorite_new (tmpname, nickname, room, server);
			g_free (tmpname);

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

			/* set default */
			gnome_config_set_string (GOSSIP_FAVORITES_PATH "/Favorites/Default",
						 favorite->name);

			/* save all */
			gnome_config_sync_file (GOSSIP_FAVORITES_PATH);

			join_dialog_setup_favorites (dialog, TRUE);
		}

		/* change widgets so they are unsensitive */
		gtk_widget_set_sensitive (dialog->preamble_label, FALSE);
		gtk_widget_set_sensitive (dialog->details_table, FALSE);
		gtk_widget_set_sensitive (dialog->join_button, FALSE);

		dialog->wait_id = g_timeout_add (2000, 
						 (GSourceFunc)join_dialog_wait_cb,
						 dialog);

		dialog->timeout_id = g_timeout_add (JOIN_TIMEOUT, 
						    (GSourceFunc)join_dialog_timeout_cb,
						    dialog);

		/* now do the join */
		provider = gossip_session_get_chatroom_provider (gossip_app_get_session ());

		id = gossip_chatroom_provider_join (provider,
						    room, server, nickname, NULL,
						    join_dialog_join_cb,
						    NULL);

		dialog->joining = TRUE;
		dialog->joining_id = id;

		return;
	}

	if (response == GTK_RESPONSE_CANCEL && dialog->joining) {
		/* change widgets so they are unsensitive */
		join_dialog_cancel (dialog, TRUE, TRUE);

		return;
	}

	gtk_widget_destroy (widget);
}

static void 
join_dialog_reload_config_cb (GtkWidget        *widget, 
			      GossipJoinDialog *dialog)
{
	join_dialog_setup_favorites (dialog, TRUE);	
}

static void 
join_dialog_destroy_cb (GtkWidget        *widget, 
			GossipJoinDialog *dialog)
{
	join_dialog_cancel (dialog, FALSE, FALSE);

	g_free (dialog);
	
	current_dialog = NULL;
}

void
gossip_join_dialog_show (void)
{
	GossipJoinDialog *dialog;
	GladeXML         *gui;

	if (current_dialog) {
		gtk_window_present (GTK_WINDOW (current_dialog->dialog));
		return;
	}		
	
        current_dialog = dialog = g_new0 (GossipJoinDialog, 1);

	gui = gossip_glade_get_file (GLADEDIR "/group-chat.glade",
				     "join_group_chat_dialog",
				     NULL,
				     "join_group_chat_dialog", &dialog->dialog,
				     "preamble_label", &dialog->preamble_label,
				     "details_table", &dialog->details_table,
				     "favorite_combobox", &dialog->favorite_combobox,
				     "nickname_entry", &dialog->nickname_entry,
				     "server_entry", &dialog->server_entry,
				     "room_entry", &dialog->room_entry,
				     "edit_button", &dialog->edit_button,
				     "delete_button", &dialog->delete_button,
				     "add_checkbutton", &dialog->add_checkbutton,
				     "joining_vbox", &dialog->joining_vbox,
				     "joining_progressbar", &dialog->joining_progressbar,
				     "join_button", &dialog->join_button,
				     NULL);
	
	gossip_glade_connect (gui, 
			      dialog,
			      "join_group_chat_dialog", "destroy", join_dialog_destroy_cb,
			      "join_group_chat_dialog", "response", join_dialog_response_cb,
			      "favorite_combobox", "changed", join_dialog_favorite_combobox_changed_cb,
			      "edit_button", "clicked", join_dialog_edit_clicked_cb,
			      "delete_button", "clicked", join_dialog_delete_clicked_cb,
			      "nickname_entry", "changed", join_dialog_entry_changed_cb,
			      "server_entry", "changed", join_dialog_entry_changed_cb,
			      "room_entry", "changed", join_dialog_entry_changed_cb,
			      "nickname_entry", "changed", join_dialog_entry_changed_cb,
			      NULL);

	gtk_widget_set_sensitive (dialog->join_button, FALSE);

	join_dialog_setup_favorites (dialog, FALSE);

	gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog), 
				      GTK_WINDOW (gossip_app_get_window ()));

	gtk_widget_show (dialog->dialog);
}
