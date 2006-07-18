/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
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

#ifndef __GOSSIP_CONF_H__
#define __GOSSIP_CONF_H__

#include <glib-object.h>

#define GOSSIP_TYPE_CONF         (gossip_conf_get_type ())
#define GOSSIP_CONF(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CONF, GossipConf))
#define GOSSIP_CONF_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_CONF, GossipConfClass))
#define GOSSIP_IS_CONF(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CONF))
#define GOSSIP_IS_CONF_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CONF))
#define GOSSIP_CONF_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CONF, GossipConfClass))

typedef struct  {
	GObject parent;
} GossipConf;

typedef struct {
	GObjectClass parent_class;
} GossipConfClass;

typedef void (*GossipConfNotifyFunc) (GossipConf *conf, const gchar *key, gpointer user_data);


GType       gossip_conf_get_type       (void) G_GNUC_CONST;
GossipConf *gossip_conf_get            (void);
void        gossip_conf_shutdown       (void);
guint       gossip_conf_notify_add     (GossipConf            *conf,
					const gchar           *key,
					GossipConfNotifyFunc   func,
					gpointer               data);
gboolean    gossip_conf_notify_remove  (GossipConf            *conf,
					guint                  id);
gboolean    gossip_conf_set_int        (GossipConf            *conf,
					const gchar           *key,
					gint                   value);
gboolean    gossip_conf_get_int        (GossipConf            *conf,
					const gchar           *key,
					gint                  *value);
gboolean    gossip_conf_set_bool       (GossipConf            *conf,
					const gchar           *key,
					gboolean               value);
gboolean    gossip_conf_get_bool       (GossipConf            *conf,
					const gchar           *key,
					gboolean              *value);
gboolean    gossip_conf_set_string     (GossipConf            *conf,
					const gchar           *key,
					const gchar           *value);
gboolean    gossip_conf_get_string     (GossipConf            *conf,
					const gchar           *key,
					gchar                **value);
gboolean    gossip_conf_get_http_proxy (GossipConf            *conf,
					gboolean              *use_http_proxy,
					gchar                **host,
					gint                  *port,
					gboolean              *use_auth,
					gchar                **username,
					gchar                **password);

#endif /* __GOSSIP_CONF_H__ */

