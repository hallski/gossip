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

#ifndef __LM_BS_LISTENER_H__
#define __LM_BS_LISTENER_H__

#if !defined (LM_INSIDE_LOUDMOUTH_H) && !defined (LM_COMPILATION)
#error "Only <loudmouth/loudmouth.h> can be included directly, this file may disappear or change contents."
#endif

G_BEGIN_DECLS

#define LM_BS_LISTENER(obj) (LmBsListener *) obj;

typedef struct  _LmBsListener LmBsListener;

typedef void    (* LmBsNewClientFunction)            (guint                  fd,
						      gpointer               user_data);

LmBsListener *lm_bs_listener_new                     (void);
LmBsListener *lm_bs_listener_new_with_context        (GMainContext          *context);
void          lm_bs_listener_set_new_client_function (LmBsListener          *listener,
						      LmBsNewClientFunction  func,
						      gpointer               user_data,
						      GDestroyNotify         notify);
guint         lm_bs_listener_start                   (LmBsListener          *listener);
void          lm_bs_listener_stop                    (LmBsListener          *listener);
guint         lm_bs_listener_get_port                (LmBsListener          *listener);
LmBsListener *lm_bs_listener_ref                     (LmBsListener          *listener);
void          lm_bs_listener_unref                   (LmBsListener          *listener);

G_END_DECLS

#endif /* __LM_BS_LISTENER_H__ */
