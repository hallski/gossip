/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Martyn Russell <ginxd@btopenworld.com>
 * Copyright (C) 2004 Imendio AB
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
#include <glade/glade.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-href.h>
#include <loudmouth/loudmouth.h>
#include <unistd.h>

#include <libgossip/gossip-session.h>
#include <libgossip/gossip-protocol.h>
#include <libgossip/gossip-vcard.h>

#include "gossip-app.h"
#include "gossip-vcard-dialog.h"

#define d(x)

#define STRING_EMPTY(x) ((x) == NULL || (x)[0] == '\0')

#define VCARD_TIMEOUT 20000


struct _GossipVCardDialog {
	GtkWidget *dialog;

	GtkWidget *table_account;
	GtkWidget *label_account;
	GtkWidget *combobox_account;

	GtkWidget *table_vcard;

	GtkWidget *label_name;
	GtkWidget *label_nickname;
	GtkWidget *label_web_site;
	GtkWidget *label_email;
	GtkWidget *label_description;

	GtkWidget *entry_name;
	GtkWidget *entry_nickname;
	GtkWidget *entry_web_site;
	GtkWidget *entry_email;

	GtkWidget *textview_description;

	GtkWidget *vbox_waiting;
	GtkWidget *progressbar_waiting;

	GtkWidget *button_ok;

	guint      wait_id;
	guint      pulse_id;
	guint      timeout_id; 

	gboolean   requesting_vcard;

	gint       last_account_selected;
};


typedef struct _GossipVCardDialog GossipVCardDialog;


enum {
	COL_ACCOUNT_IMAGE,
	COL_ACCOUNT_TEXT,
	COL_ACCOUNT_CONNECTED,
	COL_ACCOUNT_POINTER,
	COL_ACCOUNT_COUNT
};

static void           vcard_dialog_setup_accounts              (GList             *accounts,
								GossipVCardDialog *dialog);
static GossipAccount *vcard_dialog_get_account_selected        (GossipVCardDialog *dialog);
static gint           vcard_dialog_get_account_count           (GossipVCardDialog *dialog);
static void           vcard_dialog_set_account_to_last         (GossipVCardDialog *dialog);
static void           vcard_dialog_lookup_start                (GossipVCardDialog *dialog);
static void           vcard_dialog_lookup_stop                 (GossipVCardDialog *dialog);
static void           vcard_dialog_protocol_disconnected_cb    (GossipSession     *session,
								GossipProtocol    *protocol,
								GossipVCardDialog *dialog);
static void           vcard_dialog_protocol_disconnected_cb    (GossipSession     *session,
								GossipProtocol    *protocol,
								GossipVCardDialog *dialog);
static void           vcard_dialog_get_vcard_cb                (GossipResult       result,
								GossipVCard       *vcard,
								GossipVCardDialog *dialog);
static void           vcard_dialog_set_vcard                   (GossipVCardDialog *dialog);
static void           vcard_dialog_set_vcard_cb                (GossipResult       result,
								GossipVCardDialog *dialog);
static gboolean       vcard_dialog_progress_pulse_cb           (GtkWidget         *progressbar);
static gboolean       vcard_dialog_wait_cb                     (GossipVCardDialog *dialog);
static gboolean       vcard_dialog_timeout_cb                  (GossipVCardDialog *dialog);
static gboolean       vcard_dialog_account_foreach             (GtkTreeModel      *model,
								GtkTreePath       *path,
								GtkTreeIter       *iter,
								gpointer           user_data);
static void           vcard_dialog_combobox_account_changed_cb (GtkWidget         *combobox,
								GossipVCardDialog *dialog);
static void           vcard_dialog_response_cb                 (GtkDialog         *widget,
								gint               response,
								GossipVCardDialog *dialog);
static void           vcard_dialog_destroy_cb                  (GtkWidget         *widget,
								GossipVCardDialog *dialog);


static void
vcard_dialog_setup_accounts (GList             *accounts,
			     GossipVCardDialog *dialog)
{
	GossipSession   *session;

	GtkListStore    *store;
	GtkTreeIter      iter;
	GtkCellRenderer *renderer;
	GtkComboBox     *combo_box;

	GList           *l;

	gint             w, h;
	gint             size = 24;  /* default size */

	GError          *error = NULL;
	GtkIconTheme    *theme;

	GdkPixbuf       *pixbuf;

	gboolean         active_item_set = FALSE;

	session = gossip_app_get_session ();
	
	/* set up combo box with new store */
	combo_box = GTK_COMBO_BOX (dialog->combobox_account);

  	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo_box));  

	store = gtk_list_store_new (COL_ACCOUNT_COUNT,
				    GDK_TYPE_PIXBUF, 
				    G_TYPE_STRING,   /* name */
				    G_TYPE_BOOLEAN,  /* connected */
				    G_TYPE_POINTER);    

	gtk_combo_box_set_model (GTK_COMBO_BOX (dialog->combobox_account), 
				 GTK_TREE_MODEL (store));
		
	/* get theme and size details */
	theme = gtk_icon_theme_get_default ();

	if (!gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h)) {
		size = 48;
	} else {
		size = (w + h) / 2; 
	}

	/* show jabber protocol */
	pixbuf = gtk_icon_theme_load_icon (theme,
					   "im-jabber", /* icon name */
					   size,        /* size */
					   0,           /* flags */
					   &error);
	if (!pixbuf) {
		g_warning ("could not load icon: %s", error->message);
		g_error_free (error);
	}

	/* populate accounts */
	for (l = accounts; l; l = l->next) {
		GossipAccount *account;
		const gchar   *icon_id = NULL;
		gboolean       is_connected;

		account = l->data;

		error = NULL; 
		pixbuf = NULL;

		is_connected = gossip_session_is_connected (session, account);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 
				    COL_ACCOUNT_TEXT, gossip_account_get_name (account), 
				    COL_ACCOUNT_CONNECTED, is_connected,
				    COL_ACCOUNT_POINTER, g_object_ref (account),
				    -1);

		switch (gossip_account_get_type (account)) {
		case GOSSIP_ACCOUNT_TYPE_JABBER:
			icon_id = "im-jabber";
			break;
		case GOSSIP_ACCOUNT_TYPE_AIM:
			icon_id = "im-aim";
			break;
		case GOSSIP_ACCOUNT_TYPE_ICQ:
			icon_id = "im-icq";
			break;
		case GOSSIP_ACCOUNT_TYPE_MSN:
			icon_id = "im-msn";
			break;
		case GOSSIP_ACCOUNT_TYPE_YAHOO:
			icon_id = "im-yahoo";
			break;
		default:
			g_assert_not_reached ();
		}

		pixbuf = gtk_icon_theme_load_icon (theme,
						   icon_id,     /* icon name */
						   size,        /* size */
						   0,           /* flags */
						   &error);

		if (!pixbuf) {
			g_warning ("could not load stock icon: %s", icon_id);
			continue;
		}				

		
 		gtk_list_store_set (store, &iter, COL_ACCOUNT_IMAGE, pixbuf, -1); 
		g_object_unref (pixbuf);

		/* set first connected account as active account */
		if (!active_item_set && is_connected) {
			active_item_set = TRUE;
			gtk_combo_box_set_active_iter (combo_box, &iter); 
		}
	}
	
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"pixbuf", COL_ACCOUNT_IMAGE,
					"sensitive", COL_ACCOUNT_CONNECTED,
					NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"text", COL_ACCOUNT_TEXT,
					"sensitive", COL_ACCOUNT_CONNECTED,
					NULL);

	g_object_unref (store);
}

static GossipAccount *
vcard_dialog_get_account_selected (GossipVCardDialog *dialog) 
{
	GossipAccount *account;
	GtkTreeModel  *model;
	GtkTreeIter    iter;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->combobox_account));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->combobox_account), &iter);

	gtk_tree_model_get (model, &iter, COL_ACCOUNT_POINTER, &account, -1);

	return account;
}

static gint
vcard_dialog_get_account_count (GossipVCardDialog *dialog) 
{
	GtkTreeModel *model;
		
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->combobox_account));
        return gtk_tree_model_iter_n_children  (model, NULL);
}

static void
vcard_dialog_set_account_to_last (GossipVCardDialog *dialog) 
{
	g_signal_handlers_block_by_func (dialog->combobox_account, 
					 vcard_dialog_combobox_account_changed_cb, 
					 dialog);
		
	gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->combobox_account), 
				  dialog->last_account_selected);

	g_signal_handlers_unblock_by_func (dialog->combobox_account, 
					   vcard_dialog_combobox_account_changed_cb, 
					   dialog);
}

static void
vcard_dialog_lookup_start (GossipVCardDialog *dialog) 
{
	GossipSession *session;
	GossipAccount *account;

	/* update widgets */
	gtk_widget_set_sensitive (dialog->table_vcard, FALSE);
	gtk_widget_set_sensitive (dialog->combobox_account, FALSE);
	gtk_widget_set_sensitive (dialog->button_ok, FALSE);

	/* set up timers */
	dialog->wait_id = g_timeout_add (2000, 
					 (GSourceFunc)vcard_dialog_wait_cb,
					 dialog);

	dialog->timeout_id = g_timeout_add (VCARD_TIMEOUT, 
					    (GSourceFunc)vcard_dialog_timeout_cb,
					    dialog);

	/* get selected and look it up */
	session = gossip_app_get_session ();
	account = vcard_dialog_get_account_selected (dialog);

	/* request current vCard */
	gossip_session_get_vcard (session,
				  account,
				  NULL,
				  (GossipVCardCallback) vcard_dialog_get_vcard_cb,
				  dialog,
				  NULL);

	dialog->requesting_vcard = TRUE;
}

static void  
vcard_dialog_lookup_stop (GossipVCardDialog *dialog)
{
	dialog->requesting_vcard = FALSE;

	/* update widgets */
	gtk_widget_set_sensitive (dialog->table_vcard, TRUE);
	gtk_widget_set_sensitive (dialog->combobox_account, TRUE);
	gtk_widget_set_sensitive (dialog->button_ok, TRUE);

	gtk_widget_hide (dialog->vbox_waiting); 

	/* clean up timers */
	if (dialog->wait_id != 0) {
		g_source_remove (dialog->wait_id);
		dialog->wait_id = 0;
	}

	if (dialog->pulse_id != 0) {
		g_source_remove (dialog->pulse_id);
		dialog->pulse_id = 0;
	}

	if (dialog->timeout_id != 0) {
		g_source_remove (dialog->timeout_id);
		dialog->timeout_id = 0;
	}
}

static void
vcard_dialog_protocol_connected_cb (GossipSession     *session,
				    GossipProtocol    *protocol,
				    GossipVCardDialog *dialog)
{
	/* need account here first */
}

static void
vcard_dialog_protocol_disconnected_cb (GossipSession     *session,
				       GossipProtocol    *protocol,
				       GossipVCardDialog *dialog)
{
	/* need account here first */
}

static void
vcard_dialog_get_vcard_cb (GossipResult       result,
			   GossipVCard       *vcard,
			   GossipVCardDialog *dialog)
{
	GtkComboBox   *combobox;
	GtkTextBuffer *buffer;
	const gchar   *str;

	d(g_print ("Got a vCard response\n"));

	vcard_dialog_lookup_stop (dialog);

	if (result != GOSSIP_RESULT_OK) {
		d(g_print ("vCard result != GOSSIP_RESULT_OK\n"));
		return;
	}

	str = gossip_vcard_get_name (vcard);
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_name), STRING_EMPTY (str) ? "" : str);
		
	str = gossip_vcard_get_nickname (vcard);
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_nickname), STRING_EMPTY (str) ? "" : str);

	str = gossip_vcard_get_email (vcard);
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_email), STRING_EMPTY (str) ? "" : str);

	str = gossip_vcard_get_url (vcard);
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_web_site), STRING_EMPTY (str) ? "" : str);

	str = gossip_vcard_get_description (vcard);
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->textview_description));
	gtk_text_buffer_set_text (buffer, STRING_EMPTY (str) ? "" : str, -1);

	/* save position incase the next lookup fails */
	combobox = GTK_COMBO_BOX (dialog->combobox_account);
	dialog->last_account_selected = gtk_combo_box_get_active (combobox);
}

static void
vcard_dialog_set_vcard (GossipVCardDialog *dialog)
{
	GossipVCard   *vcard;
	GossipAccount *account;
	GError        *error = NULL;
	GtkTextBuffer *buffer;
	GtkTextIter    iter_begin, iter_end;
	gchar         *description;
	const gchar   *str;

	if (!gossip_app_is_connected ()) {
		d(g_print ("Not connected, not setting vCard\n"));
		return;
	}

	vcard = gossip_vcard_new ();

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_name));
	gossip_vcard_set_name (vcard, str);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_nickname));
	gossip_vcard_set_nickname (vcard, str);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_web_site));
	gossip_vcard_set_url (vcard, str);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_email));
	gossip_vcard_set_email (vcard, str);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->textview_description));
	gtk_text_buffer_get_bounds (buffer, &iter_begin, &iter_end);
	description = gtk_text_buffer_get_text (buffer, &iter_begin, &iter_end, FALSE);
	gossip_vcard_set_description (vcard, description);
	g_free (description);

	/* NOTE: if account is NULL, all accounts will get the same vcard */
	account = vcard_dialog_get_account_selected (dialog);

	gossip_session_set_vcard (gossip_app_get_session (),
				  account,
				  vcard, 
				  (GossipResultCallback) vcard_dialog_set_vcard_cb,
				  dialog, &error);
}

static void
vcard_dialog_set_vcard_cb (GossipResult       result, 
			   GossipVCardDialog *dialog)
{
  
	d(g_print ("Got a vCard response\n"));
  
	/* if multiple accounts, wait for the close button */
	if (vcard_dialog_get_account_count (dialog) <= 1) {
		gtk_widget_destroy (dialog->dialog);
	}
}


static gboolean 
vcard_dialog_progress_pulse_cb (GtkWidget *progressbar)
{
	g_return_val_if_fail (progressbar != NULL, FALSE);
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (progressbar));

	return TRUE;
}

static gboolean
vcard_dialog_wait_cb (GossipVCardDialog *dialog)
{
	gtk_widget_show (dialog->vbox_waiting);

	dialog->pulse_id = g_timeout_add (50, 
					  (GSourceFunc)vcard_dialog_progress_pulse_cb, 
					  dialog->progressbar_waiting);

	return FALSE;
}

static gboolean
vcard_dialog_timeout_cb (GossipVCardDialog *dialog)
{
	GtkWidget *md;

	vcard_dialog_lookup_stop (dialog);

	/* select last successfull account */
	vcard_dialog_set_account_to_last (dialog);

	/* show message dialog and the account dialog */
	md = gtk_message_dialog_new_with_markup (GTK_WINDOW (dialog->dialog),
						 GTK_DIALOG_MODAL |
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_INFO,
						 GTK_BUTTONS_CLOSE,
						 "<b>%s</b>\n\n%s",
						 _("The server does not seem to be responding."),
						 _("Try again later."));
	
	g_signal_connect_swapped (md, "response",
				  G_CALLBACK (gtk_widget_destroy), md);
	gtk_widget_show (md);
	
	return FALSE;
}

static gboolean
vcard_dialog_account_foreach (GtkTreeModel *model,
			     GtkTreePath  *path,
			     GtkTreeIter  *iter,
			     gpointer      user_data)
{
	GossipAccount *account;

	gtk_tree_model_get (model, iter, COL_ACCOUNT_POINTER, &account, -1);
	g_object_unref (account);

	return FALSE;
}

static void
vcard_dialog_combobox_account_changed_cb (GtkWidget         *combobox,
					  GossipVCardDialog *dialog)
{
	vcard_dialog_lookup_start (dialog);
}

static void
vcard_dialog_response_cb (GtkDialog         *widget, 
			  gint               response, 
			  GossipVCardDialog *dialog)
{
	GtkTreeModel *model;

	if (response == GTK_RESPONSE_OK) {
		vcard_dialog_set_vcard (dialog);
		return;
	}

	if (response == GTK_RESPONSE_CANCEL && dialog->requesting_vcard) {
		/* change widgets so they are unsensitive */
		vcard_dialog_lookup_stop (dialog);

		/* select last successfull account */
		vcard_dialog_set_account_to_last (dialog);
		return;
	}

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->combobox_account));
	gtk_tree_model_foreach (model, 
				(GtkTreeModelForeachFunc) vcard_dialog_account_foreach, 
				NULL);
	
	gtk_widget_destroy (dialog->dialog);
}

static void
vcard_dialog_destroy_cb (GtkWidget         *widget, 
			 GossipVCardDialog *dialog)
{
	GossipSession *session;

	vcard_dialog_lookup_stop (dialog);

	session = gossip_app_get_session ();

	g_signal_handlers_disconnect_by_func (session, 
					      vcard_dialog_protocol_connected_cb, 
					      dialog);
	g_signal_handlers_disconnect_by_func (session, 
					      vcard_dialog_protocol_disconnected_cb, 
					      dialog);

	g_free (dialog);
}

void
gossip_vcard_dialog_show (void)
{
	GossipSession     *session;
	GossipVCardDialog *dialog;
	GladeXML          *glade;
	GList             *accounts;
	GtkSizeGroup      *size_group;

	dialog = g_new0 (GossipVCardDialog, 1);

	glade = gossip_glade_get_file (GLADEDIR "/main.glade",
				       "vcard_dialog",
				       NULL,
				       "vcard_dialog", &dialog->dialog,
				       "table_account", &dialog->table_account,
				       "label_account", &dialog->label_account,
				       "combobox_account", &dialog->combobox_account,
				       "table_vcard", &dialog->table_vcard,
				       "label_name", &dialog->label_name,
				       "label_nickname", &dialog->label_nickname,
				       "label_web_site", &dialog->label_web_site,
				       "label_email", &dialog->label_email,
				       "label_description", &dialog->label_description,
				       "entry_name", &dialog->entry_name,
				       "entry_nickname", &dialog->entry_nickname,
				       "entry_web_site", &dialog->entry_web_site,
				       "entry_email", &dialog->entry_email,
				       "textview_description", &dialog->textview_description,
				       "vbox_waiting", &dialog->vbox_waiting,
				       "progressbar_waiting", &dialog->progressbar_waiting,
				       "button_ok", &dialog->button_ok,
				       NULL);

	gossip_glade_connect (glade, 
			      dialog,
			      "vcard_dialog", "destroy", vcard_dialog_destroy_cb,
			      "vcard_dialog", "response", vcard_dialog_response_cb,
			      "combobox_account", "changed", vcard_dialog_combobox_account_changed_cb,
			      NULL);

	g_object_unref (glade);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, dialog->label_account);
	gtk_size_group_add_widget (size_group, dialog->label_name);
	gtk_size_group_add_widget (size_group, dialog->label_nickname);
	gtk_size_group_add_widget (size_group, dialog->label_email);
	gtk_size_group_add_widget (size_group, dialog->label_web_site);
	gtk_size_group_add_widget (size_group, dialog->label_description);

	g_object_unref (size_group);

	/* sort out accounts */
	session = gossip_app_get_session ();
	accounts = gossip_session_get_accounts (session);

	/* populate */
	vcard_dialog_setup_accounts (accounts, dialog);

	/* select first */
		
	if (g_list_length (accounts) > 1) {
		gtk_widget_show (dialog->table_account);

		gtk_button_set_use_stock (GTK_BUTTON (dialog->button_ok), TRUE);
		gtk_button_set_label (GTK_BUTTON (dialog->button_ok), "gtk-apply");
	} else {
		/* show no accounts combo box */	
		gtk_widget_hide (dialog->table_account);

		gtk_button_set_use_stock (GTK_BUTTON (dialog->button_ok), TRUE);
		gtk_button_set_label (GTK_BUTTON (dialog->button_ok), "gtk-ok");
	}

	g_signal_connect (session, "protocol-connected",
			  G_CALLBACK (vcard_dialog_protocol_connected_cb),
			  dialog);

	g_signal_connect (session, "protocol-disconnected",
			  G_CALLBACK (vcard_dialog_protocol_disconnected_cb),
			  dialog);
}
