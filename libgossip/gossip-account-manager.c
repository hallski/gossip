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
 *
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "gossip-debug.h"
#include "gossip-protocol.h"
#include "gossip-account-manager.h"
#include "gossip-private.h"
#include "gossip-utils.h"

#include "libgossip-marshal.h"

#define DEBUG_DOMAIN "AccountManager"

/* For splitting an id of user@server/resource to just user@server with resource
 * in it's own xml tags (for Gossip release 0.10.2).
 */
#define RESOURCE_HACK

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

#ifdef RESOURCE_HACK
static gboolean need_saving = FALSE;
#endif

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
	GossipAccountType         type;

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

	type = gossip_account_get_type (account);
	name = gossip_account_get_name (account);
	gossip_debug (DEBUG_DOMAIN, "Adding %s account with name:'%s'",
		      gossip_account_type_to_string (type),
		      name);


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
				   const gchar          *id,
				   const gchar          *type_str)
{
	GossipAccountManagerPriv *priv;
	GList                    *l;
	GossipAccountType         type = GOSSIP_ACCOUNT_TYPE_COUNT;

	/* WARNING: This function is used for compatibility with old log format,
	 *          it shouldn't be used for new code.
	 */

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), NULL);
	g_return_val_if_fail (id != NULL || type_str != NULL, NULL);

	priv = GET_PRIV (manager);

	if (type_str) {
		type = gossip_account_string_to_type (type_str);
	}
	for (l = priv->accounts; l; l = l->next) {
		GossipAccount     *account;
		GossipAccountType  account_type;
		const gchar       *account_param = NULL;

		account = l->data;

		account_type = gossip_account_get_type (account);
		if (gossip_account_has_param (account, "account")) {
			gossip_account_get_param (account, "account", &account_param, NULL);
		}

		if (type != GOSSIP_ACCOUNT_TYPE_COUNT && type != account_type) {
			continue;
		}

		if (id && account_param && strcmp (id, account_param) == 0) {
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
	GossipProtocol    *protocol;
	GossipAccount     *account;
	GossipAccountType  account_type;
	xmlNodePtr         child;
	gchar             *str;

	str = xmlGetProp (node, "type");

	/* Old config file, need saving in new format */
	if (str) {
		account_type = gossip_account_string_to_type (str);
	} else {
		account_type = GOSSIP_ACCOUNT_TYPE_JABBER_LEGACY;
		need_saving = TRUE;
	}

	if (account_type == GOSSIP_ACCOUNT_TYPE_UNKNOWN) {
		return;
	}

	xmlFree (str);

	protocol = gossip_protocol_new_from_account_type (account_type);
	if (!protocol) {
		/* Maybe this Gossip setup doesn't support this account type */
		gossip_debug (DEBUG_DOMAIN, 
	                      "Protocol couldn't be created from account type:%d->'%s'",
		              account_type, gossip_account_type_to_string (account_type));
		return;
	}

	account = gossip_protocol_new_account (protocol, account_type);
	g_object_unref (protocol);

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
		}
		else if (strcmp (tag, "auto_connect") == 0) {
			gossip_account_set_auto_connect (account, strcmp (str, "yes") == 0);
		}
		else if (strcmp (tag, "use_proxy") == 0) {
			gossip_account_set_use_proxy (account, strcmp (str, "yes") == 0);
		}
		else if (strcmp (tag, "parameter") == 0) {
			gchar  *param_name;
			gchar  *type_str;
			GType   type;
			GValue *g_value;

			param_name = xmlGetProp (child, "name");
			type_str = xmlGetProp (child, "type");

			type = gossip_dbus_type_to_g_type (type_str);
			g_value = gossip_string_to_g_value (str, type);

			/* Compatibility: "id" param is now renamed
			 * to "account". */
			if (strcmp (param_name, "id") != 0) {
				gossip_account_set_param_g_value (account,
								  param_name,
								  g_value);			
			} else {
				gossip_account_set_param_g_value (account,
								  "account",
								  g_value);			
				need_saving = TRUE;
			}

			g_value_unset (g_value);
			g_free (g_value);
			xmlFree (param_name);
			xmlFree (type_str);
		}
		/* Those are deprecated and kept for compatibility only */
		else if (strcmp (tag, "resource") == 0) {
			gossip_account_set_param (account, "resource", str, NULL);
			need_saving = TRUE;
		}
		else if (strcmp (tag, "password") == 0) {
			gossip_account_set_param (account, "password", str, NULL);
			need_saving = TRUE;
		}
		else if (strcmp (tag, "server") == 0) {
			gossip_account_set_param (account, "server", str, NULL);
			need_saving = TRUE;
		}
		else if (strcmp (tag, "port") == 0) {
			guint tmp_port;

			tmp_port = atoi (str);
			if (tmp_port != 0) {
				gossip_account_set_param (account, "port",
							  tmp_port, NULL);
			}
			need_saving = TRUE;
		}
		else if (strcmp (tag, "use_ssl") == 0) {
			gossip_account_set_param (account, "use_ssl",
						  strcmp (str, "yes") == 0,
						  NULL);
			need_saving = TRUE;
		}
		else if (strcmp (tag, "id") == 0) {
			gossip_account_set_param (account, "account", str, NULL);
			need_saving = TRUE;
		}

		xmlFree (str);

		child = child->next;
	}
#ifdef RESOURCE_HACK
	const gchar *resource_found = NULL;
	const gchar *account_param = NULL;

	if (gossip_account_has_param (account, "account")) {
		gossip_account_get_param (account, "account", &account_param, NULL);
	}
	str = g_strdup (account_param);
	if (str) {
		/* FIXME: This hack is so we don't get bug
		 * reports and basically gets the resource
		 * from the id.
		 */
		const gchar *ch;

		ch = strchr (str, '/');
		if (ch) {
			resource_found = (ch + 1);
			str[ch - str] = '\0';

			g_printerr ("Converting ID... (id:'%s', resource:'%s')\n",
				    str, resource_found);
		}
	}

	if (resource_found) {
		gossip_account_set_param (account,
					  "resource", resource_found,
					  "account", str,
					  NULL);
		need_saving = TRUE;
	}

	g_free (str);
#endif /* RESOURCE_HACK */

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

#ifdef RESOURCE_HACK
	if (need_saving) {
		g_printerr ("Saving accounts... \n");
		account_manager_file_save (manager);
	}
#endif

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
	GList                    *accounts, *params;
	GList                    *l1, *l2;
	gchar                    *xml_dir;
	gchar                    *xml_file;
	mode_t                    old_mask;

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

	xmlNewChild (root, NULL,
		    "default",
		    priv->default_name);

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
	for (l1 = accounts; l1; l1 = l1->next) {
		GossipAccount *account;
		const gchar   *type;
		xmlNodePtr     node;

		account = l1->data;

		type = gossip_account_type_to_string (gossip_account_get_type (account));

		node = xmlNewChild (root, NULL, "account", NULL);

		xmlNewProp (node, "type", type);

		xmlNewTextChild (node, NULL, "name", gossip_account_get_name (account));
		xmlNewTextChild (node, NULL, "auto_connect", gossip_account_get_auto_connect (account) ? "yes" : "no");
		xmlNewTextChild (node, NULL, "use_proxy", gossip_account_get_use_proxy (account) ? "yes" : "no");

		params = gossip_account_get_param_all (account);
		for (l2 = params; l2; l2 = l2->next) {
			GossipAccountParam *param;
			gchar              *param_name;

			param_name = l2->data;
			param = gossip_account_get_param_param (account, param_name);
			
			if (param->modified) {
				gchar       *value_str;
				const gchar *type_str;
				xmlNodePtr   child;

				value_str = gossip_g_value_to_string (&param->g_value);
				type_str = gossip_g_type_to_dbus_type (G_VALUE_TYPE (&param->g_value));
				
				child = xmlNewTextChild (node, NULL, "parameter", value_str);
				xmlNewProp (child, "name", param_name);
				xmlNewProp (child, "type", type_str);
				
				g_free (value_str);
			}
			
			g_free (param_name);
		}

		g_list_free (params);
	}

	gossip_debug (DEBUG_DOMAIN, "Saving file:'%s'", xml_file);

	/* Set the umask to get the proper permissions when libxml saves the
	 * file, but also change the permissions expiicitly in case the file
	 * already exists.
	 */
	old_mask = umask (077);
	chmod (xml_file, 0600);

	/* Make sure the XML is indented properly */
	xmlIndentTreeOutput = 1;

	xmlSaveFormatFileEnc (xml_file, doc, "utf-8", 1);

	/* Reset the umask */
	umask (old_mask);

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

