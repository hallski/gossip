/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 <martyn@imendio.com>
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

#ifndef __GOSSIP_CONTACT_MANAGER_H__
#define __GOSSIP_CONTACT_MANAGER_H__

#include <glib-object.h>

#include "gossip-account.h"
#include "gossip-contact.h"

G_BEGIN_DECLS

#define GOSSIP_TYPE_CONTACT_MANAGER         (gossip_contact_manager_get_type ())
#define GOSSIP_CONTACT_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CONTACT_MANAGER, GossipContactManager))
#define GOSSIP_CONTACT_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_CONTACT_MANAGER, GossipContactManagerClass))
#define GOSSIP_IS_CONTACT_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CONTACT_MANAGER))
#define GOSSIP_IS_CONTACT_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CONTACT_MANAGER))
#define GOSSIP_CONTACT_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CONTACT_MANAGER, GossipContactManagerClass))

typedef struct _GossipContactManager      GossipContactManager;
typedef struct _GossipContactManagerClass GossipContactManagerClass;

struct _GossipContactManager {
	GObject parent;
};

struct _GossipContactManagerClass {
	GObjectClass parent_class;
};

GType                 gossip_contact_manager_get_type (void) G_GNUC_CONST;
gboolean              gossip_contact_manager_add      (GossipContactManager *manager,
						       GossipContact        *contact);
void                  gossip_contact_manager_remove   (GossipContactManager *manager,
						       GossipContact        *contact);
GossipContact *       gossip_contact_manager_find     (GossipContactManager *manager,
						       GossipAccount        *account,
						       const gchar          *contact_id);
gboolean              gossip_contact_manager_store    (GossipContactManager *manager);

G_END_DECLS

#endif /* __GOSSIP_CONTACT_MANAGER_H__ */
