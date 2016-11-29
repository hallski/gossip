/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef __LM_BS_PRIVATE_H__
#define __LM_BS_PRIVATE_H__

#if !defined (LM_INSIDE_LOUDMOUTH_H) && !defined (LM_COMPILATION)
#error "Only <loudmouth/loudmouth.h> can be included directly, this file may disappear or change contents."
#endif

G_BEGIN_DECLS

GMainContext * _lm_bs_session_get_context           (LmBsSession   *session);
void           _lm_bs_session_remove_sender         (LmBsSession   *session,
                                                     guint          fd);
void           _lm_bs_session_match_sha             (LmBsSession   *session,
                                                     const gchar   *sha,
                                                     guint          fd);
G_END_DECLS

#endif /* __LM_BS_PRIVATE_H__ */
