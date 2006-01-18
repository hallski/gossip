/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2005 Imendio AB
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
#include <libgnome/gnome-url.h>
#include <glib/gi18n.h>
#include "gossip-stock.h"
#include "gossip-app.h"
#include "gossip-ui-utils.h"

#define AVAILABLE_MESSAGE "Available"
#define AWAY_MESSAGE "Away"
#define BUSY_MESSAGE "Busy"


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
gossip_password_dialog_run (GossipAccount *account, 
			    GtkWindow     *parent)
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
			GossipSession        *session;
			GossipAccountManager *manager;
			
			session = gossip_app_get_session ();
			manager = gossip_session_get_account_manager (session);

			gossip_account_set_password (account, password);
			gossip_account_manager_store (manager);
		}
	} else {
		password = NULL;
	}
	
	gtk_widget_destroy (dialog);

	return password;
}

GdkPixbuf *
gossip_pixbuf_from_stock (const gchar *stock,
			  GtkIconSize  size)
{
	return gtk_widget_render_icon (gossip_app_get_window (),
				       stock, size, NULL);
}

GdkPixbuf *
gossip_pixbuf_from_account_type (GossipAccountType type,
				 GtkIconSize       icon_size)
{
	GtkIconTheme  *theme;
	GdkPixbuf     *pixbuf = NULL;
	GError        *error = NULL;
	gint           w, h;
	gint           size = 48;  
	const gchar   *icon_id = NULL;

	theme = gtk_icon_theme_get_default ();

	if (!gtk_icon_size_lookup (icon_size, &w, &h)) {
		size = 48;
	} else {
		size = (w + h) / 2; 
	}

	switch (type) {
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

	return pixbuf;
}

GdkPixbuf *
gossip_pixbuf_from_account (GossipAccount *account,
			    GtkIconSize    icon_size)
{
	GossipAccountType  type;
	GdkPixbuf         *pixbuf = NULL;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	type = gossip_account_get_type (account);
	pixbuf = gossip_pixbuf_from_account_type (type, icon_size);

	return pixbuf;
}

GdkPixbuf *
gossip_pixbuf_from_account_status (GossipAccount *account,
				   GtkIconSize    icon_size,
				   gboolean       online)
{
	GdkPixbuf *pixbuf = NULL;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	pixbuf = gossip_pixbuf_from_account (account, icon_size);
	g_return_val_if_fail (pixbuf != NULL, NULL);

	if (!online) {
		GdkPixbuf *modded_pixbuf;

		modded_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
						TRUE,
						8,
						gdk_pixbuf_get_width (pixbuf), 
						gdk_pixbuf_get_height (pixbuf));

		gdk_pixbuf_saturate_and_pixelate (pixbuf,
						  modded_pixbuf,
						  1.0,
						  TRUE);
		g_object_unref (pixbuf);
		pixbuf = modded_pixbuf;
	}

	return pixbuf;
}

GdkPixbuf *
gossip_pixbuf_from_account_error (GossipAccount *account,
				  GtkIconSize    icon_size)
{
	GdkPixbuf *pixbuf;
	GdkPixbuf *pixbuf_error;

	pixbuf = gossip_pixbuf_from_account_status (account, 
						    icon_size,
						    FALSE);
	if (!pixbuf) {
		return NULL;
	}

	pixbuf_error = gossip_pixbuf_from_stock (GTK_STOCK_DIALOG_ERROR,
						 GTK_ICON_SIZE_MENU);
	if (!pixbuf_error) {
		return NULL;
	}

	gdk_pixbuf_composite (pixbuf_error, 
			      pixbuf, 
			      0,0, 
			      gdk_pixbuf_get_width (pixbuf),
			      gdk_pixbuf_get_height (pixbuf),
			      0,0,
			      1,1,
			      GDK_INTERP_BILINEAR,
			      255);

	return pixbuf;
}

GdkPixbuf *
gossip_pixbuf_from_smiley (GossipSmiley type,
			   GtkIconSize  icon_size)
{
	GtkIconTheme  *theme;
	GdkPixbuf     *pixbuf = NULL;
	GError        *error = NULL;
	gint           w, h;
	gint           size;  
	const gchar   *icon_id;

	theme = gtk_icon_theme_get_default ();

	if (!gtk_icon_size_lookup (icon_size, &w, &h)) {
		size = 16;
	} else {
		size = (w + h) / 2; 
	}

	switch (type) {
	case GOSSIP_SMILEY_NORMAL:       /*  :)   */
		icon_id = "stock_smiley-1";
		break;
	case GOSSIP_SMILEY_WINK:         /*  ;)   */
		icon_id = "stock_smiley-3";
		break;
	case GOSSIP_SMILEY_BIGEYE:       /*  =)   */
		icon_id = "stock_smiley-2";
		break;
	case GOSSIP_SMILEY_NOSE:         /*  :-)  */
		icon_id = "stock_smiley-7";
		break;
	case GOSSIP_SMILEY_CRY:          /*  :'(  */
		icon_id = "stock_smiley-11";
		break;
	case GOSSIP_SMILEY_SAD:          /*  :(   */
		icon_id = "stock_smiley-4";
		break;
	case GOSSIP_SMILEY_SCEPTICAL:    /*  :/   */
		icon_id = "stock_smiley-9";
		break;
	case GOSSIP_SMILEY_BIGSMILE:     /*  :D   */
		icon_id = "stock_smiley-6";
		break;
	case GOSSIP_SMILEY_INDIFFERENT:  /*  :|   */
		icon_id = "stock_smiley-8";
		break;
	case GOSSIP_SMILEY_TOUNGE:       /*  :p   */
		icon_id = "stock_smiley-10";
		break;
	case GOSSIP_SMILEY_SHOCKED:      /*  :o   */
		icon_id = "stock_smiley-5";
		break;
	case GOSSIP_SMILEY_COOL:         /*  8)   */
		icon_id = "stock_smiley-15";
		break;
	case GOSSIP_SMILEY_SORRY:        /*  *|   */
		icon_id = "stock_smiley-12";
		break;
	case GOSSIP_SMILEY_KISS:         /*  :*   */
		icon_id = "stock_smiley-13";
		break;
	case GOSSIP_SMILEY_SHUTUP:       /*  :#   */
		icon_id = "stock_smiley-14";
		break;
	case GOSSIP_SMILEY_YAWN:         /*  |O   */
		icon_id = "";
		break;
	case GOSSIP_SMILEY_CONFUSED:     /*  :$   */
		icon_id = "stock_smiley-17";
		break;
	case GOSSIP_SMILEY_ANGEL:        /*  O)   */
		icon_id = "stock_smiley-18";
		break;
	case GOSSIP_SMILEY_OOOH:         /*  :x   */
		icon_id = "stock_smiley-19";
		break;
	case GOSSIP_SMILEY_LOOKAWAY:     /*  *)   */
		icon_id = "stock_smiley-20";
		break;
	case GOSSIP_SMILEY_BLUSH:        /*  *S   */
		icon_id = "stock_smiley-23";
		break;
	case GOSSIP_SMILEY_COOLBIGSMILE: /*  8D   */ 
		icon_id = "stock_smiley-25";
		break;
	case GOSSIP_SMILEY_ANGRY:        /*  :@   */  
		icon_id = "stock_smiley-16"; 
		break; 
	case GOSSIP_SMILEY_BOSS:         /*  @)   */  
		icon_id = "stock_smiley-21"; 
		break; 
	case GOSSIP_SMILEY_MONKEY:       /*  #)   */  
		icon_id = "stock_smiley-22"; 
		break; 
	case GOSSIP_SMILEY_SILLY:        /*  O)   */  
		icon_id = "stock_smiley-24"; 
		break; 
	case GOSSIP_SMILEY_SICK:         /*  +o(  */  
		icon_id = "stock_smiley-26"; 
		break; 

	default: 
		g_assert_not_reached ();
		icon_id = NULL;
	}

	pixbuf = gtk_icon_theme_load_icon (theme,
					   icon_id,     /* icon name */
					   size,        /* size */
					   0,           /* flags */
					   &error);

	return pixbuf;
}

GdkPixbuf *
gossip_pixbuf_offline (void)
{
	return gossip_pixbuf_from_stock (GOSSIP_STOCK_OFFLINE, 
					 GTK_ICON_SIZE_MENU);
}

static gboolean
window_get_is_on_current_workspace (GtkWindow *window)
{
	GdkWindow *gdk_window;

	gdk_window = GTK_WIDGET (window)->window;
	if (gdk_window) {
		return !(gdk_window_get_state (gdk_window) &
			 GDK_WINDOW_STATE_ICONIFIED);
	} else {
		return FALSE;
	}
}

/* Checks if the window is visible as in visible on the current workspace. */
gboolean
gossip_window_get_is_visible (GtkWindow *window)
{
	gboolean visible;

	g_object_get (window,
		      "visible", &visible,
		      NULL);

	return visible && window_get_is_on_current_workspace (window);
}

/* Takes care of moving the window to the current workspace. */
void
gossip_window_present (GtkWindow *window)
{
	gboolean visible;
	gboolean on_current;

	/* There are three cases: hidden, visible, visible on another
	 * workspace.
	 */

	g_object_get (window,
		      "visible", &visible,
		      NULL);

	on_current = window_get_is_on_current_workspace (window);

	if (visible && !on_current) {
		/* Hide it so present brings it to the current workspace. */
		gtk_widget_hide (GTK_WIDGET (window));
	}

	gtk_window_present (window);
}

GdkPixbuf *
gossip_pixbuf_for_presence_state (GossipPresenceState state)
{
	const gchar *stock = NULL; 

	switch (state) {
	case GOSSIP_PRESENCE_STATE_AVAILABLE:
		stock = GOSSIP_STOCK_AVAILABLE;
		break;
	case GOSSIP_PRESENCE_STATE_BUSY:
		stock = GOSSIP_STOCK_BUSY;
		break;
	case GOSSIP_PRESENCE_STATE_AWAY:
		stock = GOSSIP_STOCK_AWAY;
		break;
	case GOSSIP_PRESENCE_STATE_EXT_AWAY:
		stock = GOSSIP_STOCK_EXT_AWAY;
		break;
	}

	return gossip_pixbuf_from_stock (stock, GTK_ICON_SIZE_MENU);
}

GdkPixbuf *
gossip_pixbuf_for_presence (GossipPresence *presence)
{
	GossipPresenceState state; 

	g_return_val_if_fail (GOSSIP_IS_PRESENCE (presence),
			      gossip_pixbuf_offline ());

	state = gossip_presence_get_state (presence);

	return gossip_pixbuf_for_presence_state (state);
}

GdkPixbuf *  
gossip_pixbuf_for_contact (GossipContact *contact)
{
	GossipPresence *presence;
	

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), 
			      gossip_pixbuf_offline ());

	presence = gossip_contact_get_active_presence (contact);

	if (presence) {
		return gossip_pixbuf_for_presence (presence);
	}

	return gossip_pixbuf_offline ();
}

GdkPixbuf *
gossip_pixbuf_for_chatroom_status (GossipChatroom *chatroom,
				   GtkIconSize     icon_size)
{
	GossipChatroomStatus  status;
	GdkPixbuf            *pixbuf;
	const gchar          *stock_id;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	status = gossip_chatroom_get_status (chatroom);
	
	switch (status) {
	case GOSSIP_CHATROOM_CONNECTING:
		stock_id = GTK_STOCK_EXECUTE;
		break;
	case GOSSIP_CHATROOM_OPEN:
		stock_id = GOSSIP_STOCK_GROUP_MESSAGE;
		break;
	case GOSSIP_CHATROOM_ERROR:
		stock_id = GTK_STOCK_DIALOG_ERROR;
		break;	
	default:
	case GOSSIP_CHATROOM_CLOSED:
	case GOSSIP_CHATROOM_UNKNOWN:
		stock_id = GTK_STOCK_CLOSE;
		break;
	}

	pixbuf = gossip_pixbuf_from_stock (stock_id, icon_size);

	return pixbuf;
}
