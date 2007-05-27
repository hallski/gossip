/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Imendio AB
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

#include "gossip-heartbeat.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_HEARTBEAT, GossipHeartbeatPriv))

typedef struct _GossipHeartbeatPriv GossipHeartbeatPriv;

struct _GossipHeartbeatPriv {
	gint    interval;

	GSList *callbacks;

	guint   timeout_id;
};

typedef struct {
	guint               id;
	GossipHeartbeatFunc func;
	gpointer            user_data;
	GDestroyNotify      free_func;
} HeartbeatCallback;

static void         heartbeat_finalize         (GObject             *object);
static void         heartbeat_get_property     (GObject             *object,
						guint                param_id,
						GValue              *value,
						GParamSpec          *pspec);
static void         heartbeat_set_property     (GObject             *object,
						guint                param_id,
						const GValue        *value,
						GParamSpec          *pspec);
static gboolean     heartbeat_timeout_cb       (GossipHeartbeat     *heartbeat);
static void         heartbeat_start            (GossipHeartbeat     *heartbeat);
static void         heartbeat_stop             (GossipHeartbeat     *heartbeat);
static void         heartbeat_maybe_stop       (GossipHeartbeat     *heartbeat);

static HeartbeatCallback *
heartbeat_callback_new                         (GossipHeartbeatFunc  func,
						gpointer             user_data,
						GDestroyNotify       free_func);
static void         heartbeat_callback_free    (HeartbeatCallback   *callback);
static gboolean     heartbeat_callback_execute (GossipHeartbeat     *heartbeat,
						HeartbeatCallback   *callback);

enum {
	PROP_0,
	PROP_INTERVAL
};

G_DEFINE_TYPE (GossipHeartbeat, gossip_heartbeat, G_TYPE_OBJECT);

static void
gossip_heartbeat_class_init (GossipHeartbeatClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize     = heartbeat_finalize;
	object_class->get_property = heartbeat_get_property;
	object_class->set_property = heartbeat_set_property;

	g_object_class_install_property (object_class,
					 PROP_INTERVAL,
					 g_param_spec_int ("interval",
							   "Interval",
							   "Heartbeat interval.",
							   0, G_MAXINT,
							   0,
							   G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GossipHeartbeatPriv));
}

static void
gossip_heartbeat_init (GossipHeartbeat *presence)
{
	GossipHeartbeatPriv *priv;

	priv = GET_PRIV (presence);
}

static void
heartbeat_finalize (GObject *object)
{
	GossipHeartbeatPriv *priv;
	GSList              *l;

	priv = GET_PRIV (object);

	heartbeat_stop (GOSSIP_HEARTBEAT (object));

	for (l = priv->callbacks; l; l = l->next) {
		heartbeat_callback_free ((HeartbeatCallback *) l->data);
	}

	g_slist_free (priv->callbacks);

	(G_OBJECT_CLASS (gossip_heartbeat_parent_class)->finalize) (object);
}

static void
heartbeat_get_property (GObject    *object,
			guint       param_id,
			GValue     *value,
			GParamSpec *pspec)
{
	GossipHeartbeatPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_INTERVAL:
		g_value_set_int (value, priv->interval);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}
static void
heartbeat_set_property (GObject      *object,
			guint         param_id,
			const GValue *value,
			GParamSpec   *pspec)
{
	GossipHeartbeatPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_INTERVAL:
		priv->interval = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static gboolean
heartbeat_timeout_cb (GossipHeartbeat *heartbeat)
{
	GossipHeartbeatPriv *priv;
	GSList              *l;

	priv = GET_PRIV (heartbeat);

	l = priv->callbacks;
	while (l) {
		HeartbeatCallback *callback = (HeartbeatCallback *) l->data;
		GSList            *current_link = l;

		l = l->next;

		if (!heartbeat_callback_execute (heartbeat, callback)) {
			/* Remove callback if it returned FALSE */
			priv->callbacks = g_slist_delete_link (priv->callbacks,
							       current_link);
			heartbeat_callback_free (callback);
		}
	}

	heartbeat_maybe_stop (heartbeat);

	return TRUE;
}

static void
heartbeat_start (GossipHeartbeat *heartbeat)
{
	GossipHeartbeatPriv *priv;

	priv = GET_PRIV (heartbeat);

	if (priv->timeout_id != 0) {
		/* Already running */
		return;
	}

	priv->timeout_id = g_timeout_add (priv->interval,
					  (GSourceFunc) heartbeat_timeout_cb,
					  heartbeat);
}

static void
heartbeat_stop (GossipHeartbeat *heartbeat)
{
	GossipHeartbeatPriv *priv;

	priv = GET_PRIV (heartbeat);

	if (!priv->timeout_id) {
		/* No running timeout */
		return;
	}

	g_source_remove (priv->timeout_id);
	priv->timeout_id = 0;
}

static void
heartbeat_maybe_stop (GossipHeartbeat *heartbeat) 
{
	GossipHeartbeatPriv *priv;

	priv = GET_PRIV (heartbeat);

	if (g_slist_length (priv->callbacks) == 0) {
		heartbeat_stop (heartbeat);
	}
}

static HeartbeatCallback *
heartbeat_callback_new (GossipHeartbeatFunc func, 
			gpointer            user_data,
			GDestroyNotify      free_func)
{
	HeartbeatCallback *callback;
	static guint       id = 0;

	callback = g_slice_new (HeartbeatCallback);
	callback->id = ++id;
	callback->func = func;
	callback->user_data = user_data;
	callback->free_func = free_func;

	return callback;
}

static void
heartbeat_callback_free (HeartbeatCallback *callback)
{
	if (callback->free_func) {
		(callback->free_func) (callback->user_data);
	}

	g_slice_free (HeartbeatCallback, callback);
}

static gboolean
heartbeat_callback_execute (GossipHeartbeat   *heartbeat,
			    HeartbeatCallback *callback)
{
	return (callback->func) (heartbeat, callback->user_data);
}

guint
gossip_heartbeat_callback_add (GossipHeartbeat     *heartbeat,
			       GossipHeartbeatFunc  func,
			       gpointer             user_data)
{
	return gossip_heartbeat_callback_add_full (heartbeat, func, 
						   user_data, NULL);
}

guint
gossip_heartbeat_callback_add_full (GossipHeartbeat     *heartbeat,
				    GossipHeartbeatFunc  func,
				    gpointer             user_data,
				    GDestroyNotify       free_func)
{
	GossipHeartbeatPriv *priv;
	HeartbeatCallback   *callback;

	g_return_val_if_fail (GOSSIP_IS_HEARTBEAT (heartbeat), 0);
	g_return_val_if_fail (func != NULL, 0);

	priv = GET_PRIV (heartbeat);

	callback = heartbeat_callback_new (func, user_data, free_func);
	priv->callbacks = g_slist_append (priv->callbacks, callback);

	if (!priv->timeout_id) {
		heartbeat_start (heartbeat);
	}

	return callback->id;
}

void
gossip_heartbeat_callback_remove (GossipHeartbeat *heartbeat, guint id)
{
	GossipHeartbeatPriv *priv;
	GSList              *l;

	g_return_if_fail (GOSSIP_IS_HEARTBEAT (heartbeat));

	priv = GET_PRIV (heartbeat);

	for (l = priv->callbacks; l; l = l->next) {
		HeartbeatCallback *callback = (HeartbeatCallback *) l->data;

		if (callback->id == id) {
			heartbeat_callback_free (callback);
			priv->callbacks = g_slist_delete_link (priv->callbacks,
							       l);
			break;
		}
	}

	heartbeat_maybe_stop (heartbeat);
}

