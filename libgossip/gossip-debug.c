/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
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
#include <stdarg.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>

#include "gossip-debug.h"

static gchar **debug_strv;

static void
debug_init (void)
{
	static gboolean inited = FALSE;

	if (!inited) {
		const gchar *env;

		env = g_getenv ("GOSSIP_DEBUG");

		if (env) {
			debug_strv = g_strsplit (env, ":", 0);
		} else {
			debug_strv = NULL;
		}

		inited = TRUE;
	}
}

void
gossip_debug_impl (const gchar *domain, const gchar *msg, ...)
{
	gint i;

	debug_init ();

	if (debug_strv) {
		for (i = 0; debug_strv[i]; i++) {
			if (strcmp (domain, debug_strv[i]) == 0) {
				va_list args;

				g_print ("%s: ", domain);

				va_start (args, msg);
				g_vprintf (msg, args);
				va_end (args);

				g_print ("\n");
				break;
			}
		}
	}
}

