/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio HB
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
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <libgnome/gnome-i18n.h>

#include "gossip-account.h"
#include "gossip-chatroom-provider.h"
#include "gossip-contact.h"
#include "gossip-jabber-chatrooms.h"
#include "gossip-jabber-helper.h"
#include "gossip-marshal.h"
#include "gossip-vcard.h"
#include "gossip-jabber.h"
#include "gossip-utils.h"
#include "gossip-add-contact.h"
#include "gossip-transport-accounts.h"

#define d(x) x


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


static void            gossip_jabber_class_init              (GossipJabberClass            *klass);
static void            gossip_jabber_init                    (GossipJabber                 *jabber);
static void            jabber_finalize                       (GObject                      *obj);
static void            jabber_login                          (GossipProtocol               *protocol);
static void            jabber_logout                         (GossipProtocol               *protocol);
static gboolean        jabber_is_connected                   (GossipProtocol               *protocol);
static void            jabber_send_message                   (GossipProtocol               *protocol,
							      GossipMessage                *message);
static void            jabber_set_presence                   (GossipProtocol               *protocol,
							      GossipPresence               *presence);
static void            jabber_add_contact                    (GossipProtocol               *protocol,
							      const gchar                  *id,
							      const gchar                  *name,
							      const gchar                  *group,
							      const gchar                  *message);
static void            jabber_rename_contact                 (GossipProtocol               *protocol,
							      GossipContact                *contact,
							      const gchar                  *new_name);
static void            jabber_remove_contact                 (GossipProtocol               *protocol,
							      GossipContact                *contact);
static const gchar *   jabber_get_active_resource            (GossipProtocol               *protocol,
							      GossipContact                *contact);
static gboolean        jabber_async_get_vcard                (GossipProtocol               *protocol,
							      GossipContact                *contact,
							      GossipAsyncVCardCallback      callback,
							      gpointer                      user_data,
							      GError                      **error);
static gboolean        jabber_async_set_vcard                (GossipProtocol               *protocol,
							      GossipVCard                  *vcard,
							      GossipAsyncResultCallback     callback,
							      gpointer                      user_data,
							      GError                      **error);
static gboolean        jabber_async_get_version              (GossipProtocol               *protocol,
							      GossipContact                *contact,
							      GossipAsyncVersionCallback    callback,
							      gpointer                      user_data,
							      GError                      **error);
static void            jabber_connection_open_cb             (LmConnection                 *connection,
							      gboolean                      result,
							      GossipJabber                 *jabber);
static void            jabber_connection_auth_cb             (LmConnection                 *connection,
							      gboolean                      result,
							      GossipJabber                 *jabber);
static LmSSLResponse   jabber_ssl_func                       (LmConnection                 *connection,
							      LmSSLStatus                   status,
							      GossipJabber                 *jabber);
static gboolean        jabber_update_contact                 (GossipContact                *contact,
							      LmMessageNode                *node);
static LmHandlerResult jabber_message_handler                (LmMessageHandler             *handler,
							      LmConnection                 *conn,
							      LmMessage                    *message,
							      GossipJabber                 *jabber);
static LmHandlerResult jabber_presence_handler               (LmMessageHandler             *handler,
							      LmConnection                 *conn,
							      LmMessage                    *message,
							      GossipJabber                 *jabber);
static LmHandlerResult jabber_iq_handler                     (LmMessageHandler             *handler,
							      LmConnection                 *conn,
							      LmMessage                    *message,
							      GossipJabber                 *jabber);
static GossipPresence *jabber_get_presence                   (LmMessage                    *message);
static GossipContact * jabber_get_contact_from_jid           (GossipJabber                 *jabber,
							      const gchar                  *jid,
							      gboolean                     *new_item);
static void            jabber_set_proxy                      (LmConnection                 *conn);
static void            jabber_subscription_request           (GossipJabber                 *jabber,
							      LmMessage                    *m);
static void            jabber_subscription_request_dialog_cb (GtkWidget                    *dialog,
							      gint                          response,
							      GossipJabber                 *jabber);
static LmHandlerResult jabber_subscription_message_handler   (LmMessageHandler             *handler,
							      LmConnection                 *connection,
							      LmMessage                    *m,
							      GossipJabber                 *jabber);
static void            jabber_chatroom_init                  (GossipChatroomProviderIface  *iface);
static void            jabber_chatroom_join                  (GossipChatroomProvider       *provider,
							      const gchar                  *room,
							      const gchar                  *server,
							      const gchar                  *nick,
							      const gchar                  *password,
							      GossipJoinChatroomCb          callback,
							      gpointer                      user_data);
static void            jabber_chatroom_send                  (GossipChatroomProvider       *provider,
							      GossipChatroomId              id,
							      const gchar                  *message);
static void            jabber_chatroom_set_title             (GossipChatroomProvider       *provider,
							      GossipChatroomId              id,
							      const gchar                  *new_title);
static void            jabber_chatroom_change_nick           (GossipChatroomProvider       *provider,
							      GossipChatroomId              id,
							      const gchar                  *new_nick);
static void            jabber_chatroom_leave                 (GossipChatroomProvider       *provider,
							      GossipChatroomId              id);



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

	protocol_class->login               = jabber_login;
	protocol_class->logout              = jabber_logout;
	protocol_class->is_connected        = jabber_is_connected;
	protocol_class->send_message        = jabber_send_message;
	protocol_class->set_presence        = jabber_set_presence;
        protocol_class->add_contact         = jabber_add_contact;
        protocol_class->rename_contact      = jabber_rename_contact;
        protocol_class->remove_contact      = jabber_remove_contact;
	protocol_class->get_active_resource = jabber_get_active_resource;
	protocol_class->async_get_vcard     = jabber_async_get_vcard;
	protocol_class->async_set_vcard     = jabber_async_set_vcard;
	protocol_class->async_get_version   = jabber_async_get_version;
}

static void
gossip_jabber_init (GossipJabber *jabber)
{
	GossipJabberPriv *priv;
	LmMessageHandler *handler;

	priv = g_new0 (GossipJabberPriv, 1);
	jabber->priv = priv;

	priv->contacts = 
		g_hash_table_new_full (g_str_hash,
				       g_str_equal,
				       (GDestroyNotify) g_free,
				       (GDestroyNotify) g_object_unref);
	
	priv->account = gossip_account_get_default ();

	priv->contact = gossip_contact_new (GOSSIP_CONTACT_TYPE_USER);
	
	gossip_contact_set_id (priv->contact, 
			       gossip_jid_get_without_resource (priv->account->jid));

	priv->connection = lm_connection_new (priv->account->server);
	lm_connection_set_port (priv->connection, priv->account->port);

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
jabber_finalize (GObject *obj)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;

	jabber = GOSSIP_JABBER (obj);
	priv   = jabber->priv;
	
	g_hash_table_destroy (priv->contacts);
	gossip_jabber_chatrooms_free (priv->chatrooms);

	gossip_transport_account_list_free (priv->account_list);

	g_free (priv);
}

static void
jabber_login (GossipProtocol *protocol)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;
	gboolean          result;
	GError           *error;

	g_return_if_fail (GOSSIP_IS_JABBER (protocol));
	
	jabber = GOSSIP_JABBER (protocol);
	priv   = jabber->priv;

	g_print ("Logging in Jabber\n");
	
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

	if (priv->connection == NULL) {
		g_print ("Foo!!!\n");
	}

	priv->presence = gossip_presence_new ();
	gossip_presence_set_state (priv->presence, 
				   GOSSIP_PRESENCE_STATE_AVAILABLE);
	result = lm_connection_open (priv->connection, 
				     (LmResultFunction) jabber_connection_open_cb,
				     jabber, NULL, &error);

	if (result == FALSE && error) {
		/* Handle error */
		g_error_free (error);
	}

}

static void
jabber_logout (GossipProtocol *protocol)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (protocol));

	jabber = GOSSIP_JABBER (protocol);
	priv   = jabber->priv;

	if (lm_connection_is_open (priv->connection)) {
		lm_connection_close (priv->connection, NULL);
	}
}

static gboolean
jabber_is_connected (GossipProtocol *protocol)
{
	GossipJabber     *jabber;
	GossipJabberPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_JABBER (protocol), FALSE);

	jabber = GOSSIP_JABBER (protocol);
	priv   = jabber->priv;

	return lm_connection_is_authenticated (priv->connection);
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

	g_print ("Jabber::SendMessage, to: '%s'\n", jid_str);
	
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
        if (strcmp (group, "") != 0) {
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
jabber_remove_contact (GossipProtocol *protocol, GossipContact *contact)
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

static const gchar *
jabber_get_active_resource (GossipProtocol *protocol,
			    GossipContact  *contact)
{
	/* FIXME: Get the active resource */
	return NULL;
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
	gchar            *username;
	gchar            *password = NULL;
	const gchar      *resource = NULL;
	
	priv = jabber->priv;

	g_print ("Connection open!\n");

	if (result == FALSE) {
			       
		/* FIXME: Show this as an error */
		g_print ("Couldn't connect, handle this better!\n");
	}

	account = priv->account;
	if (!account->password || !account->password[0]) {
		/* FIXME: Ask the user for the password */
		g_print ("Ask the user for the password\n");
		g_signal_emit_by_name (jabber, "get-password", 
				       account, &password);
		g_print ("Got a password: '%s'\n", password);
	} else {
		password = g_strdup (account->password);
	}
	 
	username = gossip_jid_get_part_name (account->jid);
	resource = gossip_jid_get_resource (account->jid);
	/* FIXME: Decide on Resource */
	
	lm_connection_authenticate (priv->connection,
				    username, password, resource,
				    (LmResultFunction) jabber_connection_auth_cb,
				    jabber, NULL, NULL);
	g_free (username);
	g_free (password);
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
		/* FIXME: Show this with an error */
		g_print ("AUTH FAILED, SHOW DIALOG\n");
		return;
	}

	g_print ("LOGGED IN!\n");

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

static gboolean
jabber_update_contact (GossipContact *contact, LmMessageNode *node)
{
	/*const gchar   *subscription;
	const gchar   *ask; */
	const gchar   *name;
	LmMessageNode *child;
	GList         *categories = NULL;
	gboolean       updated, categories_updated;

	updated = categories_updated = FALSE;

	/* Update the item, can be name change, group change, etc.. */
	/* FIXME: Do we really need this in the contact? 
	 *
	 subscription = lm_message_node_get_attribute (node, "subscription");
	 if (subscription) {
	 g_free (item->subscription);
	 item->subscription = g_strdup (subscription);
	 }
	
	ask = lm_message_node_get_attribute (node, "ask");
	if (ask) {
		g_free (item->ask);
		item->ask = g_strdup (ask);
	}
	 */
		
	name = lm_message_node_get_attribute (node, "name");
	if (name) {
		updated = TRUE;
		gossip_contact_set_name (contact, name);
	}

	for (child = node->children; child; child = child->next) {
		if (strcmp (child->name, "group") == 0 && child->value) {
			categories = g_list_append (categories, 
						    g_strdup (child->value));
		}
	}

	/* FIXME: Should this list be freed after? */
	categories_updated = gossip_contact_set_groups (contact, categories);

	return updated || categories_updated;
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
	
	g_print ("GossipJabber: New message from: %s\n", 
		 lm_message_node_get_attribute (m->node, "from"));

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
	GossipJID        *from;
	GossipContact    *contact;
	const gchar      *type;
	
	priv = jabber->priv;

	from = gossip_jid_new (lm_message_node_get_attribute (m->node, "from"));
        g_print ("GossipJabber: New presence from: %s\n", 
                 lm_message_node_get_attribute (m->node, "from"));
	
	contact = (GossipContact *) g_hash_table_lookup (priv->contacts, 
							 gossip_jid_get_without_resource (from));

	/* FIXME-MJR: do we want to do this here or after the presence
	   update event? */
	type = lm_message_node_get_attribute (m->node, "type");
	if (!type) {
		type = "available";
	}

	if (strcmp (type, "subscribe") == 0) {
		jabber_subscription_request (jabber, m);
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}
	
	if (contact) {
		GossipPresence *presence;
		
		presence = jabber_get_presence (m);
		if (presence) {
                        gossip_contact_set_presence (contact, presence);
			g_object_unref (presence);

                        g_signal_emit_by_name (jabber,
                                               "contact-presence-updated", 
                                               contact);
                }
        }
	
        gossip_jid_unref (from);
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

	node = lm_message_node_get_child (m->node, "query");
	if (!node) {
		goto end;
	}

	xmlns = lm_message_node_get_attribute (node, "xmlns");
	if (!xmlns || strcmp (xmlns, "jabber:iq:roster") != 0) {
		goto end;
	}

	for (node = node->children; node; node = node->next) {
		GossipContact *contact;
		const gchar   *jid_str;
		const gchar   *subscription;
		gboolean       new_item = FALSE;
		gboolean       updated;

		if (strcmp (node->name, "item") != 0) {
			continue;
		}

		jid_str = lm_message_node_get_attribute (node, "jid");
		if (!jid_str) {
			continue;
		}

		contact = jabber_get_contact_from_jid (jabber, 
						       jid_str, &new_item);
		subscription = lm_message_node_get_attribute (node, 
							      "subscription");
		if (contact && subscription && strcmp (subscription, "remove") == 0) {
			g_object_ref (contact);
			g_hash_table_remove (priv->contacts, 
					     gossip_contact_get_id (contact));
			
			g_signal_emit_by_name (jabber,
					       "contact-removed", contact);
			g_object_unref (contact);
			continue;
		}
		
		updated = jabber_update_contact (contact, node);
		
		if (new_item) {
			g_signal_emit_by_name (jabber, 
					       "contact-added", contact);
		} 
		else if (updated) {
			g_signal_emit_by_name (jabber, 
					       "contact-updated", contact);
		}
	}

end:
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
	g_print ("Get contact: %s\n", gossip_jid_get_without_resource (jid));

	contact = g_hash_table_lookup (priv->contacts, 
				       gossip_jid_get_without_resource (jid));

	if (!contact) {
		g_print ("New contact\n");
		contact = gossip_contact_new (GOSSIP_CONTACT_TYPE_CONTACTLIST);
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
				      priv->presence,
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

LmConnection *
gossip_jabber_get_connection (GossipJabber *jabber)
{
	g_return_val_if_fail (GOSSIP_IS_JABBER (jabber), NULL);

	g_print ("Foo\n");

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

static void
jabber_subscription_request (GossipJabber *jabber,
			     LmMessage    *m)
{
	GtkWidget        *dialog;
	GtkWidget        *jid_label;
	GtkWidget        *add_check_button;
	gchar            *str, *tmp;
	const gchar      *from;
	GossipContact    *contact;
	gboolean          new_contact = FALSE;

	gossip_glade_get_file_simple (GLADEDIR "/main.glade",
				      "subscription_request_dialog",
				      NULL,
				      "subscription_request_dialog", &dialog,
				      "jid_label", &jid_label,
				      "add_check_button", &add_check_button,
				      NULL);
	
	from = lm_message_node_get_attribute (m->node, "from");

	tmp = g_strdup_printf (_("%s wants to be notified of your status."), from);
	str = g_strdup_printf ("<span weight='bold' size='larger'>%s</span>", tmp);
	g_free (tmp);

	gtk_label_set_text (GTK_LABEL (jid_label), str);
	gtk_label_set_use_markup (GTK_LABEL (jid_label), TRUE);
	g_free (str);

	g_signal_connect (dialog,
			  "response",
			  G_CALLBACK (jabber_subscription_request_dialog_cb),
			  g_object_ref (jabber));

	g_object_set_data (G_OBJECT (dialog), 
			   "message", lm_message_ref (m));

	g_object_set_data (G_OBJECT (dialog),
			   "add_check_button", add_check_button);

 	contact = jabber_get_contact_from_jid (jabber, from, &new_contact); 

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (add_check_button), !new_contact); 
	gtk_widget_set_sensitive (add_check_button, !new_contact);

	gtk_widget_show (dialog);
}

#define REQUEST_RESPONSE_DECIDE_LATER 1

static void
jabber_subscription_request_dialog_cb (GtkWidget    *dialog,
				       gint          response,
				       GossipJabber *jabber)
{
	LmMessage        *m;
	LmMessage        *reply = NULL;
	gboolean          add_user;
	GtkWidget        *add_check_button;
	LmMessageSubType  sub_type = LM_MESSAGE_SUB_TYPE_NOT_SET;
	const gchar      *from;
	GossipJabberPriv *priv;
	GossipContact    *contact;
	gboolean          new_contact;

	g_return_if_fail (GTK_IS_DIALOG (dialog));
	g_return_if_fail (GOSSIP_IS_JABBER (jabber));

	priv = jabber->priv;
  
	m = g_object_get_data (G_OBJECT (dialog), "message");
	if (!m) {
		g_warning ("Message not set on subscription request dialog\n");
		return;
	}

	add_check_button = g_object_get_data (G_OBJECT (dialog), 
					      "add_check_button");
	add_user = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (add_check_button));

	gtk_widget_destroy (dialog);
	
	switch (response) {
	case REQUEST_RESPONSE_DECIDE_LATER:
		/* Decide later. */
		return;
		break;
	case GTK_RESPONSE_DELETE_EVENT:
		/* Do nothing */
		return;
		break;
	case GTK_RESPONSE_YES:
		sub_type = LM_MESSAGE_SUB_TYPE_SUBSCRIBED;
		break;
	case GTK_RESPONSE_NO:
		sub_type = LM_MESSAGE_SUB_TYPE_UNSUBSCRIBED;
		break;
	default:
		g_assert_not_reached ();
		break;
	};
	
	from = lm_message_node_get_attribute (m->node, "from");
	
	reply = lm_message_new_with_sub_type (from, 
					      LM_MESSAGE_TYPE_PRESENCE,
					      sub_type);

	lm_connection_send (priv->connection, reply, NULL);
	lm_message_unref (reply);
	
 	contact = jabber_get_contact_from_jid (jabber, from, &new_contact); 
	if (add_user && !contact && sub_type == LM_MESSAGE_SUB_TYPE_SUBSCRIBED) {
		gossip_add_contact_new (from);
	}
	
	lm_message_unref (m);
	g_object_unref (jabber);
}

void
gossip_jabber_subscription_allow_all (GossipJabber *jabber)
{
	GossipJabberPriv *priv;
	LmConnection     *connection;
	LmMessageHandler *handler;

	g_return_if_fail (jabber != NULL);
	
	priv = jabber->priv;
	connection = gossip_jabber_get_connection (jabber);

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
	const gchar   *to;

	if (lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_SUBSCRIBE) {
                return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	to = lm_message_node_get_attribute (m->node, "from");

	/* clean up */
	lm_message_handler_invalidate (handler);
	
	/* send subscribed to them */
	d(g_print ("sending subscribed message to new service:'%s'\n", to));
	new_message = lm_message_new_with_sub_type (to,
						    LM_MESSAGE_TYPE_PRESENCE,
						    LM_MESSAGE_SUB_TYPE_SUBSCRIBED);

	own_contact = gossip_jabber_get_own_contact (jabber);
	id = gossip_contact_get_id (own_contact);
	lm_message_node_set_attribute (new_message->node, "from", id);

	lm_connection_send (connection, new_message, NULL);
	lm_message_unref (new_message);

	/* clean up */
 	return LM_HANDLER_RESULT_REMOVE_MESSAGE;	 
}

void
gossip_jabber_subscription_disallow_all (GossipJabber *jabber)
{
	GossipJabberPriv *priv;
	LmConnection     *connection;

	g_return_if_fail (jabber != NULL);

	priv = jabber->priv;

	connection = gossip_jabber_get_connection (jabber);

	if (priv->subscription_handler) {
		lm_connection_unregister_message_handler (connection, 
							  priv->subscription_handler,
							  LM_MESSAGE_TYPE_PRESENCE);
		priv->subscription_handler = NULL;
	}
}
