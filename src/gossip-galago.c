/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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
 *
 * Authors: Ross Burton <ross@openedhand.com>
 *          Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <glib/gi18n.h>
#include <libgalago/galago.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-presence.h>
#include <libgossip/gossip-debug.h>

#include "gossip-app.h"
#include "gossip-contact-list.h"
#include "gossip-galago.h"

#define DEBUG_DOMAIN "Galago"

#ifdef DEPRECATED
static const char *   galago_generate_person_id          (void);
#endif

static GalagoService *gossip_galago_get_service          (GossipAccount  *account);
static GalagoAccount *galago_get_account                 (GossipAccount  *account);
static void           galago_set_status                  (GalagoAccount  *account,
							  GossipPresence *presence);
static void           galago_contact_added_cb            (GossipSession  *session,
							  GossipContact  *contact,
							  gpointer        user_data);
static void           galago_contact_removed_cb          (GossipSession  *session,
							  GossipContact  *contact,
							  gpointer        user_data);
static void           galago_contact_presence_updated_cb (GossipContact  *contact,
							  GParamSpec     *param,
							  gpointer        user_data);
static void           galago_setup_accounts              (GossipSession  *session);

/* GossipContact to GalagoPerson */
static GHashTable *people = NULL;

/* User's account IDs to GalagoAccounts */
static GHashTable *accounts = NULL;

#ifdef DEPRECATED
static const char *
galago_generate_person_id (void)
{
	static gint  id = 0;
	static gchar buffer[64];

	g_snprintf (buffer, sizeof (buffer), "person-%d", id++);

	return buffer;
}
#endif /* DEPRECATED */

static GalagoService *
gossip_galago_get_service (GossipAccount *account)
{
	static GalagoService *gs = NULL;

	if (!gs) {
		/* TODO: get service from account type */

		/* Do we create a service per account we have in Gossip? */
		gs = galago_create_service (GALAGO_SERVICE_ID_JABBER,
					    GALAGO_SERVICE_ID_JABBER,
					    0);
	}

	return gs;
}

static GalagoAccount *
galago_get_account (GossipAccount *account)
{
	GalagoAccount *ga;
	GalagoService *gs;

	ga = g_hash_table_lookup (accounts, account);
	if (!ga) {
		static GalagoPerson *me = NULL;
		const gchar         *account_id;

		account_id = gossip_account_get_id (account);

		/* We could just have a person per account, but then
		 * if those accounts exist on our roster, they are not
		 * shown in test applications because they are "ME".
		 */
		if (!me) {
			me = galago_create_person (NULL);
			galago_person_set_me (me);
		}

		gs = gossip_galago_get_service (account);
		ga = galago_service_create_account (gs, me, account_id);

		g_hash_table_insert (accounts,
				     g_object_ref (account),
				     g_object_ref (ga));

		gossip_debug (DEBUG_DOMAIN, "Added account:'%s'", account_id);
	}

	return ga;
}

static void
galago_set_status (GalagoAccount  *account,
		   GossipPresence *presence)
{
	GossipPresenceState  state;
	GalagoPresence      *gp;
	GalagoStatusType     gst;
	gchar               *id;
	const gchar         *status;

	if (presence) {
		state = gossip_presence_get_state (presence);

		switch (state) {
		case GOSSIP_PRESENCE_STATE_AVAILABLE:
			gst = GALAGO_STATUS_AVAILABLE;
			id = GALAGO_STATUS_ID_AVAILABLE;
			break;
		case GOSSIP_PRESENCE_STATE_BUSY:
			gst = GALAGO_STATUS_AVAILABLE;
			id = GALAGO_STATUS_ID_BUSY;
			break;
		case GOSSIP_PRESENCE_STATE_AWAY:
			gst = GALAGO_STATUS_AWAY;
			id = GALAGO_STATUS_ID_AWAY;
			break;
		case GOSSIP_PRESENCE_STATE_EXT_AWAY:
			gst = GALAGO_STATUS_EXTENDED_AWAY;
			id = GALAGO_STATUS_ID_EXTENDED_AWAY;
			break;
		case GOSSIP_PRESENCE_STATE_HIDDEN:
			gst = GALAGO_STATUS_HIDDEN;
			id = GALAGO_STATUS_ID_HIDDEN;
			break;
		case GOSSIP_PRESENCE_STATE_UNAVAILABLE:
			gst = GALAGO_STATUS_OFFLINE;
			id = GALAGO_STATUS_ID_OFFLINE;
			break;
		default:
			g_assert_not_reached ();
		}

		status = gossip_presence_get_status (presence);
		if (!status) {
			status = gossip_presence_state_get_default_status (state);
		}

		gossip_debug (DEBUG_DOMAIN, "Setting account:'%s' status to %s",
			      galago_account_get_username (account), status);
	} else {
		gossip_debug (DEBUG_DOMAIN, "Setting account:'%s' status to Offline",
			      galago_account_get_username (account));

		gst = GALAGO_STATUS_OFFLINE;
		id = GALAGO_STATUS_ID_OFFLINE;

		status = _("Offline");
	}

	gp = galago_account_create_presence (account);
	galago_presence_clear_statuses (gp);
	galago_presence_add_status (gp, galago_status_new (gst, id, status, TRUE));
}

static void
galago_contact_added_cb (GossipSession *session,
			 GossipContact *contact,
			 gpointer       user_data)
{
	GossipAccount *account;
	GalagoService *gs;
	GalagoAccount *ga_me, *ga;
	GalagoPerson  *gpe;
	const gchar   *contact_id;

	account = gossip_contact_get_account (contact);
	contact_id = gossip_contact_get_id (contact);

	gossip_debug (DEBUG_DOMAIN, "Added contact:'%s'", contact_id);

	g_signal_connect (contact, "notify::presences",
			  G_CALLBACK (galago_contact_presence_updated_cb),
			  NULL);

	ga_me = galago_get_account (account);
	gs = galago_account_get_service (ga_me);

	gpe = g_hash_table_lookup (people, contact);
	if (!gpe) {
#ifdef DEPRECATED
		gpe = galago_create_person (galago_generate_person_id ());
#else
		gpe = galago_create_person (contact_id);
#endif
		g_hash_table_insert (people,
				     g_object_ref (contact),
				     g_object_ref (gpe));
	}

	ga = galago_service_create_account (gs, gpe, contact_id);
	galago_account_set_display_name (ga, gossip_contact_get_name (contact));
	galago_set_status (ga, gossip_contact_get_active_presence (contact));

	galago_account_add_contact (ga_me, ga);
}

static void
galago_contact_removed_cb (GossipSession *session,
			   GossipContact *contact,
			   gpointer       user_data)
{
	GossipAccount *account;
	GalagoAccount *ga, *ga_me;
	GalagoService *gs;
	GalagoPerson  *gpe;
	const gchar   *contact_id;

	account = gossip_contact_get_account (contact);
	contact_id = gossip_contact_get_id (contact);

	gossip_debug (DEBUG_DOMAIN, "Contact removed:'%s'", contact_id);

	g_signal_handlers_disconnect_by_func (contact,
					      galago_contact_presence_updated_cb,
					      NULL);

	ga_me = galago_get_account (account);
	gs = galago_account_get_service (ga_me);

	gpe = g_hash_table_lookup (people, contact);
	if (!gpe) {
		gossip_debug (DEBUG_DOMAIN, "Can not find person:'%s'", contact_id);
		return;
	}

	/* FIXME: Not sure which is right here, but if we try to get
	 * the GalagoAccount with the GalagoPerson, it fails to find
	 * some of them (usually those which we also have a
	 * GossipAccount which has the same i.
	 */
#if 0
	ga = galago_person_get_account (gpe, gs, contact_id, FALSE);
#endif

	ga = galago_service_get_account (gs, contact_id, TRUE);
	galago_account_remove_contact (ga_me, ga);

	g_hash_table_remove (people, contact);
}

static void
galago_contact_presence_updated_cb (GossipContact *contact,
				    GParamSpec    *param,
				    gpointer       user_data)
{
	GossipAccount  *account;
	GossipPresence *presence;
	GalagoService  *gs;
	GalagoPerson   *gpe;
	GalagoAccount  *ga, *ga_me;
	const gchar    *contact_id;

	contact_id = gossip_contact_get_id (contact);

	gossip_debug (DEBUG_DOMAIN, "Contact presence updated:'%s'", contact_id);

	account = gossip_contact_get_account (contact);
	ga_me = galago_get_account (account);

	presence = gossip_contact_get_active_presence (contact);

	gpe = g_hash_table_lookup (people, contact);
	if (!gpe) {
		gossip_debug (DEBUG_DOMAIN, "Can not find person:'%s'", contact_id);
		return;
	}

	gs = gossip_galago_get_service (account);
	ga = galago_person_get_account (gpe, gs, contact_id, TRUE);
	if (!ga) {
		gossip_debug (DEBUG_DOMAIN, "Can not find account from contact:'%s'", contact_id);
		return;
	}

	galago_account_set_connected (ga, gossip_contact_is_online (contact));
	galago_set_status (ga, presence);
}

static void
galago_setup_accounts (GossipSession *session)
{
	GList *accounts;
	GList *l;

	gossip_debug (DEBUG_DOMAIN, "Setting up accounts");

	accounts = gossip_session_get_accounts (session);

	for (l = accounts; l != NULL; l = l->next) {
		GossipAccount  *account;
		GossipPresence *presence = NULL;
		GalagoAccount  *ga;
		gboolean        is_connected;

		account = GOSSIP_ACCOUNT (l->data);

		ga = galago_get_account (account);
		is_connected = gossip_session_is_connected (session, account);

		galago_account_set_connected (ga, is_connected);

		presence = gossip_session_get_presence (session);
		if (presence) {
			galago_set_status (ga, presence);
		}
	}

	g_list_foreach (accounts, (GFunc) g_object_unref, NULL);
	g_list_free (accounts);
}

void
gossip_galago_init (GossipSession *session)
{
	g_return_if_fail (GOSSIP_IS_SESSION (session));

	gossip_debug (DEBUG_DOMAIN, "Initiating...");

	if (!galago_init (PACKAGE_NAME, GALAGO_INIT_FEED)) {
		gossip_debug (DEBUG_DOMAIN, "Can not initialise Galago integration");
		return;
	}

	people = g_hash_table_new_full (gossip_contact_hash,
					gossip_contact_equal,
					g_object_unref,
					g_object_unref);
	accounts = g_hash_table_new_full (gossip_account_hash,
					  gossip_account_equal,
					  g_object_unref,
					  g_object_unref);

	galago_setup_accounts (session);

	g_signal_connect (session,
			  "contact-added",
			  G_CALLBACK (galago_contact_added_cb),
			  NULL);
	g_signal_connect (session,
			  "contact-removed",
			  G_CALLBACK (galago_contact_removed_cb),
			  NULL);
}
