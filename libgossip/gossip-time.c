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
#include <stdio.h>
#include <time.h>

#include "gossip-time.h"

/* Note: gossip_time_t is always in UTC. */

gossip_time_t
gossip_time_get_current (void)
{
	return time (NULL);
}

/* The format is: "20021209T23:51:30" and is in UTC. 0 is returned on
 * failure. The alternative format "20021209" is also accepted.
 */
gossip_time_t
gossip_time_parse (const gchar *str)
{
	struct tm tm;
	gint      year, month;
	gint      n_parsed;
	
	memset (&tm, 0, sizeof (struct tm));

	n_parsed = sscanf (str, "%4d%2d%2dT%2d:%2d:%2d",
		    &year, &month, &tm.tm_mday, &tm.tm_hour,
			   &tm.tm_min, &tm.tm_sec);
	if (n_parsed != 3 && n_parsed != 6) {
		return 0;
	}
	
	tm.tm_year = year - 1900;
	tm.tm_mon = month - 1;
	tm.tm_isdst = -1;

 	return timegm (&tm);
}

/* Converts the UTC timestamp to a string, also in UTC. Returns NULL on failure. */
gchar *
gossip_time_to_string_utc (gossip_time_t  t,
			   const gchar   *format)
{
	gchar      stamp[128];
	struct tm *tm;

	g_return_val_if_fail (format != NULL, NULL);
	
	tm = gmtime (&t);
	if (strftime (stamp, sizeof (stamp), format, tm) == 0) {
		return NULL;
	}
	
	return g_strdup (stamp);
}

/* Converts the UTC timestamp to a string, in local time. Returns NULL on failure. */
gchar *
gossip_time_to_string_local (gossip_time_t  t,
			     const gchar   *format)
{
	gchar      stamp[128];
	struct tm *tm;

	g_return_val_if_fail (format != NULL, NULL);
	
	tm = localtime (&t);
	if (strftime (stamp, sizeof (stamp), format, tm) == 0) {
		return NULL;
	}
	
	return g_strdup (stamp);
}

