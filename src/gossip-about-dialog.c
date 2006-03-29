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
#include <libgnome/gnome-url.h>

#include "gossip-about-dialog.h"

#define WEB_SITE "http://developer.imendio.com/wiki/gossip"

static void about_dialog_activate_link_cb (GtkAboutDialog  *about,
					   const gchar     *link,
					   gpointer         data);

const char *authors[] = {
	"Mikael Hallendal",
	"Richard Hult",
	"Martyn Russell",
	"Geert-Jan Van den Bogaerde",
	"Kevin Dougherty",
	NULL
};

const char *documenters[] = {
	"Daniel Taylor",
	"Keywan Najafi Tonekaboni",
	"Brian Pepple",
	NULL
};

const char *artists[] = {
	"Daniel Taylor",
	NULL
};

const char *license[] = {
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
	   "59 Temple Place, Suite 330, Boston, MA  02111-1307  USA")
};

static void
about_dialog_activate_link_cb (GtkAboutDialog *about,
			       const gchar    *link,
			       gpointer        data)
{
	gnome_url_show (link, NULL);
}

void
gossip_about_dialog_new (GtkWindow *parent)
{
	char *license_trans;

	license_trans = g_strconcat (_(license[0]), "\n\n", 
				     _(license[1]), "\n\n",
				     _(license[2]), "\n\n", 
				     NULL);

        gtk_show_about_dialog (parent,
			       "artists", artists,
			       "authors", authors,
			       "comments", _("An Instant Messaging client for GNOME"),
			       "license", license_trans,
			       "wrap-license", TRUE,
			       "copyright", "Imendio AB 2002-2006",
			       "documenters", documenters,
			       "logo-icon-name", "gossip",
			       "translator-credits", _("translator-credits"),
			       "version", PACKAGE_VERSION,
			       NULL);

	gtk_about_dialog_set_url_hook (about_dialog_activate_link_cb, NULL, NULL);

	g_free (license_trans);
}


