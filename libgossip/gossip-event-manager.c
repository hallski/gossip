/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio AB
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

#include "libgossip-marshal.h"

#include "gossip-event-manager.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_EVENT_MANAGER, GossipEventManagerPriv))

typedef struct _GossipEventManagerPriv GossipEventManagerPriv;

struct _GossipEventManagerPriv {
	GHashTable *events;
};

enum {
	EVENT_ADDED,
	EVENT_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

typedef struct {
	GossipEvent                 *event;
	GossipEventActivateFunction  callback;
	GObject                     *issuer;
} EventData;

struct FindEvent {
	GossipEventId  event_id;
	GossipEvent   *event;
};

static void     event_manager_finalize              (GObject      *object);
static void     event_manager_free_event            (EventData    *data);
static void     event_manager_get_events_foreach_cb (GossipEvent  *event,
						     EventData    *data,
						     GList       **list);
static gboolean event_manager_find_foreach          (GossipEvent  *event,
						     EventData    *event_data,
						     gpointer     *p);

G_DEFINE_TYPE (GossipEventManager, gossip_event_manager, G_TYPE_OBJECT);

static void
gossip_event_manager_class_init (GossipEventManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = event_manager_finalize;

	signals[EVENT_ADDED] =
		g_signal_new ("event-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_EVENT);
	signals[EVENT_REMOVED] =
		g_signal_new ("event-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_EVENT);

	g_type_class_add_private (object_class,
				  sizeof (GossipEventManagerPriv));
}

static void
gossip_event_manager_init (GossipEventManager *manager)
{
	GossipEventManagerPriv *priv;

	priv = GET_PRIV (manager);

	priv->events = g_hash_table_new_full (gossip_event_hash,
					      gossip_event_equal,
					      g_object_unref,
					      (GDestroyNotify) event_manager_free_event);
}

static void
event_manager_finalize (GObject *object)
{
	GossipEventManagerPriv *priv;

	priv = GET_PRIV (object);

	g_hash_table_destroy (priv->events);

	(G_OBJECT_CLASS (gossip_event_manager_parent_class)->finalize) (object);
}

static void
event_manager_free_event (EventData *data)
{
	g_object_unref (data->event);
	g_object_unref (data->issuer);

	g_free (data);
}

GossipEventManager *
gossip_event_manager_new (void)
{
	return g_object_new (GOSSIP_TYPE_EVENT_MANAGER, NULL);
}

void
gossip_event_manager_add (GossipEventManager          *manager,
			  GossipEvent                 *event,
			  GossipEventActivateFunction  callback,
			  GObject                     *object)

{
	GossipEventManagerPriv *priv;
	EventData              *data;

	g_return_if_fail (GOSSIP_IS_EVENT_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_EVENT (event));
	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (callback != NULL);

	priv = GET_PRIV (manager);

	data = g_new0 (EventData, 1);

	data->event    = g_object_ref (event);
	data->issuer   = g_object_ref (object);
	data->callback = callback;

	g_hash_table_insert (priv->events, g_object_ref (event), data);

	g_signal_emit (manager, signals[EVENT_ADDED], 0, event);
}

void
gossip_event_manager_remove (GossipEventManager *manager,
			     GossipEvent        *event,
			     GObject            *object)
{
	GossipEventManagerPriv *priv;
	EventData              *data;

	g_return_if_fail (GOSSIP_IS_EVENT_MANAGER (manager));

	priv = GET_PRIV (manager);

	data = g_hash_table_lookup (priv->events, event);

	if (!data) {
		return;
	}

	g_signal_emit (manager, signals[EVENT_REMOVED], 0, event);

	g_hash_table_remove (priv->events, event);
}

void
gossip_event_manager_activate (GossipEventManager *manager,
			       GossipEvent        *event)
{
	GossipEventManagerPriv *priv;
	EventData              *data;
	GObject                *issuer;

	g_return_if_fail (GOSSIP_IS_EVENT_MANAGER (manager));

	priv = GET_PRIV (manager);

	data = g_hash_table_lookup (priv->events, event);
	if (!data) {
		return;
	}

	event = g_object_ref (event);
	issuer = g_object_ref (data->issuer);

	(data->callback) (manager, event, data->issuer);

	gossip_event_manager_remove (manager, event, issuer);

	g_object_unref (event);
	g_object_unref (issuer);
}

static gboolean
event_manager_find_foreach (GossipEvent *event,
			    EventData   *event_data,
			    gpointer    *p)
{
	GossipEventId event_id;

	event_id = GPOINTER_TO_INT (p);

	if (gossip_event_get_id (event) == event_id) {
		return TRUE;
	}

	return FALSE;
}

void
gossip_event_manager_activate_by_id (GossipEventManager *manager,
				     GossipEventId       event_id)
{
	GossipEventManagerPriv *priv;
	EventData              *data;
	GossipEvent            *event;
	GObject                *issuer;

	g_return_if_fail (GOSSIP_IS_EVENT_MANAGER (manager));

	priv = GET_PRIV (manager);

	data = g_hash_table_find (priv->events,
				  (GHRFunc) event_manager_find_foreach,
				  GINT_TO_POINTER (event_id));

	if (!data) {
		return;
	}

	event = g_object_ref (data->event);
	issuer = g_object_ref (data->issuer);

	(data->callback) (manager, data->event, data->issuer);

	gossip_event_manager_remove (manager, event, issuer);

	g_object_unref (event);
	g_object_unref (issuer);
}

static void
event_manager_get_events_foreach_cb (GossipEvent  *event,
				     EventData    *data,
				     GList       **list)
{
	if (list) {
		*list = g_list_append (*list, event);
	}
}

GList *
gossip_event_manager_get_events (GossipEventManager *manager)
{
	GossipEventManagerPriv *priv;
	GList                  *list = NULL;

	g_return_val_if_fail (GOSSIP_IS_EVENT_MANAGER (manager), NULL);

	priv = GET_PRIV (manager);

	g_hash_table_foreach (priv->events,
			      (GHFunc) event_manager_get_events_foreach_cb,
			      &list);

	return list;
}

guint
gossip_event_manager_get_event_count (GossipEventManager *manager)
{
	GossipEventManagerPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_EVENT_MANAGER (manager), 0);

	priv = GET_PRIV (manager);

	return g_hash_table_size (priv->events);
}
