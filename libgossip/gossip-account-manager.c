/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio AB
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
#include <libxml/xmlreader.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "libgossip-marshal.h"

#include "gossip-account-manager.h"

#define ACCOUNTS_XML_FILENAME "accounts.xml"
#define ACCOUNTS_DTD_FILENAME "gossip-account.dtd"

#define d(x) x

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_ACCOUNT_MANAGER, GossipAccountManagerPriv))


typedef struct _GossipAccountManagerPriv GossipAccountManagerPriv;


struct _GossipAccountManagerPriv {
	GHashTable *accounts;

	gchar      *accounts_file_name;

	gchar      *default_name;
	gchar      *default_name_override;
};


enum {
	ACCOUNT_ADDED,
	ACCOUNT_REMOVED, 
	ACCOUNT_CHANGED,
	NEW_DEFAULT,
	LAST_SIGNAL
};


static void     account_manager_finalize             (GObject               *object);
static void     account_manager_get_accounts_foreach (const gchar           *name,
						      GossipAccount         *account,
						      GList                **list);
static gboolean account_manager_get_all              (GossipAccountManager  *manager);
static gboolean account_manager_file_validate        (GossipAccountManager  *manager,
						      const gchar           *filename);
static gboolean account_manager_file_parse           (GossipAccountManager  *manager,
						      const gchar           *filename);
static gboolean account_manager_file_save            (GossipAccountManager  *manager);
static gboolean account_manager_check_exists         (GossipAccountManager  *manager,
						      GossipAccount         *account);


static guint  signals[LAST_SIGNAL] = {0};

static GList *accounts = NULL;


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
	GossipAccountManagerPriv *priv;

	priv = GET_PRIV (manager);

	priv->accounts = g_hash_table_new_full (g_str_hash, 
						g_str_equal,
						g_free,
						g_object_unref);
}

static void
account_manager_finalize (GObject *object)
{
	GossipAccountManagerPriv *priv;
	
	priv = GET_PRIV (object);

	g_hash_table_destroy (priv->accounts);

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
 	if (!account_manager_check_exists (manager, account)) { 
		const gchar *name;

		d(g_print ("Account Manager: Adding account with name:'%s'\n", 
			   gossip_account_get_name (account)));
		
		name = gossip_account_get_name (account);
		g_hash_table_insert (priv->accounts, 
				     g_strdup (name),
				     g_object_ref (account));

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

 	d(g_print ("Account Manager: Removing account with name:'%s'\n",  
 		   gossip_account_get_name (account)));

	g_hash_table_remove (priv->accounts, 
			     gossip_account_get_name (account));

	g_signal_emit (manager, signals[ACCOUNT_REMOVED], 0, account);
}

GList *
gossip_account_manager_get_accounts (GossipAccountManager *manager)
{
	GossipAccountManagerPriv *priv;
	GList                    *list = NULL;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), NULL);

	priv = GET_PRIV (manager);

	g_hash_table_foreach (priv->accounts, 
			      (GHFunc)account_manager_get_accounts_foreach,
			      &list);

	return list;
}

static void
account_manager_get_accounts_foreach (const gchar   *name,
				      GossipAccount *account,
				      GList         **list)
{
	if (list) {
		*list = g_list_append (*list, account);
	}
}

guint 
gossip_account_manager_get_count (GossipAccountManager *manager)
{
	GossipAccountManagerPriv *priv;
	
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), 0);
	
	priv = GET_PRIV (manager);
	
	return g_hash_table_size (priv->accounts);
}


GossipAccount *
gossip_account_manager_find (GossipAccountManager *manager,
			     const gchar          *name)
{
	GossipAccountManagerPriv *priv;
	
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	priv = GET_PRIV (manager);

	return g_hash_table_lookup (priv->accounts, name);
}	

void
gossip_account_manager_set_overridden_default (GossipAccountManager *manager,
					       const gchar          *name)
{
	GossipAccountManagerPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager));
	g_return_if_fail (name != NULL);

	priv = GET_PRIV (manager);

 	d(g_print ("Account Manager: Setting overriding default account with name:'%s'\n",  
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
	
 	d(g_print ("Account Manager: Setting default account with name:'%s'\n",  
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
		if (g_list_length (accounts) >= 1) {
			GossipAccount *account;
			account = g_list_nth_data (accounts, 0);
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

 	d(g_print ("Account Manager: Saving accounts\n"));  
	
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
	gboolean                  ok = TRUE;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), FALSE);

	priv = GET_PRIV (manager);

	/* use default if no file specified */
	if (!priv->accounts_file_name) {
		dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
		if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
			mkdir (dir, S_IRUSR | S_IWUSR | S_IXUSR);
		}

		file_with_path = g_build_filename (dir, ACCOUNTS_XML_FILENAME, NULL);
		g_free (dir);
	}

	/* read file in */
	if ((ok &= g_file_test (file_with_path, G_FILE_TEST_EXISTS))) {
		if ((ok &= account_manager_file_validate (manager, file_with_path))) {;
			ok &= account_manager_file_parse (manager, file_with_path);
		}
	}
	
	g_free (file_with_path);

	return ok;
 }

static gboolean
account_manager_file_validate (GossipAccountManager *manager,
			       const char           *filename)
{
	xmlParserCtxtPtr ctxt;
	xmlDocPtr        doc; 
	gboolean         success = FALSE;
	
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);
	
 	d(g_print ("Account Manager: Attempting to validate file (against DTD):'%s'\n",  
 		   filename)); 
	
	/* create a parser context */
	ctxt = xmlNewParserCtxt ();
	if (ctxt == NULL) {
		g_warning ("Failed to allocate parser context for file:'%s'", 
			   filename);
		return FALSE;
	}
	
	/* parse the file, activating the DTD validation option */
	doc = xmlCtxtReadFile (ctxt, filename, NULL, XML_PARSE_DTDVALID);
	
	/* check if parsing suceeded */
	if (doc == NULL) {
		g_warning ("Failed to parse file:'%s'", 
			   filename);
	} else {
		/* check if validation suceeded */
		if (ctxt->valid == 0) {
			g_warning ("Failed to validate file:'%s'", 
				   filename);
		} else {
			success = TRUE;
		}
		
		/* free up the resulting document */
		xmlFreeDoc(doc);
	} 
	
	/* free up the parser context */
	xmlFreeParserCtxt(ctxt);
	
	return success;
 }
 
static gboolean
account_manager_file_parse (GossipAccountManager *manager, 
			    const gchar          *filename) 
{
	GossipAccountManagerPriv *priv;
	xmlDocPtr                 doc;
	xmlTextReaderPtr          reader;
	int                       ret;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	priv = GET_PRIV (manager);
	
	d(g_print ("Account Manager: Attempting to parse file:'%s'...\n", filename));
	
	reader = xmlReaderForFile (filename, NULL, 0);
	if (reader == NULL) {
		g_warning ("could not create xml reader for file:'%s' filename",
			   filename);
		return FALSE;
	}
	
        if (xmlTextReaderPreservePattern (reader, (xmlChar*) "preserved", NULL) < 0) {
		g_warning ("could not preserve pattern for file:'%s' filename",
			   filename);
		return FALSE;
	}

	ret = xmlTextReaderRead (reader);
 	
	while (ret == 1) {
		const xmlChar *node = NULL;

		if (!(node = xmlTextReaderConstName (reader))) {
			continue;
		}

		if (xmlStrcmp (node, BAD_CAST "default") == 0) {
			xmlChar *value;
			
			value = xmlTextReaderReadString (reader);
			if (value && xmlStrlen (value) > 0) {
				priv->default_name = g_strdup ((gchar*)value);
			}
		}

		if (xmlStrcmp (node, BAD_CAST "account") == 0) {
			xmlChar       *node_type = NULL;
			xmlChar       *node_name = NULL;
			xmlChar       *node_id = NULL;
			xmlChar       *node_password = NULL;
			xmlChar       *node_server = NULL;
			xmlChar       *node_port = NULL;
			xmlChar       *node_auto_connect = NULL;
			xmlChar       *node_use_ssl = NULL;
			xmlChar       *node_use_proxy = NULL;

			const xmlChar *key = NULL;
			xmlChar       *value;

			/* get all elements */

			ret = xmlTextReaderRead (reader);
 			key = xmlTextReaderConstName (reader); 
 			value = xmlTextReaderReadString (reader); 

			while (key && xmlStrcmp (key, BAD_CAST "account") != 0 && ret == 1) {
				if (key && 
				    value && 
				    xmlStrlen (key) > 0 &&
				    xmlStrlen (value) > 0) {
					if (xmlStrcmp (key, BAD_CAST "type") == 0) {
						node_type = value;
					} else if (xmlStrcmp (key, BAD_CAST "name") == 0) {
						node_name = value;
					} else if (xmlStrcmp (key, BAD_CAST "id") == 0) {
						node_id = value;
					} else if (xmlStrcmp (key, BAD_CAST "password") == 0) {
						node_password = value;
					} else if (xmlStrcmp (key, BAD_CAST "server") == 0) {
						node_server = value;
					} else if (xmlStrcmp (key, BAD_CAST "port") == 0) {
						node_port = value;
					} else if (xmlStrcmp (key, BAD_CAST "auto_connect") == 0) {
						node_auto_connect = value;
					} else if (xmlStrcmp (key, BAD_CAST "use_ssl") == 0) {
						node_use_ssl = value;
					} else if (xmlStrcmp (key, BAD_CAST "use_proxy") == 0) {
						node_use_proxy = value;
					} 
				}
					
				ret = xmlTextReaderRead (reader);
				key = xmlTextReaderConstName (reader);
				value = xmlTextReaderReadString (reader);
			}

			if (node_name && node_id && node_server) {
				GossipAccount     *account;
				GossipAccountType  type = GOSSIP_ACCOUNT_TYPE_JABBER;
				guint16            port;
				gboolean           auto_connect, use_ssl, use_proxy;
	
				if (node_type && xmlStrcmp (node_type, BAD_CAST "jabber") == 0) {
					type = GOSSIP_ACCOUNT_TYPE_JABBER;
				}

				port = (node_port ? atoi ((char*)node_port) : 5222);

				auto_connect = (xmlStrcasecmp (node_auto_connect, BAD_CAST "yes") == 0 ? TRUE : FALSE);
				use_ssl = (xmlStrcasecmp (node_use_ssl, BAD_CAST "yes") == 0 ? TRUE : FALSE);
				use_proxy = (xmlStrcasecmp (node_use_proxy, BAD_CAST "yes") == 0 ? TRUE : FALSE);
	
				account = g_object_new (GOSSIP_TYPE_ACCOUNT, 
							"type", type,
							"name", (gchar*)node_name,
							"id", (gchar*)node_id,
							"port", port,
							"auto_connect", auto_connect,
							"use_ssl", use_ssl,
							"use_proxy", use_proxy,
							NULL);
	
				if (node_server) {
					gossip_account_set_server (account, (gchar*)node_server);
				}

				if (node_password) {
					gossip_account_set_password (account, (gchar*)node_password);
				}

				gossip_account_manager_add (manager, account);

				g_object_unref (account);
			}

			xmlFree (node_type);
			xmlFree (node_name);
			xmlFree (node_id);
			xmlFree (node_password);
			xmlFree (node_server);
			xmlFree (node_port);
			xmlFree (node_auto_connect);
			xmlFree (node_use_ssl);
			xmlFree (node_use_proxy);
		}

		ret = xmlTextReaderRead (reader);
	}
	
	if (ret != 0) {
		g_warning ("Could not parse file:'%s' filename",
			   filename);
		xmlFreeTextReader(reader);
		return FALSE;
	}

	d(g_print ("Account Manager: Parsed %d accounts\n", 
		   g_hash_table_size (priv->accounts)));
	
	d(g_print ("Account Manager: Cleaning up parser for file:'%s'\n\n", 
		   filename));
	
	doc = xmlTextReaderCurrentDoc(reader);
	xmlFreeDoc(doc);
	
	xmlCleanupParser();
	xmlFreeTextReader(reader);
	
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
					
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), FALSE);

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

	doc = xmlNewDoc (BAD_CAST "1.0");
	root = xmlNewNode (NULL, BAD_CAST "accounts");
	xmlDocSetRootElement (doc, root);

	dtd = xmlCreateIntSubset (doc, BAD_CAST "accounts", NULL, BAD_CAST dtd_file);

	if (!priv->default_name) {
		priv->default_name = g_strdup ("Default");
	}
	
	xmlNewChild (root, NULL, 
		     BAD_CAST "default", 
		     BAD_CAST priv->default_name);

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

		node = xmlNewChild (root, NULL, BAD_CAST "account", NULL);
		xmlNewChild (node, NULL, BAD_CAST "type", BAD_CAST type);
		xmlNewChild (node, NULL, BAD_CAST "name", BAD_CAST gossip_account_get_name (account));
		xmlNewChild (node, NULL, BAD_CAST "id", BAD_CAST gossip_account_get_id (account));

		xmlNewChild (node, NULL, BAD_CAST "password", BAD_CAST gossip_account_get_password (account));
		xmlNewChild (node, NULL, BAD_CAST "server", BAD_CAST gossip_account_get_server (account));
		xmlNewChild (node, NULL, BAD_CAST "port", BAD_CAST port);

		xmlNewChild (node, NULL, BAD_CAST "auto_connect", BAD_CAST (gossip_account_get_auto_connect (account) ? "yes" : "no"));
		xmlNewChild (node, NULL, BAD_CAST "use_ssl", BAD_CAST (gossip_account_get_use_ssl (account) ? "yes" : "no"));
		xmlNewChild (node, NULL, BAD_CAST "use_proxy", BAD_CAST (gossip_account_get_use_proxy (account) ? "yes" : "no"));

		g_free (type);
		g_free (port);
	}

	d(g_print ("Account Manager: Saving file:'%s'\n", xml_file));
	xmlSaveFormatFileEnc (xml_file, doc, "utf-8", 1);
	xmlFreeDoc (doc);

	xmlCleanupParser ();

	xmlMemoryDump ();
	
	g_free (xml_file);

	return TRUE;
}

static gboolean 
account_manager_check_exists (GossipAccountManager *manager,
			      GossipAccount        *account)
{
	GList *l;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), FALSE);

	for (l = accounts; l; l = l->next) {
		GossipAccount *this_account = l->data;

		if (gossip_account_equal (this_account, account)) {
			return TRUE;
		}
	}

	return FALSE;
 }

