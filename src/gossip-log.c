/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2006 Imendio AB
 * Copyright (C) 2003      Johan Wallenborg <johan.wallenborg@fishpins.se>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libgnome/gnome-url.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include "libexslt/exslt.h"

#include <libgossip/gossip-utils.h>

#include "gossip-app.h"
#include "gossip-log.h"

/* #define DEBUG_MSG(x)   */
#define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n");  

#define LOG_HEADER \
    "<?xml version='1.0' encoding='UTF-8'?>\n" \
    "<?xml-stylesheet type=\"text/xsl\" href=\"gossip-log.xsl\"?>\n" \
    "<gossip xmlns:log=\"http://gossip.imendio.org/ns/log\" version=\"1\">\n" \

#define LOG_FOOTER \
    "</gossip>\n"

static void     log_ensure_dir    (void);
static gchar *  log_get_filename  (GossipContact *contact,
				   const gchar   *suffix);
static gchar *  log_get_timestamp (GossipMessage *msg);
static gchar *  log_urlify        (const gchar   *msg);
static gboolean log_transform     (const gchar   *infile,
				   gint           fd);
static void     log_show          (GossipContact *contact);

static void
log_ensure_dir (void)
{
	gchar *dir;

	dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, "logs", NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		DEBUG_MSG (("Log: Creating directory:'%s'", dir));
		g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	}

	DEBUG_MSG (("Log: Using directory:'%s'", dir));
	g_free (dir);
}

static gchar *
log_get_filename (GossipContact *contact, 
		  const gchar   *suffix)
{
	gchar *retval;
	gchar *contact_id;
	gchar *escaped_filename;
	gchar *filename;

	contact_id = g_utf8_casefold (gossip_contact_get_id (contact), -1);
	escaped_filename = gnome_vfs_escape_host_and_path_string (contact_id);
	g_free (contact_id);

	filename = g_build_filename (g_get_home_dir (),
				     ".gnome2", PACKAGE_NAME, "logs",
				     escaped_filename,
				     NULL);
	g_free (escaped_filename);

	retval = g_strconcat (filename, suffix, NULL);
	g_free (filename);

	return retval;
}

static gchar *
log_get_timestamp (GossipMessage *message)
{
	gossip_time_t t;

	t = gossip_message_get_timestamp (message);
	return gossip_time_to_timestamp_full (t, "%Y%m%dT%H:%M:%S");
}

static gchar *
log_urlify (const gchar *msg)
{
	gint     num_matches, i;
	GArray  *start, *end;
	GString *ret;
	gchar   *esc;

	ret = g_string_new (NULL);

	start = g_array_new (FALSE, FALSE, sizeof (gint));
	end = g_array_new (FALSE, FALSE, sizeof (gint));

	num_matches = gossip_utils_url_regex_match (msg, start, end);

	if (num_matches == 0) {
		esc = g_markup_escape_text (msg, -1);
		g_string_append (ret, esc);
		g_free (esc);
	} else {
		gint   last = 0;
		gint   s = 0, e = 0;
		gchar *tmp;

		for (i = 0; i < num_matches; i++) {

			s = g_array_index (start, gint, i);
			e = g_array_index (end, gint, i);

			if (s > last) {
				tmp = gossip_utils_substring (msg, last, s);
				esc = g_markup_escape_text (tmp, -1);
				g_string_append (ret, esc);
				g_free (tmp);
				g_free (esc);
			}

			tmp = gossip_utils_substring (msg, s, e);

			g_string_append (ret, "<a href=\"");

			esc = g_markup_escape_text (tmp, -1);
			g_string_append (ret, esc);
			g_free (esc);

			g_string_append (ret, "\">");

			esc = g_markup_escape_text (tmp, -1);
			g_string_append (ret, esc);
			g_free (esc);

			g_string_append (ret, "</a>");

			last = e;

			g_free (tmp);
		}

		if (e < strlen (msg)) {
			tmp = gossip_utils_substring (msg, e, strlen (msg));
			esc = g_markup_escape_text (tmp, -1);
			g_string_append (ret, esc);
			g_free (tmp);
			g_free (esc);
		}
	}

	g_array_free (start, TRUE);
	g_array_free (end, TRUE);

	return g_string_free (ret, FALSE);
}

static gboolean
log_transform (const gchar *infile, 
	       gint         fd)
{
        xsltStylesheet  *stylesheet;
        xmlDoc          *xml_doc;
        xmlDoc          *html_doc;
	gchar           *title;
	const gchar     *params[6];

	/* Setup libxml. */
	xmlSubstituteEntitiesDefault (1);
	xmlLoadExtDtdDefaultValue = 1;
	exsltRegisterAll ();

        stylesheet = xsltParseStylesheetFile ((const xmlChar *)STYLESHEETDIR "/gossip-log.xsl");
	if (!stylesheet) {
		return FALSE;
	}

        xml_doc = xmlParseFile (infile);
	if (!xml_doc) {
		return FALSE;
	}

	title = g_strconcat ("\"", _("Conversation Log"), "\"", NULL);

	/* Set params to be passed to stylesheet. */
	params[0] = "title";
	params[1] = title;
        params[2] = NULL;

        html_doc = xsltApplyStylesheet (stylesheet, xml_doc, params);
	if (!html_doc) {
		xmlFreeDoc (xml_doc);
		return FALSE;
	}

        xmlFreeDoc (xml_doc);

	xsltSaveResultToFd (fd, html_doc, stylesheet);

	xsltFreeStylesheet (stylesheet);
        xmlFreeDoc (html_doc);

	g_free (title);

	return TRUE;
}

static void
log_show (GossipContact *contact)
{
	gchar *filename = NULL;
	gchar *infile, *outfile;
	gchar *url;
	FILE  *file;
	int   fd;

	infile = log_get_filename (contact, ".log");

	if (!g_file_test (infile, G_FILE_TEST_EXISTS)) {
		file = fopen (infile, "w+");
		if (file) {
			fprintf (file, LOG_HEADER LOG_FOOTER);
			fclose (file);
		}
	}

	if (!g_file_test (infile, G_FILE_TEST_EXISTS)) {
		g_free (infile);
		return;
	}

	fd = g_file_open_tmp ("gossip-log-XXXXXX", &outfile, NULL);
	if (fd == -1) {
		return;
	}

	DEBUG_MSG (("Log: Using temporary file:'%s'", outfile));

	if (!log_transform (infile, fd)) {
		g_warning ("Couldn't transform log file:'%s'", outfile);
		g_free (infile);
		g_free (outfile);
		return;
	}

	close (fd);

	filename = g_strconcat (outfile, ".html", NULL);
	DEBUG_MSG (("Log: Renaming temporary file:'%s' to '%s'", outfile, filename));
	if (rename (outfile, filename) == -1) {
		g_warning ("Couldn't rename temporary log file:'%s' to '%s'", 
			   outfile, filename);
		g_free (outfile);
		g_free (filename);
		return;
	}

	g_free (outfile);

	url = g_strconcat ("file://", filename, NULL);
	if (!gnome_url_show (url, NULL)) {
		g_warning ("Couldn't show log file:'%s'", filename);
	}

	g_free (url);
	g_free (filename);
}

void
gossip_log_message (GossipContact *own_contact,
		    GossipMessage *msg, 
		    gboolean       incoming)
{
	GossipContact *contact;
        gchar         *filename;
	FILE          *file;
	gchar         *body;
	const gchar   *to_or_from;
	gchar         *stamp;
	const gchar   *nick;
	const gchar   *resource;

	if (incoming) {
		contact = gossip_message_get_sender (msg);
	} else {
		contact = gossip_message_get_recipient (msg);
	}

	filename = log_get_filename (contact, ".log");

	log_ensure_dir ();

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		file = fopen (filename, "w+");
		if (file) {
			fprintf (file, LOG_HEADER);
		}
	} else {
		file = fopen (filename, "r+");
		if (file) {
			fseek (file, - strlen (LOG_FOOTER), SEEK_END);
		}
	}

	g_free (filename);

	if (!file) {
		return;
	}

	stamp = log_get_timestamp (msg);

	if (incoming) {
		to_or_from = "from";
		nick = gossip_contact_get_name (contact);
	} else {
		to_or_from = "to";
 		nick = gossip_contact_get_name (own_contact);
	}

        if (gossip_message_get_body (msg)) {
		body = log_urlify (gossip_message_get_body (msg));
	} else {
		body = g_strdup ("");
	}

	resource = gossip_message_get_explicit_resource (msg);
	if (!resource) {
		resource = "";
	}

	fprintf (file,
		 "\t<message time='%s' %s='%s' resource='%s' nick='%s'>\n"
		 "\t\t%s\n"
		 "\t</message>\n"
		 LOG_FOOTER,
		 stamp, 
		 to_or_from,
		 gossip_contact_get_id (contact),
		 resource,
		 nick,
		 body);

        fclose (file);

	g_free (stamp);
	g_free (body);
}

gboolean
gossip_log_exists (GossipContact *contact)
{
	gchar    *filename;
	gboolean  exists;

	filename = log_get_filename (contact, ".log");
	exists = g_file_test (filename, G_FILE_TEST_EXISTS);
	g_free (filename);

	return exists;
}

void
gossip_log_show (GtkWidget     *window, 
		 GossipContact *contact)
{
	GdkCursor *cursor;

	cursor = gdk_cursor_new (GDK_WATCH);

	gdk_window_set_cursor (window->window, cursor);
	gdk_cursor_unref (cursor);

	/* Let the UI redraw before we start the slow transformation. */
	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}

	log_show (contact);

	gdk_window_set_cursor (window->window, NULL);
}

