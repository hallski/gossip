/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Richard Hult <richard@imendio.com>
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

#ifndef __GOSSIP_JOIN_DIALOG_H__
#define __GOSSIP_JOIN_DIALOG_H__

#include "gossip-app.h"

typedef struct _GossipJoinDialog GossipJoinDialog;


GossipJoinDialog * gossip_join_dialog_new        (GossipApp        *app);

GtkWidget *        gossip_join_dialog_get_dialog (GossipJoinDialog *dialog);


#endif /* __GOSSIP_JOIN_DIALOG_H__ */
