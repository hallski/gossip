/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Imendio AB
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

#include "gossip-async.h"

GossipCallbackData *
gossip_callback_data_new (gpointer callback,
                          gpointer user_data,
                          gpointer data1,
                          gpointer data2,
                          gpointer data3)
{
	GossipCallbackData *data;

	data = g_slice_new0 (GossipCallbackData);

	data->callback = callback;
	data->user_data = user_data;
        data->data1 = data1;
        data->data2 = data2;
        data->data3 = data3;
	
	return data;
}

void
gossip_callback_data_free (GossipCallbackData *data)
{
        if (!data) {
                return;
        }

        g_slice_free (GossipCallbackData, data);
}
