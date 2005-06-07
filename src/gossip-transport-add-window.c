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
#include <glib/gi18n.h>
#include <loudmouth/loudmouth.h>
#include "gossip-utils.h"
#include "gossip-app.h"
#include "gossip-transport-discover.h"
#include "gossip-transport-register.h"
#include "gossip-transport-protocol.h"
#include "gossip-transport-add-window.h"
#include "gossip-transport-accounts.h"
#include "gossip-stock.h"
#include "gossip-contact.h"

#define d(x) x

#define SEARCH_MAX 15


struct _GossipTransportAddWindow {
	GtkWidget *window;

	GtkWidget *druid;

	/* page 1 */
	GtkWidget *page_1;
	GtkWidget *treeview_protocol;

	/* page 2 */
	GtkWidget *page_2;
	GtkWidget *label_service_response;
	GtkWidget *progressbar_searching;
	GtkWidget *treeview_service;
	GtkWidget *scrolledwindow_service;
	GtkWidget *entry_service;
	
	/* page 3 */
	GtkWidget *page_3;
	GtkWidget *label_service;
	GtkWidget *label_stub_service;
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
	GtkWidget *label_register_result;
	GtkWidget *progressbar_registering;

	/* disco stuff */ 
	GList *disco_list;

	const gchar *disco_type;
	guint discos_received;
	guint discos_sent;

	/* misc */
	gboolean service_found;
	gboolean using_local_service;

	guint progress_timeout_id;

	/* registraton requirements */
	GossipJID *jid;
	gchar *key;

	gboolean require_username;
	gboolean require_password;
	gboolean require_nick;
	gboolean require_email;

	guint roster_timeout_id;

	GossipTransportAccountList *al;
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

enum {
	COL_DISCO_SERVICE_NAME,
	COL_DISCO_SERVICE_JID,
	COL_DISCO_SERVICE_CAN_REGISTER,
	COL_DISCO_SERVICE_COUNT
};


static void     transport_add_window_protocol_setup                 (GossipTransportAddWindow *window);
static void     transport_add_window_protocol_populate_columns      (GossipTransportAddWindow *window);
static gboolean transport_add_window_protocol_model_foreach_cb      (GtkTreeModel             *model,
								     GtkTreePath              *path,
								     GtkTreeIter              *iter,
								     gpointer                  data);
static void     transport_add_window_service_setup                   (GossipTransportAddWindow *window);
static void     transport_add_window_service_populate_columns        (GossipTransportAddWindow *window);
static void     transport_add_window_prepare_page_1                 (GnomeDruidPage           *page,
								     GnomeDruid               *druid,
								     GossipTransportAddWindow *window);
static void     transport_add_window_prepare_page_2                 (GnomeDruidPage           *page,
								     GnomeDruid               *druid,
								     GossipTransportAddWindow *window);
static void     transport_add_window_prepare_page_3                 (GnomeDruidPage           *page,
								     GnomeDruid               *druid,
								     GossipTransportAddWindow *window);
static void     transport_add_window_prepare_page_4                 (GnomeDruidPage           *page,
								     GnomeDruid               *druid,
								     GossipTransportAddWindow *window);
static void     transport_add_window_finish_page_4                  (GnomeDruidPage           *page,
								     GtkWidget                *widget,
								     GossipTransportAddWindow *window);
static void     transport_add_window_protocol_selection_changed_cb  (GtkTreeSelection         *treeselection,
								     GossipTransportAddWindow *window);
static void     transport_add_window_service_selection_changed_cb   (GtkTreeSelection         *treeselection,
								     GossipTransportAddWindow *window);
static void     transport_add_window_druid_stop                     (GtkWidget                *widget,
								     GossipTransportAddWindow *window);
static void     transport_add_window_entry_details_changed          (GtkEntry                 *entry,
								     GossipTransportAddWindow *window);
static void     transport_add_window_entry_service_changed           (GtkEntry                 *entry,
								     GossipTransportAddWindow *window);
static gboolean transport_add_window_check_service_valid             (GossipTransportAddWindow *window);
static void     transport_add_window_destroy                        (GtkWidget                *widget,
								     GossipTransportAddWindow *window);
static void     transport_add_window_disco_cleanup                  (GossipTransportAddWindow *window);
static void     transport_add_window_check_local                    (GossipTransportAddWindow *window);
static void     transport_add_window_check_local_cb                 (GossipTransportDisco     *disco,
								     GossipTransportDiscoItem *item,
								     gboolean                  last_item,
								     gboolean                  timeout,
								     GError                   *error,
								     GossipTransportAddWindow *window);
static void     transport_add_window_check_others                   (GossipTransportAddWindow *window);
static void     transport_add_window_check_others_cb                (GossipTransportDisco     *disco,
								     GossipTransportDiscoItem *item,
								     gboolean                  last_item,
								     gboolean                  timeout,
								     GError                   *error,
								     GossipTransportAddWindow *window);
static void     transport_add_window_requirements                   (GossipTransportAddWindow *window,
								     GossipJID                *jid);
static void     transport_add_window_requirements_cb                (GossipJID                *jid,
								     const gchar              *key,
								     const gchar              *username,
								     const gchar              *password,
								     const gchar              *nick,
								     const gchar              *email,
								     gboolean                  require_username,
								     gboolean                  require_password,
								     gboolean                  require_nick,
								     gboolean                  require_email,
								     gboolean                  is_registered,
								     const gchar              *error_code,
								     const gchar              *error_reason,
								     GossipTransportAddWindow *window);
static void     transport_add_window_register                       (GossipTransportAddWindow *window);
static void     transport_add_window_register_cb                    (GossipJID                *jid,
								     const gchar              *error_code,
								     const gchar              *error_reason,
								     GossipTransportAddWindow *window);
static void     transport_add_window_roster_update_cb               (GossipProtocol           *protocol, 
 								     GossipContact            *contact, 
 								     GossipTransportAddWindow *window); 
static gboolean transport_add_window_roster_timeout_cb              (GossipTransportAddWindow *window);

static void     transport_add_window_protocol_pixbuf_cell_data_func (GtkTreeViewColumn        *tree_column,
								     GtkCellRenderer          *cell,
								     GtkTreeModel             *model,
								     GtkTreeIter              *iter,
								     GossipTransportAddWindow *window);
static void     transport_add_window_protocol_name_cell_data_func   (GtkTreeViewColumn        *tree_column,
								     GtkCellRenderer          *cell,
								     GtkTreeModel             *model,
								     GtkTreeIter              *iter,
								     GossipTransportAddWindow *window);
static gboolean transport_add_window_progress_cb                    (GtkProgressBar           *progress_bar);




static GossipTransportAddWindow *current_window = NULL;
    

GossipTransportAddWindow *
gossip_transport_add_window_show (GossipTransportAccountList *al)
{
	GossipTransportAddWindow *window;
	GladeXML                 *gui;
	
	g_return_val_if_fail (al != NULL, NULL);

	if (current_window) {
		gtk_window_present (GTK_WINDOW (current_window->window));
		return current_window;	
	}

	current_window = window = g_new0 (GossipTransportAddWindow, 1);

	/* FIXME: need a better way of doing this. */
	window->al = al;

	gui = gossip_glade_get_file (GLADEDIR "/main.glade",
				     "transport_add_window",
				     NULL,
				     "transport_add_window", &window->window,
				     "druid", &window->druid,
				     "page_1", &window->page_1,
				     "page_2", &window->page_2,
				     "page_3", &window->page_3,
				     "page_4", &window->page_4,
				     "treeview_protocol", &window->treeview_protocol,
				     "label_service_response", &window->label_service_response,
				     "progressbar_searching", &window->progressbar_searching,
				     "progressbar_registering", &window->progressbar_registering,
				     "label_service", &window->label_service,
				     "label_stub_service", &window->label_stub_service,
				     "entry_service", &window->entry_service,
				     "treeview_service", &window->treeview_service,
				     "scrolledwindow_service", &window->scrolledwindow_service,
				     "label_requirements_result", &window->label_requirements_result,
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
			      "transport_add_window", "destroy", transport_add_window_destroy,
			      "druid", "cancel", transport_add_window_druid_stop,
			      "entry_username", "changed", transport_add_window_entry_details_changed,
			      "entry_password", "changed", transport_add_window_entry_details_changed,
			      "entry_email", "changed", transport_add_window_entry_details_changed,
			      "entry_nickname", "changed", transport_add_window_entry_details_changed,
			      "entry_service", "changed", transport_add_window_entry_service_changed,
			      NULL);

	g_object_unref (gui);

	g_signal_connect_after (window->page_1, "prepare",
				G_CALLBACK (transport_add_window_prepare_page_1),
				window);
	g_signal_connect_after (window->page_2, "prepare",
				G_CALLBACK (transport_add_window_prepare_page_2),
				window);
	g_signal_connect_after (window->page_3, "prepare",
				G_CALLBACK (transport_add_window_prepare_page_3),
				window);
	g_signal_connect_after (window->page_4, "prepare",
				G_CALLBACK (transport_add_window_prepare_page_4),
				window);
	g_signal_connect_after (window->page_4, "finish",
				G_CALLBACK (transport_add_window_finish_page_4),
				window);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   FALSE, FALSE, TRUE, TRUE); 

	transport_add_window_protocol_setup (window);
	transport_add_window_service_setup (window);

	return window;
}


/*
 * 1. Check local service for service.
 * 2. Look up list at imendio.com OR use local list
 * 3. Search 5-10 of the services on the list
 *
 *
 * NOTE: 
 * How do we know if they are registered with MSN/ICQ/etc in the first
 * place? We could always go through the contact list and find out
 * which JIDs are services and query them (but what if they are not a
 * contact?) OR we could look at the actual contacts and get the
 * service part and ask the service if we are registered?!
 *
 */

static void 
transport_add_window_disco_cleanup (GossipTransportAddWindow *window)
{
	GList *l;

	/* clean up all discos which are not required */
	for (l=window->disco_list; l; l=l->next) {
		GossipTransportDisco *d = l->data;
		gossip_transport_disco_destroy (d);
	}
	
	g_list_free (window->disco_list);
	window->disco_list = NULL;
}

static void 
transport_add_window_check_local (GossipTransportAddWindow *window)
{
	GossipTransportDisco *disco;
	GossipJabber         *jabber;
	GossipContact        *own_contact;
	GossipJID            *jid;
	const gchar          *host;
	const gchar          *id;

	jabber = gossip_transport_account_list_get_jabber (window->al);
	own_contact = gossip_jabber_get_own_contact (jabber);
	id = gossip_contact_get_id (own_contact);
	jid = gossip_jid_new (id);
 	host = gossip_jid_get_part_host (jid); 
	
	window->service_found = FALSE;

	d(g_print ("running disco on local service:'%s'\n", host));

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (window->progressbar_searching), 0);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (window->progressbar_searching), 
				   _("Searching Local Services..."));

	disco = gossip_transport_disco_request (jabber,
						host, 
						(GossipTransportDiscoItemFunc) transport_add_window_check_local_cb,
						window);
	
	window->disco_list = g_list_append (window->disco_list, disco);

	gossip_jid_unref (jid);
}

static void
transport_add_window_check_local_cb (GossipTransportDisco      *disco, 
				     GossipTransportDiscoItem  *item, 
				     gboolean                  last_item, 
				     gboolean                  timeout,
				     GError                   *error,
				     GossipTransportAddWindow *window)
{
	GossipTransportAccount *account;
	gboolean                already_registered = FALSE;

	gdouble                 total;
	gdouble                 remaining;
	gdouble                 fraction;
	gchar                  *str;

	remaining = gossip_transport_disco_get_items_remaining (disco);
	total = gossip_transport_disco_get_items_total (disco);

	/* 404 = not found, service discovery not supported */
	if (error && error->code == 404) {
		d(g_print ("local server does not support service "
			   "discovery, trying 3rd party services\n"));
		transport_add_window_disco_cleanup (window);
		transport_add_window_check_others (window);
		
		return;
	}

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

	if (!window->service_found &&  
	    (!item ||
	     !gossip_transport_disco_item_has_feature (item, "jabber:iq:register") ||
	     !gossip_transport_disco_item_has_category (item, "gateway") ||
	     !gossip_transport_disco_item_has_type (item, window->disco_type))) {
		if (timeout) {
			d(g_print ("disco timed out\n"));
		} 

		if (last_item) {
			d(g_print ("all local disco services received\n"));
			transport_add_window_disco_cleanup (window);
			transport_add_window_check_others (window);
		}

		return;
	} 

	/* check we are not already registered, if so, we want to look
	   further at other servers because we might be registering a
	   second transport. */
	account = gossip_transport_account_find_by_disco_type (window->al,
							       window->disco_type);
	if (account && gossip_jid_equals_without_resource (gossip_transport_account_get_jid (account), 
							   gossip_transport_disco_item_get_jid (item))) {
		already_registered = TRUE;
	} 
	    
	if (!window->service_found && !already_registered) {
		GossipJID *jid;

		window->service_found = TRUE;
		window->using_local_service = TRUE;

		jid = gossip_transport_disco_item_get_jid (item);
		jid = gossip_jid_ref (jid);
		
		d(g_print ("disco service found: '%s'!\n", gossip_jid_get_full (jid)));
		
		transport_add_window_disco_cleanup (window);
		
		if (timeout) {
			d(g_print ("disco timed out\n"));
		}
		
		if (last_item) {
			d(g_print ("all local disco services received\n"));
		}
		
		transport_add_window_requirements (window, jid);
		
		gossip_jid_unref (jid);
	}
}

static void
transport_add_window_check_others (GossipTransportAddWindow *window)
{
	GossipJabber     *jabber;
	gchar            *str;

	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	
	d(g_print ("checking other services, protocol:'%s' not found locally\n", 
		   window->disco_type));

	jabber = gossip_transport_account_list_get_jabber (window->al);

	str = g_strdup_printf (_("Talking to available services..."));
	gtk_label_set_markup (GTK_LABEL (window->label_service_response), str);
	g_free (str);

	window->progress_timeout_id = g_timeout_add (50,
						     (GSourceFunc)transport_add_window_progress_cb,
						     GTK_PROGRESS_BAR (window->progressbar_searching));

	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (window->progressbar_searching), 
				   _("Searching 3rd Party Services..."));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview_protocol));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		GossipTransportProtocol *protocol;
		GList                   *l;
		gint                     count;

		gtk_tree_model_get (model, &iter, COL_DISCO_TYPE_DATA, &protocol, -1);
		g_return_if_fail (protocol != NULL);

 		d(g_print ("only looking up %d services from protocol service listing\n",  
 			   SEARCH_MAX)); 
		
		l = gossip_transport_protocol_get_services (protocol);
		
 		for (count=0; l && count < SEARCH_MAX; l=l->next, count++) { 
			GossipTransportDisco  *disco;
			GossipTransportService *service;
			const gchar           *host;

			service = (GossipTransportService*) l->data;
			host = gossip_transport_service_get_host (service);

			d(g_print ("running disco on remote service:'%s'\n", host));

			disco = gossip_transport_disco_request_info (jabber,
								     host, 
								     (GossipTransportDiscoItemFunc) transport_add_window_check_others_cb,
								     window);

			window->disco_list = g_list_append (window->disco_list, disco);
		}

		d(g_print ("sent %d disco requests\n", count));

		window->discos_received = 0;
		window->discos_sent = count;
	}
}

static gboolean
transport_add_window_progress_cb (GtkProgressBar *progress_bar)
{
	gtk_progress_bar_pulse (progress_bar);
	return TRUE;
}

static void
transport_add_window_check_others_cb (GossipTransportDisco     *disco, 
				      GossipTransportDiscoItem *item, 
				      gboolean                  last_item,
				      gboolean                  timeout,
				      GError                   *error,
				      GossipTransportAddWindow *window)
{
	GtkTreeView  *view;
	GtkTreeModel *model;
	GtkListStore *store;
	GtkTreeIter   iter;

	GossipJID    *jid;
	gchar        *str;

	view = GTK_TREE_VIEW (window->treeview_service);
	model = GTK_TREE_MODEL (gtk_tree_view_get_model (view));
	store = GTK_LIST_STORE (gtk_tree_view_get_model (view));

	jid = gossip_transport_disco_item_get_jid (item);
	
	window->discos_received++;

	d(g_print ("disco checking item with jid:'%s'...\n", gossip_jid_get_full (jid)));

	str = g_strdup_printf (_("Searching 3rd Party Services (%d of %d)"), 
			       (gint)window->discos_received, (gint)SEARCH_MAX);

	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (window->progressbar_searching),
				   str);

	g_free (str);

	if(last_item || window->discos_received >= window->discos_sent) {
		gint count = 0;

		if (window->progress_timeout_id) {
			g_source_remove (window->progress_timeout_id);
			window->progress_timeout_id = 0;
		}
			
		gtk_widget_show (window->label_service_response);
		gtk_widget_hide (window->progressbar_searching);

		count = gtk_tree_model_iter_n_children (model, NULL);

		if (count < 1) {
			str = g_strdup_printf ("%s\n\n%s",
					       _("Sorry, no services found at this time."),
					       _("Currently no providers are available for "
						 "the account type you are trying to configure. "
						 "Please try again later."));
			gtk_label_set_markup (GTK_LABEL (window->label_service_response), str);
			g_free (str);
						
			gtk_widget_hide (window->scrolledwindow_service);

			gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
							   FALSE, FALSE, TRUE, TRUE); 
		} else {
			str = g_strdup_printf ("%s\n%s",
					       _("Select your preferred service."),
					       _("This will be used to configure your account details."));

			gtk_widget_show (window->scrolledwindow_service);
			
			gtk_label_set_markup (GTK_LABEL (window->label_service_response), str);
			g_free (str);
		}
	}

	if (item && 
	    !timeout &&
	    gossip_transport_disco_item_has_feature (item, "jabber:iq:register") &&
	    gossip_transport_disco_item_has_category (item, "gateway") &&
	    gossip_transport_disco_item_has_type (item, window->disco_type)) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 
				    COL_DISCO_SERVICE_NAME, NULL,
				    COL_DISCO_SERVICE_JID, gossip_jid_get_full (jid),
				    COL_DISCO_SERVICE_CAN_REGISTER, TRUE,
				    -1);
	}
}

static void
transport_add_window_requirements (GossipTransportAddWindow *window, GossipJID *jid)
{
	GossipJabber *jabber;

	if (strcmp (gossip_jid_get_full (jid), "") == 0) {
		g_print ("here");
	}

	d(g_print ("asking for disco registration requirements for protocol:'%s' with service jid:'%s'\n", 
		   window->disco_type, 
		   gossip_jid_get_full (jid)));

	/* set up widgets */
	gtk_label_set_text (GTK_LABEL (window->label_service), gossip_jid_get_full (jid));

	gnome_druid_set_page (GNOME_DRUID (window->druid), 
			      GNOME_DRUID_PAGE (window->page_3));

	/* send disco request */
	jabber = gossip_transport_account_list_get_jabber (window->al);

	if (window->jid) {
		gossip_jid_unref (window->jid);
	}

	window->jid = gossip_jid_ref (jid);

	gossip_transport_requirements (jabber,
				       jid,
				       (GossipTransportRequirementsFunc) transport_add_window_requirements_cb,
				       window);
}

static void
transport_add_window_requirements_cb (GossipJID                *jid,
				      const gchar              *key,
				      const gchar              *username,
				      const gchar              *password,
				      const gchar              *nick,
				      const gchar              *email,
				      gboolean                  require_username,
				      gboolean                  require_password,
				      gboolean                  require_nick,
				      gboolean                  require_email,
				      gboolean                  is_registered,
				      const gchar              *error_code,
				      const gchar              *error_reason,
				      GossipTransportAddWindow *window)
{
	if (error_code || error_reason) {
		gchar *str;

		str = g_strdup_printf ("<b>%s</b>\n\n%s",
				       _("Unable to Register"),
				       error_reason);
		gtk_label_set_markup (GTK_LABEL (window->label_requirements_result), str);

		g_free (str);
		return;
	}

#if 0
	/* This is strange:

	   After looking at PSI and JEP-0077, registrations are now
	   accepted without a "key", but older services still require
	   a key.  To keep backwards compatibility, we will allow
	   registration without a key.  
	*/

	if (!key) {
		gchar *str;
		
		if (username) {
			gchar *reg_str;

			reg_str = g_strdup_printf (_("The service confirmed the user '%s' is already registered."),
						   username);
							  
			str = g_strdup_printf ("<b>%s</b>\n\n%s\n\n%s\n\n%s",
					       _("Unable to Register"),
					       _("When registering a token is provided by the service to register with, "
						 "in this case, the token has not been sent."),
					       reg_str,
					       _("Please try again, or perhaps another service."));

			g_free (reg_str);
		} else {
			str = g_strdup_printf ("<b>%s</b>\n\n%s\n\n%s",
					       _("Unable to Register"),
					       _("When registering a token is provided by the service to register with, "
						 "in this case, the token has not been sent."),
					       _("Please try again, or perhaps another service."));
		}

		gtk_label_set_markup (GTK_LABEL (window->label_requirements_result), str);

		g_free (str);
		return;
	}
#endif

	gtk_widget_hide (window->label_requirements_result);
	gtk_widget_show (window->label_stub_service);
	gtk_widget_show (window->label_service);

	if (require_email) {
		gtk_widget_show (window->label_email);
		gtk_widget_show (window->entry_email);
		gtk_entry_set_text (GTK_ENTRY (window->entry_email), email ? email : "");
		gtk_widget_grab_focus (window->entry_email);
	} else {
		gtk_widget_hide (window->label_email);
		gtk_widget_hide (window->entry_email);
	}

	/* FIXME: set nick from vcard and/or gconf from Micke's new branch here: */
	if (require_nick) {
		gtk_widget_show (window->label_nickname);
		gtk_widget_show (window->entry_nickname);
		gtk_entry_set_text (GTK_ENTRY (window->entry_nickname), nick ? nick : "");
		gtk_widget_grab_focus (window->entry_nickname);
	} else {
		gtk_widget_hide (window->label_nickname);
		gtk_widget_hide (window->entry_nickname);
	}

	if (require_password) {
		gtk_widget_show (window->label_password);
		gtk_widget_show (window->entry_password);
		gtk_entry_set_text (GTK_ENTRY (window->entry_password), password ? password : "");
		gtk_widget_grab_focus (window->entry_password);
	} else {
		gtk_widget_hide (window->label_password);
		gtk_widget_hide (window->entry_password);
	}

	if (require_username) {
		gtk_widget_show (window->label_username);
		gtk_widget_show (window->entry_username);
		gtk_entry_set_text (GTK_ENTRY (window->entry_username), username ? username : "");
		gtk_widget_grab_focus (window->entry_username);
	} else {
		gtk_widget_hide (window->label_username);
		gtk_widget_hide (window->entry_username);
	}

	g_free (window->key);
	window->key = g_strdup (key);

	window->require_username = require_username;
	window->require_password = require_password;
	window->require_nick = require_nick;
	window->require_email = require_email;

	/* we are bypassing using the instructions from the service
	   because usually they are in different languages */
 	gtk_widget_show (window->label_instructions); 
}

static void
transport_add_window_register (GossipTransportAddWindow *window)
{
	GossipJabber *jabber;

	const gchar  *username;
	const gchar  *password;
	const gchar  *email;
	const gchar  *nick;

	username = gtk_entry_get_text (GTK_ENTRY (window->entry_username));
	password = gtk_entry_get_text (GTK_ENTRY (window->entry_password));
	email = gtk_entry_get_text (GTK_ENTRY (window->entry_email));
	nick = gtk_entry_get_text (GTK_ENTRY (window->entry_nickname));

	jabber = gossip_transport_account_list_get_jabber (window->al);
	gossip_jabber_subscription_allow_all (jabber);

	/* watch roster for updates */
	g_signal_connect (GOSSIP_PROTOCOL (jabber),
			  "contact-added",
			  G_CALLBACK (transport_add_window_roster_update_cb),
			  window);

	/* register transport */
	d(g_print ("requesting disco registration\n"));
 	gossip_transport_register (jabber,
				   window->jid,
				   window->key, 
				   window->require_username ? username : NULL,
				   window->require_password ? password : NULL,
				   window->require_nick ? nick : NULL,
				   window->require_email ? email : NULL,
				   (GossipTransportRegisterFunc) transport_add_window_register_cb, 
				   window); 
}

static void
transport_add_window_register_cb (GossipJID                *jid,
				  const gchar              *error_code,
				  const gchar              *error_reason,
				  GossipTransportAddWindow *window)
{
 	gchar *str; 
		
	if (error_code || error_reason) {
		gtk_widget_hide (window->progressbar_registering);

		str = g_strdup_printf ("<b>%s</b>\n\n%s",
				       _("Unable to Register"),
				       error_reason);
		gtk_label_set_markup (GTK_LABEL (window->label_register_result), str);
		g_free (str);

		gnome_druid_set_show_finish (GNOME_DRUID (window->druid), TRUE);
		gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
						   FALSE, FALSE, TRUE, TRUE); 
		return;
	} 

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (window->progressbar_registering), 0.5);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (window->progressbar_registering), 
				   _("Configuring Roster"));
	
	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   FALSE, FALSE, FALSE, TRUE); 
}

static void
transport_add_window_roster_update_cb (GossipProtocol           *protocol,
				       GossipContact            *contact,
				       GossipTransportAddWindow *window)
{
	guint id;
	
	/* if already set, remove so we always wait another 2 seconds
	   after the last update */
	if (window->roster_timeout_id) {
		g_source_remove (window->roster_timeout_id);
	}

	/* allow 2 seconds of no activity to same OK we are done here */
	id = g_timeout_add (2000, 
			    (GSourceFunc) transport_add_window_roster_timeout_cb,
			    window);
				    
	window->roster_timeout_id = id;
}

static gboolean
transport_add_window_roster_timeout_cb (GossipTransportAddWindow *window)
{
 	gchar *str; 

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (window->progressbar_registering), 1);
	gtk_widget_hide (window->progressbar_registering);
	
	/* show success */
	str = g_strdup_printf ("<b>%s</b>\n\n%s",
			       _("Registration Successful!"),
			       _("You are now able to add contacts using this transport."));

	gtk_label_set_markup (GTK_LABEL (window->label_register_result), str);
	
	g_free (str);

	/* set up buttons */
	gnome_druid_set_show_finish (GNOME_DRUID (window->druid), TRUE);
	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   FALSE, TRUE, FALSE, TRUE); 

	window->roster_timeout_id = 0;

	return FALSE;
}

static void
transport_add_window_protocol_setup (GossipTransportAddWindow *window)
{
	GtkListStore     *store;
	GtkTreeSelection *selection;

	GList            *protocols;
	GList            *l;

	store = gtk_list_store_new (COL_DISCO_TYPE_COUNT,
				    G_TYPE_POINTER,  /* object */
				    G_TYPE_BOOLEAN); /* editable */
	
	gtk_tree_view_set_model (GTK_TREE_VIEW (window->treeview_protocol), 
				 GTK_TREE_MODEL (store));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview_protocol));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect (G_OBJECT (selection), "changed", 
			  G_CALLBACK (transport_add_window_protocol_selection_changed_cb), window);


	transport_add_window_protocol_populate_columns (window);

 	protocols = gossip_transport_protocol_get_all ();

	/* populate protocols */
	for (l=protocols; l; l=l->next) {
		GossipTransportProtocol *protocol;
		GtkTreeIter              iter;

		protocol = l->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, COL_DISCO_TYPE_DATA, protocol, -1);
	}

	g_list_free (protocols);

	g_object_unref (store);
}

static void 
transport_add_window_protocol_populate_columns (GossipTransportAddWindow *window)
{
	GtkTreeViewColumn *column; 
	GtkCellRenderer   *cell;
	
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (window->treeview_protocol), FALSE);

	column = gtk_tree_view_column_new ();
	
	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, cell, 
						 (GtkTreeCellDataFunc) transport_add_window_protocol_pixbuf_cell_data_func,
						 window, 
						 NULL);

	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc) transport_add_window_protocol_name_cell_data_func,
						 window,
						 NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (window->treeview_protocol), column);
}

static void  
transport_add_window_protocol_pixbuf_cell_data_func (GtkTreeViewColumn *tree_column,
						      GtkCellRenderer   *cell,
						      GtkTreeModel      *model,
						      GtkTreeIter       *iter,
						      GossipTransportAddWindow *window)
{
	GossipTransportProtocol *protocol;
	const gchar             *disco_type;
	const gchar             *icon;
	const gchar             *stock_id = NULL;
	const gchar             *core_icon = NULL;
	GdkPixbuf               *pixbuf = NULL;
	gint                     w, h;
	gint                     size = 48;  /* default size */
	
	gtk_tree_model_get (model, iter, COL_DISCO_TYPE_DATA, &protocol, -1);

	g_return_if_fail (protocol != NULL);

	disco_type = gossip_transport_protocol_get_disco_type (protocol);
	icon = gossip_transport_protocol_get_icon (protocol);

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
		stock_id = gossip_transport_protocol_get_stock_icon (protocol);
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
transport_add_window_protocol_name_cell_data_func (GtkTreeViewColumn        *tree_column,
						    GtkCellRenderer          *cell,
						    GtkTreeModel             *model,
						    GtkTreeIter              *iter,
						    GossipTransportAddWindow *window)
{
	GossipTransportProtocol *protocol;
	gchar                   *str;

	GtkTreeSelection        *selection;
	PangoAttrList           *attr_list;
	PangoAttribute          *attr_color, *attr_style, *attr_size;
	GtkStyle                *style;
	GdkColor                 color;

	const gchar             *name;
	const gchar             *description;

	gtk_tree_model_get (model, iter, COL_DISCO_TYPE_DATA, &protocol, -1);

	g_return_if_fail (protocol != NULL);

	name = gossip_transport_protocol_get_name (protocol);
	description = gossip_transport_protocol_get_description (protocol);

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
transport_add_window_service_setup (GossipTransportAddWindow *window)
{
	GtkListStore     *store;
	GtkTreeSelection *selection;

	store = gtk_list_store_new (COL_DISCO_SERVICE_COUNT,
				    G_TYPE_STRING,   /* name */
				    G_TYPE_STRING,   /* jid */
				    G_TYPE_BOOLEAN); /* can register */
	
	gtk_tree_view_set_model (GTK_TREE_VIEW (window->treeview_service), 
				 GTK_TREE_MODEL (store));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview_service));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect (G_OBJECT (selection), "changed", 
			  G_CALLBACK (transport_add_window_service_selection_changed_cb), window);

	transport_add_window_service_populate_columns (window);

	g_object_unref (store);
}

static void 
transport_add_window_service_populate_columns (GossipTransportAddWindow *window)
{
	GtkCellRenderer   *renderer;
	GtkTreeViewColumn *column; 

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (window->treeview_service), FALSE);

	renderer = gtk_cell_renderer_text_new ();

	column = gtk_tree_view_column_new_with_attributes (_("Service"), renderer,
							   "text", COL_DISCO_SERVICE_JID,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (window->treeview_service), column);
}

static void
transport_add_window_protocol_selection_changed_cb (GtkTreeSelection         *treeselection,
						    GossipTransportAddWindow *window)
{
	gboolean next;

	next = (gtk_tree_selection_count_selected_rows (treeselection) == 1);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   FALSE, next, TRUE, TRUE); 
}

static void
transport_add_window_service_selection_changed_cb (GtkTreeSelection         *treeselection,
						   GossipTransportAddWindow *window)
{
	gboolean next;

	next = transport_add_window_check_service_valid (window);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   FALSE, next, TRUE, TRUE); 
}

static void
transport_add_window_prepare_page_1 (GnomeDruidPage           *page,
				     GnomeDruid               *druid, 
				     GossipTransportAddWindow *window)
{
	GtkTreeView  *view;
	GtkListStore *store;

	gtk_widget_grab_focus (window->treeview_protocol);

	/* clear the service list */
	view = GTK_TREE_VIEW (window->treeview_service);
	store = GTK_LIST_STORE (gtk_tree_view_get_model (view));
	gtk_list_store_clear (store);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   FALSE, FALSE, TRUE, TRUE); 
}

static void
transport_add_window_prepare_page_2 (GnomeDruidPage           *page,
				     GnomeDruid               *druid, 
				     GossipTransportAddWindow *window)
{
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	gchar            *str;

	/* tell the user what is happening */
	str = g_strdup_printf ("%s",
			       _("Checking your local service first..."));
	gtk_label_set_markup (GTK_LABEL (window->label_service_response), str);
	g_free (str);

	/* set up widgets */
	gtk_widget_show (window->label_service_response);

	gtk_widget_show (window->progressbar_searching);
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (window->progressbar_searching), 0);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   FALSE, FALSE, TRUE, TRUE); 

	/* get selected item to know which protocol to configure */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview_protocol));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		GossipTransportProtocol *protocol;

		gtk_tree_model_get (model, &iter, 
				    COL_DISCO_TYPE_DATA, &protocol,
				    -1);

		g_return_if_fail (protocol != NULL);

		window->disco_type = gossip_transport_protocol_get_disco_type (protocol);
	} 
	
	/* make sure there are no discos left around first */
	transport_add_window_disco_cleanup (window); 

	/* look up local service availability first */
	transport_add_window_check_local (window);
}

static void
transport_add_window_prepare_page_3 (GnomeDruidPage           *page,
				     GnomeDruid               *druid, 
				     GossipTransportAddWindow *window)
{
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	
	GossipJID        *jid = NULL;
	const gchar      *jid_str;

	/* clean up */
	if (window->progress_timeout_id) {
		g_source_remove (window->progress_timeout_id);
		window->progress_timeout_id = 0;
	}

	/* set up widgets */
	gtk_label_set_markup (GTK_LABEL (window->label_requirements_result), 
			      "<b>Requested service requirements, please wait...</b>");

	gtk_widget_show (window->label_requirements_result);
	gtk_widget_hide (window->label_instructions);
	gtk_widget_hide (window->label_service);
	gtk_widget_hide (window->label_stub_service);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   FALSE, FALSE, TRUE, TRUE); 

	/* if local, we have already done what follows */
	if (window->using_local_service) {
		return;
	}

	/* if we have a prefered service, use that first */
	jid_str = gtk_entry_get_text (GTK_ENTRY (window->entry_service));
	
	if (jid_str && strlen (jid_str) > 0) {
		jid = gossip_jid_new (jid_str);
	}
	
	/* get selected item to know which service to use */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview_service));
	if (!jid && gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gchar *str;
		
		gtk_tree_model_get (model, &iter, COL_DISCO_SERVICE_JID, &str, -1);
		jid = gossip_jid_new (str);
		g_free (str);
	} 

	transport_add_window_requirements (window, jid);
	gossip_jid_unref (jid);
}

static void
transport_add_window_prepare_page_4 (GnomeDruidPage           *page,
				     GnomeDruid               *druid, 
				     GossipTransportAddWindow *window)
{
	gtk_label_set_markup (GTK_LABEL (window->label_register_result),
			      _("<b>Configuring your new service...</b>\n"
				"This will take a few moments, please wait."));


	gtk_widget_show (window->progressbar_registering);

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (window->progressbar_registering), 0);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (window->progressbar_registering), 
				   _("Registering With Service"));

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   FALSE, FALSE, TRUE, TRUE); 

	transport_add_window_register (window);
}

static void
transport_add_window_finish_page_4 (GnomeDruidPage           *page,
				    GtkWidget                *widget, 
				    GossipTransportAddWindow *window)
{
	gtk_widget_destroy (window->window);
}

static void
transport_add_window_druid_stop (GtkWidget                *widget, 
				 GossipTransportAddWindow *window)
{
	gtk_widget_destroy (window->window);
}

static void 
transport_add_window_entry_details_changed (GtkEntry                 *entry,
					    GossipTransportAddWindow *window)
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
						   FALSE, FALSE, TRUE, TRUE); 
	} else {
		gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
						   FALSE, TRUE, TRUE, TRUE); 
	}		
}

static void 
transport_add_window_entry_service_changed (GtkEntry                 *entry,
					   GossipTransportAddWindow *window)
{
	gboolean next;

	next = transport_add_window_check_service_valid (window);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid), 
					   FALSE, next, TRUE, TRUE); 
}

static gboolean
transport_add_window_check_service_valid (GossipTransportAddWindow *window)
{
	const gchar      *str;
	gint              len;

	GtkTreeView      *view;
	GtkTreeSelection *selection;

	gboolean          recommended_service = FALSE;
	gboolean          prefered_service = FALSE;

	str = gtk_entry_get_text (GTK_ENTRY (window->entry_service));

	if (str && (len = strlen (str)) > 0) {
		const gchar *dot;
		const gchar *at;

		at = strchr (str, '@');
		dot = strchr (str, '.');
		if (!at && 
		    dot != str + 1 && 
		    dot != str + len - 1 && 
		    dot != str + len - 2) {
			prefered_service = TRUE;
		}
	}

	view = GTK_TREE_VIEW (window->treeview_service);
	selection = gtk_tree_view_get_selection (view);

	recommended_service = gtk_tree_selection_get_selected (selection, NULL, NULL);

	return (prefered_service || recommended_service);
}

static gboolean
transport_add_window_protocol_model_foreach_cb (GtkTreeModel *model,
						GtkTreePath  *path,
						GtkTreeIter  *iter,
						gpointer      data)
{
	GossipTransportProtocol *protocol;

	gtk_tree_model_get (model, iter, COL_DISCO_TYPE_DATA, &protocol, -1);
	gossip_transport_protocol_unref (protocol);
	
	return FALSE;
}

static void
transport_add_window_destroy (GtkWidget                *widget, 
			      GossipTransportAddWindow *window)
{
	GossipJabber *jabber;
	GtkTreeModel *model;

	current_window = NULL;

	transport_add_window_disco_cleanup (window);

	if (window->progress_timeout_id) {
		g_source_remove (window->progress_timeout_id);
		window->progress_timeout_id = 0;
	}

	if (window->roster_timeout_id) {
		g_source_remove (window->roster_timeout_id);
		window->roster_timeout_id = 0;
	}

	if (window->jid) {
		gossip_transport_requirements_cancel (window->jid);
		gossip_transport_register_cancel (window->jid);
		gossip_jid_unref (window->jid);
		window->jid = NULL;
	}

	jabber = gossip_transport_account_list_get_jabber (window->al);
	gossip_jabber_subscription_disallow_all (jabber);

	g_signal_handlers_disconnect_by_func (GOSSIP_PROTOCOL (jabber),
					      transport_add_window_roster_update_cb,
					      window);
	
	g_free (window->key);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (window->treeview_protocol));
	gtk_tree_model_foreach (model, transport_add_window_protocol_model_foreach_cb, NULL);

 	g_free (window); 
}
