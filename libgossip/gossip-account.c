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
 * Authors: Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gobject/gvaluecollector.h>

#include "libgossip-marshal.h"
#include "gossip-utils.h"

#include "gossip-account.h"

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
	GossipAccountParamFunc  callback;
	gpointer                user_data;
	GossipAccount          *account;
} GossipAccountParamData;

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
						     GossipAccountParamData *data);
static void           account_param_free            (GossipAccountParam     *param);
static void           account_set_type              (GossipAccount          *account,
						     GossipAccountType       type);

enum {
	PROP_0,
	PROP_TYPE,
	PROP_NAME,
	PROP_AUTO_CONNECT,
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

	signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

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
	static gint        id = 1;

	priv = GET_PRIV (account);

	priv->unique_id    = id++;

	priv->type         = 0;
	priv->name         = NULL;
	priv->auto_connect = TRUE;
	priv->use_proxy    = FALSE;

	priv->parameters = g_hash_table_new_full (g_str_hash,
						  g_str_equal,
						  g_free,
						  (GDestroyNotify)account_param_free);
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
	const gchar       *name;

	priv = GET_PRIV (account);

	for (name = first_param_name; name; name = va_arg (var_args, gchar*)) {
		GossipAccountParam *param;
		GType               g_type;
		gchar              *error = NULL;

		param = g_hash_table_lookup (priv->parameters, name);
		if (param) {
			g_warning ("GossipAccount already has a parameter named `%s'", name);
			break;
		}
		param = g_new0 (GossipAccountParam, 1);

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
				     g_strdup (name),
				     param);
	}
}

static void
account_param_set_valist (GossipAccount *account,
			  const gchar   *first_param_name,
			  va_list        var_args)
{
	GossipAccountPriv *priv;
	const gchar       *name;

	priv = GET_PRIV (account);

	for (name = first_param_name; name; name = va_arg (var_args, gchar*)) {
		GossipAccountParam *param;
		gchar              *error = NULL;

		param = g_hash_table_lookup (priv->parameters, name);
		if (!param) {
			g_warning ("GossipAccount has no parameter named `%s'", name);
			break;
		}

		G_VALUE_COLLECT (&param->g_value, var_args, 0, &error);
		if (error) {
			g_warning ("%s: %s", G_STRFUNC, error);
			g_free (error);
			break;
		}
	}
}

static void
account_param_get_valist (GossipAccount *account,
			  const gchar   *first_param_name,
			  va_list        var_args)
{
	GossipAccountPriv *priv;
	const gchar       *name;

	priv = GET_PRIV (account);

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
gossip_account_param_new (GossipAccount *account,
			  const gchar   *first_param_name,
			  ...)
{
	va_list var_args;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	va_start (var_args, first_param_name);
	account_param_new_valist (account, first_param_name, var_args);
	va_end (var_args);

	g_signal_emit (account, signals[CHANGED], 0);
}

void
gossip_account_param_set (GossipAccount *account,
			  const gchar   *first_param_name,
			  ...)
{
	va_list var_args;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	va_start (var_args, first_param_name);
	account_param_set_valist (account, first_param_name, var_args);
	va_end (var_args);

	g_signal_emit (account, signals[CHANGED], 0);
}

void
gossip_account_param_set_g_value (GossipAccount           *account,
				  const gchar             *param_name,
				  const GValue            *g_value)
{
	GossipAccountPriv  *priv;
	GossipAccountParam *param;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GET_PRIV (account);

	param = g_hash_table_lookup (priv->parameters, param_name);
	if (!param) {
		g_warning ("GossipAccount has no parameter named `%s'", param_name);
		return;
	}

	g_value_copy (g_value, &param->g_value);

	g_signal_emit (account, signals[CHANGED], 0);
}

void
gossip_account_param_get (GossipAccount *account,
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
gossip_account_param_get_g_value (GossipAccount *account,
				  const gchar   *param_name)
{
	GossipAccountPriv  *priv;
	GossipAccountParam *param;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GET_PRIV (account);

	param = g_hash_table_lookup (priv->parameters, param_name);
	if (!param) {
		g_warning ("GossipAccount has no parameter named `%s'", param_name);
		return NULL;
	}

	return &param->g_value;
}

void
gossip_account_param_foreach (GossipAccount           *account,
			      GossipAccountParamFunc   callback,
			      gpointer                 user_data)
{
	GossipAccountPriv      *priv;
	GossipAccountParamData *data;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GET_PRIV (account);

	data = g_new0 (GossipAccountParamData, 1);
	data->callback = callback;
	data->user_data = user_data;
	data->account = account;

	g_hash_table_foreach (priv->parameters,
			      (GHFunc) account_param_get_all_foreach,
			      data);
}

static void
account_param_get_all_foreach (gchar                  *param_name,
			       GossipAccountParam     *param,
			       GossipAccountParamData *data)
{
	data->callback (data->account,
			param_name,
			param,
			data->user_data);
}

static void
account_param_free (GossipAccountParam *param)
{
	g_value_unset (&param->g_value);
	g_free (param);
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

	g_signal_emit (account, signals[CHANGED], 0);
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
	g_signal_emit (account, signals[CHANGED], 0);
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
	g_signal_emit (account, signals[CHANGED], 0);
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
	g_signal_emit (account, signals[CHANGED], 0);
}

guint
gossip_account_hash (gconstpointer key)
{
	GossipAccountPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (key), 0);

	priv = GET_PRIV (key);

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

	priv1 = GET_PRIV (a);
	priv2 = GET_PRIV (b);

	return (priv1->unique_id == priv2->unique_id);
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
		return GOSSIP_ACCOUNT_TYPE_JABBER;
	}
	else if (gossip_strncasecmp (str, "AIM", -1) == 0) {
		return GOSSIP_ACCOUNT_TYPE_AIM;
	}
	else if (gossip_strncasecmp (str, "ICQ", -1) == 0) {
		return GOSSIP_ACCOUNT_TYPE_ICQ;
	}
	else if (gossip_strncasecmp (str, "MSN", -1) == 0) {
		return GOSSIP_ACCOUNT_TYPE_MSN;
	}
	else if (gossip_strncasecmp (str, "Yahoo!", -1) == 0) {
		return GOSSIP_ACCOUNT_TYPE_YAHOO;
	}

	return GOSSIP_ACCOUNT_TYPE_UNKNOWN;
}
