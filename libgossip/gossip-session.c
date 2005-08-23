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
#include "gossip-account.h"
#include "gossip-protocol.h"

/* Temporary */
#include "gossip-jabber.h"

#include "gossip-session.h"

#define d(x)

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_SESSION, GossipSessionPriv))


typedef struct _GossipSessionPriv  GossipSessionPriv;


struct _GossipSessionPriv {
 	GossipAccountManager *account_manager; 

	GHashTable           *accounts;
	GList                *protocols;

	GossipPresence       *presence;

	GList                *contacts;

	guint                 connected_counter;
};


typedef struct {
	gchar *contact_id;
	gchar *account_name;
} FindAccount;


typedef struct {
	GossipSession *session;
	GList         *accounts;
} GetAccounts;


typedef struct {
	GossipSession *session;
	gboolean       startup;
} ConnectAccounts;


static void            gossip_session_class_init                 (GossipSessionClass  *klass);
static void            gossip_session_init                       (GossipSession       *session);
static void            session_finalize                          (GObject             *object);
static void            session_protocol_signals_setup            (GossipSession       *session,
								  GossipProtocol      *protocol);
static void            session_protocol_logged_in                (GossipProtocol      *protocol,
								  GossipAccount       *account,
								  GossipSession       *session);
static void            session_protocol_logged_out               (GossipProtocol      *protocol,
								  GossipAccount       *account,
								  GossipSession       *session);
static void            session_protocol_new_message              (GossipProtocol      *protocol,
								  GossipMessage       *message,
								  GossipSession       *session);
static void            session_protocol_contact_added            (GossipProtocol      *protocol,
								  GossipContact       *contact,
								  GossipSession       *session);
static void            session_protocol_contact_updated          (GossipProtocol      *protocol,
								  GossipContact       *contact,
								  GossipSession       *session);
static void            session_protocol_contact_presence_updated (GossipProtocol      *protocol,
								  GossipContact       *contact,
								  GossipSession       *session);
static void            session_protocol_contact_removed          (GossipProtocol      *protocol,
								  GossipContact       *contact,
								  GossipSession       *session);
static gchar *         session_protocol_get_password             (GossipProtocol      *protocol,
								  GossipAccount       *account,
								  GossipSession       *session);
static void            session_protocol_error                    (GossipProtocol      *protocol,
								  GError              *error,
								  GossipSession       *session);
static GossipProtocol *session_get_protocol                      (GossipSession       *session,
								  GossipContact       *contact);
static void            session_get_accounts_foreach_cb           (const gchar         *account_name,
								  GossipProtocol      *protocol,
								  GetAccounts         *data);
static void            session_find_account_foreach_cb           (const gchar         *account_name,
								  GossipProtocol      *protocol,
								  FindAccount         *fa);
static void            session_connect                           (GossipSession       *session,
								  GossipAccount       *account);
static void            session_connect_foreach_cb                (gchar               *account_name,
								  GossipProtocol      *protocol,
								  ConnectAccounts     *data);


/* signals */
enum {
	CONNECTED,
	DISCONNECTED,
	PROTOCOL_CONNECTED,
	PROTOCOL_DISCONNECTED,
	PROTOCOL_ERROR,
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


G_DEFINE_TYPE (GossipSession, gossip_session, G_TYPE_OBJECT);


static guint    signals[LAST_SIGNAL] = {0};
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
			      libgossip_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[DISCONNECTED] = 
		g_signal_new ("disconnected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0, 
			      NULL, NULL,
			      libgossip_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[PROTOCOL_CONNECTED] = 
		g_signal_new ("protocol-connected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 
			      2, G_TYPE_POINTER, G_TYPE_POINTER);
	
	signals[PROTOCOL_DISCONNECTED] = 
		g_signal_new ("protocol-disconnected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 
			      2, G_TYPE_POINTER, G_TYPE_POINTER);
	
	signals[PROTOCOL_ERROR] = 
		g_signal_new ("protocol-error",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      libgossip_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 
			      2, G_TYPE_POINTER, G_TYPE_POINTER);

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
	
	g_type_class_add_private (object_class, sizeof (GossipSessionPriv));
}

static void
gossip_session_init (GossipSession *session)
{
	GossipSessionPriv *priv;

	priv = GET_PRIV (session);
	
	priv->accounts = g_hash_table_new_full (g_str_hash, 
						g_str_equal,
						g_free,
						g_object_unref);

	priv->protocols = NULL;

	priv->connected_counter = 0;

#ifdef HAVE_DBUS
	gossip_dbus_init (session);
#endif
}

static void
session_finalize (GObject *object)
{
	GossipSessionPriv *priv;
	GList             *l;

	priv = GET_PRIV (object);
	
	if (priv->accounts) {
		g_hash_table_destroy (priv->accounts);
	}

	if (priv->protocols) {
		g_list_foreach (priv->protocols, 
				(GFunc)g_object_unref,
				NULL);
		g_list_free (priv->protocols);
	}

	if (priv->contacts) {
		for (l = priv->contacts; l; l = l->next) {
			g_object_unref (l->data);
		}

		g_list_free (priv->contacts);
	}

 	if (priv->account_manager) { 
 		g_object_unref (priv->account_manager); 
 	} 

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
session_protocol_signals_setup (GossipSession  *session, 
				GossipProtocol *protocol)
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
	g_signal_connect (protocol, "error", 
			  G_CALLBACK (session_protocol_error),
			  session);
}

static void
session_protocol_logged_in (GossipProtocol *protocol,
			    GossipAccount  *account,
			    GossipSession  *session)
{
	GossipSessionPriv *priv;
	
	d(g_print ("Session: Protocol logged in\n"));

	priv = GET_PRIV (session);
	
	/* Update some status? */
	priv->connected_counter++;

	g_signal_emit (session, signals[PROTOCOL_CONNECTED], 0, account, protocol);

	if (priv->connected_counter == 1) {
		/* Before this connect the session was set to be DISCONNECTED */
		g_signal_emit (session, signals[CONNECTED], 0);
	}
}

static void
session_protocol_logged_out (GossipProtocol *protocol, 
			     GossipAccount  *account,
			     GossipSession  *session) 
{
	GossipSessionPriv *priv;
	
	d(g_print ("Session: Protocol logged out\n"));

	priv = GET_PRIV (session);
	
	/* Update some status? */
	if (priv->connected_counter < 0) {
		g_warning ("We have some issues in the connection counting");
		return;
	}

	priv->connected_counter--;
	
	g_signal_emit (session, signals[PROTOCOL_DISCONNECTED], 0, account, protocol);
	
	if (priv->connected_counter == 0) {
		/* Last connected protocol was disconnected */
		g_signal_emit (session, signals[DISCONNECTED], 0);
	}
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

	d(g_print ("Session: Contact added '%s'\n",
		   gossip_contact_get_name (contact)));

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
	d(g_print ("Session: Contact updated '%s'\n",
		   gossip_contact_get_name (contact)));

	g_signal_emit (session, signals[CONTACT_UPDATED], 0, contact);
}

static void     
session_protocol_contact_presence_updated (GossipProtocol *protocol,
					   GossipContact  *contact,
					   GossipSession  *session)
{
	d(g_print ("Session: Contact presence updated '%s'\n",
		   gossip_contact_get_name (contact)));
	g_signal_emit (session, signals[CONTACT_PRESENCE_UPDATED], 0, contact);
}

static void 
session_protocol_contact_removed (GossipProtocol *protocol,
				  GossipContact  *contact,
				  GossipSession  *session)
{
	GossipSessionPriv *priv;
	
	d(g_print ("Session: Contact removed '%s'\n",
		   gossip_contact_get_name (contact)));

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

	d(g_print ("Session: Get password\n"));

	g_signal_emit (session, signals[GET_PASSWORD], 0, account, &password);
	
	return password;
}

static void
session_protocol_error (GossipProtocol *protocol,
			GError         *error,
			GossipSession  *session)
{

	d(g_print ("Session: Error:%d->'%s'\n", 
		   error->code, error->message));

	g_signal_emit (session, signals[PROTOCOL_ERROR], 0, protocol, error); 
}

static GossipProtocol *
session_get_protocol (GossipSession *session, 
		      GossipContact *contact)
{
	GossipSessionPriv *priv;
	GList             *l;
	const gchar       *id;
	
	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (session);

	id = gossip_contact_get_id (contact);

	for (l = priv->protocols; l; l = l->next) {
		GossipProtocol *protocol;
		GossipContact  *this_contact;
		
		protocol = l->data;

		if (!GOSSIP_IS_PROTOCOL (protocol)) {
			continue;
		}

		this_contact = gossip_protocol_find_contact (protocol, id);
		if (!this_contact) {
			continue;
		}
	     
		if (gossip_contact_equal (this_contact, contact)) {
			return protocol;
		}
	}

	return NULL;
}

GossipSession *
gossip_session_new (GossipAccountManager *manager)
{
	GossipSession     *session;
	GossipSessionPriv *priv;
	GList             *accounts, *l;
 
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (manager), NULL);

	session = g_object_new (GOSSIP_TYPE_SESSION, NULL);

	priv = GET_PRIV (session);

	priv->account_manager = g_object_ref (manager);

	accounts = gossip_account_manager_get_accounts (manager);
	
	for (l = accounts; l; l = l->next) {
		GossipAccount *account = l->data;
		
		gossip_session_add_account (session, account);
	}

	g_list_free (accounts);

	return session;
}

GossipAccountManager *
gossip_session_get_account_manager (GossipSession *session) 
{
	GossipSessionPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);

	priv = GET_PRIV (session);

	return priv->account_manager;
}

GList *
gossip_session_get_accounts (GossipSession *session)
{
	GossipSessionPriv *priv;
	GetAccounts       *data;
	GList             *list;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);
	
	priv = GET_PRIV (session);

	data = g_new0 (GetAccounts, 1);

	data->session = session;

	g_hash_table_foreach (priv->accounts, 
			      (GHFunc)session_get_accounts_foreach_cb,
			      data);

	list = data->accounts;

	g_free (data);

	return list;
}

static void
session_get_accounts_foreach_cb (const gchar     *account_name,
				 GossipProtocol  *protocol,
				 GetAccounts     *data)
{
	GossipSessionPriv *priv;
	GossipAccount     *account;

	priv = GET_PRIV (data->session);

	account = gossip_account_manager_find (priv->account_manager, account_name);
	if (account) {
		data->accounts = g_list_append (data->accounts, account);
	}
}

gboolean 
gossip_session_add_account (GossipSession *session,
			    GossipAccount *account)
{
	GossipSessionPriv *priv;
	GossipProtocol    *protocol;
	const gchar       *name;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), FALSE);
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);

	priv = GET_PRIV (session);

	/* check this account is not already set up */
	protocol = g_hash_table_lookup (priv->accounts, 
					gossip_account_get_name (account));
	
	if (protocol) {
		/* already added */
		return TRUE;
	}

	/* create protocol for account type */
	switch (gossip_account_get_type (account)) {
	case GOSSIP_ACCOUNT_TYPE_JABBER:
		protocol = g_object_new (GOSSIP_TYPE_JABBER, NULL);
		break;
	default:
		g_assert_not_reached();
	}
	
	/* add to list */
	priv->protocols = g_list_append (priv->protocols, 
					 g_object_ref (protocol));

	/* add to hash table */
	name = gossip_account_get_name (account);
	
	g_hash_table_insert (priv->accounts, 
			     g_strdup (name), 
			     g_object_ref (protocol));

	/* connect up all signals */ 
	session_protocol_signals_setup (session, protocol);
			
	return TRUE;
}

gboolean
gossip_session_remove_account (GossipSession *session,
			       GossipAccount *account)
{
	GossipSessionPriv *priv;
	GossipProtocol    *protocol;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), FALSE);
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);

	priv = GET_PRIV (session);

	/* get protocol details for this account */
	protocol = g_hash_table_lookup (priv->accounts, 
					gossip_account_get_name (account));
	
	if (!protocol) {
		/* not added in the first place */
		return TRUE;
	}

	/* remove from list */
	priv->protocols = g_list_remove (priv->protocols, protocol);
	g_object_unref (protocol);

	/* remove from hash table */
	return g_hash_table_remove (priv->accounts, 
				    gossip_account_get_name (account));
}

GossipAccount * 
gossip_session_find_account (GossipSession *session,
			     GossipContact *contact)
{
	GossipSessionPriv *priv;
	GossipAccount     *account = NULL;
	FindAccount       *fa;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);
	
	priv = GET_PRIV (session);

	fa = g_new0 (FindAccount, 1);
	
	fa->contact_id = g_strdup (gossip_contact_get_id (contact));

	g_hash_table_foreach (priv->accounts, 
			      (GHFunc)session_find_account_foreach_cb,
			      fa);

	if (fa->account_name) {
		account = gossip_account_manager_find (priv->account_manager,
						       fa->account_name);
	}

	g_free (fa->contact_id);
	g_free (fa->account_name);
	g_free (fa);

	return account;
}

static void
session_find_account_foreach_cb (const gchar    *account_name,
				 GossipProtocol *protocol,
				 FindAccount    *fa)
{
	if (gossip_protocol_find_contact (protocol, fa->contact_id)) {
		fa->account_name = g_strdup (account_name);
	}
}

void
gossip_session_connect (GossipSession *session,
			GossipAccount *account,
			gboolean       startup)
{
	GossipSessionPriv *priv;
	ConnectAccounts   *data;
	
	g_return_if_fail (GOSSIP_IS_SESSION (session));

	priv = GET_PRIV (session);

	g_return_if_fail (priv->accounts != NULL);

	/* temporary */
	if (!priv->presence) {
		priv->presence = gossip_presence_new_full (GOSSIP_PRESENCE_STATE_AVAILABLE, 
							   NULL);
	}

	/* connect one account if provided */
	if (account) {
		g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
		
		session_connect (session, account);
		return;
	}

	/* connect ALL accounts if no one account is specified */
	data = g_new0 (ConnectAccounts, 1);

	data->session = session;
	data->startup = startup;

	g_hash_table_foreach (priv->accounts,
			      (GHFunc)session_connect_foreach_cb,
			      data);

	g_free (data);
}

static void
session_connect (GossipSession *session,
		 GossipAccount *account) 
{
	GossipSessionPriv *priv;
	GossipProtocol    *protocol;

	priv = GET_PRIV (session);
	
	protocol = g_hash_table_lookup (priv->accounts, 
					gossip_account_get_name (account));
	
	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));
	

	/* can we not just pass the GossipAccount on the GObject init? */
	gossip_protocol_setup (protocol, account);
	
	/* setup the network connection */
	gossip_protocol_login (protocol);
}

static void
session_connect_foreach_cb (gchar           *account_name,
			    GossipProtocol  *protocol,
			    ConnectAccounts *data)
{
	GossipSessionPriv *priv;
	GossipAccount     *account;

	priv = GET_PRIV (data->session);

	account = gossip_account_manager_find (priv->account_manager,
					       account_name);
	
	if (data->startup && 
	    !gossip_account_get_auto_connect (account)) {
		return;
	}
	
	session_connect (data->session, account);
}

void
gossip_session_disconnect (GossipSession *session,
			   GossipAccount *account)
{
	GossipSessionPriv *priv;
	GList             *l;

	g_return_if_fail (GOSSIP_IS_SESSION (session));

	priv = GET_PRIV (session);

	/* connect one account if provided */
	if (account) {
		GossipProtocol    *protocol;

		g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
		
		protocol = g_hash_table_lookup (priv->accounts, 
						gossip_account_get_name (account));
		
		g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));
	
		gossip_protocol_logout (protocol);
		return;
	}

	/* disconnect ALL accounts if no one account is specified */
	for (l = priv->protocols; l; l = l->next) {
		GossipProtocol *protocol;

		protocol = GOSSIP_PROTOCOL (l->data);

		gossip_protocol_logout (protocol);
	}
}

void 
gossip_session_send_message (GossipSession *session, 
			     GossipMessage *message)
{
	GossipSessionPriv *priv;
	GossipContact     *contact;
	GossipAccount     *account;
	GList             *l;

	g_return_if_fail (GOSSIP_IS_SESSION (session));
	g_return_if_fail (message != NULL);

	priv = GET_PRIV (session);
	
	contact = gossip_message_get_recipient (message);
	account = gossip_session_find_account (session, contact);

	if (account) {
		GossipProtocol *protocol;

		protocol = g_hash_table_lookup (priv->accounts, 
						gossip_account_get_name (account));
		
		g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));
		
		gossip_protocol_send_message (protocol, message);

		return;
	}

	/* NOTE: is this right? look for the account based on the
	   recipient, if not known then send to ALL?? */
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
	GossipSessionPriv *priv;
	GList             *l;

	g_return_if_fail (GOSSIP_IS_SESSION (session));

	priv = GET_PRIV (session);

	for (l = priv->protocols; l; l = l->next) {
		GossipProtocol *protocol;

		protocol = GOSSIP_PROTOCOL (l->data);
		gossip_protocol_send_composing (protocol,
						contact,
						composing);
	}
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
gossip_session_set_presence (GossipSession  *session,
			     GossipPresence *presence)
{
	GossipSessionPriv *priv;
	GList            *l;

	g_return_if_fail (GOSSIP_IS_SESSION (session));

	priv = GET_PRIV (session);
	priv->presence = presence;

	for (l = priv->protocols; l; l = l->next) {
		GossipProtocol *protocol;

		protocol = GOSSIP_PROTOCOL (l->data);
		gossip_protocol_set_presence (protocol, presence);
	}
}

gboolean
gossip_session_is_connected (GossipSession *session,
			     GossipAccount *account)
{
	GossipSessionPriv *priv;
	GossipProtocol    *protocol;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), FALSE);
	
	priv = GET_PRIV (session);

	if (account) {
		protocol = g_hash_table_lookup (priv->accounts, 
						gossip_account_get_name (account));
		
		g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), FALSE);
		
		return gossip_protocol_is_connected (protocol);
	} 

	/* fall back to counter if no account is provided */
	return (priv->connected_counter > 0);
}

const gchar *
gossip_session_get_active_resource (GossipSession *session,
				    GossipContact *contact)
{
	GossipSessionPriv *priv;
	GossipProtocol    *protocol;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);
	
	/* get the activate resource, needed to be able to lock the
	   chat against a certain resource */

	priv = GET_PRIV (session);

	protocol = session_get_protocol (session, contact);
	if (!protocol) {
		return NULL;
	}
	
	return gossip_protocol_get_active_resource (protocol, contact);
}

GossipChatroomProvider *
gossip_session_get_chatroom_provider (GossipSession *session,
				      GossipAccount *account)
{
	GossipSessionPriv *priv;
	GossipProtocol    *protocol;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GET_PRIV (session);

	protocol = g_hash_table_lookup (priv->accounts, 
					gossip_account_get_name (account));

	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), NULL);

	return GOSSIP_CHATROOM_PROVIDER (protocol);
}

GossipContact *
gossip_session_find_contact (GossipSession *session, 
			     const gchar   *id)
{
	GossipSessionPriv *priv;
	GList             *l;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);
        g_return_val_if_fail (id != NULL, NULL);

	priv = GET_PRIV (session);

	for (l = priv->protocols; l; l = l->next) {
		GossipProtocol *protocol;
		GossipContact  *contact;
		
		protocol = l->data;

		if (!GOSSIP_IS_PROTOCOL (protocol)) {
			continue;
		}

		contact = gossip_protocol_find_contact (protocol, id);
		if (contact) {
			return contact;
		}
	}

	return NULL;
}

void
gossip_session_add_contact (GossipSession *session,
			    GossipAccount *account,
                            const gchar   *id,
                            const gchar   *name,
                            const gchar   *group,
                            const gchar   *message)
{
	GossipSessionPriv *priv;
	GossipProtocol    *protocol;

	g_return_if_fail (GOSSIP_IS_SESSION (session));
	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));
        g_return_if_fail (id != NULL);
        g_return_if_fail (name != NULL);
        g_return_if_fail (message != NULL);

	priv = GET_PRIV (session);

	protocol = g_hash_table_lookup (priv->accounts, 
					gossip_account_get_name (account));

	g_return_if_fail (GOSSIP_IS_PROTOCOL (protocol));
	
        gossip_protocol_add_contact (protocol,
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
	
	/* get the activate resource, needed to be able to lock the
	   chat against a certain resource */

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
	
	/* get the activate resource, needed to be able to lock the
	   chat against a certain resource */

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
	GossipSessionPriv *priv;
	GossipProtocol    *protocol;

	g_return_if_fail (GOSSIP_IS_SESSION (session));
        g_return_if_fail (contact != NULL);
	
	priv = GET_PRIV (session);

	protocol = session_get_protocol (session, contact);
	if (!protocol) {
		return;
	}
	
        gossip_protocol_update_contact (protocol, contact);
}

void
gossip_session_rename_group (GossipSession *session,
			     const gchar   *group,
			     const gchar   *new_name)
{
	GossipSessionPriv *priv;
	GList             *l;

	g_return_if_fail (GOSSIP_IS_SESSION (session));
        g_return_if_fail (group != NULL);
        g_return_if_fail (new_name != NULL);
	
	priv = GET_PRIV (session);

	/* 	protocol = session_get_protocol (session, NULL); */

	/* FIXME: don't just blindly do this across all protocols
	   actually pass the protocol in some how from the contact
	   list? - what if we have the same group name in 2 different
	   accounts */
	for (l = priv->protocols; l; l = l->next) {
		GossipProtocol *protocol = GOSSIP_PROTOCOL (l->data);
	
		gossip_protocol_rename_group (protocol, group, new_name);
	}
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
gossip_session_get_contacts_by_account (GossipSession *session,
					GossipAccount *account)
{
	GossipSessionPriv *priv;
	GList             *l, *list = NULL;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), NULL);

	priv = GET_PRIV (session);

	for (l = priv->contacts; l; l = l->next) {
		GossipContact *contact;
		GossipAccount *this_account;
		
		contact = l->data;

		this_account = gossip_contact_get_account (contact);

		if (!gossip_account_equal (this_account, account)) {
			continue;
		}

		list = g_list_append (list, contact);
	}
	
	return list;
}

GList *
gossip_session_get_groups (GossipSession *session)
{
	GossipSessionPriv *priv;
	GList             *l, *all_groups = NULL;
	
	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);

	priv = GET_PRIV (session);

	for (l = priv->protocols; l; l = l->next) {
		GossipProtocol *protocol;
		GList          *groups;
		
		protocol = l->data;

		if (!GOSSIP_IS_PROTOCOL (protocol)) {
			continue;
	}

		groups = gossip_protocol_get_groups (protocol);
		if (groups) {
			all_groups = g_list_concat (all_groups, groups);
		}
	}

	return all_groups;
}

const gchar *
gossip_session_get_nickname (GossipSession *session)
{
	GossipSessionPriv *priv;
	const GList       *l;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), "");

	priv = GET_PRIV (session);

	/* FIXME: this needs thinking about (mr) */
	for (l = priv->protocols; l; l = l->next) {
		GossipProtocol *protocol;
		GossipContact  *contact;

		protocol = l->data;

		if (!GOSSIP_IS_PROTOCOL (protocol)) {
			continue;
		}

		if (!gossip_protocol_is_connected (protocol)) {
			continue;
		}

#if 1
		/* FIXME: for now, use the first jabber account */
		if (!GOSSIP_IS_JABBER (protocol)) {
			continue;
		}
#endif

		contact = gossip_jabber_get_own_contact (GOSSIP_JABBER (protocol));
		return gossip_contact_get_name (contact);
	}

	return "";
}

gboolean
gossip_session_register_account (GossipSession           *session,
				 GossipAccountType        type,
				 GossipAccount           *account,
				 GossipRegisterCallback   callback,
				 gpointer                 user_data,
				 GError                 **error)
{
	GossipSessionPriv *priv;
	GossipProtocol    *protocol;
	
	g_return_val_if_fail (GOSSIP_IS_SESSION (session), FALSE);
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);
	g_return_val_if_fail (callback != NULL, FALSE);

	priv = GET_PRIV (session);
	
	protocol = g_hash_table_lookup (priv->accounts, 
					gossip_account_get_name (account));
	
	g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), FALSE);
	
        return gossip_protocol_register_account (protocol, account, 
						 callback, user_data,
                                               error);
}

gboolean
gossip_session_get_vcard (GossipSession        *session,
			  GossipAccount        *account,
			  GossipContact        *contact,
			  GossipVCardCallback   callback,
			  gpointer              user_data,
			  GError              **error)
{
	GossipSessionPriv *priv;
	
	g_return_val_if_fail (GOSSIP_IS_SESSION (session), FALSE);
	g_return_val_if_fail (callback != NULL, FALSE);

	priv = GET_PRIV (session);
	
	if (!account && !contact) {
		g_warning ("No GossipAccount and no GossipContact to use for vcard request");
		return FALSE;
	}

	/* find contact and get vcard */
	if (contact) {
		GossipProtocol *protocol;
		
		protocol = session_get_protocol (session, contact);
		
		/* use account, must be temp contact, use account protocol */
		if (!protocol && GOSSIP_IS_ACCOUNT (account)) {
			protocol = g_hash_table_lookup (priv->accounts, 
							gossip_account_get_name (account));	
		}
		
		return gossip_protocol_get_vcard (protocol, contact,
						  callback, user_data,
						  error);
	}

	/* get my vcard */
	if (account) {
		GossipProtocol *protocol;

		g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);

		protocol = g_hash_table_lookup (priv->accounts, 
						gossip_account_get_name (account));

		g_return_val_if_fail (GOSSIP_IS_PROTOCOL (protocol), FALSE);

		return gossip_protocol_get_vcard (protocol, NULL,
						  callback, user_data,
						  error);
	}

	return FALSE;
}

gboolean
gossip_session_set_vcard (GossipSession         *session,
			  GossipAccount         *account,
			  GossipVCard           *vcard,
			  GossipResultCallback   callback,
			  gpointer               user_data,
			  GError               **error)
{
	GossipSessionPriv *priv;
	GList             *l;
	gboolean           ok = TRUE;
	
	g_return_val_if_fail (GOSSIP_IS_SESSION (session), FALSE);
	g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), FALSE);
	g_return_val_if_fail (callback != NULL, FALSE);
	
	priv = GET_PRIV (session);
	
	/* if account is supplied set the vcard for that account only! */
	if (account) {
		GossipProtocol *protocol;

		g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);

		protocol = g_hash_table_lookup (priv->accounts, 
						gossip_account_get_name (account));
		
		return gossip_protocol_set_vcard (protocol,
						  vcard,
						  callback,
						  user_data,
						  error);
	}

	for (l = priv->protocols; l; l = l->next) {
		GossipProtocol *protocol;
		
		protocol = l->data;

		if (!GOSSIP_IS_PROTOCOL (protocol)) {
			continue;
		}

		/* FIXME: error is pointless here... since if this is
		   the 5th protocol, it may have already been
		   written. */
		ok &= gossip_protocol_set_vcard (protocol,
						vcard,
						callback,
						user_data,
						error);
	}
	
	return ok;
}

gboolean
gossip_session_get_version (GossipSession          *session,
			    GossipContact          *contact,
			    GossipVersionCallback   callback,
			    gpointer                user_data,
			    GError                **error)
{
	GossipProtocol *protocol;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), FALSE);
	g_return_val_if_fail (callback != NULL, FALSE);
	
	protocol = session_get_protocol (session, contact);

	if (!protocol) {
		return FALSE;
	}
	
	return gossip_protocol_get_version (protocol, contact, 
						  callback, user_data,
						  error);
}
