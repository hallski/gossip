/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2003 Imendio HB
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

#ifndef __GOSSIP_ACCOUNT_H__
#define __GOSSIP_ACCOUNT_H__

#include <glib.h>
#include "gossip-jid.h"

#define GOSSIP_ACCOUNTS_PATH "/Gossip/Accounts"

typedef struct {
	gchar     *name;
	GossipJID *jid;
	gchar     *password;
	gchar     *server;
	guint      port;
	gboolean   use_ssl;
	
	gint   ref_count;
} GossipAccount;


GossipAccount *gossip_account_new                         (const gchar   *name,
							   GossipJID     *jid,
							   const gchar   *password,
							   const gchar   *server,
							   guint          port,
							   gboolean       use_ssl);
GossipAccount *gossip_account_get_default                 (void);
GSList *       gossip_account_get_all                     (void);
GossipAccount *gossip_account_get                         (const gchar   *name);
GossipAccount *gossip_account_ref                         (GossipAccount *account);
void           gossip_account_unref                       (GossipAccount *account);
void           gossip_account_store                       (GossipAccount *account,
							   gchar         *old_name);
void           gossip_account_set_default                 (GossipAccount *account);
void           gossip_account_set_overridden_default_name (const gchar   *name);

GossipAccount *gossip_account_create_empty                (void);

#endif /* __GOSSIP_ACCOUNT_H__ */
