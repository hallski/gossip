/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Richard Hult <rhult@codefactory.se>
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
#include <loudmouth/loudmouth.h>
#include "gossip-utils.h"
#include "gossip-group-chat.h"
#include "gossip-app.h"
#include "gossip-favorite.h"
#include "gossip-favorites-dialog.h"
#include "gossip-join-dialog.h"

#define FAVORITES_PATH "/apps/gossip/group_chat_favorites"

struct _GossipJoinDialog {
	GossipApp *app;
	LmConnection *connection;
	
	GtkWidget *dialog;
	GtkEntry  *nick_entry;
	GtkEntry  *server_entry;
	GtkEntry  *room_entry;
	GtkWidget *option_menu;
	GtkWidget *edit_button;
	GtkWidget *join_button;
};

static void join_dialog_response_cb        (GtkWidget        *dialog,
					    gint              response,
					    GossipJoinDialog *join);
static void join_dialog_edit_clicked_cb    (GtkWidget        *widget,
					    GossipJoinDialog *dialog);
static void join_dialog_activate_cb        (GtkWidget        *widget,
					    GossipJoinDialog *join);
static void join_dialog_entries_changed_cb (GtkWidget        *widget,
					    GossipJoinDialog *join);
static void join_dialog_update_option_menu (GossipJoinDialog *join);


/*static void join_group_chat_cb      (GtkWidget        *widget,
				     GossipApp        *app);
*/

static void
join_dialog_update_option_menu (GossipJoinDialog *join)
{
	
	GSList    *favorites, *l;
	gchar     *default_name;
	GtkWidget *menu;
	GtkWidget *item;
	GtkWidget *default_item = NULL;
	gint       i;
	gint       default_index = 0;
	
	default_name = gnome_config_get_string (GOSSIP_FAVORITES_PATH "/Favorites/Default");

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (join->option_menu));
	if (menu) {
		gtk_widget_destroy (menu);
	}
	
	menu = gtk_menu_new ();

	favorites = gossip_favorite_get_all ();
	
	for (i = 0, l = favorites; l; i++, l = l->next) {
		GossipFavorite *favorite = l->data;

		item = gtk_menu_item_new_with_label (favorite->name);
		gtk_widget_show (item);

		if (default_name && !strcmp (default_name, favorite->name)) {
			default_index = i;
			default_item = item;
		}
		else if (!default_item) {
			/* Use the first item as default if we don't find one. */
			default_item = item;
		}

		g_signal_connect (item, "activate",
				  G_CALLBACK (join_dialog_activate_cb),
				  join);
		
		gtk_menu_append (GTK_MENU (menu), item);

		g_object_set_data_full (G_OBJECT (item),
					"nick",
					g_strdup (favorite->nick),
					g_free);

		g_object_set_data_full (G_OBJECT (item),
					"room", 
					g_strdup (favorite->room),
					g_free);

		g_object_set_data_full (G_OBJECT (item),
					"server", g_strdup (favorite->server),
					g_free);

		g_object_set_data_full (G_OBJECT (item), 
					"name", g_strdup (favorite->name),
					g_free);
		
		gossip_favorite_unref (favorite);
	}
	g_slist_free (favorites);

	gtk_widget_show (menu);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (join->option_menu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (join->option_menu), default_index);

	if (default_item) {
		gtk_widget_activate (default_item);
	}
}

static void
join_dialog_response_cb (GtkWidget        *dialog,
			 gint              response,
			 GossipJoinDialog *join)
{
	const gchar *room;
	const gchar *server;
	const gchar *nick;
	gchar       *to;
	LmMessage   *m;
	
	switch (response) {
	case GTK_RESPONSE_OK:
		nick = gtk_entry_get_text (join->nick_entry);
		server = gtk_entry_get_text (join->server_entry);
		room = gtk_entry_get_text (join->room_entry);
		
		to = g_strdup_printf ("%s@%s/%s", room, server, nick);
		
		m = lm_message_new_with_sub_type (to, LM_MESSAGE_TYPE_PRESENCE,
						  LM_MESSAGE_SUB_TYPE_AVAILABLE);
		g_free (to);

		lm_connection_send (join->connection, m, NULL);
		lm_message_unref (m);

		gossip_app_join_group_chat (join->app, room, server, nick);
		break;

	default:
		break;
	}

	gtk_widget_destroy (join->dialog);
}

static void 
join_dialog_favorites_dialog_destroy_cb (GtkWidget        *unused, 
					 GossipJoinDialog *dialog)
{
	g_return_if_fail (dialog != NULL);

	join_dialog_update_option_menu (dialog);
}

static void
join_dialog_edit_clicked_cb (GtkWidget        *widget,
			     GossipJoinDialog *dialog)
{
	GtkWidget *window;

	window = gossip_favorites_dialog_show (dialog->app);

 	g_signal_connect (window, "destroy",
 			  G_CALLBACK (join_dialog_favorites_dialog_destroy_cb),
 			  dialog);
}

static void
join_dialog_activate_cb (GtkWidget        *widget,
			 GossipJoinDialog *join)
{
	gchar *nick;
	gchar *server;
	gchar *room;
	gchar *name;
		
	nick = g_object_get_data (G_OBJECT (widget), "nick");
	server = g_object_get_data (G_OBJECT (widget), "server");
	room = g_object_get_data (G_OBJECT (widget), "room");
	name = g_object_get_data (G_OBJECT (widget), "name");
	
	gtk_entry_set_text (join->nick_entry, nick);
	gtk_entry_set_text (join->server_entry, server);
	gtk_entry_set_text (join->room_entry, room);

	/* Set default to the selected group. */
	if (name) {
		gnome_config_set_string (GOSSIP_FAVORITES_PATH "/Favorites/Default",
					 name);
		gnome_config_sync_file (GOSSIP_FAVORITES_PATH);
	}
}

static void
join_dialog_entries_changed_cb (GtkWidget        *widget,
				GossipJoinDialog *join)
{
	const gchar *str;
	gboolean     disabled;

	disabled = FALSE;

	str = gtk_entry_get_text (join->nick_entry);
	disabled |= !str || str[0] == 0;

	str = gtk_entry_get_text (join->server_entry);
	disabled |= !str || str[0] == 0;

	str = gtk_entry_get_text (join->room_entry);
	disabled |= !str || str[0] == 0;

	gtk_widget_set_sensitive (join->join_button, !disabled);
}

GossipJoinDialog *
gossip_join_dialog_new (GossipApp *app)
{
	GossipJoinDialog *join;

        join = g_new0 (GossipJoinDialog, 1);

	join->app = app;
	join->connection = gossip_app_get_connection (app);
	
	gossip_glade_get_file_simple (GLADEDIR "/group-chat.glade",
				      "join_group_chat_dialog",
				      NULL,
				      "join_group_chat_dialog", &join->dialog,
				      "favorite_optionmenu", &join->option_menu,
				      "nick_entry", &join->nick_entry,
				      "server_entry", &join->server_entry,
				      "room_entry", &join->room_entry,
				      "edit_button", &join->edit_button,
				      "join_button", &join->join_button,
				      NULL);

	g_signal_connect (join->dialog,
			  "response",
			  G_CALLBACK (join_dialog_response_cb),
			  join);

	g_signal_connect (join->edit_button,
			  "clicked",
			  G_CALLBACK (join_dialog_edit_clicked_cb),
			  join);

	g_signal_connect (join->nick_entry,
			  "changed",
			  G_CALLBACK (join_dialog_entries_changed_cb),
			  join);
	
	g_signal_connect (join->server_entry,
			  "changed",
			  G_CALLBACK (join_dialog_entries_changed_cb),
			  join);
	
	g_signal_connect (join->room_entry,
			  "changed",
			  G_CALLBACK (join_dialog_entries_changed_cb),
			  join);
	
	join_dialog_entries_changed_cb (NULL, join);
		
	join_dialog_update_option_menu (join);

	gtk_widget_show (join->dialog);

	return join;
}

GtkWidget *
gossip_join_dialog_get_dialog (GossipJoinDialog *dialog)
{
	return dialog->dialog;
}

