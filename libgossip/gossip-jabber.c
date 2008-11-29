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
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>

#include "gossip-jabber.h"
#include "gossip-account.h"
#include "gossip-avatar.h"
#include "gossip-contact.h"
#include "gossip-contact-manager.h"
#include "gossip-conf.h"
#include "gossip-chatroom.h"
#include "gossip-debug.h"
#include "gossip-chatroom-provider.h"
#include "gossip-ft.h"
#include "gossip-ft-provider.h"
#include "gossip-utils.h"
#include "gossip-vcard.h"
#include "gossip-version-info.h"
#include "gossip-session.h"

#include "gossip-jid.h"
#include "gossip-jabber-chatrooms.h"
#include "gossip-jabber-ns.h"
#include "gossip-jabber-ft.h"
#include "gossip-jabber-vcard.h"
#include "gossip-jabber-disco.h"
#include "gossip-jabber-register.h"
#include "gossip-jabber-services.h"
#include "gossip-jabber-utils.h"
#include "libgossip-marshal.h"

#include "gossip-jabber-private.h"
#include "gossip-sha.h"

#ifdef USE_TRANSPORTS
#include "gossip-transport-accounts.h"
#endif

#define DEBUG_DOMAIN "Jabber"

#define GOSSIP_JABBER_ERROR_DOMAIN "GossipJabber"

/* We use 3.5 minutes because if the port is just wrong then it
 * will timeout before then with that error.
 */
#define CONNECT_TIMEOUT            210

/* This is the timeout we will accept the user to be composing for
 * before we assume it is stuck and the server has failed to tell us
 * the user has stopped composing.
 */
#define COMPOSING_TIMEOUT          45

/* How many rand char should be happend to the resource */
#define N_RAND_CHAR                6

#define GOSSIP_JABBER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_JABBER, GossipJabberPrivate))

struct _GossipJabberPrivate {
	GossipSession         *session;

	LmConnection          *connection;
	LmSSLStatus            ssl_status;
	gboolean               ssl_disconnection;

	GossipAccount         *account;
	GossipPresence        *presence;

	GHashTable            *contact_list;

	/* Cancel registration attempt */
	gboolean               register_cancel;

	/* Cancel password change attempt */
	gboolean               change_password_cancel;

	/* Connection details */
	guint                  connection_timeout_id;
	gboolean               disconnect_request;

	/* Extended parts */
	GossipJabberChatrooms *chatrooms;
	GossipJabberFTs       *fts;

	/* Used to hold a list of composing message ids, this is so we
	 * can send the cancelation to the last message id.
	 */
	GHashTable            *composing_ids;
	GHashTable            *composing_timeouts;
	GHashTable            *composing_requests;

	GHashTable            *vcards;

	/* Transport stuff... is this in the right place? */
#ifdef USE_TRANSPORTS
	GossipTransportAccountList *account_list;
#endif
	LmMessageHandler      *subscription_handler;
};

typedef struct {
	GossipJabber     *jabber;
	gchar            *group;
	gchar            *new_name;
} RenameGroupData;

typedef struct {
	GossipJabber     *jabber;
	GossipContact    *contact;
	gpointer          user_data;
} JabberData;

static void             gossip_jabber_class_init            (GossipJabberClass          *klass);
static void             gossip_jabber_init                  (GossipJabber               *jabber);
static void             gossip_jabber_finalize              (GObject                    *object);
static void             jabber_vcard_destroy_notify_func    (gpointer                    data);
static gboolean         jabber_login_timeout_cb             (GossipJabber               *jabber);
static gboolean         jabber_logout_contact_foreach       (gpointer                    key,
							     gpointer                    value,
							     gpointer                    user_data);
static void             jabber_connected_cb                 (LmConnection               *connection,
							     gboolean                    result,
						 	     GossipJabber               *jabber);
static void             jabber_auth_cb                      (LmConnection               *connection,
							     gboolean                    result,
							     GossipJabber               *jabber);
static void             jabber_disconnected_cb              (LmConnection               *connection,
							     LmDisconnectReason          reason,
							     GossipJabber               *jabber);
static LmSSLResponse    jabber_ssl_status_cb                (LmConnection               *connection,
							     LmSSLStatus                 status,
							     GossipJabber               *jabber);
static gboolean         jabber_composing_timeout_cb         (JabberData                 *data);
static void		jabber_contact_is_avatar_latest	    (GossipJabber	        *jabber,
							     GossipContact	        *contact,
							     LmMessageNode   	        *m,
							     gboolean                    force_update);
static void             jabber_contact_get_vcard            (GossipJabber               *jabber,
							     GossipContact              *contact,
							     gboolean                    force_update);
static void             jabber_group_rename_foreach_cb      (gpointer                    key,
							     gpointer                    value,
							     gpointer                    user_data);
static GossipPresence * jabber_get_presence                 (LmMessage                  *message);
static void             jabber_get_groups_foreach_cb        (gpointer                    key,
							     gpointer                    value,
							     gpointer                    user_data);
static LmHandlerResult  jabber_message_handler              (LmMessageHandler           *handler,
							     LmConnection               *conn,
							     LmMessage                  *message,
							     GossipJabber               *jabber);
static LmHandlerResult  jabber_presence_handler             (LmMessageHandler           *handler,
							     LmConnection               *conn,
							     LmMessage                  *message,
							     GossipJabber               *jabber);
static LmHandlerResult  jabber_iq_query_handler             (LmMessageHandler           *handler,
							     LmConnection               *conn,
							     LmMessage                  *message,
							     GossipJabber               *jabber);
static LmHandlerResult  jabber_subscription_message_handler (LmMessageHandler           *handler,
							     LmConnection               *connection,
							     LmMessage                  *m,
							     GossipJabber               *jabber);
static void             jabber_request_version              (GossipJabber               *jabber,
							     LmMessage                  *m);
static void             jabber_request_ping                 (GossipJabber               *jabber,
							     LmMessage                  *m);
static void             jabber_request_roster               (GossipJabber               *jabber,
							     LmMessage                  *m);
static void             jabber_request_unknown              (GossipJabber               *jabber,
							     LmMessage                  *m);

/* Chatrooms */
static void             jabber_chatroom_init                (GossipChatroomProviderIface *iface);
static GossipChatroomId jabber_chatroom_join                (GossipChatroomProvider     *provider,
							     GossipChatroom             *chatroom,
							     GossipChatroomJoinCb        callback,
							     gpointer                    user_data);
static void             jabber_chatroom_cancel              (GossipChatroomProvider     *provider,
							     GossipChatroomId            id);
static void             jabber_chatroom_send                (GossipChatroomProvider     *provider,
							     GossipChatroomId            id,
							     const gchar                *message);
static void             jabber_chatroom_change_subject      (GossipChatroomProvider     *provider,
							     GossipChatroomId            id,
							     const gchar                *new_subject);
static void             jabber_chatroom_change_nick         (GossipChatroomProvider     *provider,
							     GossipChatroomId            id,
							     const gchar                *new_nick);
static void             jabber_chatroom_leave               (GossipChatroomProvider     *provider,
							     GossipChatroomId            id);
static void             jabber_chatroom_kick                (GossipChatroomProvider     *provider,
							     GossipChatroomId            id,
							     GossipContact              *contact,
							     const gchar                *reason);
static GossipChatroom * jabber_chatroom_find_by_id          (GossipChatroomProvider     *provider,
							     GossipChatroomId            id);
static GossipChatroom * jabber_chatroom_find                (GossipChatroomProvider     *provider,
							     GossipChatroom             *chatroom);
static void             jabber_chatroom_invite              (GossipChatroomProvider     *provider,
							     GossipChatroomId            id,
							     GossipContact              *contact,
							     const gchar                *reason);
static void             jabber_chatroom_invite_accept       (GossipChatroomProvider     *provider,
							     GossipChatroomJoinCb        callback,
							     GossipChatroomInvite       *invite,
							     const gchar                *nickname);
static void             jabber_chatroom_invite_decline      (GossipChatroomProvider     *provider,
							     GossipChatroomInvite       *invite,
							     const gchar                *reason);
static GList *          jabber_chatroom_get_rooms           (GossipChatroomProvider     *provider);
static void             jabber_chatroom_browse_rooms        (GossipChatroomProvider     *provider,
							     const gchar                *server,
							     GossipChatroomBrowseCb      callback,
							     gpointer                    user_data);

/* File Transfers */
static void             jabber_ft_init                      (GossipFTProviderIface      *iface);
static GossipFT *       jabber_ft_send                      (GossipFTProvider           *provider,
							     GossipContact              *contact,
							     const gchar                *file);
static void             jabber_ft_cancel                    (GossipFTProvider           *provider,
							     GossipFTId                  id);
static void             jabber_ft_accept                    (GossipFTProvider           *provider,
							     GossipFTId                  id);
static void             jabber_ft_decline                   (GossipFTProvider           *provider,
							     GossipFTId                  id);

/* Misc */
static JabberData *     jabber_data_new                     (GossipJabber               *jabber,
							     GossipContact              *contact,
							     gpointer                    user_data);
static void             jabber_data_free                    (gpointer                    data);

static const gchar *server_conversions[] = {
	"gmail.com", "talk.google.com",
	"googlemail.com", "talk.google.com",
	NULL
};

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

G_DEFINE_TYPE_WITH_CODE (GossipJabber, gossip_jabber, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GOSSIP_TYPE_CHATROOM_PROVIDER,
						jabber_chatroom_init);
			 G_IMPLEMENT_INTERFACE (GOSSIP_TYPE_FT_PROVIDER,
						jabber_ft_init));

static void
gossip_jabber_class_init (GossipJabberClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gossip_jabber_finalize;

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


	g_type_class_add_private (object_class, sizeof (GossipJabberPrivate));
}

static void
gossip_jabber_init (GossipJabber *jabber)
{
	GossipJabberPrivate *priv;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	priv->contact_list = 
		g_hash_table_new_full (gossip_contact_hash,
				       gossip_contact_equal,
				       g_object_unref,
				       NULL);

	priv->composing_ids =
		g_hash_table_new_full (gossip_contact_hash,
				       gossip_contact_equal,
				       g_object_unref,
				       g_free);

	priv->composing_timeouts =
		g_hash_table_new_full (gossip_contact_hash,
				       gossip_contact_equal,
				       g_object_unref,
				       jabber_data_free);

	priv->composing_requests =
		g_hash_table_new_full (gossip_contact_hash,
				       gossip_contact_equal,
				       g_object_unref,
				       NULL);

	priv->vcards = 
		g_hash_table_new_full (gossip_contact_hash,
				       gossip_contact_equal,
				       g_object_unref,
				       jabber_vcard_destroy_notify_func);
}

static void
gossip_jabber_finalize (GObject *object)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	jabber = GOSSIP_JABBER (object);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	if (priv->account) {
		g_object_unref (priv->account);
	}

	if (priv->presence) {
		g_object_unref (priv->presence);
	}

	if (priv->session) {
		g_object_unref (priv->session);
	}

	/* finalize extended modules */
	gossip_jabber_chatrooms_finalize (priv->chatrooms);
	if (priv->fts) {
		gossip_jabber_ft_finalize (priv->fts);
	}

	g_hash_table_unref (priv->vcards);

	g_hash_table_unref (priv->composing_requests);
	g_hash_table_unref (priv->composing_timeouts);
	g_hash_table_unref (priv->composing_ids);

	g_hash_table_unref (priv->contact_list);

	if (priv->connection_timeout_id != 0) {
		g_source_remove (priv->connection_timeout_id);
	}

#ifdef USE_TRANSPORTS
	gossip_transport_account_list_free (priv->account_list);
#endif

	if (priv->connection) {
		lm_connection_unref (priv->connection);
	}

	(G_OBJECT_CLASS (gossip_jabber_parent_class)->finalize) (object);
}

static void
jabber_vcard_destroy_notify_func (gpointer data)
{
	if (!data) {
		/* Nothing to do, NULL was used as a place holder so
		 * we don't request the vcard more than once.
		 */
		return;
	}

	g_object_unref (data);
}

GossipJabber *
gossip_jabber_new (gpointer session)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);

	jabber = g_object_new (GOSSIP_TYPE_JABBER, NULL);
	
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);
	priv->session = g_object_ref (session);

	return jabber;
}

GossipAccount *
gossip_jabber_new_account (void)
{
	GossipAccount *account;
	const gchar   *example_id;
	GossipJID     *jid;
	const gchar   *computer_name;

	/* Note, we now assume the default non-ssl port since we use
	 * STARTTLS once connected on the normal 5222 port. 5223 is
	 * used for legacy old ssl support.
	 */
	example_id = gossip_jid_get_example_string ();
	jid = gossip_jid_new (example_id);
	computer_name = g_get_host_name ();
	
	if (!computer_name) {
		computer_name = _("Home");
	}

	/* Set a default value for each account parameter */
	account = g_object_new (GOSSIP_TYPE_ACCOUNT,
				"name", _("new account"),
				"server", gossip_jid_get_part_host (jid),
				"resource", computer_name,
				"port", gossip_jabber_get_default_port (FALSE), 
				"use-ssl", gossip_jabber_is_ssl_supported (),
				"force-old-ssl", FALSE,
				"ignore-ssl-errors", TRUE,
				NULL);

	g_object_unref (jid);

	return account;
}

void
gossip_jabber_setup (GossipJabber  *jabber,
		     GossipAccount *account)
{
	GossipJabberPrivate *priv;
	LmMessageHandler *handler;

	g_return_if_fail (GOSSIP_IS_JABBER (jabber));
	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);
	
	priv->account = g_object_ref (account);

	/* Update the connection details */
	priv->connection = _gossip_jabber_new_connection (jabber, account);

	/* Setup the connection to send keep alive messages every 30
	 * seconds.
	 */
	lm_connection_set_keep_alive_rate (priv->connection, 30);

	lm_connection_set_disconnect_function (priv->connection,
					       (LmDisconnectFunction) jabber_disconnected_cb,
					       jabber, NULL);

	/* Set up handlers for messages and presence */
	handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_message_handler,
					  jabber, NULL);
	lm_connection_register_message_handler (priv->connection,
						handler,
						LM_MESSAGE_TYPE_MESSAGE,
						LM_HANDLER_PRIORITY_NORMAL);
	lm_message_handler_unref (handler);

	handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_presence_handler,
					  jabber, NULL);
	lm_connection_register_message_handler (priv->connection,
						handler,
						LM_MESSAGE_TYPE_PRESENCE,
						LM_HANDLER_PRIORITY_NORMAL);
	lm_message_handler_unref (handler);

	handler = lm_message_handler_new ((LmHandleMessageFunction) jabber_iq_query_handler,
					  jabber, NULL);
	lm_connection_register_message_handler (priv->connection,
						handler,
						LM_MESSAGE_TYPE_IQ,
						LM_HANDLER_PRIORITY_NORMAL);
	lm_message_handler_unref (handler);

	/* Initiate extended modules */
	priv->chatrooms = gossip_jabber_chatrooms_init (jabber);
	priv->fts = gossip_jabber_ft_init (jabber);
	gossip_jabber_disco_init (jabber);

#ifdef USE_TRANSPORTS
	/* initialise the jabber accounts module which is necessary to
	   watch roster changes to know which services are set up */
	priv->account_list = gossip_transport_account_list_new (jabber);
#endif
}

void
gossip_jabber_login (GossipJabber *jabber)
{
	GossipJabberPrivate *priv;
	GossipContact    *own_contact;
	const gchar      *id;
	const gchar      *password;
	GError           *error = NULL;
	gboolean          result;

	g_return_if_fail (GOSSIP_IS_JABBER (jabber));

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	gossip_debug (DEBUG_DOMAIN, "Refreshing connection details");

	own_contact = gossip_jabber_get_contact_from_jid (jabber,
							  gossip_account_get_id (priv->account),
							  TRUE,
							  FALSE,
							  FALSE);

	_gossip_jabber_set_connection (priv->connection, 
				       jabber,
				       priv->account);

	/* Update connection details and own contact information */
	id = gossip_account_get_id (priv->account);
	gossip_contact_set_id (own_contact, id);
	
	/* Check the saved password */
	password = gossip_account_get_password (priv->account);
	if (G_STR_EMPTY (password)) {
		/* Last, check the temporary password (not saved to disk) */
		password = gossip_account_get_password_tmp (priv->account);
	}

	/* If no password, signal an error */
	if (G_STR_EMPTY (password)) {
		g_signal_emit_by_name (jabber, "disconnecting", priv->account);
		g_signal_emit_by_name (jabber, "disconnected", priv->account, 
				       GOSSIP_JABBER_DISCONNECT_ASKED);

		gossip_debug (DEBUG_DOMAIN, "No password specified, not logging in");
		gossip_jabber_error (jabber, GOSSIP_JABBER_NO_PASSWORD);
		return;
	}
	
	gossip_debug (DEBUG_DOMAIN, "Connecting...");
	g_signal_emit_by_name (jabber, "connecting", priv->account);

	/* This is important. If we have just disconnected, this will
	 * be set to TRUE and it means that when we try to
	 * re-authenticate, we immediately will disconnect because we
	 * think we have asked to disconnect due to a long connecting
	 * time.
	 */
	priv->disconnect_request = FALSE;

	/* This is important. If we just got disconnected because of
	 * an invalid certificate, we need to reset this in case
	 * things have changed since then.
	 */
	priv->ssl_status = LM_SSL_STATUS_NO_CERT_FOUND;
	priv->ssl_disconnection = FALSE;

	/* Set up new presence information */
	if (priv->presence) {
		g_object_unref (priv->presence);
	}

	priv->presence = gossip_presence_new ();
	gossip_presence_set_state (priv->presence,
				   GOSSIP_PRESENCE_STATE_AVAILABLE);
	gossip_jabber_chatrooms_set_presence (priv->chatrooms, priv->presence);

	if (!priv->connection) {
		/* Should we emit these or just call jabber_logout()
		 * with the risk of not getting the disconnected
		 * signal from Loudmouth? 
		 */
		g_signal_emit_by_name (jabber, "disconnecting", priv->account);
		g_signal_emit_by_name (jabber, "disconnected", priv->account, 
				       GOSSIP_JABBER_DISCONNECT_ASKED);

		gossip_debug (DEBUG_DOMAIN, "No connection, not logging in");
		gossip_jabber_error (jabber, GOSSIP_JABBER_NO_CONNECTION);
		return;
	}

	result = lm_connection_open (priv->connection,
				     (LmResultFunction) jabber_connected_cb,
				     jabber, NULL, &error);

	if (result && !error) {
		/* FIXME: add timeout incase we get nothing back from
		 * Loudmouth, this happens with current CVS loudmouth
		 * 1.01 where you connect to port 5222 using SSL, the
		 * error is not reflecting back into Gossip so we just
		 * hang around waiting.
		 */
		priv->connection_timeout_id = g_timeout_add (CONNECT_TIMEOUT * 1000,
							     (GSourceFunc) jabber_login_timeout_cb,
							     jabber);

		return;
	}

	if (error->code == 1 &&
	    strcmp (error->message, "getaddrinfo() failed") == 0) {
		gossip_jabber_logout (jabber);
		gossip_jabber_error (jabber, GOSSIP_JABBER_NO_SUCH_HOST);
	}

	g_error_free (error);
}

static gboolean
jabber_login_timeout_cb (GossipJabber *jabber)
{
	GossipJabberPrivate *priv;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	priv->connection_timeout_id = 0;

	gossip_jabber_error (jabber, GOSSIP_JABBER_TIMED_OUT);

	return FALSE;
}

void
gossip_jabber_logout (GossipJabber *jabber)
{
	GossipJabberPrivate *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (jabber));

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	gossip_debug (DEBUG_DOMAIN, 
		      "Disconnecting for account:'%s'",
		      gossip_account_get_name (priv->account));

	g_signal_emit_by_name (jabber, "disconnecting", priv->account);

	if (priv->connection_timeout_id != 0) {
		g_source_remove (priv->connection_timeout_id);
		priv->connection_timeout_id = 0;
	}

	priv->disconnect_request = TRUE;

	if (priv->connection) {
		lm_connection_close (priv->connection, NULL);
	}
}

static gboolean
jabber_logout_contact_foreach (gpointer key,
			       gpointer value,
			       gpointer user_data)
{
	GossipContact *contact;
	GossipJabber  *jabber;

	contact = GOSSIP_CONTACT (key);
	jabber = GOSSIP_JABBER (user_data);

	/* Copy the list since it will be modified during traversal
	 * otherwise.
	 */

	gossip_contact_set_presence_list (contact, NULL);

	g_signal_emit_by_name (jabber, "contact-removed", contact);

	return TRUE;
}

static const gchar *
jabber_ssl_status_to_string (LmSSLStatus status)
{
	switch (status) {
	case LM_SSL_STATUS_NO_CERT_FOUND:
		return _("No certificate found");
	case LM_SSL_STATUS_UNTRUSTED_CERT:
		return _("Untrusted certificate");
	case LM_SSL_STATUS_CERT_EXPIRED:
		return _("Certificate expired");
	case LM_SSL_STATUS_CERT_NOT_ACTIVATED:
		return _("Certificate not activated");
	case LM_SSL_STATUS_CERT_HOSTNAME_MISMATCH:
		return _("Certificate host mismatch");
	case LM_SSL_STATUS_CERT_FINGERPRINT_MISMATCH:
		return _("Certificate fingerprint mismatch");
	case LM_SSL_STATUS_GENERIC_ERROR:
		return _("Unknown security error occurred");
	default:
		break;
	}

	return _("Unknown error");
}

static LmSSLResponse
jabber_ssl_status_cb (LmConnection  *connection,
		      LmSSLStatus    status,
		      GossipJabber  *jabber)
{
	GossipJabberPrivate *priv;
	GossipAccount    *account;
	const gchar      *str;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	str = jabber_ssl_status_to_string (status);
	gossip_debug (DEBUG_DOMAIN, "%s", str);

	priv->ssl_status = status;

	account = gossip_jabber_get_account (jabber);

	if (gossip_account_get_ignore_ssl_errors (account)) {
		priv->ssl_disconnection = FALSE;
		return LM_SSL_RESPONSE_CONTINUE;
	} else {
		priv->ssl_disconnection = TRUE;

		/* FIXME: Not sure if this is a LM bug, but, we don't
		 * disconnect properly in this situation when using
		 * STARTTLS - so we force a disconnect
		 */
		if (!gossip_account_get_force_old_ssl (account)) {
			GError      *error;
			const gchar *str;

			gossip_jabber_logout (jabber);

			/* This code is duplicated because of this bug */
			str = jabber_ssl_status_to_string (priv->ssl_status);
			error = gossip_jabber_error_create (priv->ssl_status, str);
			g_signal_emit_by_name (jabber, "error", account, error);
			g_error_free (error);
		}

		return LM_SSL_RESPONSE_STOP;
	}
}

static void
jabber_connected_cb (LmConnection *connection,
		     gboolean      result,
		     GossipJabber *jabber)
{
	GossipJabberPrivate *priv;
	const gchar      *id;
	const gchar      *resource;
	const gchar      *jid_str;
	const gchar      *password;
	gchar            *id_name;
#ifdef USE_RAND_RESOURCE
	gchar            *resource_rand;
	gchar             rand_str[N_RAND_CHAR + 1];
	gint              i;
#endif

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	if (priv->disconnect_request) {
		/* This is so we go no further that way we don't issue
		 * warnings for connections we stopped ourselves.
		 */
		gossip_debug (DEBUG_DOMAIN, "Stopping connecting, disconnecting...");
		gossip_jabber_logout (jabber);
		return;
	}

	if (result == FALSE) {
		gossip_debug (DEBUG_DOMAIN, "Cleaning up connection, disconnecting...");
		gossip_jabber_logout (jabber);

		if (priv->ssl_disconnection) {
			GossipAccount *account;
			GError        *error;
			const gchar   *str;

			account = gossip_jabber_get_account (jabber);
			str = jabber_ssl_status_to_string (priv->ssl_status);
			error = gossip_jabber_error_create (priv->ssl_status, str);
			g_signal_emit_by_name (jabber, "error", account, error);
			g_error_free (error);
		} else {
			gossip_jabber_error (jabber, GOSSIP_JABBER_NO_CONNECTION);
		}

		return;
	}

	gossip_debug (DEBUG_DOMAIN, "Connection open!");

	if (priv->connection_timeout_id != 0) {
		g_source_remove (priv->connection_timeout_id);
		priv->connection_timeout_id = 0;
	}

	id = gossip_account_get_id (priv->account);
	resource = gossip_account_get_resource (priv->account);

	/* Check the saved password */
	password = gossip_account_get_password (priv->account);
	if (G_STR_EMPTY (password)) {
		/* Last, check the temporary password (not saved to disk) */
		password = gossip_account_get_password_tmp (priv->account);
	}

	/* FIXME: Decide on Resource */
	jid_str = id;
	gossip_debug (DEBUG_DOMAIN, "Attempting to use JabberID:'%s'", jid_str);

	id_name = gossip_jid_string_get_part_name (jid_str);

	if (!resource) {
		gossip_debug (DEBUG_DOMAIN, "JID:'%s' is invalid, there is no resource.", jid_str);
		gossip_jabber_logout (jabber);
		gossip_jabber_error (jabber, GOSSIP_JABBER_INVALID_USER);
		return;
	}

#ifdef USE_RAND_RESOURCE
	/* appens a random string to the resource to avoid conflicts */
	for (i = 0; i < N_RAND_CHAR; i++) {
		if (g_random_boolean()) {
			rand_str[i] = g_random_int_range('A', 'Z' + 1);
		} else {
			rand_str[i] = g_random_int_range('0', '9' + 1);
		}
	}
	rand_str[N_RAND_CHAR] = '\0';
	resource_rand = g_strdup_printf ("%s.%s", resource, rand_str);

	lm_connection_authenticate (priv->connection,
				    id_name,
				    password,
				    resource_rand,
				    (LmResultFunction) jabber_auth_cb,
				    jabber, NULL, NULL);

	g_free (resource_rand);
#else
	lm_connection_authenticate (priv->connection,
				    id_name,
				    password,
				    resource,
				    (LmResultFunction) jabber_auth_cb,
				    jabber, NULL, NULL);
#endif

	g_free (id_name);
}

static void
jabber_auth_cb (LmConnection *connection,
		gboolean      result,
		GossipJabber *jabber)
{
	GossipJabberPrivate *priv;
	GossipContact    *own_contact;
	LmMessage        *m;
	LmMessageNode    *node;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	if (result == FALSE) {
		gossip_jabber_logout (jabber);
		gossip_jabber_error (jabber, GOSSIP_JABBER_AUTH_FAILED);
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "Connection logged in!");

	/* Request roster */
	m = lm_message_new_with_sub_type (NULL,
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_GET);
	node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attributes (node,
					"xmlns", XMPP_ROSTER_XMLNS,
					NULL);
	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);

	/* Notify others that we are online */
	m = lm_message_new_with_sub_type (NULL,
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_AVAILABLE);
	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);

	g_signal_emit_by_name (jabber, "connected", priv->account);

	own_contact = gossip_jabber_get_contact_from_jid (jabber,
							  gossip_account_get_id (priv->account),
							  TRUE,
							  FALSE,
							  FALSE);

	/* Request our vcard so we know what our nick name is to use
	 * in chats windows, etc.
	 */
	jabber_contact_get_vcard (jabber, own_contact, TRUE);
}

static void
jabber_disconnected_cb (LmConnection       *connection,
			LmDisconnectReason  reason,
			GossipJabber       *jabber)
{
	GossipJabberPrivate          *priv;
	GossipJabberDisconnectReason  gossip_reason;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	if (priv->connection_timeout_id != 0) {
		g_source_remove (priv->connection_timeout_id);
		priv->connection_timeout_id = 0;
	}

	/* Signal removal of each contact */
	if (priv->contact_list) {
		g_hash_table_foreach_remove (priv->contact_list,
					     jabber_logout_contact_foreach,
					     jabber);
	}

	switch (reason) {
	case LM_DISCONNECT_REASON_OK:
	case LM_DISCONNECT_REASON_RESOURCE_CONFLICT:
		gossip_reason = GOSSIP_JABBER_DISCONNECT_ASKED;
		break;

	case LM_DISCONNECT_REASON_INVALID_XML:
	case LM_DISCONNECT_REASON_PING_TIME_OUT:
	case LM_DISCONNECT_REASON_HUP:
	case LM_DISCONNECT_REASON_ERROR:
	case LM_DISCONNECT_REASON_UNKNOWN:
		gossip_reason = GOSSIP_JABBER_DISCONNECT_ERROR;
		break;
	default:
		gossip_reason = GOSSIP_JABBER_DISCONNECT_ERROR;
		break;
	}

	g_signal_emit_by_name (jabber, "disconnected", priv->account, gossip_reason);
}

static LmHandlerResult
jabber_change_password_message_handler (LmMessageHandler      *handler,
					LmConnection          *connection,
					LmMessage             *m,
					GossipJabberAsyncData *ad)
{
	GossipJabberPrivate *priv;
	LmMessageNode    *node;
	GossipResult      result = GOSSIP_RESULT_OK;
	GError           *error = NULL;

	priv = GOSSIP_JABBER_GET_PRIVATE (ad->jabber);

	if (priv->change_password_cancel) {
		gossip_jabber_async_data_free (ad);
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

	node = lm_message_node_get_child (m->node, "error");
	if (node) {
		GossipJabberError  error_code;
		const gchar         *error_code_str;
		const gchar         *error_message;

		result = GOSSIP_RESULT_ERROR_FAILED;
		error_code_str = lm_message_node_get_attribute (node, "code");

		switch (atoi (error_code_str)) {
		case 401: /* Not Authorized */
		case 407: /* Registration Required */
			error_code = GOSSIP_JABBER_UNAUTHORIZED;
			break;

		case 501: /* Not Implemented */
		case 503: /* Service Unavailable */
			error_code = GOSSIP_JABBER_UNAVAILABLE;
			break;

		case 409: /* Conflict */
			error_code = GOSSIP_JABBER_DUPLICATE_USER;
			break;

		case 408: /* Request Timeout */
		case 504: /* Remote Server Timeout */
			error_code = GOSSIP_JABBER_TIMED_OUT;
			break;

		case 302: /* Redirect */
		case 400: /* Bad Request */
		case 402: /* Payment Required */
		case 403: /* Forbidden */
		case 404: /* Not Found */
		case 405: /* Not Allowed */
		case 406: /* Not Acceptable */
		case 500: /* Internal Server Error */
		case 502: /* Remote Server Error */
		case 510: /* Disconnected */
		default:
			error_code = GOSSIP_JABBER_SPECIFIC_ERROR;
			break;
		};

		error_message = gossip_jabber_error_to_string (error_code);
		error = gossip_jabber_error_create (error_code, error_message);

		gossip_debug (DEBUG_DOMAIN, "Registration failed with error:%s->'%s'",
			      error_code_str, error_message);
	} else {
		gossip_debug (DEBUG_DOMAIN, "Registration success");
	}

	if (ad->callback) {
		(ad->callback) (result,
				error,
				ad->user_data);
	}

	/* Clean up */
	if (error) {
		g_error_free (error);
	}

	gossip_jabber_async_data_free (ad);

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

void
gossip_jabber_change_password (GossipJabber        *jabber,
			       const gchar         *new_password,
			       GossipErrorCallback  callback,
			       gpointer             user_data)
{
	GossipJabberPrivate   *priv;
	GossipJID             *jid;
	const gchar           *id;
	const gchar           *server;
	const gchar           *error_message;
	gboolean               ok;
	GossipJabberAsyncData *ad = NULL;
	LmMessage             *m;
	LmMessageNode         *node;

	g_return_if_fail (GOSSIP_IS_JABBER (jabber));
	g_return_if_fail (new_password != NULL);

	gossip_debug (DEBUG_DOMAIN, "Changing password to '%s'...", new_password);

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	/* Set flags up */
	priv->change_password_cancel = FALSE;

	/* Get credentials */
	g_object_get (priv->account,
		      "id", &id,
		      "server", &server,
		      NULL);

	jid = gossip_jid_new (id);

	/* Create & send message */
        m = lm_message_new_with_sub_type (server, 
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_SET);

        node = lm_message_node_add_child (m->node, "query", NULL);
        lm_message_node_set_attributes (node, "xmlns", XMPP_REGISTER_XMLNS, NULL);
        lm_message_node_add_child (node, "username", gossip_jid_get_part_name (jid));
        lm_message_node_add_child (node, "password", new_password);

	g_object_unref (jid);

	ad = gossip_jabber_async_data_new (jabber, callback, user_data);
	ad->message_handler = lm_message_handler_new ((LmHandleMessageFunction)
						      jabber_change_password_message_handler,
						      ad,
						      NULL);

	ok = lm_connection_send_with_reply (priv->connection, m,
					    ad->message_handler,
					    NULL);
	lm_message_unref (m);

	if (!ok) {
		GError *error;

		error_message = _("Couldn't send message!");
		error = gossip_jabber_error_create (GOSSIP_JABBER_SPECIFIC_ERROR,
						    error_message);

		gossip_debug (DEBUG_DOMAIN, "%s", error_message);

		if (ad->callback) {
			(ad->callback) (GOSSIP_RESULT_ERROR_FAILED,
					error,
					ad->user_data);
		}

		g_error_free (error);
		gossip_jabber_async_data_free (ad);
		return;
	}
}

void
gossip_jabber_change_password_cancel (GossipJabber *jabber)
{
	GossipJabberPrivate *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (jabber));

	gossip_debug (DEBUG_DOMAIN, "Changing password canceled");

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	priv->change_password_cancel = TRUE;
}

void
gossip_jabber_get_avatar_requirements (GossipJabber  *jabber,
				       guint         *min_width,
				       guint         *min_height,
				       guint         *max_width,
				       guint         *max_height,
				       gsize         *max_size,
				       gchar        **format)
{
	if (min_width) {
		*min_width = 32;
	}
	if (min_height) {
		*min_height = 32;
	}
	if (max_width) {
		*max_width = 96;
	}
	if (max_height) {
		*max_height = 96;
	}
	if (max_size) {
		*max_size = 8*1024; /* 8kb */
	}
	if (format) {
		*format = g_strdup ("image/png");
	}
}

gboolean
gossip_jabber_is_connected (GossipJabber *jabber)
{
	GossipJabberPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_JABBER (jabber), FALSE);

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	if (priv->connection == NULL) {
		return FALSE;
	}

	return lm_connection_is_open (priv->connection);
}

gboolean
gossip_jabber_is_connecting (GossipJabber *jabber)
{
	GossipJabberPrivate *priv;
	LmConnectionState  state;

	g_return_val_if_fail (GOSSIP_IS_JABBER (jabber), FALSE);

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	if (priv->connection == NULL) {
		return FALSE;
	}

	state = lm_connection_get_state (priv->connection);
	if (state == LM_CONNECTION_STATE_OPENING) {
		return TRUE;
	}

	return FALSE;
}

gboolean
gossip_jabber_is_ssl_supported (void)
{
	return lm_ssl_is_supported ();
}

gchar *
gossip_jabber_get_default_server (const gchar *username)
{
	GossipJID        *jid;
	const gchar      *str;
	gchar            *server;
	gint              i = 0;

	g_return_val_if_fail (username != NULL, NULL);

	jid = gossip_jid_new (username);
	str = gossip_jid_get_part_host (jid);

	while (server_conversions[i] != NULL) {
		if (g_ascii_strncasecmp (str, server_conversions[i], -1) == 0) {
			str = server_conversions[i + 1];
			break;
		}

		i += 2;
	}

	server = g_strdup (str);
	g_object_unref (jid);

	return server;
}

guint
gossip_jabber_get_default_port (gboolean use_ssl)
{
	if (use_ssl) {
		return LM_CONNECTION_DEFAULT_PORT_SSL;
	}

	return LM_CONNECTION_DEFAULT_PORT;
}

void
gossip_jabber_send_message (GossipJabber *jabber, GossipMessage *message)
{
	GossipJabberPrivate *priv;
	GossipContact    *recipient;
	LmMessage        *m;
	const gchar      *recipient_id;
	const gchar      *resource;
	gchar            *jid_str;

	g_return_if_fail (GOSSIP_IS_JABBER (jabber));

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	recipient = gossip_message_get_recipient (message);

	recipient_id = gossip_contact_get_id (recipient);
	resource = gossip_message_get_explicit_resource (message);

	recipient = gossip_jabber_get_contact_from_jid (jabber,
							recipient_id,
							FALSE,
							FALSE,
							TRUE);

	if (resource && g_utf8_strlen (resource, -1) > 0) {
		jid_str = g_strdup_printf ("%s/%s", recipient_id, resource);
	} else {
		jid_str = g_strdup (recipient_id);
	}

	gossip_debug (DEBUG_DOMAIN, "Sending message to:'%s'", jid_str);

	m = lm_message_new_with_sub_type (jid_str,
					  LM_MESSAGE_TYPE_MESSAGE,
					  LM_MESSAGE_SUB_TYPE_CHAT);

	lm_message_node_add_child (m->node, "body",
				   gossip_message_get_body (message));

	/* If we have had a request for composing then we send the
	 * other side composing details with every message
	 */
	if (gossip_message_is_requesting_composing (message)) {
		LmMessageNode *node;

		node = lm_message_node_add_child (m->node, "x", NULL);
		lm_message_node_set_attribute (node, "xmlns", "jabber:x:event");
		lm_message_node_add_child (node, "composing", NULL);
	}

	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);

	g_free (jid_str);
}

void
gossip_jabber_send_composing (GossipJabber  *jabber,
			      GossipContact *contact,
			      gboolean       typing)
{
	GossipJabberPrivate *priv;
	LmMessage        *m;
	LmMessageNode    *node;
	const gchar      *id = NULL;
	const gchar      *contact_id;
	gchar            *jid_str;

	g_return_if_fail (GOSSIP_IS_JABBER (jabber));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	if (!g_hash_table_lookup (priv->composing_requests, contact)) {
		return;
	}

	contact_id = gossip_contact_get_id (contact);

	gossip_debug (DEBUG_DOMAIN, "Sending %s to contact:'%s'",
		      typing ? "composing" : "not composing",
		      contact_id);

	jid_str = g_strdup (contact_id);

	m = lm_message_new_with_sub_type (jid_str,
					  LM_MESSAGE_TYPE_MESSAGE,
					  LM_MESSAGE_SUB_TYPE_CHAT);
	node = lm_message_node_add_child (m->node, "x", NULL);
	lm_message_node_set_attribute (node, "xmlns", "jabber:x:event");

	if (typing) {
		id = lm_message_node_get_attribute (m->node, "id");

		g_hash_table_insert (priv->composing_ids,
				     g_object_ref (contact),
				     g_strdup (id));

		lm_message_node_add_child (node, "composing", NULL);
		lm_message_node_add_child (node, "id", id);
	} else {
		id = g_hash_table_lookup (priv->composing_ids, contact);
		lm_message_node_add_child (node, "id", id);

		if (id) {
			g_hash_table_remove (priv->composing_ids, contact);
		}
	}

	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);

	g_free (jid_str);
}

void
gossip_jabber_set_presence (GossipJabber *jabber, GossipPresence *presence)
{
	GossipJabberPrivate *priv;
	LmMessage           *m;
	LmMessageNode       *node;
	GossipContact	    *contact;
	GossipPresenceState  state;
	const gchar         *show;
	const gchar         *status;
	const gchar         *priority;
	gchar               *sha1;
	GossipAvatar        *avatar;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	if (priv->presence) {
		g_object_unref (priv->presence);
	}

	priv->presence = g_object_ref (presence);

	m = lm_message_new_with_sub_type (NULL,
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_AVAILABLE);

	state = gossip_presence_get_state (presence);
	status = gossip_presence_get_status (presence);

	show = gossip_jabber_presence_state_to_str (presence);

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

	contact = gossip_jabber_get_own_contact (jabber);
	avatar = gossip_contact_get_avatar (contact);
	if (avatar) {
		sha1 = gossip_sha_hash (avatar->data, avatar->len);
	} else {
		sha1 = gossip_sha_hash (NULL, 0);
	}

	gossip_debug (DEBUG_DOMAIN, "Setting presence to:'%s', status:'%s', "
		      "priority:'%s' sha1:'%s'",
		      show ? show : "available",
		      status ? status : "",
		      priority, sha1);

	if (show) {
		lm_message_node_add_child (m->node, "show", show);
	}

	lm_message_node_add_child (m->node, "priority", priority);

	if (status) {
		lm_message_node_add_child (m->node, "status", status);
	}

	node = lm_message_node_add_child (m->node, "x", "");
	lm_message_node_set_attribute (node, "xmlns", "vcard-temp:x:update");

	/* I can't think of an elegant way of doing this correctly so
	 * I won't. We need to send an empty "x" element before we
	 * download the user's vcard. And an empty "photo" element
	 * when the user chooses not to use an avatar.
	 *
	 * See:
	 * http://www.jabber.org/jeps/jep-0153.html#bizrules-presence
	 */

	lm_message_node_add_child (node, "photo", sha1);
	g_free (sha1);

	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);

	/* Don't forget to set any chatroom status too */
	gossip_jabber_chatrooms_set_presence (priv->chatrooms, priv->presence);
}

void
gossip_jabber_set_subscription (GossipJabber  *jabber,
				GossipContact *contact,
				gboolean       subscribed)
{
	g_return_if_fail (GOSSIP_IS_JABBER (jabber));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	gossip_debug (DEBUG_DOMAIN, "Setting subscription for contact:'%s' as %s",
		      gossip_contact_get_id (contact),
		      subscribed ? "subscribed" : "unsubscribed");

	if (subscribed) {
		gossip_jabber_send_subscribed (jabber, contact);
	} else {
		gossip_jabber_send_unsubscribed (jabber, contact);
	}
}

gboolean
gossip_jabber_set_vcard (GossipJabber    *jabber,
			 GossipVCard     *vcard,
			 GossipCallback   callback,
			 gpointer         user_data,
			 GError          **error)
{
	g_return_val_if_fail (GOSSIP_IS_JABBER (jabber), FALSE);

	gossip_debug (DEBUG_DOMAIN, "Setting vcard for '%s'",
		      gossip_vcard_get_name (vcard));

	return gossip_jabber_vcard_set (jabber,
					vcard,
					callback, user_data,
					error);
}

void
gossip_jabber_add_contact (GossipJabber *jabber,
			   const gchar  *id,
			   const gchar  *name,
			   const gchar  *group,
			   const gchar  *message)
{
	LmMessage          *m;
	LmMessageNode      *node;
	GossipJabberPrivate *priv;
	GossipJID          *jid;
	GossipContact      *contact;
	GossipSubscription  subscription;
	gchar              *escaped;
	gboolean            add_to_roster;

	g_return_if_fail (GOSSIP_IS_JABBER (jabber));

	gossip_debug (DEBUG_DOMAIN, "Adding contact:'%s' with name:'%s' and group:'%s'",
		      id, name, group);

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	jid = gossip_jid_new (id);

	contact = gossip_jabber_get_contact_from_jid (jabber, 
						      gossip_jid_get_without_resource (jid),
						      FALSE,
						      FALSE,
						      FALSE);

	if (contact) {
		subscription = gossip_contact_get_subscription (contact);
	} else {
		subscription = GOSSIP_SUBSCRIPTION_NONE;
	}

	/* We would normally only add to roster IF not on it but we
	 * always do this because the server will inforce it otherwise
	 * and it makes sense to use our provided name/group, etc
	 */
	add_to_roster = TRUE;

	if (add_to_roster) {
		gossip_debug (DEBUG_DOMAIN, "Adding contact:'%s' to roster...", id);

		m = lm_message_new_with_sub_type (NULL,
						  LM_MESSAGE_TYPE_IQ,
						  LM_MESSAGE_SUB_TYPE_SET);

		node = lm_message_node_add_child (m->node, "query", NULL);
		lm_message_node_set_attributes (node,
						"xmlns", XMPP_ROSTER_XMLNS, NULL);

		node = lm_message_node_add_child (node, "item", NULL);

		escaped = g_markup_escape_text (name, -1);
		lm_message_node_set_attributes (node,
						"jid", gossip_jid_get_without_resource (jid),
						"name", escaped,
						NULL);
		g_free (escaped);

		if (group && g_utf8_strlen (group, -1) > 0) {
			escaped = g_markup_escape_text (group, -1);
			lm_message_node_add_child (node, "group", escaped);
			g_free (escaped);
		}

		lm_connection_send (priv->connection, m, NULL);
		lm_message_unref (m);
	}

	/* Request subscription */
	if (subscription == GOSSIP_SUBSCRIPTION_NONE ||
	    subscription == GOSSIP_SUBSCRIPTION_FROM) {
		gossip_debug (DEBUG_DOMAIN, "Sending subscribe request with message:'%s'...",
			      message);

		m = lm_message_new_with_sub_type (gossip_jid_get_without_resource (jid),
						  LM_MESSAGE_TYPE_PRESENCE,
						  LM_MESSAGE_SUB_TYPE_SUBSCRIBE);

		escaped = g_markup_escape_text (message, -1);
		lm_message_node_add_child (m->node, "status", escaped);
		g_free (escaped);

		lm_connection_send (priv->connection, m, NULL);
		lm_message_unref (m);
	} else {
		gossip_debug (DEBUG_DOMAIN, "NOT Sending subscribe request, "
			      "subscription is either TO or BOTH");
	}

	g_object_unref (jid);
}

void
gossip_jabber_rename_contact (GossipJabber  *jabber,
			      GossipContact *contact,
			      const gchar   *new_name)
{
	GossipJabberPrivate *priv;
	LmMessage        *m;
	LmMessageNode    *node;
	gchar            *escaped;
	GList            *l;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	m = lm_message_new_with_sub_type (NULL,
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_SET);
	node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attributes (node,
					"xmlns", XMPP_ROSTER_XMLNS,
					NULL);

	escaped = g_markup_escape_text (new_name, -1);

	node = lm_message_node_add_child (node, "item", NULL);
	lm_message_node_set_attributes (node,
					"jid", gossip_contact_get_id (contact),
					"name", escaped,
					NULL);
	g_free (escaped);

	for (l = gossip_contact_get_groups (contact); l; l = l->next) {
		escaped = g_markup_escape_text (l->data, -1);
		lm_message_node_add_child (node, "group", escaped);
		g_free (escaped);
	}

	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);
}

void
gossip_jabber_remove_contact (GossipJabber  *jabber,
			      GossipContact *contact)
{
	GossipJabberPrivate *priv;
	LmMessage        *m;
	LmMessageNode    *node;

	g_return_if_fail (GOSSIP_IS_JABBER (jabber));

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	/* Next remove the contact from the roster */
	m = lm_message_new_with_sub_type (NULL,
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_SET);

	node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attribute (node,
				       "xmlns",
				       XMPP_ROSTER_XMLNS);

	node = lm_message_node_add_child (node, "item", NULL);
	lm_message_node_set_attributes (node,
					"jid", gossip_contact_get_id (contact),
					"subscription", "remove",
					NULL);

	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);

	/* Remove current subscription */
	gossip_jabber_set_subscription (jabber, contact, FALSE);

	/* Remove from internal hash table */
	gossip_debug (DEBUG_DOMAIN,
		      "Contact:'%s' being removed with current ref count:%d",
		      gossip_contact_get_id (contact), G_OBJECT (contact)->ref_count);
}

void
gossip_jabber_update_contact (GossipJabber *jabber, GossipContact *contact)
{
	/* We set the groups _and_ the name here, the rename function
	 * will do exactly what we want to do so just call that.
	 */
	gossip_jabber_rename_contact (jabber,
				      contact,
				      gossip_contact_get_name (contact));
}

static void
jabber_contact_get_vcard_cb (GossipResult  result,
			     GossipVCard  *vcard,
			     gpointer      user_data)
{
	GossipJabberPrivate *priv;
	JabberData       *data;
	 
	data = user_data;
	priv = GOSSIP_JABBER_GET_PRIVATE (data->jabber);

	if (result == GOSSIP_RESULT_OK) {
		GossipContact *own_contact;
		gchar         *name;
		GossipAvatar  *avatar;

		avatar = gossip_vcard_get_avatar (vcard);
		gossip_contact_set_avatar (data->contact, avatar);
		gossip_contact_set_vcard (data->contact, vcard);

		/* Don't set the name if we are a contact list contact
		 * and the name is already set because the name used
		 * will be exactly what we personally set it to be
		 * ourselves already.
		 */
		if (gossip_contact_get_type (data->contact) != GOSSIP_CONTACT_TYPE_CONTACTLIST || 
		    gossip_contact_get_name (data->contact) == NULL) {
			name = gossip_jabber_get_name_to_use
				(gossip_contact_get_id (data->contact),
				 gossip_vcard_get_nickname (vcard),
				 gossip_vcard_get_name (vcard),
				 gossip_contact_get_name (data->contact));

			gossip_contact_set_name (data->contact, name);
			g_free (name);
		}

		/* Send presence if this is the user's VCard
		 * (Avatar support, JEP-0153)
		 */
		own_contact = gossip_jabber_get_own_contact (data->jabber);
		if (gossip_contact_equal (own_contact, data->contact)) {
			gossip_jabber_send_presence (data->jabber, NULL);
		}

		if (vcard) {
			g_object_ref (vcard);
		}

		g_hash_table_replace (priv->vcards, 
				      g_object_ref (data->contact),
				      vcard);
	}

	jabber_data_free (data);
}

static void
jabber_contact_get_vcard (GossipJabber  *jabber,
			  GossipContact *contact,
			  gboolean       force_update)
{
	GossipJabberPrivate *priv;
	JabberData       *data;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	if (!force_update) {
		/* We use this instead of the regular lookup because
		 * the VCard can be NULL, i.e. if we have requested
		 * it already and are waiting for a response from the
		 * server.
		 */
		if (g_hash_table_lookup_extended (priv->vcards, contact, NULL, NULL)) {
			gossip_debug (DEBUG_DOMAIN, 
				      "Already requested vcard for:'%s'",
				      gossip_contact_get_id (contact));
			return;
		}
	}

	g_hash_table_replace (priv->vcards, 
			      g_object_ref (contact),
			      NULL);

	data = jabber_data_new (jabber, contact, NULL);

	gossip_jabber_vcard_get (jabber,
				 gossip_contact_get_id (contact),
				 jabber_contact_get_vcard_cb,
				 data,
				 NULL);
}

static void
jabber_contact_is_avatar_latest (GossipJabber  *jabber,
				 GossipContact *contact,
				 LmMessageNode *m,
				 gboolean       force_update)
{
	if (!force_update) {
		LmMessageNode *avatar_node;
		GossipAvatar  *avatar;
		gchar         *sha1;
		gboolean       same;

		avatar_node = lm_message_node_find_child (m, "photo");
		if (!avatar_node || !avatar_node->value) {
			gossip_contact_set_avatar (contact, NULL);
			return;
		}

		avatar = gossip_contact_get_avatar (contact);
		if (avatar) {
			sha1 = gossip_sha_hash (avatar->data, avatar->len);
		} else {
			sha1 = gossip_sha_hash (NULL, 0);
		}

		same = g_ascii_strcasecmp (sha1, avatar_node->value) == 0;
		g_free (sha1);

		if (same) {
			return;
		}
	}

	jabber_contact_get_vcard (jabber, contact, force_update);
}

static void
jabber_group_rename_foreach_cb (gpointer key,
				gpointer value,
				gpointer user_data)
{
	GossipJabberPrivate *priv;
	GossipContact    *contact;
	RenameGroupData  *rg;
	LmMessage        *m;
	LmMessageNode    *node;
	gchar            *escaped;
	GList            *l;
	gboolean          found = FALSE;

	contact = GOSSIP_CONTACT (key);
	rg = (RenameGroupData *) user_data;

	priv = GOSSIP_JABBER_GET_PRIVATE (rg->jabber);

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
					"xmlns", XMPP_ROSTER_XMLNS,
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

		/* Do not include the group we are renaming */
		if (group && strcmp (group, rg->group) == 0) {
			continue;
		}

		escaped = g_markup_escape_text (group, -1);
		lm_message_node_add_child (node, "group", escaped);
		g_free (escaped);
	}

	/* Add the new group name */
	escaped = g_markup_escape_text (rg->new_name, -1);
	lm_message_node_add_child (node, "group", escaped);
	g_free (escaped);

	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);
}

void
gossip_jabber_rename_group (GossipJabber *jabber,
			    const gchar  *group,
			    const gchar  *new_name)
{
	GossipJabberPrivate *priv;
	RenameGroupData  *rg;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	rg = g_new0 (RenameGroupData, 1);

	rg->jabber = jabber;
	rg->group = g_strdup (group);
	rg->new_name = g_strdup (new_name);

	g_hash_table_foreach (priv->contact_list,
			      (GHFunc) jabber_group_rename_foreach_cb,
			      rg);

	g_free (rg->group);
	g_free (rg->new_name);
	g_free (rg);
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
		state = gossip_jabber_presence_state_from_str (node->value);
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

const gchar *
gossip_jabber_get_active_resource (GossipJabber  *jabber,
				   GossipContact *contact)
{
	GList          *list;
	GossipPresence *presence;

	list = gossip_contact_get_presence_list (contact);
	if (!list) {
		return NULL;
	}

	/* The first one is the active one. */
	presence = list->data;

	return gossip_presence_get_resource (presence);
}

static void
jabber_get_groups_foreach_cb (gpointer key,
			      gpointer value,
			      gpointer user_data)
{
	GossipContact  *contact;
	GList         **list;
	GList          *l;
	
	contact = GOSSIP_CONTACT (key);
	list = (GList **) user_data;

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

GList *
gossip_jabber_get_groups (GossipJabber *jabber)
{
	GossipJabberPrivate *priv;
	GList            *list = NULL;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	g_hash_table_foreach (priv->contact_list,
			      (GHFunc) jabber_get_groups_foreach_cb,
			      &list);

	list = g_list_sort (list, (GCompareFunc)strcmp);

	return list;
}

gboolean
gossip_jabber_get_vcard (GossipJabber         *jabber,
			 GossipContact        *contact,
			 GossipVCardCallback   callback,
			 gpointer              user_data,
			 GError              **error)
{
	GossipJabberPrivate *priv;
	const gchar      *jid_str;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	if (contact) {
		jid_str = gossip_contact_get_id (contact);
	} else {
		GossipContact *own_contact;

		own_contact = gossip_jabber_get_contact_from_jid (jabber,
								  gossip_account_get_id (priv->account),
								  TRUE,
								  FALSE,
								  FALSE);

		jid_str = gossip_contact_get_id (own_contact);
	}

	return gossip_jabber_vcard_get (jabber,
					jid_str,
					callback, 
					user_data, 
					error);
}

gboolean
gossip_jabber_get_version (GossipJabber           *jabber,
			   GossipContact          *contact,
			   GossipVersionCallback   callback,
			   gpointer                user_data,
			   GError                **error)
{
	GossipJabberPrivate *priv;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	return gossip_jabber_services_get_version (priv->connection,
						   contact,
						   callback, user_data,
						   error);
}

static gboolean
jabber_composing_timeout_cb (JabberData *data)
{
	GossipJabberPrivate *priv;

	priv = GOSSIP_JABBER_GET_PRIVATE (data->jabber);

	gossip_debug (DEBUG_DOMAIN, "Contact:'%s' is NOT composing (timed out)",
		      gossip_contact_get_id (data->contact));

	g_signal_emit_by_name (data->jabber, "composing", data->contact, FALSE);

	g_hash_table_remove (priv->composing_timeouts, data->contact);

	return FALSE;
}

static LmHandlerResult
jabber_message_handler (LmMessageHandler *handler,
			LmConnection     *conn,
			LmMessage        *m,
			GossipJabber     *jabber)
{
	LmMessageNode        *node;
	LmMessageSubType      sub_type;
	GossipJabberPrivate  *priv;
	GossipMessage        *message;
	const gchar          *from_str;
	GossipContact        *from;
	GossipContact        *own_contact;
	const gchar          *thread = NULL;
	const gchar          *subject = NULL;
	const gchar          *body = NULL;
	GossipChatroomInvite *invite;
	const gchar          *resource;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	gossip_debug (DEBUG_DOMAIN, "New message from:'%s'",
		      lm_message_node_get_attribute (m->node, "from"));

	sub_type = lm_message_get_sub_type (m);
	if (sub_type != LM_MESSAGE_SUB_TYPE_NOT_SET &&
	    sub_type != LM_MESSAGE_SUB_TYPE_NORMAL &&
	    sub_type != LM_MESSAGE_SUB_TYPE_CHAT &&
	    sub_type != LM_MESSAGE_SUB_TYPE_HEADLINE) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	from_str = lm_message_node_get_attribute (m->node, "from");

	from = gossip_jabber_get_contact_from_jid (jabber,
						   from_str,
						   FALSE,
						   FALSE,
						   TRUE);

	if (gossip_jabber_get_message_is_event (m)) {
		gboolean composing;

		composing = gossip_jabber_get_message_is_composing (m);

		gossip_debug (DEBUG_DOMAIN, "Contact:'%s' %s",
			      gossip_contact_get_id (from),
			      composing ? "is composing" : "is NOT composing");

		g_signal_emit_by_name (jabber, "composing", from, composing);

		if (composing) {
			JabberData *data;
			guint       id;

			data = g_hash_table_lookup (priv->composing_timeouts, from);
			if (data) {
				g_source_remove (GPOINTER_TO_UINT (data->user_data));
				g_hash_table_remove (priv->composing_timeouts, from);
			}

			data = jabber_data_new (jabber, from, NULL);
			id = g_timeout_add (COMPOSING_TIMEOUT * 1000,
					    (GSourceFunc) jabber_composing_timeout_cb,
					    data);
			data->user_data = GUINT_TO_POINTER (id);
			g_hash_table_insert (priv->composing_timeouts, g_object_ref (from), data);
		} else {
			JabberData *data;

			data = g_hash_table_lookup (priv->composing_timeouts, from);
			if (data) {
				g_source_remove (GPOINTER_TO_UINT (data->user_data));
				g_hash_table_remove (priv->composing_timeouts, from);
			}
		}

		/* If event, then we can ignore the rest */
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	} else {
		gboolean wants_composing;

		wants_composing = gossip_jabber_get_message_is_composing (m);

		if (wants_composing) {
			g_hash_table_insert (priv->composing_requests,
					     g_object_ref (from),
					     GINT_TO_POINTER (TRUE));
		} else {
			g_hash_table_remove (priv->composing_requests, from);
		}

		gossip_debug (DEBUG_DOMAIN, "Contact:'%s' %s composing info...",
			      gossip_contact_get_id (from),
			      wants_composing ? "wants" : "does NOT want");
	}

	node = lm_message_node_get_child (m->node, "subject");
	if (node) {
		subject = node->value;
	}

	invite = gossip_jabber_get_message_conference (jabber, m);
	if (invite) {
		GossipContact *inviter;

		inviter = gossip_chatroom_invite_get_inviter (invite);

		gossip_debug (DEBUG_DOMAIN, "Chat room invitiation from:'%s' for room:'%s', reason:'%s'",
			      gossip_contact_get_id (inviter),
			      gossip_chatroom_invite_get_id (invite),
			      gossip_chatroom_invite_get_reason (invite));

		/* We set the from to be the person that invited you,
		 * not the chatroom that sent the request on their
		 * behalf.
		 */
		from = inviter;

		/* Make sure we have some sort of body for
		 * invitations, since it is not necessary but should
		 * really exist.
		 */
		body = "";
	}

	node = lm_message_node_get_child (m->node, "body");
	if (node) {
		body = node->value;
	}

	if (!body && !invite) {
		/* If no body to the message, we ignore it since it
		 * has no purpose now (see fixes #309912).
		 */

		gossip_debug (DEBUG_DOMAIN, "Dropping new message, no <body> element");
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	node = lm_message_node_get_child (m->node, "thread");
	if (node) {
		thread = node->value;
	}

	own_contact = gossip_jabber_get_contact_from_jid (jabber,
							  gossip_account_get_id (priv->account),
							  TRUE,
							  FALSE,
							  FALSE);

	message = gossip_message_new (GOSSIP_MESSAGE_TYPE_NORMAL, own_contact);

	/* To make the sender right in private chat messages sent from
	 * groupchats, we take the name from the resource, which carries the
	 * nick for those messages.
	 */
	if (gossip_jabber_chatrooms_get_jid_is_chatroom (priv->chatrooms,
							 from_str)) {
		GossipJID   *jid;
		const gchar *resource;

		jid = gossip_jid_new (from_str);
		resource = gossip_jid_get_resource (jid);
		if (!resource) {
			resource = "";
		}

		gossip_contact_set_name (from, resource);

		g_object_unref (jid);
	}

	gossip_message_set_sender (message, from);
	gossip_message_set_body (message, body);

	resource = gossip_jid_string_get_part_resource (from_str);
	gossip_message_set_explicit_resource (message, resource);

	if (subject) {
		gossip_message_set_subject (message, subject);
	}

	if (thread) {
		gossip_message_set_thread (message, thread);
	}

	if (invite) {
		gossip_message_set_invite (message, invite);
		gossip_chatroom_invite_unref (invite);
	}

	gossip_message_set_timestamp (message,
				      gossip_jabber_get_message_timestamp (m));

	g_signal_emit_by_name (jabber, "new-message", message);
	g_signal_emit_by_name (jabber, "composing", from, FALSE);

	g_object_unref (message);

	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static LmHandlerResult
jabber_presence_handler (LmMessageHandler *handler,
			 LmConnection     *conn,
			 LmMessage        *m,
			 GossipJabber     *jabber)
{
	GossipJabberPrivate *priv;
	GossipContact    *contact;
	const gchar      *from;
	const gchar      *type;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	from = lm_message_node_get_attribute (m->node, "from");

	if (gossip_jabber_chatrooms_get_jid_is_chatroom (priv->chatrooms, from)) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	gossip_debug (DEBUG_DOMAIN, "New presence from:'%s'",
		      lm_message_node_get_attribute (m->node, "from"));

	contact = gossip_jabber_get_contact_from_jid (jabber, 
						      from, 
						      FALSE,
						      FALSE, 
						      TRUE);

	/* Get the type */
	type = lm_message_node_get_attribute (m->node, "type");
	if (!type) {
		type = "available";
	}

	if (strcmp (type, "subscribe") == 0) {
		g_signal_emit_by_name (jabber, "subscription-request", contact, NULL);

		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	} else if (strcmp (type, "subscribed") == 0) {
		/* Handled in the roster handling code */
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	} else if (strcmp (type, "unsubscribed") == 0) {
		/* Handled in the roster handling code */
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
			if (gossip_contact_get_presence_list (contact)) {
				/* Check avatar xml tags to see if we
				 * have the latest.
				 */
				jabber_contact_is_avatar_latest (jabber, contact, m->node, FALSE);
			} else {
				/* Force retrieval of the latest avatar for
				 * the contact because no presence information
				 * should mean they were offline.
				 *
				 * The reason we do this is because
				 * some clients don't support the
				 * avatar xml tags (JEP 0153).
				 */
				jabber_contact_is_avatar_latest (jabber, contact, m->node, TRUE);
			}

			gossip_presence_set_resource (presence, resource);
			gossip_contact_add_presence (contact, presence);

			g_object_unref (presence);
		}

		g_object_unref (jid);
	}

	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static LmHandlerResult
jabber_iq_query_handler (LmMessageHandler *handler,
			 LmConnection     *conn,
			 LmMessage        *m,
			 GossipJabber     *jabber)
{
	GossipJabberPrivate *priv;
	LmMessageNode    *node;
	const gchar      *xmlns;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	if (lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_GET &&
	    lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_SET &&
	    lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_RESULT) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}
	
	node = lm_message_node_get_child (m->node, "ping");
	if (node) {
		xmlns = lm_message_node_get_attribute (node, "xmlns");
		if (xmlns && strcmp (xmlns, XMPP_PING_XMLNS) == 0) {
			jabber_request_ping (jabber, m);
			return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
		}	
	}

	node = lm_message_node_get_child (m->node, "query");
	if (!node) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	xmlns = lm_message_node_get_attribute (node, "xmlns");
	if (xmlns && strcmp (xmlns, XMPP_ROSTER_XMLNS) == 0) {
		jabber_request_roster (jabber, m);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	if (xmlns && strcmp (xmlns, XMPP_REGISTER_XMLNS) == 0) {
		/* Do nothing at this point */
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	if (xmlns && strcmp (xmlns, XMPP_VERSION_XMLNS) == 0) {
		jabber_request_version (jabber, m);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	/* If a get, return error for unsupported IQ */
	if (lm_message_get_sub_type (m) == LM_MESSAGE_SUB_TYPE_GET ||
	    lm_message_get_sub_type (m) == LM_MESSAGE_SUB_TYPE_SET) {
		jabber_request_unknown (jabber, m);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
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

	/* Clean up */
	lm_message_handler_invalidate (handler);

	/* Send subscribed to them */
	gossip_debug (DEBUG_DOMAIN, "Sending subscribed message to new service:'%s'", to);
	new_message = lm_message_new_with_sub_type (to,
						    LM_MESSAGE_TYPE_PRESENCE,
						    LM_MESSAGE_SUB_TYPE_SUBSCRIBED);

	own_contact = gossip_jabber_get_own_contact (jabber);
	id = gossip_contact_get_id (own_contact);
	lm_message_node_set_attribute (new_message->node, "from", id);

	lm_connection_send (connection, new_message, NULL);
	lm_message_unref (new_message);

	/* Send our presence */
	new_message = lm_message_new_with_sub_type (to,
						    LM_MESSAGE_TYPE_PRESENCE,
						    LM_MESSAGE_SUB_TYPE_AVAILABLE);

	lm_message_node_set_attribute (new_message->node, "from", id);

	lm_connection_send (connection, new_message, NULL);
	lm_message_unref (new_message);

	g_free (to);

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

/*
 * Requests
 */
static void
jabber_request_version (GossipJabber *jabber,
			LmMessage    *m)
{
	GossipJabberPrivate  *priv;
	LmMessage         *r;
	const gchar       *from, *id;
	LmMessageNode     *node;
	GossipVersionInfo *info;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	from = lm_message_node_get_attribute (m->node, "from");
	id = lm_message_node_get_attribute (m->node, "id");

	gossip_debug (DEBUG_DOMAIN, "Version request from:'%s'", from);

	r = lm_message_new_with_sub_type (from,
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_RESULT);
	if (id) {
		lm_message_node_set_attributes (r->node,
						"id", id,
						NULL);
	}

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

static void
jabber_request_ping (GossipJabber *jabber, LmMessage *m)
{
	GossipJabberPrivate *priv;
	LmMessage        *reply;
	const gchar      *from;
	const gchar      *id;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	from = lm_message_node_get_attribute (m->node, "from");
	id = lm_message_node_get_attribute (m->node, "id");

	reply = lm_message_new_with_sub_type (from,
					      LM_MESSAGE_TYPE_IQ,
					      LM_MESSAGE_SUB_TYPE_RESULT);

	if (id) {
		lm_message_node_set_attributes (reply->node,
						"id", id,
						NULL);
	}
	
	gossip_debug (DEBUG_DOMAIN, "Ping request from:'%s'", from);
	lm_connection_send (priv->connection, reply, NULL);
	lm_message_unref (reply);
}

static void
jabber_request_roster (GossipJabber *jabber,
		       LmMessage    *m)
{
	GossipJabberPrivate *priv;
	LmMessageNode    *node;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	node = lm_message_node_get_child (m->node, "query");
	if (!node) {
		return;
	}

	for (node = node->children; node; node = node->next) {
		GossipContact     *contact;
		GossipContactType  type;
		const gchar       *jid_str;
		const gchar       *subscription;
		gboolean           added_item = FALSE;
		LmMessageNode     *child;
		GList             *groups;
		const gchar       *name;

		if (strcmp (node->name, "item") != 0) {
			continue;
		}

		jid_str = lm_message_node_get_attribute (node, "jid");
		if (!jid_str) {
			continue;
		}

		contact = gossip_jabber_get_contact_from_jid (jabber,
							      jid_str,
							      FALSE,
							      TRUE,
							      FALSE);

		if (!g_hash_table_lookup (priv->contact_list, contact)) {
			g_hash_table_insert (priv->contact_list, 
					     g_object_ref (contact),
					     GINT_TO_POINTER (1));
		}

		type = gossip_contact_get_type (contact);

		/* Subscription */
		subscription = lm_message_node_get_attribute (node, "subscription");
		if (contact && subscription) {
			GossipSubscription subscription_type;

			if (strcmp (subscription, "remove") == 0) {
				g_signal_emit_by_name (jabber, "contact-removed", contact);
				g_hash_table_remove (priv->contact_list, contact);
				continue;
			} else if (strcmp (subscription, "both") == 0) {
				subscription_type = GOSSIP_SUBSCRIPTION_BOTH;
			} else if (strcmp (subscription, "to") == 0) {
				subscription_type = GOSSIP_SUBSCRIPTION_TO;
			} else if (strcmp (subscription, "from") == 0) {
				subscription_type = GOSSIP_SUBSCRIPTION_FROM;
			} else {
				subscription_type = GOSSIP_SUBSCRIPTION_NONE;
			}

			/* In the rare cases where we have this state,
			 * NONE means that we are in the process of
			 * setting up subscription so the contact is
			 * still temporary, any other state and we
			 * assume they must be a proper contact list
			 * contact.
			 *
			 * Also, later when we present the
			 * subscription dialog to the user, we need to
			 * know if user is temporary contact or an old
			 * contact so we can silently accept
			 * subscription requests for people already on
			 * the roster with "to" or "from" conditions.
			 */
			
			added_item = TRUE;

			gossip_contact_set_subscription (contact, subscription_type);
		}

		name = lm_message_node_get_attribute (node, "name");
		if (name) {
			gchar *str;

			str = gossip_markup_unescape_text (name);
			gossip_contact_set_name (contact, str);
			g_free (str);
		}

		groups = NULL;
		for (child = node->children; child; child = child->next) {
			/* FIXME: unescape the markup here: #342927 */
			if (strcmp (child->name, "group") == 0 && child->value) {
				gchar *str;

				str  = gossip_markup_unescape_text (child->value);
				groups = g_list_prepend (groups, str);
			}
		}

		if (groups) {
			groups = g_list_reverse (groups);
			gossip_contact_set_groups (contact, groups);
			g_list_foreach (groups, (GFunc) g_free, NULL);
			g_list_free (groups);
		}

		if (added_item) {
			g_signal_emit_by_name (jabber, "contact-added", contact);
		}
	}
}

static void
jabber_request_unknown (GossipJabber *jabber,
			LmMessage    *m)
{
	GossipJabberPrivate *priv;
	LmMessageNode    *node;

	LmMessage         *new_m;
	const gchar       *from_str, *xmlns;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	/* get details */
	from_str = lm_message_node_get_attribute (m->node, "from");

	node = lm_message_node_get_child (m->node, "query");
	if (!node) {
		return;
	}

	xmlns = lm_message_node_get_attribute (node, "xmlns");

	/* construct response */
	new_m = lm_message_new_with_sub_type (from_str,
					      LM_MESSAGE_TYPE_IQ,
					      LM_MESSAGE_SUB_TYPE_ERROR);

	node = lm_message_node_add_child (new_m->node, "query", NULL);
	lm_message_node_set_attribute (node, "xmlns", xmlns);

	node = lm_message_node_add_child (new_m->node, "error", NULL);
	lm_message_node_set_attribute (node, "type", "cancel");
	lm_message_node_set_attribute (node, "code", "503");

	node = lm_message_node_add_child (node, "service-unavailable", NULL);
	lm_message_node_set_attribute (node, "xmlns", "urn:ietf:params:xml:ns:xmpp-stanzas");

	lm_connection_send (priv->connection, new_m, NULL);

	lm_message_unref (new_m);
}

/*
 * chatrooms
 */

static void
jabber_chatroom_init (GossipChatroomProviderIface *iface)
{
	iface->join            = jabber_chatroom_join;
	iface->cancel          = jabber_chatroom_cancel;
	iface->send            = jabber_chatroom_send;
	iface->change_subject  = jabber_chatroom_change_subject;
	iface->change_nick     = jabber_chatroom_change_nick;
	iface->leave           = jabber_chatroom_leave;
	iface->kick            = jabber_chatroom_kick;
	iface->find_by_id      = jabber_chatroom_find_by_id;
	iface->find            = jabber_chatroom_find;
	iface->invite          = jabber_chatroom_invite;
	iface->invite_accept   = jabber_chatroom_invite_accept;
	iface->invite_decline  = jabber_chatroom_invite_decline;
	iface->get_rooms       = jabber_chatroom_get_rooms;
	iface->browse_rooms    = jabber_chatroom_browse_rooms;
}

static GossipChatroomId
jabber_chatroom_join (GossipChatroomProvider      *provider,
		      GossipChatroom              *chatroom,
		      GossipChatroomJoinCb         callback,
		      gpointer                     user_data)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_JABBER (provider), 0);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), 0);

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	return gossip_jabber_chatrooms_join (priv->chatrooms, chatroom,
					     callback, user_data);
}

static void
jabber_chatroom_cancel (GossipChatroomProvider *provider,
			GossipChatroomId        id)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));
	g_return_if_fail (id >= 1);

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	gossip_jabber_chatrooms_cancel (priv->chatrooms, id);
}

static void
jabber_chatroom_send (GossipChatroomProvider *provider,
		      GossipChatroomId        id,
		      const gchar            *message)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	gossip_jabber_chatrooms_send (priv->chatrooms, id, message);
}

static void
jabber_chatroom_change_subject (GossipChatroomProvider *provider,
				GossipChatroomId        id,
				const gchar            *new_subject)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	gossip_jabber_chatrooms_change_subject (priv->chatrooms, id, new_subject);
}

static void
jabber_chatroom_change_nick (GossipChatroomProvider *provider,
			     GossipChatroomId        id,
			     const gchar            *new_nick)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	gossip_jabber_chatrooms_change_nick (priv->chatrooms, id, new_nick);
}

static void
jabber_chatroom_leave (GossipChatroomProvider *provider,
		       GossipChatroomId        id)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	gossip_jabber_chatrooms_leave (priv->chatrooms, id);
}

static void
jabber_chatroom_kick (GossipChatroomProvider *provider,
		      GossipChatroomId        id,
		      GossipContact          *contact,
		      const gchar            *reason)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	gossip_jabber_chatrooms_kick (priv->chatrooms, id, contact, reason);
}

static GossipChatroom *
jabber_chatroom_find_by_id (GossipChatroomProvider *provider,
			    GossipChatroomId        id)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_JABBER (provider), NULL);

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	return gossip_jabber_chatrooms_find_by_id (priv->chatrooms, id);
}

static GossipChatroom *
jabber_chatroom_find (GossipChatroomProvider *provider,
		      GossipChatroom         *chatroom)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_JABBER (provider), NULL);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	return gossip_jabber_chatrooms_find (priv->chatrooms, chatroom);
}

static void
jabber_chatroom_invite (GossipChatroomProvider *provider,
			GossipChatroomId        id,
			GossipContact          *contact,
			const gchar            *reason)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	gossip_jabber_chatrooms_invite (priv->chatrooms, id, contact, reason);
}

static void
jabber_chatroom_invite_accept (GossipChatroomProvider *provider,
			       GossipChatroomJoinCb    callback,
			       GossipChatroomInvite   *invite,
			       const gchar            *nickname)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	gossip_jabber_chatrooms_invite_accept (priv->chatrooms,
					       callback,
					       invite,
					       nickname);
}

static void
jabber_chatroom_invite_decline (GossipChatroomProvider *provider,
				GossipChatroomInvite   *invite,
				const gchar            *reason)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	gossip_jabber_chatrooms_invite_decline (priv->chatrooms,
						invite,
						reason);
}

static GList *
jabber_chatroom_get_rooms (GossipChatroomProvider *provider)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_JABBER (provider), NULL);

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	return gossip_jabber_chatrooms_get_rooms (priv->chatrooms);
}

static void
jabber_chatroom_browse_rooms (GossipChatroomProvider *provider,
			      const gchar            *server,
			      GossipChatroomBrowseCb  callback,
			      gpointer                user_data)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));
	g_return_if_fail (server != NULL);
	g_return_if_fail (callback != NULL);

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	gossip_jabber_chatrooms_browse_rooms (priv->chatrooms, server, 
					      callback, user_data);
}

/*
 * ft
 */

static void
jabber_ft_init (GossipFTProviderIface *iface)
{
	iface->send    = jabber_ft_send;
	iface->cancel  = jabber_ft_cancel;
	iface->accept  = jabber_ft_accept;
	iface->decline = jabber_ft_decline;
}

static GossipFT *
jabber_ft_send (GossipFTProvider *provider,
		GossipContact    *contact,
		const gchar      *file)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_JABBER (provider), NULL);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);
	g_return_val_if_fail (file != NULL, NULL);

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	return gossip_jabber_ft_send (priv->fts, contact, file);
}

static void
jabber_ft_cancel (GossipFTProvider *provider,
		  GossipFTId        id)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));
	g_return_if_fail (id >= 1);

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	gossip_jabber_ft_cancel (priv->fts, id);
}

static void
jabber_ft_accept (GossipFTProvider *provider,
		  GossipFTId        id)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));
	g_return_if_fail (id >= 1);

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	gossip_jabber_ft_accept (priv->fts, id);
}

static void
jabber_ft_decline (GossipFTProvider *provider,
		   GossipFTId        id)
{
	GossipJabber     *jabber;
	GossipJabberPrivate *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (provider));
	g_return_if_fail (id >= 1);

	jabber = GOSSIP_JABBER (provider);
	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	gossip_jabber_ft_decline (priv->fts, id);
}

/*
 * Misc
 */

static JabberData *
jabber_data_new (GossipJabber  *jabber,
		 GossipContact *contact,
		 gpointer       user_data)
{
	JabberData *data;

	g_return_val_if_fail (GOSSIP_IS_JABBER (jabber), NULL);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	data = g_slice_new0 (JabberData);

	data->jabber = g_object_ref (jabber);
	data->contact = g_object_ref (contact);

	data->user_data = user_data;

	return data;
}

static void
jabber_data_free (gpointer data)
{
	JabberData *jd;

	if (!data) {
		return;
	}

	jd = (JabberData *) data;

	if (jd->jabber) {
		g_object_unref (jd->jabber);
	}

	if (jd->contact) {
		g_object_unref (jd->contact);
	}

	g_slice_free (JabberData, jd);
}

/*
 * External functions
 */

GQuark
gossip_jabber_error_quark (void)
{
	return g_quark_from_static_string (GOSSIP_JABBER_ERROR_DOMAIN);
}

GossipAccount *
gossip_jabber_get_account (GossipJabber *jabber)
{
	GossipJabberPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_JABBER (jabber), NULL);

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	return priv->account;
}

GossipContact *
gossip_jabber_get_own_contact (GossipJabber *jabber)
{
	GossipJabberPrivate *priv;
	GossipContact    *own_contact;

	g_return_val_if_fail (GOSSIP_IS_JABBER (jabber), NULL);

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	own_contact = gossip_jabber_get_contact_from_jid (jabber,
							  gossip_account_get_id (priv->account),
							  TRUE,
							  FALSE,
							  FALSE);

	return own_contact;
}

GossipContact *
gossip_jabber_get_contact_from_jid (GossipJabber *jabber,
				    const gchar  *jid_str,
				    gboolean      own_contact,
				    gboolean      set_permanent,
				    gboolean      get_vcard)
{
	GossipJabberPrivate  *priv;
	GossipContact        *contact;
	GossipContactManager *contact_manager;
	GossipContactType     type;
	GossipJID            *jid;
	gboolean              is_chatroom;
	gboolean              created;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	contact_manager = gossip_session_get_contact_manager (priv->session);

	jid = gossip_jid_new (jid_str);
	is_chatroom = gossip_jabber_chatrooms_get_jid_is_chatroom (priv->chatrooms, jid_str);

	if (is_chatroom) {
		const gchar *resource;

		resource = gossip_jid_get_resource (jid);

		/* If there is no resource, this is the chatroom JID
		 * itself, it isn't a contact. So we don't set the
		 * name. Should we return NULL here as the contact?
		 */
		if (!G_STR_EMPTY (resource)) {
			type = GOSSIP_CONTACT_TYPE_CHATROOM;

			contact = gossip_contact_manager_find_or_create (contact_manager,
									 priv->account,
									 type,
									 gossip_jid_get_full (jid),
									 &created);
			gossip_contact_set_name (contact, resource);

			if (!created && set_permanent) {
				gossip_contact_set_type (contact, type);
			}
		} else {
			contact = NULL;
		}			
	} else {
		if (own_contact) {
			type = GOSSIP_CONTACT_TYPE_USER;
		} else if (set_permanent) {
			type = GOSSIP_CONTACT_TYPE_CONTACTLIST;
		} else {
			type = GOSSIP_CONTACT_TYPE_TEMPORARY;
		}

		contact = gossip_contact_manager_find_or_create (contact_manager,
								 priv->account,
								 type,
								 gossip_jid_get_without_resource (jid),
								 &created);

		if (!created && set_permanent) {
			gossip_contact_set_type (contact, type);
		}
		
		/* Don't get vcards for chatroom contacts */
		if (get_vcard) {
			/* Request contacts VCard details so we can get the
			 * real name for them for chat windows, etc
			 */
			jabber_contact_get_vcard (jabber, contact, FALSE);
		}
	}

	g_object_unref (jid);

	return contact;
}

void
gossip_jabber_send_presence (GossipJabber   *jabber,
			     GossipPresence *presence)
{
	GossipJabberPrivate *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (jabber));

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	gossip_jabber_set_presence (jabber,
				    presence ? presence : priv->presence);
}

void
gossip_jabber_send_subscribed (GossipJabber  *jabber,
			       GossipContact *contact)
{
	GossipJabberPrivate *priv;
	LmMessage        *m;
	const gchar      *id;

	g_return_if_fail (GOSSIP_IS_JABBER (jabber));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	id = gossip_contact_get_id (contact);

	m = lm_message_new_with_sub_type (id,
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_SUBSCRIBED);

	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);
}

void
gossip_jabber_send_unsubscribed (GossipJabber  *jabber,
				 GossipContact *contact)
{
	GossipJabberPrivate *priv;
	LmMessage        *m;
	const gchar      *id;

	g_return_if_fail (GOSSIP_IS_JABBER (jabber));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	id = gossip_contact_get_id (contact);

	m = lm_message_new_with_sub_type (id,
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_UNSUBSCRIBED);

	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);
}

void
gossip_jabber_subscription_allow_all (GossipJabber *jabber)
{
	GossipJabberPrivate *priv;
	LmMessageHandler *handler;

	g_return_if_fail (GOSSIP_IS_JABBER (jabber));

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	handler = priv->subscription_handler;
	if (handler) {
		lm_connection_unregister_message_handler (priv->connection,
							  handler,
							  LM_MESSAGE_TYPE_PRESENCE);
		priv->subscription_handler = NULL;
	}

	/* Set up handler to sliently catch the subscription request */
	handler = lm_message_handler_new
		((LmHandleMessageFunction) jabber_subscription_message_handler,
		 jabber, NULL);

	lm_connection_register_message_handler (priv->connection,
						handler,
						LM_MESSAGE_TYPE_PRESENCE,
						LM_HANDLER_PRIORITY_NORMAL);

	priv->subscription_handler = handler;
}

void
gossip_jabber_subscription_disallow_all (GossipJabber *jabber)
{
	GossipJabberPrivate *priv;

	g_return_if_fail (GOSSIP_IS_JABBER (jabber));

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	if (priv->subscription_handler) {
		lm_connection_unregister_message_handler (priv->connection,
							  priv->subscription_handler,
							  LM_MESSAGE_TYPE_PRESENCE);
		priv->subscription_handler = NULL;
	}
}

/*
 * Private functions
 */

gboolean
_gossip_jabber_set_connection (LmConnection  *connection, 
			       GossipJabber  *jabber,
			       GossipAccount *account)
{
	GossipJID    *jid;
	const gchar  *id;
	const gchar  *server;
	guint16       port;
	gboolean      use_proxy = FALSE;

	g_return_val_if_fail (connection != NULL, FALSE);
	g_return_val_if_fail (GOSSIP_IS_JABBER (jabber), FALSE);
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);

	gossip_debug (DEBUG_DOMAIN,
		      "Setting connection details for account:'%s'", 
		      gossip_account_get_name (account));

	id = gossip_account_get_id (account);
	if (id) {
		gossip_debug (DEBUG_DOMAIN, "- ID:'%s'", id);
		jid = gossip_jid_new (id);
		lm_connection_set_jid (connection, gossip_jid_get_without_resource (jid));
		g_object_unref (jid);
	}


	server = gossip_account_get_server (account);
	if (server) {
		gossip_debug (DEBUG_DOMAIN, "- Server:'%s'", server);
		lm_connection_set_server (connection, server);
	}

	port = gossip_account_get_port (account);
	gossip_debug (DEBUG_DOMAIN, "- Port:%d", port);
	lm_connection_set_port (connection, port);

	/* So here, we do the following:
	 * Either: 
	 * - If we should use old school SSL, set up simple SSL
	 * OR: 
	 * - Attempt to use STARTTLS.
	 * - Require STARTTLS ONLY if the user requires security.
	 * - Ignore SSL errors if we don't require security.
	 */
	if (gossip_account_get_force_old_ssl (account)) {
		LmSSL *ssl;

		gossip_debug (DEBUG_DOMAIN, "- Using OLD SSL method for connection");

		ssl = lm_ssl_new (NULL, 
				  (LmSSLFunction) jabber_ssl_status_cb, 
				  jabber, 
				  NULL);
		lm_connection_set_ssl (connection, ssl);
		lm_ssl_unref (ssl);
	} else {
		LmSSL    *ssl;

		gossip_debug (DEBUG_DOMAIN, "- Using STARTTLS method for connection");

		ssl = lm_ssl_new (NULL, 
				  (LmSSLFunction) jabber_ssl_status_cb, 
				  jabber, 
				  NULL);
		lm_connection_set_ssl (connection, ssl);
		lm_ssl_use_starttls (ssl, TRUE, gossip_account_get_use_ssl (account));
		lm_ssl_unref (ssl);
	}

	if (gossip_account_get_use_proxy (account)) {
		gchar    *host;
		gint      port;
		gchar    *username;
		gchar    *password;
		gboolean  use_auth;

		gossip_debug (DEBUG_DOMAIN, "Using proxy");

		gossip_conf_get_http_proxy (gossip_conf_get (),
					    &use_proxy,
					    &host,
					    &port,
					    &use_auth,
					    &username,
					    &password);

		if (use_proxy) {
			LmProxy  *proxy;

			gossip_debug (DEBUG_DOMAIN, "- Proxy server:'%s', port:%d", host, port);
	
			proxy = lm_proxy_new_with_server (LM_PROXY_TYPE_HTTP,
							  host, 
							  (guint) port);
			lm_connection_set_proxy (connection, proxy);
			
			if (use_auth) {
				gossip_debug (DEBUG_DOMAIN, "- Proxy auth username:'%s'", username);
				lm_proxy_set_username (proxy, username);
				lm_proxy_set_password (proxy, password);
			}
			
			lm_proxy_unref (proxy);
		}

		g_free (host);
		g_free (username);
		g_free (password);
	}

	/* Make sure we reset this even if we aren't using one */
	if (!use_proxy) {
		lm_connection_set_proxy (connection, NULL);
	}

	return TRUE;
}

LmConnection *
_gossip_jabber_new_connection (GossipJabber  *jabber,
			      GossipAccount *account)
{
	LmConnection *connection;
	const gchar  *server;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	server = gossip_account_get_server (account);
	connection = lm_connection_new (server);
 	_gossip_jabber_set_connection (connection, jabber, account);

	return connection;
}

LmConnection *
_gossip_jabber_get_connection (GossipJabber *jabber)
{
	GossipJabberPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_JABBER (jabber), NULL);

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	return priv->connection;
}

GossipSession *
_gossip_jabber_get_session (GossipJabber *jabber)
{
	GossipJabberPrivate *priv;

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	return priv->session;
}

GossipJabberFTs *
_gossip_jabber_get_fts (GossipJabber *jabber)
{
	GossipJabberPrivate *priv;

	g_return_val_if_fail (GOSSIP_IS_JABBER (jabber), NULL);

	priv = GOSSIP_JABBER_GET_PRIVATE (jabber);

	return priv->fts;
}
