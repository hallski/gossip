/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002 Richard Hult <rhult@imendo.com>
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
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkmain.h>
#include <gconf/gconf-client.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-ui-init.h>
#include "gossip-app.h"

GConfClient *gconf_client = NULL;

static void
setup_default_window_icon (void)
{
	GList        *list;
	GdkPixbuf    *pixbuf;

	pixbuf = gdk_pixbuf_new_from_file (IMAGEDIR "/gossip-online.png", NULL);

	list = g_list_append (NULL, pixbuf);

	gtk_window_set_default_icon_list (list);

	g_list_free (list);

	g_object_unref (pixbuf);
}

int
main (int argc, char *argv[])
{
	GossipApp          *app;
	GnomeProgram       *program;
	gboolean            no_connect = FALSE;
	poptContext         popt_context;
	const gchar       **args;
	struct poptOption   options[] = {
		{ "no-connect", 'a', POPT_ARG_NONE, &no_connect, 0,
		  N_("Don't connect on startup."), NULL },
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};
	
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (PACKAGE);

	program = gnome_program_init (PACKAGE, VERSION,
				      LIBGNOMEUI_MODULE,
                                      argc, argv,
                                      GNOME_PROGRAM_STANDARD_PROPERTIES,
				      GNOME_PARAM_POPT_TABLE, options,
				      GNOME_PARAM_HUMAN_READABLE_NAME, _("Gossip"),
				      NULL);

	g_object_get (program,
		      GNOME_PARAM_POPT_CONTEXT,
		      &popt_context,
		      NULL);

	args = poptGetArgs (popt_context);

	gconf_client = gconf_client_get_default ();

	gconf_client_add_dir (gconf_client,
			      "/apps/gossip",
			      GCONF_CLIENT_PRELOAD_NONE,
			      NULL);
	
	setup_default_window_icon ();
	
	app = gossip_app_new ();

	if (!no_connect) {
		gossip_app_connect_default (app);
	}
	
	gtk_main ();

	g_object_unref (gconf_client);

	return 0;
}
