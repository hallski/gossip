/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006-2007 Imendio AB
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
 * 
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#ifndef __GOSSIP_NOTIFY_H__
#define __GOSSIP_NOTIFY_H__

#include <libgossip/gossip-session.h>
#include <libgossip/gossip-event-manager.h>

G_BEGIN_DECLS

void gossip_notify_init                   (GossipSession      *session,
					   GossipEventManager *event_manager);
void gossip_notify_finalize               (void);
void gossip_notify_set_attach_widget      (GtkWidget          *new_attach_widget);
void gossip_notify_set_attach_status_icon (GtkStatusIcon      *new_attach);
gboolean gossip_notify_hint_show          (const gchar        *conf_path,
					   const gchar        *summary,
					   const gchar        *message,
					   GFunc               func,
					   gpointer            user_data);

G_END_DECLS

#endif /* __GOSSIP_NOTIFY_H__ */
