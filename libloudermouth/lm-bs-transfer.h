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
#include "lm-internals.h"
#include "lm-bs-session.h"

G_BEGIN_DECLS

#define LM_BS_TRANSFER(obj)  (LmBsTransfer *) obj;

typedef struct _LmBsTransfer LmBsTransfer;

typedef enum {
	LM_BS_TRANSFER_STATUS_INITIAL,
	LM_BS_TRANSFER_STATUS_CONNECTED,
	LM_BS_TRANSFER_STATUS_TRANSFERING,
	LM_BS_TRANSFER_STATUS_COMPLETED,
	LM_BS_TRANSFER_STATUS_INTERRUPTED
} LmBsTransferStatus;

typedef enum {
	LM_BS_TRANSFER_TYPE_SENDER,
	LM_BS_TRANSFER_TYPE_RECEIVER
} LmBsTransferType;

LmBsTransfer *     lm_bs_transfer_new                (LmBsSession       *session,
						      LmConnection      *connection,
						      LmBsTransferType   type,
						      guint              id,
						      const gchar       *sid,
						      const gchar       *peer_jid,
						      const gchar       *location,
						      gint64             file_size);
LmBsTransfer *     lm_bs_transfer_ref                (LmBsTransfer      *transfer);
void               lm_bs_transfer_unref              (LmBsTransfer      *transfer);
void               lm_bs_transfer_set_iq_id          (LmBsTransfer      *transfer,
						      const gchar       *iq_id);
void               lm_bs_transfer_add_streamhost     (LmBsTransfer      *transfer,
						      const gchar       *host,
						      guint64            port,
						      const gchar       *jid);
gboolean           lm_bs_transfer_has_streamhost     (LmBsTransfer      *transfer,
						      const gchar       *jid);
const gchar *      lm_bs_transfer_get_sid            (LmBsTransfer      *transfer);
gchar *            lm_bs_transfer_get_auth_sha       (LmBsTransfer      *transfer);
LmBsTransferType   lm_bs_transfer_get_type           (LmBsTransfer      *transfer);
LmBsTransferStatus lm_bs_transfer_get_status         (LmBsTransfer      *transfer);
void               lm_bs_transfer_error              (LmBsTransfer      *transfer,
						      const gchar       *error_msg);
const gchar *      lm_bs_transfer_get_iq_id          (LmBsTransfer      *transfer);
void               lm_bs_transfer_set_activate_cb    (LmBsTransfer      *transfer,
						      LmCallback        *activate_cb);
void               lm_bs_transfer_send_success_reply (LmBsTransfer      *transfer,
						      const gchar       *jid);
void               lm_bs_transfer_activate           (LmBsTransfer      *transfer,
						      const gchar       *jid);
gboolean           lm_bs_transfer_append_to_file     (LmBsTransfer      *transfer,
						      GString           *data);
gboolean           lm_bs_transfer_get_file_content   (LmBsTransfer      *transfer,
						      GString          **data);
void               lm_bs_transfer_close_file         (LmBsTransfer      *transfer);

G_END_DECLS

#endif /* __LM_BS_TRANSFER_H__  */
