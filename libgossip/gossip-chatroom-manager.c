/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
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
 */

#include "config.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "gossip-debug.h"
#include "gossip-chatroom-manager.h"
#include "gossip-account-manager.h"
#include "gossip-private.h"
#include "gossip-utils.h"

#include "libgossip-marshal.h"

#define DEBUG_DOMAIN "ChatroomManager"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CHATROOM_MANAGER, GossipChatroomManagerPriv))

#define CHATROOMS_XML_FILENAME "chatrooms.xml"
#define CHATROOMS_DTD_FILENAME "gossip-chatroom.dtd"

typedef struct _GossipChatroomManagerPriv GossipChatroomManagerPriv;

struct _GossipChatroomManagerPriv {
	GossipAccountManager *account_manager;
	GossipContactManager *contact_manager;

	GList                *chatrooms;

	gchar                *chatrooms_file_name;
	gchar                *default_name;
};

static void     chatroom_manager_finalize             (GObject                  *object);
static void     chatroom_manager_chatroom_enabled_cb  (GossipChatroom           *chatroom,
						       GParamSpec               *arg1,
						       GossipChatroomManager    *manager);
static void     chatroom_manager_chatroom_favorite_cb (GossipChatroom           *chatroom,
						       GParamSpec               *arg1,
						       GossipChatroomManager    *manager);
static void     chatroom_manager_chatroom_name_cb     (GossipChatroom           *chatroom,
						       GParamSpec               *arg1,
						       GossipChatroomManager    *manager);
static gboolean chatroom_manager_get_all              (GossipChatroomManager    *manager);
static gboolean chatroom_manager_file_parse           (GossipChatroomManager    *manager,
						       const gchar              *filename);
static gboolean chatroom_manager_file_save            (GossipChatroomManager    *manager);

enum {
	CHATROOM_ADDED,
	CHATROOM_REMOVED,
	CHATROOM_ENABLED,
	CHATROOM_FAVORITE_UPDATE,
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
	signals[CHATROOM_FAVORITE_UPDATE] =
		g_signal_new ("chatroom-favorite-update",
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

	g_free (priv->chatrooms_file_name);
	g_free (priv->default_name);

	g_list_foreach (priv->chatrooms, (GFunc)g_object_unref, NULL);
	g_list_free (priv->chatrooms);

	if (priv->contact_manager) {
		g_object_unref (priv->contact_manager);
	}

	if (priv->account_manager) {
		g_object_unref (priv->account_manager);
	}

	(G_OBJECT_CLASS (gossip_chatroom_manager_parent_class)->finalize) (object);
}

GossipChatroomManager *
gossip_chatroom_manager_new (GossipAccountManager *account_manager,
			     GossipContactManager *contact_manager,
			     const gchar          *filename)
{

	GossipChatroomManager     *manager;
	GossipChatroomManagerPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (account_manager), NULL);
	g_return_val_if_fail (GOSSIP_IS_CONTACT_MANAGER (contact_manager), NULL);

	manager = g_object_new (GOSSIP_TYPE_CHATROOM_MANAGER, NULL);

	priv = GET_PRIV (manager);

	if (account_manager) {
		priv->account_manager = g_object_ref (account_manager);
	}

	if (contact_manager) {
		priv->contact_manager = g_object_ref (contact_manager);
	}

	if (filename) {
		priv->chatrooms_file_name = g_strdup (filename);
	}

	/* Load file */
	chatroom_manager_get_all (manager);

	return manager;
}

static gint
chatroom_sort_func (gconstpointer a,
		    gconstpointer b)
{
	const gchar *name_a;
	const gchar *name_b;

	name_a = gossip_chatroom_get_name (GOSSIP_CHATROOM (a));
	name_b = gossip_chatroom_get_name (GOSSIP_CHATROOM (b));

	if (name_a && !name_b) {
		return -1;
	}

	if (name_b && !name_a) {
		return +1;
	}

	if (!name_a && !name_b) {
		return 0;
	}

	return strcmp (name_a, name_b);
}

gboolean
gossip_chatroom_manager_add (GossipChatroomManager *manager,
			     GossipChatroom        *chatroom)
{
	GossipChatroomManagerPriv *priv;
	GossipContact             *own_contact;
	const gchar               *name;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), FALSE);

	priv = GET_PRIV (manager);

	/* Don't add more than once */
	if (gossip_chatroom_manager_find (manager, gossip_chatroom_get_id (chatroom))) {
		return FALSE;
	}
       
	name = gossip_chatroom_get_name (chatroom);
	
	gossip_debug (DEBUG_DOMAIN, "Adding %s chatroom with name:'%s'",
		      gossip_chatroom_get_auto_connect (chatroom) ? "connecting on startup " : "",
		      name);

	/* Make sure we have an 'own_contact' */
	own_contact = gossip_contact_manager_find_or_create (priv->contact_manager,
							     gossip_chatroom_get_account (chatroom),
							     GOSSIP_CONTACT_TYPE_CHATROOM,
							     gossip_chatroom_get_own_contact_id_str (chatroom),
							     NULL);
	
	gossip_chatroom_set_own_contact (chatroom, own_contact);
	
	g_signal_connect (chatroom, "notify::enabled",
			  G_CALLBACK (chatroom_manager_chatroom_enabled_cb),
			  manager);
	
	g_signal_connect (chatroom, "notify::favorite",
			  G_CALLBACK (chatroom_manager_chatroom_favorite_cb),
			  manager);

	g_signal_connect (chatroom, "notify::name",
			  G_CALLBACK (chatroom_manager_chatroom_name_cb),
			  manager);
	
	priv->chatrooms = g_list_insert_sorted (priv->chatrooms, 
						g_object_ref (chatroom),
						chatroom_sort_func);
	
	g_signal_emit (manager, signals[CHATROOM_ADDED], 0, chatroom);
	
	return TRUE;
}

void
gossip_chatroom_manager_remove (GossipChatroomManager *manager,
				GossipChatroom        *chatroom)
{
	GossipChatroomManagerPriv *priv;
	GList                     *link;

	g_return_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));

	priv = GET_PRIV (manager);

	gossip_debug (DEBUG_DOMAIN,
		      "Removing chatroom with name:'%s'",
		      gossip_chatroom_get_name (chatroom));

	g_signal_handlers_disconnect_by_func (chatroom,
					      chatroom_manager_chatroom_name_cb,
					      manager);

	g_signal_handlers_disconnect_by_func (chatroom,
					      chatroom_manager_chatroom_favorite_cb,
					      manager);

	g_signal_handlers_disconnect_by_func (chatroom,
					      chatroom_manager_chatroom_enabled_cb,
					      manager);

	link = g_list_find (priv->chatrooms, chatroom);
	if (link) {
		priv->chatrooms = g_list_delete_link (priv->chatrooms, link);
		g_signal_emit (manager, signals[CHATROOM_REMOVED], 0, chatroom);
		g_object_unref (chatroom);
	}
}

void
gossip_chatroom_manager_set_index (GossipChatroomManager *manager,
				   GossipChatroom        *chatroom,
				   gint                   index)
{
	GossipChatroomManagerPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));

	priv = GET_PRIV (manager);

	gossip_debug (DEBUG_DOMAIN,
		      "Indexing chatroom with name:'%s' to position %d",
		      gossip_chatroom_get_name (chatroom), index);

	priv->chatrooms = g_list_remove (priv->chatrooms, chatroom);
	priv->chatrooms = g_list_insert (priv->chatrooms, chatroom, index);
}

static void
chatroom_manager_chatroom_enabled_cb (GossipChatroom        *chatroom,
				      GParamSpec            *arg1,
				      GossipChatroomManager *manager)
{
	g_signal_emit (manager, signals[CHATROOM_ENABLED], 0, chatroom);
}

static void
chatroom_manager_chatroom_favorite_cb (GossipChatroom        *chatroom,
				       GParamSpec            *arg1,
				       GossipChatroomManager *manager)
{
	GossipChatroomManagerPriv *priv;

	/* We sort here so we resort the list when a chatroom is
	 * marked as a favorite, this way, newly added favorites
	 * appear correctly in the right order.
	 */
	priv = GET_PRIV (manager);

	priv->chatrooms = g_list_sort (priv->chatrooms, chatroom_sort_func);

	g_signal_emit (manager, signals[CHATROOM_FAVORITE_UPDATE], 0, chatroom);
}

static void
chatroom_manager_chatroom_name_cb (GossipChatroom        *chatroom,
				   GParamSpec            *arg1,
				   GossipChatroomManager *manager)
{
	GossipChatroomManagerPriv *priv;

	/* We sort here so we resort the list when a chatroom name 
	 * changes, this way, the list in the favorites menu stays up
	 * to date.
	 */
	priv = GET_PRIV (manager);

	priv->chatrooms = g_list_sort (priv->chatrooms, chatroom_sort_func);
}

GList *
gossip_chatroom_manager_get_chatrooms (GossipChatroomManager *manager,
				       GossipAccount         *account)
{
	GossipChatroomManagerPriv *priv;
	GList                     *chatrooms, *l;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), NULL);

	priv = GET_PRIV (manager);

	if (!account) {
		return g_list_copy (priv->chatrooms);
	}

	chatrooms = NULL;

	for (l = priv->chatrooms; l; l = l->next) {
		GossipChatroom *chatroom;
		GossipAccount  *this_account;

		chatroom = l->data;
		this_account = gossip_chatroom_get_account (chatroom);

		if (!this_account) {
			continue;
		}

		if (gossip_account_equal (account, this_account)) {
			chatrooms = g_list_prepend (chatrooms, chatroom);
		}
	}

	return g_list_reverse (chatrooms);
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

GossipChatroom *
gossip_chatroom_manager_find_or_create (GossipChatroomManager *manager,
					GossipAccount         *account,
					const gchar           *server,
					const gchar           *room,
					gboolean              *created)
{
	GossipChatroom *chatroom;
	GList          *found;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_MANAGER (manager), NULL);
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (server != NULL, NULL);
	g_return_val_if_fail (room != NULL, NULL);

	found = gossip_chatroom_manager_find_extended (manager, 
						       account, 
						       server,
						       room);

	if (g_list_length (found) > 1) {
		g_warning ("Expected ONE chatroom to be found, actually found %d, using first", 
			   g_list_length (found));
	}

	if (created) {
		*created = found == NULL;
	}

	if (found) {
		chatroom = found->data;
		g_list_free (found);
	} else {
		chatroom = gossip_chatroom_new (account, server, room);
		gossip_chatroom_manager_add (manager, chatroom);
	}

	return chatroom;
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

	gossip_debug (DEBUG_DOMAIN,
		      "Setting default chatroom with name:'%s'",
		      gossip_chatroom_get_name (chatroom));

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

	gossip_debug (DEBUG_DOMAIN, "Saving chatrooms");

	return chatroom_manager_file_save (manager);
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
	gchar                     *name, *nick, *server;
	gchar                     *room, *password, *account_name;
	gboolean                   auto_connect;

	priv = GET_PRIV (manager);

	/* Default values. */
	name = NULL;
	nick = NULL;
	server = NULL;
	room = NULL;
	password = NULL;
	auto_connect = TRUE;
	account_name = NULL;

	child = node->children;

	while (child) {
		gchar *tag;

		if (xmlNodeIsText (child)) {
			child = child->next;
			continue;
		}

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
		else if (strcmp (tag, "account") == 0) {
			account_name = g_strdup (str);
		}

		xmlFree (str);

		child = child->next;
	}

	if (account_name && name && server && room) {
		GossipAccount *account;
		
		account = gossip_account_manager_find (priv->account_manager,
						       account_name);
		
		chatroom = gossip_chatroom_new (account, server, room);
		gossip_chatroom_set_name (chatroom, name);
		gossip_chatroom_set_auto_connect (chatroom, auto_connect);
		gossip_chatroom_set_favorite (chatroom, TRUE);

		if (nick) {
			gossip_chatroom_set_nick (chatroom, nick);
		}

		if (password) {
			gossip_chatroom_set_password (chatroom, password);
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

	priv = GET_PRIV (manager);

	gossip_debug (DEBUG_DOMAIN, "Attempting to parse file:'%s'...", filename);

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

	gossip_debug (DEBUG_DOMAIN,
		      "Parsed %d chatrooms",
		      g_list_length (priv->chatrooms));

	gossip_debug (DEBUG_DOMAIN,
		      "Default chatroom is:'%s'",
		      priv->default_name);

	xmlFreeDoc(doc);
	xmlFreeParserCtxt (ctxt);

	return TRUE;
}

static gboolean
chatroom_manager_file_save (GossipChatroomManager *manager)
{
	GossipChatroomManagerPriv *priv;
	xmlDocPtr                 doc;
	xmlNodePtr                root;
	GList                    *chatrooms;
	GList                    *l;
	gchar                    *dir;
	gchar                    *file;

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

	doc = xmlNewDoc ("1.0");
	root = xmlNewNode (NULL, "chatrooms");
	xmlDocSetRootElement (doc, root);

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
		xmlNodePtr      node;

		chatroom = l->data;

		/* We only save favorites */
		if (!gossip_chatroom_get_favorite (chatroom)) {
			continue;
		}

		node = xmlNewChild (root, NULL, "chatroom", NULL);
		xmlNewChild (node, NULL, "type", "normal"); /* We should remove this */
		xmlNewTextChild (node, NULL, "name", gossip_chatroom_get_name (chatroom));
		xmlNewTextChild (node, NULL, "nick", gossip_chatroom_get_nick (chatroom));

		xmlNewTextChild (node, NULL, "server", gossip_chatroom_get_server (chatroom));
		xmlNewTextChild (node, NULL, "room", gossip_chatroom_get_room (chatroom));

		xmlNewTextChild (node, NULL, "password", gossip_chatroom_get_password (chatroom));
		xmlNewChild (node, NULL, "auto_connect", gossip_chatroom_get_auto_connect (chatroom) ? "yes" : "no");

		account = gossip_chatroom_get_account (chatroom);
		xmlNewTextChild (node, NULL, "account", gossip_account_get_name (account));
	}

	/* Make sure the XML is indented properly */
	xmlIndentTreeOutput = 1;

	gossip_debug (DEBUG_DOMAIN, "Saving file:'%s'", file);
	xmlSaveFormatFileEnc (file, doc, "utf-8", 1);
	xmlFreeDoc (doc);

	xmlCleanupParser ();
	xmlMemoryDump ();

	g_list_free (chatrooms);

	g_free (file);

	return TRUE;
}
