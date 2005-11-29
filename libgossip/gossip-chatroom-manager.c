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
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "libgossip-marshal.h"

#include "gossip-chatroom-manager.h"

#define CHATROOMS_XML_FILENAME "chatrooms.xml"
#define CHATROOMS_DTD_FILENAME "gossip-chatroom.dtd"

#define d(x)

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CHATROOM_MANAGER, GossipChatroomManagerPriv))


typedef struct _GossipChatroomManagerPriv GossipChatroomManagerPriv;


struct _GossipChatroomManagerPriv {
	GList *chatrooms;

	gchar *chatrooms_file_name;

	gchar *default_name;
};

enum {
	CHATROOM_ADDED,
	CHATROOM_REMOVED, 
	CHATROOM_ENABLED,
	NEW_DEFAULT,
	LAST_SIGNAL
};


static void     chatroom_manager_finalize              (GObject                *object);
static void     chatroom_manager_chatroom_enabled_cb   (GossipChatroom         *chatroom,
							GParamSpec             *arg1,
							GossipChatroomManager  *manager);
static gboolean chatroom_manager_get_all               (GossipChatroomManager  *manager);
static gboolean chatroom_manager_file_parse            (GossipChatroomManager  *manager,
							const gchar            *filename);
static gboolean chatroom_manager_file_save             (GossipChatroomManager  *manager);


static guint  signals[LAST_SIGNAL] = {0};


G_DEFINE_TYPE (GossipChatroomManager, gossip_chatroom_manager, G_TYPE_OBJECT);


static void
gossip_chatroom_manager_class_init (GossipChatroomManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = chatroom_manager_finalize;


	signals[CHATROOM_ADDED] = 
		g_signal_new ("chatroom-added",
                              G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
                              NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_CHATROOM);
        signals[CHATROOM_REMOVED] = 
		g_signal_new ("chatroom-removed",
			      G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
			      0, 
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_CHATROOM);
        signals[CHATROOM_ENABLED] = 
		g_signal_new ("chatroom-enabled",
			      G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
			      0, 
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_CHATROOM);
        signals[NEW_DEFAULT] = 
		g_signal_new ("new-default",
			      G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
			      0, 
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_CHATROOM);

	g_type_class_add_private (object_class, 
				  sizeof (GossipChatroomManagerPriv));
}

static void
gossip_chatroom_manager_init (GossipChatroomManager *manager)
{
	GossipChatroomManagerPriv *priv;

	priv = GET_PRIV (manager);
}

static void
chatroom_manager_finalize (GObject *object)
{
	GossipChatroomManagerPriv *priv;
	
	priv = GET_PRIV (object);

	g_list_foreach (priv->chatrooms, (GFunc)g_object_unref, NULL);
	g_list_free (priv->chatrooms);

	g_free (priv->chatrooms_file_name);

	g_free (priv->default_name);
}

GossipChatroomManager *
gossip_chatroom_manager_new (const gchar *filename)
{
	
	GossipChatroomManager     *manager;
	GossipChatroomManagerPriv *priv;

	manager = g_object_new (GOSSIP_TYPE_CHATROOM_MANAGER, NULL);

	priv = GET_PRIV (manager);
	
	if (filename) {
		priv->chatrooms_file_name = g_strdup (filename);
	}

	/* load file */
	chatroom_manager_get_all (manager);

	return manager;
}

gboolean          
gossip_chatroom_manager_add (GossipChatroomManager *manager,
			     GossipChatroom        *chatroom)
{
	GossipChatroomManagerPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), FALSE);

	priv = GET_PRIV (manager);

	/* don't add more than once */
 	if (!gossip_chatroom_manager_find (manager, gossip_chatroom_get_id (chatroom))) { 
		const gchar        *name;
		GossipChatroomType  type;

		type = gossip_chatroom_get_type (chatroom);
		name = gossip_chatroom_get_name (chatroom);

		d(g_print ("Chatroom Manager: Adding %s%s chatroom with name:'%s'\n", 
			   gossip_chatroom_get_auto_connect (chatroom) ? "connecting on startup " : "", 
			   gossip_chatroom_get_type_as_str (type), 
			   name));
		
		g_signal_connect (chatroom, "notify::enabled", 
				  G_CALLBACK (chatroom_manager_chatroom_enabled_cb), 
				  manager);

		priv->chatrooms = g_list_append (priv->chatrooms, 
						 g_object_ref (chatroom));

		g_signal_emit (manager, signals[CHATROOM_ADDED], 0, chatroom);

		return TRUE;
	} 

	return FALSE;
}

void          
gossip_chatroom_manager_remove (GossipChatroomManager *manager,
				GossipChatroom        *chatroom)
{
	GossipChatroomManagerPriv *priv;
	
	g_return_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));

	priv = GET_PRIV (manager);

 	d(g_print ("Chatroom Manager: Removing chatroom with name:'%s'\n",  
 		   gossip_chatroom_get_name (chatroom)));

	g_signal_handlers_disconnect_by_func (chatroom, 
					      chatroom_manager_chatroom_enabled_cb, 
					      manager);

	priv->chatrooms = g_list_remove (priv->chatrooms, chatroom);

	g_signal_emit (manager, signals[CHATROOM_REMOVED], 0, chatroom);

	g_object_unref (chatroom);
}

static void
chatroom_manager_chatroom_enabled_cb (GossipChatroom        *chatroom,
				      GParamSpec            *arg1,
				      GossipChatroomManager *manager)
{
	g_signal_emit (manager, signals[CHATROOM_ENABLED], 0, chatroom);
}

GList *
gossip_chatroom_manager_get_chatrooms (GossipChatroomManager *manager)
{
	GossipChatroomManagerPriv *priv;
	GList                     *chatrooms;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), NULL);

	priv = GET_PRIV (manager);

	chatrooms = g_list_copy (priv->chatrooms);
	g_list_foreach (chatrooms, (GFunc)g_object_ref, NULL);

	return chatrooms;
}

guint 
gossip_chatroom_manager_get_count (GossipChatroomManager *manager)
{
	GossipChatroomManagerPriv *priv;
	
	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), 0);
	
	priv = GET_PRIV (manager);
	
	return g_list_length (priv->chatrooms);
}


GossipChatroom *
gossip_chatroom_manager_find (GossipChatroomManager *manager,
			      GossipChatroomId       id)
{
	GossipChatroomManagerPriv *priv;
	GList                     *l;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), NULL);
	g_return_val_if_fail (id >= 1, NULL);
	
	priv = GET_PRIV (manager);

	for (l = priv->chatrooms; l; l = l->next) {
		GossipChatroom *chatroom;

		chatroom = l->data;

		if (gossip_chatroom_get_id (chatroom) == id) {
			return chatroom;
		}
	}

	return NULL;
}	

GList *
gossip_chatroom_manager_find_extended (GossipChatroomManager *manager,
				       const gchar           *nick,
				       const gchar           *server,
				       const gchar           *room)
{
	GossipChatroomManagerPriv *priv;
	GList                     *found = NULL;
	GList                     *l;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), NULL);
	g_return_val_if_fail (nick != NULL, NULL);
	g_return_val_if_fail (server != NULL, NULL);
	g_return_val_if_fail (room != NULL, NULL);
	
	priv = GET_PRIV (manager);

	for (l = priv->chatrooms; l; l = l->next) {
		GossipChatroom *chatroom;
		const gchar    *chatroom_nick;
		const gchar    *chatroom_server;
		const gchar    *chatroom_room;

		chatroom = l->data;
		
		chatroom_nick = gossip_chatroom_get_nick (chatroom);
		chatroom_server = gossip_chatroom_get_server (chatroom);
		chatroom_room = gossip_chatroom_get_room (chatroom);
		
		if (strcmp (nick, chatroom_nick) == 0 &&
		    strcmp (server, chatroom_server) == 0 &&
		    strcmp (room, chatroom_room) == 0) {
			found = g_list_append (found, g_object_ref (chatroom));
		}
	}

	return found;
}

void 
gossip_chatroom_manager_set_default (GossipChatroomManager *manager,
				     GossipChatroom        *chatroom)
{
	GossipChatroomManagerPriv *priv;
	const gchar              *name;

	g_return_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));

	priv = GET_PRIV (manager);
	
 	d(g_print ("Chatroom Manager: Setting default chatroom with name:'%s'\n",  
 		   gossip_chatroom_get_name (chatroom))); 

	name = gossip_chatroom_get_name (chatroom);
	g_return_if_fail (name != NULL);

	g_free (priv->default_name);
	priv->default_name = g_strdup (name);

	/* save */
	gossip_chatroom_manager_add (manager, chatroom);

	g_signal_emit (manager, signals[NEW_DEFAULT], 0, chatroom);
}

GossipChatroom *
gossip_chatroom_manager_get_default (GossipChatroomManager *manager)
{
	GossipChatroomManagerPriv *priv;
	GList                     *l;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), FALSE);

	priv = GET_PRIV (manager);

	for (l = priv->chatrooms; l && priv->default_name; l = l->next) {
		GossipChatroom *chatroom;
		const gchar    *chatroom_name;

		chatroom = l->data;
		chatroom_name = gossip_chatroom_get_name (chatroom);

		if (chatroom_name && strcmp (chatroom_name, priv->default_name) == 0) {
			return chatroom;
		}
	}

	if (g_list_length (priv->chatrooms) >= 1) {
		return (GossipChatroom*)g_list_nth_data (priv->chatrooms, 0);
	}

	return NULL;
}

gboolean
gossip_chatroom_manager_store (GossipChatroomManager *manager)
{
	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), FALSE);

 	d(g_print ("Chatroom Manager: Saving chatrooms\n"));  
	
	return chatroom_manager_file_save (manager);
}
 
/*
 * API to save/load and parse the chatrooms file.
 */

static gboolean
chatroom_manager_get_all (GossipChatroomManager *manager)
{
	GossipChatroomManagerPriv *priv;
	gchar                    *dir;
	gchar                    *file_with_path = NULL;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), FALSE);

	priv = GET_PRIV (manager);

	/* Use default if no file specified. */
	if (!priv->chatrooms_file_name) {
		dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
		if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
			mkdir (dir, S_IRUSR | S_IWUSR | S_IXUSR);
		}
		
		file_with_path = g_build_filename (dir, CHATROOMS_XML_FILENAME, NULL);
		g_free (dir);
	} else {
		file_with_path = g_strdup (priv->chatrooms_file_name);
	}

	/* read file in */
	if (g_file_test (file_with_path, G_FILE_TEST_EXISTS) &&
	    !chatroom_manager_file_parse (manager, file_with_path)) {
		g_free (file_with_path);
		return FALSE;
	}
	
	g_free (file_with_path);

	return TRUE;
}

static void
chatroom_manager_parse_chatroom (GossipChatroomManager *manager,
				 xmlNodePtr            node)
{
	GossipChatroom     *chatroom;
	xmlNodePtr          child;
	gchar              *str;
	GossipChatroomType  type;
	gchar              *name, *nick, *server, *room, *password;
	gboolean            auto_connect;

	/* Default values. */
	type = GOSSIP_CHATROOM_TYPE_NORMAL;
	name = NULL;
	nick = NULL;
	server = NULL;
	room = NULL;
	password = NULL;
	auto_connect = TRUE;

	child = node->children;
	while (child) {
		gchar *tag;

		tag = (gchar *) child->name;
		str = (gchar *) xmlNodeGetContent (child);

		if (strcmp (tag, "type") == 0) {
			/* we have no types yet */
			xmlFree (str);
		}
		else if (strcmp (tag, "name") == 0) {
			name = str;
		}
		else if (strcmp (tag, "nick") == 0) {
			nick = str;
		}
		else if (strcmp (tag, "server") == 0) {
			server = str;
		}
		else if (strcmp (tag, "room") == 0) {
			room = str;
		}
		else if (strcmp (tag, "password") == 0) {
			password = str;
		}
		else if (strcmp (tag, "auto_connect") == 0) {
			if (strcmp (str, "yes") == 0) {
				auto_connect = TRUE;
			} else {
				auto_connect = FALSE;
			}
			xmlFree (str);
		}

		child = child->next;
	}

	if (name && server && room) {
		chatroom = g_object_new (GOSSIP_TYPE_CHATROOM, 
					 "type", type,
					 "name", name,
					 "server", server,
					 "room", room,
					 "auto_connect", auto_connect,
					 NULL);
		
		if (nick) {
			gossip_chatroom_set_nick (chatroom, nick);
		}
		
		if (password) {
			gossip_chatroom_set_password (chatroom, password);
		}
		
		gossip_chatroom_manager_add (manager, chatroom);
		
		g_object_unref (chatroom);
	}
	
	xmlFree (name);
	xmlFree (nick);
	xmlFree (server);
	xmlFree (room);
	xmlFree (password);
}

static gboolean
chatroom_manager_file_parse (GossipChatroomManager *manager, 
			     const gchar          *filename) 
{
	GossipChatroomManagerPriv *priv;
 	xmlParserCtxtPtr          ctxt;
	xmlDocPtr                 doc;
	xmlNodePtr                chatrooms;
	xmlNodePtr                node;
	gchar                    *str;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	priv = GET_PRIV (manager);
	
	d(g_print ("Chatroom Manager: Attempting to parse file:'%s'...\n", filename));

 	ctxt = xmlNewParserCtxt ();

	/* Parse and validate the file. */
	doc = xmlCtxtReadFile (ctxt, filename, NULL, XML_PARSE_DTDVALID);	
	if (!doc) {
		g_warning ("Failed to parse file:'%s'", filename);
		xmlFreeParserCtxt (ctxt);
		return FALSE;
	}

	if (!ctxt->valid) {
		g_warning ("Failed to validate file:'%s'",  filename);
		xmlFreeDoc(doc);
		xmlFreeParserCtxt (ctxt);
		return FALSE;
	}

	/* The root node, chatrooms. */
	chatrooms = xmlDocGetRootElement (doc);
	
	node = chatrooms->children;
	while (node) {
		if (strcmp ((gchar *) node->name, "default") == 0) {
			/* Get the default chatroom. */
			str = (gchar *) xmlNodeGetContent (node);

			g_free (priv->default_name);
			priv->default_name = g_strdup (str);
			xmlFree (str);
		}
		else if (strcmp ((gchar *) node->name, "chatroom") == 0) {
			chatroom_manager_parse_chatroom (manager, node);
		}

		node = node->next;
	}
	
	d(g_print ("Chatroom Manager: Parsed %d chatrooms\n", 
		   g_list_length (priv->chatrooms)));

	d(g_print ("Chatroom Manager: Default chatroom is:'%s'\n", 
		   priv->default_name));
	
	xmlFreeDoc(doc);
	xmlFreeParserCtxt (ctxt);
	
	return TRUE;
}

static gboolean
chatroom_manager_file_save (GossipChatroomManager *manager)
{
	GossipChatroomManagerPriv *priv;
	xmlDocPtr                 doc;  
	xmlDtdPtr                 dtd;  
	xmlNodePtr                root;
	GList                    *chatrooms;
	GList                    *l;
	gchar                    *dtd_file;
	gchar                    *xml_dir;
	gchar                    *xml_file;
					
	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), FALSE);

	priv = GET_PRIV (manager);
	
	if (priv->chatrooms_file_name) {
		xml_file = g_strdup (priv->chatrooms_file_name);
	} else {
		xml_dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
		if (!g_file_test (xml_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
			mkdir (xml_dir, S_IRUSR | S_IWUSR | S_IXUSR);
		}
					 
		xml_file = g_build_filename (xml_dir, CHATROOMS_XML_FILENAME, NULL);
		g_free (xml_dir);
	}

	dtd_file = g_build_filename (DTDDIR, CHATROOMS_DTD_FILENAME, NULL);

	doc = xmlNewDoc (BAD_CAST "1.0");
	root = xmlNewNode (NULL, BAD_CAST "chatrooms");
	xmlDocSetRootElement (doc, root);

	dtd = xmlCreateIntSubset (doc, BAD_CAST "chatrooms", NULL, BAD_CAST dtd_file);

	if (!priv->default_name) {
		priv->default_name = g_strdup ("Default");
	}
	
	xmlNewChild (root, NULL, 
		     BAD_CAST "default", 
		     BAD_CAST priv->default_name);

	chatrooms = gossip_chatroom_manager_get_chatrooms (manager);

	for (l = chatrooms; l; l = l->next) {
		GossipChatroom *chatroom;
		gchar          *type;
		xmlNodePtr      node;
	
		chatroom = l->data;

		switch (gossip_chatroom_get_type (chatroom)) {
		case GOSSIP_CHATROOM_TYPE_NORMAL:
		default:
			type = g_strdup_printf ("normal");
			break;
		}
	
		node = xmlNewChild (root, NULL, BAD_CAST "chatroom", NULL);
		xmlNewChild (node, NULL, BAD_CAST "type", BAD_CAST type);
		xmlNewChild (node, NULL, BAD_CAST "name", BAD_CAST gossip_chatroom_get_name (chatroom));
		xmlNewChild (node, NULL, BAD_CAST "nick", BAD_CAST gossip_chatroom_get_nick (chatroom));

		xmlNewChild (node, NULL, BAD_CAST "server", BAD_CAST gossip_chatroom_get_server (chatroom));
		xmlNewChild (node, NULL, BAD_CAST "room", BAD_CAST gossip_chatroom_get_room (chatroom));

		xmlNewChild (node, NULL, BAD_CAST "password", BAD_CAST gossip_chatroom_get_password (chatroom));
		xmlNewChild (node, NULL, BAD_CAST "auto_connect", BAD_CAST (gossip_chatroom_get_auto_connect (chatroom) ? "yes" : "no"));

		g_free (type);
	}

	d(g_print ("Chatroom Manager: Saving file:'%s'\n", xml_file));
	xmlSaveFormatFileEnc (xml_file, doc, "utf-8", 1);
	xmlFreeDoc (doc);

	xmlCleanupParser ();

	xmlMemoryDump ();
	
	g_free (xml_file);

	return TRUE;
}


