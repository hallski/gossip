/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#ifndef __GOSSIP_JABBER_HELPER_H__
#define __GOSSIP_JABBER_HELPER_H__

#include <loudmouth/loudmouth.h>

#include "gossip-async.h"
#include "gossip-contact.h"
#include "gossip-vcard.h"

gboolean        gossip_jabber_helper_async_get_vcard (LmConnection   *connection,
						      const gchar    *jid_str,
						      GossipAsyncVCardCallback callback,
						      gpointer        user_data,
						      GError         **error);
gboolean        gossip_jabber_helper_async_set_vcard (LmConnection   *connection,
						      GossipVCard    *vcard,
						      GossipAsyncResultCallback callback,
						      gpointer        user_data,
						      GError         **error);
gboolean        gossip_jabber_helper_async_get_version (LmConnection   *connection,
							GossipContact  *contact,
							GossipAsyncVersionCallback callback,
							gpointer        user_data,
							GError         **error);
const gchar * 
gossip_jabber_helper_presence_state_to_string       (GossipPresence *presence);

GossipPresenceState
gossip_jabber_helper_presence_state_from_str        (const gchar    *str);
const gchar *
gossip_jabber_helper_get_timestamp_from_message (LmMessage *message);

#endif /* __GOSSIP_JABBER_HELPER_H__ */
