/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2004 Imendio AB
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

#include <ctype.h>
#include <string.h>

#include <libgossip/gossip-utils.h>

#include "gossip-jid.h"

struct GossipJID {
	gchar *full;
	gchar *no_resource;
	const gchar *resource;

	guint  ref_count;
};

void         jid_free            (GossipJID   *jid);
const gchar *jid_locate_resource (const gchar *str);

void
jid_free (GossipJID *jid)
{
	g_free (jid->full);
	g_free (jid->no_resource);

	g_free (jid);
}

const gchar *
jid_locate_resource (const gchar *str)
{
	gchar *ch;

	ch = strchr (str, '/');
	if (ch) {
		return (const gchar *) (ch + 1);
	}

	return NULL;
}

/* Casefolds the node part (the part before @). */
static gchar *
jid_casefold_node (const gchar *str)
{
	gchar       *tmp;
	gchar       *ret;
	const gchar *at;

	at = strchr (str, '@');
	if (!at) {
		return g_strdup (str);
	}

	tmp = g_utf8_casefold (str, at - str);
	ret = g_strconcat (tmp, at, NULL);
	g_free (tmp);

	return ret;
}

GossipJID *
gossip_jid_new (const gchar *str_jid)
{
	GossipJID *jid;

	g_return_val_if_fail (str_jid != NULL, NULL);

	jid = g_new0 (GossipJID, 1);
	jid->ref_count = 1;
	jid->full = jid_casefold_node (str_jid);

	jid->resource = jid_locate_resource (jid->full);
	if (jid->resource) {
		jid->no_resource = g_strndup (jid->full, jid->resource - 1 - jid->full);
	} else {
		jid->no_resource = g_strdup (jid->full);
	}

	return jid;
}

void
gossip_jid_set_without_resource (GossipJID *jid, const gchar *str)
{
	gchar *resource = NULL;

	g_return_if_fail (jid != NULL);

	if (jid->resource) {
		resource = g_strdup (jid->resource);
	}

	g_free (jid->full);
	g_free (jid->no_resource);

	jid->no_resource = jid_casefold_node (str);

	if (resource) {
		jid->full = g_strdup_printf ("%s/%s",
					     jid->no_resource, resource);
		g_free (resource);
		jid->resource = jid_locate_resource (jid->full);
	} else {
		jid->full = g_strdup (jid->no_resource);
	}
}

void
gossip_jid_set_resource (GossipJID *jid, const gchar *resource)
{
	g_return_if_fail (jid != NULL);

	g_free (jid->full);

	jid->full = g_strdup_printf ("%s/%s", jid->no_resource, resource);
	jid->resource = jid_locate_resource (jid->full);
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

gboolean
gossip_jid_is_service (GossipJID *jid)
{
	gchar *ch;

	/* this basically checks to see if there is an '@'
	   sign in the jid, if not, we assume it is a component
	   or service (for example msn.jabber.org.uk) */

	g_return_val_if_fail (jid != NULL, FALSE);

	ch = strchr (jid->full, '@');
	if (!ch) {
		return TRUE;
	} else {
		return FALSE;
	}
}

gchar *
gossip_jid_get_part_name (GossipJID *jid)
{
	gchar *ch;

	g_return_val_if_fail (jid != NULL, g_strdup (""));

	for (ch = jid->full; *ch; ++ch) {
		if (*ch == '@') {
			return g_strndup (jid->full, ch - jid->full);
		}
	}

	return g_strdup ("");
}

const gchar *
gossip_jid_get_part_host (GossipJID *jid)
{
	const gchar *ch;

	g_return_val_if_fail (jid != NULL, "");

	for (ch = gossip_jid_get_without_resource (jid); *ch; ++ch) {
		if (*ch == '@') {
			return ch + 1;
		}
	}

	return "";
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
gossip_jid_equals (GossipJID *jid_a,
		   GossipJID *jid_b)
{
	g_return_val_if_fail (jid_a != NULL, FALSE);
	g_return_val_if_fail (jid_b != NULL, FALSE);

	/* NOTE: This is not strictly correct, since the node and resource are
	 * UTF8, and the domain have other rules. The node is also already
	 * casefolded.
	 */
	if (g_ascii_strcasecmp (jid_a->full, jid_b->full) == 0) {
		return TRUE;
	}

	return FALSE;
}

gboolean
gossip_jid_equals_without_resource (GossipJID *jid_a,
				    GossipJID *jid_b)
{
	const gchar *a, *b;

	g_return_val_if_fail (jid_a != NULL, FALSE);
	g_return_val_if_fail (jid_b != NULL, FALSE);

	a = gossip_jid_get_without_resource (jid_a);
	b = gossip_jid_get_without_resource (jid_b);

	/* NOTE: This is not strictly correct, since the node and resource are
	 * UTF8, and the domain have other rules. The node is also already
	 * casefolded.
	 */
	if (g_ascii_strcasecmp (a, b) == 0) {
		return TRUE;
	}

	return FALSE;
}

gboolean
gossip_jid_string_is_valid (const gchar *str,
			    gboolean     with_resource)
{
	const gchar *at;
	const gchar *dot;
	const gchar *slash;
	gint         len;

	if (!str || strlen (str) < 1) {
		return FALSE;
	}

	len = strlen (str);

	/* check for the '@' sign and make sure it isn't at the start
	   of the string or the last character */
	at = strchr (str, '@');
	if (!at ||
	    at == str ||
	    at == str + len - 1) {
		return FALSE;
	}

	/* check for the '.' character and if it exists make sure it
	   is not directly after the '@' sign or the last character. */
	dot = strchr (at, '.');
	if (dot == at + 1 ||
	    dot == str + len - 1 ||
	    dot == str + len - 2) {
		return FALSE;
	}

	/* check the '/' character exists (if we are checking with
	   resource) and make sure it is not after the '@' sign or the
	   last character */
	slash = strchr (at, '/');
	if (with_resource &&
	    (slash == NULL ||
	     slash == at + 1 ||
	     slash == str + len - 1)) {
		return FALSE;
	}

	/* if slash exists and we are expecting a JID without the
	   resource then we return FALSE */
	if (!with_resource && slash) {
		return FALSE;
	}

	return TRUE;
}

gchar *
gossip_jid_string_get_part_name (const gchar *str)
{
	const gchar *ch;

	g_return_val_if_fail (str != NULL, "");

	for (ch = str; *ch; ++ch) {
		if (*ch == '@') {
			return g_strndup (str, ch - str);
		}
	}

	return g_strdup ("");
}

gchar *
gossip_jid_string_get_part_host (const gchar *str)
{
	const gchar *r_loc;
	const gchar *ch;

	g_return_val_if_fail (str != NULL, "");

	r_loc = gossip_jid_string_get_part_resource (str);
	for (ch = str; *ch; ++ch) {
		if (*ch == '@') {
			ch++;

			if (r_loc) {
				return g_strndup (ch, r_loc - 1 - ch);
			}

			return g_strdup (ch);
		}
	}

	return g_strdup ("");
}

const gchar *
gossip_jid_string_get_part_resource (const gchar *str)
{
	gchar *ch;

	ch = strchr (str, '/');
	if (ch) {
		return (const gchar *) (ch + 1);
	}

	return NULL;
}

gint
gossip_jid_case_compare (gconstpointer a,
			 gconstpointer b)
{
	const gchar *str_a, *str_b;

	str_a = gossip_jid_get_without_resource ((GossipJID *) a);
	str_b = gossip_jid_get_without_resource ((GossipJID *) b);

	return gossip_strncasecmp (str_a, str_b, -1);
}

gboolean
gossip_jid_equal (gconstpointer v1,
		  gconstpointer v2)
{
	const gchar *a, *b;

	g_return_val_if_fail (v1 != NULL, FALSE);
	g_return_val_if_fail (v2 != NULL, FALSE);

	a = gossip_jid_get_without_resource ((GossipJID *) v1);
	b = gossip_jid_get_without_resource ((GossipJID *) v2);

	/* NOTE: This is not strictly correct, since the node and resource are
	 * UTF8, and the domain have other rules. The node is also already
	 * casefolded.
	 */
	return g_ascii_strcasecmp (a, b) == 0;
}

guint
gossip_jid_hash (gconstpointer key)
{
	GossipJID *jid = (GossipJID *) key;
	gchar     *lower;
	guint      ret_val;

	/* NOTE: This is not strictly correct, since the node and resource are
	 * UTF8, and the domain have other rules. The node is also already
	 * casefolded.
	 */
	lower = g_ascii_strdown (gossip_jid_get_without_resource (jid), -1);
	ret_val = g_str_hash (lower);
	g_free (lower);

	return ret_val;
}
