/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002 Mikael Hallendal <micke@imendio.com>
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

#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkentry.h>
#include "gossip-jid.h"

#define GOSSIP_ACCOUNTS_PATH "/Gossip/Accounts"

typedef struct {
	gchar *name;
	gchar *username;
	gchar *resource;
	gchar *password;
	gchar *server;
	guint  port;
	
	gint   ref_count;
} GossipAccount;

GossipAccount * gossip_account_new          (const gchar   *name,
					     const gchar   *username,
					     const gchar   *password,
					     const gchar   *resource,
					     const gchar   *server,
					     guint          port);
GossipAccount * gossip_account_get_default  (void);
GSList *        gossip_account_get_all      (void);
GossipAccount * gossip_account_get          (const gchar   *name);
GossipJID *     gossip_account_get_jid      (GossipAccount *account);
GossipAccount * gossip_account_ref          (GossipAccount *account);
void            gossip_account_unref        (GossipAccount *account);
void            gossip_account_store        (GossipAccount *account,
					     gchar         *old_name);

#endif /* __GOSSIP_ACCOUNT_H__ */
