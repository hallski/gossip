/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio HB
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

#include <string.h>
#include "gossip-jid.h"

struct GossipJID {
	gchar *full;
	gchar *no_resource;
	gchar *resource;

	guint  ref_count;
};

void   jid_free   (GossipJID *jid);

void
jid_free (GossipJID *jid)
{
	g_free (jid->full);
	g_free (jid->no_resource);

	/* jid->resource is a pointer into jid->full */
	
	g_free (jid);
}

GossipJID *
gossip_jid_new (const gchar *str_jid)
{
	GossipJID *jid;
	gchar     *ch;
	
	g_return_val_if_fail (str_jid != NULL, NULL);

	jid = g_new0 (GossipJID, 1);
	jid->ref_count = 1;
	jid->full = g_strdup (str_jid);
	
	ch = strchr (jid->full, '/');
	if (ch) {
		jid->resource = ch + 1;
		jid->no_resource = g_strndup (jid->full, ch - jid->full);
	} else {
		jid->resource = NULL;
		jid->no_resource = NULL;
	}

	return jid;
}

const gchar *
gossip_jid_get_full (GossipJID *jid)
{
	g_return_val_if_fail (jid != NULL, "");
	
	return jid->full;
}

const gchar * 
gossip_jid_get_without_resource (GossipJID *jid)
{
	g_return_val_if_fail (jid != NULL, "");

	if (jid->no_resource) {
		return jid->no_resource;
	}

	return jid->full;
}

const gchar *
gossip_jid_get_resource (GossipJID *jid)
{
	g_return_val_if_fail (jid != NULL, NULL);

	if (jid->resource) {
		return jid->resource;
	}

	return NULL;
}

gchar *
gossip_jid_get_part_name (GossipJID *jid)
{
	gchar *ch;

	g_return_val_if_fail (jid != NULL, "");
	
	for (ch = jid->full; *ch; ++ch) {
		if (*ch == '@') {
			return g_strndup (jid->full, ch - jid->full);
		}
	}

	return g_strdup (""); 
}

GossipJID *
gossip_jid_ref (GossipJID *jid)
{
	g_return_val_if_fail (jid != NULL, NULL);

	jid->ref_count++;
	
	return jid;
}
	
void
gossip_jid_unref (GossipJID *jid)
{
	g_return_if_fail (jid != NULL);

	jid->ref_count--;
	if (jid->ref_count <= 0) {
		jid_free (jid);
	}
}

gboolean
gossip_jid_equals (GossipJID *jid_a, GossipJID *jid_b)
{
	g_return_val_if_fail (jid_a != NULL, FALSE);
	g_return_val_if_fail (jid_b != NULL, FALSE);
	
	if (g_ascii_strcasecmp (jid_a->full, jid_b->full) == 0) {
		return TRUE;
	}

	return FALSE;
}

gboolean
gossip_jid_equals_without_resource (GossipJID *jid_a, GossipJID *jid_b)
{
	const gchar *a, *b;
	
	g_return_val_if_fail (jid_a != NULL, FALSE);
	g_return_val_if_fail (jid_b != NULL, FALSE);

	a = gossip_jid_get_without_resource (jid_a);
	b = gossip_jid_get_without_resource (jid_b);
	
	if (g_ascii_strcasecmp (a, b) == 0) {
		return TRUE;
	}

	return FALSE;
}

gboolean
gossip_jid_string_is_valid_jid (const gchar *str_jid)
{
	const gchar *at;
	const gchar *dot;
	gint         jid_len;
	
	if (!str_jid || strcmp (str_jid, "") == 0) {
		return FALSE;
	}

	jid_len = strlen (str_jid);

	at = strchr (str_jid, '@');
	if (!at || at == str_jid || at == str_jid + jid_len - 1) {
		return FALSE;
	}
	
	dot = strchr (at, '.');
	if (dot == at + 1 
	    || dot == str_jid + jid_len - 1 
	    || dot == str_jid + jid_len - 2) {
		return FALSE;
	}

	dot = strrchr (str_jid, '.');
	if (dot == str_jid + jid_len - 1 ||
	    dot == str_jid + jid_len - 2) {
		return FALSE;
	}

	return TRUE;
}

gboolean
gossip_jid_equal (gconstpointer v1, gconstpointer v2)
{
	const gchar *a, *b;

	g_return_val_if_fail (v1 != NULL, FALSE);
	g_return_val_if_fail (v2 != NULL, FALSE);

	a = gossip_jid_get_without_resource ((GossipJID *) v1);
	b = gossip_jid_get_without_resource ((GossipJID *) v2);

	return g_ascii_strcasecmp (a, b) == 0;
}

guint
gossip_jid_hash (gconstpointer key)
{
	GossipJID *jid = (GossipJID *) key;
	gchar     *lower;
	guint      ret_val;

	lower = g_ascii_strdown (gossip_jid_get_without_resource (jid), -1);
	ret_val = g_str_hash (lower);
	g_free (lower);

	return ret_val;
}

