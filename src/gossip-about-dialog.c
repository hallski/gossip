/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
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
#include <glib/gi18n.h>
#include <gtk/gtkaboutdialog.h>

#include "gossip-about-dialog.h"
#include "gossip-ui-utils.h"

#define WEB_SITE "http://developer.imendio.com/projects/gossip/"

static void about_dialog_activate_link_cb (GtkAboutDialog  *about,
					   const gchar     *link,
					   gpointer         data);

static const char *authors[] = {
	"Mikael Hallendal",
	"Richard Hult",
	"Martyn Russell",
	"Geert-Jan Van den Bogaerde",
	"Kevin Dougherty",
	"Eitan Isaacson",
	"Xavier Claessens",
	NULL
};

static const char *documenters[] = {
	"Daniel Taylor",
	"Keywan Najafi Tonekaboni",
	"Brian Pepple",
	NULL
};

static const char *artists[] = {
	"Daniel Taylor",
	NULL
};

static const char *license[] = {
	N_("Gossip is free software; you can redistribute it and/or modify "
	   "it under the terms of the GNU General Public License as published by "
	   "the Free Software Foundation; either version 2 of the License, or "
	   "(at your option) any later version."),
	N_("Gossip is distributed in the hope that it will be useful, "
	   "but WITHOUT ANY WARRANTY; without even the implied warranty of "
	   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
	   "GNU General Public License for more details."),
	N_("You should have received a copy of the GNU General Public License "
	   "along with Gossip; if not, write to the Free Software Foundation, Inc., "
	   "51 Franklin Street, Fifth Floor, Boston, MA 02110-130159 USA")
};

static void
about_dialog_activate_link_cb (GtkAboutDialog *about,
			       const gchar    *link,
			       gpointer        data)
{
	gossip_url_show (link);
}

void
gossip_about_dialog_new (GtkWindow *parent)
{
	gchar       *license_trans;
	gchar       *comments;
	gchar       *technology;
	const gchar *backend;

	gtk_about_dialog_set_url_hook (about_dialog_activate_link_cb, NULL, NULL);

	license_trans = g_strconcat (_(license[0]), "\n\n",
				     _(license[1]), "\n\n",
				     _(license[2]), "\n\n",
				     NULL);

#ifdef USE_TELEPATHY
	backend = "Telepathy";
#else
	backend = "Jabber";
#endif
	
	technology = g_strdup_printf (_("Using the %s backend"), backend);
	comments = g_strdup_printf ("%s\n%s",
				    _("An Instant Messaging client for GNOME"),
	                            technology);
	g_free (technology);

	gtk_show_about_dialog (parent,
			       "artists", artists,
			       "authors", authors,
			       "comments", comments,
			       "license", license_trans,
			       "wrap-license", TRUE,
			       "copyright", "Imendio AB 2002-2007",
			       "documenters", documenters,
			       "logo-icon-name", "gossip",
			       "translator-credits", _("translator-credits"),
			       "version", PACKAGE_VERSION,
			       "website", WEB_SITE,
			       NULL);

	g_free (license_trans);
	g_free (comments);
}


