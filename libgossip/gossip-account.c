/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002 Mikael Hallendal <micke@codefactory.se>
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
#include <string.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <libgnome/gnome-config.h>
#include <loudmouth/loudmouth.h>
#include "gossip-account.h"

static gchar *  account_get_value  (const gchar   *path,
				    const gchar   *value_name);
static void     account_free       (GossipAccount *account);

static gchar *
account_get_value (const gchar *path, const gchar *value_name)
{
	gchar *key;
	gchar *str;
	
	key = g_strdup_printf ("%s/%s=", path, value_name);
	str = gnome_config_get_string_with_default (key, NULL);
	g_free (key);
	
	return str;
}

static void
account_free (GossipAccount *account)
{
	g_return_if_fail (account != NULL);
	
	g_free (account->name);
	g_free (account->username);
	g_free (account->password);
	g_free (account->resource);
	g_free (account->server);
	
	g_free (account);
}

GossipAccount *
gossip_account_new (const gchar *name,
		    const gchar *username,
		    const gchar *password,
		    const gchar *resource,
		    const gchar *server,
		    guint        port)
{
	GossipAccount *account;
	const gchar   *str;
	
	account = g_new0 (GossipAccount, 1);
	
	str = name ? name : "";
	account->name = g_strdup (str);

	str = username ? username : "";
	account->username = g_strdup (str);
	
	str = password ? password : "";
	account->password = g_strdup (str);
	
	str = resource ? resource : "";
	account->resource = g_strdup (str);
	
	str = server ? server : "";
	account->server = g_strdup (str);
	
	account->port = port;

	account->ref_count = 1;

	return account;
}

GossipAccount *
gossip_account_get_default ()
{
	GossipAccount *account;
	gchar         *name;
	
	name = gnome_config_get_string (GOSSIP_ACCOUNTS_PATH "/Accounts/Default");
	
	if (!name) {
		return NULL;
	}

	account = gossip_account_get (name);
	g_free (name);
	
	return account;
}

GossipAccount *
gossip_account_get (const gchar *name)
{
	GossipAccount *account = g_new0 (GossipAccount, 1);
	gchar         *path;
	gchar         *key;
	
	path = g_strdup_printf ("%s/Account: %s", GOSSIP_ACCOUNTS_PATH, name);

	if (!gnome_config_has_section (path)) {
		return NULL;
	}
	
	account->name     = g_strdup (name);
	account->username = account_get_value (path, "username");
	account->resource = account_get_value (path, "resource");
	account->server   = account_get_value (path, "server");

	key = g_strdup_printf ("%s/password=", path);
	account->password = 
		gnome_config_private_get_string_with_default (key, NULL);
	g_free (key);

	key = g_strdup_printf ("%s/port=%d", path, LM_CONNECTION_DEFAULT_PORT);
	g_free (path);

	account->port = gnome_config_get_int_with_default (key, NULL);
	g_free (key);
	
	account->ref_count = 1;
	
	return account;
}

GossipJID *
gossip_account_get_jid (GossipAccount *account)
{
	GossipJID *jid;
	gchar     *str;

	g_return_val_if_fail (account != NULL, NULL);
	
	str = g_strdup_printf ("%s@%s/%s", 
			       account->username, 
			       account->server, 
			       account->resource);

	jid = gossip_jid_new (str);
	g_free (str);

	return jid;
}


GSList *
gossip_account_get_all (void)
{
	GSList    *ret_val = NULL;
	gpointer   iter;
	gchar     *key;
 	
	iter = gnome_config_init_iterator_sections (GOSSIP_ACCOUNTS_PATH);

	while ((iter = gnome_config_iterator_next (iter, &key, NULL))) {
		if (strncmp ("Account: ", key, 9) == 0) {
			GossipAccount *account;
			
			account = gossip_account_get (key + 9);
			ret_val = g_slist_prepend (ret_val, account);
		}

		g_free (key);
	}

	return ret_val;
}

GossipAccount *
gossip_account_ref (GossipAccount *account)
{
	g_return_val_if_fail (account != NULL, NULL);
	
	account->ref_count++;

	return account;
}

void
gossip_account_unref (GossipAccount *account)
{
	g_return_if_fail (account != NULL);
	
	account->ref_count--;
	
	if (account->ref_count <= 0) {
		account_free (account);
	}
}

