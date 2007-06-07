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
#include <libgossip/gossip-protocol.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-utils.h>
#include <libgossip/gossip-vcard.h>
#include <libgossip/gossip-account-manager.h>

#include "gossip-account-widget-jabber.h"
#include "gossip-app.h"
#include "gossip-glade.h"
#include "gossip-marshal.h"
#include "gossip-ui-utils.h"

typedef struct {
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
} GossipAccountWidgetJabber;

static void     account_widget_jabber_save                              (GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_protocol_connected_cb             (GossipSession             *session,
									 GossipAccount             *account,
									 GossipProtocol            *protocol,
									 GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_protocol_disconnected_cb          (GossipSession             *session,
									 GossipAccount             *account,
									 GossipProtocol            *protocol,
									 gint                       reason,
									 GossipAccountWidgetJabber *settings);
static gboolean account_widget_jabber_entry_focus_cb                    (GtkWidget                 *widget,
									 GdkEventFocus             *event,
									 GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_entry_changed_cb                  (GtkWidget                 *widget,
									 GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_checkbutton_toggled_cb            (GtkWidget                 *widget,
									 GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_entry_port_insert_text_cb         (GtkEditable               *editable,
									 gchar                     *new_text,
									 gint                       len,
									 gint                      *position,
									 GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_register_cancel                   (GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_register_cb                       (GossipResult               result,
									 GError                    *error,
									 GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_button_register_clicked_cb        (GtkWidget                 *button,
									 GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_button_forget_clicked_cb          (GtkWidget                 *button,
									 GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_cp_entry_activate_cb              (GtkWidget                 *entry,
									 GtkDialog                 *dialog);
static void     account_widget_jabber_cp_response_cb                    (GtkWidget                 *dialog,
									 gint                       response,
									 GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_button_change_password_clicked_cb (GtkWidget                 *button,
									 GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_destroy_cb                        (GtkWidget                 *widget,
									 GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_setup                             (GossipAccountWidgetJabber *settings);

static void
account_widget_jabber_save (GossipAccountWidgetJabber *settings)
{
	GossipSession        *session;
	GossipAccountManager *manager;
 	const gchar          *id;
 	const gchar          *password;
 	const gchar          *resource;
 	const gchar          *server;
 	const gchar          *port_str;
	guint16               port;
	gboolean              use_ssl;

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	id = gtk_entry_get_text (GTK_ENTRY (settings->entry_id));
	password = gtk_entry_get_text (GTK_ENTRY (settings->entry_password));
	resource = gtk_entry_get_text (GTK_ENTRY (settings->entry_resource));
	server = gtk_entry_get_text (GTK_ENTRY (settings->entry_server));
	port_str = gtk_entry_get_text (GTK_ENTRY (settings->entry_port));
	port = strtol (port_str, NULL, 10);
	use_ssl = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (settings->checkbutton_ssl));

	g_object_set (settings->account,
		      "id", id,
		      "password", password,
		      "resource", resource,
		      "server", server,
		      "port", port,
		      "use_ssl", use_ssl,
		      NULL);

	gossip_account_manager_store (manager);

	settings->account_changed = FALSE;
}

static void
account_widget_jabber_protocol_connected_cb (GossipSession             *session,
					     GossipAccount             *account,
					     GossipProtocol            *protocol,
					     GossipAccountWidgetJabber *settings)
{
	if (gossip_account_equal (account, settings->account)) {
		gtk_widget_set_sensitive (settings->button_register, FALSE);
		gtk_widget_set_sensitive (settings->button_change_password, TRUE);
	}
}

static void
account_widget_jabber_protocol_disconnected_cb (GossipSession             *session,
						GossipAccount             *account,
						GossipProtocol            *protocol,
						gint                       reason,
						GossipAccountWidgetJabber *settings)
{
	if (gossip_account_equal (account, settings->account)) {
		gtk_widget_set_sensitive (settings->button_register, TRUE);
		gtk_widget_set_sensitive (settings->button_change_password, FALSE);
	}
}

static void
account_widget_jabber_protocol_error_cb (GossipSession             *session,
					 GossipProtocol            *protocol,
					 GossipAccount             *account,
					 GError                    *error,
					 GossipAccountWidgetJabber *settings)
{
	if (gossip_account_equal (account, settings->account)) {
		/* FIXME: HANDLE ERRORS */
	}	
}

static gboolean
account_widget_jabber_entry_focus_cb (GtkWidget                 *widget,
				      GdkEventFocus             *event,
				      GossipAccountWidgetJabber *settings)
{
	if (widget == settings->entry_id) {
		GossipSession  *session;
		GossipProtocol *protocol;

		session = gossip_app_get_session ();
		protocol = gossip_session_get_protocol (session, settings->account);

		if (protocol) {
			const gchar *str;

			str = gtk_entry_get_text (GTK_ENTRY (widget));

			if (!gossip_jid_string_is_valid (str, FALSE)) {
				g_object_get (settings->account, "id", &str, NULL);
				settings->account_changed = FALSE;
			} else {
				gchar *server;

				server = gossip_jabber_get_default_server (str);
				gtk_entry_set_text (GTK_ENTRY (settings->entry_server), server);
				g_free (server);
			}

			gtk_entry_set_text (GTK_ENTRY (widget), str);
		}
	}

	if (widget == settings->entry_password ||
	    widget == settings->entry_resource ||
	    widget == settings->entry_server) {
		const gchar *str;

		str = gtk_entry_get_text (GTK_ENTRY (widget));
		if (G_STR_EMPTY (str)) {
			if (widget == settings->entry_password) {
				str = gossip_account_get_password (settings->account);
			} else if (widget == settings->entry_resource) {
				str = gossip_account_get_resource (settings->account);
			} else if (widget == settings->entry_server) {
				str = gossip_account_get_server (settings->account);
			}

			gtk_entry_set_text (GTK_ENTRY (widget), str ? str : "");
			settings->account_changed = FALSE;
		}
	}

	if (widget == settings->entry_port) {
		const gchar *str;
		
		str = gtk_entry_get_text (GTK_ENTRY (widget));
		if (G_STR_EMPTY (str)) {
			gchar   *port_str;
			guint16  port;

			port = gossip_account_get_port (settings->account);
			port_str = g_strdup_printf ("%d", port);
			gtk_entry_set_text (GTK_ENTRY (widget), port_str);
			g_free (port_str);

			settings->account_changed = FALSE;
		}
	}

	if (settings->account_changed) {
 		account_widget_jabber_save (settings); 
	}

	return FALSE;
}

static void
account_widget_jabber_entry_changed_cb (GtkWidget                 *widget,
					GossipAccountWidgetJabber *settings)
{
	if (widget == settings->entry_port) {
		const gchar *str;
		gint         pnr;

		str = gtk_entry_get_text (GTK_ENTRY (widget));
		pnr = strtol (str, NULL, 10);
		
		if ((pnr <= 0 || pnr >= 65556) && !G_STR_EMPTY (str)) {
			gchar   *port_str;
			guint16  port;
			
			port = gossip_account_get_port (settings->account);
			port_str = g_strdup_printf ("%d", port);
			g_signal_handlers_block_by_func (settings->entry_port, 
							 account_widget_jabber_entry_changed_cb, 
							 settings);
			gtk_entry_set_text (GTK_ENTRY (widget), port_str);
			g_signal_handlers_unblock_by_func (settings->entry_port, 
							   account_widget_jabber_entry_changed_cb, 
							   settings);
			g_free (port_str);

			return;
		}
	} else if (widget == settings->entry_password) {
		const gchar *str;

		str = gtk_entry_get_text (GTK_ENTRY (widget));
		gtk_widget_set_sensitive (settings->button_forget, !G_STR_EMPTY (str));
	}

	/* Save */
	settings->account_changed = TRUE;
}

static void  
account_widget_jabber_checkbutton_toggled_cb (GtkWidget                 *widget,
					      GossipAccountWidgetJabber *settings)
{
	if (widget == settings->checkbutton_ssl) {
		GossipSession  *session;
		GossipProtocol *protocol;
		guint16         port_with_ssl;
		guint16         port_without_ssl;
		guint16         port;
		gboolean        use_ssl;
		gboolean        changed = FALSE;

		session = gossip_app_get_session ();
		protocol = gossip_session_get_protocol (session, settings->account);

		port_with_ssl = gossip_jabber_get_default_port (TRUE);
		port_without_ssl = gossip_jabber_get_default_port (FALSE);

		port = gossip_account_get_port (settings->account);

		use_ssl = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

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
		
		g_object_set (settings->account,
			      "port", port,
			      "use_ssl", use_ssl,
			      NULL);

		if (changed) {
			gchar *port_str;

			port_str = g_strdup_printf ("%d", port);
			gtk_entry_set_text (GTK_ENTRY (settings->entry_port), port_str);
			g_free (port_str);
		}
	}

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
	GossipSession *session;

	if (!settings->registering) {
		return;
	}

	session = gossip_app_get_session ();
	gossip_session_register_cancel (session, settings->account);

	settings->registering = FALSE;
	gtk_widget_set_sensitive (settings->button_register, TRUE);
	gtk_widget_set_sensitive (settings->vbox_settings, TRUE);
}

static void
account_widget_jabber_register_cb (GossipResult               result,
				   GError                    *error,
				   GossipAccountWidgetJabber *settings)
{
	GtkWidget   *toplevel;
	GtkWidget   *md;
	const gchar *str;

	settings->registering = FALSE;

	/* FIXME: Not sure how to do this right, but really we
	 * shouldn't show the register button as sensitive if we have
	 * just registered.
	 */
	gtk_widget_set_sensitive (settings->button_register, TRUE);
	gtk_widget_set_sensitive (settings->vbox_settings, TRUE);

	toplevel = gtk_widget_get_toplevel (settings->vbox_settings);
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
	GossipSession *session;
	GossipVCard   *vcard;
	gchar         *nickname;
	const gchar   *name;
	const gchar   *last_part;

	settings->registering = TRUE;
	gtk_widget_set_sensitive (settings->button_register, FALSE);
	gtk_widget_set_sensitive (settings->vbox_settings, FALSE);

	session = gossip_app_get_session ();
	vcard = gossip_vcard_new ();
	name = gossip_account_get_name (settings->account);

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
					 settings->account,
					 vcard,
					 (GossipErrorCallback) 
					 account_widget_jabber_register_cb,
					 settings);
}

static void
account_widget_jabber_button_forget_clicked_cb (GtkWidget                 *button,
						GossipAccountWidgetJabber *settings)
{
	GossipSession        *session;
	GossipAccountManager *manager;

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	gossip_account_set_password (settings->account, "");
	gossip_account_manager_store (manager);

	gtk_entry_set_text (GTK_ENTRY (settings->entry_password), "");
}

static void
account_widget_jabber_change_password_cancel (GossipAccountWidgetJabber *settings)
{
	GossipSession *session;

	if (!settings->changing_password) {
		return;
	}

	session = gossip_app_get_session ();
	gossip_session_change_password_cancel (session, settings->account);

	settings->changing_password = FALSE;
	gtk_widget_set_sensitive (settings->button_change_password, TRUE);
	gtk_widget_set_sensitive (settings->vbox_settings, TRUE);
}

static void
account_widget_jabber_change_password_cb (GossipResult               result,
					  GError                    *error,
					  GossipAccountWidgetJabber *settings)
{
	GtkWidget   *toplevel;
	GtkWidget   *md;
	const gchar *str;

	settings->changing_password = FALSE;

	/* FIXME: Not sure how to do this right, but really we
	 * shouldn't show the register button as sensitive if we have
	 * just registered.
	 */
	gtk_widget_set_sensitive (settings->button_change_password, TRUE);
	gtk_widget_set_sensitive (settings->vbox_settings, TRUE);

	toplevel = gtk_widget_get_toplevel (settings->vbox_settings);
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
		GossipSession *session;
		GtkWidget     *entry;
		const gchar   *new_password;

		entry = g_object_get_data (G_OBJECT (dialog), "entry");
		new_password = gtk_entry_get_text (GTK_ENTRY (entry));
		
		settings->changing_password = TRUE;
		gtk_widget_set_sensitive (settings->button_change_password, FALSE);
		gtk_widget_set_sensitive (settings->vbox_settings, FALSE);

		session = gossip_app_get_session ();
		gossip_session_change_password (session,
						settings->account,
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
	GtkWidget   *toplevel;
	GtkWidget   *dialog;
	GtkWidget   *entry;
	GtkWidget   *hbox;
	gchar       *str;
	const gchar *id;

	toplevel = gtk_widget_get_toplevel (settings->vbox_settings);
	if (GTK_WIDGET_TOPLEVEL (toplevel) != TRUE || 
	    GTK_WIDGET_TYPE (toplevel) != GTK_TYPE_WINDOW) {
		toplevel = NULL;
	}

	/* Dialog here to get new password from user */
	id = gossip_account_get_id (settings->account);
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
account_widget_jabber_destroy_cb (GtkWidget                 *widget,
				  GossipAccountWidgetJabber *settings)
{
	GossipSession *session;

	account_widget_jabber_register_cancel (settings);
	account_widget_jabber_change_password_cancel (settings);

	if (settings->account_changed) {
 		account_widget_jabber_save (settings); 
	}

	session = gossip_app_get_session ();

	g_signal_handlers_disconnect_by_func (session,
					      account_widget_jabber_protocol_connected_cb,
					      settings);

	g_signal_handlers_disconnect_by_func (session,
					      account_widget_jabber_protocol_disconnected_cb,
					      settings);

	g_signal_handlers_disconnect_by_func (session,
					      account_widget_jabber_protocol_error_cb,
					      settings);

	g_object_unref (settings->account);
	g_free (settings);
}

static void
account_widget_jabber_setup (GossipAccountWidgetJabber *settings)
{
	GossipSession  *session;
	GossipProtocol *protocol;
	guint           port;
	gchar          *port_str; 
	const gchar    *id;
	const gchar    *resource;
	const gchar    *server;
	const gchar    *password;
	gboolean        use_ssl;
	gboolean        is_connected;

	session = gossip_app_get_session ();
	protocol = gossip_session_get_protocol (session, settings->account);

	g_object_get (settings->account,
		      "id", &id,
		      "password", &password,
		      "resource", &resource,
		      "server", &server,
		      "port", &port,
		      "use_ssl", &use_ssl,
		      NULL);

	if (gossip_protocol_is_ssl_supported (protocol)) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (settings->checkbutton_ssl), use_ssl);
	} else {
		gtk_widget_set_sensitive (settings->checkbutton_ssl, FALSE);
	}

	gtk_entry_set_text (GTK_ENTRY (settings->entry_id), id ? id : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_password), password ? password : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_resource), resource ? resource : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_server), server ? server : "");

	port_str = g_strdup_printf ("%d", port);
	gtk_entry_set_text (GTK_ENTRY (settings->entry_port), port_str);
	g_free (port_str);

	gtk_widget_set_sensitive (settings->button_forget, !G_STR_EMPTY (password));

	/* Set up connection specific buttons */
	is_connected = gossip_session_is_connected (session, settings->account);
	gtk_widget_set_sensitive (settings->button_register, !is_connected);
	gtk_widget_set_sensitive (settings->button_change_password, is_connected);

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
	GossipAccountWidgetJabber *settings;
	GladeXML                  *glade;
	GtkSizeGroup              *size_group;
	GtkWidget                 *label_id, *label_password;
	GtkWidget                 *label_server, *label_resource, *label_port; 

	settings = g_new0 (GossipAccountWidgetJabber, 1);
	settings->account = g_object_ref (account);

	glade = gossip_glade_get_file ("main.glade",
				       "vbox_jabber_settings",
				       NULL,
				       "vbox_jabber_settings", &settings->vbox_settings,
				       "button_register", &settings->button_register,
				       "button_forget", &settings->button_forget,
				       "button_change_password", &settings->button_change_password,
				       "label_id", &label_id,
				       "label_password", &label_password,
				       "label_resource", &label_resource,
				       "label_server", &label_server,
				       "label_port", &label_port,
				       "entry_id", &settings->entry_id,
				       "entry_password", &settings->entry_password,
				       "entry_resource", &settings->entry_resource,
				       "entry_server", &settings->entry_server,
				       "entry_port", &settings->entry_port,
				       "checkbutton_ssl", &settings->checkbutton_ssl,
				       NULL);

	account_widget_jabber_setup (settings);

	gossip_glade_connect (glade, 
			      settings,
			      "vbox_jabber_settings", "destroy", account_widget_jabber_destroy_cb,
			      "button_register", "clicked", account_widget_jabber_button_register_clicked_cb,
			      "button_forget", "clicked", account_widget_jabber_button_forget_clicked_cb,
			      "button_change_password", "clicked", account_widget_jabber_button_change_password_clicked_cb,
			      "entry_id", "changed", account_widget_jabber_entry_changed_cb,
			      "entry_password", "changed", account_widget_jabber_entry_changed_cb,
			      "entry_resource", "changed", account_widget_jabber_entry_changed_cb,
			      "entry_server", "changed", account_widget_jabber_entry_changed_cb,
			      "entry_port", "changed", account_widget_jabber_entry_changed_cb,
			      "entry_id", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_password", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_resource", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_server", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_port", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_port", "insert_text", account_widget_jabber_entry_port_insert_text_cb,
			      "checkbutton_ssl", "toggled", account_widget_jabber_checkbutton_toggled_cb,
			      NULL);

	g_object_unref (glade);

	/* Set up remaining widgets */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, label_id);
	gtk_size_group_add_widget (size_group, label_password);
	gtk_size_group_add_widget (size_group, label_resource);
	gtk_size_group_add_widget (size_group, label_server);
	gtk_size_group_add_widget (size_group, label_port);

	g_object_unref (size_group);

	gtk_widget_show (settings->vbox_settings);

	return settings->vbox_settings;
}
