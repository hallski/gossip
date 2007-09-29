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
 *          Part of this file is copied from GtkSourceView (gtksourceiter.c):
 *          Paolo Maggi
 *          Jeroen Zwartepoorte
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#ifdef HAVE_PLATFORM_X11
#include <gdk/gdkx.h>
#include <libgnome/gnome-url.h>
#include <libgnomeui/libgnomeui.h>
#elif defined (HAVE_PLATFORM_OSX)
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
gossip_pixbuf_offline (void)
{
	return gossip_stock_create_pixbuf (gossip_app_get_window (),
					   GOSSIP_STOCK_OFFLINE,
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

	return gossip_stock_create_pixbuf (gossip_app_get_window (),
					   stock, GTK_ICON_SIZE_MENU);
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
		return gossip_stock_create_pixbuf (gossip_app_get_window (),
						   GOSSIP_STOCK_PENDING,
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

	pixbuf = gossip_stock_create_pixbuf (gossip_app_get_window (),
					     stock_id, icon_size);

	return pixbuf;
}

/* The URL opening code can't handle schemeless strings, so we try to be
 * smart and add http if there is no scheme or doesn't look like a mail
 * address. This should work in most cases, and let us click on strings
 * like "www.gnome.org".
 */
#if 0
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
#endif

#ifdef HAVE_PLATFORM_X11
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
#elif defined(HAVE_PLATFORM_OSX)
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
#ifdef HAVE_PLATFORM_X11
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
#elif defined(HAVE_PLATFORM_OSX)
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
#ifdef HAVE_PLATFORM_X11
	gtk_window_set_default_icon_name (name);
#elif defined(HAVE_PLATFORM_OSX)
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
#ifdef HAVE_PLATFORM_X11
	/* Nothing for now. */
#elif defined(HAVE_PLATFORM_OSX)
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
