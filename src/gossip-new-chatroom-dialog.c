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
#include "gossip-new-chatroom-dialog.h" 


typedef struct {
	GtkWidget        *window;

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
} GossipNewChatroomDialog;


static void     new_chatroom_dialog_set_defaults        (GossipNewChatroomDialog *dialog);
static void     new_chatroom_dialog_update_join_button  (GossipNewChatroomDialog  *window);
static void     new_chatroom_dialog_join_custom         (GossipNewChatroomDialog  *window);
static void     new_chatroom_dialog_join_stop           (GossipNewChatroomDialog  *window);
static void     new_chatroom_dialog_join_cancel         (GossipNewChatroomDialog  *window);
static gboolean new_chatroom_dialog_progress_pulse_cb   (GtkWidget                *progressbar);
static void     new_chatroom_dialog_join_cb             (GossipChatroomProvider   *provider,
							 GossipChatroomJoinResult  result,
							 GossipChatroomId          id,
							 GossipNewChatroomDialog  *window);
static void     new_chatroom_dialog_entry_changed_cb    (GtkWidget                *widget,
							 GossipNewChatroomDialog  *window);
static void     new_chatroom_dialog_chatroom_changed_cb (GossipChatroom           *chatroom,
							 GParamSpec               *param,
							 GossipNewChatroomDialog  *window);
static void     new_chatroom_dialog_response_cb         (GtkWidget                *widget,
							 gint                      response,
							 GossipNewChatroomDialog  *dialog);
static void     new_chatroom_dialog_destroy_cb          (GtkWidget                *widget,
							 GossipNewChatroomDialog  *dialog);


static void
new_chatroom_dialog_set_defaults (GossipNewChatroomDialog *dialog)
{
	GossipSession        *session;
	GossipAccount        *account;
	GossipAccountChooser *account_chooser;
	GossipProtocol       *protocol;
	const gchar          *server;
	const gchar          *nickname;

	session = gossip_app_get_session ();
	
	account_chooser = GOSSIP_ACCOUNT_CHOOSER (dialog->account_chooser_custom);
	account = gossip_account_chooser_get_account (account_chooser);

	protocol = gossip_session_get_protocol (session, account);

	nickname = gossip_session_get_nickname (session, account);
	if (nickname) {
		gtk_entry_set_text (GTK_ENTRY (dialog->entry_nickname), nickname);
	}

	server = gossip_protocol_get_default_server 
		(protocol, gossip_account_get_id (account));
	if (server) {
		gchar *conference_server;

		conference_server = g_strconcat ("conference.", server, NULL);
		gtk_entry_set_text (GTK_ENTRY (dialog->entry_server), conference_server);
		g_free (conference_server);
	} 
}

static void
new_chatroom_dialog_update_join_button (GossipNewChatroomDialog *dialog)
{
	GossipChatroomManager *manager;
	GList                 *chatrooms;
	const gchar           *nickname;
	const gchar           *server;
	const gchar           *room;
	gboolean               disabled = FALSE;

	GtkButton             *button;
	GtkWidget             *image;

	/* first get button and icon */
	button = GTK_BUTTON (dialog->button_join);

	image = gtk_button_get_image (button);
	if (!image) {
		image = gtk_image_new ();
		gtk_button_set_image (button, image);
	}

	if (dialog->joining_chatroom) {
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
		
		nickname = gtk_entry_get_text (GTK_ENTRY (dialog->entry_nickname));
		disabled |= !nickname || nickname[0] == 0;
		
		server = gtk_entry_get_text (GTK_ENTRY (dialog->entry_server));
		disabled |= !server || server[0] == 0;
		
		room = gtk_entry_get_text (GTK_ENTRY (dialog->entry_room));
		disabled |= !room || room[0] == 0;
		
		if (!disabled) {
			manager = gossip_app_get_chatroom_manager ();
			chatrooms = gossip_chatroom_manager_find_extended (manager, nickname, server, room);
			
			if (chatrooms) {
				gtk_widget_set_sensitive (dialog->checkbutton_add, FALSE);
			} else {
				gtk_widget_set_sensitive (dialog->checkbutton_add, TRUE);
			}
			
			g_list_foreach (chatrooms, (GFunc)g_object_unref, NULL);
			g_list_free (chatrooms);
		}
	}
	
	gtk_widget_set_sensitive (dialog->button_join, !disabled);
}

static void
new_chatroom_dialog_join_custom (GossipNewChatroomDialog *dialog)
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

	account_chooser_custom = GOSSIP_ACCOUNT_CHOOSER (dialog->account_chooser_custom);
	account = gossip_account_chooser_get_account (account_chooser_custom);
	
	manager = gossip_app_get_chatroom_manager ();
	provider = gossip_session_get_chatroom_provider (session, account);

	nickname = gtk_entry_get_text (GTK_ENTRY (dialog->entry_nickname));
	server = gtk_entry_get_text (GTK_ENTRY (dialog->entry_server));
	room   = gtk_entry_get_text (GTK_ENTRY (dialog->entry_room));

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
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->checkbutton_add))) {
		gossip_chatroom_manager_add (manager, chatroom);
	}

	/* remember this chatroom */
	dialog->joining_chatroom = chatroom;

	/* change widgets so they are unsensitive */
	gtk_widget_set_sensitive (dialog->label_preamble_custom, FALSE);
	gtk_widget_set_sensitive (dialog->hbox_account_custom, FALSE);
	gtk_widget_set_sensitive (dialog->table_details, FALSE);

/*  	gtk_widget_show (dialog->table_progress);  */

	new_chatroom_dialog_update_join_button (dialog);

	/* connect change signal to update the progress label */
	dialog->joining_chatroom_pulse_id = 
		g_timeout_add (50,
			       (GSourceFunc) new_chatroom_dialog_progress_pulse_cb,
			       dialog->progressbar);

	dialog->joining_chatroom_change_id = 
		g_signal_connect (chatroom, "notify",
				  G_CALLBACK (new_chatroom_dialog_chatroom_changed_cb), 
				  dialog);

	/* now do the join */
	gossip_chatroom_provider_join (provider,
				       chatroom,
				       (GossipChatroomJoinCb) new_chatroom_dialog_join_cb,
				       dialog);
}

static void
new_chatroom_dialog_join_stop (GossipNewChatroomDialog *dialog)
{
	const gchar *last_error;

	gtk_widget_set_sensitive (dialog->label_preamble_custom, TRUE);
	gtk_widget_set_sensitive (dialog->hbox_account_custom, TRUE);
	gtk_widget_set_sensitive (dialog->table_details, TRUE);

	last_error = gossip_chatroom_get_last_error (dialog->joining_chatroom);
	if (!last_error) {
/* 		gtk_widget_hide (dialog->table_progress); */
	} else {
		GdkPixbuf *pixbuf;

		gtk_label_set_text (GTK_LABEL (dialog->label_progress_detail),
				    last_error);	

		pixbuf = gossip_pixbuf_for_chatroom_status (dialog->joining_chatroom,
							    GTK_ICON_SIZE_BUTTON);
		gtk_image_set_from_pixbuf (GTK_IMAGE (dialog->image_progress),
					   pixbuf);
		g_object_unref (pixbuf);

		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (dialog->progressbar), 0);
	}

	if (dialog->joining_chatroom_pulse_id != 0) {
		g_source_remove (dialog->joining_chatroom_pulse_id);
		dialog->joining_chatroom_pulse_id = 0;
	}

	if (dialog->joining_chatroom_change_id != 0) {
		g_signal_handler_disconnect (dialog->joining_chatroom,
					     dialog->joining_chatroom_change_id);
		dialog->joining_chatroom_change_id = 0;
	}

	if (dialog->joining_chatroom) {
		g_object_unref (dialog->joining_chatroom);
		dialog->joining_chatroom = NULL;
	}
}

static void
new_chatroom_dialog_join_cancel (GossipNewChatroomDialog *dialog)
{
	if (dialog->joining_chatroom) {
		GossipSession          *session;
		GossipAccount          *account;
		GossipAccountChooser   *account_chooser;
		GossipChatroomProvider *provider;
		GossipChatroomId        id;

		session = gossip_app_get_session ();
		
		account_chooser = GOSSIP_ACCOUNT_CHOOSER (dialog->account_chooser_custom);
		account = gossip_account_chooser_get_account (account_chooser);

		provider = gossip_session_get_chatroom_provider (session, account);
		g_object_unref (account);

		id = gossip_chatroom_get_id (dialog->joining_chatroom);
		gossip_chatroom_provider_cancel (provider, id);
	}

	new_chatroom_dialog_join_stop (dialog);
	new_chatroom_dialog_update_join_button (dialog);
}

static gboolean 
new_chatroom_dialog_progress_pulse_cb (GtkWidget *progressbar)
{
	g_return_val_if_fail (progressbar != NULL, FALSE);
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (progressbar));

	return TRUE;
}

static void
new_chatroom_dialog_join_cb (GossipChatroomProvider   *provider,
			     GossipChatroomJoinResult  result,
			     GossipChatroomId          id,
			     GossipNewChatroomDialog  *dialog)
{
	g_return_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider));
	g_return_if_fail (dialog != NULL);
	
	if (result == GOSSIP_CHATROOM_JOIN_OK ||
	    result == GOSSIP_CHATROOM_JOIN_ALREADY_OPEN) {
		gossip_group_chat_show (provider, id);
		gtk_widget_destroy (dialog->window);
		return;
	} 

	if (dialog->joining_chatroom && 
	    gossip_chatroom_get_id (dialog->joining_chatroom) == id) {
		new_chatroom_dialog_join_stop (dialog);
	}

	new_chatroom_dialog_update_join_button (dialog);
}

static void
new_chatroom_dialog_entry_changed_cb (GtkWidget             *widget,
				      GossipNewChatroomDialog *dialog)
{
	new_chatroom_dialog_update_join_button (dialog);
}

static void
new_chatroom_dialog_chatroom_changed_cb (GossipChatroom        *chatroom,
				      GParamSpec            *param,
				      GossipNewChatroomDialog *dialog)
{
	GossipChatroomStatus  status;
	GdkPixbuf            *pixbuf;
	const gchar          *status_str;
	
	status = gossip_chatroom_get_status (chatroom);
	status_str = gossip_chatroom_get_status_as_str (status);
	
	gtk_label_set_text (GTK_LABEL (dialog->label_progress_detail),
			    status_str);
	
	pixbuf = gossip_pixbuf_for_chatroom_status (dialog->joining_chatroom,
						    GTK_ICON_SIZE_BUTTON);
	gtk_image_set_from_pixbuf (GTK_IMAGE (dialog->image_progress),
				   pixbuf);
	g_object_unref (pixbuf);
	
	new_chatroom_dialog_update_join_button (dialog);
}

static void
new_chatroom_dialog_response_cb (GtkWidget               *widget, 
				 gint                     response, 
				 GossipNewChatroomDialog *dialog)
{
	if (response == GTK_RESPONSE_OK) {
		if (dialog->joining_chatroom) {
			new_chatroom_dialog_join_cancel (dialog);
		} else {
			new_chatroom_dialog_join_custom (dialog);
		}

		return;
	}

	gtk_widget_destroy (widget);
}

static void
new_chatroom_dialog_destroy_cb (GtkWidget               *widget, 
				GossipNewChatroomDialog *dialog)
{
	if (dialog->joining_chatroom_pulse_id != 0) {
		g_source_remove (dialog->joining_chatroom_pulse_id);
		dialog->joining_chatroom_pulse_id = 0;
	}

	if (dialog->joining_chatroom_change_id != 0) {
		g_signal_handler_disconnect (dialog->joining_chatroom,
					     dialog->joining_chatroom_change_id);
		dialog->joining_chatroom_change_id = 0;
	}

	if (dialog->joining_chatroom) {
		g_object_unref (dialog->joining_chatroom);
		dialog->joining_chatroom = NULL;
	}

	g_free (dialog);
}

void
gossip_new_chatroom_dialog_show (GtkWindow *parent)
{
	static GossipNewChatroomDialog *dialog = NULL;
	GladeXML                       *glade;
	GossipSession                  *session;
	GList                          *accounts;
	gint                            account_num;
	GossipChatroomManager          *manager;
	GtkSizeGroup                   *size_group;

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->window));
		return;
	}		
	
        dialog = g_new0 (GossipNewChatroomDialog, 1);

	glade = gossip_glade_get_file (GLADEDIR "/group-chat.glade",
				       "new_chatroom_dialog",
				       NULL,
				       "new_chatroom_dialog", &dialog->window,
				       "label_preamble_custom", &dialog->label_preamble_custom,
				       "hbox_account_custom", &dialog->hbox_account_custom,
				       "label_account_custom", &dialog->label_account_custom,
				       "table_details", &dialog->table_details,
 				       "label_nickname", &dialog->label_nickname, 
 				       "entry_nickname", &dialog->entry_nickname, 
				       "label_server", &dialog->label_server,
				       "entry_server", &dialog->entry_server,
				       "label_room", &dialog->label_room,
				       "entry_room", &dialog->entry_room,
				       "checkbutton_add", &dialog->checkbutton_add,
				       "table_progress", &dialog->table_progress,
				       "label_progress", &dialog->label_progress,
				       "image_progress", &dialog->image_progress,
				       "label_progress_detail", &dialog->label_progress_detail,
				       "progressbar", &dialog->progressbar,
				       "button_join", &dialog->button_join,
				       NULL);
	
	gossip_glade_connect (glade, 
			      dialog,
			      "new_chatroom_dialog", "response", new_chatroom_dialog_response_cb,
			      "new_chatroom_dialog", "destroy", new_chatroom_dialog_destroy_cb,
			      "entry_nickname", "changed", new_chatroom_dialog_entry_changed_cb,
			      "entry_server", "changed", new_chatroom_dialog_entry_changed_cb,
			      "entry_room", "changed", new_chatroom_dialog_entry_changed_cb,
			      NULL);

	g_object_add_weak_pointer (G_OBJECT (dialog->window), (gpointer) &dialog);

	/* look and feel - aligning... */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, dialog->label_account_custom);
	gtk_size_group_add_widget (size_group, dialog->label_nickname);
	gtk_size_group_add_widget (size_group, dialog->label_server);
	gtk_size_group_add_widget (size_group, dialog->label_room);
	gtk_size_group_add_widget (size_group, dialog->label_progress);

	g_object_unref (size_group);

	/* get the session and chat room manager */
	session = gossip_app_get_session ();
	manager = gossip_app_get_chatroom_manager ();

	/* account chooser for custom */
	dialog->account_chooser_custom = gossip_account_chooser_new (session);
	gtk_box_pack_start (GTK_BOX (dialog->hbox_account_custom), 
			    dialog->account_chooser_custom,
			    TRUE, TRUE, 0);

	gtk_widget_show (dialog->account_chooser_custom);

	new_chatroom_dialog_set_defaults (dialog);

	/* populate */
	accounts = gossip_session_get_accounts (session);
	account_num = g_list_length (accounts);

	g_list_foreach (accounts, (GFunc)g_object_unref, NULL);
	g_list_free (accounts);

	if (account_num > 1) {
		gtk_widget_show (dialog->hbox_account_custom);
	} else {
		/* show no accounts combo box */	
		gtk_widget_hide (dialog->hbox_account_custom);
	}

	gtk_widget_grab_focus (dialog->entry_nickname);

	/* last touches */
	gtk_window_set_transient_for (GTK_WINDOW (dialog->window), 
				      GTK_WINDOW (parent));

	gtk_widget_show (dialog->window);
}
