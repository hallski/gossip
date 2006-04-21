/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2006 Imendio AB
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
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n"); */

/* Time to wait before we use sounds for an account after it has gone
 * online/offline, so we don't spam the sound with online's, etc 
 */ 
#define SOUND_WAIT_TIME 10000

static void sound_contact_presence_updated_cb (GossipSession *session,
					       GossipContact *contact,
					       gpointer       user_data);

static GHashTable *account_states = NULL;
static GHashTable *contact_states = NULL;
static gboolean    sound_enabled = TRUE;

static gboolean
sound_protocol_timeout_cb (GossipAccount *account)
{
	g_hash_table_remove (account_states, account);
	return FALSE;
}

static void
sound_protocol_connected_cb (GossipSession  *session,
			     GossipAccount  *account,
			     GossipProtocol *protocol,
			     gpointer        user_data)
{
	guint id;

	if (g_hash_table_lookup (account_states, account)) {
		return;
	}

	DEBUG_MSG (("Sound: Account update, account:'%s' is now online",
		    gossip_account_get_id (account)));

	id = g_timeout_add (SOUND_WAIT_TIME,
			    (GSourceFunc) sound_protocol_timeout_cb, 
			    account);
	g_hash_table_insert (account_states, account, GUINT_TO_POINTER (id));
}

static void
sound_contact_presence_updated_cb (GossipSession *session,
				   GossipContact *contact,
				   gpointer       user_data)
{
	GossipPresence *presence;

	presence = gossip_contact_get_active_presence (contact);
	if (!presence) {
		if (g_hash_table_lookup (contact_states, contact)) {
			DEBUG_MSG (("Sound: Presence update, contact:'%s' is now offline",
				    gossip_contact_get_id (contact)));
			gossip_sound_play (GOSSIP_SOUND_OFFLINE);
		}
			
		g_hash_table_remove (contact_states, contact);
	} else {
		GossipAccount *account;
		
		account = gossip_contact_get_account (contact);

		/* Only show notifications after being online for some
		 * time instead of spamming notifications each time we
		 * connect.
		 */
		if (!g_hash_table_lookup (account_states, account) && 
		    !g_hash_table_lookup (contact_states, contact)) {
			DEBUG_MSG (("Sound: Presence update, contact:'%s' is now online",
				    gossip_contact_get_id (contact)));
			gossip_sound_play (GOSSIP_SOUND_ONLINE);
		}

		g_hash_table_insert (contact_states, 
				     g_object_ref (contact), 
				     g_object_ref (presence));
	}
}

void
gossip_sound_play (GossipSound sound)
{
	GossipSession       *session;
        GossipPresence      *p;
        GossipPresenceState  state;
	gboolean             enabled, silent_busy, silent_away;

	session = gossip_app_get_session ();

	/* This is the internal sound enable/disable for when events
	 * happen that we need to mute sound - e.g. changing roster
	 * contacts.
	 */
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

void 
gossip_sound_toggle (gboolean enabled)
{
	sound_enabled = enabled;
}

void 
gossip_sound_init (GossipSession *session)
{
	static gboolean inited = FALSE;

	g_return_if_fail (GOSSIP_IS_SESSION (session));

	if (inited) {
		return;
	}

	DEBUG_MSG (("Sound: Initiating..."));
	
	account_states = g_hash_table_new_full (gossip_account_hash,
						gossip_account_equal,
						(GDestroyNotify) g_object_unref,
						(GDestroyNotify) g_source_remove);

	contact_states = g_hash_table_new_full (gossip_contact_hash,
						gossip_contact_equal,
						(GDestroyNotify) g_object_unref,
						(GDestroyNotify) g_object_unref);

	g_signal_connect (session, "protocol-connected",
			  G_CALLBACK (sound_protocol_connected_cb),
			  NULL);
	g_signal_connect (session, "contact-presence-updated",
			  G_CALLBACK (sound_contact_presence_updated_cb),
			  NULL);

 	inited = TRUE;
}
