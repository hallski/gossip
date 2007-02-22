/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Eitan Isaacson <eitan@ascender.com>
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

#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-chan.h>
#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-chan-type-contact-list-gen.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-protocol.h>

#include "gossip-telepathy-contact-list.h"
#include "gossip-telepathy-group.h"
#include "gossip-telepathy-private.h"

#define DEBUG_DOMAIN "TelepathyContactList"

typedef enum {
	TELEPATHY_LIST_TYPE_UNKNOWN,
	TELEPATHY_LIST_TYPE_PUBLISH,
	TELEPATHY_LIST_TYPE_SUBSCRIBE,
	TELEPATHY_LIST_TYPE_KNOWN,
	TELEPATHY_LIST_TYPE_COUNT
} TelepathyListType;

struct _GossipTelepathyContactList {
	GossipTelepathy      *telepathy;

	GossipTelepathyGroup *known;
	GossipTelepathyGroup *publish;
	GossipTelepathyGroup *subscribe;

	GHashTable           *groups;
};

static void              telepathy_contact_list_disconnected_cb          (GossipProtocol             *telepathy,
									  GossipAccount              *account,
									  gint                        reason,
									  GossipTelepathyContactList *list);
static TelepathyListType telepathy_contact_list_get_type                 (GossipTelepathyContactList *list,
									  TpChan                     *list_chan);
static void              telepathy_contact_list_contact_added_cb         (GossipTelepathyGroup       *group,
									  GArray                     *handles,
									  GossipTelepathyContactList *list);
static void              telepathy_contact_list_contact_removed_cb       (GossipTelepathyGroup       *group,
									  GArray                     *handles,
									  GossipTelepathyContactList *list);
static void              telepathy_contact_list_local_pending_cb         (GossipTelepathyGroup       *group,
									  GArray                     *handles,
									  GossipTelepathyContactList *list);
static void              telepathy_contact_list_contact_remove_foreach   (gchar                      *object_path,
									  GossipTelepathyGroup       *group,
									  gpointer                    handle);
static GossipTelepathyGroup *
			 telepathy_contact_list_get_group                (GossipTelepathyContactList *list,
									  const gchar                *name);
static void              telepathy_contact_list_get_groups_foreach       (gchar                      *key,
									  GossipTelepathyGroup       *group,
									  GList                     **groups);
static void              telepathy_contact_list_group_channel_closed_cb  (TpChan                     *channel,
									  GossipTelepathyContactList *list);
static void              telepathy_contact_list_group_members_added_cb   (GossipTelepathyGroup       *group,
									  GArray                     *members,
									  GossipTelepathyContactList *list);
static void              telepathy_contact_list_group_members_removed_cb (GossipTelepathyGroup       *group,
									  GArray                     *members,
									  GossipTelepathyContactList *list);
static gboolean          telepathy_contact_list_find_group               (gchar                      *key,
									  GossipTelepathyGroup       *group,
									  gchar                      *group_name);

GossipTelepathyContactList *
gossip_telepathy_contact_list_init (GossipTelepathy *telepathy)
{
	GossipTelepathyContactList *list;

	list = g_new0 (GossipTelepathyContactList, 1);
	list->telepathy = telepathy;
	list->groups = g_hash_table_new_full (g_str_hash,
					      g_str_equal,
					      (GDestroyNotify) g_free,
					      (GDestroyNotify) g_object_unref);

	g_signal_connect (telepathy, "disconnected",
			  G_CALLBACK (telepathy_contact_list_disconnected_cb),
			  list);

	return list;
}

static void
telepathy_contact_list_disconnected_cb (GossipProtocol             *telepathy,
					GossipAccount              *account,
					gint                        reason,
					GossipTelepathyContactList *list)
{
	g_hash_table_remove_all (list->groups);
	if (list->known) {
		g_object_unref (list->known);
		list->known = NULL;
	}

	if (list->subscribe) {
		g_object_unref (list->subscribe);
		list->subscribe = NULL;
	}

	if (list->publish) {
		g_object_unref (list->publish);
		list->publish = NULL;
	}
}

void
gossip_telepathy_contact_list_finalize (GossipTelepathyContactList *list)
{
	g_return_if_fail (list != NULL);

	if (list->known) {
		g_object_unref (list->known);
	}

	if (list->subscribe) {
		g_object_unref (list->subscribe);
	}

	if (list->publish) {
		g_object_unref (list->publish);
	}

	g_hash_table_destroy (list->groups);
	g_free (list);
}

void
gossip_telepathy_contact_list_add (GossipTelepathyContactList *list,
				   const gchar                *id,
				   const gchar                *message)
{
	GossipTelepathyContacts *contacts;
	guint                    handle;

	g_return_if_fail (list != NULL);
	g_return_if_fail (id != NULL);

	contacts = gossip_telepathy_get_contacts (list->telepathy);
	handle = gossip_telepathy_contacts_get_handle (contacts, id);

	gossip_telepathy_group_add_member (list->subscribe, handle, message);
}

void
gossip_telepathy_contact_list_remove (GossipTelepathyContactList *list,
				      guint                       handle)
{
	g_return_if_fail (list != NULL);

	gossip_telepathy_group_remove_member (list->subscribe, handle, "");
	gossip_telepathy_group_remove_member (list->publish, handle, "");
	gossip_telepathy_group_remove_member (list->known, handle, "");
}

void
gossip_telepathy_contact_list_set_subscription (GossipTelepathyContactList *list,
						guint                       handle,
						gboolean                    subscribed)
{
	g_return_if_fail (list != NULL);

	gossip_debug (DEBUG_DOMAIN, "Setting subscription for contact:'%d' as %s",
		      handle, subscribed ? "subscribed" : "unsubscribed");

	if (subscribed) {
		gossip_telepathy_group_add_member (list->publish, handle, "");
	} else {
		gossip_telepathy_group_remove_member (list->publish, handle, "");
	}
}

void
gossip_telepathy_contact_list_newchannel (GossipTelepathyContactList *list,
					  TpChan                     *new_chan)
{
	GossipTelepathyGroup *group;

	g_return_if_fail (list != NULL);

	gossip_debug (DEBUG_DOMAIN, "new contact list channel: handle_type: %d",
		      new_chan->handle_type);

	if (new_chan->handle_type == TP_CONN_HANDLE_TYPE_LIST) {
		TelepathyListType list_type;

		list_type = telepathy_contact_list_get_type (list, new_chan);
		if (list_type == TELEPATHY_LIST_TYPE_UNKNOWN) {
			gossip_debug (DEBUG_DOMAIN, "unknown list type");
			return;
		}

		group = gossip_telepathy_group_new (list->telepathy, new_chan);

		switch (list_type) {
		case TELEPATHY_LIST_TYPE_KNOWN:
			if (list->known) {
				g_object_unref (list->known);
			}
			list->known = group;
			break;
		case TELEPATHY_LIST_TYPE_PUBLISH:
			if (list->publish) {
				g_object_unref (list->publish);
			}
			list->publish = group;
			break;
		case TELEPATHY_LIST_TYPE_SUBSCRIBE:
			if (list->subscribe) {
				g_object_unref (list->subscribe);
			}
			list->subscribe = group;
			break;
		default:
			g_assert_not_reached ();
		}

		/* Connect and setup the new contact-list group */
		if (list_type == TELEPATHY_LIST_TYPE_KNOWN ||
		    list_type == TELEPATHY_LIST_TYPE_SUBSCRIBE) {
			g_signal_connect (group, "members-added",
					  G_CALLBACK (telepathy_contact_list_contact_added_cb),
					  list);
			g_signal_connect (group, "members-removed",
					  G_CALLBACK (telepathy_contact_list_contact_removed_cb),
					  list);
		}
		if (list_type == TELEPATHY_LIST_TYPE_PUBLISH) {
			g_signal_connect (group, "local-pending",
					  G_CALLBACK (telepathy_contact_list_local_pending_cb),
					  list);
		}
		gossip_telepathy_group_setup (group);
	}
	else if (new_chan->handle_type == TP_CONN_HANDLE_TYPE_GROUP) {
		const gchar *object_path;

		object_path = dbus_g_proxy_get_path (DBUS_G_PROXY (new_chan));
		if (g_hash_table_lookup (list->groups, object_path)) {
			return;
		}

		gossip_debug (DEBUG_DOMAIN, "Server-Side group Channel");

		group = gossip_telepathy_group_new (list->telepathy, new_chan);

		dbus_g_proxy_connect_signal (DBUS_G_PROXY (new_chan),
					     "Closed",
					     G_CALLBACK
					     (telepathy_contact_list_group_channel_closed_cb),
					     list,
					     NULL);

		g_hash_table_insert (list->groups, g_strdup (object_path), group);
		g_signal_connect (group, "members-added",
				  G_CALLBACK (telepathy_contact_list_group_members_added_cb),
				  list);
		g_signal_connect (group, "members-removed",
				  G_CALLBACK (telepathy_contact_list_group_members_removed_cb),
				  list);

		gossip_telepathy_group_setup (group);
	}
}

static TelepathyListType
telepathy_contact_list_get_type (GossipTelepathyContactList *list,
				 TpChan                     *list_chan)
{
	TpConn              *tp_conn;
	GArray              *handles;
	gchar              **handle_name;
	TelepathyListType    list_type;
	GError              *error = NULL;

	tp_conn = gossip_telepathy_get_connection (list->telepathy);
	handles = g_array_new (FALSE, FALSE, sizeof (gint));
	g_array_append_val (handles, list_chan->handle);

	if (!tp_conn_inspect_handles (DBUS_G_PROXY (tp_conn),
				      TP_CONN_HANDLE_TYPE_LIST,
				      handles,
				      &handle_name,
				      &error)) {
		gossip_debug (DEBUG_DOMAIN, "InspectHandle Error: %s",
			      error->message);
		g_clear_error (&error);
		g_array_free (handles, TRUE);
		return 0;
	}

	if (strcmp (*handle_name, "subscribe") == 0) {
		list_type = TELEPATHY_LIST_TYPE_SUBSCRIBE;
	} else if (strcmp (*handle_name, "publish") == 0) {
		list_type = TELEPATHY_LIST_TYPE_PUBLISH;
	} else if (strcmp (*handle_name, "known") == 0) {
		list_type = TELEPATHY_LIST_TYPE_KNOWN;
	} else {
		list_type = TELEPATHY_LIST_TYPE_UNKNOWN;
	}

	gossip_debug (DEBUG_DOMAIN, "list type: %s (%d)",
		      *handle_name, list_type);

	g_strfreev (handle_name);
	g_array_free (handles, TRUE);

	return list_type;
}

static void
telepathy_contact_list_contact_added_cb (GossipTelepathyGroup       *group,
					 GArray                     *handles,
					 GossipTelepathyContactList *list)
{
	GossipTelepathyContacts *contacts;
	GList                   *added_list, *l;
	GossipContactType        contact_type;

	contacts = gossip_telepathy_get_contacts (list->telepathy);
	added_list = gossip_telepathy_contacts_get_from_handles (contacts, handles);

	for (l = added_list; l; l = l->next) {
		GossipContact *contact;

		contact = GOSSIP_CONTACT (l->data);
		gossip_contact_set_subscription (contact, GOSSIP_SUBSCRIPTION_BOTH);
		g_object_get (contact, "type", &contact_type, NULL);

		if (contact_type == GOSSIP_CONTACT_TYPE_TEMPORARY) {
			g_object_set (contact, "type",
				      GOSSIP_CONTACT_TYPE_CONTACTLIST,
				      NULL);
			g_signal_emit_by_name (list->telepathy, "contact-added",
					       contact);
		}
	}

	g_list_free (added_list);
}

static void
telepathy_contact_list_contact_removed_cb (GossipTelepathyGroup       *group,
					   GArray                     *handles,
					   GossipTelepathyContactList *list)
{
	GossipTelepathyContacts *contacts;
	guint i;

	contacts = gossip_telepathy_get_contacts (list->telepathy);

	for (i = 0; i < handles->len; i++) {
		guint          handle;
		GossipContact *contact;

		handle = g_array_index (handles, guint, i);
		contact = gossip_telepathy_contacts_get_from_handle (contacts,
								    handle);
		if (contact == NULL) {
			continue;
		}

		g_object_set (contact,
			      "type", GOSSIP_CONTACT_TYPE_TEMPORARY,
			      NULL);

		g_signal_emit_by_name (list->telepathy, "contact-removed",
				       contact);
	}
}

static void
telepathy_contact_list_local_pending_cb (GossipTelepathyGroup       *group,
					 GArray                     *handles,
					 GossipTelepathyContactList *list)
{
	GossipTelepathyContacts *contacts;
	guint i;

	contacts = gossip_telepathy_get_contacts (list->telepathy);

	for (i = 0; i < handles->len; i++) {
		guint          handle;
		GossipContact *contact;

		handle = g_array_index (handles, guint, i);
		contact = gossip_telepathy_contacts_get_from_handle (contacts,
								    handle);
		if (!contact) {
			continue;
		}

		g_signal_emit_by_name (list->telepathy, "subscription-request",
				       contact, NULL);
	}
}

void
gossip_telepathy_contact_list_contact_update (GossipTelepathyContactList *list,
					      guint                       contact_handle,
					      GList                      *groups)
{
	GList *l;

	g_return_if_fail (list != NULL);

	/* FIXME: Here we remove the contact from all its group, then add the new ones back.
	 * This can be potentially dangerous since if we fail after the removal, the contact
	 * looses all his previous groups. Do we accept this ? */
	g_hash_table_foreach (list->groups,
			      (GHFunc) telepathy_contact_list_contact_remove_foreach,
			      GINT_TO_POINTER (contact_handle));

	for (l = groups; l; l = l->next) {
		GossipTelepathyGroup *group;

		gossip_debug (DEBUG_DOMAIN, "Adding contact %d to group: %s",
			      contact_handle, l->data);

		group = telepathy_contact_list_get_group (list, l->data);
		if (group) {
			gossip_telepathy_group_add_member (group,
							   contact_handle, "");
		}
	}
}

void
gossip_telepathy_contact_list_rename_group (GossipTelepathyContactList *list,
					    const gchar                *group_name,
					    const gchar                *new_name)
{
	GossipTelepathyGroup *group;
	GArray               *members;

	g_return_if_fail (list != NULL);

	group = g_hash_table_find (list->groups,
				   (GHRFunc) telepathy_contact_list_find_group,
				   (gchar*) group_name);
	if (!group) {
		/* The group doesn't exists on this account */
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "rename group %s to %s", group_name, new_name);

	/* Remove all members from the old group */
	members = gossip_telepathy_group_get_members (group);
	gossip_telepathy_group_remove_members (group, members, "");
	telepathy_contact_list_group_members_removed_cb (group, members, list);
	g_hash_table_remove (list->groups,
			     gossip_telepathy_group_get_object_path (group));

	/* Add all members to the new group */
	group = telepathy_contact_list_get_group (list, new_name);
	if (group) {
		gossip_telepathy_group_add_members (group, members, "");
	}
}

GList *
gossip_telepathy_contact_list_get_groups (GossipTelepathyContactList *list)
{
	GList *group_list = NULL;

	g_return_val_if_fail (list != NULL, NULL);

	g_hash_table_foreach (list->groups,
			      (GHFunc) telepathy_contact_list_get_groups_foreach,
			      &group_list);

	group_list = g_list_sort (group_list, (GCompareFunc) strcmp);

	return group_list;
}

static void
telepathy_contact_list_contact_remove_foreach (gchar                *object_path,
					       GossipTelepathyGroup *group,
					       gpointer              handle)
{
	gossip_telepathy_group_remove_member (group, GPOINTER_TO_UINT (handle), "");
}

static GossipTelepathyGroup *
telepathy_contact_list_get_group (GossipTelepathyContactList *list,
				  const gchar                *name)
{
	GossipAccount        *account;
	TpConn               *tp_conn;
	guint                 group_handle;
	TpChan               *group_channel;
	char                 *group_object_path;
	const char           *names[2] = {name, NULL};
	GArray               *handles;
	GossipTelepathyGroup *group;
	GError               *error = NULL;

	group = g_hash_table_find (list->groups,
				   (GHRFunc) telepathy_contact_list_find_group,
				   (gchar*) name);
	if (group) {
		return group;
	}

	gossip_debug (DEBUG_DOMAIN, "creating new group: %s", name);

	tp_conn = gossip_telepathy_get_connection (list->telepathy);
	account = gossip_telepathy_get_account (list->telepathy);

	if (!tp_conn_request_handles (DBUS_G_PROXY (tp_conn),
				      TP_CONN_HANDLE_TYPE_GROUP,
				      names,
				      &handles,
				      &error)) {
		gossip_debug (DEBUG_DOMAIN, "Couldn't request the creation of a new handle for group: %s",
			      error->message);
		g_clear_error (&error);
		return NULL;
	}
	group_handle = g_array_index (handles, guint, 0);
	g_array_free (handles, TRUE);

	if (!tp_conn_request_channel (DBUS_G_PROXY (tp_conn),
				      TP_IFACE_CHANNEL_TYPE_CONTACT_LIST,
				      TP_CONN_HANDLE_TYPE_GROUP,
				      group_handle,
				      FALSE,
				      &group_object_path,
				      &error)) {
		gossip_debug (DEBUG_DOMAIN, "Couldn't request the creation of a new group channel: %s",
			      error->message);
		g_clear_error (&error);
		return NULL;
	}

	group_channel = tp_chan_new (tp_get_bus (),
				     dbus_g_proxy_get_bus_name (DBUS_G_PROXY (tp_conn)),
				     group_object_path,
				     TP_IFACE_CHANNEL_TYPE_CONTACT_LIST,
				     TP_CONN_HANDLE_TYPE_GROUP,
				     group_handle);

	dbus_g_proxy_connect_signal (DBUS_G_PROXY (group_channel),
				     "Closed",
				     G_CALLBACK
				     (telepathy_contact_list_group_channel_closed_cb),
				     list,
				     NULL);

	group = gossip_telepathy_group_new (list->telepathy,
					    group_channel);
	g_hash_table_insert (list->groups, group_object_path, group);
	g_signal_connect (group, "members-added",
			  G_CALLBACK (telepathy_contact_list_group_members_added_cb),
			  list);
	g_signal_connect (group, "members-removed",
			  G_CALLBACK (telepathy_contact_list_group_members_removed_cb),
			  list);
	gossip_telepathy_group_setup (group);

	return group;
}

static gboolean
telepathy_contact_list_find_group (gchar                 *key,
				   GossipTelepathyGroup  *group,
				   gchar                 *group_name)
{
	if (strcmp (group_name, gossip_telepathy_group_get_name (group)) == 0) {
		return TRUE;
	}

	return FALSE;
}

static void
telepathy_contact_list_get_groups_foreach (gchar                 *key,
					   GossipTelepathyGroup  *group,
					   GList                **groups)
{
	*groups = g_list_append (*groups,
				 g_strdup (gossip_telepathy_group_get_name (group)));
}

static void
telepathy_contact_list_group_channel_closed_cb (TpChan                     *channel,
						GossipTelepathyContactList *list)
{
	g_hash_table_remove (list->groups,
			     dbus_g_proxy_get_path (DBUS_G_PROXY (channel)));
}

static void
telepathy_contact_list_group_members_added_cb (GossipTelepathyGroup       *group,
					       GArray                     *members,
					       GossipTelepathyContactList *list)
{
	GossipTelepathyContacts *contacts;
	GList                   *added_list, *l;
	GList                   *contact_groups;
	const gchar             *group_name;

	contacts = gossip_telepathy_get_contacts (list->telepathy);
	added_list = gossip_telepathy_contacts_get_from_handles (contacts, members);
	group_name = gossip_telepathy_group_get_name (group);

	for (l = added_list; l; l = l->next) {
		GossipContact *contact;

		contact = GOSSIP_CONTACT (l->data);
		contact_groups = gossip_contact_get_groups (contact);

		if (!g_list_find_custom (contact_groups,
					 group_name,
					 (GCompareFunc) strcmp)) {
			gossip_debug (DEBUG_DOMAIN, "Contact %s added to group %s",
				      gossip_contact_get_name (contact),
				      group_name);
			contact_groups = g_list_append (contact_groups,
							g_strdup (group_name));
			gossip_contact_set_groups (contact, contact_groups);
		}
	}
	g_list_free (added_list);
}

static void
telepathy_contact_list_group_members_removed_cb (GossipTelepathyGroup       *group,
						 GArray                     *members,
						 GossipTelepathyContactList *list)
{
	GossipTelepathyContacts *contacts;
	const gchar             *group_name;
	guint                    i;

	group_name = gossip_telepathy_group_get_name (group);
	contacts = gossip_telepathy_get_contacts (list->telepathy);

	for (i = 0; i < members->len; i++) {
		GossipContact *contact;
		guint          handle;
		GList         *contact_groups;
		GList         *to_remove;

		handle = g_array_index (members, guint, i);
		contact = gossip_telepathy_contacts_get_from_handle (contacts,
								     handle);
		contact_groups = gossip_contact_get_groups (contact);
		contact_groups = g_list_copy (contact_groups);

		to_remove = g_list_find_custom (contact_groups,
						group_name,
						(GCompareFunc) strcmp);
		if (to_remove) {
			gossip_debug (DEBUG_DOMAIN, "Contact %d removed from group %s",
				      handle, group_name);
			contact_groups = g_list_remove_link (contact_groups,
							     to_remove);
			gossip_contact_set_groups (contact, contact_groups);
		}

		g_list_free (contact_groups);
	}
}

