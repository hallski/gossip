/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2003 Imendio AB
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
#include <gconf/gconf-client.h>
#include <glib/gi18n.h>

#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-ui-init.h>

#include <libgossip/gossip-account.h>

#include "gossip-preferences.h"
#include "gossip-stock.h"
#include "gossip-app.h"

GConfClient *gconf_client = NULL;

static void
setup_default_window_icon (void)
{
	GList        *list;
	GdkPixbuf    *pixbuf;

	pixbuf = gdk_pixbuf_new_from_file (DATADIR "/pixmaps/gossip.png", NULL);
	list = g_list_append (NULL, pixbuf);

	gtk_window_set_default_icon_list (list);

	g_list_free (list);
	g_object_unref (pixbuf);
}

int
main (int argc, char *argv[])
{
	GnomeProgram       *program;
	gboolean            no_connect = FALSE;
	gboolean            list_accounts = FALSE;
	poptContext         popt_context;
	gchar              *account_name = NULL;
	const gchar       **args;
	const GList        *accounts;

	struct poptOption   options[] = {
		{ "no-connect",
			'n',
			POPT_ARG_NONE,
			&no_connect,
			0,
			N_("Don't connect on startup"),
		  NULL },
		{ "account",
			'a',
			POPT_ARG_STRING,
			&account_name,
			0,
			N_("Which account to connect to on startup"),
		  N_("ACCOUNT-NAME") },
		{ "list-accounts",
			'l',
			POPT_ARG_NONE,
			&list_accounts,
			0,
			N_("List the available accounts"),
		  NULL },

		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	program = gnome_program_init ("gossip", PACKAGE_VERSION,
				      LIBGNOMEUI_MODULE,
                                      argc, argv,
                                      GNOME_PROGRAM_STANDARD_PROPERTIES,
				      GNOME_PARAM_POPT_TABLE, options,
				      GNOME_PARAM_HUMAN_READABLE_NAME, PACKAGE_NAME,
				      NULL);

	g_set_application_name (PACKAGE_NAME);

	g_object_get (program,
		      GNOME_PARAM_POPT_CONTEXT,
		      &popt_context,
		      NULL);

	args = poptGetArgs (popt_context);

	/* get all accounts */
	accounts = gossip_accounts_get_all (NULL);

	if (list_accounts) {
		const GList   *l;
		GossipAccount *default_account = NULL;

		if (g_list_length ((GList*)accounts) < 1) {
			g_print (_("No accounts available."));
			g_print ("\n");
		} else {
			default_account = gossip_accounts_get_default ();

		g_print (_("Available accounts:"));
		g_print ("\n");
		}
		
		for (l = accounts; l; l = l->next) {
			GossipAccount *account = l->data;
			
			g_print (" %s", gossip_account_get_name (account));
			if (strcmp (gossip_account_get_name (account), 
				    gossip_account_get_name (default_account)) == 0) {
				g_print (" ");
				g_print (_("[default]"));
			}

			g_print ("\n");
		}

		return EXIT_SUCCESS;
	}
	
	if (account_name) {
		GossipAccount *account;

		accounts = gossip_accounts_get_all (NULL);
		account = gossip_accounts_get_by_name (account_name);
		if (!account) {
			fprintf (stderr,
				 _("There is no account with the name '%s'."),
				 account_name);
			fprintf (stderr, "\n");
			return EXIT_FAILURE;
		}

		/* use the specified account as default account. */
		gossip_accounts_set_overridden_default (account_name);
	}

	gconf_client = gconf_client_get_default ();

	gconf_client_add_dir (gconf_client,
			      GCONF_PATH,
			      GCONF_CLIENT_PRELOAD_ONELEVEL,
			      NULL);

	gossip_stock_init ();
	setup_default_window_icon ();
	gossip_app_create ();
	
	if (!no_connect) {
		gossip_app_connect ();
	}
	
	gtk_main ();

	g_object_unref (gconf_client);

	return EXIT_SUCCESS;
}
