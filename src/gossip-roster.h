/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2004 Imendio AB
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

#ifndef __GOSSIP_ROSTER_H__
#define __GOSSIP_ROSTER_H__

#include <loudmouth/loudmouth.h>
#include <glib.h>
#include <glib-object.h>

#include "gossip-contact.h"
#include "gossip-jid.h"
#include "gossip-utils.h"

#define GOSSIP_TYPE_ROSTER         (gossip_roster_get_type ())
#define GOSSIP_ROSTER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_ROSTER, GossipRoster))
#define GOSSIP_ROSTER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_ROSTER, GossipRosterClass))
#define GOSSIP_IS_ROSTER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_ROSTER))
#define GOSSIP_IS_ROSTER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_ROSTER))
#define GOSSIP_ROSTER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_ROSTER, GossipRosterClass))

typedef struct _GossipRoster        GossipRoster;
typedef struct _GossipRosterClass   GossipRosterClass;
typedef struct _GossipRosterPriv    GossipRosterPriv;

typedef struct _GossipRosterItem    GossipRosterItem;
typedef struct _GossipRosterGroup   GossipRosterGroup;

struct _GossipRoster {
        GObject           parent;
        GossipRosterPriv *priv;
};

struct _GossipRosterClass {
        GObjectClass      parent_class;
};

GType              gossip_roster_get_type              (void) G_GNUC_CONST;
GossipRoster *     gossip_roster_new                   (void);

GossipRosterGroup *gossip_roster_get_group             (GossipRoster      *roster,
							const gchar       *name);
GList *            gossip_roster_get_all_groups        (GossipRoster      *roster);

GossipRosterItem * gossip_roster_get_item              (GossipRoster      *roster,
							GossipJID         *jid);
GList *            gossip_roster_get_all_items         (GossipRoster      *roster);

gboolean           gossip_roster_get_show_active_users (GossipRoster      *roster);
void               gossip_roster_set_show_active_users (GossipRoster      *roster,
							gboolean           show);

void               gossip_roster_free_group_list       (GList             *list);
void               gossip_roster_free_item_list        (GList             *list);

/* Tries to find a roster by either trying str as a jid, and later checks 
 * all items and tries to match the item names */
GossipRosterItem * gossip_roster_find_item             (GossipRoster      *roster,
							const gchar       *str);
void               gossip_roster_remove_item           (GossipRoster      *roster,
							GossipRosterItem  *item);
void               gossip_roster_rename_item           (GossipRoster      *roster,
							GossipRosterItem  *item,
							const gchar       *name);
void               gossip_roster_rename_group          (GossipRoster      *roster,
							GossipRosterGroup *group,
							const gchar       *name);

/* FIXME: GossipContact API will replace GossipRosterItem */
GossipContact *    gossip_roster_get_contact_from_item (GossipRoster      *roster,
							GossipRosterItem  *item);
const gchar *      gossip_roster_get_active_resource   (GossipRoster      *roster,
							GossipContact     *contact);

/* Group */
const gchar *      gossip_roster_group_get_name        (GossipRosterGroup *group);
GList *            gossip_roster_group_get_items       (GossipRosterGroup *group);
GossipRosterGroup* gossip_roster_group_ref             (GossipRosterGroup *group);
void               gossip_roster_group_unref           (GossipRosterGroup *group);


/* Item */

/* Returns the JID (with Resource) that has the highest priority 
 * (or connected last). 
 */
GossipJID *       gossip_roster_item_get_jid           (GossipRosterItem *item);
const gchar *     gossip_roster_item_get_name          (GossipRosterItem *item);
GossipShow        gossip_roster_item_get_show          (GossipRosterItem *item);
const gchar *     gossip_roster_item_get_status        (GossipRosterItem *item);
const gchar *     gossip_roster_item_get_subscription  (GossipRosterItem *item);
const gchar *     gossip_roster_item_get_ask           (GossipRosterItem *item);
GList *           gossip_roster_item_get_groups        (GossipRosterItem *item);
gboolean          gossip_roster_item_is_offline        (GossipRosterItem *item);
GossipRosterItem *gossip_roster_item_ref               (GossipRosterItem *item);
void              gossip_roster_item_unref             (GossipRosterItem *item);
gint              gossip_roster_item_compare           (GossipRosterItem *a,
							GossipRosterItem *b);

gboolean          gossip_roster_item_get_active        (GossipRosterItem *item);
void              gossip_roster_item_set_active        (GossipRosterItem *item,
							gboolean          active);

/* Used when receiving messages from a person not in the roster */
GossipRosterItem * gossip_roster_item_new              (GossipJID         *jid);

/* This is a way to be able to move to GossipContact in steps.
 * It will create a new GossipContact out of the data in the GossipRosterItem
 */
/*GossipContact *  gossip_roster_item_get_contact    (GossipRosterItem  *item); */

#endif /* __GOSSIP_ROSTER_H__ */
