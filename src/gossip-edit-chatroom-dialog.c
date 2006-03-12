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
#include <glade/glade.h>
#include <libgnome/gnome-config.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <libgossip/gossip-chatroom-provider.h>

#include "gossip-app.h" 
#include "gossip-edit-chatroom-dialog.h"


typedef struct {
	GtkWidget      *dialog;
	GtkWidget      *entry_name;
	GtkWidget      *entry_nickname;
	GtkWidget      *entry_server;
	GtkWidget      *entry_room;
	GtkWidget      *checkbutton_auto_connect;
	GtkWidget      *button_save;

	GossipChatroom *chatroom;
} GossipEditChatroomDialog;


static void edit_chatroom_dialog_set              (GossipEditChatroomDialog *dialog);
static void edit_chatroom_dialog_entry_changed_cb (GtkEntry                 *entry,
						   GossipEditChatroomDialog *dialog);
static void edit_chatroom_dialog_response_cb      (GtkWidget                *widget,
						   gint                      response,
						   GossipEditChatroomDialog *dialog);
static void edit_chatroom_dialog_destroy_cb       (GtkWidget                *widget,
						   GossipEditChatroomDialog *dialog);


static void
edit_chatroom_dialog_set (GossipEditChatroomDialog *dialog)
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
edit_chatroom_dialog_entry_changed_cb (GtkEntry                 *entry,
				       GossipEditChatroomDialog *dialog)
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
edit_chatroom_dialog_response_cb (GtkWidget               *widget,
				  gint                     response,
				  GossipEditChatroomDialog *dialog)
{
	switch (response) {
	case GTK_RESPONSE_OK:
		edit_chatroom_dialog_set (dialog);
		break;
	}

	gtk_widget_destroy (widget);
}

static void
edit_chatroom_dialog_destroy_cb (GtkWidget                *widget,
				 GossipEditChatroomDialog *dialog)
{
	g_object_unref (dialog->chatroom);
	g_free (dialog);
}

void
gossip_edit_chatroom_dialog_show (GtkWindow      *parent,
				  GossipChatroom *chatroom)
{
	GossipEditChatroomDialog *dialog;
	GladeXML                 *glade;

	g_return_if_fail (chatroom != NULL);
	
        dialog = g_new0 (GossipEditChatroomDialog, 1);

	dialog->chatroom = g_object_ref (chatroom);

	glade = gossip_glade_get_file (GLADEDIR "/group-chat.glade",
				       "edit_chatroom_dialog",
				       NULL,
				       "edit_chatroom_dialog", &dialog->dialog,
				       "entry_name", &dialog->entry_name,
				       "entry_nickname", &dialog->entry_nickname,
				       "entry_server", &dialog->entry_server,
				       "entry_room", &dialog->entry_room,
				       "checkbutton_auto_connect", &dialog->checkbutton_auto_connect,
				       "button_save", &dialog->button_save,
				       NULL);
	
	gossip_glade_connect (glade, 
			      dialog,
			      "edit_chatroom_dialog", "destroy", edit_chatroom_dialog_destroy_cb,
			      "edit_chatroom_dialog", "response", edit_chatroom_dialog_response_cb,
			      "entry_name", "changed", edit_chatroom_dialog_entry_changed_cb,
			      "entry_nickname", "changed", edit_chatroom_dialog_entry_changed_cb,
			      "entry_server", "changed", edit_chatroom_dialog_entry_changed_cb,
			      "entry_room", "changed", edit_chatroom_dialog_entry_changed_cb,
			      NULL);

	g_object_unref (glade);

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

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog), parent);
	}
}

