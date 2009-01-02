/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
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
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#ifdef HAVE_LIBCANBERRA_GTK
#include <canberra-gtk.h>
#include <glib/gi18n.h>
#elif  HAVE_PLATFORM_WIN32
#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif /* HAVE_WINDOWS_H */
#ifdef HAVE_MMSYSTEM_H
#include <mmsystem.h>
#endif /* HAVE_MMSYSTEM_H */
#endif /* HAVE_LIBCANBERRA_GTK */

#include <libgossip/gossip.h>

#include "gossip-preferences.h"
#include "gossip-app.h"
#include "gossip-sound.h"

#define DEBUG_DOMAIN "Sound"

/* Time to wait before we use sounds for an account after it has gone
 * online/offline, so we don't spam the sound with online's, etc
 */
#define SOUND_WAIT_TIME 10000

static void sound_contact_presence_updated_cb (GossipContact *contact,
					       GParamSpec    *param,
					       gpointer       user_data);
static void sound_contact_added_cb            (GossipSession *session,
					       GossipContact *contact,
					       gpointer       user_data);
static void sound_contact_removed_cb          (GossipSession *session,
					       GossipContact *contact,
					       gpointer       user_data);

static GHashTable    *account_states = NULL;
static GHashTable    *contact_states = NULL;
static GossipSession *saved_session = NULL;

static gboolean
sound_protocol_timeout_cb (GossipAccount *account)
{
	g_hash_table_remove (account_states, account);
	return FALSE;
}

static void
sound_protocol_connected_cb (GossipSession  *session,
			     GossipAccount  *account,
			     GossipJabber   *jabber,
			     gpointer        user_data)
{
	guint        id;
	const gchar *account_name;

	gossip_debug (DEBUG_DOMAIN, 
		      "Protocol connected for account:'%s'",
		      gossip_account_get_name (account));

	if (g_hash_table_lookup (account_states, account)) {
		return;
	}

	account_name = gossip_account_get_name (account);
	gossip_debug (DEBUG_DOMAIN, 
		      "Account update, account:'%s' is now online",
		      account_name);

	id = g_timeout_add (SOUND_WAIT_TIME,
			    (GSourceFunc) sound_protocol_timeout_cb,
			    account);
	g_hash_table_insert (account_states, g_object_ref (account),
			     GUINT_TO_POINTER (id));
}

static gboolean
sound_disconnected_contact_foreach (GossipContact  *contact,
				    GossipPresence *presence,
				    GossipAccount  *account)
{
	GossipAccount *contact_account;

	contact_account = gossip_contact_get_account (contact);

	if (gossip_account_equal (contact_account, account)) {
		return TRUE;
	}

	return FALSE;
}

static void
sound_protocol_disconnecting_cb (GossipSession *session,
				 GossipAccount *account,
				 GossipJabber  *jabber,
				 gpointer       user_data)
{
	gossip_debug (DEBUG_DOMAIN, 
		      "Protocol disconnecting for account:'%s'",
		      gossip_account_get_name (account));

	g_hash_table_remove (account_states, account);

	g_hash_table_foreach_remove (contact_states,
				     (GHRFunc) sound_disconnected_contact_foreach,
				     account);
}

static void
sound_contact_presence_updated_cb (GossipContact *contact,
				   GParamSpec    *param,
				   gpointer       user_data)
{
	GossipPresence *presence;

	if (gossip_contact_get_type (contact) != GOSSIP_CONTACT_TYPE_CONTACTLIST) {
		return;
	}

	presence = gossip_contact_get_active_presence (contact);
	if (!presence) {
		if (g_hash_table_lookup (contact_states, contact)) {
			gossip_debug (DEBUG_DOMAIN,
				      "Presence update, contact:'%s' is now offline",
				      gossip_contact_get_id (contact));
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
			gossip_debug (DEBUG_DOMAIN, 
				      "Presence update, contact:'%s' is now online",
				      gossip_contact_get_id (contact));
			gossip_sound_play (GOSSIP_SOUND_ONLINE);
		}

		g_hash_table_insert (contact_states,
				     g_object_ref (contact),
				     g_object_ref (presence));
	}
}

static void
sound_contact_added_cb (GossipSession *session,
			GossipContact *contact,
			gpointer       user_data)
{
	g_signal_connect (contact, "notify::presences",
			  G_CALLBACK (sound_contact_presence_updated_cb),
			  NULL);
	g_signal_connect (contact, "notify::type",
			  G_CALLBACK (sound_contact_presence_updated_cb),
			  NULL);
}

static void
sound_contact_removed_cb (GossipSession *session,
			  GossipContact *contact,
			  gpointer       user_data)
{
	g_signal_handlers_disconnect_by_func (contact,
					      sound_contact_presence_updated_cb,
					      NULL);

	g_hash_table_remove (contact_states, contact);
}

#ifdef HAVE_LIBCANBERRA_GTK

#if 0

static void 
callback (ca_context *c, 
	  uint32_t    id, 
	  int         error, 
	  void       *userdata)
{
        gossip_debug (DEBUG_DOMAIN,
		      "Callback called, error '%s'\n", 
		      ca_strerror (error));
}

#endif

#endif /* HAVE_LIBCANBERRA_GTK */

gboolean
gossip_sound_play (GossipSound sound)
{
	GossipSession       *session;
	GossipPresence      *p;
	GossipPresenceState  state;
	gboolean             enabled;
	gboolean             sounds_when_busy;
	gboolean             sounds_when_away;
	gboolean             success;
#ifdef HAVE_PLATFORM_WIN32
	gchar               *filename = NULL;
#endif /* HAVE_PLATFORM_WIN32 */

	session = gossip_app_get_session ();

	gossip_conf_get_bool (gossip_conf_get (),
			      GOSSIP_PREFS_SOUNDS_FOR_MESSAGES,
			      &enabled);
	if (!enabled) {
		gossip_debug (DEBUG_DOMAIN, "Preferences have sound disabled.");
		return FALSE;
	}

	gossip_conf_get_bool (gossip_conf_get (),
			      GOSSIP_PREFS_SOUNDS_WHEN_BUSY,
			      &sounds_when_busy);
	gossip_conf_get_bool (gossip_conf_get (),
			      GOSSIP_PREFS_SOUNDS_WHEN_AWAY,
			      &sounds_when_away);
	
	p = gossip_session_get_presence (gossip_app_get_session ());
	state = gossip_presence_get_state (p);

	if (!sounds_when_busy && state == GOSSIP_PRESENCE_STATE_BUSY) {
		return FALSE;
	}

	if (!sounds_when_away && (state == GOSSIP_PRESENCE_STATE_AWAY ||
				  state == GOSSIP_PRESENCE_STATE_EXT_AWAY)) {
		return FALSE;
	}

#ifdef HAVE_LIBCANBERRA_GTK
	{
	int result = 0;
	
	switch (sound) {
	case GOSSIP_SOUND_CHAT:
		gossip_debug (DEBUG_DOMAIN, "Triggering 'Chat' sound...");
		result = ca_context_play (ca_gtk_context_get (), 0,
					  CA_PROP_APPLICATION_NAME, _("Gossip Instant Messenger"),
					  CA_PROP_EVENT_ID, PACKAGE_TARNAME G_DIR_SEPARATOR_S "chat1", 
					  CA_PROP_EVENT_DESCRIPTION, _("Chat"), 
					  NULL); 
		break;
	case GOSSIP_SOUND_ONLINE:
		gossip_debug (DEBUG_DOMAIN, "Triggering 'Online' sound...");
		result = ca_context_play (ca_gtk_context_get (), 0,
					  CA_PROP_APPLICATION_NAME, _("Gossip Instant Messenger"),
					  CA_PROP_EVENT_ID, PACKAGE_TARNAME G_DIR_SEPARATOR_S "online",
					  CA_PROP_EVENT_DESCRIPTION, _("Online"),
					  NULL);
		break;
	case GOSSIP_SOUND_OFFLINE:
		gossip_debug (DEBUG_DOMAIN, "Triggering 'Offline' sound...");
		result = ca_context_play (ca_gtk_context_get (), 0,
					  CA_PROP_APPLICATION_NAME, _("Gossip Instant Messenger"),
					  CA_PROP_EVENT_ID, PACKAGE_TARNAME G_DIR_SEPARATOR_S "offline",
					  CA_PROP_EVENT_DESCRIPTION, _("Offline"),
					  NULL);
		break;
	}

	gossip_debug (DEBUG_DOMAIN,
		      "Trigger result: %d->%s\n", 
		      result,
		      ca_strerror (result));
	
	success = result == 0;
	}
#elif HAVE_PLATFORM_WIN32
	switch (sound) {
	case GOSSIP_SOUND_CHAT:
		gossip_debug (DEBUG_DOMAIN, "Triggering 'Chat' sound.");
		filename = gossip_paths_get_sound_path ("chat1.wav");
		success = PlaySound (filename, NULL, SND_ASYNC);
		break;
	case GOSSIP_SOUND_ONLINE:
		gossip_debug (DEBUG_DOMAIN, "Triggering 'Online' sound.");
		filename = gossip_paths_get_sound_path ("online.wav");
		success = PlaySound (filename, NULL, SND_ASYNC);
		break;
	case GOSSIP_SOUND_OFFLINE:
		gossip_debug (DEBUG_DOMAIN, "Triggering 'Offline' sound.");
		filename = gossip_paths_get_sound_path ("offline.wav");
		success = PlaySound (filename, NULL, SND_ASYNC);
		break;
	}
	
	g_free (filename);
#else
	success = TRUE;
#endif

	return success;
}

void
gossip_sound_init (GossipSession *session)
{
	g_return_if_fail (GOSSIP_IS_SESSION (session));

	g_assert (saved_session == NULL);

	saved_session = g_object_ref (session);

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
	g_signal_connect (session, "protocol-disconnecting",
			  G_CALLBACK (sound_protocol_disconnecting_cb),
			  NULL);
	g_signal_connect (session, "contact-added",
			  G_CALLBACK (sound_contact_added_cb),
			  NULL);
	g_signal_connect (session, "contact-removed",
			  G_CALLBACK (sound_contact_removed_cb),
			  NULL);
}

void
gossip_sound_finalize (void)
{
	g_assert (saved_session != NULL);

	g_signal_handlers_disconnect_by_func (saved_session,
					      sound_protocol_connected_cb,
					      NULL);
	g_signal_handlers_disconnect_by_func (saved_session,
					      sound_contact_presence_updated_cb,
					      NULL);

	g_hash_table_destroy (account_states);
	g_hash_table_destroy (contact_states);

	g_object_unref (saved_session);
}
