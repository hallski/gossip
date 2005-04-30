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

#ifndef __GOSSIP_CONTACT_INFO_INFO_H__
#define __GOSSIP_CONTACT_INFO_INFO_H__

#include <glib-object.h>

#include <libgossip/gossip-contact.h>

#define GOSSIP_TYPE_CONTACT_INFO         (gossip_contact_info_get_type ())
#define GOSSIP_CONTACT_INFO(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CONTACT_INFO, GossipContactInfo))
#define GOSSIP_CONTACT_INFO_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_CONTACT_INFO, GossipContactInfoClass))
#define GOSSIP_IS_CONTACT_INFO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CONTACT_INFO))
#define GOSSIP_IS_CONTACT_INFO_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CONTACT_INFO))
#define GOSSIP_CONTACT_INFO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CONTACT_INFO, GossipContactInfoClass))

typedef struct _GossipContactInfo      GossipContactInfo;
typedef struct _GossipContactInfoClass GossipContactInfoClass;

struct _GossipContactInfo {
	GObject parent;
};

struct _GossipContactInfoClass {
	GObjectClass parent_class;
};

GType               gossip_contact_info_get_type   (void) G_GNUC_CONST;
GossipContactInfo * gossip_contact_info_new        (GossipContact     *c);

GtkWidget *         gossip_contact_info_get_dialog (GossipContactInfo *info);


#endif /* __GOSSIP_CONTACT_INFO_INFO_H__ */
