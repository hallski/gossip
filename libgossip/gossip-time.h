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

#ifndef __GOSSIP_TIME_H__
#define __GOSSIP_TIME_H__

#define __USE_XOPEN
#include <time.h>

#include <glib.h>

typedef long gossip_time_t;

gossip_time_t gossip_time_from_tm           (struct tm     *tm);
struct tm *   gossip_time_to_tm             (gossip_time_t  t);
gossip_time_t gossip_time_get_current       (void);
gchar *       gossip_time_to_timestamp      (gossip_time_t  t);
gchar *       gossip_time_to_timestamp_full (gossip_time_t  t,
					     const gchar   *format);
gossip_time_t gossip_time_from_string_full  (const gchar   *time,
					     const gchar   *format);

#endif /* __GOSSIP_TIME_H__ */

