/* -*- mode: C; c-file-style: "gnu" -*- */
/*
 * Copyright (C) 2004 Imendio HB
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

#ifndef __GOSSIP_DBUS_H__
#define __GOSSIP_DBUS_H__

#include <dbus/dbus-glib.h>

#define GOSSIP_SERVICE "org.imendio.gossip"
#define GOSSIP_INTERFACE "org.imendio.Gossip"
#define GOSSIP_OBJECT "/org/imendio/Gossip"
#define GOSSIP_SET_STATUS "set_Status"

gboolean gossip_dbus_init (void);


#endif /* __DBUS_H__ */
