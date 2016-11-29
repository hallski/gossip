/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2008 Imendio AB
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
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libgossip/gossip.h>

#include "gossip-change-password-dialog.h"
#include "gossip-app.h"
#include "gossip-glade.h"
#include "ephy-spinner.h"

#define DEBUG_DOMAIN "ChangePasswordDialog"

typedef struct {
    GtkWidget     *window;

    GtkWidget     *table;

    GtkWidget     *entry_password;
    GtkWidget     *entry_confirm;

    GtkWidget     *label_match;
    GtkWidget     *throbber;
 
    GtkWidget     *button_ok;

    GossipAccount *account;
    gboolean       changing_password;
} GossipChangePasswordDialog;

static void
change_password_dialog_changed_cb (GossipResult                result,
                                   GError                     *error,
                                   GossipChangePasswordDialog *dialog)
{
    GtkWidget   *md;
    const gchar *str;

    dialog->changing_password = FALSE;

    if (result == GOSSIP_RESULT_OK) {
        GossipSession        *session;
        GossipAccountManager *manager;
        const gchar          *password;

        /* Remember password */
        password = gtk_entry_get_text (GTK_ENTRY (dialog->entry_password));
        gossip_account_set_password (dialog->account, password);

        session = gossip_app_get_session ();
        manager = gossip_session_get_account_manager (session);
        gossip_account_manager_store (manager);

        /* Show success dialog */
        str = _("Successfully changed your account password.");
        md = gtk_message_dialog_new (NULL,
                                     GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_INFO,
                                     GTK_BUTTONS_CLOSE,
                                     "%s", str);

        str = _("You should now be able to connect with your new password.");
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (md), 
                                                  "%s", str);
    } else {
        str = _("Failed to change your account password.");
        md = gtk_message_dialog_new (NULL,
                                     GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     "%s", str);
                
        if (error && error->message) {
            gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (md),
                                                      "%s", error->message);
        }
    }

    g_signal_connect_swapped (md, "response", 
                              G_CALLBACK (gtk_widget_destroy),
                              md);

    gtk_widget_show_all (md);

    /* Now clean up dialog */
    gtk_widget_destroy (dialog->window);
}

static void
change_password_dialog_start (GossipChangePasswordDialog *dialog)
{
    GossipSession *session;
    const gchar   *password;

    if (dialog->changing_password) {
        return;
    }
        
    dialog->changing_password = TRUE;

    /* Disable widgets */
    gtk_widget_set_sensitive (dialog->table, FALSE);
    gtk_widget_set_sensitive (dialog->button_ok, FALSE);

    /* Show progress widget */
    gtk_widget_show (dialog->throbber);
    ephy_spinner_start (EPHY_SPINNER (dialog->throbber));

    /* Actually set the password */
    password = gtk_entry_get_text (GTK_ENTRY (dialog->entry_password));
        
    session = gossip_app_get_session ();
    gossip_session_change_password (session,
                                    dialog->account,
                                    password,
                                    (GossipErrorCallback)
                                    change_password_dialog_changed_cb,
                                    dialog);
}

static void
change_password_dialog_stop (GossipChangePasswordDialog *dialog)
{
    GossipSession *session;

    if (!dialog->changing_password) {
        return;
    }

    /* Cancel operation */
    session = gossip_app_get_session ();
    gossip_session_change_password_cancel (session,
                                           dialog->account);
    dialog->changing_password = FALSE;

    /* Stop progress widget */
    ephy_spinner_stop (EPHY_SPINNER (dialog->throbber));
    gtk_widget_hide (dialog->throbber);

    /* Enable widgets */ 
    gtk_widget_set_sensitive (dialog->table, TRUE);
    gtk_widget_set_sensitive (dialog->button_ok, TRUE);
}

static void
change_password_dialog_entry_changed_cb (GtkWidget                  *widget, 
                                         GossipChangePasswordDialog *dialog)
{
    const gchar *str1;
    const gchar *str2;
    gboolean     empty;
    gboolean     match;

    str1 = gtk_entry_get_text (GTK_ENTRY (dialog->entry_password));
    str2 = gtk_entry_get_text (GTK_ENTRY (dialog->entry_confirm));

    empty = G_STR_EMPTY (str1) && G_STR_EMPTY (str2);

    match  = FALSE;
    match |= empty;
    match |= str1 && str2 && strcmp (str1, str2) == 0;

    if (match) {
        gtk_widget_hide (dialog->label_match);
    } else {
        gtk_widget_show (dialog->label_match);
    }

    gtk_widget_set_sensitive (dialog->button_ok, match && !empty);
}

static void
change_password_dialog_response_cb (GtkWidget                  *widget,
                                    gint                        response,
                                    GossipChangePasswordDialog *dialog)
{
    if (response == GTK_RESPONSE_OK) {
        change_password_dialog_start (dialog);
        return;
    } 

    if (response == GTK_RESPONSE_CANCEL && dialog->changing_password) {
        change_password_dialog_stop (dialog);
        return;
    }
        
    gtk_widget_destroy (widget);
}

static void
change_password_dialog_destroy_cb (GtkWidget                  *widget,
                                   GossipChangePasswordDialog *dialog)
{
    g_object_unref (dialog->account);

    g_free (dialog);
}

void
gossip_change_password_dialog_show (GossipAccount *account)
{
    GossipChangePasswordDialog *dialog;
    GladeXML                   *glade;
    GtkWidget                  *hbox_progress;

    g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

    dialog = g_new0 (GossipChangePasswordDialog, 1);

    dialog->account = g_object_ref (account);

    glade = gossip_glade_get_file ("main.glade",
                                   "change_password_dialog",
                                   NULL,
                                   "change_password_dialog", &dialog->window,
                                   "entry_password", &dialog->entry_password,
                                   "entry_confirm", &dialog->entry_confirm,
                                   "table", &dialog->table,
                                   "hbox_progress", &hbox_progress,
                                   "label_match", &dialog->label_match,
                                   "button_ok", &dialog->button_ok,
                                   NULL);

    gossip_glade_connect (glade,
                          dialog,
                          "change_password_dialog", "destroy", change_password_dialog_destroy_cb,
                          "change_password_dialog", "response", change_password_dialog_response_cb,
                          "entry_password", "changed", change_password_dialog_entry_changed_cb,
                          "entry_confirm", "changed", change_password_dialog_entry_changed_cb,
                          NULL);

    g_object_add_weak_pointer (G_OBJECT (dialog->window), (gpointer) &dialog);

    g_object_unref (glade);

    /* Set up throbber */
    dialog->throbber = ephy_spinner_new ();
    ephy_spinner_set_size (EPHY_SPINNER (dialog->throbber), GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_box_pack_end (GTK_BOX (hbox_progress), dialog->throbber, FALSE, FALSE, 0);
    gtk_widget_hide (dialog->throbber);

    gtk_widget_show (dialog->window);
}
