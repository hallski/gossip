/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include <glade/glade.h>

#ifdef HAVE_PLATFORM_X11
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#elif defined (HAVE_PLATFORM_OSX)
#include <Cocoa/Cocoa.h>
#elif defined (HAVE_PLATFORM_WIN32)
#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif /* HAVE_WINDOWS_H */
#ifdef HAVE_SHELLAPI_H
#include <shellapi.h>
#endif /* HAVE_SHELLAPI_H */
#endif

#ifdef HAVE_LIBNOTIFY
#include "gossip-notify.h"
#endif

#include "gossip-app.h"
#include "gossip-ui-utils.h"

#define DEBUG_DOMAIN "UiUtils"

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
static gchar *
url_fixup (const gchar *url)
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

#if GTK_CHECK_VERSION (2, 14, 0)

gboolean
gossip_url_show (const char *url)
{
    gchar  *real_url;
    GError *error = NULL;

    gossip_debug (DEBUG_DOMAIN, "Opening URL:'%s'...", url);

    real_url = url_fixup (url);
    gtk_show_uri (NULL, real_url, GDK_CURRENT_TIME, &error);

    if (error) {
        g_warning ("Couldn't show URL:'%s'", real_url);
        g_error_free (error);
        g_free (real_url);

        return FALSE;
    }

    g_free (real_url);

    return TRUE;
}

#elif defined(HAVE_PLATFORM_OSX)

gboolean
gossip_url_show (const char *url)
{
    gchar             *real_url;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSString          *string;

    gossip_debug (DEBUG_DOMAIN, "Opening URL:'%s'...", url);

    real_url = url_fixup (url);

    string = [NSString stringWithUTF8String: real_url];
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:string]];

    [pool release];

    g_free (real_url);

    return TRUE;
}

#elif defined(HAVE_PLATFORM_WIN32)

#ifdef HAVE_SHELLAPI_H

static const char *
url_show_error_string (gint error)
{
#ifdef HAVE_WINDOWS_H
    switch (error) {
    case 0:                      
        return "The operating system is out of memory or resources.";
    case ERROR_FILE_NOT_FOUND:   
        return "The specified file was not found.";
    case ERROR_PATH_NOT_FOUND:   
        return "The specified path was not found."; 
    case ERROR_BAD_FORMAT:       
        return "The .exe file is invalid (non-Microsoft Win32® .exe "
            "or error in .exe image).";
    case SE_ERR_ACCESSDENIED:    
        return "The operating system denied access to the specified file."; 
    case SE_ERR_ASSOCINCOMPLETE: 
        return "The file name association is incomplete or invalid."; 
    case SE_ERR_DDEBUSY:         
        return "The Dynamic Data Exchange (DDE) transaction could "
            "not be completed because other DDE transactions were "
            "being processed.";
    case SE_ERR_DDEFAIL:         
        return "The DDE transaction failed.";
    case SE_ERR_DDETIMEOUT:      
        return "The DDE transaction could not be completed because "
            "the request timed out.";
    case SE_ERR_DLLNOTFOUND:     
        return "The specified dynamic-link library (DLL) was not found.";
        /*     case SE_ERR_FNF: return "The specified file was not found."; */
    case SE_ERR_NOASSOC:         
        return "There is no application associated with the given file "
            "name extension. This error will also be returned if you "
            "attempt to print a file that is not printable.";
    case SE_ERR_OOM:             
        return "There was not enough memory to complete the operation.";
        /*     case SE_ERR_PNF: return "The specified path was not found."; */
    case SE_ERR_SHARE:           
        return "A sharing violation occurred.";
    }
#endif /* HAVE_WINDOWS_H */

    return "";
}
#endif /* HAVE_SHELLAPI_H */

gboolean 
gossip_url_show (const gchar *url)
{
    gchar     *real_url;
    gboolean   success = TRUE;
    HINSTANCE  error;

    g_return_val_if_fail (url != NULL, FALSE);

    real_url = url_fixup (url);

#ifdef HAVE_SHELLAPI_H    
    gossip_debug (DEBUG_DOMAIN, "Opening URL:'%s'...", real_url);

    error = ShellExecute ((HWND)NULL, /* parent window */
                          "open",     /* verb */
                          real_url,   /* file */
                          NULL,       /* parameters */
                          NULL,       /* path */
                          SW_SHOWNORMAL);
  
    if ((gint)error <= 32) {
        g_warning ("Failed to open URL:'%s', error:%d->'%s'", 
                   real_url, 
                   (gint) error, 
                   url_show_error_string ((gint) error));
        success = FALSE;
    }

#else  /* HAVE_SHELLAPI_H */
    g_warning ("Failed to open url:'%s', operation not supported on this platform", 
               real_url);
    success = FALSE;
#endif /* HAVE_SHELLAPI_H */

    g_free (real_url);

    return success;
}

#else /* GTK 2.14.0|HAVE_PLATFORM_{OSX|WIN32) */ 

void
gossip_url_show (const gchar *url)
{
    gchar *real_url;
    gchar *command;

    real_url = url_fixup (url);

    command = g_strconcat ("xdg-open ", real_url, NULL);
    if (!g_spawn_command_line_async (command, NULL)) {
        g_free (command);

        if (!command = g_strconcat ("gnome-open ", real_url, NULL)) {
            g_free (command);

            if (!command = g_strconcat ("exo-open ", real_url, NULL)) {
                g_warning ("Failed to open url:'%s'", real_url);
            }
        }
    }

    g_free (command);
    g_free (real_url);
}

#endif /* GTK 2.14.0|HAVE_PLATFORM_{OSX|WIN32) */ 

void
gossip_help_show (void)
{
    GtkWidget *dialog;
    gchar *message;
#if GTK_CHECK_VERSION (2, 14, 0)
    GError    *err = NULL;

    if (gtk_show_uri (NULL, "ghelp:gossip", GDK_CURRENT_TIME, &err)) {
        return;
    }

    message = g_strdup (err->message);
    g_error_free (err);
#else  /* GTK_CHECK_VERSION (2, 14, 0) */
    gchar     *command;
    GError    *err = NULL;

    command = g_strconcat ("gnome-open ", "ghelp:gossip", NULL);
    if (g_spawn_command_line_async (command, &error)) {
        g_free (command);
        return;
    }

    g_free (command);

    message = g_strdup (_("Online help is not supported on this platform."));
#endif  /* GTK_CHECK_VERSION (2, 14, 0) */

    dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (gossip_app_get_window ()),
                                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_CLOSE,
                                                 "<b>%s</b>\n\n%s",
                                                 _("Could not display the help contents."),
                                                 message);

    g_signal_connect_swapped (dialog, "response",
                              G_CALLBACK (gtk_widget_destroy),
                              dialog);

    gtk_widget_show (dialog);

    g_free (message);
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
#ifndef GDK_WINDOWING_QUARTZ
    gtk_window_set_default_icon_name (name);
#else
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
