/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003  Kevin Dougherty <gossip@kdough.net>
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

#include <time.h>
#include <config.h>
#include "gossip-idle.h"

#ifdef USE_SCREENSAVER
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/scrnsaver.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#endif

gint
gossip_idle_get_seconds (void)
{
	gint idle_secs = 0;
  static time_t timestamp = 0;
	
#ifdef USE_SCREENSAVER
	static gboolean          inited = FALSE;
	static XScreenSaverInfo *ss_info = NULL;
	gint                     event_base;
	gint                     error_base;

	if (!inited) {
    timestamp = time(NULL);
		if (XScreenSaverQueryExtension (GDK_DISPLAY (), &event_base, &error_base)) {
			ss_info = XScreenSaverAllocInfo ();
		}
		
		inited = TRUE;
	}

	if (ss_info) {
		XScreenSaverQueryInfo (GDK_DISPLAY (), DefaultRootWindow (GDK_DISPLAY ()), ss_info);
		idle_secs = ss_info->idle / 1000;
	}
#endif
  /* when idle time is below 3 seconds, we're not really idle */
  if (idle_secs < 3) {
    return timestamp - time(NULL);
  }

  /* update timestamp */
  timestamp = time(NULL);
	return idle_secs;
}
