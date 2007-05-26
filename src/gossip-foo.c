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

#include <string.h>

#include <libgossip/gossip-debug.h>

#include "gossip-app.h"
#include "gossip-idle.h"
#include "gossip-marshal.h"
#include "gossip-status-presets.h"
#include "gossip-stock.h"
#include "gossip-ui-utils.h"
#include "gossip-foo.h"

/* Number of seconds to flash the icon when explicitly entering away status
 * (activity is also allowed during this period).
 */
#define	LEAVE_SLACK 15

/* Number of seconds of slack when returning from autoaway. */
#define	BACK_SLACK 15

/* Number of seconds before entering autoaway and extended autoaway. */
#define	AWAY_TIME (5*60)
#define	EXT_AWAY_TIME (30*60)

#define DEBUG_DOMAIN_IDLE      "AppIdle"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_FOO, GossipFooPriv))

typedef struct _GossipFooPriv GossipFooPriv;

struct _GossipFooPriv {
	gint            my_prop;
	
	/* Presence set by the user (available/busy) */
	GossipPresence *presence;
	
	/* Away presence (away/xa), overrides priv->presence */
	GossipPresence *away_presence;

	time_t          leave_time;
};

static void         foo_finalize           (GObject             *object);
static void         foo_get_property       (GObject             *object,
					    guint                param_id,
					    GValue              *value,
					    GParamSpec          *pspec);
static void         foo_set_property       (GObject             *object,
					    guint                param_id,
					    const GValue        *value,
					    GParamSpec          *pspec);
static GossipPresence *   foo_get_presence (GossipFoo           *foo);
static GossipPresence *   foo_get_away_presence (GossipFoo *foo);
static void               foo_set_away_presence (GossipFoo *foo,
						 GossipPresence *presence);
static void               foo_set_away          (GossipFoo   *foo,
						 const gchar *status);
static void               foo_set_leave_time    (GossipFoo *foo,
						 time_t     t);

static void               foo_start_flash       (GossipFoo *foo);
/* clears status data from autoaway mode */
static void               foo_clear_away        (GossipFoo *foo);
static gboolean           foo_idle_check_cb     (GossipFoo *foo);

enum {
	PROP_0,
	PROP_MY_PROP
};

enum {
	START_FLASH,
	STOP_FLASH,
	UPDATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GossipFoo, gossip_foo, G_TYPE_OBJECT);

static void
gossip_foo_class_init (GossipFooClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize     = foo_finalize;
	object_class->get_property = foo_get_property;
	object_class->set_property = foo_set_property;

	g_object_class_install_property (object_class,
					 PROP_MY_PROP,
					 g_param_spec_int ("my-prop",
							   "",
							   "",
							   0, 1,
							   1,
							   G_PARAM_READWRITE));

	signals[START_FLASH] =
		g_signal_new ("start-flash",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	signals[STOP_FLASH] =
		g_signal_new ("stop-flash",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	signals[UPDATED] =
		g_signal_new ("updated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_type_class_add_private (object_class, sizeof (GossipFooPriv));
}

static void
gossip_foo_init (GossipFoo *foo)
{
	GossipFooPriv *priv;

	priv = GET_PRIV (foo);

	priv->presence = gossip_presence_new ();
	gossip_presence_set_state (priv->presence,
				   GOSSIP_PRESENCE_STATE_AVAILABLE);

	priv->away_presence = NULL;

	/* Set the idle time checker. */
	g_timeout_add (2 * 1000, 
		       (GSourceFunc) foo_idle_check_cb, foo);

}

static void
foo_finalize (GObject *object)
{
	GossipFooPriv *priv;

	priv = GET_PRIV (object);

	if (priv->presence) {
		g_object_unref (priv->presence);
	}

	if (priv->away_presence) {
		g_object_unref (priv->away_presence);
	}

	(G_OBJECT_CLASS (gossip_foo_parent_class)->finalize) (object);
}

static void
foo_get_property (GObject    *object,
		    guint       param_id,
		    GValue     *value,
		    GParamSpec *pspec)
{
	GossipFooPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_MY_PROP:
		g_value_set_int (value, priv->my_prop);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}
static void
foo_set_property (GObject      *object,
		    guint         param_id,
		    const GValue *value,
		    GParamSpec   *pspec)
{
	GossipFooPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_MY_PROP:
		priv->my_prop = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static GossipPresence *
foo_get_presence (GossipFoo *foo)
{
	GossipFooPriv *priv;
	
	g_return_val_if_fail (GOSSIP_IS_FOO (foo), NULL);

	priv = GET_PRIV (foo);

	return priv->presence;
}

static void
foo_set_leave_time (GossipFoo *foo, time_t t)
{
	GossipFooPriv *priv;

	priv = GET_PRIV (foo);

	priv->leave_time = t;
}

static void
foo_start_flash (GossipFoo *foo)
{
	g_signal_emit (foo, signals[START_FLASH], 0);
}

static void
foo_clear_away (GossipFoo *foo)
{
	GossipFooPriv *priv;

	priv = GET_PRIV (foo);

	foo_set_away_presence (foo, NULL);

	/* Clear the default state */
	gossip_status_presets_clear_default ();

	foo_set_leave_time (foo, 0);
	gossip_foo_stop_flash (foo);

	/* Force this so we don't get a delay in the display */
	gossip_foo_updated (foo);
}

static gboolean
foo_idle_check_cb (GossipFoo *foo)
{
	GossipFooPriv       *priv;
	gint32               idle;
	GossipPresenceState  state;
	gboolean             presence_changed = FALSE;

	priv = GET_PRIV (foo);

	if (!gossip_app_is_connected ()) {
		return TRUE;
	}

	idle = gossip_idle_get_seconds ();
	state = gossip_presence_get_state (gossip_foo_get_effective_presence (foo));

	/* gossip_debug (DEBUG_DOMAIN_IDLE, "Idle for:%d", idle); */

	/* We're going away, allow some slack. */
	if (gossip_foo_get_leave_time (foo) > 0) {
		if (time (NULL) - gossip_foo_get_leave_time (foo) > LEAVE_SLACK) {
			foo_set_leave_time (foo, 0);
			gossip_foo_stop_flash (foo);

			gossip_idle_reset ();
			gossip_debug (DEBUG_DOMAIN_IDLE, "OK, away now.");
		}

		return TRUE;
	}
	else if (state != GOSSIP_PRESENCE_STATE_EXT_AWAY &&
		 idle > EXT_AWAY_TIME) {
		/* Presence may be idle if the screensaver has been started and
		 * hence no away_presence set.
		 */
		if (!foo_get_away_presence (foo)) {
			GossipPresence *presence;

			presence = gossip_presence_new ();
			foo_set_away_presence (foo, presence);
			g_object_unref (presence);
		}

		/* Presence will already be away. */
		gossip_debug (DEBUG_DOMAIN_IDLE, "Going to ext away...");
		gossip_presence_set_state (foo_get_away_presence (foo),
					   GOSSIP_PRESENCE_STATE_EXT_AWAY);
		presence_changed = TRUE;
	}
	else if (state != GOSSIP_PRESENCE_STATE_AWAY &&
		 state != GOSSIP_PRESENCE_STATE_EXT_AWAY &&
		 idle > AWAY_TIME) {
		gossip_debug (DEBUG_DOMAIN_IDLE, "Going to away...");
		foo_set_away (foo, NULL);
		presence_changed = TRUE;
	}
	else if (state == GOSSIP_PRESENCE_STATE_AWAY ||
		 state == GOSSIP_PRESENCE_STATE_EXT_AWAY) {
		/* Allow some slack before returning from away. */
		if (idle >= -BACK_SLACK && idle <= 0) {
			/* gossip_debug (DEBUG_DOMAIN_IDLE, "Slack, do nothing."); */
			foo_start_flash (foo);
		}
		else if (idle < -BACK_SLACK) {
			gossip_debug (DEBUG_DOMAIN_IDLE, "No more slack, break interrupted.");
			foo_clear_away (foo);
			return TRUE;
		}
		else if (idle > BACK_SLACK) {
			/* gossip_debug (DEBUG_DOMAIN_IDLE, "Don't interrupt break."); */
			gossip_foo_stop_flash (foo);
		}
	}

	if (presence_changed) {
		gossip_foo_updated (foo);
	}

	return TRUE;
}


GossipPresence *
foo_get_away_presence (GossipFoo *foo)
{
	GossipFooPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_FOO (foo), NULL);

	priv = GET_PRIV (foo);

	return priv->away_presence;
}

void
foo_set_away_presence (GossipFoo *foo, GossipPresence *presence)
{
	GossipFooPriv *priv;

	g_return_if_fail (GOSSIP_IS_FOO (foo));

	priv = GET_PRIV (foo);

	if (priv->away_presence) {
		g_object_unref (priv->away_presence);
		priv->away_presence = NULL;
	}

	if (presence) {
		priv->away_presence = g_object_ref (presence);
	} 
}

void
foo_set_away (GossipFoo *foo, const gchar *status)
{
	GossipFooPriv *priv;

	priv = GET_PRIV (foo);

	if (!foo_get_away_presence (foo)) {
		GossipPresence *presence;

		presence = gossip_presence_new ();
		gossip_presence_set_state (presence, 
					   GOSSIP_PRESENCE_STATE_AWAY);
		foo_set_away_presence (foo, presence);
		g_object_unref (presence);
	}

	foo_set_leave_time (foo, time (NULL));
	gossip_idle_reset ();

	if (status) {
		gossip_presence_set_status (foo_get_away_presence (foo),
					    status);
	}
	
}

GossipPresence *
gossip_foo_get_effective_presence (GossipFoo *foo)
{
	GossipFooPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_FOO (foo), NULL);

	priv = GET_PRIV (foo);

	if (priv->away_presence) {
		return priv->away_presence;
	}

	return priv->presence;
}

GossipPresenceState
gossip_foo_get_current_state (GossipFoo *foo)
{
	g_return_val_if_fail (GOSSIP_IS_FOO (foo), 
			      GOSSIP_PRESENCE_STATE_UNAVAILABLE);

	if (!gossip_session_is_connected (gossip_app_get_session (), NULL)) {
		return GOSSIP_PRESENCE_STATE_UNAVAILABLE;
	}

	return gossip_presence_get_state (gossip_foo_get_effective_presence (foo));
}

GossipPresenceState
gossip_foo_get_previous_state (GossipFoo *foo)
{
	g_return_val_if_fail (GOSSIP_IS_FOO (foo),
			      GOSSIP_PRESENCE_STATE_UNAVAILABLE);

	if (!gossip_session_is_connected (gossip_app_get_session (), NULL)) {
		return GOSSIP_PRESENCE_STATE_UNAVAILABLE;
	}

	return gossip_presence_get_state (foo_get_presence (foo));
}

GdkPixbuf *
gossip_foo_get_current_status_pixbuf (GossipFoo *foo)
{
	g_return_val_if_fail (GOSSIP_IS_FOO (foo), NULL);

	if (!gossip_session_is_connected (gossip_app_get_session (), NULL)) {
		return gossip_pixbuf_from_stock (GOSSIP_STOCK_OFFLINE,
						 GTK_ICON_SIZE_MENU);
	}

	return gossip_pixbuf_for_presence (gossip_foo_get_effective_presence (foo));
}

GdkPixbuf *
gossip_foo_get_explicit_status_pixbuf (GossipFoo *foo)
{
	return gossip_pixbuf_for_presence (foo_get_presence (foo));
}

time_t
gossip_foo_get_leave_time (GossipFoo *foo)
{
	GossipFooPriv *priv;

	priv = GET_PRIV (foo);
	
	return priv->leave_time;
}

void
gossip_foo_stop_flash (GossipFoo *foo)
{
	g_signal_emit (foo, signals[STOP_FLASH], 0);
}

void
gossip_foo_updated (GossipFoo *foo)
{
	g_signal_emit (foo, signals[UPDATED], 0);
}

void
gossip_foo_set_not_away (GossipFoo *foo)
{
	/* If we just left, allow some slack. */
	if (gossip_foo_get_leave_time (foo)) {
		return;
	}

	if (foo_get_away_presence (foo)) {
		foo_clear_away (foo);
	}
}

void
gossip_foo_set_state_status (GossipFoo           *foo,
			     GossipPresenceState  state,
			     const gchar         *status)
{
	GossipFooPriv *priv;

	priv = GET_PRIV (foo);

	if (state != GOSSIP_PRESENCE_STATE_AWAY) {
		const gchar *default_status;

		/* Send NULL if it's not changed from default status string. We
		 * do this so that the translated default strings will work
		 * across two Gossips.
		 */
		default_status = gossip_presence_state_get_default_status (state);

		if (status && strcmp (status, default_status) == 0) {
			g_object_set (foo_get_presence (foo),
				      "status", NULL, NULL);
		} else {
			g_object_set (foo_get_presence (foo),
				      "status", status, NULL);
		}

		g_object_set (foo_get_presence (foo), 
			      "state", state, NULL);

		gossip_foo_stop_flash (foo);
		foo_clear_away (foo);
	} else {
		foo_start_flash (foo);
		foo_set_away (foo, status);
		gossip_foo_updated (foo);
	}
}


