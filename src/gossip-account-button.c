/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Imendio AB
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libgossip/gossip-protocol.h>
#include <libgossip/gossip-session.h>

#include "gossip-marshal.h"
#include "gossip-app.h"
#include "gossip-ui-utils.h"
#include "gossip-accounts-window.h"
#include "gossip-account-info-dialog.h"
#include "gossip-account-button.h"

#define CONNECTING_DRAW_TIME      500 /* ms */

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
	GOSSIP_TYPE_ACCOUNT_BUTTON, GossipAccountButtonPriv))

#define d(x) x


typedef struct {
	GossipAccount *account;

	guint          timeout_id;
	gboolean       pixelate;
} GossipAccountButtonPriv;


static void       account_button_finalize                 (GObject             *object);
static GtkWidget *account_button_create_menu              (GossipAccountButton *account_button);
static gboolean   account_button_button_press_event_cb    (GtkButton           *button,
							   GdkEventButton      *event,
							   GossipAccountButton *account_button);
static void       account_button_edit_activate_cb         (GtkWidget           *menuitem,
							   GossipAccount       *account);
static void       account_button_connection_activate_cb   (GtkWidget           *menuitem,
							   GossipAccount       *account);
static gboolean   account_button_connecting_timeout_cb    (GossipAccountButton *account_button);
static void       account_button_protocol_connecting_cb   (GossipSession       *session,
							   GossipAccount       *account,
							   gboolean             connecting,
							   GossipAccountButton *account_button);
static void       account_button_protocol_connected_cb    (GossipSession       *session,
							   GossipAccount       *account,
							   GossipProtocol      *protocol,
							   GossipAccountButton *account_button);
static void       account_button_protocol_disconnected_cb (GossipSession       *session,
							   GossipAccount       *account,
							   GossipProtocol      *protocol,
							   GossipAccountButton *account_button);
static void       account_button_protocol_error_cb        (GossipSession       *session,
							   GossipProtocol      *protocol,
							   GError              *error,
							   GossipAccountButton *account_button);


G_DEFINE_TYPE (GossipAccountButton, gossip_account_button, GTK_TYPE_BUTTON);


static void
gossip_account_button_class_init (GossipAccountButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = account_button_finalize;

	g_type_class_add_private (object_class, sizeof (GossipAccountButtonPriv));
}

static void
gossip_account_button_init (GossipAccountButton *account_button)
{
	GossipSession *session;

	/* We use this instead of "clicked" to get the button to disappear when
	 * clicking on the label inside the button.
	 */
	g_signal_connect (GTK_BUTTON (account_button),
			  "button-press-event",
			  G_CALLBACK (account_button_button_press_event_cb),
			  account_button);

	session = gossip_app_get_session ();

	g_signal_connect (session, "protocol-connecting",
			  G_CALLBACK (account_button_protocol_connecting_cb),
			  account_button);

	g_signal_connect (session, "protocol-connected",
			  G_CALLBACK (account_button_protocol_connected_cb),
			  account_button);

	g_signal_connect (session, "protocol-disconnected",
			  G_CALLBACK (account_button_protocol_disconnected_cb),
			  account_button);

	g_signal_connect (session, "protocol-error",
			  G_CALLBACK (account_button_protocol_error_cb),
			  account_button);
}

static void
account_button_finalize (GObject *object)
{
	GossipSession           *session;
	GossipAccountButton     *account_button;
	GossipAccountButtonPriv *priv;

	account_button = GOSSIP_ACCOUNT_BUTTON (object);
	priv = GET_PRIV (object);

	if (priv->account) {
		g_object_unref (priv->account);
	}

	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id); 
		priv->timeout_id = 0; 
	}

	session = gossip_app_get_session ();

	g_signal_handlers_disconnect_by_func (session, 
					      account_button_protocol_connecting_cb, 
					      account_button);
	g_signal_handlers_disconnect_by_func (session, 
					      account_button_protocol_connected_cb, 
					      account_button);
	g_signal_handlers_disconnect_by_func (session, 
					      account_button_protocol_disconnected_cb, 
					      account_button);
	g_signal_handlers_disconnect_by_func (session, 
					      account_button_protocol_error_cb, 
					      account_button);

	G_OBJECT_CLASS (gossip_account_button_parent_class)->finalize (object);
}

static void
account_button_align_menu_func (GtkMenu             *menu,
				gint                *x,
				gint                *y,
				gboolean            *push_in,
				GossipAccountButton *account_button)
{
	GtkWidget      *button;
	GtkRequisition  req;
	GdkScreen      *screen;
	gint            width, height;
	gint            screen_height;

	button = GTK_WIDGET (account_button);
	
	gtk_widget_size_request (GTK_WIDGET (menu), &req);
  
	gdk_window_get_origin (GTK_BUTTON (button)->event_window, x, y);
	gdk_drawable_get_size (GTK_BUTTON (button)->event_window, &width, &height);

	*y += height;

	screen = gtk_widget_get_screen (GTK_WIDGET (menu));
	
	/* Clamp to screen size. */
	screen_height = gdk_screen_get_height (screen) - *y;	
	if (req.height > screen_height) {
		/* It doesn't fit, so we see if we have the minimum space
		 * needed.
		 */
		if (req.height > screen_height && *y - height > screen_height) {
			/* Put the menu above the button instead. */
			screen_height = *y - height;
			*y -= (req.height + height);
			if (*y < 0) {
				*y = 0;
			}
		}
	}
}

static void
account_button_show_popup (GossipAccountButton *account_button)
{
	GossipAccountButtonPriv *priv;
	GtkWidget               *menu;

	priv = GET_PRIV (account_button);
	
	menu = account_button_create_menu (account_button);

	gtk_menu_popup (GTK_MENU (menu),
			NULL, NULL,
			(GtkMenuPositionFunc)account_button_align_menu_func,
			account_button,
			1,
			gtk_get_current_event_time ());
}

static gboolean
account_button_button_press_event_cb (GtkButton           *button,
				      GdkEventButton      *event,
				      GossipAccountButton *account_button)
{
	GossipAccountButtonPriv *priv;

	if (event->type == GDK_2BUTTON_PRESS ||
	    event->type == GDK_3BUTTON_PRESS) {
		return FALSE;
	}

	priv = GET_PRIV (account_button);
	
	switch (event->button) {
	case 1:
		account_button_show_popup (account_button);
		break;
	case 3:
		gossip_account_info_dialog_show (priv->account);
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

static void
account_button_edit_activate_cb (GtkWidget     *menuitem,
				 GossipAccount *account)
{
       gossip_account_info_dialog_show (account);
}

static void
account_button_connection_activate_cb (GtkWidget     *menuitem,
				       GossipAccount *account)
{
	GossipSession *session;
	gboolean       is_connected;

	session = gossip_app_get_session ();
	is_connected = gossip_session_is_connected (session, account);

	if (!is_connected) {
		gossip_session_connect (session, account, FALSE);
	} else {
		gossip_session_disconnect (session, account);
	}
}
 
static GtkWidget *
account_button_create_menu (GossipAccountButton *account_button)
{
	GossipAccountButtonPriv *priv;
	GossipSession           *session;
	GtkWidget               *menu;
	GtkWidget               *item;
	gboolean                 is_connected;

	priv = GET_PRIV (account_button);

	if (!priv->account) {
		return NULL;
	}

	menu = gtk_menu_new ();
	
	session = gossip_app_get_session ();
	is_connected = gossip_session_is_connected (session, priv->account);
	if (!is_connected) {
		item = gtk_image_menu_item_new_from_stock (GTK_STOCK_CONNECT, NULL);
		gtk_label_set_text (GTK_LABEL (GTK_BIN (item)->child), _("Connect"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
 		g_signal_connect (item, "activate",  
 				  G_CALLBACK (account_button_connection_activate_cb), priv->account); 
	} else {
		item = gtk_image_menu_item_new_from_stock (GTK_STOCK_DISCONNECT, NULL);
		gtk_label_set_text (GTK_LABEL (GTK_BIN (item)->child), _("Disconnect"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
 		g_signal_connect (item, "activate",  
 				  G_CALLBACK (account_button_connection_activate_cb), priv->account); 
	}

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_EDIT, NULL);
	gtk_label_set_text (GTK_LABEL (GTK_BIN (item)->child), _("Edit"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
 	g_signal_connect (item, "activate", 
			  G_CALLBACK (account_button_edit_activate_cb), priv->account); 

	gtk_widget_show_all (menu);

	return menu;
}

static gboolean
account_button_connecting_timeout_cb (GossipAccountButton *account_button)
{
	GossipAccountButtonPriv *priv;

	GtkWidget               *image;
	GdkPixbuf               *pixbuf = NULL;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_BUTTON (account_button), FALSE);

	priv = GET_PRIV (account_button);

	if (!priv->account) {
		return FALSE;
	}

	image = gtk_button_get_image (GTK_BUTTON (account_button));
	if (!image) {
		image = gtk_image_new ();
		gtk_button_set_image (GTK_BUTTON (account_button), image);
	}
	
	priv->pixelate = !priv->pixelate;

	pixbuf = gossip_ui_utils_get_pixbuf_from_account (priv->account);
	gdk_pixbuf_saturate_and_pixelate (pixbuf, pixbuf, 1.0, priv->pixelate);

	gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
	g_object_unref (pixbuf);

	return TRUE;
}

static void
account_button_protocol_connecting_cb (GossipSession       *session,
				       GossipAccount       *account,
				       gboolean             connecting,
				       GossipAccountButton *account_button)
{
	GossipAccountButtonPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT_BUTTON (account_button));

	priv = GET_PRIV (account_button);

	if (!gossip_account_equal (account, priv->account)) {
		return;
	}

	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id); 
		priv->timeout_id = 0; 
	}

	if (connecting) {
		priv->pixelate = FALSE;
		priv->timeout_id = g_timeout_add (CONNECTING_DRAW_TIME, 
						  (GSourceFunc)account_button_connecting_timeout_cb,
						  account_button);
	}
}

static void
account_button_protocol_connected_cb (GossipSession       *session,
				      GossipAccount       *account,
				      GossipProtocol      *protocol,
				      GossipAccountButton *account_button)
{
	GossipAccountButtonPriv *priv;

	priv = GET_PRIV (account_button);

	if (!gossip_account_equal (account, priv->account)) {
		return;
	}

	gossip_account_button_set_status (account_button, TRUE);
}

static void
account_button_protocol_disconnected_cb (GossipSession       *session,
					 GossipAccount       *account,
					 GossipProtocol      *protocol,
					 GossipAccountButton *account_button)
{
	GossipAccountButtonPriv *priv;

	priv = GET_PRIV (account_button);

	if (!gossip_account_equal (account, priv->account)) {
		return;
	}

	gossip_account_button_set_status (account_button, FALSE);
}

static void
account_button_protocol_error_cb (GossipSession       *session,
				  GossipProtocol      *protocol,
				  GError              *error,
				  GossipAccountButton *account_button)
{
	GossipAccountButtonPriv *priv;

	priv = GET_PRIV (account_button);

	/* FIXME: get account first */
/* 	if (!gossip_account_equal (account, priv->account)) { */
/* 		return; */
/* 	} */

	const gchar *str1 = _("Unknown error.");
	const gchar *str2 = _("Check your connection details.");
	
	switch (error->code) {
	case GossipProtocolErrorNoConnection:
		d(g_print ("AccountButton: Could not create connection, check host/port, etc!\n"));
		str1 = _("Connection refused.");
		str2 = _("Perhaps you are trying to connect to the wrong port?\n"
			 "Perhaps the service is not currently running?");
		break;
	case GossipProtocolErrorNoSuchHost:
		d(g_print ("AccountButton: No such host while trying to connect!\n"));
		str1 = _("Server address could not be resolved.");
		break;
	case GossipProtocolErrorTimedOut:
		d(g_print ("AccountButton: Log in timed out!\n"));
		str1 = _("Connection timed out.");
		str2 = _("Perhaps the server is not running this service.");
		break;
	case GossipProtocolErrorAuthFailed:
		d(g_print ("AccountButton: Authentication failed!\n"));
		str1 = _("Authentication failed.");
		str2 = _("Check your username and password are correct.");
		break;
	}

	gossip_account_button_set_status (account_button, FALSE);
}

GtkWidget *
gossip_account_button_new (void)
{
	GtkWidget *account_button;

	account_button = g_object_new (GOSSIP_TYPE_ACCOUNT_BUTTON, NULL);

	g_object_set (account_button, 
		      "relief", GTK_RELIEF_NONE,
		      "label", "",
		      NULL);
	
	return account_button;
}

void
gossip_account_button_set_account (GossipAccountButton *account_button,
				   GossipAccount       *account)
{
	GossipAccountButtonPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT_BUTTON (account_button));
	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
	
	priv = GET_PRIV (account_button);

	if (priv->account) {
		g_object_unref (priv->account);
	}

	priv->account = g_object_ref (account);

	gossip_account_button_set_status (account_button, FALSE);
}

void
gossip_account_button_set_status (GossipAccountButton *account_button,
				  gboolean             online)
{
	GossipAccountButtonPriv *priv;

	GtkWidget               *image;
	GdkPixbuf               *pixbuf = NULL;
	
	g_return_if_fail (GOSSIP_IS_ACCOUNT_BUTTON (account_button));

	priv = GET_PRIV (account_button);

	if (!priv->account) {
		return;
	}

	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id); 
		priv->timeout_id = 0; 
	}

	image = gtk_button_get_image (GTK_BUTTON (account_button));
	if (!image) {
		image = gtk_image_new ();
		gtk_button_set_image (GTK_BUTTON (account_button), image);
	}

	pixbuf = gossip_ui_utils_get_pixbuf_from_account_status (priv->account, online);
	gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
	g_object_unref (pixbuf);
}



