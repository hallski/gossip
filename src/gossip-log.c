/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2004 Imendio HB
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
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-url.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include "libexslt/exslt.h"
#include "gossip-utils.h"
#include "gossip-app.h"
#include "gossip-roster.h"
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
	
	dir = g_build_filename (g_get_home_dir (), ".gnome2", "Gossip", NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		mkdir (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	}
	g_free (dir);

	dir = g_build_filename (g_get_home_dir (), ".gnome2", "Gossip", "logs", NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		mkdir (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	}
	g_free (dir);
}

static gchar *
log_get_filename (GossipJID *jid, const gchar *suffix)
{
	gchar *tmp;
	gchar *ret;
	
	tmp = g_build_filename (g_get_home_dir (),
				".gnome2", "Gossip", "logs",
				gossip_jid_get_without_resource (jid),
				NULL);

	ret = g_strconcat (tmp, suffix, NULL);
	g_free (tmp);

	return ret;
}

static gchar *
log_get_timestamp (LmMessage *msg)
{
	const gchar *stamp;
	time_t       t;
	struct tm   *tm;
	gchar        buf[128];
	
	stamp = gossip_utils_get_timestamp_from_message (msg);
	if (stamp) {
		tm = lm_utils_get_localtime (stamp);
	} else {
		t  = time (NULL);
		tm = localtime (&t);
	}
	
	buf[0] = 0;
	strftime (buf, sizeof (buf), "%Y%m%dT%H:%M:%S", tm);

	return g_strdup (buf);
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
gossip_log_message (LmMessage *msg, gboolean incoming)
{
	const gchar   *jid_string;
        GossipJID     *jid;
        gchar         *filename;
	FILE          *file;
	gchar         *body;
	const gchar   *to_or_from;
	gchar         *stamp;
	LmMessageNode *node;
	gchar         *nick;
	const gchar   *resource;
	
	if (incoming) {
		jid_string = lm_message_node_get_attribute (msg->node, "from");
	} else {
		jid_string = lm_message_node_get_attribute (msg->node, "to");
	}

	jid = gossip_jid_new (jid_string);
	
	filename = log_get_filename (jid, ".log");

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
		gossip_jid_unref (jid);
		return;
	}
	
	stamp = log_get_timestamp (msg);

	if (incoming) {
		GossipRosterItem *item;

		item = gossip_roster_get_item (gossip_app_get_roster (), jid);
		if (!item) {
			nick = gossip_jid_get_part_name (jid);
		} else {
			nick = g_strdup (gossip_roster_item_get_name (item));
		}
		
		to_or_from = "from";
	} else {
		nick = g_strdup (gossip_app_get_username ());

		to_or_from = "to";
	}

	node = lm_message_node_get_child (msg->node, "body");
	if (node) {
		gchar *tmp;

		tmp = log_urlify (node->value);
		body = g_strdup (tmp); //g_markup_escape_text (tmp, -1);
		g_free (tmp);
	} else {
		body = g_strdup ("");
	}

	resource = gossip_jid_get_resource (jid);
	if (!resource) {
		resource = "";
	}
	
	fprintf (file,
		 "  <message time='%s' %s='%s' resource='%s' nick='%s'>\n"
		 "    %s\n"
		 "  </message>\n"
		 LOG_FOOTER,
		 stamp, to_or_from,
		 gossip_jid_get_without_resource (jid),
		 resource,
		 nick,
		 body);
	
        fclose (file);

	gossip_jid_unref (jid);
	g_free (stamp);
	g_free (nick);
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

        stylesheet = xsltParseStylesheetFile (STYLESHEETDIR "/gossip-log.xsl");
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
gossip_log_exists (GossipJID *jid)
{
	gchar    *infile;
	gboolean  exists;

	infile = log_get_filename (jid, ".log");
	exists = g_file_test (infile, G_FILE_TEST_EXISTS);
	g_free (infile);

	return exists;
}	

void
gossip_log_show (GossipJID *jid)
{
	gchar *infile, *outfile, *url;
	FILE  *file;
	int    fd;

	infile = log_get_filename (jid, ".log");

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

	outfile = g_build_filename (g_get_tmp_dir (), "gossip-log-XXXXXX", NULL);

	fd = mkstemp (outfile);
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
