/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Martyn Russell <mr@gnome.org>
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
#include <libxml/xmlreader.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "gossip-status-presets.h"

#define STATUS_PRESETS_XML_FILENAME "StatusPresets"
#define STATUS_PRESETS_DTD_FILENAME "gossip-status-presets.dtd"

/* this is per type of presence */
#define STATUS_PRESETS_MAX_EACH     5  

#define d(x)


typedef struct {
	gchar               *name;
	gchar               *status;
	GossipPresenceState  presence;
} StatusPreset;


static StatusPreset *status_presets_file_parse    (const gchar         *filename);
static gboolean      status_presets_file_validate (const gchar         *filename);
static gboolean      status_presets_file_save     (void);
static StatusPreset *status_preset_new            (const gchar         *name,
						   const gchar         *status,
						   GossipPresenceState  presence);
static void          status_preset_free           (StatusPreset        *status);


static GList *presets = NULL; 


void
gossip_status_presets_get_all (void)
{
	gchar    *dir;
	gchar    *file_with_path;

	/* if already set up clean up first */
	if (presets) {
		g_list_foreach (presets, (GFunc)status_preset_free, NULL);
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
		if (status_presets_file_validate (file_with_path)) {
			status_presets_file_parse (file_with_path);
		}
	}
	
	g_free (file_with_path);
}

static gboolean
status_presets_file_validate (const char *filename)
{

	xmlParserCtxtPtr ctxt;
	xmlDocPtr        doc; 
	gboolean         success = FALSE;

	g_return_val_if_fail (filename != NULL, FALSE);

	d(g_print ("Attempting to validate file (against DTD):'%s'\n", 
		   filename));

	/* create a parser context */
	ctxt = xmlNewParserCtxt ();
	if (ctxt == NULL) {
		g_warning ("Failed to allocate parser context for file:'%s'", 
			   filename);
		return FALSE;
	}

	/* parse the file, activating the DTD validation option */
	doc = xmlCtxtReadFile (ctxt, filename, NULL, XML_PARSE_DTDVALID);

	/* check if parsing suceeded */
	if (doc == NULL) {
		g_warning ("Failed to parse file:'%s'", 
			   filename);
	} else {
		/* check if validation suceeded */
		if (ctxt->valid == 0) {
			g_warning ("Failed to validate file:'%s'", 
				   filename);
		} else {
			success = TRUE;
		}

		/* free up the resulting document */
		xmlFreeDoc(doc);
	}

	/* free up the parser context */
	xmlFreeParserCtxt(ctxt);

	return success;
}

static StatusPreset *
status_presets_file_parse (const gchar *filename) 
{
	StatusPreset     *preset = NULL;

	xmlDocPtr         doc;
	xmlTextReaderPtr  reader;
	int               ret;
	gint              count[4] = { 0, 0, 0, 0};

	g_return_val_if_fail (filename != NULL, FALSE);

	d(g_print ("Attempting to parse file:'%s'...\n", filename));
	
	reader = xmlReaderForFile (filename, NULL, 0);
	if (reader == NULL) {
		g_warning ("could not create xml reader for file:'%s' filename",
			   filename);
		return NULL;
	}

        if (xmlTextReaderPreservePattern (reader, (xmlChar*) "preserved", NULL) < 0) {
		g_warning ("could not preserve pattern for file:'%s' filename",
			   filename);
		return NULL;
	}

	ret = xmlTextReaderRead (reader);

	while (ret == 1) {
		const xmlChar *node = NULL;

		if (!(node = xmlTextReaderConstName (reader))) {
			continue;
		}

		if (xmlStrcmp (node, BAD_CAST "status") == 0) {
			const xmlChar *node_status = NULL;
			xmlChar       *attr_name = NULL;
			xmlChar       *attr_presence = NULL;

			node_status = xmlTextReaderReadString (reader);
			attr_name = xmlTextReaderGetAttribute (reader, BAD_CAST "name");
			attr_presence = xmlTextReaderGetAttribute (reader, BAD_CAST "presence");

			if (node_status && xmlStrlen (node_status) > 0 &&
			    attr_presence && xmlStrlen (attr_presence) > 0) {
				StatusPreset        *sp;
				GossipPresenceState  presence = GOSSIP_PRESENCE_STATE_AVAILABLE;

				if (xmlStrcmp (attr_presence, BAD_CAST "available") == 0) {
					presence = GOSSIP_PRESENCE_STATE_AVAILABLE;
				} else if (xmlStrcmp (attr_presence, BAD_CAST "busy") == 0) {
					presence = GOSSIP_PRESENCE_STATE_BUSY;
				} else if (xmlStrcmp (attr_presence, BAD_CAST "away") == 0) {
					presence = GOSSIP_PRESENCE_STATE_AWAY;
				} else if (xmlStrcmp (attr_presence, BAD_CAST "ext_away") == 0) {
					presence = GOSSIP_PRESENCE_STATE_EXT_AWAY;
				}
				
				count[presence]++;
				if (count[presence] <= STATUS_PRESETS_MAX_EACH) {
					sp = status_preset_new ((gchar*)attr_name, 
								(gchar*)node_status,
								presence);
					
					presets = g_list_append (presets, sp);
				}

			}
			
			xmlFree (attr_name);
			xmlFree (attr_presence);
		}

		ret = xmlTextReaderRead (reader);
	}
	
	if (ret != 0) {
		g_warning ("Could not parse file:'%s' filename",
			   filename);
		xmlFreeTextReader(reader);
		return NULL;
	}

	d(g_print ("Parsed %d status presets\n", g_list_length (presets)));
	
	d(g_print ("Cleaning up parser for file:'%s'\n\n", filename));
	  
	doc = xmlTextReaderCurrentDoc(reader);
	xmlFreeDoc(doc);

	xmlCleanupParser();
	xmlFreeTextReader(reader);
	
	return preset;
}

static StatusPreset *
status_preset_new (const gchar         *name,
		   const gchar         *status,
		   GossipPresenceState  presence)
{
	StatusPreset *preset;

	preset = g_new0 (StatusPreset, 1);
	
	preset->name = g_strdup (name);
	preset->status = g_strdup (status);;
	preset->presence = presence;

	return preset;
}

static void
status_preset_free (StatusPreset *preset)
{
	g_return_if_fail (preset != NULL);
	
	g_free (preset->name);
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

		switch (sp->presence) {
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

		count[sp->presence]++;
		if (count[sp->presence] > STATUS_PRESETS_MAX_EACH) {
			continue;
		}
		
		subnode = xmlNewChild (root, NULL, BAD_CAST "status", BAD_CAST sp->status);
		xmlNewProp (subnode, BAD_CAST "name", BAD_CAST sp->name);
		xmlNewProp (subnode, BAD_CAST "presence", state);	
	}

	d(g_print ("Saving file:'%s'\n", xml_file));
	xmlSaveFormatFileEnc (xml_file, doc, "utf-8", 1);
	xmlFreeDoc (doc);

	xmlCleanupParser ();

	xmlMemoryDump ();
	
	g_free (xml_file);

	return TRUE;
}

GList *
gossip_status_presets_get (GossipPresenceState presence)
{
	GList *list = NULL;
	GList *l;
	
	for (l = presets; l; l = l->next) {
		StatusPreset *sp;

		sp = l->data;

		if (sp->presence != presence) {
			continue;
		}

		list = g_list_append (list, sp->status);
	}

	return list;
}

void
gossip_status_presets_set_last (const gchar         *name,
				const gchar         *status,
				GossipPresenceState  presence)
{
	GList        *l;
	gboolean      found = FALSE;

	g_return_if_fail (status != NULL);

	/* make sure this is not a duplicate*/
	for (l = presets; l; l = l->next) {
		StatusPreset *sp = l->data;

		if (!sp) {
			continue;
		}

		if (sp->name && name && strcmp (sp->name, name) == 0) {
			found = TRUE;
			break;
		}

		if (sp->status && status && strcmp (sp->status, status) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		StatusPreset *sp;

		sp = status_preset_new (name, status, presence);
		presets = g_list_prepend (presets, sp);

		status_presets_file_save ();
	}
}
