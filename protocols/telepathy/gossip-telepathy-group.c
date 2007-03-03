/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Xavier Claessens <xclaesse@gmail.com>
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

#include <dbus/dbus-glib.h>
#include <libtelepathy/tp-chan.h>
#include <libtelepathy/tp-chan-iface-group-gen.h>

#include <libgossip/gossip-debug.h>

#include "gossip-telepathy-group.h"
#include "gossip-telepathy-marshal.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       GOSSIP_TYPE_TELEPATHY_GROUP, GossipTelepathyGroupPriv))

#define DEBUG_DOMAIN "TelepathyGroup"

struct _GossipTelepathyGroupPriv {
	GossipTelepathy *telepathy;
	TpChan          *tp_chan;
	DBusGProxy      *group_iface;
	gchar           *group_name;
};

static void gossip_telepathy_group_class_init    (GossipTelepathyGroupClass *klass);
static void gossip_telepathy_group_init          (GossipTelepathyGroup      *group);
static void telepathy_group_destroy_cb           (DBusGProxy                *proxy,
						  GossipTelepathyGroup      *group);
static void telepathy_group_finalize             (GObject                   *object);
static void telepathy_group_members_changed_cb   (DBusGProxy                *group_iface,
						  gchar                     *message,
						  GArray                    *added,
						  GArray                    *removed,
						  GArray                    *local_pending,
						  GArray                    *remote_pending,
						  guint                      actor,
						  guint                      reason,
						  GossipTelepathyGroup      *group);

enum {
	MEMBERS_ADDED,
	MEMBERS_REMOVED,
	LOCAL_PENDING,
	REMOTE_PENDING,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (GossipTelepathyGroup, gossip_telepathy_group, G_TYPE_OBJECT);

static void
gossip_telepathy_group_class_init (GossipTelepathyGroupClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = telepathy_group_finalize;

	signals[MEMBERS_ADDED] =
		g_signal_new ("members-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_telepathy_marshal_VOID__POINTER_UINT_UINT_STRING,
			      G_TYPE_NONE,
			      4, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING);

	signals[MEMBERS_REMOVED] =
		g_signal_new ("members-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_telepathy_marshal_VOID__POINTER_UINT_UINT_STRING,
			      G_TYPE_NONE,
			      4, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING);

	signals[LOCAL_PENDING] =
		g_signal_new ("local-pending",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_telepathy_marshal_VOID__POINTER_UINT_UINT_STRING,
			      G_TYPE_NONE,
			      4, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING);

	signals[REMOTE_PENDING] =
		g_signal_new ("remote-pending",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_telepathy_marshal_VOID__POINTER_UINT_UINT_STRING,
			      G_TYPE_NONE,
			      4, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof (GossipTelepathyGroupPriv));
}

static void
gossip_telepathy_group_init (GossipTelepathyGroup *group)
{
}

static void
telepathy_group_finalize (GObject *object)
{
	GossipTelepathyGroupPriv *priv;

	priv = GET_PRIV (object);

	if (priv->group_iface) {
		g_signal_handlers_disconnect_by_func (priv->group_iface,
						      G_CALLBACK (telepathy_group_destroy_cb),
						      object);
		dbus_g_proxy_disconnect_signal (priv->group_iface, "MembersChanged",
						G_CALLBACK (telepathy_group_members_changed_cb),
						object);
	}

	g_object_unref (priv->tp_chan);
	g_free (priv->group_name);

	G_OBJECT_CLASS (gossip_telepathy_group_parent_class)->finalize (object);
}

GossipTelepathyGroup *
gossip_telepathy_group_new (GossipTelepathy *telepathy,
			    TpChan          *tp_chan)
{
	GossipTelepathyGroup     *group;
	GossipTelepathyGroupPriv *priv;
	DBusGProxy               *group_iface;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY (telepathy), NULL);
	g_return_val_if_fail (TELEPATHY_IS_CHAN (tp_chan), NULL);

	group_iface = tp_chan_get_interface (tp_chan,
					     TELEPATHY_CHAN_IFACE_GROUP_QUARK);
	g_return_val_if_fail (group_iface != NULL, NULL);

	group = g_object_new (GOSSIP_TYPE_TELEPATHY_GROUP, NULL);
	priv = GET_PRIV (group);

	priv->telepathy = telepathy;
	priv->tp_chan = g_object_ref (tp_chan);
	priv->group_iface = group_iface;

	g_signal_connect (priv->group_iface, "destroy",
			  G_CALLBACK (telepathy_group_destroy_cb),
			  group);

	return group;
}

void
gossip_telepathy_group_setup (GossipTelepathyGroup *group)
{
	GossipTelepathyGroupPriv *priv;
	GArray                   *members;
	GArray                   *local_pending;
	GArray                   *remote_pending;
	GError                   *error = NULL;

	g_return_if_fail (GOSSIP_IS_TELEPATHY_GROUP (group));

	priv = GET_PRIV (group);

	dbus_g_proxy_connect_signal (priv->group_iface, "MembersChanged",
				     G_CALLBACK (telepathy_group_members_changed_cb),
				     group, NULL);

	if (!tp_chan_iface_group_get_all_members (priv->group_iface,
						  &members,
						  &local_pending,
						  &remote_pending,
						  &error)) {
		gossip_debug (DEBUG_DOMAIN, "Couldn't get all members: %s",
			      error->message);
		g_clear_error (&error);
		return;
	}

	if (members->len > 0) {
		g_signal_emit (group, signals[MEMBERS_ADDED], 0, 
		               members, 0,
		               TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
		               NULL);
	}
	if (remote_pending->len > 0) {
		g_signal_emit (group, signals[REMOTE_PENDING], 0, 
		               remote_pending, 0,
		               TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
		               NULL);
	}
	if (local_pending->len > 0) {
		GPtrArray *info;

		if (tp_chan_iface_group_get_local_pending_members_with_info (priv->group_iface,
									     &info,
									     &error)) {
			GArray *pending; 
			gint    i;

			pending = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);
			for (i = 0; info->len > i; i++) {
				GValueArray *pending_struct;
				guint        member;
				guint        invitor;
				guint        reason;
				const gchar *message;

				pending_struct = g_ptr_array_index (info, i);
				member = g_value_get_uint (g_value_array_get_nth (pending_struct, 0));
				invitor = g_value_get_uint (g_value_array_get_nth (pending_struct, 1));
				reason = g_value_get_uint (g_value_array_get_nth (pending_struct, 2));
				message = g_value_get_string (g_value_array_get_nth (pending_struct, 3));

				g_array_insert_val (pending, 0, member);

				g_signal_emit (group, signals[LOCAL_PENDING], 0, 
				               pending, invitor, reason, message);

				g_value_array_free (pending_struct);
			}

			g_ptr_array_free (info, TRUE);
			g_array_free (pending, TRUE);
		} else {
			gossip_debug (DEBUG_DOMAIN, "GetLocalPendingMembersWithInfo failed: %s",
				      error->message);
			g_clear_error (&error);

			g_signal_emit (group, signals[LOCAL_PENDING], 0, 
			               local_pending, 0, 
			               TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
			               NULL);
		}
	}
 
	g_array_free (members, TRUE);
	g_array_free (local_pending, TRUE);
	g_array_free (remote_pending, TRUE);
}

void
gossip_telepathy_group_add_members (GossipTelepathyGroup *group,
				    GArray               *handles,
				    const gchar          *message)
{
	GossipTelepathyGroupPriv *priv;
	GError                   *error = NULL;

	g_return_if_fail (GOSSIP_IS_TELEPATHY_GROUP (group));
	g_return_if_fail (handles != NULL);

	priv = GET_PRIV (group);

	if (!tp_chan_iface_group_add_members (priv->group_iface,
					      handles,
					      message,
					      &error)) {
		gossip_debug (DEBUG_DOMAIN, "Failed to add members: %s",
			      error->message);
		g_clear_error (&error);
	}
}

void
gossip_telepathy_group_add_member (GossipTelepathyGroup *group,
				   guint                 handle,
				   const gchar          *message)
{
	GArray *handles;

	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, handle);

	gossip_telepathy_group_add_members (group, handles, message);

	g_array_free (handles, TRUE);
}

void
gossip_telepathy_group_remove_members (GossipTelepathyGroup *group,
				       GArray               *handles,
				       const gchar          *message)
{
	GossipTelepathyGroupPriv *priv;
	GError                   *error = NULL;

	g_return_if_fail (GOSSIP_IS_TELEPATHY_GROUP (group));

	priv = GET_PRIV (group);

	if (!tp_chan_iface_group_remove_members (priv->group_iface,
						 handles,
						 message,
						 &error)) {
		gossip_debug (DEBUG_DOMAIN, "Failed to remove members: %s",
			      error->message);
		g_clear_error (&error);
	}
}

void
gossip_telepathy_group_remove_member (GossipTelepathyGroup *group,
				      guint                 handle,
				      const gchar          *message)
{
	GArray *handles;

	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, handle);

	gossip_telepathy_group_remove_members (group, handles, message);

	g_array_free (handles, TRUE);
}

GArray *
gossip_telepathy_group_get_members (GossipTelepathyGroup *group)
{
	GossipTelepathyGroupPriv *priv;
	GArray                   *members;
	GError                   *error = NULL;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY_GROUP (group), NULL);

	priv = GET_PRIV (group);

	if (!tp_chan_iface_group_get_members (priv->group_iface,
					      &members,
					      &error)) {
		gossip_debug (DEBUG_DOMAIN, "Couldn't get members: %s",
			      error->message);
		g_clear_error (&error);
		return NULL;
	}

	return members;
}

static void
telepathy_group_destroy_cb (DBusGProxy           *proxy,
			    GossipTelepathyGroup *group)
{
	GossipTelepathyGroupPriv *priv;

	priv = GET_PRIV (group);

	priv->group_iface = NULL;
}

static void
telepathy_group_members_changed_cb (DBusGProxy           *group_iface,
				    gchar                *message,
				    GArray               *added,
				    GArray               *removed,
				    GArray               *local_pending,
				    GArray               *remote_pending,
				    guint                 actor,
				    guint                 reason,
				    GossipTelepathyGroup *group)
{
	GossipTelepathyGroupPriv *priv;

	priv = GET_PRIV (group);

	/* emit signals */
	if (added->len > 0) {
		g_signal_emit (group, signals[MEMBERS_ADDED], 0, 
		               added, actor, reason, message);
	}
	if (removed->len > 0) {
		g_signal_emit (group, signals[MEMBERS_REMOVED], 0, 
		               removed, actor, reason, message);
	}
	if (local_pending->len > 0) {
		g_signal_emit (group, signals[LOCAL_PENDING], 0,
		               local_pending, actor, reason, message);
	}
	if (remote_pending->len > 0) {
		g_signal_emit (group, signals[REMOTE_PENDING], 0,
		               remote_pending, actor, reason, message);
	}
}

const gchar *
gossip_telepathy_group_get_name (GossipTelepathyGroup *group)
{
	TpConn              *tp_conn;
	TelepathyHandleType  handle_type;
	guint                channel_handle;
	GArray              *group_handles;
	gchar              **group_names;
	GError              *error = NULL;

	GossipTelepathyGroupPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY_GROUP (group), NULL);

	priv = GET_PRIV (group);

	/* Lazy initialisation */
	if (priv->group_name) {
		return priv->group_name;
	}

	if (!tp_chan_get_handle (DBUS_G_PROXY (priv->tp_chan),
				 &handle_type,
				 &channel_handle,
				 &error)) {
		gossip_debug (DEBUG_DOMAIN, "Couldn't retreive channel handle for group: %s",
			      error->message);
		g_clear_error (&error);
		return NULL;
	}

	group_handles = g_array_new (FALSE, FALSE, sizeof (gint));
	g_array_append_val (group_handles, channel_handle);
	tp_conn = gossip_telepathy_get_connection (priv->telepathy);
	if (!tp_conn_inspect_handles (DBUS_G_PROXY (tp_conn),
				      handle_type,
				      group_handles,
				      &group_names,
				      &error)) {
		gossip_debug (DEBUG_DOMAIN, "Couldn't get group name: %s",
			      error->message);
		g_clear_error (&error);
		g_array_free (group_handles, TRUE);
		return NULL;
	}

	priv->group_name = *group_names;
	g_array_free (group_handles, TRUE);
	g_free (group_names);

	return priv->group_name;
}

guint 
gossip_telepathy_group_get_self_handle (GossipTelepathyGroup *group)
{
	GossipTelepathyGroupPriv *priv;
	guint                     handle;
	GError                   *error = NULL;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY_GROUP (group), 0 );

	priv = GET_PRIV (group);

	if (!tp_chan_iface_group_get_self_handle (priv->group_iface, &handle, &error)) {
		gossip_debug (DEBUG_DOMAIN, "Failed to get self handle: %s",
		              error->message);
		g_clear_error (&error);
		return 0;
	}

	return handle;
}

const gchar *
gossip_telepathy_group_get_object_path (GossipTelepathyGroup *group)
{
	GossipTelepathyGroupPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY_GROUP (group), NULL);

	priv = GET_PRIV (group);

	return dbus_g_proxy_get_path (DBUS_G_PROXY (priv->tp_chan));
}

gboolean
gossip_telepathy_group_is_member (GossipTelepathyGroup *group,
				  guint                 handle)
{
	GArray   *members;
	guint     i;
	gboolean  found = FALSE;

	members = gossip_telepathy_group_get_members (group);
	for (i = 0; i < members->len; i++) {
		if (g_array_index (members, guint, i) == handle) {
			found = TRUE;
			break;
		}
	}
	g_array_free (members, TRUE);
	
	return found;
}


