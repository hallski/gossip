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
};

static void spell_free (GossipSpell *spell);

/* 
 * Can we get this from somewhere else?
 *
 * Evolution uses Corba and all that jazz to talk to GnomeSpell, it is
 * pretty horrible really, plus their list of languages is not as
 * extensive as the one below.
 *
 * The list below is a combination of the GnomeSpell list and the list
 * of languages from the Gnome Translation Project. 
 */

const gchar *languages[] = {
	"af", N_("Afrikaans"),
	"am", N_("Amharic"),
	"ar", N_("Arabic"),
	"az", N_("Azerbaijani"),
	"be", N_("Belarusian"),
	"bg", N_("Bulgarian"),
	"bn", N_("Bengali"),
	"br", N_("Breton"),
	"bs", N_("Bosnian"),
	"ca", N_("Catalan"),
	"cs", N_("Czech"),
	"cy", N_("Welsh"),
	"da", N_("Danish"),
	"de", N_("German"),
	"de_AT", N_("German (Austria)"),
	"de_DE", N_("German (Germany)"),
	"de_CH", N_("German (Swiss)"),
	"el", N_("Greek"),
	"en", N_("English"),
	"en_CA", N_("English (Canadian)"),
	"en_GB", N_("English (British)"),
	"en_US", N_("English (American)"),
	"eo", N_("Esperanto"),
	"es", N_("Spanish"),
	"et", N_("Estonian"),
	"eu", N_("Basque"),
	"fa", N_("Persian"),
	"fi", N_("Finnish"),
	"fo", N_("Faroese"),
	"fr", N_("French"),
	"fr_FR", N_("French (France)"),
	"fr_CH", N_("French (Swiss)"),
	"ga", N_("Irish Gaelic"),
	"gd", N_("Scots Gaelic"),
	"gl", N_("Galician"),
	"gu", N_("Gujarati"),
	"gv", N_("Manx Gaelic"),
	"he", N_("Hebrew"),
	"hi", N_("Hindi"),
	"hr", N_("Croatian"),
	"hu", N_("Hungarian"),
	"id", N_("Indonesian"),
	"is", N_("Icelandic"),
	"it", N_("Italian"),
	"ja", N_("Japanese"),
	"ka", N_("Georgian"),
	"kn", N_("Kannada"),
	"ko", N_("Korean"),
	"ku", N_("Kurdish"),
	"kw", N_("Cornish"),
	"li", N_("Limburgish"),
	"lt", N_("Lithuanian"),
	"lv", N_("Latvian"),
	"mi", N_("Maori"),
	"mk", N_("Macedonian"),
	"ml", N_("Malayalam"),
	"mn", N_("Mongolian"),
	"mr", N_("Marathi"),
	"ms", N_("Malay"),
	"nb", N_("Norwegian (Bokmal)"),
	"nb_NO", N_("Norwegian (Bokmal)"),
	"ne", N_("Nepali"),
	"nl", N_("Dutch"),
	"no", N_("Norwegian"),
	"nn", N_("Norwegian (Nynorsk)"),
	"nn_NO", N_("Norwegian (Nynorsk)"),
	"or", N_("Oriya"),
	"pa", N_("Punjabi"),
	"pl", N_("Polish"),
	"pt", N_("Portuguese"),
	"pt_PT", N_("Portuguese (Portugal)"),
	"pt_BR", N_("Portuguese (Brazil)"),
	"ro", N_("Romanian"),
	"ru", N_("Russian"),
	"rw", N_("Kinyarwanda"),
	"sk", N_("Slovak"),
	"sl", N_("Slovenian"),
	"sq", N_("Albanian"),
	"sr", N_("Serbian"),
	"sv", N_("Swedish"),
	"ta", N_("Tamil"),
	"te", N_("Telugu"),
	"th", N_("Thai"),
	"tk", N_("Turkmen"),
	"tr", N_("Turkish"),
	"uk", N_("Ukrainian"),
	"vi", N_("Vietnamese"),
	"wa", N_("Walloon"),
	"xh", N_("Xhosa"),
	"yi", N_("Yiddish"),
	"zh_CN", N_("Chinese Simplified"),
	"zh_TW", N_("Chinese Traditional"),
	NULL
};

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

		lang->spell_config = new_aspell_config();
		
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

const gchar *
gossip_spell_get_language_name (const gchar *code)
{
	gint i;

	for (i = 0; code && languages[i] && languages[i+1]; i+=2) {
		const char *lang_code = languages[i];

		if (code && lang_code && strcmp (code, lang_code) == 0) {
			return _(languages[i+1]);
		}
	} 
	
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
