/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006-2007 Imendio AB
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

#include "gossip-types.h"

#include "gossip-account.h"
#include "gossip-account-manager.h"
#include "gossip-contact.h"
#include "gossip-contact-manager.h"
#include "gossip-chatroom-provider.h"
#include "gossip-debug.h"
#include "gossip-private.h"
#include "gossip-protocol.h"
#include "gossip-session.h"
#include "gossip-utils.h"

#include "libgossip-marshal.h"

#define DEBUG_DOMAIN "ContactManager"

#define CONTACTS_XML_FILENAME "contacts.xml"
#define CONTACTS_DTD_FILENAME "gossip-contact.dtd"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CONTACT_MANAGER, GossipContactManagerPriv))

typedef struct _GossipContactManagerPriv GossipContactManagerPriv;

struct _GossipContactManagerPriv {
	GossipSession *session;

	GList         *contacts;
	gchar         *contacts_file_name;

	guint          store_timeout_id;
};

static void     contact_manager_finalize         (GObject              *object);
static gboolean contact_manager_store_cb         (GossipContactManager *manager);
static void     contact_manager_contact_added_cb (GossipSession        *session,
						  GossipContact        *contact,
						  GossipContactManager *manager);
static gboolean contact_manager_get_all          (GossipContactManager *manager);
static gboolean contact_manager_file_parse       (GossipContactManager *manager,
						  const gchar          *filename);
static gboolean contact_manager_file_save        (GossipContactManager *manager);

G_DEFINE_TYPE (GossipContactManager, gossip_contact_manager, G_TYPE_OBJECT);

static void
gossip_contact_manager_class_init (GossipContactManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = contact_manager_finalize;

	g_type_class_add_private (object_class, sizeof (GossipContactManagerPriv));
}

static void
gossip_contact_manager_init (GossipContactManager *manager)
{
}

static void
contact_manager_finalize (GObject *object)
{
	GossipContactManagerPriv *priv;

	priv = GET_PRIV (object);

	g_list_foreach (priv->contacts, (GFunc) g_object_unref, NULL);
	g_list_free (priv->contacts);

	g_free (priv->contacts_file_name);

	if (priv->store_timeout_id) {
		g_source_remove (priv->store_timeout_id);
		priv->store_timeout_id = 0;
	}

	if (priv->session) {
		g_object_unref (priv->session);
	}

	g_signal_handlers_disconnect_by_func (priv->session,
					      contact_manager_contact_added_cb,
					      GOSSIP_CONTACT_MANAGER (object));

	G_OBJECT_CLASS (gossip_contact_manager_parent_class)->finalize (object);
}

GossipContactManager *
gossip_contact_manager_new (GossipSession *session,
			    const gchar   *filename)
{
	GossipContactManagerPriv *priv;
	GossipContactManager     *manager;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);

	manager = g_object_new (GOSSIP_TYPE_CONTACT_MANAGER, NULL);

	priv = GET_PRIV (manager);

	priv->session = g_object_ref (session);

	g_signal_connect (priv->session, "contact-added",
			  G_CALLBACK (contact_manager_contact_added_cb),
			  manager);

	if (filename) {
		priv->contacts_file_name = g_strdup (filename);
	}

	/* Load file */
	contact_manager_get_all (manager);

	return manager;
}

gboolean
gossip_contact_manager_add (GossipContactManager *manager,
			    GossipContact        *contact)
{
	GossipContactManagerPriv *priv;
	GossipAccount            *account;
	const gchar              *account_param;
	const gchar              *contact_id;
	gboolean                  found;
	
	g_return_val_if_fail (GOSSIP_IS_CONTACT_MANAGER (manager), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);

	priv = GET_PRIV (manager);

	account = gossip_contact_get_account (contact);
	gossip_account_param_get (account, "account", &account_param, NULL);
	contact_id = gossip_contact_get_id (contact);

	/* Don't add if it is a self contact, since we do this ourselves */
	if (!G_STR_EMPTY (account_param) && 
	    !G_STR_EMPTY (contact_id) && 
	    strcmp (account_param, contact_id) == 0) {
		return TRUE;
	}

	/* Don't add more than once */
	found = gossip_contact_manager_find (manager, account_param, contact_id) != NULL;

	if (!found) {
		gossip_debug (DEBUG_DOMAIN, 
			      "Adding contact from account:'%s' with id:'%s'",
			      account_param, contact_id);

		priv->contacts = g_list_append (priv->contacts, g_object_ref (contact));
	}

	return !found;
}

void
gossip_contact_manager_remove (GossipContactManager *manager,
			       GossipContact        *contact)
{
	GossipContactManagerPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (manager);

	gossip_debug (DEBUG_DOMAIN,
		      "Removing contact with name:'%s'",
		      gossip_contact_get_name (contact));

	priv->contacts = g_list_remove (priv->contacts, contact);

	g_object_unref (contact);
}

GossipContact *
gossip_contact_manager_find (GossipContactManager *manager,
			     const gchar          *account_param,
			     const gchar          *contact_id)
{
	GossipContactManagerPriv *priv;
	GList                    *l;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_MANAGER (manager), NULL);
	g_return_val_if_fail (account_param != NULL, NULL);
	g_return_val_if_fail (contact_id != NULL, NULL);

	priv = GET_PRIV (manager);

	for (l = priv->contacts; l; l = l->next) {
		GossipAccount *account;
		GossipContact *contact;
		const gchar   *this_account_param;
		const gchar   *this_contact_id;

		contact = l->data;
		account = gossip_contact_get_account (contact);

		gossip_account_param_get (account, 
					  "account", &this_account_param, 
					  NULL);
		this_contact_id = gossip_contact_get_id (contact);

		if (!this_account_param || !this_contact_id) {
			continue;
		}

		if (strcmp (this_account_param, account_param) == 0 &&
		    strcmp (this_contact_id, contact_id) == 0) {
			return contact;
		}
	}

	return NULL;
}

static gboolean
contact_manager_store_cb (GossipContactManager *manager)
{
	GossipContactManagerPriv *priv;

	priv = GET_PRIV (manager);

	gossip_debug (DEBUG_DOMAIN, "Saving contacts");

	if (priv->store_timeout_id) {
		priv->store_timeout_id = 0;
	}

	contact_manager_file_save (manager);

	return FALSE;
}

gboolean
gossip_contact_manager_store (GossipContactManager *manager)
{
	GossipContactManagerPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_MANAGER (manager), FALSE);

	priv = GET_PRIV (manager);

	if (priv->store_timeout_id) {
		g_source_remove (priv->store_timeout_id);
		priv->store_timeout_id = 0;
	}

	/* Throttle save */
	priv->store_timeout_id = g_timeout_add (1000, (GSourceFunc) contact_manager_store_cb, manager);

	return TRUE;
}

static void
contact_manager_contact_added_cb (GossipSession        *session,
				  GossipContact        *contact,
				  GossipContactManager *manager)
{
	gossip_contact_manager_add (manager, contact);
	gossip_contact_manager_store (manager);
}

/*
 * API to save/load and parse the contacts file.
 */

static gboolean
contact_manager_get_all (GossipContactManager *manager)
{
	GossipContactManagerPriv *priv;
	gchar                    *directory;
	gchar                    *filename = NULL;

	priv = GET_PRIV (manager);

	gossip_debug (DEBUG_DOMAIN, "Loading contacts");

	/* Use default if no file specified. */
	if (!priv->contacts_file_name) {
		directory = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
		if (!g_file_test (directory, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
			g_mkdir_with_parents (directory, S_IRUSR | S_IWUSR | S_IXUSR);
		}

		filename = g_build_filename (directory, CONTACTS_XML_FILENAME, NULL);
		g_free (directory);
	} else {
		filename = g_strdup (priv->contacts_file_name);
	}

	gossip_debug (DEBUG_DOMAIN, "Trying filename:'%s'", filename);

	/* Read file in */
	if (g_file_test (filename, G_FILE_TEST_EXISTS) &&
	    !contact_manager_file_parse (manager, filename)) {
		g_free (filename);
		return FALSE;
	}

	g_free (filename);

	return TRUE;
}

static void
contact_manager_parse_contact (GossipContactManager *manager,
			       GossipAccount        *account,
			       xmlNodePtr            node)
{
	gchar *id;
	gchar *name;

	id = xmlGetProp (node, "id");
	name = gossip_xml_node_get_child_content (node, "name");

	if (id && name) {
		GossipContactManagerPriv *priv;
		GossipProtocol           *protocol;
		GossipContact            *contact;

		priv = GET_PRIV (manager);

		protocol = gossip_session_get_protocol (priv->session, account); 
		contact = gossip_protocol_new_contact (protocol, id, name);

		gossip_contact_manager_add (manager, contact);
	}

	xmlFree (name);
	xmlFree (id);
}

static void
contact_manager_parse_self (GossipContactManager *manager,
			    GossipAccount        *account,
			    xmlNodePtr            node)
{
	GossipContactManagerPriv *priv;
	GossipProtocol           *protocol;
	GossipContact            *contact;
	const gchar              *id;
	const gchar              *name;
	gchar                    *new_name;
	
	new_name = gossip_xml_node_get_child_content (node, "name");
	if (!new_name) {
		return;
	}

	priv = GET_PRIV (manager);

	protocol = gossip_session_get_protocol (priv->session, account); 
	contact = gossip_protocol_get_own_contact (protocol);

	id = gossip_contact_get_id (contact);
	name = gossip_contact_get_name (contact);
	
	/* We only set the name here if it is the contact ID or NULL
	 * because this is only needed for offline purposes, we must
	 * trust the name in all other circumstances since it is
	 * likely that the vcard we request in the backend has set it
	 * correctly. 
	 */ 
	if ((G_STR_EMPTY (name) == TRUE) || 
	    (G_STR_EMPTY (id) == FALSE && strcmp (id, name) == 0)) {
		gossip_contact_set_name (contact, new_name);
	}

	contact = gossip_protocol_new_contact (protocol, id, NULL);
	gossip_contact_manager_add (manager, contact);

	xmlFree (new_name);
}

static gboolean
contact_manager_file_parse (GossipContactManager *manager,
			    const gchar          *filename)
{
	GossipContactManagerPriv *priv;
	GossipAccountManager     *account_manager;
	xmlParserCtxtPtr          ctxt;
	xmlDocPtr                 doc;
	xmlNodePtr                contacts;
	xmlNodePtr                node, child;

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

	if (!gossip_xml_validate (doc, CONTACTS_DTD_FILENAME)) {
		g_warning ("Failed to validate file:'%s'", filename);
		xmlFreeDoc (doc);
		xmlFreeParserCtxt (ctxt);
		return FALSE;
	}

	/* Do this now so we don't do it for each contact node */
	account_manager = gossip_session_get_account_manager (priv->session);

	/* The root node, contacts. */
	contacts = xmlDocGetRootElement (doc);

	node = contacts->children;
	while (node) {
		GossipAccount *account = NULL;
		const gchar   *name;
		gchar         *id;
		gchar         *type;

		if (xmlNodeIsText (node)) {
			node = node->next;
			continue;
		}

		name = node->name;

		id = xmlGetProp (node, "id");
		type = gossip_xml_node_get_child_content (node, "type");

		child = node->children;
		node = node->next;

		if (!id) {
			xmlFree (type);
			g_warning ("No 'id' attribute for '%s' element found?", name);
			continue;
		}

		if (!type) {
			xmlFree (id);
			g_warning ("No 'type' attribute for '%s' element found?", name);
			continue;
		}

		account = gossip_account_manager_find_by_id (account_manager, id, type);

		xmlFree (id);
		xmlFree (type);

		if (!account) {			
			g_warning ("No GossipAccount found by id:'%s' and type:'%s'", id, type);
			continue;
		}
		
		while (child) {
			if (xmlNodeIsText (child)) {
				child = child->next;
				continue;
			}

			if (strcmp ((gchar*) child->name, "self") == 0) {
				contact_manager_parse_self (manager, account, child);
			}
			else if (strcmp ((gchar*) child->name, "contact") == 0) {
				contact_manager_parse_contact (manager, account, child);
			}
			
			child = child->next;
		}
	}

	gossip_debug (DEBUG_DOMAIN,
		      "Parsed %d contacts",
		      g_list_length (priv->contacts));

	xmlFreeDoc(doc);
	xmlFreeParserCtxt (ctxt);

	return TRUE;
}

static gboolean
contact_manager_file_save (GossipContactManager *manager)
{
	GossipContactManagerPriv *priv;
	GossipAccountManager     *account_manager;
	GossipAccount            *account;
	gchar                    *directory;
	gchar                    *filename;
	mode_t                    old_mask;
	xmlParserCtxtPtr          ctxt;
	xmlDocPtr                 doc;
	xmlNodePtr                root;
	GList                    *accounts;
	GList                    *l;
	GHashTable               *nodes;
	gboolean                  create_file = FALSE;

	priv = GET_PRIV (manager);

	gossip_debug (DEBUG_DOMAIN, "Attempting to save...");

	if (!priv->contacts) {
		gossip_debug (DEBUG_DOMAIN, "No contacts to save");
		return TRUE;
	}

	if (priv->contacts_file_name) {
		filename = g_strdup (priv->contacts_file_name);
	} else {
		directory = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
		g_mkdir_with_parents (directory, S_IRUSR | S_IWUSR | S_IXUSR);

		filename = g_build_filename (directory, CONTACTS_XML_FILENAME, NULL);
		g_free (directory);
	}

	/* 
	 * Try to read the existing file in.
	 */
	ctxt = xmlNewParserCtxt ();

	/* Parse and validate the file. */
	doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);
	if (!doc) {
		gossip_debug (DEBUG_DOMAIN,
			      "Failed to parse file:'%s' (perhaps it doesn't exist)",
			      filename);
		xmlFreeParserCtxt (ctxt);
		create_file = TRUE;
	} else {
		if (!gossip_xml_validate (doc, CONTACTS_DTD_FILENAME)) {
			gossip_debug (DEBUG_DOMAIN, 
				      "Failed to validate file:'%s'", 
				      filename);
			xmlFreeDoc (doc);
			xmlFreeParserCtxt (ctxt);
			create_file = TRUE;
		} else {
			gossip_debug (DEBUG_DOMAIN, "Using existing filename:'%s'", filename);
			root = xmlDocGetRootElement (doc);
		}
	}

	if (create_file) {
		gossip_debug (DEBUG_DOMAIN, "Creating new filename:'%s'", filename);
		doc = xmlNewDoc ("1.0");
		root = xmlNewNode (NULL, "contacts");
		xmlDocSetRootElement (doc, root);
	}

	nodes = g_hash_table_new (g_direct_hash, g_direct_equal);

	/* Because each contact may belong to a different account, we
	 * need to make sure we iterate this per account.
	 */
	account_manager = gossip_session_get_account_manager (priv->session);
	accounts = gossip_account_manager_get_accounts (account_manager);

	gossip_debug (DEBUG_DOMAIN, "Checking account nodes exist");

	for (l = accounts; l; l = l->next) {
		xmlNodePtr         node;
		GossipAccountType  account_type;
		const gchar       *account_type_str;
		const gchar       *account_param;
		gboolean           exists = FALSE;
		
		account = l->data;

		if (g_hash_table_lookup (nodes, account)) {
			continue;
		}

		account_type = gossip_account_get_type (account);
		account_type_str = gossip_account_type_to_string (account_type);

		gossip_account_param_get (account, "account", &account_param, NULL);

		/* Check if this account is already in the xml file */
		node = root->children;
		while (node) {
			gchar *type;
			gchar *id;

			id = xmlGetProp (node, "id");
			type = gossip_xml_node_get_child_content (node, "type");

 			exists = type && strcmp (type, account_type_str) == 0 &&
 				 id && strcmp (id, account_param) == 0;  
			xmlFree (type);
			xmlFree (id);
				
			if (exists) {
				g_hash_table_insert (nodes, account, node);
				break;
			}

			node = node->next;
		}

		if (exists) {
			gossip_debug (DEBUG_DOMAIN, 
				      "Using existing xml node for account:'%s'", 
				      gossip_account_get_name (account));
			continue;
		}

		gossip_debug (DEBUG_DOMAIN, 
			      "Creating xml node for account:'%s'", 
			      gossip_account_get_name (account));

		/* Add node for this account */
		node = xmlNewChild (root, NULL, "account", NULL);
		xmlNewProp (node, "id", account_param);
		xmlNewChild (node, NULL, "type", account_type_str);

		g_hash_table_insert (nodes, account, node);
	}


	/* This is a self contact */
	for (l = accounts; l; l = l->next) {
		xmlNodePtr      node, child, p;
		GossipProtocol *protocol;
		GossipContact  *contact;
		const gchar    *name; 

		account = l->data;
		
		protocol = gossip_session_get_protocol (priv->session, account);
		contact = gossip_protocol_get_own_contact (protocol);
		name = gossip_contact_get_name (contact);

		node = g_hash_table_lookup (nodes, account);
		
		/* Set self details */
		p = gossip_xml_node_get_child (node, "self");
		if (!p) {
			gossip_debug (DEBUG_DOMAIN, 
				      "Creating xml node for self contact for "
				      "account:'%s'", 
				      gossip_account_get_name (account));
			child = xmlNewChild (node, NULL, "self", NULL);
		} else {
			child = p;
		}
		
		p = gossip_xml_node_get_child (child, "name");
		if (!p) {
			child = xmlNewChild (child, NULL, "name", NULL);
		} else {
			child = p;
		}
		
		gossip_debug (DEBUG_DOMAIN, 
			      "Updating xml node for self contact for "
			      "account:'%s' with name:'%s'", 
			      gossip_account_get_name (account),
			      name);
		
		xmlNodeSetContent (child, name);
	}

	for (l = priv->contacts; l; l = l->next) {
		xmlNodePtr     node, child, p;
		GossipContact *contact;
		const gchar   *account_param;
		const gchar   *id;
		const gchar   *name; 

		contact = l->data;
		account = gossip_contact_get_account (contact);

		node = g_hash_table_lookup (nodes, account);
		
		gossip_account_param_get (account, "account", &account_param, NULL);

		if (!node) {
			g_warning ("No node created for this account:'%s'", account_param);
			continue;
		}

		id = gossip_contact_get_id (contact);
		name = gossip_contact_get_name (contact);

		if (!id || !name) {
			continue;
		}

		/* This is a normal contact */
		p = gossip_xml_node_find_child_prop_value (node, "id", id); 
		if (!p) {
			gossip_debug (DEBUG_DOMAIN, 
				      "Creating xml node for contact:'%s' for "
				      "account:'%s'", 
				      id,
				      gossip_account_get_name (account));
			child = xmlNewChild (node, NULL, "contact", NULL);
			xmlNewProp (child, "id", id);
		} else {
			child = p;
		}
		
		p = gossip_xml_node_get_child (child, "name");
		if (!p) {
			gossip_debug (DEBUG_DOMAIN, "Adding name node...");
			child = xmlNewChild (child, NULL, "name", NULL);
		} else {
			child = p;
		}
		
		gossip_debug (DEBUG_DOMAIN, 
			      "Updating xml node for contact:'%s' for "
			      "account:'%s' to:'%s'", 
			      id, 
			      gossip_account_get_name (account),
			      name);
		
		xmlNodeSetContent (child, name);
	}

	/* Save own contacts */
	g_hash_table_destroy (nodes);

	gossip_debug (DEBUG_DOMAIN, "Saving file:'%s'", filename);

	/* Set the umask to get the proper permissions when libxml saves the
	 * file, but also change the permissions expiicitly in case the file
	 * already exists.
	 */
	old_mask = umask (077);
	chmod (filename, 0600);

	/* Make sure the XML is indented properly */
	xmlIndentTreeOutput = 1;
	xmlKeepBlanksDefault (0);

	xmlSaveFormatFileEnc (filename, doc, "utf-8", 1);

	/* Set back to original umask */
	umask (old_mask);

	xmlFreeDoc (doc);
	xmlCleanupParser ();
	xmlMemoryDump ();

	g_free (filename);

	return TRUE;
}
