/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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

#ifndef __GOSSIP_ACCOUNT_WIDGET_JABBER_H__
#define __GOSSIP_ACCOUNT_WIDGET_JABBER_H__

#include <libgossip/gossip-account.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_ACCOUNT_WIDGET_JABBER         (gossip_account_widget_jabber_get_type ())
#define GOSSIP_ACCOUNT_WIDGET_JABBER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_ACCOUNT_WIDGET_JABBER, GossipAccountWidgetJabber))
#define GOSSIP_ACCOUNT_WIDGET_JABBER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_ACCOUNT_WIDGET_JABBER, GossipAccountWidgetJabberClass))
#define GOSSIP_IS_ACCOUNT_WIDGET_JABBER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_ACCOUNT_WIDGET_JABBER))
#define GOSSIP_IS_ACCOUNT_WIDGET_JABBER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_ACCOUNT_WIDGET_JABBER))
#define GOSSIP_ACCOUNT_WIDGET_JABBER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_ACCOUNT_WIDGET_JABBER, GossipAccountWidgetJabberClass))

typedef struct _GossipAccountWidgetJabber      GossipAccountWidgetJabber;
typedef struct _GossipAccountWidgetJabberClass GossipAccountWidgetJabberClass;

struct _GossipAccountWidgetJabber {
	GtkVBox      parent;
};

struct _GossipAccountWidgetJabberClass {
	GtkVBoxClass parent_class;
};

GType      gossip_account_widget_jabber_get_type   (void) G_GNUC_CONST;
GtkWidget *gossip_account_widget_jabber_new        (GossipAccount *account);

G_END_DECLS

#endif /* __GOSSIP_ACCOUNT_WIDGET_JABBER_H__ */
