/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* 
 * Copyright (C) 2006-2007 Imendio AB
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#ifndef __PEEKABO_DBUS_H__
#define __PEEKABO_DBUS_H__

#include <dbus/dbus-glib.h>

#include <libgossip/gossip-presence.h>

gboolean peekaboo_dbus_get_presence       (const gchar           *id,
                                           GossipPresenceState   *state,
                                           gchar                **status);
gboolean peekaboo_dbus_get_name           (const gchar           *id,
                                           gchar                **name);
gboolean peekaboo_dbus_get_roster_visible (gboolean              *visible);
gboolean peekaboo_dbus_get_open_chats     (gchar               ***open_chats);
gboolean peekaboo_dbus_send_message       (const gchar           *contact_id);
gboolean peekaboo_dbus_new_message        (void);
gboolean peekaboo_dbus_toggle_roster      (void);

#endif /* __PEEKABO_DBUS_H__ */
