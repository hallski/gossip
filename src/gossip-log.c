/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio HB
 * Copyright (C) 2003 Johan Wallenborg <johan.wallenborg@fishpins.se>
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
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gossip-utils.h"
#include "gossip-log.h"

static void
ensure_dir (void)
{
	gchar *dir;
	
	dir = g_build_filename (g_get_home_dir (), ".gnome2", NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		mkdir (dir, 0755);
	}
	g_free (dir);
	
	dir = g_build_filename (g_get_home_dir (), ".gnome2", "Gossip", NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		mkdir (dir, 0755);
	}
	g_free (dir);

	dir = g_build_filename (g_get_home_dir (), ".gnome2", "Gossip", "logs", NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		mkdir (dir, 0755);
	}
	g_free (dir);
}

void
gossip_log_message (LmMessage *msg, gboolean incoming)
{
	FILE          *file;
        gchar         *filename;
	const gchar   *jid_string;
        GossipJID     *jid;
	const gchar   *body = "";
	const gchar   *to_or_from;
	gchar         *stamp;
	LmMessageNode *node;
	gchar         *nick;
	
	if (incoming) {
		jid_string = lm_message_node_get_attribute (msg->node, "from");
	} else {
		jid_string = lm_message_node_get_attribute (msg->node, "to");
	}

	jid = gossip_jid_new (jid_string);

	ensure_dir ();
	
	filename = g_build_filename (g_get_home_dir (),
				     ".gnome2", "Gossip", "logs",
				     gossip_jid_get_without_resource (jid),
				     NULL);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		file = fopen (filename, "w+");
		if (file) {
			fprintf (file,
				 "<?xml version='1.0' encoding='UTF-8'?>\n"
				 "<gossip-log>\n");
		}
	} else {
		file = fopen (filename, "r+");
		if (file) {
			fseek (file, - (strlen ("</gossip-log>") + 1), SEEK_END);
		} 
	}
	
	g_free (filename);
	
        if (!file) {
		gossip_jid_unref (jid);
		return;
	}

	stamp = gossip_utils_get_timestamp (gossip_utils_get_timestamp_from_message (msg));

	if (incoming) {
		nick = g_strdup (gossip_roster_old_get_nick_from_jid (gossip_app_get_roster (), jid));
		if (!nick) {
			nick = gossip_jid_get_part_name (jid);
		}

		to_or_from = "from";
	} else {
		nick = g_strdup (gossip_app_get_username ());

		to_or_from = "to";
	}

	node = lm_message_node_get_child (msg->node, "body");
	if (node) {
		body = node->value;
	} 

	fprintf (file,
		 "    <message time='%s' %s='%s' resource='%s' nick='%s'>%s</message>\n"
		 "</gossip-log>\n",
		 stamp, to_or_from,
		 gossip_jid_get_without_resource (jid),
		 gossip_jid_get_resource (jid),
		 nick,
		 body);
	
        fclose (file);

	gossip_jid_unref (jid);
	g_free (stamp);
	g_free (nick);
}
