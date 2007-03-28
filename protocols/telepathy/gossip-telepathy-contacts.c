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

#include <string.h>

#include <libtelepathy/tp-conn-gen.h>
#include <libtelepathy/tp-conn-iface-aliasing-gen.h>
#include <libtelepathy/tp-conn-iface-presence-gen.h>
#include <libtelepathy/tp-conn-iface-avatars-gen.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-protocol.h>
#include <libgossip/gossip-utils.h>

#include "gossip-telepathy-contacts.h"

#define DEBUG_DOMAIN "TelepathyContacts"
#define MAX_AVATAR_REQUESTS 10

struct _GossipTelepathyContacts {
	GossipTelepathy *telepathy;

	DBusGProxy      *aliasing_iface;
	DBusGProxy      *avatars_iface;
	DBusGProxy      *presence_iface;

	GHashTable      *contacts;

	GList           *avatar_requests_queue;
};

typedef struct {
	GossipTelepathyContacts *contacts;
	guint                    handle;
} TelepathyContactsAvatarRequestData;

typedef struct {
	GossipTelepathyContacts *contacts;
	guint                   *handles;
} TelepathyContactsAliasesRequestData;

static void     telepathy_contacts_disconnecting_cb        (GossipProtocol                       *telepathy,
							    GossipAccount                        *account,
							    GossipTelepathyContacts              *contacts);
void            telepathy_contacts_connected_cb            (GossipProtocol                       *telepathy,
							    GossipAccount                        *account,
							    GossipTelepathyContacts              *contacts);
static void     telepathy_contacts_disconnecting_foreach   (gchar                                *id,
							    GossipContact                        *contact,
							    GossipTelepathyContacts              *contacts);
static gboolean telepathy_contacts_find_foreach            (gchar                                *id,
							    GossipContact                        *contact,
							    guint                                 handle);
static GossipContact *
                telepathy_contacts_new_contact             (GossipTelepathyContacts              *contacts,
							    const gchar                          *id);
static void     telepathy_contacts_get_info                (GossipTelepathyContacts              *contacts,
							    GArray                               *handles);
static void     telepathy_contacts_request_avatar          (GossipTelepathyContacts              *contacts,
							    guint                                 handle);
static void     telepathy_contacts_start_avatar_requests   (GossipTelepathyContacts              *contacts);
static void     telepathy_contacts_avatar_update_cb        (DBusGProxy                           *proxy,
							    guint                                 handle,
							    gchar                                *new_token,
							    GossipTelepathyContacts              *contacts);
static void     telepathy_contacts_request_avatar_cb       (DBusGProxy                           *proxy,
							    GArray                               *avatar_data,
							    char                                 *mime_type,
							    GError                               *error,
							    TelepathyContactsAvatarRequestData   *data);
static void     telepathy_contacts_set_avatar_cb           (DBusGProxy                           *proxy,
							    char                                 *token,
							    GError                               *error,
							    GossipCallbackData                   *data);
static void     telepathy_contacts_aliases_update_cb       (DBusGProxy                           *proxy,
							    GPtrArray                            *renamed_handlers,
							    GossipTelepathyContacts              *contacts);
static void     telepathy_contacts_request_aliases_cb      (DBusGProxy                           *proxy,
							    gchar                               **contact_names,
							    GError                               *error,
							    TelepathyContactsAliasesRequestData  *data);
static void     telepathy_contacts_presence_update_cb      (DBusGProxy                           *proxy,
							    GHashTable                           *handle_table,
							    GossipTelepathyContacts              *contacts);
static void     telepathy_contacts_parse_presence_foreach  (guint                                 contact_handle,
							    GValueArray                          *presence_struct,
							    GossipTelepathyContacts              *contacts);
static void     telepathy_contacts_presences_table_foreach (const gchar                          *state_str,
							    GHashTable                           *presences_table,
							    GossipPresence                      **presence);
static GossipPresenceState
		    telepathy_presence_state_from_str      (const gchar                          *str);
static const gchar *telepathy_presence_state_to_str        (GossipPresenceState                   presence_state);

static guint n_avatar_requests = 0;


GossipTelepathyContacts *
gossip_telepathy_contacts_init (GossipTelepathy *telepathy)
{
	GossipTelepathyContacts *contacts;

	g_return_val_if_fail (GOSSIP_IS_TELEPATHY (telepathy), NULL);

	contacts = g_slice_new0 (GossipTelepathyContacts);
	contacts->telepathy = telepathy;
	contacts->contacts = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    (GDestroyNotify) g_free,
						    (GDestroyNotify) g_object_unref);
	g_signal_connect (telepathy, "disconnecting",
			  G_CALLBACK (telepathy_contacts_disconnecting_cb),
			  contacts);
	g_signal_connect (telepathy, "connected",
			  G_CALLBACK (telepathy_contacts_connected_cb),
			  contacts);

	return contacts;
}

void
gossip_telepathy_contacts_finalize (GossipTelepathyContacts *contacts)
{
	g_return_if_fail (contacts != NULL);

	g_hash_table_destroy (contacts->contacts);
	g_slice_free (GossipTelepathyContacts, contacts);
}

static void
telepathy_contacts_disconnecting_cb (GossipProtocol          *telepathy,
				     GossipAccount           *account,
				     GossipTelepathyContacts *contacts)
{
	g_hash_table_foreach (contacts->contacts,
			      (GHFunc) telepathy_contacts_disconnecting_foreach,
			      contacts);

	contacts->aliasing_iface = NULL;
	contacts->avatars_iface = NULL;
	contacts->presence_iface = NULL;
}

void
telepathy_contacts_connected_cb (GossipProtocol          *telepathy,
				 GossipAccount           *account,
				 GossipTelepathyContacts *contacts)
{
	TpConn *tp_conn;

	tp_conn = gossip_telepathy_get_connection (contacts->telepathy);
	contacts->aliasing_iface = tp_conn_get_interface (tp_conn,
							  TELEPATHY_CONN_IFACE_ALIASING_QUARK);
	contacts->avatars_iface = tp_conn_get_interface (tp_conn,
							 TELEPATHY_CONN_IFACE_AVATARS_QUARK);
	contacts->presence_iface = tp_conn_get_interface (tp_conn,
							  TELEPATHY_CONN_IFACE_PRESENCE_QUARK);

	if (contacts->aliasing_iface) {
		dbus_g_proxy_connect_signal (contacts->aliasing_iface,
					     "AliasesChanged",
					     G_CALLBACK (telepathy_contacts_aliases_update_cb),
					     contacts, NULL);
	}

	if (contacts->avatars_iface) {
		dbus_g_proxy_connect_signal (contacts->avatars_iface,
					     "AvatarUpdated",
					     G_CALLBACK (telepathy_contacts_avatar_update_cb),
					     contacts, NULL);
	}

	if (contacts->presence_iface) {
		dbus_g_proxy_connect_signal (contacts->presence_iface,
					     "PresenceUpdate",
					     G_CALLBACK (telepathy_contacts_presence_update_cb),
					     contacts, NULL);
	}
}

GossipContact *
gossip_telepathy_contacts_find (GossipTelepathyContacts *contacts,
				const gchar             *id)
{
	g_return_val_if_fail (contacts != NULL, NULL);
	g_return_val_if_fail (id != NULL, NULL);

	return g_hash_table_lookup (contacts->contacts, id);
}

GossipContact *
gossip_telepathy_contacts_new (GossipTelepathyContacts *contacts,
			       const gchar             *id,
			       const gchar             *name)
{
	GossipContact *contact;
	guint          handle;

	g_return_val_if_fail (contacts != NULL, NULL);
	g_return_val_if_fail (id != NULL, NULL);

	contact = g_hash_table_lookup (contacts->contacts, id);

	if (contact) {
		return contact;
	}

	if (gossip_telepathy_get_connection (contacts->telepathy)) {
		handle = gossip_telepathy_contacts_get_handle (contacts, id);
		contact = gossip_telepathy_contacts_get_from_handle (contacts, handle);
	} else {
		contact = telepathy_contacts_new_contact (contacts, id);
	}

	if (!G_STR_EMPTY (name)) {
		gossip_contact_set_name (contact, name);
	}

	return contact;
}

guint
gossip_telepathy_contacts_get_handle (GossipTelepathyContacts *contacts,
				      const gchar             *id)
{
	GossipContact *contact;
	TpConn        *tp_conn;
	guint          handle;
	gboolean       success;
	GError        *error = NULL;

	g_return_val_if_fail (contacts != NULL, 0);
	g_return_val_if_fail (id != NULL, 0);

	/* Checks if we already have this contact id */
	contact = g_hash_table_lookup (contacts->contacts, id);
	if (contact) {
		handle = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (contact),
							      "telepathy-handle"));
		if (handle != 0) {
			return handle;
		}
	}

	/* The id is unknown, requests a new handle */
	tp_conn = gossip_telepathy_get_connection (contacts->telepathy);
	contact = gossip_telepathy_get_own_contact (contacts->telepathy);

	if (strcmp (gossip_contact_get_id (contact), id) == 0) {
		success = tp_conn_get_self_handle (DBUS_G_PROXY (tp_conn),
						   &handle, &error);
	} else {
		const gchar *contact_ids[] = {id, NULL};
		GArray      *handles;

		success = tp_conn_request_handles (DBUS_G_PROXY (tp_conn),
						   TP_HANDLE_TYPE_CONTACT,
						   contact_ids, &handles, &error);

		handle = g_array_index(handles, guint, 0);
		g_array_free (handles, TRUE);
	}

	if (!success) {
		gossip_debug (DEBUG_DOMAIN, 
			      "RequestHandle Error: %s, %d",
			      error ? error->message : "No error given",
			      handle);
		g_clear_error (&error);
		return 0;
	}

	return handle;
}

GossipContact *
gossip_telepathy_contacts_get_from_handle (GossipTelepathyContacts *contacts,
					   guint                    handle)
{
	GossipContact *contact;
	GArray        *handles;
	GList         *list;

	g_return_val_if_fail (contacts != NULL, NULL);
	g_return_val_if_fail (handle != 0, NULL);

	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, handle);

	list = gossip_telepathy_contacts_get_from_handles (contacts, handles);
	g_array_free (handles, TRUE);

	if (!list) {
		return NULL;
	}

	contact = list->data;
	g_list_free (list);

	return contact;
}

GList *
gossip_telepathy_contacts_get_from_handles (GossipTelepathyContacts *contacts,
					    GArray                  *handles)
{
	GossipContact  *contact;
	GossipAccount  *account;
	TpConn         *tp_conn;
	gchar         **handles_names;
	gchar         **id;
	GArray         *new_handles;
	GList          *list = NULL;
	guint           i;
	GError         *error = NULL;

	g_return_val_if_fail (contacts != NULL, NULL);
	g_return_val_if_fail (handles != NULL, NULL);

	/* Search all handles we already have */
	new_handles = g_array_new (FALSE, FALSE, sizeof (guint));
	for (i = 0; i < handles->len; i++) {
		guint handle;

		handle = g_array_index (handles, guint, i);
		contact = g_hash_table_find (contacts->contacts,
					     (GHRFunc) telepathy_contacts_find_foreach,
					     GUINT_TO_POINTER (handle));
		if (contact) {
			list = g_list_prepend (list, contact);
		} else {
			g_array_append_val (new_handles, handle);
		}
	}

	if (new_handles->len == 0) {
		return list;
	}

	tp_conn = gossip_telepathy_get_connection (contacts->telepathy);
	account = gossip_telepathy_get_account (contacts->telepathy);

	/* Holds all handles we don't have yet.
	 * FIXME: We should release them at some point. */
	if (!tp_conn_hold_handles (DBUS_G_PROXY (tp_conn),
				   TP_HANDLE_TYPE_CONTACT,
				   new_handles, &error)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "HoldHandles Error: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		g_array_free (new_handles, TRUE);
		return list;
	}

	/* Get the IDs of all new handles */
	if (!tp_conn_inspect_handles (DBUS_G_PROXY (tp_conn),
				      TP_HANDLE_TYPE_CONTACT,
				      new_handles,
				      &handles_names,
				      &error)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "InspectHandle Error: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		g_array_free (new_handles, TRUE);
		return list;
	}

	i = 0;
	for (id = handles_names; *id && i < new_handles->len; id++) {
		GossipContact *contact;
		guint          handle;

		handle = g_array_index (new_handles, guint, i);
		contact = g_hash_table_lookup (contacts->contacts, *id);

		if (!contact) {
			contact = telepathy_contacts_new_contact (contacts, *id);
		}

		g_object_set_data (G_OBJECT (contact), "telepathy-handle",
				   GUINT_TO_POINTER (handle));

		list = g_list_prepend (list, contact);

		i++;
	}

	telepathy_contacts_get_info (contacts, new_handles);
	g_array_free (new_handles, TRUE);
	g_strfreev (handles_names);

	return list;
}

gboolean
gossip_telepathy_contacts_set_avatar (GossipTelepathyContacts *contacts,
				      GossipAvatar            *avatar,
				      GossipCallback           callback,
				      gpointer                 user_data)
{
	GossipCallbackData *data;
	GArray             *avatar_data;

	g_return_val_if_fail (contacts != NULL, FALSE);
	g_return_val_if_fail (avatar != NULL, FALSE);

	if (!contacts->avatars_iface) {
		return FALSE;
	}

	avatar_data = g_array_new (FALSE, FALSE, sizeof (guchar));
	g_array_append_vals (avatar_data, avatar->data, avatar->len);
	data = g_new0 (GossipCallbackData, 1);
	data->callback = callback;
	data->user_data = user_data;

	tp_conn_iface_avatars_set_avatar_async (contacts->avatars_iface,
						avatar_data,
						avatar->format,
						(tp_conn_iface_avatars_set_avatar_reply)
						telepathy_contacts_set_avatar_cb,
						data);
	g_array_free (avatar_data, TRUE);

	return TRUE;
}

void
gossip_telepathy_contacts_get_avatar_requirements (GossipTelepathyContacts  *contacts,
						   guint                    *min_width,
						   guint                    *min_height,
						   guint                    *max_width,
						   guint                    *max_height,
						   gsize                    *max_size,
						   gchar                   **format)
{
	gchar  **formats = NULL;
	gchar   *f;
	guint    min_w, min_h;
	guint    max_w, max_h;
	guint    max_s;
	GError  *error = NULL;

	g_return_if_fail (contacts != NULL);

	if (!contacts->avatars_iface) {
		return;
	}

	if (!tp_conn_iface_avatars_get_avatar_requirements (contacts->avatars_iface,
							    &formats,
							    &min_w, &min_h,
							    &max_w, &max_h,
							    &max_s, &error)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "Couldn't get avatar requirements: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		/* Set default values */
		min_w = 96; min_h = 96;
		max_w = 96; max_h = 96;
		max_s = 8*1024;
		f = g_strdup ("image/png");
	} else {
		f = g_strdup (formats[0]);
		g_strfreev (formats);
	}

	if (min_width) {
		*min_width = min_w;
	}
	if (min_height) {
		*min_height = min_h;
	}
	if (max_width) {
		*max_width = max_w;
	}
	if (max_height) {
		*max_height = max_h;
	}
	if (format) {
		*format = f;
	}
}

void
gossip_telepathy_contacts_rename (GossipTelepathyContacts *contacts,
				  GossipContact           *contact,
				  const gchar             *new_name)
{
	guint        handle;
	GHashTable  *new_alias;
	GError      *error = NULL;
	const gchar *id;

	if (!contacts->aliasing_iface) {
		return;
	}

	id = gossip_contact_get_id (contact);
	handle = gossip_telepathy_contacts_get_handle (contacts, id);
	gossip_debug (DEBUG_DOMAIN, "rename handle: %d", handle);

	new_alias = g_hash_table_new_full (g_direct_hash,
					   g_direct_equal,
					   NULL,
					   g_free);

	g_hash_table_insert (new_alias,
			     GUINT_TO_POINTER (handle),
			     g_strdup (new_name));

	if (!tp_conn_iface_aliasing_set_aliases (contacts->aliasing_iface,
						 new_alias,
						 &error)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "Couldn't rename contact: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
	}

	g_hash_table_destroy (new_alias);
}

void
gossip_telepathy_contacts_send_presence (GossipTelepathyContacts *contacts,
					 GossipPresence          *presence)
{
	GHashTable          *status_ids;
	GHashTable          *status_options;
	const gchar         *status_id;
	const gchar         *message;
	GossipPresenceState  presence_state;
	GError              *error = NULL;
	GValue               value_message = {0, };

	g_return_if_fail (contacts != NULL);
	g_return_if_fail (GOSSIP_IS_PRESENCE (presence));

	if (!contacts->presence_iface) {
		return;
	}

	status_ids = g_hash_table_new_full (g_str_hash,
					    g_str_equal,
					    g_free,
					    (GDestroyNotify) g_hash_table_destroy);
	status_options = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						NULL,
						(GDestroyNotify) g_value_unset);

	presence_state = gossip_presence_get_state (presence);
	status_id = telepathy_presence_state_to_str (presence_state);
	message = gossip_presence_get_status (presence);

	if (message) {
		g_value_init (&value_message, G_TYPE_STRING);
		g_value_set_string (&value_message, message);
		g_hash_table_insert (status_options, "message", &value_message);
	}

	g_hash_table_insert (status_ids,
			     g_strdup (status_id),
			     status_options);

	gossip_debug (DEBUG_DOMAIN, "sending presence...");
	if (!tp_conn_iface_presence_set_status (contacts->presence_iface,
						status_ids,
						&error)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "Could not set presence: %s", 
			      error ? error->message : "No error given");
		g_clear_error (&error);
	}

	g_hash_table_destroy (status_ids);
}

static void
telepathy_contacts_disconnecting_foreach (gchar                   *id,
					  GossipContact           *contact,
					  GossipTelepathyContacts *contacts)
{
	g_object_set_data (G_OBJECT (contact), "telepathy-handle", NULL);
	g_signal_emit_by_name (contacts->telepathy, "contact-removed", contact);
}

static gboolean
telepathy_contacts_find_foreach (gchar         *id,
				 GossipContact *contact,
				 guint          handle)
{
	guint this_handle;

	this_handle = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (contact),
							   "telepathy-handle"));
	if (this_handle == handle) {
		return TRUE;
	}

	return FALSE;
}

static GossipContact *
telepathy_contacts_new_contact (GossipTelepathyContacts *contacts,
				const gchar             *id)
{
	GossipContact *contact;
	GossipAccount *account;

	account = gossip_telepathy_get_account (contacts->telepathy);

	contact = g_object_new (GOSSIP_TYPE_CONTACT,
				"type", GOSSIP_CONTACT_TYPE_TEMPORARY,
				"account", account,
				"id", id,
				NULL);

	gossip_debug (DEBUG_DOMAIN, "new contact created: %s", id);

	g_hash_table_insert (contacts->contacts,
			     g_strdup (id),
			     contact);

	return contact;
}

static void
telepathy_contacts_get_info (GossipTelepathyContacts *contacts,
			     GArray                  *handles)
{
	GError *error = NULL;

	if (contacts->presence_iface) {
		/* FIXME: We should use GetPresence instead */
		if (!tp_conn_iface_presence_request_presence (contacts->presence_iface,
							      handles, &error)) {
			gossip_debug (DEBUG_DOMAIN, 
				      "Could not request presences: %s",
				      error ? error->message : "No error given");
			g_clear_error (&error);
		}
	}

	if (contacts->aliasing_iface) {
		TelepathyContactsAliasesRequestData *data;

		data = g_slice_new (TelepathyContactsAliasesRequestData);
		data->contacts = contacts;
		data->handles = g_memdup (handles->data, handles->len * sizeof (guint));

		tp_conn_iface_aliasing_request_aliases_async (contacts->aliasing_iface,
							      handles,
							      (tp_conn_iface_aliasing_request_aliases_reply)
							      telepathy_contacts_request_aliases_cb,
							      data);
	}

	if (contacts->avatars_iface) {
		guint i;

		for (i = 0; i < handles->len; i++) {
			guint handle;

			handle = g_array_index (handles, gint, i);
			telepathy_contacts_request_avatar (contacts, handle);
		}
	}
}

static void
telepathy_contacts_request_avatar (GossipTelepathyContacts *contacts,
				   guint                    handle)
{
	/* We queue avatar requests to not send too many dbus async
	 * calls at once. If we don't we reach the dbus's limit of
	 * pending calls */
	contacts->avatar_requests_queue = g_list_append (contacts->avatar_requests_queue,
							 GUINT_TO_POINTER (handle));
	telepathy_contacts_start_avatar_requests (contacts);
}

static void
telepathy_contacts_start_avatar_requests (GossipTelepathyContacts *contacts)
{
	TelepathyContactsAvatarRequestData *data;

	while (n_avatar_requests <  MAX_AVATAR_REQUESTS &&
	       contacts->avatar_requests_queue) {
		data = g_slice_new (TelepathyContactsAvatarRequestData);
		data->contacts = contacts;
		data->handle = GPOINTER_TO_UINT (contacts->avatar_requests_queue->data);

		n_avatar_requests++;
		contacts->avatar_requests_queue = g_list_remove (contacts->avatar_requests_queue,
								 contacts->avatar_requests_queue->data);

		tp_conn_iface_avatars_request_avatar_async (contacts->avatars_iface,
							    data->handle,
							    (tp_conn_iface_avatars_request_avatar_reply)
							    telepathy_contacts_request_avatar_cb,
							    data);
	}
}

static void
telepathy_contacts_avatar_update_cb (DBusGProxy              *proxy,
				     guint                    handle,
				     gchar                   *new_token,
				     GossipTelepathyContacts *contacts)
{
	gossip_debug (DEBUG_DOMAIN, "Changing avatar for %d to %s",
		      handle, new_token);

	telepathy_contacts_request_avatar (contacts, handle);
}

static void
telepathy_contacts_request_avatar_cb (DBusGProxy                         *proxy,
				      GArray                             *avatar_data,
				      char                               *mime_type,
				      GError                             *error,
				      TelepathyContactsAvatarRequestData *data)
{
	GossipContact *contact;

	contact = gossip_telepathy_contacts_get_from_handle (data->contacts,
							     data->handle);

	if (error) {
		gossip_debug (DEBUG_DOMAIN, "Error requesting avatar %s: %s",
			      gossip_contact_get_name (contact),
			      error ? error->message : "No error given");
	} else {
		GossipAvatar *avatar;

		avatar = gossip_avatar_new (avatar_data->data,
					    avatar_data->len,
					    mime_type);
		gossip_contact_set_avatar (contact, avatar);
		gossip_avatar_unref (avatar);
	}

	n_avatar_requests--;
	telepathy_contacts_start_avatar_requests (data->contacts);

	g_slice_free (TelepathyContactsAvatarRequestData, data);
}

static void
telepathy_contacts_set_avatar_cb (DBusGProxy         *proxy,
				  char               *token,
				  GError             *error,
				  GossipCallbackData *data)
{
	GossipCallback callback;
	GossipResult   result = GOSSIP_RESULT_OK;

	callback = data->callback;

	if (error) {
		gossip_debug (DEBUG_DOMAIN,
			      "Couldn't set your own avatar: %s",
			      error ? error->message : "No error given");
		result = GOSSIP_RESULT_ERROR_INVALID_REPLY;
	}

	if (callback) {
		(callback) (result, data->user_data);
	}

	g_free (data);
}

static void
telepathy_contacts_aliases_update_cb (DBusGProxy              *proxy,
				      GPtrArray               *renamed_handlers,
				      GossipTelepathyContacts *contacts)
{
	gint i;

	for (i = 0; renamed_handlers->len > i; i++) {
		guint          handle;
		const gchar   *alias;
		GValueArray   *renamed_struct;
		GossipContact *contact;

		renamed_struct = g_ptr_array_index (renamed_handlers, i);
		handle = g_value_get_uint(g_value_array_get_nth (renamed_struct, 0));
		alias = g_value_get_string(g_value_array_get_nth (renamed_struct, 1));

		if (alias && *alias == '\0') {
			alias = NULL;
		}

		contact = gossip_telepathy_contacts_get_from_handle (contacts, handle);
		gossip_contact_set_name (contact, alias);

		gossip_debug (DEBUG_DOMAIN, "contact %d renamed to %s (update cb)", handle, alias);
	}
}

static void
telepathy_contacts_request_aliases_cb (DBusGProxy                           *proxy,
				       gchar                               **contact_names,
				       GError                               *error,
				       TelepathyContactsAliasesRequestData  *data)
{
	guint   i = 0;
	gchar **name;

	for (name = contact_names; *name && !error; name++) {
		GossipContact *contact;

		contact = gossip_telepathy_contacts_get_from_handle (data->contacts,
								     data->handles[i]);
		gossip_contact_set_name (contact, *name);

		gossip_debug (DEBUG_DOMAIN, "contact %d renamed to %s (request cb)",
			      data->handles[i], *name);

		i++;
	}

	g_free (data->handles);
	g_slice_free (TelepathyContactsAliasesRequestData, data);
}

static void
telepathy_contacts_presence_update_cb (DBusGProxy              *proxy,
				       GHashTable              *handle_table,
				       GossipTelepathyContacts *contacts)
{
	g_hash_table_foreach (handle_table,
			      (GHFunc) telepathy_contacts_parse_presence_foreach,
			      contacts);
}

static void
telepathy_contacts_parse_presence_foreach (guint                    handle,
					   GValueArray             *presence_struct,
					   GossipTelepathyContacts *contacts)
{
	GHashTable     *presences_table;
	GossipContact  *contact;
	GossipPresence *presence = NULL;

	contact = gossip_telepathy_contacts_get_from_handle (contacts, handle);
	presences_table = g_value_get_boxed (g_value_array_get_nth (presence_struct, 1));

	g_hash_table_foreach (presences_table,
			      (GHFunc) telepathy_contacts_presences_table_foreach,
			      &presence);

	if (presence) {
		gossip_contact_add_presence (contact, presence);
		g_object_unref (presence);
	} else {
		g_object_set (contact, "presences", NULL, NULL);
	}
}

static void
telepathy_contacts_presences_table_foreach (const gchar     *state_str,
					    GHashTable      *presences_table,
					    GossipPresence **presence)
{
	GossipPresenceState  state;
	const GValue        *message;

	if (*presence) {
		g_object_unref (*presence);
		*presence = NULL;
	}

	state = telepathy_presence_state_from_str (state_str);
	if (state == GOSSIP_PRESENCE_STATE_UNAVAILABLE) {
		return;
	}

	*presence = gossip_presence_new ();
	gossip_presence_set_state (*presence, state);

	message = g_hash_table_lookup (presences_table, "message");
	if (message != NULL) {
		gossip_presence_set_status (*presence,
					    g_value_get_string (message));
	}

	gossip_presence_set_resource (*presence, "");
}

static GossipPresenceState
telepathy_presence_state_from_str (const gchar *str)
{
	if (strcmp (str, "available") == 0) {
		return GOSSIP_PRESENCE_STATE_AVAILABLE;
	} else if ((strcmp (str, "dnd") == 0) || (strcmp (str, "busy") == 0)) {
		return GOSSIP_PRESENCE_STATE_BUSY;
	} else if ((strcmp (str, "away") == 0) || (strcmp (str, "brb") == 0)) {
		return GOSSIP_PRESENCE_STATE_AWAY;
	} else if (strcmp (str, "xa") == 0) {
		return GOSSIP_PRESENCE_STATE_EXT_AWAY;
	} else if (strcmp (str, "hidden") == 0) {
		return GOSSIP_PRESENCE_STATE_HIDDEN;
	} else if (strcmp (str, "offline") == 0) {
		return GOSSIP_PRESENCE_STATE_UNAVAILABLE;
	} else if (strcmp (str, "chat") == 0) {
		/* We don't support chat, so treat it like available. */
		return GOSSIP_PRESENCE_STATE_AVAILABLE;
	}

	return GOSSIP_PRESENCE_STATE_AVAILABLE;
}

static const gchar *
telepathy_presence_state_to_str (GossipPresenceState presence_state)
{
	switch (presence_state) {
	case GOSSIP_PRESENCE_STATE_AVAILABLE:
		return "available";
	case GOSSIP_PRESENCE_STATE_BUSY:
		return "dnd";
	case GOSSIP_PRESENCE_STATE_AWAY:
		return "away";
	case GOSSIP_PRESENCE_STATE_EXT_AWAY:
		return "xa";
	case GOSSIP_PRESENCE_STATE_HIDDEN:
		return "hidden";
	case GOSSIP_PRESENCE_STATE_UNAVAILABLE:
		return "offline";
	default:
		return NULL;
	}

	return NULL;
}

