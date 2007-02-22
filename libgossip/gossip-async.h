/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio AB
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

#include <glib.h>
#include <glib-object.h>

#include "gossip-vcard.h"
#include "gossip-version-info.h"

G_BEGIN_DECLS 

typedef struct {
	gpointer callback;
	gpointer user_data;
	gpointer data1;
	gpointer data2;
	gpointer data3;
} GossipCallbackData;

typedef enum {
	GOSSIP_RESULT_OK,
	GOSSIP_RESULT_ERROR_INVALID_REPLY,
	GOSSIP_RESULT_ERROR_TIMEOUT,
	GOSSIP_RESULT_ERROR_FAILED,
	GOSSIP_RESULT_ERROR_UNAVAILABLE
} GossipResult;

typedef void (*GossipVCardCallback)   (GossipResult       result,
				       GossipVCard       *vcard,
				       gpointer           user_data);
typedef void (*GossipCallback)        (GossipResult       result,
				       gpointer           user_data);
typedef void (*GossipErrorCallback)   (GossipResult       result,
				       GError            *error,
				       gpointer           user_data);
typedef void (*GossipVersionCallback) (GossipResult       result,
				       GossipVersionInfo *info,
				       gpointer           user_data);

G_END_DECLS 

#endif /* __GOSSIP_ASYNC_H__ */
