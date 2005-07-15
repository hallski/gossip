/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2003 Imendio AB
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

#include "gossip-app.h"
#include "gossip-favorite.h"
#include "gossip-edit-favorite-dialog.h"
#include "gossip-join-dialog.h"


typedef struct {
	GtkWidget      *dialog;
	GtkWidget      *name_entry;
	GtkWidget      *nickname_entry;
	GtkWidget      *server_entry;
	GtkWidget      *room_entry;
	GtkWidget      *save_button;

	GossipFavorite *favorite;
} GossipEditFavoriteDialog;


static void edit_favorite_dialog_set              (GossipEditFavoriteDialog *dialog);
static void edit_favorite_dialog_entry_changed_cb (GtkWidget                *widget,
						   GossipEditFavoriteDialog *dialog);
static void edit_favorite_dialog_response_cb      (GtkWidget                *widget,
						   gint                      response,
						   GossipEditFavoriteDialog *dialog);
static void edit_favorite_dialog_destroy_cb       (GtkWidget                *widget,
						   GossipEditFavoriteDialog *dialog);


static void
edit_favorite_dialog_set (GossipEditFavoriteDialog *dialog)
{
	GossipFavorite *favorite;
	gchar          *path;
	gchar          *key;
	const gchar    *str;

	favorite = dialog->favorite;

	str = gtk_entry_get_text (GTK_ENTRY (dialog->name_entry));

	if (strcmp (favorite->name, str) != 0) {
		gchar *old_path = g_strdup_printf ("%s/Favorite: %s",
						   GOSSIP_FAVORITES_PATH,
						   favorite->name);
		gnome_config_clean_section (old_path);
		g_free (old_path);
	}

	/* set favorite information */
	str = gtk_entry_get_text (GTK_ENTRY (dialog->name_entry));
	g_free (favorite->name);
	favorite->name = g_strdup (str);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->nickname_entry));
	g_free (favorite->nick);
	favorite->nick = g_strdup (str);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->server_entry));
	g_free (favorite->server);
	favorite->server = g_strdup (str);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->room_entry));
	g_free (favorite->room);
	favorite->room = g_strdup (str);
	
	/* store with gconf */
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
edit_favorite_dialog_entry_changed_cb (GtkWidget                *widget,
				       GossipEditFavoriteDialog *dialog)
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
edit_favorite_dialog_response_cb (GtkWidget                *widget,
				  gint                      response,
				  GossipEditFavoriteDialog *dialog)
{
	switch (response) {
	case GTK_RESPONSE_OK:
		edit_favorite_dialog_set (dialog);
		break;
	}

	gtk_widget_destroy (widget);
}

static void
edit_favorite_dialog_destroy_cb (GtkWidget                 *widget,
				 GossipEditFavoriteDialog *dialog)
{
	gossip_favorite_unref (dialog->favorite);
	g_free (dialog);
}

GtkWidget *
gossip_edit_favorite_dialog_show (GossipFavorite *favorite)
{
	GossipEditFavoriteDialog *dialog;
	GladeXML                 *gui;

	g_return_val_if_fail (favorite != NULL, NULL);
	
        dialog = g_new0 (GossipEditFavoriteDialog, 1);

	dialog->favorite = gossip_favorite_ref (favorite);

	gui = gossip_glade_get_file (GLADEDIR "/group-chat.glade",
				     "edit_favorite_dialog",
				     NULL,
				     "edit_favorite_dialog", &dialog->dialog,
				     "name_entry", &dialog->name_entry,
				     "nickname_entry", &dialog->nickname_entry,
				     "server_entry", &dialog->server_entry,
				     "room_entry", &dialog->room_entry,
				     "save_button", &dialog->save_button,
				     NULL);
	
	gossip_glade_connect (gui, 
			      dialog,
			      "edit_favorite_dialog", "destroy", edit_favorite_dialog_destroy_cb,
			      "edit_favorite_dialog", "response", edit_favorite_dialog_response_cb,
			      "name_entry", "changed", edit_favorite_dialog_entry_changed_cb,
			      "nickname_entry", "changed", edit_favorite_dialog_entry_changed_cb,
			      "server_entry", "changed", edit_favorite_dialog_entry_changed_cb,
			      "room_entry", "changed", edit_favorite_dialog_entry_changed_cb,
			      NULL);

	gtk_entry_set_text (GTK_ENTRY (dialog->name_entry), favorite->name);
	gtk_entry_set_text (GTK_ENTRY (dialog->nickname_entry), favorite->nick);
	gtk_entry_set_text (GTK_ENTRY (dialog->server_entry), favorite->server);
	gtk_entry_set_text (GTK_ENTRY (dialog->room_entry), favorite->room);

 	return dialog->dialog;
}

