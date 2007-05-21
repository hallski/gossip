/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
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
 * Where:
 *   - <account> is a string representation of the account name.
 *   - <contact> is a string representation of the contact id.
 *   - <date> is "20060102" and represents a day
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

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "gossip-chatroom-manager.h"
#include "gossip-protocol.h"
#include "gossip-debug.h"
#include "gossip-session.h"
#include "gossip-contact-manager.h"
#include "gossip-account-manager.h"
#include "gossip-time.h"
#include "gossip-private.h"
#include "gossip-utils.h"

#ifdef HAVE_GNOME
#include <libgnomevfs/gnome-vfs.h>
#endif

#define DEBUG_DOMAIN "Log"

#define LOG_PROTOCOL_VERSION   "2.0"

#define LOG_HEADER \
    "<?xml version='1.0' encoding='utf-8'?>\n" \
    "<?xml-stylesheet type=\"text/xsl\" href=\"gossip-log.xsl\"?>\n" \
    "<log>\n"

#define LOG_FOOTER \
    "</log>\n"

#define LOG_FILENAME_PREFIX       "file://"
#define LOG_FILENAME_SUFFIX       ".log"

#define LOG_DIR_CREATE_MODE       (S_IRUSR | S_IWUSR | S_IXUSR)
#define LOG_FILE_CREATE_MODE      (S_IRUSR | S_IWUSR)

#define LOG_DIR_CHATROOMS         "chatrooms"

#define LOG_KEY_FILENAME          "contacts.ini"
#define LOG_KEY_GROUP_SELF        "Self"
#define LOG_KEY_GROUP_CONTACTS    "Contacts"

#define LOG_KEY_VALUE_NAME        "Name"

#define LOG_TIME_FORMAT_FULL      "%Y%m%dT%H:%M:%S"
#define LOG_TIME_FORMAT           "%Y%m%d"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_LOG_MANAGER, GossipLogManagerPriv))

typedef struct _GossipLogManagerPriv  GossipLogManagerPriv;

struct _GossipLogManagerPriv {
	GossipSession *session;

	GHashTable    *message_handlers;
};

struct _GossipLogSearchHit {
	GossipAccount *account;
	GossipContact *contact;
	gchar         *filename;
	gchar         *date;
};

typedef struct {
	GossipAccount *account;
	GHashTable    *names;
	gchar         *filename;
} ContactNames;

typedef struct {
	GossipContact        *contact;
	GossipChatroom       *chatroom;
	GossipLogMessageFunc  func;
	gpointer              user_data;
} HandlerData;

typedef enum {
	LOG_VERSION_0,    /* filenames were "<account id>/<contact id>/" */
	LOG_VERSION_1,    /* filenames were "<protocol type>/<account id>/<contact id>/" */
	LOG_VERSION_LAST  /* filenames are  "<account name>/<contact id>/"*/
} LogVersion;

static void            gossip_log_manager_class_init           (GossipLogManagerClass *klass);
static void            gossip_log_manager_init                 (GossipLogManager      *manager);
static void            log_manager_finalize                    (GObject               *object);
#ifdef HAVE_GNOME
static void            log_move_contact_dirs                   (GossipLogManager      *manager,
								const gchar           *log_directory,
								const gchar           *filename,
								const gchar           *account_type);
#endif
static void            log_account_added_cb                    (GossipAccountManager  *manager,
								GossipAccount         *account,
								gpointer               user_data);
static void            log_account_removed_cb                  (GossipAccountManager  *manager,
								GossipAccount         *account,
								gpointer               user_data);
static void            log_account_renamed_cb                  (GossipAccount         *account,
								GParamSpec            *param,
								gpointer               user_data);
static gchar *         log_escape                              (const gchar           *str);
static gchar *         log_unescape                            (const gchar           *str);
static void            log_handler_free                        (HandlerData           *data);
static void            log_handlers_notify_foreach             (GossipLogMessageFunc   func,
								HandlerData           *data,
								GList                **list);
static void            log_handlers_notify_all                 (GossipLogManager      *manager,
								GossipContact         *own_contact,
								GossipContact         *contact,
								GossipChatroom        *chatroom,
								GossipMessage         *message);
static gboolean        log_check_dir                           (gchar                **directory);
#ifdef HAVE_GNOME
static LogVersion      log_check_version                       (void);
#endif
static gboolean        log_check_dir                           (gchar **directory);
static gchar *         log_get_basedir                         (GossipAccount         *account);
static gchar *         log_get_timestamp_from_message          (GossipMessage         *msg);
static gchar *         log_get_timestamp_filename              (void);
static gchar *         log_get_contact_id_from_filename        (const gchar           *filename);
static GossipChatroom *log_get_chatroom_from_filename          (GossipLogManager      *manager,
								GossipAccount         *account,
								const gchar           *filename);
static gboolean        log_get_all_log_files                   (GList                **files);
static void            log_get_all_log_files_for_chatrooms_dir (const gchar           *chatrooms_dir,
								GList                **files);
static void            log_get_all_log_files_in_directory      (const gchar           *directory,
								GList                **files);
static void            log_get_all_log_files_for_account_dir   (const gchar           *account_dir,
								GList                **files);
static void            log_get_all_log_files_for_type_dir      (const gchar           *type_dir,
								GList                **files);
static GossipAccount * log_get_account_from_filename           (GossipLogManager      *manager,
								const gchar           *filename);
static gchar *         log_get_date_from_filename              (const gchar           *filename);
static gchar *         log_get_filename_by_date_for_contact    (GossipContact         *contact,
								const gchar           *particular_date);
static gchar *         log_get_filename_by_date_for_chatroom   (GossipChatroom        *chatroom,
								const gchar           *particular_date);
static gboolean        log_set_name                            (GossipLogManager      *manager,
								GossipContact         *contact);
static gchar *         log_get_contact_log_dir                 (GossipContact *contact);
static gchar *         log_get_chatroom_log_dir                (GossipChatroom *chatroom);

G_DEFINE_TYPE (GossipLogManager, gossip_log_manager, G_TYPE_OBJECT);

static void
gossip_log_manager_class_init (GossipLogManagerClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = log_manager_finalize;

	g_type_class_add_private (object_class, sizeof (GossipLogManagerPriv));
}

static void
gossip_log_manager_init (GossipLogManager *manager)
{
	GossipLogManagerPriv *priv;

	priv = GET_PRIV (manager);

	priv->message_handlers = g_hash_table_new_full (g_direct_hash,
							g_direct_equal,
							NULL,
							(GDestroyNotify) log_handler_free);
}

static void
log_manager_finalize (GObject *object)
{
	GossipLogManagerPriv *priv;
	GossipAccountManager *account_manager;

	priv = GET_PRIV (object);

	account_manager = gossip_session_get_account_manager (priv->session);

	g_signal_handlers_disconnect_by_func (account_manager, 
					      log_account_added_cb,
					      NULL);
	g_signal_handlers_disconnect_by_func (account_manager, 
					      log_account_removed_cb,
					      NULL);

	if (priv->session) {
		g_object_unref (priv->session);
	}

	g_hash_table_destroy (priv->message_handlers);

	(G_OBJECT_CLASS (gossip_log_manager_parent_class)->finalize) (object);
}


GossipLogManager *
gossip_log_manager_new (GossipSession *session)
{
	GossipLogManagerPriv *priv;
	GossipLogManager     *manager;
	GossipAccountManager *account_manager;
#ifdef HAVE_GNOME
	LogVersion            version;
#endif
	GList                *accounts, *l;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);

	manager = g_object_new (GOSSIP_TYPE_LOG_MANAGER, NULL);
	
	priv = GET_PRIV (manager);

	priv->session = g_object_ref (session);

	account_manager = gossip_session_get_account_manager (priv->session);
	accounts = gossip_account_manager_get_accounts (account_manager);
	for (l = accounts; l; l = l->next) {
		log_account_added_cb (account_manager, l->data, NULL);
		g_object_unref (l->data);
	}
	g_list_free (accounts);

	g_signal_connect (account_manager, "account-added",
			  G_CALLBACK (log_account_added_cb),
			  NULL);
	g_signal_connect (account_manager, "account-removed",
			  G_CALLBACK (log_account_removed_cb),
			  NULL);

	/* We only support this when running GNOME. */
#ifdef HAVE_GNOME
	/* Check for new log protocol version, and if so we fix the
	 * differences here.
	 */
	version = log_check_version ();
	if (version == LOG_VERSION_0 || version == LOG_VERSION_1) {
		GDir        *dir;
		gchar       *log_directory;
		const gchar *basename;

		/* - Difference between version_0 and version_2 is we now use
		 *   account's name instead of account's id for the folder
		 * - Difference between version_1 and version_2 is we don't
		 *   prefix with the protocol type anymore and we use account
		 *   name instead of account id.
		 */

		if (log_check_dir (&log_directory)) {
			gossip_debug (DEBUG_DOMAIN, "No log directory exists");
			g_free (log_directory);
			return manager;
		}

		dir = g_dir_open (log_directory, 0, NULL);
		if (!dir) {
			gossip_debug (DEBUG_DOMAIN, "Could not open directory:'%s'", log_directory);
			g_free (log_directory);
			return manager;
		}

		while ((basename = g_dir_read_name (dir)) != NULL) {
			gchar *filename;

			if (strcmp (basename, "version") == 0) {
				continue;
			}

			filename = g_build_filename (log_directory, basename, NULL);
			if (version == LOG_VERSION_0) {
				log_move_contact_dirs (manager, log_directory, filename, NULL);
			} else {
				GDir        *type_dir;
				const gchar *basename2;

				type_dir = g_dir_open (filename, 0, NULL);
				if (!type_dir) {
					gossip_debug (DEBUG_DOMAIN, "Could not open directory:'%s'", filename);
					g_free (filename);
					continue;
				}

				while ((basename2 = g_dir_read_name (type_dir)) != NULL) {
					gchar *filename2;

					filename2 = g_build_filename (filename, basename2, NULL);
					log_move_contact_dirs (manager, log_directory, filename2, basename);
					g_free (filename2);
				}
				g_dir_close (type_dir);
				g_remove (filename);
			}
			g_free (filename);
		}
		g_dir_close (dir);
		g_free (log_directory);
	}
#endif /* HAVE_GNOME */

	return manager;
}

#ifdef HAVE_GNOME
static void
log_move_contact_dirs (GossipLogManager *manager,
		       const gchar      *log_directory,
		       const gchar      *filename,
		       const gchar      *type_str)
{
	GossipLogManagerPriv *priv;
	GnomeVFSResult        result = GNOME_VFS_OK;
	GnomeVFSURI          *new_uri, *old_uri;
	GnomeVFSURI          *new_uri_unknown;
	GossipAccount        *account;
	gchar                *basename;

	priv = GET_PRIV (manager);

	new_uri_unknown = gnome_vfs_uri_new (log_directory);
	new_uri_unknown = gnome_vfs_uri_append_path (new_uri_unknown, "Unknown");
	basename = g_path_get_basename (filename);

	if (strcmp (basename, LOG_DIR_CHATROOMS) == 0) {
		/* Special case */
		old_uri = gnome_vfs_uri_new (filename);
		new_uri = gnome_vfs_uri_ref (new_uri_unknown);
		g_mkdir_with_parents (gnome_vfs_uri_get_path (new_uri), LOG_DIR_CREATE_MODE);
		new_uri = gnome_vfs_uri_append_path (new_uri, basename);
	} else if (strcmp (basename, LOG_KEY_FILENAME) == 0) {
		/* Remove this file since it can't be
		 * placed anywhere and it will be
		 * regenerated anyway. 
		 */
		gossip_debug (DEBUG_DOMAIN, 
			      "Removing:'%s', will be recreated in the right place",
			      filename);
		g_remove (filename);
		g_free (basename);
		gnome_vfs_uri_unref (new_uri_unknown);
		return;
	} else {
		GossipAccountManager *account_manager;

		account_manager = gossip_session_get_account_manager (priv->session);
		account = gossip_account_manager_find_by_id (account_manager,
							     basename,
							     type_str);
		if (!account) {
			/* We must have other directories in
			 * here which are not account
			 * directories, so we just ignore them.
			 */
			gossip_debug (DEBUG_DOMAIN, 
				      "No account related to filename:'%s'",
				      filename);
			old_uri = gnome_vfs_uri_new (filename);
			new_uri = gnome_vfs_uri_dup (new_uri_unknown);
			g_mkdir_with_parents (gnome_vfs_uri_get_path (new_uri), LOG_DIR_CREATE_MODE);
			new_uri = gnome_vfs_uri_append_path (new_uri, basename);
		} else {
			const gchar *account_name;

			old_uri = gnome_vfs_uri_new (filename);
			new_uri = gnome_vfs_uri_new (log_directory);

			account_name = gossip_account_get_name (account);
			new_uri = gnome_vfs_uri_append_path (new_uri, account_name);
			g_mkdir_with_parents (gnome_vfs_uri_get_path (new_uri), LOG_DIR_CREATE_MODE);
		}
	}

	result = gnome_vfs_move_uri (old_uri, new_uri, TRUE);
			
	gossip_debug (DEBUG_DOMAIN,
		      "Transfering old URI:'%s' to new URI:'%s' returned:%d->'%s'",
		      gnome_vfs_uri_get_path (old_uri),
		      gnome_vfs_uri_get_path (new_uri),
		      result, 
		      gnome_vfs_result_to_string (result));
			
	g_free (basename);
	gnome_vfs_uri_unref (old_uri);
	gnome_vfs_uri_unref (new_uri);
	gnome_vfs_uri_unref (new_uri_unknown);
}
#endif

static void
log_account_added_cb (GossipAccountManager *manager,
		      GossipAccount        *account,
		      gpointer              user_data)
{
	g_object_set_data_full (G_OBJECT (account), "log-name",
				g_strdup (gossip_account_get_name (account)),
				(GDestroyNotify) g_free);
	g_signal_connect (account, "notify::name",
			  G_CALLBACK (log_account_renamed_cb),
			  NULL);
}

static void
log_account_removed_cb (GossipAccountManager *manager,
			GossipAccount        *account,
			gpointer              user_data)
{
	g_signal_handlers_disconnect_by_func (account,
					      G_CALLBACK (log_account_renamed_cb),
					      NULL);
}

static void
log_account_renamed_cb (GossipAccount *account,
			GParamSpec    *param,
			gpointer       user_data)
{
#ifdef HAVE_GNOME
	const gchar    *old_name;
	const gchar    *new_name;
	gchar          *log_directory;
	gchar          *new_uri;
	gchar          *old_uri;
	GnomeVFSResult  result;

	old_name = g_object_get_data (G_OBJECT (account), "log-name");
	new_name = gossip_account_get_name (account);

	if (strcmp (old_name, new_name) == 0) {
		return;
	}

	if (log_check_dir (&log_directory)) {
		gossip_debug (DEBUG_DOMAIN, "No log directory exists");
		g_free (log_directory);
		return;
	}

	old_uri = g_build_filename (log_directory, old_name, NULL);
	new_uri = g_build_filename (log_directory, new_name, NULL);
	result = gnome_vfs_move (old_uri, new_uri, TRUE);

	gossip_debug (DEBUG_DOMAIN,
		      "Transfering logs from:'%s' to:'%s' returned:%d->'%s'",
		      old_uri, new_uri, result, 
		      gnome_vfs_result_to_string (result));

	g_object_set_data_full (G_OBJECT (account), "log-name",
				g_strdup (new_name),
				(GDestroyNotify) g_free);

	g_free (log_directory);
	g_free (old_uri);
	g_free (new_uri);
#endif
}

static gchar *
log_escape (const gchar *str)
{
#ifdef HAVE_GNOME
	return gnome_vfs_escape_host_and_path_string (str);
#else
	return g_strdup (str);
#endif
}

static gchar *
log_unescape (const gchar *str)
{
#ifdef HAVE_GNOME
	return gnome_vfs_unescape_string_for_display (str);
#else
	return g_strdup (str);
#endif
}

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
gossip_log_handler_add_for_contact (GossipLogManager     *manager,
				    GossipContact        *contact,
				    GossipLogMessageFunc  func,
				    gpointer              user_data)
{
	GossipLogManagerPriv *priv;
	HandlerData          *data;

	g_return_if_fail (GOSSIP_IS_LOG_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (func != NULL);

	priv = GET_PRIV (manager);

	data = g_new0 (HandlerData, 1);

	data->contact = g_object_ref (contact);

	data->func = func;
	data->user_data = user_data;

	g_hash_table_insert (priv->message_handlers, func, data);
}

void
gossip_log_handler_add_for_chatroom (GossipLogManager     *manager,
				     GossipChatroom       *chatroom,
				     GossipLogMessageFunc  func,
				     gpointer              user_data)
{
	GossipLogManagerPriv *priv;
	HandlerData          *data;

	g_return_if_fail (GOSSIP_IS_LOG_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (func != NULL);

	priv = GET_PRIV (manager);

	data = g_new0 (HandlerData, 1);

	data->chatroom = g_object_ref (chatroom);

	data->func = func;
	data->user_data = user_data;

	g_hash_table_insert (priv->message_handlers, func, data);
}

void
gossip_log_handler_remove (GossipLogManager     *manager,
			   GossipLogMessageFunc  func)
{
	GossipLogManagerPriv *priv;

	g_return_if_fail (GOSSIP_IS_LOG_MANAGER (manager));

	priv = GET_PRIV (manager);

	g_hash_table_remove (priv->message_handlers, func);
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
log_handlers_notify_all (GossipLogManager *manager,
			 GossipContact    *own_contact,
			 GossipContact    *contact,
			 GossipChatroom   *chatroom,
			 GossipMessage    *message)
{
	GossipLogManagerPriv *priv;
	GossipLogMessageFunc  func;
	GList                *handlers = NULL;
	GList                *l;
	HandlerData          *data;

	priv = GET_PRIV (manager);

	g_hash_table_foreach (priv->message_handlers,
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

#ifdef HAVE_GNOME
static LogVersion
log_check_version (void)
{
	gchar *log_directory;
	gchar *filename;
	gchar *content;

	if (log_check_dir (&log_directory)) {
		gossip_debug (DEBUG_DOMAIN, "No log directory exists");
		g_free (log_directory);
		return LOG_VERSION_LAST;
	}

	filename = g_build_filename (log_directory, "version", NULL);
	if (!g_file_test (filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		GDir        *dir;
		const gchar *basename;
		gboolean     found = FALSE;

		gossip_debug (DEBUG_DOMAIN,
			      "Version file:'%s' does not exist, creating with version:'%s'",
			      filename, LOG_PROTOCOL_VERSION);

		/* Create version file */
		g_file_set_contents (filename, LOG_PROTOCOL_VERSION, -1, NULL);
		g_chmod (filename, LOG_FILE_CREATE_MODE);
		g_free (filename);

		/* There is a bug with log version 1.0, the version file was
		 * not created when gossip creates a new log directory.
		 * Here we try to guess if we have version_0 or version_1 */
		dir = g_dir_open (log_directory, 0, NULL);
		if (!dir) {
			gossip_debug (DEBUG_DOMAIN, "Could not open directory:'%s'",
				      log_directory);
			g_free (log_directory);
			return LOG_VERSION_0;
		}

		while ((basename = g_dir_read_name (dir)) != NULL) {
			gchar *contacts_file;

			contacts_file = g_build_filename (log_directory,
							  basename,
							  "contacts.ini",
							  NULL);
			if (g_file_test (contacts_file, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
				found = TRUE;
				break;
			}
			
			g_free (contacts_file);
		}

		g_dir_close (dir);
		g_free (log_directory);

		if (!found) {
			return LOG_VERSION_1;
		}

		return LOG_VERSION_0;
	}
	g_free (log_directory);

	g_file_get_contents (filename, &content, NULL, NULL);
	g_file_set_contents (filename, LOG_PROTOCOL_VERSION, -1, NULL);
	g_free (filename);
		
	if (!content) {
		return LOG_VERSION_0;
	}

	if (strcmp (content, LOG_PROTOCOL_VERSION) == 0) {
		g_free (content);
		return LOG_VERSION_LAST;
	}

	gossip_debug (DEBUG_DOMAIN, 
		      "Version file exists but is older version:'%s'", content);

	if (g_ascii_strncasecmp (content, "1.0", 3) == 0) {
		g_free (content);
		return LOG_VERSION_1;
	}

	g_free (content);
	
	return LOG_VERSION_LAST;
}
#endif /* HAVE_GNOME */

static gboolean
log_check_dir (gchar **directory)
{
	gchar    *dir;
	gboolean  created = FALSE;
	gchar    *filename;

	dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, "logs", NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		gossip_debug (DEBUG_DOMAIN, "Creating directory:'%s'", dir);
		created = TRUE;
		g_mkdir_with_parents (dir, LOG_DIR_CREATE_MODE);

		filename = g_build_filename (dir, "version", NULL);
		g_file_set_contents (filename, LOG_PROTOCOL_VERSION, -1, NULL);
		g_chmod (filename, LOG_FILE_CREATE_MODE);
		g_free (filename);
	}

	if (directory) {
		*directory = dir;
	} else {
		g_free (dir);
	}

	return created;
}


static gchar *
log_get_basedir (GossipAccount *account)
{
	const gchar *name;
	gchar       *name_escaped;
	gchar       *basedir;
	gchar       *log_dir;

	if (log_check_dir (&log_dir)) {
		gossip_debug (DEBUG_DOMAIN, "No log directory exists");
		g_free (log_dir);
		return NULL;
	}

	name = gossip_account_get_name (account);
	name_escaped = log_escape (name);
	basedir = g_build_filename (log_dir, name_escaped, NULL);
	g_free (name_escaped);
	g_free (log_dir);

	return basedir;
}

static gchar *
log_get_timestamp_from_message (GossipMessage *message)
{
	GossipTime t;

	t = gossip_message_get_timestamp (message);

	/* We keep the timestamps in the messages as UTC. */
	return gossip_time_to_string_utc (t, LOG_TIME_FORMAT_FULL);
}

static gchar *
log_get_timestamp_filename (void)
{
	GossipTime t;

	t = gossip_time_get_current ();
	return gossip_time_to_string_local (t, LOG_TIME_FORMAT);
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

static GossipAccount *
log_get_account_from_filename (GossipLogManager *manager,
			       const gchar      *filename)
{
	GossipLogManagerPriv *priv;
	GossipAccountManager *account_manager;
	GossipAccount        *account;
	gchar                *log_directory;
	const gchar          *p1;
	const gchar          *p2;
	gchar                *name;

	priv = GET_PRIV (manager);

	log_check_dir (&log_directory);

	p1 = strstr (filename, log_directory);
	if (!p1) {
		g_free (log_directory);
		return NULL;
	}

	p1 += strlen (log_directory) + 1;

	/* Get account name */
	p2 = strstr (p1, G_DIR_SEPARATOR_S);
	if (!p2) {
		g_free (log_directory);
		return NULL;
	}
	name = g_strndup (p1, p2 - p1);

	account_manager = gossip_session_get_account_manager (priv->session);
	account = gossip_account_manager_find (account_manager, name);

	g_free (name);
	g_free (log_directory);

	return account;
}

static GossipChatroom *
log_get_chatroom_from_filename (GossipLogManager *manager,
				GossipAccount    *account,
				const gchar      *filename)
{
	GossipLogManagerPriv  *priv;
	GossipChatroomManager *chatroom_manager;
	GossipChatroom        *chatroom;
	GList                 *found;
	gchar                 *server;
	gchar                 *room;

	priv = GET_PRIV (manager);

	room = g_strdup (filename);

	server = strstr (room, "@");
	if (!server) {
		g_free (room);
		return NULL;
	}

	server[0] = '\0';
	server++;

	chatroom_manager = gossip_session_get_chatroom_manager (priv->session);
	found = gossip_chatroom_manager_find_extended (chatroom_manager, account, server, room);

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
log_get_all_log_files_for_chatrooms_dir (const gchar  *chatrooms_dir,
					 GList       **files)
{
	GDir        *dir;
	const gchar *name;
	gchar       *path;

	dir = g_dir_open (chatrooms_dir, 0, NULL);
	if (!dir) {
		gossip_debug (DEBUG_DOMAIN, "Could not open directory:'%s'", chatrooms_dir);
		return;
	}

	while ((name = g_dir_read_name (dir)) != NULL) {
		path = g_build_filename (chatrooms_dir, name, NULL);
		log_get_all_log_files_in_directory (path, files);
		g_free (path);
	}

	g_dir_close (dir);
}

static void
log_get_all_log_files_in_directory (const gchar *directory, GList **files)
{
	GDir        *dir;
	const gchar *name;
	gchar       *path;

	dir = g_dir_open (directory, 0, NULL);
	if (!dir) {
		gossip_debug (DEBUG_DOMAIN, "Could not open directory:'%s'", directory);
		return;
	}

	while ((name = g_dir_read_name (dir)) != NULL) {
		if (!g_str_has_suffix (name, LOG_FILENAME_SUFFIX)) {
			continue;
		}

		path = g_build_filename (directory, name, NULL);

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
		gossip_debug (DEBUG_DOMAIN, "Could not open directory:'%s'", account_dir);
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
			log_get_all_log_files_in_directory (path, files);
		}

		g_free (path);
	}

	g_dir_close (dir);
}

static void
log_get_all_log_files_for_type_dir (const gchar  *type_dir,
				    GList       **files)
{
	GDir        *dir;
	const gchar *name;
	gchar       *path;

	dir = g_dir_open (type_dir, 0, NULL);
	if (!dir) {
		gossip_debug (DEBUG_DOMAIN, "Could not open directory:'%s'", type_dir);
		return;
	}

	while ((name = g_dir_read_name (dir)) != NULL) {
		path = g_build_filename (type_dir, name, NULL);

		if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
			g_free (path);
			continue;
		}

		log_get_all_log_files_for_account_dir (path, files);

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
		g_free (log_directory);

		gossip_debug (DEBUG_DOMAIN, "No log directory exists");
		return TRUE;
	}

	dir = g_dir_open (log_directory, 0, NULL);
	if (!dir) {
		g_free (log_directory);
		gossip_debug (DEBUG_DOMAIN, "Could not open directory:'%s'", log_directory);
		return FALSE;
	}

	while ((name = g_dir_read_name (dir)) != NULL) {
		account_dir = g_build_filename (log_directory, name, NULL);
		log_get_all_log_files_for_type_dir (account_dir, files);
		g_free (account_dir);
	}

	g_dir_close (dir);
	g_free (log_directory);

	return TRUE;
}

static gchar *
log_get_filename_by_date_for_contact (GossipContact *contact,
				      const gchar   *particular_date)
{
	gchar         *filename;
	gchar         *dirname;
	gchar         *directory;
	gchar         *basename;
	gchar         *todays_date = NULL;
	const gchar   *date;

	directory = log_get_contact_log_dir (contact);
	if (!directory) {
		return NULL;
	}

	if (!particular_date) {
		date = todays_date = log_get_timestamp_filename ();
	} else {
		date = particular_date;
	}

	basename = g_strconcat (date, LOG_FILENAME_SUFFIX, NULL);

	filename = g_build_filename (directory,
				     basename,
				     NULL);
	
	g_free (todays_date);
	g_free (directory);
	g_free (basename);

	gossip_debug (DEBUG_DOMAIN, "Using file:'%s' for contact:'%s'",
		      filename,
		      gossip_contact_get_id (contact));

	dirname = g_path_get_dirname (filename);
	if (!g_file_test (dirname, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		gossip_debug (DEBUG_DOMAIN, "Creating directory:'%s'", dirname);
		g_mkdir_with_parents (dirname, LOG_DIR_CREATE_MODE);
	}
	g_free (dirname);

	return filename;
}

static gchar *
log_get_filename_by_date_for_chatroom (GossipChatroom *chatroom,
				       const gchar    *particular_date)
{
	gchar         *filename;
	gchar         *dirname;
	gchar         *basename;
	gchar         *directory;
	gchar         *todays_date = NULL;
	const gchar   *date;

	directory = log_get_chatroom_log_dir (chatroom);
	if (!directory) {
		return NULL;
	}

	if (!particular_date) {
		date = todays_date = log_get_timestamp_filename ();
	} else {
		date = particular_date;
	}

	basename = g_strconcat (date, LOG_FILENAME_SUFFIX, NULL);

	filename = g_build_filename (directory, basename, NULL);
	
	g_free (basename);
	g_free (directory);
	g_free (todays_date);

	gossip_debug (DEBUG_DOMAIN, "Using file:'%s' for chatroom:'%s'",
		      filename,
		      gossip_chatroom_get_id_str (chatroom));

	dirname = g_path_get_dirname (filename);
	if (!g_file_test (dirname, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		gossip_debug (DEBUG_DOMAIN, "Creating directory:'%s'", dirname);
		g_mkdir_with_parents (dirname, LOG_DIR_CREATE_MODE);
	}

	g_free (dirname);

	return filename;
}

static gboolean
log_set_name (GossipLogManager *manager,
	      GossipContact    *contact)
{
	GossipLogManagerPriv *priv;
	GossipContactManager *contact_manager;

	priv = GET_PRIV (manager);

	gossip_debug (DEBUG_DOMAIN, 
		      "Setting name:'%s' for contact:'%s'", 
		      gossip_contact_get_name (contact),
		      gossip_contact_get_id (contact)); 

	contact_manager = gossip_session_get_contact_manager (priv->session);
	gossip_contact_manager_add (contact_manager, contact);

	return gossip_contact_manager_store (contact_manager);
}

GList *
gossip_log_get_contacts (GossipLogManager *manager, GossipAccount *account)
{
	GossipLogManagerPriv *priv;
	GossipContactManager *contact_manager;
	GossipContact        *contact;
	GList                *contacts = NULL;
	gchar                *directory;
	GDir                 *dir;
	const gchar          *filename;
	gchar                *filename_unescaped;

	g_return_val_if_fail (GOSSIP_IS_LOG_MANAGER (manager), NULL);
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GET_PRIV (manager);

	directory = log_get_basedir (account);

	dir = g_dir_open (directory, 0, NULL);
	if (!dir) {
		gossip_debug (DEBUG_DOMAIN, "Could not open directory:'%s'", directory);
		g_free (directory);
		return NULL;
	}

	/* Do this here instead of for every file */
	contact_manager = gossip_session_get_contact_manager (priv->session);

	gossip_debug (DEBUG_DOMAIN, "Collating a list of contacts in:'%s'", directory);

	while ((filename = g_dir_read_name (dir)) != NULL) {
		if (!g_file_test (directory, G_FILE_TEST_IS_DIR)) {
			continue;
		}

		if (strcmp (filename, LOG_DIR_CHATROOMS) == 0) {
			continue;
		}

		filename_unescaped = log_unescape (filename);

		contact = gossip_contact_manager_find (contact_manager, 
						       account,
						       filename_unescaped);
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
gossip_log_get_chatrooms (GossipLogManager *manager, GossipAccount *account)
{
	GList          *chatrooms = NULL;
	GossipChatroom *chatroom;
	gchar          *directory;
	GDir           *dir;
	const gchar    *filename;
	gchar          *filename_unescaped;
	gchar          *basedir;

	g_return_val_if_fail (GOSSIP_IS_LOG_MANAGER (manager), NULL);
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	basedir = log_get_basedir (account);

	directory = g_build_path (G_DIR_SEPARATOR_S,
				  basedir,
				  LOG_DIR_CHATROOMS,
				  NULL);

	g_free (basedir);

	dir = g_dir_open (directory, 0, NULL);
	if (!dir) {
		gossip_debug (DEBUG_DOMAIN, "Could not open directory:'%s'", directory);
		g_free (directory);
		return NULL;
	}

	gossip_debug (DEBUG_DOMAIN, "Collating a list of chatrooms in:'%s'", directory);

	while ((filename = g_dir_read_name (dir)) != NULL) {
		gchar *full;

		full = g_build_filename (directory, filename, NULL);
		if (!g_file_test (full, G_FILE_TEST_IS_DIR)) {
			g_free (full);
			continue;
		}

		g_free (full);

		filename_unescaped = log_unescape (filename);
		chatroom = log_get_chatroom_from_filename (manager, account, filename_unescaped);

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

/* Format is just date, 20061201. */
gchar *
gossip_log_get_date_readable (const gchar *date)
{
	GossipTime t;

	t = gossip_time_parse (date);

	return gossip_time_to_string_local (t, "%a %d %b %Y");
}

static gchar *
log_get_contact_log_dir (GossipContact *contact)
{
	gchar       *directory;
	const gchar *contact_id;
	gchar       *contact_id_escaped;
	gchar       *basedir;
		
	basedir = log_get_basedir (gossip_contact_get_account (contact));

	contact_id = gossip_contact_get_id (contact);
	contact_id_escaped = log_escape (contact_id);

	directory = g_build_path (G_DIR_SEPARATOR_S,
				  basedir,
				  contact_id_escaped,
				  NULL);

	g_free (contact_id_escaped);
	g_free (basedir);

	return directory;
}

/*
 * Contact functions
 */
GList *
gossip_log_get_dates_for_contact (GossipContact *contact)
{
	GList       *dates = NULL;
	gchar       *date;
	gchar       *directory;
	GDir        *dir;
	const gchar *filename;
	const gchar *p;

	directory = log_get_contact_log_dir (contact);
	if (!directory) {
		return NULL;
	}

	dir = g_dir_open (directory, 0, NULL);
	if (!dir) {
		gossip_debug (DEBUG_DOMAIN, "Could not open directory:'%s'", directory);
		g_free (directory);
		return NULL;
	}

	gossip_debug (DEBUG_DOMAIN, "Collating a list of dates in:'%s'", directory);

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

	gossip_debug (DEBUG_DOMAIN, "Parsed %d dates", g_list_length (dates));

	return dates;
}

GList *
gossip_log_get_messages_for_contact (GossipLogManager *manager,
				     GossipContact    *contact,
				     const gchar      *date)
{
	GossipLogManagerPriv *priv;
	GossipContact        *own_contact;
	gchar                *filename;
	GList                *messages = NULL;
	gboolean              get_own_name = FALSE;
	gboolean              get_name = FALSE;
	xmlParserCtxtPtr      ctxt;
	xmlDocPtr             doc;
	xmlNodePtr            log_node;
	xmlNodePtr            node;

	g_return_val_if_fail (GOSSIP_IS_LOG_MANAGER (manager), NULL);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (manager);

	filename = log_get_filename_by_date_for_contact (contact, date);

	gossip_debug (DEBUG_DOMAIN, "Attempting to parse filename:'%s'...", filename);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		gossip_debug (DEBUG_DOMAIN, "Filename:'%s' does not exist", filename);
		g_free (filename);
		return NULL;
	}

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

	own_contact = gossip_session_get_own_contact (priv->session, 
						      gossip_contact_get_account (contact));

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
			log_set_name (manager, own_contact);
			get_own_name = FALSE;
		}

		if (from && get_name) {
			gossip_contact_set_name (contact, name);
			log_set_name (manager, contact);
			get_name = FALSE;
		}
	}

	/* Now get the messages. */
	for (node = log_node->children; node; node = node->next) {
		GossipMessage *message;
		gchar         *time;
		GossipTime     t;
		gchar         *who;
		gchar         *name;
		gchar         *body;
		gboolean       sent = TRUE;

		if (strcmp (node->name, "message") != 0) {
			continue;
		}

		body = xmlNodeGetContent (node);

		time = xmlGetProp (node, "time");
		t = gossip_time_parse (time);

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

	gossip_debug (DEBUG_DOMAIN, "Parsed %d messages", g_list_length (messages));

	g_free (filename);
	xmlFreeDoc (doc);
	xmlFreeParserCtxt (ctxt);

	return messages;
}

void
gossip_log_message_for_contact (GossipLogManager *manager,
				GossipMessage    *message,
				gboolean          incoming)
{
	GossipLogManagerPriv *priv;
	GossipAccount        *account;
	GossipContact        *contact;
	GossipContact        *own_contact;
	GossipContact        *own_contact_saved;
	gchar                *filename;
	FILE                 *file;
	const gchar          *to_or_from = "";
	gchar                *timestamp;
	gchar                *body;
	gchar                *resource;
	gchar                *name;
	gchar                *contact_id;
	const gchar          *str;
	const gchar          *body_str;
	gboolean              new_file = FALSE;
	gboolean              save_contact = FALSE;
	gboolean              save_own_contact = FALSE;

	g_return_if_fail (GOSSIP_IS_LOG_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_MESSAGE (message));

	priv = GET_PRIV (manager);

	body_str = gossip_message_get_body (message);
	if (!body_str || strcmp (body_str, "") == 0) {
		gossip_debug (DEBUG_DOMAIN, "Skipping message with no content");
		return;
	}

	if (incoming) {
		to_or_from = "from";
		contact = gossip_message_get_sender (message);
		own_contact = gossip_message_get_recipient (message);

		if (gossip_contact_get_type (contact) == GOSSIP_CONTACT_TYPE_TEMPORARY) {
			gossip_debug (DEBUG_DOMAIN, "Skipping message from temporary contact:'%s'",
				      gossip_contact_get_id (contact));
			return;
		}
	} else {
		to_or_from = "to";
		contact = gossip_message_get_recipient (message);
		own_contact = gossip_message_get_sender (message);
	}

	account = gossip_contact_get_account (contact);
	own_contact_saved = gossip_session_get_own_contact (priv->session, account);

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
		log_set_name (manager, own_contact_saved);
	}

	if (new_file || save_contact) {
		log_set_name (manager, contact);
	}

	if (!file) {
		g_free (filename);
		return;
	}

	body = g_markup_escape_text (body_str, -1);

	timestamp = log_get_timestamp_from_message (message);

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

	g_free (filename);
	g_free (timestamp);
	g_free (body);
	g_free (resource);
	g_free (name);
	g_free (contact_id);

	log_handlers_notify_all (manager, own_contact, contact, NULL, message);
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

GList *
gossip_log_get_last_for_contact (GossipLogManager *manager,
				 GossipContact    *contact)
{
	GList *messages;
	GList *dates;
	GList *l;

	g_return_val_if_fail (GOSSIP_IS_LOG_MANAGER (manager), NULL);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	dates = gossip_log_get_dates_for_contact (contact);
	if (g_list_length (dates) < 1) {
		return NULL;
	}

	l = g_list_last (dates);
	messages = gossip_log_get_messages_for_contact (manager, contact, l->data);

	g_list_foreach (dates, (GFunc) g_free, NULL);
	g_list_free (dates);

	return messages;
}

static gchar *
log_get_chatroom_log_dir (GossipChatroom *chatroom)
{
	gchar       *directory;
	const gchar *chatroom_id;
	gchar       *chatroom_id_escaped;
	gchar       *basedir;
	
	if (!gossip_chatroom_get_account (chatroom)) {
		return NULL;
	}

	basedir = log_get_basedir (gossip_chatroom_get_account (chatroom));

	chatroom_id = gossip_chatroom_get_id_str (chatroom);
	chatroom_id_escaped = log_escape (chatroom_id);

	directory = g_build_path (G_DIR_SEPARATOR_S,
				  basedir,
				  LOG_DIR_CHATROOMS,
				  chatroom_id_escaped,
				  NULL);
	g_free (basedir);
	g_free (chatroom_id_escaped);

	return directory;
}

/*
 * Chatroom functions
 */
GList *
gossip_log_get_dates_for_chatroom (GossipChatroom *chatroom)
{
	GList          *dates = NULL;
	gchar          *date;
	gchar          *directory;
	GDir           *dir;
	const gchar    *filename;
	const gchar    *p;

	directory = log_get_chatroom_log_dir (chatroom);

	dir = g_dir_open (directory, 0, NULL);
	if (!dir) {
		gossip_debug (DEBUG_DOMAIN, "Could not open directory:'%s'", directory);
		g_free (directory);
		return NULL;
	}

	gossip_debug (DEBUG_DOMAIN, "Collating a list of dates in:'%s'", directory);
	
	g_free (directory);

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

	g_dir_close (dir);

	gossip_debug (DEBUG_DOMAIN, "Parsed %d dates", g_list_length (dates));

	return dates;
}

GList *
gossip_log_get_messages_for_chatroom (GossipLogManager *manager,
				      GossipChatroom   *chatroom,
				      const gchar      *date)
{
	GossipLogManagerPriv *priv;
	GossipContact        *own_contact;
	gchar                *filename;
	GList                *messages = NULL;
	xmlParserCtxtPtr      ctxt;
	xmlDocPtr             doc;
	xmlNodePtr            log_node;
	xmlNodePtr            node;

	g_return_val_if_fail (GOSSIP_IS_LOG_MANAGER (manager), NULL);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (manager);

	filename = log_get_filename_by_date_for_chatroom (chatroom, date);

	gossip_debug (DEBUG_DOMAIN, "Attempting to parse filename:'%s'...", filename);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		gossip_debug (DEBUG_DOMAIN, "Filename:'%s' does not exist", filename);
		g_free (filename);
		return NULL;
	}

	/* Get own contact from log contact. */
	own_contact = gossip_session_get_own_contact (priv->session, 
						      gossip_chatroom_get_account (chatroom));

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

	priv = GET_PRIV (manager);

	/* Now get the messages. */
	for (node = log_node->children; node; node = node->next) {
		GossipMessage  *message;
		GossipContact  *contact;
		gchar          *time;
		GossipTime      t;
		gchar          *who;
		gchar          *name;
		gchar          *body;

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

		body = xmlNodeGetContent (node);

		time = xmlGetProp (node, "time");
		t = gossip_time_parse (time);
		
		contact = gossip_contact_new_full (GOSSIP_CONTACT_TYPE_CHATROOM,
						   gossip_chatroom_get_account (chatroom),
						   who, name);

		message = gossip_message_new (GOSSIP_MESSAGE_TYPE_CHAT_ROOM,
					      own_contact);

		gossip_message_set_sender (message, contact);
		g_object_unref (contact);

		gossip_message_set_body (message, body);
		gossip_message_set_timestamp (message, t);

		messages = g_list_append (messages, message);

		xmlFree (time);
		xmlFree (who);
		xmlFree (name);
		xmlFree (body);
	}

	gossip_debug (DEBUG_DOMAIN, "Parsed %d messages", g_list_length (messages));

	g_free (filename);
	xmlFreeDoc (doc);
	xmlFreeParserCtxt (ctxt);

	return messages;
}

void
gossip_log_message_for_chatroom (GossipLogManager *manager,
				 GossipChatroom   *chatroom,
				 GossipMessage    *message,
				 gboolean          incoming)
{
	GossipLogManagerPriv *priv;
	GossipContact        *contact;
	GossipContact        *own_contact;
	gchar                *filename;
	FILE                 *file;
	gchar                *timestamp;
	gchar                *body;
	gchar                *name;
	gchar                *contact_id;
	const gchar          *str;
	const gchar          *body_str;
	gboolean              new_file = FALSE;
	gboolean              save_contact = FALSE;

	g_return_if_fail (GOSSIP_IS_LOG_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_MESSAGE (message));

	priv = GET_PRIV (manager);

	body_str = gossip_message_get_body (message);
	if (!body_str || strcmp (body_str, "") == 0) {
		gossip_debug (DEBUG_DOMAIN, "Skipping message with no content");
		return;
	}

	contact = gossip_message_get_sender (message);

	own_contact = gossip_session_get_own_contact (priv->session, 
						      gossip_chatroom_get_account (chatroom));

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
		log_set_name (manager, contact);
	}

	if (!file) {
		g_free (filename);
		return;
	}

	timestamp = log_get_timestamp_from_message (message);

	body = g_markup_escape_text (body_str, -1);

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

	log_handlers_notify_all (manager, own_contact, NULL, chatroom, message);
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

/* FIXME: Use this code for searching since that is really slow. */
#if 0
static void
log_foo (GtkWidget *window)
{
	GdkCursor *cursor;

	cursor = gdk_cursor_new (GDK_WATCH);
	gdk_window_set_cursor (window->window, cursor);
	gdk_cursor_unref (cursor);

	/* Let the UI redraw before we start the slow transformation. */
	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}

	// ...

	gdk_window_set_cursor (window->window, NULL);
}
#endif

/*
 * Searching
 */
GList *
gossip_log_search_new (GossipLogManager *manager,
		       const gchar      *text)
{
	GossipLogManagerPriv *priv;
	GossipContactManager *contact_manager;
	GList                *files;
	GList                *l;
	const gchar          *filename;
	gchar                *text_casefold;
	gchar                *contents;
	gchar                *contents_casefold;
	GList                *hits = NULL;

	g_return_val_if_fail (GOSSIP_IS_LOG_MANAGER (manager), NULL);
	g_return_val_if_fail (!G_STR_EMPTY (text), NULL);

	priv = GET_PRIV (manager);

	text_casefold = g_utf8_casefold (text, -1);

	if (log_get_all_log_files (&files)) {
		gossip_debug (DEBUG_DOMAIN, "Found %d log files in total", g_list_length (files));
	} else {
		gossip_debug (DEBUG_DOMAIN, "Failed to retrieve all log files");
	}

	/* Do this here instead of for each file */
	contact_manager = gossip_session_get_contact_manager (priv->session);

	for (l = files; l; l = l->next) {
		GMappedFile *file;
		gsize        length;

		filename = l->data;

		/* FIXME: Handle chatrooms */
		if (strstr (filename, LOG_DIR_CHATROOMS)) {
			continue;
		}

		file = g_mapped_file_new (filename, FALSE, NULL);
		if (!file) {
			continue;
		}

		length = g_mapped_file_get_length (file);
		contents = g_mapped_file_get_contents (file);

		contents_casefold = g_utf8_casefold (contents, length);

		g_mapped_file_free (file);

		if (strstr (contents_casefold, text_casefold)) {
			GossipLogSearchHit *hit;
			GossipAccount      *account;
			gchar              *contact_id;

			account = log_get_account_from_filename (manager, filename);
			if (!account) {
				/* We must have other directories in
				 * here which are not account
				 * directories, so we just ignore them.
				 */
				continue;
			}

			contact_id = log_get_contact_id_from_filename (filename);

			hit = g_new0 (GossipLogSearchHit, 1);

			hit->date = log_get_date_from_filename (filename);
			hit->filename = g_strdup (filename);
			hit->account = g_object_ref (account);
			hit->contact = gossip_contact_manager_find (contact_manager, 
								    account,
								    contact_id);

			hits = g_list_append (hits, hit);

			gossip_debug (DEBUG_DOMAIN, 
				      "Found text:'%s' in file:'%s' on date:'%s'...",
				      text, hit->filename, hit->date);

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
