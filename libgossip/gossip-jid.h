/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef __GOSSIP_JID_H__
#define __GOSSIP_JID_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_JID         (gossip_jid_get_type ())
#define GOSSIP_JID(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_JID, GossipJID))
#define GOSSIP_JID_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_JID, GossipJIDClass))
#define GOSSIP_IS_JID(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_JID))
#define GOSSIP_IS_JID_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_JID))
#define GOSSIP_JID_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_JID, GossipJIDClass))

typedef struct _GossipJID      GossipJID;
typedef struct _GossipJIDClass GossipJIDClass;

struct _GossipJID {
    GObject parent;
};

struct _GossipJIDClass {
    GObjectClass parent_class;
};

GType        gossip_jid_get_type                 (void) G_GNUC_CONST;

GossipJID *  gossip_jid_new                      (const gchar   *str_jid);
void         gossip_jid_set_without_resource     (GossipJID     *jid,
                                                  const gchar   *str);
void         gossip_jid_set_resource             (GossipJID     *jid,
                                                  const gchar   *resource);
const gchar *gossip_jid_get_full                 (GossipJID     *jid);
const gchar *gossip_jid_get_without_resource     (GossipJID     *jid);
const gchar *gossip_jid_get_resource             (GossipJID     *jid);
gchar *      gossip_jid_get_part_name            (GossipJID     *jid);
const gchar *gossip_jid_get_part_host            (GossipJID     *jid);
gboolean     gossip_jid_is_service               (GossipJID     *jid);

/* Compare functions */
gboolean     gossip_jid_equals                   (GossipJID     *jid_a,
                                                  GossipJID     *jid_b);
gboolean     gossip_jid_equals_without_resource  (GossipJID     *jid_a,
                                                  GossipJID     *jid_b);

/* String functions */
gboolean     gossip_jid_string_is_valid          (const gchar   *str,
                                                  gboolean       with_resource);
gchar *      gossip_jid_string_get_part_name     (const gchar   *str);
gchar *      gossip_jid_string_get_part_host     (const gchar   *str);
const gchar *gossip_jid_string_get_part_resource (const gchar   *str);
gchar *      gossip_jid_string_escape            (const gchar   *jid_str);
gchar *      gossip_jid_string_unescape          (const gchar   *jid_str);

gint         gossip_jid_case_compare             (gconstpointer  a,
                                                  gconstpointer  b);

/* Hash table functions */
gboolean     gossip_jid_equal                    (gconstpointer  v1,
                                                  gconstpointer  v2);
guint        gossip_jid_hash                     (gconstpointer  key);

gboolean     gossip_jid_equal_without_resource   (gconstpointer  v1,
                                                  gconstpointer  v2);
guint        gossip_jid_hash_without_resource    (gconstpointer  key);

const gchar *gossip_jid_get_example_string       (void);

G_END_DECLS

#endif /* __GOSSIP_JID_H__ */
