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

#include <stdlib.h>
#include <string.h>
#include <gconf/gconf-client.h>
#include <glib/gi18n.h>

#include <dbus/dbus-glib.h>
#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-conn-gen.h>
#include <libtelepathy/tp-connmgr.h>
#include <libtelepathy/tp-chan.h>
#include <libtelepathy/tp-chan-gen.h>
#include <libtelepathy/tp-chan-type-text-gen.h>
#include <libtelepathy/tp-interfaces.h>


#include <libgossip/gossip-account.h>
#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-vcard.h>
#include <libgossip/gossip-utils.h>
#include <libgossip/gossip-chatroom-provider.h>
#include <libgossip/gossip-chatroom-contact.h>
#include <libgossip/gossip-ft-provider.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-utils.h>


#include "gossip-telepathy.h"
#include "gossip-telepathy-contacts.h"
#include "gossip-telepathy-message.h"
#include "gossip-telepathy-chatrooms.h"
#include "gossip-telepathy-contact-list.h"
#include "gossip-telepathy-private.h"


#define DEBUG_DOMAIN "Telepathy"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_TELEPATHY, GossipTelepathyPriv))

/* This is the timeout we will accept the user to be composing for
 * before we assume it is stuck and the server has failed to tell us
 * the user has stopped composing.
 */
#define COMPOSING_TIMEOUT   45

struct _GossipTelepathyPriv {
	GossipContact              *contact;
	GossipAccount              *account;
	GossipPresence             *presence;
	GossipVCard                *vcard;

	/* Cancel registration attempt */
	gboolean                    register_cancel;

	/* Connection details */
	guint                       connection_timeout_id;
	gboolean                    disconnect_request;

	/* Used to hold a list of composing message ids, this is so we
	 * can send the cancelation to the last message id.
	 */
	GHashTable                 *composing_ids;
	GHashTable                 *composing_timeouts;
	GHashTable                 *composing_requests;

	/* Telepathy connection object */
	TpConn                     *tp_conn;

	TelepathyConnectionStatus   status;

	/* Channels and interfaces in other modules */
	GossipTelepathyContacts    *contacts;
	GossipTelepathyContactList *contact_list;
	GossipTelepathyChatrooms   *chatrooms;
	GossipTelepathyMessage     *message;
};

static void             gossip_telepathy_class_init              (GossipTelepathyClass         *klass);
static void             gossip_telepathy_init                    (GossipTelepathy              *telepathy);
static void             telepathy_finalize                       (GObject                      *obj);
static void             telepathy_setup                          (GossipProtocol               *protocol,
								  GossipAccount                *account);
static void             telepathy_login                          (GossipProtocol               *protocol);
static void             telepathy_logout                         (GossipProtocol               *protocol);
static void             telepathy_register_account               (GossipProtocol               *protocol,
								  GossipAccount                *account,
								  GossipVCard                  *vcard,
								  GossipResultErrorCallback     callback,
								  gpointer                      user_data);
static void             telepathy_register_cancel                (GossipProtocol               *protocol);
static gboolean         telepathy_is_connected                   (GossipProtocol               *protocol);
static gboolean         telepathy_is_connecting                  (GossipProtocol               *protocol);
static gboolean         telepathy_is_valid_username              (GossipProtocol               *protocol,
								  const gchar                  *username);
static gboolean         telepathy_is_ssl_supported               (GossipProtocol               *protocol);
static const gchar *    telepathy_get_example_username           (GossipProtocol               *protocol);
static gchar *          telepathy_get_default_server             (GossipProtocol               *protocol,
								  const gchar                  *username);
static guint            telepathy_get_default_port               (GossipProtocol               *protocol,
								  gboolean                      use_ssl);
static void             telepathy_send_message                   (GossipProtocol               *protocol,
								  GossipMessage                *message);
static void             telepathy_send_composing                 (GossipProtocol               *protocol,
								  GossipContact                *contact,
								  gboolean                      typing);
static void             telepathy_set_presence                   (GossipProtocol               *protocol,
								  GossipPresence               *presence);
static void             telepathy_set_subscription               (GossipProtocol               *protocol,
								  GossipContact                *contact,
								  gboolean                      subscribed);
static gboolean         telepathy_set_vcard                      (GossipProtocol               *protocol,
								  GossipVCard                  *vcard,
								  GossipResultCallback          callback,
								  gpointer                      user_data,
								  GError                      **error);
static void             telepathy_avatars_get_requirements       (GossipProtocol               *protocol,
								  guint                        *min_width,
								  guint                        *min_height,
								  guint                        *max_width,
								  guint                        *max_height,
								  gsize                        *max_size,
								  gchar                       **format);
static void             telepathy_change_password                (GossipProtocol             *protocol,
								  const gchar                *new_password,
								  GossipResultErrorCallback   callback,
								  gpointer                    user_data);
static void             telepathy_change_password_cancel         (GossipProtocol             *protocol);
static GossipContact *  telepathy_contact_find                   (GossipProtocol               *protocol,
								  const gchar                  *id);
static void             telepathy_contact_add                    (GossipProtocol               *protocol,
								  const gchar                  *id,
								  const gchar                  *name,
								  const gchar                  *group,
								  const gchar                  *message);
static void             telepathy_contact_rename                 (GossipProtocol               *protocol,
								  GossipContact                *contact,
								  const gchar                  *new_name);
static void             telepathy_contact_remove                 (GossipProtocol               *protocol,
								  GossipContact                *contact);
static void             telepathy_contact_update                 (GossipProtocol               *protocol,
								  GossipContact                *contact);
static void             telepathy_group_rename                   (GossipProtocol               *protocol,
								  const gchar                  *group,
								  const gchar                  *new_name);
static const GList *    telepathy_get_contacts                   (GossipProtocol               *protocol);
static GossipContact *  telepathy_get_own_contact                (GossipProtocol               *protocol);
static const gchar *    telepathy_get_active_resource            (GossipProtocol               *protocol,
								  GossipContact                *contact);
static GList *          telepathy_get_groups                     (GossipProtocol               *protocol);
static gboolean         telepathy_get_vcard                      (GossipProtocol               *protocol,
								  GossipContact                *contact,
								  GossipVCardCallback           callback,
								  gpointer                      user_data,
								  GError                      **error);
static gboolean         telepathy_get_version                    (GossipProtocol               *protocol,
								  GossipContact                *contact,
								  GossipVersionCallback         callback,
								  gpointer                      user_data,
								  GError                      **error);
static void             telepathy_connection_status_changed_cb   (DBusGProxy                   *proxy,
								  guint                         status,
								  guint                         reason,
								  GossipTelepathy              *telepathy);
static void             telepathy_connection_setup               (GossipTelepathy              *telepathy);
static void             telepathy_connection_destroy_cb          (DBusGProxy                   *proxy,
								  GossipTelepathy              *telepathy);
void                    telepathy_retrieve_open_channels         (GossipTelepathy              *telepathy);
static void             telepathy_error                          (GossipProtocol               *protocol,
								  GossipProtocolError           code);
static TpConn *         telepathy_get_existing_connection        (GossipAccount                *account);
static GError *         telepathy_error_create                   (GossipProtocolError           code,
								  const gchar                  *reason);
static void             telepathy_newchannel_cb                  (DBusGProxy                   *proxy,
								  const char                   *object_path,
								  const char                   *channel_type,
								  guint                         handle_type,
								  guint                         handle,
								  gboolean                      suppress_handle,
								  GossipTelepathy              *telepathy);
static void             telepathy_contact_rename                 (GossipProtocol               *protocol,
								  GossipContact                *contact,
								  const gchar                  *new_name);
/* chatrooms */
static void             telepathy_chatroom_init                  (GossipChatroomProviderIface  *iface);
static GossipChatroomId telepathy_chatroom_join                  (GossipChatroomProvider       *provider,
								  GossipChatroom               *chatroom,
								  GossipChatroomJoinCb          callback,
								  gpointer                      user_data);
static void             telepathy_chatroom_cancel                (GossipChatroomProvider       *provider,
								  GossipChatroomId              id);
static void             telepathy_chatroom_send                  (GossipChatroomProvider       *provider,
								  GossipChatroomId              id,
								  const gchar                  *message);
static void             telepathy_chatroom_change_topic          (GossipChatroomProvider       *provider,
								  GossipChatroomId              id,
								  const gchar                  *new_topic);
static void             telepathy_chatroom_change_nick           (GossipChatroomProvider       *provider,
								  GossipChatroomId              id,
								  const gchar                  *new_nick);
static void             telepathy_chatroom_leave                 (GossipChatroomProvider       *provider,
								  GossipChatroomId              id);
static GossipChatroom * telepathy_chatroom_find                  (GossipChatroomProvider       *provider,
								  GossipChatroom               *chatroom);
static void             telepathy_chatroom_invite                (GossipChatroomProvider       *provider,
								  GossipChatroomId              id,
								  GossipContact                *contact,
								  const gchar                  *reason);
static void             telepathy_chatroom_invite_accept         (GossipChatroomProvider       *provider,
								  GossipChatroomJoinCb          callback,
								  GossipChatroomInvite         *invite,
								  const gchar                  *nickname);
static void             telepathy_chatroom_invite_decline        (GossipChatroomProvider       *provider,
								  GossipChatroomInvite         *invite,
								  const gchar                  *reason);
static GList *          telepathy_chatroom_get_rooms             (GossipChatroomProvider       *provider);
static void             telepathy_chatroom_browse_rooms          (GossipChatroomProvider       *provider,
								  const gchar                  *server,
								  GossipChatroomBrowseCb        callback,
								  gpointer                      user_data);
/* fts */
static void             telepathy_ft_init                        (GossipFTProviderIface        *iface);
static GossipFTId       telepathy_ft_send                        (GossipFTProvider             *provider,
								  GossipContact                *contact,
								  const gchar                  *file);
static void             telepathy_ft_cancel                      (GossipFTProvider             *provider,
								  GossipFTId                    id);
static void             telepathy_ft_accept                      (GossipFTProvider             *provider,
								  GossipFTId                    id);
static void             telepathy_ft_decline                     (GossipFTProvider             *provider,
								  GossipFTId                    id);
static void             telepathy_insert_params_foreach          (GossipAccount                *account,
								  gchar                        *param_name,
								  GossipAccountParam           *param,
								  GHashTable                   *protocol_params);

G_DEFINE_TYPE_WITH_CODE (GossipTelepathy, gossip_telepathy, GOSSIP_TYPE_PROTOCOL,
			 G_IMPLEMENT_INTERFACE (GOSSIP_TYPE_CHATROOM_PROVIDER,
						telepathy_chatroom_init);
			 G_IMPLEMENT_INTERFACE (GOSSIP_TYPE_FT_PROVIDER,
						telepathy_ft_init));

static void
gossip_telepathy_class_init (GossipTelepathyClass *klass)
{
	GObjectClass        *object_class;
	GossipProtocolClass *protocol_class;

	object_class = G_OBJECT_CLASS (klass);
	protocol_class = GOSSIP_PROTOCOL_CLASS (klass);

	object_class->finalize = telepathy_finalize;

	protocol_class->setup                   = telepathy_setup;
	protocol_class->login                   = telepathy_login;
	protocol_class->logout                  = telepathy_logout;
	protocol_class->is_connected            = telepathy_is_connected;
	protocol_class->is_connecting           = telepathy_is_connecting;
	protocol_class->is_valid_username       = telepathy_is_valid_username;
	protocol_class->is_ssl_supported        = telepathy_is_ssl_supported;
	protocol_class->get_example_username    = telepathy_get_example_username;
	protocol_class->get_default_server      = telepathy_get_default_server;
	protocol_class->get_default_port        = telepathy_get_default_port;
	protocol_class->set_presence            = telepathy_set_presence;
	protocol_class->set_subscription        = telepathy_set_subscription;
	protocol_class->set_vcard               = telepathy_set_vcard;
	protocol_class->send_message            = telepathy_send_message;
	protocol_class->send_composing          = telepathy_send_composing;
	protocol_class->find_contact            = telepathy_contact_find;
	protocol_class->add_contact             = telepathy_contact_add;
	protocol_class->rename_contact          = telepathy_contact_rename;
	protocol_class->remove_contact          = telepathy_contact_remove;
	protocol_class->update_contact          = telepathy_contact_update;
	protocol_class->rename_group            = telepathy_group_rename;
	protocol_class->get_contacts            = telepathy_get_contacts;
	protocol_class->get_own_contact         = telepathy_get_own_contact;
	protocol_class->get_active_resource     = telepathy_get_active_resource;
	protocol_class->get_groups              = telepathy_get_groups;
	protocol_class->get_vcard               = telepathy_get_vcard;
	protocol_class->get_version             = telepathy_get_version;
	protocol_class->register_account        = telepathy_register_account;
	protocol_class->register_cancel         = telepathy_register_cancel;
	protocol_class->change_password         = telepathy_change_password;
	protocol_class->change_password_cancel  = telepathy_change_password_cancel;
	protocol_class->get_avatar_requirements = telepathy_avatars_get_requirements;

	g_type_class_add_private (object_class, sizeof (GossipTelepathyPriv));
}

static void
gossip_telepathy_init (GossipTelepathy *telepathy)
{
	GossipTelepathyPriv *priv;

	priv = GET_PRIV (telepathy);

	priv->composing_ids =
		g_hash_table_new_full (gossip_contact_hash,
				       gossip_contact_equal,
				       (GDestroyNotify) g_object_unref,
				       (GDestroyNotify) g_free);

	priv->composing_timeouts =
		g_hash_table_new_full (gossip_contact_hash,
				       gossip_contact_equal,
				       (GDestroyNotify) g_object_unref, NULL);

	priv->composing_requests =
		g_hash_table_new_full (gossip_contact_hash,
				       gossip_contact_equal,
				       (GDestroyNotify) g_object_unref, NULL);

	priv->status = TP_CONN_STATUS_DISCONNECTED;

	priv->contact_list = gossip_telepathy_contact_list_init (telepathy);
	priv->chatrooms = gossip_telepathy_chatrooms_init (telepathy);
	priv->contacts = gossip_telepathy_contacts_init (telepathy);
	priv->message = gossip_telepathy_message_init (telepathy);
}

static void
telepathy_finalize (GObject *object)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	gossip_debug (DEBUG_DOMAIN, "telepathy_finalize");

	telepathy = GOSSIP_TELEPATHY (object);
	priv = GET_PRIV (telepathy);

	if (priv->presence) {
		g_object_unref (priv->presence);
	}

	if (priv->account) {
		g_object_unref (priv->account);
	}

	if (priv->contact) {
		g_object_unref (priv->contact);
	}

	if (priv->vcard) {
		g_object_unref (priv->vcard);
	}

	g_hash_table_destroy (priv->composing_ids);
	g_hash_table_destroy (priv->composing_timeouts);
	g_hash_table_destroy (priv->composing_requests);

	if (priv->connection_timeout_id != 0) {
		g_source_remove (priv->connection_timeout_id);
		priv->connection_timeout_id = 0;
	}

	gossip_telepathy_contact_list_finalize (priv->contact_list);
	gossip_telepathy_chatrooms_finalize (priv->chatrooms);
	gossip_telepathy_contacts_finalize (priv->contacts);
	gossip_telepathy_message_finalize (priv->message);

	if (priv->tp_conn) {
		g_signal_handlers_disconnect_by_func (priv->tp_conn,
						      telepathy_connection_destroy_cb,
						      telepathy);
		g_object_unref (priv->tp_conn);
	}
}

static void
telepathy_setup (GossipProtocol *protocol,
		 GossipAccount  *account)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
	g_return_if_fail (GOSSIP_IS_TELEPATHY (protocol));

	telepathy = GOSSIP_TELEPATHY (protocol);
	priv = GET_PRIV (telepathy);

	priv->account = g_object_ref (account);
}

static TpConn *
telepathy_get_existing_connection (GossipAccount *account)
{
	TpConn       *tp_conn = NULL;
	const gchar  *account_id = NULL;
	gchar       **name;
	gchar       **name_list;
	gboolean      found = FALSE;

	/* FIXME: We shouldn't depend on the account parameter */
	if (gossip_account_has_param (account, "account")) {
		gossip_account_param_get (account, "account", &account_id, NULL);
	}

	dbus_g_proxy_call (tp_get_bus_proxy (),
			   "ListNames", NULL,
			   G_TYPE_INVALID,
			   G_TYPE_STRV, &name_list,
			   G_TYPE_INVALID);

	for (name = name_list; !found && *name; name++) {
		if (g_str_has_prefix (*name, "org.freedesktop.Telepathy.Connection.")) {
			guint         handle = 0;
			GError       *error = NULL;
			GArray       *handles = NULL;
			gchar       **handle_name = NULL;
			gchar        *obj_path = NULL;
			gchar        *protocol = NULL;
			const gchar  *account_protocol = NULL;
			DBusGProxy   *conn_iface = NULL;

			obj_path = g_strdup_printf ("/%s", *name);
			g_strdelimit (obj_path, ".", '/');

			conn_iface = dbus_g_proxy_new_for_name (tp_get_bus (),
								*name,
								obj_path,
								TP_IFACE_CONN_INTERFACE);

			if (!tp_conn_get_protocol (conn_iface, &protocol, &error)) {
				gossip_debug (DEBUG_DOMAIN, "Error getting protocol : %s",
					      error->message);
				goto next;
			}

			account_protocol = gossip_account_get_protocol (account);

			if (!account_protocol || strcmp (protocol, account_protocol) != 0) {
				goto next;
			}

			if (!tp_conn_get_self_handle (conn_iface, &handle, &error)) {
				gossip_debug (DEBUG_DOMAIN, "Error getting self handle: %s",
					      error->message);
				goto next;
			}

			handles = g_array_new (FALSE, FALSE, sizeof (gint));
			g_array_append_val (handles, handle);

			if (!tp_conn_inspect_handles (conn_iface,
						      TP_CONN_HANDLE_TYPE_CONTACT,
						      handles, &handle_name,
						      &error)) {
				gossip_debug (DEBUG_DOMAIN, "InspectHandle Error: %s, %d",
					      error->message, handle);
				goto next;
			}

			if (!account_id || strcmp (*handle_name, account_id) == 0) {
				tp_conn = tp_conn_new (tp_get_bus (),
						       *name,
						       obj_path);

				gossip_debug (DEBUG_DOMAIN,
					      "Found connected account: %s",
					      account_id);
				found = TRUE;
			}
	next:
			g_free (obj_path);
			g_free (protocol);
			g_clear_error (&error);
			g_strfreev (handle_name);
			if (conn_iface) {
				g_object_unref (conn_iface);
			}
			if (handles) {
				g_array_free (handles, TRUE);
			}
		}
	}

	g_strfreev (name_list);

	return tp_conn;
}

static void
telepathy_login (GossipProtocol *protocol)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;
	guint                status;
	GHashTable          *connection_parameters;
	GError              *error = NULL;
	const gchar         *protocol_name;
	const gchar         *cmgr_name;
	TpConnMgr           *conn_manager;
	TpConnMgrInfo       *cmgr_info;
	gchar               *requested_pass = NULL;
	GValue               requested_pass_value = {0, };
	GossipAccountParam  *pass_param;

	g_return_if_fail (GOSSIP_IS_TELEPATHY (protocol));

	telepathy = GOSSIP_TELEPATHY (protocol);
	priv = GET_PRIV (telepathy);

	gossip_debug (DEBUG_DOMAIN, "Connecting...");
	g_signal_emit_by_name (telepathy, "connecting", priv->account);

	priv->presence = gossip_presence_new ();
	gossip_presence_set_state (priv->presence,
				   GOSSIP_PRESENCE_STATE_AVAILABLE);

	priv->tp_conn = telepathy_get_existing_connection (priv->account);
	if (priv->tp_conn) {
		/* Assume the existing connection is already connected.
		 * Connect to the StatusChanged signal so we can track
		 * disconnects */
		dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->tp_conn),
					     "StatusChanged",
					     G_CALLBACK
					     (telepathy_connection_status_changed_cb),
					     telepathy, NULL);

		telepathy_connection_setup (telepathy);
		telepathy_retrieve_open_channels (telepathy);

		return;
	}

	gossip_debug (DEBUG_DOMAIN, "No existing connection, connecting...");
	connection_parameters = g_hash_table_new (g_str_hash, g_str_equal);
	gossip_account_param_foreach (priv->account,
				      (GossipAccountParamFunc) telepathy_insert_params_foreach,
				      connection_parameters);

	/* Request the password to the user if not set */
	if ((pass_param = gossip_account_param_get_param (priv->account, "password")) &&
	    G_VALUE_HOLDS (&pass_param->g_value, G_TYPE_STRING)) {
		const gchar *str;
		
		str = g_value_get_string (&pass_param->g_value);
		if (G_STR_EMPTY (str)) { 
			g_signal_emit_by_name (protocol, "get-password", 
					       priv->account,
					       &requested_pass);

			if (requested_pass == NULL) {
				g_hash_table_destroy (connection_parameters);
				return;
			}

			g_value_init (&requested_pass_value, G_TYPE_STRING);
			g_value_set_static_string (&requested_pass_value, requested_pass);
			g_hash_table_insert (connection_parameters, "password", 
					     &requested_pass_value);
		}
	}

	protocol_name = gossip_account_get_protocol (priv->account);
	cmgr_name = gossip_account_get_cmgr_name (priv->account);
	cmgr_info = tp_connmgr_get_info ((gchar*) cmgr_name);

	conn_manager = tp_connmgr_new (tp_get_bus (),
				       cmgr_info->bus_name,
				       cmgr_info->object_path,
				       TP_IFACE_CONN_MGR_INTERFACE);
	priv->tp_conn = tp_connmgr_new_connection (conn_manager,
						   connection_parameters,
						   (gchar*) protocol_name);

	g_hash_table_destroy (connection_parameters);
	g_free (requested_pass);
	g_object_unref (conn_manager);
	tp_connmgr_info_free (cmgr_info);

	if (!priv->tp_conn) {
		gossip_debug (DEBUG_DOMAIN, "Failed to create a connection");
		telepathy_error (GOSSIP_PROTOCOL (telepathy),
				 GOSSIP_PROTOCOL_NO_CONNECTION);
		return;
	}

	priv->status = TP_CONN_STATUS_CONNECTING;

	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->tp_conn),
				     "StatusChanged",
				     G_CALLBACK
				     (telepathy_connection_status_changed_cb),
				     telepathy, NULL);

	/* Manually call the callback if the connection isn't
	 * disconnected like we expect */
	if (!tp_conn_get_status (DBUS_G_PROXY (priv->tp_conn), &status, &error)  &&
	    status != TP_CONN_STATUS_DISCONNECTED) {
		gossip_debug (DEBUG_DOMAIN, "The connection wasn't disconnected");
		g_clear_error (&error);
		telepathy_connection_status_changed_cb (DBUS_G_PROXY (priv->tp_conn),
							status,
							TP_CONN_STATUS_REASON_NONE_SPECIFIED ,
							telepathy);
	}
}

static void
telepathy_connection_setup (GossipTelepathy *telepathy)
{
	GossipTelepathyPriv  *priv;
	guint                 handle;
	GArray               *handles;
	gchar               **handles_names;
	GError               *error = NULL;

	priv = GET_PRIV (telepathy);

	priv->status = TP_CONN_STATUS_CONNECTED;

	g_signal_emit_by_name (telepathy, "connected", priv->account);
	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->tp_conn),
				     "NewChannel",
				     G_CALLBACK (telepathy_newchannel_cb),
				     telepathy, NULL);

	g_signal_connect (priv->tp_conn, "destroy",
			  G_CALLBACK (telepathy_connection_destroy_cb),
			  telepathy);

	/* FIXME: Shouldn't be a chatroom contact, we do that to be able to
	 *        add it into a chatroom */
	priv->contact = GOSSIP_CONTACT (gossip_chatroom_contact_new (priv->account));

	g_object_set (priv->contact,
		      "type", GOSSIP_CONTACT_TYPE_USER,
		      NULL);

	/* Get our own handle and id */
	if (!tp_conn_get_self_handle (DBUS_G_PROXY (priv->tp_conn),
				      &handle, &error)) {
		gossip_debug (DEBUG_DOMAIN, "GetSelfHandle Error: %s",
			      error->message);
		g_clear_error (&error);
		goto skip;
	}
	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, handle);
	if (!tp_conn_inspect_handles (DBUS_G_PROXY (priv->tp_conn),
				      TP_CONN_HANDLE_TYPE_CONTACT,
				      handles,
				      &handles_names,
				      &error)) {
		gossip_debug (DEBUG_DOMAIN, "InspectHandle Error: %s",
			      error->message);
		g_clear_error (&error);
		g_array_free (handles, TRUE);
		goto skip;
	}

	gossip_debug (DEBUG_DOMAIN, "Set account ID to %s", *handles_names);
	gossip_account_set_id (priv->account, *handles_names);
	gossip_contact_set_id (priv->contact, *handles_names);
	g_array_free (handles, TRUE);
	g_strfreev (handles_names);
skip:
	gossip_telepathy_contacts_setup (priv->contacts);
}

static void
telepathy_connection_destroy_cb (DBusGProxy      *proxy,
				 GossipTelepathy *telepathy)
{
	GossipTelepathyPriv *priv;

	priv = GET_PRIV (telepathy);

	gossip_debug (DEBUG_DOMAIN, "Connection destroyed, CM crashed ?");
	/* Calling manually the StatusChanged cb since the connection
	 * maybe died before emitting it */
	telepathy_connection_status_changed_cb (proxy,
						TP_CONN_STATUS_DISCONNECTED,
						TP_CONN_STATUS_REASON_NONE_SPECIFIED,
						telepathy);
}

void
telepathy_retrieve_open_channels (GossipTelepathy *telepathy)
{
	GossipTelepathyPriv *priv;
	GPtrArray           *channels;
	GError              *error = NULL;
	guint                i;

	priv = GET_PRIV (telepathy);

	if (!tp_conn_list_channels (DBUS_G_PROXY (priv->tp_conn),
				    &channels, &error)) {
		gossip_debug (DEBUG_DOMAIN,
			      "Failed to get list of open channels: %s",
			      error->message);
		g_clear_error (&error);
		return;
	}

	for (i = 0; channels->len > i; i++) {
		GValueArray                   *chan_struct;
		const gchar                   *object_path;
		const gchar                   *chan_iface;
		TelepathyConnectionHandleType  handle_type;
		guint                          handle;

		chan_struct = g_ptr_array_index (channels, i);
		object_path = g_value_get_boxed (g_value_array_get_nth (chan_struct, 0));
		chan_iface = g_value_get_string (g_value_array_get_nth (chan_struct, 1));
		handle_type = g_value_get_uint (g_value_array_get_nth (chan_struct, 2));
		handle = g_value_get_uint (g_value_array_get_nth (chan_struct, 3));

		telepathy_newchannel_cb (DBUS_G_PROXY (priv->tp_conn),
					 object_path, chan_iface,
					 handle_type, handle,
					 FALSE, telepathy);
	}

	g_ptr_array_free (channels, TRUE);
}

static void
telepathy_connection_status_changed_cb (DBusGProxy                      *proxy,
					TelepathyConnectionStatus        status,
					TelepathyConnectionStatusReason  reason,
					GossipTelepathy                 *telepathy)
{
	GossipTelepathyPriv       *priv;
	GossipProtocolError        gossip_protocol_error;
	TelepathyConnectionStatus  old_status;

	gossip_debug (DEBUG_DOMAIN, "Status changed. new status: %d reason: %d",
		      status, reason);

	priv = GET_PRIV (telepathy);

	old_status = priv->status;
	priv->status = status;

	if (status == TP_CONN_STATUS_CONNECTED) {
		/* Logged in */
		telepathy_connection_setup (telepathy);

		return;
	}

	if (status == TP_CONN_STATUS_DISCONNECTED) {
		if (priv->tp_conn) {
			g_signal_handlers_disconnect_by_func (priv->tp_conn,
							      telepathy_connection_destroy_cb,
							      telepathy);
			g_object_unref (priv->tp_conn);
			priv->tp_conn = NULL;
		}

		if (priv->presence) {
			g_object_unref (priv->presence);
			priv->presence = NULL;
		}
		gossip_debug (DEBUG_DOMAIN, "Disconnected.");
	}


	if (old_status == TP_CONN_STATUS_CONNECTED &&
	    status == TP_CONN_STATUS_DISCONNECTED) {
		/* We got disconnected */

		g_signal_emit_by_name (telepathy, "disconnecting", priv->account);
		if (reason == TP_CONN_STATUS_REASON_REQUESTED) {
			/* Logged out */
			g_signal_emit_by_name (telepathy, "disconnected",
					       priv->account,
					       GOSSIP_DISCONNECT_ASKED);
		} else {
			/* Some error */
			g_signal_emit_by_name (telepathy, "disconnected",
					       priv->account,
					       GOSSIP_DISCONNECT_ERROR);
		}

		return;
	}

	if (status == TP_CONN_STATUS_DISCONNECTED) {
		/* We have trouble connecting */

		switch (reason) {
		case TP_CONN_STATUS_REASON_NETWORK_ERROR:
			gossip_protocol_error = GOSSIP_PROTOCOL_NO_CONNECTION;
			break;
		case TP_CONN_STATUS_REASON_AUTHENTICATION_FAILED:
			gossip_protocol_error = GOSSIP_PROTOCOL_AUTH_FAILED;
			break;
		default:
			gossip_protocol_error = GOSSIP_PROTOCOL_SPECIFIC_ERROR;
			break;
		}

		g_signal_emit_by_name (telepathy, "disconnecting", priv->account);
		g_signal_emit_by_name (telepathy, "disconnected", priv->account,
				       GOSSIP_DISCONNECT_ERROR);

		telepathy_error (GOSSIP_PROTOCOL (telepathy), gossip_protocol_error);
	}
}

static void
telepathy_logout (GossipProtocol *protocol)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;
	GError              *error = NULL;

	g_return_if_fail (GOSSIP_IS_TELEPATHY (protocol));

	telepathy = GOSSIP_TELEPATHY (protocol);
	priv = GET_PRIV (telepathy);

	g_signal_emit_by_name (telepathy, "disconnecting", priv->account);

	if (!priv->tp_conn) {
		return;
	}

	if (!tp_conn_disconnect (DBUS_G_PROXY (priv->tp_conn), &error)) {
		gossip_debug (DEBUG_DOMAIN, "Error logging out: %s",
			      error->message);
		g_clear_error (&error);
	}
}

static void
telepathy_error (GossipProtocol      *protocol,
		 GossipProtocolError  code)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;
	GError              *error;
	const gchar         *message;

	g_return_if_fail (GOSSIP_IS_TELEPATHY (protocol));

	telepathy = GOSSIP_TELEPATHY (protocol);
	priv = GET_PRIV (telepathy);

	message = gossip_protocol_error_to_string (code);
	gossip_debug (DEBUG_DOMAIN, "Protocol error:%d->'%s'", code, message);

	error = telepathy_error_create (code, message);
	g_signal_emit_by_name (protocol, "error", priv->account, error);
	g_clear_error (&error);
}

static GError *
telepathy_error_create (GossipProtocolError  code,
			const gchar         *reason)
{
	static GQuark  quark = 0;
	GError        *error;

	if (!quark) {
		quark = g_quark_from_static_string ("gossip-telepathy");
	}

	error = g_error_new_literal (quark, code, reason);

	return error;
}

void
telepathy_insert_params_foreach (GossipAccount      *account,
				 gchar              *param_name,
				 GossipAccountParam *param,
				 GHashTable         *protocol_params)
{
	if (G_VALUE_HOLDS (&param->g_value, G_TYPE_STRING)) {
		const gchar *str;

		str = g_value_get_string (&param->g_value);
		if (!str || *str == '\0') {
			return;
		}
	}

	g_hash_table_insert (protocol_params, param_name, &param->g_value);
}

static void
telepathy_register_account (GossipProtocol            *protocol,
			    GossipAccount             *account,
			    GossipVCard               *vcard,
			    GossipResultErrorCallback  callback,
			    gpointer                   user_data)
{
	GossipResult result = GOSSIP_RESULT_ERROR_UNAVAILABLE;

	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	if (gossip_account_has_param (account, "register")) {
		gossip_account_param_set (account, "register", TRUE, NULL);
		result = GOSSIP_RESULT_OK;
	}

	if (callback) {
		(callback) (result, NULL, user_data);
	}
}

static void
telepathy_register_cancel (GossipProtocol *protocol)
{
	gossip_debug (DEBUG_DOMAIN, "telepathy_register_cancel");
}

static gboolean
telepathy_is_connected (GossipProtocol *protocol)
{
	GossipTelepathyPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY (protocol), FALSE);

	priv = GET_PRIV (protocol);

	return priv->status == TP_CONN_STATUS_CONNECTED;
}

static gboolean
telepathy_is_connecting (GossipProtocol *protocol)
{
	GossipTelepathyPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY (protocol), FALSE);

	priv = GET_PRIV (protocol);

	return priv->status == TP_CONN_STATUS_CONNECTING;
}

static gboolean
telepathy_is_valid_username (GossipProtocol *protocol,
			     const gchar    *username)
{
	gossip_debug (DEBUG_DOMAIN, "telepathy_is_valid_username");

	return TRUE;
}

static gboolean
telepathy_is_ssl_supported (GossipProtocol *protocol)
{
	gossip_debug (DEBUG_DOMAIN, "telepathy_is_ssl_supported");

	return TRUE;
}

static const gchar *
telepathy_get_example_username (GossipProtocol *protocol)
{
	return "";
}

static gchar *
telepathy_get_default_server (GossipProtocol *protocol,
			      const gchar    *username)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;
	const gchar         *server;

	telepathy = GOSSIP_TELEPATHY (protocol);
	priv = GET_PRIV (telepathy);

	gossip_account_param_get (priv->account,
				  "server", &server,
				  NULL);

	if (server) {
		return g_strdup (server);
	} else {
		return g_strdup ("");
	}
}

static guint
telepathy_get_default_port (GossipProtocol *protocol,
			    gboolean        use_ssl)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;
	guint                port = 0;

	telepathy = GOSSIP_TELEPATHY (protocol);
	priv = GET_PRIV (telepathy);

	gossip_account_param_get (priv->account,
				  "port", &port,
				  NULL);
	return port;
}

static void
telepathy_send_message (GossipProtocol *protocol,
			GossipMessage  *message)
{
	GossipTelepathyPriv *priv;
	GossipContact       *recipient;
	GossipTelepathy     *telepathy;
	guint                handle;
	const gchar         *id;

	telepathy = GOSSIP_TELEPATHY (protocol);

	priv = GET_PRIV (telepathy);

	recipient = gossip_message_get_recipient (message);
	id = gossip_contact_get_id (recipient);

	handle = gossip_telepathy_contacts_get_handle (priv->contacts, id);

	gossip_telepathy_message_send (priv->message,
				       handle,
				       gossip_message_get_body (message));

}

static void
telepathy_send_composing (GossipProtocol *protocol,
			  GossipContact  *contact,
			  gboolean        typing)
{
	gossip_debug (DEBUG_DOMAIN, "telepathy_send_composing");
}

static void
telepathy_set_presence (GossipProtocol *protocol,
			GossipPresence *presence)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	g_return_if_fail (GOSSIP_IS_TELEPATHY (protocol));
	g_return_if_fail (GOSSIP_IS_PRESENCE (presence));

	telepathy = GOSSIP_TELEPATHY (protocol);
	priv = GET_PRIV (telepathy);

	if (priv->presence) {
		g_object_unref (priv->presence);
	}
	priv->presence = g_object_ref (presence);

	gossip_telepathy_contacts_send_presence (priv->contacts, presence);
}

static void
telepathy_set_subscription (GossipProtocol *protocol,
			    GossipContact  *contact,
			    gboolean        subscribed)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;
	guint                handle;
	const gchar         *id;

	g_return_if_fail (GOSSIP_IS_TELEPATHY (protocol));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	telepathy = GOSSIP_TELEPATHY (protocol);

	priv = GET_PRIV (telepathy);

	id = gossip_contact_get_id (contact);
	handle = gossip_telepathy_contacts_get_handle (priv->contacts, id);
	gossip_telepathy_contact_list_set_subscription (priv->contact_list,
							handle,
							subscribed);
}

static gboolean
telepathy_set_vcard (GossipProtocol       *protocol,
		     GossipVCard          *vcard,
		     GossipResultCallback  callback,
		     gpointer              user_data,
		     GError              **error)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;
	GossipAvatar        *avatar;

	gossip_debug (DEBUG_DOMAIN, "telepathy_set_vcard");

	telepathy = GOSSIP_TELEPATHY (protocol);
	priv = GET_PRIV (telepathy);

	avatar = gossip_vcard_get_avatar (vcard);

	return gossip_telepathy_contacts_set_avatar (priv->contacts,
						     avatar,
						     callback, user_data);
}

static void
telepathy_avatars_get_requirements (GossipProtocol  *protocol,
				    guint           *min_width,
				    guint           *min_height,
				    guint           *max_width,
				    guint           *max_height,
				    gsize           *max_size,
				    gchar          **format)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	telepathy = GOSSIP_TELEPATHY (protocol);

	priv = GET_PRIV (telepathy);

	gossip_telepathy_contacts_get_avatar_requirements (priv->contacts,
							   min_width, min_height,
							   max_width, max_height,
							   max_size, format);
}

static void
telepathy_change_password (GossipProtocol            *protocol,
			   const gchar               *new_password,
			   GossipResultErrorCallback  callback,
			   gpointer                   user_data)
{
	gossip_debug (DEBUG_DOMAIN, "telepathy_change_password");
	if (callback) {
		callback (GOSSIP_RESULT_ERROR_UNAVAILABLE, NULL, user_data);
	}
}

static void
telepathy_change_password_cancel (GossipProtocol *protocol)
{
	gossip_debug (DEBUG_DOMAIN, "telepathy_change_password_cancel");
}

static GossipContact *
telepathy_contact_find (GossipProtocol *protocol,
			const gchar    *id)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	telepathy = GOSSIP_TELEPATHY (protocol);

	priv = GET_PRIV (telepathy);

	return gossip_telepathy_contacts_find (priv->contacts, id, NULL);
}

static void
telepathy_contact_add (GossipProtocol *protocol,
		       const gchar    *id,
		       const gchar    *name,
		       const gchar    *group,
		       const gchar    *message)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	telepathy = GOSSIP_TELEPATHY (protocol);

	priv = GET_PRIV (telepathy);

	gossip_telepathy_contact_list_add (priv->contact_list,
					   id, message);
	/* FIXME: set the contact's alias and group */
}

static void
telepathy_contact_rename (GossipProtocol *protocol,
			  GossipContact  *contact,
			  const gchar    *new_name)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	telepathy = GOSSIP_TELEPATHY (protocol);

	priv = GET_PRIV (telepathy);

	gossip_telepathy_contacts_rename (priv->contacts, contact, new_name);
}

static void
telepathy_contact_remove (GossipProtocol *protocol,
			  GossipContact  *contact)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;
	guint                handle;
	const gchar         *id;

	telepathy = GOSSIP_TELEPATHY (protocol);

	priv = GET_PRIV (telepathy);

	id = gossip_contact_get_id (contact);
	handle = gossip_telepathy_contacts_get_handle (priv->contacts, id);
	gossip_telepathy_contact_list_remove (priv->contact_list, handle);
}

static void
telepathy_contact_update (GossipProtocol *protocol,
			  GossipContact  *contact)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;
	guint                handle;
	GList               *groups;
	const gchar         *id;

	telepathy = GOSSIP_TELEPATHY (protocol);
	priv = GET_PRIV (telepathy);

	id = gossip_contact_get_id (contact);
	handle = gossip_telepathy_contacts_get_handle (priv->contacts, id);
	groups = gossip_contact_get_groups (contact);

	gossip_telepathy_contact_list_contact_update (priv->contact_list,
						      handle, groups);
}

static void
telepathy_group_rename (GossipProtocol *protocol,
			const gchar    *group,
			const gchar    *new_name)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	telepathy = GOSSIP_TELEPATHY (protocol);
	priv = GET_PRIV (telepathy);

	gossip_telepathy_contact_list_rename_group (priv->contact_list,
						    group, new_name);
}

static const GList *
telepathy_get_contacts (GossipProtocol *protocol)
{
	gossip_debug (DEBUG_DOMAIN, "telepathy_get_contacts");

	return NULL;
}

static GossipContact *
telepathy_get_own_contact (GossipProtocol *protocol)
{
	GossipTelepathy *telepathy;

	gossip_debug (DEBUG_DOMAIN, "telepathy_get_own_contact");

	telepathy = GOSSIP_TELEPATHY (protocol);

	return gossip_telepathy_get_own_contact (telepathy);
}

static const gchar *
telepathy_get_active_resource (GossipProtocol *protocol,
			       GossipContact  *contact)
{
	gossip_debug (DEBUG_DOMAIN, "telepathy_get_active_resource");

	return NULL;
}

static gboolean
telepathy_get_vcard (GossipProtocol       *protocol,
		     GossipContact        *contact,
		     GossipVCardCallback   callback,
		     gpointer              user_data,
		     GError              **error)
{
	gossip_debug (DEBUG_DOMAIN, "telepathy_get_vcard");

	if (callback) {
		callback (GOSSIP_RESULT_ERROR_UNAVAILABLE, NULL, user_data);
	}

	return TRUE;
}

static gboolean
telepathy_get_version (GossipProtocol         *protocol,
		       GossipContact          *contact,
		       GossipVersionCallback   callback,
		       gpointer                user_data,
		       GError                **error)
{
	gossip_debug (DEBUG_DOMAIN, "telepathy_get_version");

	if (callback) {
		callback (GOSSIP_RESULT_ERROR_UNAVAILABLE, NULL, user_data);
	}

	return TRUE;
}

static GList *
telepathy_get_groups (GossipProtocol *protocol)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	telepathy = GOSSIP_TELEPATHY (protocol);
	priv = GET_PRIV (telepathy);

	return gossip_telepathy_contact_list_get_groups (priv->contact_list);
}

/*
 * Telepathy specifics
 */

static void
telepathy_newchannel_cb (DBusGProxy      *proxy,
			 const char      *object_path,
			 const char      *channel_type,
			 guint            handle_type,
			 guint            channel_handle,
			 gboolean         suppress_handle,
			 GossipTelepathy *telepathy)
{
	GossipTelepathyPriv *priv;
	TpChan              *new_chan;
	const gchar         *bus_name;

	gossip_debug (DEBUG_DOMAIN,
		      "telepathy_newchannel_cb, suppress_handle: %d",
		      suppress_handle);

	/* If we created the channel somewhere else, we should deal with it there */
	if (suppress_handle) {
		return;
	}

	priv = GET_PRIV (telepathy);

	bus_name = dbus_g_proxy_get_bus_name (DBUS_G_PROXY (priv->tp_conn));
	new_chan = tp_chan_new (tp_get_bus (),
				bus_name,
				object_path,
				channel_type, handle_type, channel_handle);

	gossip_debug (DEBUG_DOMAIN, "telepathy_newchannel_cb: handle: %d handle_type: %d type: %s",
		      new_chan->handle,
		      new_chan->handle_type,
		      channel_type);

	if (strcmp (channel_type, TP_IFACE_CHANNEL_TYPE_CONTACT_LIST) == 0) {
		gossip_telepathy_contact_list_newchannel (priv->contact_list,
							  new_chan);
	}
	else if (strcmp (channel_type, TP_IFACE_CHANNEL_TYPE_TEXT) == 0 &&
		 handle_type == TP_CONN_HANDLE_TYPE_CONTACT)
	{
		gossip_telepathy_message_newchannel (priv->message,
						     new_chan,
						     new_chan->handle);
	} else {
		/* TODO: This should be re-examined soon.
		   We will need to provide all other channels to external handlers
		*/
		gossip_debug (DEBUG_DOMAIN, "Channel type unsupported");
	}
	g_object_unref (new_chan);
}

/*
 * Chatrooms
 */

static void
telepathy_chatroom_init (GossipChatroomProviderIface *iface)
{
	iface->join           = telepathy_chatroom_join;
	iface->cancel         = telepathy_chatroom_cancel;
	iface->send           = telepathy_chatroom_send;
	iface->change_topic   = telepathy_chatroom_change_topic;
	iface->change_nick    = telepathy_chatroom_change_nick;
	iface->leave          = telepathy_chatroom_leave;
	iface->find           = telepathy_chatroom_find;
	iface->invite         = telepathy_chatroom_invite;
	iface->invite_accept  = telepathy_chatroom_invite_accept;
	iface->invite_decline = telepathy_chatroom_invite_decline;
	iface->get_rooms      = telepathy_chatroom_get_rooms;
	iface->browse_rooms   = telepathy_chatroom_browse_rooms;
}

static GossipChatroomId
telepathy_chatroom_join (GossipChatroomProvider *provider,
			 GossipChatroom         *chatroom,
			 GossipChatroomJoinCb    callback,
			 gpointer                user_data)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY (provider), 0);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), 0);

	telepathy = GOSSIP_TELEPATHY (provider);
	priv = GET_PRIV (telepathy);

	return gossip_telepathy_chatrooms_join (priv->chatrooms,
						chatroom,
						callback,
						user_data);
}

static void
telepathy_chatroom_cancel (GossipChatroomProvider *provider,
			   GossipChatroomId        id)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	g_return_if_fail (GOSSIP_IS_TELEPATHY (provider));
	g_return_if_fail (id >= 1);

	telepathy = GOSSIP_TELEPATHY (provider);
	priv = GET_PRIV (telepathy);

	return gossip_telepathy_chatrooms_cancel (priv->chatrooms, id);
}

static void
telepathy_chatroom_send (GossipChatroomProvider *provider,
			 GossipChatroomId        id,
			 const gchar            *message)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	g_return_if_fail (GOSSIP_IS_TELEPATHY (provider));

	telepathy = GOSSIP_TELEPATHY (provider);
	priv = GET_PRIV (telepathy);

	gossip_telepathy_chatrooms_send (priv->chatrooms, id, message);
}

static void
telepathy_chatroom_change_topic (GossipChatroomProvider *provider,
				 GossipChatroomId        id,
				 const gchar            *new_topic)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	g_return_if_fail (GOSSIP_IS_TELEPATHY (provider));

	telepathy = GOSSIP_TELEPATHY (provider);
	priv = GET_PRIV (telepathy);

	gossip_telepathy_chatrooms_change_topic (priv->chatrooms, id, new_topic);
}

static void
telepathy_chatroom_change_nick (GossipChatroomProvider *provider,
				GossipChatroomId        id,
				const gchar            *new_nick)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	g_return_if_fail (GOSSIP_IS_TELEPATHY (provider));

	telepathy = GOSSIP_TELEPATHY (provider);
	priv = GET_PRIV (telepathy);

	gossip_telepathy_chatrooms_change_nick (priv->chatrooms, id, new_nick);
}

static void
telepathy_chatroom_leave (GossipChatroomProvider *provider,
			  GossipChatroomId        id)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	g_return_if_fail (GOSSIP_IS_TELEPATHY (provider));

	telepathy = GOSSIP_TELEPATHY (provider);
	priv = GET_PRIV (telepathy);

	gossip_telepathy_chatrooms_leave (priv->chatrooms, id);
}

static GossipChatroom *
telepathy_chatroom_find (GossipChatroomProvider *provider,
			 GossipChatroom         *chatroom)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY (provider), NULL);

	telepathy = GOSSIP_TELEPATHY (provider);
	priv = GET_PRIV (telepathy);

	return gossip_telepathy_chatrooms_find (priv->chatrooms, chatroom);
}

static void
telepathy_chatroom_invite (GossipChatroomProvider *provider,
			   GossipChatroomId        id,
			   GossipContact          *contact,
			   const gchar            *reason)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	g_return_if_fail (GOSSIP_IS_TELEPATHY (provider));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	telepathy = GOSSIP_TELEPATHY (provider);
	priv = GET_PRIV (telepathy);

	gossip_telepathy_chatrooms_invite (priv->chatrooms, id, contact, reason);
}

static void
telepathy_chatroom_invite_accept (GossipChatroomProvider *provider,
				  GossipChatroomJoinCb    callback,
				  GossipChatroomInvite   *invite,
				  const gchar            *nickname)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	g_return_if_fail (GOSSIP_IS_TELEPATHY (provider));

	telepathy = GOSSIP_TELEPATHY (provider);
	priv = GET_PRIV (telepathy);

	gossip_telepathy_chatrooms_invite_accept (priv->chatrooms,
						  callback,
						  invite,
						  nickname);
}

static void
telepathy_chatroom_invite_decline (GossipChatroomProvider *provider,
				   GossipChatroomInvite   *invite,
				   const gchar            *reason)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	g_return_if_fail (GOSSIP_IS_TELEPATHY (provider));

	telepathy = GOSSIP_TELEPATHY (provider);
	priv = GET_PRIV (telepathy);

	gossip_telepathy_chatrooms_invite_decline (priv->chatrooms,
						   invite,
						   reason);
}

static GList *
telepathy_chatroom_get_rooms (GossipChatroomProvider *provider)
{
	GossipTelepathy     *telepathy;
	GossipTelepathyPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY (provider), NULL);

	telepathy = GOSSIP_TELEPATHY (provider);
	priv = GET_PRIV (telepathy);

	return gossip_telepathy_chatrooms_get_rooms (priv->chatrooms);
}

static void
telepathy_chatroom_browse_rooms (GossipChatroomProvider *provider,
				 const gchar            *server,
				 GossipChatroomBrowseCb  callback,
				 gpointer                user_data)
{
	gossip_debug (DEBUG_DOMAIN, "telepathy_chatroom_browse_rooms");
}

/*
 * File transfer
 */

static void
telepathy_ft_init (GossipFTProviderIface *iface)
{
	iface->send = telepathy_ft_send;
	iface->cancel = telepathy_ft_cancel;
	iface->accept = telepathy_ft_accept;
	iface->decline = telepathy_ft_decline;
}

static GossipFTId
telepathy_ft_send (GossipFTProvider *provider,
		   GossipContact    *contact,
		   const gchar      *file)
{
	gossip_debug (DEBUG_DOMAIN, "telepathy_ft_send");

	return 0;
}

static void
telepathy_ft_cancel (GossipFTProvider *provider,
		     GossipFTId        id)
{
	gossip_debug (DEBUG_DOMAIN, "telepathy_ft_cancel");
}

static void
telepathy_ft_accept (GossipFTProvider *provider,
		     GossipFTId        id)
{
	gossip_debug (DEBUG_DOMAIN, "telepathy_ft_accept");
}

static void
telepathy_ft_decline (GossipFTProvider *provider,
		      GossipFTId        id)
{
	gossip_debug (DEBUG_DOMAIN, "telepathy_ft_decline");
}

/*
 * External functions
 */

GossipAccount *
gossip_telepathy_get_account (GossipTelepathy * telepathy)
{
	GossipTelepathyPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY (telepathy), NULL);

	priv = GET_PRIV (telepathy);

	return priv->account;
}

TpConn *
gossip_telepathy_get_connection (GossipTelepathy     *telepathy)
{
	GossipTelepathyPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY (telepathy), NULL);

	priv = GET_PRIV (telepathy);

	return priv->tp_conn;
}

GossipContact *
gossip_telepathy_get_own_contact (GossipTelepathy *telepathy)
{
	GossipTelepathyPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY (telepathy), NULL);

	gossip_debug (DEBUG_DOMAIN, "gossip_telepathy_get_own_contact");

	priv = GET_PRIV (telepathy);

	return priv->contact;
}

GossipTelepathyContacts *
gossip_telepathy_get_contacts (GossipTelepathy *telepathy)
{
	GossipTelepathyPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY (telepathy), NULL);

	priv = GET_PRIV (telepathy);

	return priv->contacts;
}

