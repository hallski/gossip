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

#ifndef __GOSSIP_PROTOCOL_H__
#define __GOSSIP_PROTOCOL_H__

#include <glib-object.h>

#include "gossip-async.h"
#include "gossip-contact.h"
#include "gossip-message.h"
#include "gossip-account.h"
#include "gossip-vcard.h"

#define GOSSIP_TYPE_PROTOCOL         (gossip_protocol_get_type ())
#define GOSSIP_PROTOCOL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), \
 			              GOSSIP_TYPE_PROTOCOL, GossipProtocol))
#define GOSSIP_PROTOCOL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), \
				      GOSSIP_TYPE_PROTOCOL, \
			              GossipProtocolClass))
#define GOSSIP_IS_PROTOCOL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o),\
			              GOSSIP_TYPE_PROTOCOL))
#define GOSSIP_IS_PROTOCOL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), \
				      GOSSIP_TYPE_PROTOCOL))
#define GOSSIP_PROTOCOL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),\
				      GOSSIP_TYPE_PROTOCOL, \
				      GossipProtocolClass))


typedef enum {
	GOSSIP_PROTOCOL_NO_CONNECTION,
	GOSSIP_PROTOCOL_NO_SUCH_HOST,
	GOSSIP_PROTOCOL_TIMED_OUT,
	GOSSIP_PROTOCOL_AUTH_FAILED,
	GOSSIP_PROTOCOL_DUPLICATE_USER,
	GOSSIP_PROTOCOL_INVALID_USER,
	GOSSIP_PROTOCOL_UNAVAILABLE,
	GOSSIP_PROTOCOL_UNAUTHORIZED,
	GOSSIP_PROTOCOL_SPECIFIC_ERROR,
} GossipProtocolError;


typedef struct _GossipProtocol      GossipProtocol;
typedef struct _GossipProtocolClass GossipProtocolClass;


struct _GossipProtocol {
	GObject parent;
};


struct _GossipProtocolClass {
	GObjectClass parent_class;

	/* virtual functions */
	void            (*setup)               (GossipProtocol  *protocol,
						GossipAccount   *account);
	void            (*login)               (GossipProtocol  *protocol);
	void            (*logout)              (GossipProtocol  *protocol);
	gboolean        (*is_connected)        (GossipProtocol  *protocol);
	gboolean        (*is_valid_username)   (GossipProtocol  *protocol,
						const gchar     *username);
	gboolean        (*is_ssl_supported)    (GossipProtocol  *protocol);
	const gchar *   (*get_example_username)(GossipProtocol  *protocol);
	gchar *         (*get_default_server)  (GossipProtocol  *protocol,
						const gchar     *username);
	guint16         (*get_default_port)    (GossipProtocol  *protocol,
						gboolean         use_ssl);
	
	void            (*send_message)        (GossipProtocol  *protocol,
						GossipMessage   *message);
	void            (*send_composing)      (GossipProtocol  *protocol,
						GossipContact   *contact,
						gboolean         typing);
	void            (*set_presence)        (GossipProtocol  *protocol,
						GossipPresence  *presence);
	void            (*set_subscription)    (GossipProtocol *protocol,
						GossipContact  *contact,
						gboolean        subscribed);
	gboolean        (*set_vcard)           (GossipProtocol  *protocol,
					        GossipVCard     *vcard,
						GossipResultCallback callback,
					        gpointer         user_data,
					        GError         **error);
	GossipContact * (*find_contact)        (GossipProtocol  *protocol,
						const gchar     *id);
	void            (*add_contact)         (GossipProtocol  *protocol,
						const gchar     *id,
						const gchar     *name,
						const gchar     *group,
						const gchar     *message);
	void            (*rename_contact)      (GossipProtocol  *protocol,
						GossipContact   *contact,
						const gchar     *new_name);
	void            (*remove_contact)      (GossipProtocol  *protocol,
						GossipContact   *contact);
	void            (*update_contact)      (GossipProtocol  *protocol,
						GossipContact   *contact);
	void            (*rename_group)        (GossipProtocol  *protocol,
						const gchar     *group,
						const gchar     *new_name);
	const GList *   (*get_contacts)        (GossipProtocol  *protocol);
	GossipContact * (*get_own_contact)     (GossipProtocol  *protocol);
	const gchar *   (*get_active_resource) (GossipProtocol  *protocol,
						GossipContact   *contact);
	GList *         (*get_groups)          (GossipProtocol  *protocol);
	
	
	gboolean        (*get_vcard)           (GossipProtocol  *protocol,
					        GossipContact   *contact,
					        GossipVCardCallback callback,
					        gpointer         user_data,
					        GError         **error);
	gboolean        (*get_version)         (GossipProtocol  *protocol,
					        GossipContact   *contact,
						GossipVersionCallback callback,
					        gpointer         user_data,
					        GError         **error);
	void            (*register_account)    (GossipProtocol  *protocol,
					        GossipAccount   *account,
						GossipVCard     *vcard,
					        GossipRegisterCallback callback,
					        gpointer         user_data);
	void            (*register_cancel)     (GossipProtocol  *protocol);
};


GType           gossip_protocol_get_type              (void) G_GNUC_CONST;

GossipProtocol *gossip_protocol_new_from_account_type (GossipAccountType        type);

void            gossip_protocol_setup                 (GossipProtocol          *protocol,
						       GossipAccount           *account);
void            gossip_protocol_login                 (GossipProtocol          *protocol);
void            gossip_protocol_logout                (GossipProtocol          *protocol);
gboolean        gossip_protocol_is_connected          (GossipProtocol          *protocol);
gboolean        gossip_protocol_is_valid_username     (GossipProtocol          *protocol,
						       const gchar             *username);
gboolean        gossip_protocol_is_ssl_supported      (GossipProtocol          *protocol);
const gchar   * gossip_protocol_get_example_username  (GossipProtocol          *protocol);
gchar         * gossip_protocol_get_default_server    (GossipProtocol          *protocol,
						       const gchar             *username);
guint16         gossip_protocol_get_default_port      (GossipProtocol          *protocol,
						       gboolean                 use_ssl);
void            gossip_protocol_send_message          (GossipProtocol          *protocol,
						       GossipMessage           *message);
void            gossip_protocol_send_composing        (GossipProtocol          *protocol,
						       GossipContact           *contact,
						       gboolean                 typing);
void            gossip_protocol_set_presence          (GossipProtocol          *protocol,
						       GossipPresence          *presence);
void            gossip_protocol_set_subscription      (GossipProtocol          *protocol,
						       GossipContact           *contact,
						       gboolean                 subscribed);
gboolean        gossip_protocol_set_vcard             (GossipProtocol          *protocol,
						       GossipVCard             *vcard,
						       GossipResultCallback     callback,
						       gpointer                 user_data,
						       GError                 **error);
GossipContact * gossip_protocol_find_contact          (GossipProtocol          *protocol,
						       const gchar             *id);
void            gossip_protocol_add_contact           (GossipProtocol          *protocol,
						       const gchar             *id,
						       const gchar             *name,
						       const gchar             *group,
						       const gchar             *message);
void            gossip_protocol_rename_contact        (GossipProtocol          *protocol,
						       GossipContact           *contact,
						       const gchar             *new_name);
void            gossip_protocol_remove_contact        (GossipProtocol          *protocol,
						       GossipContact           *contact);
void            gossip_protocol_update_contact        (GossipProtocol          *protocol,
						       GossipContact           *contact);
void            gossip_protocol_rename_group          (GossipProtocol          *protocol,
						       const gchar             *group,
						       const gchar             *new_name);
const GList *   gossip_protocol_get_contacts          (GossipProtocol          *protocol);
GossipContact * gossip_protocol_get_own_contact       (GossipProtocol          *protocol);
const gchar *   gossip_protocol_get_active_resource   (GossipProtocol          *protocol,
						       GossipContact           *contact);
GList *         gossip_protocol_get_groups            (GossipProtocol          *protocol);
gboolean        gossip_protocol_get_vcard             (GossipProtocol          *protocol,
						       GossipContact           *contact,
						       GossipVCardCallback      callback,
						       gpointer                 user_data,
						       GError                 **error);
gboolean        gossip_protocol_get_version           (GossipProtocol          *protocol,
						       GossipContact           *contact,
						       GossipVersionCallback    callback,
						       gpointer                 user_data,
						       GError                 **error);
void            gossip_protocol_register_account      (GossipProtocol          *protocol,
						       GossipAccount           *account,
						       GossipVCard             *vcard,
						       GossipRegisterCallback   callback,
						       gpointer                 user_data);
void            gossip_protocol_register_cancel       (GossipProtocol          *protocol);
const gchar *   gossip_protocol_error_to_string       (GossipProtocolError      error);

#endif /* __GOSSIP_PROTOCOL_H__ */
