/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2005 Imendio AB
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

#ifndef __GOSSIP_UTILS_H__
#define __GOSSIP_UTILS_H__

#include "gossip-account.h"
#include "gossip-presence.h"

typedef struct {
	GossipPresenceState  state;
	gchar               *string;
} GossipStatusEntry;

GList *      gossip_utils_get_status_messages        (void);
void         gossip_utils_set_status_messages        (GList            *list);
void         gossip_utils_free_status_messages       (GList            *list);

gchar *      gossip_utils_substring                  (const gchar      *str,
						      gint              start,
						      gint              end);

gint         gossip_utils_url_regex_match            (const gchar      *msg,
						      GArray           *start,
						      GArray           *end);
GossipPresenceState
gossip_utils_get_presence_state_from_show_string     (const gchar      *str); 
gint         gossip_utils_str_case_cmp               (const gchar      *s1, 
						      const gchar      *s2);
gint         gossip_utils_str_n_case_cmp             (const gchar      *s1, 
						      const gchar      *s2,
						      gsize             n);
const gchar *gossip_utils_jid_str_locate_resource    (const gchar      *str);
gchar *      gossip_utils_jid_str_get_part_name      (const gchar      *jid);

gchar *      gossip_utils_jid_str_get_part_host      (const gchar      *jid);


#endif /*  __GOSSIP_UTILS_H__ */
