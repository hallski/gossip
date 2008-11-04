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
 */

#include "config.h"

#include <gtk/gtk.h>

#include "gossip-contact.h"
#include "gossip-message.h"
#include "gossip-ft.h"
#include "gossip-debug.h"
#include "gossip-stock.h"
#include "gossip-event.h"

#define DEBUG_DOMAIN "Event"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_EVENT, GossipEventPriv))

typedef struct _GossipEventPriv GossipEventPriv;

struct _GossipEventPriv {
	GossipEventId    id;
	GossipEventType  type;

	gchar           *msg;

	/* A GossipMessage or GossipContact depending on event type */
	GObject         *data;
};

static void gossip_event_class_init (GossipEventClass *klass);
static void gossip_event_init       (GossipEvent      *event);
static void gossip_event_finalize   (GObject          *object);
static void event_get_property      (GObject          *object,
				     guint             param_id,
				     GValue           *value,
				     GParamSpec       *pspec);
static void event_set_property      (GObject          *object,
				     guint             param_id,
				     const GValue     *value,
				     GParamSpec       *pspec);

enum {
	PROP_0,
	PROP_ID,
	PROP_TYPE,
	PROP_MSG,
	PROP_DATA
};

static gpointer gossip_event_parent_class = NULL;

/* Define ourselves instead of using G_DEFINE_TYPE so that we can name it  * 
 * _get_gtype as get_type is used for returning the type of event          */
GType
gossip_event_get_gtype (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GossipEventClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) gossip_event_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GossipEvent),
			0,    /* n_preallocs */
			(GInstanceInitFunc) gossip_event_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "GossipEvent",
					       &info, 0);
	}

	return type;
}

static void
gossip_event_class_init (GossipEventClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	gossip_event_parent_class = g_type_class_peek_parent (klass);

	object_class->finalize     = gossip_event_finalize;
	object_class->get_property = event_get_property;
	object_class->set_property = event_set_property;

	g_object_class_install_property (object_class,
					 PROP_ID,
					 g_param_spec_int ("id",
							   "Event id",
							   "The event identification",
							   1,
							   G_MAXINT,
							   1,
							   G_PARAM_READABLE));


	g_object_class_install_property (object_class,
					 PROP_TYPE,
					 g_param_spec_int ("type",
							   "Event type",
							   "The type of the event",
							   GOSSIP_EVENT_NEW_MESSAGE,
							   GOSSIP_EVENT_ERROR,
							   GOSSIP_EVENT_NEW_MESSAGE,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_MSG,
					 g_param_spec_string ("message",
							      "Event message",
							      "Human readable event message",
							      "",
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_DATA,
					 g_param_spec_object ("data",
							      "Event data",
							      "Data object sent with the event",
							      G_TYPE_OBJECT,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GossipEventPriv));
}

static void
gossip_event_init (GossipEvent *event)
{
	GossipEventPriv      *priv;
	static GossipEventId  id = 0;

	priv = GET_PRIV (event);

	priv->msg = NULL;
	priv->data = NULL;

	priv->id = ++id;
}

static void
gossip_event_finalize (GObject *object)
{
	GossipEventPriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->msg);
	if (priv->data) {
		g_object_unref (priv->data);
	}

	(G_OBJECT_CLASS (gossip_event_parent_class)->finalize) (object);
}

static void
event_get_property (GObject    *object,
		    guint       param_id,
		    GValue     *value,
		    GParamSpec *pspec)
{
	GossipEventPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ID:
		g_value_set_int (value, priv->id);
		break;
	case PROP_TYPE:
		g_value_set_int (value, priv->type);
		break;
	case PROP_MSG:
		if (priv->msg) {
			g_value_set_string (value, priv->msg);
		} else {
			g_value_set_string (value, "");
		}
		break;
	case PROP_DATA:
		if (priv->data) {
			g_value_set_object (value, priv->data);
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
event_set_property (GObject      *object,
		    guint         param_id,
		    const GValue *value,
		    GParamSpec   *pspec)
{
	GossipEventPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_TYPE:
		priv->type = g_value_get_int (value);
		break;
	case PROP_MSG:
		g_free (priv->msg);
		priv->msg = g_strdup (g_value_get_string (value));
		break;
	case PROP_DATA:
		if (priv->data) {
			g_object_unref (priv->data);
		}

		priv->data = g_object_ref (g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

GossipEvent *
gossip_event_new (GossipEventType type)
{
	gossip_debug (DEBUG_DOMAIN, "New of type:%d", type);

	return g_object_new (GOSSIP_TYPE_EVENT,
			     "type", type,
			     NULL);
}

GossipEventId
gossip_event_get_id (GossipEvent *event)
{
	GossipEventPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_EVENT (event), -1);

	priv = GET_PRIV (event);

	return priv->id;
}

const gchar *
gossip_event_get_message (GossipEvent *event)
{
	GossipEventPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_EVENT (event), "");

	priv = GET_PRIV (event);

	return priv->msg;
}

GossipContact *
gossip_event_get_contact (GossipEvent *event)
{
	GossipMessage *message;
	GossipContact *contact;
	GossipFT      *ft;

	switch (gossip_event_get_type (event)) {
	case GOSSIP_EVENT_NEW_MESSAGE:
		message = GOSSIP_MESSAGE (gossip_event_get_data (event));
		contact = gossip_message_get_sender (message);
		break;

	case GOSSIP_EVENT_SUBSCRIPTION_REQUEST:
		contact = GOSSIP_CONTACT (gossip_event_get_data (event));
		break;

	case GOSSIP_EVENT_FILE_TRANSFER_REQUEST:
		ft = GOSSIP_FT (gossip_event_get_data (event));
		contact = gossip_ft_get_contact (ft);
		break;

	default:
		/* Not handled */
		gossip_debug (DEBUG_DOMAIN,
			      "Event type does not have a contact: %d",
			      gossip_event_get_type (event));
		return NULL;
	}

	return contact;
}

GossipEventType
gossip_event_get_type (GossipEvent *event)
{
	GossipEventPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_EVENT (event), -1);

	priv = GET_PRIV (event);

	return priv->type;
}

GObject *
gossip_event_get_data (GossipEvent *event)
{
	GossipEventPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_EVENT (event), NULL);

	priv = GET_PRIV (event);

	return priv->data;
}

void
gossip_event_set_data (GossipEvent *event,
		       GObject     *data)
{
	GossipEventPriv *priv;

	g_return_if_fail (GOSSIP_IS_EVENT (event));

	priv = GET_PRIV (event);

	if (priv->data) {
		g_object_unref (priv->data);
	}

	priv->data = g_object_ref (data);
}

guint
gossip_event_hash (gconstpointer key)
{
	GossipEventId id;

	g_return_val_if_fail (GOSSIP_IS_EVENT (key), 0);

	id = gossip_event_get_id (GOSSIP_EVENT (key));

	return g_int_hash (&id);
}

gboolean
gossip_event_equal (gconstpointer a,
		    gconstpointer b)
{
	GossipEventId id_a;
	GossipEventId id_b;

	g_return_val_if_fail (GOSSIP_IS_EVENT (a), FALSE);
	g_return_val_if_fail (GOSSIP_IS_EVENT (b), FALSE);

	id_a = gossip_event_get_id (GOSSIP_EVENT (a));
	id_b = gossip_event_get_id (GOSSIP_EVENT (b));

	return (id_a == id_b);
}

gint
gossip_event_compare (gconstpointer a,
		      gconstpointer b)
{
	GossipEventId id_a;
	GossipEventId id_b;

	g_return_val_if_fail (GOSSIP_IS_EVENT (a), 0);
	g_return_val_if_fail (GOSSIP_IS_EVENT (b), 0);

	id_a = gossip_event_get_id (GOSSIP_EVENT (a));
	id_b = gossip_event_get_id (GOSSIP_EVENT (b));

	if (id_a == id_b) {
		return 0;
	}

	if (id_a < id_b) {
		return -1;
	}

	return 1;
}

const gchar *
gossip_event_get_stock_id (GossipEvent *event)
{
	switch (gossip_event_get_type (event)) {
	case GOSSIP_EVENT_NEW_MESSAGE:
	case GOSSIP_EVENT_SERVER_MESSAGE:
		return GOSSIP_STOCK_MESSAGE;

	case GOSSIP_EVENT_FILE_TRANSFER_REQUEST:
		return GOSSIP_STOCK_FILE_TRANSFER;

	case GOSSIP_EVENT_SUBSCRIPTION_REQUEST:
		return GTK_STOCK_DIALOG_QUESTION;

	default:
		/* Shouldn't happen */
		return GTK_STOCK_DIALOG_WARNING;
	}

	return NULL;
}

