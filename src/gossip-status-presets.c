/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Martyn Russell <mr@gnome.org>
 * Copyright (C) 2005 Imendio AB
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
#include <glib.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "gossip-status-presets.h"

#define DEBUG_MSG(x)  
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n");  */

#define STATUS_PRESETS_XML_FILENAME "status-presets.xml"
#define STATUS_PRESETS_DTD_FILENAME "gossip-status-presets.dtd"
#define STATUS_PRESETS_MAX_EACH     15


typedef struct {
	gchar               *status;
	GossipPresenceState  state;
} StatusPreset;


static void          status_presets_file_parse    (const gchar         *filename);
static gboolean      status_presets_file_save     (void);
static StatusPreset *status_preset_new            (const gchar         *status,
						   GossipPresenceState  state);
static void          status_preset_free           (StatusPreset        *status);


static GList *presets = NULL; 


void
gossip_status_presets_get_all (void)
{
	gchar *dir;
	gchar *file_with_path;

	/* If already set up clean up first. */
	if (presets) {
		g_list_foreach (presets, (GFunc) status_preset_free, NULL);
		g_list_free (presets);
		presets = NULL;
	}

	dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		mkdir (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	}

	file_with_path = g_build_filename (dir, STATUS_PRESETS_XML_FILENAME, NULL);
	g_free (dir);

	if (g_file_test(file_with_path, G_FILE_TEST_EXISTS)) {
		status_presets_file_parse (file_with_path);
	}
	
	g_free (file_with_path);
}

static void
status_presets_file_parse (const gchar *filename) 
{
	xmlParserCtxtPtr  ctxt;
	xmlDocPtr         doc;
	xmlNodePtr        presets_node;
	xmlNodePtr        node;
	
	DEBUG_MSG (("StatusPresets: Attempting to parse file:'%s'...", filename));

 	ctxt = xmlNewParserCtxt ();

	/* Parse and validate the file. */
	doc = xmlCtxtReadFile (ctxt, filename, NULL, XML_PARSE_DTDVALID);	
	if (!doc) {
		g_warning ("Failed to parse file:'%s'", filename);
		xmlFreeParserCtxt (ctxt);
		return;
	}

	if (!ctxt->valid) {
		g_warning ("Failed to validate file:'%s'",  filename);
		xmlFreeDoc(doc);
		xmlFreeParserCtxt (ctxt);
		return;
	}
	
	/* The root node, presets. */
	presets_node = xmlDocGetRootElement (doc);

	node = presets_node->children;
	while (node) {
		if (strcmp ((gchar *) node->name, "status") == 0) {
			gchar               *status;
			gchar               *state_str;
			GossipPresenceState  state;
			StatusPreset        *preset;

			status = (gchar *) xmlNodeGetContent (node);
			state_str = (gchar *) xmlGetProp (node, BAD_CAST ("presence"));

			if (state_str) {
				if (strcmp (state_str, "available") == 0) {
					state = GOSSIP_PRESENCE_STATE_AVAILABLE;
				}
				else if (strcmp (state_str, "busy") == 0) {
					state = GOSSIP_PRESENCE_STATE_BUSY;
				}
				else if (strcmp (state_str, "away") == 0) {
					state = GOSSIP_PRESENCE_STATE_AWAY;
				}
				else if (strcmp (state_str, "ext_away") == 0) {
					state = GOSSIP_PRESENCE_STATE_EXT_AWAY;
				} else {
					state = GOSSIP_PRESENCE_STATE_AVAILABLE;
				}
				
				preset = status_preset_new (status, state);
				presets = g_list_append (presets, preset);
			}

			xmlFree (status);
			xmlFree (state_str);
		}

		node = node->next;
	}
	
	DEBUG_MSG (("StatusPresets: Parsed %d status presets", g_list_length (presets)));

	xmlFreeDoc (doc);
	xmlFreeParserCtxt (ctxt);
}

static StatusPreset *
status_preset_new (const gchar         *status,
		   GossipPresenceState  state)
{
	StatusPreset *preset;

	preset = g_new0 (StatusPreset, 1);
	
	preset->status = g_strdup (status);
	preset->state = state;

	return preset;
}

static void
status_preset_free (StatusPreset *preset)
{
	g_free (preset->status);
	g_free (preset);
}

static gboolean
status_presets_file_save (void)
{
	xmlDocPtr   doc;  
	xmlDtdPtr   dtd;  
	xmlNodePtr  root;
	GList      *l;
	gchar      *dtd_file;
	gchar      *xml_dir;
	gchar      *xml_file;
	gint        count[4] = { 0, 0, 0, 0};

	xml_dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
	if (!g_file_test (xml_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		mkdir (xml_dir, S_IRUSR | S_IWUSR | S_IXUSR);
	}

	xml_file = g_build_filename (xml_dir, STATUS_PRESETS_XML_FILENAME, NULL);
	g_free (xml_dir);

	dtd_file = g_build_filename (DTDDIR, STATUS_PRESETS_DTD_FILENAME, NULL);

	doc = xmlNewDoc (BAD_CAST "1.0");
	root = xmlNewNode (NULL, BAD_CAST "presets");
	xmlDocSetRootElement (doc, root);

	dtd = xmlCreateIntSubset (doc, BAD_CAST "presets", NULL, BAD_CAST dtd_file);

	for (l = presets; l; l = l->next) {
		StatusPreset *sp;
		xmlNodePtr    subnode;
		xmlChar      *state;

		sp = l->data;

		switch (sp->state) {
		case GOSSIP_PRESENCE_STATE_AVAILABLE:
			state = BAD_CAST "available";
			break;
		case GOSSIP_PRESENCE_STATE_BUSY:
			state = BAD_CAST "busy";
			break;
		case GOSSIP_PRESENCE_STATE_AWAY:
			state = BAD_CAST "away";
			break;
		case GOSSIP_PRESENCE_STATE_EXT_AWAY:
			state = BAD_CAST "ext_away";
			break;
		default:
			continue;
		}

		count[sp->state]++;
		if (count[sp->state] > STATUS_PRESETS_MAX_EACH) {
			continue;
		}
		
		subnode = xmlNewChild (root,
				       NULL,
				       BAD_CAST "status",
				       BAD_CAST sp->status);
		xmlNewProp (subnode, BAD_CAST "presence", state);	
	}

	DEBUG_MSG (("StatusPresets: Saving file:'%s'", xml_file));
	xmlSaveFormatFileEnc (xml_file, doc, "utf-8", 1);
	xmlFreeDoc (doc);

	g_free (xml_file);

	return TRUE;
}

GList *
gossip_status_presets_get (GossipPresenceState state, gint max_number)
{
	GList *list = NULL;
	GList *l;
	gint   i;

	i = 0;
	for (l = presets; l; l = l->next) {
		StatusPreset *sp;

		sp = l->data;

		if (sp->state != state) {
			continue;
		}
		
		list = g_list_append (list, sp->status);
		i++;

		if (max_number != -1 && i >= max_number) {
			break;
		}
	}

	return list;
}

void
gossip_status_presets_set_last (const gchar         *status,
				GossipPresenceState  state)
{
	GList        *l;
	StatusPreset *preset;
	gint          num;

	/* Remove any duplicate. */
	for (l = presets; l; l = l->next) {
		preset = l->data;

		if (state == preset->state) {
			if (strcmp (status, preset->status) == 0) {
				status_preset_free (preset);
				presets = g_list_delete_link (presets, l);
				break;
			}
		}
	}

	preset = status_preset_new (status, state);
	presets = g_list_prepend (presets, preset);

	num = 0;
	for (l = presets; l; l = l->next) {
		preset = l->data;

		if (state != preset->state) {
			continue;
		}

		num++;
		
		if (num > STATUS_PRESETS_MAX_EACH) {
			status_preset_free (preset);
			presets = g_list_delete_link (presets, l);
			break;
		}
	}

	status_presets_file_save ();
}

void
gossip_status_presets_reset (void)
{
	g_list_foreach (presets, (GFunc) status_preset_free, NULL);
	g_list_free (presets);

	presets = NULL;

	status_presets_file_save ();
}
