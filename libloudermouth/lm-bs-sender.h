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

#ifndef __LM_BS_SENDER_H__
#define __LM_BS_SENDER_H__

#if !defined (LM_INSIDE_LOUDMOUTH_H) && !defined (LM_COMPILATION)
#error "Only <loudmouth/loudmouth.h> can be included directly, this file may disappear or change contents."
#endif


G_BEGIN_DECLS

#define LM_BS_SENDER(obj) (LmBsSender *) obj;

typedef struct _LmBsSender LmBsSender;

LmBsSender * lm_bs_sender_new          (LmBsClient *client,
                                        LmBsSession *session);
LmBsSender * lm_bs_sender_ref          (LmBsSender *sender);
void         lm_bs_sender_unref        (LmBsSender *sender);
void         lm_bs_sender_set_transfer (LmBsSender *sender,
                                        LmBsTransfer *transfer);

G_END_DECLS

#endif /* __LM_BS_SENDER_H__  */

