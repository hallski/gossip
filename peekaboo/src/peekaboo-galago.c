/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2006 Imendio AB
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <libgalago/galago.h>

#include "peekaboo-galago.h"

/* #define DEBUG_MSG(x)  */
#define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n"); 

gboolean
peekaboo_galago_init (void)
{
        if (!galago_init ("Peekaboo", GALAGO_INIT_CLIENT)) {
		g_warning ("Could not initialise Galago");
		return FALSE;
	}

	return TRUE;
}

static void
galago_services_foreach (GalagoService *service,
			 gpointer       user_data)
{
	DEBUG_MSG (("\t* id:'%s', name:'%s'", 
		    galago_service_get_id (service), 
		    galago_service_get_name (service)));
}

GList *
peekaboo_galago_get_services (void)
{
	GList *services;
	
	services = galago_get_services (GALAGO_REMOTE, TRUE);
	g_return_val_if_fail (services != NULL, NULL);

	DEBUG_MSG (("Printing Services:"));
	g_list_foreach (services, (GFunc) galago_services_foreach, NULL);
	
	return services;
}

static void
galago_people_foreach (GalagoPerson *person,
		       gpointer      user_data)
{
	DEBUG_MSG (("\t* id:'%s', name:'%s'", 
		    galago_person_get_id (person), 
		    galago_person_get_display_name (person)));
}

GList *
peekaboo_galago_get_people (void)
{
	GList *people;

	people = galago_get_people (GALAGO_REMOTE, TRUE);
	g_return_val_if_fail (people != NULL, NULL);

	DEBUG_MSG (("Printing People:"));
	g_list_foreach (people, (GFunc) galago_people_foreach, NULL);
	
	return people;
}

static void
galago_accounts_foreach (GalagoAccount *account,
			 gpointer       user_data)
{
	DEBUG_MSG (("\t* id:'%s', name:'%s'", 
		    galago_account_get_username (account), 
		    galago_account_get_display_name (account)));
}

GList *
peekaboo_galago_get_accounts (void)
{
	GalagoService *service;
	GList         *accounts;

	service = galago_get_service (GALAGO_SERVICE_ID_JABBER, GALAGO_REMOTE, TRUE);
	accounts = galago_service_get_accounts (service, TRUE);
	g_return_val_if_fail (accounts != NULL, NULL);

	DEBUG_MSG (("Printing Accounts:"));
	g_list_foreach (accounts, (GFunc) galago_accounts_foreach, NULL);
	
	return accounts;
}

gboolean
peekaboo_galago_get_state_and_name (const gchar          *id,
				    gchar               **name,
				    GossipPresenceState  *state)
{
	GalagoService  *service;
	GalagoAccount  *account;
	GalagoPresence *presence;
	GalagoStatus   *status;

	g_return_val_if_fail (id != NULL, FALSE);

	service = galago_get_service (GALAGO_SERVICE_ID_JABBER, GALAGO_REMOTE, TRUE);
	account = galago_service_get_account (service, id, TRUE);

	if (name) {
		const gchar *display_name;

		display_name = galago_account_get_display_name (account);
		if (display_name) {
			*name = g_strdup (display_name);
		} else {
			*name = g_strdup (id);
		}
	}

	if (!state) {
		return TRUE;
	}

	presence = galago_account_get_presence (account, TRUE);
	status = galago_presence_get_active_status (presence);

	switch (galago_status_get_primitive (status)) {
	case GALAGO_STATUS_AVAILABLE:
		*state = GOSSIP_PRESENCE_STATE_AVAILABLE;
		break;
	case GALAGO_STATUS_AWAY:
		*state = GOSSIP_PRESENCE_STATE_AWAY;
		break;
	case GALAGO_STATUS_EXTENDED_AWAY:
		*state = GOSSIP_PRESENCE_STATE_EXT_AWAY;
		break;
	case GALAGO_STATUS_HIDDEN:
		*state = GOSSIP_PRESENCE_STATE_HIDDEN;
		break;
	case GALAGO_STATUS_OFFLINE:
	case GALAGO_STATUS_UNSET:
	default:
		*state = GOSSIP_PRESENCE_STATE_UNAVAILABLE;
		break;
	}

	return TRUE;
}
