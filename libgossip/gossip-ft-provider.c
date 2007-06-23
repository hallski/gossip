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
 *
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include "gossip-ft-provider.h"

#include "libgossip-marshal.h"

static void ft_provider_base_init (gpointer g_class);

enum {
	REQUEST,
	CANCELLED,
	COMPLETE,
	PROGRESS,
	ERROR,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

GType
gossip_ft_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =
		{
			sizeof (GossipFTProviderIface),
			ft_provider_base_init,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "GossipFTProvider",
					       &info, 0);

		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

static void
ft_provider_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		signals[REQUEST] =
			g_signal_new ("file-transfer-request",
				      G_TYPE_FROM_CLASS (g_class),
				      G_SIGNAL_RUN_LAST,
				      0,
				      NULL, NULL,
				      libgossip_marshal_VOID__OBJECT,
				      G_TYPE_NONE,
				      1, GOSSIP_TYPE_FT);

		signals[CANCELLED] =
			g_signal_new ("file-transfer-cancelled",
				      G_TYPE_FROM_CLASS (g_class),
				      G_SIGNAL_RUN_LAST,
				      0,
				      NULL, NULL,
				      libgossip_marshal_VOID__INT,
				      G_TYPE_NONE,
				      1, G_TYPE_INT);

		signals[COMPLETE] =
			g_signal_new ("file-transfer-complete",
				      G_TYPE_FROM_CLASS (g_class),
				      G_SIGNAL_RUN_LAST,
				      0,
				      NULL, NULL,
				      libgossip_marshal_VOID__OBJECT,
				      G_TYPE_NONE,
				      1, GOSSIP_TYPE_FT);

		signals[PROGRESS] =
			g_signal_new ("file-transfer-progress",
				      G_TYPE_FROM_CLASS (g_class),
				      G_SIGNAL_RUN_LAST,
				      0,
				      NULL, NULL,
				      libgossip_marshal_VOID__OBJECT_DOUBLE,
				      G_TYPE_NONE,
				      2, GOSSIP_TYPE_FT, G_TYPE_DOUBLE);

		signals[ERROR] =
			g_signal_new ("file-transfer-error",
				      G_TYPE_FROM_CLASS (g_class),
				      G_SIGNAL_RUN_LAST,
				      0,
				      NULL, NULL,
				      libgossip_marshal_VOID__OBJECT_POINTER,
				      G_TYPE_NONE,
				      2, GOSSIP_TYPE_FT, G_TYPE_POINTER);

		initialized = TRUE;
	}
}

GossipFTId
gossip_ft_provider_send (GossipFTProvider *provider,
			 GossipContact    *contact,
			 const gchar      *file)
{
	g_return_val_if_fail (GOSSIP_IS_FT_PROVIDER (provider), 0);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), 0);
	g_return_val_if_fail (file != NULL, 0);

	if (GOSSIP_FT_PROVIDER_GET_IFACE (provider)->send) {
		return GOSSIP_FT_PROVIDER_GET_IFACE (provider)->send (provider,
								      contact,
								      file);
	}

	return 0;
}

void
gossip_ft_provider_cancel (GossipFTProvider *provider,
			   GossipFTId        id)
{
	g_return_if_fail (GOSSIP_IS_FT_PROVIDER (provider));
	g_return_if_fail (id > 0);

	if (GOSSIP_FT_PROVIDER_GET_IFACE (provider)->cancel) {
		GOSSIP_FT_PROVIDER_GET_IFACE (provider)->cancel (provider,
								 id);
	}
}

void
gossip_ft_provider_accept (GossipFTProvider *provider,
			   GossipFTId        id)
{
	g_return_if_fail (GOSSIP_IS_FT_PROVIDER (provider));
	g_return_if_fail (id > 0);

	if (GOSSIP_FT_PROVIDER_GET_IFACE (provider)->accept) {
		GOSSIP_FT_PROVIDER_GET_IFACE (provider)->accept (provider,
								 id);
	}
}

void
gossip_ft_provider_decline (GossipFTProvider *provider,
			    GossipFTId        id)
{
	g_return_if_fail (GOSSIP_IS_FT_PROVIDER (provider));
	g_return_if_fail (id > 0);

	if (GOSSIP_FT_PROVIDER_GET_IFACE (provider)->decline) {
		GOSSIP_FT_PROVIDER_GET_IFACE (provider)->decline (provider,
								  id);
	}
}
