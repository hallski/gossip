/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio AB
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

#include <libgossip/gossip-utils.h>

#include "gossip-ui-utils.h"
#include "gossip-app.h"
#include "gossip-add-contact.h"


typedef struct {
	GtkWidget   *dialog;
	GtkWidget   *druid;
	
	/* Page one */
	GtkWidget   *one_page;
	GtkWidget   *one_accounts_vbox;
	GtkWidget   *one_accounts_combobox;
	GtkWidget   *one_system_vbox;
	GtkWidget   *one_system_combobox;
	GtkWidget   *one_id_label;
	GtkWidget   *one_id_entry;
	GtkWidget   *one_search_button;
	GtkWidget   *one_example_label;

	/* Page two */
	GtkWidget   *two_page;
	GtkWidget   *two_vcard_label;
	GtkWidget   *two_information_table;
	GtkWidget   *two_id_label;
	GtkWidget   *two_name_label;
	GtkWidget   *two_email_label;
	GtkWidget   *two_country_label;
	GtkWidget   *two_id_stub_label;
	GtkWidget   *two_name_stub_label;
	GtkWidget   *two_email_stub_label;
	GtkWidget   *two_country_stub_label;
	GtkWidget   *two_nick_entry;
	GtkWidget   *two_group_combo;
	GtkWidget   *two_group_entry;
	
	GtkWidget   *last_page;
	GtkWidget   *last_label;

	GCompletion *group_completion;
	guint        idle_complete;
} GossipAddContact;


typedef struct {
	GossipAccount *account;
	GtkComboBox   *combobox;
} SetAccountData;


enum {
	COL_NAME,
	NUM_OF_COLS
};


enum {
	COL_SYS_IMAGE,
	COL_SYS_TEXT,
	COL_SYS_ACCOUNT,
	COL_SYS_PROTOCOL,
	COL_SYS_COUNT
};


enum {
	COL_ACCOUNT_IMAGE,
	COL_ACCOUNT_TEXT,
	COL_ACCOUNT_POINTER,
	COL_ACCOUNT_COUNT
};


static void           add_contact_setup_accounts               (GList            *accounts,
								GossipAddContact *dialog);
static void           add_contact_setup_systems                (GList            *accounts,
								GossipAddContact *dialog);
static GossipAccount *add_contact_get_selected_account         (GossipAddContact *dialog);
static void           add_contact_set_selected_account         (GossipAddContact *dialog,
								GossipAccount    *account);
static gboolean       add_contact_set_selected_account_foreach (GtkTreeModel     *model,
								GtkTreePath      *path,
								GtkTreeIter      *iter,
								SetAccountData   *data);
static gboolean       add_contact_complete_group_idle          (GossipAddContact *dialog);
static void           add_contact_vcard_handler                (GossipResult      result,
								GossipVCard      *vcard,
								GossipAddContact *contact);
static void           add_contact_1_prepare                    (GnomeDruidPage   *page,
								GnomeDruid       *druid,
								GossipAddContact *dialog);
static void           add_contact_2_prepare                    (GnomeDruidPage   *page,
								GnomeDruid       *druid,
								GossipAddContact *dialog);
static void           add_contact_last_prepare                 (GnomeDruidPage   *page,
								GnomeDruid       *druid,
								GossipAddContact *dialog);
static void           add_contact_last_finished                (GnomeDruidPage   *page,
								GnomeDruid       *druid,
								GossipAddContact *dialog);
static void           add_contact_1_id_entry_changed           (GtkEntry         *entry,
								GossipAddContact *dialog);
static void           add_contact_1_system_combobox_changed    (GtkComboBox      *combo_box,
								GossipAddContact *dialog);
static void           add_contact_2_nick_entry_changed         (GtkEntry         *entry,
								GossipAddContact *dialog);
static gboolean       add_contact_2_nick_entry_key_pressed     (GtkWidget        *entry,
								GdkEvent         *event,
								GossipAddContact *dialog);
static void           add_contact_2_group_entry_text_inserted  (GtkEntry         *entry,
								const gchar      *text,
								gint              length,
								gint             *position,
								GossipAddContact *dialog);
static gboolean       add_contact_account_foreach              (GtkTreeModel     *model,
								GtkTreePath      *path,
								GtkTreeIter      *iter,
								gpointer          user_data);
static void           add_contact_destroy                      (GtkWidget        *unused,
								GossipAddContact *dialog);
static void           add_contact_cancel                       (GtkWidget        *unused,
								GossipAddContact *dialog);


static void
add_contact_setup_accounts (GList            *accounts,
			    GossipAddContact *dialog)
{
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

	/* set up combo box with new store */
	combo_box = GTK_COMBO_BOX (dialog->one_accounts_combobox);

  	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo_box));  

	store = gtk_list_store_new (COL_ACCOUNT_COUNT,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_STRING,    /* name */
				    G_TYPE_POINTER);    

	gtk_combo_box_set_model (GTK_COMBO_BOX (dialog->one_accounts_combobox), 
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

		account = l->data;

		error = NULL; 
		pixbuf = NULL;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 
				    COL_ACCOUNT_TEXT, gossip_account_get_name (account), 
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
							   size,      /* size */
							   0,         /* flags */
							   &error);

			if (!pixbuf) {
			g_warning ("could not load stock icon: %s", icon_id);
				continue;
			}

		
 		gtk_list_store_set (store, &iter, COL_ACCOUNT_IMAGE, pixbuf, -1); 
		g_object_unref (pixbuf);
	}
	
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"pixbuf", COL_ACCOUNT_IMAGE,
							 NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"text", COL_ACCOUNT_TEXT,
					NULL);

	g_object_unref (store);
}

static void
add_contact_setup_systems (GList            *accounts,
			   GossipAddContact *dialog)
{
	GtkListStore    *store;
	GtkTreeIter      iter;
	GtkCellRenderer *renderer;
	GtkComboBox     *combo_box;

	gint             w, h;
	gint             size = 24;  /* default size */

	GError          *error = NULL;
	GtkIconTheme    *theme;

	GdkPixbuf       *pixbuf;

	/* set up combo box with new store */
	combo_box = GTK_COMBO_BOX (dialog->one_system_combobox);

  	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo_box));  

	store = gtk_list_store_new (COL_SYS_COUNT,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_STRING,    /* name */
				    G_TYPE_POINTER,   /* account */
				    G_TYPE_POINTER);  /* protocol */

	gtk_combo_box_set_model (GTK_COMBO_BOX (dialog->one_system_combobox), 
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
		
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 
			    COL_SYS_IMAGE, pixbuf, 
			    COL_SYS_TEXT, _("Jabber"),
			    COL_SYS_ACCOUNT, NULL,
			    COL_SYS_PROTOCOL, NULL,
			    -1);

	gtk_combo_box_set_active_iter (combo_box, &iter);
	
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"pixbuf", COL_SYS_IMAGE,
					NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"text", COL_SYS_TEXT,
					NULL);

	g_object_unref (store);
}

static GossipAccount *
add_contact_get_selected_account (GossipAddContact *dialog) 
{
	GossipAccount *account;
	GtkTreeModel  *model;
	GtkTreeIter    iter;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->one_accounts_combobox));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->one_accounts_combobox), &iter);

	gtk_tree_model_get (model, &iter, COL_ACCOUNT_POINTER, &account, -1);

	return account;
}

static void
add_contact_set_selected_account (GossipAddContact *dialog,
				  GossipAccount    *account) 
{
	GtkComboBox    *combobox;
	GtkTreeModel   *model;
	GtkTreeIter     iter;

	SetAccountData *data;

	combobox = GTK_COMBO_BOX (dialog->one_accounts_combobox);
	model = gtk_combo_box_get_model (combobox);
	gtk_combo_box_get_active_iter (combobox, &iter);

	data = g_new0 (SetAccountData, 1);

	data->account = g_object_ref (account);
	data->combobox = g_object_ref (combobox);

	gtk_tree_model_foreach (model, 
				(GtkTreeModelForeachFunc) add_contact_set_selected_account_foreach, 
				data);
	
	g_object_unref (data->account);
	g_object_unref (data->combobox);

	g_free (data);
}

static gboolean
add_contact_set_selected_account_foreach (GtkTreeModel   *model,
					  GtkTreePath    *path,
					  GtkTreeIter    *iter,
					  SetAccountData *data)
{
	GossipAccount *account1, *account2;

	account1 = GOSSIP_ACCOUNT (data->account);
	gtk_tree_model_get (model, iter, COL_ACCOUNT_POINTER, &account2, -1);

	if (gossip_account_equal (account1, account2)) {
		GtkComboBox *combobox;

		combobox = GTK_COMBO_BOX (data->combobox);
		gtk_combo_box_set_active_iter (combobox, iter);
		return TRUE;
	}
		
	return FALSE;
}

static gboolean 
add_contact_complete_group_idle (GossipAddContact *dialog)
{
	GtkEntry    *entry = GTK_ENTRY (dialog->two_group_entry);
	const gchar *prefix;
	gchar       *new_prefix;
	gint         text_len;

	prefix = gtk_entry_get_text (entry);
	text_len = strlen (prefix);

	g_completion_complete (dialog->group_completion, 
			       (gchar *)prefix, 
			       &new_prefix);

	if (new_prefix) {
		g_signal_handlers_block_by_func (entry,
						 add_contact_2_group_entry_text_inserted,
						 dialog);
		
  		gtk_entry_set_text (entry, new_prefix); 
					  
		g_signal_handlers_unblock_by_func (entry, 
						   add_contact_2_group_entry_text_inserted, 
						   dialog);

		gtk_editable_set_position (GTK_EDITABLE (entry), text_len);
		gtk_editable_select_region (GTK_EDITABLE (entry),
					    text_len, -1);
		g_free (new_prefix);
	}

	dialog->idle_complete = 0;
	return FALSE;
}


static void
add_contact_vcard_handler (GossipResult       result,
			   GossipVCard       *vcard,
			   GossipAddContact  *dialog)
{
	gchar       *text;

	const gchar *str = NULL;
	const gchar *no_info = _("No information is available for this contact.");

        if (result != GOSSIP_RESULT_OK) {
		text = g_strdup_printf ("<b>%s</b>", no_info);
		gtk_label_set_markup (GTK_LABEL (dialog->two_vcard_label), text);
		g_free (text);
                return;
        }

	/* name */
	str = gossip_vcard_get_name (vcard);
	if (str && g_utf8_strlen (str, -1) > 0) {
		gtk_widget_show (dialog->two_name_label);
		gtk_widget_show (dialog->two_name_stub_label);
		gtk_label_set_text (GTK_LABEL (dialog->two_name_label), str);
	} else {
		gtk_widget_hide (dialog->two_name_label);
		gtk_widget_hide (dialog->two_name_stub_label);
		gtk_label_set_text (GTK_LABEL (dialog->two_name_label), "");	
	}
	
	/* email */
	str = gossip_vcard_get_email (vcard);
	if (str && g_utf8_strlen (str, -1) > 0) {
		gtk_widget_show (dialog->two_email_label);
		gtk_widget_show (dialog->two_email_stub_label);
		gtk_label_set_text (GTK_LABEL (dialog->two_email_label), str);
	} else {
		gtk_widget_hide (dialog->two_email_label);
		gtk_widget_hide (dialog->two_email_stub_label);
		gtk_label_set_text (GTK_LABEL (dialog->two_email_label), "");	
	}

	/* country */
	str = gossip_vcard_get_country (vcard);
	if (str && g_utf8_strlen (str, -1) > 0) {
		gtk_widget_show (dialog->two_country_label);
		gtk_widget_show (dialog->two_country_stub_label);
		gtk_label_set_text (GTK_LABEL (dialog->two_country_label), str);
	} else {
		gtk_widget_hide (dialog->two_country_label);
		gtk_widget_hide (dialog->two_country_stub_label);
		gtk_label_set_text (GTK_LABEL (dialog->two_country_label), "");	
	}

	if (!gossip_vcard_get_name (vcard) &&
	    !gossip_vcard_get_email (vcard) && 
	    !gossip_vcard_get_country (vcard)) {
		text = g_strdup_printf ("<b>%s</b>", no_info);
		gtk_label_set_markup (GTK_LABEL (dialog->two_vcard_label), text);
		g_free (text);

		gtk_widget_show(dialog->two_vcard_label);
		gtk_widget_hide(dialog->two_information_table);
	} else {
		gtk_widget_hide(dialog->two_vcard_label);
		gtk_widget_show(dialog->two_information_table);
	}
}

static void
add_contact_1_prepare (GnomeDruidPage   *page, 
		       GnomeDruid       *druid, 
		       GossipAddContact *dialog)
{
        gboolean     valid = FALSE;
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (dialog->one_id_entry));
        if (strcmp (str, "") != 0) {
                valid = TRUE;
        }

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (dialog->druid),
					   FALSE, valid, TRUE, FALSE);
		
	gtk_widget_grab_focus (dialog->one_id_entry);
}

static void
add_contact_2_prepare (GnomeDruidPage   *page,
		       GnomeDruid       *druid, 
		       GossipAddContact *dialog)
{
        GossipContact           *contact;
	GossipAccount           *account;

	GtkTreeModel            *model;
	GtkTreeIter              iter;

	const gchar             *id;
	GList                   *groups = NULL;
	GList                   *l;
	GList                   *group_strings = NULL;
	gint                     changed;
	gchar                   *str;
	
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->one_system_combobox));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->one_system_combobox) , &iter);

	id = gtk_entry_get_text (GTK_ENTRY (dialog->one_id_entry));
	gtk_label_set_text (GTK_LABEL (dialog->two_id_label), id);

	changed = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog->two_nick_entry),
						      "changed"));
                                        
	/* if the nick has NOT been changed */
	if (!changed) {
		gchar *username;

		username = gossip_utils_jid_str_get_part_name (id);
		
		/* set up name */
		gtk_entry_set_text (GTK_ENTRY (dialog->two_nick_entry),
				    username);
		g_free (username);
	}

	/* vcard */
	account = add_contact_get_selected_account (dialog);

	contact = gossip_contact_new (GOSSIP_CONTACT_TYPE_TEMPORARY, account);
	gossip_contact_set_id (contact, id);

	gossip_session_get_vcard (gossip_app_get_session (), 
				  account, 
				  contact, 
				  (GossipVCardCallback) add_contact_vcard_handler,
				  dialog,
				  NULL);

	str = g_strdup_printf ("<b>%s</b>",
			       _("Information requested, please wait..."));
	gtk_label_set_markup (GTK_LABEL (dialog->two_vcard_label), str);
	g_free (str);

	gtk_widget_show(dialog->two_vcard_label);
	gtk_widget_hide(dialog->two_information_table);
	
	groups = gossip_session_get_groups (gossip_app_get_session ());
	g_completion_clear_items (dialog->group_completion);

	for (l = groups; l; l = l->next) {
		group_strings = g_list_prepend (group_strings, 
						(gchar *) l->data);
	}

	group_strings = g_list_sort (group_strings, (GCompareFunc) strcmp);

	if (group_strings) {
		gtk_combo_set_popdown_strings (GTK_COMBO (dialog->two_group_combo),
					       group_strings);
		g_completion_add_items (dialog->group_completion, group_strings);
	}

	gtk_entry_set_text (GTK_ENTRY (dialog->two_group_entry), "");

	/* set focus and buttons up */
	gtk_widget_grab_focus (dialog->two_nick_entry);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (dialog->druid),
					   TRUE,
					   TRUE,
					   TRUE,
					   FALSE);

	/* FIXME: check Jabber ID, if not valid show dialog. */
}

static void
add_contact_last_prepare (GnomeDruidPage   *page,
			  GnomeDruid       *druid,
			  GossipAddContact *dialog)
{
	const gchar *nick;
	gchar       *str;

  	gnome_druid_set_show_finish (GNOME_DRUID (dialog->druid), TRUE);
	
	nick = gtk_entry_get_text (GTK_ENTRY (dialog->two_nick_entry));
	str = g_strdup_printf (_("%s will be added to your contact list."),
			       nick);
	
	gtk_label_set_text (GTK_LABEL (dialog->last_label), str);
	g_free (str);
}

static void
add_contact_last_finished (GnomeDruidPage   *page,
			   GnomeDruid       *druid,
			   GossipAddContact *dialog)
{
	GossipAccount *account;
	const gchar   *id;
        const gchar   *name;
 	const gchar   *group;
	const gchar *message;

	message = _("I would like to add you to my contact list.");

	account = add_contact_get_selected_account (dialog);

	id = gtk_label_get_text (GTK_LABEL (dialog->two_id_label));
        name = gtk_entry_get_text (GTK_ENTRY (dialog->two_nick_entry));
        group = gtk_entry_get_text (GTK_ENTRY (dialog->two_group_entry));
        
        gossip_session_add_contact (gossip_app_get_session (),
				    account,
                                    id, name, group, message);

	gtk_widget_destroy (dialog->dialog);
}

static void
add_contact_1_id_entry_changed (GtkEntry         *entry, 
				GossipAddContact *dialog)
{
	gboolean valid;

	/* check id ok */
	valid = TRUE;
        gnome_druid_set_buttons_sensitive (GNOME_DRUID (dialog->druid),
                                           FALSE, valid, TRUE, FALSE);
}

static void
add_contact_1_system_combobox_changed (GtkComboBox      *combo_box,
				       GossipAddContact *dialog)
{
	GtkTreeModel            *model;
	GtkTreeIter              iter;

	gchar                   *name;

	gchar                   *str;
	gchar                   *example;

	gboolean                 valid;

	model = gtk_combo_box_get_model (combo_box);
	gtk_combo_box_get_active_iter (combo_box, &iter);

	gtk_tree_model_get (model, &iter, 
			    COL_SYS_TEXT, &name, 
		/*	    COL_SYS_PROTOCOL, &protocol, */ /* TRANSPORTS */
			    -1);

	str = g_strdup_printf (_("%s ID of new contact:"), name);
	gtk_label_set_text (GTK_LABEL (dialog->one_id_label), str);
	g_free (str);

		/* must be a jabber system selected */
		example = g_strdup_printf (_("Example: %s"), "user@jabber.org");

	str = g_strdup_printf ("<span size=\"smaller\">%s</span>", example);
	gtk_label_set_markup (GTK_LABEL (dialog->one_example_label), str);
	g_free (str);

	g_free (example);
	g_free (name);

	/* check entry is valid or not */
	valid = TRUE;

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (dialog->druid),
					   TRUE,
					   valid,
					   TRUE,
					   FALSE);
}

static void
add_contact_2_nick_entry_changed (GtkEntry         *entry,
				  GossipAddContact *dialog)
{
	const gchar     *str;
	gboolean         forward_sensitive = TRUE;
	
	str = gtk_entry_get_text (GTK_ENTRY (entry));;
	if (!str || strcmp (str, "") == 0) {
		forward_sensitive = FALSE;
	}

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (dialog->druid),
					   TRUE,
					   forward_sensitive,
					   TRUE,
					   FALSE);
}
 
static gboolean
add_contact_2_nick_entry_key_pressed (GtkWidget        *entry,
				      GdkEvent         *event,
				      GossipAddContact *dialog)
{
	g_object_set_data (G_OBJECT (entry), 
			   "changed", GINT_TO_POINTER (1));

	return FALSE;
}

static void
add_contact_2_group_entry_text_inserted (GtkEntry         *entry, 
					 const gchar      *text,
					 gint              length,
					 gint             *position,
					 GossipAddContact *dialog)
{
	if (!dialog->idle_complete) {
		dialog->idle_complete = 
			g_idle_add ((GSourceFunc) add_contact_complete_group_idle,
				    dialog);
	}
}

static gboolean
add_contact_account_foreach (GtkTreeModel *model,
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
add_contact_destroy (GtkWidget        *widget,
		     GossipAddContact *dialog)
{
	GtkTreeModel *model;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->one_accounts_combobox));
	gtk_tree_model_foreach (model, 
				(GtkTreeModelForeachFunc) add_contact_account_foreach, 
				NULL);

	g_free (dialog);
}

static void
add_contact_cancel (GtkWidget        *widget, 
		    GossipAddContact *dialog)
{
	gtk_widget_destroy (dialog->dialog);
}

void
gossip_add_contact_show (GossipContact *contact)
{
	GossipSession    *session;
	GossipAddContact *dialog;
	GladeXML         *glade;
	GList            *accounts;
	GnomeDruid       *druid;

	dialog = g_new0 (GossipAddContact, 1);
	
	dialog->group_completion = g_completion_new (NULL);

	glade = gossip_glade_get_file (
		GLADEDIR "/main.glade",
		"add_contact_dialog",
		NULL,
		"add_contact_dialog", &dialog->dialog,
		"add_contact_druid", &dialog->druid,
		"1_page", &dialog->one_page,
		"1_accounts_vbox", &dialog->one_accounts_vbox,
		"1_accounts_combobox", &dialog->one_accounts_combobox,
		"1_system_vbox", &dialog->one_system_vbox,
		"1_system_combobox", &dialog->one_system_combobox,
		"1_id_label", &dialog->one_id_label,
		"1_id_entry", &dialog->one_id_entry,
		"1_search_button", &dialog->one_search_button,
		"1_example_label", &dialog->one_example_label,
		"2_page", &dialog->two_page,
		"2_vcard_label", &dialog->two_vcard_label,
		"2_information_table", &dialog->two_information_table,
		"2_id_label", &dialog->two_id_label,
		"2_name_label", &dialog->two_name_label,
		"2_email_label", &dialog->two_email_label,
		"2_country_label", &dialog->two_country_label,
		"2_id_stub_label", &dialog->two_id_stub_label,
		"2_name_stub_label", &dialog->two_name_stub_label,
		"2_email_stub_label", &dialog->two_email_stub_label,
		"2_country_stub_label", &dialog->two_country_stub_label,
		"2_nick_entry", &dialog->two_nick_entry,
		"2_group_combo", &dialog->two_group_combo,
		"2_group_entry", &dialog->two_group_entry,
		"last_page", &dialog->last_page,
		"last_label", &dialog->last_label,
		NULL);
	
	gossip_glade_connect (
		glade, dialog,
		"add_contact_dialog", "destroy", add_contact_destroy,
		"add_contact_druid", "cancel", add_contact_cancel,
		"1_id_entry", "changed", add_contact_1_id_entry_changed,
		"1_system_combobox", "changed", add_contact_1_system_combobox_changed,
		"2_nick_entry", "changed", add_contact_2_nick_entry_changed,
		"2_nick_entry", "key_press_event", add_contact_2_nick_entry_key_pressed,
		"last_page", "finish", add_contact_last_finished,
		NULL);
	
	g_object_unref (glade);
	
	g_signal_connect_after (dialog->one_page, "prepare",
				G_CALLBACK (add_contact_1_prepare),
				dialog);
	g_signal_connect_after (dialog->two_page, "prepare",
				G_CALLBACK (add_contact_2_prepare),
				dialog);
	g_signal_connect_after (dialog->last_page, "prepare",
				G_CALLBACK (add_contact_last_prepare),
				dialog);
	g_signal_connect_after (dialog->two_group_entry, "insert_text",
				G_CALLBACK (add_contact_2_group_entry_text_inserted),
				dialog);

	g_object_set_data (G_OBJECT (dialog->two_nick_entry), 
			   "changed", GINT_TO_POINTER (0));

	druid = GNOME_DRUID (dialog->druid);
	
	g_object_set (G_OBJECT (druid->next), 
		      "can-default", TRUE,
		      "has-default", TRUE,
		      NULL);
	
	session = gossip_app_get_session ();
	accounts = gossip_session_get_accounts (session);

	/* populate accounts */
	add_contact_setup_accounts (accounts, dialog);

	/* select first */
	gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->one_accounts_combobox), 0);
		
	if (g_list_length (accounts) > 1) {
		gtk_widget_show (dialog->one_accounts_vbox);
	} else {
		/* show no accounts combo box */
		gtk_widget_hide (dialog->one_accounts_vbox);
	}

	/* populate systems */
	accounts = NULL;
	add_contact_setup_systems (accounts, dialog);

	/* do we skip a stage? */
	if (contact) {
		GossipAccount *account;

		account = gossip_contact_get_account (contact);
		add_contact_set_selected_account (dialog, account);

                gtk_entry_set_text (GTK_ENTRY (dialog->one_id_entry), 
				    gossip_contact_get_id (contact));
		gnome_druid_set_page (GNOME_DRUID (dialog->druid),
				      GNOME_DRUID_PAGE (dialog->two_page));
		add_contact_2_prepare (GNOME_DRUID_PAGE (dialog->two_page),
				       GNOME_DRUID (dialog->druid), 
				       dialog);
	} else {
		add_contact_1_prepare (GNOME_DRUID_PAGE (dialog->one_page), 
				       GNOME_DRUID (dialog->druid),
				       dialog);
	}


	gtk_widget_show (dialog->dialog);
}
