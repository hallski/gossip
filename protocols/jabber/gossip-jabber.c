/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2005 Imendio AB
 * Copyright (C) 2004 Martyn Russell <mr@gnome.org>
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

/** FIXMES
 *
 *  Need to keep an internal list of resources a contact is logged in from
 *  so that we know where to send messages...
 *
 **/

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gconf/gconf-client.h>
#include <glib/gi18n.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-vcard.h>
#include <libgossip/gossip-utils.h>
#include <libgossip/gossip-chatroom-provider.h>

#include "gossip-jabber-chatrooms.h"
#include "gossip-jabber-helper.h"
#include "gossip-transport-accounts.h"
#include "gossip-jabber-private.h"
#include "gossip-jabber.h"

#define d(x) 

#define XMPP_VERSION_XMLNS "jabber:iq:version"
#define XMPP_ROSTER_XMLNS  "jabber:iq:roster"
#define XMPP_REGISTER_XMLNS "jabber:iq:register"


struct _GossipJabberPriv {
	LmConnection          *connection;
	
	GossipContact         *contact;
	
	/* Replace this */
	GossipAccount         *account;
	GossipPresence        *presence;

	GossipJabberChatrooms *chatrooms;

	GHashTable            *contacts;

	/* transport stuff... is this in the right place? */
	GossipTransportAccountList *account_list;

	LmMessageHandler      *subscription_handler;
};

typedef struct {
	LmConnection          *connection;
	LmMessageHandler      *message_handler;

	GossipAccount         *account;

	GossipAsyncRegisterCallback callback;
	gpointer               user_data;
} RegisterAccountData;


typedef struct {
	GossipJabber          *jabber;
	gchar                 *group;
	gchar                 *new_name;
} RenameGroupData;


static void            gossip_jabber_class_init            (GossipJabberClass            *klass);
static void            gossip_jabber_init                  (GossipJabber                 *jabber);
static void            jabber_finalize                     (GObject                      *obj);
static void            jabber_setup                        (GossipProtocol               *protocol,
							    GossipAccount                *account);
static void            jabber_login                        (GossipProtocol               *protocol);
static void            jabber_logout                       (GossipProtocol               *protocol);
static gboolean        jabber_logout_contact_foreach       (gpointer                      key,
							    GossipContact                *contact,
							    GossipJabber                 *jabber);
static gboolean        jabber_register                     (GossipProtocol               *protocol,
							    GossipAccount                *account,
							    GossipAsyncRegisterCallback   callback,
							    gpointer                      user_data,
							    GError                      **error);
static void            jabber_register_connection_open_cb  (LmConnection                 *connection,
							    gboolean                      result,
							    RegisterAccountData          *ra);
static const gchar *   jabber_register_error_to_str        (gint                          error_code);
static LmHandlerResult jabber_register_message_handler     (LmMessageHandler             *handler,
							    LmConnection                 *conn,
							    LmMessage                    *m,
							    RegisterAccountData          *ra);
static gboolean        jabber_is_connected                 (GossipProtocol               *protocol);
static void            jabber_contact_set_subscription     (GossipProtocol               *protocol,
							    GossipContact                *contact,
							    gboolean                      subscribed);
static void            jabber_send_message                 (GossipProtocol               *protocol,
							    GossipMessage                *message);
static void            jabber_set_presence                 (GossipProtocol               *protocol,
							    GossipPresence               *presence);
static GossipContact * jabber_find_contact                 (GossipProtocol               *protocol,
							    const gchar                  *id);
static void            jabber_add_contact                  (GossipProtocol               *protocol,
							    const gchar                  *id,
							    const gchar                  *name,
							    const gchar                  *group,
							    const gchar                  *message);
static void            jabber_rename_contact               (GossipProtocol               *protocol,
							    GossipContact                *contact,
							    const gchar                  *new_name);
static void            jabber_remove_contact               (GossipProtocol               *protocol,
							    GossipContact                *contact);
static void            jabber_update_contact               (GossipProtocol               *protocol,
							    GossipContact                *contact);
static void            jabber_rename_group                 (GossipProtocol               *protocol,
							    const gchar                  *group,
							    const gchar                  *new_name);
static void            jabber_rename_group_foreach_cb      (const gchar                  *jid,
							    GossipContact                *contact,
							    RenameGroupData              *rg);
static const GList *   jabber_get_contacts                 (GossipProtocol               *protocol);
static const gchar *   jabber_get_active_resource          (GossipProtocol               *protocol,
							    GossipContact                *contact);
static GList *         jabber_get_groups                   (GossipProtocol               *protocol);
static void            jabber_get_groups_foreach_cb        (const gchar                  *jid,
							    GossipContact                *contact,
							    GList                       **list);
static gboolean        jabber_async_get_vcard              (GossipProtocol               *protocol,
							    GossipContact                *contact,
							    GossipAsyncVCardCallback      callback,
							    gpointer                      user_data,
							    GError                      **error);
static gboolean        jabber_async_set_vcard              (GossipProtocol               *protocol,
							    GossipVCard                  *vcard,
							    GossipAsyncResultCallback     callback,
							    gpointer                      user_data,
							    GError                      **error);
static gboolean        jabber_async_get_version            (GossipProtocol               *protocol,
							    GossipContact                *contact,
							    GossipAsyncVersionCallback    callback,
							    gpointer                      user_data,
							    GError                      **error);
static void            jabber_connection_open_cb           (LmConnection                 *connection,
							    gboolean                      result,
							    GossipJabber                 *jabber);
static void            jabber_connection_auth_cb           (LmConnection                 *connection,
							    gboolean                      result,
							    GossipJabber                 *jabber);
static LmSSLResponse   jabber_ssl_func                     (LmConnection                 *connection,
							    LmSSLStatus                   status,
							    GossipJabber                 *jabber);
static void            jabber_disconnect_func              (LmConnection                 *connection,
							    LmDisconnectReason            reason,
							    GossipJabber                 *jabber);
static LmHandlerResult jabber_message_handler              (LmMessageHandler             *handler,
							    LmConnection                 *conn,
							    LmMessage                    *message,
							    GossipJabber                 *jabber);
static LmHandlerResult jabber_presence_handler             (LmMessageHandler             *handler,
							    LmConnection                 *conn,
							    LmMessage                    *message,
							    GossipJabber                 *jabber);
static LmHandlerResult jabber_iq_handler                   (LmMessageHandler             *handler,
							    LmConnection                 *conn,
							    LmMessage                    *message,
							    GossipJabber                 *jabber);
static GossipPresence *jabber_get_presence                 (LmMessage                    *message);
static GossipContact * jabber_get_contact_from_jid         (GossipJabber                 *jabber,
							    const gchar                  *jid,
							    gboolean                     *new_item);
static void            jabber_set_proxy                    (LmConnection                 *conn);
static LmHandlerResult jabber_subscription_message_handler (LmMessageHandler             *handler,
							    LmConnection                 *connection,
							    LmMessage                    *m,
							    GossipJabber                 *jabber);
static void            jabber_chatroom_init                (GossipChatroomProviderIface  *iface);
static void            jabber_chatroom_join                (GossipChatroomProvider       *provider,
							    const gchar                  *room,
							    const gchar                  *server,
							    const gchar                  *nick,
							    const gchar                  *password,
							    GossipJoinChatroomCb          callback,
							    gpointer                      user_data);
static void            jabber_chatroom_send                (GossipChatroomProvider       *provider,
							    GossipChatroomId              id,
							    const gchar                  *message);
static void            jabber_chatroom_set_title           (GossipChatroomProvider       *provider,
							    GossipChatroomId              id,
							    const gchar                  *new_title);
static void            jabber_chatroom_change_nick         (GossipChatroomProvider       *provider,
							    GossipChatroomId              id,
							    const gchar                  *new_nick);
static void            jabber_chatroom_leave               (GossipChatroomProvider       *provider,
							    GossipChatroomId              id);
static const gchar *   jabber_chatroom_get_room_name       (GossipChatroomProvider       *provider,
							    GossipChatroomId              id);
static void            jabber_signal_logged_out            (GossipJabber                 *jabber);
static void            jabber_version_request              (GossipJabber                 *jabber,
							    LmMessage                    *m);

extern GConfClient *gconf_client;

G_DEFINE_TYPE_WITH_CODE (GossipJabber, gossip_jabber, GOSSIP_TYPE_PROTOCOL,
			 G_IMPLEMENT_INTERFACE (GOSSIP_TYPE_CHATROOM_PROVIDER,
						jabber_chatroom_init));

static void
gossip_jabber_class_init (GossipJabberClass *klass)
{
	GObjectClass        *object_class = G_OBJECT_CLASS (klass);
	GossipProtocolClass *protocol_class = GOSSIP_PROTOCOL_CLASS (klass);
	
	object_class->finalize = jabber_finalize;

	protocol_class->setup               = jabber_setup;
	protocol_class->login               = jabber_login;
	protocol_class->logout              = jabber_logout;
	protocol_class->async_register      = jabber_register;
	protocol_class->is_connected        = jabber_is_connected;
	protocol_class->contact_set_subscription  = jabber_contact_set_subscription;
	protocol_class->send_message        = jabber_send_message;
	protocol_class->set_presence        = jabber_set_presence;
        protocol_class->find_contact        = jabber_find_contact;
        protocol_class->add_contact         = jabber_add_contact;
        protocol_class->rename_contact      = jabber_rename_contact;
        protocol_class->remove_contact      = jabber_remove_contact;
	protocol_class->update_contact      = jabber_update_contact;
 	protocol_class->rename_group        = jabber_rename_group;
	protocol_class->get_contacts        = jabber_get_contacts;
	protocol_class->get_active_resource = jabber_get_active_resource;
 	protocol_class->get_groups          = jabber_get_groups;
	protocol_class->async_get_vcard     = jabber_async_get_vcard;
	protocol_class->async_set_vcard     = jabber_async_set_vcard;
	protocol_class->async_get_version   = jabber_async_get_version;

}

static void
gossip_jabber_init (GossipJabber *jabber)
{
	GossipJabberPriv *priv;

	priv = g_new0 (GossipJabberPriv, 1);
	jabber->priv = priv;

	priv->contacts = 
		g_hash_table_new_full (g_str_hash,
				       g_str_equal,
				       (GDestroyNotify) g_free,
				       (GDestroyNotify) g_object_unref);
}
	
static void
jabber_finalize (GObject *obj)
{
 	GossipJabber     *jabber;
 	GossipJabberPriv *priv;

 	jabber = GOSSIP_JABBER (obj);
 	priv   = jabber->priv;

 	gossip_account_unref (priv->account);
	
 	g_hash_table_destroy (priv->contacts);
 	gossip_jabber_chatrooms_free (priv->chatrooms);
	
 	gossip_transport_account_list_free (priv->account_list);
	
 	g_free (priv);
}

static void
jabber_setup (GossipProtocol *protocol,
 	      GossipAccount  *account)
{
 	GossipJabber     *jabber;
 	GossipJabberPriv *priv;
 	LmMessageHandler *handler;
 	gchar            *id;
	
 	g_return_if_fail (GOSSIP_IS_JABBER (protocol));
  	g_return_if_fail (account != NULL);
	
 	jabber = GOSSIP_JABBER (protocol);
 	priv   = jabber->priv;
 	
 	priv->account = gossip_account_ref (account);
 
 	priv->contact = gossip_contact_new (GOSSIP_CONTACT_TYPE_USER);

	id = g_strdup_printf ("%s@%s", 
			      priv->account->username, priv->account->host);
	g_object_set (priv->contact,
		      "id", id,
		      "name", priv->account->username,
		      NULL);

	priv->connection = lm_connection_new (priv->account->server);

	/* setup the connection to send keep alive messages every 30 seconds */
        lm_connection_set_keep_alive_rate (priv->connection, 30);

	lm_connection_set_disconnect_function (priv->connection,
					       (LmDisconnectFunction) jabber_disconnect_func,
					       jabber, NULL);

	lm_connection_set_port (priv->connection, priv->account->port);
	lm_connection_set_jid (priv->connection, id);
	g_free (id);

	handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_message_handler,
					  jabber, NULL);
	lm_connection_register_message_handler (priv->connection,
						handler,
						LM_MESSAGE_TYPE_MESSAGE,
						LM_HANDLER_PRIORITY_FIRST);
	lm_message_handler_unref (handler);

	handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_presence_handler,
					  jabber, NULL);
	lm_connection_register_message_handler (priv->connection,
						handler,
						LM_MESSAGE_TYPE_PRESENCE,
						LM_HANDLER_PRIORITY_FIRST);
	lm_message_handler_unref (handler);

	handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_iq_handler,
					  jabber, NULL);
	lm_connection_register_message_handler (priv->connection,
						handler,
						LM_MESSAGE_TYPE_IQ,
						LM_HANDLER_PRIORITY_FIRST);
	lm_message_handler_unref (handler);

	priv->chatrooms = gossip_jabber_chatrooms_new (jabber, 
						       priv->connection);

	/* initialise the jabber accounts module which is necessary to
	   watch roster changes to know which services are set up */
	priv->account_list = gossip_transport_account_list_new (jabber);
}

static void
jabber_login (GossipProtocol *protocol)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;
	GError           *error = NULL;
	gboolean          result;

	g_return_if_fail (GOSSIP_IS_JABBER (protocol));
	
	jabber = GOSSIP_JABBER (protocol);
	priv   = jabber->priv;

	d(g_print ("Protocol: Logging in Jabber\n"));
	
	if (priv->account->use_ssl) {
		LmSSL *ssl = lm_ssl_new (NULL,
					 (LmSSLFunction) jabber_ssl_func,
					 jabber, NULL);
		lm_connection_set_ssl (priv->connection, ssl);
		lm_ssl_unref (ssl);
	}

	if (priv->account->use_proxy) {
		jabber_set_proxy (priv->connection);
	} else {
		/* FIXME: Just pass NULL when Loudmouth > 0.17.1 */
		LmProxy *proxy;

		proxy = lm_proxy_new (LM_PROXY_TYPE_NONE);
		lm_connection_set_proxy (priv->connection, proxy);
		lm_proxy_unref (proxy);
	}

	if (!priv->connection) {
		gossip_protocol_error (GOSSIP_PROTOCOL (jabber), 
				       GossipProtocolErrorNoConnection,
				       _("Could not open connection"));
		return;
	}

	priv->presence = gossip_presence_new ();
	gossip_presence_set_state (priv->presence, 
				   GOSSIP_PRESENCE_STATE_AVAILABLE);
	gossip_jabber_chatrooms_set_presence (priv->chatrooms, priv->presence);

	result = lm_connection_open (priv->connection, 
				     (LmResultFunction) jabber_connection_open_cb,
				     jabber, NULL, &error);

	if (result && !error) {
		return;
	}

	if (error->code == 1 && 
	    strcmp (error->message, "getaddrinfo() failed") == 0) {
		/* host lookup failed */
		gossip_protocol_error (GOSSIP_PROTOCOL (jabber), 
				       GossipProtocolErrorNoSuchHost,
				       _("Could not find the server you wanted to use"));
	}

	g_error_free (error);
}

static void
jabber_logout (GossipProtocol *protocol)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (protocol));

	jabber = GOSSIP_JABBER (protocol);
	priv   = jabber->priv;

	if (priv->connection && 
	    lm_connection_is_open (priv->connection)) {
		lm_connection_close (priv->connection, NULL);
	}
}

static gboolean
jabber_logout_contact_foreach (gpointer       key,
			       GossipContact *contact,
			       GossipJabber  *jabber)
{
	g_signal_emit_by_name (jabber, "contact-removed", contact);
	return TRUE;
}

static gboolean            
jabber_register (GossipProtocol               *protocol,
		 GossipAccount                *account,
		 GossipAsyncRegisterCallback   callback,
		 gpointer                      user_data,
		 GError                      **error)
{
	GossipJabber         *jabber;
	GossipJabberPriv     *priv;
	RegisterAccountData  *ra;
	gboolean              result;

	g_return_val_if_fail (GOSSIP_IS_JABBER (protocol), FALSE);
	g_return_val_if_fail (callback != NULL, FALSE);

	jabber = GOSSIP_JABBER (protocol);
	priv   = jabber->priv;

	ra = g_new0 (RegisterAccountData, 1);
	
	ra->connection = lm_connection_new (account->server);
	ra->account = gossip_account_ref (account);

	ra->callback = callback;
	ra->user_data = user_data;

	d(g_print ("Protocol: Registering with Jabber server...\n"));

	if (account->use_ssl) {
		LmSSL *ssl = lm_ssl_new (NULL,
					 (LmSSLFunction) jabber_ssl_func,
					 jabber, NULL);
		lm_connection_set_ssl (ra->connection, ssl);
		lm_ssl_unref (ssl);
	}

	if (account->use_proxy) {
		jabber_set_proxy (ra->connection);
	} else {
		/* FIXME: Just pass NULL when Loudmouth > 0.17.1 */
		LmProxy *proxy;

		proxy = lm_proxy_new (LM_PROXY_TYPE_NONE);
		lm_connection_set_proxy (ra->connection, proxy);
		lm_proxy_unref (proxy);
	}

	if (ra->connection == NULL) {
		if (error) {
			*error = g_error_new_literal (g_quark_from_string ("gossip-jabber"),
						      0, _("Connection could not be created"));
		}

		gossip_account_unref (account);
		g_free (ra);

		d(g_print ("Protocol: Connection could not be created\n"));
		return FALSE;
	}

	result = lm_connection_open (ra->connection, 
				     (LmResultFunction) jabber_register_connection_open_cb,
				     ra, NULL, error);

	if (!result) {
		gossip_account_unref (ra->account);
		g_free (ra);

		d(g_print ("Protocol: Connection could not be opened\n"));
		return FALSE;
	}

	return TRUE;
}

static void
jabber_register_connection_open_cb (LmConnection        *connection,
				    gboolean             result,
				    RegisterAccountData *ra)
{
	LmMessage        *m;
	LmMessageNode    *node;
	const gchar      *str = NULL;
	gboolean          ok = FALSE;
	
	if (result == FALSE) {
		str = _("Connection could not be opened");
		d(g_print ("Protocol: %s\n", str));

		if (ra->callback) {
			(ra->callback) (GOSSIP_ASYNC_ERROR_REGISTRATION, 
					str, ra->user_data);
		}

		gossip_account_unref (ra->account);
		g_free (ra);

		return;
	} else {
		d(g_print ("Protocol: Connection open!\n"));
	}

	ra->message_handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_register_message_handler,
						      ra, NULL);

        m = lm_message_new_with_sub_type (ra->account->server,
                                          LM_MESSAGE_TYPE_IQ,
                                          LM_MESSAGE_SUB_TYPE_SET);

	lm_message_node_add_child (m->node, "query", NULL);
	node = lm_message_node_get_child (m->node, "query");

        lm_message_node_set_attribute (node, "xmlns", XMPP_REGISTER_XMLNS);
	
	lm_message_node_add_child (node, "username", ra->account->username);
	lm_message_node_add_child (node, "password", ra->account->password);

        ok = lm_connection_send_with_reply (ra->connection, m, 
					    ra->message_handler, NULL);
        lm_message_unref (m);

	if (!ok) {
		str = _("Couldn't send message!");
		d(g_print ("Protocol: %s\n", str));

		if (ra->callback) {
			(ra->callback) (GOSSIP_ASYNC_ERROR_REGISTRATION, 
					str, ra->user_data);
		}

		gossip_account_unref (ra->account);
		g_free (ra);
	} else {
		d(g_print ("Protocol: Sent registration details\n"));
	}
}

static const gchar *
jabber_register_error_to_str (gint error_code) 
{
	switch (error_code) {
	case 302: return _("Redirect");
	case 400: return _("Bad Request");
	case 401: return _("Not Authorized");
	case 402: return _("Payment Required");
	case 403: return _("Forbidden");
	case 404: return _("Not Found");
	case 405: return _("Not Allowed");
	case 406: return _("Not Acceptable");
	case 407: return _("Registration Required");
	case 408: return _("Request Timeout");
	case 409: return _("Conflict");
	case 500: return _("Internal Server Error");
	case 501: return _("Not Implemented");
	case 502: return _("Remote Server Error");
	case 503: return _("Service Unavailable");
	case 504: return _("Remote Server Timeout");
	case 510: return _("Disconnected");
	};

	return _("Unknown");
}

static LmHandlerResult
jabber_register_message_handler (LmMessageHandler     *handler,
				 LmConnection         *conn,
				 LmMessage            *m,
				 RegisterAccountData  *ra)
{
	GossipAsyncResult  result = GOSSIP_ASYNC_OK;

	LmMessageNode     *node;
	const gchar       *error_code = NULL;
	const gchar       *error_reason = NULL;

	node = lm_message_node_get_child (m->node, "error");	
	if (node) {
		result = GOSSIP_ASYNC_ERROR_REGISTRATION;

		error_code = lm_message_node_get_attribute (node, "code");
		error_reason = jabber_register_error_to_str (atoi (error_code));

		d(g_print ("Protocol: Registration failed with error:%s->'%s'\n",
			   error_code, error_reason));
	} else {
		d(g_print ("Protocol: Registration success\n"));
	}

	if (ra->callback) {
		(ra->callback) (result, 
				error_reason, ra->user_data);
	}
	
	lm_connection_close (ra->connection, NULL);
	
	gossip_account_unref (ra->account);
	g_free (ra);
	
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static gboolean
jabber_is_connected (GossipProtocol *protocol)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_JABBER (protocol), FALSE);

	jabber = GOSSIP_JABBER (protocol);
	priv   = jabber->priv;

	if (priv->connection == NULL) {
		return FALSE;
	}

	return lm_connection_is_authenticated (priv->connection);
}

static void
jabber_contact_set_subscription (GossipProtocol *protocol,
				 GossipContact  *contact,
				 gboolean        subscribed)
{
	GossipJabber     *jabber;

	g_return_if_fail (GOSSIP_IS_JABBER (protocol));

	jabber = GOSSIP_JABBER (protocol);

	if (subscribed) {
		gossip_jabber_send_subscribed (jabber, contact);
	} else {
		gossip_jabber_send_unsubscribed (jabber, contact);
	}
}

static void
jabber_send_message (GossipProtocol *protocol, GossipMessage *message)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;
	GossipContact    *recipient;
	LmMessage        *m;
	gboolean          result;
	GError           *error = NULL;
	const gchar      *id;
	const gchar      *resource;
	gchar            *jid_str;

	g_return_if_fail (GOSSIP_IS_JABBER (protocol));

	jabber = GOSSIP_JABBER (protocol);
	priv   = jabber->priv;

	recipient = gossip_message_get_recipient (message);

	/* FIXME: Create a full JID (with resource) and send to that */
	id = gossip_contact_get_id (recipient);
	resource = gossip_message_get_explicit_resource (message);

	if (resource) {
		jid_str = g_strdup_printf ("%s/%s", id, resource);
	} else {
		jid_str = (gchar *) id;
	}

	d(g_print ("Protocol: Sending message to: '%s'\n", jid_str));
	
	m = lm_message_new_with_sub_type (jid_str,
					  LM_MESSAGE_TYPE_MESSAGE,
					  LM_MESSAGE_SUB_TYPE_CHAT);
	if (jid_str != id) {
		g_free (jid_str);
	}

	lm_message_node_add_child (m->node, "body",
				   gossip_message_get_body (message));
	result = lm_connection_send (priv->connection, m, &error);
	lm_message_unref (m);

	if (!result) {
		g_error ("lm_connection_send failed");
	}
}

static void
jabber_set_presence (GossipProtocol *protocol, GossipPresence *presence)
{
	GossipJabber        *jabber;
	GossipJabberPriv    *priv;
	LmMessage           *m;
	GossipPresenceState  state;
	const gchar         *show = NULL;
	const gchar         *priority;
	
	jabber = GOSSIP_JABBER (protocol);
	priv = jabber->priv;

	if (priv->presence) {
		g_object_unref (priv->presence);
	}
	priv->presence = g_object_ref (presence);
	
	m = lm_message_new_with_sub_type (NULL, 
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_AVAILABLE);

	state = gossip_presence_get_state (presence);
	show = gossip_jabber_helper_presence_state_to_string (presence);
	
	switch (state) {
	case GOSSIP_PRESENCE_STATE_BUSY:
		priority = "40";
		break;
	case GOSSIP_PRESENCE_STATE_AWAY:
		priority = "30";
		break;
	case GOSSIP_PRESENCE_STATE_EXT_AWAY:
		priority = "0";
		break;
	default:
		priority = "50";
		break;
	}

	if (show) {
		lm_message_node_add_child (m->node, "show", show);
	}
	lm_message_node_add_child (m->node, "priority", priority);
	
	if (gossip_presence_get_status (presence)) {
		lm_message_node_add_child (m->node, "status",
					   gossip_presence_get_status (presence));
	}
	
	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);

	gossip_jabber_chatrooms_set_presence (priv->chatrooms, presence);
}

static GossipContact *
jabber_find_contact (GossipProtocol *protocol,
		     const gchar    *id)
{
        GossipJabber     *jabber;
	GossipJabberPriv *priv;
	GossipJID        *jid;
	GossipContact    *contact;

        jabber = GOSSIP_JABBER (protocol);
	priv = jabber->priv;

	jid = gossip_jid_new (id);
	contact =  g_hash_table_lookup (priv->contacts, 
					gossip_jid_get_without_resource (jid));

	gossip_jid_unref (jid);

	return contact;
}

static void
jabber_add_contact (GossipProtocol *protocol,
                    const gchar    *id,
                    const gchar    *name,
                    const gchar    *group,
                    const gchar    *message)
{
        GossipJabber     *jabber;
        GossipJabberPriv *priv;
	LmMessage        *m;
        LmMessageNode    *node;

        jabber = GOSSIP_JABBER (protocol);
        priv   = jabber->priv;

        /* Request subscription */
        m = lm_message_new_with_sub_type (id, LM_MESSAGE_TYPE_PRESENCE,
                                          LM_MESSAGE_SUB_TYPE_SUBSCRIBE);
        lm_message_node_add_child (m->node, "status", message);
        lm_connection_send (priv->connection, m, NULL);
        lm_message_unref (m);

        /* Add to roster */
        m = lm_message_new_with_sub_type (NULL, LM_MESSAGE_TYPE_IQ,
                                          LM_MESSAGE_SUB_TYPE_SET);
        node = lm_message_node_add_child (m->node, "query", NULL);
        lm_message_node_set_attributes (node,
                                        "xmlns", "jabber:iq:roster", NULL);
        node = lm_message_node_add_child (node, "item", NULL);
        lm_message_node_set_attributes (node,
                                        "jid", id,
                                        "subscription", "none",
                                        "ask", "subscribe",
                                        "name", name,
                                        NULL);
        if (group && strcmp (group, "") != 0) {
                lm_message_node_add_child (node, "group", group);
        }

        lm_connection_send (priv->connection, m, NULL);
        lm_message_unref (m);
}

static void
jabber_rename_contact (GossipProtocol *protocol,
                       GossipContact  *contact,
                       const gchar    *new_name)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;
	LmMessage        *m;
        LmMessageNode    *node;
	gchar            *escaped;
	GList            *l; 
	
	jabber = GOSSIP_JABBER (protocol);
	priv = jabber->priv;

	m = lm_message_new_with_sub_type (NULL, 
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_SET);
	node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attributes (node,
					"xmlns", "jabber:iq:roster",
					NULL);
	
	escaped = g_markup_escape_text (new_name, -1);

	node = lm_message_node_add_child (node, "item", NULL);
	lm_message_node_set_attributes (node, 
					"jid", gossip_contact_get_id (contact),
					"name", escaped,
					NULL);
        g_free (escaped);
	
        for (l = gossip_contact_get_groups (contact); l; l = l->next) {
                escaped = g_markup_escape_text ((const gchar *) l->data, -1);
                lm_message_node_add_child (node, "group", escaped);
                g_free (escaped);
	}	

	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);
}

static void
jabber_remove_contact (GossipProtocol *protocol, 
		       GossipContact  *contact)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;
	LmMessage        *m;
        LmMessageNode    *node;
	
	jabber = GOSSIP_JABBER (protocol);
	priv = jabber->priv;

	m = lm_message_new_with_sub_type (NULL,
 					  LM_MESSAGE_TYPE_IQ,
 					  LM_MESSAGE_SUB_TYPE_SET);
	
 	node = lm_message_node_add_child (m->node, "query", NULL);
 	lm_message_node_set_attribute (node,
				       "xmlns",
				       "jabber:iq:roster");
	
 	node = lm_message_node_add_child (node, "item", NULL);
 	lm_message_node_set_attributes (node,
					"jid", gossip_contact_get_id (contact),
					"subscription", "remove",
					NULL);
 	
 	lm_connection_send (priv->connection, m, NULL);
 	lm_message_unref (m);

	m = lm_message_new_with_sub_type (gossip_contact_get_id (contact),
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_UNSUBSCRIBE);
	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);
}

static const GList *
jabber_get_contacts (GossipProtocol *protocol)
{
	/* FIXME: Keep track of own contacts */
	return NULL;
}

static void
jabber_update_contact (GossipProtocol *protocol, 
		       GossipContact  *contact)
{
	/* we set the groups _and_ the name here, the rename function
	   will do exactly what we want to do so just call that */
	jabber_rename_contact (protocol, 
			       contact, 
			       gossip_contact_get_name (contact));
}

static void
jabber_rename_group (GossipProtocol *protocol,
		     const gchar    *group,
		     const gchar    *new_name)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;

	RenameGroupData  *rg;

	jabber = GOSSIP_JABBER (protocol);
	priv = jabber->priv;
	
	rg = g_new0 (RenameGroupData, 1);
	
	rg->jabber = jabber;
	rg->group = g_strdup (group);
	rg->new_name = g_strdup (new_name);

	g_hash_table_foreach (priv->contacts, 
			      (GHFunc)jabber_rename_group_foreach_cb, 
			      rg);

	g_free (rg->group);
	g_free (rg->new_name);
	g_free (rg);
}

static void
jabber_rename_group_foreach_cb (const gchar     *jid,
				GossipContact   *contact,
				RenameGroupData *rg)
{
	GossipJabberPriv *priv;
	LmMessage        *m;
        LmMessageNode    *node;
	gchar            *escaped;
	GList            *l; 
	gboolean          found = FALSE;

	priv = rg->jabber->priv;

        for (l = gossip_contact_get_groups (contact); l && !found; l = l->next) {
		gchar *group = (gchar*)l->data;

		if (group && strcmp (group, rg->group) == 0) {
			found = TRUE;
		}
	}
	
	if (!found) {
		return;
	}

	m = lm_message_new_with_sub_type (NULL, 
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_SET);
	node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attributes (node,
					"xmlns", "jabber:iq:roster",
					NULL);
	
	escaped = g_markup_escape_text (gossip_contact_get_name (contact), -1);
	
	node = lm_message_node_add_child (node, "item", NULL);
	lm_message_node_set_attributes (node, 
					"jid", gossip_contact_get_id (contact),
					"name", escaped,
					NULL);
	g_free (escaped);
	
	for (l = gossip_contact_get_groups (contact); l; l = l->next) {
		const gchar *group = (const gchar*) l->data;
		
		/* do not include the group we are renaming */
		if (group && strcmp (group, rg->group) == 0) {
			continue;
		}

		escaped = g_markup_escape_text (group, -1);
		lm_message_node_add_child (node, "group", escaped);
		g_free (escaped);
	}	

	/* add the new group name */
	escaped = g_markup_escape_text (rg->new_name, -1);
	lm_message_node_add_child (node, "group", escaped);
	g_free (escaped);
	
	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);
}

static const gchar *
jabber_get_active_resource (GossipProtocol *protocol,
			    GossipContact  *contact)
{
	/* FIXME: Get the active resource */
	return NULL;
}

static GList *
jabber_get_groups (GossipProtocol *protocol)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;
	GList            *list = NULL;

	jabber = GOSSIP_JABBER (protocol);
	priv = jabber->priv;
	
	g_hash_table_foreach (priv->contacts, 
			      (GHFunc)jabber_get_groups_foreach_cb, 
			      &list);

	list = g_list_sort (list, (GCompareFunc)strcmp);

	return list;
}

static void
jabber_get_groups_foreach_cb (const gchar    *jid,
			      GossipContact  *contact,
			      GList         **list)
{
	GList *l;

	if (!gossip_contact_get_groups (contact)) {
		return;
	}

	for (l = gossip_contact_get_groups (contact); l; l = l->next) {
		gchar *group;
		GList *found;

		group = (gchar*) l->data;
		found = g_list_find_custom (*list, 
					    group, 
					    (GCompareFunc)strcmp);
		
		if (!found) {
			*list = g_list_prepend (*list, g_strdup (group));
		}
	}
}

static gboolean
jabber_async_get_vcard (GossipProtocol            *protocol,
			GossipContact             *contact,
			GossipAsyncVCardCallback   callback,
			gpointer                   user_data,
			GError                   **error)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv; 
	const gchar      *jid_str;

	jabber = GOSSIP_JABBER (protocol);
	priv   = jabber->priv;
	
	if (contact) {
		jid_str = gossip_contact_get_id (contact);
	} else {
		jid_str = NULL;
	}
	
	return gossip_jabber_helper_async_get_vcard (priv->connection,
						     jid_str,
						     callback, user_data, error);
}

static gboolean
jabber_async_set_vcard (GossipProtocol             *protocol,
			GossipVCard                *vcard,
			GossipAsyncResultCallback   callback,
			gpointer                    user_data,
			GError                    **error)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv; 

	jabber = GOSSIP_JABBER (protocol);
	priv   = jabber->priv;
	
	return gossip_jabber_helper_async_set_vcard (priv->connection,
						     vcard,
						     callback, user_data,
						     error);
}

static gboolean
jabber_async_get_version (GossipProtocol              *protocol,
			  GossipContact               *contact,
			  GossipAsyncVersionCallback   callback,
			  gpointer                     user_data,
			  GError                     **error)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv; 

	jabber = GOSSIP_JABBER (protocol);
	priv   = jabber->priv;
	
	return gossip_jabber_helper_async_get_version (priv->connection,
						       contact,
						       callback, user_data,
						       error);
}

static void
jabber_connection_open_cb (LmConnection *connection,
			   gboolean      result,
			   GossipJabber *jabber)
{
	GossipJabberPriv *priv;
	GossipAccount    *account;
	gchar            *password = NULL;
	
	priv = jabber->priv;

	if (result == FALSE) {
		gossip_protocol_error (GOSSIP_PROTOCOL (jabber), 
				       GossipProtocolErrorNoConnection,
				       _("Could not open connection"));
		return;
	}

	d(g_print ("Protocol: Connection open!\n"));

	account = priv->account;
	if (!account->password || !account->password[0]) {
		/* FIXME: Ask the user for the password */
		g_signal_emit_by_name (jabber, "get-password", 
				       account, &password);
	} else {
		password = g_strdup (account->password);
	}
	 
	/* FIXME: Decide on Resource */
	
	lm_connection_authenticate (priv->connection,
				    account->username, password, 
				    account->resource,
				    (LmResultFunction) jabber_connection_auth_cb,
				    jabber, NULL, NULL);
}

static void
jabber_connection_auth_cb (LmConnection *connection,
			   gboolean      result,
			   GossipJabber *jabber)
{
	GossipJabberPriv *priv;
	LmMessage        *m;
	LmMessageNode    *node;
	
	priv = jabber->priv;
	
	if (result == FALSE) {
		gossip_protocol_error (GOSSIP_PROTOCOL (jabber), 
				       GossipProtocolErrorAuthFailed,
				       _("Authentication failed"));
		return;
	}

	d(g_print ("Protocol: Connection logged in!\n"));

	/* Request roster */
	m = lm_message_new_with_sub_type (NULL,
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_GET);
	node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attributes (node, 
					"xmlns", "jabber:iq:roster",
					NULL);
	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);

	/* Notify others that we are online */
	m = lm_message_new_with_sub_type (NULL,
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_AVAILABLE);
	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);

	g_signal_emit_by_name (jabber, "logged-in");
}

static LmSSLResponse 
jabber_ssl_func (LmConnection *connection,
		 LmSSLStatus   status,
		 GossipJabber *jabber)
{
	return LM_SSL_RESPONSE_CONTINUE;
}

static void 
jabber_disconnect_func (LmConnection       *connection,
			LmDisconnectReason  reason,
			GossipJabber       *jabber)
{
	jabber_signal_logged_out (jabber);
}

static LmHandlerResult
jabber_message_handler (LmMessageHandler *handler,
			LmConnection     *conn,
			LmMessage        *m,
			GossipJabber     *jabber)
{
	GossipJabberPriv *priv;
	GossipMessage    *message;
	const gchar      *from_str;
	GossipContact    *from;
	const gchar      *thread = "";
	const gchar      *body = "";
	LmMessageNode    *node;
	
	priv = jabber->priv;
	
	d(g_print ("Protocol: New message from: %s\n", 
		   lm_message_node_get_attribute (m->node, "from")));

	switch (lm_message_get_sub_type (m)) {
	case LM_MESSAGE_SUB_TYPE_NOT_SET:
	case LM_MESSAGE_SUB_TYPE_NORMAL:
	case LM_MESSAGE_SUB_TYPE_CHAT:
	case LM_MESSAGE_SUB_TYPE_HEADLINE: /* For now, fixes #120009 */
		from_str = lm_message_node_get_attribute (m->node, "from");
		from = jabber_get_contact_from_jid (jabber, from_str, NULL);
		
		message = gossip_message_new (GOSSIP_MESSAGE_TYPE_NORMAL,
					      priv->contact);
		
		gossip_message_set_sender (message, from);

		node = lm_message_node_get_child (m->node, "body");
		if (node) {
			body = node->value;
		}

		node = lm_message_node_get_child (m->node, "thread");
		if (node) {
			thread = node->value;
		}
			
		gossip_message_set_body (message, body);
		gossip_message_set_thread (message, thread);

		gossip_message_set_timestamp (message,
					      gossip_jabber_helper_get_timestamp_from_lm_message (m));
		
		g_signal_emit_by_name (jabber, "new-message", message);

		g_object_unref (message);

		/* FIXME: Set timestamp, composing request */
		/* Do the stuff */
		break;
	case LM_MESSAGE_SUB_TYPE_ERROR:
		/* FIXME: Emit error */
		break;
	default:
		break;
	}

	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static LmHandlerResult
jabber_presence_handler (LmMessageHandler *handler,
			 LmConnection     *conn,
			 LmMessage        *m,
			 GossipJabber     *jabber)
{
	GossipJabberPriv *priv;
	GossipContact    *contact;
	const gchar      *from;
	const gchar      *type;
	gboolean          new_item = FALSE;

	priv = jabber->priv;

	from = lm_message_node_get_attribute (m->node, "from");
        d(g_print ("Protocol: New presence from: %s\n", 
		   lm_message_node_get_attribute (m->node, "from")));

	if (gossip_jabber_chatrooms_get_jid_is_chatroom (priv->chatrooms,
							 from)) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}
	
	contact = jabber_get_contact_from_jid (jabber, from, &new_item); 

	type = lm_message_node_get_attribute (m->node, "type");
	if (!type) {
		type = "available";
	}

	if (strcmp (type, "subscribe") == 0) {
		g_signal_emit_by_name (jabber, "subscription-request", 
				       contact, NULL);

		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	} else if (strcmp (type, "subscribed") == 0) {
		/* Handle this? */
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}
	
	if (contact) {
		GossipPresence *presence;
		GossipJID      *jid;
		const gchar    *resource;
		
		jid = gossip_jid_new (from);
		resource = gossip_jid_get_resource (jid);
		if (!resource) {
			resource = "";
		}

		presence = jabber_get_presence (m);
		if (!presence) {
			presence = gossip_contact_get_presence_for_resource (contact, resource);
			if (presence) {
				gossip_contact_remove_presence (contact,
								presence);
			}
		} else {
			gossip_presence_set_resource (presence, resource);
			gossip_contact_add_presence (contact, presence);

			g_object_unref (presence);
		}
		
		gossip_jid_unref (jid);
	
		g_signal_emit_by_name (jabber,
				       "contact-presence-updated", 
				       contact);
	}
	
	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
/*	return LM_HANDLER_RESULT_REMOVE_MESSAGE; */
}

static LmHandlerResult
jabber_iq_handler (LmMessageHandler *handler,
		   LmConnection     *conn,
		   LmMessage        *m,
		   GossipJabber     *jabber)
{
	GossipJabberPriv *priv;
	LmMessageNode    *node;
	const gchar      *xmlns;
	
	priv = jabber->priv;
	
	if (lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_GET &&
	    lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_SET &&
	    lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_RESULT) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	node = lm_message_node_get_child (m->node, "query");
	if (!node) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	xmlns = lm_message_node_get_attribute (node, "xmlns");
	
	if (xmlns && strcmp (xmlns, XMPP_VERSION_XMLNS) == 0) {
		jabber_version_request (jabber, m);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	if (!xmlns || strcmp (xmlns, XMPP_ROSTER_XMLNS) != 0) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	for (node = node->children; node; node = node->next) {
		GossipContact *contact;
		const gchar   *jid_str;
		const gchar   *subscription;
		gboolean       new_item = FALSE;
		gboolean       updated;
		LmMessageNode *subnode;
		LmMessageNode *child;
		GList         *groups = NULL;

		const gchar   *name;
		GList         *new_groups = NULL;
		gboolean       name_updated, groups_updated;
		

		if (strcmp (node->name, "item") != 0) {
			continue;
		}

		jid_str = lm_message_node_get_attribute (node, "jid");
		if (!jid_str) {
			continue;
		}

		contact = jabber_get_contact_from_jid (jabber, jid_str, &new_item);
		g_object_set (contact, "type", GOSSIP_CONTACT_TYPE_CONTACTLIST, NULL);

		/* groups */
		for (subnode = node->children; subnode; subnode = subnode->next) {
			if (strcmp (subnode->name, "group") != 0) {
				continue;
			}
			
			if (subnode->value) {
				groups = g_list_append (groups, subnode->value);
			}
		}
		
		/* FIXME: why is this here if we set the groups below */
		if (groups) {
			gossip_contact_set_groups (contact, groups);
		}

		/* subscription */
		subscription = lm_message_node_get_attribute (node, "subscription");
		if (contact && subscription && strcmp (subscription, "remove") == 0) {
			g_object_ref (contact);
			g_hash_table_remove (priv->contacts, 
					     gossip_contact_get_id (contact));
			
			g_signal_emit_by_name (jabber,
					       "contact-removed", contact);
			g_object_unref (contact);
			continue;
		}
		
		/* find out if any thing has updated */
		name_updated = groups_updated = FALSE;

		name = lm_message_node_get_attribute (node, "name");
		if (name) {
			name_updated = TRUE;
			gossip_contact_set_name (contact, name);
		}

		for (child = node->children; child; child = child->next) {
			if (strcmp (child->name, "group") == 0 && child->value) {
				new_groups = g_list_append (new_groups, child->value);
			}
		}

		if (new_groups) {
			/* does not get free'd */
			groups_updated = gossip_contact_set_groups (contact, new_groups);
		}

		updated = (name_updated || groups_updated);

		
		if (new_item) {
			g_signal_emit_by_name (jabber, 
					       "contact-added", contact);
		}
		else if (updated) {
			g_signal_emit_by_name (jabber, 
					       "contact-updated", contact);
		}
	}

	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
        /*return LM_HANDLER_RESULT_REMOVE_MESSAGE; */
}

static GossipPresence *
jabber_get_presence (LmMessage *m)
{
	GossipPresence      *presence;
	LmMessageNode       *node;
	GossipPresenceState  state;
	
	if (lm_message_node_find_child (m->node, "error")) {
		/* FIXME: Handle this (send offline?) */
	}

	if (lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_AVAILABLE) {
		return NULL;
	}

	presence = gossip_presence_new ();

	node = lm_message_node_get_child (m->node, "show");
	if (node) {
		state = gossip_jabber_helper_presence_state_from_str (node->value);
	} else {
		state = GOSSIP_PRESENCE_STATE_AVAILABLE;
	}

	gossip_presence_set_state (presence, state);
	
	node = lm_message_node_get_child (m->node, "status");
	if (node) {
		gossip_presence_set_status (presence, node->value);
	}
	
	node = lm_message_node_get_child (m->node, "priority");
	if (node) {
		gossip_presence_set_priority (presence, atoi (node->value));
	}

	return presence;
}

static GossipContact *
jabber_get_contact_from_jid (GossipJabber *jabber, 
			     const gchar  *jid_str,
			     gboolean     *new_item)
{
	GossipJabberPriv *priv;
	GossipContact    *contact;
	GossipJID        *jid;
	gchar            *name;
	gboolean          tmp_new_item = FALSE;

	/* FIXME: Lookup contact in hash table */
	priv = jabber->priv;

	jid = gossip_jid_new (jid_str);
	d(g_print ("Protocol: Get contact: %s\n", gossip_jid_get_without_resource (jid)));

	contact = g_hash_table_lookup (priv->contacts, 
				       gossip_jid_get_without_resource (jid));

	if (!contact) {
		d(g_print ("Protocol: New contact\n"));
		contact = gossip_contact_new (GOSSIP_CONTACT_TYPE_TEMPORARY);
		gossip_contact_set_id (contact, 
				       gossip_jid_get_without_resource (jid));

		name = gossip_jid_get_part_name (jid);
		gossip_contact_set_name (contact, name);
		g_free (name);
		tmp_new_item = TRUE;

		g_hash_table_insert (priv->contacts, 
				     g_strdup (gossip_contact_get_id (contact)),
				     g_object_ref (contact));
	}

	gossip_jid_unref (jid);

	if (new_item) {
		*new_item = tmp_new_item;
	}

	return contact;
}

#define CONF_HTTP_PROXY_PREFIX "/system/http_proxy"

void
jabber_set_proxy (LmConnection *conn)
{
	gboolean  use_http_proxy;
	
	use_http_proxy = gconf_client_get_bool (gconf_client,
						CONF_HTTP_PROXY_PREFIX "/use_http_proxy",
						NULL);
	if (use_http_proxy) {
		LmProxy  *proxy;
		gchar    *host;
		gint      port;
		gboolean  use_auth;
		
		host = gconf_client_get_string (gconf_client,
						CONF_HTTP_PROXY_PREFIX "/host",
						NULL);

		port = gconf_client_get_int (gconf_client,
					     CONF_HTTP_PROXY_PREFIX "/port",
					     NULL);

		proxy = lm_proxy_new_with_server (LM_PROXY_TYPE_HTTP,
						  host, (guint) port);
		g_free (host);

		lm_connection_set_proxy (conn, proxy);

		use_auth = gconf_client_get_bool (gconf_client,
						  CONF_HTTP_PROXY_PREFIX "/use_authentication",
						  NULL);
		if (use_auth) {
			gchar *username;
			gchar *password;

			username = gconf_client_get_string (gconf_client,
							    CONF_HTTP_PROXY_PREFIX "/authentication_user",
							    NULL);
			password = gconf_client_get_string (gconf_client,
							    CONF_HTTP_PROXY_PREFIX "/authentication_password",
							    NULL);
			lm_proxy_set_username (proxy, username);
			lm_proxy_set_password (proxy, password);

			g_free (username);
			g_free (password);
		}
		
		lm_proxy_unref (proxy);
	}

}

static void
jabber_chatroom_init (GossipChatroomProviderIface *iface)
{
	iface->join  = jabber_chatroom_join;
	iface->send  = jabber_chatroom_send;
	iface->set_title = jabber_chatroom_set_title;
	iface->change_nick = jabber_chatroom_change_nick;
	iface->leave = jabber_chatroom_leave;
	iface->get_room_name = jabber_chatroom_get_room_name;
}

static void
jabber_chatroom_join (GossipChatroomProvider      *provider,
		       const gchar                 *room,
		       const gchar                 *server,
		       const gchar                 *nick,
		       const gchar                 *password,
		       GossipJoinChatroomCb         callback,
		       gpointer                     user_data)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));

	jabber = GOSSIP_JABBER (provider);
	priv   = jabber->priv;

	gossip_jabber_chatrooms_join (priv->chatrooms,
				      room, server, nick, password, 
				      callback, user_data);
}

static void
jabber_chatroom_send (GossipChatroomProvider *provider,
		       GossipChatroomId       id,
		       const gchar           *message)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));

	jabber = GOSSIP_JABBER (provider);
	priv   = jabber->priv;

	gossip_jabber_chatrooms_send (priv->chatrooms, id, message);
}

static void
jabber_chatroom_set_title (GossipChatroomProvider *provider,
			   GossipChatroomId        id,
			   const gchar            *new_title)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));

	jabber = GOSSIP_JABBER (provider);
	priv   = jabber->priv;

	gossip_jabber_chatrooms_set_title (priv->chatrooms, id, new_title);
}

static void
jabber_chatroom_change_nick (GossipChatroomProvider *provider,
			     GossipChatroomId        id,
			     const gchar            *new_nick)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));

	jabber = GOSSIP_JABBER (provider);
	priv   = jabber->priv;

	gossip_jabber_chatrooms_change_nick (priv->chatrooms, id, new_nick);
}

static void
jabber_chatroom_leave (GossipChatroomProvider *provider,
		       GossipChatroomId        id)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));

	jabber = GOSSIP_JABBER (provider);
	priv   = jabber->priv;

	gossip_jabber_chatrooms_leave (priv->chatrooms, id);
}

static const gchar *
jabber_chatroom_get_room_name (GossipChatroomProvider *provider,
			       GossipChatroomId        id)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_JABBER (provider), NULL);

	jabber = GOSSIP_JABBER (provider);
	priv   = jabber->priv;

	return gossip_jabber_chatrooms_get_room_name (priv->chatrooms, id);
}

static void
jabber_signal_logged_out (GossipJabber *jabber)
{
	GossipJabberPriv *priv;

	priv = jabber->priv;

	g_signal_emit_by_name (jabber, "logged-out");

	/* signal removal of each contact */
	if (priv->contacts) {
		g_hash_table_foreach_remove (priv->contacts,
					     (GHRFunc) jabber_logout_contact_foreach,
					     jabber);
	}
}

static void
jabber_version_request (GossipJabber *jabber, LmMessage *m)
{
	GossipJabberPriv  *priv;
	LmMessage         *r;
	const gchar       *from, *id;
	LmMessageNode     *node;
	GossipVersionInfo *info;

	priv = jabber->priv;

	from = lm_message_node_get_attribute (m->node, "from");
	id = lm_message_node_get_attribute (m->node, "id");

	d(g_print ("Protocol: Version request from:'%s'\n", from));

	r = lm_message_new_with_sub_type (from,
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_RESULT);
	lm_message_node_set_attributes (r->node,
					"id", id,
					NULL);
	info = gossip_version_info_get_own ();
	node = lm_message_node_add_child (r->node, "query", NULL);
	lm_message_node_set_attributes (node, 
					"xmlns", XMPP_VERSION_XMLNS,
					NULL);

	if (gossip_version_info_get_name (info)) {
		lm_message_node_add_child (node, 
					   "name", 
					   gossip_version_info_get_name (info));
	}

	if (gossip_version_info_get_version (info)) {
		lm_message_node_add_child (node,
					   "version",
					   gossip_version_info_get_version (info));
	}

	if (gossip_version_info_get_os (info)) {
		lm_message_node_add_child (node,
					   "os",
					   gossip_version_info_get_os (info));
	}

	lm_connection_send (priv->connection, r, NULL);
	lm_message_unref (r);
}

LmConnection *
_gossip_jabber_get_connection (GossipJabber *jabber)
{
	g_return_val_if_fail (GOSSIP_IS_JABBER (jabber), NULL);

	return jabber->priv->connection;
}

GossipContact *
gossip_jabber_get_own_contact (GossipJabber *jabber)
{
	g_return_val_if_fail (GOSSIP_IS_JABBER (jabber), NULL);

	return jabber->priv->contact;
}

/* 
 * added by mjr for branch merge
 */

void
gossip_jabber_subscription_allow_all (GossipJabber *jabber)
{
	GossipJabberPriv *priv;
	LmConnection     *connection;
	LmMessageHandler *handler;

	g_return_if_fail (jabber != NULL);
	
	priv = jabber->priv;
	connection = _gossip_jabber_get_connection (jabber);

	handler = priv->subscription_handler;
	if (handler) {
		lm_connection_unregister_message_handler (connection, 
							  handler,
							  LM_MESSAGE_TYPE_PRESENCE);
		priv->subscription_handler = NULL;
	}

	/* set up handler to sliently catch the subscription request */
	handler = lm_message_handler_new ((LmHandleMessageFunction)jabber_subscription_message_handler, 
					  jabber, NULL);

	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_PRESENCE,
						LM_HANDLER_PRIORITY_FIRST);

	priv->subscription_handler = handler;
}

static LmHandlerResult
jabber_subscription_message_handler (LmMessageHandler  *handler,
				     LmConnection      *connection,
				     LmMessage         *m,
				     GossipJabber      *jabber)
{
	LmMessage     *new_message;
	GossipContact *own_contact;
	const gchar   *id;
	const gchar   *from;
	gchar         *to;

	if (lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_SUBSCRIBE) {
                return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	from = lm_message_node_get_attribute (m->node, "from");
	to = g_strdup (from);

	/* clean up */
	lm_message_handler_invalidate (handler);
	
	/* send subscribed to them */
	d(g_print ("Protocol: Sending subscribed message to new service:'%s'\n", to));
	new_message = lm_message_new_with_sub_type (to,
						    LM_MESSAGE_TYPE_PRESENCE,
						    LM_MESSAGE_SUB_TYPE_SUBSCRIBED);

	own_contact = gossip_jabber_get_own_contact (jabber);
	id = gossip_contact_get_id (own_contact);
	lm_message_node_set_attribute (new_message->node, "from", id);

	lm_connection_send (connection, new_message, NULL);
	lm_message_unref (new_message);

	/* send our presence */
	new_message = lm_message_new_with_sub_type (to, 
						    LM_MESSAGE_TYPE_PRESENCE,
						    LM_MESSAGE_SUB_TYPE_AVAILABLE);

	lm_message_node_set_attribute (new_message->node, "from", id);

	lm_connection_send (connection, new_message, NULL);
	lm_message_unref (new_message);

	/* clean up */
	g_free (to);

 	return LM_HANDLER_RESULT_REMOVE_MESSAGE;	 
}

void
gossip_jabber_subscription_disallow_all (GossipJabber *jabber)
{
	GossipJabberPriv *priv;
	LmConnection     *connection;

	g_return_if_fail (jabber != NULL);

	priv = jabber->priv;

	connection = _gossip_jabber_get_connection (jabber);

	if (priv->subscription_handler) {
		lm_connection_unregister_message_handler (connection, 
							  priv->subscription_handler,
							  LM_MESSAGE_TYPE_PRESENCE);
		priv->subscription_handler = NULL;
	}
}

void
gossip_jabber_send_subscribed (GossipJabber  *jabber,
			       GossipContact *contact)
{
	LmConnection *connection;
	LmMessage    *m;
	const gchar  *id;

	g_return_if_fail (jabber != NULL);
	g_return_if_fail (contact != NULL);

	id = gossip_contact_get_id (contact);
	connection = _gossip_jabber_get_connection (jabber);

	m = lm_message_new_with_sub_type (id, 
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_SUBSCRIBED);

	lm_connection_send (connection, m, NULL);
	lm_message_unref (m);
}

void
gossip_jabber_send_unsubscribed (GossipJabber  *jabber,
				 GossipContact *contact)
{
	LmConnection *connection;
	LmMessage    *m;
	const gchar  *id;

	g_return_if_fail (jabber != NULL);
	g_return_if_fail (contact != NULL);

	id = gossip_contact_get_id (contact);
	connection = _gossip_jabber_get_connection (jabber);

	m = lm_message_new_with_sub_type (id, 
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_UNSUBSCRIBED);

	lm_connection_send (connection, m, NULL);
	lm_message_unref (m);
}
