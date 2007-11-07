/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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

#include "config.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libgossip/gossip-jabber.h>
#include <libgossip/gossip-jid.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-utils.h>
#include <libgossip/gossip-vcard.h>
#include <libgossip/gossip-account-manager.h>
#include <libgossip/gossip-debug.h>

#include "gossip-account-widget-jabber.h"
#include "gossip-app.h"
#include "gossip-glade.h"
#include "gossip-marshal.h"
#include "gossip-ui-utils.h"

#define DEBUG_DOMAIN "AccountWidgetJabber"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_ACCOUNT_WIDGET_JABBER, GossipAccountWidgetJabberPriv))

typedef struct _GossipAccountWidgetJabberPriv GossipAccountWidgetJabberPriv;

struct _GossipAccountWidgetJabberPriv {
	GossipAccount *account;
	gboolean       account_changed;

	GtkWidget     *vbox_settings;

	GtkWidget     *button_register;

	GtkWidget     *button_forget;
	GtkWidget     *button_change_password;

	GtkWidget     *entry_id;
	GtkWidget     *entry_password;
	GtkWidget     *entry_resource;
	GtkWidget     *entry_server;
	GtkWidget     *entry_port;

	GtkWidget     *checkbutton_ssl;

	gboolean       registering;
	gboolean       changing_password;
};

static void     gossip_account_widget_jabber_class_init                 (GossipAccountWidgetJabberClass *klass);
static void     gossip_account_widget_jabber_init                       (GossipAccountWidgetJabber      *settings);
static void     account_widget_jabber_dispose                           (GObject                        *object);
static void     account_widget_jabber_save                              (GossipAccountWidgetJabber      *settings);
static void     account_widget_jabber_password_changed_cb               (GossipAccount                  *account,
									 GParamSpec                     *param,
									 GossipAccountWidgetJabber      *settings);
static void     account_widget_jabber_protocol_connected_cb             (GossipSession                  *session,
									 GossipAccount                  *account,
									 GossipJabber                   *jabber,
									 GossipAccountWidgetJabber      *settings);
static void     account_widget_jabber_protocol_disconnected_cb          (GossipSession                  *session,
									 GossipAccount                  *account,
									 GossipJabber                   *jabber,
									 gint                            reason,
									 GossipAccountWidgetJabber      *settings);
static void     account_widget_jabber_protocol_error_cb                 (GossipSession                  *session,
									 GossipJabber                   *jabber,
									 GossipAccount                  *account,
									 GError                         *error,
									 GossipAccountWidgetJabber      *settings);
static void     account_widget_jabber_realize_cb                        (GtkWidget                      *widget,
									 gpointer                        user_data);
static gboolean account_widget_jabber_entry_focus_cb                    (GtkWidget                      *widget,
									 GdkEventFocus                  *event,
									 GossipAccountWidgetJabber      *settings);
static void     account_widget_jabber_entry_changed_cb                  (GtkWidget                      *widget,
									 GossipAccountWidgetJabber      *settings);
static void     account_widget_jabber_checkbutton_toggled_cb            (GtkWidget                      *widget,
									 GossipAccountWidgetJabber      *settings);
static void     account_widget_jabber_entry_port_insert_text_cb         (GtkEditable                    *editable,
									 gchar                          *new_text,
									 gint                            len,
									 gint                           *position,
									 GossipAccountWidgetJabber      *settings);
static void     account_widget_jabber_register_cancel                   (GossipAccountWidgetJabber      *settings);
static void     account_widget_jabber_change_password_cancel            (GossipAccountWidgetJabber      *settings);
static void     account_widget_jabber_register_cb                       (GossipResult                    result,
									 GError                         *error,
									 GossipAccountWidgetJabber      *settings);
static void     account_widget_jabber_button_register_clicked_cb        (GtkWidget                      *button,
									 GossipAccountWidgetJabber      *settings);
static void     account_widget_jabber_button_forget_clicked_cb          (GtkWidget                      *button,
									 GossipAccountWidgetJabber      *settings);
static void     account_widget_jabber_cp_entry_activate_cb              (GtkWidget                      *entry,
									 GtkDialog                      *dialog);
static void     account_widget_jabber_cp_response_cb                    (GtkWidget                      *dialog,
									 gint                            response,
									 GossipAccountWidgetJabber      *settings);
static void     account_widget_jabber_button_change_password_clicked_cb (GtkWidget                      *button,
									 GossipAccountWidgetJabber      *settings);
static void     account_widget_jabber_setup                             (GossipAccountWidgetJabber      *settings,
									 GossipAccount                  *account);

G_DEFINE_TYPE (GossipAccountWidgetJabber, gossip_account_widget_jabber, GTK_TYPE_VBOX);

static void
gossip_account_widget_jabber_class_init (GossipAccountWidgetJabberClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = account_widget_jabber_dispose;

	g_type_class_add_private (object_class, sizeof (GossipAccountWidgetJabberPriv));
}

static void
gossip_account_widget_jabber_init (GossipAccountWidgetJabber *settings)
{
	GossipAccountWidgetJabberPriv *priv;
	GladeXML                      *glade;
	GtkSizeGroup                  *size_group;
	GtkWidget                     *label_id, *label_password;
	GtkWidget                     *label_server, *label_resource, *label_port; 

	priv = GET_PRIV (settings);

	glade = gossip_glade_get_file ("main.glade",
				       "vbox_jabber_settings",
				       NULL,
				       "vbox_jabber_settings", &priv->vbox_settings,
				       "button_register", &priv->button_register,
				       "button_forget", &priv->button_forget,
				       "button_change_password", &priv->button_change_password,
				       "label_id", &label_id,
				       "label_password", &label_password,
				       "label_resource", &label_resource,
				       "label_server", &label_server,
				       "label_port", &label_port,
				       "entry_id", &priv->entry_id,
				       "entry_password", &priv->entry_password,
				       "entry_resource", &priv->entry_resource,
				       "entry_server", &priv->entry_server,
				       "entry_port", &priv->entry_port,
				       "checkbutton_ssl", &priv->checkbutton_ssl,
				       NULL);

	gossip_glade_connect (glade, 
			      settings,
			      "button_register", "clicked", account_widget_jabber_button_register_clicked_cb,
			      "button_forget", "clicked", account_widget_jabber_button_forget_clicked_cb,
			      "button_change_password", "clicked", account_widget_jabber_button_change_password_clicked_cb,
			      "entry_id", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_password", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_resource", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_server", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_port", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_port", "insert_text", account_widget_jabber_entry_port_insert_text_cb,
			      NULL);

	g_object_unref (glade);

	/* We do this manually so we can block it. */
	g_signal_connect (priv->checkbutton_ssl, "toggled", 
			  G_CALLBACK (account_widget_jabber_checkbutton_toggled_cb),
			  settings);

	g_signal_connect (priv->entry_id, "changed", 
			  G_CALLBACK (account_widget_jabber_entry_changed_cb),
			  settings);
	g_signal_connect (priv->entry_password, "changed", 
			  G_CALLBACK (account_widget_jabber_entry_changed_cb),
			  settings);
	g_signal_connect (priv->entry_password, "changed", 
			  G_CALLBACK (account_widget_jabber_entry_changed_cb),
			  settings);
	g_signal_connect (priv->entry_resource, "changed", 
			  G_CALLBACK (account_widget_jabber_entry_changed_cb),
			  settings);
	g_signal_connect (priv->entry_server, "changed", 
			  G_CALLBACK (account_widget_jabber_entry_changed_cb),
			  settings);
	g_signal_connect (priv->entry_port, "changed", 
			  G_CALLBACK (account_widget_jabber_entry_changed_cb),
			  settings);

	g_signal_connect (settings, "realize",
			  G_CALLBACK (account_widget_jabber_realize_cb),
			  NULL);

	/* Set up remaining widgets */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, label_id);
	gtk_size_group_add_widget (size_group, label_password);
	gtk_size_group_add_widget (size_group, label_resource);
	gtk_size_group_add_widget (size_group, label_server);
	gtk_size_group_add_widget (size_group, label_port);

	g_object_unref (size_group);

	/* Add our vbox and show ourselves */
	gtk_box_pack_start (GTK_BOX (settings), priv->vbox_settings, FALSE, FALSE, 0); 
	gtk_widget_show (GTK_WIDGET (settings));
}

static void
account_widget_jabber_dispose (GObject *object)
{
	GossipAccountWidgetJabber     *settings;
	GossipAccountWidgetJabberPriv *priv;
	GossipSession                 *session;

	settings = GOSSIP_ACCOUNT_WIDGET_JABBER (object);
	priv = GET_PRIV (settings);

	account_widget_jabber_register_cancel (settings);
	account_widget_jabber_change_password_cancel (settings);

	if (priv->account_changed) {
		gossip_debug (DEBUG_DOMAIN, "Disposing of this widget and accounts changed, saving accounts...");
 		account_widget_jabber_save (settings); 
	}

	session = gossip_app_get_session ();

	g_signal_handlers_disconnect_by_func (priv->account,
					      account_widget_jabber_password_changed_cb,
					      settings);

	g_signal_handlers_disconnect_by_func (session,
					      account_widget_jabber_protocol_connected_cb,
					      settings);

	g_signal_handlers_disconnect_by_func (session,
					      account_widget_jabber_protocol_disconnected_cb,
					      settings);

	g_signal_handlers_disconnect_by_func (session,
					      account_widget_jabber_protocol_error_cb,
					      settings);

	g_object_unref (priv->account);
	
	G_OBJECT_CLASS (gossip_account_widget_jabber_parent_class)->dispose (object);
}

static void
account_widget_jabber_save (GossipAccountWidgetJabber *settings)
{
	GossipAccountWidgetJabberPriv *priv;
	GossipSession                 *session;
	GossipAccountManager          *manager;
 	const gchar                   *id;
 	const gchar                   *password;
 	const gchar                   *resource;
 	const gchar                   *server;
 	const gchar                   *port_str;
	guint16                        port;
	gboolean                       use_ssl;

	priv = GET_PRIV (settings);

	gossip_debug (DEBUG_DOMAIN, "Saving settings to account");

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	id = gtk_entry_get_text (GTK_ENTRY (priv->entry_id));
	password = gtk_entry_get_text (GTK_ENTRY (priv->entry_password));
	resource = gtk_entry_get_text (GTK_ENTRY (priv->entry_resource));
	server = gtk_entry_get_text (GTK_ENTRY (priv->entry_server));
	port_str = gtk_entry_get_text (GTK_ENTRY (priv->entry_port));
	port = strtol (port_str, NULL, 10);
	use_ssl = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->checkbutton_ssl));

	gossip_account_set_id (priv->account, id);
	gossip_account_set_password (priv->account, password);
	gossip_account_set_resource (priv->account, resource);
	gossip_account_set_server (priv->account, server);
	gossip_account_set_port (priv->account, port);
	gossip_account_set_use_ssl (priv->account, use_ssl);

	gossip_account_manager_store (manager);

	priv->account_changed = FALSE;
}

static void
account_widget_jabber_password_changed_cb (GossipAccount             *account,
					   GParamSpec                *param,
					   GossipAccountWidgetJabber *settings)
{
	GossipAccountWidgetJabberPriv *priv;
	const gchar                   *password;

	priv = GET_PRIV (settings);

	password = gossip_account_get_password (account);
	gtk_entry_set_text (GTK_ENTRY (priv->entry_password), password ? password : "");
}

static void
account_widget_jabber_protocol_connected_cb (GossipSession             *session,
					     GossipAccount             *account,
					     GossipJabber              *jabber,
					     GossipAccountWidgetJabber *settings)
{
	GossipAccountWidgetJabberPriv *priv;

	priv = GET_PRIV (settings);

	if (gossip_account_equal (account, priv->account)) {
		gtk_widget_set_sensitive (priv->button_register, FALSE);
		gtk_widget_set_sensitive (priv->button_change_password, TRUE);
	}
}

static void
account_widget_jabber_protocol_disconnected_cb (GossipSession             *session,
						GossipAccount             *account,
						GossipJabber              *jabber,
						gint                       reason,
						GossipAccountWidgetJabber *settings)
{
	GossipAccountWidgetJabberPriv *priv;

	priv = GET_PRIV (settings);

	if (gossip_account_equal (account, priv->account)) {
		gtk_widget_set_sensitive (priv->button_register, TRUE);
		gtk_widget_set_sensitive (priv->button_change_password, FALSE);
	}
}

static void
account_widget_jabber_protocol_error_cb (GossipSession             *session,
					 GossipJabber              *jabber,
					 GossipAccount             *account,
					 GError                    *error,
					 GossipAccountWidgetJabber *settings)
{
	GossipAccountWidgetJabberPriv *priv;

	priv = GET_PRIV (settings);

	if (gossip_account_equal (account, priv->account)) {
		/* FIXME: HANDLE ERRORS */
	}	
}

static gboolean
account_widget_jabber_entry_focus_cb (GtkWidget                 *widget,
				      GdkEventFocus             *event,
				      GossipAccountWidgetJabber *settings)
{
	GossipAccountWidgetJabberPriv *priv;

	priv = GET_PRIV (settings);

	if (widget == priv->entry_id) {
		GossipSession *session;
		GossipJabber  *jabber;

		session = gossip_app_get_session ();
		jabber = gossip_session_get_protocol (session,
						      priv->account);

		if (jabber) {
			const gchar *str;

			str = gtk_entry_get_text (GTK_ENTRY (widget));
			if (G_STR_EMPTY (str)) {
				priv->account_changed = FALSE;
				return FALSE;
			}
				
			if (!gossip_jid_string_is_valid (str, FALSE)) {
				str = gossip_account_get_id (priv->account);
				priv->account_changed = FALSE;
			} else {
				gchar *server;
				
				server = gossip_jabber_get_default_server (str);
				gtk_entry_set_text (GTK_ENTRY (priv->entry_server), server);
				g_free (server);
			}
			
			gtk_entry_set_text (GTK_ENTRY (widget), str);
		}
	}

	if (widget == priv->entry_password ||
	    widget == priv->entry_resource ||
	    widget == priv->entry_server) {
		const gchar *str;

		str = gtk_entry_get_text (GTK_ENTRY (widget));
		if (G_STR_EMPTY (str)) {
			if (widget == priv->entry_password) {
				str = gossip_account_get_password (priv->account);
			} else if (widget == priv->entry_resource) {
				str = gossip_account_get_resource (priv->account);
			} else if (widget == priv->entry_server) {
				str = gossip_account_get_server (priv->account);
			}

			gtk_entry_set_text (GTK_ENTRY (widget), str ? str : "");
			priv->account_changed = FALSE;
		}
	}

	if (widget == priv->entry_port) {
		const gchar *str;
		
		str = gtk_entry_get_text (GTK_ENTRY (widget));
		if (G_STR_EMPTY (str)) {
			gchar   *port_str;
			guint16  port;

			port = gossip_account_get_port (priv->account);
			port_str = g_strdup_printf ("%d", port);
			gtk_entry_set_text (GTK_ENTRY (widget), port_str);
			g_free (port_str);

			priv->account_changed = FALSE;
		}
	}

	if (priv->account_changed) {
		gossip_debug (DEBUG_DOMAIN, "Entry changed, saving accounts...");
 		account_widget_jabber_save (settings); 
	}

	return FALSE;
}

static void
account_widget_jabber_entry_changed_cb (GtkWidget                 *widget,
					GossipAccountWidgetJabber *settings)
{
	GossipAccountWidgetJabberPriv *priv;

	priv = GET_PRIV (settings);

	if (widget == priv->entry_port) {
		const gchar *str;
		gint         pnr;

		str = gtk_entry_get_text (GTK_ENTRY (widget));
		pnr = strtol (str, NULL, 10);
		
		if ((pnr <= 0 || pnr >= 65556) && !G_STR_EMPTY (str)) {
			gchar   *port_str;
			guint16  port;
			
			port = gossip_account_get_port (priv->account);
			port_str = g_strdup_printf ("%d", port);
			g_signal_handlers_block_by_func (priv->entry_port, 
							 account_widget_jabber_entry_changed_cb, 
							 widget);
			gtk_entry_set_text (GTK_ENTRY (widget), port_str);
			g_signal_handlers_unblock_by_func (priv->entry_port, 
							   account_widget_jabber_entry_changed_cb, 
							   widget);
			g_free (port_str);

			return;
		}
	} else if (widget == priv->entry_password) {
		const gchar *str;

		str = gtk_entry_get_text (GTK_ENTRY (widget));
		gtk_widget_set_sensitive (priv->button_forget, !G_STR_EMPTY (str));
	}

	/* Save */
	priv->account_changed = TRUE;
}

static void  
account_widget_jabber_checkbutton_toggled_cb (GtkWidget                 *checkbutton,
					      GossipAccountWidgetJabber *settings)
{
	GossipAccountWidgetJabberPriv *priv;

	priv = GET_PRIV (settings);

	if (checkbutton == priv->checkbutton_ssl) {
		guint16        port_with_ssl;
		guint16        port_without_ssl;
		guint16        port;
		gboolean       use_ssl;
		gboolean       changed = FALSE;

		port_with_ssl = gossip_jabber_get_default_port (TRUE);
		port_without_ssl = gossip_jabber_get_default_port (FALSE);

		port = gossip_account_get_port (priv->account);

		use_ssl = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton));

		if (use_ssl) {
			if (port == port_without_ssl) {
				port = port_with_ssl;
				changed = TRUE;
			}
		} else {
			if (port == port_with_ssl) {
				port = port_without_ssl;
				changed = TRUE;
			}
		}
		
		gossip_account_set_port (priv->account, port);
		gossip_account_set_use_ssl (priv->account, use_ssl);

		if (changed) {
			gchar *port_str;

			port_str = g_strdup_printf ("%d", port);
			gtk_entry_set_text (GTK_ENTRY (priv->entry_port), port_str);
			g_free (port_str);
		}
	}

	gossip_debug (DEBUG_DOMAIN, "Checkbutton toggled, saving accounts...");
	account_widget_jabber_save (settings); 
}

static void
account_widget_jabber_entry_port_insert_text_cb (GtkEditable               *editable,
						 gchar                     *new_text,
						 gint                       len,
						 gint                      *position,
						 GossipAccountWidgetJabber *settings)
{
	gint i;

	for (i = 0; i < len; ++i) {
		gchar *ch;

		ch = new_text + i;
		if (!isdigit (*ch)) {
			g_signal_stop_emission_by_name (editable,
							"insert-text");
			return;
		}
	}
}

static void
account_widget_jabber_register_cancel (GossipAccountWidgetJabber *settings)
{
	GossipAccountWidgetJabberPriv *priv;
	GossipSession                 *session;

	priv = GET_PRIV (settings);

	if (!priv->registering) {
		return;
	}

	session = gossip_app_get_session ();
	gossip_session_register_cancel (session, priv->account);

	priv->registering = FALSE;
	gtk_widget_set_sensitive (priv->button_register, TRUE);
	gtk_widget_set_sensitive (priv->vbox_settings, TRUE);
}

static void
account_widget_jabber_realize_cb (GtkWidget *widget,
				  gpointer   user_data)
{
	GossipAccountWidgetJabberPriv *priv;

	priv = GET_PRIV (widget);
	
	/* Set focus */
	gtk_widget_grab_focus (priv->entry_id); 
}

static void
account_widget_jabber_register_cb (GossipResult               result,
				   GError                    *error,
				   GossipAccountWidgetJabber *settings)
{
	GossipAccountWidgetJabberPriv *priv;
	GtkWidget                     *toplevel;
	GtkWidget                     *md;
	const gchar                   *str;

	priv = GET_PRIV (settings);

	priv->registering = FALSE;

	/* FIXME: Not sure how to do this right, but really we
	 * shouldn't show the register button as sensitive if we have
	 * just registered.
	 */
	gtk_widget_set_sensitive (priv->button_register, TRUE);
	gtk_widget_set_sensitive (priv->vbox_settings, TRUE);

	toplevel = gtk_widget_get_toplevel (priv->vbox_settings);
	if (GTK_WIDGET_TOPLEVEL (toplevel) != TRUE || 
	    GTK_WIDGET_TYPE (toplevel) != GTK_TYPE_WINDOW) {
		toplevel = NULL;
	}
	
	if (result == GOSSIP_RESULT_OK) {
		str = _("Successfully registered your new account settings.");
		md = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					     GTK_DIALOG_MODAL,
					     GTK_MESSAGE_INFO,
					     GTK_BUTTONS_CLOSE,
					     str);

		str = _("You should now be able to connect to your new account.");
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (md), str);
	} else {
		str = _("Failed to register your new account settings.");
		md = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					     GTK_DIALOG_MODAL,
					     GTK_MESSAGE_ERROR,
					     GTK_BUTTONS_CLOSE,
					     str);
		
		if (error && error->message) {
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (md),
								  error->message);
		}
	}

	g_signal_connect_swapped (md, "response", 
				  G_CALLBACK (gtk_widget_destroy),
				  md);

	gtk_widget_show_all (md);
}

static void
account_widget_jabber_button_register_clicked_cb (GtkWidget                 *button,
						  GossipAccountWidgetJabber *settings)
{
	GossipAccountWidgetJabberPriv *priv;
	GossipSession                 *session;
	GossipVCard                   *vcard;
	gchar                         *nickname;
	const gchar                   *name;
	const gchar                   *last_part;

	priv = GET_PRIV (settings);

	priv->registering = TRUE;
	gtk_widget_set_sensitive (priv->button_register, FALSE);
	gtk_widget_set_sensitive (priv->vbox_settings, FALSE);

	session = gossip_app_get_session ();
	vcard = gossip_vcard_new ();
	name = gossip_account_get_name (priv->account);

	last_part = strstr (name, " ");
	if (last_part) {
		gint len;

		len = last_part - name;
		nickname = g_strndup (name, len);
	} else {
		nickname = g_strdup (name);
	}

	gossip_vcard_set_name (vcard, name);
	gossip_vcard_set_nickname (vcard, nickname);

	g_free (nickname);

	gossip_session_register_account (session,
					 priv->account,
					 vcard,
					 (GossipErrorCallback) 
					 account_widget_jabber_register_cb,
					 settings);
}

static void
account_widget_jabber_button_forget_clicked_cb (GtkWidget                 *button,
						GossipAccountWidgetJabber *settings)
{
	GossipAccountWidgetJabberPriv *priv;
	GossipSession                 *session;
	GossipAccountManager          *manager;

	priv = GET_PRIV (settings);

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	gossip_account_set_password (priv->account, "");
	gossip_account_manager_store (manager);

	gtk_entry_set_text (GTK_ENTRY (priv->entry_password), "");
}

static void
account_widget_jabber_change_password_cancel (GossipAccountWidgetJabber *settings)
{
	GossipAccountWidgetJabberPriv *priv;
	GossipSession                 *session;

	priv = GET_PRIV (settings);

	if (!priv->changing_password) {
		return;
	}

	session = gossip_app_get_session ();
	gossip_session_change_password_cancel (session, priv->account);

	priv->changing_password = FALSE;
	gtk_widget_set_sensitive (priv->button_change_password, TRUE);
	gtk_widget_set_sensitive (priv->vbox_settings, TRUE);
}

static void
account_widget_jabber_change_password_cb (GossipResult               result,
					  GError                    *error,
					  GossipAccountWidgetJabber *settings)
{
	GossipAccountWidgetJabberPriv *priv;
	GtkWidget                     *toplevel;
	GtkWidget                     *md;
	const gchar                   *str;

	priv = GET_PRIV (settings);

	priv->changing_password = FALSE;

	/* FIXME: Not sure how to do this right, but really we
	 * shouldn't show the register button as sensitive if we have
	 * just registered.
	 */
	gtk_widget_set_sensitive (priv->button_change_password, TRUE);
	gtk_widget_set_sensitive (priv->vbox_settings, TRUE);

	toplevel = gtk_widget_get_toplevel (priv->vbox_settings);
	if (GTK_WIDGET_TOPLEVEL (toplevel) != TRUE || 
	    GTK_WIDGET_TYPE (toplevel) != GTK_TYPE_WINDOW) {
		toplevel = NULL;
	}
	
	if (result == GOSSIP_RESULT_OK) {
		str = _("Successfully changed your account password.");
		md = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					     GTK_DIALOG_MODAL,
					     GTK_MESSAGE_INFO,
					     GTK_BUTTONS_CLOSE,
					     str);

		str = _("You should now be able to connect with your new password.");
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (md), str);
	} else {
		str = _("Failed to change your account password.");
		md = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					     GTK_DIALOG_MODAL,
					     GTK_MESSAGE_ERROR,
					     GTK_BUTTONS_CLOSE,
					     str);
		
		if (error && error->message) {
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (md),
								  error->message);
		}
	}

	g_signal_connect_swapped (md, "response", 
				  G_CALLBACK (gtk_widget_destroy),
				  md);

	gtk_widget_show_all (md);
}

static void
account_widget_jabber_cp_entry_activate_cb (GtkWidget *entry,
					    GtkDialog *dialog)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static void
account_widget_jabber_cp_response_cb (GtkWidget                 *dialog,
				      gint                       response,
				      GossipAccountWidgetJabber *settings)
{
	if (response == GTK_RESPONSE_OK) {
		GossipAccountWidgetJabberPriv *priv;
		GossipSession                 *session;
		GtkWidget                     *entry;
		const gchar                   *new_password;
		
		priv = GET_PRIV (settings);

		entry = g_object_get_data (G_OBJECT (dialog), "entry");
		new_password = gtk_entry_get_text (GTK_ENTRY (entry));
		
		priv->changing_password = TRUE;
		gtk_widget_set_sensitive (priv->button_change_password, FALSE);
		gtk_widget_set_sensitive (priv->vbox_settings, FALSE);

		session = gossip_app_get_session ();
		gossip_session_change_password (session,
						priv->account,
						new_password,
						(GossipErrorCallback)
						account_widget_jabber_change_password_cb,
						settings);
	}

	gtk_widget_destroy (dialog);
}

static void
account_widget_jabber_button_change_password_clicked_cb (GtkWidget                 *button,
							 GossipAccountWidgetJabber *settings)
{
	GossipAccountWidgetJabberPriv *priv;
	GtkWidget                     *toplevel;
	GtkWidget                     *dialog;
	GtkWidget                     *entry;
	GtkWidget                     *hbox;
	gchar                         *str;
	const gchar                   *id;
	
	priv = GET_PRIV (settings);

	toplevel = gtk_widget_get_toplevel (priv->vbox_settings);
	if (GTK_WIDGET_TOPLEVEL (toplevel) != TRUE || 
	    GTK_WIDGET_TYPE (toplevel) != GTK_TYPE_WINDOW) {
		toplevel = NULL;
	}

	/* Dialog here to get new password from user */
	id = gossip_account_get_id (priv->account);
	str = g_strdup_printf ("<b>%s</b>", id);
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 0,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_OK_CANCEL,
					 _("Please enter a new password for this account:\n%s"),
					 str);

	g_free (str);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    hbox, FALSE, TRUE, 4);

	entry = gtk_entry_new ();
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 4);

	g_object_set (GTK_MESSAGE_DIALOG (dialog)->label, "use-markup", TRUE, NULL);
	g_object_set_data (G_OBJECT (dialog), "entry", entry);

	g_signal_connect (entry, "activate",
			  G_CALLBACK (account_widget_jabber_cp_entry_activate_cb),
			  dialog);
	g_signal_connect (dialog, "response",
			  G_CALLBACK (account_widget_jabber_cp_response_cb),
			  settings);

	gtk_widget_show_all (dialog);
}

static void
account_widget_jabber_setup (GossipAccountWidgetJabber *settings,
			     GossipAccount             *account)
{
	GossipAccountWidgetJabberPriv *priv;
	GossipSession                 *session;
	guint                          port;
	gchar                         *port_str; 
	const gchar                   *id;
	const gchar                   *resource;
	const gchar                   *server;
	const gchar                   *password;
	gboolean                       use_ssl;
	gboolean                       is_connected;

	priv = GET_PRIV (settings);
	
	priv->account = g_object_ref (account);

	id = gossip_account_get_id (priv->account);
	password = gossip_account_get_password (priv->account);
	resource = gossip_account_get_resource (priv->account);
	server = gossip_account_get_server (priv->account);
	port = gossip_account_get_port (priv->account);
	use_ssl = gossip_account_get_use_ssl (priv->account);

	if (gossip_jabber_is_ssl_supported ()) {
		g_signal_handlers_block_by_func (priv->checkbutton_ssl, 
						 account_widget_jabber_checkbutton_toggled_cb,
						 settings);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton_ssl), use_ssl);
		g_signal_handlers_unblock_by_func (priv->checkbutton_ssl, 
						   account_widget_jabber_checkbutton_toggled_cb,
						   settings);
	} else {
		gtk_widget_set_sensitive (priv->checkbutton_ssl, FALSE);
	}

	g_signal_handlers_block_by_func (priv->entry_id, 
					 account_widget_jabber_entry_changed_cb, 
					 settings);
	g_signal_handlers_block_by_func (priv->entry_password, 
					 account_widget_jabber_entry_changed_cb, 
					 settings);
	g_signal_handlers_block_by_func (priv->entry_resource, 
					 account_widget_jabber_entry_changed_cb, 
					 settings);
	g_signal_handlers_block_by_func (priv->entry_server, 
					 account_widget_jabber_entry_changed_cb, 
					 settings);
	g_signal_handlers_block_by_func (priv->entry_port, 
					 account_widget_jabber_entry_changed_cb, 
					 settings);

	gtk_entry_set_text (GTK_ENTRY (priv->entry_id), id ? id : "");
	gtk_entry_set_text (GTK_ENTRY (priv->entry_password), password ? password : "");
	gtk_entry_set_text (GTK_ENTRY (priv->entry_resource), resource ? resource : "");
	gtk_entry_set_text (GTK_ENTRY (priv->entry_server), server ? server : "");

	port_str = g_strdup_printf ("%d", port);
	gtk_entry_set_text (GTK_ENTRY (priv->entry_port), port_str);
	g_free (port_str);

	g_signal_handlers_unblock_by_func (priv->entry_id, 
					   account_widget_jabber_entry_changed_cb, 
					   settings);
	g_signal_handlers_unblock_by_func (priv->entry_password, 
					   account_widget_jabber_entry_changed_cb, 
					   settings);
	g_signal_handlers_unblock_by_func (priv->entry_resource, 
					   account_widget_jabber_entry_changed_cb, 
					   settings);
	g_signal_handlers_unblock_by_func (priv->entry_server, 
					   account_widget_jabber_entry_changed_cb, 
					   settings);
	g_signal_handlers_unblock_by_func (priv->entry_port, 
					   account_widget_jabber_entry_changed_cb, 
					   settings);
	
	gtk_widget_set_sensitive (priv->button_forget, !G_STR_EMPTY (password));

	/* Set up connection specific buttons */
	session = gossip_app_get_session ();
	is_connected = gossip_session_is_connected (session, priv->account);
	gtk_widget_set_sensitive (priv->button_register, !is_connected);
	gtk_widget_set_sensitive (priv->button_change_password, is_connected);

	/* Make sure we update the password entry if the password
	 * changes, while logging in for example.
	 */
	g_signal_connect (priv->account, "notify::password",
			  G_CALLBACK (account_widget_jabber_password_changed_cb), 
			  settings);
	
	/* Set up protocol signals */
	g_signal_connect (session, "protocol-connected",
			  G_CALLBACK (account_widget_jabber_protocol_connected_cb),
			  settings);

	g_signal_connect (session, "protocol-disconnected",
			  G_CALLBACK (account_widget_jabber_protocol_disconnected_cb),
			  settings);

	g_signal_connect (session, "protocol-error",
			  G_CALLBACK (account_widget_jabber_protocol_error_cb),
			  settings);
}

GtkWidget *
gossip_account_widget_jabber_new (GossipAccount *account)
{
	gpointer object;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	object = g_object_new (GOSSIP_TYPE_ACCOUNT_WIDGET_JABBER, NULL);
	if (!object) {
		return NULL;
	}

	account_widget_jabber_setup (GOSSIP_ACCOUNT_WIDGET_JABBER (object), account);

	return GTK_WIDGET (object);
}

