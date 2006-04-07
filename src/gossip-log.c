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

#ifdef USE_GNOMEVFS_FOR_URL
#include <libgnomevfs/gnome-vfs.h>
#else
#include <libgnome/gnome-url.h>
#endif

#include <libgnomevfs/gnome-vfs-utils.h>
#include <libxslt/xslt.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include "libexslt/exslt.h"

#include <libgossip/gossip-message.h>
#include <libgossip/gossip-utils.h>

#include "gossip-app.h"
#include "gossip-log.h"
#include "gossip-marshal.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_LOG, GossipLogPriv))

/* #define DEBUG_MSG(x)   */
#define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n");  

#define LOG_HEADER \
    "<?xml version='1.0' encoding='utf-8'?>\n" \
    "<?xml-stylesheet type=\"text/xsl\" href=\"gossip-log.xsl\"?>\n" \
    "<log>\n"

#define LOG_FOOTER \
    "</log>\n"

#define LOG_FILENAME_PREFIX    "file://"
#define LOG_FILENAME_SUFFIX    ".log"

#define LOG_DTD_FILENAME       "gossip-log.dtd"

#define LOG_CREATE_MODE        (S_IRUSR | S_IWUSR)

#define LOG_KEY_FILENAME       "contacts.ini"
#define LOG_KEY_GROUP_SELF     "Self"
#define LOG_KEY_GROUP_CONTACTS "Contacts"

#define LOG_KEY_VALUE_NAME     "Name"

#define LOG_TIME_FORMAT_FULL   "%Y%m%dT%H:%M:%S"
#define LOG_TIME_FORMAT        "%Y%m%d"

struct _GossipLogPriv {
	GossipContact *contact;
	GossipContact *own_contact;

	/* Currently used file details for new messages */
	gchar         *filename;
	gchar         *date;
}; 

struct _GossipLogSearchHit {
	gchar         *account_id;
	gchar         *contact_id;
	gchar         *contact_name;
	gchar         *filename;
	gchar         *date;
};

static void           gossip_log_class_init            (GossipLogClass  *klass);
static void           gossip_log_init                  (GossipLog       *log);
static void           gossip_log_finalize              (GObject         *object);
static gboolean       log_check_dir                    (gchar          **directory);
static gchar *        log_get_timestamp_from_message   (GossipMessage   *msg);
static gchar *        log_get_timestamp_filename       (void);
static gchar *        log_get_contact_id_from_filename (const gchar     *filename);
static gchar *        log_get_account_id_from_filename (const gchar     *filename);
static gchar *        log_get_date_from_filename       (const gchar     *filename);
static gchar *        log_get_filename_for_date        (GossipLog       *log,
							const gchar     *date);
static const gchar *  log_get_filename_for_today       (GossipLog       *log);
static GList *        log_get_messages                 (GossipLog       *log,
							const gchar     *filename);
static const gchar *  log_get_contacts_filename        (GossipAccount   *account);
static GossipContact *log_get_contact                  (GossipAccount   *account,
							const gchar     *contact_id);
static gchar         *log_get_contact_name_from_id     (const gchar     *account_id,
							const gchar     *contact_id);
static gboolean       log_set_name                     (GossipAccount   *account,
							GossipContact   *contact);
static GossipContact *log_get_own_contact              (GossipAccount   *account);
static gboolean       log_set_own_name                 (GossipAccount   *account,
							GossipContact   *contact);
static gchar *        log_urlify                       (const gchar     *msg);
static gboolean       log_transform                    (const gchar     *infile,
							gint             fd);
static void           log_show                         (GossipLog       *log);

enum {
	NEW_MESSAGE,

	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static gpointer parent_class = NULL;

static GHashTable *logs = NULL;
static GHashTable *contact_files = NULL;

G_DEFINE_TYPE (GossipLog, gossip_log, G_TYPE_OBJECT);

static void
gossip_log_class_init (GossipLogClass *klass)
{
        GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gossip_log_finalize;

	signals[NEW_MESSAGE] = 
		g_signal_new ("new-message",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
                              gossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_MESSAGE);

  	g_type_class_add_private (object_class, sizeof (GossipLogPriv));
}

static void
gossip_log_init (GossipLog *log)
{
}

static void
gossip_log_finalize (GObject *object)
{
	GossipLogPriv *priv;

	priv = GET_PRIV (object);

	if (priv->contact) {
		g_object_unref (priv->contact);
	}

	if (priv->own_contact) {
		g_object_unref (priv->own_contact);
	}

	g_free (priv->filename);
	g_free (priv->date);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

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
log_get_timestamp_from_message (GossipMessage *message)
{
	gossip_time_t t;

	t = gossip_message_get_timestamp (message);
	return gossip_time_to_timestamp_full (t, LOG_TIME_FORMAT_FULL);
}

static gchar *
log_get_timestamp_filename (void)
{
	gossip_time_t t;

	t = gossip_time_get_current ();
	return gossip_time_to_timestamp_full (t, LOG_TIME_FORMAT);
}

static gchar *
log_get_contact_id_from_filename (const gchar *filename)
{
	gchar *basename;
	gchar *p;

	basename = g_path_get_basename (filename);
	p = strstr (basename, "-");

	p[0] = '\0';

	return basename;
}

static gchar *
log_get_account_id_from_filename (const gchar *filename)
{
	gchar *str;
	gchar *dirname;
	gchar *account_id;

	dirname = g_path_get_dirname (filename);

	str = g_strrstr (dirname, G_DIR_SEPARATOR_S);
	if (!str) {
		g_free (dirname);
		return NULL;
	}

	str++;

	account_id = g_strdup (str);
	g_free (dirname);

	return account_id; 
}

static gchar *
log_get_date_from_filename (const gchar *filename)
{
	const gchar *start;

	if (!g_str_has_suffix (filename, LOG_FILENAME_SUFFIX)) {
		return NULL;
	}

	start = g_strrstr (filename, "-");
	if (!start) {
		return NULL;
	}

	start++;
	return g_strndup (start, strlen (start) - strlen (LOG_FILENAME_SUFFIX));
}

static gboolean
log_get_all_log_files (const gchar  *root_location, 
		       GList       **files)
{
	GDir        *dir;
	const gchar *filename;
	gchar       *location;

	gboolean     ok = TRUE;

	g_return_val_if_fail (files != NULL, FALSE);

	if (root_location == NULL) {
		gchar *log_directory;

		*files = NULL;

		if (log_check_dir (&log_directory)) {
			DEBUG_MSG (("Log: No log directory exists"));
			return TRUE;
		}

		ok = log_get_all_log_files (log_directory, files);
		g_free (log_directory);

		return ok;
	}
	
	dir = g_dir_open (root_location, 0, NULL);
	if (!dir) {
		DEBUG_MSG (("Log: Could not open directory:'%s'", root_location));
		return FALSE;
	}
	
	DEBUG_MSG (("Log: Finding all log files in directory:'%s'", root_location));
	
	while (ok && (filename = g_dir_read_name (dir)) != NULL) {
		location = g_build_filename (root_location, filename, NULL);
		
		if (g_file_test (location, G_FILE_TEST_IS_DIR)) {
			/* Open directory */
			ok = log_get_all_log_files (location, files);
		} else {
			if (g_str_has_suffix (filename, LOG_FILENAME_SUFFIX)) {
				*files = g_list_insert_sorted (*files, 
							       location, 
							       (GCompareFunc) strcmp);

				/* Don't free location */
				continue;
			}
 		}

		g_free (location);
	}

	g_dir_close (dir);

	return ok;
}

static gchar *
log_get_filename_for_date (GossipLog   *log,
			   const gchar *date)
{
	GossipLogPriv *priv;

	GossipAccount *account;

	const gchar   *contact_id;
	gchar         *contact_id_escaped;
	const gchar   *account_id;
	gchar         *account_id_escaped;
	gchar         *log_directory;
	gchar         *filename;
	gchar         *basename;
	gchar         *dirname;

	priv = GET_PRIV (log);

	account = gossip_contact_get_account (priv->contact);
	if (!account) {
		DEBUG_MSG (("Log: Contact has no account, can not get log filename"));
		return NULL;
	}
	
	if (priv->filename && 
	    priv->date && 
	    strcmp (priv->date, date) == 0) {
		return g_strdup (priv->filename);
	}

	contact_id = gossip_contact_get_id (priv->contact);
	contact_id_escaped = gnome_vfs_escape_host_and_path_string (contact_id);

	account_id = gossip_account_get_id (account);
	account_id_escaped = gnome_vfs_escape_host_and_path_string (account_id);
	
	basename = g_strconcat (contact_id_escaped, "-", date, LOG_FILENAME_SUFFIX, NULL);

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

	DEBUG_MSG (("Log: Using contact log file:'%s' for contact:'%s' on date:'%s'", 
		    filename, contact_id, date));

	return filename;
}

static const gchar *
log_get_filename_for_today (GossipLog *log)
{
	GossipLogPriv *priv;
	gchar         *date_today;

	GossipAccount *account;

	const gchar   *contact_id;
	gchar         *contact_id_escaped;
	const gchar   *account_id;
	gchar         *account_id_escaped;
	gchar         *log_directory;
	gchar         *filename;
	gchar         *basename;
	gchar         *dirname;

	priv = GET_PRIV (log);

	account = gossip_contact_get_account (priv->contact);
	if (!account) {
		DEBUG_MSG (("Log: Contact has no account, can not get log filename"));
		return NULL;
	}
	
	date_today = log_get_timestamp_filename ();
	if (priv->filename && 
	    priv->date && 
	    strcmp (priv->date, date_today) == 0) {
		g_free (date_today);
		return priv->filename;
	}

	DEBUG_MSG (("Log: Generating new filename for today:'%s'", date_today));

	contact_id = gossip_contact_get_id (priv->contact);
	contact_id_escaped = gnome_vfs_escape_host_and_path_string (contact_id);

	account_id = gossip_account_get_id (account);
	account_id_escaped = gnome_vfs_escape_host_and_path_string (account_id);
	
	basename = g_strconcat (contact_id_escaped, "-", date_today, NULL);

	g_free (priv->date);
	priv->date = date_today;

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

	g_free (priv->filename);
	priv->filename = g_strconcat (filename, LOG_FILENAME_SUFFIX, NULL);

	g_free (filename);

	DEBUG_MSG (("Log: Using contact log file:'%s' for contact:'%s'", 
		    priv->filename, contact_id));

	return priv->filename;
}

static GList *
log_get_messages (GossipLog   *log,
		  const gchar *filename)
{
	GossipLogPriv    *priv;
	GossipAccount    *account;
	GList            *messages = NULL;
	gboolean          get_own_name = FALSE;
	gboolean          get_name = FALSE;
	xmlParserCtxtPtr  ctxt;
	xmlDocPtr         doc;
	xmlNodePtr        log_node;
	xmlNodePtr        node;

	DEBUG_MSG (("Log: Attempting to parse filename:'%s'...", filename));

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		DEBUG_MSG (("Log: Filename:'%s' does not exist", filename));
		return NULL;
	}

	priv = GET_PRIV (log);

	/* Get own contact from log contact. */
	account = gossip_contact_get_account (priv->contact);

	/* Create parser. */
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

	if (gossip_contact_get_name (priv->own_contact) == NULL ||
	    strcmp (gossip_contact_get_id (priv->own_contact),
		    gossip_contact_get_name (priv->own_contact)) == 0) {
		get_own_name = TRUE;
	}

	if (gossip_contact_get_name (priv->contact) == NULL ||
	    strcmp (gossip_contact_get_id (priv->contact),
		    gossip_contact_get_name (priv->contact)) == 0) {
		get_name = TRUE;
	}

	/* First get the other contact's details and our name if that
	 * is lacking. 
	 */
	for (node = log_node->children; node && (get_own_name || get_name); node = node->next) {
		gchar *to;
		gchar *from;
		gchar *name;

		if (strcmp (node->name, "message") != 0) {
			continue;
		}

		to = xmlGetProp (node, "to");
		from = xmlGetProp (node, "from");
		name = xmlGetProp (node, "nick");

		if (!name) {
			continue;
		}

		if (to && get_own_name) {
			gossip_contact_set_name (priv->own_contact, name);
			log_set_own_name (account, priv->own_contact);
			get_own_name = FALSE;
		}

		if (from && get_name) {
			gossip_contact_set_name (priv->contact, name);
			log_set_name (account, priv->contact);
			get_name = FALSE;
		}
	}

	/* Now get the messages. */
	for (node = log_node->children; node; node = node->next) {
		GossipMessage *message;
		gchar         *time;
		gossip_time_t  t;
		gchar         *who;
		gchar         *name;
		gchar         *body;
		gboolean       sent = TRUE;

		if (strcmp (node->name, "message") != 0) {
			continue;
		}

		body = xmlNodeGetContent (node);
		
		time = xmlGetProp (node, "time");
		t = gossip_time_from_string_full (time, "%Y%m%dT%H:%M:%S");
		
		name = xmlGetProp (node, "nick");
		
		who = xmlGetProp (node, "to");
		if (!who) {
			sent = FALSE;
			who = xmlGetProp (node, "from");
		}
		
		if (sent) {			
			message = gossip_message_new (GOSSIP_MESSAGE_TYPE_NORMAL, priv->contact);
			gossip_message_set_sender (message, priv->own_contact);
		} else {
			message = gossip_message_new (GOSSIP_MESSAGE_TYPE_NORMAL, priv->own_contact);
			gossip_message_set_sender (message, priv->contact);
		}
		
		gossip_message_set_body (message, body);
		gossip_message_set_timestamp (message, t);
		
		messages = g_list_append (messages, message);
		
		xmlFree (time);
		xmlFree (who);
		xmlFree (name);
		xmlFree (body);
	}
	
 	DEBUG_MSG (("Log: Parsed %d messages", g_list_length (messages))); 

	xmlFreeDoc (doc);
	xmlFreeParserCtxt (ctxt);
	
	return messages;
}

static const gchar *
log_get_contacts_filename (GossipAccount *account)
{
	const gchar *account_id;
	gchar       *account_id_escaped;
	gchar       *log_directory;
	gchar       *filename;

	if (!contact_files) {
		contact_files = g_hash_table_new_full (gossip_account_hash,
						       gossip_account_equal,
						       (GDestroyNotify) g_object_unref,
						       (GDestroyNotify) g_free);
	}

	filename = g_hash_table_lookup (contact_files, account);
	if (filename) {
		return filename;
	}

	DEBUG_MSG (("Log: No contacts file recorded against account id:'%s'...", 
		    gossip_account_get_id (account))); 

	account_id = gossip_account_get_id (account);
	account_id_escaped = gnome_vfs_escape_host_and_path_string (account_id);

	log_check_dir (&log_directory);

	filename = g_build_filename (log_directory,
				     account_id_escaped,
				     LOG_KEY_FILENAME,
				     NULL);

	g_free (log_directory);
	g_free (account_id_escaped);

	DEBUG_MSG (("Log: Saving contacts file:'%s' for account id:'%s'", 
		    filename, account_id)); 
	g_hash_table_insert (contact_files, g_object_ref (account), filename);

	return filename;
}

static GossipContact *
log_get_contact (GossipAccount *account,
		 const gchar   *contact_id)
{
	GossipContact *contact;
	GKeyFile      *key_file;
	const gchar   *filename;
	gchar         *name = NULL;

	filename = log_get_contacts_filename (account);

	key_file = g_key_file_new ();

	g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL);

	name = g_key_file_get_string (key_file, LOG_KEY_GROUP_CONTACTS, contact_id, NULL);
	DEBUG_MSG (("Log: Found name for contact ID:'%s', name:'%s'", contact_id, name)); 

	g_key_file_free (key_file);

	contact = gossip_contact_new_full (GOSSIP_CONTACT_TYPE_TEMPORARY, 
					   account,
					   contact_id,
					   name);

	g_free (name);

	return contact;
}

static gchar *
log_get_contact_name_from_id (const gchar *account_id,
			      const gchar *contact_id)
{
	GKeyFile *key_file;
	gchar    *log_directory;
	gchar    *account_id_escaped;
	gchar    *filename;
	gchar    *name = NULL;

	account_id_escaped = gnome_vfs_escape_host_and_path_string (account_id);

	log_check_dir (&log_directory);

	filename = g_build_filename (log_directory,
				     account_id_escaped,
				     LOG_KEY_FILENAME,
				     NULL);

	g_free (log_directory);
	g_free (account_id_escaped);

	key_file = g_key_file_new ();

	g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL);

	name = g_key_file_get_string (key_file, LOG_KEY_GROUP_CONTACTS, contact_id, NULL);
	DEBUG_MSG (("Log: Found name for contact ID:'%s', name:'%s'", contact_id, name)); 

	g_key_file_free (key_file);

	return name;
}

static gboolean 
log_set_name (GossipAccount *account,
	      GossipContact *contact)
{
	GKeyFile    *key_file;
	const gchar *filename;
	gchar       *content;
	gsize        length;
	gboolean     ok = TRUE;

	filename = log_get_contacts_filename (account);

	key_file = g_key_file_new ();

	g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL);

	g_key_file_set_string (key_file, 
			       LOG_KEY_GROUP_CONTACTS, 
			       gossip_contact_get_id (contact), 
			       gossip_contact_get_name (contact));

	content = g_key_file_to_data (key_file, &length, NULL);
	if (content) {
		GError *error = NULL;

		DEBUG_MSG (("Log: The contacts key file:'%s' has been saved with %" G_GSIZE_FORMAT " bytes of data",
			    filename, length));

		ok = g_file_set_contents (filename, content, length, &error);
		g_free (content);

		if (error) {
			DEBUG_MSG (("Log: Could not save file:'%s' with %" G_GSIZE_FORMAT " bytes of data, error:%d->'%s'",
				    filename, length, error->code, error->message));
			g_error_free (error);
		}
	} else {
		DEBUG_MSG (("Log: Could not get content to save from GKeyFile"));
		ok = FALSE;
	}

	g_key_file_free (key_file);

	return ok;
}

static GossipContact *
log_get_own_contact (GossipAccount *account)
{
	GossipContact *contact;
	GKeyFile      *key_file;
	const gchar   *filename;
	const gchar   *id;
	gchar         *name;

	filename = log_get_contacts_filename (account);

	key_file = g_key_file_new ();

	g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL);

	id = gossip_account_get_id (account);
	name = g_key_file_get_string (key_file, LOG_KEY_GROUP_SELF, LOG_KEY_VALUE_NAME, NULL);

	g_key_file_free (key_file);

	if (!name) {
		DEBUG_MSG (("Log: No name in contacts file, using id for name.")); 
		name = g_strdup (id);
	}

	DEBUG_MSG (("Log: Own contact ID:'%s', name:'%s'", id, name)); 
	contact = gossip_contact_new_full (GOSSIP_CONTACT_TYPE_TEMPORARY, 
					   account, id, name);
	
	g_free (name);

	return contact;
}

static gboolean
log_set_own_name (GossipAccount *account, 
		  GossipContact *contact)
{
	GKeyFile    *key_file;
	const gchar *filename;
	gchar       *content;
	gsize        length;
	gboolean     ok = TRUE;

	filename = log_get_contacts_filename (account);

	key_file = g_key_file_new ();

	g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL);

	g_key_file_set_string (key_file, 
			       LOG_KEY_GROUP_SELF, 
			       LOG_KEY_VALUE_NAME, 
			       gossip_contact_get_name (contact));

	content = g_key_file_to_data (key_file, &length, NULL);
	if (content) {
		GError *error = NULL;

		DEBUG_MSG (("Log: The contacts key file:'%s' has been saved with %" G_GSIZE_FORMAT " bytes of data",
			    filename, length));

		ok = g_file_set_contents (filename, content, length, &error);
		g_free (content);

		if (error) {
			DEBUG_MSG (("Log: Could not save file:'%s' with %" G_GSIZE_FORMAT " bytes of data, error:%d->'%s'",
				    filename, length, error->code, error->message));
			g_error_free (error);
		}
	} else {
		DEBUG_MSG (("Log: Could not get content to save from GKeyFile"));
		ok = FALSE;
	}

	g_key_file_free (key_file);

	return ok;
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

	num_matches = gossip_utils_regex_match (GOSSIP_REGEX_ALL, 
						msg, start, end);

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
log_show (GossipLog *log)
{
	GossipLogPriv  *priv;
#if 0
	GnomeVFSResult  result;
#endif
	gchar          *filename = NULL;
	gchar          *outfile;
	gchar          *url;
	FILE           *file;
	gint            fd;

	priv = GET_PRIV (log);

	if (!g_file_test (priv->filename, G_FILE_TEST_EXISTS)) {
		file = g_fopen (priv->filename, "w+");
		if (file) {
			g_fprintf (file, LOG_HEADER LOG_FOOTER);
			fclose (file);
		}
	}

	if (!g_file_test (priv->filename, G_FILE_TEST_EXISTS)) {
		return;
	}

	fd = g_file_open_tmp ("gossip-log-XXXXXX", &outfile, NULL);
	if (fd == -1) {
		return;
	}

	DEBUG_MSG (("Log: Using temporary file:'%s'", outfile));

	if (!log_transform (priv->filename, fd)) {
		g_warning ("Couldn't transform log file:'%s'", outfile);
		g_free (outfile);
		return;
	}

	close (fd);

	filename = g_strconcat (outfile, ".html", NULL);
	if (rename (outfile, filename) == -1) {
		g_warning ("Couldn't rename temporary log file:'%s' to '%s'", 
			   outfile, filename);
		g_free (outfile);
		g_free (filename);
		return;
	}

	g_free (outfile);

	url = g_strconcat (LOG_FILENAME_PREFIX, filename, NULL);

#ifdef USE_GNOMEVFS_FOR_URL
	result = gnome_vfs_url_show (url);
	if (result == GNOME_VFS_OK) {
		g_warning ("Couldn't show log file:'%s'", filename);
	}
#else
	gnome_url_show (url, NULL);
#endif

	g_free (url);
	g_free (filename);
}

GossipLog *
gossip_log_get_by_contact_id (GossipAccount *account,
			      const gchar   *contact_id)
{
	GossipLog     *log;
	GossipContact *contact;

	contact = log_get_contact (account, contact_id);
	log = gossip_log_get (contact);
	g_object_unref (contact);
	
	return log;
}

GossipLog *
gossip_log_get (GossipContact *contact)
{
	GossipLog     *log;
	GossipLogPriv *priv;
	GossipAccount *account;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);
	
	if (!logs) {
		logs = g_hash_table_new_full (gossip_contact_hash,
					      gossip_contact_equal,
					      (GDestroyNotify) g_object_unref,
					      (GDestroyNotify) g_object_unref);
	}

	log = g_hash_table_lookup (logs, contact);
	if (log) {
		return log;
	}

	account = gossip_contact_get_account (contact);

	/* Create new log. */
	log = g_object_new (GOSSIP_TYPE_LOG, NULL);
	
	priv = GET_PRIV (log);

	priv->contact = g_object_ref (contact);

	/* Note: We get this AFTER priv->contact because it is used to
	 * get the own contact details.
	 */
	priv->own_contact = log_get_own_contact (account);

	/* Note: priv->timestamp and priv->filename is filled in for
	 * us by log_get_filename(). 
	 */
	log_get_filename_for_today (log);

	return log;
}

GossipContact *
gossip_log_get_own_contact (GossipLog *log)
{
	GossipLogPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_LOG (log), NULL);

	priv = GET_PRIV (log);

	return priv->own_contact;
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
	GList         *l;

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

	while ((filename = g_dir_read_name (dir)) != NULL) {
		gboolean duplicate = FALSE;

		if (!g_str_has_suffix (filename, LOG_FILENAME_SUFFIX)) {
			continue;
		}

		filename_unescaped = gnome_vfs_unescape_string_for_display (filename);
		DEBUG_MSG (("Log: Using log file:'%s' to create contact", filename_unescaped));
		
		contact_id = log_get_contact_id_from_filename (filename_unescaped);
		g_free (filename_unescaped);

		contact = log_get_contact (account, contact_id);
		g_free (contact_id);

		for (l = contacts; l && !duplicate; l = l->next) {
			if (gossip_contact_equal (l->data, contact)) {
				duplicate = TRUE;
			}
		}

		/* Avoid duplicates */
		if (!duplicate) {
			contacts = g_list_append (contacts, contact);
		} else {
			g_object_unref (contact);
		}
	}
	
	g_free (directory);
	g_dir_close (dir);
	
	return contacts;
}

GList *  
gossip_log_get_messages (GossipLog   *log,
			 const gchar *date)
{
	gchar *filename;
	GList *messages;

 	g_return_val_if_fail (GOSSIP_IS_LOG (log), NULL);

	filename = log_get_filename_for_date (log, date);
	messages = log_get_messages (log, filename); 
	g_free (filename);

	return messages;
}

GList *
gossip_log_get_dates (GossipLog *log)
{
	GossipLogPriv *priv;
	GossipAccount *account;
	GList         *dates = NULL;
	gchar         *date;
	gchar         *log_directory;
	gchar         *directory;
	const gchar   *account_id;
	gchar         *account_id_escaped;
	GDir          *dir;
	const gchar   *filename;
	gchar         *filename_unescaped;
	gchar         *contact_id;

	g_return_val_if_fail (GOSSIP_IS_LOG (log), NULL);

	priv = GET_PRIV (log);

	/* Get own contact from log contact. */
	account = gossip_contact_get_account (priv->contact);
	
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

	while ((filename = g_dir_read_name (dir)) != NULL) {
		if (!g_str_has_suffix (filename, LOG_FILENAME_SUFFIX)) {
			continue;
		}

		filename_unescaped = gnome_vfs_unescape_string_for_display (filename);
		DEBUG_MSG (("Log: Using log file:'%s' to create contact", filename_unescaped));
		
		contact_id = log_get_contact_id_from_filename (filename_unescaped);
		if (strcmp (contact_id, gossip_contact_get_id (priv->contact)) != 0) {
			g_free (contact_id);
			g_free (filename_unescaped);
			continue;
		}

		date = log_get_date_from_filename (filename_unescaped);
		DEBUG_MSG (("Log: Using date:'%s' for:'%s'", 
			    date, contact_id));

		dates = g_list_insert_sorted (dates, date, (GCompareFunc) strcmp);

		g_free (contact_id);
		g_free (filename_unescaped);
	}
	
	g_free (directory);
	g_dir_close (dir);

 	DEBUG_MSG (("Log: Parsed %d dates", g_list_length (dates))); 

	return dates;
}

gchar *
gossip_log_get_date_readable (const gchar *date)
{
	gossip_time_t t;
	
	t = gossip_time_from_string_full (date, LOG_TIME_FORMAT);
	return gossip_time_to_timestamp_full (t, "%a %d %b %Y");
}


void
gossip_log_message (GossipLog     *log,
		    GossipMessage *message, 
		    gboolean       incoming)
{
	GossipLogPriv *priv;
	GossipContact *contact;
	GossipContact *own_contact;
        const gchar   *filename;
	FILE          *file;
	gchar         *body;
	const gchar   *to_or_from;
	gchar         *timestamp;
	const gchar   *name;
	const gchar   *resource;
	gboolean       save_contact = FALSE;
	gboolean       save_own_contact = FALSE;

	g_return_if_fail (GOSSIP_IS_LOG (log));
	g_return_if_fail (GOSSIP_IS_MESSAGE (message));

	priv = GET_PRIV (log);

	if (incoming) {
		to_or_from = "from";

		contact = gossip_message_get_sender (message);
		own_contact = gossip_message_get_recipient (message);
	} else {
		to_or_from = "to";
		contact = gossip_message_get_recipient (message);
		own_contact = gossip_message_get_sender (message);
	}

	name = gossip_contact_get_name (contact);

	filename = log_get_filename_for_today (log);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		file = g_fopen (filename, "w+");
		if (file) {
			g_fprintf (file, LOG_HEADER);
		}

		/* ONLY save the name when we create new files, we are
		 * more efficient this way. 
		 */
		
		save_contact = TRUE;
		save_own_contact = TRUE;
	} else {
		const gchar *own_id;
		const gchar *own_name;

		const gchar *str;
				
		file = g_fopen (filename, "r+");
		if (file) {
			fseek (file, - strlen (LOG_FOOTER), SEEK_END);
		}

		/* Check the message name and our name match, if not
		 * we save the message name since ours is out of
		 * sync. 
		 */
		own_id = gossip_contact_get_id (priv->own_contact);
		own_name = gossip_contact_get_name (priv->own_contact);
		str = gossip_contact_get_name (own_contact);

		if (!own_name || 
		    strcmp (own_name, str) != 0 ||
		    strcmp (own_name, own_id) == 0) {
			gossip_contact_set_name (priv->own_contact, str);
			save_own_contact = TRUE;
		}

	}

	if (save_own_contact) {
		GossipAccount *account;

		account = gossip_contact_get_account (contact);
		log_set_own_name (account, priv->own_contact);
	}

	if (save_contact) {
		GossipAccount *account;

		account = gossip_contact_get_account (contact);
		log_set_name (account, contact);
	}

	if (!file) {
		return;
	}

	timestamp = log_get_timestamp_from_message (message);

        if (gossip_message_get_body (message)) {
		body = log_urlify (gossip_message_get_body (message));
	} else {
		body = g_strdup ("");
	}

	resource = gossip_message_get_explicit_resource (message);
	if (!resource) {
		resource = "";
	}

	g_fprintf (file,
		   "<message time='%s' %s='%s' resource='%s' nick='%s'>"
		   "%s"
		   "</message>\n"
		   LOG_FOOTER,
		   timestamp, 
		   to_or_from,
		   gossip_contact_get_id (contact),
		   resource,
		   name,
		   body);
	
        fclose (file);

	g_chmod (filename, LOG_CREATE_MODE);

	g_free (timestamp);
	g_free (body);

	/* Now signal new message */
	g_signal_emit (log, signals[NEW_MESSAGE], 0, message);
}

gboolean
gossip_log_exists (GossipLog *log)
{
	GList    *dates;
	gboolean  exists;

	g_return_val_if_fail (GOSSIP_IS_LOG (log), FALSE);

	dates = gossip_log_get_dates (log);
	exists = g_list_length > 0;

	g_list_foreach (dates, (GFunc) g_free, NULL);
	g_list_free (dates);

	return exists;
}

void
gossip_log_show (GtkWidget     *window, 
		 GossipContact *contact)
{
	GdkCursor *cursor;
	GossipLog *log;

	cursor = gdk_cursor_new (GDK_WATCH);

	gdk_window_set_cursor (window->window, cursor);
	gdk_cursor_unref (cursor);

	/* Let the UI redraw before we start the slow transformation. */
	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}

	log = gossip_log_get (contact);
	
	log_show (log);

	gdk_window_set_cursor (window->window, NULL);
}

GList *
gossip_log_search_new (const gchar *text)
{
	GList       *files;
	GList       *l;
	const gchar *filename;
	gchar       *text_casefold;
	gchar       *contents;
	gchar       *contents_casefold;
	GList       *hits = NULL;

	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (strlen (text) > 0, NULL);

	text_casefold = g_utf8_casefold (text, -1);

	if (log_get_all_log_files (NULL, &files)) {
		DEBUG_MSG (("Log: Found %d log files in total", g_list_length (files)));
	} else {
		DEBUG_MSG (("Log: Failed to retrieve all log files"));
	}

	for (l = files; l; l = l->next) {
		filename = l->data;

		if (!g_file_get_contents (filename, &contents, NULL, NULL)) {
			continue;
		}
		
		if (!contents || strlen (contents) < 1) {
			continue;
		}
		
		contents_casefold = g_utf8_casefold (contents, -1);
		g_free (contents);

		if (strstr (contents_casefold, text_casefold)) {
			GossipLogSearchHit *hit;
			
			hit = g_new0 (GossipLogSearchHit, 1);

			hit->account_id = log_get_account_id_from_filename (filename);
			hit->contact_id = log_get_contact_id_from_filename (filename);

			hit->contact_name = log_get_contact_name_from_id (hit->account_id,
									  hit->contact_id);

			hit->date = log_get_date_from_filename (filename);

			hit->filename = g_strdup (filename);

			hits = g_list_append (hits, hit);

			DEBUG_MSG (("Log: Found text:'%s' in file:'%s' on date:'%s'...", 
				    text, hit->filename, hit->date));
		}

		g_free (contents_casefold);
	}

	g_list_foreach (files, (GFunc) g_free, NULL);
	g_list_free (files);
	
	g_free (text_casefold);

	return hits;
}

void
gossip_log_search_free (GList *hits)
{
	GList              *l;
	GossipLogSearchHit *hit;

	g_return_if_fail (hits != NULL);
	
	for (l = hits; l; l = l->next) {
		hit = l->data;

		g_free (hit->account_id);
		g_free (hit->contact_id);
		g_free (hit->contact_name);
		g_free (hit->date);
		g_free (hit->filename);

		g_free (hit);
	}
	
	g_list_free (hits);
}

const gchar *
gossip_log_search_hit_get_account_id (GossipLogSearchHit *hit)
{
	g_return_val_if_fail (hit != NULL, NULL);
	
	return hit->account_id;
}

const gchar *
gossip_log_search_hit_get_contact_id (GossipLogSearchHit *hit)
{
	g_return_val_if_fail (hit != NULL, NULL);
	
	return hit->contact_id;
}

const gchar *
gossip_log_search_hit_get_contact_name (GossipLogSearchHit *hit)
{
	g_return_val_if_fail (hit != NULL, NULL);
	
	return hit->contact_name;
}

const gchar *
gossip_log_search_hit_get_date (GossipLogSearchHit *hit)
{
	g_return_val_if_fail (hit != NULL, NULL);
	
	return hit->date;
}

const gchar *
gossip_log_search_hit_get_filename (GossipLogSearchHit *hit)
{
	g_return_val_if_fail (hit != NULL, NULL);
	
	return hit->filename;
}
