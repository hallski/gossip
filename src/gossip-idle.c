/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Kevin Dougherty <gossip@kdough.net>
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
#include <time.h>
#include <stdlib.h>

#ifdef HAVE_XSS
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/scrnsaver.h>
#include <gdk/gdkx.h>
#elif defined(HAVE_COCOA)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <IOKit/IOKitLib.h>
#endif

#include <gdk/gdk.h>
#include "gossip-idle.h"

static time_t timestamp = 0;;

gint32
gossip_idle_get_seconds (void)
{
	static gboolean          inited = FALSE;
#ifdef HAVE_XSS
	static XScreenSaverInfo *ss_info = NULL;
	gint                     event_base;
	gint                     error_base;
	gint32                   idle = 0;

	if (!inited) {
		timestamp = time (NULL);
		if (XScreenSaverQueryExtension (GDK_DISPLAY (), &event_base, &error_base)) {
			ss_info = XScreenSaverAllocInfo ();
		}

		inited = TRUE;
	}

	if (ss_info) {
		XScreenSaverQueryInfo (GDK_DISPLAY (), DefaultRootWindow (GDK_DISPLAY ()), ss_info);
		idle = ss_info->idle / 1000;
	}

	/* When idle time is low enough, we're not really idle. */
	if (idle < 3) {
		return timestamp - time (NULL);
	}

	timestamp = time (NULL);

	return idle;

#elif defined(HAVE_COCOA)
	static mach_port_t         port;
	static io_registry_entry_t object;
	CFMutableDictionaryRef     properties;
	CFTypeRef                  idle_object;
	uint64_t                   idle_time;

	if (!inited) {
		io_iterator_t iter;

		timestamp = time (NULL);

		inited = TRUE;

		IOMasterPort (MACH_PORT_NULL, &port);
		IOServiceGetMatchingServices (port,
					      IOServiceMatching ("IOHIDSystem"),
					      &iter);
		if (iter == 0) {
			g_warning ("Couldn't access IOHIDSystem\n");
			return 0;
		}

		object = IOIteratorNext (iter);
	}

	if (!object) {
		return 5;
	}

	idle_time = 5;
	properties = 0;
	if (IORegistryEntryCreateCFProperties (object, &properties, kCFAllocatorDefault, 0) ==
	    KERN_SUCCESS && properties != NULL) {
		CFTypeID type;

		idle_object = CFDictionaryGetValue (properties, CFSTR ("HIDIdleTime"));

		type = CFGetTypeID (idle_object);
		if (type == CFNumberGetTypeID ()) {
			CFNumberGetValue ((CFNumberRef) idle_object,
					  kCFNumberSInt64Type,
					  &idle_time);
			idle_time >>= 30;
		} else {
			idle_time = 5;
		}
	}

	CFRelease ((CFTypeRef) properties);

	/* When idle time is low enough, we're not really idle. */
	if (idle_time < 3) {
		return timestamp - time (NULL);
	}

	timestamp = time (NULL);

	return idle_time;
#else
	return 5;
#endif
}

void
gossip_idle_reset (void)
{
	timestamp = time (NULL);
}

