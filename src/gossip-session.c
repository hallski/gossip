/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio HB
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

#include <loudmouth/loudmouth.h>

#include "gossip-account.h"
#include "gossip-jabber.h"
#include "gossip-marshal.h"
#include "gossip-session.h"

struct _GossipSessionPriv {
	LmConnection  *lm_conn;
	GossipAccount *lm_account;
	
	GList *protocols;
	GList *contacts;
};

static void     session_class_init            (GossipSessionClass *klass);
static void     session_init                  (GossipSession      *session);
static void     session_finalize              (GObject            *obj);

static void     session_lm_login              (GossipSession      *session);
static void     session_lm_init               (GossipSession      *session);
static void     session_lm_connection_open_cb (LmConnection       *connection,
					       gboolean            result,
					       GossipSession      *session);
static void     session_lm_connection_auth_cb (LmConnection       *connection,
					       gboolean            result,
					       GossipSession      *session);
#if 0
static void     session_lm_disconnected_cb    (LmConnection       *connection,
					       LmDisconnectReason  reason,
					       GossipSession      *session); 
#endif
static LmSSLResponse
session_lm_ssl_func                           (LmConnection       *connection,
					       LmSSLStatus         status,
					       GossipSession      *session);
static LmHandlerResult
session_lm_message_handler                    (LmMessageHandler   *handler,
					       LmConnection       *conn,
					       LmMessage          *message,
					       GossipSession      *session);
static LmHandlerResult
session_lm_presence_handler                   (LmMessageHandler   *handler,
					       LmConnection       *conn,
					       LmMessage          *message,
					       GossipSession      *session);
static LmHandlerResult
session_lm_iq_handler                         (LmMessageHandler   *handler,
					       LmConnection       *conn,
					       LmMessage          *message,
					       GossipSession      *session); 
static void
session_lm_set_presence                       (GossipSession      *session,
					       GossipPresence     *presence);

/* Signals */
enum {
	NEW_MESSAGE,
	PRESENCE_UPDATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

GType
gossip_session_get_type (void)
{
	static GType object_type = 0;
	
	if (!object_type) {
		static const GTypeInfo object_info = {
			sizeof (GossipSessionClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) session_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GossipSession),
			0,              /* n_preallocs */
			(GInstanceInitFunc) session_init,
		};

		object_type = g_type_register_static (G_TYPE_OBJECT,
						      "GossipSession",
                                                      &object_info, 0);
	}

	return object_type;
}

static void
session_class_init (GossipSessionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = session_finalize;

	signals[NEW_MESSAGE] = 
		g_signal_new ("new-message",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	signals[PRESENCE_UPDATED] =
		g_signal_new ("gossip-updated",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE,
			      2, G_TYPE_POINTER, G_TYPE_POINTER);
}

static void
session_init (GossipSession *session)
{
	GossipSessionPriv *priv;
	
	priv = g_new0 (GossipSessionPriv, 1);
	session->priv = priv;
	
	priv->protocols = NULL;
	priv->contacts = NULL;

	session_lm_init (session);
}

static void
session_finalize (GObject *obj)
{
	GossipSession     *session;
	GossipSessionPriv *priv;

	session = GOSSIP_SESSION (obj);
	priv = session->priv;
}

static void
session_lm_init (GossipSession *session)
{
	GossipSessionPriv *priv;
	LmMessageHandler  *handler;
	priv  = session->priv;
	
	/* FIXME: Don't hard code */
	priv->lm_account = gossip_account_get_default ();
	
	priv->lm_conn = lm_connection_new (priv->lm_account->server);
	lm_connection_set_port (priv->lm_conn, priv->lm_account->port);

	handler = lm_message_handler_new ((LmHandleMessageFunction) session_lm_message_handler,
					  session, NULL);
	lm_connection_register_message_handler (priv->lm_conn,
						handler,
						LM_MESSAGE_TYPE_MESSAGE,
						LM_HANDLER_PRIORITY_FIRST);
	lm_message_handler_unref (handler);

	handler = lm_message_handler_new ((LmHandleMessageFunction) session_lm_presence_handler,
					  session, NULL);
	lm_connection_register_message_handler (priv->lm_conn,
						handler,
						LM_MESSAGE_TYPE_PRESENCE,
						LM_HANDLER_PRIORITY_FIRST);
	lm_message_handler_unref (handler);

	handler = lm_message_handler_new ((LmHandleMessageFunction) session_lm_iq_handler,
					  session, NULL);
	lm_connection_register_message_handler (priv->lm_conn,
						handler,
						LM_MESSAGE_TYPE_IQ,
						LM_HANDLER_PRIORITY_FIRST);
	lm_message_handler_unref (handler);
}

static void
session_lm_login (GossipSession *session)
{
	GossipSessionPriv *priv;
	gboolean           result;
	GError            *error;
	
	priv = session->priv;

	if (priv->lm_account->use_ssl) {
		LmSSL *ssl = lm_ssl_new (NULL,
					 (LmSSLFunction) session_lm_ssl_func,
					 session, NULL);
		lm_connection_set_ssl (priv->lm_conn, ssl);
		lm_ssl_unref (ssl);
	}

	result = lm_connection_open (priv->lm_conn, 
				     (LmResultFunction) session_lm_connection_open_cb,
				     session, NULL, &error);

	if (result == FALSE && error) {
		/* Handle error */
		g_error_free (error);
	}
}

static void
session_lm_connection_open_cb (LmConnection  *connection,
			       gboolean       result,
			       GossipSession *session)
{
	GossipSessionPriv *priv;

	priv = session->priv;

	/* Don't hard code password here */
	lm_connection_authenticate (priv->lm_conn,
				    priv->lm_account->username,
				    priv->lm_account->password,
				    priv->lm_account->resource,
				    (LmResultFunction) session_lm_connection_auth_cb,
				    session, NULL, NULL);
}

static void
session_lm_connection_auth_cb (LmConnection  *connection,
			       gboolean       result,
			       GossipSession *session)
{
	g_print ("LOGGED IN!\n");
}

#if 0
static void
session_lm_disconnected_cb (LmConnection       *connection,
			    LmDisconnectReason  reason,
			    GossipSession      *session)
{
}
#endif

static LmSSLResponse
session_lm_ssl_func (LmConnection  *connection,
		     LmSSLStatus    status,
		     GossipSession *session)
{
	return LM_SSL_RESPONSE_CONTINUE;
}

static LmHandlerResult
session_lm_message_handler (LmMessageHandler *handler,
			    LmConnection     *conn,
			    LmMessage        *message,
			    GossipSession    *session)
{
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult
session_lm_presence_handler (LmMessageHandler *handler,
			     LmConnection     *conn,
			     LmMessage        *message,
			     GossipSession    *session)
{
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult
session_lm_iq_handler (LmMessageHandler *handler,
		       LmConnection     *conn,
		       LmMessage        *message,
		       GossipSession    *session)
{
	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static void
session_lm_set_presence (GossipSession  *session,
			 GossipPresence *presence)
{
	GossipSessionPriv  *priv;
	LmMessage          *m;
	GossipPresenceType  type;
	const gchar        *show = NULL;
	const gchar        *priority;
	
	priv = session->priv;
	
	m = lm_message_new_with_sub_type (NULL, 
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_AVAILABLE);

	type = gossip_presence_get_type (presence);
	show = gossip_jabber_presence_type_to_string (presence);
	
	switch (type) {
	case GOSSIP_PRESENCE_TYPE_BUSY:
		priority = "40";
		break;
	case GOSSIP_PRESENCE_TYPE_AWAY:
		priority = "30";
		break;
	case GOSSIP_PRESENCE_TYPE_EXT_AWAY:
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
	
	lm_connection_send (priv->lm_conn, m, NULL);
	lm_message_unref (m);
}

void
gossip_session_login (GossipSession *session)
{
	GossipSessionPriv *priv;
	GList            *l;
	
	g_return_if_fail (GOSSIP_IS_SESSION (session));

	priv = session->priv;
	/* Connect jabber */
	session_lm_login (session);

	/* Need to be async, start connection here */
	for (l = priv->protocols; l; l = l->next) {
	/*	GossipProtocol *protocol;

		protocol = GOSSIP_PROTOCOL (l->data);

		gossip_protocol_login (protocol);*/
	}
}

void 
gossip_session_send_msg (GossipSession *session,
			 GossipContact *contact,
			 const char     *message)
{
/*	GossipProtocol *protocol;*/
	
	if (gossip_contact_get_type (contact) == GOSSIP_CONTACT_TYPE_JABBER) {
		/* Send through Jabber */
	}
/*
	protocol = session_get_protocol (gossip_contact_get_type (contact));

	gossip_protocol_send_msg (protocol, 
				  gossip_contact_get_id (contact),
				  message);
				  */
}

void 
gossip_session_set_presence (GossipSession *session, GossipPresence *presence)
{
	/* Go over all protocols and set presence */

	session_lm_set_presence (session, presence);

}


