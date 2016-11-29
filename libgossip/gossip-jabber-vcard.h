/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2004 Imendio AB
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

#ifndef __GOSSIP_JABBER_VCARD_H__
#define __GOSSIP_JABBER_VCARD_H__

#include <libgossip/gossip-async.h>
#include <libgossip/gossip-vcard.h>

#include "gossip-jabber.h"

G_BEGIN_DECLS

gboolean gossip_jabber_vcard_get (GossipJabber         *jabber,
                                  const gchar          *jid_str,
                                  GossipVCardCallback   callback,
                                  gpointer              user_data,
                                  GError              **error);
gboolean gossip_jabber_vcard_set (GossipJabber         *jabber,
                                  GossipVCard          *vcard,
                                  GossipCallback        callback,
                                  gpointer              user_data,
                                  GError              **error);

G_END_DECLS

#endif /* __GOSSIP_JABBER_VCARD_H__ */
