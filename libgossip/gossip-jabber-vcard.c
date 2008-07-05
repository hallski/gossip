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
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <loudmouth/loudmouth.h>

#include <libgossip/gossip-avatar.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-ft.h>
#include <libgossip/gossip-ft-provider.h>
#include <libgossip/gossip-utils.h>

#include "gossip-jabber-vcard.h"
#include "gossip-jabber-private.h"
#include "gossip-jid.h"

#define DEBUG_DOMAIN "JabberVCard"

static LmHandlerResult
jabber_vcard_get_cb (LmMessageHandler   *handler,
		     LmConnection       *connection,
		     LmMessage          *m,
		     GossipCallbackData *data)
{
	GossipVCard         *vcard;
	GossipVCardCallback  callback;
	LmMessageNode       *vcard_node, *photo_node, *node;
	LmMessageSubType     type;

	gossip_debug (DEBUG_DOMAIN, "Received!");

	if (!data || !data->callback) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	callback = data->callback;

	/* check for error */
	type = lm_message_get_sub_type (m);

	if (type == LM_MESSAGE_SUB_TYPE_ERROR) {
		GossipResult result = GOSSIP_RESULT_ERROR_INVALID_REPLY;

		node = lm_message_node_get_child (m->node, "error");
		if (node) {
			const gchar *str;
			gint         code;

			str = lm_message_node_get_attribute (node, "code");
			code = str ? atoi (str) : 0;

			switch (code) {
			case 404:
				/* Receipient unavailable */
				gossip_debug (DEBUG_DOMAIN, "Receipient is unavailable");
				result = GOSSIP_RESULT_ERROR_UNAVAILABLE;
				break;

			case 503:
				/* Service unavailable */
				gossip_debug (DEBUG_DOMAIN, "Service is unavailable");
				result = GOSSIP_RESULT_ERROR_UNAVAILABLE;
				break;

			default:
				gossip_debug (DEBUG_DOMAIN, "Unhandled presence error:%d", code);
				result = GOSSIP_RESULT_ERROR_INVALID_REPLY;
				break;
			}
		}

		(callback) (result, NULL, data->user_data);

		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	/* No vcard node */
	vcard_node = lm_message_node_get_child (m->node, "vCard");
	if (!vcard_node) {
		gossip_debug (DEBUG_DOMAIN, "Invalid VCard, no 'vCard' node");

		(callback) (GOSSIP_RESULT_ERROR_INVALID_REPLY,
			    NULL,
			    data->user_data);

		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	vcard = gossip_vcard_new ();

	node = lm_message_node_get_child (vcard_node, "FN");
	if (node) {
		gossip_vcard_set_name (vcard, node->value);
		gossip_debug (DEBUG_DOMAIN, "- Name:'%s'", node->value);
	}

	node = lm_message_node_get_child (vcard_node, "NICKNAME");
	if (node) {
		gossip_vcard_set_nickname (vcard, node->value);
		gossip_debug (DEBUG_DOMAIN, "- Nickname:'%s'", node->value);
	}

	node = lm_message_node_get_child (vcard_node, "BDAY");
	if (node) {
		gossip_vcard_set_birthday (vcard, node->value);
		gossip_debug (DEBUG_DOMAIN, "- Birthday:'%s'", node->value);
	}

	node = lm_message_node_get_child (vcard_node, "EMAIL");
	if (node) {
		const gchar *email = NULL;

		if (node->value) {
			/* CRACK ALERT:
			 * Included for legacy crappy vcards which
			 * don't work and don't abide by the standards.
			 */
			email = node->value;
			gossip_debug (DEBUG_DOMAIN, "- Email:'%s' (legacy node)", node->value);
		}

		/* Correct method: */
		node = lm_message_node_get_child (node, "USERID");
		if (node && node->value) {
			email = node->value;
			gossip_debug (DEBUG_DOMAIN, "- Email:'%s'", node->value);
		}

		/* Some checking */
		if (email && strchr (email, '@')) {
			gossip_vcard_set_email (vcard, email);
		}
	}

	node = lm_message_node_get_child (vcard_node, "URL");
	if (node) {
		gossip_vcard_set_url (vcard, node->value);
		gossip_debug (DEBUG_DOMAIN, "- URL:'%s'", node->value);
	}

	node = lm_message_node_get_child (vcard_node, "DESC");
	if (node) {
		gossip_vcard_set_description (vcard, node->value);
		gossip_debug (DEBUG_DOMAIN, "- Description:'%s'", node->value);
	}

	photo_node = lm_message_node_get_child (vcard_node, "PHOTO");
	if (photo_node) {
		GossipAvatar *avatar;
		guchar       *decoded_avatar = NULL;
		gsize         len = 0;
		const gchar  *type = NULL;
		gboolean      default_type = FALSE; 
		
		node = lm_message_node_get_child (photo_node, "BINVAL");
		if (node && node->value) {
			decoded_avatar = g_base64_decode (node->value, &len);
		}

		node = lm_message_node_get_child (photo_node, "TYPE");
		if (node && node->value) {
			type = node->value;
		}

		if (!type) {
			default_type = TRUE;
			type = "image/png";
		}
			
		if (decoded_avatar) {
			avatar = gossip_avatar_new (decoded_avatar, len, type);
			gossip_vcard_set_avatar (vcard, avatar);
		
			g_free (decoded_avatar);
			gossip_avatar_unref (avatar);
		}


		gossip_debug (DEBUG_DOMAIN, 
			      "- Avatar: %" G_GSIZE_FORMAT " bytes, type:'%s' (default:'%s')",
			      len,
			      type,
			      default_type ? "yes" : "no");
	}

	(callback) (GOSSIP_RESULT_OK, vcard, data->user_data);

	g_object_unref (vcard);

	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static void
jabber_vcard_free (GossipCallbackData *data)
{
	g_slice_free (GossipCallbackData, data);
}

gboolean
gossip_jabber_vcard_get (GossipJabber         *jabber,
			 const gchar          *jid_str,
			 GossipVCardCallback   callback,
			 gpointer              user_data,
			 GError              **error)
{
	LmConnection       *connection;
	LmMessage          *m;
	LmMessageNode      *node;
	LmMessageHandler   *handler;
	GossipCallbackData *data;

	connection = _gossip_jabber_get_connection (jabber);

	gossip_debug (DEBUG_DOMAIN, "Requesting VCard, JID:'%s'", jid_str);

	m = lm_message_new (jid_str,
			    LM_MESSAGE_TYPE_IQ);


	node = lm_message_node_add_child (m->node, "vCard", NULL);
	lm_message_node_set_attribute (node, "xmlns", "vcard-temp");

	data = g_slice_new0 (GossipCallbackData);
	data->callback = callback;
	data->user_data = user_data;

	handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_vcard_get_cb,
					  data,
					  (GDestroyNotify) jabber_vcard_free);

	if (!lm_connection_send_with_reply (connection, m, handler, error)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "Failed to get VCard for JID:'%s' (could not send request)", 
			      jid_str);

		lm_message_unref (m);
		lm_message_handler_unref (handler);
		return FALSE;
	}

	/* FIXME: Set a timeout */

	lm_message_unref (m);
	lm_message_handler_unref (handler);

	return TRUE;
}

static LmHandlerResult
jabber_vcard_set_cb (LmMessageHandler   *handler,
		     LmConnection       *connection,
		     LmMessage          *m,
		     GossipCallbackData *data)
{
	GossipCallback callback;

	gossip_debug (DEBUG_DOMAIN, "Sent!");

	if (!data || !data->callback) {
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

	callback = data->callback;
	(callback) (GOSSIP_RESULT_OK, data->user_data);

	gossip_debug (DEBUG_DOMAIN, "Notifying contacts with presence update...");

	/* Send our current presence to indicate the avatar has changed */
	gossip_jabber_send_presence (GOSSIP_JABBER (data->data1), NULL);

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

gboolean
gossip_jabber_vcard_set (GossipJabber    *jabber,
			 GossipVCard     *vcard,
			 GossipCallback   callback,
			 gpointer         user_data,
			 GError         **error)
{
	LmConnection       *connection;
	LmMessage          *m;
	LmMessageNode      *node;
	LmMessageNode      *child;
	LmMessageHandler   *handler;
	GossipCallbackData *data;
	GossipAvatar       *avatar;
	GossipContact      *own_contact;
	const gchar        *id;
	const gchar        *str;
	gboolean            result;

	gossip_debug (DEBUG_DOMAIN, "Setting...");

	connection = _gossip_jabber_get_connection (jabber);

	m = lm_message_new_with_sub_type (NULL,
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_SET);

	own_contact = gossip_jabber_get_own_contact (jabber);
	id = gossip_contact_get_id (own_contact);
	lm_message_node_set_attribute (m->node, "from", id);

	node = lm_message_node_add_child (m->node, "vCard", NULL);
	lm_message_node_set_attribute (node, "xmlns", "vcard-temp");

	str = gossip_vcard_get_name (vcard);
	if (!G_STR_EMPTY (str)) {
		lm_message_node_add_child (node, "FN", str);
	}

	str = gossip_vcard_get_nickname (vcard);
	if (!G_STR_EMPTY (str)) {
		lm_message_node_add_child (node, "NICKNAME", str);
	}

	str = gossip_vcard_get_birthday (vcard);
	if (!G_STR_EMPTY (str)) {
		lm_message_node_add_child (node, "BDAY", str);
	}

	str = gossip_vcard_get_url (vcard);
	if (!G_STR_EMPTY (str)) {
		lm_message_node_add_child (node, "URL", str);
	}
	
	str = gossip_vcard_get_email (vcard);
	if (!G_STR_EMPTY (str)) {
		child = lm_message_node_add_child (node, "EMAIL", NULL);
		lm_message_node_add_child (child, "USERID", str);
	}

	str = gossip_vcard_get_description (vcard);
	if (!G_STR_EMPTY (str)) {
		lm_message_node_add_child (node, "DESC", str);
	}

	avatar = gossip_vcard_get_avatar (vcard);
	if (avatar != NULL) {
		gchar *avatar_encoded;

		node = lm_message_node_add_child (node, "PHOTO", NULL);

		if (avatar->format) {
			lm_message_node_add_child (node, "TYPE", avatar->format);
		}

		avatar_encoded = g_base64_encode (avatar->data, avatar->len);
		lm_message_node_add_child (node, "BINVAL", avatar_encoded);
		g_free (avatar_encoded);
	}

	data = g_new0 (GossipCallbackData, 1);

	data->callback = callback;
	data->user_data = user_data;
	data->data1 = jabber;

	handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_vcard_set_cb,
					  data,
					  g_free);

	result = lm_connection_send_with_reply (connection, m, handler, error);

	lm_message_unref (m);
	lm_message_handler_unref (handler);

	return result;
}
