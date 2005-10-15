/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2005 Imendio AB
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

#include <config.h>

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

#define LOG_HEADER \
    "<?xml version='1.0' encoding='UTF-8'?>\n" \
    "<?xml-stylesheet type=\"text/xsl\" href=\"gossip-log.xsl\"?>\n" \
    "<gossip xmlns:log=\"http://gossip.imendio.org/ns/log\" version=\"1\">\n" \

#define LOG_FOOTER \
    "</gossip>\n"

static void
log_ensure_dir (void)
{
	gchar *dir;

	dir = g_build_filename (g_get_home_dir (), ".gnome2", NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		mkdir (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	}
	g_free (dir);

	dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		mkdir (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	}
	g_free (dir);

	dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, "logs", NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		mkdir (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	}
	g_free (dir);
}

static gchar *
log_get_filename (GossipContact *contact, const gchar *suffix)
{
	gchar *tmp;
	gchar *ret;
	gchar *case_folded_str;
	gchar *escaped_str;

	case_folded_str = g_utf8_casefold (gossip_contact_get_id (contact),
					   -1);
	escaped_str = gnome_vfs_escape_host_and_path_string (case_folded_str);
	g_free (case_folded_str);

	tmp = g_build_filename (g_get_home_dir (),
				".gnome2", PACKAGE_NAME, "logs",
				escaped_str,
				NULL);
	g_free (escaped_str);

	ret = g_strconcat (tmp, suffix, NULL);
	g_free (tmp);

	return ret;
}

static gchar *
log_get_timestamp (GossipMessage *msg)
{
	gossip_time_t t;

	t = gossip_message_get_timestamp (msg);
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

void
gossip_log_message (GossipMessage *msg, gboolean incoming)
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
 		nick = gossip_session_get_nickname (gossip_app_get_session ());
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
		 "  <message time='%s' %s='%s' resource='%s' nick='%s'>\n"
		 "    %s\n"
		 "  </message>\n"
		 LOG_FOOTER,
		 stamp, to_or_from,
		 gossip_contact_get_id (contact),
		 resource,
		 nick,
		 body);

        fclose (file);

	g_free (stamp);
	g_free (body);
}

static gboolean
log_transform (const gchar *infile, gint fd_outfile)
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

	xsltSaveResultToFd (fd_outfile, html_doc, stylesheet);

	xsltFreeStylesheet (stylesheet);
        xmlFreeDoc (html_doc);

	g_free (title);

	return TRUE;
}

gboolean
gossip_log_exists (GossipContact *contact)
{
	gchar    *infile;
	gboolean  exists;

	infile = log_get_filename (contact, ".log");
	exists = g_file_test (infile, G_FILE_TEST_EXISTS);

	g_free (infile);

	return exists;
}

/* Copyright (C) 1991, 1992, 1996, 1998 Free Software Foundation, Inc.
 * This file is derived from mkstemp.c from the GNU C Library.
 */

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#ifndef TMP_MAX
#define TMP_MAX 16384
#endif

static int
our_mkstemps (char *template, int suffix_len)
{
	static const char letters[]
		= "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	static guint64 value;
	struct timeval tv;
	char *XXXXXX;
	size_t len;
	int count;

	len = strlen (template);

	if ((int) len < 6 + suffix_len
	    || strncmp (&template[len - 6 - suffix_len], "XXXXXX", 6))
	{
		return -1;
	}

	XXXXXX = &template[len - 6 - suffix_len];

	/* Get some more or less random data.  */
	gettimeofday (&tv, NULL);
	value += ((guint64) tv.tv_usec << 16) ^ tv.tv_sec ^ getpid ();

	for (count = 0; count < TMP_MAX; ++count)
	{
		guint64 v = value;
		int fd;

		/* Fill in the random bits.  */
		XXXXXX[0] = letters[v % 62];
		v /= 62;
		XXXXXX[1] = letters[v % 62];
		v /= 62;
		XXXXXX[2] = letters[v % 62];
		v /= 62;
		XXXXXX[3] = letters[v % 62];
		v /= 62;
		XXXXXX[4] = letters[v % 62];
		v /= 62;
		XXXXXX[5] = letters[v % 62];

		fd = open (template, O_RDWR|O_CREAT|O_EXCL, 0600);
		if (fd >= 0)
			/* The file does not exist.  */
			return fd;

		/* This is a random value.  It is only necessary that the next
		   TMP_MAX values generated by adding 7777 to VALUE are different
		   with (module 2^32).  */
		value += 7777;
	}

	/* We return the null string if we can't find a unique file name.  */
	template[0] = '\0';

	return -1;
}

static void
log_show (GossipContact *contact)
{
	gchar *infile, *outfile, *url;
	FILE  *file;
	int    fd;

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

	outfile = g_build_filename (g_get_tmp_dir (),
				    "gossip-log-XXXXXX.html",
				    NULL);

	fd = our_mkstemps (outfile, 5);
	if (fd == -1) {
		g_free (outfile);
		return;
	}

	if (!log_transform (infile, fd)) {
		/* Error dialog... */

		g_free (infile);
		g_free (outfile);
		return;
	}

	close (fd);

	url = g_strconcat ("file://", outfile, NULL);
	if (!gnome_url_show (url, NULL)) {
		/* Error dialog... */
	}

	g_free (url);
	g_free (outfile);
}

void
gossip_log_show (GtkWidget *window, GossipContact *contact)
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

