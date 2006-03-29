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

#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <libgnome/gnome-url.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include "libexslt/exslt.h"

#include <libgossip/gossip-message.h>
#include <libgossip/gossip-utils.h>

#include "gossip-app.h"
#include "gossip-log.h"

/*
 * A quick note about log structures:
 * 
 * Logs will be in ~/.gnome2/Gossip/logs
 * 
 * New log files are created daily per contact to make it easier to
 * manage older logs with file systems commands, etc.
 * 
 * The basic structure is:
 *   ~/.gnome2/Gossip/logs/<account>/<contact>-<date>.log
 *
 * Where <date> is "20060102" and represents a day. So each day as new
 * log is created for each contact you talk with. 
 *
 * In addition to all this, there is also a file in each account
 * directory called "contacts.ini" which holds a contact ID to
 * nickname translation for all contacts. This is updated daily when a
 * new file is created.
 */

/* #define DEBUG_MSG(x)   */
#define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n");  

#define LOG_HEADER \
    "<?xml version='1.0' encoding='utf-8'?>\n" \
    "<?xml-stylesheet type=\"text/xsl\" href=\"gossip-log.xsl\"?>\n" \
    "<log>\n"

#define LOG_FOOTER \
    "</log>\n"

#define LOG_FILENAME_PREFIX "file://"
#define LOG_FILENAME_SUFFIX ".log"

#define LOG_DTD_FILENAME    "gossip-log.dtd"

#define LOG_CREATE_MODE     (S_IRUSR | S_IWUSR | S_IXUSR)

#define LOG_KEY_FILENAME    "contacts.ini"
#define LOG_KEY_GROUP       "Contacts"
#define LOG_KEY_GROUP       "Contacts"

static gboolean  log_check_dir              (gchar         **directory);
static gchar *   log_get_filename           (const gchar    *account_id,
					     const gchar    *contact_id);
static gchar *   log_get_timestamp_full     (GossipMessage  *msg);
static gchar *   log_get_timestamp_filename (void);
static gchar *   log_urlify                 (const gchar    *msg);
static gboolean  log_transform              (const gchar    *infile,
					     gint            fd);
static void      log_show                   (GossipContact  *contact);
static GList *   log_parse_filename         (GossipContact  *own_contact,
					     GossipAccount  *account,
					     const gchar    *filename);
static GKeyFile *log_get_key_file           (const gchar    *account_id,
					     const gchar    *contact_id,
					     gchar         **filename);
static gchar *   log_get_nick               (const gchar    *account_id,
					     const gchar    *contact_id);
static gboolean  log_set_nick               (const gchar    *account_id,
					     const gchar    *contact_id,
					     const gchar    *nick);

static gboolean
log_check_dir (gchar **directory)
{
	gchar    *dir;
	gboolean  created = FALSE;

	dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, "logs", NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		DEBUG_MSG (("Log: Creating directory:'%s'", dir));
		created = TRUE;
		g_mkdir_with_parents (dir, LOG_CREATE_MODE);
	}

	if (directory) {
		*directory = dir;
	} else {
		g_free (dir);
	}

	return created;
}

static gchar *
log_get_filename_by_contact (GossipContact *contact)
{
	GossipAccount *account;
	const gchar   *account_id;
	const gchar   *contact_id;
	
	account = gossip_contact_get_account (contact);
	if (!account) {
		DEBUG_MSG (("Contact has no account, can not get log filename"));
		return NULL;
	}

	account_id = gossip_account_get_id (account);
	contact_id = gossip_contact_get_id (contact);

	return log_get_filename (account_id, contact_id);
}

static gchar *
log_get_filename (const gchar *account_id,
		  const gchar *contact_id)
{
	gchar *retval;
	gchar *contact_id_casefold;
	gchar *contact_id_escaped;
	gchar *account_id_casefold;
	gchar *account_id_escaped;
	gchar *log_directory;
	gchar *filename;
	gchar *basename;
	gchar *dirname;
	gchar *stamp;

	contact_id_casefold = g_utf8_casefold (contact_id, -1);
	contact_id_escaped = gnome_vfs_escape_host_and_path_string (contact_id_casefold);
	g_free (contact_id_casefold);

	account_id_casefold = g_utf8_casefold (account_id, -1);
	account_id_escaped = gnome_vfs_escape_host_and_path_string (account_id_casefold);
	g_free (account_id_casefold);
	
	stamp = log_get_timestamp_filename ();
	basename = g_strconcat (contact_id_escaped, "-", stamp, NULL);
	g_free (stamp);

	log_check_dir (&log_directory);
	filename = g_build_filename (log_directory,
				     account_id_escaped,
				     basename,
				     NULL);

	dirname = g_path_get_dirname (filename);
	if (!g_file_test (dirname, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		DEBUG_MSG (("Log: Creating directory:'%s'", dirname));
		g_mkdir_with_parents (dirname, LOG_CREATE_MODE);
	}

	g_free (dirname);
	g_free (basename);

	g_free (log_directory);
	g_free (contact_id_escaped);
	g_free (account_id_escaped);

	retval = g_strconcat (filename, LOG_FILENAME_SUFFIX, NULL);
	g_free (filename);

	DEBUG_MSG (("Log: Using contact log file:'%s' for contact:'%s'", 
		    retval, contact_id));

	return retval;
}

static gchar *
log_get_timestamp_full (GossipMessage *message)
{
	gossip_time_t t;

	t = gossip_message_get_timestamp (message);
	return gossip_time_to_timestamp_full (t, "%Y%m%dT%H:%M:%S");
}

static gchar *
log_get_timestamp_filename (void)
{
	gossip_time_t t;

	t = gossip_time_get_current ();
	return gossip_time_to_timestamp_full (t, "%Y%m%d");
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

	infile = log_get_filename_by_contact (contact);

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

	url = g_strconcat (LOG_FILENAME_PREFIX, filename, NULL);
	if (!gnome_url_show (url, NULL)) {
		g_warning ("Couldn't show log file:'%s'", filename);
	}

	g_free (url);
	g_free (filename);
}

static GList *
log_parse_filename (GossipContact *own_contact,
		    GossipAccount *account,
		    const gchar   *filename)
{
	xmlParserCtxtPtr  ctxt;
	xmlDocPtr         doc;
	xmlNodePtr        log_node;
	xmlNodePtr        node;
	GossipContact    *contact = NULL;
	GList            *messages = NULL;

	DEBUG_MSG (("Log: Attempting to parse filename:'%s'...", filename));

 	ctxt = xmlNewParserCtxt ();

	/* Parse and validate the file. */
	doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);	
	if (!doc) {
		g_warning ("Failed to parse file:'%s'", filename);
		xmlFreeParserCtxt (ctxt);
		return NULL;
	}

#if 0
	if (!gossip_utils_xml_validate (doc, STATUS_PRESETS_DTD_FILENAME)) {
		g_warning ("Failed to validate file:'%s'", filename);
		xmlFreeDoc(doc);
		xmlFreeParserCtxt (ctxt);
		return NULL;
	}
#endif

	/* The root node, presets. */
	log_node = xmlDocGetRootElement (doc);
	if (!log_node) {
		xmlFreeDoc (doc);
		xmlFreeParserCtxt (ctxt);
		
		return NULL;
	}

	/* First get the other contact's details. */
	for (node = log_node->children; node; node = node->next) {
		gchar *who;
		gchar *nick;

		if (strcmp (node->name, "message") != 0) {
			continue;
		}

		who = xmlGetProp (node, "to");
		if (!who) {
			who = xmlGetProp (node, "from");
		}

		nick = xmlGetProp (node, "nick");
		
		contact = gossip_contact_new_full (GOSSIP_CONTACT_TYPE_TEMPORARY, 
						   account,
						   who,
						   nick);
		break;
	}

	/* Now get the messages. */
	for (node = log_node->children; node; node = node->next) {
		GossipMessage *message;
		gchar         *time;
		gossip_time_t  t;
		gchar         *who;
		gchar         *nick;
		gchar         *body;
		gboolean       sent = TRUE;

		if (strcmp (node->name, "message") != 0) {
			continue;
		}

		body = xmlNodeGetContent (node);
		
		time = xmlGetProp (node, "time");
		t = gossip_time_from_string_full (time, "%Y%m%dT%H:%M:%S");
		
		nick = xmlGetProp (node, "nick");
		
		who = xmlGetProp (node, "to");
		if (!who) {
			sent = FALSE;
			who = xmlGetProp (node, "from");
		}
		
		if (sent) {			
			message = gossip_message_new (GOSSIP_MESSAGE_TYPE_NORMAL, contact);
			gossip_message_set_sender (message, own_contact);
		} else {
			message = gossip_message_new (GOSSIP_MESSAGE_TYPE_NORMAL, own_contact);
			gossip_message_set_sender (message, contact);
		}
		
		gossip_message_set_body (message, body);
		gossip_message_set_timestamp (message, t);
		
		messages = g_list_append (messages, message);
		
		xmlFree (time);
		xmlFree (who);
		xmlFree (nick);
		xmlFree (body);
	}
	
 	DEBUG_MSG (("Log: Parsed %d messages", g_list_length (messages))); 

	xmlFreeDoc (doc);
	xmlFreeParserCtxt (ctxt);
	
	if (contact) {
		g_object_unref (contact);
	}

	return messages;
}

static GKeyFile *
log_get_key_file (const gchar  *account_id,
		  const gchar  *contact_id,
		  gchar       **filename_to_return)
{
	GKeyFile *key_file;
	gchar    *account_id_casefold;
	gchar    *account_id_escaped;
	gchar    *log_directory;
	gchar    *filename;

	key_file = g_key_file_new ();

	account_id_casefold = g_utf8_casefold (account_id, -1);
	account_id_escaped = gnome_vfs_escape_host_and_path_string (account_id_casefold);
	g_free (account_id_casefold);

	log_check_dir (&log_directory);

	filename = g_build_filename (log_directory,
				     account_id_escaped,
				     LOG_KEY_FILENAME,
				     NULL);

	g_free (log_directory);
	g_free (account_id_escaped);

	g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL);

	if (filename_to_return) {
		*filename_to_return = filename;
	} else {
		g_free (filename);
	}

	return key_file;
}

static gchar *
log_get_nick (const gchar *account_id,
	      const gchar *contact_id)
{
	GKeyFile *key_file;
	gchar    *nick = NULL;

	key_file = log_get_key_file (account_id, contact_id, NULL);
	nick = g_key_file_get_string (key_file, LOG_KEY_GROUP, contact_id, NULL);

	g_key_file_free (key_file);

	return nick;
}

static gboolean 
log_set_nick (const gchar *account_id,
	      const gchar *contact_id,
	      const gchar *nick)
{
	GKeyFile *key_file;
	gchar    *filename;
	gchar    *content;
	gsize     length;
	gboolean  ok = TRUE;

	key_file = log_get_key_file (account_id, contact_id, &filename);
	g_key_file_set_string (key_file, LOG_KEY_GROUP, contact_id, nick);

	content = g_key_file_to_data (key_file, &length, NULL);
	if (content) {
		GError *error = NULL;

		DEBUG_MSG (("Log: The nick key file:'%s' has been saved with %d bytes of data",
			    filename, (int) length));

		ok = g_file_set_contents (filename, content, length, &error);
		g_free (content);

		if (error) {
			DEBUG_MSG (("Log: Could not file:'%s' with %d bytes of data, error:%d->'%s'",
				    filename, (int) length, error->code, error->message));
			g_error_free (error);
		}
	} else {
		DEBUG_MSG (("Log: Could not get content to save from GKeyFile"));
		ok = FALSE;
	}

	g_free (filename);
	g_key_file_free (key_file);

	return ok;
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

	filename = log_get_filename_by_contact (contact);

	log_check_dir (NULL);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		GossipAccount *account;

		file = g_fopen (filename, "w+");

		if (file) {
			g_fprintf (file, LOG_HEADER);
		}

		/* ONLY save the nick when we create new files, we are
		 * more efficient this way. 
		 */
		account = gossip_contact_get_account (own_contact);
		
		log_set_nick (gossip_account_get_id (account),
			      gossip_contact_get_id (contact),
			      gossip_contact_get_name (contact));
	} else {
		file = g_fopen (filename, "r+");
		if (file) {
			fseek (file, - strlen (LOG_FOOTER), SEEK_END);
		}
	}

	g_free (filename);

	if (!file) {
		return;
	}

	stamp = log_get_timestamp_full (msg);

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

	g_fprintf (file,
		   "<message time='%s' %s='%s' resource='%s' nick='%s'>"
		   "%s"
		   "</message>\n"
		   LOG_FOOTER,
		   stamp, 
		   to_or_from,
		   gossip_contact_get_id (contact),
		   resource,
		   nick,
		   body);
	
        fclose (file);

	g_chmod (filename, LOG_CREATE_MODE);

	g_free (stamp);
	g_free (body);
}

gboolean
gossip_log_exists (GossipContact *contact)
{
	gchar    *filename;
	gboolean  exists;

	filename = log_get_filename_by_contact (contact);
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

GList *
gossip_log_get_contacts (GossipAccount *account)
{
	GList         *contacts = NULL;
	GossipContact *contact;
	gchar         *log_directory;
	gchar         *directory;
	const gchar   *account_id;
	gchar         *account_id_escaped;
	GDir          *dir;
	const gchar   *filename;
	gchar         *filename_unescaped;
	gchar         *contact_id;
	const gchar   *p;
	gint           len_prefix;
	gint           len_suffix;
	gchar         *nick;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	if (log_check_dir (&log_directory)) {
		DEBUG_MSG (("Log: No log directory exists"));
		return NULL;
	}

	account_id = gossip_account_get_id (account);
	account_id_escaped = gnome_vfs_escape_host_and_path_string (account_id);
	directory = g_build_path (G_DIR_SEPARATOR_S, 
				  log_directory, 
				  account_id_escaped,
				  NULL);
	g_free (account_id_escaped);
	g_free (log_directory);
	
	if (!g_file_test (directory, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		DEBUG_MSG (("Log: Log directory exists, but no account directory does, trying:'%s'",
			    directory));
		g_free (directory);
		return NULL;
	}

	dir = g_dir_open (directory, 0, NULL);
	if (!dir) {
		DEBUG_MSG (("Log: Could not open directory:'%s'", directory));
		g_free (directory);
		return NULL;
	}

	DEBUG_MSG (("Log: Obtaining contacts in log directory:'%s'",
		    directory));

	len_prefix = strlen (LOG_FILENAME_PREFIX);
	len_suffix = strlen (LOG_FILENAME_SUFFIX);

	while ((filename = g_dir_read_name (dir)) != NULL) {
		p = filename;

		p += strlen (filename);
		p -= len_suffix;

		/* If the extension does not match, ignore it */
		if (strcmp (p, LOG_FILENAME_SUFFIX) != 0) {
			continue;
		}

		filename_unescaped = gnome_vfs_unescape_string_for_display (filename);
		if (!filename_unescaped) {
			continue;
		}

		DEBUG_MSG (("Log: Using log file:'%s' to create contact", filename_unescaped));
		
		p = filename_unescaped;
		if (strstr (filename_unescaped, LOG_FILENAME_PREFIX)) {
			p += len_prefix;
		} else if (strstr (filename_unescaped, "file:")) {
			p += 5;
		}

		/* The 9 that is subtracted is the "-YYYYMMDD" part of the file name. */
		if (strstr (filename_unescaped, LOG_FILENAME_SUFFIX)) {
			contact_id = g_strndup (p, strlen (p) - len_suffix - 9);
		} else {
			contact_id = g_strndup (p, strlen (p) - 9);
		}

		g_free (filename_unescaped);

		nick = log_get_nick (account_id, contact_id);
		contact = gossip_contact_new_full (GOSSIP_CONTACT_TYPE_TEMPORARY, 
						   account,
						   contact_id,
						   nick);
		DEBUG_MSG (("Log: Adding to contacts id:'%s' with nick:'%s'", 
			    contact_id, nick));
		g_free (nick);

		contacts = g_list_append (contacts, contact);
	}
	
	g_free (directory);
	g_dir_close (dir);
	
	return contacts;
}

GList *  
gossip_log_get_for_contact (GossipContact *own_contact,
			    GossipAccount *account,
			    const gchar   *contact_id)
{
	gchar         *filename;
	GList         *messages;

 	g_return_val_if_fail (GOSSIP_IS_CONTACT (own_contact), NULL);
 	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (contact_id != NULL, NULL);

	filename = log_get_filename (gossip_account_get_id (account), contact_id);
	DEBUG_MSG (("Log: Retrieving for:'%s', filename:'%s'", contact_id, filename));
	
	messages = log_parse_filename (own_contact, account, filename);

	g_free (filename);

	return messages;
}
