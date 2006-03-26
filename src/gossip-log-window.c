/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
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

#include <gtk/gtk.h> 

#include "gossip-app.h"
#include "gossip-account-chooser.h"
#include "gossip-log.h"
#include "gossip-log-window.h"
#include "gossip-ui-utils.h"

typedef struct {
	GtkWidget      *window;

	GtkWidget      *table;

	GtkWidget      *label_account;
	GtkWidget      *account_chooser;
	GtkWidget      *checkbutton_all_accounts;

	GtkWidget      *combobox_contacts;
	GtkWidget      *checkbutton_all_contacts;

	GtkWidget      *entry_date;
	GtkWidget      *checkbutton_all_dates;

	GtkWidget      *entry_find;
	GtkWidget      *button_previous;
	GtkWidget      *button_next;
	GtkWidget      *togglebutton_highlight;
	GtkWidget      *checkbutton_match_case;

	GtkWidget      *scrolledwindow;
	GossipChatView *chatview;

	GtkWidget      *button_close;
} GossipLogWindow;

static void   log_window_contacts_set_selected  (GossipLogWindow *window,
						 GossipContact   *contact);
static gchar *log_window_contacts_get_selected  (GossipLogWindow *window);
static void   log_window_contacts_populate      (GossipLogWindow *window);
static void   log_window_contacts_setup         (GossipLogWindow *window);
static void   log_window_contacts_changed_cb    (GtkWidget       *combobox,
						 GossipLogWindow *window);
static void   log_window_accounts_changed_cb    (GtkWidget       *combobox,
						 GossipLogWindow *window);
static void   log_window_entry_find_changed_cb  (GtkWidget       *entry,
						 GossipLogWindow *window);
static void   log_window_entry_find_activate_cb (GtkWidget       *entry,
						 GossipLogWindow *window);
static void   log_window_close_clicked_cb       (GtkWidget       *widget,
						 GossipLogWindow *window);

enum {
	COL_CONTACT_NAME, 
	COL_CONTACT_ID,
	COL_CONTACT_COUNT
};

static void   
log_window_contacts_set_selected  (GossipLogWindow *window,
				   GossipContact   *contact)
{
	GtkComboBox  *combobox;
	GtkTreeModel *model;
	GtkTreeIter   iter;
	const gchar  *id;
	gchar        *this_id;
	gboolean      ok;

	id = gossip_contact_get_id (contact);
	
	g_return_if_fail (id != NULL);

	combobox = GTK_COMBO_BOX (window->combobox_contacts);
	model = gtk_combo_box_get_model (combobox);

	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		return;
	}
	
	for (ok = TRUE; ok; ok = gtk_tree_model_iter_next (model, &iter)) {
		gtk_tree_model_get (model, &iter, 
				    COL_CONTACT_ID, &this_id, 
				    -1);

		if (!this_id) {
			continue;
		}

		if (strcmp (id, this_id) == 0) {
			gtk_combo_box_set_active_iter (combobox, &iter);
			g_free (this_id);
			break;
		} 
			
		g_free (this_id);
	}
}

static gchar *
log_window_contacts_get_selected (GossipLogWindow *window)
{
	GtkComboBox  *combobox;
	GtkTreeModel *model;
	GtkTreeIter   iter;
	gchar        *contact;

	combobox = GTK_COMBO_BOX (window->combobox_contacts);

	model = gtk_combo_box_get_model (combobox);
	if (!gtk_combo_box_get_active_iter (combobox, &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter, COL_CONTACT_ID, &contact, -1);
	
	return contact;
}

static void
log_window_contacts_populate (GossipLogWindow *window)
{
	GossipAccountChooser *account_chooser;
	GossipAccount        *account;     
	GossipContact        *contact;
	GList                *contacts;
	GList                *l;
	
	GtkComboBox          *combobox;
	GtkTreeModel         *model;
	GtkListStore         *store;
	GtkTreeIter           iter;
	
	account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser);
	account = gossip_account_chooser_get_account (account_chooser);

	combobox = GTK_COMBO_BOX (window->combobox_contacts);
	model = gtk_combo_box_get_model (combobox);
	store = GTK_LIST_STORE (model);

	gtk_list_store_clear (store);

	contacts = gossip_log_get_contacts (account);

	for (l = contacts; l; l = l->next) {
		contact = l->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 
				    COL_CONTACT_ID, gossip_contact_get_id (contact), 
				    COL_CONTACT_NAME, gossip_contact_get_name (contact), 
				    -1);

		if (l == contacts) {
			gtk_combo_box_set_active_iter (combobox, &iter);
		}
	}

	g_list_foreach (contacts, (GFunc) g_object_unref, NULL);
	g_list_free (contacts);

	g_object_unref (account);
}

static void
log_window_contacts_setup (GossipLogWindow *window)
{
	GtkListStore         *store;
	GtkCellRenderer      *renderer;
	GtkComboBox          *combobox;

	combobox = GTK_COMBO_BOX (window->combobox_contacts);

  	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combobox));  
 
	store = gtk_list_store_new (COL_CONTACT_COUNT,
				    G_TYPE_STRING,    /* name */
				    G_TYPE_STRING);   /* id */

	gtk_combo_box_set_model (combobox, GTK_TREE_MODEL (store));

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
					"text", COL_CONTACT_NAME,				
					NULL);

	g_object_unref (store);
}

static void
log_window_contacts_changed_cb (GtkWidget       *combobox,
				GossipLogWindow *window)
{
	GossipSession        *session;
	GossipAccountChooser *account_chooser;
	GossipAccount        *account;   
	GossipContact        *own_contact;
	GossipContact        *sender;
	GossipMessage        *message;
	gchar                *contact_id;
	GList                *messages;
	GList                *l;

	account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser);
	account = gossip_account_chooser_get_account (account_chooser);

	contact_id = log_window_contacts_get_selected (window);

	session = gossip_app_get_session ();
	own_contact = gossip_session_get_own_contact (session, account);

	/* Note: in this case we are offline :/ */
	if (!own_contact) {
		gchar *id;

		own_contact = gossip_contact_new (GOSSIP_CONTACT_TYPE_USER,
						  account);

		/* FIXME: Jabber ism's till we can save and load the
		 * roster without needing to connect to a server.
		 */
		id = g_strdup_printf ("%s/%s", 
				      gossip_account_get_id (account),
				      gossip_account_get_resource (account));
	
		g_object_set (own_contact,
			      "id", id, 
			      "name", id,
			      NULL);
		g_free (id);
	}

	messages = gossip_log_get_for_contact (own_contact, 
					       account, 
					       contact_id);

	gossip_chat_view_clear (window->chatview);

	for (l = messages; l; l = l->next) {
		message = l->data;

		sender = gossip_message_get_sender (message);

		if (gossip_contact_equal (own_contact, sender)) {
			gossip_chat_view_append_message_from_self (window->chatview, 
								   message,
								   own_contact);
		} else {
			gossip_chat_view_append_message_from_other (window->chatview, 
								    message,
								    own_contact);
		}
	}

	gossip_chat_view_scroll_down (window->chatview);

	gtk_widget_grab_focus (window->entry_find);

	g_list_foreach (messages, (GFunc) g_object_unref, NULL);
	g_list_free (messages);

	g_free (contact_id);

	g_object_unref (account);
}

static void
log_window_accounts_changed_cb (GtkWidget       *combobox,
				GossipLogWindow *window)
{
	log_window_contacts_populate (window);
}

static void
log_window_find (GossipLogWindow *window, 
		 gboolean         new_search)
{
	GtkTextBuffer      *buffer;
	GtkTextIter         iter_at_mark;
	GtkTextIter         iter_match_start;
	GtkTextIter         iter_match_end;

	static GtkTextMark *find_mark = NULL;
	static gboolean     wrapped = FALSE;
     
	const gchar        *str;
    
	gboolean            found;
	gboolean            from_start = FALSE;
    
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (window->chatview));
    
	str = gtk_entry_get_text (GTK_ENTRY (window->entry_find));

	if (new_search) {
		find_mark = NULL;
		from_start = TRUE;
	}
     
	if (find_mark) {
		gtk_text_buffer_get_iter_at_mark (buffer, &iter_at_mark, find_mark);
	} else {
		gtk_text_buffer_get_start_iter (buffer, &iter_at_mark);
	}
    
	found = gtk_text_iter_forward_search (&iter_at_mark, 
					      str, 
					      GTK_TEXT_SEARCH_TEXT_ONLY, 
					      &iter_match_start, 
					      &iter_match_end,
					      NULL);
    
	if (find_mark) {
		gtk_text_buffer_delete_mark (buffer, find_mark); 
		find_mark = NULL;
	} else {
		from_start = TRUE;
	}
    
	if (!found) {
		if (from_start) {
			gtk_widget_set_sensitive (window->button_next, FALSE);
			return;
		}
	
		/* Here we wrap around. */
		if (!new_search && !wrapped) {
			wrapped = TRUE;
			log_window_find (window, FALSE);
			wrapped = FALSE;
		}

		gtk_widget_set_sensitive (window->button_next, FALSE);
		return;
	}
    
	/* set new mark and show on screen */
	find_mark = gtk_text_buffer_create_mark (buffer, NULL, &iter_match_end, TRUE); 
	gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (window->chatview), find_mark); 

	gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &iter_match_start);
	gtk_text_buffer_move_mark_by_name (buffer, "insert", &iter_match_end);	
}

static void
log_window_entry_find_changed_cb (GtkWidget       *entry,
				  GossipLogWindow *window)
{
	log_window_find (window, TRUE);
}

static void
log_window_entry_find_activate_cb (GtkWidget       *entry,
				   GossipLogWindow *window)
{
	log_window_find (window, FALSE);
}

static void
log_window_close_clicked_cb (GtkWidget       *widget,
			     GossipLogWindow *window)
{
	gtk_widget_destroy (window->window);
}

void
gossip_log_window_show (GtkWindow     *parent,
			GossipContact *contact)
{
	GossipLogWindow      *window = NULL;
	GladeXML             *glade;
	GossipSession        *session;
	GList                *accounts;
	gint                  account_num;
	GossipAccountChooser *account_chooser;

        window = g_new0 (GossipLogWindow, 1);

	glade = gossip_glade_get_file (GLADEDIR "/main.glade",
				       "log_window",
				       NULL,
				       "log_window", &window->window,
				       "table", &window->table, 
				       "label_account", &window->label_account,
 				       "combobox_contacts", &window->combobox_contacts, 
				       "entry_find", &window->entry_find,
				       "button_previous", &window->button_previous,
				       "button_next", &window->button_next,
				       "togglebutton_highlight", &window->togglebutton_highlight,
				       "scrolledwindow", &window->scrolledwindow,
				       "button_close", &window->button_close,
				       NULL);
	
	gossip_glade_connect (glade, 
			      window,
			      "combobox_contacts", "changed", log_window_contacts_changed_cb,
			      "entry_find", "changed", log_window_entry_find_changed_cb,
			      "entry_find", "activate", log_window_entry_find_activate_cb,
			      "button_close", "clicked", log_window_close_clicked_cb,
			      NULL);

	/* Configure GossipChatView */
	window->chatview = gossip_chat_view_new ();
	gtk_container_add (GTK_CONTAINER (window->scrolledwindow), GTK_WIDGET (window->chatview));
 	gtk_widget_show (GTK_WIDGET (window->chatview));

	/* Account chooser for chat rooms */
	session = gossip_app_get_session ();

	window->account_chooser = gossip_account_chooser_new (session);
	account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser);
	gossip_account_chooser_set_can_select_all (account_chooser, TRUE);
	
	gtk_table_attach (GTK_TABLE (window->table), 
			  window->account_chooser,
			  1, 2, 0, 1, GTK_FILL, 0, 0, 0);
			  
	g_signal_connect (window->account_chooser, "changed",
			  G_CALLBACK (log_window_accounts_changed_cb),
			  window);

	/* Populate */
	accounts = gossip_session_get_accounts (session);
	account_num = g_list_length (accounts);

	g_list_foreach (accounts, (GFunc)g_object_unref, NULL);
	g_list_free (accounts);

	if (account_num > 1) {
		gtk_widget_show (window->label_account);
		gtk_widget_show (window->account_chooser);
	} else {
		gtk_widget_hide (window->label_account);
		gtk_widget_hide (window->account_chooser);
	}

	/* Contacts */
	log_window_contacts_setup (window);
	log_window_contacts_populate (window);

	/* Select contact */
	if (contact) {
		log_window_contacts_set_selected (window, contact);
	}

	/* Last touches */
	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (window->window), 
					      GTK_WINDOW (parent));
	}

	gtk_widget_show (window->window);
}
