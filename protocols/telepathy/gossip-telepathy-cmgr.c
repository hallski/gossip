/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Eitan Isaacson <eitan@ascender.com>
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
#include <libtelepathy/tp-helpers.h>

#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-utils.h>

#include "gossip-telepathy-cmgr.h"

#define DEBUG_DOMAIN "TelepathyCmgr"

static void telepathy_cmgr_list_protocols_foreach (const gchar        *key,
						   gpointer            value,
						   GSList            **list);
static void telepathy_cmgr_list_params_foreach    (const gchar        *key,
						   TpConnMgrProtParam *param,
						   GossipAccount      *account);

GSList *
gossip_telepathy_cmgr_list (void)
{
	return tp_connmgr_list_cms ();
}

GSList *
gossip_telepathy_cmgr_list_protocols (const gchar *cmgr)
{
	TpConnMgrInfo *cmgr_info;
	GSList        *list = NULL;

	g_return_val_if_fail (cmgr != NULL, NULL);

	cmgr_info = tp_connmgr_get_info ((gchar*) cmgr);
	g_hash_table_foreach (cmgr_info->protocols,
			      (GHFunc) telepathy_cmgr_list_protocols_foreach,
			      &list);

	tp_connmgr_info_free (cmgr_info);

	return list;
}

GossipAccount *
gossip_telepathy_cmgr_new_account (GossipAccountType  type,
				   const gchar       *cmgr_name,
				   const gchar       *protocol_name)
{
	TpConnMgrInfo *cmgr_info;
	GossipAccount *account;
	GHashTable    *params;

	g_return_val_if_fail (cmgr_name != NULL, NULL);
	g_return_val_if_fail (protocol_name != NULL, NULL);

	gossip_debug (DEBUG_DOMAIN, 
		      "Trying to create account with cmgr name:'%s', protocol:'%s'", 
		      cmgr_name, 
		      protocol_name);

	cmgr_info = tp_connmgr_get_info (cmgr_name);
	g_return_val_if_fail (cmgr_info != NULL, NULL);

	params = g_hash_table_lookup (cmgr_info->protocols, protocol_name);
	g_return_val_if_fail (params != NULL, NULL);

	account = g_object_new (GOSSIP_TYPE_ACCOUNT,
				"type", type,
				"name", _("new account"),
				"auto_connect", TRUE,
				"use_proxy", FALSE,
				NULL);

	g_hash_table_foreach (params,
			      (GHFunc) telepathy_cmgr_list_params_foreach,
			      account);

	tp_connmgr_info_free (cmgr_info);

	return account;
}

static void
telepathy_cmgr_list_protocols_foreach (const gchar  *key,
				       gpointer      value,
				       GSList      **list)
{
	*list = g_slist_prepend (*list, g_strdup (key));
}

static void
telepathy_cmgr_list_params_foreach (const gchar        *key,
				    TpConnMgrProtParam *param,
				    GossipAccount      *account)
{
	GValue *g_value;
	GType   type;

	gossip_debug (DEBUG_DOMAIN, 
		      "- key:'%s' %s", 
		      key, 
		      param->default_value ? "(has default)" : "");


	type = gossip_dbus_type_to_g_type (param->dbus_type);
	if (param->default_value) {
		g_value = gossip_string_to_g_value (param->default_value, type);
	} else {
		g_value = g_new0 (GValue, 1);
		g_value_init (g_value, type);
	}

	gossip_account_new_param_g_value (account, key, g_value, param->flags);

	g_value_unset (g_value);
	g_free (g_value);
}

