/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2007 Imendio AB
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
 * 
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 * 
 *          Part of this file is copied from GtkSourceView (gtksourceiter.c):
 *          Paolo Maggi
 *          Jeroen Zwartepoorte
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#ifdef HAVE_GNOME
#include <gdk/gdkx.h>
#include <libgnome/gnome-url.h>
#include <libgnomeui/libgnomeui.h>
#elif defined (HAVE_COCOA)
#include <Cocoa/Cocoa.h>
#endif

#include <libgossip/gossip-session.h>
#include <libgossip/gossip-account-manager.h>
#include <libgossip/gossip-conf.h>
#include <libgossip/gossip-paths.h>
#include <libgossip/gossip-stock.h>

#ifdef HAVE_LIBNOTIFY
#include "gossip-notify.h"
#endif

#include "gossip-app.h"
#include "gossip-ui-utils.h"

static void hint_dialog_response_cb             (GtkWidget       *widget,
						 gint             response,
						 GtkWidget       *checkbutton);

static GladeXML *
get_glade_file (const gchar *filename,
		const gchar *root,
		const gchar *domain,
		const gchar *first_required_widget,
		va_list      args)
{
	gchar      *path;
	GladeXML   *gui;
	const char *name;
	GtkWidget **widget_ptr;

	path = gossip_paths_get_glade_path (filename);
	gui = glade_xml_new (path, root, domain);
	g_free (path);

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
	GtkWidget   *dialog;
	GtkWidget   *checkbox;
	GtkWidget   *entry;
	GtkWidget   *vbox;
	gchar       *str;
	gchar       *password;
	const gchar *name;

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

	name = gossip_account_get_name (account);
	str = g_strdup_printf (_("Logging in to account '%s'"), name);
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

			gossip_account_set_param (account, "password", password, NULL);
			gossip_account_manager_store (manager);
		}
	} else {
		password = NULL;
	}

	gtk_widget_destroy (dialog);

	return password;
}

static void
hint_dialog_response_cb (GtkWidget *widget,
			 gint       response,
			 GtkWidget *checkbutton)
{
	GFunc        func;
	gpointer     user_data;
	const gchar *conf_path;
	gboolean     hide_hint;

	conf_path = g_object_get_data (G_OBJECT (widget), "conf_path");
	func = g_object_get_data (G_OBJECT (widget), "func");
	user_data = g_object_get_data (G_OBJECT (widget), "user_data");

	hide_hint = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton));
	gossip_conf_set_bool (gossip_conf_get (), conf_path, !hide_hint);

	gtk_widget_destroy (widget);

	if (func) {
		(func) ((gpointer) conf_path, user_data);
	}
}

gboolean
gossip_hint_dialog_show (const gchar *conf_path,
			 const gchar *message1,
			 const gchar *message2,
			 GtkWindow   *parent,
			 GFunc        func,
			 gpointer     user_data)
{
	GtkWidget *dialog;
	GtkWidget *checkbutton;
	GtkWidget *vbox;
	gboolean   ok;
	gboolean   show_hint = TRUE;

	g_return_val_if_fail (conf_path != NULL, FALSE);
	g_return_val_if_fail (message1 != NULL, FALSE);

	ok = gossip_conf_get_bool (gossip_conf_get (),
				   conf_path,
				   &show_hint);

	if (ok && !show_hint) {
		return FALSE;
	}

	dialog = gtk_message_dialog_new_with_markup (parent,
						     GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_MESSAGE_INFO,
						     GTK_BUTTONS_CLOSE,
						     "<b>%s</b>",
						     message1);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  "%s", message2);
	checkbutton = gtk_check_button_new_with_label (_("Do not show this again"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), TRUE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width  (GTK_CONTAINER (vbox), 6);
	gtk_box_pack_start (GTK_BOX (vbox), checkbutton, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, FALSE, FALSE, 0);

	g_object_set_data_full (G_OBJECT (dialog), "conf_path", g_strdup (conf_path), g_free);
	g_object_set_data (G_OBJECT (dialog), "user_data", user_data);
	g_object_set_data (G_OBJECT (dialog), "func", func);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (hint_dialog_response_cb),
			  checkbutton);

	gtk_widget_show_all (dialog);

	return TRUE;
}

gboolean
gossip_hint_show (const gchar         *conf_path,
		  const gchar         *message1,
		  const gchar         *message2,
		  GtkWindow           *parent,
		  GFunc                func,
		  gpointer             user_data)
{
#ifdef HAVE_LIBNOTIFY
	return gossip_notify_hint_show (conf_path,
					message1, message2,
					func, user_data);
#else
	return gossip_hint_dialog_show (conf_path,
					message1, message2,
					parent,
					func, user_data);
#endif
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
	case GOSSIP_ACCOUNT_TYPE_JABBER_LEGACY:
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

	/* FIXME: we should have an artwork for these protocols */
	case GOSSIP_ACCOUNT_TYPE_IRC:
	case GOSSIP_ACCOUNT_TYPE_SALUT:
	case GOSSIP_ACCOUNT_TYPE_UNKNOWN:
	default:
		icon_id = "im";
		break;
	}

	pixbuf = gtk_icon_theme_load_icon (theme,
					   icon_id,     /* Icon name */
					   size,        /* Size */
					   0,           /* Flags */
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

	g_return_val_if_fail (window != NULL, FALSE);

	g_object_get (window,
		      "visible", &visible,
		      NULL);

	return visible && window_get_is_on_current_workspace (window);
}

/* Takes care of moving the window to the current workspace. */
void
gossip_window_present (GtkWindow *window,
		       gboolean   steal_focus)
{
	gboolean visible;
	gboolean on_current;
	guint32  timestamp;

	g_return_if_fail (window != NULL);

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

	timestamp = gtk_get_current_event_time ();
	if (steal_focus && timestamp != GDK_CURRENT_TIME) {
		gtk_window_present_with_time (window, timestamp);
	} else {
		gtk_window_present (window);
	}
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
		/* NOTE: GTK_STOCK_DIALOG_ERROR is too big for menu images */
		stock_id = GTK_STOCK_DIALOG_QUESTION;
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
	GossipAvatar    *avatar;
	GError          *error = NULL;

	g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);

	avatar = gossip_vcard_get_avatar (vcard);
	if (!avatar) {
		return NULL;
	}

	loader = gdk_pixbuf_loader_new ();

	if (!gdk_pixbuf_loader_write (loader, avatar->data, avatar->len,
				      &error)) {
		g_warning ("Couldn't write avatar image:%p with "
			   "length:%" G_GSIZE_FORMAT " to pixbuf loader: %s",
			   avatar->data, avatar->len, error->message);
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
	GossipAvatar    *avatar;
	GError          *error = NULL;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	avatar = gossip_contact_get_avatar (contact);
	if (!avatar) {
		return NULL;
	}

	loader = gdk_pixbuf_loader_new ();

	if (!gdk_pixbuf_loader_write (loader, avatar->data, avatar->len, &error)) {
		g_warning ("Couldn't write avatar image:%p with "
			   "length:%" G_GSIZE_FORMAT " to pixbuf loader: %s",
			   avatar->data, avatar->len, error->message);
		g_error_free (error);
		return NULL;
	}

	gdk_pixbuf_loader_close (loader, NULL);

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	g_object_ref (pixbuf);
	g_object_unref (loader);

	return pixbuf;
}

static gboolean
ui_utils_gdk_pixbuf_is_opaque (GdkPixbuf *pixbuf)
{
	int            width;
	int            height;
	int            rowstride; 
	int            i;
        unsigned char *pixels;
        unsigned char *row;

        if (!gdk_pixbuf_get_has_alpha(pixbuf)) {
                return TRUE;
	}

        width     = gdk_pixbuf_get_width (pixbuf);
        height    = gdk_pixbuf_get_height (pixbuf);
        rowstride = gdk_pixbuf_get_rowstride (pixbuf);
        pixels    = gdk_pixbuf_get_pixels (pixbuf);

        row = pixels;
        for (i = 3; i < rowstride; i+=4) {
                if (row[i] != 0xff) {
                        return FALSE;
		}
        }

        for (i = 1; i < height - 1; i++) {
		row = pixels + (i * rowstride);
                if (row[3] != 0xff || row[rowstride-1] != 0xff) {
                        return FALSE;
                }
        }

        row = pixels + ((height-1) * rowstride);
        for (i = 3; i < rowstride; i+=4) {
                if (row[i] != 0xff) {
                        return FALSE;
		}
        }

        return TRUE;
}

/* From pidgin */
static void
ui_utils_pixbuf_roundify (GdkPixbuf *pixbuf)
{
	int     width;
	int     height;
	int     rowstride;
	guchar *pixels;

	g_print ("%s called\n", G_STRFUNC);

	if (!gdk_pixbuf_get_has_alpha(pixbuf)) {
		return;
	}

	width     = gdk_pixbuf_get_width(pixbuf);
	height    = gdk_pixbuf_get_height(pixbuf);
	rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	pixels    = gdk_pixbuf_get_pixels(pixbuf);

	if (width < 6 || height < 6) {
		return;
	}

	/* Top left */
	pixels[3] = 0;
	pixels[7] = 0x80;
	pixels[11] = 0xC0;
	pixels[rowstride + 3] = 0x80;
	pixels[rowstride * 2 + 3] = 0xC0;

	/* Top right */
	pixels[width * 4 - 1] = 0;
	pixels[width * 4 - 5] = 0x80;
	pixels[width * 4 - 9] = 0xC0;
	pixels[rowstride + (width * 4) - 1] = 0x80;
	pixels[(2 * rowstride) + (width * 4) - 1] = 0xC0;

	/* Bottom left */
	pixels[(height - 1) * rowstride + 3] = 0;
	pixels[(height - 1) * rowstride + 7] = 0x80;
	pixels[(height - 1) * rowstride + 11] = 0xC0;
	pixels[(height - 2) * rowstride + 3] = 0x80;
	pixels[(height - 3) * rowstride + 3] = 0xC0;

	/* Bottom right */
	pixels[height * rowstride - 1] = 0;
	pixels[(height - 1) * rowstride - 1] = 0x80;
	pixels[(height - 2) * rowstride - 1] = 0xC0;
	pixels[height * rowstride - 5] = 0x80;
	pixels[height * rowstride - 9] = 0xC0;
}

GdkPixbuf *
gossip_pixbuf_from_avatar_scaled (GossipAvatar *avatar,
				  gint          width,
				  gint          height)
{
	GdkPixbuf        *tmp_pixbuf;
	GdkPixbuf        *ret_pixbuf;
	GdkPixbufLoader	 *loader;
	GError           *error = NULL;
	int               orig_width;
	int               orig_height;
	int               scale_width;
	int               scale_height;

	if (!avatar) {
		return NULL;
	}

	loader = gdk_pixbuf_loader_new ();

	if (!gdk_pixbuf_loader_write (loader, avatar->data, avatar->len, &error)) {
		g_warning ("Couldn't write avatar image:%p with "
			   "length:%" G_GSIZE_FORMAT " to pixbuf loader: %s",
			   avatar->data, avatar->len, error->message);
		g_error_free (error);
		return NULL;
	}

	gdk_pixbuf_loader_close (loader, NULL);

	tmp_pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
	scale_width = orig_width = gdk_pixbuf_get_width (tmp_pixbuf);
	scale_height = orig_height = gdk_pixbuf_get_height (tmp_pixbuf);
	if(scale_height > scale_width) {
		scale_width = (gdouble) width * (double)scale_width / (double)scale_height;
		scale_height = height;
	} else {
		scale_height = (gdouble) height * (double)scale_height / (double)scale_width;
		scale_width = width;
	}

	ret_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 32, 32);
	gdk_pixbuf_fill (ret_pixbuf, 0x00000000);
	gdk_pixbuf_scale (tmp_pixbuf, ret_pixbuf, 
			  (width-scale_width)/2,
			  (height-scale_height)/2,
			  scale_width, 
			  scale_height, 
			  (width-scale_width)/2, 
			  (height-scale_height)/2, 
			  (double)scale_width/(double)orig_width, 
			  (double)scale_height/(double)orig_height,
			  GDK_INTERP_BILINEAR);

	if (ui_utils_gdk_pixbuf_is_opaque (ret_pixbuf)) {
		ui_utils_pixbuf_roundify (ret_pixbuf);
	}

	g_object_unref (loader);

	return ret_pixbuf;
}

GdkPixbuf *
gossip_pixbuf_avatar_from_contact_scaled (GossipContact *contact,
					  gint           width,
					  gint           height)
{
	GossipAvatar *avatar;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	avatar = gossip_contact_get_avatar (contact);

	return gossip_pixbuf_from_avatar_scaled (avatar, width, height);
}

GdkPixbuf *
gossip_pixbuf_avatar_from_vcard_scaled (GossipVCard *vcard,
					GtkIconSize  size)
{
	GossipAvatar *avatar;
	gint          width, height;

	g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);

	avatar = gossip_vcard_get_avatar (vcard);

	if (!gtk_icon_size_lookup (size, &width, &height)) {
		height = width = 48;
	}

	return gossip_pixbuf_from_avatar_scaled (avatar, width, height);
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

static const gchar *
g_utf8_strrcasestr (const gchar *haystack, const gchar *needle)
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

	i = haystack_len - needle_len;
	p = g_utf8_offset_to_pointer (caseless_haystack, i);
	needle_len = strlen (needle);

	while (p >= caseless_haystack)
	{
		if (strncmp (p, needle, needle_len) == 0)
		{
			ret = pointer_from_offset_skipping_decomp (haystack, i);
			goto finally_1;
		}

		p = g_utf8_prev_char (p);
		i--;
	}

finally_1:
	g_free (caseless_haystack);

	return ret;
}

static gboolean
backward_lines_match (const GtkTextIter *start,
		      const gchar      **lines,
		      gboolean           visible_only,
		      gboolean           slice,
		      GtkTextIter       *match_start,
		      GtkTextIter       *match_end)
{
	GtkTextIter line, next;
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

	line = next = *start;
	if (gtk_text_iter_get_line_offset (&next) == 0)
	{
		if (!gtk_text_iter_backward_line (&next))
			return FALSE;
	}
	else
		gtk_text_iter_set_line_offset (&next, 0);

	if (slice)
	{
		if (visible_only)
			line_text = gtk_text_iter_get_visible_slice (&next, &line);
		else
			line_text = gtk_text_iter_get_slice (&next, &line);
	}
	else
	{
		if (visible_only)
			line_text = gtk_text_iter_get_visible_text (&next, &line);
		else
			line_text = gtk_text_iter_get_text (&next, &line);
	}

	if (match_start) /* if this is the first line we're matching */
	{
		found = g_utf8_strrcasestr (line_text, *lines);
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

	forward_chars_with_skipping (&next, offset, visible_only, !slice, FALSE);

	/* If match start needs to be returned, set it to the
	 * start of the search string.
	 */
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

	/* try to match the rest of the lines forward, passing NULL
	 * for match_start so lines_match will try to match the entire
	 * line */
	return lines_match (&next, lines, visible_only,
			    slice, NULL, match_end);
}

gboolean
gossip_text_iter_backward_search (const GtkTextIter   *iter,
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

	if (limit && gtk_text_iter_compare (iter, limit) <= 0)
		return FALSE;

	if (*str == '\0')
	{
		/* If we can move one char, return the empty string there */
		match = *iter;

		if (gtk_text_iter_backward_char (&match))
		{
			if (limit && gtk_text_iter_equal (&match, limit))
				return FALSE;

			if (match_start)
				*match_start = match;
			if (match_end)
				*match_end = match;
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}

	visible_only = TRUE;
	slice = TRUE;

	/* locate all lines */
	lines = strbreakup (str, "\n", -1);

	search = *iter;

	while (TRUE)
	{
		/* This loop has an inefficient worst-case, where
		 * gtk_text_iter_get_text () is called repeatedly on
		 * a single line.
		 */
		GtkTextIter end;

		if (limit && gtk_text_iter_compare (&search, limit) <= 0)
			break;

		if (backward_lines_match (&search, (const gchar**)lines,
					  visible_only, slice, &match, &end))
		{
			if (limit == NULL || (limit &&
					      gtk_text_iter_compare (&end, limit) > 0))
			{
				retval = TRUE;

				if (match_start)
					*match_start = match;
				if (match_end)
					*match_end = end;
			}
			break;
		}

		if (gtk_text_iter_get_line_offset (&search) == 0)
		{
			if (!gtk_text_iter_backward_line (&search))
				break;
		}
		else
		{
			gtk_text_iter_set_line_offset (&search, 0);
		}
	}

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
	gchar             *real_url;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSString          *string;

	real_url = fixup_url (url);

	string = [NSString stringWithUTF8String: real_url];
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

void
gossip_help_show (void)
{
#ifdef HAVE_GNOME
	gboolean   ok;
	GtkWidget *dialog;
	GError    *err = NULL;

	ok = gnome_help_display ("gossip.xml", NULL, &err);
	if (ok) {
		return;
	}

	dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (gossip_app_get_window ()),
						     GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_MESSAGE_ERROR,
						     GTK_BUTTONS_CLOSE,
						     "<b>%s</b>\n\n%s",
						     _("Could not display the help contents."),
						     err->message);

	g_signal_connect_swapped (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  dialog);

	gtk_widget_show (dialog);

	g_error_free (err);
#elif defined(HAVE_COCOA)
	/* Nothing for now. */
#endif
}

static void
link_button_hook (GtkLinkButton *button,
		  const gchar *link,
		  gpointer user_data)
{
	gossip_url_show (link);
}

GtkWidget *
gossip_link_button_new (const gchar *url,
			const gchar *title)
{
	static gboolean hook = FALSE;

	if (!hook) {
		hook = TRUE;
		gtk_link_button_set_uri_hook (link_button_hook, NULL, NULL);
	}

	return gtk_link_button_new_with_label (url, title);
}

/* FIXME: Do this in a proper way at some point, probably in GTK+? */
void
gossip_window_set_default_icon_name (const gchar *name)
{
#ifdef HAVE_GNOME
	gtk_window_set_default_icon_name (name);
#elif defined(HAVE_COCOA)
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	gchar             *path;
	NSString          *tmp;
	NSImage           *image;

	path = gossip_paths_get_image_path ("gossip-logo.png");

	tmp = [NSString stringWithUTF8String:path];
	image = [[NSImage alloc] initWithContentsOfFile:tmp];
	[NSApp setApplicationIconImage:image];
	[image release];

	g_free (path);

	[pool release];
#endif
}

void
gossip_request_user_attention (void)
{
#ifdef HAVE_GNOME
	/* Nothing for now. */
#elif defined(HAVE_COCOA)
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	[NSApp requestUserAttention: NSInformationalRequest];
	[pool release];
#endif
}


void
gossip_toggle_button_set_state_quietly (GtkWidget *widget,
					GCallback  callback,
					gpointer   user_data,
					gboolean   active)
{
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));

	g_signal_handlers_block_by_func (widget, callback, user_data);
	g_object_set (widget, "active", active, NULL);
	g_signal_handlers_unblock_by_func (widget, callback, user_data);
}
