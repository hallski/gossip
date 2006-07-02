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

#include <config.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkmain.h>
#include <glib/gi18n.h>
#include <glib/goption.h>

#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-ui-init.h>

#include <libgossip/gossip-account-manager.h>
#include <libgossip/gossip-account.h>

#include "gossip-preferences.h"
#include "gossip-stock.h"
#include "gossip-app.h"

#define GNOME_PARAM_GOPTION_CONTEXT "goption-context"

static gboolean  no_connect = FALSE;
static gboolean  multiple_instances = FALSE;
static gboolean  list_accounts = FALSE;
static gchar    *account_name = NULL;

static const GOptionEntry options[] = {
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

int
main (int argc, char *argv[])
{
	GnomeProgram          *program;
	GossipAccountManager  *account_manager;
	GossipAccount         *account = NULL;
	GOptionContext        *context;
	GList                 *accounts;

	
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (_("- Gossip Instant Messenger"));
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);	

	program = gnome_program_init ("gossip", PACKAGE_VERSION,
				      LIBGNOMEUI_MODULE,
                                      argc, argv,
                                      GNOME_PROGRAM_STANDARD_PROPERTIES,
				      GNOME_PARAM_GOPTION_CONTEXT, context,
				      GNOME_PARAM_HUMAN_READABLE_NAME, PACKAGE_NAME,
				      NULL);

	g_option_context_free (context);

	g_set_application_name (PACKAGE_NAME);
	gtk_window_set_default_icon_name ("gossip");

	/* Get all accounts. */
 	account_manager = gossip_account_manager_new (NULL);

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
			if (gossip_account_equal (account, def)) {
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
	gossip_app_create (account_manager, multiple_instances);
	
	if (!no_connect) {
		gossip_app_connect (account, TRUE);
	}
	
	gtk_main ();

	g_object_unref (account_manager);

	g_object_unref (program);

	return EXIT_SUCCESS;
}
