/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2005 Imendio AB
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

#ifndef __GOSSIP_LOG_H__
#define __GOSSIP_LOG_H__

#include <glib.h>
#include <libgossip/gossip-message.h>

#define GOSSIP_TYPE_LOG         (gossip_log_get_type ())
#define GOSSIP_LOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_LOG, GossipLog))
#define GOSSIP_LOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_LOG, GossipLogClass))
#define GOSSIP_IS_LOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_LOG))
#define GOSSIP_IS_LOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_LOG))
#define GOSSIP_LOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_LOG, GossipLogClass))

typedef struct _GossipLog      GossipLog;
typedef struct _GossipLogClass GossipLogClass;
typedef struct _GossipLogPriv  GossipLogPriv;

struct _GossipLog {
	GObject      parent;
};


struct _GossipLogClass {
	GObjectClass parent_class;
};

GType          gossip_log_get_type        (void) G_GNUC_CONST;
GossipLog *    gossip_log_get             (GossipContact *contact);
GossipContact *gossip_log_get_own_contact (GossipLog     *log);
GList *        gossip_log_get_contacts    (GossipAccount *account);
GList *        gossip_log_get_messages    (GossipLog     *log,
					   const gchar   *date);
GList *        gossip_log_get_dates       (GossipLog     *log);

void           gossip_log_message         (GossipLog     *log,
					   GossipMessage *message,
					   gboolean       incoming);
gboolean       gossip_log_exists          (GossipLog     *log);
void           gossip_log_show            (GtkWidget     *window,
					   GossipContact *contact);

#endif /* __GOSSIP_LOG_H__ */
