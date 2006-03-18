/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Martyn Russell <mr@gnome.org>
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
#include <stdlib.h>

#ifdef HAVE_ASPELL
#include <aspell.h>
#endif /* HAVE_ASPELL */

#include <glib/gi18n.h>

#include "gossip-spell.h"

#define DEBUG_MSG(x)  
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n");   */

typedef struct {
#ifdef HAVE_ASPELL
	AspellConfig       *spell_config;
	AspellCanHaveError *spell_possible_err;
        AspellSpeller      *spell_checker;
#endif /* HAVE_ASPELL */
} SpellLanguage;

struct _GossipSpell {
	gint                ref_count;
	GList              *languages;
	gboolean            has_backend;
	GHashTable         *lang_names;
};

static void spell_free (GossipSpell *spell);

#define ISO_CODES_DATADIR ISO_CODES_PREFIX"/share/xml/iso-codes"
#define ISO_CODES_LOCALESDIR ISO_CODES_PREFIX"/share/locale"


GossipSpell * 
gossip_spell_new (GList *languages)
{
	GossipSpell *spell;
	GList       *l;

#ifndef HAVE_ASPELL
	return NULL;
#else 

	DEBUG_MSG (("Spell: Initiating")); 

	spell = g_new0 (GossipSpell, 1);

	spell->ref_count = 1;

	if (!languages) {
		SpellLanguage *lang;
		const gchar   *language = NULL;

		language = getenv ("LANG");
		if (!language) {
			language = "en";
			DEBUG_MSG (("Spell: Using default language ('%s')", language)); 
		} else {
			DEBUG_MSG (("Spell: Using language from environment variable LANG ('%s')", language)); 
		}

		
		lang = g_new0 (SpellLanguage, 1);

		lang->spell_config = new_aspell_config ();
		
		aspell_config_replace (lang->spell_config, "encoding", "utf-8");
		aspell_config_replace (lang->spell_config, "lang", language);
		
		lang->spell_possible_err = new_aspell_speller (lang->spell_config);
		
		if (aspell_error_number (lang->spell_possible_err) == 0) {
			lang->spell_checker = to_aspell_speller (lang->spell_possible_err);
			spell->has_backend = TRUE;
			
			DEBUG_MSG (("Spell: Using ASpell back end for language:'%s'", language)); 
		} else {
			DEBUG_MSG (("Spell: No back end supported")); 
		}

 		spell->languages = g_list_append (spell->languages, lang);
	} else {
		for (l = languages; l; l = l->next) {
			SpellLanguage *lang;
			const gchar   *language;

			language = l->data;

			DEBUG_MSG (("Spell: Using language:'%s'", language)); 

			lang = g_new0 (SpellLanguage, 1);
			
			lang->spell_config = new_aspell_config();
			
			aspell_config_replace (lang->spell_config, "encoding", "utf-8");
			aspell_config_replace (lang->spell_config, "lang", language);

			lang->spell_possible_err = new_aspell_speller (lang->spell_config);

			if (aspell_error_number (lang->spell_possible_err) == 0) {
				lang->spell_checker = to_aspell_speller (lang->spell_possible_err);
				spell->has_backend = TRUE;
				
				DEBUG_MSG (("Spell: Using ASpell back end for language:'%s'", language)); 
			}

			spell->languages = g_list_append (spell->languages, lang);
		}
	}

	return spell;
#endif /* HAVE_ASPELL */
}

static void
spell_free (GossipSpell *spell)
{
	g_return_if_fail (spell != NULL);
	
	g_list_foreach (spell->languages, (GFunc)g_free, NULL);
	g_list_free (spell->languages);
	
	if (spell->lang_names) {
		g_hash_table_destroy (spell->lang_names);
	}

	g_free (spell);
}	

GossipSpell *
gossip_spell_ref (GossipSpell *spell)
{
	g_return_val_if_fail (spell != NULL, NULL);

	spell->ref_count++;
	return spell;
}

void
gossip_spell_unref (GossipSpell *spell)
{
	g_return_if_fail (spell != NULL);

	spell->ref_count--;

	if (spell->ref_count < 1) {
		spell_free (spell);
	}
}

gboolean
gossip_spell_has_backend (GossipSpell *spell)
{
	g_return_val_if_fail (spell != NULL, FALSE);

	return spell->has_backend;
}

static void
gossip_spell_lang_table_parse_start_tag (GMarkupParseContext *ctx,
					 const gchar         *element_name,
					 const gchar        **attr_names,
					 const gchar        **attr_values,
					 gpointer             data,
					 GError             **error)
{
	GossipSpell *spell = (GossipSpell *)data;
	const char  *ccode_longB, *ccode_longT, *ccode, *lang_name;

	if (!g_str_equal (element_name, "iso_639_entry") ||
	    attr_names == NULL || attr_values == NULL) {
		return;
	}

	ccode = NULL;
	ccode_longB = NULL;
	ccode_longT = NULL;
	lang_name = NULL;

	while (*attr_names && *attr_values) {
		if (g_str_equal (*attr_names, "iso_639_1_code")) {
			/* skip if empty */
			if (**attr_values) {
				g_return_if_fail (strlen (*attr_values) == 2);
				ccode = *attr_values;
			}
		}
		else if (g_str_equal (*attr_names, "iso_639_2B_code")) {
			/* skip if empty */
			if (**attr_values) {
				g_return_if_fail (strlen (*attr_values) == 3);
				ccode_longB = *attr_values;
			}
		}
		else if (g_str_equal (*attr_names, "iso_639_2T_code")) {
			/* skip if empty */
			if (**attr_values) {
				g_return_if_fail (strlen (*attr_values) == 3);
				ccode_longT = *attr_values;
			}
		}
		else if (g_str_equal (*attr_names, "name")) {
			lang_name = *attr_values;
		}

		attr_names++;
		attr_values++;
	}

	if (lang_name == NULL) {
		return;
	}

	if (ccode != NULL) {
		g_hash_table_insert (spell->lang_names,
				     g_strdup (ccode),
				     g_strdup (lang_name));
	}
	
	if (ccode_longB != NULL) {
		g_hash_table_insert (spell->lang_names,
				     g_strdup (ccode_longB),
				     g_strdup (lang_name));
	}
	
	if (ccode_longT != NULL) {
		g_hash_table_insert (spell->lang_names,
				     g_strdup (ccode_longT),
				     g_strdup (lang_name));
	}
}

static void
gossip_spell_lang_table_init (GossipSpell *spell)
{
	GError *err = NULL;
	gchar  *buf;
	gsize   buf_len;

	spell->lang_names = g_hash_table_new_full (g_str_hash, g_str_equal,
						   g_free, g_free);

	bindtextdomain ("iso_639", ISO_CODES_LOCALESDIR);
	bind_textdomain_codeset ("iso_639", "UTF-8");

	if (g_file_get_contents (ISO_CODES_DATADIR "/iso_639.xml", &buf, &buf_len, &err)) {
		GMarkupParseContext *ctx;
		GMarkupParser        parser = {
			gossip_spell_lang_table_parse_start_tag,
			NULL, NULL, NULL, NULL
		};

		ctx = g_markup_parse_context_new (&parser, 0, spell, NULL);

		if (!g_markup_parse_context_parse (ctx, buf, buf_len, &err)) {
			g_warning ("Failed to parse '%s': %s",
				   ISO_CODES_DATADIR"/iso_639.xml",
				   err->message);
			g_error_free (err);
		}

		g_markup_parse_context_free (ctx);
		g_free (buf);
	} else {
		g_warning ("Failed to load '%s': %s",
				ISO_CODES_DATADIR"/iso_639.xml", err->message);
		g_error_free (err);
	}
}

const char *
gossip_spell_get_language_name (GossipSpell *spell, const char *lang)
{
#if HAVE_ASPELL
	const gchar *lang_name;
	gint         len;
	
	if (lang == NULL) {
		return "";
	}

	len = strlen (lang);
	if (len != 2 && len != 3) {
		return "";
	}

	if (!spell->lang_names) {
		gossip_spell_lang_table_init (spell);
	}
	
	lang_name = (const gchar*) g_hash_table_lookup (spell->lang_names, lang);

	if (lang_name) {
		return dgettext ("iso_639", lang_name);
	}
	
#endif
	return "";
}

GList *
gossip_spell_get_language_codes (GossipSpell *spell)
{
#ifdef HAVE_ASPELL
	AspellConfig              *config;
	AspellDictInfoList        *dlist;
	AspellDictInfoEnumeration *dels;
	const AspellDictInfo      *entry;
	GList                     *codes = NULL;

	g_return_val_if_fail (spell != NULL, FALSE);

	DEBUG_MSG (("Spell: Listing available language codes:"));

	config = new_aspell_config ();

	/* the returned pointer should _not_ need to be deleted */
	dlist = get_aspell_dict_info_list (config);

	/* config is no longer needed */
	delete_aspell_config (config);

	dels = aspell_dict_info_list_elements (dlist);

	while ((entry = aspell_dict_info_enumeration_next (dels)) != 0) {
		if (g_list_find_custom (codes, entry->code, (GCompareFunc)strcmp)) {
			continue;
		}

		DEBUG_MSG (("Spell:  + %s (%s)", 
			   entry->code, 
			   gossip_spell_get_language_name (entry->code)));
		codes = g_list_append (codes, g_strdup (entry->code));
	}

	delete_aspell_dict_info_enumeration (dels);

	DEBUG_MSG (("Spell: %d language codes in total:", g_list_length (codes)));

	return codes;
#else  /* HAVE_ASPELL */
	return NULL;
#endif /* HAVE_ASPELL */
}

gboolean
gossip_spell_check (GossipSpell *spell, 
		    const gchar *word)
{
#ifdef HAVE_ASPELL
	GList    *l;
#endif /* HAVE_ASPELL */

	gint         langs;
	gboolean     correct = FALSE;

	gint         len;

	const gchar *p;
	gunichar     c;

	gboolean     digit;

	g_return_val_if_fail (spell != NULL, FALSE);
	g_return_val_if_fail (word != NULL, FALSE);

	len = strlen (word);
	g_return_val_if_fail (len > 0, FALSE);

	if (!spell->languages) {
		return FALSE;
	}

	/* Ignore certan cases like numbers, etc. */
	for (p = word, digit = TRUE; *p && digit; p = g_utf8_next_char (p)) {
		c = g_utf8_get_char (p);
		digit = g_unichar_isdigit (c);
	}

	if (digit) {
		/* We don't spell check digits. */
		DEBUG_MSG (("Not spell checking word:'%s', it is all digits", word));
		return TRUE;
	}
		
	langs = g_list_length (spell->languages);

#ifdef HAVE_ASPELL
	for (l = spell->languages; l; l = l->next) {
		SpellLanguage *lang;

		lang = l->data;
		if (!lang) {
			continue;
		}

		correct = aspell_speller_check (lang->spell_checker, word, len);
		if (langs > 1 && correct) {
			break;
		}
	}
#endif /* HAVE_ASPELL */

	return correct;
}

GList *
gossip_spell_suggestions (GossipSpell *spell, 
			  const gchar *word)
{
#ifdef HAVE_ASPELL
	GList *l1;
#endif /* HAVE_ASPELL */
	GList *l2 = NULL;

#ifdef HAVE_ASPELL
	AspellWordList *suggestions = NULL;
	AspellStringEnumeration *elements = NULL;

	const char *next = NULL;
#endif /* HAVE_ASPELL */

	g_return_val_if_fail (spell != NULL, NULL);
	g_return_val_if_fail (word != NULL, NULL);
	g_return_val_if_fail (word[0] != 0, NULL);

#ifdef HAVE_ASPELL
	for (l1 = spell->languages; l1; l1 = l1->next) {
		SpellLanguage *lang;

		lang = l1->data;
		if (!lang) {
			continue;
		}

		suggestions = (AspellWordList*) aspell_speller_suggest (lang->spell_checker,
								word, strlen (word));

	elements = aspell_word_list_elements (suggestions);
	
	while ((next = aspell_string_enumeration_next (elements)) != NULL) {
			l2 = g_list_append (l2, g_strdup (next));
		}
	}
	
	delete_aspell_string_enumeration (elements);
#endif /* HAVE_ASPELL */

	return l2;
}

gboolean
gossip_spell_supported (void)
{
	if (g_getenv ("GOSSIP_SPELL_DISABLED")) {
		return FALSE;
	}

#ifdef HAVE_ASPELL
	return TRUE;
#else  /* HAVE_ASPELL */
	return FALSE;
#endif /* HAVE_ASPELL */
}
