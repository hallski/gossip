/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2006 Imendio AB
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

/* Part of this file is copied from GtkSourceView (gtksourceiter.c):
 *  Copyright (C) 2000 - 2005 Paolo Maggi 
 *  Copyright (C) 2002, 2003 Jeroen Zwartepoorte
 */

#include <config.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#ifdef HAVE_GNOME
#include <libgnome/gnome-url.h>
#include <libgnomeui/gnome-href.h>
#elif defined (HAVE_COCOA)
#include <Cocoa/Cocoa.h>
#endif

#include "gossip-stock.h"
#include "gossip-app.h"
#include "gossip-ui-utils.h"


struct SizeData {
	gint     width;
	gint     height;
	gboolean preserve_aspect_ratio;
};

static void
pixbuf_from_avatar_size_prepared_cb (GdkPixbufLoader *loader,
				     gint             width,
				     gint             height,
						 struct SizeData *data);

static GladeXML *
get_glade_file (const gchar *filename,
		const gchar *root,
		const gchar *domain,
		const gchar *first_required_widget, 
		va_list      args)
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
	GtkWidget *entry;
	GtkWidget *vbox;
	gchar     *str;
	gchar     *password;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);
	
	str = g_strdup_printf (_("Please enter your %s account password"),
			       gossip_account_get_name (account));
	dialog = gtk_message_dialog_new_with_markup (parent,
						     GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_MESSAGE_QUESTION,
						     GTK_BUTTONS_OK_CANCEL,
						     "<b>%s</b>",
						     str);
	g_free (str);

	str = g_strdup_printf (_("Logging in with: %s"), 
			       gossip_account_get_id (account));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", str);
	g_free (str);

	checkbox = gtk_check_button_new_with_label (_("Remember Password?"));

	entry = gtk_entry_new ();
	gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE); 
	
	g_signal_connect (entry,
			  "activate",
			  G_CALLBACK (password_dialog_activate_cb),
			  dialog);
	
	vbox = gtk_vbox_new (FALSE, 6);

 	gtk_container_set_border_width  (GTK_CONTAINER (vbox), 6); 
	
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), checkbox, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, FALSE, FALSE, 0);

 	gtk_widget_show_all (dialog); 
	
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
		g_object_unref (pixbuf);
		return NULL;
	}

	gdk_pixbuf_composite (pixbuf_error, 
			      pixbuf, 
			      0, 0,
			      gdk_pixbuf_get_width (pixbuf_error),
			      gdk_pixbuf_get_height (pixbuf_error),
			      0, 0,
			      1, 1,
			      GDK_INTERP_BILINEAR,
			      255);

	g_object_unref (pixbuf_error);

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
	case GOSSIP_PRESENCE_STATE_HIDDEN:
	case GOSSIP_PRESENCE_STATE_UNAVAILABLE:
		stock = GOSSIP_STOCK_OFFLINE;
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
	GossipPresence     *presence;
	GossipSubscription  subscription;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), 
			      gossip_pixbuf_offline ());

	presence = gossip_contact_get_active_presence (contact);

	if (presence) {
		return gossip_pixbuf_for_presence (presence);
	}

	subscription = gossip_contact_get_subscription (contact);

	if (subscription != GOSSIP_SUBSCRIPTION_BOTH && 
	    subscription != GOSSIP_SUBSCRIPTION_TO) {
		return gossip_pixbuf_from_stock (GOSSIP_STOCK_PENDING, 
						 GTK_ICON_SIZE_MENU);
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
	case GOSSIP_CHATROOM_STATUS_JOINING:
		stock_id = GTK_STOCK_EXECUTE;
		break;
	case GOSSIP_CHATROOM_STATUS_ACTIVE:
		stock_id = GOSSIP_STOCK_GROUP_MESSAGE;
		break;
	case GOSSIP_CHATROOM_STATUS_ERROR:
		stock_id = GTK_STOCK_DIALOG_ERROR;
		break;	
	default:
	case GOSSIP_CHATROOM_STATUS_INACTIVE:
	case GOSSIP_CHATROOM_STATUS_UNKNOWN:
		stock_id = GTK_STOCK_DISCONNECT;
		break;
	}

	pixbuf = gossip_pixbuf_from_stock (stock_id, icon_size);

	return pixbuf;
}

GdkPixbuf *
gossip_pixbuf_avatar_from_vcard (GossipVCard *vcard) 
{
	GdkPixbuf	*pixbuf;
	GdkPixbufLoader	*loader;
	const guchar    *avatar;
	gsize		 len;
	GError          *error = NULL;
	
	g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);
	
	avatar = gossip_vcard_get_avatar (vcard, &len);
	if (!avatar) {
		return NULL;
	}

	loader = gdk_pixbuf_loader_new ();

	if (!gdk_pixbuf_loader_write (loader, avatar, len, &error)) {
		g_warning ("Couldn't write avatar image:%p with "
			   "length:%" G_GSIZE_FORMAT " to pixbuf loader: %s",
			   avatar, len, error->message);
		g_error_free (error);
		return NULL;
	}

	gdk_pixbuf_loader_close (loader, NULL);

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	g_object_ref (pixbuf);
	g_object_unref (loader);

	return pixbuf;
}

GdkPixbuf *
gossip_pixbuf_avatar_from_contact (GossipContact *contact) 
{
	GdkPixbuf	*pixbuf;
	GdkPixbufLoader	*loader;
	const guchar	*avatar;
	gsize		 len;
	GError          *error = NULL;
	
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);
	
	avatar = gossip_contact_get_avatar (contact, &len);
	if (!avatar) {
		return NULL;
	}

	loader = gdk_pixbuf_loader_new ();

	if (!gdk_pixbuf_loader_write (loader, avatar, len, &error)) {
		g_warning ("Couldn't write avatar image:%p with "
			   "length:%" G_GSIZE_FORMAT " to pixbuf loader: %s",
			   avatar, len, error->message);
		g_error_free (error);
		return NULL;
	}

	gdk_pixbuf_loader_close (loader, NULL);

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	g_object_ref (pixbuf);
	g_object_unref (loader);

	return pixbuf;
}

static void
pixbuf_from_avatar_size_prepared_cb (GdkPixbufLoader *loader,
				     int              width,
				     int              height,
				     struct SizeData *data)
{
	g_return_if_fail (width > 0 && height > 0);

	if (data->preserve_aspect_ratio && (data->width > 0 || data->height > 0)) {
		if (width < data->width && height < data->height) {
			width = width;
			height = height;
		}

		if (data->width < 0) {
			width = width * (double) data->height / (gdouble) height;
			height = data->height;
		} else if (data->height < 0) {
			height = height * (double) data->width / (double) width;
			width = data->width;
		} else if ((double) height * (double) data->width >
			   (double) width * (double) data->height) {
			width = 0.5 + (double) width * (double) data->height / (double) height;
			height = data->height;
		} else {
			height = 0.5 + (double) height * (double) data->width / (double) width;
			width = data->width;
		}
	} else {
		if (data->width > 0) {
			width = data->width;
		}

		if (data->height > 0) {
			height = data->height;
		}
	}

	gdk_pixbuf_loader_set_size (loader, width, height);
}

GdkPixbuf *
gossip_pixbuf_from_avatar_scaled (const guchar *avatar, 
				  gsize         len,
				  gint          width,
				  gint          height)
{
	GdkPixbuf        *pixbuf;
	GdkPixbufLoader	 *loader;
	struct SizeData   data;
	GError           *error = NULL;

	if (!avatar) {
		return NULL;
	}

	data.width = width;
	data.height = height;
	data.preserve_aspect_ratio = TRUE;

	loader = gdk_pixbuf_loader_new ();

	g_signal_connect (loader, "size-prepared", 
			  G_CALLBACK (pixbuf_from_avatar_size_prepared_cb), 
			  &data);

	if (!gdk_pixbuf_loader_write (loader, avatar, len, &error)) {
		g_warning ("Couldn't write avatar image:%p with "
			   "length:%" G_GSIZE_FORMAT " to pixbuf loader: %s",
			   avatar, len, error->message);
		g_error_free (error);
		return NULL;
	}

	gdk_pixbuf_loader_close (loader, NULL);

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	g_object_ref (pixbuf);
	g_object_unref (loader);

	return pixbuf;
}

GdkPixbuf *
gossip_pixbuf_avatar_from_contact_scaled (GossipContact *contact, 
					  gint           width,
					  gint           height)
{
	const guchar *avatar;
	gsize         len;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	avatar = gossip_contact_get_avatar (contact, &len);
	if (!avatar) {
		return NULL;
	}
	
	return gossip_pixbuf_from_avatar_scaled (avatar, len, width, height);
}

GdkPixbuf *
gossip_pixbuf_avatar_from_vcard_scaled (GossipVCard *vcard, 
					GtkIconSize  size)
{
	const guchar *avatar;
	gsize	      len;
	gint          width, height;
	
	g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);

	avatar = gossip_vcard_get_avatar (vcard, &len);
	if (!avatar) {
		return NULL;
	}

	if (!gtk_icon_size_lookup (size, &width, &height)) {
		height = width = 48;
	}

	return gossip_pixbuf_from_avatar_scaled (avatar, len, width, height);
}

/* Stolen from GtkSourceView, hence the weird intendation. Please keep it like
 * that to make it easier to apply changes from the original code.
 */
#define GTK_TEXT_UNKNOWN_CHAR 0xFFFC

/* this function acts like g_utf8_offset_to_pointer() except that if it finds a
 * decomposable character it consumes the decomposition length from the given
 * offset.  So it's useful when the offset was calculated for the normalized
 * version of str, but we need a pointer to str itself. */
static const gchar *
pointer_from_offset_skipping_decomp (const gchar *str, gint offset)
{
	gchar *casefold, *normal;
	const gchar *p, *q;

	p = str;
	while (offset > 0)
	{
		q = g_utf8_next_char (p);
		casefold = g_utf8_casefold (p, q - p);
		normal = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
		offset -= g_utf8_strlen (normal, -1);
		g_free (casefold);
		g_free (normal);
		p = q;
	}
	return p;
}

static const gchar *
g_utf8_strcasestr (const gchar *haystack, const gchar *needle)
{
	gsize needle_len;
	gsize haystack_len;
	const gchar *ret = NULL;
	gchar *p;
	gchar *casefold;
	gchar *caseless_haystack;
	gint i;

	g_return_val_if_fail (haystack != NULL, NULL);
	g_return_val_if_fail (needle != NULL, NULL);

	casefold = g_utf8_casefold (haystack, -1);
	caseless_haystack = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
	g_free (casefold);

	needle_len = g_utf8_strlen (needle, -1);
	haystack_len = g_utf8_strlen (caseless_haystack, -1);

	if (needle_len == 0)
	{
		ret = (gchar *)haystack;
		goto finally_1;
	}

	if (haystack_len < needle_len)
	{
		ret = NULL;
		goto finally_1;
	}

	p = (gchar*)caseless_haystack;
	needle_len = strlen (needle);
	i = 0;

	while (*p)
	{
		if ((strncmp (p, needle, needle_len) == 0))
		{
			ret = pointer_from_offset_skipping_decomp (haystack, i);
			goto finally_1;
		}

		p = g_utf8_next_char (p);
		i++;
	}

finally_1:
	g_free (caseless_haystack);

	return ret;
}

static gboolean
g_utf8_caselessnmatch (const char *s1, const char *s2,
		       gssize n1, gssize n2)
{
	gchar *casefold;
	gchar *normalized_s1;
	gchar *normalized_s2;
	gint len_s1;
	gint len_s2;
	gboolean ret = FALSE;

	g_return_val_if_fail (s1 != NULL, FALSE);
	g_return_val_if_fail (s2 != NULL, FALSE);
	g_return_val_if_fail (n1 > 0, FALSE);
	g_return_val_if_fail (n2 > 0, FALSE);

	casefold = g_utf8_casefold (s1, n1);
	normalized_s1 = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
	g_free (casefold);

	casefold = g_utf8_casefold (s2, n2);
	normalized_s2 = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
	g_free (casefold);

	len_s1 = strlen (normalized_s1);
	len_s2 = strlen (normalized_s2);

	if (len_s1 < len_s2)
		goto finally_2;

	ret = (strncmp (normalized_s1, normalized_s2, len_s2) == 0);

finally_2:
	g_free (normalized_s1);
	g_free (normalized_s2);	

	return ret;
}

static void
forward_chars_with_skipping (GtkTextIter *iter,
			     gint         count,
			     gboolean     skip_invisible,
			     gboolean     skip_nontext,
			     gboolean     skip_decomp)
{
	gint i;

	g_return_if_fail (count >= 0);

	i = count;

	while (i > 0)
	{
		gboolean ignored = FALSE;

		/* minimal workaround to avoid the infinite loop of bug #168247.
		 * It doesn't fix the problemjust the symptom...
		 */
		if (gtk_text_iter_is_end (iter))
			return;

		if (skip_nontext && gtk_text_iter_get_char (iter) == GTK_TEXT_UNKNOWN_CHAR)
			ignored = TRUE;

		if (!ignored && skip_invisible &&
		    /* _gtk_text_btree_char_is_invisible (iter)*/ FALSE)
			ignored = TRUE;

		if (!ignored && skip_decomp)
		{
			/* being UTF8 correct sucks; this accounts for extra
			   offsets coming from canonical decompositions of
			   UTF8 characters (e.g. accented characters) which 
			   g_utf8_normalize() performs */
			gchar *normal;
			gchar buffer[6];
			gint buffer_len;

			buffer_len = g_unichar_to_utf8 (gtk_text_iter_get_char (iter), buffer);
			normal = g_utf8_normalize (buffer, buffer_len, G_NORMALIZE_NFD);
			i -= (g_utf8_strlen (normal, -1) - 1);
			g_free (normal);
		}

		gtk_text_iter_forward_char (iter);

		if (!ignored)
			--i;
	}
}

static gboolean
lines_match (const GtkTextIter *start,
	     const gchar      **lines,
	     gboolean           visible_only,
	     gboolean           slice,
	     GtkTextIter       *match_start,
	     GtkTextIter       *match_end)
{
	GtkTextIter next;
	gchar *line_text;
	const gchar *found;
	gint offset;

	if (*lines == NULL || **lines == '\0')
	{
		if (match_start)
			*match_start = *start;
		if (match_end)
			*match_end = *start;
		return TRUE;
	}

	next = *start;
	gtk_text_iter_forward_line (&next);

	/* No more text in buffer, but *lines is nonempty */
	if (gtk_text_iter_equal (start, &next))
		return FALSE;

	if (slice)
	{
		if (visible_only)
			line_text = gtk_text_iter_get_visible_slice (start, &next);
		else
			line_text = gtk_text_iter_get_slice (start, &next);
	}
	else
	{
		if (visible_only)
			line_text = gtk_text_iter_get_visible_text (start, &next);
		else
			line_text = gtk_text_iter_get_text (start, &next);
	}

	if (match_start) /* if this is the first line we're matching */
	{
		found = g_utf8_strcasestr (line_text, *lines);
	}
	else
	{
		/* If it's not the first line, we have to match from the
		 * start of the line.
		 */
		if (g_utf8_caselessnmatch (line_text, *lines, strlen (line_text),
					   strlen (*lines)))
			found = line_text;
		else
			found = NULL;
	}

	if (found == NULL)
	{
		g_free (line_text);
		return FALSE;
	}

	/* Get offset to start of search string */
	offset = g_utf8_strlen (line_text, found - line_text);

	next = *start;

	/* If match start needs to be returned, set it to the
	 * start of the search string.
	 */
	forward_chars_with_skipping (&next, offset, visible_only, !slice, FALSE);
	if (match_start)
	{
		*match_start = next;
	}

	/* Go to end of search string */
	forward_chars_with_skipping (&next, g_utf8_strlen (*lines, -1), visible_only, !slice, TRUE);

	g_free (line_text);

	++lines;

	if (match_end)
		*match_end = next;

	/* pass NULL for match_start, since we don't need to find the
	 * start again.
	 */
	return lines_match (&next, lines, visible_only, slice, NULL, match_end);
}

/* strsplit () that retains the delimiter as part of the string. */
static gchar **
strbreakup (const char *string,
	    const char *delimiter,
	    gint        max_tokens)
{
	GSList *string_list = NULL, *slist;
	gchar **str_array, *s, *casefold, *new_string;
	guint i, n = 1;

	g_return_val_if_fail (string != NULL, NULL);
	g_return_val_if_fail (delimiter != NULL, NULL);

	if (max_tokens < 1)
		max_tokens = G_MAXINT;

	s = strstr (string, delimiter);
	if (s)
	{
		guint delimiter_len = strlen (delimiter);

		do
		{
			guint len;

			len = s - string + delimiter_len;
			new_string = g_new (gchar, len + 1);
			strncpy (new_string, string, len);
			new_string[len] = 0;
			casefold = g_utf8_casefold (new_string, -1);
			g_free (new_string);
			new_string = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
			g_free (casefold);
			string_list = g_slist_prepend (string_list, new_string);
			n++;
			string = s + delimiter_len;
			s = strstr (string, delimiter);
		} while (--max_tokens && s);
	}

	if (*string)
	{
		n++;
		casefold = g_utf8_casefold (string, -1);
		new_string = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
		g_free (casefold);
		string_list = g_slist_prepend (string_list, new_string);
	}

	str_array = g_new (gchar*, n);

	i = n - 1;

	str_array[i--] = NULL;
	for (slist = string_list; slist; slist = slist->next)
		str_array[i--] = slist->data;

	g_slist_free (string_list);

	return str_array;
}

gboolean
gossip_text_iter_forward_search (const GtkTextIter   *iter,
				 const gchar         *str,
				 GtkTextIter         *match_start,
				 GtkTextIter         *match_end,
				 const GtkTextIter   *limit)
{
	gchar **lines = NULL;
	GtkTextIter match;
	gboolean retval = FALSE;
	GtkTextIter search;
	gboolean visible_only;
	gboolean slice;

	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (str != NULL, FALSE);

	if (limit && gtk_text_iter_compare (iter, limit) >= 0)
		return FALSE;

	if (*str == '\0') {
		/* If we can move one char, return the empty string there */
		match = *iter;

		if (gtk_text_iter_forward_char (&match)) {
			if (limit && gtk_text_iter_equal (&match, limit)) {
				return FALSE;
			}

			if (match_start) {
				*match_start = match;
			}
			if (match_end) {
				*match_end = match;
			}
			return TRUE;
		} else {
			return FALSE;
		}
	}
	
	visible_only = TRUE;
	slice = FALSE;

	/* locate all lines */
	lines = strbreakup (str, "\n", -1);

	search = *iter;

	do {
		/* This loop has an inefficient worst-case, where
		 * gtk_text_iter_get_text () is called repeatedly on
		 * a single line.
		 */
		GtkTextIter end;

		if (limit && gtk_text_iter_compare (&search, limit) >= 0) {
			break;
		}

		if (lines_match (&search, (const gchar**)lines,
				 visible_only, slice, &match, &end)) {
			if (limit == NULL ||
			    (limit && gtk_text_iter_compare (&end, limit) <= 0)) {
				retval = TRUE;

				if (match_start) {
					*match_start = match;
				}
				if (match_end) {
					*match_end = end;
				}
			}
			break;
		}
	} while (gtk_text_iter_forward_line (&search));

	g_strfreev ((gchar**)lines);

	return retval;
}

/* The URL opening code can't handle schemeless strings, so we try to be
 * smart and add http if there is no scheme or doesn't look like a mail
 * address. This should work in most cases, and let us click on strings
 * like "www.gnome.org".
 */
static gchar *
fixup_url (const gchar *url)
{
	gchar *real_url;

	if (!g_str_has_prefix (url, "http://") &&
	    !strstr (url, ":/") &&
	    !strstr (url, "@")) {
		real_url = g_strdup_printf ("http://%s", url);
	} else {
		real_url = g_strdup (url);
	}

	return real_url;
}

#ifdef HAVE_GNOME
void
gossip_url_show (const char *url)
{
	gchar  *real_url;
	GError *error = NULL;

	real_url = fixup_url (url);
	gnome_url_show (real_url, &error);
	if (error) {
		g_warning ("Couldn't show URL:'%s'", real_url);
		g_error_free (error);
	}

	g_free (real_url);
}
#elif defined(HAVE_COCOA)
void
gossip_url_show (const char *url)
{
	gchar  *real_url;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	real_url = fixup_url (url);
	
	NSString *string = [NSString stringWithUTF8String: real_url];
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:string]];

	[pool release];
	
	g_free (real_url);
}
#else
void
gossip_url_show (const char *url)
{
}
#endif

/* FIXME: Replace with GtkLinkButton in 2.10. */
GtkWidget *
gossip_link_button_new (const gchar *url,
			const gchar *title)
{
#ifdef HAVE_GNOME
	return gnome_href_new (url, title);
#else
	return gtk_label_new (title);
#endif
}

