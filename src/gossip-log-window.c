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
#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-log.h>
#include <libgossip/gossip-message.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-utils.h>
#include <libgossip/gossip-time.h>

#include "gossip-app.h"
#include "gossip-account-chooser.h"
#include "gossip-glade.h"
#include "gossip-log-window.h"
#include "gossip-ui-utils.h"

#define DEBUG_DOMAIN "LogWindow"

typedef struct {
	GtkWidget        *window;

	GtkWidget        *notebook;

	GtkWidget        *entry_find;
	GtkWidget        *button_find;
	GtkWidget        *treeview_find;
	GtkWidget        *scrolledwindow_find;
	GossipChatView   *chatview_find;
	GtkWidget        *button_previous;
	GtkWidget        *button_next;

	GtkWidget        *vbox_contacts;
	GtkWidget        *account_chooser_contacts;
	GtkWidget        *entry_contacts;
	GtkWidget        *calendar_contacts;
	GtkWidget        *treeview_contacts;
	GtkWidget        *scrolledwindow_contacts;
	GossipChatView   *chatview_contacts;

	GtkWidget        *vbox_chatrooms;
	GtkWidget        *account_chooser_chatrooms;
	GtkWidget        *entry_chatrooms;
	GtkWidget        *calendar_chatrooms;
	GtkWidget        *treeview_chatrooms;
	GtkWidget        *scrolledwindow_chatrooms;
	GossipChatView   *chatview_chatrooms;

	GtkWidget        *vbox_links_find;
	GtkWidget        *entry_links_find;
	GtkWidget        *button_links_find;
	GtkWidget        *button_links_open;
	GtkWidget        *treeview_links;

	gchar            *last_links_find;
	gchar            *last_find;

	GossipLogManager *log_manager;
} GossipLogWindow;

/* Searching */
static void            log_window_find_text_data_func                 (GtkCellLayout    *cell_layout,
								       GtkCellRenderer  *cell,
								       GtkTreeModel     *tree_model,
								       GtkTreeIter      *iter,
								       GossipLogWindow  *window);
static void            log_window_find_pixbuf_data_func               (GtkCellLayout    *cell_layout,
								       GtkCellRenderer  *cell,
								       GtkTreeModel     *tree_model,
								       GtkTreeIter      *iter,
								       GossipLogWindow  *window);
static void            log_window_find_changed_cb                     (GtkTreeSelection *selection,
								       GossipLogWindow  *window);
static void            log_window_find_populate                       (GossipLogWindow  *window,
								       const gchar      *search_criteria);
static void            log_window_find_setup                          (GossipLogWindow  *window);
static void            log_window_button_find_clicked_cb              (GtkWidget        *widget,
								       GossipLogWindow  *window);
static void            log_window_button_next_clicked_cb              (GtkWidget        *widget,
								       GossipLogWindow  *window);
static void            log_window_button_previous_clicked_cb          (GtkWidget        *widget,
								       GossipLogWindow  *window);

/* Contacts */
static void            log_window_contacts_text_data_func             (GtkCellLayout    *cell_layout,
								       GtkCellRenderer  *cell,
								       GtkTreeModel     *tree_model,
								       GtkTreeIter      *iter,
								       GossipLogWindow  *window);
static void            log_window_contacts_pixbuf_data_func           (GtkCellLayout    *cell_layout,
								       GtkCellRenderer  *cell,
								       GtkTreeModel     *tree_model,
								       GtkTreeIter      *iter,
								       GossipLogWindow  *window);
static void            log_window_contacts_changed_cb                 (GtkTreeSelection *selection,
								       GossipLogWindow  *window);
static void            log_window_contacts_set_selected               (GossipLogWindow  *window,
								       GossipContact    *contact);
static GossipContact * log_window_contacts_get_selected               (GossipLogWindow  *window);
static void            log_window_contacts_populate                   (GossipLogWindow  *window);
static void            log_window_contacts_setup                      (GossipLogWindow  *window);
static void            log_window_contacts_accounts_changed_cb        (GtkWidget        *combobox,
								       GossipLogWindow  *window);
static void            log_window_contacts_new_message_cb             (GossipContact    *own_contact,
								       GossipMessage    *message,
								       GossipLogWindow  *window);
static gboolean        log_window_contacts_is_today_selected          (GossipLogWindow  *window);
static void            log_window_contacts_get_messages               (GossipLogWindow  *window,
								       const gchar      *date);
static void            log_window_calendar_contacts_day_selected_cb   (GtkWidget        *calendar,
								       GossipLogWindow  *window);
static void            log_window_calendar_contacts_month_changed_cb  (GtkWidget        *calendar,
								       GossipLogWindow  *window);
static void            log_window_entry_contacts_changed_cb           (GtkWidget        *entry,
								       GossipLogWindow  *window);
static void            log_window_entry_contacts_activate_cb          (GtkWidget        *entry,
								       GossipLogWindow  *window);

/* Chatrooms */
static void            log_window_chatrooms_text_data_func            (GtkCellLayout    *cell_layout,
								       GtkCellRenderer  *cell,
								       GtkTreeModel     *tree_model,
								       GtkTreeIter      *iter,
								       GossipLogWindow  *window);
static void            log_window_chatrooms_pixbuf_data_func          (GtkCellLayout    *cell_layout,
								       GtkCellRenderer  *cell,
								       GtkTreeModel     *tree_model,
								       GtkTreeIter      *iter,
								       GossipLogWindow  *window);
static void            log_window_chatrooms_changed_cb                (GtkTreeSelection *selection,
								       GossipLogWindow  *window);
static void            log_window_chatrooms_set_selected              (GossipLogWindow  *window,
								       GossipChatroom   *chatroom);
static GossipChatroom *log_window_chatrooms_get_selected              (GossipLogWindow  *window);
static void            log_window_chatrooms_populate                  (GossipLogWindow  *window);
static void            log_window_chatrooms_setup                     (GossipLogWindow  *window);
static void            log_window_chatrooms_accounts_changed_cb       (GtkWidget        *combobox,
								       GossipLogWindow  *window);
static void            log_window_chatrooms_new_message_cb            (GossipContact    *own_contact,
								       GossipMessage    *message,
								       GossipLogWindow  *window);
static gboolean        log_window_chatrooms_is_today_selected         (GossipLogWindow  *window);
static void            log_window_chatrooms_get_messages              (GossipLogWindow  *window,
								       const gchar      *date);
static void            log_window_calendar_chatrooms_day_selected_cb  (GtkWidget        *calendar,
								       GossipLogWindow  *window);
static void            log_window_calendar_chatrooms_month_changed_cb (GtkWidget        *calendar,
								       GossipLogWindow  *window);
static void            log_window_entry_chatrooms_changed_cb          (GtkWidget        *entry,
								       GossipLogWindow  *window);
static void            log_window_entry_chatrooms_activate_cb         (GtkWidget        *entry,
								       GossipLogWindow  *window);
/* Window */
static void            log_window_destroy_cb                          (GtkWidget        *widget,
								       GossipLogWindow  *window);

enum {
	COL_FIND_STATUS,
	COL_FIND_ACCOUNT,
	COL_FIND_CONTACT,
	COL_FIND_CONTACT_NAME,
	COL_FIND_DATE,
	COL_FIND_DATE_READABLE,
	COL_FIND_COUNT
};

enum {
	COL_CONTACTS_STATUS,
	COL_CONTACTS_NAME,
	COL_CONTACTS_POINTER,
	COL_CONTACTS_COUNT
};

enum {
	COL_CHATROOMS_STATUS,
	COL_CHATROOMS_NAME,
	COL_CHATROOMS_POINTER,
	COL_CHATROOMS_COUNT
};

enum {
	COL_LINKS_STATUS,
	COL_LINKS_ACCOUNT,
	COL_LINKS_CONTACT,
	COL_LINKS_CONTACT_NAME,
	COL_LINKS_DATE,
	COL_LINKS_DATE_READABLE,
	COL_LINKS_URL,
	COL_LINKS_COUNT
};

/*
 * Search code.
 */
static void
log_window_find_pixbuf_data_func (GtkCellLayout   *cell_layout,
				  GtkCellRenderer *cell,
				  GtkTreeModel    *tree_model,
				  GtkTreeIter     *iter,
				  GossipLogWindow *window)
{
	GossipAccount *account;
	GossipSession *session;
	GdkPixbuf     *pixbuf;
	gboolean       online;

	gtk_tree_model_get (tree_model, iter, COL_FIND_ACCOUNT, &account, -1);

	session = gossip_app_get_session ();
	online = gossip_session_is_connected (session, account);
	pixbuf = gossip_account_status_create_pixbuf (account, 
						      GTK_ICON_SIZE_MENU,
						      online);

	g_object_set (cell, "pixbuf", pixbuf, NULL);
	g_object_set (cell, "visible", TRUE, NULL);

	g_object_unref (pixbuf);
	g_object_unref (account);
}

static void
log_window_find_text_data_func (GtkCellLayout   *cell_layout,
				GtkCellRenderer *cell,
				GtkTreeModel    *tree_model,
				GtkTreeIter     *iter,
				GossipLogWindow *window)
{
	GossipAccount *account;

	gtk_tree_model_get (tree_model, iter, COL_FIND_ACCOUNT, &account, -1);

	g_object_set (cell, "text", gossip_account_get_name (account), NULL);
	g_object_unref (account);
}

static void
log_window_entry_find_changed_cb (GtkWidget       *entry,
				  GossipLogWindow *window)
{
	const gchar *str;
	gboolean     is_sensitive = TRUE;

	str = gtk_entry_get_text (GTK_ENTRY (window->entry_find));

	is_sensitive &= !G_STR_EMPTY (str);
	is_sensitive &= 
		!window->last_find || 
		(window->last_find && strcmp (window->last_find, str) != 0);

	gtk_widget_set_sensitive (window->button_find, is_sensitive);
}

static void
log_window_find_changed_cb (GtkTreeSelection *selection,
			    GossipLogWindow  *window)
{
	GtkTreeView   *view;
	GtkTreeModel  *model;
	GtkTreeIter    iter;
	GossipAccount *account;
	GossipContact *contact;
	gchar         *date;
	GossipContact *own_contact;
	GossipContact *sender;
	GossipMessage *message;
	GList         *messages;
	GList         *l;
	gboolean       can_do_previous;
	gboolean       can_do_next;

	/* Get selected information */
	view = GTK_TREE_VIEW (window->treeview_find);
	model = gtk_tree_view_get_model (view);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		gtk_widget_set_sensitive (window->button_previous, FALSE);
		gtk_widget_set_sensitive (window->button_next, FALSE);

		gossip_chat_view_clear (window->chatview_find);
	
		return;
	}

	gtk_widget_set_sensitive (window->button_previous, TRUE);
	gtk_widget_set_sensitive (window->button_next, TRUE);

	gtk_tree_model_get (model, &iter,
			    COL_FIND_ACCOUNT, &account,
			    COL_FIND_CONTACT, &contact,
			    COL_FIND_DATE, &date,
			    -1);

	/* Clear all current messages shown in the textview */
	gossip_chat_view_clear (window->chatview_find);

	/* Turn off scrolling temporarily */
	gossip_chat_view_allow_scroll (window->chatview_find, FALSE);

	/* Get own contact to know which messages are from me or the contact */
	own_contact = gossip_session_get_own_contact (gossip_app_get_session (),
						      account);
	g_object_unref (account);

	/* Get messages */
	messages = gossip_log_get_messages_for_contact (window->log_manager, contact, date);
	g_object_unref (contact);
	g_free (date);

	for (l = messages; l; l = l->next) {
		message = l->data;

		sender = gossip_message_get_sender (message);

		if (gossip_contact_equal (own_contact, sender)) {
			gossip_chat_view_append_message_from_self (window->chatview_find,
								   message,
								   own_contact,
								   NULL);
		} else {
			gossip_chat_view_append_message_from_other (window->chatview_find,
								    message,
								    sender,
								    NULL);
		}
	}

	g_list_foreach (messages, (GFunc) g_object_unref, NULL);
	g_list_free (messages);

	/* Scroll to the most recent messages */
	gossip_chat_view_allow_scroll (window->chatview_find, TRUE);

	/* Highlight and find messages */
	gossip_chat_view_highlight (window->chatview_find,
				    window->last_find);
	gossip_chat_view_find_next (window->chatview_find,
				    window->last_find,
				    TRUE);
	gossip_chat_view_find_abilities (window->chatview_find,
					 window->last_find,
					 &can_do_previous,
					 &can_do_next);
	gtk_widget_set_sensitive (window->button_previous, can_do_previous);
	gtk_widget_set_sensitive (window->button_next, can_do_next);
	gtk_widget_set_sensitive (window->button_find, FALSE);
}

static void
log_window_find_populate (GossipLogWindow *window,
			  const gchar     *search_criteria)
{
	GossipSession      *session;
	GossipAccount      *account;
	GossipContact      *contact;

	GList              *hits;
	GList              *l;
	GossipLogSearchHit *hit;

	GtkTreeView        *view;
	GtkTreeModel       *model;
	GtkTreeSelection   *selection;
	GtkListStore       *store;
	GtkTreeIter         iter;

	gossip_debug (DEBUG_DOMAIN, "Clearing search results treeview/textview");
		
	view = GTK_TREE_VIEW (window->treeview_find);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);
	store = GTK_LIST_STORE (model);

	session = gossip_app_get_session ();

	gossip_chat_view_clear (window->chatview_find);

	gtk_list_store_clear (store);

	if (G_STR_EMPTY (search_criteria)) {
		/* Just clear the search. */
		gossip_debug (DEBUG_DOMAIN, "No search results found");
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "Starting search...");
	hits = gossip_log_search_new (window->log_manager, search_criteria);

	gossip_debug (DEBUG_DOMAIN, "Adding %d hits", g_list_length (hits));
	for (l = hits; l; l = l->next) {
		const gchar *date;
		gchar       *date_readable;

		hit = l->data;

		account = gossip_log_search_hit_get_account (hit);
		contact = gossip_log_search_hit_get_contact (hit);

		/* Protect against invalid data (corrupt or old log files. */
		if (!account || !contact) {
			continue;
		}

		date = gossip_log_search_hit_get_date (hit);
		date_readable = gossip_log_get_date_readable (date);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_FIND_ACCOUNT, account,
				    COL_FIND_CONTACT, contact,
				    COL_FIND_CONTACT_NAME, gossip_contact_get_name (contact),
				    COL_FIND_DATE, date,
				    COL_FIND_DATE_READABLE, date_readable,
				    -1);

		g_free (date_readable);
	}

	if (hits) {
		gossip_log_search_free (hits);
	}
}

static void
log_window_find_setup (GossipLogWindow *window)
{
	GtkTreeView       *view;
	GtkTreeModel      *model;
	GtkTreeSelection  *selection;
	GtkTreeSortable   *sortable;
	GtkTreeViewColumn *column;
	GtkListStore      *store;
	GtkCellRenderer   *cell;
	gint               offset;

	view = GTK_TREE_VIEW (window->treeview_find);
	selection = gtk_tree_view_get_selection (view);

	/* New store */
	store = gtk_list_store_new (COL_FIND_COUNT,
				    GDK_TYPE_PIXBUF,        /* account status */
				    GOSSIP_TYPE_ACCOUNT,    /* account */
				    GOSSIP_TYPE_CONTACT,    /* contact */
				    G_TYPE_STRING,          /* name */
				    G_TYPE_STRING,          /* date */
				    G_TYPE_STRING);         /* date_readable */

	model = GTK_TREE_MODEL (store);
	sortable = GTK_TREE_SORTABLE (store);

	gtk_tree_view_set_model (view, model);

	/* New column */
	column = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc)
						 log_window_find_pixbuf_data_func,
						 window,
						 NULL);

	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc)
						 log_window_find_text_data_func,
						 window,
						 NULL);

	gtk_tree_view_column_set_title (column, _("Account"));
	gtk_tree_view_append_column (view, column);

	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_clickable (column, TRUE);

	cell = gtk_cell_renderer_text_new ();
	offset = gtk_tree_view_insert_column_with_attributes (view, -1, _("Conversation With"),
							      cell, "text", COL_FIND_CONTACT_NAME,
							      NULL);

	column = gtk_tree_view_get_column (view, offset - 1);
	gtk_tree_view_column_set_sort_column_id (column, COL_FIND_CONTACT_NAME);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_clickable (column, TRUE);

	cell = gtk_cell_renderer_text_new ();
	offset = gtk_tree_view_insert_column_with_attributes (view, -1, _("Date"),
							      cell, "text", COL_FIND_DATE_READABLE,
							      NULL);

	column = gtk_tree_view_get_column (view, offset - 1);
	gtk_tree_view_column_set_sort_column_id (column, COL_FIND_DATE);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_clickable (column, TRUE);

	/* Set up treeview properties */
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	gtk_tree_sortable_set_sort_column_id (sortable,
					      COL_FIND_DATE,
					      GTK_SORT_ASCENDING);

	/* Set up signals */
	g_signal_connect (selection, "changed",
			  G_CALLBACK (log_window_find_changed_cb),
			  window);

	g_object_unref (store);
}

static void
log_window_button_find_clicked_cb (GtkWidget       *widget,
				   GossipLogWindow *window)
{
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (window->entry_find));

	/* Don't find the same crap again */
	if (window->last_find && strcmp (window->last_find, str) == 0) {
		gossip_debug (DEBUG_DOMAIN, 
			      "Not searching for:'%s' in all log files "
			      "(same as last search)", 
			      str);
		return;
	}

	g_free (window->last_find);
	window->last_find = g_strdup (str);

	gossip_debug (DEBUG_DOMAIN, "Searching for:'%s' in all log files", str);
	log_window_find_populate (window, str);
}

static void
log_window_button_next_clicked_cb (GtkWidget       *widget,
				   GossipLogWindow *window)
{
	if (window->last_find) {
		gboolean can_do_previous;
		gboolean can_do_next;

		gossip_chat_view_find_next (window->chatview_find,
					    window->last_find,
					    FALSE);
		gossip_chat_view_find_abilities (window->chatview_find,
						 window->last_find,
						 &can_do_previous,
						 &can_do_next);
		gtk_widget_set_sensitive (window->button_previous, can_do_previous);
		gtk_widget_set_sensitive (window->button_next, can_do_next);
	}
}

static void
log_window_button_previous_clicked_cb (GtkWidget       *widget,
				       GossipLogWindow *window)
{
	if (window->last_find) {
		gboolean can_do_previous;
		gboolean can_do_next;

		gossip_chat_view_find_previous (window->chatview_find,
						window->last_find,
						FALSE);
		gossip_chat_view_find_abilities (window->chatview_find,
						 window->last_find,
						 &can_do_previous,
						 &can_do_next);
		gtk_widget_set_sensitive (window->button_previous, can_do_previous);
		gtk_widget_set_sensitive (window->button_next, can_do_next);
	}
}

/*
 * Contacts Code
 */
static void
log_window_contacts_pixbuf_data_func (GtkCellLayout   *cell_layout,
				      GtkCellRenderer *cell,
				      GtkTreeModel    *tree_model,
				      GtkTreeIter     *iter,
				      GossipLogWindow *window)
{
	GossipContact  *contact;
	GdkPixbuf      *pixbuf = NULL;

	gtk_tree_model_get (tree_model, iter,
			    COL_CONTACTS_POINTER, &contact,
			    -1);

	if (contact) {
		pixbuf = gossip_pixbuf_for_contact (contact);
		g_object_unref (contact);
	}

	/* We show nothing because the pixbuf may be inaccurate since
	 * the contact is just a temporary contact and the real contact
	 * may actually be online.
	 */
	g_object_set (cell, "pixbuf", pixbuf, NULL);
	g_object_set (cell, "visible", FALSE, NULL);

	if (pixbuf) {
		g_object_unref (pixbuf);
	}
}

static void
log_window_contacts_text_data_func (GtkCellLayout   *cell_layout,
				    GtkCellRenderer *cell,
				    GtkTreeModel    *tree_model,
				    GtkTreeIter     *iter,
				    GossipLogWindow *window)
{
	GossipContact *contact;

	gtk_tree_model_get (tree_model, iter,
			    COL_CONTACTS_POINTER, &contact,
			    -1);

	if (contact) {
		g_object_set (cell, "text", gossip_contact_get_name (contact), NULL);
		g_object_unref (contact);
	}
}

static void
log_window_contacts_changed_cb (GtkTreeSelection *selection,
				GossipLogWindow  *window)
{
	/* Use last date by default */
	gtk_calendar_clear_marks (GTK_CALENDAR (window->calendar_contacts));

	log_window_contacts_get_messages (window, NULL);
}

static void
log_window_contacts_set_selected  (GossipLogWindow *window,
				   GossipContact   *contact)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	GtkTreePath      *path;
	GossipContact    *this_contact;
	gboolean          ok;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	view = GTK_TREE_VIEW (window->treeview_contacts);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		return;
	}

	for (ok = TRUE; ok; ok = gtk_tree_model_iter_next (model, &iter)) {
		gtk_tree_model_get (model, &iter, COL_CONTACTS_POINTER, &this_contact, -1);

		if (!this_contact) {
			continue;
		}

		if (gossip_contact_equal (contact, this_contact)) {
			gtk_tree_selection_select_iter (selection, &iter);
			path = gtk_tree_model_get_path (model, &iter);
			gtk_tree_view_scroll_to_cell (view, path, NULL, TRUE, 0.5, 0.0);
			gtk_tree_path_free (path);
			g_object_unref (this_contact);
			break;
		}

		g_object_unref (this_contact);
	}
}

static GossipContact *
log_window_contacts_get_selected (GossipLogWindow *window)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	GossipContact    *contact;

	view = GTK_TREE_VIEW (window->treeview_contacts);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter, COL_CONTACTS_POINTER, &contact, -1);

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

	GtkTreeView          *view;
	GtkTreeModel         *model;
	GtkTreeSelection     *selection;
	GtkListStore         *store;
	GtkTreeIter           iter;

	account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser_contacts);
	account = gossip_account_chooser_get_account (account_chooser);

	view = GTK_TREE_VIEW (window->treeview_contacts);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);
	store = GTK_LIST_STORE (model);

	/* Block signals to stop the logs being retrieved prematurely */
	g_signal_handlers_block_by_func (selection,
					 log_window_contacts_changed_cb,
					 window);

	gtk_list_store_clear (store);

	contacts = gossip_log_get_contacts (window->log_manager, account);

	/* Contacts */
	for (l = contacts; l; l = l->next) {
		contact = l->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_CONTACTS_NAME, gossip_contact_get_name (contact),
				    COL_CONTACTS_POINTER, contact,
				    -1);
	}

	/* Unblock signals */
	g_signal_handlers_unblock_by_func (selection,
					   log_window_contacts_changed_cb,
					   window);

	g_list_foreach (contacts, (GFunc) g_object_unref, NULL);
	g_list_free (contacts);

	g_object_unref (account);
}

static void
log_window_contacts_setup (GossipLogWindow *window)
{
	GtkTreeView       *view;
	GtkTreeModel      *model;
	GtkTreeSelection  *selection;
	GtkTreeSortable   *sortable;
	GtkTreeViewColumn *column;
	GtkListStore      *store;
	GtkCellRenderer   *cell;

	view = GTK_TREE_VIEW (window->treeview_contacts);
	selection = gtk_tree_view_get_selection (view);

	/* new store */
	store = gtk_list_store_new (COL_CONTACTS_COUNT,
				    GDK_TYPE_PIXBUF,  /* status */
				    G_TYPE_STRING,    /* name */
				    GOSSIP_TYPE_CONTACT);

	model = GTK_TREE_MODEL (store);
	sortable = GTK_TREE_SORTABLE (store);

	gtk_tree_view_set_model (view, model);

	/* new column */
	column = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc)
						 log_window_contacts_pixbuf_data_func,
						 window,
						 NULL);

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc)
						 log_window_contacts_text_data_func,
						 window,
						 NULL);

	gtk_tree_view_append_column (view, column);

	/* set up treeview properties */
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	gtk_tree_sortable_set_sort_column_id (sortable,
					      COL_CONTACTS_NAME,
					      GTK_SORT_ASCENDING);

	/* set up signals */
	g_signal_connect (selection, "changed",
			  G_CALLBACK (log_window_contacts_changed_cb),
			  window);

	g_object_unref (store);
}

static void
log_window_contacts_accounts_changed_cb (GtkWidget       *combobox,
					 GossipLogWindow *window)
{
	/* Clear all current messages shown in the textview */
	gossip_chat_view_clear (window->chatview_contacts);

	log_window_contacts_populate (window);
}

static void
log_window_contacts_new_message_cb (GossipContact   *own_contact,
				    GossipMessage   *message,
				    GossipLogWindow *window)
{
	GossipContact *sender;

	/* Get own contact to know which messages are from me or the contact */
	sender = gossip_message_get_sender (message);

	if (gossip_contact_equal (own_contact, sender)) {
		gossip_chat_view_append_message_from_self (window->chatview_contacts,
							   message,
							   own_contact,
							   NULL);
	} else {
		gossip_chat_view_append_message_from_other (window->chatview_contacts,
							    message,
							    sender,
							    NULL);
	}

	/* Scroll to the most recent messages */
	gossip_chat_view_scroll_down_smoothly (window->chatview_contacts);
}

static gboolean
log_window_contacts_is_today_selected (GossipLogWindow *window)
{
	GossipTime  t;
	gchar      *timestamp;
	guint       year_selected;
	guint       year;
	guint       month;
	guint       month_selected;
	guint       day;
	guint       day_selected;
	gboolean    selected;

	t = gossip_time_get_current ();
	timestamp = gossip_time_to_string_local (t, "%Y%m%d");

	sscanf (timestamp, "%4d%2d%2d", &year, &month, &day);

	gtk_calendar_get_date (GTK_CALENDAR (window->calendar_contacts),
			       &year_selected,
			       &month_selected,
			       &day_selected);

	/* Hack since this starts at 0 */
	month_selected++;

	selected = (day_selected == day &&
		    month_selected == month &&
		    year_selected == year);

	g_free (timestamp);

	return selected;
}

static void
log_window_contacts_get_messages (GossipLogWindow *window,
				  const gchar     *date_to_show)
{
	GossipAccount *account;
	GossipContact *contact;
	GossipContact *own_contact;
	GossipContact *sender;
	GossipMessage *message;
	GList         *messages;
	GList         *dates = NULL;
	GList         *l;
	const gchar   *date;
	guint          year_selected;
	guint          year;
	guint          month;
	guint          month_selected;
	guint          day;

	contact = log_window_contacts_get_selected (window);
	if (!contact) {
		return;
	}

	/* Either use the supplied date or get the last */
	date = date_to_show;
	if (!date) {
		gboolean day_selected = FALSE;

		/* Get a list of dates and show them on the calendar */
		dates = gossip_log_get_dates_for_contact (contact);

		for (l = dates; l; l = l->next) {
			const gchar *str;

			str = l->data;
			if (!str) {
				continue;
			}

			sscanf (str, "%4d%2d%2d", &year, &month, &day);
			gtk_calendar_get_date (GTK_CALENDAR (window->calendar_contacts),
					       &year_selected,
					       &month_selected,
					       NULL);

			month_selected++;

			if (!l->next) {
				date = str;
			}

			if (year != year_selected || month != month_selected) {
				continue;
			}


			gossip_debug (DEBUG_DOMAIN, "Marking date:'%s'", str);
			gtk_calendar_mark_day (GTK_CALENDAR (window->calendar_contacts), day);

			if (l->next) {
				continue;
			}

			day_selected = TRUE;

			g_signal_handlers_block_by_func (window->calendar_contacts,
							 log_window_calendar_contacts_day_selected_cb,
							 window);
			gtk_calendar_select_day (GTK_CALENDAR (window->calendar_contacts), day);
			g_signal_handlers_unblock_by_func (window->calendar_contacts,
							   log_window_calendar_contacts_day_selected_cb,
							   window);
		}

		if (!day_selected) {
			/* Unselect the day in the calendar */
			g_signal_handlers_block_by_func (window->calendar_contacts,
							 log_window_calendar_contacts_day_selected_cb,
							 window);
			gtk_calendar_select_day (GTK_CALENDAR (window->calendar_contacts), 0);
			g_signal_handlers_unblock_by_func (window->calendar_contacts,
							   log_window_calendar_contacts_day_selected_cb,
							   window);
		}
	} else {
		sscanf (date, "%4d%2d%2d", &year, &month, &day);
		gtk_calendar_get_date (GTK_CALENDAR (window->calendar_contacts),
				       &year_selected,
				       &month_selected,
				       NULL);

		month_selected++;

		if (year != year_selected && month != month_selected) {
			day = 0;
		}

		g_signal_handlers_block_by_func (window->calendar_contacts,
						 log_window_calendar_contacts_day_selected_cb,
						 window);

		gtk_calendar_select_day (GTK_CALENDAR (window->calendar_contacts), day);

		g_signal_handlers_unblock_by_func (window->calendar_contacts,
						   log_window_calendar_contacts_day_selected_cb,
						   window);
	}

	if (log_window_contacts_is_today_selected (window)) {
		gossip_log_handler_add_for_contact
			(window->log_manager, 
			 contact,
			 (GossipLogMessageFunc) log_window_contacts_new_message_cb,
			 window);
	} else {
		gossip_log_handler_remove
			(window->log_manager, 
			 (GossipLogMessageFunc) log_window_contacts_new_message_cb);
	}

	/* Clear all current messages shown in the textview */
	gossip_chat_view_clear (window->chatview_contacts);

	/* Turn off scrolling temporarily */
	gossip_chat_view_allow_scroll (window->chatview_contacts, FALSE);

	/* Get own contact to know which messages are from me or the contact */
	account = gossip_contact_get_account (contact);
	own_contact = gossip_session_get_own_contact (gossip_app_get_session (),
						      account);

	/* Get messages */
	messages = gossip_log_get_messages_for_contact (window->log_manager, contact, date);

	for (l = messages; l; l = l->next) {
		message = l->data;

		sender = gossip_message_get_sender (message);

		if (gossip_contact_equal (own_contact, sender)) {
			gossip_chat_view_append_message_from_self (window->chatview_contacts,
								   message,
								   own_contact,
								   NULL);
		} else {
			gossip_chat_view_append_message_from_other (window->chatview_contacts,
								    message,
								    sender,
								    NULL);
		}
	}

	g_list_foreach (messages, (GFunc) g_object_unref, NULL);
	g_list_free (messages);

	g_list_foreach (dates, (GFunc) g_free, NULL);
	g_list_free (dates);

	g_object_unref (contact);

	/* Turn back on scrolling */
	gossip_chat_view_allow_scroll (window->chatview_contacts, TRUE);

	/* Scroll to the most recent messages */
	gossip_chat_view_scroll_down (window->chatview_contacts);

	/* Give the search entry main focus */
	gtk_widget_grab_focus (window->entry_contacts);
}

static void
log_window_calendar_contacts_day_selected_cb (GtkWidget       *calendar,
					      GossipLogWindow *window)
{
	guint  year;
	guint  month;
	guint  day;

	gchar *date;

	gtk_calendar_get_date (GTK_CALENDAR (calendar), &year, &month, &day);

	/* We need this hear because it appears that the months start from 0 */
	month++;

	date = g_strdup_printf ("%4.4d%2.2d%2.2d", year, month, day);

	gossip_debug (DEBUG_DOMAIN, "Currently selected date is:'%s'", date);

	log_window_contacts_get_messages (window, date);

	g_free (date);
}

static void
log_window_calendar_contacts_month_changed_cb (GtkWidget       *calendar,
					       GossipLogWindow *window)
{
	GossipContact *contact;
	guint          year_selected;
	guint          month_selected;

	GList         *dates;
	GList         *l;

	gtk_calendar_clear_marks (GTK_CALENDAR (calendar));

	contact = log_window_contacts_get_selected (window);
	if (!contact) {
		gossip_debug (DEBUG_DOMAIN, "No contact selected to get dates for...");
		return;
	}

	g_object_get (calendar,
		      "month", &month_selected,
		      "year", &year_selected,
		      NULL);

	/* We need this hear because it appears that the months start from 0 */
	month_selected++;

	/* Get the log object for this contact */
	dates = gossip_log_get_dates_for_contact (contact);
	g_object_unref (contact);

	for (l = dates; l; l = l->next) {
		const gchar *str;
		guint        year;
		guint        month;
		guint        day;

		str = l->data;
		if (!str) {
			continue;
		}

		sscanf (str, "%4d%2d%2d", &year, &month, &day);

		if (year == year_selected && month == month_selected) {
			gossip_debug (DEBUG_DOMAIN, "Marking date:'%s'", str);
			gtk_calendar_mark_day (GTK_CALENDAR (window->calendar_contacts), day);
		}
	}

	g_list_foreach (dates, (GFunc) g_free, NULL);
	g_list_free (dates);

	gossip_debug (DEBUG_DOMAIN,
		      "Currently showing month %d and year %d",
		      month_selected, year_selected);
}

static void
log_window_entry_contacts_changed_cb (GtkWidget       *entry,
				      GossipLogWindow *window)
{
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (window->entry_contacts));
	gossip_chat_view_highlight (window->chatview_contacts, str);

	if (str) {
		gossip_chat_view_find_next (window->chatview_contacts,
					    str,
					    TRUE);
	}
}

static void
log_window_entry_contacts_activate_cb (GtkWidget       *entry,
				       GossipLogWindow *window)
{
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (window->entry_contacts));

	if (str) {
		gossip_chat_view_find_next (window->chatview_contacts,
					    str,
					    FALSE);
	}
}

/*
 * Chatrooms Code
 */
static void
log_window_chatrooms_pixbuf_data_func (GtkCellLayout   *cell_layout,
				       GtkCellRenderer *cell,
				       GtkTreeModel    *tree_model,
				       GtkTreeIter     *iter,
				       GossipLogWindow *window)
{
	GossipChatroom *chatroom;
	GdkPixbuf      *pixbuf = NULL;

	gtk_tree_model_get (tree_model, iter,
			    COL_CHATROOMS_POINTER, &chatroom,
			    -1);

	if (chatroom) {
		pixbuf = gossip_pixbuf_for_chatroom_status (chatroom,
							    GTK_ICON_SIZE_MENU);
		g_object_unref (chatroom);
	}

	g_object_set (cell, "pixbuf", pixbuf, NULL);
	g_object_set (cell, "visible", TRUE, NULL);

	if (pixbuf) {
		g_object_unref (pixbuf);
	}
}

static void
log_window_chatrooms_text_data_func (GtkCellLayout   *cell_layout,
				     GtkCellRenderer *cell,
				     GtkTreeModel    *tree_model,
				     GtkTreeIter     *iter,
				     GossipLogWindow *window)
{
	GossipChatroom *chatroom;

	gtk_tree_model_get (tree_model, iter,
			    COL_CHATROOMS_POINTER, &chatroom,
			    -1);

	if (chatroom) {
		g_object_set (cell, "text", gossip_chatroom_get_name (chatroom), NULL);
		g_object_unref (chatroom);
	}
}

static void
log_window_chatrooms_changed_cb (GtkTreeSelection *selection,
				 GossipLogWindow  *window)
{
	/* Use last date by default */
	gtk_calendar_clear_marks (GTK_CALENDAR (window->calendar_chatrooms));

	log_window_chatrooms_get_messages (window, NULL);
}

static void
log_window_chatrooms_set_selected  (GossipLogWindow *window,
				    GossipChatroom  *chatroom)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	GossipChatroom   *this_chatroom;
	gboolean          ok;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));

	view = GTK_TREE_VIEW (window->treeview_chatrooms);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		return;
	}

	for (ok = TRUE; ok; ok = gtk_tree_model_iter_next (model, &iter)) {
		gtk_tree_model_get (model, &iter, COL_CHATROOMS_POINTER, &this_chatroom, -1);

		if (!this_chatroom) {
			continue;
		}

		if (gossip_chatroom_equal_full (chatroom, this_chatroom)) {
			gtk_tree_selection_select_iter (selection, &iter);
			g_object_unref (this_chatroom);
			break;
		}

		g_object_unref (this_chatroom);
	}
}

static GossipChatroom *
log_window_chatrooms_get_selected (GossipLogWindow *window)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	GossipChatroom   *chatroom;

	view = GTK_TREE_VIEW (window->treeview_chatrooms);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter, COL_CHATROOMS_POINTER, &chatroom, -1);

	return chatroom;
}

static void
log_window_chatrooms_populate (GossipLogWindow *window)
{
	GossipAccountChooser *account_chooser;
	GossipAccount        *account;
	GossipChatroom       *chatroom;
	GList                *chatrooms;
	GList                *l;

	GtkTreeView          *view;
	GtkTreeModel         *model;
	GtkTreeSelection     *selection;
	GtkListStore         *store;
	GtkTreeIter           iter;

	account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser_chatrooms);
	account = gossip_account_chooser_get_account (account_chooser);

	view = GTK_TREE_VIEW (window->treeview_chatrooms);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);
	store = GTK_LIST_STORE (model);

	/* Block signals to stop the logs being retrieved prematurely */
	g_signal_handlers_block_by_func (selection,
					 log_window_chatrooms_changed_cb,
					 window);

	gtk_list_store_clear (store);

	chatrooms = gossip_log_get_chatrooms (window->log_manager, account);

	/* Chatrooms */
	for (l = chatrooms; l; l = l->next) {
		chatroom = l->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_CHATROOMS_NAME, gossip_chatroom_get_name (chatroom),
				    COL_CHATROOMS_POINTER, chatroom,
				    -1);
	}

	/* Unblock signals */
	g_signal_handlers_unblock_by_func (selection,
					   log_window_chatrooms_changed_cb,
					   window);

	g_list_foreach (chatrooms, (GFunc) g_object_unref, NULL);
	g_list_free (chatrooms);

	g_object_unref (account);
}

static void
log_window_chatrooms_setup (GossipLogWindow *window)
{
	GtkTreeView       *view;
	GtkTreeModel      *model;
	GtkTreeSelection  *selection;
	GtkTreeSortable   *sortable;
	GtkTreeViewColumn *column;
	GtkListStore      *store;
	GtkCellRenderer   *cell;

	view = GTK_TREE_VIEW (window->treeview_chatrooms);
	selection = gtk_tree_view_get_selection (view);

	/* new store */
	store = gtk_list_store_new (COL_CHATROOMS_COUNT,
				    GDK_TYPE_PIXBUF,  /* status */
				    G_TYPE_STRING,    /* name */
				    GOSSIP_TYPE_CHATROOM);

	model = GTK_TREE_MODEL (store);
	sortable = GTK_TREE_SORTABLE (store);

	gtk_tree_view_set_model (view, model);

	/* new column */
	column = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc)
						 log_window_chatrooms_pixbuf_data_func,
						 window,
						 NULL);

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc)
						 log_window_chatrooms_text_data_func,
						 window,
						 NULL);

	gtk_tree_view_append_column (view, column);

	/* set up treeview properties */
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	gtk_tree_sortable_set_sort_column_id (sortable,
					      COL_CHATROOMS_NAME,
					      GTK_SORT_ASCENDING);

	/* set up signals */
	g_signal_connect (selection, "changed",
			  G_CALLBACK (log_window_chatrooms_changed_cb),
			  window);

	g_object_unref (store);
}

static void
log_window_chatrooms_accounts_changed_cb (GtkWidget       *combobox,
					  GossipLogWindow *window)
{
	/* Clear all current messages shown in the textview */
	gossip_chat_view_clear (window->chatview_chatrooms);

	log_window_chatrooms_populate (window);
}

static void
log_window_chatrooms_new_message_cb (GossipContact   *own_contact,
				     GossipMessage   *message,
				     GossipLogWindow *window)
{
	GossipContact *sender;

	sender = gossip_message_get_sender (message);

	if (gossip_contact_equal (own_contact, sender)) {
		gossip_chat_view_append_message_from_self (window->chatview_chatrooms,
							   message,
							   own_contact,
							   NULL);
	} else {
		gossip_chat_view_append_message_from_other (window->chatview_chatrooms,
							    message,
							    sender,
							    NULL);
	}

	/* Scroll to the most recent messages */
	gossip_chat_view_scroll_down_smoothly (window->chatview_chatrooms);
}

static gboolean
log_window_chatrooms_is_today_selected (GossipLogWindow *window)
{
	GossipTime  t;
	gchar      *timestamp;
	guint       year_selected;
	guint       year;
	guint       month;
	guint       month_selected;
	guint       day;
	guint       day_selected;
	gboolean    selected;

	t = gossip_time_get_current ();
	timestamp = gossip_time_to_string_local (t, "%Y%m%d");

	sscanf (timestamp, "%4d%2d%2d", &year, &month, &day);

	gtk_calendar_get_date (GTK_CALENDAR (window->calendar_chatrooms),
			       &year_selected,
			       &month_selected,
			       &day_selected);

	/* Hack since this starts at 0 */
	month_selected++;

	selected = (day_selected == day &&
		    month_selected == month &&
		    year_selected == year);

	g_free (timestamp);

	return selected;
}

static void
log_window_chatrooms_get_messages (GossipLogWindow *window,
				   const gchar     *date_to_show)
{
	GossipAccount  *account;
	GossipChatroom *chatroom;
	GossipContact  *own_contact;
	GossipMessage  *message;
	GList          *messages;
	GList          *dates = NULL;
	GList          *l;
	const gchar    *date;
	guint           year_selected;
	guint           year;
	guint           month;
	guint           month_selected;
	guint           day;

	chatroom = log_window_chatrooms_get_selected (window);
	if (!chatroom) {
		return;
	}

	account = gossip_chatroom_get_account (chatroom);
	if (!account) {
		/* Protect against invalid data. */
		gossip_debug (DEBUG_DOMAIN, "No account for the chatroom");
		return;
	}

	/* Get own contact to know which messages are from me or the contact */
	own_contact = gossip_session_get_own_contact (gossip_app_get_session (),
						      account);
	/* Either use the supplied date or get the last */
	date = date_to_show;
	if (!date) {
		gboolean day_selected = FALSE;

		/* Get a list of dates and show them on the calendar */
		dates = gossip_log_get_dates_for_chatroom (chatroom);

		for (l = dates; l; l = l->next) {
			const gchar *str;

			str = l->data;
			if (!str) {
				continue;
			}

			sscanf (str, "%4d%2d%2d", &year, &month, &day);
			gtk_calendar_get_date (GTK_CALENDAR (window->calendar_chatrooms),
					       &year_selected,
					       &month_selected,
					       NULL);

			month_selected++;

			if (!l->next) {
				date = str;
			}

			if (year != year_selected || month != month_selected) {
				continue;
			}

			gossip_debug (DEBUG_DOMAIN, "Marking date:'%s'", str);
			gtk_calendar_mark_day (GTK_CALENDAR (window->calendar_chatrooms), day);

			if (l->next) {
				continue;
			}

			day_selected = TRUE;

			g_signal_handlers_block_by_func (window->calendar_chatrooms,
							 log_window_calendar_chatrooms_day_selected_cb,
							 window);
			gtk_calendar_select_day (GTK_CALENDAR (window->calendar_chatrooms), day);
			g_signal_handlers_unblock_by_func (window->calendar_chatrooms,
							   log_window_calendar_chatrooms_day_selected_cb,
							   window);
		}

		if (!day_selected) {
			/* Unselect the day in the calendar */
			g_signal_handlers_block_by_func (window->calendar_chatrooms,
							 log_window_calendar_chatrooms_day_selected_cb,
							 window);
			gtk_calendar_select_day (GTK_CALENDAR (window->calendar_chatrooms), 0);
			g_signal_handlers_unblock_by_func (window->calendar_chatrooms,
							   log_window_calendar_chatrooms_day_selected_cb,
							   window);
		}
	} else {
		sscanf (date, "%4d%2d%2d", &year, &month, &day);
		gtk_calendar_get_date (GTK_CALENDAR (window->calendar_chatrooms),
				       &year_selected,
				       &month_selected,
				       NULL);

		month_selected++;

		if (year != year_selected && month != month_selected) {
			day = 0;
		}

		g_signal_handlers_block_by_func (window->calendar_chatrooms,
						 log_window_calendar_chatrooms_day_selected_cb,
						 window);

		gtk_calendar_select_day (GTK_CALENDAR (window->calendar_chatrooms), day);

		g_signal_handlers_unblock_by_func (window->calendar_chatrooms,
						   log_window_calendar_chatrooms_day_selected_cb,
						   window);
	}

	if (log_window_chatrooms_is_today_selected (window)) {
		gossip_log_handler_add_for_chatroom
			(window->log_manager, 
			 chatroom,
			 (GossipLogMessageFunc) log_window_chatrooms_new_message_cb,
			 window);
	} else {
		gossip_log_handler_remove
			(window->log_manager,
			 (GossipLogMessageFunc) log_window_chatrooms_new_message_cb);
	}

	/* Clear all current messages shown in the textview */
	gossip_chat_view_clear (window->chatview_chatrooms);

	/* Turn off scrolling temporarily */
	gossip_chat_view_allow_scroll (window->chatview_chatrooms, FALSE);

	/* Get messages */
	messages = gossip_log_get_messages_for_chatroom (window->log_manager, chatroom, date);

	for (l = messages; l; l = l->next) {
		message = l->data;

		gossip_chat_view_append_message_from_other (window->chatview_chatrooms,
							    message,
							    gossip_message_get_sender (message),
							    NULL);
	}

	g_list_foreach (messages, (GFunc) g_object_unref, NULL);
	g_list_free (messages);

	g_list_foreach (dates, (GFunc) g_free, NULL);
	g_list_free (dates);

	g_object_unref (chatroom);

	/* Turn back on scrolling */
	gossip_chat_view_allow_scroll (window->chatview_chatrooms, TRUE);

	/* Scroll to the most recent messages */
	gossip_chat_view_scroll_down (window->chatview_chatrooms);

	/* Give the search entry main focus */
	gtk_widget_grab_focus (window->entry_chatrooms);
}

static void
log_window_calendar_chatrooms_day_selected_cb (GtkWidget       *calendar,
					       GossipLogWindow *window)
{
	guint  year;
	guint  month;
	guint  day;
	gchar *date;

	gtk_calendar_get_date (GTK_CALENDAR (calendar), &year, &month, &day);

	/* We need this hear because it appears that the months start from 0 */
	month++;

	date = g_strdup_printf ("%4.4d%2.2d%2.2d", year, month, day);

	gossip_debug (DEBUG_DOMAIN, "Currently selected date is:'%s'", date);

	log_window_chatrooms_get_messages (window, date);

	g_free (date);
}

static void
log_window_calendar_chatrooms_month_changed_cb (GtkWidget       *calendar,
						GossipLogWindow *window)
{
	GossipChatroom *chatroom;
	guint           year_selected;
	guint           month_selected;
	GList          *dates;
	GList          *l;

	gtk_calendar_clear_marks (GTK_CALENDAR (calendar));

	chatroom = log_window_chatrooms_get_selected (window);
	if (!chatroom) {
		gossip_debug (DEBUG_DOMAIN, "No chatroom selected to get dates for...");
		return;
	}

	g_object_get (calendar,
		      "month", &month_selected,
		      "year", &year_selected,
		      NULL);

	/* We need this hear because it appears that the months start from 0 */
	month_selected++;

	/* Get the log object for this contact */
	dates = gossip_log_get_dates_for_chatroom (chatroom);
	g_object_unref (chatroom);

	for (l = dates; l; l = l->next) {
		const gchar *str;
		guint        year;
		guint        month;
		guint        day;

		str = l->data;
		if (!str) {
			continue;
		}

		sscanf (str, "%4d%2d%2d", &year, &month, &day);

		if (year == year_selected && month == month_selected) {
			gossip_debug (DEBUG_DOMAIN, "Marking date:'%s'", str);
			gtk_calendar_mark_day (GTK_CALENDAR (window->calendar_chatrooms), day);
		}
	}

	g_list_foreach (dates, (GFunc) g_free, NULL);
	g_list_free (dates);

	gossip_debug (DEBUG_DOMAIN,
		      "Currently showing month %d and year %d",
		      month_selected, year_selected);
}

static void
log_window_entry_chatrooms_changed_cb (GtkWidget       *entry,
				       GossipLogWindow *window)
{
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (window->entry_chatrooms));
	gossip_chat_view_highlight (window->chatview_chatrooms, str);

	if (str) {
		gossip_chat_view_find_next (window->chatview_chatrooms,
					    str,
					    TRUE);
	}
}

static void
log_window_entry_chatrooms_activate_cb (GtkWidget       *entry,
					GossipLogWindow *window)
{
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (window->entry_chatrooms));

	if (str) {
		gossip_chat_view_find_next (window->chatview_chatrooms,
					    str,
					    FALSE);
	}
}

/*
 * Links
 */
static void
log_window_links_changed_cb (GtkTreeSelection *selection,
			     GossipLogWindow  *window)
{
	GtkTreeView   *view;
	GtkTreeModel  *model;
	GtkTreeIter    iter;

	/* Get selected information */
	view = GTK_TREE_VIEW (window->treeview_links);
	model = gtk_tree_view_get_model (view);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		gtk_widget_set_sensitive (window->button_links_open, FALSE);
		return;
	}

	gtk_widget_set_sensitive (window->button_links_open, TRUE);
}

static void
log_window_links_populate (GossipLogWindow *window,
			   const gchar     *search_criteria)
{
	GossipAccount      *account;
	GossipContact      *contact;

	GList              *hits;
	GList              *l;
	GossipLogSearchHit *hit;

	GtkTreeView        *view;
	GtkTreeModel       *model;
	GtkTreeSelection   *selection;
	GtkListStore       *store;
	GtkTreeIter         iter;

	gossip_debug (DEBUG_DOMAIN, "Clearing link results treeview");
		
	view = GTK_TREE_VIEW (window->treeview_links);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);
	store = GTK_LIST_STORE (model);

	gtk_list_store_clear (store);

	gossip_debug (DEBUG_DOMAIN, "Starting link search...");
	hits = gossip_log_search_links_new (window->log_manager, 
					    G_STR_EMPTY (search_criteria) ? NULL : search_criteria);

	gossip_debug (DEBUG_DOMAIN, "Adding %d hits", g_list_length (hits));
	for (l = hits; l; l = l->next) {
		const gchar *url;
		const gchar *date;
		gchar       *date_readable;

		hit = l->data;

		account = gossip_log_search_hit_get_account (hit);
		contact = gossip_log_search_hit_get_contact (hit);

		/* Protect against invalid data (corrupt or old log files. */
		if (!account || !contact) {
			continue;
		}

		url = gossip_log_search_hit_get_link (hit);
		date = gossip_log_search_hit_get_date (hit);
		date_readable = gossip_log_get_date_readable (date);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_LINKS_ACCOUNT, account,
				    COL_LINKS_CONTACT, contact,
				    COL_LINKS_CONTACT_NAME, gossip_contact_get_name (contact),
				    COL_LINKS_DATE, date,
				    COL_LINKS_DATE_READABLE, date_readable,
				    COL_LINKS_URL, url,
				    -1);

		g_free (date_readable);
	}

	if (hits) {
		gossip_log_search_free (hits);
	}
}

static void
log_window_links_setup (GossipLogWindow *window)
{
	GtkTreeView       *view;
	GtkTreeModel      *model;
	GtkTreeSelection  *selection;
	GtkTreeSortable   *sortable;
	GtkTreeViewColumn *column;
	GtkListStore      *store;
	GtkCellRenderer   *cell;
	gint               offset;

	view = GTK_TREE_VIEW (window->treeview_links);
	selection = gtk_tree_view_get_selection (view);

	/* New store */
	store = gtk_list_store_new (COL_LINKS_COUNT,
				    GDK_TYPE_PIXBUF,        /* account status */
				    GOSSIP_TYPE_ACCOUNT,    /* account */
				    GOSSIP_TYPE_CONTACT,    /* contact */
				    G_TYPE_STRING,          /* name */
				    G_TYPE_STRING,          /* date */
				    G_TYPE_STRING,          /* date_readable */
				    G_TYPE_STRING);         /* url */

	model = GTK_TREE_MODEL (store);
	sortable = GTK_TREE_SORTABLE (store);

	gtk_tree_view_set_model (view, model);

	/* Account */
	column = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc)
						 log_window_find_pixbuf_data_func,
						 window,
						 NULL);

	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc)
						 log_window_find_text_data_func,
						 window,
						 NULL);

	gtk_tree_view_column_set_title (column, _("Account"));
	gtk_tree_view_append_column (view, column);

	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_clickable (column, TRUE);

	/* Who */
	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	offset = gtk_tree_view_insert_column_with_attributes (view, -1, _("Conversation With"),
							      cell, "text", COL_LINKS_CONTACT_NAME,
							      NULL);

	column = gtk_tree_view_get_column (view, offset - 1);
	gtk_tree_view_column_set_sort_column_id (column, COL_LINKS_CONTACT_NAME);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_clickable (column, TRUE);

	/* Date */
	cell = gtk_cell_renderer_text_new ();
	offset = gtk_tree_view_insert_column_with_attributes (view, -1, _("Date"),
							      cell, "text", COL_LINKS_DATE_READABLE,
							      NULL);

	column = gtk_tree_view_get_column (view, offset - 1);
	gtk_tree_view_column_set_sort_column_id (column, COL_LINKS_DATE);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_clickable (column, TRUE);

	/* Link */
	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	offset = gtk_tree_view_insert_column_with_attributes (view, -1, _("Link"),
							      cell, "text", COL_LINKS_URL,
							      NULL);

	column = gtk_tree_view_get_column (view, offset - 1);
	gtk_tree_view_column_set_sort_column_id (column, COL_LINKS_URL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_clickable (column, TRUE);

	/* Set up treeview properties */
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	gtk_tree_sortable_set_sort_column_id (sortable,
					      COL_LINKS_DATE,
					      GTK_SORT_ASCENDING);

	/* Set up signals */
	g_signal_connect (selection, "changed",
			  G_CALLBACK (log_window_links_changed_cb),
			  window);

	g_object_unref (store);
}

static void
log_window_button_links_find_clicked_cb (GtkWidget       *widget,
					 GossipLogWindow *window)
{
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (window->entry_links_find));

	/* Don't find the same crap again */
	if (window->last_links_find && strcmp (window->last_links_find, str) == 0) {
		gossip_debug (DEBUG_DOMAIN, 
			      "Not searching for:'%s' in all links in all "
			      "log files (same as last search)", 
			      str);
		return;
	}

	g_free (window->last_links_find);
	window->last_links_find = g_strdup (str);

	gossip_debug (DEBUG_DOMAIN, "Searching for:'%s' in links in all log files", str);
	log_window_links_populate (window, str);
}

static void
log_window_button_links_open_clicked_cb (GtkWidget       *widget,
					 GossipLogWindow *window)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	gchar            *url;

	/* Get selected information */
	view = GTK_TREE_VIEW (window->treeview_links);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return;
	}
	
	gtk_tree_model_get (model, &iter, COL_LINKS_URL, &url, -1);
	gossip_url_show (url);
	g_free (url);
}

/*
 * Other window callbacks
 */
static void
log_window_destroy_cb (GtkWidget       *widget,
		       GossipLogWindow *window)
{
	g_free (window->last_links_find);
	g_free (window->last_find);

	gossip_log_handler_remove
		(window->log_manager,
		 (GossipLogMessageFunc) log_window_contacts_new_message_cb);
	gossip_log_handler_remove
		(window->log_manager,
		 (GossipLogMessageFunc) log_window_chatrooms_new_message_cb);

	g_object_unref (window->log_manager);

	g_free (window);
}

void
gossip_log_window_show (GossipContact  *contact,
			GossipChatroom *chatroom)
{
	static GossipLogWindow *window = NULL;
	GossipSession          *session;
	GossipLogManager       *log_manager;
	GossipAccountChooser   *account_chooser;
	GList                  *accounts;
	gint                    account_num;
	GladeXML               *glade;

	if (window) {
		gtk_window_present (GTK_WINDOW (window->window));

		if (contact) {
			gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), 1);
			log_window_contacts_set_selected (window, contact);
		} else if (chatroom) {
			gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), 2);
			log_window_chatrooms_set_selected (window, chatroom);
		}

		return;
	}

	window = g_new0 (GossipLogWindow, 1);

	log_manager = gossip_session_get_log_manager (gossip_app_get_session ());
	window->log_manager = g_object_ref (log_manager);

	glade = gossip_glade_get_file ("main.glade",
				       "log_window",
				       NULL,
				       "log_window", &window->window,
				       "notebook", &window->notebook,
				       "entry_find", &window->entry_find,
				       "button_find", &window->button_find,
				       "treeview_find", &window->treeview_find,
				       "scrolledwindow_find", &window->scrolledwindow_find,
				       "button_previous", &window->button_previous,
				       "button_next", &window->button_next,
				       "entry_contacts", &window->entry_contacts,
				       "calendar_contacts", &window->calendar_contacts,
				       "vbox_contacts", &window->vbox_contacts,
				       "treeview_contacts", &window->treeview_contacts,
				       "scrolledwindow_contacts", &window->scrolledwindow_contacts,
				       "entry_chatrooms", &window->entry_chatrooms,
				       "calendar_chatrooms", &window->calendar_chatrooms,
				       "vbox_chatrooms", &window->vbox_chatrooms,
				       "treeview_chatrooms", &window->treeview_chatrooms,
				       "scrolledwindow_chatrooms", &window->scrolledwindow_chatrooms,
				       "entry_links_find", &window->entry_links_find,
				       "button_links_find", &window->button_links_find,
				       "button_links_open", &window->button_links_open,
				       "treeview_links", &window->treeview_links,
				       NULL);
	gossip_glade_connect (glade,
			      window,
			      "log_window", "destroy", log_window_destroy_cb,
			      "entry_find", "changed", log_window_entry_find_changed_cb,
			      "button_previous", "clicked", log_window_button_previous_clicked_cb,
			      "button_next", "clicked", log_window_button_next_clicked_cb,
			      "button_find", "clicked", log_window_button_find_clicked_cb,
			      "entry_contacts", "changed", log_window_entry_contacts_changed_cb,
			      "entry_contacts", "activate", log_window_entry_contacts_activate_cb,
			      "entry_chatrooms", "changed", log_window_entry_chatrooms_changed_cb,
			      "entry_chatrooms", "activate", log_window_entry_chatrooms_activate_cb,
			      "button_links_find", "clicked", log_window_button_links_find_clicked_cb,
			      "button_links_open", "clicked", log_window_button_links_open_clicked_cb,
			      NULL);

	g_object_unref (glade);

	g_object_add_weak_pointer (G_OBJECT (window->window),
				   (gpointer) &window);

	/* We set this up here so we can block it when needed. */
	g_signal_connect (window->calendar_contacts, "day-selected",
			  G_CALLBACK (log_window_calendar_contacts_day_selected_cb),
			  window);
	g_signal_connect (window->calendar_contacts, "month-changed",
			  G_CALLBACK (log_window_calendar_contacts_month_changed_cb),
			  window);

	g_signal_connect (window->calendar_chatrooms, "day-selected",
			  G_CALLBACK (log_window_calendar_chatrooms_day_selected_cb),
			  window);
	g_signal_connect (window->calendar_chatrooms, "month-changed",
			  G_CALLBACK (log_window_calendar_chatrooms_month_changed_cb),
			  window);

	/* Configure Search GossipChatView */
	window->chatview_find = gossip_chat_view_new ();
	gtk_container_add (GTK_CONTAINER (window->scrolledwindow_find),
			   GTK_WIDGET (window->chatview_find));
	gtk_widget_show (GTK_WIDGET (window->chatview_find));

	/* Configure Contacts GossipChatView */
	window->chatview_contacts = gossip_chat_view_new ();
	gtk_container_add (GTK_CONTAINER (window->scrolledwindow_contacts),
			   GTK_WIDGET (window->chatview_contacts));
	gtk_widget_show (GTK_WIDGET (window->chatview_contacts));

	/* Configure Chatrooms GossipChatView */
	window->chatview_chatrooms = gossip_chat_view_new ();
	gtk_container_add (GTK_CONTAINER (window->scrolledwindow_chatrooms),
			   GTK_WIDGET (window->chatview_chatrooms));
	gtk_widget_show (GTK_WIDGET (window->chatview_chatrooms));

	/* Account chooser for contacts & chat rooms */
	session = gossip_app_get_session ();

	window->account_chooser_contacts = gossip_account_chooser_new (session);
	account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser_contacts);
	gossip_account_chooser_set_can_select_all (account_chooser, TRUE);

	gtk_box_pack_start (GTK_BOX (window->vbox_contacts),
			    window->account_chooser_contacts,
			    FALSE, TRUE, 0);

	g_signal_connect (window->account_chooser_contacts, "changed",
			  G_CALLBACK (log_window_contacts_accounts_changed_cb),
			  window);

	window->account_chooser_chatrooms = gossip_account_chooser_new (session);
	account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser_chatrooms);
	gossip_account_chooser_set_can_select_all (account_chooser, TRUE);

	gtk_box_pack_start (GTK_BOX (window->vbox_chatrooms),
			    window->account_chooser_chatrooms,
			    FALSE, TRUE, 0);

	g_signal_connect (window->account_chooser_chatrooms, "changed",
			  G_CALLBACK (log_window_chatrooms_accounts_changed_cb),
			  window);
	/* Populate */
	accounts = gossip_session_get_accounts (session);
	account_num = g_list_length (accounts);

	g_list_foreach (accounts, (GFunc)g_object_unref, NULL);
	g_list_free (accounts);

	if (account_num > 1) {
		gtk_widget_show (window->vbox_contacts);
		gtk_widget_show (window->account_chooser_contacts);
		gtk_widget_show (window->vbox_chatrooms);
		gtk_widget_show (window->account_chooser_chatrooms);
	} else {
		gtk_widget_hide (window->vbox_contacts);
		gtk_widget_hide (window->account_chooser_contacts);
		gtk_widget_hide (window->vbox_chatrooms);
		gtk_widget_hide (window->account_chooser_chatrooms);
	}

	/* Search List */
	log_window_find_setup (window);

	/* Contacts */
	log_window_contacts_setup (window);
	log_window_contacts_populate (window);

	/* Chatrooms */
	log_window_chatrooms_setup (window);
	log_window_chatrooms_populate (window);

	/* Links List */
	log_window_links_setup (window);
	log_window_links_populate (window, NULL);

	/* Select contact or chatroom */
	if (contact) {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), 1);
		log_window_contacts_set_selected (window, contact);
	} else if (chatroom) {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), 2);
		log_window_chatrooms_set_selected (window, chatroom);
	} else {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), 0);
	}

	gtk_widget_show (window->window);
}
