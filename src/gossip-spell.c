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
#endif

#include "gossip-spell.h"

#define d(x)

struct _GossipSpell {
	gint                ref_count;

	gchar              *language;
	
	gboolean            has_backend;

#ifdef HAVE_ASPELL
	/* aspell */
	AspellConfig       *spell_config;
	AspellCanHaveError *spell_possible_err;
        AspellSpeller      *spell_checker;
#endif
};


static void spell_free (GossipSpell *spell);


GossipSpell * 
gossip_spell_new (const gchar *language)
{
	GossipSpell *spell;
	
#ifndef HAVE_ASPELL
	return NULL;
#endif

	spell = g_new0 (GossipSpell, 1);

	spell->ref_count = 1;

	if (!language) {
		const gchar *lang = NULL;

		lang = getenv ("LANG");
		if (!lang) {
			lang = "en_US";
		}

		spell->language = g_strdup (lang);
	} else {
		spell->language = g_strdup (language);
	}

#ifdef HAVE_ASPELL
	spell->spell_config = new_aspell_config();

	aspell_config_replace (spell->spell_config, "lang", spell->language);
	aspell_config_replace (spell->spell_config, "encoding", "utf-8");

	spell->spell_possible_err = new_aspell_speller (spell->spell_config);

	if (aspell_error_number (spell->spell_possible_err) == 0) {
		spell->spell_checker = to_aspell_speller (spell->spell_possible_err);
		spell->has_backend = TRUE;
	}
#endif

	return spell;
}

static void
spell_free (GossipSpell *spell)
{
	g_return_if_fail (spell != NULL);
	
	g_free (spell->language);
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

gboolean
gossip_spell_check (GossipSpell *spell, const gchar *word)
{
	gboolean correct = FALSE;

	g_return_val_if_fail (spell != NULL, FALSE);
	g_return_val_if_fail (word != NULL, FALSE);
	g_return_val_if_fail (strlen (word) > 0, FALSE);

#ifdef HAVE_ASPELL
	correct = aspell_speller_check (spell->spell_checker, 
					word, strlen (word));
#endif

	return correct;
}

GList *
gossip_spell_suggestions (GossipSpell *spell, const gchar *word)
{
	GList *l = NULL;

#ifdef HAVE_ASPELL
	AspellWordList *suggestions = NULL;
	AspellStringEnumeration *elements = NULL;

	const char *next = NULL;
#endif

	g_return_val_if_fail (spell != NULL, NULL);
	g_return_val_if_fail (word != NULL, NULL);
	g_return_val_if_fail (strlen (word) > 0, NULL);

#ifdef HAVE_ASPELL
	suggestions = (AspellWordList*) aspell_speller_suggest (spell->spell_checker,
								word, strlen (word));

	elements = aspell_word_list_elements (suggestions);
	
	while ((next = aspell_string_enumeration_next (elements)) != NULL) {
		l = g_list_append (l, g_strdup (next));
	}
	
	delete_aspell_string_enumeration (elements);
#endif

	return l;
}

gboolean
gossip_spell_supported (void)
{
#ifdef HAVE_ASPELL
	return TRUE;
#else 
	return FALSE;
#endif
}
