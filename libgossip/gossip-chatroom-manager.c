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

#include "gossip-utils.h"
#include "gossip-chatroom-manager.h"
#include "gossip-protocol.h"

#define CHATROOMS_XML_FILENAME "chatrooms.xml"
#define CHATROOMS_DTD_FILENAME "gossip-chatroom.dtd"

#define DEBUG_MSG(x)
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n");  */

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CHATROOM_MANAGER, GossipChatroomManagerPriv))

typedef struct _GossipChatroomManagerPriv GossipChatroomManagerPriv;

struct _GossipChatroomManagerPriv {
	GossipAccountManager *account_manager;
	GossipSession        *session;

	GList                *chatrooms;
	GList                *chatrooms_to_start; /* for auto connect */

	gchar                *chatrooms_file_name;

	gchar                *default_name;
};

static void     chatroom_manager_finalize              (GObject                  *object);
static void     chatroom_manager_chatroom_enabled_cb   (GossipChatroom           *chatroom,
							GParamSpec               *arg1,
							GossipChatroomManager    *manager);
static void     chatroom_manager_chatroom_favourite_cb (GossipChatroom           *chatroom,
							GParamSpec               *arg1,
							GossipChatroomManager    *manager);
static gboolean chatroom_manager_get_all               (GossipChatroomManager    *manager);
static gboolean chatroom_manager_file_parse            (GossipChatroomManager    *manager,
							const gchar              *filename);
static gboolean chatroom_manager_file_save             (GossipChatroomManager    *manager);
static void     chatroom_manager_join_cb               (GossipChatroomProvider   *provider,
							GossipChatroomJoinResult  result,
							GossipChatroomId          id,
							GossipChatroomManager    *manager);
static void     chatroom_manager_protocol_connected_cb (GossipSession            *session,
							GossipAccount            *account,
							GossipProtocol           *protocol,
							GossipChatroomManager    *manager);

enum {
	CHATROOM_ADDED,
	CHATROOM_REMOVED, 
	CHATROOM_ENABLED,
	CHATROOM_AUTO_CONNECT_UPDATE,
	CHATROOM_FAVOURITE_UPDATE,
	NEW_DEFAULT,
	LAST_SIGNAL
};

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
        signals[CHATROOM_AUTO_CONNECT_UPDATE] = 
		g_signal_new ("chatroom-auto-connect-update",
			      G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
			      0, 
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT_OBJECT_INT,
			      G_TYPE_NONE,
			      3, 
			      GOSSIP_TYPE_CHATROOM_PROVIDER, 
			      GOSSIP_TYPE_CHATROOM, 
			      G_TYPE_INT);
        signals[CHATROOM_FAVOURITE_UPDATE] = 
		g_signal_new ("chatroom-favourite-update",
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
}

static void
chatroom_manager_finalize (GObject *object)
{
	GossipChatroomManagerPriv *priv;
	
	priv = GET_PRIV (object);

	g_list_foreach (priv->chatrooms, (GFunc)g_object_unref, NULL);
	g_list_free (priv->chatrooms);

	g_list_foreach (priv->chatrooms_to_start, (GFunc)g_object_unref, NULL);
	g_list_free (priv->chatrooms_to_start);

	if (priv->account_manager) {
		g_object_unref (priv->account_manager);
	}

	if (priv->session) {
		g_object_unref (priv->session);

		g_signal_handlers_disconnect_by_func (priv->session, 
						      chatroom_manager_protocol_connected_cb, 
						      NULL);
	}
	
	g_free (priv->chatrooms_file_name);

	g_free (priv->default_name);

	(G_OBJECT_CLASS (gossip_chatroom_manager_parent_class)->finalize) (object);
}

GossipChatroomManager *
gossip_chatroom_manager_new (GossipAccountManager *account_manager,
			     GossipSession        *session,
			     const gchar          *filename)
{
	
	GossipChatroomManager     *manager;
	GossipChatroomManagerPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (account_manager), NULL);
	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);
	
	manager = g_object_new (GOSSIP_TYPE_CHATROOM_MANAGER, NULL);

	priv = GET_PRIV (manager);

	if (account_manager) {
		priv->account_manager = g_object_ref (account_manager);
	}

	if (session) {
		priv->session = g_object_ref (session);
	}
	
	if (filename) {
		priv->chatrooms_file_name = g_strdup (filename);
	}

	/* load file */
	chatroom_manager_get_all (manager);

	/* connect protocol connected/disconnected signals */
	g_signal_connect (priv->session, 
			  "protocol-connected",
			  G_CALLBACK (chatroom_manager_protocol_connected_cb),
			  manager);

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

		DEBUG_MSG (("ChatroomManager: Adding %s%s chatroom with name:'%s'", 
			   gossip_chatroom_get_auto_connect (chatroom) ? "connecting on startup " : "", 
			   gossip_chatroom_type_to_string (type), 
			   name));
		
		g_signal_connect (chatroom, "notify::enabled", 
				  G_CALLBACK (chatroom_manager_chatroom_enabled_cb), 
				  manager);

		g_signal_connect (chatroom, "notify::favourite", 
				  G_CALLBACK (chatroom_manager_chatroom_favourite_cb), 
				  manager);

		priv->chatrooms = g_list_append (priv->chatrooms, 
						 g_object_ref (chatroom));

		if (gossip_chatroom_get_auto_connect (chatroom)) {
			priv->chatrooms_to_start = g_list_append (priv->chatrooms_to_start, 
								  g_object_ref (chatroom));
		}

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

 	DEBUG_MSG (("ChatroomManager: Removing chatroom with name:'%s'",  
 		   gossip_chatroom_get_name (chatroom)));

	g_signal_handlers_disconnect_by_func (chatroom, 
					      chatroom_manager_chatroom_enabled_cb, 
					      manager);

	g_signal_handlers_disconnect_by_func (chatroom, 
					      chatroom_manager_chatroom_favourite_cb, 
					      manager);

	priv->chatrooms = g_list_remove (priv->chatrooms, chatroom);
	priv->chatrooms_to_start = g_list_remove (priv->chatrooms_to_start, chatroom);
	
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

static void
chatroom_manager_chatroom_favourite_cb (GossipChatroom        *chatroom,
					GParamSpec            *arg1,
					GossipChatroomManager *manager)
{
	g_signal_emit (manager, signals[CHATROOM_FAVOURITE_UPDATE], 0, chatroom);
}

GList *
gossip_chatroom_manager_get_chatrooms (GossipChatroomManager *manager,
				       GossipAccount         *account)
{
	GossipChatroomManagerPriv *priv;
	GList                     *chatrooms = NULL;
	GList                     *l;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), NULL);

	priv = GET_PRIV (manager);

	if (!account) {
		return chatrooms = g_list_copy (priv->chatrooms);
	}	

	for (l = priv->chatrooms; l; l = l->next) {
		GossipChatroom *chatroom;
		GossipAccount  *this_account;
		
		chatroom = l->data;
		
		this_account = gossip_chatroom_get_account (chatroom);
		if (!this_account) {
			continue;
		}

		if (gossip_account_equal (account, this_account)) {
			chatrooms = g_list_append (chatrooms, chatroom);
		}
	}

	return chatrooms;
}

guint 
gossip_chatroom_manager_get_count (GossipChatroomManager *manager,
				   GossipAccount         *account)
{
	GossipChatroomManagerPriv *priv;
	GList                     *l;
	guint                      count = 0;
	
	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), 0);
	
	priv = GET_PRIV (manager);

	if (!account) {
		return g_list_length (priv->chatrooms);
	}

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), 0);
	
	for (l = priv->chatrooms; l; l = l->next) {
		GossipChatroom *chatroom;
		GossipAccount  *this_account;
		
		chatroom = l->data;
		
		this_account = gossip_chatroom_get_account (chatroom);
		if (!this_account) {
			continue;
		}

		if (gossip_account_equal (account, this_account)) {
			count++;
		}
	}

	return count;
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
				       GossipAccount         *account,
				       const gchar           *server,
				       const gchar           *room)
{
	GossipChatroomManagerPriv *priv;
	GList                     *found = NULL;
	GList                     *l;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), NULL);
	g_return_val_if_fail (server != NULL, NULL);
	g_return_val_if_fail (room != NULL, NULL);
	
	priv = GET_PRIV (manager);

	for (l = priv->chatrooms; l; l = l->next) {
		GossipChatroom *chatroom;
		const gchar    *chatroom_server;
		const gchar    *chatroom_room;
		gboolean        same_account = FALSE;

		chatroom = l->data;
		
		chatroom_server = gossip_chatroom_get_server (chatroom);
		chatroom_room = gossip_chatroom_get_room (chatroom);

		if (account) {
			GossipAccount *this_account;

			this_account = gossip_chatroom_get_account (chatroom);
			if (this_account) {
				same_account = gossip_account_equal (account, this_account);
			} else {
				same_account = FALSE;
			}
		}
		
		if (same_account && 
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
	
 	DEBUG_MSG (("ChatroomManager: Setting default chatroom with name:'%s'",  
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

 	DEBUG_MSG (("ChatroomManager: Saving chatrooms"));  
	
	return chatroom_manager_file_save (manager);
}

void
gossip_chatroom_manager_join_favourites (GossipChatroomManager *manager)
{
	GossipChatroomManagerPriv *priv;
	GList                     *l;

	g_return_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager));

	priv = GET_PRIV (manager);
	
	for (l = priv->chatrooms; l; l = l->next) {
		GossipChatroomProvider *provider;
		GossipChatroom         *chatroom;
		GossipAccount          *account;

		chatroom = l->data;

		if (!gossip_chatroom_get_is_favourite (chatroom)) {
			continue;
		}
		
		account = gossip_chatroom_get_account (chatroom);

		if (!gossip_session_is_connected (priv->session, account)) {
			continue;
		}
		
		provider = gossip_session_get_chatroom_provider (priv->session, account);
		
		gossip_chatroom_provider_join (provider,
					       chatroom,
					       (GossipChatroomJoinCb) chatroom_manager_join_cb,
					       manager);
	}
}
 
/*
 * API to save/load and parse the chatrooms file.
 */

static gboolean
chatroom_manager_get_all (GossipChatroomManager *manager)
{
	GossipChatroomManagerPriv *priv;
	gchar                     *dir;
	gchar                     *file_with_path = NULL;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), FALSE);

	priv = GET_PRIV (manager);

	/* Use default if no file specified. */
	if (!priv->chatrooms_file_name) {
		dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
		if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
			g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
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
				 xmlNodePtr             node)
{
	GossipChatroomManagerPriv *priv;
	GossipChatroom            *chatroom;
	xmlNodePtr                 child;
	gchar                     *str;
	GossipChatroomType         type;
	gchar                     *name, *nick, *server;
	gchar                     *room, *password, *account_name;
	gboolean                   auto_connect, favourite;

	priv = GET_PRIV (manager);

	/* default values. */
	type = GOSSIP_CHATROOM_TYPE_NORMAL;
	name = NULL;
	nick = NULL;
	server = NULL;
	room = NULL;
	password = NULL;
	auto_connect = TRUE;
	favourite = FALSE;
	account_name = NULL;

	child = node->children;

	while (child) {
		gchar *tag;

		tag = (gchar *) child->name;
		str = (gchar *) xmlNodeGetContent (child);

		if (strcmp (tag, "type") == 0) {
			/* we have no types yet */
		}
		else if (strcmp (tag, "name") == 0) {
			name = g_strdup (str);
		}
		else if (strcmp (tag, "nick") == 0) {
			nick = g_strdup (str);
		}
		else if (strcmp (tag, "server") == 0) {
			server = g_strdup (str);
		}
		else if (strcmp (tag, "room") == 0) {
			room = g_strdup (str);
		}
		else if (strcmp (tag, "password") == 0) {
			password = g_strdup (str);
		}
		else if (strcmp (tag, "auto_connect") == 0) {
			if (strcmp (str, "yes") == 0) {
				auto_connect = TRUE;
			} else {
				auto_connect = FALSE;
			}
		}
		else if (strcmp (tag, "favourite") == 0) {
			if (strcmp (str, "yes") == 0) {
				favourite = TRUE;
			} else {
				favourite = FALSE;
			}
		}
		else if (strcmp (tag, "account") == 0) {
			account_name = g_strdup (str);
		}

		xmlFree (str);

		child = child->next;
	}

	if (name && server && room) {
		chatroom = g_object_new (GOSSIP_TYPE_CHATROOM, 
					 "type", type,
					 "name", name,
					 "server", server,
					 "room", room,
					 "auto_connect", auto_connect,
					 "favourite", favourite,
					 NULL);
		
		if (nick) {
			gossip_chatroom_set_nick (chatroom, nick);
		}
		
		if (password) {
			gossip_chatroom_set_password (chatroom, password);
		}

		if (account_name) {
			GossipAccount *account = NULL;

			if (priv->account_manager) {
				account = gossip_account_manager_find (priv->account_manager, 
								       account_name);
			}

			if (account) {
				gossip_chatroom_set_account (chatroom, account);
			}
		}
		
		gossip_chatroom_manager_add (manager, chatroom);
		
		g_object_unref (chatroom);
	}
	
	g_free (name);
	g_free (nick);
	g_free (server);
	g_free (room);
	g_free (password);
	g_free (account_name);
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
	
	DEBUG_MSG (("ChatroomManager: Attempting to parse file:'%s'...", filename));

 	ctxt = xmlNewParserCtxt ();

	/* Parse and validate the file. */
	doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);	
	if (!doc) {
		g_warning ("Failed to parse file:'%s'", filename);
		xmlFreeParserCtxt (ctxt);
		return FALSE;
	}
	
	if (!gossip_xml_validate (doc, CHATROOMS_DTD_FILENAME)) {
		g_warning ("Failed to validate file:'%s'", filename);
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
	
	DEBUG_MSG (("ChatroomManager: Parsed %d chatrooms", 
		   g_list_length (priv->chatrooms)));

	DEBUG_MSG (("ChatroomManager: Default chatroom is:'%s'", 
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
	gchar                    *dir;
	gchar                    *file;
					
	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), FALSE);

	priv = GET_PRIV (manager);
	
	if (priv->chatrooms_file_name) {
		file = g_strdup (priv->chatrooms_file_name);
	} else {
		dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
		if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
			g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
		}
					 
		file = g_build_filename (dir, CHATROOMS_XML_FILENAME, NULL);
		g_free (dir);
	}

	dtd_file = g_build_filename (DTDDIR, CHATROOMS_DTD_FILENAME, NULL);

	doc = xmlNewDoc ("1.0");
	root = xmlNewNode (NULL, "chatrooms");
	xmlDocSetRootElement (doc, root);

	dtd = xmlCreateIntSubset (doc, "chatrooms", NULL, dtd_file);

	if (!priv->default_name) {
		priv->default_name = g_strdup ("Default");
	}
	
	xmlNewTextChild (root, NULL, 
			 "default", 
			 priv->default_name);

	chatrooms = gossip_chatroom_manager_get_chatrooms (manager, NULL);

	for (l = chatrooms; l; l = l->next) {
		GossipChatroom *chatroom;
		GossipAccount  *account;
		gchar          *type;
		xmlNodePtr      node;
	
		chatroom = l->data;

		switch (gossip_chatroom_get_type (chatroom)) {
		case GOSSIP_CHATROOM_TYPE_NORMAL:
		default:
			type = g_strdup_printf ("normal");
			break;
		}
	
		node = xmlNewChild (root, NULL, "chatroom", NULL);
		xmlNewChild (node, NULL, "type", type);
		xmlNewTextChild (node, NULL, "name", gossip_chatroom_get_name (chatroom));
		xmlNewTextChild (node, NULL, "nick", gossip_chatroom_get_nick (chatroom));

		xmlNewTextChild (node, NULL, "server", gossip_chatroom_get_server (chatroom));
		xmlNewTextChild (node, NULL, "room", gossip_chatroom_get_room (chatroom));

		xmlNewTextChild (node, NULL, "password", gossip_chatroom_get_password (chatroom));
		xmlNewChild (node, NULL, "auto_connect", gossip_chatroom_get_auto_connect (chatroom) ? "yes" : "no");
		xmlNewChild (node, NULL, "favourite", gossip_chatroom_get_is_favourite (chatroom) ? "yes" : "no");

		account = gossip_chatroom_get_account (chatroom);
		if (account) {
			xmlNewTextChild (node, NULL, "account", gossip_account_get_name (account));
			g_object_unref (account);
		}

		g_free (type);
	}

	DEBUG_MSG (("ChatroomManager: Saving file:'%s'", file));
	xmlSaveFormatFileEnc (file, doc, "utf-8", 1);
	xmlFreeDoc (doc);

	xmlCleanupParser ();

	xmlMemoryDump ();
	
	g_free (file);

	return TRUE;
}

static void
chatroom_manager_join_cb (GossipChatroomProvider   *provider,
			  GossipChatroomJoinResult  result,
			  GossipChatroomId          id,
			  GossipChatroomManager    *manager)
{
	GossipChatroom *chatroom;

	chatroom = gossip_chatroom_provider_find (provider, id);

	g_signal_emit (manager, signals[CHATROOM_AUTO_CONNECT_UPDATE], 0, 
		       provider, chatroom, result);
}

static void
chatroom_manager_protocol_connected_cb (GossipSession         *session,
					GossipAccount         *account,
					GossipProtocol        *protocol,
					GossipChatroomManager *manager)
{
	GossipChatroomManagerPriv *priv;
	GList                     *chatrooms, *l;

	g_return_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager));

	priv = GET_PRIV (manager);
	
	DEBUG_MSG (("ChatroomManager: Account:'%s' connected, "
		    "checking chatroom auto connects.",
		   gossip_account_get_name (account)));

	chatrooms = gossip_chatroom_manager_get_chatrooms (manager, account);

	for (l = chatrooms; l; l = l->next) {
		GossipChatroom *chatroom;

		chatroom = l->data;
		
		if (g_list_find (priv->chatrooms_to_start, chatroom)) {
			GossipChatroomProvider *provider;

			/* found in list, it needs to be connected to */
			provider = gossip_session_get_chatroom_provider (session, account);

			gossip_chatroom_provider_join (provider,
						       chatroom,
						       (GossipChatroomJoinCb) chatroom_manager_join_cb,
						       manager);

			/* take out of list */
			priv->chatrooms_to_start = g_list_remove (priv->chatrooms_to_start,
								  chatroom);
		}
	}
}
