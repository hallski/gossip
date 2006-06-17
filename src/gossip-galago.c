/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2006 Imendio AB
 * Copyright (C) 2005 Ross Burton <ross@openedhand.com>
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
#include <glib/gi18n.h>
#include <libgalago/galago.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libgossip/gossip-account.h>

#include "gossip-app.h"
#include "gossip-contact-list.h"
#include "gossip-galago.h"

/* #define DEBUG_MSG(x)  */
#define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n"); 

#ifdef DEPRECATED
static const char *   galago_generate_person_id          (GossipContact  *contact);
#endif

static GalagoService *gossip_galago_get_service          (GossipAccount  *account);
static GalagoAccount *galago_get_account                 (GossipAccount  *account);
static void           galago_set_status                  (GalagoAccount  *account,
							  GossipPresence *presence);
static void           galago_presence_changed_cb         (GossipSession  *gossip_session,
							  GossipPresence *gossip_presence,
							  gpointer        userdata);
static void           galago_contact_added_cb            (GossipSession  *session,
							  GossipContact  *contact,
							  gpointer        user_data);
static void           galago_contact_updated_cb          (GossipSession  *session,
							  GossipContact  *contact,
							  gpointer        user_data);
static void           galago_contact_presence_updated_cb (GossipSession  *session,
							  GossipContact  *contact,
							  gpointer        user_data);
static void           galago_contact_removed_cb          (GossipSession  *session,
							  GossipContact  *contact,
							  gpointer        user_data);
static void           galago_setup_accounts              (GossipSession  *session);

static GalagoPerson *me = NULL;

/* hash of GossipContact to GalagoPerson */
static GHashTable *person_table = NULL;

/* hash of the user's account IDs to GalagoAccounts */
/* TODO: change key to GossipAccount */
static GHashTable *accounts = NULL;

#ifdef DEPRECATED
static const char *
galago_generate_person_id (GossipContact *contact)
{
	static int id = 0;
	static char temp[64];

	g_return_val_if_fail (contact != NULL, NULL);
	g_snprintf (temp, sizeof (temp), "person-%d", id++);

	return temp;
}
#endif /* DEPRECATED */

static GalagoService *
gossip_galago_get_service (GossipAccount *account)
{
	static GalagoService *gs = NULL;

	g_return_val_if_fail (account != NULL, NULL);

	/* TODO: get service from account type */
	if (!gs) {
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
	const gchar   *account_id;
  
	account_id = gossip_account_get_id (account);

	ga = g_hash_table_lookup (accounts, account_id);
	if (!ga) {
		gs = gossip_galago_get_service (account);
		ga = galago_service_create_account (gs, me, account_id);
		g_hash_table_insert (accounts, g_strdup (account_id), ga);
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
	char                *id;
	const char          *status;

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
		default:
			g_assert_not_reached ();
		}
		
		status = gossip_presence_get_status (presence);
		if (!status) {
			status = gossip_presence_state_get_default_status (state);
		}

		DEBUG_MSG (("Galago: Setting status to %s", status));
	} else {
		DEBUG_MSG (("Galago: Setting status to offline"));

		gst = GALAGO_STATUS_OFFLINE;
		id = GALAGO_STATUS_ID_OFFLINE;

		status = _("Offline");
	}

	gp = galago_account_create_presence (account);
	galago_presence_clear_statuses (gp);
	galago_presence_add_status (gp, galago_status_new (gst, id, status, TRUE));
}

static void
galago_presence_changed_cb (GossipSession  *session, 
			    GossipPresence *presence,
			    gpointer        userdata)
{
	GList *accounts;
	GList *l;

	DEBUG_MSG (("Galago: Session presence changed"));

	accounts = gossip_session_get_accounts (session);

	for (l = accounts; l != NULL; l = l->next) {
		GossipAccount *account;
		GalagoAccount *ga;

		account = GOSSIP_ACCOUNT (l->data);
		ga = galago_get_account (account);
		galago_set_status (ga, presence);
	}

	g_list_foreach (accounts, (GFunc) g_object_unref, NULL);
	g_list_free (accounts);
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

	DEBUG_MSG (("Galago: Contact added:'%s'", contact_id));

	ga_me = galago_get_account (account);
	gs = galago_account_get_service (ga_me);

	gpe = g_hash_table_lookup (person_table, contact);
	if (gpe == NULL) {
		/* We did generate a person id here, not sure why? */
		gpe = galago_create_person (contact_id);
		g_hash_table_insert (person_table, contact, gpe);
	}

	ga = galago_service_create_account (gs, gpe, contact_id);
	galago_account_set_display_name (ga, gossip_contact_get_name (contact));
	galago_set_status (ga, gossip_contact_get_active_presence (contact));

	galago_account_add_contact (ga_me, ga);
}

static void
galago_contact_updated_cb (GossipSession  *session, 
			   GossipContact  *contact,
			   gpointer        user_data)
{
	DEBUG_MSG (("Galago: Contact updated:'%s'", 
		   gossip_contact_get_id (contact)));

	/* TODO */
}

static void 
galago_contact_presence_updated_cb (GossipSession *session, 
				    GossipContact *contact,
				    gpointer       user_data)
{
	GossipAccount  *account;
	GossipPresence *presence;
	GalagoService  *gs;
	GalagoPerson   *gpe;
	GalagoAccount  *ga, *ga_me;

	DEBUG_MSG (("Galago: Contact presence updated:'%s'", 
		   gossip_contact_get_id (contact)));
	
	account = gossip_contact_get_account (contact);
	ga_me = galago_get_account (account);

	presence = gossip_contact_get_active_presence (contact);

	gpe = g_hash_table_lookup (person_table, contact);
	if (!gpe) { 
		g_warning ("Cannot find person"); 
		return;
	}

	gs = gossip_galago_get_service (account);
	ga = galago_person_get_account (gpe, gs, gossip_contact_get_id (contact), FALSE);
	if (!ga) { 
		g_warning ("Cannot find account"); 
		return;
	}

	galago_account_set_connected (ga, gossip_contact_is_online (contact));
	galago_set_status (ga, presence);
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

	DEBUG_MSG (("Galago: Contact removed:'%s'", 
		   gossip_contact_get_id (contact)));

	account = gossip_contact_get_account (contact);

	ga_me = galago_get_account (account);
	gs = galago_account_get_service (ga_me);

	gpe = g_hash_table_lookup (person_table, contact);
	if (!gpe) { 
		g_warning ("Cannot find person"); 
		return;
	}
	
	ga = galago_person_get_account (gpe, gs, gossip_contact_get_id (contact), FALSE);
	galago_account_remove_contact (ga_me, ga);
}

static void
galago_setup_accounts (GossipSession *session)
{
	GList *accounts;
	GList *l;

	DEBUG_MSG (("Galago: Setting up accounts"));

	accounts = gossip_session_get_accounts (session);

	for (l = accounts; l != NULL; l = l->next) {
		GossipAccount  *account;
		GossipPresence *presence = NULL;
		GalagoAccount  *ga;

		account = GOSSIP_ACCOUNT (l->data);

		ga = galago_get_account (account);
		galago_account_set_connected (ga, 
					      gossip_session_is_connected (session, account));

		presence = gossip_session_get_presence (session);
		if (presence) {
			galago_presence_changed_cb (session, presence, NULL);
		}
	}

	g_list_foreach (accounts, (GFunc) g_object_unref, NULL);
	g_list_free (accounts);
}

void
gossip_galago_init (GossipSession *session)
{
	g_return_if_fail (GOSSIP_IS_SESSION (session));
	
	DEBUG_MSG (("Galago: Initiating..."));

	if (!galago_init (PACKAGE_NAME, GALAGO_INIT_FEED)) {
		g_warning ("Cannot initialise Galago integration");
		return;
	}

	me = galago_create_person (NULL);
	galago_person_set_me (me);

	person_table = g_hash_table_new (gossip_contact_hash, 
					 gossip_contact_equal);
	accounts = g_hash_table_new (g_str_hash, g_str_equal);

	galago_setup_accounts (session);

	g_signal_connect (session,
			  "presence-changed",
			  G_CALLBACK (galago_presence_changed_cb),
			  NULL);

	g_signal_connect (session,
			  "contact-added",
			  G_CALLBACK (galago_contact_added_cb),
			  NULL);
	g_signal_connect (session,
			  "contact-updated",
			  G_CALLBACK (galago_contact_updated_cb),
			  NULL);
	g_signal_connect (session,
			  "contact-presence-updated",
			  G_CALLBACK (galago_contact_presence_updated_cb),
			  NULL);
	g_signal_connect (session,
			  "contact-removed",
			  G_CALLBACK (galago_contact_removed_cb),
			  NULL);
}
