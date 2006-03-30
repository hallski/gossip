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

#include <gtk/gtk.h> 

#include "gossip-app.h"
#include "gossip-account-chooser.h"
#include "gossip-log.h"
#include "gossip-log-window.h"
#include "gossip-ui-utils.h"

/* #define DEBUG_MSG(x)   */
#define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n");  

typedef struct {
	GtkWidget      *window;

	GtkWidget      *vbox_account;
	GtkWidget      *account_chooser;

	GtkWidget      *treeview;

	GtkWidget      *calendar;

	GtkWidget      *entry_find;

	GtkWidget      *scrolledwindow;
	GossipChatView *chatview;

	GtkWidget      *button_close;

	GossipLog      *log;
} GossipLogWindow;

static void           log_window_text_data_func           (GtkCellLayout     *cell_layout,
							   GtkCellRenderer   *cell,
							   GtkTreeModel      *tree_model,
							   GtkTreeIter       *iter,
							   GossipLogWindow   *window);
static void           log_window_pixbuf_data_func         (GtkCellLayout     *cell_layout,
							   GtkCellRenderer   *cell,
							   GtkTreeModel      *tree_model,
							   GtkTreeIter       *iter,
							   GossipLogWindow   *window);
static void           log_window_contacts_changed_cb      (GtkTreeSelection  *selection,
							   GossipLogWindow   *window);
static void           log_window_contacts_set_selected    (GossipLogWindow   *window,
							   GossipContact     *contact);
static GossipContact *log_window_contacts_get_selected    (GossipLogWindow   *window);
static void           log_window_contacts_populate        (GossipLogWindow   *window);
static void           log_window_contacts_setup           (GossipLogWindow   *window);
static void           log_window_accounts_changed_cb      (GtkWidget         *combobox,
							   GossipLogWindow   *window);
static void           log_window_new_message_cb           (GossipLog         *log,
							   GossipMessage     *message,
							   GossipLogWindow   *window);
static void           log_window_find                     (GossipLogWindow   *window,
							   gboolean           new_search);
static void           log_window_update_messages          (GossipLogWindow   *window,
							   const gchar       *date);
static void           log_window_calendar_day_selected_cb (GtkWidget         *calendar,
							   GossipLogWindow   *window);
static void           log_window_entry_find_changed_cb    (GtkWidget         *entry,
							   GossipLogWindow   *window);
static void           log_window_entry_find_activate_cb   (GtkWidget         *entry,
							   GossipLogWindow   *window);
static void           log_window_close_clicked_cb         (GtkWidget         *widget,
							   GossipLogWindow   *window);
static void           log_window_destroy_cb               (GtkWidget         *widget, 
							   GossipLogWindow   *window);

enum {
	COL_STATUS,
	COL_NAME, 
	COL_POINTER,
	COL_COUNT
};

static void
log_window_pixbuf_data_func (GtkCellLayout   *cell_layout,
			     GtkCellRenderer *cell,
			     GtkTreeModel    *tree_model,
			     GtkTreeIter     *iter,
			     GossipLogWindow *window)
{
	GossipContact *contact;
	GdkPixbuf     *pixbuf;

	gtk_tree_model_get (tree_model, iter, COL_POINTER, &contact, -1);

	pixbuf = gossip_pixbuf_for_contact (contact);

	g_object_set (cell, "pixbuf", pixbuf, NULL);
	g_object_set (cell, "visible", FALSE, NULL);
	g_object_unref (pixbuf);
	g_object_unref (contact);
}

static void
log_window_text_data_func (GtkCellLayout   *cell_layout,
			   GtkCellRenderer *cell,
			   GtkTreeModel    *tree_model,
			   GtkTreeIter     *iter,
			   GossipLogWindow *window)
{
	GossipContact *contact;

	gtk_tree_model_get (tree_model, iter, COL_POINTER, &contact, -1);

	g_object_set (cell, "text", gossip_contact_get_name (contact), NULL);
	g_object_unref (contact);
}

static void
log_window_contacts_changed_cb (GtkTreeSelection *selection,
				GossipLogWindow  *window)
{
	/* Use last date by default */
	gtk_calendar_clear_marks (GTK_CALENDAR (window->calendar));

	log_window_update_messages (window, NULL);
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

	view = GTK_TREE_VIEW (window->treeview);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		return;
	}
	
	for (ok = TRUE; ok; ok = gtk_tree_model_iter_next (model, &iter)) {
		gtk_tree_model_get (model, &iter, COL_POINTER, &this_contact, -1);

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

	view = GTK_TREE_VIEW (window->treeview);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter, COL_POINTER, &contact, -1);
	
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

	view = GTK_TREE_VIEW (window->treeview);
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
				    COL_NAME, gossip_contact_get_name (contact), 
				    COL_POINTER, contact,
				    -1);

/* 		if (l == contacts) { */
/* 			gtk_tree_selection_select_iter (selection, &iter); */
/* 		} */
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

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);

	/* new store */
	store = gtk_list_store_new (COL_COUNT,
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
						 log_window_pixbuf_data_func,
						 window, 
						 NULL);

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, cell, 
						 (GtkTreeCellDataFunc) 
						 log_window_text_data_func,
						 window, 
						 NULL);

	gtk_tree_view_append_column (view, column);

	/* set up treeview properties */
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	gtk_tree_sortable_set_sort_column_id (sortable, 
					      COL_NAME, 
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
	gossip_chat_view_clear (window->chatview);

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
		gossip_chat_view_append_message_from_self (window->chatview, 
							   message,
							   own_contact);
	} else {
		gossip_chat_view_append_message_from_other (window->chatview, 
							    message,
							    sender);
	}

	/* Scroll to the most recent messages */
	gossip_chat_view_scroll_down (window->chatview);	
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
			return;
		}
	
		/* Here we wrap around. */
		if (!new_search && !wrapped) {
			wrapped = TRUE;
			log_window_find (window, FALSE);
			wrapped = FALSE;
		}

		return;
	}
    
	/* set new mark and show on screen */
	find_mark = gtk_text_buffer_create_mark (buffer, NULL, &iter_match_end, TRUE); 
	gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (window->chatview), find_mark); 

	gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &iter_match_start);
	gtk_text_buffer_move_mark_by_name (buffer, "insert", &iter_match_end);	
}

static void
log_window_update_messages (GossipLogWindow *window, 
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
		dates = gossip_log_get_dates (window->log);

		for (l = dates; l; l = l->next) {
			guint  day;
			guint  day_p;
			gchar *str;
			
			str = l->data;
			if (!str) {
				continue;
			}
			
			DEBUG_MSG (("LogWindow: Marking date:'%s'", str));
			
			/* FIXME: There is an obvious bug here. What
			 * if the month is not the same then if we
			 * have a day for every day from any month
			 * then we highlight all.
			*/

			day_p = strlen (str) - 2;
			
			day = atoi (str + day_p);
			gtk_calendar_mark_day (GTK_CALENDAR (window->calendar), day);
			
			if (l->next) {
				continue;
			}

			date = str;
			
			g_signal_handlers_block_by_func (window->calendar, 
							 log_window_calendar_day_selected_cb, 
							 window);
			gtk_calendar_select_day (GTK_CALENDAR (window->calendar), day);
			g_signal_handlers_unblock_by_func (window->calendar, 
							   log_window_calendar_day_selected_cb, 
							   window);
		}
	}

	/* Clear all current messages shown in the textview */
	gossip_chat_view_clear (window->chatview);

	/* Get own contact to know which messages are from me or the contact */
	own_contact = gossip_log_get_own_contact (window->log); 

	/* Get messages */
	messages = gossip_log_get_messages (window->log, date);

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
								    sender);
		}
	}

	/* Scroll to the most recent messages */
	gossip_chat_view_scroll_down (window->chatview);
	
	/* Give the search entry main focus */
	gtk_widget_grab_focus (window->entry_find);

	/* Clean up */ 
	g_list_foreach (messages, (GFunc) g_object_unref, NULL);
	g_list_free (messages);

	g_list_foreach (dates, (GFunc) g_free, NULL);
	g_list_free (dates);
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

	log_window_update_messages (window, date);
	g_free (date);
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

static void
log_window_destroy_cb (GtkWidget       *widget, 
		       GossipLogWindow *window)
{
	if (window->log) {
		g_object_unref (window->log);
	}

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

        window = g_new0 (GossipLogWindow, 1);

	glade = gossip_glade_get_file (GLADEDIR "/main.glade",
				       "log_window",
				       NULL,
				       "log_window", &window->window,
				       "vbox_account", &window->vbox_account,
 				       "treeview", &window->treeview, 
				       "entry_find", &window->entry_find,
				       "scrolledwindow", &window->scrolledwindow,
 				       "calendar", &window->calendar, 
				       "button_close", &window->button_close,
				       NULL);
	
	gossip_glade_connect (glade, 
			      window,
			      "log_window", "destroy", log_window_destroy_cb,
			      "entry_find", "changed", log_window_entry_find_changed_cb,
			      "entry_find", "activate", log_window_entry_find_activate_cb,
			      "button_close", "clicked", log_window_close_clicked_cb,
			      NULL);

	/* We set this up here so we can block it when needed. */
	g_signal_connect (window->calendar, "day-selected", 
			  G_CALLBACK (log_window_calendar_day_selected_cb),
			  window);

	/* Configure GossipChatView */
	window->chatview = gossip_chat_view_new ();
	gtk_container_add (GTK_CONTAINER (window->scrolledwindow), GTK_WIDGET (window->chatview));
 	gtk_widget_show (GTK_WIDGET (window->chatview));

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
