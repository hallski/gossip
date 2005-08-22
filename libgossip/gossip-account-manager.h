/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 <mr@gnome.org>
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

#ifndef __GOSSIP_ACCOUNT_MANAGER_H__
#define __GOSSIP_ACCOUNT_MANAGER_H__

#include <glib-object.h>

#include "gossip-account.h"

#define GOSSIP_TYPE_ACCOUNT_MANAGER         (gossip_account_manager_get_type ())
#define GOSSIP_ACCOUNT_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_ACCOUNT_MANAGER, GossipAccountManager))
#define GOSSIP_ACCOUNT_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_ACCOUNT_MANAGER, GossipAccountManagerClass))
#define GOSSIP_IS_ACCOUNT_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_ACCOUNT_MANAGER))
#define GOSSIP_IS_ACCOUNT_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_ACCOUNT_MANAGER))
#define GOSSIP_ACCOUNT_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_ACCOUNT_MANAGER, GossipAccountManagerClass))


typedef struct _GossipAccountManager      GossipAccountManager;
typedef struct _GossipAccountManagerClass GossipAccountManagerClass;


struct _GossipAccountManager {
	GObject parent;
};


struct _GossipAccountManagerClass {
	GObjectClass parent_class;
};


GType          gossip_account_manager_get_type               (void) G_GNUC_CONST;
GossipAccountManager *
               gossip_account_manager_new                    (const gchar          *filename);
gboolean       gossip_account_manager_add                    (GossipAccountManager *manager,
							      GossipAccount        *account);
void           gossip_account_manager_remove                 (GossipAccountManager *manager,
							      GossipAccount        *account);
GList *        gossip_account_manager_get_accounts           (GossipAccountManager *manager);
guint          gossip_account_manager_get_count              (GossipAccountManager *manager);
GossipAccount *gossip_account_manager_get_default            (GossipAccountManager *manager);
GossipAccount *gossip_account_manager_find                   (GossipAccountManager *manager,
							      const gchar          *name);
void           gossip_account_manager_set_overridden_default (GossipAccountManager *manager,
							      const gchar          *name);
void           gossip_account_manager_set_default            (GossipAccountManager *manager,
							      GossipAccount        *account);
gboolean       gossip_account_manager_store                  (GossipAccountManager *manager);


#endif /* __GOSSIP_ACCOUNT_MANAGER_H__ */
