/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002 Richard Hult <richard@imendio.com>
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

#ifndef __GOSSIP_APP_H__
#define __GOSSIP_APP_H__

#include <glib-object.h>
#include "gossip-account.h"

#define GOSSIP_TYPE_APP         (gossip_app_get_type ())
#define GOSSIP_APP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_APP, GossipApp))
#define GOSSIP_APP_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_APP, GossipAppClass))
#define GOSSIP_IS_APP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_APP))
#define GOSSIP_IS_APP_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_APP))
#define GOSSIP_APP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_APP, GossipAppClass))

typedef struct _GossipApp      GossipApp;
typedef struct _GossipAppClass GossipAppClass;
typedef struct _GossipAppPriv  GossipAppPriv;

#include "gossip-roster-old.h"

struct _GossipApp {
        GObject        parent;

        GossipAppPriv *priv;
};

struct _GossipAppClass {
        GObjectClass parent_class;
};

/* Note: We don't support "free to chat". */
typedef enum {
	GOSSIP_SHOW_AVAILABLE, /* available (null) */
	GOSSIP_SHOW_BUSY,      /* busy (dnd) */
	GOSSIP_SHOW_AWAY,      /* away (away) */
	GOSSIP_SHOW_EXT_AWAY   /* extended away (xa) */
} GossipShow;

/*typedef enum {
	GOSSIP_STATUS_AVAILABLE,
	GOSSIP_STATUS_AWAY,
	GOSSIP_STATUS_EXT_AWAY,
	GOSSIP_STATUS_BUSY,
	GOSSIP_STATUS_OFFLINE
} GossipStatus;
*/

GType            gossip_app_get_type                (void) G_GNUC_CONST;
void             gossip_app_create                  (void);
void             gossip_app_connect                 (void);
void             gossip_app_join_group_chat         (const gchar   *room,
						     const gchar   *server,
						     const gchar   *nick);
const gchar *    gossip_app_get_username            (void);
GossipJID *      gossip_app_get_jid                 (void);
GossipRosterOld *gossip_app_get_roster              (void);
LmConnection *   gossip_app_get_connection          (void);
GossipShow       gossip_app_get_show                (void);
GossipApp *      gossip_app_get                     (void);
void             gossip_app_set_overridden_resource (const gchar   *resource);
gboolean         gossip_app_is_connected            (void);


#endif /* __GOSSIP_APP_H__ */
