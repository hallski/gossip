/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Imendio HB
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
#include <libgnome/gnome-i18n.h>
#include <loudmouth/loudmouth.h>
#include "gossip-account.h"

static gchar *  account_get_value  (const gchar   *path,
				    const gchar   *value_name);
static void     account_free       (GossipAccount *account);

static gchar *overridden_default_name = NULL;

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
	gossip_jid_unref (account->jid);
	g_free (account->password);
	g_free (account->server);

	g_free (account);
}

GossipAccount *
gossip_account_new (const gchar *name,
		    GossipJID   *jid,
		    const gchar *password,
		    const gchar *server,
		    guint        port,
		    gboolean     use_ssl)
{
	GossipAccount *account;
	const gchar   *str;
	
	account = g_new0 (GossipAccount, 1);
	
	str = name ? name : "";
	account->name = g_strdup (str);

	account->jid = gossip_jid_ref (jid);
	
	str = password ? password : "";
	account->password = g_strdup (str);
	
	str = server ? server : "";
	account->server = g_strdup (str);
	
	account->port = port;
	account->use_ssl = use_ssl;

	account->ref_count = 1;

	return account;
}

GossipAccount *
gossip_account_get_default ()
{
	GossipAccount *account = NULL;
	gchar         *name;

	if (overridden_default_name) {
		account = gossip_account_get (overridden_default_name);
	}

	if (!account) {
		name = gnome_config_get_string (GOSSIP_ACCOUNTS_PATH "/Accounts/Default");
		if (name) {
			account = gossip_account_get (name);
			g_free (name);
		}
	}

	return account;
}

GossipAccount *
gossip_account_get (const gchar *name)
{
	GossipAccount *account = g_new0 (GossipAccount, 1);
	gchar         *path;
	gchar         *key;
	gchar         *username;
	gchar         *resource;
	gchar         *jid_str;
	
	path = g_strdup_printf ("%s/Account: %s", GOSSIP_ACCOUNTS_PATH, name);

	if (!gnome_config_has_section (path)) {
		return NULL;
	}
	
	account->name     = g_strdup (name);
	account->server   = account_get_value (path, "server");
	jid_str = account_get_value (path, "jid");
	
	if (!jid_str || strcmp (jid_str, "") == 0) {
		username = account_get_value (path, "username");
		resource = account_get_value (path, "resource");
		jid_str = g_strdup_printf ("%s@%s/%s", username, account->server, resource);
		g_free (username);
		g_free (resource);
	}

	g_print ("Got: %s\n", jid_str);

	account->jid = gossip_jid_new (jid_str);

	g_free (jid_str);

	key = g_strdup_printf ("%s/password=", path);
	account->password = 
		gnome_config_private_get_string_with_default (key, NULL);
	g_free (key);

	key = g_strdup_printf ("%s/port=%d", path, LM_CONNECTION_DEFAULT_PORT);
	account->port = gnome_config_get_int_with_default (key, NULL);
	g_free (key);

	key = g_strdup_printf ("%s/use_ssl=", path);
	account->use_ssl = gnome_config_get_bool_with_default (key, FALSE);
	g_free (key);
	
	g_free (path);
	
	account->ref_count = 1;
	
	return account;
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

void
gossip_account_store (GossipAccount *account, gchar *old_name)
{
	gchar *str;
	gchar *path;
	gchar *key;
					 
	if (old_name) {
		gchar *old_path = g_strdup_printf ("%s/Account: %s",
						   GOSSIP_ACCOUNTS_PATH,
						   old_name);
		gnome_config_clean_section (old_path);
		g_free (old_path);
	}

	path = g_strdup_printf ("%s/Account: %s", 
				GOSSIP_ACCOUNTS_PATH, account->name);
	
	key = g_strdup_printf ("%s/jid", path);
	gnome_config_set_string (key, gossip_jid_get_full (account->jid));
	g_free (key);
	
	key = g_strdup_printf ("%s/server", path);
	gnome_config_set_string (key, account->server);
	g_free (key);

	key = g_strdup_printf ("%s/port", path);
	gnome_config_set_int (key, account->port);
	g_free (key);
	
	key = g_strdup_printf ("%s/password", path);
	gnome_config_private_set_string (key, account->password);
	g_free (key);

	key = g_strdup_printf ("%s/use_ssl", path);
	gnome_config_set_bool (key, account->use_ssl);
	g_free (key);

	g_free (path);

	str = gnome_config_get_string (GOSSIP_ACCOUNTS_PATH"/Accounts/Default");
	if (!str) {
		gnome_config_set_string (GOSSIP_ACCOUNTS_PATH "/Accounts/Default",
					 account->name);
	}
	g_free (str);

 	gnome_config_sync ();
}

void
gossip_account_set_default (GossipAccount *account)
{
	if (overridden_default_name) {
		return;
	}
	
	gnome_config_set_string (GOSSIP_ACCOUNTS_PATH "/Accounts/Default",
				 account->name);

	gnome_config_sync ();
}

/* Note: This is to emulate a different default account, mostly for debuggin
 * purposes.
 */
void
gossip_account_set_overridden_default_name (const gchar *name)
{
	g_free (overridden_default_name);
	overridden_default_name = g_strdup (name);
}

GossipAccount *
gossip_account_create_empty (void)
{
	GossipAccount *account;
	gchar         *jid_str;
	GossipJID     *tmp_jid;

	jid_str = g_strdup_printf ("/%s", _("Home"));
	tmp_jid = gossip_jid_new (jid_str);
	g_free (jid_str);

	account = gossip_account_new ("Default",
				      tmp_jid, NULL,
				      NULL,
				      LM_CONNECTION_DEFAULT_PORT,
				      FALSE);

	gossip_jid_unref (tmp_jid);

	return account;
}	
