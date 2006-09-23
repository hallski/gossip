/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2006 Imendio AB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
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
#include <time.h>
#include <sys/types.h>
#include <regex.h>

#include <glib/gi18n.h>
#include <libxml/uri.h>

#include "gossip-utils.h"
#include "gossip-conf.h"
#include "gossip-paths.h"
#include "gossip-debug.h"

#define DEBUG_DOMAIN "Utils"

static void regex_init (void);

gchar *
gossip_substring (const gchar *str, 
		  gint         start, 
		  gint         end)
{
	return g_strndup (str + start, end - start);
}

/*
 * Regular Expression code to match urls.
 */
#define USERCHARS "-A-Za-z0-9"
#define PASSCHARS "-A-Za-z0-9,?;.:/!%$^*&~\"#'"
#define HOSTCHARS "-A-Za-z0-9"
#define PATHCHARS "-A-Za-z0-9_$.+!*(),;:@&=?/~#%"
#define SCHEME    "(news:|telnet:|nntp:|file:/|https?:|ftps?:|webcal:)"
#define USER      "[" USERCHARS "]+(:["PASSCHARS "]+)?"
#define URLPATH   "/[" PATHCHARS "]*[^]'.}>) \t\r\n,\\\"]"

static regex_t dingus[GOSSIP_REGEX_ALL];

static void
regex_init (void)
{
	static gboolean  inited = FALSE;
	const gchar     *expression;
	gint             i;
	
	if (inited) {
		return;
	}

	for (i = 0; i < GOSSIP_REGEX_ALL; i++) {
		switch (i) {
		case GOSSIP_REGEX_AS_IS:
			expression = 
				SCHEME "//(" USER "@)?[" HOSTCHARS ".]+"
				"(:[0-9]+)?(" URLPATH ")?";
			break;
		case GOSSIP_REGEX_BROWSER:
			expression = 
				"(www|ftp)[" HOSTCHARS "]*\\.[" HOSTCHARS ".]+"
				"(:[0-9]+)?(" URLPATH ")?";
			break;
		case GOSSIP_REGEX_EMAIL:
			expression = 
				"(mailto:)?[a-z0-9][a-z0-9.-]*@[a-z0-9]"
				"[a-z0-9-]*(\\.[a-z0-9][a-z0-9-]*)+";
			break;
		case GOSSIP_REGEX_OTHER:
			expression = 
				"news:[-A-Z\\^_a-z{|}~!\"#$%&'()*+,./0-9;:=?`]+"
				"@[" HOSTCHARS ".]+(:[0-9]+)?";
			break;
		default:
			/* Silence the compiler. */
			expression = NULL;
			continue;
		}
		
		memset (&dingus[i], 0, sizeof (regex_t));
		regcomp (&dingus[i], expression, REG_EXTENDED | REG_ICASE);
	}
	
	inited = TRUE;
}

gint
gossip_regex_match (GossipRegExType  type, 
		    const gchar     *msg,
		    GArray          *start,
		    GArray          *end)
{
	regmatch_t matches[1];
	gint       ret = 0;
	gint       num_matches = 0;
	gint       offset = 0;
	gint       i;

	g_return_val_if_fail (type >= 0 || type <= GOSSIP_REGEX_ALL, 0);

	regex_init ();

	while (!ret && type != GOSSIP_REGEX_ALL) {
		ret = regexec (&dingus[type], msg + offset, 1, matches, 0);
		if (ret == 0) {
			gint s;
			
			num_matches++;
			
			s = matches[0].rm_so + offset;
			offset = matches[0].rm_eo + offset;
			
			g_array_append_val (start, s);
			g_array_append_val (end, offset);
		}
	}

 	if (type != GOSSIP_REGEX_ALL) { 
		gossip_debug (DEBUG_DOMAIN, 
			      "Found %d matches for regex type:%d", 
			      num_matches, type);
		return num_matches;
	}

	/* If GOSSIP_REGEX_ALL then we run ALL regex's on the string. */
	for (i = 0; i < GOSSIP_REGEX_ALL; i++, ret = 0) { 
		while (!ret) {
			ret = regexec (&dingus[i], msg + offset, 1, matches, 0);
			if (ret == 0) {
				gint s;
				
				num_matches++;
				
				s = matches[0].rm_so + offset;
				offset = matches[0].rm_eo + offset;
				
				g_array_append_val (start, s);
				g_array_append_val (end, offset);
			}
		}
	}

	gossip_debug (DEBUG_DOMAIN, 
		      "Found %d matches for ALL regex types", 
		      num_matches);

	return num_matches;
}

gint
gossip_strcasecmp (const gchar *s1, 
		   const gchar *s2)
{
	return gossip_strncasecmp (s1, s2, -1);
}	

gint
gossip_strncasecmp (const gchar *s1, 
		    const gchar *s2, 
		    gsize        n)
{
	gchar *u1, *u2;
	gint   ret_val;
	
	u1 = g_utf8_casefold (s1, n);
	u2 = g_utf8_casefold (s2, n);

	ret_val = g_utf8_collate (u1, u2);
	g_free (u1);
	g_free (u2);

	return ret_val;
}

gboolean 
gossip_xml_validate (xmlDoc      *doc, 
		     const gchar *dtd_filename)
{
	gchar        *path, *escaped;
	xmlValidCtxt  cvp;
	xmlDtd       *dtd;
	gboolean      ret;
		
	path = gossip_paths_get_dtd_path (dtd_filename);

	/* The list of valid chars is taken from libxml. */
	escaped = xmlURIEscapeStr (path, ":@&=+$,/?;");

	g_free (path);
	
	memset (&cvp, 0, sizeof (cvp));
	dtd = xmlParseDTD (NULL, escaped);
	ret = xmlValidateDtd (&cvp, doc, dtd);
	
	xmlFree (escaped);
	xmlFreeDtd (dtd);
	
        return ret;
}

guchar *
gossip_base64_decode (const char *str, 
		      gsize      *ret_len)
{
	guchar      *out = NULL;
	gchar        tmp = 0;
	const gchar *c;
	gint32       tmp2 = 0;
	gint         len = 0, n = 0;

	g_return_val_if_fail (str != NULL, NULL);

	c = str;

	while (*c) {
		if (*c >= 'A' && *c <= 'Z') {
			tmp = *c - 'A';
		} else if (*c >= 'a' && *c <= 'z') {
			tmp = 26 + (*c - 'a');
		} else if (*c >= '0' && *c <= 57) {
			tmp = 52 + (*c - '0');
		} else if (*c == '+') {
			tmp = 62;
		} else if (*c == '/') {
			tmp = 63;
		} else if (*c == '\r' || *c == '\n') {
			c++;
			continue;
		} else if (*c == '=') {
			if (n == 3) {
				out = g_realloc (out, len + 2);
				out[len] = (guchar)(tmp2 >> 10) & 0xff;
				len++;
				out[len] = (guchar)(tmp2 >> 2) & 0xff;
				len++;
			} else if (n == 2) {
				out = g_realloc (out, len + 1);
				out[len] = (guchar)(tmp2 >> 4) & 0xff;
				len++;
			}
			break;
		}

		tmp2 = ((tmp2 << 6) | (tmp & 0xff));
		n++;

		if (n == 4) {
			out = g_realloc (out, len + 3);
			out[len] = (guchar)((tmp2 >> 16) & 0xff);
			len++;
			out[len] = (guchar)((tmp2 >> 8) & 0xff);
			len++;
			out[len] = (guchar)(tmp2 & 0xff);
			len++;
			tmp2 = 0;
			n = 0;
		}

		c++;
	}

	out = g_realloc (out, len + 1);
	out[len] = 0;

	if (ret_len != NULL) {
		*ret_len = len;
	}

	return out;
}

gchar *
gossip_base64_encode (const guchar *data, 
		      gsize         len)
{
	gchar              *out;
	gchar              *rv;
	static const gchar  alphabet[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
		"0123456789+/";

	if (data == NULL || len < 1) {
		return NULL;
	}

	rv = out = g_malloc (((len / 3) + 1) * 4 + 1);

	for (; len >= 3; len -= 3) {
		*out++ = alphabet[data[0] >> 2];
		*out++ = alphabet[((data[0] << 4) & 0x30) | (data[1] >> 4)];
		*out++ = alphabet[((data[1] << 2) & 0x3c) | (data[2] >> 6)];
		*out++ = alphabet[data[2] & 0x3f];
		data += 3;
	}

	if (len > 0) {
		unsigned char fragment;

		*out++ = alphabet[data[0] >> 2];
		fragment = (data[0] << 4) & 0x30;

		if (len > 1) {
			fragment |= data[1] >> 4;
		}

		*out++ = alphabet[fragment];
		*out++ = (len < 2) ? '=' : alphabet[(data[1] << 2) & 0x3c];
		*out++ = '=';
	}

	*out = '\0';

	return rv;
}

GType
gossip_dbus_type_to_g_type (const gchar *dbus_type_string)
{
	if (dbus_type_string == NULL)
		return G_TYPE_NONE;
	
	if (dbus_type_string[0] == 's') {
		return G_TYPE_STRING;
	}
	else if (dbus_type_string[0] == 'b') {
		return G_TYPE_BOOLEAN;
	}
	else if (dbus_type_string[0] == 'q') {
		return G_TYPE_UINT;
	}
	else if (dbus_type_string[0] == 'n') {
		return G_TYPE_INT;
	}

	g_assert_not_reached ();
	return G_TYPE_NONE;
}

const gchar *
gossip_g_type_to_dbus_type (GType g_type)
{
	switch (g_type)
	{
	case G_TYPE_STRING:
		return "s";
	case G_TYPE_BOOLEAN:
		return "b";
	case G_TYPE_UINT:
		return "q";
	case G_TYPE_INT:
		return "n";
	default:
		g_assert_not_reached ();
	}
	
	return NULL;
}

gchar *
gossip_g_value_to_string (const GValue *value)
{
	gchar  *return_string = NULL;
	GValue string_g_value = {0, };

	g_value_init (&string_g_value, G_TYPE_STRING);
	g_value_transform (value, &string_g_value);
	return_string = g_value_dup_string (&string_g_value);
	g_value_unset (&string_g_value);

	return return_string;
}

GValue *
gossip_string_to_g_value (const gchar *str, GType type)
{
	GValue *g_value;
	
	g_value = g_new0 (GValue, 1);
	g_value_init (g_value, type);
	
	switch (type)
	{
	case G_TYPE_BOOLEAN:
		g_value_set_boolean (g_value, (str[0] == 'y' || str[0] == 'T'));
		break;
	case G_TYPE_UINT:
		g_value_set_uint (g_value, atoi (str));
		break;
	case G_TYPE_INT:
		g_value_set_int (g_value, atoi (str));
		break;
	case G_TYPE_STRING:
		g_value_set_string (g_value, str);
		break;
	default:
		g_assert_not_reached ();
	}
	
	return g_value;
}
