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
	GossipAccount         *account;

	GossipChatroomId       id;
	gchar                 *id_str;

	gchar                 *name;
	gchar                 *description;
	gchar                 *subject;
	gchar                 *nick;
	gchar                 *server;
	gchar                 *room;
	gchar                 *password;
	gboolean               auto_connect;
	gboolean               favourite;

	GossipChatroomFeature  features;
	GossipChatroomStatus   status;

	guint                  occupants;

	GossipChatroomError    last_error;

	GHashTable            *contacts;
};

static void gossip_chatroom_class_init (GossipChatroomClass *klass);
static void gossip_chatroom_init       (GossipChatroom      *chatroom);
static void chatroom_finalize          (GObject             *object);
static void chatroom_get_property      (GObject             *object,
					guint                param_id,
					GValue              *value,
					GParamSpec          *pspec);
static void chatroom_set_property      (GObject             *object,
					guint                param_id,
					const GValue        *value,
					GParamSpec          *pspec);

enum {
	PROP_0,
	PROP_ACCOUNT,
	PROP_ID,
	PROP_ID_STR,
	PROP_NAME,
	PROP_DESCRIPTION,
	PROP_SUBJECT,
	PROP_NICK,
	PROP_SERVER,
	PROP_ROOM,
	PROP_PASSWORD,
	PROP_AUTO_CONNECT,
	PROP_FAVOURITE,
	PROP_FEATURES,
	PROP_STATUS,
	PROP_OCCUPANTS,
	PROP_LAST_ERROR,
};

enum {
	CONTACT_JOINED,
	CONTACT_LEFT,
	CONTACT_INFO_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

GType
gossip_chatroom_error_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] = {
			/* MUC errors */
			{ GOSSIP_CHATROOM_ERROR_NONE, 
			  "GOSSIP_CHATROOM_ERROR_NONE", 
			  "none" },
			{ GOSSIP_CHATROOM_ERROR_PASSWORD_INVALID_OR_MISSING, 
			  "GOSSIP_CHATROOM_ERROR_PASSWORD_INVALID_OR_MISSING", 
			  "password invalid or missing" },
			{ GOSSIP_CHATROOM_ERROR_USER_BANNED, 
			  "GOSSIP_CHATROOM_ERROR_USER_BANNED", 
			  "user banned" },
			{ GOSSIP_CHATROOM_ERROR_ROOM_NOT_FOUND, 
			  "GOSSIP_CHATROOM_ERROR_ROOM_NOT_FOUND", 
			  "room not found" },
			{ GOSSIP_CHATROOM_ERROR_ROOM_CREATION_RESTRICTED, 
			  "GOSSIP_CHATROOM_ERROR_ROOM_CREATION_RESTRICTED", 
			  "room creation restricted" },
			{ GOSSIP_CHATROOM_ERROR_USE_RESERVED_ROOM_NICK, 
			  "GOSSIP_CHATROOM_ERROR_USE_RESERVED_ROOM_NICK", 
			  "use reserved room nick" },
			{ GOSSIP_CHATROOM_ERROR_NOT_ON_MEMBERS_LIST, 
			  "GOSSIP_CHATROOM_ERROR_NOT_ON_MEMBERS_LIST", 
			  "not on members list" },
			{ GOSSIP_CHATROOM_ERROR_NICK_IN_USE, 
			  "GOSSIP_CHATROOM_ERROR_NICK_IN_USE", 
			  "nick in use" },
			{ GOSSIP_CHATROOM_ERROR_MAXIMUM_USERS_REACHED, 
			  "GOSSIP_CHATROOM_ERROR_MAXIMUM_USERS_REACHED", 
			  "maximum users reached" },
			/* Internal errors */
			{ GOSSIP_CHATROOM_ERROR_ALREADY_OPEN, 
			  "GOSSIP_CHATROOM_ERROR_ALREADY_OPEN", 
			  "already open" },
			{ GOSSIP_CHATROOM_ERROR_TIMED_OUT, 
			  "GOSSIP_CHATROOM_ERROR_TIMED_OUT", 
			  "timed out" },
			{ GOSSIP_CHATROOM_ERROR_CANCELED, 
			  "GOSSIP_CHATROOM_ERROR_CANCELED", 
			  "canceled" },
			{ GOSSIP_CHATROOM_ERROR_UNKNOWN, 
			  "GOSSIP_CHATROOM_ERROR_UNKNOWN", 
			  "unknown" },
			{ 0, NULL, NULL }
		};

		etype = g_enum_register_static ("GossipChatroomError", values);
	}

	return etype;
}

G_DEFINE_TYPE (GossipChatroom, gossip_chatroom, G_TYPE_OBJECT);

static void
gossip_chatroom_class_init (GossipChatroomClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = chatroom_finalize;
	object_class->get_property = chatroom_get_property;
	object_class->set_property = chatroom_set_property;

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
					 g_param_spec_string ("id-str",
							      "Chatroom String ID",
							      "Chatroom represented as 'room@server'",
							      NULL,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Chatroom Name",
							      "The friendly name for this chatroom",
							      "",
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_DESCRIPTION,
					 g_param_spec_string ("description",
							      "Chatroom Description",
							      "A more detailed description of this chatroom",
							      "",
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_SUBJECT,
					 g_param_spec_string ("subject",
							      "Chatroom Subject",
							      "The conversation subject",
							      "Random Crap",
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
					 g_param_spec_boolean ("auto-connect",
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
					 PROP_FEATURES,
					 g_param_spec_int ("features",
							   "Chatroom Features",
							   "Features of the room, hidden, passworded, etc",
							   G_MININT,
							   G_MAXINT,
							   0,
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
					 PROP_OCCUPANTS,
					 g_param_spec_uint ("occupants",
							   "Chatroom Occupant Count",
							   "The number of estimated occupants in the room",
							   0,
							   G_MAXUINT,
							   0,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_LAST_ERROR,
					 g_param_spec_enum ("last-error",
							    "Last Error",
							    "The last error that was given when trying to connect",
							    gossip_chatroom_error_get_type (),
							    GOSSIP_CHATROOM_ERROR_NONE,
							    G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_ACCOUNT,
					 g_param_spec_object ("account",
							      "Chatroom Account",
							      "The account associated with an chatroom",
							      GOSSIP_TYPE_ACCOUNT,
							      G_PARAM_READWRITE));

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
gossip_chatroom_init (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;
	static guint        id = 1;

	priv = GET_PRIV (chatroom);

	priv->id           = id++;

	priv->auto_connect = FALSE;
	priv->favourite    = FALSE;

	priv->status       = GOSSIP_CHATROOM_STATUS_INACTIVE;

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

	if (priv->account) {
		g_object_unref (priv->account);
	}

	g_hash_table_destroy (priv->contacts);

	(G_OBJECT_CLASS (gossip_chatroom_parent_class)->finalize) (object);
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
	case PROP_ACCOUNT:
		g_value_set_object (value, priv->account);
		break;
	case PROP_ID:
		g_value_set_int (value, priv->id);
		break;
	case PROP_ID_STR:
		g_value_set_string (value, priv->id_str);
		break;
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, priv->description);
		break;
	case PROP_SUBJECT:
		g_value_set_string (value, priv->subject);
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
	case PROP_FEATURES:
		g_value_set_int (value, priv->features);
		break;
	case PROP_STATUS:
		g_value_set_int (value, priv->status);
		break;
	case PROP_OCCUPANTS:
		g_value_set_uint (value, priv->occupants);
		break;
	case PROP_LAST_ERROR:
		g_value_set_enum (value, priv->last_error);
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
	case PROP_ACCOUNT:
		gossip_chatroom_set_account (GOSSIP_CHATROOM (object),
					     g_value_get_object (value));
		break;
	case PROP_NAME:
		gossip_chatroom_set_name (GOSSIP_CHATROOM (object),
					  g_value_get_string (value));
		break;
	case PROP_DESCRIPTION:
		gossip_chatroom_set_description (GOSSIP_CHATROOM (object),
						 g_value_get_string (value));
		break;
	case PROP_SUBJECT:
		gossip_chatroom_set_subject (GOSSIP_CHATROOM (object),
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
	case PROP_FEATURES:
		gossip_chatroom_set_features (GOSSIP_CHATROOM (object),
					      g_value_get_int (value));
		break;
	case PROP_STATUS:
		gossip_chatroom_set_status (GOSSIP_CHATROOM (object),
					    g_value_get_int (value));
		break;
	case PROP_OCCUPANTS:
		gossip_chatroom_set_occupants (GOSSIP_CHATROOM (object),
					       g_value_get_uint (value));
		break;
	case PROP_LAST_ERROR:
		gossip_chatroom_set_last_error (GOSSIP_CHATROOM (object),
						g_value_get_enum (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
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

GossipChatroomId
gossip_chatroom_get_id (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), 0);

	priv = GET_PRIV (chatroom);
	return priv->id;
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
gossip_chatroom_get_description (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (chatroom);
	return priv->description;
}

const gchar *
gossip_chatroom_get_subject (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (chatroom);
	return priv->subject;
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
gossip_chatroom_get_favourite (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), FALSE);

	priv = GET_PRIV (chatroom);

	return priv->favourite;
}

GossipChatroomFeature
gossip_chatroom_get_features (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), 0);

	priv = GET_PRIV (chatroom);

	return priv->features;
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

GossipChatroomStatus
gossip_chatroom_get_occupants (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), 0);

	priv = GET_PRIV (chatroom);

	return priv->occupants;
}

GossipChatroomError
gossip_chatroom_get_last_error (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), GOSSIP_CHATROOM_ERROR_NONE);

	priv = GET_PRIV (chatroom);

	return priv->last_error;
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
gossip_chatroom_set_description (GossipChatroom *chatroom,
				 const gchar    *description)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (description != NULL);

	priv = GET_PRIV (chatroom);

	g_free (priv->description);
	priv->description = g_strdup (description);

	g_object_notify (G_OBJECT (chatroom), "description");
}

void
gossip_chatroom_set_subject (GossipChatroom *chatroom,
			     const gchar    *subject)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (subject != NULL);

	priv = GET_PRIV (chatroom);

	g_free (priv->subject);
	priv->subject = g_strdup (subject);

	g_object_notify (G_OBJECT (chatroom), "subject");
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

	g_object_notify (G_OBJECT (chatroom), "auto-connect");
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
gossip_chatroom_set_features (GossipChatroom         *chatroom,
			      GossipChatroomFeature   features)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));

	priv = GET_PRIV (chatroom);

	priv->features = features;

	g_object_notify (G_OBJECT (chatroom), "features");
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
gossip_chatroom_set_occupants (GossipChatroom *chatroom,
			       guint           occupants)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));

	priv = GET_PRIV (chatroom);

	priv->occupants = occupants;

	g_object_notify (G_OBJECT (chatroom), "occupants");
}

void
gossip_chatroom_set_last_error (GossipChatroom      *chatroom,
				GossipChatroomError  last_error)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));

	priv = GET_PRIV (chatroom);

	priv->last_error = last_error;

	g_object_notify (G_OBJECT (chatroom), "last-error");
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
gossip_chatroom_error_to_string (GossipChatroomError error)
{
	switch (error) {
	case GOSSIP_CHATROOM_ERROR_PASSWORD_INVALID_OR_MISSING:
		return _("The chat room you tried to join requires a password. "
			 "You either failed to supply a password or the password you tried was incorrect.");
	case GOSSIP_CHATROOM_ERROR_USER_BANNED:
		return _("You have been banned from this chatroom.");
	case GOSSIP_CHATROOM_ERROR_ROOM_NOT_FOUND:
		return _("The conference room you tried to join could not be found.");
	case GOSSIP_CHATROOM_ERROR_ROOM_CREATION_RESTRICTED:
		return _("Chatroom creation is restricted on this server.");
	case GOSSIP_CHATROOM_ERROR_USE_RESERVED_ROOM_NICK:
		return _("Chatroom reserved nick names must be used on this server.");
	case GOSSIP_CHATROOM_ERROR_NOT_ON_MEMBERS_LIST:
		return _("You are not on the chatroom's members list.");
	case GOSSIP_CHATROOM_ERROR_NICK_IN_USE:
		return _("The nickname you have chosen is already in use.");
	case GOSSIP_CHATROOM_ERROR_MAXIMUM_USERS_REACHED:
		return _("The maximum number of users for this chatroom has been reached.");
	case GOSSIP_CHATROOM_ERROR_TIMED_OUT:
		return _("The remote conference server did not respond in a sensible time.");
	case GOSSIP_CHATROOM_ERROR_UNKNOWN:
		return _("An unknown error occurred, check your details are correct.");
	case GOSSIP_CHATROOM_ERROR_CANCELED:
		return _("Joining the chatroom was canceled.");
	default:
		break;
	}

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

gboolean 
gossip_chatroom_contact_can_message_all (GossipChatroom *chatroom,
					 GossipContact  *contact)
{
	GossipChatroomPriv        *priv;
	GossipChatroomContactInfo *info;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);

	priv = GET_PRIV (chatroom);

	info = gossip_chatroom_get_contact_info	(chatroom, contact);
	if (!info) {
		return FALSE;
	}

	if (info->role != GOSSIP_CHATROOM_ROLE_PARTICIPANT &&
	    info->role != GOSSIP_CHATROOM_ROLE_MODERATOR) {
		return FALSE;
	}
	
	return TRUE;
}

gboolean 
gossip_chatroom_contact_can_change_subject (GossipChatroom *chatroom,
					    GossipContact  *contact)
{
	GossipChatroomPriv        *priv;
	GossipChatroomContactInfo *info;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);

	priv = GET_PRIV (chatroom);

	info = gossip_chatroom_get_contact_info	(chatroom, contact);
	if (!info) {
		return FALSE;
	}

	if (info->role != GOSSIP_CHATROOM_ROLE_PARTICIPANT &&
	    info->role != GOSSIP_CHATROOM_ROLE_MODERATOR) {
		return FALSE;
	}
	
	return TRUE;
}

gboolean 
gossip_chatroom_contact_can_kick (GossipChatroom *chatroom,
				  GossipContact  *contact)
{
	GossipChatroomPriv        *priv;
	GossipChatroomContactInfo *info;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);

	priv = GET_PRIV (chatroom);

	info = gossip_chatroom_get_contact_info	(chatroom, contact);
	if (!info) {
		return FALSE;
	}

	if (info->role != GOSSIP_CHATROOM_ROLE_MODERATOR) {
		return FALSE;
	}
	
	return TRUE;
}

gboolean 
gossip_chatroom_contact_can_change_role (GossipChatroom *chatroom,
					 GossipContact  *contact)
{
	GossipChatroomPriv        *priv;
	GossipChatroomContactInfo *info;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);

	priv = GET_PRIV (chatroom);

	info = gossip_chatroom_get_contact_info	(chatroom, contact);
	if (!info) {
		return FALSE;
	}

	if (info->role != GOSSIP_CHATROOM_ROLE_MODERATOR) {
		return FALSE;
	}
	
	return TRUE;
}
