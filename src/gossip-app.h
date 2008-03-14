/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2007 Imendio AB
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

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-event-manager.h>
#include <libgossip/gossip-chatroom-manager.h>

#include "gossip-heartbeat.h"
#include "gossip-self-presence.h"
#include "gossip-chat-manager.h"

G_BEGIN_DECLS

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
	GObject parent;
};

struct _GossipAppClass {
	GObjectClass parent_class;
};

GType                  gossip_app_get_type             (void) G_GNUC_CONST;
void                   gossip_app_connect              (GossipAccount        *account,
							gboolean              startup);
void                   gossip_app_disconnect           (GossipAccount        *account);
void                   gossip_app_net_down             (void);
void                   gossip_app_net_up               (void);
void                   gossip_app_create               (GossipSession        *session);
GossipApp *            gossip_app_get                  (void);
gboolean               gossip_app_is_connected         (void);
gboolean               gossip_app_is_window_visible    (void);
void                   gossip_app_toggle_visibility    (void);
void                   gossip_app_set_visibility       (gboolean              visible);
void                   gossip_app_set_not_away         (void);
void                   gossip_app_set_presence         (GossipPresenceState   state,
							const gchar          *status);
GtkWidget *            gossip_app_get_window           (void);
GossipSession *        gossip_app_get_session          (void);
GossipChatroomManager *gossip_app_get_chatroom_manager (void);
GossipChatManager *    gossip_app_get_chat_manager     (void);
GossipEventManager *   gossip_app_get_event_manager    (void);
GossipSelfPresence *   gossip_app_get_self_presence    (void);
GossipHeartbeat *      gossip_app_get_flash_heartbeat  (void);

G_END_DECLS

#endif /* __GOSSIP_APP_H__ */
