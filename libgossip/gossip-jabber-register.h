/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Imendio AB
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

#ifndef __GOSSIP_JABBER_REGISTER_H__
#define __GOSSIP_JABBER_REGISTER_H__

#include <glib.h>

#include "gossip-jabber.h"

G_BEGIN_DECLS

void gossip_jabber_register_account (GossipJabber        *jabber,
				     GossipAccount       *account,
				     GossipVCard         *vcard,
				     GossipErrorCallback  callback,
				     gpointer             user_data);
void gossip_jabber_register_cancel  (GossipJabber        *jabber);


G_END_DECLS

#endif /* __GOSSIP_JABBER_REGISTER_H__ */
