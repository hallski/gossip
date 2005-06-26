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
	GossipProtocolErrorNoConnection,
	GossipProtocolErrorNoSuchHost,
	GossipProtocolErrorTimedOut,
	GossipProtocolErrorAuthFailed,
	
	GossipProtocolErrorSpecific,

} GossipProtocolError;

typedef struct _GossipProtocol      GossipProtocol;
typedef struct _GossipProtocolClass GossipProtocolClass;

struct _GossipProtocol {
	GObject parent;
};

struct _GossipProtocolClass {
	GObjectClass parent_class;

	/* virtual functions */
	void          (*setup)                 (GossipProtocol  *protocol,
						GossipAccount   *account);
	void          (*login)                 (GossipProtocol  *protocol);
	void          (*logout)                (GossipProtocol  *protocol);
	gboolean      (*is_connected)          (GossipProtocol  *protocol);
	
	void          (*contact_set_subscription) (GossipProtocol *protocol,
						   GossipContact  *contact,
						   gboolean        subscribed);
	
	void          (*send_message)          (GossipProtocol  *protocol,
						GossipMessage   *message);
	void          (*send_composing)        (GossipProtocol  *protocol,
						GossipContact   *contact,
						gboolean         typing);
	void          (*set_presence)          (GossipProtocol  *protocol,
						GossipPresence  *presence);
	GossipContact * (*find_contact)        (GossipProtocol  *protocol,
						const gchar     *id);
	void          (*add_contact)           (GossipProtocol  *protocol,
						const gchar     *id,
						const gchar     *name,
						const gchar     *group,
						const gchar     *message);
	void          (*rename_contact)        (GossipProtocol  *protocol,
						GossipContact   *contact,
						const gchar     *new_name);
	void          (*remove_contact)        (GossipProtocol  *protocol,
						GossipContact   *contact);
	void          (*update_contact)        (GossipProtocol  *protocol,
						GossipContact   *contact);
	void          (*rename_group)          (GossipProtocol  *protocol,
						const gchar     *group,
						const gchar     *new_name);
	const GList * (*get_contacts)          (GossipProtocol  *protocol);
	const gchar * (*get_active_resource)   (GossipProtocol  *protocol,
						GossipContact   *contact);
	GList *       (*get_groups)            (GossipProtocol  *protocol);
	
	
	gboolean      (*async_register)        (GossipProtocol  *protocol,
					        GossipAccount   *account,
					        GossipAsyncRegisterCallback callback,
					        gpointer         user_data,
					        GError         **error);
	gboolean      (*async_get_vcard)       (GossipProtocol  *protocol,
					        GossipContact   *contact,
					        GossipAsyncVCardCallback callback,
					        gpointer         user_data,
					        GError         **error);
	gboolean      (*async_set_vcard)       (GossipProtocol  *protocol,
					        GossipVCard     *vcard,
					        GossipAsyncResultCallback callback,
					        gpointer         user_data,
					        GError         **error);
	gboolean      (*async_get_version)     (GossipProtocol  *protocol,
					        GossipContact   *contact,
					        GossipAsyncVersionCallback callback,
					        gpointer                   user_data,
					        GError         **error);
};


GType          gossip_protocol_get_type                 (void) G_GNUC_CONST;

void           gossip_protocol_setup                    (GossipProtocol               *protocol,
							 GossipAccount                *account);
void           gossip_protocol_login                    (GossipProtocol               *protocol);
void           gossip_protocol_logout                   (GossipProtocol               *protocol);
gboolean       gossip_protocol_is_connected             (GossipProtocol               *protocol);
void           gossip_protocol_contact_set_subscription (GossipProtocol               *protocol,
							 GossipContact                *contact,
							 gboolean                      subscribed);
void           gossip_protocol_send_message             (GossipProtocol               *protocol,
							 GossipMessage                *message);
void           gossip_protocol_send_composing           (GossipProtocol               *protocol,
							 GossipContact                *contact,
							 gboolean                      typing);
void           gossip_protocol_set_presence             (GossipProtocol               *protocol,
							 GossipPresence               *presence);
GossipContact *gossip_protocol_find_contact             (GossipProtocol               *protocol,
							 const gchar                  *id);
void           gossip_protocol_add_contact              (GossipProtocol               *protocol,
							 const gchar                  *id,
							 const gchar                  *name,
							 const gchar                  *group,
							 const gchar                  *message);
void           gossip_protocol_rename_contact           (GossipProtocol               *protocol,
							 GossipContact                *contact,
							 const gchar                  *new_name);
void           gossip_protocol_remove_contact           (GossipProtocol               *protocol,
							 GossipContact                *contact);
const GList *  gossip_protocol_get_contacts             (GossipProtocol               *protocol);
void           gossip_protocol_update_contact           (GossipProtocol               *protocol,
							 GossipContact                *contact);
void           gossip_protocol_rename_group             (GossipProtocol               *protocol,
							 const gchar                  *group,
							 const gchar                  *new_name);
const gchar *  gossip_protocol_get_active_resource      (GossipProtocol               *protocol,
							 GossipContact                *contact);
GList *        gossip_protocol_get_groups               (GossipProtocol               *protocol);
gboolean       gossip_protocol_async_register           (GossipProtocol               *protocol,
							 GossipAccount                *account,
							 GossipAsyncRegisterCallback   callback,
							 gpointer                      user_data,
							 GError                      **error);
gboolean       gossip_protocol_async_get_vcard          (GossipProtocol               *protocol,
							 GossipContact                *contact,
							 GossipAsyncVCardCallback      callback,
							 gpointer                      user_data,
							 GError                      **error);
gboolean       gossip_protocol_async_set_vcard          (GossipProtocol               *protocol,
							 GossipVCard                  *vcard,
							 GossipAsyncResultCallback     callback,
							 gpointer                      user_data,
							 GError                      **error);
gboolean       gossip_protocol_async_get_version        (GossipProtocol               *protocol,
							 GossipContact                *contact,
							 GossipAsyncVersionCallback    callback,
							 gpointer                      user_data,
							 GError                      **error);
void           gossip_protocol_error                    (GossipProtocol               *protocol,
							 GossipProtocolError           code,
							 const gchar                  *reason);


#endif /* __GOSSIP_PROTOCOL_H__ */
