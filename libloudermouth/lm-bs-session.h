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

#ifndef __LM_BS_SESSION_H__
#define __LM_BS_SESSION_H__

#if !defined (LM_INSIDE_LOUDMOUTH_H) && !defined (LM_COMPILATION)
#error "Only <loudmouth/loudmouth.h> can be included directly, this file may disappear or change contents."
#endif

#include <loudmouth/lm-connection.h>

G_BEGIN_DECLS

#define LM_BS_SESSION(obj) (LmBsSession *) obj;

typedef struct _LmBsSession LmBsSession;

typedef void    (* LmBsFailureFunction)             (gpointer       user_data,
						     guint          id,
						     const gchar   *failure_message);
typedef void    (* LmBsCompleteFunction)            (gpointer       user_data,
						     guint          id);

LmBsSession *   lm_bs_session_new                   (GMainContext  *context);
LmBsSession *   lm_bs_session_get_default           (GMainContext  *context);
LmBsSession *   lm_bs_session_ref                   (LmBsSession   *session);
void            lm_bs_session_unref                 (LmBsSession   *session);
void            lm_bs_session_set_failure_function  (LmBsSession   *session,
						     LmBsFailureFunction  function,
						     gpointer       user_data,
						     GDestroyNotify notify);
void            lm_bs_session_set_complete_function (LmBsSession   *session,
						     LmBsCompleteFunction function,
						     gpointer       user_data,
						     GDestroyNotify notify);
void            lm_bs_session_add_streamhost        (LmBsSession   *session,
						     guint          id,
						     const gchar   *host,
						     const gchar   *port,
						     const gchar   *jid);
void            lm_bs_session_receive_file          (LmBsSession   *session, 
						     LmConnection  *connection,
						     guint          id,
						     const gchar   *sid,
						     const gchar   *sender,
						     const gchar   *location,
						     guint64        file_size);
void            lm_bs_session_send_file             (LmBsSession   *session, 
						     LmConnection  *connection,
						     guint          id,
						     const gchar   *sid,
						     const gchar   *receiver,
						     const gchar   *location,
						     guint64        file_size);
void            lm_bs_session_remove_transfer       (LmBsSession   *session,
						     guint          fd);
void            lm_bs_session_activate_streamhost   (LmBsSession   *session,
						     const gchar   *iq_id,
						     const gchar   *jid);
void            lm_bs_session_set_iq_id             (LmBsSession   *session,
						     guint          id,
						     const gchar   *iq_id);
guint           lm_bs_session_start_listener        (LmBsSession   *session);
GMainContext * _lm_bs_session_get_context           (LmBsSession   *session);
void           _lm_bs_session_transfer_error        (LmBsSession   *session,
						     guint          id,
						     const gchar   *msg);
void           _lm_bs_session_transfer_completed    (LmBsSession   *session,
						     guint          id);
void           _lm_bs_session_remove_sender         (LmBsSession   *session,
						     guint          fd);
void           _lm_bs_session_match_sha             (LmBsSession   *session,
						     const gchar   *sha,
						     guint          fd);

G_END_DECLS

#endif /* __LM_BS_SESSION_H__ */
