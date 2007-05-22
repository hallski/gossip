/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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

#include "config.h"

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "gossip-chatroom.h"

#include "libgossip-marshal.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CHATROOM, GossipChatroomPriv))

typedef struct _GossipChatroomPriv GossipChatroomPriv;

struct _GossipChatroomPriv {
	GossipAccount        *account;

	GossipChatroomType    type;

	GossipChatroomId      id;

	gchar                *id_str;

	gchar                *name;
	gchar                *nick;
	gchar                *server;
	gchar                *room;
	gchar                *password;
	gboolean              auto_connect;
	gboolean              favourite;

	GossipChatroomStatus  status;
	gchar                *last_error;

	GHashTable           *contacts;
};

struct _GossipChatroomInvite {
	guint          ref_count;

	GossipContact *invitor;
	gchar         *id;
	gchar         *reason;
};

static void chatroom_class_init   (GossipChatroomClass *class);
static void chatroom_init         (GossipChatroom      *chatroom);
static void chatroom_finalize     (GObject             *object);
static void chatroom_get_property (GObject             *object,
				   guint                param_id,
				   GValue              *value,
				   GParamSpec          *pspec);
static void chatroom_set_property (GObject             *object,
				   guint                param_id,
				   const GValue        *value,
				   GParamSpec          *pspec);

enum {
	PROP_0,
	PROP_ID,
	PROP_ID_STR,
	PROP_TYPE,
	PROP_NAME,
	PROP_NICK,
	PROP_SERVER,
	PROP_ROOM,
	PROP_PASSWORD,
	PROP_AUTO_CONNECT,
	PROP_FAVOURITE,
	PROP_STATUS,
	PROP_LAST_ERROR,
	PROP_ACCOUNT,
};

enum {
	CONTACT_JOINED,
	CONTACT_LEFT,
	CONTACT_INFO_CHANGED,
	LAST_SIGNAL
};

static guint    signals[LAST_SIGNAL] = {0};

static gpointer parent_class = NULL;

GType
gossip_chatroom_get_gtype (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GossipChatroomClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) chatroom_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GossipChatroom),
			0,    /* n_preallocs */
			(GInstanceInitFunc) chatroom_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "GossipChatroom",
					       &info, 0);
	}

	return type;
}

GType
gossip_chatroom_invite_get_gtype (void)
{
	static GType type_id = 0;

	if (!type_id) {
		type_id = g_boxed_type_register_static ("GossipChatroomInvite",
							(GBoxedCopyFunc) gossip_chatroom_invite_ref,
							(GBoxedFreeFunc) gossip_chatroom_invite_unref);
	}

	return type_id;
}

static void
chatroom_class_init (GossipChatroomClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	object_class->finalize     = chatroom_finalize;
	object_class->get_property = chatroom_get_property;
	object_class->set_property = chatroom_set_property;

	/*
	 * properties
	 */

	g_object_class_install_property (object_class,
					 PROP_ID,
					 g_param_spec_int ("id",
							   "Chatroom Id",
							   "Id to uniquely identify this room",
							   G_MININT,
							   G_MAXINT,
							   0,
							   G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_ID_STR,
					 g_param_spec_string ("id_str",
							      "Chatroom String ID",
							      "Chatroom represented as 'room@server'",
							      NULL,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_TYPE,
					 g_param_spec_int ("type",
							   "Chatroom Type",
							   "The chatroom type, e.g. normal",
							   G_MININT,
							   G_MAXINT,
							   GOSSIP_CHATROOM_TYPE_NORMAL,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Chatroom Name",
							      "What you call this chatroom",
							      "Default",
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_NICK,
					 g_param_spec_string ("nick",
							      "Chatroom Nick Name",
							      "For example 'the boy wonder'",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_SERVER,
					 g_param_spec_string ("server",
							      "Chatroom Server",
							      "Machine to connect to",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_ROOM,
					 g_param_spec_string ("room",
							      "Chatroom Room",
							      "Room name",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_PASSWORD,
					 g_param_spec_string ("password",
							      "Chatroom Password",
							      "Authentication token",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_AUTO_CONNECT,
					 g_param_spec_boolean ("auto_connect",
							       "Chatroom Auto Connect",
							       "Connect on startup",
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_FAVOURITE,
					 g_param_spec_boolean ("favourite",
							       "Chatroom Favourite",
							       "Used to connect favourites quickly",
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_STATUS,
					 g_param_spec_int ("status",
							   "Chatroom Status",
							   "Status of the room, open, closed, etc",
							   G_MININT,
							   G_MAXINT,
							   GOSSIP_CHATROOM_STATUS_UNKNOWN,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_LAST_ERROR,
					 g_param_spec_string ("last_error",
							      "Last Error",
							      "The last error that was given when trying to connect",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_ACCOUNT,
					 g_param_spec_object ("account",
							      "Chatroom Account",
							      "The account associated with an chatroom",
							      GOSSIP_TYPE_ACCOUNT,
							      G_PARAM_READWRITE));

	/*
	 * signals
	 */

	signals[CONTACT_JOINED] =
		g_signal_new ("contact-joined",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_CONTACT);
	signals[CONTACT_LEFT] =
		g_signal_new ("contact-left",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_CONTACT);
	signals[CONTACT_INFO_CHANGED] =
		g_signal_new ("contact-info-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_CONTACT);

	g_type_class_add_private (object_class, sizeof (GossipChatroomPriv));
}

static void
chatroom_init (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;
	static guint        id = 1;

	priv = GET_PRIV (chatroom);

	priv->id           = id++;
	priv->id_str       = NULL;

	priv->type         = 0;

	priv->name         = NULL;
	priv->nick         = NULL;
	priv->server       = NULL;
	priv->room         = NULL;
	priv->password     = NULL;

	priv->auto_connect = FALSE;
	priv->favourite    = FALSE;

	priv->status       = GOSSIP_CHATROOM_STATUS_INACTIVE;
	priv->last_error   = NULL;

	priv->account      = NULL;
	priv->contacts     = g_hash_table_new_full (gossip_contact_hash,
						    gossip_contact_equal,
						    (GDestroyNotify) g_object_unref,
						    g_free);
}

static void
chatroom_finalize (GObject *object)
{
	GossipChatroomPriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->id_str);

	g_free (priv->name);
	g_free (priv->nick);
	g_free (priv->server);
	g_free (priv->room);
	g_free (priv->password);

	g_free (priv->last_error);

	if (priv->account) {
		g_object_unref (priv->account);
	}

	g_hash_table_destroy (priv->contacts);

	(G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
chatroom_get_property (GObject    *object,
		       guint       param_id,
		       GValue     *value,
		       GParamSpec *pspec)
{
	GossipChatroomPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ID:
		g_value_set_int (value, priv->id);
		break;
	case PROP_ID_STR:
		g_value_set_string (value, priv->id_str);
		break;
	case PROP_TYPE:
		g_value_set_int (value, priv->type);
		break;
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_NICK:
		g_value_set_string (value, priv->nick);
		break;
	case PROP_SERVER:
		g_value_set_string (value, priv->server);
		break;
	case PROP_ROOM:
		g_value_set_string (value, priv->room);
		break;
	case PROP_PASSWORD:
		g_value_set_string (value, priv->password);
		break;
	case PROP_AUTO_CONNECT:
		g_value_set_boolean (value, priv->auto_connect);
		break;
	case PROP_FAVOURITE:
		g_value_set_boolean (value, priv->favourite);
		break;
	case PROP_STATUS:
		g_value_set_int (value, priv->status);
		break;
	case PROP_LAST_ERROR:
		g_value_set_string (value, priv->last_error);
		break;
	case PROP_ACCOUNT:
		g_value_set_object (value, priv->account);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
chatroom_set_property (GObject      *object,
		       guint         param_id,
		       const GValue *value,
		       GParamSpec   *pspec)
{
	GossipChatroomPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_TYPE:
		priv->type = g_value_get_int (value);
		break;
	case PROP_NAME:
		gossip_chatroom_set_name (GOSSIP_CHATROOM (object),
					  g_value_get_string (value));
		break;
	case PROP_NICK:
		gossip_chatroom_set_nick (GOSSIP_CHATROOM (object),
				       g_value_get_string (value));
		break;
	case PROP_SERVER:
		gossip_chatroom_set_server (GOSSIP_CHATROOM (object),
					   g_value_get_string (value));
		break;
	case PROP_ROOM:
		gossip_chatroom_set_room (GOSSIP_CHATROOM (object),
					  g_value_get_string (value));
		break;
	case PROP_PASSWORD:
		gossip_chatroom_set_password (GOSSIP_CHATROOM (object),
					     g_value_get_string (value));
		break;
	case PROP_AUTO_CONNECT:
		gossip_chatroom_set_auto_connect (GOSSIP_CHATROOM (object),
						 g_value_get_boolean (value));
		break;
	case PROP_FAVOURITE:
		gossip_chatroom_set_favourite (GOSSIP_CHATROOM (object),
					       g_value_get_boolean (value));
		break;
	case PROP_STATUS:
		gossip_chatroom_set_status (GOSSIP_CHATROOM (object),
					    g_value_get_int (value));
		break;
	case PROP_LAST_ERROR:
		gossip_chatroom_set_last_error (GOSSIP_CHATROOM (object),
						g_value_get_string (value));
		break;
	case PROP_ACCOUNT:
		gossip_chatroom_set_account (GOSSIP_CHATROOM (object),
					     g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

GossipChatroomId
gossip_chatroom_get_id (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), 0);

	priv = GET_PRIV (chatroom);
	return priv->id;
}

GossipChatroomType
gossip_chatroom_get_type (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), 0);

	priv = GET_PRIV (chatroom);
	return priv->type;
}

const gchar *
gossip_chatroom_get_id_str (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (chatroom);

	if (!priv->id_str) {
		priv->id_str = g_strdup ("");
	}

	return priv->id_str;
}

const gchar *
gossip_chatroom_get_name (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (chatroom);
	return priv->name;
}

const gchar *
gossip_chatroom_get_nick (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (chatroom);
	return priv->nick;
}

const gchar *
gossip_chatroom_get_server (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (chatroom);
	return priv->server;
}

const gchar *
gossip_chatroom_get_room (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (chatroom);
	return priv->room;
}

const gchar *
gossip_chatroom_get_password (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (chatroom);
	return priv->password;
}

gboolean
gossip_chatroom_get_auto_connect (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), FALSE);

	priv = GET_PRIV (chatroom);
	return priv->auto_connect;
}

gboolean
gossip_chatroom_get_is_favourite (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), FALSE);

	priv = GET_PRIV (chatroom);
	return priv->favourite;
}

GossipChatroomStatus
gossip_chatroom_get_status (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom),
			      GOSSIP_CHATROOM_STATUS_UNKNOWN);

	priv = GET_PRIV (chatroom);

	return priv->status;
}

const gchar *
gossip_chatroom_get_last_error (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (chatroom);
	return priv->last_error;
}

GossipAccount *
gossip_chatroom_get_account (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (chatroom);

	if (!priv->account) {
		return NULL;
	}

	return priv->account;
}

GossipChatroomContactInfo *
gossip_chatroom_get_contact_info (GossipChatroom *chatroom,
				  GossipContact  *contact)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (chatroom);

	return g_hash_table_lookup (priv->contacts, contact);
}

void
gossip_chatroom_set_name (GossipChatroom *chatroom,
			  const gchar    *name)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (name != NULL);

	priv = GET_PRIV (chatroom);

	g_free (priv->name);
	priv->name = g_strdup (name);

	g_object_notify (G_OBJECT (chatroom), "name");
}

void
gossip_chatroom_set_nick (GossipChatroom *chatroom,
			  const gchar    *nick)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (nick != NULL);

	priv = GET_PRIV (chatroom);

	g_free (priv->nick);
	priv->nick = g_strdup (nick);

	g_object_notify (G_OBJECT (chatroom), "nick");
}

void
gossip_chatroom_set_server (GossipChatroom *chatroom,
			    const gchar    *server)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (server != NULL);

	priv = GET_PRIV (chatroom);

	g_free (priv->server);
	priv->server = g_strdup (server);

	if (priv->room && priv->server) {
		g_free (priv->id_str);
		priv->id_str = g_strdup_printf ("%s@%s", priv->room, priv->server);
	}

	g_object_notify (G_OBJECT (chatroom), "server");
}

void
gossip_chatroom_set_room (GossipChatroom *chatroom,
			  const gchar    *room)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (room != NULL);

	priv = GET_PRIV (chatroom);

	g_free (priv->room);
	priv->room = g_strdup (room);

	if (priv->room && priv->server) {
		g_free (priv->id_str);
		priv->id_str = g_strdup_printf ("%s@%s", priv->room, priv->server);
	}

	g_object_notify (G_OBJECT (chatroom), "room");
}

void
gossip_chatroom_set_password (GossipChatroom *chatroom,
			      const gchar    *password)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (password != NULL);

	priv = GET_PRIV (chatroom);

	g_free (priv->password);
	priv->password = g_strdup (password);

	g_object_notify (G_OBJECT (chatroom), "password");
}

void
gossip_chatroom_set_auto_connect (GossipChatroom *chatroom,
				  gboolean        auto_connect)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));

	priv = GET_PRIV (chatroom);
	priv->auto_connect = auto_connect;

	g_object_notify (G_OBJECT (chatroom), "auto_connect");
}

void
gossip_chatroom_set_favourite (GossipChatroom *chatroom,
			       gboolean        favourite)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));

	priv = GET_PRIV (chatroom);
	priv->favourite = favourite;

	g_object_notify (G_OBJECT (chatroom), "favourite");
}

static void
chatroom_clear_contacts (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	priv = GET_PRIV (chatroom);

	g_hash_table_remove_all (priv->contacts);
}

void
gossip_chatroom_set_status (GossipChatroom       *chatroom,
			    GossipChatroomStatus  status)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));

	priv = GET_PRIV (chatroom);

	priv->status = status;

	if (status == GOSSIP_CHATROOM_STATUS_INACTIVE) {
		chatroom_clear_contacts (chatroom);
	}

	g_object_notify (G_OBJECT (chatroom), "status");
}

void
gossip_chatroom_set_last_error (GossipChatroom *chatroom,
				const gchar    *last_error)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));

	priv = GET_PRIV (chatroom);

	g_free (priv->last_error);
	if (last_error) {
		priv->last_error = g_strdup (last_error);
	} else {
		priv->last_error = NULL;
	}

	g_object_notify (G_OBJECT (chatroom), "last_error");
}

void
gossip_chatroom_set_account (GossipChatroom *chatroom,
			     GossipAccount  *account)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GET_PRIV (chatroom);
	if (priv->account) {
		g_object_unref (priv->account);
	}

	priv->account = g_object_ref (account);

	g_object_notify (G_OBJECT (chatroom), "account");
}

void
gossip_chatroom_set_contact_info (GossipChatroom            *chatroom,
				  GossipContact             *contact,
				  GossipChatroomContactInfo *info)
{
	GossipChatroomPriv        *priv;
	GossipChatroomContactInfo *chatroom_contact;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	g_return_if_fail (info != NULL);

	priv = GET_PRIV (chatroom);

	chatroom_contact = g_new0 (GossipChatroomContactInfo, 1);
	chatroom_contact->role = info->role;
	chatroom_contact->affiliation = info->affiliation;
	
	g_hash_table_insert (priv->contacts, 
			     g_object_ref (contact),
			     chatroom_contact);

	g_signal_emit (chatroom, signals[CONTACT_INFO_CHANGED], 0, contact);
}

guint
gossip_chatroom_hash (gconstpointer key)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (key), 0);

	priv = GET_PRIV (key);

	return g_int_hash (&priv->id);
}

gboolean
gossip_chatroom_equal (gconstpointer a,
		       gconstpointer b)
{
	GossipChatroomPriv *priv1;
	GossipChatroomPriv *priv2;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (a), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (b), FALSE);

	priv1 = GET_PRIV (a);
	priv2 = GET_PRIV (b);

	if (!priv1 || !priv2) {
		return FALSE;
	}

	return (priv1->id == priv2->id);
}

gboolean
gossip_chatroom_equal_full (gconstpointer a,
			    gconstpointer b)
{
	GossipChatroomPriv *priv1;
	GossipChatroomPriv *priv2;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (a), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (b), FALSE);

	priv1 = GET_PRIV (a);
	priv2 = GET_PRIV (b);

	if (!priv1 || !priv2) {
		return FALSE;
	}

	if ((priv1->account && !priv2->account) ||
	    (priv2->account && !priv1->account) ||
	    !gossip_account_equal (priv1->account, priv2->account)) {
		return FALSE;
	}

	if ((priv1->name && !priv2->name) ||
	    (priv2->name && !priv1->name) ||
	    strcmp (priv1->name, priv2->name) != 0) {
		return FALSE;
	}

	if ((priv1->nick && !priv2->nick) ||
	    (priv2->nick && !priv1->nick) ||
	    strcmp (priv1->nick, priv2->nick) != 0) {
		return FALSE;
	}

	if ((priv1->server && !priv2->server) ||
	    (priv2->server && !priv1->server) ||
	    strcmp (priv1->server, priv2->server) != 0) {
		return FALSE;
	}

	if ((priv1->room && !priv2->room) ||
	    (priv2->room && !priv1->room) ||
	    strcmp (priv1->room, priv2->room) != 0) {
		return FALSE;
	}

	return TRUE;
}

const gchar *
gossip_chatroom_type_to_string (GossipChatroomType type)
{
	switch (type) {
	case GOSSIP_CHATROOM_TYPE_NORMAL: return _("Normal");
	}

	return "";
}

const gchar *
gossip_chatroom_status_to_string (GossipChatroomStatus status)
{
	switch (status) {
	case GOSSIP_CHATROOM_STATUS_JOINING:
		return _("Joining");
	case GOSSIP_CHATROOM_STATUS_ACTIVE:
		return _("Active");
	case GOSSIP_CHATROOM_STATUS_INACTIVE:
		return _("Inactive");
	case GOSSIP_CHATROOM_STATUS_UNKNOWN:
		return _("Unknown");
	case GOSSIP_CHATROOM_STATUS_ERROR:
		return _("Error");
	}

	g_warning ("Invalid chatroom status: %d", status);

	return "";
}

const gchar *
gossip_chatroom_role_to_string (GossipChatroomRole role,
				gint               nr)
{
	switch (role) {
	case GOSSIP_CHATROOM_ROLE_MODERATOR:
		return ngettext ("Moderator", "Moderators", nr);
	case GOSSIP_CHATROOM_ROLE_PARTICIPANT:
		return ngettext ("Participant", "Participants", nr);
	case GOSSIP_CHATROOM_ROLE_VISITOR:
		return ngettext ("Visitor", "Visitors", nr);
	case GOSSIP_CHATROOM_ROLE_NONE:
		return _("No role");
	}

	g_warning ("Invalid role: %d", role);

	return "";
}

const gchar *
gossip_chatroom_affiliation_to_string (GossipChatroomAffiliation affiliation,
				       gint                      nr)
{
	switch (affiliation) {
	case GOSSIP_CHATROOM_AFFILIATION_OWNER:
		return ngettext ("Owner", "Owners", nr);
	case GOSSIP_CHATROOM_AFFILIATION_ADMIN:
		return ngettext ("Administrator", "Administrators", nr);
	case GOSSIP_CHATROOM_AFFILIATION_MEMBER:
		return ngettext ("Member", "Members", nr);
	case GOSSIP_CHATROOM_AFFILIATION_OUTCAST:
		return ngettext ("Outcast", "Outcasts", nr);
	case GOSSIP_CHATROOM_AFFILIATION_NONE:
		return _("No affiliation");
	}

	g_warning ("Invalid affiliation: %d", affiliation);

	return "";
}

void
gossip_chatroom_contact_joined (GossipChatroom            *chatroom,
				GossipContact             *contact,
				GossipChatroomContactInfo *info)
{
	GossipChatroomPriv        *priv;
	GossipChatroomContactInfo *chatroom_contact;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (chatroom);

	chatroom_contact = g_new0 (GossipChatroomContactInfo, 1);
	if (info) {
		chatroom_contact->role = info->role;
		chatroom_contact->affiliation = info->affiliation;		
	} else {
		chatroom_contact->role = GOSSIP_CHATROOM_ROLE_NONE;
		chatroom_contact->affiliation = GOSSIP_CHATROOM_AFFILIATION_NONE;
	}
	if (!g_hash_table_lookup (priv->contacts, contact)) {
		g_hash_table_insert (priv->contacts,
				     g_object_ref (contact),
				     chatroom_contact);

		g_signal_emit (chatroom, signals[CONTACT_JOINED], 0, contact);
	}
}

void
gossip_chatroom_contact_left (GossipChatroom *chatroom,
			      GossipContact  *contact)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (chatroom);

	if (g_hash_table_lookup (priv->contacts, contact)) {
		g_signal_emit (chatroom, signals[CONTACT_LEFT], 0, contact);
		g_hash_table_remove (priv->contacts, contact);
	}
}

/*
 * Chatroom invite functions
 */

GossipChatroomInvite *
gossip_chatroom_invite_ref (GossipChatroomInvite *invite)
{
	g_return_val_if_fail (invite != NULL, NULL);
	g_return_val_if_fail (invite->ref_count > 0, NULL);

	invite->ref_count++;

	return invite;
}

void
gossip_chatroom_invite_unref (GossipChatroomInvite *invite)
{
	g_return_if_fail (invite != NULL);
	g_return_if_fail (invite->ref_count > 0);

	invite->ref_count--;

	if (invite->ref_count > 0) {
		return;
	}

	if (invite->invitor) {
		g_object_unref (invite->invitor);
	}

	g_free (invite->id);
	g_free (invite->reason);
}

GType
gossip_chatroom_invite_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static
			("GossipChatroomInvite",
			 (GBoxedCopyFunc) gossip_chatroom_invite_ref,
			 (GBoxedFreeFunc) gossip_chatroom_invite_unref);
	}

	return type;
}

GossipChatroomInvite *
gossip_chatroom_invite_new (GossipContact *invitor,
			    const gchar   *id,
			    const gchar   *reason)
{
	GossipChatroomInvite *invite;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (invitor), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	invite = g_new0 (GossipChatroomInvite, 1);

	invite->ref_count = 1;

	invite->invitor = g_object_ref (invitor);
	invite->id = g_strdup (id);

	if (reason) {
		invite->reason = g_strdup (reason);
	}

	return invite;
}

GossipContact *
gossip_chatroom_invite_get_invitor (GossipChatroomInvite *invite)
{
	g_return_val_if_fail (invite != NULL, NULL);

	return invite->invitor;
}

const gchar *
gossip_chatroom_invite_get_id (GossipChatroomInvite *invite)
{
	g_return_val_if_fail (invite != NULL, NULL);

	return invite->id;
}

const gchar *
gossip_chatroom_invite_get_reason (GossipChatroomInvite *invite)
{
	g_return_val_if_fail (invite != NULL, NULL);

	return invite->reason;
}

