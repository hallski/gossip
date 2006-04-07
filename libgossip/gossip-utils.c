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
#include <gconf/gconf-client.h>
#include <glib/gi18n.h>
#include "gossip-utils.h"

#define DEBUG_MSG(x)   
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n");   */

static void utils_regex_init (void);
static void utils_free_entry (GossipStatusEntry *entry);

#define AVAILABLE_MESSAGE "Available"
#define AWAY_MESSAGE      "Away"
#define BUSY_MESSAGE      "Busy"

GList *
gossip_utils_get_status_messages (void)
{
	GConfClient       *gconf_client;
	GSList            *list, *l;
	GList             *ret = NULL;
	GossipStatusEntry *entry;
	const gchar       *status;
	
	gconf_client = gconf_client_get_default ();
	
	list = gconf_client_get_list (gconf_client,
				      "/apps/gossip/status/preset_messages",
				      GCONF_VALUE_STRING,
				      NULL);

	g_object_unref (gconf_client);

	/* This is really ugly, but we can't store a list of pairs and a dir
	 * with entries wouldn't work since we have no guarantee on the order of
	 * entries.
	 */
	
	for (l = list; l; l = l->next) {
		entry = g_new (GossipStatusEntry, 1);

		status = l->data;

		if (strncmp (status, "available/", 10) == 0) {
			entry->string = g_strdup (&status[10]);
			entry->state = GOSSIP_PRESENCE_STATE_AVAILABLE;
		}
		else if (strncmp (status, "busy/", 5) == 0) {
			entry->string = g_strdup (&status[5]);
			entry->state = GOSSIP_PRESENCE_STATE_BUSY;
		}
		else if (strncmp (status, "away/", 5) == 0) {
			entry->string = g_strdup (&status[5]);
			entry->state = GOSSIP_PRESENCE_STATE_AWAY;
		} else {
			continue;
		}
		
		ret = g_list_append (ret, entry);
	}
	
	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);
	
	return ret;

}

void 
gossip_utils_set_status_messages (GList *list)
{
	GConfClient       *gconf_client;
	GList             *l;
	GossipStatusEntry *entry;
	GSList            *slist = NULL;
	const gchar       *state;
	gchar             *str;
	
	for (l = list; l; l = l->next) {
		entry = l->data;

		switch (entry->state) {
		case GOSSIP_PRESENCE_STATE_AVAILABLE:
			state = "available";
			break;
		case GOSSIP_PRESENCE_STATE_BUSY:
			state = "busy";
			break;
		case GOSSIP_PRESENCE_STATE_AWAY:
			state = "away";
			break;
		default:
			state = NULL;
			g_assert_not_reached ();
		}
		
		str = g_strdup_printf ("%s/%s", state, entry->string);
		slist = g_slist_append (slist, str);
	}

	gconf_client = gconf_client_get_default ();
	gconf_client_set_list (gconf_client,
			       "/apps/gossip/status/preset_messages",
			       GCONF_VALUE_STRING,
			       slist,
			       NULL);
	g_object_unref (gconf_client);

	g_slist_foreach (slist, (GFunc) g_free, NULL);
	g_slist_free (slist);
}

static void
utils_free_entry (GossipStatusEntry *entry)
{
	g_free (entry->string);
	g_free (entry);
}

void
gossip_utils_free_status_messages (GList *list)
{
	g_list_foreach (list, (GFunc) utils_free_entry, NULL);
	g_list_free (list);
}

gchar *
gossip_utils_substring (const gchar *str, gint start, gint end)
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
utils_regex_init (void)
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
		regcomp (&dingus[i], expression, REG_EXTENDED);
	}
	
	inited = TRUE;
}

gint
gossip_utils_regex_match (GossipRegExType  type, 
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

	utils_regex_init ();

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
		DEBUG_MSG (("Utils: Found %d matches for regex type:%d", num_matches, type));
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

	DEBUG_MSG (("Utils: Found %d matches for ALL regex types", num_matches));

	return num_matches;
}

GossipPresenceState
gossip_utils_get_presence_state_from_show_string (const gchar *str)
{
	if (!str || !str[0]) {
		return GOSSIP_PRESENCE_STATE_AVAILABLE;
	}
	else if (strcmp (str, "dnd") == 0) {
		return GOSSIP_PRESENCE_STATE_BUSY;
	}
	else if (strcmp (str, "away") == 0) {
		return GOSSIP_PRESENCE_STATE_AWAY;
	}
	else if (strcmp (str, "xa") == 0) {
		return GOSSIP_PRESENCE_STATE_EXT_AWAY;
	}
	/* We don't support chat, so treat it like available. */
	else if (strcmp (str, "chat") == 0) {
		return GOSSIP_PRESENCE_STATE_AVAILABLE;
	}

	return GOSSIP_PRESENCE_STATE_AVAILABLE;
}

gint
gossip_utils_str_case_cmp (const gchar *s1, const gchar *s2)
{
	return gossip_utils_str_n_case_cmp (s1, s2, -1);
}	

gint
gossip_utils_str_n_case_cmp (const gchar *s1, const gchar *s2, gsize n)
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
gossip_utils_xml_validate (xmlDoc *doc, const gchar *dtd_filename)
{
	gchar        *path;
	xmlValidCtxt  cvp;
	xmlDtd       *dtd;
	gboolean      ret;
		
	path = g_build_filename (DTDDIR, dtd_filename, NULL);

	memset (&cvp, 0, sizeof (cvp));
	dtd = xmlParseDTD (NULL, path);
	ret = xmlValidateDtd (&cvp, doc, dtd);
	
	xmlFreeDtd (dtd);
	g_free (path);
	
        return ret;
}
