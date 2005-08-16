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

#include "gossip-contact-groups.h"

#define CONTACT_GROUPS_XML_FILENAME "ContactGroups"
#define CONTACT_GROUPS_DTD_FILENAME "gossip-contact-groups.dtd"

#define d(x)


typedef struct {
	gchar    *name;
	gboolean  expanded;
} ContactGroup;


static ContactGroup *contact_groups_file_parse    (const gchar  *filename);
static gboolean      contact_groups_file_validate (const gchar  *filename);
static gboolean      contact_groups_file_save     (void);

static ContactGroup *contact_group_new            (const gchar  *name,
						   gboolean      expanded);
static void          contact_group_free           (ContactGroup *group);


static GList *groups = NULL; 


void
gossip_contact_groups_get_all (void)
{
	gchar    *dir;
	gchar    *file_with_path;

	/* if already set up clean up first */
	if (groups) {
		g_list_foreach (groups, (GFunc)contact_group_free, NULL);
		g_list_free (groups);
		groups = NULL;
	}

	dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
	if (!g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		mkdir (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	}

	file_with_path = g_build_filename (dir, CONTACT_GROUPS_XML_FILENAME, NULL);
	g_free (dir);

	if (g_file_test(file_with_path, G_FILE_TEST_EXISTS)) {
		if (contact_groups_file_validate (file_with_path)) {
			contact_groups_file_parse (file_with_path);
		}
	}
	
	g_free (file_with_path);
}

static gboolean
contact_groups_file_validate (const char *filename)
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

static ContactGroup *
contact_groups_file_parse (const gchar *filename) 
{
	ContactGroup *group = NULL;

	xmlDocPtr                doc;
	xmlTextReaderPtr         reader;
	int                      ret;

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
			
		if (xmlStrcmp (node, BAD_CAST "group") == 0) {
			xmlChar *attr_name = NULL;
			xmlChar *attr_expanded = NULL;

			attr_name = xmlTextReaderGetAttribute (reader, BAD_CAST "name");
			attr_expanded = xmlTextReaderGetAttribute (reader, BAD_CAST "expanded");

			if (attr_name && xmlStrlen (attr_name) > 0 &&
			    attr_expanded && xmlStrlen (attr_expanded) > 0) {
				ContactGroup *cg;
				gboolean expanded;

				expanded = (xmlStrcasecmp (attr_expanded, BAD_CAST "yes") == 0 ? TRUE : FALSE);
				cg = contact_group_new ((gchar*)attr_name, expanded);

				groups = g_list_append (groups, cg);
			}
			
			xmlFree (attr_name);
			xmlFree (attr_expanded);
		}

		ret = xmlTextReaderRead (reader);
	}
	
	if (ret != 0) {
		g_warning ("Could not parse file:'%s' filename",
			   filename);
		xmlFreeTextReader(reader);
		return NULL;
	}

	d(g_print ("Parsed %d contact groups\n", g_list_length (groups)));
	
	d(g_print ("Cleaning up parser for file:'%s'\n\n", filename));
	  
	doc = xmlTextReaderCurrentDoc(reader);
	xmlFreeDoc(doc);

	xmlCleanupParser();
	xmlFreeTextReader(reader);
	
	return group;
}

static ContactGroup *
contact_group_new (const gchar *name,
		   gboolean     expanded)
{
	ContactGroup *group;

	group = g_new0 (ContactGroup, 1);
	
	group->name = g_strdup (name);
	group->expanded = expanded;

	return group;
}

static void
contact_group_free (ContactGroup *group)
{
	g_return_if_fail (group != NULL);
	
	g_free (group->name);
	
	g_free (group);
}

static gboolean
contact_groups_file_save (void)
{
	xmlDocPtr   doc;  
	xmlDtdPtr   dtd;  
	xmlNodePtr  root;
	xmlNodePtr  node;
	GList      *l;
	gchar      *dtd_file;
	gchar      *xml_dir;
	gchar      *xml_file;

	xml_dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
	if (!g_file_test (xml_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		mkdir (xml_dir, S_IRUSR | S_IWUSR | S_IXUSR);
	}

	xml_file = g_build_filename (xml_dir, CONTACT_GROUPS_XML_FILENAME, NULL);
	g_free (xml_dir);

	dtd_file = g_build_filename (DTDDIR, CONTACT_GROUPS_DTD_FILENAME, NULL);

	doc = xmlNewDoc (BAD_CAST "1.0");
	root = xmlNewNode (NULL, BAD_CAST "contacts");
	xmlDocSetRootElement (doc, root);

	dtd = xmlCreateIntSubset (doc, BAD_CAST "contacts", NULL, BAD_CAST dtd_file);

	node = xmlNewChild (root, NULL, BAD_CAST "account", NULL);
	xmlNewProp (node, BAD_CAST "name", BAD_CAST "Default");

	for (l = groups; l; l = l->next) {
		ContactGroup *cg;
		xmlNodePtr    subnode;

		cg = l->data;
		
		subnode = xmlNewChild (node, NULL, BAD_CAST "group", NULL);
		xmlNewProp (subnode, BAD_CAST "expanded", BAD_CAST (cg->expanded ? "yes" : "no"));	
		xmlNewProp (subnode, BAD_CAST "name", BAD_CAST cg->name);
	}

	d(g_print ("Saving file:'%s'\n", xml_file));
	xmlSaveFormatFileEnc (xml_file, doc, "utf-8", 1);
	xmlFreeDoc (doc);

	xmlCleanupParser ();

	xmlMemoryDump ();
	
	g_free (xml_file);

	return TRUE;
}

gboolean
gossip_contact_group_get_expanded (const gchar *group)
{
	GList    *l;
	gboolean  default_val = TRUE;

	g_return_val_if_fail (group != NULL, default_val);
	
	for (l = groups; l; l = l->next) {
		ContactGroup *cg = l->data;

		if (!cg || !cg->name) {
			continue;
		}

		if (strcmp (cg->name, group) == 0) {
			return cg->expanded;
		}
	}

	return default_val;
}

void    
gossip_contact_group_set_expanded (const gchar *group, 
				   gboolean     expanded)
{
	GList        *l;
	ContactGroup *cg;
	gboolean      changed = FALSE;

	g_return_if_fail (group != NULL);
	
	for (l = groups; l; l = l->next) {
		ContactGroup *cg = l->data;

		if (!cg || !cg->name) {
			continue;
		}

		if (strcmp (cg->name, group) == 0) {
			cg->expanded = expanded;
			changed = TRUE;
			break;
		}
	}

	/* if here... we don't have a ContactGroup for the group. */
	if (!changed) {
		cg = contact_group_new (group, expanded);
		groups = g_list_append (groups, cg);
	}

	contact_groups_file_save ();
}
