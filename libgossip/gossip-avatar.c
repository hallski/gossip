/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Xavier Claessens <xclaesse@gmail.com>
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

#include <config.h>

#include "gossip-avatar.h"
#include "gossip-debug.h"
#include "gossip-utils.h"

#define DEBUG_DOMAIN "Avatar"

GossipAvatar *
gossip_avatar_new (guchar *data,
		   gsize   len,
		   gchar  *format)
{
	GossipAvatar *avatar;

	g_return_val_if_fail (data != NULL, NULL);
	g_return_val_if_fail (len > 0, NULL);
	g_return_val_if_fail (format != NULL, NULL);

	avatar = g_slice_new0 (GossipAvatar);
	avatar->data = g_memdup (data, len);
	avatar->len = len;
	avatar->format = g_strdup (format);
	avatar->refcount = 1;

	return avatar;
}

void
gossip_avatar_unref (GossipAvatar *avatar)
{
	if (!avatar) {
		return;
	}

	avatar->refcount--;
	if (avatar->refcount == 0) {
		g_free (avatar->data);
		g_free (avatar->format);
		g_slice_free (GossipAvatar, avatar);
	}
}

GossipAvatar *
gossip_avatar_ref (GossipAvatar *avatar)
{
	if (!avatar) {
		return NULL;
	}

	avatar->refcount++;

	return avatar;
}

