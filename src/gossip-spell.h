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

#ifndef __GOSSIP_SPELL_H__
#define __GOSSIP_SPELL_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GossipSpell GossipSpell;

GossipSpell *gossip_spell_new                (GList       *languages);
GossipSpell *gossip_spell_ref                (GossipSpell *spell);
void         gossip_spell_unref              (GossipSpell *spell);
gboolean     gossip_spell_has_backend        (GossipSpell *spell);
gboolean     gossip_spell_check              (GossipSpell *spell,
					      const gchar *word);
GList *      gossip_spell_suggestions        (GossipSpell *spell,
					      const gchar *word);
gboolean     gossip_spell_supported          (void);
const gchar *gossip_spell_get_language_name  (GossipSpell *spell,
					      const gchar *code);
GList       *gossip_spell_get_language_codes (GossipSpell *spell);

G_END_DECLS

#endif /* __GOSSIP_SPELL_H__ */
