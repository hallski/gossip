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
#include <libgnomeui/gnome-druid.h>
#include <libgnome/gnome-i18n.h>
#include <loudmouth/loudmouth.h>
#include "gossip-utils.h"
#include "gossip-app.h"
#include "gossip-disco.h"
#include "gossip-disco-register.h"
#include "gossip-disco-servers.h"
#include "gossip-disco-window.h"
#include "gossip-stock.h"

#define d(x) x

#define SEARCH_MAX 5


struct _GossipDiscoWindow {
	GtkWidget *window;

	GtkWidget *druid;

	/* page 1 */
	GtkWidget *page_1;
	GtkWidget *treeview_protocol;

	/* page 2 */
	GtkWidget *page_2;
	GtkWidget *label_server_response;
	GtkWidget *label_server_result;
	GtkWidget *progressbar_searching;
	
	/* page 3 */
	GtkWidget *page_3;
	GtkWidget *label_server;
	GtkWidget *expander_requirements_response;
	GtkWidget *label_requirements_result;
	GtkWidget *label_instructions;
	GtkWidget *label_username;
	GtkWidget *label_password;
	GtkWidget *label_email;
	GtkWidget *label_nickname;
	GtkWidget *entry_username;
	GtkWidget *entry_password;
	GtkWidget *entry_email;
	GtkWidget *entry_nickname;

	/* page 4 */
	GtkWidget *page_4;
	GtkWidget *label_register_response;
	GtkWidget *label_register_result;

	GList *disco_list;
	GossipDiscoRegister *reg;

	GossipJID *server_jid;
	gboolean server_found;

	const gchar *disco_type;
};

enum {
	COL_DISCO_NAME,
	COL_DISCO_JID,
	COL_DISCO_REGISTERED,
	COL_DISCO_COUNT
};

enum {
	COL_DISCO_TYPE_DATA,
	COL_DISCO_TYPE_COUNT
};


static void     disco_window_setup                 (GossipDiscoWindow   *window);
static void     disco_window_setup_protocols_cb    (GList               *protocols,
						    GossipDiscoWindow   *window);
static void     disco_window_populate_columns      (GossipDiscoWindow   *window);
static gboolean disco_window_model_foreach_cb      (GtkTreeModel        *model,
						    GtkTreePath         *path,
						    GtkTreeIter         *iter,
						    gpointer             data);
static void     disco_window_prepare_page_1        (GnomeDruidPage      *page,
						    GnomeDruid          *druid,
						    GossipDiscoWindow   *window);
static void     disco_window_prepare_page_2        (GnomeDruidPage      *page,
						    GnomeDruid          *druid,
						    GossipDiscoWindow   *window);
static void     disco_window_prepare_page_3        (GnomeDruidPage      *page,
						    GnomeDruid          *druid,
						    GossipDiscoWindow   *window);
static void     disco_window_prepare_page_4        (GnomeDruidPage      *page,
						    GnomeDruid          *druid,
						    GossipDiscoWindow   *window);
static void     disco_window_finish_page_4         (GnomeDruidPage      *page,
						    GtkWidget           *widget,
						    GossipDiscoWindow   *window);
static void     disco_window_selection_changed_cb  (GtkTreeSelection    *treeselection,
						    GossipDiscoWindow   *window);
static void     disco_window_druid_stop            (GtkWidget           *widget,
						    GossipDiscoWindow   *window);
static void     disco_window_entry_details_changed (GtkEntry            *entry,
						    GossipDiscoWindow   *window);
static void     disco_window_destroy               (GtkWidget           *widget,
						    GossipDiscoWindow   *window);
static void     disco_window_disco_cleanup         (GossipDiscoWindow   *window);
static void     disco_window_check_local           (GossipDiscoWindow   *window);
static void     disco_window_check_local_cb        (GossipDisco         *disco,
						    GossipDiscoItem     *item,
						    gboolean             last_item,
						    gboolean             timeout,
						    GossipDiscoWindow   *window);
static void     disco_window_check_others          (GossipDiscoWindow   *window);
static void     disco_window_check_others_cb       (GossipDisco         *disco,
						    GossipDiscoItem     *item,
						    gboolean             last_item,
						    gboolean             timeout,
						    GossipDiscoWindow   *window);
static void     disco_window_search_failed         (GossipDiscoWindow   *window);
static void     disco_window_proceed_to_register   (GossipDiscoWindow   *window);
static void     disco_window_requirements          (GossipDiscoWindow   *window);
static void     disco_window_register              (GossipDiscoWindow   *window);
static void     disco_window_register_1_cb         (GossipDiscoRegister *reg,
						    const gchar         *error_code,
						    const gchar         *error_reason,
						    GossipDiscoWindow   *window);
static void     disco_window_register_2_cb         (GossipDiscoRegister *reg,
						    const gchar         *error_code,
						    const gchar         *error_reason,
						    GossipDiscoWindow   *window);
static void     disco_window_pixbuf_cell_data_func (GtkTreeViewColumn   *tree_column,
						    GtkCellRenderer     *cell,
						    GtkTreeModel        *model,
						    GtkTreeIter         *iter,
						    GossipDiscoWindow   *window);
static void     disco_window_name_cell_data_func   (GtkTreeViewColumn   *tree_column,
						    GtkCellRenderer     *cell,
						    GtkTreeModel        *model,
						    GtkTreeIter         *iter,
						    GossipDiscoWindow   *window);



static GossipDiscoWindow *current_window = NULL;
    

GossipDiscoWindow *
gossip_disco_window_show (void)
{
	GossipDiscoWindow *window;
	GladeXML          *gui;
	
	if (current_window) {
		gtk_window_present (GTK_WINDOW (current_window->window));
		return current_window;	
	}

	current_window = window = g_new0 (GossipDiscoWindow, 1);

	gui = gossip_glade_get_file (GLADEDIR "/main.glade",
				     "disco_window",
				     NULL,
				     "disco_window", &window->window,
				     "druid", &window->druid,
				     "page_1", &window->page_1,
				     "page_2", &window->page_2,
				     "page_3", &window->page_3,
				     "page_4", &window->page_4,
				     "treeview_protocol", &window->treeview_protocol,
				     "label_server_response", &window->label_server_response,
				     "label_server_result", &window->label_server_result,
				     "progressbar_searching", &window->progressbar_searching,
				     "label_server", &window->label_server,
				     "expander_requirements_response", &window->expander_requirements_response,
				     "label_requirements_result", &window->label_requirements_result,
 				     "label_register_response", &window->label_register_response, 
 				     "label_register_result", &window->label_register_result, 
				     "label_instructions", &window->label_instructions,
				     "label_username", &window->label_username,
				     "label_password", &window->label_password,
				     "label_email", &window->label_email, 
				     "label_nickname", &window->label_nickname,
				     "entry_username", &window->entry_username,
				     "entry_password", &window->entry_password,
				     "entry_email", &window->entry_email, 
				     "entry_nickname", &window->entry_nickname,
				     NULL);

	gossip_glade_connect (gui,
			      window,
			      "disco_window", "destroy", disco_window_destroy,
			      "druid", "cancel", disco_window_druid_stop,
			      "entry_username", "changed", disco_window_entry_details_changed,
			      "entry_password", "changed", disco_window_entry_details_changed,
			      "entry_email", "changed", disco_window_entry_details_changed,
			      "entry_nickname", "changed", disco_window_entry_details_changed,
			      NULL);

	g_object_unref (gui);

	g_signal_connect_after (window->page_1, "prepare",
				G_CALLBACK (disco_window_prepare_page_1),
				window);
	g_signal_connect_after (window->page_2, "prepare",
				G_CALLBACK (disco_window_prepare_page_2),
				window);
	g_signal_connect_after (window->page_3, "prepare",
				G_CALLBACK (disco_window_prepare_page_3),
				window);
	g_signal_connect_after (window->page_4, "prepare",
				G_CALLBACK (disco_window_prepare_page_4),
				window);
	g_signal_connect_after (window->page_4, "finish",
				G_CALLBACK (disco_window_finish_page_4),
				window);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   FALSE, FALSE, TRUE, TRUE); 

	disco_window_setup (window);

	return window;
}


/*
 * 1. Check local server for service.
 * 2. Look up list at imendio.com OR use local list
 * 3. Search 5-10 of the servers on the list
 *
 *
 * NOTE: 
 * How do we know if they are registered with MSN/ICQ/etc in the first
 * place? We could always go through the contact list and find out
 * which JIDs are services and query them (but what if they are not a
 * contact?) OR we could look at the actual contacts and get the
 * server part and ask the server if we are registered?!
 *
 */

static void 
disco_window_disco_cleanup (GossipDiscoWindow *window)
{
	GList *l;

	/* clean up all discos which are not required */
	for (l=window->disco_list; l; l=l->next) {
		GossipDisco *d = l->data;
		gossip_disco_destroy (d);
	}
	
	g_list_free (window->disco_list);
	window->disco_list = NULL;
}

static void 
disco_window_check_local (GossipDiscoWindow *window)
{
	GossipDisco *disco;
	GossipJID   *jid;
	const gchar *host;

	jid = gossip_app_get_jid ();
 	host = gossip_jid_get_part_host (jid); 

	window->server_found = FALSE;

	d(g_print ("running disco on local server:'%s'\n", host));

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (window->progressbar_searching), 0);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (window->progressbar_searching), 
				   _("Searching Local Services..."));

	disco = gossip_disco_request (host, 
				      (GossipDiscoItemFunc) disco_window_check_local_cb,
				      window);
	
	window->disco_list = g_list_append (window->disco_list, disco);
}

static void
disco_window_check_local_cb (GossipDisco       *disco, 
			     GossipDiscoItem   *item, 
			     gboolean           last_item, 
			     gboolean           timeout,
			     GossipDiscoWindow *window)
{
	gdouble  total;
	gdouble         remaining;
	gdouble         fraction;
	gchar          *str;

	remaining = gossip_disco_get_items_remaining (disco);
	total = gossip_disco_get_items_total (disco);

	/* show progress */
	fraction = 1 - (remaining / total);

	d(g_print ("disco local items: complete:%d, items:%d, remaining:%d, fraction:%f\n", 
		   (gint)total - (gint)remaining, (gint)remaining, (gint)total, (gfloat)fraction));

	str = g_strdup_printf (_("Searching Local Services (%d of %d)"), 
			       (gint)total - (gint)remaining, (gint)total);

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (window->progressbar_searching), 
				       fraction);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (window->progressbar_searching),
				   str);

	g_free (str);

	if (!window->server_found &&  
	    (!item ||
	     !gossip_disco_item_has_feature (item, "jabber:iq:register") ||
	     !gossip_disco_item_has_category (item, "gateway") ||
	     !gossip_disco_item_has_type (item, window->disco_type))) {
		if (timeout) {
			d(g_print ("disco timed out\n"));
		} 

		if (last_item) {
			d(g_print ("all local disco services received\n"));
			disco_window_disco_cleanup (window);
			disco_window_check_others (window);
		}

		return;
	} 

	if (!window->server_found) {
		GossipJID *jid;

		jid = gossip_disco_item_get_jid (item);

		window->server_found = TRUE;
		window->server_jid = gossip_jid_ref (jid);

		d(g_print ("disco server found: '%s'!\n", 
			   gossip_jid_get_full (jid)));

		disco_window_disco_cleanup (window);

		if (timeout) {
			d(g_print ("disco timed out\n"));
		}

		if (last_item) {
			d(g_print ("all local disco services received\n"));
		}
		 
		disco_window_proceed_to_register (window);
	}
}

static void
disco_window_check_others (GossipDiscoWindow *window)
{
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	
	d(g_print ("checking other servers, protocol:'%s' not found locally\n", 
		   window->disco_type));

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (window->progressbar_searching), 0);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (window->progressbar_searching), 
				   _("Searching 3rd Party Services..."));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview_protocol));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		GossipDiscoProtocol *protocol;
		GList               *l;
		gint                 count;

		gtk_tree_model_get (model, &iter, COL_DISCO_TYPE_DATA, &protocol, -1);
		g_return_if_fail (protocol != NULL);

 		d(g_print ("only looking up %d servers from protocol server listing\n",  
 			   SEARCH_MAX)); 
		
		l = gossip_disco_protocol_get_servers (protocol);
		
 		for (count=0; l && count < SEARCH_MAX; l=l->next, count++) { 
			GossipDisco       *disco;
			GossipDiscoServer *server;
			const gchar       *host;

			server = (GossipDiscoServer*) l->data;
			host = gossip_disco_server_get_host (server);

			d(g_print ("running disco on remote server:'%s'\n", host));

			disco = gossip_disco_request (host, 
						      (GossipDiscoItemFunc) disco_window_check_others_cb,
						      window);

			window->disco_list = g_list_append (window->disco_list, disco);
		}
	}
}

static void
disco_window_check_others_cb (GossipDisco       *disco, 
			      GossipDiscoItem   *item, 
			      gboolean           last_item,
			      gboolean           timeout,
			      GossipDiscoWindow *window)
{
	gdouble  total;
	gdouble         remaining;
	gdouble         fraction;
	gchar          *str;

	remaining = gossip_disco_get_items_remaining (disco);
	total = gossip_disco_get_items_total (disco);

	/* show progress */
	fraction = 1 - (remaining / total);

	d(g_print ("disco 3rd party items: complete:%d, items:%d, remaining:%d, fraction:%f\n", 
		   (gint)total - (gint)remaining, (gint)remaining, (gint)total, (gfloat)fraction));

	str = g_strdup_printf (_("Searching 3rd Party Services (%d of %d)"), 
			       (gint)total - (gint)remaining, (gint)total);

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (window->progressbar_searching), 
				       fraction);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (window->progressbar_searching),
				   str);

	g_free (str);

	if (!window->server_found &&
	    (!item ||
	     !gossip_disco_item_has_feature (item, "jabber:iq:register") ||
	     !gossip_disco_item_has_category (item, "gateway") ||
	     !gossip_disco_item_has_type (item, window->disco_type))) {
		if (timeout) {
			d(g_print ("disco timed out\n"));
		} 

		if(last_item) {
			disco_window_disco_cleanup (window);
			disco_window_search_failed (window);
			d(g_print ("all disco services received\n"));
		}

		return;
	} 

	if (!window->server_found) {
		GossipJID *jid;

		jid = gossip_disco_item_get_jid (item);

		window->server_found = TRUE;
		window->server_jid = gossip_jid_ref (jid);

		d(g_print ("disco server found: '%s'!\n", 
			   gossip_jid_get_full (jid)));
		
		disco_window_disco_cleanup (window);

		if (timeout) {
			d(g_print ("disco timed out\n"));
		} 

		if (last_item) {
			d(g_print ("all disco services received\n"));
		}
		 
		disco_window_proceed_to_register (window);
	}
}

static void
disco_window_search_failed (GossipDiscoWindow *window)
{
	gchar *str;

	str = g_strdup_printf ("<b>%s</b>\n\n%s",
			       _("Sorry, no services found at this time."),
			       _("Currently no providers are available for "
				 "the account type you are trying to configure. "
				 "Please try again later."));
	
	gtk_widget_show (window->label_server_response);
	gtk_label_set_markup (GTK_LABEL (window->label_server_response), str);
	
	g_free (str);

	gtk_widget_hide (window->progressbar_searching);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   TRUE, FALSE, TRUE, TRUE); 
}

static void 
disco_window_proceed_to_register (GossipDiscoWindow *window)
{
	d(g_print ("proceeding to register protocol:'%s' with jid:'%s'\n", 
		   window->disco_type, 
		   gossip_jid_get_full (window->server_jid)));

	gtk_label_set_text (GTK_LABEL (window->label_server), 
			    gossip_jid_get_full (window->server_jid));

	gnome_druid_set_page (GNOME_DRUID (window->druid), 
			      GNOME_DRUID_PAGE (window->page_3));

	disco_window_requirements (window);
}

static void
disco_window_requirements (GossipDiscoWindow *window)
{
	GossipDiscoRegister *reg;

	d(g_print ("asking for disco registration requirements\n"));

	reg = gossip_disco_register_requirements (gossip_jid_get_full (window->server_jid),
						  (GossipDiscoRegisterFunc) disco_window_register_1_cb,
						  window);

	window->reg = reg;
}

static void
disco_window_register (GossipDiscoWindow *window)
{
	const gchar *username;
	const gchar *password;
	const gchar *email;
	const gchar *nickname;

	username = gtk_entry_get_text (GTK_ENTRY (window->entry_username));
	password = gtk_entry_get_text (GTK_ENTRY (window->entry_password));
	email = gtk_entry_get_text (GTK_ENTRY (window->entry_email));
	nickname = gtk_entry_get_text (GTK_ENTRY (window->entry_nickname));

	gossip_disco_register_set_username (window->reg, username);
	gossip_disco_register_set_password (window->reg, password);
	gossip_disco_register_set_email (window->reg, email);
	gossip_disco_register_set_nickname (window->reg, nickname);

	d(g_print ("requesting disco registration\n"));

 	gossip_disco_register_request (window->reg, 
				       (GossipDiscoRegisterFunc) disco_window_register_2_cb, 
				       window); 
}


static void
disco_window_register_1_cb (GossipDiscoRegister *reg,
			    const gchar         *error_code,
			    const gchar         *error_reason,
			    GossipDiscoWindow   *window)
{
	gtk_widget_hide (window->expander_requirements_response);
	gtk_widget_show (window->label_register_result);

	if (error_code || error_reason) {
		gchar *str;

		str = g_strdup_printf ("%s\n\n%s",
				       _("Unable to Register"),
				       error_reason);

		gtk_widget_show (window->label_requirements_result);
		gtk_label_set_markup (GTK_LABEL (window->label_requirements_result), str);

		g_free (str);
		return;
	}

	if (gossip_disco_register_get_require_nickname (reg)) {
		gtk_widget_show (window->label_nickname);
		gtk_widget_show (window->entry_nickname);
		gtk_widget_grab_focus (window->entry_nickname);
	} else {
		gtk_widget_hide (window->label_nickname);
		gtk_widget_hide (window->entry_nickname);
	}

	if (gossip_disco_register_get_require_email (reg)) {
		gtk_widget_show (window->label_email);
		gtk_widget_show (window->entry_email);
		gtk_widget_grab_focus (window->entry_email);
	} else {
		gtk_widget_hide (window->label_email);
		gtk_widget_hide (window->entry_email);
	}

	if (gossip_disco_register_get_require_password (reg)) {
		gtk_widget_show (window->label_password);
		gtk_widget_show (window->entry_password);
		gtk_widget_grab_focus (window->entry_password);
	} else {
		gtk_widget_hide (window->label_password);
		gtk_widget_hide (window->entry_password);
	}

	if (gossip_disco_register_get_require_username (reg)) {
		gtk_widget_show (window->label_username);
		gtk_widget_show (window->entry_username);
		gtk_widget_grab_focus (window->entry_username);
	} else {
		gtk_widget_hide (window->label_username);
		gtk_widget_hide (window->entry_username);
	}

	/* we are bypassing this because each server is unique and may
	   have the instructions in different languages */
	gtk_widget_show (window->label_instructions);
/* 	gtk_label_set_text (GTK_LABEL (window->label_instructions), */
/* 			    gossip_disco_register_get_instructions (reg)); */

	window->reg = reg;
}

static void
disco_window_register_2_cb (GossipDiscoRegister *reg,
			    const gchar         *error_code,
			    const gchar         *error_reason,
			    GossipDiscoWindow   *window)
{
	gtk_widget_hide (window->label_register_response);
	gtk_widget_show (window->label_register_result);

	if (error_code || error_reason) {
		gchar *str;

		str = g_strdup_printf ("<b>%s</b>\n\n%s",
				       _("Unable to Register"),
				       error_reason);

		gtk_label_set_markup (GTK_LABEL (window->label_register_result), str);

		g_free (str);

		gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
						   FALSE, FALSE, TRUE, TRUE); 

	} else {
		gchar *str;

		str = g_strdup_printf ("<b>%s</b>\n\n%s",
				       _("Registration Successful!"),
				       _("You are now able to add contacts using this transport."));
		
		gtk_label_set_markup (GTK_LABEL (window->label_register_result), str);

		g_free (str);

		gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
						   FALSE, TRUE, FALSE, TRUE); 
	}
}

static void
disco_window_setup (GossipDiscoWindow *window)
{
	GtkListStore     *store;
	GtkTreeSelection *selection;

	store = gtk_list_store_new (COL_DISCO_TYPE_COUNT,
				    G_TYPE_POINTER,  /* object */
				    G_TYPE_BOOLEAN); /* editable */
	
	gtk_tree_view_set_model (GTK_TREE_VIEW (window->treeview_protocol), 
				 GTK_TREE_MODEL (store));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview_protocol));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect (G_OBJECT (selection), "changed", 
			  G_CALLBACK (disco_window_selection_changed_cb), window);


	disco_window_populate_columns (window);

 	gossip_disco_protocols_get_supported ((GossipDiscoProtocolsFunc)disco_window_setup_protocols_cb,
					      window); 


	g_object_unref (store);
}

static void
disco_window_setup_protocols_cb (GList             *protocol_list,
				 GossipDiscoWindow *window)
{
	GtkTreeView  *view;
	GtkListStore *store;

	GList        *l;

	view = GTK_TREE_VIEW (window->treeview_protocol);
	store = GTK_LIST_STORE (gtk_tree_view_get_model (view));

	/* populate protocols */
	for (l=protocol_list; l; l=l->next) {
		GossipDiscoProtocol *protocol;
		GtkTreeIter          iter;

		protocol = l->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, COL_DISCO_TYPE_DATA, protocol, -1);
	}

	g_list_free (protocol_list);
}

static void 
disco_window_populate_columns (GossipDiscoWindow *window)
{
	GtkTreeViewColumn *column; 
	GtkCellRenderer   *cell;
	
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (window->treeview_protocol), FALSE);

	column = gtk_tree_view_column_new ();
	
	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, cell, 
						 (GtkTreeCellDataFunc) disco_window_pixbuf_cell_data_func,
						 window, 
						 NULL);

	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc) disco_window_name_cell_data_func,
						 window,
						 NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (window->treeview_protocol), column);
}

static void  
disco_window_pixbuf_cell_data_func (GtkTreeViewColumn *tree_column,
				    GtkCellRenderer   *cell,
				    GtkTreeModel      *model,
				    GtkTreeIter       *iter,
				    GossipDiscoWindow *window)
{
	GossipDiscoProtocol *protocol;
	const gchar         *disco_type;
	const gchar         *icon;
	const gchar         *stock_id = NULL;
	const gchar         *core_icon = NULL;
	GdkPixbuf           *pixbuf = NULL;
	gint                 w, h;
	gint                 size = 48;  /* default size */
	
	gtk_tree_model_get (model, iter, COL_DISCO_TYPE_DATA, &protocol, -1);

	g_return_if_fail (protocol != NULL);

	disco_type = gossip_disco_protocol_get_disco_type (protocol);
	icon = gossip_disco_protocol_get_icon (protocol);

	/* these can not be overridden */
	if (strcmp (disco_type, "aim") == 0) {
		core_icon = "im-aim";
	} else if (strcmp (disco_type, "icq") == 0) {
		core_icon = "im-icq";
	} else if (strcmp (disco_type, "msn") == 0) {
		core_icon = "im-msn";
	} else if (strcmp (disco_type, "yahoo") == 0) {
		core_icon = "im-yahoo";
	} else {
		stock_id = gossip_disco_protocol_get_stock_icon (protocol);
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
			      NULL); 

		return;
	} else if (icon) {
		GError      *error = NULL;

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
disco_window_name_cell_data_func (GtkTreeViewColumn *tree_column,
				  GtkCellRenderer   *cell,
				  GtkTreeModel      *model,
				  GtkTreeIter       *iter,
				  GossipDiscoWindow *window)
{
	GossipDiscoProtocol *protocol;
	gchar               *str;

	GtkTreeSelection    *selection;
	PangoAttrList       *attr_list;
	PangoAttribute      *attr_color, *attr_style, *attr_size;
	GtkStyle            *style;
	GdkColor             color;

	const gchar         *name;
	const gchar         *description;

	gtk_tree_model_get (model, iter, COL_DISCO_TYPE_DATA, &protocol, -1);

	g_return_if_fail (protocol != NULL);

	name = gossip_disco_protocol_get_name (protocol);
	description = gossip_disco_protocol_get_description (protocol);

	str = g_strdup_printf ("%s%s%s", 
			       name, 
			       description ? "\n" : "",
			       description ? description : "");

	style = gtk_widget_get_style (window->treeview_protocol);
	color = style->text_aa[GTK_STATE_NORMAL];

	attr_list = pango_attr_list_new ();

	attr_style = pango_attr_style_new (PANGO_STYLE_ITALIC);
	attr_style->start_index = strlen (name) + 1;
	attr_style->end_index = -1;
	pango_attr_list_insert (attr_list, attr_style);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview_protocol));

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
disco_window_selection_changed_cb (GtkTreeSelection  *treeselection,
				   GossipDiscoWindow *window)
{
	gboolean next = FALSE;

	if (gtk_tree_selection_count_selected_rows (treeselection) == 1) {
		next = TRUE;
	}

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   FALSE, next, TRUE, TRUE); 
}

static void
disco_window_prepare_page_1 (GnomeDruidPage    *page,
			     GnomeDruid        *druid, 
			     GossipDiscoWindow *window)
{
	gtk_widget_grab_focus (window->treeview_protocol);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   FALSE, FALSE, TRUE, TRUE); 
}

static void
disco_window_prepare_page_2 (GnomeDruidPage    *page,
			     GnomeDruid        *druid, 
			     GossipDiscoWindow *window)
{
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;

	/* set up widgets */
	gtk_widget_show (window->label_server_response);
	gtk_widget_show (window->progressbar_searching);

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (window->progressbar_searching), 0);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   TRUE, FALSE, TRUE, TRUE); 

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview_protocol));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		GossipDiscoProtocol *protocol;

		gtk_tree_model_get (model, &iter, 
				    COL_DISCO_TYPE_DATA, &protocol,
				    -1);

		g_return_if_fail (protocol != NULL);

		window->disco_type = gossip_disco_protocol_get_disco_type (protocol);
	} 

	disco_window_check_local (window);
}

static void
disco_window_prepare_page_3 (GnomeDruidPage    *page,
			     GnomeDruid        *druid, 
			     GossipDiscoWindow *window)
{
	gtk_widget_show (window->expander_requirements_response);
	gtk_widget_hide (window->label_requirements_result);
	gtk_widget_hide (window->label_instructions);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   FALSE, FALSE, TRUE, TRUE); 

/* 	disco_window_requirements (window); */
}

static void
disco_window_prepare_page_4 (GnomeDruidPage    *page,
			     GnomeDruid        *druid, 
			     GossipDiscoWindow *window)
{
	gtk_widget_show (window->label_register_response);
	gtk_widget_hide (window->label_register_result);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   FALSE, FALSE, TRUE, TRUE); 

	gnome_druid_set_show_finish (GNOME_DRUID (window->druid), TRUE);

	disco_window_register (window);
}

static void
disco_window_finish_page_4 (GnomeDruidPage    *page,
			    GtkWidget         *widget, 
			    GossipDiscoWindow *window)
{
	gtk_widget_destroy (window->window);
}

static void
disco_window_druid_stop (GtkWidget         *widget, 
			 GossipDiscoWindow *window)
{
	gtk_widget_destroy (window->window);
}

static void 
disco_window_entry_details_changed (GtkEntry          *entry,
				    GossipDiscoWindow *window)
{
	const gchar *username;
	const gchar *password;
	const gchar *email;
	const gchar *nickname;

	username = gtk_entry_get_text (GTK_ENTRY (window->entry_username));
	password = gtk_entry_get_text (GTK_ENTRY (window->entry_password));
	email = gtk_entry_get_text (GTK_ENTRY (window->entry_email));
	nickname = gtk_entry_get_text (GTK_ENTRY (window->entry_nickname));
	
	if ((GTK_WIDGET_VISIBLE (window->entry_username) && 
	     (!username || strlen (username) < 1)) ||
	    (GTK_WIDGET_VISIBLE (window->entry_password) && 
	     (!password || strlen (password) < 1)) ||
	    (GTK_WIDGET_VISIBLE (window->entry_email) && 
	     (!email || strlen (email) < 1)) ||
	    (GTK_WIDGET_VISIBLE (window->entry_nickname) && 
	     (!nickname || strlen (nickname) < 1))) {
		gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
						   TRUE, FALSE, TRUE, TRUE); 
	} else {
		gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
						   TRUE, TRUE, TRUE, TRUE); 
	}		
}

static gboolean
disco_window_model_foreach_cb (GtkTreeModel *model,
			       GtkTreePath  *path,
			       GtkTreeIter  *iter,
			       gpointer      data)
{
	GossipDiscoProtocol *protocol;

	gtk_tree_model_get (model, iter, COL_DISCO_TYPE_DATA, &protocol, -1);
	gossip_disco_protocol_free (protocol);
	
	return FALSE;
}

static void
disco_window_destroy (GtkWidget         *widget, 
		      GossipDiscoWindow *window)
{
	GtkTreeModel *model;

	current_window = NULL;

	disco_window_disco_cleanup (window);

	gossip_disco_register_destroy (window->reg);

	gossip_jid_unref (window->server_jid);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (window->treeview_protocol));
	gtk_tree_model_foreach (model, disco_window_model_foreach_cb, NULL);

 	g_free (window); 
}
