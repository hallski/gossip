/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Martyn Russell <mr@gnome.org>
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

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libgossip/gossip-session.h>
#include <libgossip/gossip-account.h>
#include <libgossip/gossip-contact.h>

#include "gossip-chat-invite.h"
#include "gossip-app.h"


typedef struct {
	GossipChatroomId  id;
	GossipContact    *contact;
	GtkWidget        *entry;
} ChatInviteData;


static void chat_invite_menu_activate_cb   (GtkWidget      *menuitem,
					    gpointer        user_data);
static void chat_invite_dialog_response_cb (GtkWidget      *dialog,
					    gint            response,
					    ChatInviteData *cid);
static void chat_invite_entry_activate_cb  (GtkWidget      *entry,
					    GtkDialog      *dialog);


GtkWidget *
gossip_chat_invite_contact_menu (GossipContact *contact)
{
	GossipSession          *session;
	GossipAccount          *account;
	GossipChatroomProvider *provider;
	GList                  *rooms = NULL;
	GList                  *l;
	GtkWidget              *menu = NULL;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	session = gossip_app_get_session ();
	account = gossip_contact_get_account (contact);
	provider = gossip_session_get_chatroom_provider (session, account);

	rooms = gossip_chatroom_provider_get_rooms (provider);

	if (!rooms || g_list_length (rooms) < 1) {
		g_list_free (rooms);
		return NULL;
	}

	menu = gtk_menu_new ();
	
	for (l = rooms; l; l = l->next) {
		GossipChatroomId  id;
		GossipChatroom   *chatroom;
		const gchar      *name;
		GtkWidget        *item;
		
		id = GPOINTER_TO_INT(l->data);
		chatroom = gossip_chatroom_provider_find (provider, id);
		name = gossip_chatroom_get_name (chatroom);
		
		if (name == NULL || strlen (name) < 1) {
			continue;
		}
		
		item = gtk_menu_item_new_with_label (name);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		
		g_object_set_data_full (G_OBJECT (item), "contact", 
					g_object_ref (contact),
					g_object_unref);
		g_object_set_data (G_OBJECT (item), "chatroom_id", l->data);
		
		g_signal_connect (GTK_MENU_ITEM (item), "activate",
				  G_CALLBACK (chat_invite_menu_activate_cb),
				  NULL);
	}

	g_list_free (rooms);
	
	return menu;
}

GtkWidget *
gossip_chat_invite_groupchat_menu (GossipContact    *contact,
				   GossipChatroomId  id)
{
	GossipSession *session;
	GossipAccount *account;
	GList         *list = NULL;
	GList         *l;
	GtkWidget     *menu = NULL;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	account = gossip_contact_get_account (contact);

	session = gossip_app_get_session ();

	list = gossip_session_get_contacts_by_account (session, account);
	if (!list || g_list_length (list) < 1) {
		g_list_free (list);
		return NULL;
	}

	for (l = list; l; l = l->next) {
		GossipContact *contact = NULL;
		const gchar   *name;
		GtkWidget     *item;
		
		/* get name */
		contact = l->data;
			
		if (!gossip_contact_is_online (contact)) {
			continue;
		}
			
		name = gossip_contact_get_name (contact);
		if (!name && strlen (name) > 0) {
			continue;
		}

		if (!menu) {
			menu = gtk_menu_new ();
		}

		/* get name */
		item = gtk_menu_item_new_with_label (name);
		
		/* set data */
		g_object_set_data_full (G_OBJECT (item), "contact", 
					g_object_ref (contact), 
					g_object_unref);
		g_object_set_data (G_OBJECT (item), "chatroom_id", 
				   GINT_TO_POINTER (id));

		g_signal_connect (G_OBJECT (item), "activate", 
				  G_CALLBACK (chat_invite_menu_activate_cb),
				  NULL);
		
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}

	g_list_free (list);

	return menu;
}

static void 
chat_invite_menu_activate_cb (GtkWidget *menuitem,
			      gpointer   user_data)
{
	GossipContact    *contact;
	GossipChatroomId  id;
	gpointer          pid;

	contact = g_object_get_data (G_OBJECT (menuitem), "contact");
	pid = g_object_get_data (G_OBJECT (menuitem), "chatroom_id");
	
	id = GPOINTER_TO_INT (pid);

	gossip_chat_invite_dialog (contact, id);
}

gboolean
gossip_chat_invite_dialog (GossipContact    *contact,
			   GossipChatroomId  id)
{
	ChatInviteData *cid;
	gchar          *str;

	GtkWidget      *dialog;
	GtkWidget      *entry;
	GtkWidget      *hbox;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);
	g_return_val_if_fail (id >= 0, FALSE);

	/* construct dialog for invitiation text */
	str = g_strdup_printf ("<b>%s</b>", gossip_contact_get_name (contact));

	dialog = gtk_message_dialog_new (GTK_WINDOW (gossip_app_get_window ()),
					 0,
					 GTK_MESSAGE_INFO,
					 GTK_BUTTONS_OK_CANCEL,
					 _("Please enter your invitation message to:\n%s"),
					 str);
	
	g_free (str);

	g_object_set (GTK_MESSAGE_DIALOG (dialog)->label,
		      "use-markup", TRUE,
		      NULL);

        entry = gtk_entry_new ();
	gtk_widget_show (entry);

	gtk_entry_set_text (GTK_ENTRY (entry), 
                            _("You have been invited to join a chat conference."));
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
	
	g_signal_connect (entry,
			  "activate",
			  G_CALLBACK (chat_invite_entry_activate_cb),
			  dialog);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), 
			    hbox, FALSE, TRUE, 4);

	/* save details to pass on to response callback */
	cid = g_new0 (ChatInviteData, 1);

	cid->id = id;
	cid->contact = g_object_ref (contact);
	cid->entry = g_object_ref (entry);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (chat_invite_dialog_response_cb),
			  cid);

	gtk_widget_show (dialog);

	return TRUE;
}

static void 
chat_invite_dialog_response_cb (GtkWidget      *dialog, 
				gint            response, 
				ChatInviteData *cid) 
{
	const gchar *invite;

	if (response == GTK_RESPONSE_OK) {
		GossipSession          *session;
		GossipAccount          *account;
		GossipChatroomProvider *provider;

		session = gossip_app_get_session ();
		account = gossip_contact_get_account (cid->contact);
		provider = gossip_session_get_chatroom_provider (session, account);

		invite = gtk_entry_get_text (GTK_ENTRY (cid->entry));

		/* NULL uses the other end (in their language) */
		invite = (strlen (invite) > 0) ? invite : NULL;

		gossip_chatroom_provider_invite (provider,
						 cid->id,
						 gossip_contact_get_id (cid->contact),
						 invite);
	}
	
	g_object_unref (cid->contact);
	g_object_unref (cid->entry);

	g_free (cid);

	gtk_widget_destroy (dialog);
}

static void
chat_invite_entry_activate_cb (GtkWidget *entry, 
				       GtkDialog *dialog)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}
