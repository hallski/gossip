/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Martyn Russell (ginxd@btopenworld.com)
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
#include <gnome.h>

#include "gossip-disco-servers.h"

#define d(x) x


struct _GossipDiscoProtocol {
	guint  ref_count;
	
	gchar *name;
	gchar *description;
	gchar *disco_type;
	gchar *icon;
	gchar *stock_icon;

	gchar *url;

	GList *servers;
};

struct _GossipDiscoServer {
	gchar *name;
	gchar *host;
	gchar *port;
};


static int    disco_servers_print_error (GnomeVFSResult  result,
					 const char     *uri_string);
static GList *              disco_servers_parse           (const char     *servers);

static GossipDiscoProtocol *disco_protocols_parse_file    (const gchar    *filename);
static gboolean             disco_protocols_validate_file (const gchar    *filename);
static GossipDiscoProtocol *disco_protocol_new            (void);

static GossipDiscoServer   *disco_server_new              (const gchar    *name,
							   const gchar    *host);



gboolean 
gossip_disco_servers_fetch (GossipDiscoServersFunc func,
			    gpointer               user_data)
{
	GnomeVFSResult  result;
	GList          *list;
	int             bytes_read;
	char           *servers;
	const char     *uri;
	
	uri = "http://www.jabber.org/servers.xml";

	g_return_val_if_fail (func != NULL, FALSE);

	result = gnome_vfs_read_entire_file (uri, 
					     &bytes_read, 
					     &servers);
	if (result != GNOME_VFS_OK) {
		return disco_servers_print_error (result, uri);  
	}

	d(g_print ("opened URI:'%s' and read %d bytes\n", uri, bytes_read));
	
	list = disco_servers_parse (servers);

	(func) (list, user_data);
       
 	g_free (servers); 
	g_list_foreach (list, (GFunc)g_free, NULL);
	g_list_free (list);

	return TRUE;
}

static int
disco_servers_print_error (GnomeVFSResult  result, 
			   const char     *uri_string)
{
	const char *error_string;

	error_string = gnome_vfs_result_to_string (result);

	g_warning ("Error %s occured opening location %s\n", 
		   error_string, uri_string);

	return FALSE;
}

static GList *
disco_servers_parse (const char *servers)
{
	const gchar *marker;
	const gchar *item_str;
	const gchar *name_str;
	GList       *list;

	item_str = "jid=";
	name_str = "name=";

	g_return_val_if_fail (servers != NULL, NULL);

	d(g_print ("parsing server list (%d bytes)\n", strlen (servers))); 

	/* \n\n is what we look for that divides the HTTP header
	   and the HTTP body */
	marker = strstr (servers, "\n\n");

	list = NULL;

	/* tried using the LmParser but because the string is not
	   proper Jabber, it throws it out :( */
	while ((marker = strstr (marker, item_str))) {
		const gchar *s1;
		const gchar *s2;
		
		gchar       *jid = NULL;
		gchar       *name = NULL;
		
		/* find jid */
		s1 = strstr (marker, "'"); 
		s2 = strstr (s1 + 1, "'");
		
		if (s1 && s2) {
			jid = g_strndup (s1 + 1, s2 - s1 - 1);
		}
		
		/* find name */
		marker = strstr (s2, name_str) + 1; 
		s1 = strstr (marker, "'"); 
		s2 = strstr (s1 + 1, "'");
		
		if (s1 && s2) {
			name = g_strndup (s1 + 1, s2 - s1 - 1);
		}
		
		d(g_print ("found jid:'%s' with desc:'%s'\n", jid, name));
		
		if (jid) {
			list = g_list_append (list, jid);
		}

		g_free (name);
	}
	
	d(g_print ("found %d servers\n", g_list_length (list)));

	return list;
}

gboolean
gossip_disco_protocols_get_supported (GossipDiscoProtocolsFunc func,
				      gpointer                 user_data)
{
	GnomeVFSResult  result;
	GList          *files = NULL;
	GList          *l;
	GList          *protocol_list = NULL;
	gchar          *dir;

	g_return_val_if_fail (func != NULL, FALSE);

	d(g_print ("attempting to get a list of protocols with uri:'%s'\n", PROTOCOLSDIR));

	/* 
	 * look up files packaged with Gossip 
	 */
	result = gnome_vfs_directory_list_load (&files, PROTOCOLSDIR, 
						GNOME_VFS_FILE_INFO_DEFAULT | 
						GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
						GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

	if (result != GNOME_VFS_OK) {
		disco_servers_print_error (result, PROTOCOLSDIR);  
		return FALSE;
	}

	if (!files || g_list_length (files) < 1) {
		d(g_print ("no protocol xml files found in %s\n", PROTOCOLSDIR));
		return TRUE;
	}
	
	for (l=files; l; l=l->next) {
		GossipDiscoProtocol *protocol;
		GnomeVFSFileInfo    *file;
		gchar               *file_with_path;

		file = (GnomeVFSFileInfo*)l->data;

		if (file->mime_type && strcmp (file->mime_type, "text/xml") != 0) {
			continue;
		}

		file_with_path = g_build_filename (PROTOCOLSDIR, file->name, NULL);

		if (!disco_protocols_validate_file (file_with_path)) {
			g_free (file_with_path);
			continue;
		}

		protocol = disco_protocols_parse_file (file_with_path);

		if (protocol) {
			protocol_list = g_list_append (protocol_list, protocol);
		}

		g_free (file_with_path);
	}

	/* 
	 * look up user files in ~/.gnome2/Gossip/protocols 
	 */
	dir = g_build_filename (g_get_home_dir (), ".gnome2", "Gossip", "protocols", NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		mkdir (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	}

	d(g_print ("attempting to get a list of protocols with uri:'%s'\n", dir));

	result = gnome_vfs_directory_list_load (&files, dir, 
						GNOME_VFS_FILE_INFO_DEFAULT | 
						GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
						GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

	if (result != GNOME_VFS_OK) {
		g_free (dir);
		disco_servers_print_error (result, PROTOCOLSDIR);  
		return FALSE;
	}

	if (!files || g_list_length (files) < 1) {
		g_free (dir);
		d(g_print ("no protocol xml files found in %s\n", PROTOCOLSDIR));
		return TRUE;
	}
	
	for (l=files; l; l=l->next) {
		GossipDiscoProtocol *protocol;
		GnomeVFSFileInfo    *file;
		gchar               *file_with_path;

		file = (GnomeVFSFileInfo*)l->data;

		if (file->mime_type && strcmp (file->mime_type, "text/xml") != 0) {
			continue;
		}

		file_with_path = g_build_filename (dir, file->name, NULL);

		if (!disco_protocols_validate_file (file_with_path)) {
			g_free (file_with_path);
			continue;
		}

		protocol = disco_protocols_parse_file (file_with_path);

		if (protocol) {
			protocol_list = g_list_append (protocol_list, protocol);
		}

		g_free (file_with_path);
	}
	
	(func)(protocol_list, user_data);

	g_free (dir);
	
	return TRUE;
}

static gboolean
disco_protocols_validate_file (const char *filename)
{

    xmlParserCtxtPtr ctxt;
    xmlDocPtr        doc; 
    
    gboolean         ret = FALSE;

    g_return_val_if_fail (filename != NULL, FALSE);

    d(g_print ("attempting to validate file (against DTD):'%s'\n", filename));

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

static GossipDiscoProtocol *
disco_protocols_parse_file (const gchar *filename) 
{
	GossipDiscoProtocol *protocol = NULL;

	xmlDocPtr            doc;
	xmlTextReaderPtr     reader;
	int                  ret;

	g_return_val_if_fail (filename != NULL, FALSE);

	d(g_print ("attempting to parse file:'%s'...\n", filename));
	
	reader = xmlReaderForFile(filename, NULL, 0);
	if (reader == NULL) {
		g_warning ("could not create xml reader for file:'%s' filename",
			   filename);
		return NULL;
	}

        if (xmlTextReaderPreservePattern(reader, "preserved", NULL) < 0) {
		g_warning ("could not preserve pattern for file:'%s' filename",
			   filename);
		return NULL;
	}

	ret = xmlTextReaderRead(reader);

	if (ret == 1) {
		protocol = disco_protocol_new ();
	}
	
	while (ret == 1) {
		const gchar *node = NULL;
		const gchar *value = NULL;

		if (!(node = xmlTextReaderConstName(reader))) {
			continue;
		}
			
		if (strcmp (node, "name") == 0 ||
		    strcmp (node, "description") == 0 ||
		    strcmp (node, "disco-type") == 0 ||
		    strcmp (node, "icon") == 0 ||
		    strcmp (node, "stock-icon") == 0) {
			const gchar *node_text = NULL;

			if ((ret = xmlTextReaderRead(reader)) != 1) {
				break;
			}
			
			node_text = xmlTextReaderConstName(reader);
			
			if (!value && strcmp (node_text, "#text") == 0) {
				value = xmlTextReaderConstValue(reader);
			}
		}

		if (strcmp (node, "server-list") == 0) {
			xmlChar *url;

			/* get attributes */
			url = xmlTextReaderGetAttribute (reader, "url");

			if (url) {
				protocol->url = g_strdup (url);
				xmlFree (url);
			}
			
			while ((ret = xmlTextReaderRead(reader)) == 1) {
				GossipDiscoServer *server;
				xmlChar           *name, *host, *port;
				const gchar       *server_node; 

				server_node = xmlTextReaderConstName(reader);

				if (!server_node || strcmp (server_node, "server") != 0) {
					continue;
				}

				/* get attributes */
				name = xmlTextReaderGetAttribute (reader, "name");
				host = xmlTextReaderGetAttribute (reader, "host");
				port = xmlTextReaderGetAttribute (reader, "port");

				server = disco_server_new (name, host);

				if (server && port) {
					server->port = g_strdup (port);
				}

				/* add to list */
				protocol->servers = g_list_append (protocol->servers, server);

				xmlFree (name);
				xmlFree (host);
				xmlFree (port);
			}
			
			continue;
		}
		
		if (node && value) {
			if (!protocol->name && strcmp (node, "name") == 0) {
				protocol->name = g_strdup (value);
			} else if (!protocol->description && strcmp (node, "description") == 0) {
				protocol->description = g_strdup (value);
			} else if (!protocol->disco_type && strcmp (node, "disco-type") == 0) {
				protocol->disco_type = g_strdup (value);
			} else if (!protocol->icon && strcmp (node, "icon") == 0) {
				protocol->icon = g_strdup (value);
			} else if (!protocol->stock_icon && strcmp (node, "stock-icon") == 0) {
				protocol->stock_icon = g_strdup (value);
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
	
	d(g_print ("protocol name:'%s'\n"
		   "protocol description:'%s'\n"
		   "protocol disco_type:'%s'\n"
		   "protocol icon:'%s'\n"
		   "protocol stock_icon:'%s'\n"
		   "protocol url:'%s'\n"
		   "protocol servers:%d\n",
		   protocol->name,
		   protocol->description,
		   protocol->disco_type,
		   protocol->icon,
		   protocol->stock_icon,
		   protocol->url, 
		   g_list_length (protocol->servers)));
	
	d(g_print ("cleaning up parser for file:'%s'\n\n", filename));
	  
	doc = xmlTextReaderCurrentDoc(reader);
	xmlFreeDoc(doc);

	xmlCleanupParser();
	xmlFreeTextReader(reader);
	
	return protocol;
}

static GossipDiscoProtocol *
disco_protocol_new ()
{
	GossipDiscoProtocol *protocol;

	protocol = g_new0 (GossipDiscoProtocol, 1);
	
	/* should we set a default icon here? */

	return protocol;
}

void
gossip_disco_protocol_free (GossipDiscoProtocol *protocol)
{
	g_return_if_fail (protocol != NULL);
	
	g_free (protocol->name);
	g_free (protocol->description);
	g_free (protocol->disco_type);
	g_free (protocol->icon);
	g_free (protocol->stock_icon);
	
	g_free (protocol->url);
	
	if (protocol->servers) {
		g_list_foreach (protocol->servers, (GFunc)gossip_disco_server_free, NULL);
		g_list_free (protocol->servers);
	}
	
	g_free (protocol);
}

const gchar *
gossip_disco_protocol_get_name (GossipDiscoProtocol *protocol)
{
	g_return_val_if_fail (protocol != NULL, NULL);

	return protocol->name;
}

const gchar *
gossip_disco_protocol_get_description (GossipDiscoProtocol *protocol)
{
	g_return_val_if_fail (protocol != NULL, NULL);

	return protocol->description;
}

const gchar *
gossip_disco_protocol_get_disco_type (GossipDiscoProtocol *protocol)
{
	g_return_val_if_fail (protocol != NULL, NULL);

	return protocol->disco_type;
}

const gchar *
gossip_disco_protocol_get_icon (GossipDiscoProtocol *protocol)
{
	g_return_val_if_fail (protocol != NULL, NULL);

	return protocol->icon;
}

const gchar *
gossip_disco_protocol_get_stock_icon (GossipDiscoProtocol *protocol)
{
	g_return_val_if_fail (protocol != NULL, NULL);

	return protocol->stock_icon;
}

const gchar *
gossip_disco_protocol_get_url (GossipDiscoProtocol *protocol)
{
	g_return_val_if_fail (protocol != NULL, NULL);

	return protocol->url;
}

GList *
gossip_disco_protocol_get_servers (GossipDiscoProtocol *protocol)
{
	g_return_val_if_fail (protocol != NULL, NULL);

	return protocol->servers;
}

static GossipDiscoServer *
disco_server_new (const gchar *name, const gchar *host)
{
	GossipDiscoServer *server;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (host != NULL, NULL);

	server = g_new0 (GossipDiscoServer, 1);
	
	server->name = g_strdup (name);
	server->host = g_strdup (host);

	return server;
}

void
gossip_disco_server_free (GossipDiscoServer *server)
{
	g_return_if_fail (server != NULL);
	
	g_free (server->name);
	g_free (server->host);
	g_free (server->port);
	
	g_free (server);
}

const gchar *
gossip_disco_server_get_name (GossipDiscoServer *server)
{
	g_return_val_if_fail (server != NULL, NULL);

	return server->name;
}

const gchar *
gossip_disco_server_get_host (GossipDiscoServer *server)
{
	g_return_val_if_fail (server != NULL, NULL);

	return server->host;
}

gint 
gossip_disco_server_get_port (GossipDiscoServer *server)
{
	g_return_val_if_fail (server != NULL, 5222);

	return atoi (server->port);
}

