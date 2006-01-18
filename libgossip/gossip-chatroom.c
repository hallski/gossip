/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Martyn Russell <mr@gnome.org>
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

#include <glib.h>
#include <glib/gi18n.h>

#include "libgossip-marshal.h"

#include "gossip-chatroom.h"

#define GOSSIP_CHATROOM_GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CHATROOM, GossipChatroomPriv))

#define d(x)


typedef struct _GossipChatroomPriv GossipChatroomPriv;


struct _GossipChatroomPriv {
	GossipChatroomType    type;

	GossipChatroomId      id;
	
	gchar                *name;
	gchar                *nick;
	gchar                *server;
	gchar                *room;
	gchar                *password;
	gboolean              auto_connect;
	
	GossipChatroomStatus  status;
	gchar                *last_error;

	GossipAccount        *account;
};


static void     chatroom_class_init     (GossipChatroomClass *class);
static void     chatroom_init           (GossipChatroom      *chatroom);
static void     chatroom_finalize       (GObject            *object);
static void     chatroom_get_property   (GObject            *object,
					guint               param_id,
					GValue             *value,
					GParamSpec         *pspec);
static void     chatroom_set_property   (GObject            *object,
					guint               param_id,
					const GValue       *value,
					GParamSpec         *pspec);


enum {
	PROP_0,
	PROP_ID,
	PROP_TYPE,
	PROP_NAME,
	PROP_NICK,
	PROP_SERVER,
	PROP_ROOM,
	PROP_PASSWORD,
	PROP_AUTO_CONNECT,
	PROP_STATUS,
	PROP_LAST_ERROR,
	PROP_ACCOUNT,
};


enum {
	CHANGED,
	LAST_SIGNAL
};


static guint     signals[LAST_SIGNAL] = {0};

static gpointer  parent_class = NULL;


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
					 PROP_STATUS,
					 g_param_spec_int ("status",
							   "Chatroom Status",
							   "Status of the room, open, closed, etc",
							   G_MININT,
							   G_MAXINT,
							   GOSSIP_CHATROOM_UNKNOWN,
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

        signals[CHANGED] = 
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
			      0, 
			      NULL, NULL,
			      libgossip_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);


	g_type_class_add_private (object_class, sizeof (GossipChatroomPriv));
}

static void
chatroom_init (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;
	static guint        id = 1;

	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);

	priv->id           = id++;

	priv->type         = 0;
	priv->name         = NULL;
	priv->nick         = NULL;
	priv->server       = NULL;
	priv->room         = NULL;
	priv->password     = NULL;
	priv->auto_connect = FALSE;
	priv->status       = GOSSIP_CHATROOM_CLOSED;
	priv->last_error   = NULL;
	priv->account      = NULL;
}

static void
chatroom_finalize (GObject *object)
{
	GossipChatroomPriv *priv;
	
	priv = GOSSIP_CHATROOM_GET_PRIV (object);
	
	g_free (priv->name);
	g_free (priv->nick);
	g_free (priv->server);
	g_free (priv->room);
	g_free (priv->password);

	g_free (priv->last_error);
	
	if (priv->account) {
		g_object_unref (priv->account);
	}

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
chatroom_get_property (GObject    *object,
		       guint       param_id,
		       GValue     *value,
		       GParamSpec *pspec)
{
	GossipChatroomPriv *priv;
	
	priv = GOSSIP_CHATROOM_GET_PRIV (object);

	switch (param_id) {
	case PROP_ID:
		g_value_set_int (value, priv->id);
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
	
	priv = GOSSIP_CHATROOM_GET_PRIV (object);
	
	switch (param_id) {
	case PROP_TYPE:
		priv->type = g_value_get_int (value);
		g_signal_emit (GOSSIP_CHATROOM (object), signals[CHANGED], 0);
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
	
	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	return priv->id;
}

GossipChatroomType
gossip_chatroom_get_type (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), 0);
	
	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	return priv->type;
}

const gchar *
gossip_chatroom_get_name (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	return priv->name;
}

const gchar *
gossip_chatroom_get_nick (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	return priv->nick;
}

const gchar *
gossip_chatroom_get_server (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	return priv->server;
}

const gchar *
gossip_chatroom_get_room (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	return priv->room;
}

const gchar *
gossip_chatroom_get_password (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	return priv->password;
}

gboolean
gossip_chatroom_get_auto_connect (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), TRUE);

	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	return priv->auto_connect;
}

GossipChatroomStatus
gossip_chatroom_get_status (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), GOSSIP_CHATROOM_UNKNOWN);

	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	
	return priv->status;
}

const gchar *
gossip_chatroom_get_last_error (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	return priv->last_error;
}

GossipAccount *
gossip_chatroom_get_account (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	
	if (!priv->account) {
		return NULL;
	}

	return g_object_ref (priv->account);
}

void 
gossip_chatroom_set_name (GossipChatroom *chatroom,
			  const gchar    *name)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (name != NULL);
	
	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	
	g_free (priv->name);
	priv->name = g_strdup (name);

	g_object_notify (G_OBJECT (chatroom), "name");
	g_signal_emit (chatroom, signals[CHANGED], 0);
}

void
gossip_chatroom_set_nick (GossipChatroom *chatroom,
			  const gchar    *nick)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (nick != NULL);
	
	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);

	g_free (priv->nick);
	priv->nick = g_strdup (nick);

	g_object_notify (G_OBJECT (chatroom), "nick");
	g_signal_emit (chatroom, signals[CHANGED], 0);
}

void
gossip_chatroom_set_server (GossipChatroom *chatroom,
			    const gchar    *server)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (server != NULL);

	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	
	g_free (priv->server);
	priv->server = g_strdup (server);

	g_object_notify (G_OBJECT (chatroom), "server");
	g_signal_emit (chatroom, signals[CHANGED], 0);
}

void
gossip_chatroom_set_room (GossipChatroom *chatroom,
			  const gchar    *room)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (room != NULL);
	
	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	
	g_free (priv->room);
	priv->room = g_strdup (room);

	g_object_notify (G_OBJECT (chatroom), "room");
	g_signal_emit (chatroom, signals[CHANGED], 0);
}

void
gossip_chatroom_set_password (GossipChatroom *chatroom,
			      const gchar    *password)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (password != NULL);
	
	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	
	g_free (priv->password);
	priv->password = g_strdup (password);

	g_object_notify (G_OBJECT (chatroom), "password");
	g_signal_emit (chatroom, signals[CHANGED], 0);
}

void
gossip_chatroom_set_auto_connect (GossipChatroom *chatroom,
				  gboolean        auto_connect)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));

	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	priv->auto_connect = auto_connect;

	g_object_notify (G_OBJECT (chatroom), "auto_connect");
	g_signal_emit (chatroom, signals[CHANGED], 0);
}

void
gossip_chatroom_set_status (GossipChatroom       *chatroom,
			    GossipChatroomStatus  status)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));

	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);

	priv->status = status;

	g_object_notify (G_OBJECT (chatroom), "status");
	g_signal_emit (chatroom, signals[CHANGED], 0);
}

void
gossip_chatroom_set_last_error (GossipChatroom *chatroom,
				const gchar    *last_error)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	
	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	
	g_free (priv->last_error);
	if (last_error) {
		priv->last_error = g_strdup (last_error);
	} else {
		priv->last_error = NULL;
	}

	g_object_notify (G_OBJECT (chatroom), "last_error");
	g_signal_emit (chatroom, signals[CHANGED], 0);
}

void
gossip_chatroom_set_account (GossipChatroom *chatroom,
			     GossipAccount  *account)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GOSSIP_CHATROOM_GET_PRIV (chatroom);
	if (priv->account) {
		g_object_unref (priv->account);
	}
		
	priv->account = g_object_ref (account);

	g_object_notify (G_OBJECT (chatroom), "account");
	g_signal_emit (chatroom, signals[CHANGED], 0);
}

guint 
gossip_chatroom_hash (gconstpointer key)
{
	GossipChatroomPriv *priv;
	
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (key), 0);

	priv = GOSSIP_CHATROOM_GET_PRIV (key);

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

	priv1 = GOSSIP_CHATROOM_GET_PRIV (a);
	priv2 = GOSSIP_CHATROOM_GET_PRIV (b);

	if (!priv1) {
		return FALSE;
	}

	if (!priv2) {
		return FALSE;
	}
	
	return (priv1->id == priv2->id);
}

const gchar *
gossip_chatroom_get_type_as_str (GossipChatroomType type)
{
	switch (type) {
	case GOSSIP_CHATROOM_TYPE_NORMAL: return _("Normal");
	}

	return "";
}

const gchar *
gossip_chatroom_get_status_as_str (GossipChatroomStatus status)
{
	switch (status) {
	case GOSSIP_CHATROOM_CONNECTING: return _("Connecting...");
	case GOSSIP_CHATROOM_OPEN: return _("Open");
	case GOSSIP_CHATROOM_CLOSED: return _("Closed");
	case GOSSIP_CHATROOM_UNKNOWN: return _("Unknown");
	case GOSSIP_CHATROOM_ERROR: return _("Error");
	}

	return "";
}
