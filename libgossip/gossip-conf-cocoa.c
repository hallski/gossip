/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
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
#include <Cocoa/Cocoa.h>

#include "gossip-conf.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                       GOSSIP_TYPE_CONF, GossipConfPriv))

#define POOL_ALLOC   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init]
#define POOL_RELEASE [pool release]

typedef struct {
	NSUserDefaults *defaults;
} GossipConfPriv;

typedef struct {
	GossipConf           *conf;
	GossipConfNotifyFunc  func;
	gpointer               user_data;
} GossipConfNotifyData;

static void conf_finalize (GObject *object);

G_DEFINE_TYPE (GossipConf, gossip_conf, G_TYPE_OBJECT);


static GossipConf *global_conf = NULL;


static void
gossip_conf_class_init (GossipConfClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize = conf_finalize;

	g_type_class_add_private (object_class, sizeof (GossipConfPriv));
}

static void
gossip_conf_init (GossipConf *conf)
{
	GossipConfPriv *priv;

	priv = GET_PRIV (conf);

	priv->defaults = [NSUserDefaults standardUserDefaults];
}

static void
conf_finalize (GObject *object)
{
	GossipConfPriv *priv;

	priv = GET_PRIV (object);

	/*	gconf_client_remove_dir (priv->gconf_client,
				 GOSSIP_CONF_ROOT,
				 NULL);
	gconf_client_remove_dir (priv->gconf_client,
				 DESKTOP_INTERFACE_ROOT,
				 NULL);
	gconf_client_remove_dir (priv->gconf_client,
				 HTTP_PROXY_ROOT,
				 NULL);
	*/
	
	G_OBJECT_CLASS (gossip_conf_parent_class)->finalize (object);
}

GossipConf *
gossip_conf_get (void)
{
	if (!global_conf) {
		global_conf = g_object_new (GOSSIP_TYPE_CONF, NULL);
	}

	return global_conf;
}

void
gossip_conf_shutdown (void)
{
	if (global_conf) {
		g_object_unref (global_conf);
		global_conf = NULL;
	}
}

gboolean
gossip_conf_set_int (GossipConf  *conf,
		     const gchar *key,
		     gint         value)
{
	GossipConfPriv *priv;
	NSString       *string;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

	priv = GET_PRIV (conf);

	POOL_ALLOC;

	string = [NSString stringWithUTF8String: key];
	[priv->defaults setInteger: value forKey: string];

	POOL_RELEASE;
	 
	return TRUE;
}

gboolean
gossip_conf_get_int (GossipConf  *conf,
		     const gchar *key,
		     gint        *value)
{
	GossipConfPriv *priv;
	NSString       *string;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

	priv = GET_PRIV (conf);

	POOL_ALLOC;
	
	string = [NSString stringWithUTF8String: key];
	*value = [priv->defaults integerForKey: string];

	POOL_RELEASE;
	
	return TRUE;
}

gboolean
gossip_conf_set_bool (GossipConf  *conf,
		      const gchar *key,
		      gboolean     value)
{
	GossipConfPriv *priv;
	NSString       *string;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

	priv = GET_PRIV (conf);

	POOL_ALLOC;

	string = [NSString stringWithUTF8String: key];
	[priv->defaults setBool: value forKey: string];

	POOL_RELEASE;
	 
	return TRUE;
}

gboolean
gossip_conf_get_bool (GossipConf  *conf,
		      const gchar *key,
		      gboolean    *value)
{
	GossipConfPriv *priv;
	NSString       *string;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

	priv = GET_PRIV (conf);

	POOL_ALLOC;
	
	string = [NSString stringWithUTF8String: key];
	*value = [priv->defaults boolForKey: string];

	POOL_RELEASE;
	
	return TRUE;
}

gboolean
gossip_conf_set_string (GossipConf  *conf,
			const gchar *key,
			const gchar *value)
{
	GossipConfPriv *priv;
	NSString       *string, *nsvalue;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

	priv = GET_PRIV (conf);

	POOL_ALLOC;
	
	string = [NSString stringWithUTF8String: key];
	nsvalue = [NSString stringWithUTF8String: value];
	[priv->defaults setObject: nsvalue forKey: string];

	POOL_RELEASE;
	
	return TRUE;
}

gboolean
gossip_conf_get_string (GossipConf   *conf,
			const gchar  *key,
			gchar       **value)
{
	GossipConfPriv *priv;
	NSString       *string, *nsvalue;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

	priv = GET_PRIV (conf);

	POOL_ALLOC;
	
	string = [NSString stringWithUTF8String: key];
	nsvalue = [priv->defaults stringForKey: string];

	*value = g_strdup ([nsvalue UTF8String]);

	POOL_RELEASE;

	return *value != NULL;
}

gboolean
gossip_conf_set_string_list (GossipConf  *conf,
			     const gchar *key,
			     GSList      *value)
{
	GossipConfPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

	priv = GET_PRIV (conf);

	return TRUE; /*gconf_client_set_string_list (priv->gconf_client,
					     key,
					     value,
					     NULL);
		     */
}

gboolean
gossip_conf_get_string_list (GossipConf   *conf,
			     const gchar  *key,
			     GSList      **value)
{
	GossipConfPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

	priv = GET_PRIV (conf);

	*value = NULL; /*gconf_client_get_string_list (priv->gconf_client,
					       key,
					       &error);
		       */
	return TRUE;
}

/*
static void
conf_notify_data_free (GossipConfNotifyData *data)
{
	g_object_unref (data->conf);
	g_slice_free (GossipConfNotifyData, data);
}

static void
conf_notify_func (GConfClient *client,
		  guint        id,
		  GConfEntry  *entry,
		  gpointer     user_data)
{
	GossipConfNotifyData *data;

	data = user_data;

	data->func (data->conf,
		    gconf_entry_get_key (entry),
		    data->user_data);
}
*/

guint
gossip_conf_notify_add (GossipConf           *conf,
			const gchar          *key,
			GossipConfNotifyFunc func,
			gpointer              user_data)
{
	GossipConfPriv       *priv;
	guint                  id;
	GossipConfNotifyData *data;
	
	g_return_val_if_fail (GOSSIP_IS_CONF (conf), 0);

	priv = GET_PRIV (conf);

	data = g_slice_new (GossipConfNotifyData);
	data->func = func;
	data->user_data = user_data;
	data->conf = g_object_ref (conf);
		
	id = 0; /*gconf_client_notify_add (priv->gconf_client,
				      key,
				      conf_notify_func,
				      data,
				      (GFreeFunc) conf_notify_data_free,
				      NULL);
	*/
	return id;
}

gboolean
gossip_conf_notify_remove (GossipConf *conf,
			   guint       id)
{
	GossipConfPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);
	
	priv = GET_PRIV (conf);

	/*gconf_client_notify_remove (priv->gconf_client, id);*/
	
	return TRUE;
}

/* Special cased settings that also need to be in the backends. */

gboolean
gossip_conf_get_http_proxy (GossipConf  *conf,
			    gboolean    *use_http_proxy,
			    gchar      **host,
			    gint        *port,
			    gboolean    *use_auth,
			    gchar      **username,
			    gchar      **password)
{
	GossipConfPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);
	
	priv = GET_PRIV (conf);
	/*	
	*use_http_proxy = gconf_client_get_bool (priv->gconf_client,
						 HTTP_PROXY_ROOT "/use_http_proxy",
						 NULL);
	*host = gconf_client_get_string (priv->gconf_client,
					 HTTP_PROXY_ROOT "/host",
					 NULL);
	*port = gconf_client_get_int (priv->gconf_client,
				      HTTP_PROXY_ROOT "/port",
				      NULL);
	*use_auth = gconf_client_get_bool (priv->gconf_client,
					   HTTP_PROXY_ROOT "/use_authentication",
					   NULL);
	*username = gconf_client_get_string (priv->gconf_client,
					     HTTP_PROXY_ROOT "/authentication_user",
					     NULL);
	*password = gconf_client_get_string (priv->gconf_client,
					     HTTP_PROXY_ROOT "/authentication_password",
					     NULL);
	*/

	*use_http_proxy = FALSE;
	*host = NULL;
	*port = 0;
	*use_auth = FALSE;
	*username = NULL;
	*password = NULL;
	
	return TRUE;
}

