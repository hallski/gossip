/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2006 Imendio AB
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
 * Authors: Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __GOSSIP_ACCOUNT_H__
#define __GOSSIP_ACCOUNT_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_ACCOUNT         (gossip_account_get_gtype ())
#define GOSSIP_ACCOUNT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_ACCOUNT, GossipAccount))
#define GOSSIP_ACCOUNT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_ACCOUNT, GossipAccountClass))
#define GOSSIP_IS_ACCOUNT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_ACCOUNT))
#define GOSSIP_IS_ACCOUNT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_ACCOUNT))
#define GOSSIP_ACCOUNT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_ACCOUNT, GossipAccountClass))

typedef struct _GossipAccount      GossipAccount;
typedef struct _GossipAccountClass GossipAccountClass;

struct _GossipAccount {
	GObject parent;
};

struct _GossipAccountClass {
	GObjectClass parent_class;
};

typedef enum {
	GOSSIP_ACCOUNT_PARAM_FLAG_REQUIRED    = 1 << 0,
	GOSSIP_ACCOUNT_PARAM_FLAG_REGISTER    = 1 << 1,
	GOSSIP_ACCOUNT_PARAM_FLAG_HAS_DEFAULT = 1 << 2,
	GOSSIP_ACCOUNT_PARAM_FLAG_ALL         = (1 << 3) - 1
} GossipAccountParamFlags;

typedef struct {
	GossipAccountParamFlags flags;
	GValue                  g_value;
	gboolean                modified;
} GossipAccountParam;

typedef enum {
	GOSSIP_ACCOUNT_TYPE_JABBER,
	GOSSIP_ACCOUNT_TYPE_AIM,
	GOSSIP_ACCOUNT_TYPE_ICQ,
	GOSSIP_ACCOUNT_TYPE_MSN,
	GOSSIP_ACCOUNT_TYPE_YAHOO,
	GOSSIP_ACCOUNT_TYPE_UNKNOWN,
	GOSSIP_ACCOUNT_TYPE_COUNT
} GossipAccountType;

typedef void (*GossipAccountParamFunc) (GossipAccount      *account,
					const gchar        *param_name,
					GossipAccountParam *param,
					gpointer            user_data);

GType             gossip_account_get_gtype         (void) G_GNUC_CONST;
void              gossip_account_param_new         (GossipAccount           *account,
						    const gchar             *first_param_name,
						    ...);
void              gossip_account_param_new_g_value (GossipAccount           *account,
						    const gchar             *param_name,
						    const GValue            *g_value,
						    GossipAccountParamFlags  flags);
void              gossip_account_param_set         (GossipAccount           *account,
						    const gchar             *first_param_name,
						    ...);
void              gossip_account_param_set_g_value (GossipAccount           *account,
						    const gchar             *param_name,
						    const GValue            *g_value);
void              gossip_account_param_get         (GossipAccount           *account,
						    const gchar             *first_param_name,
						    ...);
const GValue *    gossip_account_param_get_g_value (GossipAccount           *account,
						    const gchar             *param_name);
gboolean          gossip_account_has_param         (GossipAccount           *account,
						    const gchar             *param_name);
GList *           gossip_account_param_get_all     (GossipAccount           *account);
GossipAccountParam *
                  gossip_account_param_get_param   (GossipAccount           *account,
						    const gchar             *param_name);
void              gossip_account_param_foreach     (GossipAccount           *account,
						    GossipAccountParamFunc   callback,
						    gpointer                 user_data);
GossipAccountType gossip_account_get_type          (GossipAccount           *account);
const gchar *     gossip_account_get_id            (GossipAccount           *account);
const gchar *     gossip_account_get_name          (GossipAccount           *account);
gboolean          gossip_account_get_auto_connect  (GossipAccount           *account);
gboolean          gossip_account_get_use_proxy     (GossipAccount           *account);
void              gossip_account_set_id            (GossipAccount           *account,
						    const gchar             *id);
void              gossip_account_set_name          (GossipAccount           *account,
						    const gchar             *name);
void              gossip_account_set_auto_connect  (GossipAccount           *account,
						    gboolean                 auto_connect);
void              gossip_account_set_use_proxy     (GossipAccount           *account,
						    gboolean                 use_proxy);
#ifdef USE_TELEPATHY
const gchar *     gossip_account_get_protocol      (GossipAccount           *account);
const gchar *     gossip_account_get_cmgr_name     (GossipAccount           *account);
void              gossip_account_set_protocol      (GossipAccount           *account,
						    const gchar             *protocol);
void              gossip_account_set_cmgr_name     (GossipAccount           *account,
						    const gchar             *cmgr_name);
#endif
guint             gossip_account_hash              (gconstpointer            key);
gboolean          gossip_account_equal             (gconstpointer            v1,
						    gconstpointer            v2);
const gchar *     gossip_account_type_to_string    (GossipAccountType        type);
GossipAccountType gossip_account_string_to_type    (const gchar             *str);

G_END_DECLS

#endif /* __GOSSIP_ACCOUNT_H__ */
