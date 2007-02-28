/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Eitan Isaacson <eitan@ascender.com>
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

#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-chan.h>
#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-chan-type-text-gen.h>
#include <libtelepathy/tp-chan-iface-chat-state-gen.h>

#include <libgossip/gossip-protocol.h>
#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-account.h>
#include <libgossip/gossip-message.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-time.h>

#include "gossip-telepathy-message.h"
#include "gossip-telepathy-contacts.h"
#include "gossip-telepathy-private.h"

#define DEBUG_DOMAIN "TelepathyMessage"

/* Close the text channel when inactive for X seconds */
#define TELEPATHY_MESSAGE_TIMEOUT 60

typedef struct {
	GossipTelepathy *telepathy;
	TpChan          *text_chan;
	DBusGProxy      *text_iface;
	DBusGProxy	*chat_state_iface;
	guint            timeout_id;
	guint            id;
} TelepathyMessageChan;

struct _GossipTelepathyMessage {
	GossipTelepathy  *telepathy;
	GHashTable       *text_channels;
};

static void     telepathy_message_disconnected_cb (GossipProtocol         *telepathy,
						   GossipAccount          *account,
						   gint                    reason,
						   GossipTelepathyMessage *message);
static void     telepathy_message_received_cb     (DBusGProxy             *text_iface,
						   guint                   message_id,
						   guint                   timestamp,
						   guint                   from_handle,
						   guint                   message_type,
						   guint                   message_flags,
						   gchar                  *message_body,
						   TelepathyMessageChan   *msg_chan);
static void     telepathy_message_sent_cb         (DBusGProxy             *text_iface,
						   guint                   timestamp,
						   guint                   message_type,
						   gchar                  *message_body,
						   TelepathyMessageChan   *msg_chan);
static void     telepathy_message_state_cb        (DBusGProxy             *chat_state_iface,
						   guint                   contact_handle,
						   guint                   state,
						   TelepathyMessageChan   *msg_chan);
static void     telepathy_message_ack_pending     (TelepathyMessageChan   *msg_chan);
static void     telepathy_message_emit            (TelepathyMessageChan   *msg_chan,
						   guint                   timestamp,
						   guint                   from_handle,
						   const gchar            *message_body);
static gboolean telepathy_message_timeout         (TelepathyMessageChan   *msg_chan);
static void     telepathy_message_timeout_reset   (TelepathyMessageChan   *msg_chan);
static void     telepathy_message_free            (TelepathyMessageChan   *msg_chan);
static void     telepathy_message_closed_cb       (TpChan                 *text_chan,
						   GossipTelepathyMessage *message);
static gboolean telepathy_message_find_chan       (guint                   key,
						   TelepathyMessageChan   *msg_chan,
						   TpChan                 *text_chan);

GossipTelepathyMessage *
gossip_telepathy_message_init (GossipTelepathy *telepathy)
{
	GossipTelepathyMessage *message;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY (telepathy), NULL);

	message = g_new0 (GossipTelepathyMessage, 1);
	message->telepathy = telepathy;
	message->text_channels = g_hash_table_new_full (g_direct_hash,
							g_direct_equal,
							NULL,
							(GDestroyNotify)
							telepathy_message_free);
	g_signal_connect (telepathy, "disconnected",
			  G_CALLBACK (telepathy_message_disconnected_cb),
			  message);
	return message;
}

static void
telepathy_message_disconnected_cb (GossipProtocol         *telepathy,
				   GossipAccount          *account,
				   gint                    reason,
				   GossipTelepathyMessage *message)
{
	g_hash_table_remove_all (message->text_channels);
}

void
gossip_telepathy_message_finalize (GossipTelepathyMessage *message)
{
	g_return_if_fail (message != NULL);

	g_hash_table_destroy (message->text_channels);
	g_free (message);
}

void
gossip_telepathy_message_newchannel (GossipTelepathyMessage *message,
				     TpChan                 *new_chan,
				     guint                   id)
{
	TelepathyMessageChan *msg_chan;
	DBusGProxy           *text_iface;
	DBusGProxy	     *chat_state_iface;

	g_return_if_fail (message != NULL);
	g_return_if_fail (TELEPATHY_IS_CHAN (new_chan));

	if (g_hash_table_lookup (message->text_channels,
				 GUINT_TO_POINTER (id))) {
		return;
	}

	text_iface = tp_chan_get_interface (new_chan,
					    TELEPATHY_CHAN_IFACE_TEXT_QUARK);
	chat_state_iface = tp_chan_get_interface (new_chan,
						  TELEPATHY_CHAN_IFACE_CHAT_STATE_QUARK);

	msg_chan = g_slice_new0 (TelepathyMessageChan);
	msg_chan->text_chan = g_object_ref (new_chan);
	msg_chan->text_iface = text_iface;
	msg_chan->chat_state_iface = chat_state_iface;
	msg_chan->telepathy = message->telepathy;
	msg_chan->id = id;

	g_hash_table_insert (message->text_channels,
			     GUINT_TO_POINTER (id),
			     msg_chan);

	telepathy_message_ack_pending (msg_chan);

	dbus_g_proxy_connect_signal (DBUS_G_PROXY (new_chan), "Closed",
				     G_CALLBACK (telepathy_message_closed_cb),
				     message, NULL);
	dbus_g_proxy_connect_signal (text_iface, "Received",
				     G_CALLBACK (telepathy_message_received_cb),
				     msg_chan, NULL);
	dbus_g_proxy_connect_signal (text_iface, "Sent",
				     G_CALLBACK (telepathy_message_sent_cb),
				     msg_chan, NULL);

	if (chat_state_iface != NULL) {
		dbus_g_proxy_connect_signal (chat_state_iface, "ChatStateChanged",
					     G_CALLBACK (telepathy_message_state_cb),
					     msg_chan, NULL);
	}

	telepathy_message_timeout_reset (msg_chan);
}

void
gossip_telepathy_message_send (GossipTelepathyMessage *message,
			       guint                   id,
			       const gchar            *message_body)
{
	TelepathyMessageChan *msg_chan;
	GError               *error = NULL;

	g_return_if_fail (message != NULL);

	msg_chan = g_hash_table_lookup (message->text_channels,
					GUINT_TO_POINTER (id));

	if (!msg_chan) {
		TpConn      *tp_conn;
		TpChan      *text_chan;
		const gchar *bus_name;

		gossip_debug (DEBUG_DOMAIN,
			      "Text channel to %d does not exist, creating",
			      id);

		tp_conn = gossip_telepathy_get_connection (message->telepathy);
		bus_name = dbus_g_proxy_get_bus_name (DBUS_G_PROXY (tp_conn));

		text_chan = tp_conn_new_channel (tp_get_bus (),
						 tp_conn,
						 bus_name,
						 TP_IFACE_CHANNEL_TYPE_TEXT,
						 TP_HANDLE_TYPE_CONTACT,
						 id, TRUE);

		gossip_telepathy_message_newchannel (message, text_chan, id);
		g_object_unref (text_chan);

		msg_chan = g_hash_table_lookup (message->text_channels,
						GUINT_TO_POINTER (id));
		g_return_if_fail (msg_chan != NULL);
	}

	gossip_debug (DEBUG_DOMAIN, "Real sending message: %s", message_body);
	if (!tp_chan_type_text_send (msg_chan->text_iface,
				     TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
				     message_body,
				     &error)) {
		gossip_debug (DEBUG_DOMAIN, "Send Error: %s", error->message);
		g_clear_error (&error);
	}

	telepathy_message_timeout_reset (msg_chan);
}

void
gossip_telepathy_message_send_state (GossipTelepathyMessage *message,
				     guint                   id,
				     guint                   state)
{
	TelepathyMessageChan *msg_chan;
	GError               *error = NULL;

	g_return_if_fail (message != NULL);

	msg_chan = g_hash_table_lookup (message->text_channels,
					GUINT_TO_POINTER (id));

	if (msg_chan && msg_chan->chat_state_iface) {
		gossip_debug (DEBUG_DOMAIN, "Set state: %d", state);
		if (!tp_chan_iface_chat_state_set_chat_state (msg_chan->chat_state_iface,
							      state,
							      &error)) {
			gossip_debug (DEBUG_DOMAIN, "Set Chat State Error: %s",
				      error->message);
			g_clear_error (&error);
		}
	}
}

static void
telepathy_message_received_cb (DBusGProxy           *text_iface,
			       guint                 message_id,
			       guint                 timestamp,
			       guint                 from_handle,
			       guint                 message_type,
			       guint                 message_flags,
			       gchar                *message_body,
			       TelepathyMessageChan *msg_chan)
{
	GArray *message_ids;

	gossip_debug (DEBUG_DOMAIN, "Message received: %s", message_body);

	telepathy_message_emit (msg_chan,
				timestamp,
				from_handle,
				message_body);

	message_ids = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (message_ids, message_id);
	tp_chan_type_text_acknowledge_pending_messages (msg_chan->text_iface,
							message_ids, NULL);
	g_array_free (message_ids, TRUE);

	telepathy_message_timeout_reset (msg_chan);
}

static void
telepathy_message_sent_cb (DBusGProxy           *text_iface,
			   guint                 timestamp,
			   guint                 message_type,
			   gchar                *message_body,
			   TelepathyMessageChan *msg_chan)
{
	gossip_debug (DEBUG_DOMAIN, "Message sent: %s", message_body);

	if (msg_chan->text_chan->handle_type == TP_HANDLE_TYPE_ROOM) {
		telepathy_message_emit (msg_chan, timestamp, 0, message_body);
	}
	telepathy_message_timeout_reset (msg_chan);
}

static void
telepathy_message_ack_pending (TelepathyMessageChan *msg_chan)
{
	GPtrArray *messages_list;
	guint      i;
	GError    *error = NULL;

	/* If we do this call async, don't forget to ignore Received signal
	 * until we get the answer */
	if (!tp_chan_type_text_list_pending_messages (msg_chan->text_iface,
						      TRUE,
						      &messages_list,
						      &error)) {
		gossip_debug (DEBUG_DOMAIN, "Error retrieving pending messages: %s",
			      error->message);
		g_clear_error (&error);
		return;
	}

	for (i = 0; i < messages_list->len; i++) {
		GValueArray *message_struct;
		const gchar *message_body;
		guint        message_id;
		guint        timestamp;
		guint        from_handle;
		guint        message_type;
		guint        message_flags;

		message_struct = (GValueArray *) g_ptr_array_index (messages_list, i);

		message_id = g_value_get_uint (g_value_array_get_nth (message_struct, 0));
		timestamp = g_value_get_uint (g_value_array_get_nth (message_struct, 1));
		from_handle = g_value_get_uint (g_value_array_get_nth (message_struct, 2));
		message_type = g_value_get_uint (g_value_array_get_nth (message_struct, 3));
		message_flags = g_value_get_uint (g_value_array_get_nth (message_struct, 4));
		message_body = g_value_get_string (g_value_array_get_nth (message_struct, 5));

		telepathy_message_emit (msg_chan,
					timestamp,
					from_handle,
					message_body);
	}

	g_ptr_array_free (messages_list, TRUE);
}

static void
telepathy_message_emit (TelepathyMessageChan *msg_chan,
			guint                 timestamp,
			guint                 from_handle,
			const gchar          *message_body)
{
	GossipMessage *message;
	GossipContact *from;
	GossipContact *recipient;

	if (from_handle == 0) {
		from = gossip_telepathy_get_own_contact (msg_chan->telepathy);
	} else {
		GossipTelepathyContacts *contacts;

		contacts = gossip_telepathy_get_contacts (msg_chan->telepathy);
		from = gossip_telepathy_contacts_get_from_handle (contacts,
								  from_handle);
	}

	recipient = gossip_telepathy_get_own_contact (msg_chan->telepathy);

	if (msg_chan->text_chan->handle_type != TP_HANDLE_TYPE_ROOM) {
		message = gossip_message_new (GOSSIP_MESSAGE_TYPE_NORMAL, recipient);
	} else {
		message = gossip_message_new (GOSSIP_MESSAGE_TYPE_CHAT_ROOM, recipient);
	}

	gossip_message_set_sender (message, from);
	gossip_message_set_body (message, message_body);
	gossip_message_set_timestamp (message, (GossipTime) timestamp);

	if (msg_chan->text_chan->handle_type != TP_HANDLE_TYPE_ROOM) {
		g_signal_emit_by_name (msg_chan->telepathy, "new-message",
				       message);
	} else {
		g_signal_emit_by_name (msg_chan->telepathy,
				       "chatroom-new-message",
				       msg_chan->id,
				       message);
	}

	g_object_unref (message);
}

static gboolean
telepathy_message_timeout (TelepathyMessageChan *msg_chan)
{
	GError *error = NULL;

	gossip_debug (DEBUG_DOMAIN, "Timeout, closing channel...");

	msg_chan->timeout_id = 0;

	if (!tp_chan_close (DBUS_G_PROXY (msg_chan->text_chan), &error)) {
		gossip_debug (DEBUG_DOMAIN, "Error closing text channel: %s",
			      error->message);
		g_clear_error (&error);
	}

	return FALSE;
}

static void
telepathy_message_timeout_reset (TelepathyMessageChan *msg_chan)
{
	if (msg_chan->text_chan->handle_type == TP_HANDLE_TYPE_ROOM) {
		return;
	}

	/* It is recommended to keep a text channel open only long enough
	 * for a conversation. Since Gossip does not have this concept,
	 * I added a timout
	 */
	if (msg_chan->timeout_id) {
		g_source_remove (msg_chan->timeout_id);
	}
	msg_chan->timeout_id =
		g_timeout_add (TELEPATHY_MESSAGE_TIMEOUT*1000,
			       (GSourceFunc) telepathy_message_timeout,
			       msg_chan);
}

static void
telepathy_message_free (TelepathyMessageChan *msg_chan)
{
	g_object_unref (msg_chan->text_chan);
	if (msg_chan->timeout_id) {
		g_source_remove (msg_chan->timeout_id);
	}

	g_slice_free (TelepathyMessageChan, msg_chan);
}

static void
telepathy_message_closed_cb (TpChan                 *text_chan,
			     GossipTelepathyMessage *message)
{
	TelepathyMessageChan *msg_chan;

	msg_chan = g_hash_table_find (message->text_channels,
				      (GHRFunc) telepathy_message_find_chan,
				      text_chan);

	g_return_if_fail (msg_chan != NULL);
	gossip_debug (DEBUG_DOMAIN, "Channel closed: %d", text_chan->handle);

	g_hash_table_remove (message->text_channels,
			     GUINT_TO_POINTER (msg_chan->id));
}

static gboolean
telepathy_message_find_chan (guint                 key,
			     TelepathyMessageChan *msg_chan,
			     TpChan               *text_chan)
{
	if (msg_chan->text_chan->handle == text_chan->handle) {
		return TRUE;
	}

	return FALSE;
}

static void
telepathy_message_state_cb (DBusGProxy *chat_state_iface,
  			    guint contact_handle,
			    guint state,
			    TelepathyMessageChan *msg_chan)
{
	GossipContact 		*from;
	gboolean 		 composing;
	GossipTelepathyContacts *contacts;

	composing = (state == TP_CHANNEL_CHAT_STATE_COMPOSING);
	contacts = gossip_telepathy_get_contacts (msg_chan->telepathy);
	from = gossip_telepathy_contacts_get_from_handle (contacts,
							  contact_handle);

	g_signal_emit_by_name (msg_chan->telepathy, "composing", from, composing);
}

