/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Martyn Russell <mr@gnome.org>
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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <loudmouth/loudmouth.h>
#include "gossip-app.h"
#include "gossip-stock.h"
#include "gossip-add-contact.h"

#define d(x)


struct _GossipTransportAccountsWindow {
	GtkWidget *window;

	GtkWidget *treeview_accounts;

	GtkWidget *button_add;
	GtkWidget *button_remove;

	GtkWidget *button_close;
};

enum {
	COL_TRANSPORT_ACC_DATA,
	COL_TRANSPORT_ACC_COUNT
};


static void     transport_accounts_window_roster_update_cb       (GossipSession                 *session,
								  GossipContact                 *contact,
								  GossipTransportAccountsWindow *window);
static void     transport_accounts_window_account_update_cb      (GossipTransportAccount        *account,
								  GossipTransportAccountsWindow *window);
static void     transport_accounts_window_update                 (GossipTransportAccountsWindow *window);
static void     transport_accounts_window_setup                  (GossipTransportAccountsWindow *window);
static void     transport_accounts_window_populate_columns       (GossipTransportAccountsWindow *window);
static gboolean transport_accounts_window_model_foreach_cb       (GtkTreeModel                  *model,
								  GtkTreePath                   *path,
								  GtkTreeIter                   *iter,
								  gpointer                       data);
static void     transport_accounts_window_selection_changed_cb   (GtkTreeSelection              *treeselection,
								  GossipTransportAccountsWindow *window);
static void     transport_accounts_window_pixbuf_cell_data_func  (GtkTreeViewColumn             *tree_column,
								  GtkCellRenderer               *cell,
								  GtkTreeModel                  *model,
								  GtkTreeIter                   *iter,
								  GossipTransportAccountsWindow *window);
static void     transport_accounts_window_name_cell_data_func    (GtkTreeViewColumn             *tree_column,
								  GtkCellRenderer               *cell,
								  GtkTreeModel                  *model,
								  GtkTreeIter                   *iter,
								  GossipTransportAccountsWindow *window);
static void     transport_accounts_window_button_add_clicked     (GtkButton                     *button,
								  GossipTransportAccountsWindow *window);
static void     transport_accounts_window_button_remove_clicked  (GtkButton                     *button,
								  GossipTransportAccountsWindow *window);
static void     transport_accounts_window_button_close_clicked   (GtkButton                     *button,
								  GossipTransportAccountsWindow *window);
static void     transport_accounts_window_destroy                (GtkWidget                     *widget,
								  GossipTransportAccountsWindow *window);



static GossipTransportAccountsWindow *current_window = NULL;


GossipTransportAccountsWindow *
gossip_transport_accounts_window_show (void)
{
	GossipTransportAccountsWindow *window;
	GossipTransportAccountList    *al;
	GossipJabber                  *jabber;
	GList                         *l;
	GladeXML                      *gui;

	if (current_window) {
		gtk_window_present (GTK_WINDOW (current_window->window));
		return current_window;
	}
	
	/* FIXME: need to do this better, plus need a gui to select */
	l = gossip_transport_account_lists_get_all ();
	al = g_list_nth_data (l, 0);

	g_return_val_if_fail (al != NULL, NULL);

	jabber = gossip_transport_account_list_get_jabber (al);

	/* set up window */
	current_window = window = g_new0 (GossipTransportAccountsWindow, 1);

	gui = gossip_glade_get_file (GLADEDIR "/transports.glade",
				     "transport_accounts_window",
				     NULL,
				     "transport_accounts_window", &window->window,
				     "treeview_accounts", &window->treeview_accounts,
				     "button_add", &window->button_add,
				     "button_remove", &window->button_remove,
				     "button_close", &window->button_close,
				     NULL);

	gossip_glade_connect (gui,
			      window,
			      "transport_accounts_window", "destroy", transport_accounts_window_destroy,
			      "button_add", "clicked", transport_accounts_window_button_add_clicked,
			      "button_remove", "clicked", transport_accounts_window_button_remove_clicked,
			      "button_close", "clicked", transport_accounts_window_button_close_clicked,
			      NULL);

	g_object_unref (gui);

	/* set up model */
	transport_accounts_window_setup (window);

	g_signal_connect (GOSSIP_PROTOCOL (jabber),
			  "contact-added",
			  G_CALLBACK (transport_accounts_window_roster_update_cb),
			  window);
	g_signal_connect (GOSSIP_PROTOCOL (jabber),
			  "contact-removed",
			  G_CALLBACK (transport_accounts_window_roster_update_cb),
			  window);

	return window;
}

static void
transport_accounts_window_roster_update_cb (GossipSession                 *session,
					    GossipContact                 *contact,
					    GossipTransportAccountsWindow *window)
{
	/* from the transport add wizard, this is called to refresh
	   the list of transports, we can not listen for the message
	   because it is removed by the transport add wizard to make
	   sure the roster/app modules don't get the message and
	   present a dialog */

	transport_accounts_window_update (window);
}

static void
transport_accounts_window_account_update_cb (GossipTransportAccount        *account,
					     GossipTransportAccountsWindow *window)
{
	GtkTreeView  *view;
	GtkTreeModel *model;
	GtkTreeIter   iter;
	gboolean      valid;

	if (FALSE) { 
		transport_accounts_window_account_update_cb (NULL, NULL);
	}

	view = GTK_TREE_VIEW (window->treeview_accounts);
	model = gtk_tree_view_get_model (view);

	for (valid = gtk_tree_model_get_iter_first (model, &iter);
	     valid;
	     valid = gtk_tree_model_iter_next (model, &iter)) {
		gtk_tree_model_get (model, &iter, 0, &account, -1);
		
		if (account == account) {
			GtkTreePath *path;

			path = gtk_tree_model_get_path (model, &iter);
			gtk_tree_model_row_changed (model, path, &iter);
			gtk_tree_path_free (path);
		}
	}
}

static void 
transport_accounts_window_update (GossipTransportAccountsWindow *window)
{
	GtkTreeView  *view;
	GtkTreeModel *model;
	GtkListStore *store;

	GList        *accounts;
	GList        *l;

	view = GTK_TREE_VIEW (window->treeview_accounts);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);

	/* clear store first (for updates) */
	gtk_list_store_clear (store);

	/* unref each account */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (window->treeview_accounts));
	gtk_tree_model_foreach (model, transport_accounts_window_model_foreach_cb, NULL);

	/* get updated accounts list */
	accounts = gossip_transport_accounts_get_all ();

	/* populate accounts */
	for (l=accounts; l; l=l->next) {
		GossipTransportAccount *account;
		GtkTreeIter             iter;

		account = l->data;

		/* reference so we don't loose it */
		gossip_transport_account_ref (account);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, COL_TRANSPORT_ACC_DATA, account, -1);
	}

	g_list_free (accounts);
}

static void
transport_accounts_window_setup (GossipTransportAccountsWindow *window)
{
	GtkListStore     *store;
	GtkTreeSelection *selection;

	store = gtk_list_store_new (COL_TRANSPORT_ACC_COUNT,
				    GOSSIP_TYPE_ACCOUNT,  /* object */
				    G_TYPE_BOOLEAN);      /* editable */
	
	gtk_tree_view_set_model (GTK_TREE_VIEW (window->treeview_accounts), 
				 GTK_TREE_MODEL (store));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview_accounts));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect (G_OBJECT (selection), "changed", 
			  G_CALLBACK (transport_accounts_window_selection_changed_cb), window);


	transport_accounts_window_populate_columns (window);
	transport_accounts_window_update (window);

	g_object_unref (store);
}

static void 
transport_accounts_window_populate_columns (GossipTransportAccountsWindow *window)
{
	GtkTreeViewColumn *column; 
	GtkCellRenderer   *cell;
	
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (window->treeview_accounts), FALSE);

	column = gtk_tree_view_column_new ();
	
	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, cell, 
						 (GtkTreeCellDataFunc) transport_accounts_window_pixbuf_cell_data_func,
						 window, 
						 NULL);

	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc) transport_accounts_window_name_cell_data_func,
						 window,
						 NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (window->treeview_accounts), column);
}

static void  
transport_accounts_window_pixbuf_cell_data_func (GtkTreeViewColumn             *tree_column,
						 GtkCellRenderer               *cell,
						 GtkTreeModel                  *model,
						 GtkTreeIter                   *iter,
						 GossipTransportAccountsWindow *window)
{
	GossipTransportAccount *account;
	const gchar            *disco_type;
	const gchar            *icon;
	const gchar            *stock_id = NULL;
	const gchar            *core_icon = NULL;
	GdkPixbuf              *pixbuf = NULL;
	gint                    w, h;
	gint                    size = 48;  /* default size */

	gtk_tree_model_get (model, iter, COL_TRANSPORT_ACC_DATA, &account, -1);

	g_return_if_fail (account != NULL);

	disco_type = gossip_transport_account_get_disco_type (account);
	
	/* we should get these from the GossipDiscoProtocol API */
	icon = NULL;

	/* these can not be overridden */
	if (strcmp (disco_type, "aim") == 0) {
		core_icon = "im-aim";
	} else if (strcmp (disco_type, "icq") == 0) {
		core_icon = "im-icq";
	} else if (strcmp (disco_type, "msn") == 0) {
		core_icon = "im-msn";
	} else if (strcmp (disco_type, "yahoo") == 0) {
		core_icon = "im-yahoo";
	}

	if (core_icon || icon) {
		if (!gtk_icon_size_lookup (GTK_ICON_SIZE_DND, &w, &h)) {
			size = 48;
		} else {
			/* we average the two, this way if the height
			   and width are not equal, they meet in the middle */
			size = (w + h) / 2; 
		}
	}

	if (core_icon) {
		GError       *error = NULL;
		GtkIconTheme *theme;
				
		theme = gtk_icon_theme_get_default ();
		pixbuf = gtk_icon_theme_load_icon (theme,
						   core_icon, /* icon name */
						   size,      /* size */
						   0,         /* flags */
						   &error);
		if (!pixbuf) {
			g_warning ("could not load icon: %s", error->message);
			g_error_free (error);

			g_object_set (cell, 
				      "visible", TRUE,
				      "stock_id", NULL,
				      "pixbuf", NULL,
				      NULL); 

			return;
		}
	} else if (stock_id) {
		g_object_set (cell, 
			      "visible", TRUE,
			      "stock-id", stock_id,
			      "stock-size", GTK_ICON_SIZE_DND, 
			      "pixbuf", NULL,
			      NULL); 

		return;
	} else if (icon) {
		GError *error = NULL;

		pixbuf = gdk_pixbuf_new_from_file_at_size (icon, w, h, &error);

		if (!pixbuf) {
			g_warning ("could not load icon: %s", error->message);
			g_error_free (error);

			g_object_set (cell, 
				      "visible", TRUE,
				      "stock_id", NULL,
				      "pixbuf", NULL,
				      NULL); 

			return;
		}
	}

	g_object_set (cell, 
		      "visible", TRUE,
		      "stock_id", NULL,
		      "pixbuf", pixbuf,
		      NULL); 
	
	g_object_unref (pixbuf); 
}

static void  
transport_accounts_window_name_cell_data_func (GtkTreeViewColumn             *tree_column,
					       GtkCellRenderer               *cell,
					       GtkTreeModel                  *model,
					       GtkTreeIter                   *iter,
					       GossipTransportAccountsWindow *window)
{
	GossipTransportAccount *account;
	GossipJID              *jid;
	gchar                  *str;

	GtkTreeSelection       *selection;
	PangoAttrList          *attr_list;
	PangoAttribute         *attr_color, *attr_style, *attr_size;
	GtkStyle               *style;
	GdkColor                color;

	const gchar            *name = NULL;
	const gchar            *description = NULL;
	const gchar            *disco_type;
	const gchar            *username;
	gchar                  *contacts = NULL;
	gint                    count;

	gint                    pango_index;
	gchar                  *line;

	gtk_tree_model_get (model, iter, COL_TRANSPORT_ACC_DATA, &account, -1);

	g_return_if_fail (account != NULL);

	jid = gossip_transport_account_get_jid (account);
	disco_type = gossip_transport_account_get_disco_type (account);
	username = gossip_transport_account_get_username (account);
	count = gossip_transport_account_count_contacts (account);
	
	if (count == 1) {
		contacts = g_strdup_printf (_("1 contact"));
	} else if (count < 1) {
		contacts = g_strdup_printf (_("No contacts"));
	} else if (count > 1) {
		contacts = g_strdup_printf (_("%d contacts"), count);
	}

	/* should get this from the protocol API */
	if (strcmp (disco_type, "aim") == 0) {
		name = "AIM";
	} else if (strcmp (disco_type, "icq") == 0) {
		name = "ICQ";
	} else if (strcmp (disco_type, "msn") == 0) {
		name = "MSN";
	} else if (strcmp (disco_type, "yahoo") == 0) {
		name = "Yahoo!";
	}

	if (!name) {
		name = gossip_jid_get_full (jid);
	} else {
		/* we should really use user name, 
		   e.g. someone@hotmail.com */
/* 		description = gossip_jid_get_full (jid); */
	}

	/* if we do not have a user name yet, use the disco type */
	if (!username) {
		username = name;
	}

	str = g_strdup_printf ("%s\n%s%s%s", 
			       username ? username : "",
			       description ? description : "",
			       description ? "\n" : "",
			       contacts);

	g_free (contacts);


	line = strchr (str, '\n');
	pango_index = line - str;

	style = gtk_widget_get_style (window->treeview_accounts);
	color = style->text_aa[GTK_STATE_NORMAL];

	attr_list = pango_attr_list_new ();

	attr_style = pango_attr_style_new (PANGO_STYLE_ITALIC);
	attr_style->start_index = pango_index;
/* 	attr_style->start_index = strlen (name) + 1; */
	attr_style->end_index = -1;
	pango_attr_list_insert (attr_list, attr_style);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview_accounts));

	if (!gtk_tree_selection_iter_is_selected (selection, iter)) {
		attr_color = pango_attr_foreground_new (color.red, color.green, color.blue);
		attr_color->start_index = attr_style->start_index;
		attr_color->end_index = -1;
		pango_attr_list_insert (attr_list, attr_color);
	}

	attr_size = pango_attr_size_new (pango_font_description_get_size (style->font_desc) / 1.2);
	attr_size->start_index = attr_style->start_index;
	attr_size->end_index = -1;
	pango_attr_list_insert (attr_list, attr_size);
	
	g_object_set (cell,
		      "visible", TRUE,
		      "weight", PANGO_WEIGHT_NORMAL,
		      "text", str,
		      "attributes", attr_list,
		      NULL);

	pango_attr_list_unref (attr_list);

	g_free (str);
}

static void
transport_accounts_window_selection_changed_cb (GtkTreeSelection              *treeselection,
						GossipTransportAccountsWindow *window)
{
	gboolean remove = FALSE;

	if (gtk_tree_selection_count_selected_rows (treeselection) == 1) {
		remove = TRUE;
	}

	gtk_widget_set_sensitive (window->button_remove, remove);
}

static gboolean
transport_accounts_window_model_foreach_cb (GtkTreeModel *model,
					    GtkTreePath  *path,
					    GtkTreeIter  *iter,
					    gpointer      data)
{
	GossipTransportAccount *account;

	gtk_tree_model_get (model, iter, COL_TRANSPORT_ACC_DATA, &account, -1);
	gossip_transport_account_unref (account);
	
	return FALSE;
}

static void
transport_accounts_window_button_add_clicked (GtkButton                     *button,
					      GossipTransportAccountsWindow *window)
{
	GossipTransportAccountList *al;
	GList                      *l;

#if 0
	GtkTreeSelection           *selection;
	GtkTreeModel               *model;
	GtkTreeIter                 iter;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview_accounts));

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return;
	}

	gtk_tree_model_get (model, &iter, COL_TRANSPORT_ACC_DATA, &account, -1);

 	al = gossip_transport_account_list_find (account); 
#endif

	/* FIXME: need to do this better, plus need a gui to select */
	l = gossip_transport_account_lists_get_all ();
	al = g_list_nth_data (l, 0);
	if (!al) {
		return;
	}

	gossip_transport_add_window_show (al);
}

static void
transport_accounts_window_button_remove_clicked (GtkButton                     *button,
						 GossipTransportAccountsWindow *window)
{
	GossipTransportAccount  *account;
	gint                     count;

	GtkTreeSelection        *selection;
	GtkTreeModel            *model;
	GtkTreeIter              iter;

	GtkWidget               *dialog;
	gchar                   *str = NULL;
	gint                     response;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview_accounts));

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return;
	}

	gtk_tree_model_get (model, &iter, COL_TRANSPORT_ACC_DATA, &account, -1);
	count = gossip_transport_account_count_contacts (account);

	if (count == 1) {
		str = g_strdup_printf (_("If you continue, you will not be able to talk "
					 "to the 1 contact you are using this account for!"));
	} else if (count < 1) {
		str = g_strdup_printf (_("If you continue, you will not be able to add "
					 "or talk to contacts using this transport!"));
	} else if (count > 1) {
		str = g_strdup_printf (_("If you continue, you will not be able to talk "
					 "to the %d contacts you are using this account for!"), 
				       count);
	}
		
	dialog = gtk_message_dialog_new (NULL,
					 0,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_YES_NO,
					 "%s\n\n%s",
					 str,
					 _("Are you sure you want to remove this account?"));
	
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	
	g_free (str);

	if (response == GTK_RESPONSE_NO) {
		return;
	}

	gossip_transport_account_remove (account);
}

static void
transport_accounts_window_button_close_clicked (GtkButton                     *button,
						GossipTransportAccountsWindow *window)
{
	gtk_widget_destroy (window->window);
}

static void
transport_accounts_window_destroy (GtkWidget                     *widget, 
				   GossipTransportAccountsWindow *window)
{
	GossipTransportAccountList *al;
	GossipJabber               *jabber;
	GList                      *l;
	GtkTreeModel               *model;

	current_window = NULL;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (window->treeview_accounts));
	gtk_tree_model_foreach (model, transport_accounts_window_model_foreach_cb, NULL);

	/* FIXME: need to do this better, plus need a gui to select */
	l = gossip_transport_account_lists_get_all ();
	al = g_list_nth_data (l, 0);

	g_return_if_fail (al != NULL);

	jabber = gossip_transport_account_list_get_jabber (al);

 	g_signal_handlers_disconnect_by_func (GOSSIP_PROTOCOL (jabber),  
 					      transport_accounts_window_roster_update_cb, 
 					      window); 

 	g_free (window); 
}
