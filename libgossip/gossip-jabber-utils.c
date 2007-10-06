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
 * 
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 */

#include <string.h>

#include <glib/gi18n.h>

#include "gossip-debug.h"
#include "gossip-presence.h"
#include "gossip-time.h"
#include "gossip-chatroom.h"
#include "gossip-jabber.h"
#include "gossip-jabber-utils.h"
#include "gossip-jid.h"

#define DEBUG_DOMAIN "JabberUtils"

GossipJabberAsyncData *
gossip_jabber_async_data_new (GossipJabber        *jabber,
			      GossipErrorCallback  callback,
			      gpointer             user_data)
{
	GossipJabberAsyncData *ad;

	ad = g_new0 (GossipJabberAsyncData, 1);

	ad->jabber = g_object_ref (jabber);

	ad->callback = callback;
	ad->user_data = user_data;

	return ad;
}

void
gossip_jabber_async_data_free (GossipJabberAsyncData *ad)
{
	if (!ad) {
		return;
	}

	if (ad->jabber) {
		g_object_unref (ad->jabber);
	}

	g_free (ad);
}

const gchar *
gossip_jabber_presence_state_to_str (GossipPresence *presence)
{
	switch (gossip_presence_get_state (presence)) {
	case GOSSIP_PRESENCE_STATE_BUSY:
		return "dnd";
	case GOSSIP_PRESENCE_STATE_AWAY:
		return "away";
	case GOSSIP_PRESENCE_STATE_EXT_AWAY:
		return "xa";
	default:
		return NULL;
	}

	return NULL;
}

GossipPresenceState
gossip_jabber_presence_state_from_str (const gchar *str)
{
	if (!str || !str[0]) {
		return GOSSIP_PRESENCE_STATE_AVAILABLE;
	} else if (strcmp (str, "dnd") == 0) {
		return GOSSIP_PRESENCE_STATE_BUSY;
	} else if (strcmp (str, "away") == 0) {
		return GOSSIP_PRESENCE_STATE_AWAY;
	} else if (strcmp (str, "xa") == 0) {
		return GOSSIP_PRESENCE_STATE_EXT_AWAY;
	} else if (strcmp (str, "chat") == 0) {
		/* We don't support chat, so treat it like available. */
		return GOSSIP_PRESENCE_STATE_AVAILABLE;
	}

	return GOSSIP_PRESENCE_STATE_AVAILABLE;
}

GossipTime
gossip_jabber_get_message_timestamp (LmMessage *m)
{
	LmMessageNode *node;
	const gchar   *xmlns;
	const gchar   *stamp;
	GossipTime     t;

	g_return_val_if_fail (m != NULL, gossip_time_get_current ());
	g_return_val_if_fail (m->node != NULL, gossip_time_get_current ());

	stamp = NULL;
	for (node = m->node->children; node && node->name; node = node->next) {
		if (strcmp (node->name, "x") != 0) {
			continue;
		}

		xmlns = lm_message_node_get_attribute (node, "xmlns");
		if (xmlns && strcmp (xmlns, "jabber:x:delay") == 0) {
			stamp = lm_message_node_get_attribute (node, "stamp");
			break;
		}
	}

	if (!stamp) {
		return gossip_time_get_current ();
	}

	t = gossip_time_parse (stamp);
	if (!t) {
		return gossip_time_get_current ();
	}

	return t;
}

GossipChatroomInvite *
gossip_jabber_get_message_conference (GossipJabber *jabber,
				      LmMessage    *m)
{
	LmMessageNode        *node;
	GossipChatroomInvite *invite;
	GossipContact        *contact;
	const gchar          *contact_id;
	const gchar          *id;
	const gchar          *reason = NULL;

	g_return_val_if_fail (m != NULL, NULL);
	g_return_val_if_fail (m->node != NULL, NULL);
	g_return_val_if_fail (m->node->children != NULL, NULL);

	node = lm_message_node_find_child (m->node, "invite");
	if (!node) {
		return NULL;
	}

	contact_id = lm_message_node_get_attribute (node, "from");
	contact = gossip_jabber_get_contact_from_jid (jabber,
						      contact_id,
						      NULL,
						      TRUE);

	id = lm_message_node_get_attribute (m->node, "from");

	node = lm_message_node_find_child (node, "reason");
	if (node) {
		reason = lm_message_node_get_value (node);
	}

	invite = gossip_chatroom_invite_new (contact, id, reason);
	g_object_unref (contact);

	return invite;
}

gboolean
gossip_jabber_get_message_is_event (LmMessage *m)
{
	LmMessageNode *node;
	const gchar   *xmlns;

	g_return_val_if_fail (m != NULL, FALSE);
	g_return_val_if_fail (m->node != NULL, FALSE);
	g_return_val_if_fail (m->node->children != NULL, FALSE);

	if (lm_message_node_find_child (m->node, "body")) {
		return FALSE;
	}

	for (node = m->node->children; node && node->name; node = node->next) {
		if (strcmp (node->name, "x") != 0) {
			continue;
		}

		xmlns = lm_message_node_get_attribute (node, "xmlns");
		if (xmlns && strcmp (xmlns, "jabber:x:event") == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

gboolean
gossip_jabber_get_message_is_composing (LmMessage *m)
{
	LmMessageNode *node;
	const gchar   *xmlns;

	g_return_val_if_fail (m != NULL, FALSE);
	g_return_val_if_fail (m->node != NULL, FALSE);
	g_return_val_if_fail (m->node->children != NULL, FALSE);

	for (node = m->node->children; node && node->name; node = node->next) {
		if (strcmp (node->name, "x") != 0) {
			continue;
		}

		xmlns = lm_message_node_get_attribute (node, "xmlns");
		if (xmlns && strcmp (xmlns, "jabber:x:event") == 0) {
			LmMessageNode *composing;

			composing = lm_message_node_find_child (node, "composing");
			if (composing) {
				return TRUE;
			}
		}
	}

	return FALSE;
}

gchar *
gossip_jabber_get_name_to_use (const gchar *jid_str,
			       const gchar *nickname,
			       const gchar *full_name)
{
	if (nickname && strlen (nickname) > 0) {
		return g_strdup (nickname);
	}

	if (full_name && strlen (full_name) > 0) {
		return g_strdup (full_name);
	}

	if (jid_str && strlen (jid_str) > 0) {
		GossipJID *jid;
		gchar     *part_name;

		jid = gossip_jid_new (jid_str);
		part_name = gossip_jid_get_part_name (jid);
		gossip_jid_unref (jid);

		return part_name;
	}

	return "";
}

GError *
gossip_jabber_error_create (GossipJabberError  code,
			    const gchar       *reason)
{
	static GQuark  quark = 0;
	GError        *error;

	if (!quark) {
		quark = g_quark_from_static_string ("gossip-jabber");
	}

	error = g_error_new_literal (quark, code, reason);
	return error;
}

void
gossip_jabber_error (GossipJabber      *jabber, 
		     GossipJabberError  code)
{
	GossipAccount *account;
	GError        *error;
	const gchar   *message;

	g_return_if_fail (GOSSIP_IS_JABBER (jabber));

	account = gossip_jabber_get_account (jabber);

	message = gossip_jabber_error_to_string (code);
	gossip_debug (DEBUG_DOMAIN, "Error:%d->'%s'", code, message);

	error = gossip_jabber_error_create (code, message);
	g_signal_emit_by_name (jabber, "error", account, error);
	g_error_free (error);
}

const gchar *
gossip_jabber_error_to_string (GossipJabberError error)
{
	const gchar *str = _("An unknown error occurred.");

	switch (error) {
	case GOSSIP_JABBER_NO_CONNECTION:
		str = _("Connection refused.");
		break;
	case GOSSIP_JABBER_NO_SUCH_HOST:
		str = _("Server address could not be resolved.");
		break;
	case GOSSIP_JABBER_TIMED_OUT:
		str = _("Connection timed out.");
		break;
	case GOSSIP_JABBER_AUTH_FAILED:
		str = _("Authentication failed.");
		break;
	case GOSSIP_JABBER_DUPLICATE_USER:
		str = _("The username you are trying already exists.");
		break;
	case GOSSIP_JABBER_INVALID_USER:
		str = _("The username you are trying is not valid.");
		break;
	case GOSSIP_JABBER_UNAVAILABLE:
		str = _("This feature is unavailable.");
		break;
	case GOSSIP_JABBER_UNAUTHORIZED:
		str = _("This feature is unauthorized.");
		break;
	case GOSSIP_JABBER_SPECIFIC_ERROR:
		str = _("A specific protocol error occurred that was unexpected.");
		break;
	}

	return str;
}
