/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include <string.h>

#include <libgossip/gossip-session.h>

#include "gossip-transport-discover.h"
#include "gossip-transport-register.h"
#include "gossip-jabber.h"
#include "gossip-jabber-private.h"
#include "gossip-jid.h"

#include "gossip-transport-accounts.h"

#define DEBUG_MSG(x)
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n"); */


struct _GossipTransportAccountList {
    /* accounts associated with a GossipJabber */
    GList        *accounts;

    GossipJabber *jabber;

    gint          sig_added;
    gint          sig_removed;
};

struct _GossipTransportAccount {
    gint          ref_count;

    GossipJID    *jid;
    gchar        *disco_type;

    gchar        *name;

    gchar        *username;
    gchar        *password;

    gboolean      received_account_details;
    gboolean      received_disco_type;
};



static void          transport_accounts_contact_added_cb   (GossipProtocol             *protocol,
                                                            GossipContact              *contact,
                                                            GossipTransportAccountList *al);
static void          transport_accounts_contact_removed_cb (GossipProtocol             *protocol,
                                                            GossipContact              *contact,
                                                            GossipTransportAccountList *al);
static GossipTransportAccount *
transport_account_new                 (GossipJID                  *jid);
static void          transport_account_free               (GossipTransportAccount   *account);
static void          transport_account_update_info_cb     (GossipTransportDisco     *disco,
                                                           GossipTransportDiscoItem *item,
                                                           gboolean                  last_item,
                                                           gboolean                  timeout,
                                                           GError                   *error,
                                                           GossipTransportAccount   *account);
static void          transport_account_update_username_cb (GossipJID                *jid,
                                                           const gchar              *key,
                                                           const gchar              *username,
                                                           const gchar              *password,
                                                           const gchar              *nick,
                                                           const gchar              *email,
                                                           gboolean                  require_username,
                                                           gboolean                  require_password,
                                                           gboolean                  require_nick,
                                                           gboolean                  require_email,
                                                           gboolean                  is_registered,
                                                           const gchar              *error_code,
                                                           const gchar              *error_reason,
                                                           GossipTransportAccount   *account);
static void          transport_account_unregister_cb      (GossipJID                *jid,
                                                           const gchar              *error_code,
                                                           const gchar              *error_reason,
                                                           gpointer                  user_data);
static GossipJabber *transport_account_get_jabber         (GossipTransportAccount   *account);


static GList *account_lists = NULL;


GossipTransportAccountList *
gossip_transport_account_list_new (GossipJabber *jabber)
{
    GossipTransportAccountList *al;

    g_return_val_if_fail (jabber != NULL, NULL);

    al = g_new0 (GossipTransportAccountList, 1);

    al->jabber = g_object_ref (jabber);

    al->sig_added = g_signal_connect (jabber,
                                      "contact-added",
                                      G_CALLBACK (transport_accounts_contact_added_cb),
                                      al);
    al->sig_removed = g_signal_connect (jabber,
                                        "contact-removed",
                                        G_CALLBACK (transport_accounts_contact_removed_cb),
                                        al);

    account_lists = g_list_append (account_lists, al);

    return al;
}

void
gossip_transport_account_list_free (GossipTransportAccountList *al)
{
    g_return_if_fail (al != NULL);

    account_lists = g_list_remove (account_lists, al);

    g_signal_handler_disconnect (al->jabber,
                                 al->sig_added);
    g_signal_handler_disconnect (al->jabber,
                                 al->sig_removed);

    g_object_unref (al->jabber);

    /* free items */
    g_list_foreach (al->accounts, (GFunc)transport_account_free, NULL);
    g_list_free (al->accounts);

    g_free (al);
}

static void
transport_accounts_contact_added_cb (GossipProtocol             *protocol,
                                     GossipContact              *contact,
                                     GossipTransportAccountList *al)
{
    const gchar            *id;
    GossipJID              *jid;
    GossipTransportAccount *account;

    id = gossip_contact_get_id (contact);

    jid = gossip_jid_new (id);
    if (!gossip_jid_is_service (jid)) {
        return;
    }

    /* should we worry about subscription here? */
    account = transport_account_new (jid);

    /* add to list */
    al->accounts = g_list_append (al->accounts, account);

    /* request further information */
    account->received_account_details = FALSE;
    account->received_disco_type = TRUE;

    /* confirm disco type */
    if (TRUE) {
        gossip_transport_disco_request_info (al->jabber,
                                             gossip_jid_get_without_resource (account->jid),
                                             (GossipTransportDiscoItemFunc) transport_account_update_info_cb,
                                             account);
    }

    /* confirm account user name */
    gossip_transport_requirements (al->jabber,
                                   account->jid,
                                   (GossipTransportRequirementsFunc) transport_account_update_username_cb,
                                   account);
}

static void
transport_accounts_contact_removed_cb (GossipProtocol             *protocol,
                                       GossipContact              *contact,
                                       GossipTransportAccountList *al)
{
    const gchar            *id;
    GossipJID              *jid;
    GList                  *l;

    id = gossip_contact_get_id (contact);

    jid = gossip_jid_new (id);
    if (!gossip_jid_is_service (jid)) {
        return;
    }

    for (l=al->accounts; l; l=l->next) {
        GossipTransportAccount *account;

        account = l->data;
        if (!account) {
            return;
        }

        if (gossip_jid_equals_without_resource (account->jid, jid)) {
            /* remove from list */
            al->accounts = g_list_remove (al->accounts, account);

            /* clean up */
            transport_account_free (account);
        }
    }
}

GossipTransportAccountList *
gossip_transport_account_list_find (GossipTransportAccount *account)
{
    GList *l1;

    g_return_val_if_fail (account != NULL, NULL);

    for (l1=account_lists; l1; l1=l1->next) {
        GossipTransportAccountList *al;
        GList *l2;

        al = l1->data;
        if (!al || !al->accounts) {
            continue;
        }

        for (l2=al->accounts; l2; l2=l2->next) {
            GossipTransportAccount *tmp_account;

            tmp_account = l2->data;
            if (tmp_account == account) {
                return al;
            }
        }
    }

    return NULL;
}

GossipJabber *
gossip_transport_account_list_get_jabber (GossipTransportAccountList *al)
{
    g_return_val_if_fail (al != NULL, NULL);
    return al->jabber;
}

GList *
gossip_transport_account_lists_get_all (void)
{
    return account_lists;
}

GList *
gossip_transport_accounts_get (GossipTransportAccountList *al)
{
    g_return_val_if_fail (al != NULL, NULL);
    return al->accounts;
}


GList *
gossip_transport_accounts_get_all (void)
{
    GList *l1;
    GList *concat_list = NULL;

    /* this function collates ALL account lists across many Jabber
       sessions and returns a list with ALL transports. */

    for (l1=account_lists; l1; l1=l1->next) {
        GossipTransportAccountList *al;
        GList *l2;

        al = l1->data;
        if (!al || !al->accounts) {
            continue;
        }

        for (l2=al->accounts; l2; l2=l2->next) {
            GossipTransportAccount *account;

            account = l2->data;
            concat_list = g_list_append (concat_list, account);
        }
    }

    return concat_list;
}

const gchar *
gossip_transport_account_guess_disco_type (GossipJID *jid)
{
    const gchar *str;

    g_return_val_if_fail (jid != NULL, NULL);

    str = gossip_jid_get_without_resource (jid);

    if (strstr (str, "aim")) {
        return "aim";
    } else if (strstr (str, "icq")) {
        return "icq";
    } else if (strstr (str, "msn")) {
        return "msn";
    } else if (strstr (str, "yahoo")) {
        return "yahoo";
    }

    return "";
}

static GossipTransportAccount *
transport_account_new (GossipJID *jid)
{
    GossipTransportAccount *account;
    const gchar            *disco_type;

    g_return_val_if_fail (jid != NULL, NULL);

    account = g_new0 (GossipTransportAccount, 1);

    account->ref_count = 1;

    account->jid = gossip_jid_ref (jid);

    /* if core protocol, we fill in some details */
    disco_type = gossip_transport_account_guess_disco_type (jid);
    if (disco_type) {
        account->disco_type = g_strdup (disco_type);

        if (strcmp (disco_type, "aim") == 0) {
            account->name = g_strdup_printf ("AIM");
        } else if (strcmp (disco_type, "icq") == 0) {
            account->name = g_strdup_printf ("ICQ");
        } else if (strcmp (disco_type, "msn") == 0) {
            account->name = g_strdup_printf ("MSN");
        } else if (strcmp (disco_type, "yahoo") == 0) {
            account->name = g_strdup_printf ("Yahoo!");
        }
    }

    return account;
}

static void
transport_account_free (GossipTransportAccount *account)
{
    g_return_if_fail (account != NULL);

    gossip_jid_unref (account->jid);

    g_free (account->disco_type);

    g_free (account->username);
    g_free (account->password);

    g_free (account);
}

GossipTransportAccount *
gossip_transport_account_ref (GossipTransportAccount *account)
{
    g_return_val_if_fail (account != NULL, NULL);

    account->ref_count++;
    return account;
}

void
gossip_transport_account_unref (GossipTransportAccount *account)
{
    g_return_if_fail (account != NULL);

    account->ref_count--;

    if (account->ref_count < 1) {
        transport_account_free (account);
    }
}

GossipJID *
gossip_transport_account_get_jid (GossipTransportAccount *account)
{
    g_return_val_if_fail (account != NULL, NULL);

    return account->jid;
}

const gchar *
gossip_transport_account_get_disco_type (GossipTransportAccount *account)
{
    g_return_val_if_fail (account != NULL, NULL);

    return account->disco_type;
}

const gchar *
gossip_transport_account_get_name (GossipTransportAccount *account)
{
    g_return_val_if_fail (account != NULL, NULL);

    return account->name;
}

const gchar *
gossip_transport_account_get_username (GossipTransportAccount *account)
{
    g_return_val_if_fail (account != NULL, NULL);

    return account->username;
}

const gchar *
gossip_transport_account_get_password (GossipTransportAccount *account)
{
    g_return_val_if_fail (account != NULL, NULL);

    return account->password;
}

gint
gossip_transport_account_count_contacts (GossipTransportAccount *account)
{
    GossipJabber  *jabber;
    const GList   *items;
    const GList   *l;
    const gchar   *service;
    gint           count;

    g_return_val_if_fail (account != NULL, 0);

    jabber = transport_account_get_jabber (account);
    items = gossip_protocol_get_contacts (GOSSIP_PROTOCOL (jabber));

    service = gossip_jid_get_without_resource (account->jid);

    for (l = items, count = 0; l; l = l->next) {
        GossipContact *item;
        GossipJID     *jid;
        const gchar   *id;

        item = l->data;
        id = gossip_contact_get_id (item);
        jid = gossip_jid_new (id);

        if (!gossip_jid_is_service (jid)) {
            const gchar *host;

            host = gossip_jid_get_part_host (jid);
            if (strcmp (service, host) == 0) {
                count++;
            }
        }

        gossip_jid_unref (jid);
    }

    return count;
}

GossipTransportAccount *
gossip_transport_account_find_by_disco_type (GossipTransportAccountList *al,
                                             const gchar                *disco_type)
{
    GList *l;

    g_return_val_if_fail (al != NULL, NULL);
    g_return_val_if_fail (disco_type != NULL, NULL);

    for (l=al->accounts; l; l=l->next) {
        GossipTransportAccount *account;

        account = l->data;
        if (strcmp (account->disco_type, disco_type) == 0) {
            return account;
        }
    }

    return NULL;
}

static void
transport_account_update_info_cb (GossipTransportDisco     *disco,
                                  GossipTransportDiscoItem *item,
                                  gboolean                  last_item,
                                  gboolean                  timeout,
                                  GError                   *error,
                                  GossipTransportAccount   *account)
{
    const gchar *type;

    type = gossip_transport_disco_item_get_type (item);
    if (type) {
        account->disco_type = g_strdup (type);
    }

    account->received_disco_type = TRUE;
}

static void
transport_account_update_username_cb (GossipJID              *jid,
                                      const gchar            *key,
                                      const gchar            *username,
                                      const gchar            *password,
                                      const gchar            *nick,
                                      const gchar            *email,
                                      gboolean                require_username,
                                      gboolean                require_password,
                                      gboolean                require_nick,
                                      gboolean                require_email,
                                      gboolean                is_registered,
                                      const gchar            *error_code,
                                      const gchar            *error_reason,
                                      GossipTransportAccount *account)
{
    if (username) {
        account->username = g_strdup (username);
    }

    if (password) {
        account->password = g_strdup (password);
    }

    account->received_account_details = TRUE;
}

void
gossip_transport_account_remove (GossipTransportAccount *account)
{
    GossipJabber               *jabber;
    GossipJID                  *service_jid;
    GossipJID                  *from_jid;
    GossipContact              *own_contact;

    LmConnection               *connection;
    LmMessage                  *m;
    LmMessageNode              *node;

    const GList                *items;
    const GList                *l;

    const gchar                *id;

    g_return_if_fail (account != NULL);

    jabber = transport_account_get_jabber (account);
    connection = gossip_jabber_get_connection (jabber);

    service_jid = gossip_transport_account_get_jid (account);
    own_contact = gossip_jabber_get_own_contact (jabber);

    /* it is important the resource is included, usually MSN and
       ICQ have a "registered" resource which if not included does
       not remove them off the roster */
    id = gossip_contact_get_id (own_contact);
    from_jid = gossip_jid_new (id);

    DEBUG_MSG (("ProtocolTransport: Removing Service:'%s'"
                gossip_jid_get_full (service_jid)));

    /* remove contacts associated with the service from roster */
    items = gossip_protocol_get_contacts (GOSSIP_PROTOCOL (jabber));

    for (l=items; l; l=l->next) {
        GossipContact *item;
        GossipJID     *contact_jid;
        const gchar   *contact_id;
        const gchar   *contact_host;

        item = l->data;
        contact_id = gossip_contact_get_id (item);
        contact_jid = gossip_jid_new (contact_id);
        contact_host = gossip_jid_get_part_host (contact_jid);

        gossip_jid_unref (contact_jid);

        if (!contact_host ||
            strcmp (gossip_jid_get_without_resource (service_jid), contact_host) != 0) {
            continue;
        }

        DEBUG_MSG (("ProtocolTransport: Removing '%s' from roster", contact_id));

        m = lm_message_new_with_sub_type (NULL,
                                          LM_MESSAGE_TYPE_IQ,
                                          LM_MESSAGE_SUB_TYPE_SET);

        node = lm_message_node_add_child (m->node, "query", NULL);
        lm_message_node_set_attribute (node,
                                       "xmlns",
                                       "jabber:iq:roster");

        node = lm_message_node_add_child (node, "item", NULL);
        lm_message_node_set_attributes (node,
                                        "jid", contact_id,
                                        "subscription", "remove",
                                        NULL);

        lm_connection_send (connection, m, NULL);
        lm_message_unref (m);
    }

    /* remove from roster the service */
    DEBUG_MSG (("ProtocolTransport: Removing service '%s' from contact list",
                gossip_jid_get_full (service_jid)));

    m = lm_message_new_with_sub_type (NULL,
                                      LM_MESSAGE_TYPE_IQ,
                                      LM_MESSAGE_SUB_TYPE_SET);

    lm_message_node_set_attribute (m->node, "from",
                                   gossip_jid_get_full (from_jid));
    node = lm_message_node_add_child (m->node, "query", NULL);
    lm_message_node_set_attribute (node,
                                   "xmlns",
                                   "jabber:iq:roster");

    node = lm_message_node_add_child (node, "item", NULL);
    lm_message_node_set_attributes (node,
                                    "jid", gossip_jid_get_full (service_jid),
                                    "subscription", "remove",
                                    NULL);

    lm_connection_send (connection, m, NULL);
    lm_message_unref (m);

    /* unregister the service - new method */
    DEBUG_MSG (("ProtocolTransport: Request disco unregister"));

    gossip_transport_unregister (jabber,
                                 service_jid,
                                 (GossipTransportUnregisterFunc) transport_account_unregister_cb,
                                 NULL);

    /* clean up */
    gossip_jid_unref (from_jid);
}

static void
transport_account_unregister_cb (GossipJID   *jid,
                                 const gchar *error_code,
                                 const gchar *error_reason,
                                 gpointer     user_data)
{
    DEBUG_MSG (("ProtocolTransport: Request disco unregister - response "
                "(jid:'%s', error:'%s'->'%s')",
                gossip_jid_get_full (jid), error_code, error_reason));
}

static GossipJabber *
transport_account_get_jabber (GossipTransportAccount *account)
{
    GossipTransportAccountList *al;
    GossipJabber               *jabber;

    al = gossip_transport_account_list_find (account);
    jabber = gossip_transport_account_list_get_jabber (al);

    return jabber;
}

