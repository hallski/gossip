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
#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h> 

#include "gossip-app.h"
#include "gossip-account-chooser.h"
#include "gossip-log.h"
#include "gossip-log-window.h"
#include "gossip-ui-utils.h"

#define DEBUG_MSG(x)   
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n");   */

typedef struct {
	GtkWidget      *window;

	GtkWidget      *entry_search;
	GtkWidget      *button_search;
	GtkWidget      *treeview_search;
	GtkWidget      *scrolledwindow_search;
	GossipChatView *chatview_search;

	GtkWidget      *vbox_account;
	GtkWidget      *account_chooser;

	GtkWidget      *entry_browse;
	GtkWidget      *calendar;
	GtkWidget      *treeview_browse;
	GtkWidget      *scrolledwindow_browse;
	GossipChatView *chatview_browse;

	gchar          *last_search;
	
	GtkWidget      *button_close;

	GossipLog      *log;
} GossipLogWindow;

static void           log_window_search_text_data_func     (GtkCellLayout    *cell_layout,
							    GtkCellRenderer  *cell,
							    GtkTreeModel     *tree_model,
							    GtkTreeIter      *iter,
							    GossipLogWindow  *window);
static void           log_window_search_pixbuf_data_func   (GtkCellLayout    *cell_layout,
							    GtkCellRenderer  *cell,
							    GtkTreeModel     *tree_model,
							    GtkTreeIter      *iter,
							    GossipLogWindow  *window);
static void           log_window_entry_search_activate_cb  (GtkWidget        *entry,
							    GossipLogWindow  *window);
static void           log_window_search_changed_cb         (GtkTreeSelection *selection,
							    GossipLogWindow  *window);
static void           log_window_search_populate           (GossipLogWindow  *window,
							    const gchar      *search_criteria);
static void           log_window_search_setup              (GossipLogWindow  *window);
static void           log_window_contacts_text_data_func   (GtkCellLayout    *cell_layout,
							    GtkCellRenderer  *cell,
							    GtkTreeModel     *tree_model,
							    GtkTreeIter      *iter,
							    GossipLogWindow  *window);
static void           log_window_contacts_pixbuf_data_func (GtkCellLayout    *cell_layout,
							    GtkCellRenderer  *cell,
							    GtkTreeModel     *tree_model,
							    GtkTreeIter      *iter,
							    GossipLogWindow  *window);
static void           log_window_contacts_changed_cb       (GtkTreeSelection *selection,
							    GossipLogWindow  *window);
static void           log_window_contacts_set_selected     (GossipLogWindow  *window,
							    GossipContact    *contact);
static GossipContact *log_window_contacts_get_selected     (GossipLogWindow  *window);
static void           log_window_contacts_populate         (GossipLogWindow  *window);
static void           log_window_contacts_setup            (GossipLogWindow  *window);
static void           log_window_accounts_changed_cb       (GtkWidget        *combobox,
							    GossipLogWindow  *window);
static void           log_window_new_message_cb            (GossipLog        *log,
							    GossipMessage    *message,
							    GossipLogWindow  *window);
static void           log_window_contacts_get_messages     (GossipLogWindow  *window,
							    const gchar      *date);
static void           log_window_calendar_day_selected_cb  (GtkWidget        *calendar,
							    GossipLogWindow  *window);
static void           log_window_calendar_month_changed_cb (GtkWidget        *calendar,
							    GossipLogWindow  *window);
static void           log_window_entry_browse_changed_cb     (GtkWidget        *entry,
							    GossipLogWindow  *window);
static void           log_window_entry_browse_activate_cb    (GtkWidget        *entry,
							    GossipLogWindow  *window);
static void           log_window_button_search_clicked_cb  (GtkWidget        *widget,
							    GossipLogWindow  *window);
static void           log_window_button_close_clicked_cb   (GtkWidget        *widget,
							    GossipLogWindow  *window);
static void           log_window_destroy_cb                (GtkWidget        *widget,
							    GossipLogWindow  *window);

enum {
	COL_SEARCH_STATUS,
	COL_SEARCH_ACCOUNT,
	COL_SEARCH_CONTACT_ID, 
	COL_SEARCH_CONTACT_NAME,
	COL_SEARCH_DATE,
	COL_SEARCH_DATE_READABLE,
	COL_SEARCH_COUNT
};

enum {
	COL_BROWSE_STATUS,
	COL_BROWSE_NAME, 
	COL_BROWSE_POINTER,
	COL_BROWSE_COUNT
};

/*
 * Search code.
 */

static void
log_window_search_pixbuf_data_func (GtkCellLayout   *cell_layout,
				    GtkCellRenderer *cell,
				    GtkTreeModel    *tree_model,
				    GtkTreeIter     *iter,
				    GossipLogWindow *window)
{
	GossipAccount *account;
	GossipSession *session;
	GdkPixbuf     *pixbuf;
	gboolean       online;

	gtk_tree_model_get (tree_model, iter, COL_SEARCH_ACCOUNT, &account, -1);

	session = gossip_app_get_session ();
	online = gossip_session_is_connected (session, account);
	pixbuf = gossip_pixbuf_from_account_status (account, GTK_ICON_SIZE_MENU, online);

	g_object_set (cell, "pixbuf", pixbuf, NULL);
	g_object_set (cell, "visible", TRUE, NULL);

	g_object_unref (pixbuf);
	g_object_unref (account);
}

static void
log_window_search_text_data_func (GtkCellLayout   *cell_layout,
				  GtkCellRenderer *cell,
				  GtkTreeModel    *tree_model,
				  GtkTreeIter     *iter,
				  GossipLogWindow *window)
{
	GossipAccount *account;

	gtk_tree_model_get (tree_model, iter, COL_SEARCH_ACCOUNT, &account, -1);

	g_object_set (cell, "text", gossip_account_get_name (account), NULL);
	g_object_unref (account);
}

static void
log_window_entry_search_activate_cb (GtkWidget       *entry,
				     GossipLogWindow *window)
{
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (window->entry_search));

	g_free (window->last_search);
	window->last_search = g_strdup (str);

	log_window_search_populate (window, str);
}

static void
log_window_search_changed_cb (GtkTreeSelection *selection,
			      GossipLogWindow  *window)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeIter       iter;

	GossipAccount    *account;
	gchar            *contact_id;
	gchar            *date;

	GossipLog        *log;
	GossipContact    *own_contact;
	GossipContact    *sender;
	GossipMessage    *message;

	GList            *messages;
	GList            *l;

	/* Get selected information */
	view = GTK_TREE_VIEW (window->treeview_search);
	model = gtk_tree_view_get_model (view);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		gtk_widget_set_sensitive (window->button_search, FALSE);
		return;
	}

	gtk_tree_model_get (model, &iter, 
			    COL_SEARCH_ACCOUNT, &account,
			    COL_SEARCH_CONTACT_ID, &contact_id,
			    COL_SEARCH_DATE, &date, 
			    -1);

	log = gossip_log_get_by_contact_id (account, contact_id);
	g_free (contact_id);
	g_object_unref (account);

	if (!log) {
		g_free (date);
		return;
	}

	gtk_widget_set_sensitive (window->button_search, TRUE);

	/* Clear all current messages shown in the textview */
	gossip_chat_view_clear (window->chatview_search);

	/* Turn off scrolling temporarily */
	gossip_chat_view_scroll (window->chatview_search, FALSE);

	/* Get own contact to know which messages are from me or the contact */
	own_contact = gossip_log_get_own_contact (log); 

	/* Get messages */
	messages = gossip_log_get_messages (log, date);
	g_free (date);

	for (l = messages; l; l = l->next) {
		message = l->data;

		sender = gossip_message_get_sender (message);

		if (gossip_contact_equal (own_contact, sender)) {
			gossip_chat_view_append_message_from_self (window->chatview_search, 
								   message,
								   own_contact);
		} else {
			gossip_chat_view_append_message_from_other (window->chatview_search, 
								    message,
								    sender);
		}
	}

	g_list_foreach (messages, (GFunc) g_object_unref, NULL);
	g_list_free (messages);

	g_object_unref (log);

	/* Scroll to the most recent messages */
	gossip_chat_view_scroll (window->chatview_search, TRUE);

	/* Highlight and find messages */
	gossip_chat_view_highlight (window->chatview_search,
				    window->last_search);
	gossip_chat_view_find (window->chatview_search, 
			       window->last_search,
			       TRUE);
}

static void
log_window_search_populate (GossipLogWindow *window,
			    const gchar     *search_criteria)
{
	GossipSession        *session;
	GossipAccountManager *manager;
	const gchar          *account_id;
	GossipAccount        *account;

	GList                *hits;
	GList                *l;
	GossipLogSearchHit   *hit;

	GtkTreeView          *view;
	GtkTreeModel         *model;
	GtkTreeSelection     *selection;
	GtkListStore         *store;
	GtkTreeIter           iter;

	view = GTK_TREE_VIEW (window->treeview_search);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);
	store = GTK_LIST_STORE (model);

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	gossip_chat_view_clear (window->chatview_search);

	gtk_list_store_clear (store);

	hits = gossip_log_search_new (search_criteria);

	for (l = hits; l; l = l->next) {
		const gchar *date;
		gchar       *date_readable;

		hit = l->data;

		account_id = gossip_log_search_hit_get_account_id (hit);
		account = gossip_account_manager_find_by_id (manager, account_id);

		date = gossip_log_search_hit_get_date (hit);
		date_readable = gossip_log_get_date_readable (date);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 
				    COL_SEARCH_ACCOUNT, account,
				    COL_SEARCH_CONTACT_ID, gossip_log_search_hit_get_contact_id (hit),
				    COL_SEARCH_CONTACT_NAME, gossip_log_search_hit_get_contact_name (hit),
				    COL_SEARCH_DATE, date,
				    COL_SEARCH_DATE_READABLE, date_readable,
				    -1);

		g_free (date_readable);
	}

	if (hits) {
		gossip_log_search_free (hits);
	}
}

static void
log_window_search_setup (GossipLogWindow *window)
{
	GtkTreeView       *view;
	GtkTreeModel      *model;
	GtkTreeSelection  *selection;
	GtkTreeSortable   *sortable;
	GtkTreeViewColumn *column;
	GtkListStore      *store;
	GtkCellRenderer   *cell;
	gint               offset;

	view = GTK_TREE_VIEW (window->treeview_search);
	selection = gtk_tree_view_get_selection (view);

	/* New store */
	store = gtk_list_store_new (COL_SEARCH_COUNT,
				    GDK_TYPE_PIXBUF,        /* account status */
				    GOSSIP_TYPE_ACCOUNT,    /* account */
				    G_TYPE_STRING,          /* contact_id */
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
						 log_window_search_pixbuf_data_func,
						 window, 
						 NULL);

	cell = gtk_cell_renderer_text_new ();
/* 	g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL); */
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, cell, 
						 (GtkTreeCellDataFunc) 
						 log_window_search_text_data_func,
						 window, 
						 NULL);

	gtk_tree_view_column_set_title (column, _("Account"));
	gtk_tree_view_append_column (view, column);

	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_clickable (column, TRUE);

	cell = gtk_cell_renderer_text_new ();
	offset = gtk_tree_view_insert_column_with_attributes (view, -1, _("Conversation With"),
							      cell, "text", COL_SEARCH_CONTACT_NAME,
							      NULL);

	column = gtk_tree_view_get_column (view, offset - 1);
	gtk_tree_view_column_set_sort_column_id (column, COL_SEARCH_CONTACT_NAME);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_clickable (column, TRUE);

	cell = gtk_cell_renderer_text_new ();
	offset = gtk_tree_view_insert_column_with_attributes (view, -1, _("Date"),
							      cell, "text", COL_SEARCH_DATE_READABLE,
							      NULL);

	column = gtk_tree_view_get_column (view, offset - 1);
	gtk_tree_view_column_set_sort_column_id (column, COL_SEARCH_DATE);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_clickable (column, TRUE);

	/* Set up treeview properties */
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	gtk_tree_sortable_set_sort_column_id (sortable, 
					      COL_SEARCH_DATE, 
					      GTK_SORT_ASCENDING);

	/* set up signals */
	g_signal_connect (selection, "changed", 
			  G_CALLBACK (log_window_search_changed_cb), 
			  window);

	g_object_unref (store);
}

/*
 * Browse Code
 */
static void
log_window_contacts_pixbuf_data_func (GtkCellLayout   *cell_layout,
				      GtkCellRenderer *cell,
				      GtkTreeModel    *tree_model,
				      GtkTreeIter     *iter,
				      GossipLogWindow *window)
{
	GossipContact *contact;
	GdkPixbuf     *pixbuf;

	gtk_tree_model_get (tree_model, iter, COL_BROWSE_POINTER, &contact, -1);

	pixbuf = gossip_pixbuf_for_contact (contact);

	g_object_set (cell, "pixbuf", pixbuf, NULL);
	g_object_set (cell, "visible", FALSE, NULL);
	g_object_unref (pixbuf);
	g_object_unref (contact);
}

static void
log_window_contacts_text_data_func (GtkCellLayout   *cell_layout,
				    GtkCellRenderer *cell,
				    GtkTreeModel    *tree_model,
				    GtkTreeIter     *iter,
				    GossipLogWindow *window)
{
	GossipContact *contact;

	gtk_tree_model_get (tree_model, iter, COL_BROWSE_POINTER, &contact, -1);

	g_object_set (cell, "text", gossip_contact_get_name (contact), NULL);
	g_object_unref (contact);
}

static void
log_window_contacts_changed_cb (GtkTreeSelection *selection,
				GossipLogWindow  *window)
{
	/* Use last date by default */
	gtk_calendar_clear_marks (GTK_CALENDAR (window->calendar));

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
	GossipContact    *this_contact;
	gboolean          ok;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	view = GTK_TREE_VIEW (window->treeview_browse);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		return;
	}
	
	for (ok = TRUE; ok; ok = gtk_tree_model_iter_next (model, &iter)) {
		gtk_tree_model_get (model, &iter, COL_BROWSE_POINTER, &this_contact, -1);

		if (!this_contact) {
			continue;
		}

		if (gossip_contact_equal (contact, this_contact)) {
			gtk_tree_selection_select_iter (selection, &iter);
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

	view = GTK_TREE_VIEW (window->treeview_browse);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter, COL_BROWSE_POINTER, &contact, -1);
	
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
	
	account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser);
	account = gossip_account_chooser_get_account (account_chooser);

	view = GTK_TREE_VIEW (window->treeview_browse);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);
	store = GTK_LIST_STORE (model);

	/* Block signals to stop the logs being retrieved prematurely */
	g_signal_handlers_block_by_func (selection, 
					 log_window_contacts_changed_cb, 
					 window);

	gtk_list_store_clear (store);

	contacts = gossip_log_get_contacts (account);

	for (l = contacts; l; l = l->next) {
		contact = l->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 
				    COL_BROWSE_NAME, gossip_contact_get_name (contact), 
				    COL_BROWSE_POINTER, contact,
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

	view = GTK_TREE_VIEW (window->treeview_browse);
	selection = gtk_tree_view_get_selection (view);

	/* new store */
	store = gtk_list_store_new (COL_BROWSE_COUNT,
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
					      COL_BROWSE_NAME, 
					      GTK_SORT_ASCENDING);

	/* set up signals */
	g_signal_connect (selection, "changed", 
			  G_CALLBACK (log_window_contacts_changed_cb), 
			  window);

	g_object_unref (store);
}

static void
log_window_accounts_changed_cb (GtkWidget       *combobox,
				GossipLogWindow *window)
{
	/* Clear all current messages shown in the textview */
	gossip_chat_view_clear (window->chatview_browse);

	log_window_contacts_populate (window);
}

static void
log_window_new_message_cb (GossipLog       *log,
			   GossipMessage   *message,
			   GossipLogWindow *window)
{
	GossipContact *own_contact;
	GossipContact *sender;

	/* Get own contact to know which messages are from me or the contact */
	own_contact = gossip_log_get_own_contact (window->log); 

	sender = gossip_message_get_sender (message);
	
	if (gossip_contact_equal (own_contact, sender)) {
		gossip_chat_view_append_message_from_self (window->chatview_browse, 
							   message,
							   own_contact);
	} else {
		gossip_chat_view_append_message_from_other (window->chatview_browse, 
							    message,
							    sender);
	}

	/* Scroll to the most recent messages */
	gossip_chat_view_scroll_down (window->chatview_browse);	
}

static void
log_window_contacts_get_messages (GossipLogWindow *window, 
				  const gchar     *date_to_show)
{
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

	/* Get the log object for this contact */
	if (window->log) {
		g_signal_handlers_disconnect_by_func (window->log, 
						      log_window_new_message_cb,
						      window);
		g_object_unref (window->log);
	}

	window->log = gossip_log_get (contact);

	g_return_if_fail (GOSSIP_IS_LOG (window->log));

	g_signal_connect (window->log, 
			  "new-message",
			  G_CALLBACK (log_window_new_message_cb),
			  window);

	/* Either use the supplied date or get the last */
	date = date_to_show;
	if (!date) {
		gboolean day_selected = FALSE;

		/* Get a list of dates and show them on the calendar */
		dates = gossip_log_get_dates (window->log);

		for (l = dates; l; l = l->next) {
			const gchar *str;
			
			str = l->data;
			if (!str) {
				continue;
			}
				
			sscanf (str, "%4d%2d%2d", &year, &month, &day);
			gtk_calendar_get_date (GTK_CALENDAR (window->calendar), 
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


			DEBUG_MSG (("LogWindow: Marking date:'%s'", str));
			gtk_calendar_mark_day (GTK_CALENDAR (window->calendar), day);
				
			if (l->next) {
				continue;
			}

			day_selected = TRUE;

			g_signal_handlers_block_by_func (window->calendar, 
							 log_window_calendar_day_selected_cb, 
							 window);
			gtk_calendar_select_day (GTK_CALENDAR (window->calendar), day);
			g_signal_handlers_unblock_by_func (window->calendar, 
							   log_window_calendar_day_selected_cb, 
							   window);
		}

		if (!day_selected) {
			/* Unselect the day in the calendar */
			g_signal_handlers_block_by_func (window->calendar, 
							 log_window_calendar_day_selected_cb, 
							 window);
			gtk_calendar_select_day (GTK_CALENDAR (window->calendar), 0);
			g_signal_handlers_unblock_by_func (window->calendar, 
							   log_window_calendar_day_selected_cb, 
							   window);
		}
	} else {
		sscanf (date, "%4d%2d%2d", &year, &month, &day);
		gtk_calendar_get_date (GTK_CALENDAR (window->calendar), 
				       &year_selected, 
				       &month_selected,
				       NULL);

		month_selected++;

		if (year != year_selected && month != month_selected) {
			day = 0;
		} 

		g_signal_handlers_block_by_func (window->calendar, 
						 log_window_calendar_day_selected_cb, 
						 window);

		gtk_calendar_select_day (GTK_CALENDAR (window->calendar), day);

		g_signal_handlers_unblock_by_func (window->calendar, 
						   log_window_calendar_day_selected_cb, 
						   window);
	}

	/* Clear all current messages shown in the textview */
	gossip_chat_view_clear (window->chatview_browse);

	/* Turn off scrolling temporarily */
	gossip_chat_view_scroll (window->chatview_search, FALSE);

	/* Get own contact to know which messages are from me or the contact */
	own_contact = gossip_log_get_own_contact (window->log); 

	/* Get messages */
	messages = gossip_log_get_messages (window->log, date);

	for (l = messages; l; l = l->next) {
		message = l->data;

		sender = gossip_message_get_sender (message);

		if (gossip_contact_equal (own_contact, sender)) {
			gossip_chat_view_append_message_from_self (window->chatview_browse, 
								   message,
								   own_contact);
		} else {
			gossip_chat_view_append_message_from_other (window->chatview_browse, 
								    message,
								    sender);
		}
	}

 	g_list_foreach (messages, (GFunc) g_object_unref, NULL);
	g_list_free (messages);

	g_list_foreach (dates, (GFunc) g_free, NULL);
	g_list_free (dates);

	/* Turn off scrolling temporarily */
	gossip_chat_view_scroll (window->chatview_search, TRUE);

	/* Scroll to the most recent messages */
	gossip_chat_view_scroll_down (window->chatview_browse);
	
	/* Give the search entry main focus */
	gtk_widget_grab_focus (window->entry_browse);
}

static void
log_window_calendar_day_selected_cb (GtkWidget       *calendar,
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

	DEBUG_MSG (("LogWindow: Currently selected date is:'%s'", date));

	log_window_contacts_get_messages (window, date);

	g_free (date);
}

static void
log_window_calendar_month_changed_cb (GtkWidget       *calendar,
				      GossipLogWindow *window)
{
	guint  year_selected;
	guint  month_selected;

	GList *dates;
	GList *l;

	gtk_calendar_clear_marks (GTK_CALENDAR (calendar));

	if (!window->log) {
		DEBUG_MSG (("LogWindow: No contact's log available to get dates for..."));
		return;
	}

	g_object_get (calendar, 
		      "month", &month_selected,
		      "year", &year_selected,
		      NULL);

	/* We need this hear because it appears that the months start from 0 */
	month_selected++;

	/* Get the log object for this contact */
	dates = gossip_log_get_dates (window->log);
	
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
			DEBUG_MSG (("LogWindow: Marking date:'%s'", str));
			gtk_calendar_mark_day (GTK_CALENDAR (window->calendar), day);
		}
	}

	g_list_foreach (dates, (GFunc) g_free, NULL);
	g_list_free (dates);

	DEBUG_MSG (("LogWindow: Currently showing month %d and year %d", 
		    month_selected, year_selected));
}

static void
log_window_entry_browse_changed_cb (GtkWidget       *entry,
				  GossipLogWindow *window)
{
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (window->entry_browse));
	gossip_chat_view_highlight (window->chatview_browse, str);

	if (str) {
		gossip_chat_view_find (window->chatview_browse, 
				       str,
				       TRUE);
	}
}

static void
log_window_entry_browse_activate_cb (GtkWidget       *entry,
				   GossipLogWindow *window)
{
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (window->entry_browse));

	if (str) {
		gossip_chat_view_find (window->chatview_browse, 
				       str,
				       FALSE);
	}
}

static void
log_window_button_search_clicked_cb (GtkWidget       *widget,
				     GossipLogWindow *window)
{
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (window->entry_search));

	if (str) {
		gossip_chat_view_find (window->chatview_search, 
				       str,
				       FALSE);
	}
}

static void
log_window_button_close_clicked_cb (GtkWidget       *widget,
				    GossipLogWindow *window)
{
	gtk_widget_destroy (window->window);
}

static void
log_window_destroy_cb (GtkWidget       *widget, 
		       GossipLogWindow *window)
{
	if (window->log) {
		g_object_unref (window->log);
	}

	g_free (window->last_search);

 	g_free (window); 
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
	GtkWidget            *notebook;

        window = g_new0 (GossipLogWindow, 1);

	glade = gossip_glade_get_file (GLADEDIR "/main.glade",
				       "log_window",
				       NULL,
				       "log_window", &window->window,
				       "notebook", &notebook,
				       "entry_search", &window->entry_search,
				       "button_search", &window->button_search,
 				       "treeview_search", &window->treeview_search, 
				       "scrolledwindow_search", &window->scrolledwindow_search,
				       "entry_browse", &window->entry_browse,
 				       "calendar", &window->calendar, 
				       "vbox_account", &window->vbox_account,
 				       "treeview_browse", &window->treeview_browse, 
				       "scrolledwindow_browse", &window->scrolledwindow_browse,
				       "button_close", &window->button_close,
				       NULL);
	
	gossip_glade_connect (glade, 
			      window,
			      "log_window", "destroy", log_window_destroy_cb,
			      "entry_search", "activate", log_window_entry_search_activate_cb,
			      "button_search", "clicked", log_window_button_search_clicked_cb,
			      "entry_browse", "changed", log_window_entry_browse_changed_cb,
			      "entry_browse", "activate", log_window_entry_browse_activate_cb,
			      "button_close", "clicked", log_window_button_close_clicked_cb,
			      NULL);

	/* We set this up here so we can block it when needed. */
	g_signal_connect (window->calendar, "day-selected", 
			  G_CALLBACK (log_window_calendar_day_selected_cb),
			  window);
	g_signal_connect (window->calendar, "month-changed", 
			  G_CALLBACK (log_window_calendar_month_changed_cb),
			  window);

	/* Configure Search GossipChatView */
	window->chatview_search = gossip_chat_view_new ();
	gtk_container_add (GTK_CONTAINER (window->scrolledwindow_search), 
			   GTK_WIDGET (window->chatview_search));
 	gtk_widget_show (GTK_WIDGET (window->chatview_search));

	/* Configure Browse GossipChatView */
	window->chatview_browse = gossip_chat_view_new ();
	gtk_container_add (GTK_CONTAINER (window->scrolledwindow_browse), 
			   GTK_WIDGET (window->chatview_browse));
 	gtk_widget_show (GTK_WIDGET (window->chatview_browse));

	/* Account chooser for chat rooms */
	session = gossip_app_get_session ();

	window->account_chooser = gossip_account_chooser_new (session);
	account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->account_chooser);
	gossip_account_chooser_set_can_select_all (account_chooser, TRUE);

	gtk_box_pack_start (GTK_BOX (window->vbox_account), 
			    window->account_chooser,
			    FALSE, TRUE, 0);
			  
	g_signal_connect (window->account_chooser, "changed",
			  G_CALLBACK (log_window_accounts_changed_cb),
			  window);

	/* Populate */
	accounts = gossip_session_get_accounts (session);
	account_num = g_list_length (accounts);

	g_list_foreach (accounts, (GFunc)g_object_unref, NULL);
	g_list_free (accounts);

	if (account_num > 1) {
		gtk_widget_show (window->vbox_account);
		gtk_widget_show (window->account_chooser);
	} else {
		gtk_widget_hide (window->vbox_account);
		gtk_widget_hide (window->account_chooser);
	}

	/* Search List */
	log_window_search_setup (window);

	/* Contacts */
	log_window_contacts_setup (window);
	log_window_contacts_populate (window);

	/* Select contact */
	if (contact) {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 1);
		log_window_contacts_set_selected (window, contact);
	}

	/* Last touches */
	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (window->window), 
					      GTK_WINDOW (parent));
	}

	gtk_widget_show (window->window);
}
