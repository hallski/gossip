/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2006 Imendio AB
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

#include <config.h>

#include <string.h>
#include <stdlib.h>

#include "gossip-time.h"

static gchar *time_tz_orig = NULL;

static void
time_set_tz_utc (void)
{
        const gchar *tmp;

        if (time_tz_orig == NULL) {
                tmp = g_getenv ("TZ");

                if (tmp != NULL) {
                        time_tz_orig = g_strconcat ("TZ=", tmp, NULL);
                } else {
                        time_tz_orig = g_strdup ("TZ");
                }
        }

        putenv ("TZ=UTC");
}

static void
time_reset_tz (void)
{
        if (time_tz_orig != NULL) {
                putenv (time_tz_orig);
        }
}

gossip_time_t
gossip_time_from_tm (struct tm *tm)
{
	gossip_time_t t;

	time_set_tz_utc ();
	t = mktime (tm);
        time_reset_tz ();

	return t;
}

struct tm *
gossip_time_to_tm (gossip_time_t t)
{
        time_t tt;

        tt = t;

        return gmtime (&tt);
}

gossip_time_t
gossip_time_get_current (void)
{
	time_t     t;
	struct tm *tm;
	
	t  = time (NULL);
	tm = localtime (&t);

	return gossip_time_from_tm (tm);
}

gchar *
gossip_time_to_timestamp (gossip_time_t t)
{
	gchar      stamp[128];
	struct tm *tm;

	if (t <= 0) {
		t = gossip_time_get_current ();
	}

	tm = gossip_time_to_tm (t);
	strftime (stamp, sizeof (stamp), "%H:%M", tm);

	return g_strdup (stamp);
}

gchar *
gossip_time_to_timestamp_full (gossip_time_t  t,
			       const gchar   *format)
{
	gchar      stamp[128];
	struct tm *tm;
	
	if (t <= 0) {
		t = gossip_time_get_current ();
	}

	tm = gossip_time_to_tm (t);
	strftime (stamp, sizeof (stamp), format, tm);

	return g_strdup (stamp);
}

gossip_time_t 
gossip_time_from_string_full (const gchar *time,
			      const gchar *format)
{
	struct tm tm;
	
	memset (&tm, 0, sizeof (struct tm));

	strptime (time, format, &tm);
	return gossip_time_from_tm (&tm);
}
