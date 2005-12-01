/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Imendio AB
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>

#include <libgalago/galago.h>

#include <dbus/dbus-glib-lowlevel.h>

#include <libgossip/gossip-account.h>

#include "gossip-app.h"
#include "gossip-contact-list.h"
#include "gossip-galago.h"

#define d(x)


static const char *   galago_generate_person_id          (GossipContact     *contact);
static GalagoService *galago_get_service                 (GossipAccount     *account);
static GalagoAccount *galago_get_account                 (GossipAccount     *account);
static void           galago_set_status                  (GalagoAccount     *account,
							  GossipPresence    *presence);
static void           galago_presence_changed_cb         (GossipSession     *gossip_session,
							  GossipPresence    *gossip_presence,
							  gpointer           userdata);
static void           galago_contact_added_cb            (GossipSession     *session,
							  GossipContact     *contact,
							  GossipContactList *list);
static void           galago_contact_updated_cb          (GossipSession     *session,
							  GossipContact     *contact,
							  GossipContactList *list);
static void           galago_contact_presence_updated_cb (GossipSession     *session,
							  GossipContact     *contact,
							  GossipContactList *list);
static void           galago_contact_removed_cb          (GossipSession     *session,
							  GossipContact     *contact,
							  GossipContactList *list);
static void           galago_setup_accounts              (GossipSession     *session);


static GalagoPerson *me = NULL;

/* hash of GossipContact to GalagoPerson */
static GHashTable *person_table = NULL;

/* hash of the user's account IDs to GalagoAccounts */
/* TODO: change key to GossipAccount */
static GHashTable *accounts = NULL;


static const char *
galago_generate_person_id (GossipContact *contact)
{
	static int id = 0;
	static char temp[64];
	g_return_val_if_fail (contact != NULL, NULL);
	g_snprintf (temp, sizeof (temp), "person-%d", id++);
	return temp;
}

static GalagoService *
galago_get_service (GossipAccount *account)
{
	static GalagoService *service = NULL;

	g_return_val_if_fail (account != NULL, NULL);

	/* TODO: get service from account type */
	if (!service) {
		service = galago_service_new (GALAGO_SERVICE_ID_JABBER, 
					      GALAGO_SERVICE_ID_JABBER, 
					      TRUE, 
					      0);
	}

	return service;
}

static GalagoAccount *
galago_get_account (GossipAccount *account)
{
	GalagoAccount *ga;
	GalagoService *service;
  
	ga = g_hash_table_lookup (accounts, gossip_account_get_id (account));
	if (!ga) {
		service = galago_get_service (account);
		ga = galago_account_new (service, 
					 me, 
					 gossip_account_get_id (account));
		g_hash_table_insert (accounts, 
				     g_strdup (gossip_account_get_id (account)), 
				     ga);
	}

	return ga;
}

static void
galago_set_status (GalagoAccount  *account, 
		   GossipPresence *presence)
{
	GossipPresenceState  state;
	GalagoPresence      *galago_presence;
	GalagoStatusType     type;
	char                *id;
	const char          *status;

	if (presence) {
		state = gossip_presence_get_state (presence);
						
		switch (state) {
		case GOSSIP_PRESENCE_STATE_AVAILABLE:
			type = GALAGO_STATUS_AVAILABLE;
			id = GALAGO_STATUS_ID_AVAILABLE;
			break;
		case GOSSIP_PRESENCE_STATE_BUSY:
			type = GALAGO_STATUS_AVAILABLE;
			id = GALAGO_STATUS_ID_BUSY;
			break;
		case GOSSIP_PRESENCE_STATE_AWAY:
			type = GALAGO_STATUS_AWAY;
			id = GALAGO_STATUS_ID_AWAY;
			break;
		case GOSSIP_PRESENCE_STATE_EXT_AWAY:
			type = GALAGO_STATUS_EXTENDED_AWAY;
			id = GALAGO_STATUS_ID_EXTENDED_AWAY;
			break;
		default:
			g_assert_not_reached ();
		}
		
		status = gossip_presence_get_status (presence);
		if (!status)
			status = gossip_presence_state_get_default_status (state);
		d(g_print ("Galago: Setting status to %s\n", status));
	} else {
		d(g_print ("Galago: Setting status to offline\n"));
		type = GALAGO_STATUS_OFFLINE;
		id = GALAGO_STATUS_ID_OFFLINE;
		status = _("Offline");
	}

	galago_presence = galago_presence_new (account);
	galago_presence_clear_statuses (galago_presence);
	galago_presence_add_status (galago_presence, 
				    galago_status_new (type, id, status, TRUE));
}

static void
galago_presence_changed_cb (GossipSession  *gossip_session, 
			    GossipPresence *gossip_presence, 
			    gpointer        userdata)
{
	GList *accounts;
	GList *l;

	d(g_print ("Galago: Session presence changed\n"));

	accounts = gossip_session_get_accounts (gossip_session);

	for (l = accounts; l != NULL; l = l->next) {
		GossipAccount *gossip_account;
		GalagoAccount *galago_account;

		gossip_account = GOSSIP_ACCOUNT (l->data);
		galago_account = galago_get_account (gossip_account);
		galago_set_status (galago_account, gossip_presence);
	}

	g_list_foreach (accounts, (GFunc)g_object_unref, NULL);
	g_list_free (accounts);
}

static void
galago_contact_added_cb (GossipSession     *session, 
			 GossipContact     *contact, 
			 GossipContactList *list)
{
	GalagoService *service;
	GalagoAccount *my_gaccount, *gaccount;
	GalagoPerson  *person;

	d(g_print ("Galago: Contact added:'%s'\n", 
		   gossip_contact_get_id (contact)));

	my_gaccount = galago_get_account (gossip_contact_get_account (contact));
	service = galago_account_get_service (my_gaccount);

	person = g_hash_table_lookup (person_table, contact);
	if (person == NULL) {
		person = galago_person_new (galago_generate_person_id (contact), 
					    TRUE);
		g_hash_table_insert (person_table, contact, person);
	}

	gaccount = galago_account_new (service, 
				       person, 
				       gossip_contact_get_id (contact));
	galago_account_set_display_name (gaccount, 
					 gossip_contact_get_name (contact));
	galago_set_status (gaccount, gossip_contact_get_active_presence (contact));

	galago_account_add_contact (my_gaccount, gaccount);
}

static void
galago_contact_updated_cb (GossipSession     *session, 
			   GossipContact     *contact, 
			   GossipContactList *list)
{
	d(g_print ("Galago: Contact updated:'%s'\n", 
		   gossip_contact_get_id (contact)));

	/* TODO */
}

static void 
galago_contact_presence_updated_cb (GossipSession     *session, 
				    GossipContact     *contact, 
				    GossipContactList *list)
{
	GossipPresence *presence;
	GalagoService  *service;
	GalagoPerson   *person;
	GalagoAccount  *gaccount, *my_gaccount;

	d(g_print ("Galago: Contact presence updated:'%s'\n", 
		   gossip_contact_get_id (contact)));

	my_gaccount = galago_get_account (gossip_contact_get_account (contact));

	presence = gossip_contact_get_active_presence (contact);

	person = g_hash_table_lookup (person_table, contact);
	if (!person) { 
		g_warning ("Cannot find person"); 
		return;
	}

	service = galago_get_service (gossip_contact_get_account (contact));
	gaccount = galago_person_get_account (person, 
					      service, 
					      gossip_contact_get_id (contact), 
					      FALSE);
	if (!gaccount) { 
		g_warning ("Cannot find account"); 
		return;
	}

	galago_account_set_connected (gaccount, gossip_contact_is_online (contact));
	galago_set_status (gaccount, presence);
}

static void
galago_contact_removed_cb (GossipSession     *session,
			   GossipContact     *contact,
			   GossipContactList *list)
{
	GalagoAccount *my_galago_account, *galago_account;
	GalagoService *service;
	GalagoPerson  *galago_person;

	d(g_print ("Galago: Contact removed:'%s'\n", 
		   gossip_contact_get_id (contact)));

	my_galago_account = galago_get_account (gossip_contact_get_account (contact));
	service = galago_account_get_service (my_galago_account);

	galago_person = g_hash_table_lookup (person_table, contact);
	if (!person) { 
		g_warning ("Cannot find person"); 
		return;
	}
	
	galago_account = galago_person_get_account (galago_person, 
						    service, 
						    gossip_contact_get_id (contact), 
						    FALSE);
	galago_account_remove_contact (my_galago_account, galago_account);
}

static void
galago_setup_accounts (GossipSession *session)
{
	GList *accounts;
	GList *l;

	d(g_print ("Galago: Setting up accounts\n"));

	accounts = gossip_session_get_accounts (session);

	for (l = accounts; l != NULL; l = l->next) {
		GossipAccount  *account;
		GossipPresence *presence = NULL;
		GalagoAccount  *galago_account;

		account = GOSSIP_ACCOUNT (l->data);

		galago_account = galago_get_account (account);
		galago_account_set_connected (galago_account, 
					      gossip_session_is_connected (session, account));

		presence = gossip_session_get_presence (session);
		if (presence) {
			galago_presence_changed_cb (session, presence, NULL);
		}
	}

	g_list_foreach (accounts, (GFunc)g_object_unref, NULL);
	g_list_free (accounts);
}

void
gossip_galago_init (GossipSession *session)
{
	g_return_if_fail (GOSSIP_IS_SESSION (session));
	
	d(g_print ("Galago: Initiating...\n"));

	if (!galago_glib_init (PACKAGE_NAME, TRUE, NULL)) {
		g_warning ("Cannot initialise Galago integration");
		return;
	}

	me = galago_person_me_new (TRUE);
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
