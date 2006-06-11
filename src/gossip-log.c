/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2006 Imendio AB
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
 *   ~/.gnome2/Gossip/logs/<account>/<contact>/<date>.log
 *
 * Where <date> is "20060102" and represents a day. So each day as new
 * log is created for each contact you talk with. 
 *
 * In addition to all this, there is also a file in each account
 * directory called "contacts.ini" which holds a contact ID to
 * nickname translation for all contacts. This is updated daily when a
 * new file is created.
 * 
 * For group chat, files are stored as:
 *   ~/.gnome2/Gossip/logs/<account>/chatrooms/<room@server>/<date>.log
 * 
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

#define DEBUG_MSG(x)
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n"); */

#define LOG_HEADER \
    "<?xml version='1.0' encoding='utf-8'?>\n" \
    "<?xml-stylesheet type=\"text/xsl\" href=\"gossip-log.xsl\"?>\n" \
    "<log>\n"

#define LOG_FOOTER \
    "</log>\n"

#define LOG_FILENAME_PREFIX    "file://"
#define LOG_FILENAME_SUFFIX    ".log"

#define LOG_DTD_FILENAME       "gossip-log.dtd"

#define LOG_DIR_CREATE_MODE    (S_IRUSR | S_IWUSR | S_IXUSR)
#define LOG_FILE_CREATE_MODE   (S_IRUSR | S_IWUSR)

#define LOG_DIR_CHATROOMS      "chatrooms"

#define LOG_KEY_FILENAME       "contacts.ini"
#define LOG_KEY_GROUP_SELF     "Self"
#define LOG_KEY_GROUP_CONTACTS "Contacts"

#define LOG_KEY_VALUE_NAME     "Name"

#define LOG_TIME_FORMAT_FULL   "%Y%m%dT%H:%M:%S"
#define LOG_TIME_FORMAT        "%Y%m%d"

typedef struct {
	GossipContact        *contact;
	GossipChatroom       *chatroom;
	GossipLogMessageFunc  func;
	gpointer              user_data;
} HandlerData;

struct _GossipLogSearchHit {
	GossipAccount *account;
	GossipContact *contact;
	gchar         *filename;
	gchar         *date;
};

static void            log_handlers_notify_foreach             (GossipLogMessageFunc   func,
								HandlerData           *data,
								GList                **list);
static void            log_handlers_notify_all                 (GossipContact   *own_contact,
								GossipContact   *contact,
								GossipChatroom  *chatroom,
								GossipMessage   *message);
static gboolean        log_check_dir                           (gchar          **directory);
static gchar *         log_get_timestamp_from_message          (GossipMessage   *msg);
static gchar *         log_get_timestamp_filename              (void);
static gchar *         log_get_contact_id_from_filename        (const gchar     *filename);
static GossipChatroom *log_get_chatroom_from_filename          (GossipAccount   *account,
								const gchar     *filename);
static void            log_get_all_log_files_for_chatroom_dir  (const gchar     *chatrooms_dir,
								GList          **files);
static void            log_get_all_log_files_for_chatrooms_dir (const gchar     *chatrooms_dir,
								GList          **files);
static void            log_get_all_log_files_for_contact_dir   (const gchar     *contact_dir,
								GList          **files);
static void            log_get_all_log_files_for_account_dir   (const gchar     *account_dir,
								GList          **files);
static gchar *         log_get_account_id_from_filename        (const gchar     *filename);
static gchar *         log_get_date_from_filename              (const gchar     *filename);
static gchar *         log_get_filename_by_date_for_contact    (GossipContact   *contact,
								const gchar     *particular_date);
static gchar *         log_get_filename_by_date_for_chatroom   (GossipChatroom  *chatroom,
								const gchar     *particular_date);
static const gchar *   log_get_contacts_filename               (GossipAccount   *account);
static GossipContact * log_get_contact                         (GossipAccount   *account,
								const gchar     *contact_id);
static gboolean        log_set_name                            (GossipAccount   *account,
								GossipContact   *contact);
static gboolean        log_set_own_name                        (GossipAccount   *account,
								GossipContact   *contact);
static gchar *         log_urlify                              (const gchar     *msg);


#ifdef DEPRECATED 
static gboolean        log_transform                           (const gchar     *infile,
							        gint             fd);
static void            log_show                                (GossipLog       *log);
#endif

static GHashTable *contact_files = NULL;
static GHashTable *message_handlers = NULL;

static void
log_handler_free (HandlerData *data)
{
	if (data->contact) {
		g_object_unref (data->contact);
	}

	if (data->chatroom) {
		g_object_unref (data->chatroom);
	}

	g_free (data);
}

void
gossip_log_handler_add_for_contact (GossipContact        *contact,
				    GossipLogMessageFunc  func,
				    gpointer              user_data)
{
	HandlerData *data;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (func != NULL);

	if (!message_handlers) {
		message_handlers = g_hash_table_new_full (g_direct_hash,
							  g_direct_equal,
							  NULL,
							  (GDestroyNotify) log_handler_free);
	}

	data = g_new0 (HandlerData, 1);
	
	data->contact = g_object_ref (contact);

	data->func = func;
	data->user_data = user_data;
	
	g_hash_table_insert (message_handlers, func, data);
}

void
gossip_log_handler_add_for_chatroom (GossipChatroom       *chatroom,
				     GossipLogMessageFunc  func,
				     gpointer              user_data)
{
	HandlerData *data;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (func != NULL);

	if (!message_handlers) {
		message_handlers = g_hash_table_new_full (g_direct_hash,
							  g_direct_equal,
							  NULL,
							  (GDestroyNotify) g_free);
	}

	data = g_new0 (HandlerData, 1);
	
	data->chatroom = g_object_ref (chatroom);

	data->func = func;
	data->user_data = user_data;
	
	g_hash_table_insert (message_handlers, func, data);
}

void
gossip_log_handler_remove (GossipLogMessageFunc func)
{
	if (!message_handlers) {
		return;
	}

	g_hash_table_remove (message_handlers, func);
}

static void
log_handlers_notify_foreach (GossipLogMessageFunc   func,
			     HandlerData           *data,
			     GList                **list)
{
	if (data && data->func == func) {
		*list = g_list_append (*list, data);
	}
}

static void
log_handlers_notify_all (GossipContact  *own_contact,
			 GossipContact  *contact,
			 GossipChatroom *chatroom,
			 GossipMessage  *message)
{
	GossipLogMessageFunc  func;
	GList                *handlers = NULL;
	GList                *l;
	HandlerData          *data;

	if (!message_handlers) {
		return;
	}

	g_hash_table_foreach (message_handlers, 
			      (GHFunc) log_handlers_notify_foreach,
			      &handlers);

	for (l = handlers; l; l = l->next) {
		data = l->data;

		if (!data || !data->func) {
			continue;
		}

		if (data->contact) {
			if (!contact || 
			    !gossip_contact_equal (data->contact, contact)) {
				continue;
			}
		}

		if (data->chatroom) {
			if (!chatroom ||
			    !gossip_chatroom_equal_full (data->chatroom, chatroom)) {
				continue;
			}
		}

		func = data->func;
		(func)(own_contact, message, data->user_data);
	}
	
	g_list_free (handlers);
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
		g_mkdir_with_parents (dir, LOG_DIR_CREATE_MODE);
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
	gchar  *contact_id;
	gchar **strv;
	gint    i = 0;

	if (!g_str_has_suffix (filename, LOG_FILENAME_SUFFIX)) {
		return NULL;
	}

	strv = g_strsplit (filename, G_DIR_SEPARATOR_S, -1);
	while (strv[i] != NULL) {
		i++;
	} 

	contact_id = g_strdup (strv[i - 2]);
	g_strfreev (strv);

	return contact_id;
}

static gchar *
log_get_account_id_from_filename (const gchar *filename)
{
	gchar       *log_directory;
	const gchar *p1;
	const gchar *p2;

	log_check_dir (&log_directory);
	
	p1 = strstr (filename, log_directory);
	if (!p1) {
		g_free (log_directory);
		return NULL;
	}

	p1 += strlen (log_directory) + 1;
	
	p2 = strstr (p1, G_DIR_SEPARATOR_S);
	if (!p2) {
		g_free (log_directory);
		return NULL;
	}

	return g_strndup (p1, p2 - p1); 
}

static GossipChatroom *
log_get_chatroom_from_filename (GossipAccount *account,
				const gchar   *filename) 
{
	GossipChatroomManager *manager;
	GossipChatroom        *chatroom;
	GList                 *found;
	gchar                 *server;
	gchar                 *room;

	room = g_strdup (filename);

	server = strstr (room, "@");
	if (!server) {
		g_free (room);
		return NULL;
	}

	server[0] = '\0';
	server++;

	manager = gossip_app_get_chatroom_manager ();
	found = gossip_chatroom_manager_find_extended (manager, account, server, room);

	if (!found) {
		chatroom = g_object_new (GOSSIP_TYPE_CHATROOM, 
					 "type", GOSSIP_CHATROOM_TYPE_NORMAL,
					 "account", account,
					 "name", room,
					 "server", server,
					 "room", room,
					 NULL);
	} else {
		/* FIXME: What do we do if there are more than
		 * 1 chatrooms found.
		 */
		chatroom = found->data;
	}
	
	g_free (room);

	return chatroom;
}

static gchar *
log_get_date_from_filename (const gchar *filename)
{
	const gchar *start;
	const gchar *end;

	start = g_strrstr (filename, G_DIR_SEPARATOR_S);
	if (!start) {
		return NULL;
	}

	start++;

	end = strstr (start, LOG_FILENAME_SUFFIX);
	if (!end) {
		return NULL;
	}

	return g_strndup (start, end - start);
}

static void
log_get_all_log_files_for_chatroom_dir (const gchar  *chatroom_dir,
					 GList       **files)
{
	GDir        *dir;
	const gchar *name;
	gchar       *path;

	dir = g_dir_open (chatroom_dir, 0, NULL);
	if (!dir) {
		DEBUG_MSG (("Log: Could not open directory:'%s'", chatroom_dir));
		return;
	}
	
	while ((name = g_dir_read_name (dir)) != NULL) {
		if (!g_str_has_suffix (name, LOG_FILENAME_SUFFIX)) {
			continue;
		}
			
		path = g_build_filename (chatroom_dir, name, NULL);

		if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
			*files = g_list_insert_sorted (*files, 
						       path, 
						       (GCompareFunc) strcmp);
			
			/* Don't free path. */
			continue;
		}
		
		g_free (path);
	}

	g_dir_close (dir);
}

static void
log_get_all_log_files_for_chatrooms_dir (const gchar  *chatrooms_dir,
					 GList       **files)
{
	GDir        *dir;
	const gchar *name;
	gchar       *path;

	dir = g_dir_open (chatrooms_dir, 0, NULL);
	if (!dir) {
		DEBUG_MSG (("Log: Could not open directory:'%s'", chatrooms_dir));
		return;
	}
	
	while ((name = g_dir_read_name (dir)) != NULL) {
		path = g_build_filename (chatrooms_dir, name, NULL);
		log_get_all_log_files_for_chatroom_dir (path, files);
		g_free (path);
	}

	g_dir_close (dir);
}

static void
log_get_all_log_files_for_contact_dir (const gchar  *contact_dir,
				       GList       **files)
{
	GDir        *dir;
	const gchar *name;
	gchar       *path;

	dir = g_dir_open (contact_dir, 0, NULL);
	if (!dir) {
		DEBUG_MSG (("Log: Could not open directory:'%s'", contact_dir));
		return;
	}
	
	while ((name = g_dir_read_name (dir)) != NULL) {
		if (!g_str_has_suffix (name, LOG_FILENAME_SUFFIX)) {
			continue;
		}
			
		path = g_build_filename (contact_dir, name, NULL);

		if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
			*files = g_list_insert_sorted (*files, 
						       path, 
						       (GCompareFunc) strcmp);
			
			/* Don't free path. */
			continue;
		}
		
		g_free (path);
	}

	g_dir_close (dir);
}

static void
log_get_all_log_files_for_account_dir (const gchar  *account_dir,
				       GList       **files)
{
	GDir        *dir;
	const gchar *name;
	gchar       *path;

	dir = g_dir_open (account_dir, 0, NULL);
	if (!dir) {
		DEBUG_MSG (("Log: Could not open directory:'%s'", account_dir));
		return;
	}
	
	while ((name = g_dir_read_name (dir)) != NULL) {
		path = g_build_filename (account_dir, name, NULL);

		if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
			g_free (path);
			continue;
		}

		if (strcmp (name, LOG_DIR_CHATROOMS) == 0) {
			log_get_all_log_files_for_chatrooms_dir (path, files);
		} else {
			log_get_all_log_files_for_contact_dir (path, files);
		}

		g_free (path);
	}

	g_dir_close (dir);
}

static gboolean
log_get_all_log_files (GList **files)
{
	gchar       *log_directory;
	GDir        *dir;
	const gchar *name;
	gchar       *account_dir;

	*files = NULL;

	if (log_check_dir (&log_directory)) {
		DEBUG_MSG (("Log: No log directory exists"));
		return TRUE;
	}
	
	dir = g_dir_open (log_directory, 0, NULL);
	if (!dir) {
		DEBUG_MSG (("Log: Could not open directory:'%s'", log_directory));
		return FALSE;
	}
	
	while ((name = g_dir_read_name (dir)) != NULL) {
		account_dir = g_build_filename (log_directory, name, NULL);
		log_get_all_log_files_for_account_dir (account_dir, files);
		g_free (account_dir);
	}
	
	g_dir_close (dir);

	return TRUE;
}

static gchar *
log_get_filename_by_date_for_contact (GossipContact *contact,
				      const gchar   *particular_date)
{
	GossipAccount *account;
	const gchar   *account_id;
	gchar         *account_id_escaped;
	const gchar   *contact_id;
	gchar         *contact_id_escaped;
	gchar         *log_directory;
	gchar         *filename;
	gchar         *dirname;
	gchar         *basename;
	gchar         *todays_date = NULL;
	const gchar   *date;

	if (!particular_date) {
		date = todays_date = log_get_timestamp_filename ();
	} else {
		date = particular_date;
	}

	account = gossip_contact_get_account (contact);
	if (!account) {
		DEBUG_MSG (("Log: Contact has no account, can not get log filename"));
		return NULL;
	}
	
	account_id = gossip_account_get_id (account);
	account_id_escaped = gnome_vfs_escape_host_and_path_string (account_id);

	log_check_dir (&log_directory);

	contact_id = gossip_contact_get_id (contact);
	contact_id_escaped = gnome_vfs_escape_host_and_path_string (contact_id);
	
	basename = g_strconcat (date, LOG_FILENAME_SUFFIX, NULL);
	filename = g_build_filename (log_directory,
				     account_id_escaped,
				     contact_id_escaped,
				     basename,
				     NULL);
	
	g_free (basename);
	g_free (contact_id_escaped);
	
	DEBUG_MSG (("Log: Using file:'%s' for contact:'%s'", 
		    filename, 
		    gossip_contact_get_id (contact)));
	
	dirname = g_path_get_dirname (filename);
	if (!g_file_test (dirname, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		DEBUG_MSG (("Log: Creating directory:'%s'", dirname));
		g_mkdir_with_parents (dirname, LOG_DIR_CREATE_MODE);
	}

	g_free (dirname);

	g_free (log_directory);
	g_free (account_id_escaped);

	g_free (todays_date);

	return filename;
}

static gchar *
log_get_filename_by_date_for_chatroom (GossipChatroom *chatroom,
				       const gchar    *particular_date)
{
	GossipAccount *account;
	const gchar   *account_id;
	gchar         *account_id_escaped;
	const gchar   *chatroom_id;
	gchar         *chatroom_id_escaped;
	gchar         *log_directory;
	gchar         *filename;
	gchar         *dirname;
	gchar         *basename;
	gchar         *todays_date = NULL;
	const gchar   *date;

	if (!particular_date) {
		date = todays_date = log_get_timestamp_filename ();
	} else {
		date = particular_date;
	}

	account = gossip_chatroom_get_account (chatroom);
	if (!account) {
		DEBUG_MSG (("Log: Contact has no account, can not get log filename"));
		return NULL;
	}
	
	account_id = gossip_account_get_id (account);
	account_id_escaped = gnome_vfs_escape_host_and_path_string (account_id);

	log_check_dir (&log_directory);

	chatroom_id = gossip_chatroom_get_id_str (chatroom);
	chatroom_id_escaped = gnome_vfs_escape_host_and_path_string (chatroom_id);
	
	basename = g_strconcat (date, LOG_FILENAME_SUFFIX, NULL);
	filename = g_build_filename (log_directory,
				     account_id_escaped,
				     LOG_DIR_CHATROOMS,
				     chatroom_id_escaped,
				     basename,
				     NULL);
	
	g_free (basename);
	g_free (chatroom_id_escaped);
	
	DEBUG_MSG (("Log: Using file:'%s' for chatroom:'%s'", 
		    filename, 
		    gossip_chatroom_get_id_str (chatroom)));

	dirname = g_path_get_dirname (filename);
	if (!g_file_test (dirname, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		DEBUG_MSG (("Log: Creating directory:'%s'", dirname));
		g_mkdir_with_parents (dirname, LOG_DIR_CREATE_MODE);
	}

	g_free (dirname);

	g_free (log_directory);
	g_free (account_id_escaped);

	g_free (todays_date);

	return filename;
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
	GossipContact *contact = NULL;
	GKeyFile      *key_file;
	const gchar   *filename;
	gchar         *name = NULL;

	filename = log_get_contacts_filename (account);

	key_file = g_key_file_new ();

	if (g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL)) {
		name = g_key_file_get_string (key_file, LOG_KEY_GROUP_CONTACTS, contact_id, NULL);
	}

	if (!name) {
		DEBUG_MSG (("Log: No name found for contact ID:'%s'", contact_id));
	}
	
	g_key_file_free (key_file);

	if (name) {
		DEBUG_MSG (("Log: Using name:'%s' for contact ID:'%s'", name, contact_id));
		contact = gossip_contact_new_full (GOSSIP_CONTACT_TYPE_TEMPORARY, 
						   account,
						   contact_id,
						   name);

		g_free (name);
	}

	return contact;
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

		DEBUG_MSG (("Log: The contacts key file:'%s' has been saved with %"
			    G_GSIZE_FORMAT " bytes of data",
			    filename, length));

		ok = g_file_set_contents (filename, content, length, &error);
		g_free (content);

		if (error) {
			DEBUG_MSG (("Log: Could not save file:'%s' with %"
				    G_GSIZE_FORMAT " bytes of data, error:%d->'%s'",
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

GossipContact *
gossip_log_get_own_contact (GossipAccount *account)
{
	GossipContact *contact;
	GKeyFile      *key_file;
	const gchar   *filename;
	const gchar   *id;
	gchar         *name = NULL;

	filename = log_get_contacts_filename (account);

	key_file = g_key_file_new ();

	if (g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL)) {
		name = g_key_file_get_string (key_file, LOG_KEY_GROUP_SELF, LOG_KEY_VALUE_NAME, NULL);
	}

	g_key_file_free (key_file);
	
	id = gossip_account_get_id (account);

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

		DEBUG_MSG (("Log: The contacts key file:'%s' has been saved with %" 
			    G_GSIZE_FORMAT " bytes of data",
			    filename, length));

		ok = g_file_set_contents (filename, content, length, &error);
		g_free (content);

		if (error) {
			DEBUG_MSG (("Log: Could not save file:'%s' with %"
				    G_GSIZE_FORMAT " bytes of data, error:%d->'%s'",
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

	num_matches = gossip_regex_match (GOSSIP_REGEX_ALL, msg, start, end);

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
				tmp = gossip_substring (msg, last, s);
				esc = g_markup_escape_text (tmp, -1);
				g_string_append (ret, esc);
				g_free (tmp);
				g_free (esc);
			}

			tmp = gossip_substring (msg, s, e);

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
			tmp = gossip_substring (msg, e, strlen (msg));
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

#ifdef DEPRECATED

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
#ifdef USE_GNOMEVFS_FOR_URL
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

#endif /* DEPRECATED */

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
	
	dir = g_dir_open (directory, 0, NULL);
	if (!dir) {
		DEBUG_MSG (("Log: Could not open directory:'%s'", directory));
		g_free (directory);
		return NULL;
	}

	DEBUG_MSG (("Log: Collating a list of contacts in:'%s'", directory));

	while ((filename = g_dir_read_name (dir)) != NULL) {
		if (!g_file_test (directory, G_FILE_TEST_IS_DIR)) {
			continue;
		}

		if (strcmp (filename, LOG_DIR_CHATROOMS) == 0) {
			continue;
		}

 		filename_unescaped = gnome_vfs_unescape_string_for_display (filename);
		contact = log_get_contact (account, filename_unescaped);
		g_free (filename_unescaped);

		if (!contact) {
			continue;
		}

		contacts = g_list_append (contacts, contact);
	}
	
	g_free (directory);
	g_dir_close (dir);
	
	return contacts;
}

GList *
gossip_log_get_chatrooms (GossipAccount *account)
{
	GList          *chatrooms = NULL;
	GossipChatroom *chatroom;
	gchar          *log_directory;
	gchar          *directory;
	const gchar    *account_id;
	gchar          *account_id_escaped;
	GDir           *dir;
	const gchar    *filename;
	gchar          *filename_unescaped;

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
				  LOG_DIR_CHATROOMS,
				  NULL);

	g_free (account_id_escaped);
	g_free (log_directory);
	
	dir = g_dir_open (directory, 0, NULL);
	if (!dir) {
		DEBUG_MSG (("Log: Could not open directory:'%s'", directory));
		g_free (directory);
		return NULL;
	}

	DEBUG_MSG (("Log: Collating a list of chatrooms in:'%s'", directory));

	while ((filename = g_dir_read_name (dir)) != NULL) {
		gchar *full;

		full = g_build_filename (directory, filename, NULL);
		if (!g_file_test (full, G_FILE_TEST_IS_DIR)) {
			g_free (full);
			continue;
		}

		g_free (full);
		
 		filename_unescaped = gnome_vfs_unescape_string_for_display (filename);
		chatroom = log_get_chatroom_from_filename (account, filename_unescaped);

		g_free (filename_unescaped);

		if (!chatroom) {
			continue;
		}

		chatrooms = g_list_append (chatrooms, chatroom);
	}
	
	g_free (directory);
	g_dir_close (dir);
	
	return chatrooms;
}

gchar *
gossip_log_get_date_readable (const gchar *date)
{
	gossip_time_t t;
	
	t = gossip_time_from_string_full (date, LOG_TIME_FORMAT);
	return gossip_time_to_timestamp_full (t, "%a %d %b %Y");
}

/*
 * Contact functions 
 */
GList *
gossip_log_get_dates_for_contact (GossipContact *contact)
{
	GossipAccount  *account;
	GList          *dates = NULL;
	gchar          *date;
	gchar          *log_directory;
	gchar          *directory;
	const gchar    *account_id;
	gchar          *account_id_escaped;
	const gchar    *contact_id;
	gchar          *contact_id_escaped;
	GDir           *dir;
	const gchar    *filename;
	const gchar    *p;

	account = gossip_contact_get_account (contact);
	
	if (log_check_dir (&log_directory)) {
		DEBUG_MSG (("Log: No log directory exists"));
		return NULL;
	}

	account_id = gossip_account_get_id (account);
	account_id_escaped = gnome_vfs_escape_host_and_path_string (account_id);

	contact_id = gossip_contact_get_id (contact);
	contact_id_escaped = gnome_vfs_escape_host_and_path_string (contact_id);

	directory = g_build_path (G_DIR_SEPARATOR_S, 
				  log_directory, 
				  account_id_escaped,
				  contact_id_escaped,
				  NULL);

	g_free (contact_id_escaped);
	g_free (account_id_escaped);
	g_free (log_directory);
	
	dir = g_dir_open (directory, 0, NULL);
	if (!dir) {
		DEBUG_MSG (("Log: Could not open directory:'%s'", directory));
		g_free (directory);
		return NULL;
	}

	DEBUG_MSG (("Log: Collating a list of dates in:'%s'", directory));

	while ((filename = g_dir_read_name (dir)) != NULL) {
		if (!g_str_has_suffix (filename, LOG_FILENAME_SUFFIX)) {
			continue;
		}

		p = strstr (filename, LOG_FILENAME_SUFFIX);
		date = g_strndup (filename, p - filename);
		if (!date) {
			continue;
		}

		dates = g_list_insert_sorted (dates, date, (GCompareFunc) strcmp);
	}
	
	g_free (directory);
	g_dir_close (dir);

 	DEBUG_MSG (("Log: Parsed %d dates", g_list_length (dates))); 

	return dates;
} 

GList *
gossip_log_get_messages_for_contact (GossipContact *contact,
				     const gchar   *date)
{
	GossipAccount    *account;
	GossipContact    *own_contact;
	gchar            *filename;
	GList            *messages = NULL;
	gboolean          get_own_name = FALSE;
	gboolean          get_name = FALSE;
	xmlParserCtxtPtr  ctxt;
	xmlDocPtr         doc;
	xmlNodePtr        log_node;
	xmlNodePtr        node;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	filename = log_get_filename_by_date_for_contact (contact, date);

	DEBUG_MSG (("Log: Attempting to parse filename:'%s'...", filename));

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		DEBUG_MSG (("Log: Filename:'%s' does not exist", filename));
		g_free (filename);
		return NULL;
	}

	/* Get own contact from log contact. */
	account = gossip_contact_get_account (contact);
	own_contact = gossip_log_get_own_contact (account);

	/* Create parser. */
 	ctxt = xmlNewParserCtxt ();

	/* Parse and validate the file. */
	doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);	
	if (!doc) {
		g_warning ("Failed to parse file:'%s'", filename);
		g_free (filename);
		xmlFreeParserCtxt (ctxt);
		return NULL;
	}

	/* The root node, presets. */
	log_node = xmlDocGetRootElement (doc);
	if (!log_node) {
		g_free (filename);
		xmlFreeDoc (doc);
		xmlFreeParserCtxt (ctxt);
		return NULL;
	}

	if (gossip_contact_get_name (own_contact) == NULL ||
	    strcmp (gossip_contact_get_id (own_contact),
		    gossip_contact_get_name (own_contact)) == 0) {
		get_own_name = TRUE;
	}

	if (gossip_contact_get_name (contact) == NULL ||
	    strcmp (gossip_contact_get_id (contact),
		    gossip_contact_get_name (contact)) == 0) {
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
			gossip_contact_set_name (own_contact, name);
			log_set_own_name (account, own_contact);
			get_own_name = FALSE;
		}

		if (from && get_name) {
			gossip_contact_set_name (contact, name);
			log_set_name (account, contact);
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
		xmlFree (name);
		xmlFree (body);
	}
	
 	DEBUG_MSG (("Log: Parsed %d messages", g_list_length (messages))); 

	g_free (filename);
	xmlFreeDoc (doc);
	xmlFreeParserCtxt (ctxt);
	
	return messages;
}

void
gossip_log_message_for_contact (GossipMessage *message,
				gboolean       incoming)
{
	GossipAccount *account;
	GossipContact *contact;
	GossipContact *own_contact;
	GossipContact *own_contact_saved;
        const gchar   *filename;
	FILE          *file;
	const gchar   *to_or_from = "";
	gchar         *timestamp;
	gchar         *body;
	gchar         *resource;
	gchar         *name;
	gchar         *contact_id;
	const gchar   *str; 
	gboolean       new_file = FALSE;
	gboolean       save_contact = FALSE;
	gboolean       save_own_contact = FALSE;

	g_return_if_fail (GOSSIP_IS_MESSAGE (message));

	if (incoming) {
		to_or_from = "from";
		contact = gossip_message_get_sender (message);
		own_contact = gossip_message_get_recipient (message);
	} else {
		to_or_from = "to";
		contact = gossip_message_get_recipient (message);
		own_contact = gossip_message_get_sender (message);
	}

	account = gossip_contact_get_account (contact);
	own_contact_saved = gossip_log_get_own_contact (account);

	filename = log_get_filename_by_date_for_contact (contact, NULL);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		file = g_fopen (filename, "w+");
		if (file) {
			g_fprintf (file, LOG_HEADER);
		}

		/* ONLY save the name when we create new files, we are
		 * more efficient this way. 
		 */
		
		new_file = TRUE;
	} else {
		const gchar *own_id;
		const gchar *own_name;
		const gchar *real_name;
				
		file = g_fopen (filename, "r+");
		if (file) {
			fseek (file, - strlen (LOG_FOOTER), SEEK_END);
		}

		/* Check the message name and our name match, if not
		 * we save the message name since ours is out of
		 * sync. 
		 */
		own_id = gossip_contact_get_id (own_contact_saved);
		own_name = gossip_contact_get_name (own_contact_saved);

		real_name = gossip_contact_get_name (own_contact);

		if (real_name && (!own_name || 
				  strcmp (own_name, real_name) != 0 ||
				  strcmp (own_name, own_id) == 0)) {
			gossip_contact_set_name (own_contact_saved, real_name);
			save_own_contact = TRUE;
		}
	}

	if (new_file || save_own_contact) {
		GossipAccount *account;

		account = gossip_contact_get_account (contact);
		log_set_own_name (account, own_contact_saved);
	}

	if (new_file || save_contact) {
		GossipAccount *account;

		account = gossip_contact_get_account (contact);
		log_set_name (account, contact);
	}

	if (!file) {
		return;
	}

	timestamp = log_get_timestamp_from_message (message);

	/* Make sure we escape all data written */
	str = gossip_message_get_body (message); 
        if (str) {
		body = log_urlify (str);
	} else {
		body = g_strdup ("");
	}

	str = gossip_contact_get_name (contact);
	if (!str) {
		name = g_strdup ("");
	} else {
		name = g_markup_escape_text (str, -1);
	}

	str = gossip_contact_get_id (contact);
	if (!str) {
		contact_id = g_strdup ("");
	} else {
		contact_id = g_markup_escape_text (str, -1);
	}
	
	str = gossip_message_get_explicit_resource (message);
	if (!str) {
		resource = g_strdup ("");
	} else {
		resource = g_markup_escape_text (str, -1);
	}
	
	g_fprintf (file,
		   "<message time='%s' %s='%s' resource='%s' nick='%s'>"
		   "%s"
		   "</message>\n"
		   LOG_FOOTER,
		   timestamp, 
		   to_or_from,
		   contact_id,
		   resource,
		   name,
		   body);
	
        fclose (file);

	if (new_file) {
		g_chmod (filename, LOG_FILE_CREATE_MODE);
	}

	g_free (timestamp);
	g_free (body);
	g_free (resource);
	g_free (name);
	g_free (contact_id);

	log_handlers_notify_all (own_contact, contact, NULL, message);
}

gboolean
gossip_log_exists_for_contact (GossipContact *contact)
{
	GList    *dates;
	gboolean  exists;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);

	dates = gossip_log_get_dates_for_contact (contact);
	exists = g_list_length (dates) > 0;

	g_list_foreach (dates, (GFunc) g_free, NULL);
	g_list_free (dates);

	return exists;
}

#ifdef DEPRECATED
void
gossip_log_show_for_contact (GtkWidget     *window, 
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

	log = gossip_log_lookup_for_contact (contact);
	
	log_show (log);

	gdk_window_set_cursor (window->window, NULL);
}
#endif /* DEPRECATED */

/*
 * Chatroom functions 
 */
GList *
gossip_log_get_dates_for_chatroom (GossipChatroom *chatroom)
{
	GossipAccount  *account;
	GList          *dates = NULL;
	gchar          *date;
	gchar          *log_directory;
	gchar          *directory;
	const gchar    *account_id;
	gchar          *account_id_escaped;
	const gchar    *chatroom_id;
	gchar          *chatroom_id_escaped;
	GDir           *dir;
	const gchar    *filename;
	const gchar    *p;

	account = gossip_chatroom_get_account (chatroom);
	if (!account) {
		return NULL;
	}
	
	if (log_check_dir (&log_directory)) {
		DEBUG_MSG (("Log: No log directory exists"));
		return NULL;
	}

	account_id = gossip_account_get_id (account);
	account_id_escaped = gnome_vfs_escape_host_and_path_string (account_id);

	chatroom_id = gossip_chatroom_get_id_str (chatroom);
	chatroom_id_escaped = gnome_vfs_escape_host_and_path_string (chatroom_id);
	
	directory = g_build_path (G_DIR_SEPARATOR_S, 
				  log_directory, 
				  account_id_escaped,
				  LOG_DIR_CHATROOMS,
				  chatroom_id_escaped,
				  NULL);

	g_free (account_id_escaped);
	g_free (chatroom_id_escaped);
	g_free (log_directory);
	
	dir = g_dir_open (directory, 0, NULL);
	if (!dir) {
		DEBUG_MSG (("Log: Could not open directory:'%s'", directory));
		g_free (directory);
		return NULL;
	}

	DEBUG_MSG (("Log: Collating a list of dates in:'%s'", directory));

	while ((filename = g_dir_read_name (dir)) != NULL) {
		if (!g_str_has_suffix (filename, LOG_FILENAME_SUFFIX)) {
			continue;
		}

		p = strstr (filename, LOG_FILENAME_SUFFIX);
		date = g_strndup (filename, p - filename);
		if (!date) {
			continue;
		}

		dates = g_list_insert_sorted (dates, date, (GCompareFunc) strcmp);
	}
	
	g_free (directory);
	g_dir_close (dir);

 	DEBUG_MSG (("Log: Parsed %d dates", g_list_length (dates))); 

	return dates;
}

GList *
gossip_log_get_messages_for_chatroom (GossipChatroom *chatroom,
				      const gchar    *date)
{
	GossipAccount    *account;
	GossipContact    *own_contact;
	gchar            *filename;
	GList            *messages = NULL;
	xmlParserCtxtPtr  ctxt;
	xmlDocPtr         doc;
	xmlNodePtr        log_node;
	xmlNodePtr        node;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	filename = log_get_filename_by_date_for_chatroom (chatroom, date);

	DEBUG_MSG (("Log: Attempting to parse filename:'%s'...", filename));

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		DEBUG_MSG (("Log: Filename:'%s' does not exist", filename));
		g_free (filename);
		return NULL;
	}

	/* Get own contact from log contact. */
	account = gossip_chatroom_get_account (chatroom);
	own_contact = gossip_log_get_own_contact (account);

	/* Create parser. */
 	ctxt = xmlNewParserCtxt ();

	/* Parse and validate the file. */
	doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);	
	if (!doc) {
		g_warning ("Failed to parse file:'%s'", filename);
		g_free (filename);
		xmlFreeParserCtxt (ctxt);
		return NULL;
	}

	/* The root node, presets. */
	log_node = xmlDocGetRootElement (doc);
	if (!log_node) {
		g_free (filename);
		xmlFreeDoc (doc);
		xmlFreeParserCtxt (ctxt);
		return NULL;
	}

	/* Now get the messages. */
	for (node = log_node->children; node; node = node->next) {
		GossipMessage *message;
		GossipContact *contact;
		gchar         *time;
		gossip_time_t  t;
		gchar         *who;
		gchar         *name;
		gchar         *body;
		gboolean       sent = FALSE;

		if (strcmp (node->name, "message") != 0) {
			continue;
		}

		who = xmlGetProp (node, "from");
		if (!who) {
			continue;
		}

		name = xmlGetProp (node, "nick");
		if (!name) {
			g_free (who);
			continue;
		}

		/* Should we use type temporary or chatroom? */
		contact = gossip_contact_new_full (GOSSIP_CONTACT_TYPE_TEMPORARY, 
						   account,
						   who,
						   name);
		body = xmlNodeGetContent (node);
		
		time = xmlGetProp (node, "time");
		t = gossip_time_from_string_full (time, "%Y%m%dT%H:%M:%S");

		sent = gossip_contact_equal (contact, own_contact);
		
		if (sent) {			
			message = gossip_message_new (GOSSIP_MESSAGE_TYPE_CHAT_ROOM, contact);
			gossip_message_set_sender (message, own_contact);
		} else {
			message = gossip_message_new (GOSSIP_MESSAGE_TYPE_CHAT_ROOM, own_contact);
			gossip_message_set_sender (message, contact);
		}
		
		gossip_message_set_body (message, body);
		gossip_message_set_timestamp (message, t);
		
		messages = g_list_append (messages, message);

		g_object_unref (contact);
		
		xmlFree (time);
		xmlFree (who);
		xmlFree (name);
		xmlFree (body);
	}
	
 	DEBUG_MSG (("Log: Parsed %d messages", g_list_length (messages))); 

	g_free (filename);
	xmlFreeDoc (doc);
	xmlFreeParserCtxt (ctxt);
	
	return messages;
}

void
gossip_log_message_for_chatroom (GossipChatroom *chatroom,
				 GossipMessage  *message,
				 gboolean        incoming)
{
	GossipAccount *account;
	GossipContact *contact;
	GossipContact *own_contact;
        gchar         *filename;
	FILE          *file;
	gchar         *timestamp;
	gchar         *body;
	gchar         *name;
	gchar         *contact_id;
	const gchar   *str; 
	gboolean       new_file = FALSE;
	gboolean       save_contact = FALSE;

	g_return_if_fail (GOSSIP_IS_MESSAGE (message));

	contact = gossip_message_get_sender (message);

	account = gossip_chatroom_get_account (chatroom);
	own_contact = gossip_log_get_own_contact (account);

	filename = log_get_filename_by_date_for_chatroom (chatroom, NULL);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		file = g_fopen (filename, "w+");
		if (file) {
			g_fprintf (file, LOG_HEADER);
		}

		/* ONLY save the name when we create new files, we are
		 * more efficient this way. 
		 */
		
		new_file = TRUE;
	} else {
		file = g_fopen (filename, "r+");
		if (file) {
			fseek (file, - strlen (LOG_FOOTER), SEEK_END);
		}
	}

	if (new_file || save_contact) {
		GossipAccount *account;

		account = gossip_contact_get_account (contact);
		log_set_name (account, contact);
	}

	if (!file) {
		g_free (filename);
		return;
	}

	timestamp = log_get_timestamp_from_message (message);

	/* Make sure we escape all data written */
	str = gossip_message_get_body (message); 
        if (str) {
		body = log_urlify (str);
	} else {
		body = g_strdup ("");
	}

	str = gossip_contact_get_name (contact);
	if (!str) {
		name = g_strdup ("");
	} else {
		name = g_markup_escape_text (str, -1);
	}

	str = gossip_contact_get_id (contact);
	if (!str) {
		contact_id = g_strdup ("");
	} else {
		contact_id = g_markup_escape_text (str, -1);
	}
	
	g_fprintf (file,
		   "<message time='%s' from='%s' nick='%s'>"
		   "%s"
		   "</message>\n"
		   LOG_FOOTER,
		   timestamp, 
		   contact_id,
		   name,
		   body);

        fclose (file);

	if (new_file) {
		g_chmod (filename, LOG_FILE_CREATE_MODE);
	}

	g_free (timestamp);
	g_free (body);
	g_free (name);
	g_free (contact_id);
	g_free (filename);

	log_handlers_notify_all (own_contact, NULL, chatroom, message);
	g_object_unref (own_contact);
}

gboolean
gossip_log_exists_for_chatroom (GossipChatroom *chatroom)
{
	GList    *dates;
	gboolean  exists;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), FALSE);

	dates = gossip_log_get_dates_for_chatroom (chatroom);
	exists = g_list_length (dates) > 0;

	g_list_foreach (dates, (GFunc) g_free, NULL);
	g_list_free (dates);

	return exists;
}

#ifdef DEPRECATED 
void
gossip_log_show_for_chatroom (GtkWidget      *window,
			      GossipChatroom *chatroom)
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

	log = gossip_log_lookup_for_chatroom (chatroom);
	
	log_show (log);

	gdk_window_set_cursor (window->window, NULL);	
}
#endif /* DEPRECATED */

/*
 * Searching
 */
GList *
gossip_log_search_new (const gchar *text)
{
	GossipSession        *session;
	GossipAccountManager *manager;
	GList                *files;
	GList                *l;
	const gchar          *filename;
	gchar                *text_casefold;
	gchar                *contents;
	gchar                *contents_casefold;
	GList                *hits = NULL;

	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (strlen (text) > 0, NULL);

	text_casefold = g_utf8_casefold (text, -1);

	if (log_get_all_log_files (&files)) {
		DEBUG_MSG (("Log: Found %d log files in total", g_list_length (files)));
	} else {
		DEBUG_MSG (("Log: Failed to retrieve all log files"));
	}

	session = gossip_app_get_session ();
 	manager = gossip_session_get_account_manager (session);

	for (l = files; l; l = l->next) {
		filename = l->data;

		if (!g_file_get_contents (filename, &contents, NULL, NULL)) {
			continue;
		}
		
		if (!contents || strlen (contents) < 1) {
			continue;
		}
		
		/* FIXME: Handle chatrooms */
		if (strstr (filename, LOG_DIR_CHATROOMS)) {
			continue;
		}

		contents_casefold = g_utf8_casefold (contents, -1);
		g_free (contents);

		if (strstr (contents_casefold, text_casefold)) {
			GossipLogSearchHit *hit;
			GossipAccount      *account;
			gchar              *account_id;
			gchar              *contact_id;

			account_id = log_get_account_id_from_filename (filename);
			account = gossip_account_manager_find_by_id (manager, account_id);

			contact_id = log_get_contact_id_from_filename (filename);
			
			hit = g_new0 (GossipLogSearchHit, 1);

			hit->account = g_object_ref (account);
			hit->contact = log_get_contact (account, contact_id);

			hit->date = log_get_date_from_filename (filename);

			hit->filename = g_strdup (filename);

			hits = g_list_append (hits, hit);

			DEBUG_MSG (("Log: Found text:'%s' in file:'%s' on date:'%s'...", 
				    text, hit->filename, hit->date));

			g_free (account_id);
			g_free (contact_id);
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

		if (hit->account) {
			g_object_unref (hit->account);
		}

		if (hit->contact) {
			g_object_unref (hit->contact);
		}

		g_free (hit->date);
		g_free (hit->filename);

		g_free (hit);
	}
	
	g_list_free (hits);
}

GossipAccount *
gossip_log_search_hit_get_account (GossipLogSearchHit *hit)
{
	g_return_val_if_fail (hit != NULL, NULL);
	
	return hit->account;
}

GossipContact *
gossip_log_search_hit_get_contact (GossipLogSearchHit *hit)
{
	g_return_val_if_fail (hit != NULL, NULL);
	
	return hit->contact;
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
