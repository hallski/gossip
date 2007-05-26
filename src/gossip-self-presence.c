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
#include "gossip-self-presence.h"

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

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_SELF_PRESENCE, GossipSelfPresencePriv))

typedef struct _GossipSelfPresencePriv GossipSelfPresencePriv;

struct _GossipSelfPresencePriv {
	/* Presence set by the user (available/busy) */
	GossipPresence *presence;
	
	/* Away presence (away/xa), overrides priv->presence */
	GossipPresence *away_presence;

	time_t          leave_time;
};

static void             self_presence_finalize           (GObject            *object);
static GossipPresence * self_presence_get_presence       (GossipSelfPresence *self_presence);
static GossipPresence * self_presence_get_away_presence  (GossipSelfPresence *self_presence);
static void             self_presence_set_away_presence  (GossipSelfPresence *self_presence,
							  GossipPresence     *presence);
static void             self_presence_set_away           (GossipSelfPresence *self_presence,
							  const gchar        *status);
static void             self_presence_set_leave_time     (GossipSelfPresence *self_presence,
							  time_t              t);

static void             self_presence_start_flash        (GossipSelfPresence *self_presence);
/* clears status data from autoaway mode */
static void             self_presence_clear_away         (GossipSelfPresence *self_presence);
static gboolean         self_presence_idle_check_cb      (GossipSelfPresence *self_presence);

enum {
	START_FLASH,
	STOP_FLASH,
	UPDATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GossipSelfPresence, gossip_self_presence, G_TYPE_OBJECT);

static void
gossip_self_presence_class_init (GossipSelfPresenceClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize = self_presence_finalize;

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

	g_type_class_add_private (object_class, sizeof (GossipSelfPresencePriv));
}

static void
gossip_self_presence_init (GossipSelfPresence *self_presence)
{
	GossipSelfPresencePriv *priv;

	priv = GET_PRIV (self_presence);

	priv->presence = gossip_presence_new ();
	gossip_presence_set_state (priv->presence,
				   GOSSIP_PRESENCE_STATE_AVAILABLE);

	priv->away_presence = NULL;

	/* Set the idle time checker. */
	g_timeout_add (2 * 1000, 
		       (GSourceFunc) self_presence_idle_check_cb, self_presence);

}

static void
self_presence_finalize (GObject *object)
{
	GossipSelfPresencePriv *priv;

	priv = GET_PRIV (object);

	if (priv->presence) {
		g_object_unref (priv->presence);
	}

	if (priv->away_presence) {
		g_object_unref (priv->away_presence);
	}

	(G_OBJECT_CLASS (gossip_self_presence_parent_class)->finalize) (object);
}

static GossipPresence *
self_presence_get_presence (GossipSelfPresence *self_presence)
{
	GossipSelfPresencePriv *priv;
	
	g_return_val_if_fail (GOSSIP_IS_SELF_PRESENCE (self_presence), NULL);

	priv = GET_PRIV (self_presence);

	return priv->presence;
}

static void
self_presence_set_leave_time (GossipSelfPresence *self_presence, time_t t)
{
	GossipSelfPresencePriv *priv;

	priv = GET_PRIV (self_presence);

	priv->leave_time = t;
}

static void
self_presence_start_flash (GossipSelfPresence *self_presence)
{
	g_signal_emit (self_presence, signals[START_FLASH], 0);
}

static void
self_presence_clear_away (GossipSelfPresence *self_presence)
{
	GossipSelfPresencePriv *priv;

	priv = GET_PRIV (self_presence);

	self_presence_set_away_presence (self_presence, NULL);

	/* Clear the default state */
	gossip_status_presets_clear_default ();

	self_presence_set_leave_time (self_presence, 0);
	gossip_self_presence_stop_flash (self_presence);

	/* Force this so we don't get a delay in the display */
	gossip_self_presence_updated (self_presence);
}

static gboolean
self_presence_idle_check_cb (GossipSelfPresence *self_presence)
{
	GossipSelfPresencePriv       *priv;
	gint32               idle;
	GossipPresenceState  state;
	gboolean             presence_changed = FALSE;

	priv = GET_PRIV (self_presence);

	if (!gossip_app_is_connected ()) {
		return TRUE;
	}

	idle = gossip_idle_get_seconds ();
	state = gossip_presence_get_state (gossip_self_presence_get_effective (self_presence));

	/* gossip_debug (DEBUG_DOMAIN_IDLE, "Idle for:%d", idle); */

	/* We're going away, allow some slack. */
	if (gossip_self_presence_get_leave_time (self_presence) > 0) {
		if (time (NULL) - gossip_self_presence_get_leave_time (self_presence) > LEAVE_SLACK) {
			self_presence_set_leave_time (self_presence, 0);
			gossip_self_presence_stop_flash (self_presence);

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
		if (!self_presence_get_away_presence (self_presence)) {
			GossipPresence *presence;

			presence = gossip_presence_new ();
			self_presence_set_away_presence (self_presence, presence);
			g_object_unref (presence);
		}

		/* Presence will already be away. */
		gossip_debug (DEBUG_DOMAIN_IDLE, "Going to ext away...");
		gossip_presence_set_state (self_presence_get_away_presence (self_presence),
					   GOSSIP_PRESENCE_STATE_EXT_AWAY);
		presence_changed = TRUE;
	}
	else if (state != GOSSIP_PRESENCE_STATE_AWAY &&
		 state != GOSSIP_PRESENCE_STATE_EXT_AWAY &&
		 idle > AWAY_TIME) {
		gossip_debug (DEBUG_DOMAIN_IDLE, "Going to away...");
		self_presence_set_away (self_presence, NULL);
		presence_changed = TRUE;
	}
	else if (state == GOSSIP_PRESENCE_STATE_AWAY ||
		 state == GOSSIP_PRESENCE_STATE_EXT_AWAY) {
		/* Allow some slack before returning from away. */
		if (idle >= -BACK_SLACK && idle <= 0) {
			/* gossip_debug (DEBUG_DOMAIN_IDLE, "Slack, do nothing."); */
			self_presence_start_flash (self_presence);
		}
		else if (idle < -BACK_SLACK) {
			gossip_debug (DEBUG_DOMAIN_IDLE, "No more slack, break interrupted.");
			self_presence_clear_away (self_presence);
			return TRUE;
		}
		else if (idle > BACK_SLACK) {
			/* gossip_debug (DEBUG_DOMAIN_IDLE, "Don't interrupt break."); */
			gossip_self_presence_stop_flash (self_presence);
		}
	}

	if (presence_changed) {
		gossip_self_presence_updated (self_presence);
	}

	return TRUE;
}


GossipPresence *
self_presence_get_away_presence (GossipSelfPresence *self_presence)
{
	GossipSelfPresencePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_SELF_PRESENCE (self_presence), NULL);

	priv = GET_PRIV (self_presence);

	return priv->away_presence;
}

void
self_presence_set_away_presence (GossipSelfPresence *self_presence, 
				 GossipPresence     *presence)
{
	GossipSelfPresencePriv *priv;

	g_return_if_fail (GOSSIP_IS_SELF_PRESENCE (self_presence));

	priv = GET_PRIV (self_presence);

	if (priv->away_presence) {
		g_object_unref (priv->away_presence);
		priv->away_presence = NULL;
	}

	if (presence) {
		priv->away_presence = g_object_ref (presence);
	} 
}

void
self_presence_set_away (GossipSelfPresence *self_presence, const gchar *status)
{
	GossipSelfPresencePriv *priv;

	priv = GET_PRIV (self_presence);

	if (!self_presence_get_away_presence (self_presence)) {
		GossipPresence *presence;

		presence = gossip_presence_new ();
		gossip_presence_set_state (presence, 
					   GOSSIP_PRESENCE_STATE_AWAY);
		self_presence_set_away_presence (self_presence, presence);
		g_object_unref (presence);
	}

	self_presence_set_leave_time (self_presence, time (NULL));
	gossip_idle_reset ();

	if (status) {
		gossip_presence_set_status (self_presence_get_away_presence (self_presence),
					    status);
	}
	
}

GossipPresence *
gossip_self_presence_get_effective (GossipSelfPresence *self_presence)
{
	GossipSelfPresencePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_SELF_PRESENCE (self_presence), NULL);

	priv = GET_PRIV (self_presence);

	if (priv->away_presence) {
		return priv->away_presence;
	}

	return priv->presence;
}

GossipPresenceState
gossip_self_presence_get_current_state (GossipSelfPresence *self_presence)
{
	g_return_val_if_fail (GOSSIP_IS_SELF_PRESENCE (self_presence), 
			      GOSSIP_PRESENCE_STATE_UNAVAILABLE);

	if (!gossip_session_is_connected (gossip_app_get_session (), NULL)) {
		return GOSSIP_PRESENCE_STATE_UNAVAILABLE;
	}

	return gossip_presence_get_state (gossip_self_presence_get_effective (self_presence));
}

GossipPresenceState
gossip_self_presence_get_previous_state (GossipSelfPresence *self_presence)
{
	g_return_val_if_fail (GOSSIP_IS_SELF_PRESENCE (self_presence),
			      GOSSIP_PRESENCE_STATE_UNAVAILABLE);

	if (!gossip_session_is_connected (gossip_app_get_session (), NULL)) {
		return GOSSIP_PRESENCE_STATE_UNAVAILABLE;
	}

	return gossip_presence_get_state (self_presence_get_presence (self_presence));
}

GdkPixbuf *
gossip_self_presence_get_current_pixbuf (GossipSelfPresence *self_presence)
{
	g_return_val_if_fail (GOSSIP_IS_SELF_PRESENCE (self_presence), NULL);

	if (!gossip_session_is_connected (gossip_app_get_session (), NULL)) {
		return gossip_pixbuf_from_stock (GOSSIP_STOCK_OFFLINE,
						 GTK_ICON_SIZE_MENU);
	}

	return gossip_pixbuf_for_presence (gossip_self_presence_get_effective (self_presence));
}

GdkPixbuf *
gossip_self_presence_get_explicit_pixbuf (GossipSelfPresence *self_presence)
{
	return gossip_pixbuf_for_presence (self_presence_get_presence (self_presence));
}

time_t
gossip_self_presence_get_leave_time (GossipSelfPresence *self_presence)
{
	GossipSelfPresencePriv *priv;

	priv = GET_PRIV (self_presence);
	
	return priv->leave_time;
}

void
gossip_self_presence_stop_flash (GossipSelfPresence *self_presence)
{
	g_signal_emit (self_presence, signals[STOP_FLASH], 0);
}

void
gossip_self_presence_updated (GossipSelfPresence *self_presence)
{
	g_signal_emit (self_presence, signals[UPDATED], 0);
}

void
gossip_self_presence_set_not_away (GossipSelfPresence *self_presence)
{
	/* If we just left, allow some slack. */
	if (gossip_self_presence_get_leave_time (self_presence)) {
		return;
	}

	if (self_presence_get_away_presence (self_presence)) {
		self_presence_clear_away (self_presence);
	}
}

void
gossip_self_presence_set_state_status (GossipSelfPresence  *self_presence,
				       GossipPresenceState  state,
				       const gchar         *status)
{
	GossipSelfPresencePriv *priv;

	priv = GET_PRIV (self_presence);

	if (state != GOSSIP_PRESENCE_STATE_AWAY) {
		const gchar *default_status;

		/* Send NULL if it's not changed from default status string. We
		 * do this so that the translated default strings will work
		 * across two Gossips.
		 */
		default_status = gossip_presence_state_get_default_status (state);

		if (status && strcmp (status, default_status) == 0) {
			g_object_set (self_presence_get_presence (self_presence),
				      "status", NULL, NULL);
		} else {
			g_object_set (self_presence_get_presence (self_presence),
				      "status", status, NULL);
		}

		g_object_set (self_presence_get_presence (self_presence), 
			      "state", state, NULL);

		gossip_self_presence_stop_flash (self_presence);
		self_presence_clear_away (self_presence);
	} else {
		self_presence_start_flash (self_presence);
		self_presence_set_away (self_presence, status);
		gossip_self_presence_updated (self_presence);
	}
}


