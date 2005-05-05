/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio AB
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

#ifndef __GOSSIP_CONTACT_LIST_H__
#define __GOSSIP_CONTACT_LIST_H__

#include <gtk/gtktreeview.h>

#include <libgossip/gossip-contact.h>

#define GOSSIP_TYPE_CONTACT_LIST         (gossip_contact_list_get_type ())
#define GOSSIP_CONTACT_LIST(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CONTACT_LIST, GossipContactList))
#define GOSSIP_CONTACT_LIST_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_CONTACT_LIST, GossipContactListClass))
#define GOSSIP_IS_CONTACT_LIST(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CONTACT_LIST))
#define GOSSIP_IS_CONTACT_LIST_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CONTACT_LIST))
#define GOSSIP_CONTACT_LIST_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CONTACT_LIST, GossipContactListClass))

typedef struct _GossipContactList      GossipContactList;
typedef struct _GossipContactListClass GossipContactListClass;
typedef struct _GossipContactListPriv  GossipContactListPriv;

struct _GossipContactList {
	GtkTreeView            parent;

	GossipContactListPriv *priv;
};

struct _GossipContactListClass {
	GtkTreeViewClass       parent_class;
};

GType               gossip_contact_list_get_type     (void) G_GNUC_CONST;
GossipContactList * gossip_contact_list_new (void);

GossipContact *  gossip_contact_list_get_selected  (GossipContactList *list);
char *           gossip_contact_list_get_selected_group (GossipContactList *list);

gboolean     gossip_contact_list_get_show_offline (GossipContactList *list);
void         gossip_contact_list_set_show_offline (GossipContactList *list,
						   gboolean           show_offline);

#endif /* __GOSSIP_CONTACT_LIST_H__ */

