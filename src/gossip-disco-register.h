/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Martyn Russell (mr@gnome.org)
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

#ifndef __GOSSIP_DISCO_REGISTER_H__
#define __GOSSIP_DISCO_REGISTER_H__

#include <glib-object.h>
#include <loudmouth/loudmouth.h>
#include "gossip-disco-register.h"
#include "gossip-jid.h"

G_BEGIN_DECLS


typedef struct _GossipDiscoRegister GossipDiscoRegister;

typedef void (*GossipDiscoRegisterFunc) (GossipDiscoRegister *reg,
					 const gchar         *error_code,
					 const gchar         *error_reason,
					 gpointer             user_data);


void                 gossip_disco_register_destroy              (GossipDiscoRegister     *reg);

GossipDiscoRegister *gossip_disco_register_requirements         (const char              *to,
								 GossipDiscoRegisterFunc  func,
								 gpointer                 user_data);

gchar               *gossip_disco_register_get_instructions     (GossipDiscoRegister     *reg);
gboolean             gossip_disco_register_get_require_username (GossipDiscoRegister     *reg);
gboolean             gossip_disco_register_get_require_password (GossipDiscoRegister     *reg);
gboolean             gossip_disco_register_get_require_email    (GossipDiscoRegister     *reg);
gboolean             gossip_disco_register_get_require_nickname (GossipDiscoRegister     *reg);

GossipDiscoRegister *gossip_disco_register_request              (GossipDiscoRegister     *reg,
								 GossipDiscoRegisterFunc  func,
								 gpointer                 user_data);

void                 gossip_disco_register_set_username         (GossipDiscoRegister     *reg,
								 const gchar             *username);
void                 gossip_disco_register_set_password         (GossipDiscoRegister     *reg,
								 const gchar             *password);
void                 gossip_disco_register_set_email            (GossipDiscoRegister     *reg,
								 const gchar             *email);
void                 gossip_disco_register_set_nickname         (GossipDiscoRegister     *reg,
								 const gchar             *nickname);

gboolean             gossip_disco_register_check_registered     (GossipJID               *jid);


        
G_END_DECLS

#endif /* __GOSSIP_DISCO_REGISTER_H__ */
