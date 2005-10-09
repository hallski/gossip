/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
 */

#include <config.h>

#include "libgossip-marshal.h"
#include "gossip-protocol.h"

static void    gossip_protocol_class_init       (GossipProtocolClass *klass);
static void    gossip_protocol_init             (GossipProtocol      *protocol);

enum {
	LOGGED_IN,
	LOGGED_OUT,
	NEW_MESSAGE,
	CONTACT_ADDED,
	CONTACT_UPDATED,
	CONTACT_PRESENCE_UPDATED,
	CONTACT_REMOVED,
	COMPOSING_EVENT,

	/* Used for protocols to request information from user */
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
	klass->setup               = NULL;
	klass->login               = NULL;
	klass->logout              = NULL;
	klass->register_account    = NULL;
	klass->is_connected        = NULL;
	klass->send_message        = NULL;
	klass->send_composing      = NULL;
	klass->set_presence        = NULL;
        klass->find_contact        = NULL;
        klass->add_contact         = NULL;
        klass->rename_contact      = NULL;
        klass->remove_contact      = NULL;
	klass->update_contact      = NULL;
	klass->rename_group        = NULL;
	klass->get_active_resource = NULL;
	klass->get_groups          = NULL;
	klass->get_vcard           = NULL;
	klass->set_vcard           = NULL;

	signals[LOGGED_IN] = 
		g_signal_new ("logged-in",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__POINTER,
			      G_TYPE_NONE, 
			      1, G_TYPE_POINTER);

	/* Maybe include a reason for disconnect? */
	signals[LOGGED_OUT] = 
		g_signal_new ("logged-out",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__POINTER,
			      G_TYPE_NONE, 
			      1, G_TYPE_POINTER);

	signals[NEW_MESSAGE] = 
		g_signal_new ("new-message",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	signals[CONTACT_ADDED] = 
		g_signal_new ("contact-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	signals[CONTACT_UPDATED] = 
		g_signal_new ("contact-updated",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	
	signals[CONTACT_PRESENCE_UPDATED] =
		g_signal_new ("contact-presence-updated",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	signals[CONTACT_REMOVED] = 
		g_signal_new ("contact-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	signals[COMPOSING_EVENT] = 
		g_signal_new ("composing-event",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__POINTER_BOOLEAN,
			      G_TYPE_NONE,
			      2, G_TYPE_POINTER, G_TYPE_BOOLEAN);
	signals[GET_PASSWORD] =
		g_signal_new ("get-password",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_STRING__POINTER,
			      G_TYPE_STRING,
			      1, G_TYPE_POINTER);

        signals[SUBSCRIPTION_REQUEST] =
                g_signal_new ("subscription-request",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
                              libgossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	signals[ERROR] = 
		g_signal_new ("error",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

}

static void
gossip_protocol_init (GossipProtocol *protocol)
{
	/* FIXME: Implement */
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
gossip_protocol_register_account (GossipProtocol          *protocol,
				  GossipAccount           *account,
				  GossipRegisterCallback   callback,
				  gpointer                 user_data,
				  GError                 **error)
{
	GossipProtocolClass *klass;

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), FALSE);
	g_return_val_if_fail (callback != NULL, FALSE);

	klass = GOSSIP_PROTOCOL_GET_CLASS (protocol);
	if (klass->register_account) {
		return klass->register_account (protocol, account, 
					      callback, user_data, 
					      error);
	}

	return FALSE;
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
gossip_protocol_set_vcard (GossipProtocol        *protocol,
			   GossipVCard           *vcard,
			   GossipResultCallback   callback,
			   gpointer               user_data,
			   GError               **error)
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
gossip_protocol_error (GossipProtocol      *protocol, 
		       GossipProtocolError  code,
		       const gchar         *reason)
{
	GError        *error;
	static GQuark  quark = 0;

	g_return_if_fail (protocol != NULL);
	g_return_if_fail (reason != NULL);

	if (!quark) {
		quark = g_quark_from_static_string ("gossip-protocol");
	}
	
	error = g_error_new_literal (quark, code, reason);
	g_signal_emit_by_name (protocol, "error", error);
 	g_error_free (error); 
}
