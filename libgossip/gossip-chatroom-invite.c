/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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

#include <glib.h>

#include "gossip-chatroom-invite.h"

struct _GossipChatroomInvite {
	guint          ref_count;

	GossipContact *inviter;
	gchar         *id;
	gchar         *reason;
};

GType
gossip_chatroom_invite_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static
			("GossipChatroomInvite",
			 (GBoxedCopyFunc) gossip_chatroom_invite_ref,
			 (GBoxedFreeFunc) gossip_chatroom_invite_unref);
	}

	return type;
}

GossipChatroomInvite *
gossip_chatroom_invite_new (GossipContact *inviter,
			    const gchar   *id,
			    const gchar   *reason)
{
	GossipChatroomInvite *invite;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (inviter), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	invite = g_new0 (GossipChatroomInvite, 1);

	invite->ref_count = 1;

	invite->inviter = g_object_ref (inviter);
	invite->id = g_strdup (id);
		invite->reason = g_strdup (reason);

	return invite;
}

GossipChatroomInvite *
gossip_chatroom_invite_ref (gpointer data)
{
	GossipChatroomInvite *invite = data;

	g_return_val_if_fail (invite != NULL, NULL);
	g_return_val_if_fail (invite->ref_count > 0, NULL);

	invite->ref_count++;

	return invite;
}

void
gossip_chatroom_invite_unref (gpointer data)
{
	GossipChatroomInvite *invite = data;
	
	g_return_if_fail (invite != NULL);
	g_return_if_fail (invite->ref_count > 0);

	invite->ref_count--;

	if (invite->ref_count > 0) {
		return;
	}

	if (invite->inviter) {
		g_object_unref (invite->inviter);
	}

	g_free (invite->id);
	g_free (invite->reason);
}

GossipContact *
gossip_chatroom_invite_get_inviter (GossipChatroomInvite *invite)
{
	g_return_val_if_fail (invite != NULL, NULL);

	return invite->inviter;
}

const gchar *
gossip_chatroom_invite_get_id (GossipChatroomInvite *invite)
{
	g_return_val_if_fail (invite != NULL, NULL);

	return invite->id;
}

const gchar *
gossip_chatroom_invite_get_reason (GossipChatroomInvite *invite)
{
	g_return_val_if_fail (invite != NULL, NULL);

	return invite->reason;
}

