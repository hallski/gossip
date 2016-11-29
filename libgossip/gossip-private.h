/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * 
 * Authors: Martyn Russell
 */

#ifndef __GOSSIP_PRIVATE_H__
#define __GOSSIP_PRIVATE_H__

G_BEGIN_DECLS

#include "gossip-log.h"
#include "gossip-session.h"
#include "gossip-contact-manager.h"
#include "gossip-chatroom-manager.h"
#include "gossip-account-manager.h"

GossipChatroomManager *gossip_chatroom_manager_new (GossipAccountManager  *account_manager,
                                                    GossipContactManager  *contact_manager,
                                                    const gchar           *filename);
GossipContactManager * gossip_contact_manager_new  (GossipSession         *session,
                                                    const gchar           *filename);
GossipAccountManager * gossip_account_manager_new  (const gchar           *filename);
GossipLogManager *     gossip_log_manager_new      (GossipSession         *session);

G_END_DECLS

#endif /* __GOSSIP_PRIVATE_H__ */

