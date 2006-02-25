/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio AB
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
#include <libgnome/gnome-sound.h>
#include <libgnome/gnome-triggers.h>

#include <libgossip/gossip-session.h>

#include "gossip-preferences.h"
#include "gossip-app.h"
#include "gossip-sound.h"

#define DEBUG_MSG(x)  
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n");  */


static gboolean sound_enabled = TRUE;


void
gossip_sound_play (GossipSound sound)
{
	GossipSession       *session;
        GossipPresence      *p;
        GossipPresenceState  state;
	gboolean             enabled, silent_busy, silent_away;

	session = gossip_app_get_session ();

	/* this is the internal sound enable/disable for when events
	   happen that we need to mute sound - e.g. changing roster contacts */
	if (!sound_enabled) {
		DEBUG_MSG (("Sound: Play request ignored, sound currently disabled."));
		return;
	}

	enabled = gconf_client_get_bool (gossip_app_get_gconf_client (),
					 GCONF_PATH "/sound/play_sounds",
					 NULL);
	if (!enabled) {
		DEBUG_MSG (("Sound: Preferences have sound disabled."));
		return;
	}

	silent_busy = gconf_client_get_bool (gossip_app_get_gconf_client (),
					     GCONF_PATH "/sound/silent_busy",
					     NULL);
	silent_away = gconf_client_get_bool (gossip_app_get_gconf_client (),
					     GCONF_PATH "/sound/silent_away",
					     NULL);

        p = gossip_session_get_presence (gossip_app_get_session ());
        state = gossip_presence_get_state (p);

        if (silent_busy && state == GOSSIP_PRESENCE_STATE_BUSY) {
		return;
	}

	if (silent_away && (state == GOSSIP_PRESENCE_STATE_AWAY || 
			    state == GOSSIP_PRESENCE_STATE_EXT_AWAY)) {
		return;
	}

	switch (sound) {
	case GOSSIP_SOUND_CHAT:
		DEBUG_MSG (("Sound: Triggering 'Chat' event."));
		gnome_triggers_do (NULL, NULL, "gossip", "Chat", NULL);
		break;
	case GOSSIP_SOUND_ONLINE:
		DEBUG_MSG (("Sound: Triggering 'Online' event."));
		gnome_triggers_do (NULL, NULL, "gossip", "Online", NULL);
		break;
	case GOSSIP_SOUND_OFFLINE:
		DEBUG_MSG (("Sound: Triggering 'Offline' event."));
		gnome_triggers_do (NULL, NULL, "gossip", "Offline", NULL);
		break;
	default:
		DEBUG_MSG (("Sound: Unknown sound type."));
		return;
	}
}		

void gossip_sound_toggle (gboolean enabled)
{
	sound_enabled = enabled;
}
