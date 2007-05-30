/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2007 Imendio AB
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
 * Authors: Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gobject/gvaluecollector.h>
#include <gtk/gtkiconfactory.h>
#include <gtk/gtkicontheme.h>
#include <gtk/gtkstock.h>

#include "gossip-account.h"
#include "gossip-stock.h"
#include "gossip-utils.h"

#include "libgossip-marshal.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_ACCOUNT, GossipAccountPriv))

typedef struct _GossipAccountPriv GossipAccountPriv;

struct _GossipAccountPriv {
	gint               unique_id;

	GossipAccountType  type;
	gchar             *name;
	gboolean           auto_connect;
	gboolean           use_proxy;

	GHashTable        *parameters;
};

typedef struct {
	GossipAccountType  account_type;
	GList             *params;
} GetParamsData;

static void           account_class_init            (GossipAccountClass     *class);
static void           account_init                  (GossipAccount          *account);
static void           account_finalize              (GObject                *object);
static void           account_get_property          (GObject                *object,
						     guint                   param_id,
						     GValue                 *value,
						     GParamSpec             *pspec);
static void           account_set_property          (GObject                *object,
						     guint                   param_id,
						     const GValue           *value,
						     GParamSpec             *pspec);
static void           account_param_new_valist      (GossipAccount          *account,
						     const gchar            *first_param_name,
						     va_list                 var_args);
static void           account_param_set_valist      (GossipAccount          *account,
						     const gchar            *first_param_name,
						     va_list                 var_args);
static void           account_param_get_valist      (GossipAccount          *account,
						     const gchar            *first_param_name,
						     va_list                 var_args);
static void           account_param_get_all_foreach (gchar                  *param_name,
						     GossipAccountParam     *param,
						     GetParamsData          *data);
static void           account_param_free            (GossipAccountParam     *param);
static void           account_set_type              (GossipAccount          *account,
						     GossipAccountType       type);

enum {
	PROP_0,
	PROP_TYPE,
	PROP_NAME,
	PROP_AUTO_CONNECT,
	PROP_USE_PROXY,
};

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

	g_object_class_install_property (object_class,
					 PROP_TYPE,
					 g_param_spec_int ("type",
							   "Account Type",
							   "The account protocol type, e.g. MSN",
							   GOSSIP_ACCOUNT_TYPE_JABBER_LEGACY,
							   GOSSIP_ACCOUNT_TYPE_UNKNOWN,
							   GOSSIP_ACCOUNT_TYPE_JABBER_LEGACY,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Account Name",
							      "What you call this account",
							      "Default",
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_AUTO_CONNECT,
					 g_param_spec_boolean ("auto_connect",
							       "Account Auto Connect",
							       "Connect on startup",
							       TRUE,
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
account_init (GossipAccount *account)
{
	GossipAccountPriv *priv;

	priv = GET_PRIV (account);

	priv->type         = 0;
	priv->name         = NULL;
	priv->auto_connect = TRUE;
	priv->use_proxy    = FALSE;

	priv->parameters = g_hash_table_new_full (g_str_hash,
						  g_str_equal,
						  g_free,
						  (GDestroyNotify) account_param_free);
}

static void
account_finalize (GObject *object)
{
	GossipAccountPriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->name);

	g_hash_table_unref (priv->parameters);

	(G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
account_get_property (GObject    *object,
		      guint       param_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	GossipAccountPriv *priv;
	GossipAccount     *account;

	priv = GET_PRIV (object);
	account = GOSSIP_ACCOUNT (object);

	switch (param_id) {
	case PROP_TYPE:
		g_value_set_int (value, priv->type);
		break;
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_AUTO_CONNECT:
		g_value_set_boolean (value, priv->auto_connect);
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
	case PROP_TYPE:
		account_set_type (GOSSIP_ACCOUNT (object),
				  g_value_get_int (value));
		break;
	case PROP_NAME:
		gossip_account_set_name (GOSSIP_ACCOUNT (object),
					 g_value_get_string (value));
		break;
	case PROP_AUTO_CONNECT:
		gossip_account_set_auto_connect (GOSSIP_ACCOUNT (object),
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
account_param_new_valist (GossipAccount *account,
			  const gchar   *first_param_name,
			  va_list        var_args)
{
	GossipAccountPriv *priv;
	GossipAccountType  account_type;
	const gchar       *param_name;

	priv = GET_PRIV (account);

	account_type = gossip_account_get_type (account);

	for (param_name = first_param_name; 
	     param_name; 
	     param_name = va_arg (var_args, gchar*)) {
		GossipAccountParam *param;
		GType               g_type;
		gchar              *error = NULL;

		param = g_hash_table_lookup (priv->parameters, param_name);
		if (param) {
			g_warning ("GossipAccount already has a parameter named `%s'", param_name);
			break;
		}

		param = g_slice_new0 (GossipAccountParam);

		g_type = va_arg (var_args, GType);
		g_value_init (&param->g_value, g_type);

		G_VALUE_COLLECT (&param->g_value, var_args, 0, &error);
		if (error) {
			g_warning ("%s: %s", G_STRFUNC, error);
			g_free (error);
			break;
		}

		param->flags = va_arg (var_args, GossipAccountParamFlags);

		g_hash_table_insert (priv->parameters,
				     g_strdup (param_name),
				     param);
	}
}

static void
account_param_set_valist (GossipAccount *account,
			  const gchar   *first_param_name,
			  va_list        var_args)
{
	GossipAccountPriv *priv;
	GossipAccountType  account_type;
	const gchar       *param_name;

	priv = GET_PRIV (account);

	account_type = gossip_account_get_type (account);

	for (param_name = first_param_name; 
	     param_name; 
	     param_name = va_arg (var_args, gchar*)) {
		GossipAccountParam *param;
		gchar              *error = NULL;
		GValue              g_value = {0, };

		param = g_hash_table_lookup (priv->parameters, param_name);
		if (!param) {
			g_warning ("GossipAccount has no parameter named `%s'", param_name);
			break;
		}

		g_value_init (&g_value, G_VALUE_TYPE (&param->g_value));
		G_VALUE_COLLECT (&g_value, var_args, 0, &error);
		if (error) {
			g_value_unset (&g_value);
			g_warning ("%s: %s", G_STRFUNC, error);
			g_free (error);
			break;
		}
		if (!gossip_g_value_equal (&g_value, &param->g_value)) {
			param->modified = TRUE;
			g_value_copy (&g_value, &param->g_value);
		}
		g_value_unset (&g_value);
	}
}

static void
account_param_get_valist (GossipAccount *account,
			  const gchar   *first_param_name,
			  va_list        var_args)
{
	GossipAccountPriv *priv;
	GossipAccountType  account_type;
	const gchar       *name;

	priv = GET_PRIV (account);

	account_type = gossip_account_get_type (account);

	for (name = first_param_name; name; name = va_arg (var_args, gchar*)) {
		GossipAccountParam *param;
		gchar              *error = NULL;

		param = g_hash_table_lookup (priv->parameters, name);
		if (!param) {
			g_warning ("GossipAccount has no parameter named `%s'", name);
			break;
		}

		G_VALUE_LCOPY (&param->g_value, var_args, 0, &error);
		if (error) {
			g_warning ("%s: %s", G_STRFUNC, error);
			g_free (error);
			break;
		}
	}
}

void
gossip_account_new_param (GossipAccount *account,
			  const gchar   *first_param_name,
			  ...)
{
	va_list var_args;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	va_start (var_args, first_param_name);
	account_param_new_valist (account, first_param_name, var_args);
	va_end (var_args);
}

void
gossip_account_new_param_g_value (GossipAccount           *account,
				  const gchar             *param_name,
				  const GValue            *g_value,
				  GossipAccountParamFlags  flags)
{
	GossipAccountParam *param;
	GossipAccountPriv  *priv;
	GossipAccountType   account_type;

	priv = GET_PRIV (account);

	account_type = gossip_account_get_type (account);

	param = g_hash_table_lookup (priv->parameters, param_name);
	if (param) {
		g_warning ("GossipAccount already has a parameter named `%s'", 
			   param_name);
		return;
	}

	param = g_new0 (GossipAccountParam, 1);
	g_value_init (&param->g_value, G_VALUE_TYPE (g_value));
	g_value_copy (g_value, &param->g_value);
	param->flags = flags;

	g_hash_table_insert (priv->parameters,
			     g_strdup (param_name),
			     param);
}

void
gossip_account_set_param (GossipAccount *account,
			  const gchar   *first_param_name,
			  ...)
{
	va_list var_args;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	va_start (var_args, first_param_name);
	account_param_set_valist (account, first_param_name, var_args);
	va_end (var_args);
}

void
gossip_account_set_param_g_value (GossipAccount *account,
				  const gchar   *param_name,
				  const GValue  *g_value)
{
	GossipAccountPriv  *priv;
	GossipAccountParam *param;
	GossipAccountType   account_type;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GET_PRIV (account);

	account_type = gossip_account_get_type (account);

	param = g_hash_table_lookup (priv->parameters, param_name);
	if (!param) {
		g_warning ("GossipAccount has no parameter named `%s'", param_name);
		return;
	}

	if (!gossip_g_value_equal (g_value, &param->g_value)) {
		param->modified = TRUE;
		g_value_copy (g_value, &param->g_value);
	}
}

void
gossip_account_get_param (GossipAccount *account,
			  const gchar   *first_param_name,
			  ...)
{
	va_list var_args;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	va_start (var_args, first_param_name);
	account_param_get_valist (account, first_param_name, var_args);
	va_end (var_args);
}

const GValue *
gossip_account_get_param_g_value (GossipAccount *account,
				  const gchar   *param_name)
{
	GossipAccountPriv  *priv;
	GossipAccountParam *param;
	GossipAccountType   account_type;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GET_PRIV (account);

	account_type = gossip_account_get_type (account);
	
	param = g_hash_table_lookup (priv->parameters, param_name);
	if (!param) {
		g_warning ("GossipAccount has no parameter named `%s'", param_name);
		return NULL;
	}

	return &param->g_value;
}

gboolean
gossip_account_has_param (GossipAccount *account,
			  const gchar   *param_name)
{
	GossipAccountPriv *priv;
	GossipAccountType  account_type;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);

	priv = GET_PRIV (account);

	account_type = gossip_account_get_type (account);

	return g_hash_table_lookup (priv->parameters, param_name) != NULL;
}

static void
account_param_get_all_foreach (gchar              *param_name,
			       GossipAccountParam *param,
			       GetParamsData      *data)
{
	data->params = g_list_prepend (data->params, g_strdup (param_name));
}

GList *
gossip_account_get_param_all (GossipAccount *account)
{
	GossipAccountPriv *priv;
	GetParamsData      data;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GET_PRIV (account);

	data.account_type = gossip_account_get_type (account);
	data.params = NULL;

	g_hash_table_foreach (priv->parameters,
			      (GHFunc) account_param_get_all_foreach,
			      &data);

	data.params = g_list_sort (data.params, (GCompareFunc) strcmp);

	return data.params;
}

GossipAccountParam *
gossip_account_get_param_param (GossipAccount *account,
				const gchar   *param_name)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GET_PRIV (account);

	return g_hash_table_lookup (priv->parameters, param_name);
}


static void
account_param_free (GossipAccountParam *param)
{
	g_value_unset (&param->g_value);
	g_slice_free (GossipAccountParam, param);
}

GossipAccountType
gossip_account_get_type (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), 0);

	priv = GET_PRIV (account);
	return priv->type;
}

const gchar *
gossip_account_get_name (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GET_PRIV (account);
	return priv->name;
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
gossip_account_get_use_proxy (GossipAccount *account)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);

	priv = GET_PRIV (account);
	return priv->use_proxy;
}

void
account_set_type (GossipAccount     *account,
		  GossipAccountType  type)
{
	GossipAccountPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GET_PRIV (account);
	priv->type = type;
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
	case GOSSIP_ACCOUNT_TYPE_JABBER_LEGACY:  return "Jabber";
       	case GOSSIP_ACCOUNT_TYPE_UNKNOWN:
	default:
		break;
	}

	return "Unknown";
}

GossipAccountType
gossip_account_string_to_type (const gchar *str)
{
	if (gossip_strncasecmp (str, "Jabber", -1) == 0) {
		return GOSSIP_ACCOUNT_TYPE_JABBER_LEGACY;
	} else {
		g_warning ("You have an unsupported account of type '%s' specified in your accounts.xml file. To get rid of this warning edit ~/.gnome2/Gossip/accounts.xml and remove it.", str);
	}

	return GOSSIP_ACCOUNT_TYPE_UNKNOWN;
}

GdkPixbuf *
gossip_account_type_create_pixbuf (GossipAccountType    type,
				   GtkIconSize          icon_size)
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
	case GOSSIP_ACCOUNT_TYPE_JABBER_LEGACY:
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
	case GOSSIP_ACCOUNT_TYPE_SALUT:
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
gossip_account_create_pixbuf (GossipAccount       *account,
			      GtkIconSize          icon_size)
{
	GossipAccountType  type;
	GdkPixbuf         *pixbuf = NULL;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	type = gossip_account_get_type (account);
	pixbuf = gossip_account_type_create_pixbuf (type, icon_size);

	return pixbuf;
}

GdkPixbuf *
gossip_account_status_create_pixbuf (GossipAccount       *account,
				     GtkIconSize          icon_size,
				     gboolean             online)
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

