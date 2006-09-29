/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Martyn Russell <mr@gnome.org>
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

#include <string.h>

#include <glib.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libxml/xmlreader.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <loudmouth/loudmouth.h>

#include <config.h>

#include "gossip-jabber-private.h"
#include "gossip-transport-accounts.h"
#include "gossip-transport-protocol.h"

#define DEBUG_MSG(x)
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n"); */


struct _GossipTransportProtocol {
	guint  ref_count;

	gchar *name;
	gchar *disco_type;
	gchar *stock_icon;
	gchar *description;
	gchar *example;

	gchar *url;
	GList *services;

	gchar *icon;
};

struct _GossipTransportService {
	gchar *name;
	gchar *host;
	gchar *port;
};


typedef struct {
	gchar *id;

	GossipTransportProtocol *protocol;
	GossipTransportProtocolIDFunc func;

	gpointer user_data;
} ProtocolID;


static int                      transport_gnomevfs_print_error     (GnomeVFSResult                 result,
								    const char                    *uri_string);
static GossipTransportProtocol *transport_protocol_file_parse      (const gchar                   *filename);
static gboolean                 transport_protocol_file_validate   (const gchar                   *filename);
static GossipTransportProtocol *transport_protocol_new             (void);
static LmHandlerResult          transport_protocol_message_handler (LmMessageHandler              *handler,
								    LmConnection                  *connection,
								    LmMessage                     *m,
								    gpointer                       user_data);
static ProtocolID *             transport_protocol_id_new          (const char                    *id,
								    GossipTransportProtocol       *protocol,
								    GossipTransportProtocolIDFunc  func,
								    gpointer                       user_data);
static void                     transport_protocol_id_free         (ProtocolID                    *pi);


static GossipTransportService   *transport_service_new               (const gchar                   *name,
								    const gchar                   *host);



static int
transport_gnomevfs_print_error (GnomeVFSResult  result,
				const char     *uri_string)
{
	const char *error_string;

	error_string = gnome_vfs_result_to_string (result);

	g_warning ("Error %s occured opening location %s",
		   error_string, uri_string);

	return FALSE;
}

GList *
gossip_transport_protocol_get_all (void)
{
	GnomeVFSResult  result;
	GList          *files = NULL;
	GList          *l;
	GList          *protocol_list = NULL;
	gchar          *dir;

	DEBUG_MSG (("ProtocolTransport: Attempting to get a list of "
		    "protocols with uri:'%s'",
		    PROTOCOLSDIR));

	/*
	 * look up files packaged with Gossip
	 */
	result = gnome_vfs_directory_list_load (&files, PROTOCOLSDIR,
						GNOME_VFS_FILE_INFO_DEFAULT |
						GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
						GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

	if (result != GNOME_VFS_OK) {
		transport_gnomevfs_print_error (result, PROTOCOLSDIR);
		return NULL;
	}

	if (!files || g_list_length (files) < 1) {
		DEBUG_MSG (("ProtocolTransport: No protocol xml files found in %s",
			    PROTOCOLSDIR));
		return NULL;
	}

	for (l=files; l; l=l->next) {
		GossipTransportProtocol *protocol;
		GnomeVFSFileInfo        *file;
		gchar                   *file_with_path;

		file = (GnomeVFSFileInfo*)l->data;

		if (file->mime_type && strcmp (file->mime_type, "text/xml") != 0) {
			continue;
		}

		file_with_path = g_build_filename (PROTOCOLSDIR, file->name, NULL);

		if (!transport_protocol_file_validate (file_with_path)) {
			g_free (file_with_path);
			continue;
		}

		protocol = transport_protocol_file_parse (file_with_path);

		if (protocol) {
			protocol_list = g_list_append (protocol_list, protocol);
		}

		g_free (file_with_path);
	}

	/*
	 * Look up user files in ~/.gnome2/Gossip/protocols
	 */
	dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, "protocols", NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	}

	DEBUG_MSG (("ProtocolTransport: Attempting to get a list of "
		    "protocols with uri:'%s'",
		    dir));

	result = gnome_vfs_directory_list_load (&files, dir,
						GNOME_VFS_FILE_INFO_DEFAULT |
						GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
						GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

	if (result != GNOME_VFS_OK) {
		g_free (dir);
		transport_gnomevfs_print_error (result, PROTOCOLSDIR);
		return NULL;
	}

	if (!files || g_list_length (files) < 1) {
		g_free (dir);
		DEBUG_MSG (("ProtocolTransport: No protocol xml files found in %s",
			    PROTOCOLSDIR));
		return NULL;
	}

	for (l=files; l; l=l->next) {
		GossipTransportProtocol *protocol;
		GnomeVFSFileInfo        *file;
		gchar                   *file_with_path;

		file = (GnomeVFSFileInfo*)l->data;

		if (file->mime_type && strcmp (file->mime_type, "text/xml") != 0) {
			continue;
		}

		file_with_path = g_build_filename (dir, file->name, NULL);

		if (!transport_protocol_file_validate (file_with_path)) {
			g_free (file_with_path);
			continue;
		}

		protocol = transport_protocol_file_parse (file_with_path);

		if (protocol) {
			protocol_list = g_list_append (protocol_list, protocol);
		}

		g_free (file_with_path);
	}

/* 	(func)(protocol_list, user_data); */

	g_free (dir);

	return protocol_list;
}

static gboolean
transport_protocol_file_validate (const char *filename)
{

	xmlParserCtxtPtr ctxt;
	xmlDocPtr        doc;

	gboolean         ret = FALSE;

	g_return_val_if_fail (filename != NULL, FALSE);

	DEBUG_MSG (("ProtocolTransport: Attempting to validate file (against DTD):'%s'",
		    filename));

	/* create a parser context */
	ctxt = xmlNewParserCtxt();
	if (ctxt == NULL) {
		g_warning ("failed to allocate parser context for file:'%s'",
			   filename);
		return FALSE;
	}

	/* parse the file, activating the DTD validation option */
	doc = xmlCtxtReadFile(ctxt, filename, NULL, XML_PARSE_DTDVALID);

	/* check if parsing suceeded */
	if (doc == NULL) {
		g_warning ("failed to parse file:'%s'",
			   filename);
	} else {
		/* check if validation suceeded */
		if (ctxt->valid == 0) {
			g_warning ("failed to validate file:'%s'",
				   filename);
		} else {
			ret = TRUE;
		}

		/* free up the resulting document */
		xmlFreeDoc(doc);
	}

	/* free up the parser context */
	xmlFreeParserCtxt(ctxt);

	return ret;
}

static GossipTransportProtocol *
transport_protocol_file_parse (const gchar *filename)
{
	GossipTransportProtocol *protocol = NULL;

	xmlDocPtr                doc;
	xmlTextReaderPtr         reader;
	int                      ret;

	g_return_val_if_fail (filename != NULL, FALSE);

	DEBUG_MSG (("ProtocolTransport: Attempting to parse file:'%s'...", filename));

	reader = xmlReaderForFile(filename, NULL, 0);
	if (reader == NULL) {
		g_warning ("could not create xml reader for file:'%s' filename",
			   filename);
		return NULL;
	}

	if (xmlTextReaderPreservePattern(reader, (xmlChar *) "preserved", NULL) < 0) {
		g_warning ("could not preserve pattern for file:'%s' filename",
			   filename);
		return NULL;
	}

	ret = xmlTextReaderRead(reader);

	if (ret == 1) {
		protocol = transport_protocol_new ();
	}

	while (ret == 1) {
		const gchar *node = NULL;
		const gchar *value = NULL;

		if (!(node = (const gchar *) xmlTextReaderConstName(reader))) {
			continue;
		}

		if (strcmp (node, "name") == 0 ||
		    strcmp (node, "disco-type") == 0 ||
		    strcmp (node, "stock-icon") == 0 ||
		    strcmp (node, "description") == 0 ||
		    strcmp (node, "example") == 0 ||
		    strcmp (node, "icon") == 0) {
			const gchar *node_text = NULL;

			if ((ret = xmlTextReaderRead(reader)) != 1) {
				break;
			}

			node_text = (const gchar *) xmlTextReaderConstName(reader);

			if (!value && strcmp (node_text, "#text") == 0) {
				value = (const gchar *) xmlTextReaderConstValue(reader);
			}
		}

		if (strcmp (node, "server-list") == 0) {
			xmlChar *url;

			/* get attributes */
			url = xmlTextReaderGetAttribute (reader, (xmlChar *) "url");

			if (url) {
				protocol->url = g_strdup ((gchar *) url);
				xmlFree (url);
			}

			while ((ret = xmlTextReaderRead(reader)) == 1) {
				GossipTransportService *service;
				xmlChar               *name, *host, *port;
				const gchar           *server_node;

				server_node = (const gchar *) xmlTextReaderConstName(reader);

				if (!server_node || strcmp (server_node, "server") != 0) {
					continue;
				}

				/* get attributes */
				name = xmlTextReaderGetAttribute (reader, (xmlChar *) "name");
				host = xmlTextReaderGetAttribute (reader, (xmlChar *) "host");
				port = xmlTextReaderGetAttribute (reader, (xmlChar *) "port");

				service = transport_service_new ((gchar *) name, (gchar *) host);

				if (service && port) {
					service->port = g_strdup ((gchar *) port);
				}

				/* add to list */
				protocol->services = g_list_append (protocol->services, service);

				xmlFree (name);
				xmlFree (host);
				xmlFree (port);
			}

			continue;
		}

		if (node && value) {
			if (!protocol->name && strcmp (node, "name") == 0) {
				protocol->name = g_strdup (value);
			} else if (!protocol->disco_type && strcmp (node, "disco-type") == 0) {
				protocol->disco_type = g_strdup (value);
			} else if (!protocol->stock_icon && strcmp (node, "stock-icon") == 0) {
				protocol->stock_icon = g_strdup (value);
			} else if (!protocol->description && strcmp (node, "description") == 0) {
				protocol->description = g_strdup (value);
			} else if (!protocol->example && strcmp (node, "example") == 0) {
				protocol->example = g_strdup (value);
			} else if (!protocol->icon && strcmp (node, "icon") == 0) {
				protocol->icon = g_strdup (value);
			}
		}

		ret = xmlTextReaderRead(reader);
	}

	if (ret != 0) {
		g_warning ("could not parse file:'%s' filename",
			   filename);
		xmlFreeTextReader(reader);
		return NULL;
	}

	DEBUG_MSG (("ProtocolTransport: protocol name:'%s'\n"
		   "\tdisco_type:'%s'\n"
		   "\tstock_icon:'%s'\n"
		   "\tdescription:'%s'\n"
		   "\texample:'%s'\n"
		   "\ticon:'%s'\n"
		   "\turl:'%s'\n"
		   "\tservices:%d",
		   protocol->name,
		   protocol->disco_type,
		   protocol->stock_icon,
		   protocol->description,
		   protocol->example,
		   protocol->icon,
		   protocol->url,
		   g_list_length (protocol->services)));

	DEBUG_MSG (("ProtocolTransport: cleaning up parser for file:'%s'", filename));

	doc = xmlTextReaderCurrentDoc(reader);
	xmlFreeDoc(doc);

	xmlCleanupParser();
	xmlFreeTextReader(reader);

	return protocol;
}

static GossipTransportProtocol *
transport_protocol_new ()
{
	GossipTransportProtocol *protocol;

	protocol = g_new0 (GossipTransportProtocol, 1);

	protocol->ref_count = 1;

	/* should we set a default icon here? */

	return protocol;
}

static void
transport_protocol_free (GossipTransportProtocol *protocol)
{
	g_return_if_fail (protocol != NULL);

	g_free (protocol->name);
	g_free (protocol->disco_type);
	g_free (protocol->stock_icon);
	g_free (protocol->description);
	g_free (protocol->example);
	g_free (protocol->icon);

	g_free (protocol->url);

	if (protocol->services) {
		g_list_foreach (protocol->services, (GFunc)gossip_transport_service_free, NULL);
		g_list_free (protocol->services);
	}

	g_free (protocol);
}

GossipTransportProtocol *
gossip_transport_protocol_ref (GossipTransportProtocol *protocol)
{
	g_return_val_if_fail (protocol != NULL, NULL);

	protocol->ref_count++;
	return protocol;
}

void
gossip_transport_protocol_unref (GossipTransportProtocol *protocol)
{
	g_return_if_fail (protocol != NULL);

	protocol->ref_count--;

	if (protocol->ref_count < 1) {
		transport_protocol_free (protocol);
	}
}

const gchar *
gossip_transport_protocol_get_name (GossipTransportProtocol *protocol)
{
	g_return_val_if_fail (protocol != NULL, NULL);

	return protocol->name;
}

const gchar *
gossip_transport_protocol_get_disco_type (GossipTransportProtocol *protocol)
{
	g_return_val_if_fail (protocol != NULL, NULL);

	return protocol->disco_type;
}

const gchar *
gossip_transport_protocol_get_stock_icon (GossipTransportProtocol *protocol)
{
	g_return_val_if_fail (protocol != NULL, NULL);

	return protocol->stock_icon;
}

const gchar *
gossip_transport_protocol_get_description (GossipTransportProtocol *protocol)
{
	g_return_val_if_fail (protocol != NULL, NULL);

	return protocol->description;
}

const gchar *
gossip_transport_protocol_get_example (GossipTransportProtocol *protocol)
{
	g_return_val_if_fail (protocol != NULL, NULL);

	return protocol->example;
}

const gchar *
gossip_transport_protocol_get_icon (GossipTransportProtocol *protocol)
{
	g_return_val_if_fail (protocol != NULL, NULL);

	return protocol->icon;
}

const gchar *
gossip_transport_protocol_get_url (GossipTransportProtocol *protocol)
{
	g_return_val_if_fail (protocol != NULL, NULL);

	return protocol->url;
}

GList *
gossip_transport_protocol_get_services (GossipTransportProtocol *protocol)
{
	g_return_val_if_fail (protocol != NULL, NULL);

	return protocol->services;
}

GossipTransportProtocol *
gossip_transport_protocol_find_by_disco_type (const gchar *disco_type)
{
	GList *protocols;
	GList *l;

	g_return_val_if_fail (disco_type != NULL, NULL);

	protocols = gossip_transport_protocol_get_all ();

	for (l=protocols; l; l=l->next) {
		GossipTransportProtocol *protocol;

		protocol = (GossipTransportProtocol*) l->data;

		if (protocol->disco_type &&
		    strcmp (protocol->disco_type, disco_type) == 0) {
			return protocol;
		}
	}

	return NULL;
}

static GossipTransportService *
transport_service_new (const gchar *name,
		      const gchar *host)
{
	GossipTransportService *service;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (host != NULL, NULL);

	service = g_new0 (GossipTransportService, 1);

	service->name = g_strdup (name);
	service->host = g_strdup (host);

	return service;
}

void
gossip_transport_service_free (GossipTransportService *service)
{
	g_return_if_fail (service != NULL);

	g_free (service->name);
	g_free (service->host);
	g_free (service->port);

	g_free (service);
}

const gchar *
gossip_transport_service_get_name (GossipTransportService *service)
{
	g_return_val_if_fail (service != NULL, NULL);

	return service->name;
}

const gchar *
gossip_transport_service_get_host (GossipTransportService *service)
{
	g_return_val_if_fail (service != NULL, NULL);

	return service->host;
}

gint
gossip_transport_service_get_port (GossipTransportService *service)
{
	g_return_val_if_fail (service != NULL, 5222);

	return atoi (service->port);
}

/*
 * utils
 */
void
gossip_transport_protocol_id_to_jid (GossipTransportProtocol       *protocol,
				     const gchar                   *id,
				     GossipTransportProtocolIDFunc  func,
				     gpointer                       user_data)
{
	/* id can be someone@hotmail.com for MSN or a 123456787 for
	   ICQ and and this will convert the id into a jabber id */

	GossipTransportAccount *account;
	GossipJabber           *jabber;
	GossipJID              *jid;

	LmConnection           *connection;
	LmMessageHandler       *handler;

	LmMessage              *m;
	LmMessageNode          *node;

	ProtocolID             *pi;

	g_return_if_fail (protocol != NULL);
	g_return_if_fail (id != NULL);
	g_return_if_fail (func != NULL);

#ifdef FIXME_MJR
	account = gossip_transport_account_find_by_disco_type (protocol->disco_type);
#else
	jabber = NULL;
	account = NULL;
#endif
	jid = gossip_transport_account_get_jid (account);

	/* create new object to store details */
	pi = transport_protocol_id_new (id, protocol, func, user_data);

	/* set up message handler */
	connection = gossip_jabber_get_connection (jabber);

	handler = lm_message_handler_new (transport_protocol_message_handler,
					  pi,
					  NULL);

	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_IQ,
						LM_HANDLER_PRIORITY_NORMAL);

	/* create message */
	m = lm_message_new_with_sub_type (gossip_jid_get_full (jid),
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_SET);

	DEBUG_MSG (("ProtocolTransport: Requesting id to jid translation for: %s", id));

	lm_message_node_add_child (m->node, "query", NULL);
	node = lm_message_node_get_child (m->node, "query");

	lm_message_node_set_attribute (node, "xmlns", "jabber:iq:gateway");

	lm_message_node_add_child (node, "prompt", id);

	lm_connection_send (connection, m, NULL);
	lm_message_unref (m);
}

static LmHandlerResult
transport_protocol_message_handler (LmMessageHandler *handler,
				    LmConnection     *connection,
				    LmMessage        *m,
				    gpointer          user_data)
{
	LmMessageNode *node;
	ProtocolID    *pi;
	GossipJID     *jid;
	const char    *xmlns;

	if (lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_RESULT &&
	    lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_ERROR) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	node = lm_message_node_get_child (m->node, "query");
	if (!node) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	xmlns = lm_message_node_get_attribute (node, "xmlns");
	if (!xmlns || strcmp (xmlns, "jabber:iq:gateway") != 0) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	node = lm_message_node_get_child (node, "prompt");
	if (!node || !node->value) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	pi = (ProtocolID*) user_data;

	jid = gossip_jid_new (node->value);
	DEBUG_MSG (("ProtocolTransport: Translation is: %s\n",
		    gossip_jid_get_full (jid)));

	/* call callback */
	(pi->func)(jid, pi->id, pi->user_data);

	/* clean up */
	lm_connection_unregister_message_handler (connection,
						  handler,
						  LM_MESSAGE_TYPE_IQ);

	lm_message_handler_unref (handler);

	transport_protocol_id_free (pi);

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static ProtocolID  *transport_protocol_id_new (const char *id,
					       GossipTransportProtocol *protocol,
					       GossipTransportProtocolIDFunc func,
					       gpointer user_data)
{
	ProtocolID *pi;

	g_return_val_if_fail (id != NULL, NULL);
	g_return_val_if_fail (protocol != NULL, NULL);
	g_return_val_if_fail (func != NULL, NULL);

	pi = g_new0 (ProtocolID, 1);

	pi->id = g_strdup (id);

	pi->protocol = gossip_transport_protocol_ref (protocol);
	pi->func = func;

	pi->user_data = user_data;

	return pi;
}

static void
transport_protocol_id_free (ProtocolID *pi)
{
	g_return_if_fail (pi != NULL);

	g_free (pi->id);

	gossip_transport_protocol_unref (pi->protocol);

	g_free (pi);
}
