/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
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

#include "config.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "gossip-debug.h"
#include "gossip-jabber.h"
#include "gossip-account-manager.h"
#include "gossip-private.h"
#include "gossip-utils.h"

#include "libgossip-marshal.h"

#define DEBUG_DOMAIN "AccountManager"

#define ACCOUNTS_XML_FILENAME "accounts.xml"
#define ACCOUNTS_DTD_FILENAME "gossip-account.dtd"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_ACCOUNT_MANAGER, GossipAccountManagerPriv))

typedef struct _GossipAccountManagerPriv GossipAccountManagerPriv;

struct _GossipAccountManagerPriv {
	GList *accounts;

	gchar *accounts_file_name;

	gchar *default_name;
	gchar *default_name_override;
};

enum {
	ACCOUNT_ADDED,
	ACCOUNT_REMOVED,
	NEW_DEFAULT,
	LAST_SIGNAL
};

static void     account_manager_finalize               (GObject              *object);
static gboolean account_manager_get_all                (GossipAccountManager *manager);
static gboolean account_manager_file_parse             (GossipAccountManager *manager,
							const gchar          *filename);
static gboolean account_manager_file_save              (GossipAccountManager *manager);

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE (GossipAccountManager, gossip_account_manager, G_TYPE_OBJECT);

static void
gossip_account_manager_class_init (GossipAccountManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = account_manager_finalize;

	signals[ACCOUNT_ADDED] =
		g_signal_new ("account-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_ACCOUNT);
	signals[ACCOUNT_REMOVED] =
		g_signal_new ("account-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_ACCOUNT);
	signals[NEW_DEFAULT] =
		g_signal_new ("new-default",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_ACCOUNT);

	g_type_class_add_private (object_class,
				  sizeof (GossipAccountManagerPriv));
}

static void
gossip_account_manager_init (GossipAccountManager *manager)
{
}

static void
account_manager_finalize (GObject *object)
{
	GossipAccountManagerPriv *priv;

	priv = GET_PRIV (object);

	g_list_foreach (priv->accounts, (GFunc) g_object_unref, NULL);
	g_list_free (priv->accounts);

	g_free (priv->accounts_file_name);

	g_free (priv->default_name);
	g_free (priv->default_name_override);

	G_OBJECT_CLASS (gossip_account_manager_parent_class)->finalize (object);
}

GossipAccountManager *
gossip_account_manager_new (const gchar *filename)
{

	GossipAccountManager     *manager;
	GossipAccountManagerPriv *priv;

	manager = g_object_new (GOSSIP_TYPE_ACCOUNT_MANAGER, NULL);

	priv = GET_PRIV (manager);

	if (filename) {
		priv->accounts_file_name = g_strdup (filename);
	}

	/* load file */
	account_manager_get_all (manager);

	return manager;
}

gboolean
gossip_account_manager_add (GossipAccountManager *manager,
			    GossipAccount        *account)
{
	GossipAccountManagerPriv *priv;
	GList                    *l;
	const gchar              *name;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), FALSE);
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);

	priv = GET_PRIV (manager);

	/* Don't add more than once */
	for (l = priv->accounts; l; l = l->next) {
		if (gossip_account_equal (l->data, account)) {
			return FALSE;
		}
	}

	priv->accounts = g_list_append (priv->accounts, g_object_ref (account));
	gossip_account_manager_set_unique_name (manager, account);

	name = gossip_account_get_name (account);
	gossip_debug (DEBUG_DOMAIN, "Adding account with name:'%s'", name);

	g_signal_emit (manager, signals[ACCOUNT_ADDED], 0, account);

	return TRUE;
}

void
gossip_account_manager_remove (GossipAccountManager *manager,
			       GossipAccount        *account)
{
	GossipAccountManagerPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GET_PRIV (manager);

	gossip_debug (DEBUG_DOMAIN,
		      "Removing account with name:'%s'",
		      gossip_account_get_name (account));

	priv->accounts = g_list_remove (priv->accounts, account);

	g_signal_emit (manager, signals[ACCOUNT_REMOVED], 0, account);

	g_object_unref (account);
}

GList *
gossip_account_manager_get_accounts (GossipAccountManager *manager)
{
	GossipAccountManagerPriv *priv;
	GList                    *accounts;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), NULL);

	priv = GET_PRIV (manager);

	accounts = g_list_copy (priv->accounts);
	g_list_foreach (accounts, (GFunc) g_object_ref, NULL);

	return accounts;
}

guint
gossip_account_manager_get_count (GossipAccountManager *manager)
{
	GossipAccountManagerPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), 0);

	priv = GET_PRIV (manager);

	return g_list_length (priv->accounts);
}


GossipAccount *
gossip_account_manager_find (GossipAccountManager *manager,
			     const gchar          *name)
{
	GossipAccountManagerPriv *priv;
	GList                    *l;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	priv = GET_PRIV (manager);

	for (l = priv->accounts; l; l = l->next) {
		GossipAccount *account;
		const gchar   *account_name;

		account = l->data;
		account_name = gossip_account_get_name (account);

		if (strcmp (account_name, name) == 0) {
			return account;
		}
	}

	return NULL;
}

GossipAccount *
gossip_account_manager_find_by_id (GossipAccountManager *manager,
				   const gchar          *id)
{
	GossipAccountManagerPriv *priv;
	GList                    *l;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), NULL);
	g_return_val_if_fail (!G_STR_EMPTY (id), NULL);

	priv = GET_PRIV (manager);

	for (l = priv->accounts; l; l = l->next) {
		GossipAccount *account;
		const gchar   *this_id = NULL;

		account = l->data;

		this_id = gossip_account_get_id (account);

		if (this_id && strcmp (id, this_id) == 0) {
			return account;
		}
	}

	return NULL;
}

void
gossip_account_manager_set_overridden_default (GossipAccountManager *manager,
					       const gchar          *name)
{
	GossipAccountManagerPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager));
	g_return_if_fail (name != NULL);

	priv = GET_PRIV (manager);

	gossip_debug (DEBUG_DOMAIN,
		      "Setting overriding default account with name:'%s'",
		      name);

	g_free (priv->default_name_override);
	priv->default_name_override = g_strdup (name);
}

void
gossip_account_manager_set_default (GossipAccountManager *manager,
				    GossipAccount        *account)
{
	GossipAccountManagerPriv *priv;
	const gchar              *name;

	g_return_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GET_PRIV (manager);

	gossip_debug (DEBUG_DOMAIN,
		      "Setting default account with name:'%s'",
		      gossip_account_get_name (account));

	name = gossip_account_get_name (account);
	g_return_if_fail (name != NULL);

	g_free (priv->default_name);
	priv->default_name = g_strdup (name);

	/* save */
	gossip_account_manager_add (manager, account);

	g_signal_emit (manager, signals[NEW_DEFAULT], 0, account);
}

GossipAccount *
gossip_account_manager_get_default (GossipAccountManager *manager)
{
	GossipAccountManagerPriv *priv;
	const gchar              *name;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), FALSE);

	priv = GET_PRIV (manager);

	/* use override name first */
	name = priv->default_name_override;

	/* use default name second */
	if (!name) {
		name = priv->default_name;

		/* check it exists, if not use the first one we find */
		if (name && !gossip_account_manager_find (manager, name)) {
			name = NULL;
		}
	}

	if (!name) {
		/* if one or more entries, use that */
		if (gossip_account_manager_get_count (manager) >= 1) {
			GossipAccount *account;

			account = priv->accounts->data;

			name = gossip_account_get_name (account);
			gossip_account_manager_set_default (manager, account);

			/* We return the default account ref here. */
			return account;
		}
	} else {
		return gossip_account_manager_find (manager, name);
	}

	return NULL;
}

gboolean
gossip_account_manager_store (GossipAccountManager *manager)
{
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), FALSE);

	gossip_debug (DEBUG_DOMAIN, "Saving accounts");

	return account_manager_file_save (manager);
}

/*
 * API to save/load and parse the accounts file.
 */

static gboolean
account_manager_get_all (GossipAccountManager *manager)
{
	GossipAccountManagerPriv *priv;
	gchar                    *dir;
	gchar                    *file_with_path = NULL;

	priv = GET_PRIV (manager);

	/* Use default if no file specified. */
	if (!priv->accounts_file_name) {
		dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
		if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
			g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
		}

		file_with_path = g_build_filename (dir, ACCOUNTS_XML_FILENAME, NULL);
		g_free (dir);
	} else {
		file_with_path = g_strdup (priv->accounts_file_name);
	}

	/* Read file in */
	if (g_file_test (file_with_path, G_FILE_TEST_EXISTS) &&
	    !account_manager_file_parse (manager, file_with_path)) {
		g_free (file_with_path);
		return FALSE;
	}

	g_free (file_with_path);

	return TRUE;
}

static void
account_manager_parse_account (GossipAccountManager *manager, xmlNodePtr node)
{
	GossipAccount *account;
	xmlNodePtr     child;
	gchar         *str;

	account = gossip_jabber_new_account ();

	child = node->children;
	while (child) {
		gchar *tag;

		tag = (gchar *) child->name;
		str = (gchar *) xmlNodeGetContent (child);

		if (!str) {
			continue;
		}

		if (strcmp (tag, "name") == 0) {
			gossip_account_set_name (account, str);
		} else if (strcmp (tag, "id") == 0) {
			gossip_account_set_id (account, str);
		} else if (strcmp (tag, "resource") == 0) {
			gossip_account_set_resource (account, str);
		} else if (strcmp (tag, "password") == 0) {
#ifdef HAVE_GNOME_KEYRING
			const gchar *current_password;

			/* If the current password is empty, then we
			 * see if there is a password saved on disk
			 * and try that instead. 
			 *
			 * This helps with migrating to the gnome
			 * keyring. 
			 */
			current_password = gossip_account_get_password (account);
			if (G_STR_EMPTY (current_password) && !G_STR_EMPTY (str)) {
				gossip_account_set_password (account, str);
			}
#else  /* HAVE_GNOME_KEYRING */
			gossip_account_set_password (account, str);
#endif /* HAVE_GNOME_KEYRING */
		} else if (strcmp (tag, "server") == 0) {
			gossip_account_set_server (account, str);
		} else if (strcmp (tag, "port") == 0) {
			guint port;

			port = atoi (str);
			if (port != 0) {
				gossip_account_set_port (account, port);
			}
		} else if (strcmp (tag, "auto_connect") == 0) {
			gossip_account_set_auto_connect (account, strcmp (str, "yes") == 0);
		} else if (strcmp (tag, "use_ssl") == 0) {
			gossip_account_set_use_ssl (account, strcmp (str, "yes") == 0);
		} else if (strcmp (tag, "use_proxy") == 0) {
			gossip_account_set_use_proxy (account, strcmp (str, "yes") == 0);
		}

		xmlFree (str);

		child = child->next;
	}

	gossip_account_manager_add (manager, account);

	g_object_unref (account);
}

static gboolean
account_manager_file_parse (GossipAccountManager *manager,
			    const gchar          *filename)
{
	GossipAccountManagerPriv *priv;
	xmlParserCtxtPtr          ctxt;
	xmlDocPtr                 doc;
	xmlNodePtr                accounts;
	xmlNodePtr                node;
	gchar                    *str;

	priv = GET_PRIV (manager);

	gossip_debug (DEBUG_DOMAIN,
		      "Attempting to parse file:'%s'...",
		      filename);

	ctxt = xmlNewParserCtxt ();

	/* Parse and validate the file. */
	doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);
	if (!doc) {
		g_warning ("Failed to parse file:'%s'", filename);
		xmlFreeParserCtxt (ctxt);
		return FALSE;
	}

	if (!gossip_xml_validate (doc, ACCOUNTS_DTD_FILENAME)) {
		g_warning ("Failed to validate file:'%s'", filename);
		xmlFreeDoc (doc);
		xmlFreeParserCtxt (ctxt);
		return FALSE;
	}

	/* The root node, accounts. */
	accounts = xmlDocGetRootElement (doc);

	node = accounts->children;
	while (node) {
		if (strcmp ((gchar *) node->name, "default") == 0) {
			/* Get the default account. */
			str = (gchar *) xmlNodeGetContent (node);

			g_free (priv->default_name);
			priv->default_name = g_strdup (str);

			xmlFree (str);
		}
		else if (strcmp ((gchar *) node->name, "account") == 0) {
			account_manager_parse_account (manager, node);
		}

		node = node->next;
	}

	gossip_debug (DEBUG_DOMAIN,
		      "Parsed %d accounts",
		      g_list_length (priv->accounts));

	gossip_debug (DEBUG_DOMAIN,
		      "Default account is:'%s'",
		      priv->default_name);

	xmlFreeDoc(doc);
	xmlFreeParserCtxt (ctxt);

	return TRUE;
}

static gboolean
account_manager_file_save (GossipAccountManager *manager)
{
	GossipAccountManagerPriv *priv;
	xmlDocPtr                 doc;
	xmlDocPtr                 old_doc;
	xmlNodePtr                root;
	xmlParserCtxtPtr          ctxt;
	GList                    *accounts;
	GList                    *l;
	gchar                    *xml_dir;
	gchar                    *xml_file;
#ifndef G_OS_WIN32
	mode_t                    old_mask;
#endif /* G_OS_WIN32 */

	priv = GET_PRIV (manager);

	if (priv->accounts_file_name) {
		xml_file = g_strdup (priv->accounts_file_name);
	} else {
		xml_dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
		g_mkdir_with_parents (xml_dir, S_IRUSR | S_IWUSR | S_IXUSR);

		xml_file = g_build_filename (xml_dir, ACCOUNTS_XML_FILENAME, NULL);
		g_free (xml_dir);
	}

	doc = xmlNewDoc ("1.0");
	root = xmlNewNode (NULL, "accounts");
	xmlDocSetRootElement (doc, root);

	if (!priv->default_name) {
		priv->default_name = g_strdup ("Default");
	}

	xmlNewChild (root, NULL, "default", priv->default_name);

	/* Copy not used accounts in the new document */
	ctxt = xmlNewParserCtxt ();
	old_doc = xmlCtxtReadFile (ctxt, xml_file, NULL, 0);

	if (old_doc) {
		xmlNodePtr node;

		node = xmlDocGetRootElement (old_doc);
		for (node = node->children; node; node = node->next) {
			if (strcmp ((gchar *) node->name, "default") == 0) {
				continue;
			}
		}

		xmlFreeDoc(old_doc);
	}

	xmlFreeParserCtxt (ctxt);

	accounts = gossip_account_manager_get_accounts (manager);
	for (l = accounts; l; l = l->next) {
		GossipAccount *account;
		gchar         *port;
		xmlNodePtr     node;
	
		account = l->data;

		port = g_strdup_printf ("%d", gossip_account_get_port (account));

		node = xmlNewChild (root, NULL, "account", NULL);
		xmlNewTextChild (node, NULL, "name", gossip_account_get_name (account));
		xmlNewTextChild (node, NULL, "id", gossip_account_get_id (account));

		xmlNewTextChild (node, NULL, "resource", gossip_account_get_resource (account));
#ifdef HAVE_GNOME_KEYRING
		xmlAddChild (node, xmlNewComment ("Password is stored in GNOME Keyring"));
#else  /* HAVE_GNOME_KEYRING */
		xmlNewTextChild (node, NULL, "password", gossip_account_get_password (account));
#endif /* HAVE_GNOME_KEYRING */
		xmlNewTextChild (node, NULL, "server", gossip_account_get_server (account));
		xmlNewChild (node, NULL, "port", port);

		xmlNewChild (node, NULL, "auto_connect", gossip_account_get_auto_connect (account) ? "yes" : "no");
		xmlNewChild (node, NULL, "use_ssl", gossip_account_get_use_ssl (account) ? "yes" : "no");
		xmlNewChild (node, NULL, "use_proxy", gossip_account_get_use_proxy (account) ? "yes" : "no");

		g_free (port);
	}

	gossip_debug (DEBUG_DOMAIN, "Saving file:'%s'", xml_file);

#ifndef G_OS_WIN32
	/* Set the umask to get the proper permissions when libxml saves the
	 * file, but also change the permissions expiicitly in case the file
	 * already exists.
	 */
	old_mask = umask (077);
#endif /* G_OS_WIN32 */

	chmod (xml_file, 0600);

	/* Make sure the XML is indented properly */
	xmlIndentTreeOutput = 1;

	xmlSaveFormatFileEnc (xml_file, doc, "utf-8", 1);

#ifndef G_OS_WIN32
	/* Reset the umask */
	umask (old_mask);
#endif /* G_OS_WIN32 */

	xmlFreeDoc (doc);
	xmlCleanupParser ();
	xmlMemoryDump ();

	g_free (xml_file);

	g_list_foreach (accounts, (GFunc) g_object_unref, NULL);
	g_list_free (accounts);

	return TRUE;
}

gboolean
gossip_account_manager_set_unique_name (GossipAccountManager *manager,
					GossipAccount        *account)
{
	GossipAccountManagerPriv *priv;
	GList                    *l;
	gchar                    *new_name;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), FALSE);

	priv = GET_PRIV (manager);

	new_name = g_strdup (gossip_account_get_name (account));
	l = priv->accounts;
	while (l) {
		if (l->data != account &&
		    strcmp (new_name, gossip_account_get_name (l->data)) == 0) {
			gchar *str;

			str = g_strdup_printf ("%s_", new_name);
			g_free (new_name);
			new_name = str;

			l = priv->accounts;
			continue;
		}
		l = l->next;
	}
	gossip_account_set_name (account, new_name);
	g_free (new_name);

	return TRUE;
}

