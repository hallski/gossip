/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio HB
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

#ifndef __GOSSIP_SESSION_H
#define __GOSSIP_SESSION_H__

#include <glib-object.h>

#include "gossip-contact.h"
#include "gossip-presence.h"

#define GOSSIP_TYPE_SESSION         (gossip_session_get_type ())
#define GOSSIP_SESSION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), \
 			             GOSSIP_TYPE_SESSION, GossipSession))
#define GOSSIP_SESSION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), \
				     GOSSIP_TYPE_SESSION, \
			             GossipSessionClass))
#define GOSSIP_IS_SESSION(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o),\
			             GOSSIP_TYPE_SESSION))
#define GOSSIP_IS_SESSION_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), \
				     GOSSIP_TYPE_SESSION))
#define GOSSIP_SESSION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),\
				     GOSSIP_TYPE_SESSION, \
				     GossipSessionClass))

typedef struct _GossipSession      GossipSession;
typedef struct _GossipSessionClass GossipSessionClass;
typedef struct _GossipSessionPriv  GossipSessionPriv;

struct _GossipSession {
	GObject parent;

	GossipSessionPriv *priv;
};

struct _GossipSessionClass {
	GObjectClass parent_class;
};

GType            gossip_session_get_type       (void) G_GNUC_CONST;

void             gossip_session_login          (GossipSession  *session);

void             gossip_session_send_msg       (GossipSession  *session,
						GossipContact  *contact,
						const char     *message);

void             gossip_session_send_typing    (GossipSession  *session,
						GossipContact  *contact,
						gboolean        typing);

void             gossip_session_set_presence   (GossipSession  *session,
						GossipPresence *presence);

/* Contact management */

/* Add, remove, move */

/* Returns a GSList of KolibriContact */
GSList *         gossip_session_get_contacts   (GossipSession  *session);

#endif /* __GOSSIP_SESSION_H__ */

