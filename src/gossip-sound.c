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
#include <gconf/gconf-client.h>
#include <libgnome/gnome-sound.h>
#include "gossip-preferences.h"
#include "gossip-app.h"
#include "gossip-session.h"
#include "gossip-sound.h"

extern GConfClient *gconf_client;

void
gossip_sound_play (GossipSound sound)
{
	gboolean             enabled, silent_busy, silent_away;
	const gchar         *file;
	gchar               *str;
        GossipPresence      *p;
        GossipPresenceState  state;

	if (!gossip_session_is_connected (gossip_app_get_session ())) {
		return;
	}
	
	enabled = gconf_client_get_bool (gconf_client,
					 GCONF_PATH "/sound/play_sounds",
					 NULL);
	silent_busy = gconf_client_get_bool (gconf_client,
					     GCONF_PATH "/sound/silent_busy",
					     NULL);
	silent_away = gconf_client_get_bool (gconf_client,
					     GCONF_PATH "/sound/silent_away",
					     NULL);

	if (!enabled) {
		return;
	}

        p = gossip_session_get_presence (gossip_app_get_session ());
        state = gossip_presence_get_state (p);

        if (silent_busy && state == GOSSIP_PRESENCE_STATE_BUSY) {
		return;
	}

	if (silent_away && (state == GOSSIP_PRESENCE_STATE_AWAY || state == GOSSIP_PRESENCE_STATE_EXT_AWAY)) {
		return;
	}

	switch (sound) {
	case GOSSIP_SOUND_CHAT:
		file = "chat1.wav";
		break;
	case GOSSIP_SOUND_ONLINE:
		/* Disable for now, it's annoying. */
		return;

		file = "online.wav";
		break;
	case GOSSIP_SOUND_OFFLINE:
		/* Disable for now, it's annoying. */
		return;

		file = "offline.wav";
		break;
	default:
		return;
	}

	str = g_build_filename (DATADIR "/gossip", file, NULL);
	gnome_sound_play (str);
	g_free (str);
}		

