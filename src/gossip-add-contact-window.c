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

#include "gossip-account-chooser.h"
#include "gossip-add-contact-window.h"
#include "gossip-app.h"
#include "gossip-ui-utils.h"

typedef struct {
	GtkWidget   *window;
	GtkWidget   *druid;
	
	/* Page one */
	GtkWidget   *one_page;
	GtkWidget   *one_accounts_vbox;
	GtkWidget   *one_accounts_chooser;
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

static void           add_contact_window_setup_systems                (GList            *accounts,
								       GossipAddContact *window);
static gboolean       add_contact_window_complete_group_idle          (GossipAddContact *window);
static void           add_contact_window_vcard_handler                (GossipResult      result,
								       GossipVCard      *vcard,
								       GossipAddContact *contact);
static void           add_contact_window_1_prepare                    (GnomeDruidPage   *page,
								       GnomeDruid       *druid,
								       GossipAddContact *window);
static void           add_contact_window_2_prepare                    (GnomeDruidPage   *page,
								       GnomeDruid       *druid,
								       GossipAddContact *window);
static void           add_contact_window_last_prepare                 (GnomeDruidPage   *page,
								       GnomeDruid       *druid,
								       GossipAddContact *window);
static void           add_contact_window_last_finished                (GnomeDruidPage   *page,
								       GnomeDruid       *druid,
								       GossipAddContact *window);
static void           add_contact_window_1_id_entry_changed           (GtkEntry         *entry,
								       GossipAddContact *window);
static void           add_contact_window_1_system_combobox_changed    (GtkComboBox      *combo_box,
								       GossipAddContact *window);
static void           add_contact_window_2_nick_entry_changed         (GtkEntry         *entry,
								       GossipAddContact *window);
static gboolean       add_contact_window_2_nick_entry_key_pressed     (GtkWidget        *entry,
								       GdkEvent         *event,
								       GossipAddContact *window);
static void           add_contact_window_2_group_entry_text_inserted  (GtkEntry         *entry,
								       const gchar      *text,
								       gint              length,
								       gint             *position,
								       GossipAddContact *window);
static void           add_contact_window_destroy                      (GtkWidget        *unused,
								       GossipAddContact *window);
static void           add_contact_window_cancel                       (GtkWidget        *unused,
								       GossipAddContact *window);

static void
add_contact_window_setup_systems (GList            *accounts,
				  GossipAddContact *window)
{
	GtkListStore    *store;
	GtkTreeIter      iter;
	GtkCellRenderer *renderer;
	GtkComboBox     *combo_box;

	GdkPixbuf       *pixbuf;

	/* set up combo box with new store */
	combo_box = GTK_COMBO_BOX (window->one_system_combobox);

  	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo_box));  

	store = gtk_list_store_new (COL_SYS_COUNT,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_STRING,          /* name */
				    GOSSIP_TYPE_ACCOUNT,    /* account */
				    G_TYPE_POINTER);        /* protocol */

	gtk_combo_box_set_model (GTK_COMBO_BOX (window->one_system_combobox), 
				 GTK_TREE_MODEL (store));
		
	pixbuf = gossip_pixbuf_from_account_type (GOSSIP_ACCOUNT_TYPE_JABBER,
						  GTK_ICON_SIZE_MENU);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 
			    COL_SYS_IMAGE, pixbuf, 
			    COL_SYS_TEXT, _("Jabber"),
			    COL_SYS_ACCOUNT, NULL,
			    COL_SYS_PROTOCOL, NULL,
			    -1);

	g_object_unref (pixbuf);

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

static gboolean 
add_contact_window_complete_group_idle (GossipAddContact *window)
{
	GtkEntry    *entry = GTK_ENTRY (window->two_group_entry);
	const gchar *prefix;
	gchar       *new_prefix;
	gint         text_len;

	prefix = gtk_entry_get_text (entry);
	text_len = strlen (prefix);

	g_completion_complete (window->group_completion, 
			       (gchar *)prefix, 
			       &new_prefix);

	if (new_prefix) {
		g_signal_handlers_block_by_func (entry,
						 add_contact_window_2_group_entry_text_inserted,
						 window);
		
  		gtk_entry_set_text (entry, new_prefix); 
					  
		g_signal_handlers_unblock_by_func (entry, 
						   add_contact_window_2_group_entry_text_inserted, 
						   window);

		gtk_editable_set_position (GTK_EDITABLE (entry), text_len);
		gtk_editable_select_region (GTK_EDITABLE (entry),
					    text_len, -1);
		g_free (new_prefix);
	}

	window->idle_complete = 0;
	return FALSE;
}

static void
add_contact_window_vcard_handler (GossipResult       result,
				  GossipVCard       *vcard,
				  GossipAddContact  *window)
{
	gchar    *text;
	gpointer  p;
	gboolean  changed;

	const gchar *str = NULL;
	const gchar *no_info = _("No information is available for this contact.");

        if (result != GOSSIP_RESULT_OK) {
		text = g_strdup_printf ("<b>%s</b>", no_info);
		gtk_label_set_markup (GTK_LABEL (window->two_vcard_label), text);
		g_free (text);
                return;
        }

	p = g_object_get_data (G_OBJECT (window->two_nick_entry), "changed");
	changed = GPOINTER_TO_INT (p);

	/* name */
	str = gossip_vcard_get_name (vcard);
	if (str && g_utf8_strlen (str, -1) > 0) {
		gtk_widget_show (window->two_name_label);
		gtk_widget_show (window->two_name_stub_label);
		gtk_label_set_text (GTK_LABEL (window->two_name_label), str);
		
		if (!changed) {
			gtk_entry_set_text (GTK_ENTRY (window->two_nick_entry), str);
		}
	} else {
		gtk_widget_hide (window->two_name_label);
		gtk_widget_hide (window->two_name_stub_label);
		gtk_label_set_text (GTK_LABEL (window->two_name_label), "");	
	}
	
	/* email */
	str = gossip_vcard_get_email (vcard);
	if (str && g_utf8_strlen (str, -1) > 0) {
		gtk_widget_show (window->two_email_label);
		gtk_widget_show (window->two_email_stub_label);
		gtk_label_set_text (GTK_LABEL (window->two_email_label), str);
	} else {
		gtk_widget_hide (window->two_email_label);
		gtk_widget_hide (window->two_email_stub_label);
		gtk_label_set_text (GTK_LABEL (window->two_email_label), "");	
	}

	/* country */
	str = gossip_vcard_get_country (vcard);
	if (str && g_utf8_strlen (str, -1) > 0) {
		gtk_widget_show (window->two_country_label);
		gtk_widget_show (window->two_country_stub_label);
		gtk_label_set_text (GTK_LABEL (window->two_country_label), str);
	} else {
		gtk_widget_hide (window->two_country_label);
		gtk_widget_hide (window->two_country_stub_label);
		gtk_label_set_text (GTK_LABEL (window->two_country_label), "");	
	}

	if (!gossip_vcard_get_name (vcard) &&
	    !gossip_vcard_get_email (vcard) && 
	    !gossip_vcard_get_country (vcard)) {
		text = g_strdup_printf ("<b>%s</b>", no_info);
		gtk_label_set_markup (GTK_LABEL (window->two_vcard_label), text);
		g_free (text);

		gtk_widget_show(window->two_vcard_label);
		gtk_widget_hide(window->two_information_table);
	} else {
		gtk_widget_hide(window->two_vcard_label);
		gtk_widget_show(window->two_information_table);
	}
}

static void
add_contact_window_1_prepare (GnomeDruidPage   *page, 
			      GnomeDruid       *druid, 
			      GossipAddContact *window)
{
        gboolean     valid = FALSE;
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (window->one_id_entry));
        if (strcmp (str, "") != 0) {
                valid = TRUE;
        }

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid),
					   FALSE, valid, TRUE, FALSE);
		
	gtk_widget_grab_focus (window->one_id_entry);
}

static void
add_contact_window_2_prepare (GnomeDruidPage   *page,
			      GnomeDruid       *druid, 
			      GossipAddContact *window)
{
        GossipContact        *contact;
	GossipAccount        *account;
	GossipAccountChooser *account_chooser;

	GtkTreeModel         *model;
	GtkTreeIter           iter;
	
	const gchar          *id;
	GList                *groups = NULL;
	GList                *l;
	GList                *group_strings = NULL;
	gint                  changed;
	gchar                *str;
	gpointer              p;
	
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (window->one_system_combobox));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (window->one_system_combobox) , &iter);

	id = gtk_entry_get_text (GTK_ENTRY (window->one_id_entry));
	gtk_label_set_text (GTK_LABEL (window->two_id_label), id);

	p = g_object_get_data (G_OBJECT (window->two_nick_entry), "changed");
	changed = GPOINTER_TO_INT (p);
                                        
	/* If the nick has NOT been changed */
	if (!changed) {
		gtk_entry_set_text (GTK_ENTRY (window->two_nick_entry), id);
	}

	/* VCard */
	account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->one_accounts_chooser);
	account = gossip_account_chooser_get_account (account_chooser);

	contact = gossip_contact_new (GOSSIP_CONTACT_TYPE_TEMPORARY, account);
	gossip_contact_set_id (contact, id);

	gossip_session_get_vcard (gossip_app_get_session (), 
				  account, 
				  contact, 
				  (GossipVCardCallback) add_contact_window_vcard_handler,
				  window,
				  NULL);

	str = g_strdup_printf ("<b>%s</b>",
			       _("Information requested, please wait..."));
	gtk_label_set_markup (GTK_LABEL (window->two_vcard_label), str);
	g_free (str);

	gtk_widget_show(window->two_vcard_label);
	gtk_widget_hide(window->two_information_table);
	
	groups = gossip_session_get_groups (gossip_app_get_session ());
	g_completion_clear_items (window->group_completion);

	for (l = groups; l; l = l->next) {
		group_strings = g_list_prepend (group_strings, 
						(gchar *) l->data);
	}

	group_strings = g_list_sort (group_strings, (GCompareFunc) strcmp);

	if (group_strings) {
		gtk_combo_set_popdown_strings (GTK_COMBO (window->two_group_combo),
					       group_strings);
		g_completion_add_items (window->group_completion, group_strings);
	}

	gtk_entry_set_text (GTK_ENTRY (window->two_group_entry), "");

	/* Set focus and buttons up */
	gtk_widget_grab_focus (window->two_nick_entry);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid),
					   TRUE,
					   TRUE,
					   TRUE,
					   FALSE);

	/* FIXME: check Jabber ID, if not valid show window. */

	g_object_unref (account);
}

static void
add_contact_window_last_prepare (GnomeDruidPage   *page,
				 GnomeDruid       *druid,
				 GossipAddContact *window)
{
	const gchar *nick;
	gchar       *str;

  	gnome_druid_set_show_finish (GNOME_DRUID (window->druid), TRUE);
	
	nick = gtk_entry_get_text (GTK_ENTRY (window->two_nick_entry));
	str = g_strdup_printf (_("%s will be added to your contact list."),
			       nick);
	
	gtk_label_set_text (GTK_LABEL (window->last_label), str);
	g_free (str);
}

static void
add_contact_window_last_finished (GnomeDruidPage   *page,
				  GnomeDruid       *druid,
				  GossipAddContact *window)
{
	GossipAccount        *account;
	GossipAccountChooser *account_chooser;
	const gchar          *id;
        const gchar          *name;
 	const gchar          *group;
	const gchar          *message;

	message = _("I would like to add you to my contact list.");

	account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->one_accounts_chooser);
	account = gossip_account_chooser_get_account (account_chooser);

	id = gtk_label_get_text (GTK_LABEL (window->two_id_label));
        name = gtk_entry_get_text (GTK_ENTRY (window->two_nick_entry));
        group = gtk_entry_get_text (GTK_ENTRY (window->two_group_entry));
        
        gossip_session_add_contact (gossip_app_get_session (),
				    account,
				    id, name, group, message);

	gtk_widget_destroy (window->window);

	g_object_unref (account);
}

static void
add_contact_window_1_id_entry_changed (GtkEntry         *entry, 
				       GossipAddContact *window)
{
	gboolean valid;

	/* check id ok */
	valid = TRUE;
        gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid),
                                           FALSE, valid, TRUE, FALSE);
}

static void
add_contact_window_1_system_combobox_changed (GtkComboBox      *combo_box,
					      GossipAddContact *window)
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
	gtk_label_set_text (GTK_LABEL (window->one_id_label), str);
	g_free (str);

	/* must be a jabber system selected */
	example = g_strdup_printf (_("Example: %s"), "user@jabber.org");

	str = g_strdup_printf ("<span size=\"smaller\">%s</span>", example);
	gtk_label_set_markup (GTK_LABEL (window->one_example_label), str);
	g_free (str);

	g_free (example);
	g_free (name);

	/* check entry is valid or not */
	valid = TRUE;

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid),
					   TRUE,
					   valid,
					   TRUE,
					   FALSE);
}

static void
add_contact_window_2_nick_entry_changed (GtkEntry         *entry,
					 GossipAddContact *window)
{
	const gchar     *str;
	gboolean         forward_sensitive = TRUE;
	
	str = gtk_entry_get_text (GTK_ENTRY (entry));;
	if (!str || strcmp (str, "") == 0) {
		forward_sensitive = FALSE;
	}

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (window->druid),
					   TRUE,
					   forward_sensitive,
					   TRUE,
					   FALSE);
}
 
static gboolean
add_contact_window_2_nick_entry_key_pressed (GtkWidget        *entry,
					     GdkEvent         *event,
					     GossipAddContact *window)
{
	g_object_set_data (G_OBJECT (entry), 
			   "changed", GINT_TO_POINTER (1));

	return FALSE;
}

static void
add_contact_window_2_group_entry_text_inserted (GtkEntry         *entry, 
						const gchar      *text,
						gint              length,
						gint             *position,
						GossipAddContact *window)
{
	if (!window->idle_complete) {
		window->idle_complete = 
			g_idle_add ((GSourceFunc) add_contact_window_complete_group_idle,
				    window);
	}
}
			  
static void
add_contact_window_destroy (GtkWidget        *widget,
			    GossipAddContact *window)
{
	g_free (window);
}

static void
add_contact_window_cancel (GtkWidget        *widget, 
			   GossipAddContact *window)
{
	gtk_widget_destroy (window->window);
}

void
gossip_add_contact_window_show (GtkWindow     *parent,
				GossipContact *contact)
{
	GossipSession    *session;
	GossipAddContact *window;
	GladeXML         *glade;
	GList            *accounts;
	GnomeDruid       *druid;

	window = g_new0 (GossipAddContact, 1);
	
	window->group_completion = g_completion_new (NULL);

	glade = gossip_glade_get_file (
		GLADEDIR "/main.glade",
		"add_contact_window",
		NULL,
		"add_contact_window", &window->window,
		"druid", &window->druid,
		"1_page", &window->one_page,
		"1_accounts_vbox", &window->one_accounts_vbox,
		"1_system_vbox", &window->one_system_vbox,
		"1_system_combobox", &window->one_system_combobox,
		"1_id_label", &window->one_id_label,
		"1_id_entry", &window->one_id_entry,
		"1_search_button", &window->one_search_button,
		"1_example_label", &window->one_example_label,
		"2_page", &window->two_page,
		"2_vcard_label", &window->two_vcard_label,
		"2_information_table", &window->two_information_table,
		"2_id_label", &window->two_id_label,
		"2_name_label", &window->two_name_label,
		"2_email_label", &window->two_email_label,
		"2_country_label", &window->two_country_label,
		"2_id_stub_label", &window->two_id_stub_label,
		"2_name_stub_label", &window->two_name_stub_label,
		"2_email_stub_label", &window->two_email_stub_label,
		"2_country_stub_label", &window->two_country_stub_label,
		"2_nick_entry", &window->two_nick_entry,
		"2_group_combo", &window->two_group_combo,
		"2_group_entry", &window->two_group_entry,
		"last_page", &window->last_page,
		"last_label", &window->last_label,
		NULL);
	
	gossip_glade_connect (
		glade, window,
		"add_contact_window", "destroy", add_contact_window_destroy,
		"druid", "cancel", add_contact_window_cancel,
		"1_id_entry", "changed", add_contact_window_1_id_entry_changed,
		"1_system_combobox", "changed", add_contact_window_1_system_combobox_changed,
		"2_nick_entry", "changed", add_contact_window_2_nick_entry_changed,
		"2_nick_entry", "key_press_event", add_contact_window_2_nick_entry_key_pressed,
		"last_page", "finish", add_contact_window_last_finished,
		NULL);
	
	g_object_unref (glade);
	
	g_signal_connect_after (window->one_page, "prepare",
				G_CALLBACK (add_contact_window_1_prepare),
				window);
	g_signal_connect_after (window->two_page, "prepare",
				G_CALLBACK (add_contact_window_2_prepare),
				window);
	g_signal_connect_after (window->last_page, "prepare",
				G_CALLBACK (add_contact_window_last_prepare),
				window);
	g_signal_connect_after (window->two_group_entry, "insert_text",
				G_CALLBACK (add_contact_window_2_group_entry_text_inserted),
				window);

	g_object_set_data (G_OBJECT (window->two_nick_entry), 
			   "changed", GINT_TO_POINTER (0));

	druid = GNOME_DRUID (window->druid);
	
	g_object_set (G_OBJECT (druid->next), 
		      "can-default", TRUE,
		      "has-default", TRUE,
		      NULL);

	session = gossip_app_get_session ();

	window->one_accounts_chooser = gossip_account_chooser_new (session);
	gtk_box_pack_start (GTK_BOX (window->one_accounts_vbox), 
			    window->one_accounts_chooser,
			    TRUE, TRUE, 0);
	gtk_widget_show (window->one_accounts_chooser);
	
	accounts = gossip_session_get_accounts (session);
	if (g_list_length (accounts) > 1) {
		gtk_widget_show (window->one_accounts_vbox);
	} else {
		/* show no accounts combo box */
		gtk_widget_hide (window->one_accounts_vbox);
	}
	
	g_list_foreach (accounts, (GFunc)g_object_unref, NULL);
	g_list_free (accounts);

	/* populate systems */
	add_contact_window_setup_systems (NULL, window);

	/* do we skip a stage? */
	if (contact) {
		GossipAccount        *account;
		GossipAccountChooser *account_chooser;

		account = gossip_contact_get_account (contact);
		account_chooser = GOSSIP_ACCOUNT_CHOOSER (window->one_accounts_chooser);
		gossip_account_chooser_set_account (account_chooser, account);

                gtk_entry_set_text (GTK_ENTRY (window->one_id_entry), 
				    gossip_contact_get_id (contact));
		gnome_druid_set_page (GNOME_DRUID (window->druid),
				      GNOME_DRUID_PAGE (window->two_page));
		add_contact_window_2_prepare (GNOME_DRUID_PAGE (window->two_page),
					      GNOME_DRUID (window->druid), 
					      window);
	} else {
		add_contact_window_1_prepare (GNOME_DRUID_PAGE (window->one_page), 
					      GNOME_DRUID (window->druid),
					      window);
	}

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (window->window), parent); 
	}

	gtk_widget_show (window->window);
}
