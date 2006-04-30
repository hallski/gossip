/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio AB
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

#ifndef __GOSSIP_STOCK_H__
#define __GOSSIP_STOCK_H__

#define GOSSIP_STOCK_OFFLINE             "gossip-offline"
#define GOSSIP_STOCK_AVAILABLE           "gossip-available"
#define GOSSIP_STOCK_BUSY                "gossip-busy"
#define GOSSIP_STOCK_AWAY                "gossip-away"
#define GOSSIP_STOCK_EXT_AWAY            "gossip-extended-away"
#define GOSSIP_STOCK_PENDING             "gossip-pending"

#define GOSSIP_STOCK_MESSAGE             "gossip-message"
#define GOSSIP_STOCK_TYPING              "gossip-typing"


#define GOSSIP_STOCK_CONTACT_INFORMATION "vcard_16"

#define GOSSIP_STOCK_AIM                 "gossip-aim"
#define GOSSIP_STOCK_ICQ                 "gossip-icq"
#define GOSSIP_STOCK_MSN                 "gossip-msn"
#define GOSSIP_STOCK_YAHOO               "gossip-yahoo"

#define GOSSIP_STOCK_GROUP_MESSAGE       "gossip-group-message"

void gossip_stock_init (void);

#endif /* __GOSSIP_STOCK_ICONS_H__ */
