#/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
 *
 * Authors: Mikael Hallendal <micke@imendio.com>
 */

#include "config.h"

#include <glib/gi18n.h>

#include "gossip-jabber.h"
#include "gossip-protocol.h"

#include "libgossip-marshal.h"

static void gossip_protocol_class_init (GossipProtocolClass *klass);
static void gossip_protocol_init       (GossipProtocol      *protocol);

enum {
	CONNECTING,
	CONNECTED,
	DISCONNECTING,
	DISCONNECTED,
	NEW_MESSAGE,
	CONTACT_ADDED,
	CONTACT_REMOVED,
	COMPOSING,

	/* Used to get password from user. */
	GET_PASSWORD,

	SUBSCRIPTION_REQUEST,

	ERROR,

	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GossipProtocol, gossip_protocol, G_TYPE_OBJECT);

static void
gossip_protocol_class_init (GossipProtocolClass *klass)
{
	klass->setup                = NULL;
	klass->login                = NULL;
	klass->logout               = NULL;
	klass->is_connected         = NULL;
	klass->is_connecting        = NULL;
	klass->get_example_username = NULL;
	klass->get_default_server   = NULL;
	klass->get_default_port     = NULL;
	klass->set_presence         = NULL;
	klass->set_subscription     = NULL;
	klass->set_vcard            = NULL;
	klass->send_message         = NULL;
	klass->send_composing       = NULL;
	klass->find_contact         = NULL;
	klass->add_contact          = NULL;
	klass->rename_contact       = NULL;
	klass->remove_contact       = NULL;
	klass->update_contact       = NULL;
	klass->rename_group         = NULL;
	klass->get_contacts         = NULL;
	klass->get_own_contact      = NULL;
	klass->get_active_resource  = NULL;
	klass->get_groups           = NULL;
	klass->get_vcard            = NULL;
	klass->get_version          = NULL;
	klass->register_account     = NULL;
	klass->new_account          = NULL;

	signals[CONNECTING] =
		g_signal_new ("connecting",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_ACCOUNT);

	signals[CONNECTED] =
		g_signal_new ("connected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_ACCOUNT);

	signals[DISCONNECTING] =
		g_signal_new ("disconnecting",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_ACCOUNT);

	signals[DISCONNECTED] =
		g_signal_new ("disconnected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT_INT,
			      G_TYPE_NONE,
			      2, GOSSIP_TYPE_ACCOUNT, G_TYPE_INT);

	signals[NEW_MESSAGE] =
		g_signal_new ("new-message",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_MESSAGE);

	signals[CONTACT_ADDED] =
		g_signal_new ("contact-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_CONTACT);

	signals[CONTACT_REMOVED] =
		g_signal_new ("contact-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_CONTACT);

	signals[COMPOSING] =
		g_signal_new ("composing",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT_BOOLEAN,
			      G_TYPE_NONE,
			      2, GOSSIP_TYPE_CONTACT, G_TYPE_BOOLEAN);

	signals[GET_PASSWORD] =
		g_signal_new ("get-password",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_STRING__OBJECT,
			      G_TYPE_STRING,
			      1, GOSSIP_TYPE_ACCOUNT);

	signals[SUBSCRIPTION_REQUEST] =
		g_signal_new ("subscription-request",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_CONTACT);

	signals[ERROR] =
		g_signal_new ("error",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__OBJECT_POINTER,
			      G_TYPE_NONE,
			      2, GOSSIP_TYPE_ACCOUNT, G_TYPE_POINTER);

}

static void
gossip_protocol_init (GossipProtocol *protocol)
{
	/* FIXME: Implement */
}

GossipProtocol *
gossip_protocol_new (void)
{
	return g_object_new (GOSSIP_TYPE_JABBER, NULL);
}

GossipAccount *
gossip_protocol_new_account (GossipProtocol *protocol)
{
	GossipProtocolClass *klass;

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), NULL);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->new_account) {
		return klass->new_account (protocol);
	}

	return NULL;
}

GossipContact *
gossip_protocol_new_contact (GossipProtocol *protocol,
			     const gchar    *id,
			     const gchar    *name)
{
	GossipProtocolClass *klass;

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), NULL);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->new_contact) {
		return klass->new_contact (protocol, id, name);
	}

	return NULL;
}

void
gossip_protocol_setup (GossipProtocol *protocol,
		       GossipAccount  *account)
{
	GossipProtocolClass *klass;

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));
	g_return_if_fail (account != NULL);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->setup) {
		klass->setup (protocol, account);
	}
}

void
gossip_protocol_login (GossipProtocol *protocol)
{
	GossipProtocolClass *klass;

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->login) {
		klass->login (protocol);
	}
}

void
gossip_protocol_logout (GossipProtocol *protocol)
{
	GossipProtocolClass *klass;

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->logout) {
		klass->logout (protocol);
	}
}

gboolean
gossip_protocol_is_connected (GossipProtocol *protocol)
{
	GossipProtocolClass *klass;

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), FALSE);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->is_connected) {
		return klass->is_connected (protocol);
	}

	return FALSE;
}

gboolean
gossip_protocol_is_connecting (GossipProtocol *protocol)
{
	GossipProtocolClass *klass;

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), FALSE);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->is_connecting) {
		return klass->is_connecting (protocol);
	}

	return FALSE;
}

gboolean
gossip_protocol_is_ssl_supported (GossipProtocol *protocol)
{
	GossipProtocolClass *klass;

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), FALSE);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->is_ssl_supported) {
		return klass->is_ssl_supported (protocol);
	}

	return FALSE;
}

const gchar *
gossip_protocol_get_example_username (GossipProtocol *protocol)
{
	GossipProtocolClass *klass;

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), NULL);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->get_example_username) {
		return klass->get_example_username (protocol);
	}

	return NULL;
}

gchar *
gossip_protocol_get_default_server (GossipProtocol *protocol,
				    const gchar    *username)
{
	GossipProtocolClass *klass;

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), NULL);
	g_return_val_if_fail (username != NULL, NULL);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->get_default_server) {
		return klass->get_default_server (protocol, username);
	}

	return NULL;
}

guint
gossip_protocol_get_default_port (GossipProtocol *protocol,
				  gboolean        use_ssl)
{
	GossipProtocolClass *klass;

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), 0);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->get_default_port) {
		return klass->get_default_port (protocol, use_ssl);
	}

	return 0;
}

void
gossip_protocol_send_message (GossipProtocol *protocol,
			      GossipMessage  *message)
{
	GossipProtocolClass *klass;

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->send_message) {
		klass->send_message (protocol, message);
	}
}

void
gossip_protocol_send_composing (GossipProtocol *protocol,
				GossipContact  *contact,
				gboolean        composing)
{
	GossipProtocolClass *klass;

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->send_composing) {
		klass->send_composing (protocol, contact, composing);
	}
}

void
gossip_protocol_set_presence (GossipProtocol *protocol,
			      GossipPresence *presence)
{
	GossipProtocolClass *klass;

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->set_presence) {
		klass->set_presence (protocol, presence);
	}
}

void
gossip_protocol_set_subscription (GossipProtocol *protocol,
				  GossipContact  *contact,
				  gboolean        subscribed)
{
	GossipProtocolClass *klass;

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->set_subscription) {
		klass->set_subscription (protocol, contact, subscribed);
	}
}

gboolean
gossip_protocol_set_vcard (GossipProtocol  *protocol,
			   GossipVCard     *vcard,
			   GossipCallback   callback,
			   gpointer         user_data,
			   GError         **error)
{
	GossipProtocolClass *klass;

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), FALSE);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->set_vcard) {
		return klass->set_vcard (protocol, vcard,
					 callback, user_data,
					 error);
	}

	/* Don't report error if protocol doesn't implement this */
	return TRUE;
}

GossipContact *
gossip_protocol_find_contact (GossipProtocol *protocol,
			      const gchar    *id)
{
	GossipProtocolClass *klass;

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), NULL);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->find_contact) {
		return klass->find_contact (protocol, id);
	}

	return NULL;
}

void
gossip_protocol_add_contact (GossipProtocol *protocol,
			     const gchar    *id,
			     const gchar    *name,
			     const gchar    *group,
			     const gchar    *message)
{
	GossipProtocolClass *klass;

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->add_contact) {
		klass->add_contact (protocol, id, name, group, message);
	}
}

void
gossip_protocol_rename_contact (GossipProtocol *protocol,
				GossipContact  *contact,
				const gchar    *new_name)
{
	GossipProtocolClass *klass;

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->rename_contact) {
		klass->rename_contact (protocol, contact, new_name);
	}
}

void
gossip_protocol_remove_contact (GossipProtocol *protocol,
				GossipContact  *contact)
{
	GossipProtocolClass *klass;

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->remove_contact) {
		klass->remove_contact (protocol, contact);
	}
}

void
gossip_protocol_update_contact (GossipProtocol *protocol,
				GossipContact  *contact)
{
	GossipProtocolClass *klass;

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->update_contact) {
		klass->update_contact (protocol, contact);
	}
}

void
gossip_protocol_rename_group (GossipProtocol *protocol,
			      const gchar    *group,
			      const gchar    *new_name)
{
	GossipProtocolClass *klass;

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->rename_group) {
		klass->rename_group (protocol, group, new_name);
	}
}

GossipContact *
gossip_protocol_get_own_contact (GossipProtocol *protocol)
{
	GossipProtocolClass *klass;

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), NULL);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->get_own_contact) {
		return klass->get_own_contact (protocol);
	}

	return NULL;
}

const GList *
gossip_protocol_get_contacts (GossipProtocol *protocol)
{
	GossipProtocolClass *klass;

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), NULL);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->get_contacts) {
		return klass->get_contacts (protocol);
	}

	return NULL;
}

const gchar *
gossip_protocol_get_active_resource (GossipProtocol *protocol,
				     GossipContact  *contact)
{
	GossipProtocolClass *klass;

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), NULL);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->get_active_resource) {
		return klass->get_active_resource (protocol, contact);
	}

	return NULL;
}

GList *
gossip_protocol_get_groups (GossipProtocol *protocol)
{
	GossipProtocolClass *klass;

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), NULL);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->get_groups) {
		return klass->get_groups (protocol);
	}

	return NULL;
}

gboolean
gossip_protocol_get_vcard (GossipProtocol       *protocol,
			   GossipContact        *contact,
			   GossipVCardCallback   callback,
			   gpointer              user_data,
			   GError              **error)
{
	GossipProtocolClass *klass;

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), FALSE);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->get_vcard) {
		return klass->get_vcard (protocol, contact,
					 callback, user_data,
					 error);
	}

	/* Don't report error if protocol doesn't implement this */
	return TRUE;
}

gboolean
gossip_protocol_get_version (GossipProtocol         *protocol,
			     GossipContact          *contact,
			     GossipVersionCallback   callback,
			     gpointer                user_data,
			     GError                **error)
{
	GossipProtocolClass *klass;

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), FALSE);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->get_version) {
		return klass->get_version (protocol, contact,
						 callback, user_data,
						 error);
	}

	/* Don't report error if protocol doesn't implement this */
	return TRUE;
}

void
gossip_protocol_register_account (GossipProtocol      *protocol,
				  GossipAccount       *account,
				  GossipVCard         *vcard,
				  GossipErrorCallback  callback,
				  gpointer             user_data)
{
	GossipProtocolClass *klass;

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));
	g_return_if_fail (callback != NULL);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->register_account) {
		klass->register_account (protocol, account, vcard,
					 callback, user_data);
	}
}

void
gossip_protocol_register_cancel (GossipProtocol *protocol)
{
	GossipProtocolClass *klass;

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->register_cancel) {
		klass->register_cancel (protocol);
	}
}

void
gossip_protocol_change_password (GossipProtocol      *protocol,
				 const gchar         *new_password,
				 GossipErrorCallback  callback,
				 gpointer             user_data)
{
	GossipProtocolClass *klass;

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));
	g_return_if_fail (new_password != NULL);
	g_return_if_fail (callback != NULL);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->change_password) {
		klass->change_password (protocol, new_password,
					callback, user_data);
	}
}

void
gossip_protocol_change_password_cancel (GossipProtocol *protocol)
{
	GossipProtocolClass *klass;

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->change_password_cancel) {
		klass->change_password_cancel (protocol);
	}
}

void
gossip_protocol_get_avatar_requirements (GossipProtocol  *protocol,
					 guint           *min_width,
					 guint           *min_height,
					 guint           *max_width,
					 guint           *max_height,
					 gsize           *max_size,
					 gchar          **format)
{
	GossipProtocolClass *klass;

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->get_avatar_requirements) {
		klass->get_avatar_requirements (protocol,
						min_width, min_height,
						max_width, max_height,
						max_size,
						format);
	}
}

const gchar *
gossip_protocol_error_to_string (GossipProtocolError error)
{
	const gchar *str = _("An unknown error occurred.");

	switch (error) {
	case GOSSIP_PROTOCOL_NO_CONNECTION:
		str = _("Connection refused.");
		break;
	case GOSSIP_PROTOCOL_NO_SUCH_HOST:
		str = _("Server address could not be resolved.");
		break;
	case GOSSIP_PROTOCOL_TIMED_OUT:
		str = _("Connection timed out.");
		break;
	case GOSSIP_PROTOCOL_AUTH_FAILED:
		str = _("Authentication failed.");
		break;
	case GOSSIP_PROTOCOL_DUPLICATE_USER:
		str = _("The username you are trying already exists.");
		break;
	case GOSSIP_PROTOCOL_INVALID_USER:
		str = _("The username you are trying is not valid.");
		break;
	case GOSSIP_PROTOCOL_UNAVAILABLE:
		str = _("This feature is unavailable.");
		break;
	case GOSSIP_PROTOCOL_UNAUTHORIZED:
		str = _("This feature is unauthorized.");
		break;
	case GOSSIP_PROTOCOL_SPECIFIC_ERROR:
		str = _("A specific protocol error occurred that was unexpected.");
		break;
	}

	return str;
}
