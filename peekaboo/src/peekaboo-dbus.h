/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2006 Imendio AB
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
 */

#ifndef __PEEKABO_DBUS_H__
#define __PEEKABO_DBUS_H__

#include <dbus/dbus-glib.h>

gboolean peekaboo_dbus_get_roster_visible (void);
char **  peekaboo_dbus_get_open_chats     (void);
void     peekaboo_dbus_send_message       (const gchar *contact_id);
void     peekaboo_dbus_new_message        (void);
void     peekaboo_dbus_toggle_roster      (void);

#endif /* __PEEKABO_DBUS_H__ */