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

#ifndef __GOSSIP_FT_WINDOW_H__
#define __GOSSIP_FT_WINDOW_H__

#include <libgossip/gossip-session.h>
#include <libgossip/gossip-contact.h>


void gossip_ft_window_init               (GossipSession *session);
void gossip_ft_window_finalize           (GossipSession *session);

void gossip_ft_window_send_file          (GossipContact *account);
void gossip_ft_window_send_file_from_uri (GossipContact *contact,
					  const gchar   *file);

#endif /* __GOSSIP_FT_WINDOW_H__ */