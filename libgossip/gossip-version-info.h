/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2004 Imendio AB
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

#ifndef __GOSSIP_VERSION_INFO_H__
#define __GOSSIP_VERSION_INFO_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_VERSION_INFO         (gossip_version_info_get_type ())
#define GOSSIP_VERSION_INFO(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_VERSION_INFO, GossipVersionInfo))
#define GOSSIP_VERSION_INFO_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_VERSION_INFO, GossipVersionInfoClass))
#define GOSSIP_IS_VERSION_INFO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_VERSION_INFO))
#define GOSSIP_IS_VERSION_INFO_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_VERSION_INFO))
#define GOSSIP_VERSION_INFO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_VERSION_INFO, GossipVersionInfoClass))

typedef struct _GossipVersionInfo      GossipVersionInfo;
typedef struct _GossipVersionInfoClass GossipVersionInfoClass;

struct _GossipVersionInfo {
    GObject parent;
};

struct _GossipVersionInfoClass {
    GObjectClass parent_class;
};

GType               gossip_version_info_get_type    (void) G_GNUC_CONST;

GossipVersionInfo * gossip_version_info_new         (void);

const gchar *       gossip_version_info_get_name    (GossipVersionInfo *info);
void                gossip_version_info_set_name    (GossipVersionInfo *info,
                                                     const gchar       *name);
const gchar *       gossip_version_info_get_version (GossipVersionInfo *info);
void                gossip_version_info_set_version (GossipVersionInfo *info,
                                                     const gchar       *version);
const gchar *       gossip_version_info_get_os      (GossipVersionInfo *info);
void                gossip_version_info_set_os      (GossipVersionInfo *info,
                                                     const gchar       *os);

/* Don't free the returned version info */
GossipVersionInfo * gossip_version_info_get_own     (void);

G_END_DECLS

#endif /* __GOSSIP_VERSION_INFO_H__ */

