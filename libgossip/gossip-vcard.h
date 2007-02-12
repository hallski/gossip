/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2006 Imendio AB
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

#ifndef __GOSSIP_VCARD_H__
#define __GOSSIP_VCARD_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_VCARD         (gossip_vcard_get_type ())
#define GOSSIP_VCARD(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_VCARD, GossipVCard))
#define GOSSIP_VCARD_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_VCARD, GossipVCardClass))
#define GOSSIP_IS_VCARD(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_VCARD))
#define GOSSIP_IS_VCARD_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_VCARD))
#define GOSSIP_VCARD_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_VCARD, GossipVCardClass))

typedef struct _GossipVCardClass GossipVCardClass;

struct _GossipVCard {
	GObject parent;
};

struct _GossipVCardClass {
	GObjectClass parent_class;
};

GType         gossip_vcard_get_type        (void) G_GNUC_CONST;

GossipVCard * gossip_vcard_new             (void);

const gchar * gossip_vcard_get_name        (GossipVCard  *vcard);
const gchar * gossip_vcard_get_nickname    (GossipVCard  *vcard);
const gchar * gossip_vcard_get_birthday    (GossipVCard  *vcard);
const gchar * gossip_vcard_get_email       (GossipVCard  *vcard);
const gchar * gossip_vcard_get_url         (GossipVCard  *vcard);
const gchar * gossip_vcard_get_country     (GossipVCard  *vcard);
const gchar * gossip_vcard_get_description (GossipVCard  *vcard);
GossipAvatar *gossip_vcard_get_avatar      (GossipVCard  *vcard);

void          gossip_vcard_set_name        (GossipVCard  *vcard,
					    const gchar  *name);
void          gossip_vcard_set_nickname    (GossipVCard  *vcard,
					    const gchar  *nickname);
void          gossip_vcard_set_birthday    (GossipVCard  *vcard,
					    const gchar  *birthday);
void          gossip_vcard_set_email       (GossipVCard  *vcard,
					    const gchar  *email);
void          gossip_vcard_set_url         (GossipVCard  *vcard,
					    const gchar  *url);
void          gossip_vcard_set_country     (GossipVCard  *vcard,
					    const gchar  *country);
void          gossip_vcard_set_description (GossipVCard  *vcard,
					    const gchar  *desc);
void          gossip_vcard_set_avatar      (GossipVCard  *vcard,
					    GossipAvatar *avatar);

G_END_DECLS

#endif /* __GOSSIP_VCARD_H__ */
