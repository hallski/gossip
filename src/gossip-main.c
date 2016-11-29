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
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libgossip/gossip.h>

#ifdef HAVE_DBUS
#include "gossip-dbus.h"
#endif

#ifdef HAVE_GALAGO
#include "gossip-galago.h"
#endif

#include "gossip-app.h"
#include "gossip-preferences.h"
#include "gossip-ui-utils.h"

static gboolean  no_connect = FALSE;
static gboolean  multiple_instances = FALSE;
static gboolean  list_accounts = FALSE;
static gchar    *account_name = NULL;

int
main (int argc, char *argv[])
{
    gchar                *localedir;
    GError               *error = NULL;
    gboolean              init_galago;
    GossipSession        *session;
    GossipAccountManager *account_manager;
    GossipAccount        *account = NULL;
    GOptionContext       *context;
    GList                *accounts;
    GOptionEntry          options[] = {
        { "no-connect", 'n',
          0, G_OPTION_ARG_NONE, &no_connect,
          N_("Don't connect on startup"),
          NULL },
        { "multiple-instances", 'm',
          0, G_OPTION_ARG_NONE, &multiple_instances,
          N_("Allow multiple instances of the application to run at the same time"),
          NULL },
        { "list-accounts", 'l',
          0, G_OPTION_ARG_NONE, &list_accounts,
          N_("List the available accounts"),
          NULL },
        { "account", 'a',
          0, G_OPTION_ARG_STRING, &account_name,
          N_("Which account to connect to on startup"),
          N_("ACCOUNT-NAME") },
        { NULL }
    };

    localedir = gossip_paths_get_locale_path ();
    bindtextdomain (GETTEXT_PACKAGE, localedir);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
    g_free (localedir);

    context = g_option_context_new (_("- Gossip Instant Messenger"));
    g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

    g_set_application_name (PACKAGE_NAME);

#ifdef HAVE_PLATFORM_X11
    /* This is really crap! On Windows, we can't use DATADIR because it
     * conflicts with a definition in the win32 header files meaning
     * Gossip won't compile. Here, we need it defined for
     * GNOME_PROGRAM_STANDARD_PROPERTIES which needs it. So we simply add
     * this hack below to keep everyone happy.
     */
// #define DATADIR SHAREDIR
#endif

    if (!gtk_init_with_args (&argc, &argv,
                             _("- Gossip Instant Messenger"),
                             options,
                             GETTEXT_PACKAGE,
                             &error)) {
        g_printerr ("%s\n", error->message);
        return 1;
    }

    gossip_window_set_default_icon_name (PACKAGE_TARNAME);

    /* Get all accounts. */
    session = gossip_session_new (NULL, NULL, NULL);
    account_manager = gossip_session_get_account_manager (session);

    if (account_name && no_connect) {
        g_printerr (_("You can not use --no-connect together with --account"));
        g_printerr ("\n");

        return EXIT_FAILURE;
    }

    if (list_accounts) {
        GList         *l;
        GossipAccount *def = NULL;

        accounts = gossip_account_manager_get_accounts (account_manager);
        if (!accounts) {
            g_printerr (_("No accounts available."));
            g_printerr ("\n");
        } else {
            def = gossip_account_manager_get_default (account_manager);

            g_printerr (_("Available accounts:"));
            g_printerr ("\n");
        }

        for (l = accounts; l; l = l->next) {
            GossipAccount *account = l->data;

            g_print (" %s", gossip_account_get_name (account));
            if (def && gossip_account_equal (account, def)) {
                g_printerr (" ");
                g_printerr (_("[default]"));
            }

            g_printerr ("\n");
        }

        g_list_foreach (accounts, (GFunc) g_object_unref, NULL);
        g_list_free (accounts);

        return EXIT_SUCCESS;
    }

    if (account_name) {
        account = gossip_account_manager_find (account_manager,
                                               account_name);
        if (!account) {
            g_printerr (_("There is no account with the name '%s'."),
                        account_name);
            g_printerr ("\n");

            return EXIT_FAILURE;
        }
    }

    gossip_stock_init ();

#ifdef HAVE_DBUS
    init_galago = gossip_dbus_init_for_session (session, multiple_instances);
#else
    init_galago = TRUE;
#endif

#ifdef HAVE_GALAGO
    if (init_galago) {
        gossip_galago_init (session);
    }
#endif

    gossip_app_create (session);
    g_object_unref (session);

    if (!no_connect) {
        gossip_app_connect (account, TRUE);
    }

    gtk_main ();

#ifdef HAVE_DBUS
    gossip_dbus_finalize_for_session ();
#endif

    g_object_unref (gossip_app_get ());

    gossip_stock_finalize ();

    return EXIT_SUCCESS;
}

