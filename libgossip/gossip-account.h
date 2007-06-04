/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2007 Imendio AB
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
 * 
 * Author: Martyn Russell <martyn@imendio.com>
 */

#ifndef __GOSSIP_ACCOUNT_H__
#define __GOSSIP_ACCOUNT_H__

#include <glib-object.h>
#include <gtk/gtkiconfactory.h>

#define GOSSIP_TYPE_ACCOUNT         (gossip_account_get_type ())
#define GOSSIP_ACCOUNT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_ACCOUNT, GossipAccount))
#define GOSSIP_ACCOUNT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_ACCOUNT, GossipAccountClass))
#define GOSSIP_IS_ACCOUNT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_ACCOUNT))
#define GOSSIP_IS_ACCOUNT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_ACCOUNT))
#define GOSSIP_ACCOUNT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_ACCOUNT, GossipAccountClass))

typedef struct _GossipAccount      GossipAccount;
typedef struct _GossipAccountClass GossipAccountClass;

struct _GossipAccount {
	GObject      parent;
};

struct _GossipAccountClass {
	GObjectClass parent_class;
};

typedef enum {
	GOSSIP_ACCOUNT_TYPE_JABBER,
	GOSSIP_ACCOUNT_TYPE_AIM,
	GOSSIP_ACCOUNT_TYPE_ICQ,
	GOSSIP_ACCOUNT_TYPE_MSN,
	GOSSIP_ACCOUNT_TYPE_YAHOO,
	GOSSIP_ACCOUNT_TYPE_IRC,
	GOSSIP_ACCOUNT_TYPE_UNKNOWN,
	GOSSIP_ACCOUNT_TYPE_COUNT
} GossipAccountType;

GType             gossip_account_get_type             (void) G_GNUC_CONST;

const gchar *     gossip_account_get_name             (GossipAccount     *account);
const gchar *     gossip_account_get_id               (GossipAccount     *account);
const gchar *     gossip_account_get_password         (GossipAccount     *account);
const gchar *     gossip_account_get_resource         (GossipAccount     *account);
const gchar *     gossip_account_get_server           (GossipAccount     *account);
guint16           gossip_account_get_port             (GossipAccount     *account);
gboolean          gossip_account_get_auto_connect     (GossipAccount     *account);
gboolean          gossip_account_get_use_ssl          (GossipAccount     *account);
gboolean          gossip_account_get_use_proxy        (GossipAccount     *account);

void              gossip_account_set_id               (GossipAccount     *account,
						       const gchar       *id);
void              gossip_account_set_name             (GossipAccount     *account,
						       const gchar       *name);
void              gossip_account_set_password         (GossipAccount     *account,
						       const gchar       *password);
void              gossip_account_set_resource         (GossipAccount     *account,
						       const gchar       *resource);
void              gossip_account_set_server           (GossipAccount     *account,
						       const gchar       *server);
void              gossip_account_set_port             (GossipAccount     *account,
						       guint16            port);
void              gossip_account_set_auto_connect     (GossipAccount     *account,
						       gboolean           auto_connect);
void              gossip_account_set_use_ssl          (GossipAccount     *account,
						       gboolean           use_ssl);
void              gossip_account_set_use_proxy        (GossipAccount     *account,
						       gboolean           use_proxy);

guint             gossip_account_hash                 (gconstpointer      key);
gboolean          gossip_account_equal                (gconstpointer      a,
						       gconstpointer      b);

/* Utils */
const gchar *     gossip_account_type_to_string       (GossipAccountType  type);
GossipAccountType gossip_account_string_to_type       (const gchar       *str);
GdkPixbuf *       gossip_account_type_create_pixbuf   (GossipAccountType  type,
						       GtkIconSize        icon_size);
GdkPixbuf *       gossip_account_create_pixbuf        (GossipAccount     *account,
						       GtkIconSize        icon_size);
GdkPixbuf *       gossip_account_status_create_pixbuf (GossipAccount     *account,
						       GtkIconSize        icon_size,
						       gboolean           online);

#endif /* __GOSSIP_ACCOUNT_H__ */
