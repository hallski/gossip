/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2006 Imendio AB
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
#include <string.h>

#include <libgnomevfs/gnome-vfs.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "libgossip-marshal.h"
#include "gossip-utils.h"
#include "gossip-account-manager.h"

#define RESOURCE_HACK /* For splitting an id of user@server/resource
		       * to just user@server with resource in it's own
		       * xml tags (for Gossip release 0.10.2).
		       */

#define ACCOUNTS_XML_FILENAME "accounts.xml"
#define ACCOUNTS_DTD_FILENAME "gossip-account.dtd"

#define DEBUG_MSG(x) 
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n"); */

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

static void     account_manager_finalize   (GObject              *object);
static gboolean account_manager_get_all    (GossipAccountManager *manager);
static gboolean account_manager_file_parse (GossipAccountManager *manager,
					    const gchar          *filename);
static gboolean account_manager_file_save  (GossipAccountManager *manager);

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

	g_list_foreach (priv->accounts, (GFunc)g_object_unref, NULL);
	g_list_free (priv->accounts);

	g_free (priv->accounts_file_name);

	g_free (priv->default_name);
	g_free (priv->default_name_override);
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

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), FALSE);
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);

	priv = GET_PRIV (manager);

	/* don't add more than once */
 	if (!gossip_account_manager_find (manager, gossip_account_get_name (account))) { 
		const gchar       *name;
		GossipAccountType  type;

		type = gossip_account_get_type (account);
		name = gossip_account_get_name (account);

		DEBUG_MSG (("Account Manager: Adding %s account with name:'%s'", 
			   gossip_account_get_type_as_str (type), 
			   name));
		
		priv->accounts = g_list_append (priv->accounts, g_object_ref (account));

		g_signal_emit (manager, signals[ACCOUNT_ADDED], 0, account);

		return TRUE;
	} 

	return FALSE;
}

void          
gossip_account_manager_remove (GossipAccountManager *manager,
			       GossipAccount        *account)
{
	GossipAccountManagerPriv *priv;
	
	g_return_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GET_PRIV (manager);

 	DEBUG_MSG (("Account Manager: Removing account with name:'%s'",  
 		   gossip_account_get_name (account)));

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
	g_list_foreach (accounts, (GFunc)g_object_ref, NULL);

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

void
gossip_account_manager_set_overridden_default (GossipAccountManager *manager,
					       const gchar          *name)
{
	GossipAccountManagerPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager));
	g_return_if_fail (name != NULL);

	priv = GET_PRIV (manager);

 	DEBUG_MSG (("Account Manager: Setting overriding default account with name:'%s'",  
 		   name)); 

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
	
 	DEBUG_MSG (("Account Manager: Setting default account with name:'%s'",  
 		   gossip_account_get_name (account))); 

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
			GList         *l;

			l = gossip_account_manager_get_accounts (manager);
			account = g_list_nth_data (l, 0);
			g_list_free (l);

			name = gossip_account_get_name (account);
			gossip_account_manager_set_default (manager, account);

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

 	DEBUG_MSG (("Account Manager: Saving accounts"));  
	
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
			mkdir (dir, S_IRUSR | S_IWUSR | S_IXUSR);
		}
		
		file_with_path = g_build_filename (dir, ACCOUNTS_XML_FILENAME, NULL);
		g_free (dir);
	} else {
		file_with_path = g_strdup (priv->accounts_file_name);
	}

	/* read file in */
	if (g_file_test (file_with_path, G_FILE_TEST_EXISTS) &&
	    !account_manager_file_parse (manager, file_with_path)) {
		g_free (file_with_path);
		return FALSE;
	}
	
	g_free (file_with_path);

	return TRUE;
}

static void
account_manager_parse_account (GossipAccountManager *manager,
			       xmlNodePtr            node)
{
	GossipAccount     *account;
	xmlNodePtr         child;
	gchar             *str;
	GossipAccountType  type;
	gchar             *name, *id, *password;
	gchar             *server, *resource;
	guint16            port;
	gboolean           auto_connect, use_ssl, use_proxy;

	/* Default values. */
	type = GOSSIP_ACCOUNT_TYPE_JABBER;
	name = NULL;
	id = NULL;
	password = NULL;
	resource = NULL;
	server = NULL;
	port = 5222;
	auto_connect = TRUE;
	use_ssl = FALSE;
	use_proxy = FALSE;

	child = node->children;
	while (child) {
		gchar *tag;

		tag = (gchar *) child->name;
		str = (gchar *) xmlNodeGetContent (child);

		if (strcmp (tag, "type") == 0) {
			if (strcmp (str, "jabber") == 0) {
				type = GOSSIP_ACCOUNT_TYPE_JABBER;
				xmlFree (str);
			}
		}
		else if (strcmp (tag, "name") == 0) {
			name = str;
		}
		else if (strcmp (tag, "id") == 0) {
			id = str;
		}
		else if (strcmp (tag, "password") == 0) {
			password = str;
		}
		else if (strcmp (tag, "server") == 0) {
			server = str;
		}
		else if (strcmp (tag, "resource") == 0) {
			resource = str;
		}
		else if (strcmp (tag, "port") == 0) {
			guint tmp_port;
			
			tmp_port = atoi (str);
			if (tmp_port != 0) {
				port = tmp_port;
			}
			xmlFree (str);
		}
		else if (strcmp (tag, "auto_connect") == 0) {
			if (strcmp (str, "yes") == 0) {
				auto_connect = TRUE;
			} else {
				auto_connect = FALSE;
			}
			xmlFree (str);
		}
		else if (strcmp (tag, "use_ssl") == 0) {
			if (strcmp (str, "yes") == 0) {
				use_ssl = TRUE;
			} else {
				use_ssl = FALSE;
			}
			xmlFree (str);
		}
		else if (strcmp (tag, "use_proxy") == 0) {
			if (strcmp (str, "yes") == 0) {
				use_proxy = TRUE;
			} else {
				use_proxy = FALSE;
			}
			xmlFree (str);
		}

		child = child->next;
	}

	if (name && id) {
		account = g_object_new (GOSSIP_TYPE_ACCOUNT, 
					"type", type,
					"name", name,
					"id", id,
					"port", port,
					"auto_connect", auto_connect,
					"use_ssl", use_ssl,
					"use_proxy", use_proxy,
					NULL);
		
		if (resource) {
			gossip_account_set_resource (account, resource);
		}

		if (server) {
			gossip_account_set_server (account, server);
		}
		
		if (password) {
			gossip_account_set_password (account, password);
		}
		
#ifdef RESOURCE_HACK
		const gchar *resource_found = NULL;

		g_printerr ("Checking account id:'%s' is in the correct form since it "
			    "has changed this version...\n", id);

		if (id) {
			/* FIXME: This hack is so we don't get bug
			 * reports and basically gets the resource
			 * from the id. */
			const gchar *ch;
			
			ch = strchr (id, '/');
			if (ch) {
				resource_found = (ch + 1);
				id[ch - id] = '\0';

				g_printerr ("\tConverting ID... "
					    "(id:'%s', resource:'%s')\n", id, resource_found);
			}
		}

		if (resource_found) {
			gossip_account_set_id (account, id);
			gossip_account_set_resource (account, resource_found);

			g_printerr ("\tSaving accounts... \n");
			account_manager_file_save (manager);
		}
#endif /* RESOURCE_HACK */

		gossip_account_manager_add (manager, account);
		
		g_object_unref (account);
	}
	
	xmlFree (name);
	xmlFree (id);
	xmlFree (resource);
	xmlFree (server);
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
	
	DEBUG_MSG (("Account Manager: Attempting to parse file:'%s'...", filename));

 	ctxt = xmlNewParserCtxt ();

	/* Parse and validate the file. */
	doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);
	if (!doc) {
		g_warning ("Failed to parse file:'%s'", filename);
		xmlFreeParserCtxt (ctxt);
		return FALSE;
	}

	if (!gossip_utils_xml_validate (doc, ACCOUNTS_DTD_FILENAME)) {
		g_warning ("Failed to validate file:'%s'", filename);
		xmlFreeDoc(doc);
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
	
	DEBUG_MSG (("Account Manager: Parsed %d accounts", 
		   g_list_length (priv->accounts)));

	DEBUG_MSG (("Account Manager: Default account is:'%s'", 
		   priv->default_name));
	
	xmlFreeDoc(doc);
	xmlFreeParserCtxt (ctxt);
	
	return TRUE;
}

static gboolean
account_manager_file_save (GossipAccountManager *manager)
{
	GossipAccountManagerPriv *priv;
	xmlDocPtr                 doc;  
	xmlDtdPtr                 dtd;  
	xmlNodePtr                root;
	GList                    *accounts;
	GList                    *l;
	gchar                    *dtd_file;
	gchar                    *xml_dir;
	gchar                    *xml_file;
					
	priv = GET_PRIV (manager);
	
	if (priv->accounts_file_name) {
		xml_file = g_strdup (priv->accounts_file_name);
	} else {
		xml_dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
		if (!g_file_test (xml_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
			mkdir (xml_dir, S_IRUSR | S_IWUSR | S_IXUSR);
		}
					 
		xml_file = g_build_filename (xml_dir, ACCOUNTS_XML_FILENAME, NULL);
		g_free (xml_dir);
	}

	dtd_file = g_build_filename (DTDDIR, ACCOUNTS_DTD_FILENAME, NULL);

	doc = xmlNewDoc ("1.0");
	root = xmlNewNode (NULL, "accounts");
	xmlDocSetRootElement (doc, root);

	dtd = xmlCreateIntSubset (doc,"accounts", NULL,dtd_file);

	if (!priv->default_name) {
		priv->default_name = g_strdup ("Default");
	}
	
	xmlNewChild (root, NULL, 
		    "default", 
		    priv->default_name);

	accounts = gossip_account_manager_get_accounts (manager);

	for (l = accounts; l; l = l->next) {
		GossipAccount *account;
		gchar         *type, *port;
		xmlNodePtr     node;
	
		account = l->data;

		switch (gossip_account_get_type (account)) {
		case GOSSIP_ACCOUNT_TYPE_JABBER:
		default:
			type = g_strdup_printf ("jabber");
			break;
		}
	
		port = g_strdup_printf ("%d", gossip_account_get_port (account));

		node = xmlNewChild (root, NULL, "account", NULL);
		xmlNewChild (node, NULL, "type", type);
		xmlNewTextChild (node, NULL, "name", gossip_account_get_name (account));
		xmlNewTextChild (node, NULL, "id", gossip_account_get_id (account));

		xmlNewTextChild (node, NULL, "password", gossip_account_get_password (account));
		xmlNewTextChild (node, NULL, "resource", gossip_account_get_resource (account));
		xmlNewTextChild (node, NULL, "server", gossip_account_get_server (account));
		xmlNewChild (node, NULL, "port", port);

		xmlNewChild (node, NULL, "auto_connect", gossip_account_get_auto_connect (account) ? "yes" : "no");
		xmlNewChild (node, NULL, "use_ssl", gossip_account_get_use_ssl (account) ? "yes" : "no");
		xmlNewChild (node, NULL, "use_proxy", gossip_account_get_use_proxy (account) ? "yes" : "no");

		g_free (type);
		g_free (port);
	}

	DEBUG_MSG (("Account Manager: Saving file:'%s'", xml_file));
	xmlSaveFormatFileEnc (xml_file, doc, "utf-8", 1);
	xmlFreeDoc (doc);

	xmlCleanupParser ();

	xmlMemoryDump ();
	
	g_free (xml_file);

	return TRUE;
}


