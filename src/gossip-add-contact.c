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
#include <libgnome/gnome-i18n.h>
#include "gossip-utils.h"
#include "gossip-app.h"
#include "gossip-add-contact.h"
#include "gossip-transport-accounts.h"
#include "gossip-transport-protocol.h"


typedef struct {
	GtkWidget *dialog;
	GtkWidget *druid;
	
	/* Page one */
	GtkWidget *one_page;
	GtkWidget *one_system_combobox;
	GtkWidget *one_id_label;
	GtkWidget *one_id_entry;
	GtkWidget *one_search_button;
	GtkWidget *one_example_label;

	/* Page two */
	GtkWidget *two_page;
	GtkWidget *two_waiting_label;
	GtkWidget *two_information_table;
	GtkWidget *two_id_label;
	GtkWidget *two_name_label;
	GtkWidget *two_email_label;
	GtkWidget *two_country_label;
	GtkWidget *two_id_stub_label;
	GtkWidget *two_name_stub_label;
	GtkWidget *two_email_stub_label;
	GtkWidget *two_country_stub_label;
	GtkWidget *two_nick_entry;
	GtkWidget *two_group_combo;
	GtkWidget *two_group_entry;
	
	/* Page three */
	GtkWidget *three_page;
	GtkWidget *three_message_label;
	GtkWidget *three_message_text_view;

	GtkWidget *last_page;
	GtkWidget *last_label;

	GCompletion *group_completion;
	guint        idle_complete;

/* 	LmMessageHandler *vcard_handler; */
} GossipAddContact;


enum {
	COL_NAME,
	NUM_OF_COLS
};


/* static void     add_contact_dialog_destroyed    (GtkWidget        *unused, */
/* 						 GossipAddContact *contact); */
/* static void     add_contact_cancel              (GtkWidget        *unused, */
/* 						 GossipAddContact *contact); */
/* static void     add_contact_prepare_page_1      (GnomeDruidPage   *page, */
/* 						 GnomeDruid       *druid, */
/* 						 GossipAddContact *contact); */
/* static void     add_contact_prepare_page_2      (GnomeDruidPage   *page, */
/* 						 GnomeDruid       *druid, */
/* 						 GossipAddContact *contact); */
/* static void add_contact_prepare_page_3          (GnomeDruidPage   *page, */
/* 						 GnomeDruid       *druid, */
/* 						 GossipAddContact *contact); */
/* static void add_contact_prepare_page_last       (GnomeDruidPage   *page, */
/* 						 GnomeDruid       *druid, */
/* 						 GossipAddContact *contact); */
/* static void add_contact_last_page_finished      (GnomeDruidPage   *page, */
/* 						 GnomeDruid       *druid, */
/* 						 GossipAddContact *contact); */
/* static void add_contact_1_id_entry_changed      (GtkEntry         *entry, */
/* 						 GossipAddContact *contact); */
/* static void add_contact_1_search_button_clicked (GtkButton        *button, */
/* 						 GossipAddContact *contact); */
/* static void add_contact_2_nick_entry_changed    (GtkEntry         *entry, */
/* 						 GossipAddContact *contact); */
/* static gboolean */
/* add_contact_2_nick_entry_key_pressed            (GtkWidget        *entry, */
/* 						 GdkEvent         *event, */
/* 						 GossipAddContact *contact); */
/* static void */
/* add_contact_2_group_entry_text_inserted         (GtkEntry         *entry, */
/* 						 const gchar      *text, */
/* 						 gint              length, */
/* 						 gint             *position, */
/* 						 GossipAddContact *contact); */

enum {
	COL_SYS_IMAGE,
	COL_SYS_TEXT,
	COL_SYS_ACCOUNT,
	COL_SYS_PROTOCOL,
	COL_SYS_COUNT
};


static void             add_contact_dialog_destroyed            (GtkWidget        *unused,
								 GossipAddContact *contact);
static void             add_contact_cancel                      (GtkWidget        *unused,
								 GossipAddContact *contact);
static void             add_contact_setup_systems               (GList            *accounts,
								 GossipAddContact *contact);
static gboolean         add_contact_check_system_id_valid       (GossipAddContact *contact);
static void             add_contact_prepare_page_1              (GnomeDruidPage   *page,
								 GnomeDruid       *druid,
								 GossipAddContact *contact);
static void             add_contact_prepare_page_2              (GnomeDruidPage   *page,
								 GnomeDruid       *druid,
								 GossipAddContact *contact);
static void             add_contact_prepare_page_3              (GnomeDruidPage   *page,
								 GnomeDruid       *druid,
								 GossipAddContact *contact);
static void             add_contact_prepare_page_last           (GnomeDruidPage   *page,
								 GnomeDruid       *druid,
								 GossipAddContact *contact);
static void             add_contact_last_page_finished          (GnomeDruidPage   *page,
								 GnomeDruid       *druid,
								 GossipAddContact *contact);
static void             add_contact_1_id_entry_changed          (GtkEntry         *entry,
								 GossipAddContact *contact);
static void             add_contact_1_search_button_clicked     (GtkButton        *button,
								 GossipAddContact *contact);
static void             add_contact_1_system_combobox_changed   (GtkComboBox      *combo_box,
								 GossipAddContact *contact);
static void     add_contact_page_2_vcard_handler (GossipAsyncResult  result,
                                                  GossipVCard       *vcard,
                                                  GossipAddContact  *contact);

static void             add_contact_2_nick_entry_changed        (GtkEntry         *entry,
								 GossipAddContact *contact);
static gboolean         add_contact_2_nick_entry_key_pressed    (GtkWidget        *entry,
								 GdkEvent         *event,
								 GossipAddContact *contact);
static void             add_contact_2_group_entry_text_inserted (GtkEntry         *entry,
								 const gchar      *text,
								 gint              length,
								 gint             *position,
								 GossipAddContact *contact);
static void             add_conact_protocol_id_cb               (GossipJID        *jid,
								 const gchar      *id,
								 GossipAddContact *contact);



static void
add_contact_dialog_destroyed (GtkWidget *unused, GossipAddContact *contact)
{
	g_free (contact);
}

static void
add_contact_cancel (GtkWidget *widget, GossipAddContact *contact)
{
	gtk_widget_destroy (contact->dialog);
}

static void
add_contact_setup_systems (GList            *accounts,
			   GossipAddContact *contact)
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
	combo_box = GTK_COMBO_BOX (contact->one_system_combobox);

  	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo_box));  

	store = gtk_list_store_new (COL_SYS_COUNT,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_STRING,    /* name */
				    G_TYPE_POINTER,   /* account */
				    G_TYPE_POINTER);  /* protocol */

	gtk_combo_box_set_model (GTK_COMBO_BOX (contact->one_system_combobox), 
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

	/* populate accounts */
	for (l=accounts; l; l=l->next) {
		GossipTransportAccount  *account;
		GossipTransportProtocol *protocol;

		const gchar             *name;
		const gchar             *disco_type;

		const gchar             *icon = NULL;
		const gchar             *stock_id = NULL;
		const gchar             *core_icon = NULL;

		account = l->data;

		error = NULL; 
		pixbuf = NULL;

		name = gossip_transport_account_get_name (account);
		disco_type = gossip_transport_account_get_disco_type (account);

		protocol = gossip_transport_protocol_find_by_disco_type (disco_type);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 
				    COL_SYS_TEXT, name, 
				    COL_SYS_ACCOUNT, account,
				    COL_SYS_PROTOCOL, protocol,
				    -1);

		if (strcmp (disco_type, "aim") == 0) {
			core_icon = "im-aim";
		} else if (strcmp (disco_type, "icq") == 0) {
			core_icon = "im-icq";
		} else if (strcmp (disco_type, "msn") == 0) {
			core_icon = "im-msn";
		} else if (strcmp (disco_type, "yahoo") == 0) {
			core_icon = "im-yahoo";
		}

		if (core_icon) {
			pixbuf = gtk_icon_theme_load_icon (theme,
							   core_icon, /* icon name */
							   size,      /* size */
							   0,         /* flags */
							   &error);
			if (!pixbuf) {
				g_warning ("could not load icon: %s", error->message);
				g_error_free (error);
				continue;
			}
		} else if (stock_id) {
			pixbuf = gtk_widget_render_icon (gossip_app_get_window (),
							 stock_id, 
							 GTK_ICON_SIZE_MENU,
							 NULL);

			if (!pixbuf) {
				g_warning ("could not load stock icon: %s", stock_id);
				continue;
			}				
		} else if (icon) {		
			pixbuf = gdk_pixbuf_new_from_file_at_size (icon, w, h, &error);
			
			if (!pixbuf) {
				g_warning ("could not load icon: %s", error->message);
				g_error_free (error);
				continue;
			}
		}
		
 		gtk_list_store_set (store, &iter, COL_SYS_IMAGE, pixbuf, -1); 
		g_object_unref (pixbuf);
	}
	
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"pixbuf", COL_SYS_IMAGE,
					NULL);

/*  	gtk_cell_layout_reorder (GTK_CELL_LAYOUT (combo_box), renderer, 0);  */
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"text", COL_SYS_TEXT,
					NULL);

/*  	gtk_cell_layout_reorder (GTK_CELL_LAYOUT (combo_box), renderer, 1);  */

	g_object_unref (store);
}

static void
add_contact_prepare_page_1 (GnomeDruidPage   *page, 
			    GnomeDruid       *druid, 
			    GossipAddContact *contact)
{
        gboolean     valid = FALSE;
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (contact->one_id_entry));
        if (strcmp (str, "") != 0) {
                valid = TRUE;
        }

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (contact->druid),
					   FALSE, valid, TRUE, FALSE);
		
	gtk_widget_grab_focus (contact->one_id_entry);
}

static void
add_contact_prepare_page_2 (GnomeDruidPage   *page,
			    GnomeDruid       *druid, 
			    GossipAddContact *contact)
{
        GossipContact           *c;
	
	GossipTransportProtocol *protocol = NULL;
	GtkTreeModel            *model;
	GtkTreeIter              iter;

	const gchar             *id;
	gchar                   *str;
	GList                   *groups = NULL;
	GList                   *l;
	GList                   *group_strings = NULL;
	gint                     changed;
	
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (contact->one_system_combobox));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (contact->one_system_combobox) , &iter);
	gtk_tree_model_get (model, &iter, COL_SYS_PROTOCOL, &protocol, -1);

	id = gtk_entry_get_text (GTK_ENTRY (contact->one_id_entry));
	gtk_label_set_text (GTK_LABEL (contact->two_id_label), id);

	changed = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (contact->two_nick_entry),
						      "changed"));
                                        
	/* if the nick has NOT been changed */
	if (!changed) { 
		GossipJID *jid;

		/* set up name */
		jid = gossip_jid_new (id);
		str = gossip_jid_get_part_name (jid);
		gtk_entry_set_text (GTK_ENTRY (contact->two_nick_entry), str);
		g_free (str);
		gossip_jid_unref (jid);

		/* vcard */
		c = gossip_contact_new (GOSSIP_CONTACT_TYPE_TEMPORARY);
		gossip_contact_set_id (c, id);
		
		gossip_session_async_get_vcard (gossip_app_get_session (), c, 
						(GossipAsyncVCardCallback) add_contact_page_2_vcard_handler,
						contact, NULL);
	}

	if (protocol) {
		/* translate ID to JID */
		gossip_transport_protocol_id_to_jid (protocol, 
						     id, 
						     (GossipTransportProtocolIDFunc)add_conact_protocol_id_cb,
						     contact);
	} else {
#ifdef DEPRECATED		
		LmMessage      *m;
		LmMessageNode  *node;

		if (contact->vcard_handler) {
			lm_message_handler_invalidate (contact->vcard_handler);
			lm_message_handler_unref (contact->vcard_handler);
		}
#endif
		
		gtk_widget_show(contact->two_waiting_label);
		gtk_widget_hide(contact->two_information_table);
	}
	
	groups = gossip_session_get_groups (gossip_app_get_session ());
	g_completion_clear_items (contact->group_completion);

	for (l = groups; l; l = l->next) {
		group_strings = g_list_prepend (group_strings, 
						(gchar *) l->data);
	}

	group_strings = g_list_sort (group_strings, (GCompareFunc) strcmp);

	if (group_strings) {
		gtk_combo_set_popdown_strings (GTK_COMBO (contact->two_group_combo),
					       group_strings);
		g_completion_add_items (contact->group_completion, group_strings);
	}

	gtk_entry_set_text (GTK_ENTRY (contact->two_group_entry), "");

	/* set focus and buttons up */
	gtk_widget_grab_focus (contact->two_nick_entry);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (contact->druid),
					   TRUE,
					   TRUE,
					   TRUE,
					   FALSE);

	/* FIXME: check Jabber ID, if not valid show dialog. */
}

static void
add_conact_protocol_id_cb (GossipJID *jid, 
			   const gchar *id,
			   GossipAddContact *contact)
{
	/* set up name */
	gtk_label_set_text (GTK_LABEL (contact->two_id_label), 
			    gossip_jid_get_full (jid));
}

static void
add_contact_page_2_vcard_handler (GossipAsyncResult  result,
                                  GossipVCard       *vcard,
                                  GossipAddContact  *contact)
{
	const gchar *str = NULL;

        if (result != GOSSIP_ASYNC_OK) {
                return;
        }

	/* name */
	str = gossip_vcard_get_name (vcard);
	if (str && g_utf8_strlen (str, -1) > 0) {
		gtk_widget_show (contact->two_name_label);
		gtk_widget_show (contact->two_name_stub_label);
		gtk_label_set_text (GTK_LABEL (contact->two_name_label), str);
	} else {
		gtk_widget_hide (contact->two_name_label);
		gtk_widget_hide (contact->two_name_stub_label);
		gtk_label_set_text (GTK_LABEL (contact->two_name_label), "");	
	}
	
	/* email */
	str = gossip_vcard_get_email (vcard);
	if (str && g_utf8_strlen (str, -1) > 0) {
		gtk_widget_show (contact->two_email_label);
		gtk_widget_show (contact->two_email_stub_label);
		gtk_label_set_text (GTK_LABEL (contact->two_email_label), str);
	} else {
		gtk_widget_hide (contact->two_email_label);
		gtk_widget_hide (contact->two_email_stub_label);
		gtk_label_set_text (GTK_LABEL (contact->two_email_label), "");	
	}

	/* country */
	str = gossip_vcard_get_country (vcard);
	if (str && g_utf8_strlen (str, -1) > 0) {
		gtk_widget_show (contact->two_country_label);
		gtk_widget_show (contact->two_country_stub_label);
		gtk_label_set_text (GTK_LABEL (contact->two_country_label), str);
	} else {
		gtk_widget_hide (contact->two_country_label);
		gtk_widget_hide (contact->two_country_stub_label);
		gtk_label_set_text (GTK_LABEL (contact->two_country_label), "");	
	}

	gtk_widget_hide(contact->two_waiting_label);
	gtk_widget_show(contact->two_information_table);
}

static void
add_contact_prepare_page_3 (GnomeDruidPage   *page,
			    GnomeDruid       *druid, 
			    GossipAddContact *contact)
{
	gchar *str;
	
	str = g_strdup_printf (_("What request message do you want to send to %s?"),
			       gtk_entry_get_text (GTK_ENTRY (contact->two_nick_entry)));
	
	gtk_label_set_text (GTK_LABEL (contact->three_message_label), str);
	g_free (str);
}

static void
add_contact_prepare_page_last (GnomeDruidPage   *page,
			       GnomeDruid       *druid,
			       GossipAddContact *contact)
{
	const gchar *nick;
	gchar       *str;

  	gnome_druid_set_show_finish (GNOME_DRUID (contact->druid), TRUE);
	
	nick = gtk_entry_get_text (GTK_ENTRY (contact->two_nick_entry));
	str = g_strdup_printf (_("%s will be added to your contact list."),
			       nick);
	
	gtk_label_set_text (GTK_LABEL (contact->last_label), str);
	g_free (str);
}

static void
add_contact_last_page_finished (GnomeDruidPage   *page,
				GnomeDruid       *druid,
				GossipAddContact *contact)
{
	const gchar   *id;
        const gchar   *name;
 	const gchar   *group;
	GtkTextBuffer *buffer;
	GtkTextIter    start, end;
	gchar         *message;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (contact->three_message_text_view));
	gtk_text_buffer_get_bounds (buffer, &start, &end);

	message = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);

	id = gtk_label_get_text (GTK_LABEL (contact->two_id_label));
        name = gtk_entry_get_text (GTK_ENTRY (contact->two_nick_entry));
        group = gtk_entry_get_text (GTK_ENTRY (contact->two_group_entry));
        
        gossip_session_add_contact (gossip_app_get_session (),
                                    id, name, group, message);
        g_free (message);

	gtk_widget_destroy (contact->dialog);
}

static void
add_contact_1_id_entry_changed (GtkEntry *entry, GossipAddContact *contact)
{
	gboolean valid;

	valid = add_contact_check_system_id_valid (contact);
        gnome_druid_set_buttons_sensitive (GNOME_DRUID (contact->druid),
                                           FALSE, valid, TRUE, FALSE);
}

/* should this be in one of the gossip-transport-* files? */
static gboolean
add_contact_check_system_id_valid (GossipAddContact *contact)
{
	GossipTransportProtocol *protocol = NULL;

	GtkTreeModel            *model;
	GtkTreeIter              iter;

	const gchar             *str;
	const gchar             *disco_type;

	str = gtk_entry_get_text (GTK_ENTRY (contact->one_id_entry));

	if (!str || strlen (str) < 1) {
		return FALSE;
	}

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (contact->one_system_combobox));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (contact->one_system_combobox), &iter);

	gtk_tree_model_get (model, &iter, COL_SYS_PROTOCOL, &protocol, -1);
	
	if (!protocol) {
		return gossip_jid_string_is_valid_jid (str);
	}
	
	disco_type = gossip_transport_protocol_get_disco_type (protocol);
	if (!disco_type) {
		/* not sure what protocol it is, we assume its OK */
		return TRUE;
	}

	/* icq */
	if (strcmp (disco_type, "icq") == 0) {
		const gchar *p;

		p = str;

		while (*p) {
			gunichar c;
			
			c = g_utf8_get_char (p);
			
			if (!g_unichar_isdigit (c)) {
				return FALSE;
			}

			p = g_utf8_next_char (p);
		}
	}
		
	/* other core transports */
	if (strcmp (disco_type, "aim") == 0 ||
	    strcmp (disco_type, "msn") == 0 ||
	    strcmp (disco_type, "yahoo") == 0) {
		const gchar *at;
		const gchar *dot;
		gint         len;

		len = strlen (str);
		
		at = strchr (str, '@');
		if (!at || at == str || at == str + len - 1) {
			return FALSE;
		}
		
		dot = strchr (at, '.');
		if (dot == at + 1 
		    || dot == str + len - 1 
		    || dot == str + len - 2) {
			return FALSE;
		}
		
		dot = strrchr (str, '.');
		if (dot == str + len - 1) {
			return FALSE;
		}
	}
	
	/* everything else */
	return TRUE;
}

static void
add_contact_1_search_button_clicked (GtkButton        *button,
				     GossipAddContact *contact)
{
}

static void
add_contact_1_system_combobox_changed (GtkComboBox      *combo_box,
				       GossipAddContact *contact)
{
	GossipTransportProtocol *protocol = NULL;

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
			    COL_SYS_PROTOCOL, &protocol,
			    -1);

	str = g_strdup_printf (_("%s ID of new contact:"), name);
	gtk_label_set_text (GTK_LABEL (contact->one_id_label), str);
	g_free (str);

	if (!protocol) {
		/* must be a jabber system selected */
		example = g_strdup_printf (_("Example: %s"), "user@jabber.org");
	} else {
		example = g_strdup_printf (_("Example: %s"), 
					   gossip_transport_protocol_get_example (protocol));
	}

	str = g_strdup_printf ("<span size=\"smaller\">%s</span>", example);
	gtk_label_set_markup (GTK_LABEL (contact->one_example_label), str);
	g_free (str);

	g_free (example);
	g_free (name);

	/* check entry is valid or not */
	valid = add_contact_check_system_id_valid (contact);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (contact->druid),
					   TRUE,
					   valid,
					   TRUE,
					   FALSE);
}

static void
add_contact_2_nick_entry_changed (GtkEntry         *entry,
				    GossipAddContact *contact)
{
	const gchar     *str;
	gboolean         forward_sensitive = TRUE;
	
	str = gtk_entry_get_text (GTK_ENTRY (entry));;
	if (!str || strcmp (str, "") == 0) {
		forward_sensitive = FALSE;
	}

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (contact->druid),
					   TRUE,
					   forward_sensitive,
					   TRUE,
					   FALSE);
}
 
static gboolean
add_contact_2_nick_entry_key_pressed (GtkWidget        *entry,
				      GdkEvent         *event,
				      GossipAddContact *contact)
{
	g_object_set_data (G_OBJECT (entry), 
			   "changed", GINT_TO_POINTER (1));

	return FALSE;
}

static gboolean 
add_contact_complete_group_idle (GossipAddContact *contact)
{
	GtkEntry    *entry = GTK_ENTRY (contact->two_group_entry);
	const gchar *prefix;
	gchar       *new_prefix;
	gint         text_len;

	prefix = gtk_entry_get_text (entry);
	text_len = strlen (prefix);

	g_completion_complete (contact->group_completion, 
			       (gchar *)prefix, 
			       &new_prefix);

	if (new_prefix) {
		g_signal_handlers_block_by_func (entry,
						 add_contact_2_group_entry_text_inserted, contact);
		
  		gtk_entry_set_text (entry, new_prefix); 
					  
		g_signal_handlers_unblock_by_func (entry, 
						   add_contact_2_group_entry_text_inserted, contact);

		gtk_editable_set_position (GTK_EDITABLE (entry), text_len);
		gtk_editable_select_region (GTK_EDITABLE (entry),
					    text_len, -1);
		g_free (new_prefix);
	}

	contact->idle_complete = 0;
	return FALSE;
}

static void
add_contact_2_group_entry_text_inserted (GtkEntry         *entry, 
					 const gchar      *text,
					 gint              length,
					 gint             *position,
					 GossipAddContact *contact)
{
	if (!contact->idle_complete) {
		contact->idle_complete = 
			g_idle_add ((GSourceFunc) add_contact_complete_group_idle,
				    contact);
	}
}

void
gossip_add_contact_new (const gchar *id)
{
	GossipAddContact *contact;
	GladeXML         *glade;
	GList            *accounts;

	contact = g_new0 (GossipAddContact, 1);
	
	contact->group_completion = g_completion_new (NULL);

	glade = gossip_glade_get_file (
		GLADEDIR "/main.glade",
		"add_contact_dialog",
		NULL,
		"add_contact_dialog", &contact->dialog,
		"add_contact_druid", &contact->druid,
		"1_page", &contact->one_page,
		"1_system_combobox", &contact->one_system_combobox,
		"1_id_label", &contact->one_id_label,
		"1_id_entry", &contact->one_id_entry,
		"1_search_button", &contact->one_search_button,
		"1_example_label", &contact->one_example_label,
		"2_page", &contact->two_page,
		"2_waiting_label", &contact->two_waiting_label,
		"2_information_table", &contact->two_information_table,
		"2_id_label", &contact->two_id_label,
		"2_name_label", &contact->two_name_label,
		"2_email_label", &contact->two_email_label,
		"2_country_label", &contact->two_country_label,
		"2_id_stub_label", &contact->two_id_stub_label,
		"2_name_stub_label", &contact->two_name_stub_label,
		"2_email_stub_label", &contact->two_email_stub_label,
		"2_country_stub_label", &contact->two_country_stub_label,
		"2_nick_entry", &contact->two_nick_entry,
		"2_group_combo", &contact->two_group_combo,
		"2_group_entry", &contact->two_group_entry,
		"3_page", &contact->three_page,
		"3_message_label", &contact->three_message_label,
		"3_message_text_view", &contact->three_message_text_view,
		"last_page", &contact->last_page,
		"last_label", &contact->last_label,
		NULL);
	
	gossip_glade_connect (
		glade, contact,
		"add_contact_dialog", "destroy", add_contact_dialog_destroyed,
		"add_contact_druid", "cancel", add_contact_cancel,
		"1_id_entry", "changed", add_contact_1_id_entry_changed,
		"1_search_button", "clicked", add_contact_1_search_button_clicked,
		"1_system_combobox", "changed", add_contact_1_system_combobox_changed,
		"2_nick_entry", "changed", add_contact_2_nick_entry_changed,
		"2_nick_entry", "key_press_event", add_contact_2_nick_entry_key_pressed,
		"last_page", "finish", add_contact_last_page_finished,
		NULL);
	
	g_object_unref (glade);
	
	g_signal_connect_after (contact->one_page, "prepare",
				G_CALLBACK (add_contact_prepare_page_1),
				contact);
	g_signal_connect_after (contact->two_page, "prepare",
				G_CALLBACK (add_contact_prepare_page_2),
				contact);
	g_signal_connect_after (contact->three_page, "prepare",
				G_CALLBACK (add_contact_prepare_page_3),
				contact);
	g_signal_connect_after (contact->last_page, "prepare",
				G_CALLBACK (add_contact_prepare_page_last),
				contact);
	g_signal_connect_after (contact->two_group_entry, "insert_text",
				G_CALLBACK (add_contact_2_group_entry_text_inserted),
				contact);

	g_object_set_data (G_OBJECT (contact->two_nick_entry), 
			   "changed", GINT_TO_POINTER (0));

	if (id) {
                gtk_entry_set_text (GTK_ENTRY (contact->one_id_entry), id);
		gnome_druid_set_page (GNOME_DRUID (contact->druid),
				      GNOME_DRUID_PAGE (contact->two_page));
		add_contact_prepare_page_2 (GNOME_DRUID_PAGE (contact->two_page),
					    GNOME_DRUID (contact->druid), contact);
	} else {
		add_contact_prepare_page_1 (GNOME_DRUID_PAGE (contact->one_page), 
					    GNOME_DRUID (contact->druid), contact);

#ifdef FIXME_MJR
		/* set up systems combo box */
 		accounts = gossip_transport_accounts_get (al); 
#else
		accounts = NULL;
#endif
		add_contact_setup_systems (accounts, contact);
	}
	
	gtk_widget_show (contact->dialog);
}
