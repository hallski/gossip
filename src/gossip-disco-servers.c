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
#include <gnome.h>

#include "gossip-disco-servers.h"

#define d(x) x


static int    disco_servers_print_error (GnomeVFSResult  result,
					 const char     *uri_string);

static GList *disco_servers_parse       (const char     *servers);



gboolean 
gossip_disco_servers_fetch (GossipDiscoServersFunc func,
			    gpointer               user_data)
{
	GnomeVFSResult  result;
	GList          *list;

	int             bytes_read = 0;
	char           *servers = NULL;

	const char     *uri = "http://www.jabber.org/servers.xml";

	g_return_val_if_fail (func != NULL, FALSE);

	if (!gnome_vfs_init ()) {
		g_warning ("could not initialize GnomeVFS");
		return FALSE;
	}
	
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
	const char *error_string = NULL;

	error_string = gnome_vfs_result_to_string (result);

	g_warning ("Error %s occured opening location %s\n", 
		   error_string, uri_string);

	return FALSE;
}

static GList *
disco_servers_parse (const char *servers)
{
	const gchar *marker = NULL;
	const gchar *item_str = "jid=";
	const gchar *name_str = "name=";

	GList       *list = NULL;

	g_return_val_if_fail (servers != NULL, NULL);

	d(g_print ("parsing server list (%d bytes)\n", strlen (servers))); 

	/* \n\n is what we look for that divides the HTTP header
	   and the HTTP body */
	marker = strstr (servers, "\n\n");

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

