/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#ifndef __GOSSIP_ASYNC_H__
#define __GOSSIP_ASYNC_H__

#include "gossip-vcard.h"
#include "gossip-version-info.h"

typedef enum {
	GOSSIP_ASYNC_OK,
	GOSSIP_ASYNC_ERROR_INVALID_REPLY,
	GOSSIP_ASYNC_ERROR_TIMEOUT,
        GOSSIP_ASYNC_ERROR_REGISTRATION
} GossipAsyncResult;

typedef void (*GossipAsyncRegisterCallback) (GossipAsyncResult  result,
                                             const gchar       *err_message,
                                             gpointer           user_data);
typedef void (*GossipAsyncVCardCallback)    (GossipAsyncResult  result,
                                             GossipVCard       *vcard,
                                             gpointer           user_data);

typedef void (*GossipAsyncResultCallback)   (GossipAsyncResult  result,
                                             gpointer           user_data);
typedef void (*GossipAsyncVersionCallback)  (GossipAsyncResult  result,
                                             GossipVersionInfo *info,
                                             gpointer           user_data);

#endif /* __GOSSIP_ASYNC_H__ */
