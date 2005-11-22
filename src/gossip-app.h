/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2004 Imendio AB
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
#include <gtk/gtkwidget.h>
#include <gconf/gconf-client.h>

#include <libgossip/gossip-account-manager.h>
#include <libgossip/gossip-event-manager.h>
#include <libgossip/gossip-session.h>

#include "gossip-chat-manager.h"
#include "gossip-ui-utils.h"

#define GOSSIP_TYPE_APP         (gossip_app_get_type ())
#define GOSSIP_APP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_APP, GossipApp))
#define GOSSIP_APP_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_APP, GossipAppClass))
#define GOSSIP_IS_APP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_APP))
#define GOSSIP_IS_APP_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_APP))
#define GOSSIP_APP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_APP, GossipAppClass))


typedef struct _GossipApp      GossipApp;
typedef struct _GossipAppClass GossipAppClass;
typedef struct _GossipAppPriv  GossipAppPriv;


struct _GossipApp {
        GObject        parent;

        GossipAppPriv *priv;
};


struct _GossipAppClass {
        GObjectClass parent_class;
};


GType               gossip_app_get_type          (void) G_GNUC_CONST;

void                gossip_app_create            (GossipAccountManager *manager);
void                gossip_app_connect           (gboolean              startup);
void                gossip_app_net_down          (void);
void                gossip_app_net_up            (void);
const gchar *       gossip_app_get_username      (void);
GossipApp *         gossip_app_get               (void);
gboolean            gossip_app_is_connected      (void);
GtkWidget *         gossip_app_get_window        (void);
GConfClient *       gossip_app_get_gconf_client  (void);
void                gossip_app_force_non_away    (void);
void                gossip_app_set_presence      (GossipPresenceState   state,
						  const gchar          *status);
GossipSession *     gossip_app_get_session       (void);
GossipChatManager  *gossip_app_get_chat_manager  (void);
GossipEventManager *gossip_app_get_event_manager (void);




#endif /* __GOSSIP_APP_H__ */
