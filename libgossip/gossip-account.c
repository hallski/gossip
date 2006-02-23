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

#include "gossip-account.h"

#define GOSSIP_ACCOUNT_GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_ACCOUNT, GossipAccountPriv))

#define d(x)


typedef struct _GossipAccountPriv GossipAccountPriv;


struct _GossipAccountPriv {
	gint               unique_id;

	GossipAccountType  type;

	gchar             *name;
	gchar             *id;
	gchar             *host;
	gchar             *password;
	gchar             *server;
	guint16            port;
	gboolean           enabled;
	gboolean           auto_connect;
	gboolean           use_ssl;
	gboolean           use_proxy;
};


static void account_class_init   (GossipAccountClass *class);
static void account_init         (GossipAccount      *account);
static void account_finalize     (GObject            *object);
static void account_get_property (GObject            *object,
				  guint               param_id,
				  GValue             *value,
				  GParamSpec         *pspec);
static void account_set_property (GObject            *object,
				  guint               param_id,
				  const GValue       *value,
				  GParamSpec         *pspec);
static void account_set_type     (GossipAccount      *account,
				  GossipAccountType   type);


enum {
	PROP_0,
	PROP_TYPE,
	PROP_NAME,
	PROP_ID,
	PROP_PASSWORD,
	PROP_SERVER,
	PROP_PORT,
	PROP_AUTO_CONNECT,
	PROP_USE_SSL,
	PROP_USE_PROXY
};


enum {
	CHANGED,
	LAST_SIGNAL
};


static guint     signals[LAST_SIGNAL] = {0};

static gpointer  parent_class = NULL;


GType
gossip_account_get_gtype (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GossipAccountClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) account_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GossipAccount),
			0,    /* n_preallocs */
			(GInstanceInitFunc) account_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "GossipAccount",
					       &info, 0);
	}

	return type;

}

static void
account_class_init (GossipAccountClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	object_class->finalize     = account_finalize;
	object_class->get_property = account_get_property;
	object_class->set_property = account_set_property;

	/* 
	 * properties 
	 */

	g_object_class_install_property (object_class,
					 PROP_TYPE,
					 g_param_spec_int ("type",
							   "Account Type",
							   "The account protocol type, e.g. MSN",
							   G_MININT,
							   G_MAXINT,
							   GOSSIP_ACCOUNT_TYPE_JABBER,
							   G_PARAM_READWRITE));


	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Account Name",
							      "What you call this account",
							      "Default",
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_ID,
					 g_param_spec_string ("id",
							      "Account ID",
							      "For example someone@jabber.org",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_PASSWORD,
					 g_param_spec_string ("password",
							      "Account Password",
							      "Authentication token",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_SERVER,
					 g_param_spec_string ("server",
							      "Account Server",
							      "Machine to connect to",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_PORT,
					 g_param_spec_int ("port",
							   "Account Port",
							   "Port used in the connection to Server",
							   0,
							   65535,
							   0,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_AUTO_CONNECT,
					 g_param_spec_boolean ("auto_connect",
							       "Account Auto Connect",
							       "Connect on startup",
							       TRUE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_USE_SSL,
					 g_param_spec_boolean ("use_ssl",
							       "Account Uses SSL",
							       "Identifies if the connection uses secure methods",
							       FALSE,
							       G_PARAM_READWRITE));
	
	g_object_class_install_property (object_class,
					 PROP_USE_PROXY,
					 g_param_spec_boolean ("use_proxy",
							       "Account Uses Proxy",
							       "Identifies if the connection uses the environment proxy",
							       FALSE,
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


	g_type_class_add_private (object_class, sizeof (GossipAccountPriv));
}

static void
account_init (GossipAccount *account)
{
	GossipAccountPriv *priv;
	static gint        id = 1;
 
	priv = GOSSIP_ACCOUNT_GET_PRIV (account);

	priv->unique_id    = id++;

	priv->type         = 0;
	priv->name         = NULL;
	priv->id           = NULL;
	priv->password     = NULL;
	priv->server       = NULL;
	priv->port         = 0;
	priv->enabled      = TRUE;
	priv->auto_connect = TRUE;
	priv->use_ssl      = FALSE;
	priv->use_proxy    = FALSE;
}

static void
account_finalize (GObject *object)
{
	GossipAccountPriv *priv;
	
	priv = GOSSIP_ACCOUNT_GET_PRIV (object);
	
	g_free (priv->name);
	g_free (priv->id);
	g_free (priv->password);
	g_free (priv->server);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
account_get_property (GObject    *object,
		      guint       param_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	GossipAccountPriv *priv;
	
	priv = GOSSIP_ACCOUNT_GET_PRIV (object);

	switch (param_id) {
	case PROP_TYPE:
		g_value_set_int (value, priv->type);
		break;
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	case PROP_PASSWORD:
		g_value_set_string (value, priv->password);
		break;
	case PROP_SERVER:
		g_value_set_string (value, priv->server);
		break;
	case PROP_PORT:
		g_value_set_int (value, priv->port);
		break;
	case PROP_AUTO_CONNECT:
		g_value_set_boolean (value, priv->auto_connect);
		break;
	case PROP_USE_SSL:
		g_value_set_boolean (value, priv->use_ssl);
		break;
	case PROP_USE_PROXY:
		g_value_set_boolean (value, priv->use_proxy);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}
	
static void
account_set_property (GObject      *object,
		      guint         param_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	GossipAccountPriv *priv;
	
	priv = GOSSIP_ACCOUNT_GET_PRIV (object);
	
	switch (param_id) {
	case PROP_TYPE:
		account_set_type (GOSSIP_ACCOUNT (object),
				  g_value_get_int (value));
		break;
	case PROP_NAME:
		gossip_account_set_name (GOSSIP_ACCOUNT (object),
					 g_value_get_string (value));
		break;
	case PROP_ID:
		gossip_account_set_id (GOSSIP_ACCOUNT (object),
				       g_value_get_string (value));
		break;
	case PROP_PASSWORD:
		gossip_account_set_password (GOSSIP_ACCOUNT (object),
					     g_value_get_string (value));
		break;
	case PROP_SERVER:
		gossip_account_set_server (GOSSIP_ACCOUNT (object),
					   g_value_get_string (value));
		break;
	case PROP_PORT:
		gossip_account_set_port (GOSSIP_ACCOUNT (object),
					 g_value_get_int (value));
		break;
	case PROP_AUTO_CONNECT:
		gossip_account_set_auto_connect (GOSSIP_ACCOUNT (object),
						 g_value_get_boolean (value));
		break;
	case PROP_USE_SSL:
		gossip_account_set_use_ssl (GOSSIP_ACCOUNT (object),
					    g_value_get_boolean (value));
		break;
	case PROP_USE_PROXY:
		gossip_account_set_use_proxy (GOSSIP_ACCOUNT (object),
					    g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
account_set_type (GossipAccount     *account,
		  GossipAccountType  type)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	priv->type = type;

	g_signal_emit (account, signals[CHANGED], 0);
}

GossipAccountType
gossip_account_get_type (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), 0);
	
	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->type;
}

const gchar *
gossip_account_get_name (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->name;
}

const gchar *
gossip_account_get_id (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->id;
}

const gchar *
gossip_account_get_password (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->password;
}

const gchar *
gossip_account_get_server (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->server;
}

guint16
gossip_account_get_port (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), 0);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->port;
}

gboolean
gossip_account_get_auto_connect (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), TRUE);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->auto_connect;
}

gboolean
gossip_account_get_use_ssl (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->use_ssl;
}

gboolean
gossip_account_get_use_proxy (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	return priv->use_proxy;
}


void 
gossip_account_set_name (GossipAccount *account,
			 const gchar   *name)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
	g_return_if_fail (name != NULL);
	
	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	
	g_free (priv->name);
	priv->name = g_strdup (name);

	g_object_notify (G_OBJECT (account), "name");
	g_signal_emit (account, signals[CHANGED], 0);
}

void
gossip_account_set_id (GossipAccount *account,
		       const gchar   *id)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
	g_return_if_fail (id != NULL);
	
	priv = GOSSIP_ACCOUNT_GET_PRIV (account);

	g_free (priv->id);
	priv->id = g_strdup (id);

	g_object_notify (G_OBJECT (account), "id");
	g_signal_emit (account, signals[CHANGED], 0);
}

void
gossip_account_set_password (GossipAccount *account,
			     const gchar   *password)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
	g_return_if_fail (password != NULL);
	
	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	
	g_free (priv->password);
	priv->password = g_strdup (password);

	g_object_notify (G_OBJECT (account), "password");
	g_signal_emit (account, signals[CHANGED], 0);
}

void
gossip_account_set_server (GossipAccount *account,
			   const gchar   *server)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
	g_return_if_fail (server != NULL);

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	
	g_free (priv->server);
	priv->server = g_strdup (server);

	g_object_notify (G_OBJECT (account), "server");
	g_signal_emit (account, signals[CHANGED], 0);
}

void
gossip_account_set_port (GossipAccount *account,
			 guint16        port)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	priv->port = port;

	g_object_notify (G_OBJECT (account), "port");
	g_signal_emit (account, signals[CHANGED], 0);
}

void
gossip_account_set_enabled (GossipAccount *account,
			    gboolean       enabled)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	priv->enabled = enabled;

	g_object_notify (G_OBJECT (account), "enabled");
	g_signal_emit (account, signals[CHANGED], 0);
}

void
gossip_account_set_auto_connect (GossipAccount *account,
				 gboolean       auto_connect)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	priv->auto_connect = auto_connect;

	g_object_notify (G_OBJECT (account), "auto_connect");
	g_signal_emit (account, signals[CHANGED], 0);
}

void
gossip_account_set_use_ssl (GossipAccount *account,
 			    gboolean       use_ssl)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	priv->use_ssl = use_ssl;

	g_object_notify (G_OBJECT (account), "use_ssl");
	g_signal_emit (account, signals[CHANGED], 0);
}

void
gossip_account_set_use_proxy (GossipAccount *account,
			      gboolean       use_proxy)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GOSSIP_ACCOUNT_GET_PRIV (account);
	priv->use_proxy = use_proxy;

	g_object_notify (G_OBJECT (account), "use_proxy");
	g_signal_emit (account, signals[CHANGED], 0);
}

guint 
gossip_account_hash (gconstpointer key)
{
	GossipAccountPriv *priv;
	
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (key), 0);

	priv = GOSSIP_ACCOUNT_GET_PRIV (key);

	return g_int_hash (&priv->unique_id);
}

gboolean
gossip_account_equal (gconstpointer a, 
		      gconstpointer b)
{
	GossipAccountPriv *priv1;
	GossipAccountPriv *priv2;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (a), FALSE);
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (b), FALSE);

	priv1 = GOSSIP_ACCOUNT_GET_PRIV (a);
	priv2 = GOSSIP_ACCOUNT_GET_PRIV (b);

	if (!priv1) {
		return FALSE;
	}

	if (!priv2) {
		return FALSE;
	}
	
	return (priv1->unique_id == priv2->unique_id);
}

const gchar *
gossip_account_get_type_as_str (GossipAccountType type)
{
	switch (type) {
	case GOSSIP_ACCOUNT_TYPE_JABBER: return "Jabber";
	case GOSSIP_ACCOUNT_TYPE_AIM:    return "AIM";
	case GOSSIP_ACCOUNT_TYPE_ICQ:    return "ICQ";
	case GOSSIP_ACCOUNT_TYPE_MSN:    return "MSN";
	case GOSSIP_ACCOUNT_TYPE_YAHOO:  return "Yahoo!";
	}

	return "";
}


