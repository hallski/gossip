/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Martyn Russell <mr@gnome.org>
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

#include <glib.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libxml/xmlreader.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "gossip-account.h"

#define ACCOUNTS_XML_FILENAME "accounts.xml"
#define ACCOUNTS_DTD_FILENAME "gossip-account.dtd"

#define GOSSIP_ACCOUNT_GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_ACCOUNT, GossipAccountPriv))

#define d(x)


typedef struct _GossipAccountPriv GossipAccountPriv;

struct _GossipAccountPriv {
	GossipAccountType  type;

	gchar             *name;
	gchar             *id;
	gchar             *host;
	gchar             *password;
	gchar             *server;
	guint16            port;
	gboolean           auto_connect;
	gboolean           use_ssl;
	gboolean           use_proxy;
};


static void     account_class_init     (GossipAccountClass *class);
static void     account_init           (GossipAccount      *account);
static void     account_finalize       (GObject            *object);
static void     account_get_property   (GObject            *object,
					guint               param_id,
					GValue             *value,
					GParamSpec         *pspec);
static void     account_set_property   (GObject            *object,
					guint               param_id,
					const GValue       *value,
					GParamSpec         *pspec);
static gboolean accounts_file_parse    (const gchar        *filename);
static gboolean accounts_file_validate (const gchar        *filename);
static gboolean accounts_file_save     (void);
static gboolean accounts_check_exists  (GossipAccount      *account);


enum {
	PROP_0,
	PROP_TYPE,
	PROP_NAME,
	PROP_ID,
	PROP_PASSWORD,
	PROP_SERVER,
	PROP_PORT,
	PROP_AUTO_CONNECT,
	PROP_USE_SSL,
	PROP_USE_PROXY
};


static GList    *accounts = NULL;

static gchar    *accounts_file_name = NULL;
static gchar    *default_name = NULL;
static gchar    *default_name_override = NULL;

static gpointer  parent_class = NULL;


GType
gossip_account_get_gtype (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GossipAccountClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) account_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GossipAccount),
			0,    /* n_preallocs */
			(GInstanceInitFunc) account_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "GossipAccount",
					       &info, 0);
	}

	return type;

}

static void
account_class_init (GossipAccountClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	object_class->finalize     = account_finalize;
	object_class->get_property = account_get_property;
	object_class->set_property = account_set_property;

	g_object_class_install_property (object_class,
					 PROP_TYPE,
					 g_param_spec_int ("type",
							   "Account Type",
							   "The account protocol type, e.g. MSN",
							   G_MININT,
							   G_MAXINT,
							   GOSSIP_ACCOUNT_TYPE_JABBER,
							   G_PARAM_READWRITE));


	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Account Name",
							      "What you call this account",
							      "Default",
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_ID,
					 g_param_spec_string ("id",
							      "Account ID",
							      "For example someone@jabber.org",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_PASSWORD,
					 g_param_spec_string ("password",
							      "Account Password",
							      "Authentication token",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_SERVER,
					 g_param_spec_string ("server",
							      "Account Server",
							      "Machine to connect to",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_PORT,
					 g_param_spec_int ("port",
							   "Account Port",
							   "Port used in the connection to Server",
							   0,
							   65535,
							   0,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_AUTO_CONNECT,
					 g_param_spec_boolean ("auto_connect",
							       "Account Auto Connect",
							       "Connect on startup",
							       TRUE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_USE_SSL,
					 g_param_spec_boolean ("use_ssl",
							       "Account Uses SSL",
							       "Identifies if the connection uses secure methods",
							       FALSE,
							       G_PARAM_READWRITE));
	
	g_object_class_install_property (object_class,
					 PROP_USE_PROXY,
					 g_param_spec_boolean ("use_proxy",
							       "Account Uses Proxy",
							       "Identifies if the connection uses the environment proxy",
							       FALSE,
							       G_PARAM_READWRITE));
	
	
	g_type_class_add_private (object_class, sizeof (GossipAccountPriv));
}

static void
account_init (GossipAccount *account)
{
	GossipAccountPriv *priv;
	
	priv = GOSSIP_ACCOUNT_GET_PRIV (account);

	priv->type         = 0;
	priv->name         = NULL;
	priv->id           = NULL;
	priv->password     = NULL;
	priv->server       = NULL;
	priv->port         = 0;
	priv->auto_connect = TRUE;
	priv->use_ssl      = FALSE;
	priv->use_proxy    = FALSE;
}

static void
account_finalize (GObject *object)
{
	GossipAccountPriv *priv;
	
	priv = GOSSIP_ACCOUNT_GET_PRIV (object);
	
	g_free (priv->name);
	g_free (priv->id);
	g_free (priv->password);
	g_free (priv->server);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
account_get_property (GObject    *object,
		      guint       param_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	GossipAccountPriv *priv;
	
	priv = GOSSIP_ACCOUNT_GET_PRIV (object);

	switch (param_id) {
	case PROP_TYPE:
		g_value_set_int (value, priv->type);
		break;
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	case PROP_PASSWORD:
		g_value_set_string (value, priv->password);
		break;
	case PROP_SERVER:
		g_value_set_string (value, priv->server);
		break;
	case PROP_PORT:
		g_value_set_int (value, priv->port);
		break;
	case PROP_AUTO_CONNECT:
		g_value_set_boolean (value, priv->auto_connect);
		break;
	case PROP_USE_SSL:
		g_value_set_boolean (value, priv->use_ssl);
		break;
	case PROP_USE_PROXY:
		g_value_set_boolean (value, priv->use_proxy);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}
	
static void
account_set_property (GObject      *object,
		      guint         param_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	GossipAccountPriv *priv;
	
	priv = GOSSIP_ACCOUNT_GET_PRIV (object);
	
	switch (param_id) {
	case PROP_TYPE:
		priv->type = g_value_get_int (value);
		break;
	case PROP_NAME:
		gossip_account_set_name (GOSSIP_ACCOUNT (object),
					 g_value_get_string (value));
		break;
	case PROP_ID:
		gossip_account_set_id (GOSSIP_ACCOUNT (object),
				       g_value_get_string (value));
		break;
	case PROP_PASSWORD:
		gossip_account_set_password (GOSSIP_ACCOUNT (object),
					     g_value_get_string (value));
		break;
	case PROP_SERVER:
		gossip_account_set_server (GOSSIP_ACCOUNT (object),
					   g_value_get_string (value));
		break;
	case PROP_PORT:
		gossip_account_set_port (GOSSIP_ACCOUNT (object),
					 g_value_get_int (value));
		break;
	case PROP_AUTO_CONNECT:
		gossip_account_set_auto_connect (GOSSIP_ACCOUNT (object),
						 g_value_get_boolean (value));
		break;
	case PROP_USE_SSL:
		gossip_account_set_use_ssl (GOSSIP_ACCOUNT (object),
					    g_value_get_boolean (value));
		break;
	case PROP_USE_PROXY:
		gossip_account_set_use_proxy (GOSSIP_ACCOUNT (object),
					    g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

GossipAccountType
gossip_account_get_type (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), 0);
	
	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->type;
}

const gchar *
gossip_account_get_name (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->name;
}

const gchar *
gossip_account_get_id (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->id;
}

const gchar *
gossip_account_get_password (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->password;
}

const gchar *
gossip_account_get_server (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->server;
}

guint16
gossip_account_get_port (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), 0);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->port;
}

gboolean
gossip_account_get_auto_connect (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), TRUE);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->auto_connect;
}

gboolean
gossip_account_get_use_ssl (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->use_ssl;
}

gboolean
gossip_account_get_use_proxy (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->use_proxy;
}


void 
gossip_account_set_name (GossipAccount *account,
			 const gchar   *name)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
	g_return_if_fail (name != NULL);
	
	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	
	g_free (priv->name);
	priv->name = g_strdup (name);
}

void
gossip_account_set_id (GossipAccount *account,
		       const gchar   *id)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
	g_return_if_fail (id != NULL);
	
	priv = GOSSIP_ACCOUNT_GET_PRIV (account);

	g_free (priv->id);
	priv->id = g_strdup (id);
}

void
gossip_account_set_password (GossipAccount *account,
			     const gchar   *password)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
	g_return_if_fail (password != NULL);
	
	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	
	g_free (priv->password);
	priv->password = g_strdup (password);
}

void
gossip_account_set_server (GossipAccount *account,
			   const gchar   *server)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
	g_return_if_fail (server != NULL);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	
	g_free (priv->server);
	priv->server = g_strdup (server);
}

void
gossip_account_set_port (GossipAccount *account,
			 guint16        port)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	priv->port = port;
}

void
gossip_account_set_auto_connect (GossipAccount *account,
				 gboolean       auto_connect)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	priv->auto_connect = auto_connect;
}

void
gossip_account_set_use_ssl (GossipAccount *account,
 			    gboolean       use_ssl)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	priv->use_ssl = use_ssl;
}

void
gossip_account_set_use_proxy (GossipAccount *account,
			      gboolean       use_proxy)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	priv->use_proxy = use_proxy;
}


gboolean 
gossip_account_name_equals (GossipAccount *a, 
			    GossipAccount *b)
{
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (a), FALSE); 
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (b), FALSE); 

	g_return_val_if_fail (gossip_account_get_name (a) != NULL, FALSE); 
	g_return_val_if_fail (gossip_account_get_name (b) != NULL, FALSE); 

	/* FIXME: this can't be the best way? */
	return (strcmp (gossip_account_get_name (a), 
			gossip_account_get_name (b)) == 0);
}

/*
 * API to save accounts to file.
 */

const GList *
gossip_accounts_get_all (const gchar *filename)
{
	gchar *dir;
	gchar *file_with_path = NULL;

	g_free (accounts_file_name);
	accounts_file_name = NULL;

	if (filename) {
		gboolean file_exists; 

		file_exists = g_file_test (filename, G_FILE_TEST_EXISTS);
		g_return_val_if_fail (file_exists, NULL);

		file_with_path = g_strdup (filename);
		
		/* remember for saving */
		accounts_file_name = g_strdup (filename);
	}

	/* if already set up clean up first */
	if (accounts) {
		g_list_foreach (accounts, (GFunc)g_object_unref, NULL);
		g_list_free (accounts);
		accounts = NULL;
	}

	/* use default if no file specified */
	if (!file_with_path) {
		dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
		if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
			mkdir (dir, S_IRUSR | S_IWUSR | S_IXUSR);
		}

		file_with_path = g_build_filename (dir, ACCOUNTS_XML_FILENAME, NULL);
		g_free (dir);
	}

	/* read file in */
	if (g_file_test (file_with_path, G_FILE_TEST_EXISTS)) {
		if (accounts_file_validate (file_with_path)) {
			accounts_file_parse (file_with_path);
		}
	}
	
	g_free (file_with_path);

	return accounts;
}

static gboolean
accounts_file_validate (const char *filename)
{
	xmlParserCtxtPtr ctxt;
	xmlDocPtr        doc; 
	gboolean         success = FALSE;
	
	g_return_val_if_fail (filename != NULL, FALSE);
	
	d(g_print ("Attempting to validate file (against DTD):'%s'\n", 
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
accounts_file_parse (const gchar *filename) 
{
	xmlDocPtr         doc;
	xmlTextReaderPtr  reader;
	int               ret;

	g_return_val_if_fail (filename != NULL, FALSE);
	
	d(g_print ("Attempting to parse file:'%s'...\n", filename));
	
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
				default_name = g_strdup ((gchar*)value);
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
				if (key && value && 
				    xmlStrlen (key) > 0 && xmlStrlen (value) > 0) {
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
					} else if (xmlStrcmp (key, BAD_CAST "auto-connect") == 0) {
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
	
				if (node_type) {
					if (xmlStrcmp (node_type, BAD_CAST "jabber") == 0) {
						type = GOSSIP_ACCOUNT_TYPE_JABBER;
					}
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

				accounts = g_list_append (accounts, account);
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

	d(g_print ("Parsed %d accounts\n", g_list_length (accounts)));
	
	d(g_print ("Cleaning up parser for file:'%s'\n\n", filename));
	  
	doc = xmlTextReaderCurrentDoc(reader);
	xmlFreeDoc(doc);

	xmlCleanupParser();
	xmlFreeTextReader(reader);
	
	return TRUE;
}

static gboolean
accounts_file_save (void)
{
	xmlDocPtr      doc;  
	xmlDtdPtr      dtd;  
	xmlNodePtr     root;
	GList         *l;
	gchar         *dtd_file;
	gchar         *xml_dir;
	gchar         *xml_file;
					 
	if (accounts_file_name) {
		xml_file = g_strdup (accounts_file_name);
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

	if (!default_name) {
		default_name = g_strdup ("Default");
	}
	
	xmlNewChild (root, NULL, 
		     BAD_CAST "default", 
		     BAD_CAST default_name);

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

	d(g_print ("Saving file:'%s'\n", xml_file));
	xmlSaveFormatFileEnc (xml_file, doc, "utf-8", 1);
	xmlFreeDoc (doc);

	xmlCleanupParser ();

	xmlMemoryDump ();
	
	g_free (xml_file);

	return TRUE;
}

static gboolean 
accounts_check_exists (GossipAccount *account)
{
	GList *l;

	for (l = accounts; l; l = l->next) {
		GossipAccount *this_account = l->data;

		if (strcmp (gossip_account_get_name (this_account), 
			    gossip_account_get_name (account)) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

GossipAccount *
gossip_accounts_get_default (void)
{
	const gchar *name;

	/* use override name first */
	name = default_name_override;

	/* use default name second */
	if (!name) {
		name = default_name;

		/* check it exists, if not use the first one we find */
		if (name && !gossip_accounts_get_by_name (name)) {
			name = NULL;
		}
	}

	if (!name) {
		/* if one or more entries, use that */
		if (g_list_length (accounts) >= 1) {
			GossipAccount *account;
			account = g_list_nth_data (accounts, 0);
			name = gossip_account_get_name (account);

			gossip_accounts_set_default (account);

			return account;
		}
	} else {
		return gossip_accounts_get_by_name (name);
	}

	return NULL;
}

GossipAccount *
gossip_accounts_get_by_name (const gchar *name)
{
	GList *l;
	
	g_return_val_if_fail (name != NULL, NULL);

	for (l = accounts; l; l = l->next) {
		GossipAccount *account;
		const gchar   *account_name;

		account = l->data;
		account_name = gossip_account_get_name (account);

		if (account_name && strcmp (account_name, name) == 0) {
			return account;
		}
	}
	
	return NULL;
}	

void
gossip_accounts_set_overridden_default (const gchar *name)
{
	g_return_if_fail (name != NULL);

	g_free (default_name_override);
	default_name_override = g_strdup (name);
}

void 
gossip_accounts_set_default (GossipAccount *account)
{
	const gchar *name;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	name = gossip_account_get_name (account);
	g_return_if_fail (name != NULL);

	g_free (default_name);
	default_name = g_strdup (name);

	/* save */
	gossip_accounts_store (account);
}

gboolean
gossip_accounts_store (GossipAccount *account)
{
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);

	if (!accounts_check_exists (account)) {
		accounts = g_list_append (accounts, g_object_ref (account));
	}

	return accounts_file_save ();
}



