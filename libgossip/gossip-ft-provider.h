/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Imendio AB
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

#ifndef __GOSSIP_FT_PROVIDER_H__
#define __GOSSIP_FT_PROVIDER_H__

#include <glib-object.h>

#include "gossip-contact.h"
#include "gossip-ft.h"

G_BEGIN_DECLS

#define GOSSIP_TYPE_FT_PROVIDER           (gossip_ft_provider_get_type ())
#define GOSSIP_FT_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOSSIP_TYPE_FT_PROVIDER, GossipFTProvider))
#define GOSSIP_IS_FT_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOSSIP_TYPE_FT_PROVIDER))
#define GOSSIP_FT_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GOSSIP_TYPE_FT_PROVIDER, GossipFTProviderIface))

typedef struct _GossipFTProvider      GossipFTProvider;
typedef struct _GossipFTProviderIface GossipFTProviderIface;

struct _GossipFTProviderIface {
	GTypeInterface g_iface;

	/* Virtual Table */
	GossipFT*  (*send)    (GossipFTProvider *provider,
			       GossipContact    *contact,
			       const gchar      *file);
	void       (*cancel)  (GossipFTProvider *provider,
			       GossipFTId        id);
	void       (*accept)  (GossipFTProvider *provider,
			       GossipFTId        id);
	void       (*decline) (GossipFTProvider *provider,
			       GossipFTId        id);
};

GType      gossip_ft_provider_get_type (void) G_GNUC_CONST;

GossipFT * gossip_ft_provider_send     (GossipFTProvider *provider,
					GossipContact    *contact,
					const gchar      *file);
void       gossip_ft_provider_cancel   (GossipFTProvider *provider,
					GossipFTId        id);
void       gossip_ft_provider_accept   (GossipFTProvider *provider,
					GossipFTId        id);
void       gossip_ft_provider_decline  (GossipFTProvider *provider,
					GossipFTId        id);

G_END_DECLS

#endif /* __GOSSIP_FT_PROVIDER_H__ */
