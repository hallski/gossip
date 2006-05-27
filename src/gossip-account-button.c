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
#include "gossip-accounts-dialog.h"
#include "gossip-account-button.h"

#define DEBUG_MSG(x)  
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n");  */

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_ACCOUNT_BUTTON, GossipAccountButtonPriv))

#define CONNECTING_DRAW_TIME  500 /* ms */

typedef struct {
	GossipAccount *account;

	GError        *last_error;

	guint          timeout_id;
	gboolean       pixelate;

	gboolean       connected;
	gboolean       connecting;

} GossipAccountButtonPriv;

static void       account_button_finalize                  (GObject             *object);
static void       account_button_align_menu_func           (GtkMenu             *menu,
							    gint                *x,
							    gint                *y,
							    gboolean            *push_in,
							    GossipAccountButton *account_button);
static void       account_button_menu_selection_done_cb    (GtkMenuShell        *menushell,
							    GossipAccountButton *account_button);
static void       account_button_menu_detach               (GtkWidget           *attach_widget,
							    GtkMenu             *menu);
static void       account_button_menu_popup                (GossipAccountButton *account_button);
static void       account_button_clicked_cb                (GtkWidget           *widget,
							    gpointer             user_data);
static GtkWidget *account_button_create_menu               (GossipAccountButton *account_button);
static void       account_button_update_tooltip            (GossipAccountButton *account_button);
static void       account_button_edit_activate_cb          (GtkWidget           *menuitem,
							    GossipAccount       *account);
static void       account_button_connection_activate_cb    (GtkWidget           *menuitem,
							    GossipAccountButton *account_button);
static gboolean   account_button_connecting_timeout_cb     (GossipAccountButton *account_button);
static void       account_button_protocol_connecting_cb    (GossipSession       *session,
							    GossipAccount       *account,
							    GossipAccountButton *account_button);
static void       account_button_protocol_disconnecting_cb (GossipSession       *session,
							    GossipAccount       *account,
							    GossipAccountButton *account_button);
static void       account_button_protocol_connected_cb     (GossipSession       *session,
							    GossipAccount       *account,
							    GossipProtocol      *protocol,
							    GossipAccountButton *account_button);
static void       account_button_protocol_disconnected_cb  (GossipSession       *session,
							    GossipAccount       *account,
							    GossipProtocol      *protocol,
							    GossipAccountButton *account_button);
static void       account_button_protocol_error_cb         (GossipSession       *session,
							    GossipProtocol      *protocol,
							    GossipAccount       *account,
							    GError              *error,
							    GossipAccountButton *account_button);
static void       account_button_account_notify_cb         (GossipAccount       *account,
							    GParamSpec          *param,
							    GossipAccountButton *account_button);

G_DEFINE_TYPE (GossipAccountButton, gossip_account_button, GTK_TYPE_TOGGLE_TOOL_BUTTON);

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

        g_signal_connect (account_button, "clicked",
                          G_CALLBACK (account_button_clicked_cb),
                          NULL);

	session = gossip_app_get_session ();

	g_signal_connect (session, "protocol-connecting",
			  G_CALLBACK (account_button_protocol_connecting_cb),
			  account_button);

	g_signal_connect (session, "protocol-connected",
			  G_CALLBACK (account_button_protocol_connected_cb),
			  account_button);

	g_signal_connect (session, "protocol-disconnecting",
			  G_CALLBACK (account_button_protocol_disconnecting_cb),
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

	g_signal_handlers_disconnect_by_func (priv->account, 
					      account_button_account_notify_cb, 
					      account_button);
	if (priv->account) {
		g_object_unref (priv->account);
	}

	if (priv->last_error) {
		g_error_free (priv->last_error);
		priv->last_error = NULL;
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
					      account_button_protocol_disconnecting_cb, 
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
	GtkWidget      *widget;
	GtkRequisition  req;
	GdkScreen      *screen;
	gint            screen_height;

	widget = GTK_WIDGET (account_button);

	gtk_widget_size_request (GTK_WIDGET (menu), &req);

	gdk_window_get_origin (widget->window, x, y); 

	*x += widget->allocation.x + 1;
	*y += widget->allocation.y;

	screen = gtk_widget_get_screen (GTK_WIDGET (menu));
	screen_height = gdk_screen_get_height (screen);	

	if (req.height > screen_height) {
		/* Too big for screen height anyway. */
		*y = 0;
		return;
	}

	if ((*y + req.height + widget->allocation.height) > screen_height) {
		/* Can't put it below the button. */
		*y -= req.height;
		*y += 1;
	} else {
		/* Put menu below button. */
		*y += widget->allocation.height;
		*y -= 1;
	}

	*push_in = FALSE;
}

static void
account_button_menu_selection_done_cb (GtkMenuShell        *menushell,
				       GossipAccountButton *account_button)
{
 	gtk_widget_destroy (GTK_WIDGET (menushell)); 

	g_signal_handlers_block_by_func (account_button,
					 account_button_clicked_cb,
					 NULL);

	gtk_toggle_tool_button_set_active (
		GTK_TOGGLE_TOOL_BUTTON (account_button), FALSE);

	g_signal_handlers_unblock_by_func (account_button,
					   account_button_clicked_cb,
					   NULL);
}

static void   
account_button_menu_detach (GtkWidget *attach_widget,
			    GtkMenu   *menu)
{
	/* We don't need to do anything, but attaching the menu means
	 * we don't own the ref count and it is cleaned up properly. 
	 */
}

static void
account_button_menu_popup (GossipAccountButton *account_button)
{
	GtkWidget *menu;

	menu = account_button_create_menu (account_button);

	g_signal_connect (menu, "selection-done", 
			  G_CALLBACK (account_button_menu_selection_done_cb),
			  account_button);

	gtk_menu_attach_to_widget (GTK_MENU (menu), 
				   GTK_WIDGET (account_button), 
				   account_button_menu_detach);

	gtk_menu_popup (GTK_MENU (menu),
			NULL, NULL,
			(GtkMenuPositionFunc) account_button_align_menu_func,
			account_button,
			1,
			gtk_get_current_event_time ());
}

static void
account_button_clicked_cb (GtkWidget *widget,
			   gpointer   user_data)
{
	account_button_menu_popup (GOSSIP_ACCOUNT_BUTTON (widget));
}

static void
account_button_edit_activate_cb (GtkWidget     *menuitem,
				 GossipAccount *account)
{
	gossip_accounts_dialog_show (account);
}

static void
account_button_connection_activate_cb (GtkWidget           *menuitem,
				       GossipAccountButton *account_button)
{
	GossipAccountButtonPriv *priv;
	GossipSession           *session;

	priv = GET_PRIV (account_button);

	session = gossip_app_get_session ();

	if (!priv->connected && !priv->connecting) {
		gossip_session_connect (session, priv->account, FALSE);
	} else {
		gossip_session_disconnect (session, priv->account);
	}
}
 
static GtkWidget *
account_button_create_menu (GossipAccountButton *account_button)
{
	GossipAccountButtonPriv *priv;
	GtkWidget               *menu;
	GtkWidget               *item;

	priv = GET_PRIV (account_button);

	if (!priv->account) {
		return NULL;
	}

	menu = gtk_menu_new ();
	
	if (!priv->connected) {
		if (priv->connecting) {
			item = gtk_image_menu_item_new_from_stock (GTK_STOCK_STOP, NULL);
			gtk_label_set_text (GTK_LABEL (GTK_BIN (item)->child), _("Stop Connecting"));
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
			g_signal_connect (item, "activate",  
					  G_CALLBACK (account_button_connection_activate_cb), 
					  account_button); 
		} else {
			item = gtk_image_menu_item_new_from_stock (GTK_STOCK_CONNECT, NULL);
			gtk_label_set_text (GTK_LABEL (GTK_BIN (item)->child), _("Connect"));
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
			g_signal_connect (item, "activate",  
					  G_CALLBACK (account_button_connection_activate_cb), 
					  account_button); 
		}
	} else {
		item = gtk_image_menu_item_new_from_stock (GTK_STOCK_DISCONNECT, NULL);
		gtk_label_set_text (GTK_LABEL (GTK_BIN (item)->child), _("Disconnect"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
 		g_signal_connect (item, "activate",  
 				  G_CALLBACK (account_button_connection_activate_cb),
				  account_button); 
	}

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_EDIT, NULL);
	gtk_label_set_text (GTK_LABEL (GTK_BIN (item)->child), _("Edit"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
 	g_signal_connect (item, "activate", 
			  G_CALLBACK (account_button_edit_activate_cb),
			  priv->account); 

	gtk_widget_show_all (menu);

	return menu;
}

static void
account_button_update_tooltip (GossipAccountButton *account_button) 
{
	GossipAccountButtonPriv *priv;
	GtkTooltips             *tooltips;
	const gchar             *status;
	const gchar             *error = NULL;
	const gchar             *suggestion1 = NULL;
	const gchar             *suggestion2 = NULL;
	gchar                   *str;
	gboolean                 has_error = FALSE;

	g_return_if_fail (GOSSIP_IS_ACCOUNT_BUTTON (account_button));
	priv = GET_PRIV (account_button);

	if (!priv->account) {
		return;
	}

	if (priv->connected) {
		status =  _("Connected");
	} else {
		status = priv->connecting ? _("Connecting") : _("Disconnected");
	}

	if (priv->last_error) {
		gint code;
	
		has_error = TRUE;
		
		code = priv->last_error->code;
		error = gossip_protocol_error_to_string (code);

		switch (code) {
		case GOSSIP_PROTOCOL_NO_CONNECTION:
			suggestion1 = _("Perhaps you are trying to connect to the wrong port?");
			suggestion2 = _("Perhaps the service is not currently running?");
			break;
		case GOSSIP_PROTOCOL_NO_SUCH_HOST:
			suggestion1 = _("Check your connection details.");
			break;
		case GOSSIP_PROTOCOL_TIMED_OUT:
			suggestion1 = _("Perhaps the server is not running this service.");
			break;
		case GOSSIP_PROTOCOL_AUTH_FAILED:
			suggestion1 = _("Check your username and password are correct.");
			break;
		default:
			suggestion1 = _("Check your connection details.");
			break;
		}
	}

	str = g_strdup_printf ("%s"  /* account */
			       "%s"  /* line feed */
			       "%s"  /* error */
			       "%s"  /* line feed */
			       "%s"  /* suggestion 1 */
			       "%s"  /* line feed */
			       "%s", /* suggestion 2 */
			       gossip_account_get_id (priv->account),
			       error ? "\n\n" : "",
			       error ? error : "",
			       error ? "\n" : "",
			       suggestion1 ? suggestion1 : "",  
			       suggestion2 ? "\n" : "",
			       suggestion2 ? suggestion2 : "");   

	tooltips = gtk_tooltips_new ();
	gtk_tool_item_set_tooltip (GTK_TOOL_ITEM (account_button), tooltips, str, NULL);

	g_free (str);
}

static gboolean
account_button_connecting_timeout_cb (GossipAccountButton *account_button)
{
	GossipAccountButtonPriv *priv;

	GtkWidget               *image;
	GdkPixbuf               *pixbuf = NULL;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_BUTTON (account_button), FALSE);

	priv = GET_PRIV (account_button);

	DEBUG_MSG (("AccountButton: Account:'%s' timed out trying to connect", 
		    gossip_account_get_id (priv->account)));


	if (!priv->account) {
		return FALSE;
	}

	image = gtk_tool_button_get_icon_widget (GTK_TOOL_BUTTON (account_button));
	if (!image) {
		image = gtk_image_new ();
		gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (account_button),
						 image);
	}
	
	priv->pixelate = !priv->pixelate;

	pixbuf = gossip_pixbuf_from_account_status (priv->account, 
						    GTK_ICON_SIZE_BUTTON, 
						    priv->pixelate);

	gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
	g_object_unref (pixbuf);

	return TRUE;
}

static void
account_button_protocol_connecting_cb (GossipSession       *session,
				       GossipAccount       *account,
				       GossipAccountButton *account_button)
{
	GossipAccountButtonPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT_BUTTON (account_button));

	priv = GET_PRIV (account_button);

	if (!gossip_account_equal (account, priv->account)) {
		return;
	}

	DEBUG_MSG (("AccountButton: Account:'%s' connecting", 
		    gossip_account_get_id (account)));

	if (priv->last_error) {
		g_error_free (priv->last_error);
		priv->last_error = NULL;
	}

	priv->connected = FALSE;
	priv->connecting = TRUE;

	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id); 
		priv->timeout_id = 0; 
	}

	priv->pixelate = FALSE;
	priv->timeout_id = g_timeout_add (CONNECTING_DRAW_TIME, 
					  (GSourceFunc)account_button_connecting_timeout_cb,
					  account_button);

	account_button_update_tooltip (account_button);
}

static void 
account_button_protocol_disconnecting_cb (GossipSession       *session,
					  GossipAccount       *account,
					  GossipAccountButton *account_button)
{
	GossipAccountButtonPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT_BUTTON (account_button));

	priv = GET_PRIV (account_button);

	if (!gossip_account_equal (account, priv->account)) {
		return;
	}

	DEBUG_MSG (("AccountButton: Account:'%s' disconnecting", 
		    gossip_account_get_id (account)));

	priv->connected = FALSE;
	priv->connecting = FALSE;
	
	gossip_account_button_set_status (account_button, FALSE);
	account_button_update_tooltip (account_button);
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

	DEBUG_MSG (("AccountButton: Account:'%s' connected", 
		    gossip_account_get_id (account)));

	if (priv->last_error) {
		g_error_free (priv->last_error);
		priv->last_error = NULL;
	}

	priv->connected = TRUE;
	priv->connecting = FALSE;

	gossip_account_button_set_status (account_button, TRUE);
	account_button_update_tooltip (account_button);
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

	DEBUG_MSG (("AccountButton: Account:'%s' disconnected", 
		    gossip_account_get_id (account)));
	
	priv->connected = FALSE;
	priv->connecting = FALSE;

	gossip_account_button_set_status (account_button, FALSE);
	account_button_update_tooltip (account_button);
}

static void
account_button_protocol_error_cb (GossipSession       *session,
				  GossipProtocol      *protocol,
				  GossipAccount       *account,
				  GError              *error,
				  GossipAccountButton *account_button)
{
	GossipAccountButtonPriv *priv;

	priv = GET_PRIV (account_button);

 	if (!gossip_account_equal (account, priv->account)) { 
 		return; 
 	} 

	DEBUG_MSG (("AccountButton: Account:'%s' error:%d->'%s'", 
		    gossip_account_get_id (account),
		    error->code, 
		    error->message));

	priv->connecting = FALSE;

	if (priv->last_error) {
		g_error_free (priv->last_error);
	}

	priv->last_error = g_error_copy (error);

	gossip_account_button_set_status (account_button, FALSE);
	account_button_update_tooltip (account_button);
}

static void
account_button_account_notify_cb (GossipAccount       *account,
				  GParamSpec          *param,
				  GossipAccountButton *account_button)
{
	gtk_tool_button_set_label (GTK_TOOL_BUTTON (account_button),
				   gossip_account_get_name (account));

	gtk_tool_item_set_is_important (GTK_TOOL_ITEM (account_button),
					gossip_account_get_auto_connect (account));

	account_button_update_tooltip (account_button);
}

GtkWidget *
gossip_account_button_new (void)
{
	GtkWidget *account_button;

	account_button = g_object_new (GOSSIP_TYPE_ACCOUNT_BUTTON, NULL);

	/* Should we show the text for the account? */
	gtk_tool_item_set_is_important (GTK_TOOL_ITEM (account_button),
					FALSE);

	/* Should we show the item when docked horizontally. */
	gtk_tool_item_set_visible_horizontal (GTK_TOOL_ITEM (account_button),
					      TRUE);
	
	return account_button;
}

GossipAccount *
gossip_account_button_get_account (GossipAccountButton *account_button)
{
	GossipAccountButtonPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_BUTTON (account_button), NULL);

	priv = GET_PRIV (account_button);

	return priv->account;
}

void
gossip_account_button_set_account (GossipAccountButton *account_button,
				   GossipAccount       *account)
{
	GossipAccountButtonPriv *priv;
	GossipSession           *session;

	g_return_if_fail (GOSSIP_IS_ACCOUNT_BUTTON (account_button));
	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
	
	priv = GET_PRIV (account_button);

	if (priv->account) {
		g_signal_handlers_disconnect_by_func (account, 
						      account_button_account_notify_cb, 
						      account_button);
		g_object_unref (priv->account);
	}

	gtk_tool_button_set_label (GTK_TOOL_BUTTON (account_button),
				   gossip_account_get_name (account));

	/* Should we show the text for the account? */
	gtk_tool_item_set_is_important (GTK_TOOL_ITEM (account_button),
					gossip_account_get_auto_connect (account));

	priv->account = g_object_ref (account);
	g_signal_connect (priv->account, "notify", 
			  G_CALLBACK (account_button_account_notify_cb), 
			  account_button);

	session = gossip_app_get_session ();
	priv->connected = gossip_session_is_connected (session, priv->account);

	gossip_account_button_set_status (account_button, FALSE);
	account_button_update_tooltip (account_button);
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

	image = gtk_tool_button_get_icon_widget (GTK_TOOL_BUTTON (account_button));
	if (!image) {
		image = gtk_image_new ();
		gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (account_button),
						 image);
	}

	if (priv->last_error) {
		pixbuf = gossip_pixbuf_from_account_error (priv->account, 
							   GTK_ICON_SIZE_BUTTON);
	} else {
		pixbuf = gossip_pixbuf_from_account_status (priv->account, 
							    GTK_ICON_SIZE_BUTTON,
							    online);
	}

	gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
	g_object_unref (pixbuf);
}

gboolean
gossip_account_button_is_important (GossipAccountButton *account_button)
{
	GossipAccountButtonPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_BUTTON (account_button), FALSE);

	priv = GET_PRIV (account_button);

	if (!priv->account) {
		return FALSE;
	}

	return gossip_account_get_auto_connect (priv->account);
}

gboolean
gossip_account_button_is_error_shown (GossipAccountButton *account_button)
{
	GossipAccountButtonPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_BUTTON (account_button), FALSE);

	priv = GET_PRIV (account_button);

	return (priv->last_error != NULL ? TRUE : FALSE);
}
