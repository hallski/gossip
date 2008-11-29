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
 */

#include <config.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "gossip-session.h"
#include "gossip-debug.h"
#include "gossip-jabber-utils.h"
#include "gossip-contact-manager.h"
#include "gossip-account-manager.h"
#include "gossip-private.h"
#include "gossip-utils.h"

#include "libgossip-marshal.h"

#define DEBUG_DOMAIN "ContactManager"

#define CONTACTS_XML_FILENAME "contacts.xml"
#define CONTACTS_DTD_FILENAME "gossip-contact.dtd"

#define GOSSIP_CONTACT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CONTACT_MANAGER, GossipContactManagerPrivate))

typedef struct _GossipContactManagerPrivate GossipContactManagerPrivate;

struct _GossipContactManagerPrivate {
	GossipSession *session;

	GHashTable    *contacts;
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

	g_type_class_add_private (object_class, sizeof (GossipContactManagerPrivate));
}

static void
gossip_contact_manager_init (GossipContactManager *manager)
{
}

static void
contact_manager_finalize (GObject *object)
{
	GossipContactManagerPrivate *priv;

	priv = GOSSIP_CONTACT_GET_PRIVATE (object);

	g_hash_table_unref (priv->contacts);

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
	GossipContactManagerPrivate *priv;
	GossipContactManager     *manager;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);

	manager = g_object_new (GOSSIP_TYPE_CONTACT_MANAGER, NULL);

	priv = GOSSIP_CONTACT_GET_PRIVATE (manager);

	priv->session = g_object_ref (session);

	g_signal_connect (priv->session, "contact-added",
			  G_CALLBACK (contact_manager_contact_added_cb),
			  manager);

	priv->contacts = g_hash_table_new_full (gossip_contact_hash,
						gossip_contact_equal,
						g_object_unref,
						NULL);

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
	GossipContactManagerPrivate *priv;
	GossipAccount            *account;
	GossipContact            *own_contact;
	
	g_return_val_if_fail (GOSSIP_IS_CONTACT_MANAGER (manager), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);

	priv = GOSSIP_CONTACT_GET_PRIVATE (manager);

	account = gossip_contact_get_account (contact);
	own_contact = gossip_contact_manager_get_own_contact (manager, account);

	/* Don't add if it is a self contact, since we do this ourselves */
	if (gossip_contact_equal (own_contact, contact)) {
		return FALSE;
	}

	/* Don't add more than once */
	if (g_hash_table_lookup (priv->contacts, contact)) {
		return FALSE;
	}

	gossip_debug (DEBUG_DOMAIN, 
		      "Adding contact with account:'%s' with id:'%s'",
		      gossip_account_get_name (account),
		      gossip_contact_get_id (contact));

	g_hash_table_insert (priv->contacts, 
			     g_object_ref (contact),
			     GINT_TO_POINTER (1));

	return TRUE;
}

void
gossip_contact_manager_remove (GossipContactManager *manager,
			       GossipContact        *contact)
{
	GossipContactManagerPrivate *priv;
	GossipAccount            *account;

	g_return_if_fail (GOSSIP_IS_CONTACT_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GOSSIP_CONTACT_GET_PRIVATE (manager);

	account = gossip_contact_get_account (contact);
	gossip_debug (DEBUG_DOMAIN, 
		      "Removing contact with account:'%s' with id:'%s'",
		      gossip_account_get_name (account),
		      gossip_contact_get_id (contact));

	g_hash_table_remove (priv->contacts, contact);
}

GossipContact *
gossip_contact_manager_find (GossipContactManager *manager,
			     GossipAccount        *account,
			     const gchar          *contact_id)
{
	GossipContactManagerPrivate *priv;
	GossipContact            *found = NULL;
	GList                    *contacts;
	GList                    *l;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_MANAGER (manager), NULL);
	g_return_val_if_fail (contact_id != NULL, NULL);

	priv = GOSSIP_CONTACT_GET_PRIVATE (manager);

	contacts = g_hash_table_get_keys (priv->contacts);

	for (l = contacts; l && !found; l = l->next) {
		GossipContact *this_contact;
		const gchar   *this_contact_id;
		gboolean       equal;

		this_contact = l->data;
		this_contact_id = gossip_contact_get_id (this_contact);

		if (!this_contact_id) {
			continue;
		}

		equal = TRUE;
		
		if (account) {
			GossipAccount *this_account;

			this_account = gossip_contact_get_account (this_contact);
			equal &= gossip_account_equal (account, this_account);
		}

		equal &= g_str_equal (contact_id, this_contact_id);
		
		if (equal) {
			found = this_contact;
		}
	}

	g_list_free (contacts);

	return found;
}

GossipContact *
gossip_contact_manager_find_extended (GossipContactManager *manager,
				      GossipAccount        *account,
				      GossipContactType     contact_type,
				      const gchar          *contact_id)
{
	GossipContactManagerPrivate *priv;
	GossipContact            *found = NULL;
	GList                    *contacts;
	GList                    *l;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_MANAGER (manager), NULL);
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (contact_id != NULL, NULL);

	priv = GOSSIP_CONTACT_GET_PRIVATE (manager);

	contacts = g_hash_table_get_keys (priv->contacts);

	for (l = contacts; l && !found; l = l->next) {
		GossipContact     *this_contact;
		GossipContactType  this_contact_type;
		const gchar       *this_contact_id;
		gboolean           equal;
		
		this_contact = l->data;
		this_contact_id = gossip_contact_get_id (this_contact);

		if (!this_contact_id) {
			continue;
		}

		equal = TRUE;
		
		if (account) {
			GossipAccount *this_account;

			this_account = gossip_contact_get_account (this_contact);
			equal &= gossip_account_equal (account, this_account);
		}

		this_contact_type = gossip_contact_get_type (this_contact);
		
		equal &= contact_type == this_contact_type;
		equal &= g_str_equal (contact_id, this_contact_id);
		
		if (equal) {
			found = this_contact;
		}
	}

	g_list_free (contacts);

	return found;
}

GossipContact *
gossip_contact_manager_find_or_create (GossipContactManager *manager,
				       GossipAccount        *account,
				       GossipContactType     contact_type,
				       const gchar          *contact_id,
				       gboolean             *created)
{
	GossipContact *contact;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_MANAGER (manager), NULL);
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (contact_id != NULL, NULL);

	/* Special case chatrooms, since we want to ONLY search for
	 * the contact id, account and type together for chatrooms,
	 * for other contacts, just account and contact id are fine
	 * because there is a chance that a contact is temporary until
	 * added to your contact list.
	 */
	if (contact_type == GOSSIP_CONTACT_TYPE_CHATROOM) { 
		contact = gossip_contact_manager_find_extended (manager, 
								account, 
								contact_type,
								contact_id);
	} else {
		contact = gossip_contact_manager_find (manager, 
						       account, 
						       contact_id);
	}

	if (created) {
		*created = contact == NULL;
	}

	if (contact) {
		gossip_debug (DEBUG_DOMAIN,
			      "Get contact:'%s', (%s)",
			      gossip_contact_get_id (contact), 
			      gossip_contact_type_to_string (contact_type));

		return contact;
	} else {
		gchar *name;

		gossip_debug (DEBUG_DOMAIN,
			      "New contact:'%s' (%s)",
			      contact_id,
			      gossip_contact_type_to_string (contact_type));

		name = gossip_jabber_get_name_to_use (contact_id, NULL, NULL, NULL);

		/* Create the contact using a default name as the ID */
		contact = gossip_contact_new (contact_type, account);
		gossip_contact_set_id (contact, contact_id);
		gossip_contact_set_name (contact, name);
		g_free (name);

		gossip_contact_manager_add (manager, contact);
		gossip_contact_manager_store (manager);
	}

	return contact;
}

GossipContact *
gossip_contact_manager_get_own_contact (GossipContactManager *manager,
					GossipAccount        *account)
{
	GossipContact     *contact;
	GossipContactType  contact_type;
	const gchar       *contact_id;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_MANAGER (manager), NULL);
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	contact_type = GOSSIP_CONTACT_TYPE_USER;
	contact_id = gossip_account_get_id (account);

	contact = gossip_contact_manager_find_extended (manager, 
							account, 
							contact_type,
							contact_id);

	if (contact) {
		gossip_debug (DEBUG_DOMAIN,
			      "Get own contact:'%s', (%s)",
			      gossip_contact_get_id (contact), 
			      gossip_contact_type_to_string (contact_type));

		return contact;
	} else {
		GossipContactManagerPrivate *priv;
		gchar                    *name;

		gossip_debug (DEBUG_DOMAIN,
			      "New own contact:'%s' (%s)",
			      contact_id,
			      gossip_contact_type_to_string (contact_type));
		
		name = gossip_jabber_get_name_to_use (contact_id, NULL, NULL, NULL);

		/* Create the contact using a default name as the ID */
		contact = gossip_contact_new (contact_type, account);
		gossip_contact_set_id (contact, contact_id);
		gossip_contact_set_name (contact, name);
		g_free (name);

		/* Don't use _manager_add() - recursive loop */
		priv = GOSSIP_CONTACT_GET_PRIVATE (manager);
		
		g_hash_table_insert (priv->contacts, 
				     contact,
				     GINT_TO_POINTER (1));

		gossip_contact_manager_store (manager);
		
		return contact;
	}
}

static gboolean
contact_manager_store_cb (GossipContactManager *manager)
{
	GossipContactManagerPrivate *priv;

	priv = GOSSIP_CONTACT_GET_PRIVATE (manager);

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
	GossipContactManagerPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_MANAGER (manager), FALSE);

	priv = GOSSIP_CONTACT_GET_PRIVATE (manager);

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
	GossipContactManagerPrivate *priv;
	gchar                    *directory;
	gchar                    *filename = NULL;

	priv = GOSSIP_CONTACT_GET_PRIVATE (manager);

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
		GossipContact *contact;

		contact = gossip_contact_manager_find_or_create (manager, 
								 account,
								 GOSSIP_CONTACT_TYPE_TEMPORARY,
								 id,
								 NULL);
		
		gossip_contact_set_name (contact, name);
	}

	xmlFree (name);
	xmlFree (id);
}

static void
contact_manager_parse_self (GossipContactManager *manager,
			    GossipAccount        *account,
			    xmlNodePtr            node)
{
	GossipContactManagerPrivate *priv;
	GossipContact               *own_contact;
	const gchar                 *id;
	const gchar                 *name;
	gchar                       *new_name;
	
	new_name = gossip_xml_node_get_child_content (node, "name");
	if (!new_name) {
		return;
	}

	priv = GOSSIP_CONTACT_GET_PRIVATE (manager);

	own_contact = gossip_contact_manager_get_own_contact (manager, account);

	id = gossip_contact_get_id (own_contact);
	name = gossip_contact_get_name (own_contact);
	
	/* We only set the name here if it is the contact ID or NULL
	 * because this is only needed for offline purposes, we must
	 * trust the name in all other circumstances since it is
	 * likely that the vcard we request in the backend has set it
	 * correctly. 
	 */ 
	if (G_STR_EMPTY (name) || (!G_STR_EMPTY (id) && strcmp (id, name) == 0)) {
		gossip_contact_set_name (own_contact, new_name);
	}

	xmlFree (new_name);
}

static gboolean
contact_manager_file_parse (GossipContactManager *manager,
			    const gchar          *filename)
{
	GossipContactManagerPrivate *priv;
	GossipAccountManager     *account_manager;
	xmlParserCtxtPtr          ctxt;
	xmlDocPtr                 doc;
	xmlNodePtr                contacts;
	xmlNodePtr                node, child;

	priv = GOSSIP_CONTACT_GET_PRIVATE (manager);

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
		GossipAccount *account;
		const gchar   *name;
		gchar         *account_name;

		if (xmlNodeIsText (node)) {
			node = node->next;
			continue;
		}

		name = node->name;

		account_name = xmlGetProp (node, "name");

		child = node->children;
		node = node->next;

		if (!account_name) {
			g_warning ("No 'name' attribute for '%s' element found?", name);
			continue;
		}

		account = gossip_account_manager_find (account_manager, account_name);
		if (!account) {			
			gossip_debug (DEBUG_DOMAIN, "No GossipAccount found by name:'%s'", account_name);
			continue;
		}
		
		xmlFree (account_name);
		
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
		      g_hash_table_size (priv->contacts));

	xmlFreeDoc(doc);
	xmlFreeParserCtxt (ctxt);

	return TRUE;
}

static gboolean
contact_manager_file_save (GossipContactManager *manager)
{
	GossipContactManagerPrivate *priv;
	GossipAccountManager     *account_manager;
	GossipAccount            *account;
	gchar                    *directory;
	gchar                    *filename;
#ifndef G_OS_WIN32
	mode_t                    old_mask;
#endif /* G_OS_WIN32 */
	xmlParserCtxtPtr          ctxt;
	xmlDocPtr                 doc;
	xmlNodePtr                root;
	GList                    *accounts;
	GList                    *contacts;
	GList                    *l;
	GHashTable               *nodes;
	gboolean                  create_file = FALSE;

	priv = GOSSIP_CONTACT_GET_PRIVATE (manager);

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

	for (l = accounts; l; l = l->next) {
		xmlNodePtr node;
		gboolean   exists = FALSE;
		const gchar *account_name;
		
		account = l->data;

		if (g_hash_table_lookup (nodes, account)) {
			continue;
		}

		account_name = gossip_account_get_name (account);

		/* Check if this account is already in the xml file */
		node = root->children;
		while (node) {
			gchar *name;

			name = xmlGetProp (node, "name");

 			exists = name && strcmp (name, account_name) == 0;  
			xmlFree (name);
				
			if (exists) {
				g_hash_table_insert (nodes, account, node);
				break;
			}

			node = node->next;
		}

		if (exists) {
			continue;
		}

		/* Add node for this account */
		node = xmlNewChild (root, NULL, "account", NULL);
		xmlNewProp (node, "name", account_name);

		g_hash_table_insert (nodes, account, node);
	}

	/* This is a self contact */
	for (l = accounts; l; l = l->next) {
		xmlNodePtr     node, child, p;
		GossipContact *own_contact;
		const gchar   *name; 
		gchar         *str;

		account = l->data;
		
		own_contact = gossip_contact_manager_get_own_contact (manager, account);
		name = gossip_contact_get_name (own_contact);

		node = g_hash_table_lookup (nodes, account);
		
		/* Set self details */
		p = gossip_xml_node_get_child (node, "self");
		if (!p) {
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
	
		str = g_markup_escape_text (name, -1);
		xmlNodeSetContent (child, str);
		g_free (str);
	}

	contacts = g_hash_table_get_keys (priv->contacts);

	for (l = contacts; l; l = l->next) {
		xmlNodePtr         node, child, p;
		GossipContact     *contact;
		GossipContactType  type;
		const gchar       *id;
		const gchar       *name; 
		gchar             *str;

		contact = l->data;

		type = gossip_contact_get_type (contact);

		if (type == GOSSIP_CONTACT_TYPE_CHATROOM ||
		    type == GOSSIP_CONTACT_TYPE_USER) {
			/* Ignore the user contact since that is
			 * ourselves and we already added that above.
			 * Plus don't add chatroom contacts, they seem
			 * pointless, the nick is in the id itself so
			 * there is no need to store it offline.
			 */
			continue;
		}

		account = gossip_contact_get_account (contact);
		node = g_hash_table_lookup (nodes, account);

		if (!node) {
			g_warning ("No node created for this account:'%s'",
				   gossip_account_get_name (account));
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
			child = xmlNewChild (node, NULL, "contact", NULL);
			xmlNewProp (child, "id", id);
		} else {
			child = p;
		}
		
		p = gossip_xml_node_get_child (child, "name");
		if (!p) {
			child = xmlNewChild (child, NULL, "name", NULL);
		} else {
			child = p;
		}
		
		str = g_markup_escape_text (name, -1);
		xmlNodeSetContent (child, str);
		g_free (str);
	}

	g_list_free (contacts);

	/* Save own contacts */
	g_hash_table_destroy (nodes);

	gossip_debug (DEBUG_DOMAIN, "Saving file:'%s'", filename);

#ifndef G_OS_WIN32
	/* Set the umask to get the proper permissions when libxml saves the
	 * file, but also change the permissions expiicitly in case the file
	 * already exists.
	 */
	old_mask = umask (077);
#endif

	chmod (filename, 0600);

	/* Make sure the XML is indented properly */
	xmlIndentTreeOutput = 1;
	xmlKeepBlanksDefault (0);

	xmlSaveFormatFileEnc (filename, doc, "utf-8", 1);

#ifndef G_OS_WIN32
	/* Set back to original umask */
	umask (old_mask);
#endif /* G_OS_WIN32 */

	xmlFreeDoc (doc);
	xmlCleanupParser ();
	xmlMemoryDump ();

	g_free (filename);

	return TRUE;
}
