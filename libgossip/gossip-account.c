/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2006 Imendio AB
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
 * Author: Martyn Russell <martyn@imendio.com>
 */

#include <config.h>

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <gtk/gtkicontheme.h>
#include <gtk/gtkstock.h>

/* #include <gdk-pixbuf/gdk-pixbuf.h> */
/* #include <gtk/gtkenums.h> */

#include "gossip-account.h"
#include "gossip-stock.h"
#include "gossip-utils.h"

#include "libgossip-marshal.h"


#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_ACCOUNT, GossipAccountPriv))

typedef struct _GossipAccountPriv GossipAccountPriv;

struct _GossipAccountPriv {
	gchar             *name;
	gchar             *id;
	gchar             *password;
	gchar             *resource;
	gchar             *server;
	guint16            port;
	gboolean           auto_connect;
	gboolean           use_ssl;
	gboolean           use_proxy;
};

static void gossip_account_class_init (GossipAccountClass *klass);
static void gossip_account_init       (GossipAccount      *account);
static void account_finalize          (GObject            *object);
static void account_get_property      (GObject            *object,
				       guint               param_id,
				       GValue             *value,
				       GParamSpec         *pspec);
static void account_set_property      (GObject            *object,
				       guint               param_id,
				       const GValue       *value,
				       GParamSpec         *pspec);

enum {
	PROP_0,
	PROP_NAME,
	PROP_ID,
	PROP_PASSWORD,
	PROP_RESOURCE,
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

G_DEFINE_TYPE (GossipAccount, gossip_account, G_TYPE_OBJECT);

static void
gossip_account_class_init (GossipAccountClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = account_finalize;
	object_class->get_property = account_get_property;
	object_class->set_property = account_set_property;

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
					 PROP_RESOURCE,
					 g_param_spec_string ("resource",
							      "Connection Resource",
							      "The name for the connection",
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
	
	g_type_class_add_private (object_class, sizeof (GossipAccountPriv));
}

static void
gossip_account_init (GossipAccount *account)
{
	GossipAccountPriv *priv;

	priv = GET_PRIV (account);

	priv->name         = NULL;

	priv->id           = NULL;
	priv->password     = NULL;
	priv->resource     = NULL;
	priv->server       = NULL;
	priv->port         = 0;

	priv->auto_connect = TRUE;

	priv->use_ssl      = FALSE;
	priv->use_proxy    = FALSE;
}

static void
account_finalize (GObject *object)
{
	GossipAccountPriv *priv;
	
	priv = GET_PRIV (object);
	
	g_free (priv->name);

	g_free (priv->id);
	g_free (priv->password);
	g_free (priv->resource);
	g_free (priv->server);
	
	(G_OBJECT_CLASS (gossip_account_parent_class)->finalize) (object);
}

static void
account_get_property (GObject    *object,
		      guint       param_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	GossipAccountPriv *priv;
	
	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	case PROP_PASSWORD:
		g_value_set_string (value, priv->password);
		break;
	case PROP_RESOURCE:
		g_value_set_string (value, priv->resource);
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
	
	priv = GET_PRIV (object);
	
	switch (param_id) {
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
	case PROP_RESOURCE:
		gossip_account_set_resource (GOSSIP_ACCOUNT (object),
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

const gchar *
gossip_account_get_name (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GET_PRIV (account);
	return priv->name;
}

const gchar *
gossip_account_get_id (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GET_PRIV (account);
	return priv->id;
}

const gchar *
gossip_account_get_password (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GET_PRIV (account);
	return priv->password;
}

const gchar *
gossip_account_get_resource (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GET_PRIV (account);
	return priv->resource;
}

const gchar *
gossip_account_get_server (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GET_PRIV (account);
	return priv->server;
}

guint16
gossip_account_get_port (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), 0);

	priv = GET_PRIV (account);
	return priv->port;
}

gboolean
gossip_account_get_auto_connect (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), TRUE);

	priv = GET_PRIV (account);
	return priv->auto_connect;
}

gboolean
gossip_account_get_use_ssl (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);

	priv = GET_PRIV (account);
	return priv->use_ssl;
}

gboolean
gossip_account_get_use_proxy (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);

	priv = GET_PRIV (account);
	return priv->use_proxy;
}

void 
gossip_account_set_name (GossipAccount *account,
			 const gchar   *name)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
	g_return_if_fail (name != NULL);
	
	priv = GET_PRIV (account);
	
	g_free (priv->name);
	priv->name = g_strdup (name);

	g_object_notify (G_OBJECT (account), "name");
}

void
gossip_account_set_id (GossipAccount *account,
		       const gchar   *id)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
	g_return_if_fail (id != NULL);
	
	priv = GET_PRIV (account);

	g_free (priv->id);
	priv->id = g_strdup (id);

	g_object_notify (G_OBJECT (account), "id");
}

void
gossip_account_set_password (GossipAccount *account,
			     const gchar   *password)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
	g_return_if_fail (password != NULL);
	
	priv = GET_PRIV (account);
	
	g_free (priv->password);
	priv->password = g_strdup (password);

	g_object_notify (G_OBJECT (account), "password");
}

void
gossip_account_set_resource (GossipAccount *account,
			     const gchar   *resource)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
	g_return_if_fail (resource != NULL);

	priv = GET_PRIV (account);
	
	g_free (priv->resource);
	priv->resource = g_strdup (resource);

	g_object_notify (G_OBJECT (account), "resource");
}

void
gossip_account_set_server (GossipAccount *account,
			   const gchar   *server)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
	g_return_if_fail (server != NULL);

	priv = GET_PRIV (account);
	
	g_free (priv->server);
	priv->server = g_strdup (server);

	g_object_notify (G_OBJECT (account), "server");
}

void
gossip_account_set_port (GossipAccount *account,
			 guint16        port)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GET_PRIV (account);
	priv->port = port;

	g_object_notify (G_OBJECT (account), "port");
}

void
gossip_account_set_auto_connect (GossipAccount *account,
				 gboolean       auto_connect)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GET_PRIV (account);
	priv->auto_connect = auto_connect;

	g_object_notify (G_OBJECT (account), "auto_connect");
}

void
gossip_account_set_use_ssl (GossipAccount *account,
 			    gboolean       use_ssl)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GET_PRIV (account);
	priv->use_ssl = use_ssl;

	g_object_notify (G_OBJECT (account), "use_ssl");
}

void
gossip_account_set_use_proxy (GossipAccount *account,
			      gboolean       use_proxy)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GET_PRIV (account);
	priv->use_proxy = use_proxy;

	g_object_notify (G_OBJECT (account), "use_proxy");
}

guint
gossip_account_hash (gconstpointer key)
{
	return g_direct_hash (key);
}

gboolean
gossip_account_equal (gconstpointer a,
		      gconstpointer b)
{
	return g_direct_equal (a, b);
}

const gchar *
gossip_account_type_to_string (GossipAccountType type)
{
	switch (type) {
	case GOSSIP_ACCOUNT_TYPE_JABBER:  return "Jabber";
	case GOSSIP_ACCOUNT_TYPE_AIM:     return "AIM";
	case GOSSIP_ACCOUNT_TYPE_ICQ:     return "ICQ";
	case GOSSIP_ACCOUNT_TYPE_MSN:     return "MSN";
	case GOSSIP_ACCOUNT_TYPE_YAHOO:   return "Yahoo!";
	case GOSSIP_ACCOUNT_TYPE_IRC:     return "IRC";
		
	case GOSSIP_ACCOUNT_TYPE_UNKNOWN:
	case GOSSIP_ACCOUNT_TYPE_COUNT: 
		return "Unknown";
	}

	return "";
}

GossipAccountType
gossip_account_string_to_type (const gchar *str)
{
	if (gossip_strncasecmp (str, "Jabber", -1) == 0) {
		return GOSSIP_ACCOUNT_TYPE_JABBER;
	} else if (gossip_strncasecmp (str, "AIM", -1) == 0) {
	} else if (gossip_strncasecmp (str, "ICQ", -1) == 0) {
	} else if (gossip_strncasecmp (str, "MSN", -1) == 0) {
	} else if (gossip_strncasecmp (str, "Yahoo!", -1) == 0) {
	} else if (gossip_strncasecmp (str, "IRC", -1) == 0) {
	} else {
		g_warning ("You have an unsupported account of type '%s' "
			   "specified in your accounts.xml file. To get "
			   "rid of this warning edit "
			   "~/.gnome2/Gossip/accounts.xml and remove it.", 
			   str);
	}

	return GOSSIP_ACCOUNT_TYPE_UNKNOWN;
}

GdkPixbuf *
gossip_account_type_create_pixbuf (GossipAccountType type,
				   GtkIconSize       icon_size)
{
	GtkIconTheme  *theme;
	GdkPixbuf     *pixbuf = NULL;
	GError        *error = NULL;
	gint           w, h;
	gint           size = 48;
	const gchar   *icon_id = NULL;

	theme = gtk_icon_theme_get_default ();

	if (!gtk_icon_size_lookup (icon_size, &w, &h)) {
		size = 48;
	} else {
		size = (w + h) / 2;
	}

	switch (type) {
	case GOSSIP_ACCOUNT_TYPE_JABBER:
		icon_id = "im-jabber";
		break;
	case GOSSIP_ACCOUNT_TYPE_AIM:
		icon_id = "im-aim";
		break;
	case GOSSIP_ACCOUNT_TYPE_ICQ:
		icon_id = "im-icq";
		break;
	case GOSSIP_ACCOUNT_TYPE_MSN:
		icon_id = "im-msn";
		break;
	case GOSSIP_ACCOUNT_TYPE_YAHOO:
		icon_id = "im-yahoo";
		break;

	/* FIXME: we should have an artwork for these protocols */
	case GOSSIP_ACCOUNT_TYPE_IRC:
	case GOSSIP_ACCOUNT_TYPE_UNKNOWN:
	default:
		icon_id = "im";
		break;
	}

	pixbuf = gtk_icon_theme_load_icon (theme,
					   icon_id,     /* Icon name */
					   size,        /* Size */
					   0,           /* Flags */
					   &error);

	return pixbuf;
}

GdkPixbuf *
gossip_account_create_pixbuf (GossipAccount *account,
			      GtkIconSize    icon_size)
{
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	return gossip_account_type_create_pixbuf (GOSSIP_ACCOUNT_TYPE_JABBER, icon_size);
}

GdkPixbuf *
gossip_account_status_create_pixbuf (GossipAccount *account,
				     GtkIconSize    icon_size,
				     gboolean       online)
{
	GdkPixbuf *pixbuf = NULL;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	pixbuf = gossip_account_create_pixbuf (account, icon_size);
	g_return_val_if_fail (pixbuf != NULL, NULL);

	if (!online) {
		GdkPixbuf *modded_pixbuf;

		modded_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
						TRUE,
						8,
						gdk_pixbuf_get_width (pixbuf),
						gdk_pixbuf_get_height (pixbuf));

		gdk_pixbuf_saturate_and_pixelate (pixbuf,
						  modded_pixbuf,
						  1.0,
						  TRUE);
		g_object_unref (pixbuf);
		pixbuf = modded_pixbuf;
	}

	return pixbuf;

}


