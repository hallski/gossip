/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio HB
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


struct _GossipRoster {
        GHashTable       *items;
        GHashTable       *groups;

        LmConnect        *connection;
        LmMessageHandler *presence_handler;
        LmMessageHandler *iq_handler;
};
   
struct _GossipRosterGroup {
        gchar *name;
        GList *items; /* List of RosterItems */
};

struct _GossipRosterItem {
        gchar        *jid;
        gchar        *name;
        GossipStatus *show;
        gchar        *status;
        gchar        *subscription;
        gchar        *ask;
        GList        *groups; /* Group names as strings */
        GList        *resources; /* Should be {Resource, Priority} */
};
    
/* Signals for item_added, item_removed, ... */

enum {
        LAST_SIGNAL
};

GObjectClass *parent_class;
static guint signals[LAST_SIGNAL];

static void     roster_class_init            (GossipRosterClass *klass);
static void     roster_init                  (GossipRoster      *roster);
static void     roster_finalize              (GObject           *object);

GType
gossip_roster_get_type (void)
{
        static GType object_type = 0;
         
        if (!object_type) {
                static const GTypeInfo object_info = {
                        sizeof (GossipRosterClass),
                        NULL,           /* base_init */
                        NULL,           /* base_finalize */
                        (GClassInitFunc) roster_class_init,
                        NULL,           /* class_finalize */
                        NULL,           /* class_data */
                        sizeof (GossipRoster),
                        0,              /* n_preallocs */
                        (GInstanceInitFunc) roster_init,
                };
 
                object_type = g_type_register_static (G_TYPE_OBJECT,
                                                      "GossipRoster",
                                                      &object_info,
                                                      0);
        }
 
        return object_type;
}

static void     
roster_class_init (GossipRosterClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS(klass);

        parent_class = G_OBJECT_CLASS(g_type_class_peak_parent(klass));
        
        object_class->finalize = roster_finalize;
        
        /* Create signals */
}

static void
roster_init (GossipRos *roster)
{
        GossipRosterPriv *priv;

        priv = g_new0 (GossipRosterPriv, 1);
        roster->priv = priv;

        priv->items  = g_hash_table_new (g_str_hash, g_str_equal);
        priv->groups = g_hash_table_new (g_str_hash, g_str_equal);

        priv->connection       = NULL;
        priv->presence_handler = NULL;
        priv->iq_handler       = NULL;
}

static void
roster_finalize (GObject *object)
{
        GossipRoster     *roster;
        GossipRosterPriv *priv;

        roster = GOSSIP_ROSTER(object);
        priv   = roster->priv;

        g_hash_table_destroy (priv->items);
        g_hash_table_destroy (priv->groups);
}

