/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Imendio AB
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

#ifndef __GOSSIP_JABBER_FT_UTILS_H__
#define __GOSSIP_JABBER_FT_UTILS_H__

#include <glib.h>


size_t  gossip_jabber_ft_base64_encode_close  (guint8 const *in,
					       size_t        inlen,
					       gboolean      break_lines,
					       guint8       *out,
					       int          *state,
					       unsigned int *save);
size_t  gossip_jabber_ft_base64_encode_step   (guint8 const *in,
					       size_t        len,
					       gboolean      break_lines,
					       guint8       *out,
					       int          *state,
					       unsigned int *save);
size_t  gossip_jabber_ft_base64_decode_step   (guint8 const *in,
					       size_t        len,
					       guint8       *out,
					       int          *state,
					       guint        *save);
guint8 *gossip_jabber_ft_base64_encode_simple (guint8 const *data,
					       size_t        len);
size_t  gossip_jabber_ft_base64_decode_simple (guint8       *data,
					       size_t        len);


#endif /* __GOSSIP_JABBER_FT_UTILS_H__ */
