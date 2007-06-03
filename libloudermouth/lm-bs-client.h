/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Free Software Foundation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 */

#ifndef __LM_BS_CLIENT_H__
#define __LM_BS_CLIENT_H__

#include "lm-internals.h"

#if !defined (LM_INSIDE_LOUDMOUTH_H) && !defined (LM_COMPILATION)
#error "Only <loudmouth/loudmouth.h> can be included directly, this file may disappear or change contents."
#endif


G_BEGIN_DECLS

#define LM_BS_CLIENT(obj) (LmBsClient *) obj;

typedef struct _LmBsClient LmBsClient;

typedef enum {
	LM_BS_CLIENT_STATUS_INITIAL,
	LM_BS_CLIENT_STATUS_CONNECTING,
	LM_BS_CLIENT_STATUS_CONNECTED,
	LM_BS_CLIENT_STATUS_AUTHENTICATED,
	LM_BS_CLIENT_STATUS_INVALID,
	LM_BS_CLIENT_STATUS_INTERRUPTED
} LmBsClientStatus;

typedef void (* LmBsClientFunction)               (LmBsClient   *client,
						   gpointer       user_data);
typedef void (* LmBsClientReadFunction)           (LmBsClient   *client,
						   gpointer      user_data,
						   gpointer      user_data2);

LmBsClient *     lm_bs_client_new                 (guint64       port,
						   const gchar  *host,
						   LmCallback   *connected_cb,
						   LmCallback   *disconnected_cb,
						   LmCallback   *data_read_cb,
						   LmCallback   *data_written_cb);
LmBsClient *     lm_bs_client_new_from_fd         (gint          fd,
						   GMainContext *context);
LmBsClient *     lm_bs_client_new_with_context    (guint64       port,
						   const gchar  *host,
						   LmCallback   *connected_cb,
						   LmCallback   *disconnected_cb,
						   LmCallback   *data_read_cb,
						   LmCallback   *data_written_cb,
						   GMainContext *context);
guint            lm_bs_client_connect             (LmBsClient   *client);
guint            lm_bs_client_stop                (LmBsClient   *client);
LmBsClient *     lm_bs_client_ref                 (LmBsClient   *client);
void             lm_bs_client_unref               (LmBsClient   *client);
gchar *          lm_bs_client_get_host            (LmBsClient   *client);
LmSocket         lm_bs_client_get_fd              (LmBsClient   *client);
LmBsClientStatus lm_bs_client_get_status          (LmBsClient   *client);
void             lm_bs_client_set_data_read_cb    (LmBsClient   *client,
						   LmCallback   *data_read_cb);
void             lm_bs_client_set_data_written_cb (LmBsClient   *client,
						   LmCallback   *data_written_cb);
void             lm_bs_client_set_disconnected_cb (LmBsClient   *client,
						   LmCallback   *disconnected_cb);
void             lm_bs_client_write_data          (LmBsClient   *client,
						   GString      *data);
void             lm_bs_client_do_read             (LmBsClient   *client);
void             lm_bs_client_remove_watch        (LmBsClient   *client);


G_END_DECLS

#endif /* __LM_BS_CLIENT_H__ */
