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

#ifndef __LM_BS_TRANSFER_H__
#define __LM_BS_TRANSFER_H__

#if !defined (LM_INSIDE_LOUDMOUTH_H) && !defined (LM_COMPILATION)
#error "Only <loudmouth/loudmouth.h> can be included directly, this file may disappear or change contents."
#endif

G_BEGIN_DECLS

#include <glib-object.h>

#include "lm-internals.h"

#define LM_TYPE_BS_TRANSFER         (lm_bs_transfer_get_type ())
#define LM_BS_TRANSFER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), LM_TYPE_BS_TRANSFER, LmBsTransfer))
#define LM_BS_TRANSFER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), LM_TYPE_BS_TRANSFER, LmBsTransferClass))
#define LM_IS_BS_TRANSFER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), LM_TYPE_BS_TRANSFER))
#define LM_IS_BS_TRANSFER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), LM_TYPE_BS_TRANSFER))
#define LM_BS_TRANSFER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), LM_TYPE_BS_TRANSFER, LmBsTransferClass))

typedef struct _LmBsTransfer      LmBsTransfer;
typedef struct _LmBsTransferClass LmBsTransferClass;

struct _LmBsTransfer {
	GObject parent;
};

struct _LmBsTransferClass {
	GObjectClass parent_class;
};

#include "lm-bs-session.h"

typedef enum {
	LM_BS_TRANSFER_DIRECTION_SENDER,
	LM_BS_TRANSFER_DIRECTION_RECEIVER
} LmBsTransferDirection;

typedef enum {
	LM_BS_TRANSFER_STATUS_INITIAL,
	LM_BS_TRANSFER_STATUS_CONNECTED,
	LM_BS_TRANSFER_STATUS_TRANSFERING,
	LM_BS_TRANSFER_STATUS_COMPLETE,
	LM_BS_TRANSFER_STATUS_INTERRUPTED
} LmBsTransferStatus;

typedef enum {
	LM_BS_TRANSFER_ERROR_CLIENT_DISCONNECTED,
	LM_BS_TRANSFER_ERROR_PROTOCOL_SPECIFIC,
	LM_BS_TRANSFER_ERROR_UNABLE_TO_CONNECT
} LmBsTransferError;

GType              lm_bs_transfer_get_type              (void) G_GNUC_CONST;

LmBsTransfer *     lm_bs_transfer_new                   (LmBsSession       *session,
							 LmConnection      *connection,
							 LmBsTransferDirection   direction,
							 guint              id,
							 const gchar       *sid,
							 const gchar       *peer_jid,
							 const gchar       *location,
							 gint64             file_size);
void               lm_bs_transfer_set_iq_id             (LmBsTransfer      *transfer,
							 const gchar       *iq_id);
void               lm_bs_transfer_add_streamhost        (LmBsTransfer      *transfer,
							 const gchar       *host,
							 guint64            port,
							 const gchar       *jid);
gboolean           lm_bs_transfer_has_streamhost        (LmBsTransfer      *transfer,
							 const gchar       *jid);
guint              lm_bs_transfer_get_id                (LmBsTransfer      *transfer);
const gchar *      lm_bs_transfer_get_sid               (LmBsTransfer      *transfer);
gchar *            lm_bs_transfer_get_auth_sha          (LmBsTransfer      *transfer);
LmBsTransferDirection   lm_bs_transfer_get_direction         (LmBsTransfer      *transfer);
LmBsTransferStatus lm_bs_transfer_get_status            (LmBsTransfer      *transfer);
guint64            lm_bs_transfer_get_bytes_transferred (LmBsTransfer      *transfer);
guint64            lm_bs_transfer_get_bytes_total       (LmBsTransfer      *transfer);
void               lm_bs_transfer_error                 (LmBsTransfer      *transfer,
							 GError            *error);
const gchar *      lm_bs_transfer_get_iq_id             (LmBsTransfer      *transfer);
void               lm_bs_transfer_set_activate_cb       (LmBsTransfer      *transfer,
							 LmCallback        *activate_cb);
void               lm_bs_transfer_send_success_reply    (LmBsTransfer      *transfer,
							 const gchar       *jid);
void               lm_bs_transfer_activate              (LmBsTransfer      *transfer,
							 const gchar       *jid);
gboolean           lm_bs_transfer_append_to_file        (LmBsTransfer      *transfer,
							 GString           *data);
gboolean           lm_bs_transfer_get_file_content      (LmBsTransfer      *transfer,
							 GString          **data);
void               lm_bs_transfer_close_file            (LmBsTransfer      *transfer);

G_END_DECLS

#endif /* __LM_BS_TRANSFER_H__  */
