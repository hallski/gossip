/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003      Imendio HB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
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
#include <time.h>
#include <sys/types.h>
#include <regex.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <libgnome/gnome-url.h>
#include <libgnome/gnome-i18n.h>
#include <loudmouth/loudmouth.h>
#include "gossip-stock.h"
#include "gossip-utils.h"

extern GConfClient *gconf_client;

typedef enum {
    TIMESTAMP_STYLE_IRC,
    TIMESTAMP_STYLE_NORMAL
} TimestampStyle;

void
gossip_option_menu_setup (GtkWidget     *option_menu,
			  GCallback      func,
			  gpointer       user_data,
			  gconstpointer  str1, ...)
{
	GtkWidget     *menu;
	GtkWidget     *item;
	gint           i;
	va_list        args;
	gconstpointer  str;
	gint           type;
	
       	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu));
	if (menu) {
		gtk_widget_destroy (menu);
	}
	
	menu = gtk_menu_new ();

	va_start (args, str1);
	for (str = str1, i = 0; str != NULL; str = va_arg (args, gpointer), i++) {
		item = gtk_menu_item_new_with_label (str);
		gtk_widget_show (item);

		gtk_menu_append (GTK_MENU (menu), item);

		type = va_arg (args, gint);
		
		g_object_set_data (G_OBJECT (item),
				   "data",
				   GINT_TO_POINTER (type));
		if (func) {
			g_signal_connect (item,
					  "activate",
					  func,
					  user_data);
		}
	}
	va_end (args);

	gtk_widget_show (menu);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
}

void
gossip_option_menu_set_history (GtkOptionMenu *option_menu,
				gpointer       user_data)
{
	GtkWidget *menu;
	GtkWidget *item;
	GList     *children, *l;
	gint       i;
	
       	menu = gtk_option_menu_get_menu (option_menu);

	children = GTK_MENU_SHELL (menu)->children;
	for (i = 0, l = children; l; i++, l = l->next) {
		item = l->data;

		if (user_data == g_object_get_data (G_OBJECT (item), "data")) {
			gtk_option_menu_set_history (option_menu, i);
			break;
		}
	}
}

gpointer
gossip_option_menu_get_history (GtkOptionMenu *option_menu)
{
	GtkWidget    *menu;
	GtkWidget    *item;
	
       	menu = gtk_option_menu_get_menu (option_menu);

	item = gtk_menu_get_active (GTK_MENU (menu));

	return g_object_get_data (G_OBJECT (item), "data");
}

static void
tagify_bold_labels (GladeXML *xml)
{
        const gchar *str;
        gchar       *s;
        GtkWidget   *label;
	GList       *labels, *l;

	labels = glade_xml_get_widget_prefix (xml, "boldlabel");

	for (l = labels; l; l = l->next) {
		label = l->data;

		if (!GTK_IS_LABEL (label)) {
			g_warning ("Not a label, check your glade file.");
			continue;
		}
 
		str = gtk_label_get_text (GTK_LABEL (label));

		s = g_strdup_printf ("<b>%s</b>", str);
		gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
		gtk_label_set_label (GTK_LABEL (label), s);
		g_free (s);
	}

	g_list_free (labels);
}

static GladeXML *
get_glade_file (const gchar *filename,
		const gchar *root,
		const gchar *domain,
		const gchar *first_required_widget, va_list args)
{
	GladeXML   *gui;
	const char *name;
	GtkWidget **widget_ptr;

	gui = glade_xml_new (filename, root, domain);
	if (!gui) {
		g_warning ("Couldn't find necessary glade file '%s'", filename);
		return NULL;
	}

	for (name = first_required_widget; name; name = va_arg (args, char *)) {
		widget_ptr = va_arg (args, void *);
		
		*widget_ptr = glade_xml_get_widget (gui, name);
		
		if (!*widget_ptr) {
			g_warning ("Glade file '%s' is missing widget '%s'.",
				   filename, name);
			continue;
		}
	}

	tagify_bold_labels (gui);
	
	return gui;
}

void
gossip_glade_get_file_simple (const gchar *filename,
			      const gchar *root,
			      const gchar *domain,
			      const gchar *first_required_widget, ...)
{
	va_list   args;
	GladeXML *gui;

	va_start (args, first_required_widget);

	gui = get_glade_file (filename,
			      root,
			      domain,
			      first_required_widget,
			      args);
	
	va_end (args);

	if (!gui) {
		return;
	}

	g_object_unref (gui);
}

GladeXML *
gossip_glade_get_file (const gchar *filename,
		       const gchar *root,
		       const gchar *domain,
		       const gchar *first_required_widget, ...)
{
	va_list   args;
	GladeXML *gui;

	va_start (args, first_required_widget);

	gui = get_glade_file (filename,
			      root,
			      domain,
			      first_required_widget,
			      args);
	
	va_end (args);

	if (!gui) {
		return NULL;
	}

	return gui;
}

void
gossip_glade_connect (GladeXML *gui,
		      gpointer  user_data,
		      gchar     *first_widget, ...)
{
	va_list      args;
	const gchar *name;
	const gchar *signal;
	GtkWidget   *widget;
	gpointer    *callback;

	va_start (args, first_widget);
	
	for (name = first_widget; name; name = va_arg (args, char *)) {
		signal = va_arg (args, void *);
		callback = va_arg (args, void *);

		widget = glade_xml_get_widget (gui, name);
		if (!widget) {
			g_warning ("Glade file is missing widget '%s', aborting",
				   name);
			continue;
		}

		g_signal_connect (widget,
				  signal,
				  G_CALLBACK (callback),
				  user_data);
	}

	va_end (args);
}

void
gossip_glade_setup_size_group (GladeXML         *gui,
			       GtkSizeGroupMode  mode,
			       gchar            *first_widget, ...)
{
	va_list       args;
	GtkWidget    *widget;
	GtkSizeGroup *size_group;
	const gchar  *name;

	va_start (args, first_widget);

	size_group = gtk_size_group_new (mode);
	
	for (name = first_widget; name; name = va_arg (args, char *)) {
		widget = glade_xml_get_widget (gui, name);
		if (!widget) {
			g_warning ("Glade file is missing widget '%s'", name);
			continue;
		}

		gtk_size_group_add_widget (size_group, widget);
	}

	g_object_unref (size_group);

	va_end (args);
}

static void
password_dialog_activate_cb (GtkWidget *entry, GtkDialog *dialog)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

gchar *
gossip_utils_substring (const gchar *str, gint start, gint end)
{
	return g_strndup (str + start, end - start);
}

const gchar *
gossip_utils_get_timestamp_from_message (LmMessage *m)
{
	LmMessageNode *node;
	const gchar   *timestamp = NULL;
	const gchar   *xmlns;
	
	for (node = m->node->children; node; node = node->next) {
                if (strcmp (node->name, "x") == 0) {
			xmlns = lm_message_node_get_attribute (node, "xmlns");
			if (xmlns && strcmp (xmlns, "jabber:x:delay") == 0) {
                                timestamp = lm_message_node_get_attribute 
					(node, "stamp");
                        }
                }
        }
	
	return timestamp;
}

gchar *
gossip_password_dialog_run (GossipAccount *account, GtkWindow *parent)
{
	GtkWidget *dialog;
	GtkWidget *checkbox;
	GtkWidget *entry, *hbox;
	gchar     *password;
	
	dialog = gtk_message_dialog_new (parent,
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_OK_CANCEL,
					 _("Please enter your password:"));
	
	checkbox = gtk_check_button_new_with_label (_("Remember Password?"));
	gtk_widget_show (checkbox);

	gtk_container_set_border_width (GTK_CONTAINER (checkbox), 2);
	
	entry = gtk_entry_new ();
	gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE); 
	gtk_widget_show (entry);
	
	g_signal_connect (entry,
			  "activate",
			  G_CALLBACK (password_dialog_activate_cb),
			  dialog);
	
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, FALSE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), checkbox, FALSE, TRUE, 4);
	
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		password = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox))) {
			g_free (account->password);
			account->password = password;
			gossip_account_store (account, NULL);
		}
	} else {
		password = NULL;
	}
	
	gtk_widget_destroy (dialog);

	return password;
}

const gchar *
gossip_get_icon_for_show_string (const gchar *str)
{
	if (!str || !str[0]) {
		return GOSSIP_STOCK_AVAILABLE;
	}
	else if (strcmp (str, "dnd") == 0) {
		return GOSSIP_STOCK_BUSY;
	}
	else if (strcmp (str, "away") == 0) {
		return GOSSIP_STOCK_AWAY;
	}
	else if (strcmp (str, "xa") == 0) {
		return GOSSIP_STOCK_EXT_AWAY;
	}
	/* We don't support chat, so treat it like available. */
	else if (strcmp (str, "chat") == 0) {
		return GOSSIP_STOCK_AVAILABLE;
	}

	return GOSSIP_STOCK_AVAILABLE;
}

const gchar *
gossip_show_to_string (GossipShow show)
{
	switch (show) {
	case GOSSIP_SHOW_BUSY:
		return "dnd";
	case GOSSIP_SHOW_AWAY:
		return "away";
	case GOSSIP_SHOW_EXT_AWAY:
		return "xa";
	case GOSSIP_SHOW_AVAILABLE:
		return NULL;
	}

	return NULL;
}

GossipShow 
gossip_show_from_string (const gchar *str)
{
	if (!str) {
		return GOSSIP_SHOW_AVAILABLE;
	}
	if (strcmp (str, "dnd") == 0) {
		return GOSSIP_SHOW_BUSY;
	}
	if (strcmp (str, "away") == 0) {
		return GOSSIP_SHOW_AWAY;
	}
	if (strcmp (str, "xa") == 0) {
		return GOSSIP_SHOW_EXT_AWAY;
	}

	return GOSSIP_SHOW_AVAILABLE;
}

#define ve_string_empty(x) ((x)==NULL||(x)[0]=='\0')

/* stolen from gsearchtool */
GtkWidget *
gossip_hig_dialog_new (GtkWindow      *parent,
		       GtkDialogFlags flags,
		       GtkMessageType type,
		       GtkButtonsType buttons,
		       const gchar    *header,
		       const gchar    *messagefmt,
		       ...)
{
        GtkWidget *dialog;
        GtkWidget *dialog_vbox;
        GtkWidget *dialog_action_area;
        GtkWidget *hbox;
        GtkWidget *vbox;
        GtkWidget *label;
        GtkWidget *button;
        GtkWidget *image;
        gchar     *title;
        va_list    args;
        gchar     *msg;
        gchar     *hdr;
 
        if (!ve_string_empty (messagefmt)) {
                va_start (args, messagefmt);
                msg = g_strdup_vprintf (messagefmt, args);
                va_end (args);
        } else {
                msg = NULL;
        }
	
        hdr = g_markup_escape_text (header, -1);
	
        dialog = gtk_dialog_new ();
	
        gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
        gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
        gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
        gtk_window_set_title (GTK_WINDOW (dialog), "");
	
        dialog_vbox = GTK_DIALOG (dialog)->vbox;
        gtk_box_set_spacing (GTK_BOX (dialog_vbox), 12);
 
        hbox = gtk_hbox_new (FALSE, 12);
        gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
        gtk_widget_show (hbox);
 
        if (type == GTK_MESSAGE_ERROR) {
                image = gtk_image_new_from_stock ("gtk-dialog-error", GTK_ICON_SIZE_DIALOG);
        }
	else if (type == GTK_MESSAGE_QUESTION) {
                image = gtk_image_new_from_stock ("gtk-dialog-question", GTK_ICON_SIZE_DIALOG);
        }
	else if (type == GTK_MESSAGE_INFO) {
                image = gtk_image_new_from_stock ("gtk-dialog-info", GTK_ICON_SIZE_DIALOG);
        }
	else if (type == GTK_MESSAGE_WARNING) {
                image = gtk_image_new_from_stock ("gtk-dialog-warning", GTK_ICON_SIZE_DIALOG);
        } else {
                image = NULL;
                g_assert_not_reached ();
        }
	
        gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
        gtk_widget_show (image);
 
        vbox = gtk_vbox_new (FALSE, 6);
        gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
        gtk_widget_show (vbox);
	
        title = g_strconcat ("<span weight='bold' size='larger'>", hdr, "</span>\n", NULL);
        label = gtk_label_new (title);
        gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
        gtk_widget_show (label);
        g_free (title);
	
        if (!ve_string_empty (msg)) {
                label = gtk_label_new (msg);
                gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
                gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
                gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
                gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
                gtk_widget_show (label);
        }
 
        dialog_action_area = GTK_DIALOG (dialog)->action_area;
        gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);
	
        switch (buttons) {
	case GTK_BUTTONS_NONE:
		break;
		
	case GTK_BUTTONS_OK:
		
		button = gtk_button_new_from_stock (GTK_STOCK_OK);
		gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);
		GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
		gtk_widget_show (button);
		
		gtk_dialog_set_default_response (GTK_DIALOG (dialog),
						 GTK_RESPONSE_OK);
		break;
		
	case GTK_BUTTONS_CLOSE:
		
		button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
		gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_CLOSE);
		GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
		gtk_widget_show (button);
		
		gtk_dialog_set_default_response (GTK_DIALOG (dialog),
						 GTK_RESPONSE_CLOSE);
		break;
		
	case GTK_BUTTONS_CANCEL:
		
		button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
		gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_CANCEL);
		GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
		gtk_widget_show (button);
		
		gtk_dialog_set_default_response (GTK_DIALOG (dialog),
						 GTK_RESPONSE_CLOSE);
		break;
		
	case GTK_BUTTONS_YES_NO:
		
		button = gtk_button_new_from_stock (GTK_STOCK_NO);
		gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_NO);
		GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
		gtk_widget_show (button);
 
		button = gtk_button_new_from_stock (GTK_STOCK_YES);
		gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_YES);
		GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
		gtk_widget_show (button);
 
		gtk_dialog_set_default_response (GTK_DIALOG (dialog),
						 GTK_RESPONSE_YES);
		break;
 
 
	case GTK_BUTTONS_OK_CANCEL:
 
		button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
		gtk_widget_show (button);
		gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_CANCEL);
		GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
 
		button = gtk_button_new_from_stock (GTK_STOCK_OK);
		gtk_widget_show (button);
		gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);
		GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
 
		gtk_dialog_set_default_response (GTK_DIALOG (dialog),
						 GTK_RESPONSE_OK);
		break;
 
	default:
		g_assert_not_reached ();
		break;
        }
 
        if (parent != NULL) {
                gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));
        }
        if (flags & GTK_DIALOG_MODAL) {
                gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
        }
        if (flags & GTK_DIALOG_DESTROY_WITH_PARENT) {
                gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
        }
 
        g_free (msg);
        g_free (hdr);
 
        return dialog;
}
