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

#include "gossip-account.h"
#include "gossip-jabber.h"
#include "gossip-marshal.h"
#include "gossip-session.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_SESSION, GossipSessionPriv))

typedef struct _GossipSessionPriv  GossipSessionPriv;
struct _GossipSessionPriv {
	GossipJabber   *default_jabber;
	GList          *protocols;

	gchar          *account_name;

	GossipPresence *presence;

	GList          *contacts;
};

static void     gossip_session_class_init     (GossipSessionClass *klass);
static void     gossip_session_init           (GossipSession      *session);
static void     session_finalize              (GObject            *object);
static void     session_connect_protocol      (GossipSession      *session,
					       GossipProtocol     *protocol);

static void     session_protocol_logged_in     (GossipProtocol     *protocol,
						GossipSession      *session);
static void     session_protocol_logged_out    (GossipProtocol     *protocol,
						GossipSession      *session);
static void     session_protocol_new_message   (GossipProtocol     *protocol,
						GossipMessage      *message,
						GossipSession      *session);
static void     session_protocol_contact_added (GossipProtocol     *protocol,
						GossipContact      *contact,
						GossipSession      *session);
static void     session_protocol_contact_updated (GossipProtocol   *protocol,
						  GossipContact    *contact,
						  GossipSession    *session);
static void     
session_protocol_contact_presence_updated        (GossipProtocol    *protocol,
						  GossipContact     *contact,
						  GossipSession     *session);
static void     session_protocol_contact_removed (GossipProtocol    *protocol,
						  GossipContact     *contact,
						  GossipSession     *session);
static gchar *  session_protocol_get_password    (GossipProtocol    *protocol,
						  GossipAccount     *account,
						  GossipSession     *session);
static GossipProtocol *
session_get_protocol                             (GossipSession    *session,
						  GossipContact    *contact);

/* Signals */
enum {
	CONNECTED,
	DISCONNECTED,
	PROTOCOL_CONNECTED,
	PROTOCOL_DISCONNECTED,
	NEW_MESSAGE,
	CONTACT_ADDED,
	CONTACT_UPDATED,
	CONTACT_PRESENCE_UPDATED,
	CONTACT_REMOVED,
	COMPOSING_EVENT,

	/* Used for protocols to retreive information from UI */
	GET_PASSWORD,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE (GossipSession, gossip_session, G_TYPE_OBJECT);

static gpointer parent_class;

static void
gossip_session_class_init (GossipSessionClass *klass)
{
	GObjectClass *object_class;
	
	object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = session_finalize;

	signals[CONNECTED] = 
		g_signal_new ("connected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0, 
			      NULL, NULL,
			      gossip_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[DISCONNECTED] = 
		g_signal_new ("disconnected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0, 
			      NULL, NULL,
			      gossip_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[PROTOCOL_CONNECTED] = 
		g_signal_new ("protocol-connected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE, 
			      1, G_TYPE_POINTER);
	
	signals[PROTOCOL_DISCONNECTED] = 
		g_signal_new ("protocol-disconnected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE, 
			      1, G_TYPE_POINTER);
	
	signals[NEW_MESSAGE] = 
		g_signal_new ("new-message",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	signals[CONTACT_ADDED] = 
		g_signal_new ("contact-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	signals[CONTACT_UPDATED] = 
		g_signal_new ("contact-updated",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	
	signals[CONTACT_PRESENCE_UPDATED] = 
		g_signal_new ("contact-presence-updated",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	signals[CONTACT_REMOVED] = 
		g_signal_new ("contact-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
	signals[COMPOSING_EVENT] =
		g_signal_new ("composing-event",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER_BOOLEAN,
			      G_TYPE_NONE,
			      2, G_TYPE_POINTER, G_TYPE_BOOLEAN);
	signals[GET_PASSWORD] =
		g_signal_new ("get-password",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_STRING__POINTER,
			      G_TYPE_STRING,
			      1, G_TYPE_POINTER);
	
	g_type_class_add_private (object_class, sizeof (GossipSessionPriv));
}

static void
gossip_session_init (GossipSession *session)
{
	GossipSessionPriv *priv;
	GossipProtocol    *protocol;
	
	priv = GET_PRIV (session);
	
	priv->protocols = NULL;

	protocol = g_object_new (GOSSIP_TYPE_JABBER, NULL);

	priv->default_jabber = GOSSIP_JABBER (protocol);
	session_connect_protocol (session, protocol);
	
	priv->protocols = g_list_prepend (priv->protocols, protocol);
}

static void
session_finalize (GObject *object)
{
	GossipSessionPriv *priv;
	GList             *l;

	priv = GET_PRIV (object);
	
	if (priv->protocols) {
		for (l = priv->protocols; l; l = l->next) {
			g_object_unref (G_OBJECT (l->data));
		}
		g_list_free (priv->protocols);
	}
	priv->default_jabber = NULL;

	if (priv->contacts) {
		for (l = priv->contacts; l; l = l->next) {
			g_object_unref (l->data);
		}

		g_list_free (priv->contacts);
	}

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
session_connect_protocol (GossipSession *session, GossipProtocol *protocol)
{
	g_signal_connect (protocol, "logged-in", 
			  G_CALLBACK (session_protocol_logged_in),
			  session);
	g_signal_connect (protocol, "logged-out",
			  G_CALLBACK (session_protocol_logged_out),
			  session);
	g_signal_connect (protocol, "new_message",
			  G_CALLBACK (session_protocol_new_message),
			  session);
	g_signal_connect (protocol, "contact-added",
			  G_CALLBACK (session_protocol_contact_added),
			  session);
	g_signal_connect (protocol, "contact-updated",
			  G_CALLBACK (session_protocol_contact_updated),
			  session);
	g_signal_connect (protocol, "contact-presence-updated",
			  G_CALLBACK (session_protocol_contact_presence_updated),
			  session);
	g_signal_connect (protocol, "contact-removed",
			  G_CALLBACK (session_protocol_contact_removed),
			  session);
	g_signal_connect (protocol, "get-password",
			  G_CALLBACK (session_protocol_get_password),
			  session);
}

static void
session_protocol_logged_in (GossipProtocol *protocol, GossipSession *session)
{
	g_print ("Session: Protocol logged in\n");

	/* Update some status? */
	g_signal_emit (session, signals[PROTOCOL_CONNECTED], 0, protocol);
}

static void
session_protocol_logged_out (GossipProtocol *protocol, GossipSession *session) 
{
	g_print ("Session: Protocol logged out\n");

	/* Update some status? */
	g_signal_emit (session, signals[PROTOCOL_DISCONNECTED], 0, protocol);

	/* For now, we currently only have one protocol */
	g_signal_emit (session, signals[DISCONNECTED], 0);
}

static void
session_protocol_new_message (GossipProtocol *protocol,
			      GossipMessage  *message,
			      GossipSession  *session)
{
	/* Just relay the signal */
	g_signal_emit (session, signals[NEW_MESSAGE], 0, message);
}

static void
session_protocol_contact_added (GossipProtocol *protocol,
				GossipContact  *contact,
				GossipSession  *session)
{
	GossipSessionPriv *priv;

	g_print ("Session: Contact added '%s'\n",
		 gossip_contact_get_name (contact));

	priv = GET_PRIV (session);
	
	priv->contacts = g_list_prepend (priv->contacts, 
					 g_object_ref (contact));
	
	g_signal_emit (session, signals[CONTACT_ADDED], 0, contact);
}

static void
session_protocol_contact_updated (GossipProtocol *protocol,
				  GossipContact  *contact,
				  GossipSession  *session)
{
	g_print ("Session: Contact updated '%s'\n",
		 gossip_contact_get_name (contact));
	g_signal_emit (session, signals[CONTACT_UPDATED], 0, contact);
}

static void     
session_protocol_contact_presence_updated (GossipProtocol *protocol,
					   GossipContact  *contact,
					   GossipSession  *session)
{
	g_print ("Session: Contact presence updated '%s'\n",
		 gossip_contact_get_name (contact));
	g_signal_emit (session, signals[CONTACT_PRESENCE_UPDATED], 0, contact);
}

static void 
session_protocol_contact_removed (GossipProtocol *protocol,
				  GossipContact  *contact,
				  GossipSession  *session)
{
	GossipSessionPriv *priv;
	
	g_print ("Session: Contact removed '%s'\n",
		 gossip_contact_get_name (contact));

	priv = GET_PRIV (session);
	
	g_signal_emit (session, signals[CONTACT_REMOVED], 0, contact);

	priv->contacts = g_list_remove (priv->contacts, contact);
	g_object_unref (contact);
}

static gchar *
session_protocol_get_password (GossipProtocol *protocol,
			       GossipAccount  *account,
			       GossipSession  *session)
{
	gchar *password;

	g_print ("Get password\n");

	g_signal_emit (session, signals[GET_PASSWORD], 0, account, &password);
	
	return password;
}

static GossipProtocol *
session_get_protocol (GossipSession *session, GossipContact *contact)
{
	GossipSessionPriv *priv;
	
	/* FIXME: Look up which protocol backend handles a certain contact */

	priv = GET_PRIV (session);

	return GOSSIP_PROTOCOL (priv->default_jabber);
}

GossipSession *
gossip_session_new (void)
{
	return g_object_new (GOSSIP_TYPE_SESSION, NULL);
}

void
gossip_session_connect (GossipSession *session)
{
	GossipSessionPriv *priv;
	GList            *l;
	
	g_return_if_fail (GOSSIP_IS_SESSION (session));

	priv = GET_PRIV (session);

	/* Temporary */
	priv->presence = gossip_presence_new_full (GOSSIP_PRESENCE_STATE_AVAILABLE, 
						   NULL);

	/* Need to be async, start connection here */
	for (l = priv->protocols; l; l = l->next) {
		GossipProtocol *protocol;

		protocol = GOSSIP_PROTOCOL (l->data);
		/* Setup the network connection */
		gossip_protocol_login (protocol);
	}
}

void
gossip_session_disconnect (GossipSession *session)
{
	GossipSessionPriv *priv;
	GList             *l;

	g_return_if_fail (GOSSIP_IS_SESSION (session));

	priv = GET_PRIV (session);

	for (l = priv->protocols; l; l = l->next) {
		GossipProtocol *protocol;

		protocol = GOSSIP_PROTOCOL (l->data);

		gossip_protocol_logout (protocol);
	}
}

void 
gossip_session_send_message (GossipSession *session, GossipMessage *message)
{
	GossipSessionPriv *priv;
	GossipContact     *contact;
	GList             *l;

	g_return_if_fail (GOSSIP_IS_SESSION (session));
	g_return_if_fail (message != NULL);

	priv = GET_PRIV (session);
	
	contact = gossip_message_get_recipient (message);

	/* FIXME: New to solve how to see which protocol to send through */
	for (l = priv->protocols; l; l = l->next) {
		GossipProtocol *protocol = GOSSIP_PROTOCOL (l->data);

		gossip_protocol_send_message (protocol, message);
	}
}

void
gossip_session_send_composing (GossipSession  *session,
			       GossipContact  *contact,
			       gboolean        composing)
{
	/* FIXME: Implement */
}

GossipPresence *
gossip_session_get_presence (GossipSession *session)
{
	GossipSessionPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);

	priv = GET_PRIV (session);
	
	return priv->presence;
}

void 
gossip_session_set_presence (GossipSession *session, GossipPresence *presence)
{
	/* Go over all protocols and set presence */
}

gboolean
gossip_session_is_connected (GossipSession *session)
{
	GossipSessionPriv *priv;
	GList *l;

	priv = GET_PRIV (session);
	
	for (l = priv->protocols; l; l = l->next) {
		if (gossip_protocol_is_connected (GOSSIP_PROTOCOL (l->data))) {
			return TRUE;
		}
	}
		
	return FALSE;
}

const gchar *
gossip_session_get_active_resource (GossipSession *session,
				    GossipContact *contact)
{
	GossipSessionPriv *priv;
	GossipProtocol    *protocol;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);
	
	/* Get the activate resource, needed to be able to lock the chat 
	 * against a certain resource 
	 */

	priv = GET_PRIV (session);

	protocol = session_get_protocol (session, contact);
	if (!protocol) {
		return NULL;
	}
	
	return gossip_protocol_get_active_resource (protocol, contact);
}

GossipChatroomProvider *
gossip_session_get_chatroom_provider (GossipSession *session)
{
	GossipSessionPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);

	priv = GET_PRIV (session);

	return GOSSIP_CHATROOM_PROVIDER (priv->default_jabber);
}

GossipContact *
gossip_session_find_contact (GossipSession *session, const gchar *str)
{
	/* FIXME: Implement */

	return NULL;
}

void
gossip_session_add_contact (GossipSession *session,
                            const gchar   *id,
                            const gchar   *name,
                            const gchar   *group,
                            const gchar   *message)
{
	GossipSessionPriv *priv;

	g_return_if_fail (GOSSIP_IS_SESSION (session));
        g_return_if_fail (id != NULL);
        g_return_if_fail (name != NULL);
        g_return_if_fail (group != NULL);
        g_return_if_fail (message != NULL);

	priv = GET_PRIV (session);

        /* FIXME: Lookup the correct protocol */
        gossip_protocol_add_contact (GOSSIP_PROTOCOL (priv->default_jabber),
                                     id, name, group, message);
}

void
gossip_session_rename_contact (GossipSession *session,
                               GossipContact *contact,
                               const gchar   *new_name)
{
	GossipSessionPriv *priv;
	GossipProtocol    *protocol;

	g_return_if_fail (GOSSIP_IS_SESSION (session));
        g_return_if_fail (GOSSIP_IS_CONTACT (contact));
        g_return_if_fail (new_name != NULL);
	
	/* Get the activate resource, needed to be able to lock the chat 
	 * against a certain resource 
	 */

	priv = GET_PRIV (session);

	protocol = session_get_protocol (session, contact);
	if (!protocol) {
		return;
	}
	
        gossip_protocol_rename_contact (protocol, contact, new_name);
}

void
gossip_session_remove_contact (GossipSession *session,
                               GossipContact *contact)
{
	GossipSessionPriv *priv;
	GossipProtocol    *protocol;

	g_return_if_fail (GOSSIP_IS_SESSION (session));
        g_return_if_fail (GOSSIP_IS_CONTACT (contact));
	
	/* Get the activate resource, needed to be able to lock the chat 
	 * against a certain resource 
	 */

	priv = GET_PRIV (session);

	protocol = session_get_protocol (session, contact);
	if (!protocol) {
		return;
	}
	
        gossip_protocol_remove_contact (protocol, contact);
}

void
gossip_session_update_contact (GossipSession *session,
                               GossipContact *contact)
{
        /* FIXME: Implement */
}

const GList *
gossip_session_get_contacts (GossipSession *session)
{
	GossipSessionPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);

	priv = GET_PRIV (session);

	return priv->contacts;
}

GList *
gossip_session_get_groups (GossipSession *session)
{
	/* FIXME: Implement */
	
	return NULL;
}

const gchar *
gossip_session_get_nickname (GossipSession *session)
{
	GossipSessionPriv *priv;
	GossipContact     *contact;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), "");

	priv = GET_PRIV (session);

	contact = gossip_jabber_get_own_contact (priv->default_jabber);

	return gossip_contact_get_name (contact);
}

gboolean
gossip_session_async_register (GossipSession  *session,
                               GossipAccountType type,
                               const gchar    *id,
                               const gchar    *password,
                               gboolean        use_ssl,
                               GossipAsyncRegisterCallback callback,
                               gpointer        user_data,
                               GError        **error)
{
	GossipSessionPriv *priv;
	
	g_return_val_if_fail (GOSSIP_IS_SESSION (session), FALSE);
	g_return_val_if_fail (id != NULL, FALSE);
        g_return_val_if_fail (password != NULL, FALSE);
	g_return_val_if_fail (callback != NULL, FALSE);

	priv = GET_PRIV (session);
	
	/* FIXME: Call set_vcard on all protocols?
	 *        Currently only calling it on the main Jabber account
	 */
#if 0
        return gossip_protocol_async_register (GOSSIP_PROTOCOL (priv->default_jabber),
                                               type,
                                               id,
                                               password,
                                               use_ssl,
                                               callback,
                                               user_data,
                                               error);
#endif
        return TRUE;
}

gboolean
gossip_session_async_get_vcard (GossipSession             *session,
				GossipContact             *contact,
				GossipAsyncVCardCallback   callback,
				gpointer                   user_data,
				GError                   **error)
{
	GossipSessionPriv *priv;
	GossipProtocol    *protocol;
	
	g_return_val_if_fail (GOSSIP_IS_SESSION (session), FALSE);
	g_return_val_if_fail (callback != NULL, FALSE);

	priv = GET_PRIV (session);
	
	if (contact) {
		protocol = session_get_protocol (session, contact);
	} else {
		/* Requesting users vCard, asking the default jabber account
		 */
		protocol = GOSSIP_PROTOCOL (priv->default_jabber);
	}

	if (!protocol) {
		return FALSE;
	}

	return gossip_protocol_async_get_vcard (protocol, contact,
						callback, user_data,
						error);
}

/* Returns TRUE if all backends succeeded in setting the vcard */
gboolean
gossip_session_async_set_vcard (GossipSession  *session,
				GossipVCard    *vcard,
				GossipAsyncResultCallback callback,
				gpointer        user_data,
				GError         **error)
{
	GossipSessionPriv *priv;
	
	g_return_val_if_fail (GOSSIP_IS_SESSION (session), FALSE);
	g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), FALSE);
	g_return_val_if_fail (callback != NULL, FALSE);
	
	priv = GET_PRIV (session);
	
	/* FIXME: Call set_vcard on all protocols?
	 *        Currently only calling it on the main Jabber account
	 */
	return gossip_protocol_async_set_vcard (GOSSIP_PROTOCOL (priv->default_jabber),
						vcard,
						callback,
						user_data,
						error);
}

gboolean
gossip_session_async_get_version (GossipSession               *session,
				  GossipContact               *contact,
				  GossipAsyncVersionCallback   callback,
				  gpointer                     user_data,
				  GError                     **error)
{
	GossipProtocol *protocol;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);
	g_return_val_if_fail (callback != NULL, FALSE);
	
	protocol = session_get_protocol (session, contact);

	if (!protocol) {
		return FALSE;
	}
	
	return gossip_protocol_async_get_version (protocol, contact, 
						  callback, user_data,
						  error);
}

