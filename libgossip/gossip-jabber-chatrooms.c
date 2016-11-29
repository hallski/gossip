/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2004-2008 Imendio AB
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

#include <stdlib.h>
#include <string.h>


#include "gossip-debug.h"
#include "gossip-chatroom.h"
#include "gossip-chatroom-invite.h"
#include "gossip-chatroom-manager.h"
#include "gossip-ft.h"
#include "gossip-ft-provider.h"
#include "gossip-message.h"
#include "gossip-utils.h"

#include "gossip-jabber-chatrooms.h"
#include "gossip-jabber-disco.h"
#include "gossip-jabber-utils.h"
#include "gossip-jabber-private.h"
#include "gossip-jid.h"

#define DEBUG_DOMAIN "JabberChatrooms"

#define XMPP_MUC_XMLNS       "http://jabber.org/protocol/muc"
#define XMPP_MUC_OWNER_XMLNS "http://jabber.org/protocol/muc#owner"
#define XMPP_MUC_USER_XMLNS  "http://jabber.org/protocol/muc#user"
#define XMPP_MUC_ADMIN_XMLNS "http://jabber.org/protocol/muc#admin"

#define JOIN_TIMEOUT 20000

struct _GossipJabberChatrooms {
    GossipJabber   *jabber;
    GossipPresence *presence;
    LmConnection   *connection;

    GHashTable     *chatrooms_by_id;
    GHashTable     *chatrooms_by_pointer;
    GHashTable     *chatrooms_by_jid;
    GHashTable     *join_timeouts;
    GHashTable     *join_callbacks;
};

static void            logged_out_cb                  (GossipJabber          *jabber,
                                                       GossipAccount         *account,
                                                       gint                   reason,
                                                       GossipJabberChatrooms *chatrooms);
static void            join_timeout_destroy_notify_cb (gpointer               data);
static LmHandlerResult message_handler                (LmMessageHandler      *handler,
                                                       LmConnection          *conn,
                                                       LmMessage             *message,
                                                       GossipJabberChatrooms *chatrooms);
static LmHandlerResult presence_handler               (LmMessageHandler      *handler,
                                                       LmConnection          *conn,
                                                       LmMessage             *message,
                                                       GossipJabberChatrooms *chatrooms);



GossipJabberChatrooms *
gossip_jabber_chatrooms_init (GossipJabber *jabber)
{
    GossipJabberChatrooms *chatrooms;
    LmConnection          *connection;
    LmMessageHandler      *handler;

    g_return_val_if_fail (GOSSIP_IS_JABBER (jabber), NULL);
        
    connection = _gossip_jabber_get_connection (jabber);
    g_return_val_if_fail (connection != NULL, NULL);

    chatrooms = g_new0 (GossipJabberChatrooms, 1);

    chatrooms->jabber = g_object_ref (jabber);
    chatrooms->connection = lm_connection_ref (connection);
    chatrooms->presence = NULL;

    g_signal_connect (chatrooms->jabber, "disconnected",
                      G_CALLBACK (logged_out_cb),
                      chatrooms);

    chatrooms->chatrooms_by_id = 
        g_hash_table_new_full (g_direct_hash,
                               g_direct_equal,
                               NULL,
                               (GDestroyNotify) g_object_unref);
    chatrooms->chatrooms_by_pointer = 
        g_hash_table_new_full (gossip_chatroom_hash,
                               gossip_chatroom_equal,
                               (GDestroyNotify) g_object_unref,
                               NULL);
    chatrooms->chatrooms_by_jid = 
        g_hash_table_new_full (gossip_jid_hash_without_resource,
                               gossip_jid_equal_without_resource, 
                               (GDestroyNotify) g_object_unref,
                               (GDestroyNotify) g_object_unref);
    chatrooms->join_timeouts = 
        g_hash_table_new_full (gossip_chatroom_hash,
                               gossip_chatroom_equal,
                               (GDestroyNotify) g_object_unref,
                               join_timeout_destroy_notify_cb);
    chatrooms->join_callbacks = 
        g_hash_table_new_full (gossip_chatroom_hash,
                               gossip_chatroom_equal,
                               (GDestroyNotify) g_object_unref,
                               (GDestroyNotify) gossip_callback_data_free);

    /* Set up message and presence handlers */
    handler = lm_message_handler_new ((LmHandleMessageFunction) message_handler,
                                      chatrooms, 
                                      NULL);

    lm_connection_register_message_handler (chatrooms->connection,
                                            handler,
                                            LM_MESSAGE_TYPE_MESSAGE,
                                            LM_HANDLER_PRIORITY_NORMAL);
    lm_message_handler_unref (handler);

    handler = lm_message_handler_new ((LmHandleMessageFunction) presence_handler,
                                      chatrooms, 
                                      NULL);

    lm_connection_register_message_handler (chatrooms->connection,
                                            handler,
                                            LM_MESSAGE_TYPE_PRESENCE,
                                            LM_HANDLER_PRIORITY_FIRST);
    lm_message_handler_unref (handler);

    return chatrooms;
}

void
gossip_jabber_chatrooms_finalize (GossipJabberChatrooms *chatrooms)
{
    if (!chatrooms) {
        /* We don't error here, because if no connection is
         * made, then we can clean up a GossipJabber object
         * without any chatrooms ever existing.
         */
        return;
    }

    g_hash_table_unref (chatrooms->chatrooms_by_id);
    chatrooms->chatrooms_by_id = NULL;

    g_hash_table_unref (chatrooms->chatrooms_by_pointer);
    chatrooms->chatrooms_by_pointer = NULL;

    g_hash_table_unref (chatrooms->chatrooms_by_jid);
    chatrooms->chatrooms_by_jid = NULL;

    g_hash_table_unref (chatrooms->join_timeouts);
    chatrooms->join_timeouts = NULL;

    g_hash_table_unref (chatrooms->join_callbacks);
    chatrooms->join_timeouts = NULL;

    g_signal_handlers_disconnect_by_func (chatrooms->jabber,
                                          logged_out_cb,
                                          chatrooms);

    if (chatrooms->presence) {
        g_object_unref (chatrooms->presence);
    }

    lm_connection_unref (chatrooms->connection);
    g_object_unref (chatrooms->jabber);

    g_free (chatrooms);
}

static void
logged_out_foreach (gpointer key,
                    gpointer value,
                    gpointer user_data)
{
    GossipJabberChatrooms *chatrooms;
    GossipChatroomId       id;

    id = GPOINTER_TO_INT (key);
    chatrooms = (GossipJabberChatrooms *) user_data;

    gossip_jabber_chatrooms_leave (chatrooms, id);
}

static void
logged_out_cb (GossipJabber          *jabber,
               GossipAccount         *account,
               gint                   reason,
               GossipJabberChatrooms *chatrooms)
{
    g_hash_table_foreach (chatrooms->chatrooms_by_id,
                          logged_out_foreach,
                          chatrooms);
}

static LmHandlerResult
message_handler (LmMessageHandler      *handler,
                 LmConnection          *conn,
                 LmMessage             *m,
                 GossipJabberChatrooms *chatrooms)
{
    LmMessageNode    *node;
    GossipJID        *jid;
    GossipChatroom   *chatroom;
    GossipChatroomId  id;
    GossipContact    *contact;
    GossipMessage    *message;
    const gchar      *from;

    if (lm_message_get_sub_type (m) != LM_MESSAGE_SUB_TYPE_GROUPCHAT) {
        return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
    }

    from = lm_message_node_get_attribute (m->node, "from");
    jid = gossip_jid_new (from);

    chatroom = g_hash_table_lookup (chatrooms->chatrooms_by_jid, jid);

    if (!chatroom) {
        g_object_unref (jid);
        return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;   
    }

    id = gossip_chatroom_get_id (chatroom);

    contact = gossip_jabber_get_contact_from_jid (chatrooms->jabber, 
                                                  from, 
                                                  FALSE,
                                                  FALSE,
                                                  FALSE);

    node = lm_message_node_get_child (m->node, "body");
    if (node) {
        if (gossip_jid_get_resource (jid) == NULL) {
            g_signal_emit_by_name (chatrooms->jabber,
                                   "chatroom-new-event",
                                   id, 
                                   node->value);
        } else {
            GossipTime timestamp;

            timestamp = gossip_jabber_get_message_timestamp (m);

            /* NOTE: We don't use the chatroom contact
             * here, we use the actual REAL contact
             * instead, this is to keep things sane in
             * the UI code.
             */
            message = gossip_message_new (GOSSIP_MESSAGE_TYPE_CHAT_ROOM,
                                          gossip_jabber_get_own_contact (chatrooms->jabber));

            timestamp = gossip_jabber_get_message_timestamp (m);
            gossip_message_set_timestamp (message, timestamp);
            gossip_message_set_sender (message, contact);
            gossip_message_set_body (message, node->value);

            g_signal_emit_by_name (chatrooms->jabber,
                                   "chatroom-new-message",
                                   id, 
                                   message);

            g_object_unref (message);
        }
    }

    node = lm_message_node_get_child (m->node, "subject");
    if (node) {
        /* We don't handle subject change twice! Above, you
         * may notice that we emit the "chatroom-new-event"
         * signal. This is used to emit the subject for the
         * chatroom when there is NO contact. The subject
         * here has the same content EXCEPT that it doesn't
         * include the name of the person that set the
         * subject, just what the subject _IS_. We only use
         * this when we know the contact.
         */
        if (contact) {
            g_signal_emit_by_name (chatrooms->jabber,
                                   "chatroom-subject-changed",
                                   id, 
                                   contact, 
                                   node->value);
        }
    }

    g_object_unref (jid);

    return LM_HANDLER_RESULT_REMOVE_MESSAGE;

}

static GossipChatroomError 
error_from_code (const gchar *xmlns, 
                 gint         code)
{
    if (G_STR_EMPTY (xmlns)) {
        /* 503 is really service not available,
         * 403 is really subject change not authorized
         * 405 is really kick/ban/etc not authorized
         */
        switch (code) {
        case 400: return GOSSIP_CHATROOM_ERROR_BAD_REQUEST;
        case 403: return GOSSIP_CHATROOM_ERROR_UNAUTHORIZED_REQUEST; 
        case 405: return GOSSIP_CHATROOM_ERROR_UNAUTHORIZED_REQUEST;
        case 409: return GOSSIP_CHATROOM_ERROR_NICK_IN_USE;
        case 503: return GOSSIP_CHATROOM_ERROR_ROOM_NOT_FOUND;

            /* Legacy Errors */
        case 502: return GOSSIP_CHATROOM_ERROR_ROOM_NOT_FOUND;
        case 504: return GOSSIP_CHATROOM_ERROR_TIMED_OUT;
        default:
            break;
        }
    }

    if (strcmp (xmlns, XMPP_MUC_XMLNS) == 0) {
        switch (code) {
            /* 503 could mean room is full */
        case 401: return GOSSIP_CHATROOM_ERROR_PASSWORD_INVALID_OR_MISSING;
        case 403: return GOSSIP_CHATROOM_ERROR_USER_BANNED;
        case 404: return GOSSIP_CHATROOM_ERROR_ROOM_NOT_FOUND;
        case 405: return GOSSIP_CHATROOM_ERROR_ROOM_CREATION_RESTRICTED;
        case 406: return GOSSIP_CHATROOM_ERROR_USE_RESERVED_ROOM_NICK;
        case 407: return GOSSIP_CHATROOM_ERROR_NOT_ON_MEMBERS_LIST;
        case 409: return GOSSIP_CHATROOM_ERROR_NICK_IN_USE;
        case 503: return GOSSIP_CHATROOM_ERROR_ROOM_NOT_FOUND;
        default:
            break;
        }
    } else if (strcmp (xmlns, XMPP_MUC_USER_XMLNS) == 0) {
        switch (code) {
        case 401: return GOSSIP_CHATROOM_ERROR_PASSWORD_INVALID_OR_MISSING;
        case 403: return GOSSIP_CHATROOM_ERROR_FORBIDDEN;
        case 404: return GOSSIP_CHATROOM_ERROR_ROOM_NOT_FOUND;
        case 405: return GOSSIP_CHATROOM_ERROR_ROOM_CREATION_RESTRICTED;
        case 406: return GOSSIP_CHATROOM_ERROR_USE_RESERVED_ROOM_NICK;
        case 407: return GOSSIP_CHATROOM_ERROR_NOT_ON_MEMBERS_LIST;
        case 409: return GOSSIP_CHATROOM_ERROR_NICK_IN_USE;
        default:
            break;
        }
    } else if (strcmp (xmlns, XMPP_MUC_OWNER_XMLNS) == 0 ||
               strcmp (xmlns, XMPP_MUC_ADMIN_XMLNS) == 0) {
        switch (code) {
        case 403: return GOSSIP_CHATROOM_ERROR_FORBIDDEN;
        default:
            break;
        }
    }
        
    return GOSSIP_CHATROOM_ERROR_UNKNOWN;
}

static LmMessageNode *
find_muc_user_node (LmMessageNode *parent_node)
{
    LmMessageNode *child;

    /* Should have a function in Loudmouth to find a child with xmlns */
    child = parent_node->children;

    if (!child) {
        return NULL;
    }

    while (child) {
        if (strcmp (child->name, "x") == 0) {
            const gchar *xmlns;

            xmlns = lm_message_node_get_attribute (child, "xmlns");

            if (xmlns && strcmp (xmlns, XMPP_MUC_USER_XMLNS) == 0) {
                return child;
            }
        }

        child = child->next;
    }

    return NULL;
}

static GossipChatroomRole
get_role (LmMessageNode *muc_node)
{
    LmMessageNode *item_node;
    const gchar   *role;

    if (!muc_node) {
        return GOSSIP_CHATROOM_ROLE_NONE;
    }

    item_node = lm_message_node_get_child (muc_node, "item");
    if (!item_node) {
        return GOSSIP_CHATROOM_ROLE_NONE;
    }

    role = lm_message_node_get_attribute (item_node, "role");
    if (!role) {
        return GOSSIP_CHATROOM_ROLE_NONE;
    }

    if (strcmp (role, "moderator") == 0) {
        return GOSSIP_CHATROOM_ROLE_MODERATOR;
    }
    else if (strcmp (role, "participant") == 0) {
        return GOSSIP_CHATROOM_ROLE_PARTICIPANT;
    }
    else if (strcmp (role, "visitor") == 0) {
        return GOSSIP_CHATROOM_ROLE_VISITOR;
    } else {
        return GOSSIP_CHATROOM_ROLE_NONE;
    }
}

static GossipChatroomAffiliation
get_affiliation (LmMessageNode *muc_node)
{
    LmMessageNode *item_node;
    const gchar   *affiliation;

    if (!muc_node) {
        return GOSSIP_CHATROOM_AFFILIATION_NONE;
    }

    item_node = lm_message_node_get_child (muc_node, "item");
    if (!item_node) {
        return GOSSIP_CHATROOM_AFFILIATION_NONE;
    }

    affiliation = lm_message_node_get_attribute (item_node, "affiliation");
    if (!affiliation) {
        return GOSSIP_CHATROOM_AFFILIATION_NONE;
    }

    if (strcmp (affiliation, "owner") == 0) {
        return GOSSIP_CHATROOM_AFFILIATION_OWNER;
    }
    else if (strcmp (affiliation, "admin") == 0) {
        return GOSSIP_CHATROOM_AFFILIATION_ADMIN;
    }
    else if (strcmp (affiliation, "member") == 0) {
        return GOSSIP_CHATROOM_AFFILIATION_MEMBER;
    }
    else if (strcmp (affiliation, "outcast") == 0) {
        return GOSSIP_CHATROOM_AFFILIATION_OUTCAST;
    } else {
        return GOSSIP_CHATROOM_AFFILIATION_NONE;
    }
}

static void
leave_chatroom (GossipJabberChatrooms *chatrooms,
                GossipChatroom        *chatroom)
{
    GossipJID        *jid;
    GossipChatroomId  id;

    id = gossip_chatroom_get_id (chatroom);

    gossip_debug (DEBUG_DOMAIN, 
                  "ID[%d] Leaving room", 
                  id);

    gossip_chatroom_set_last_error (chatroom, GOSSIP_CHATROOM_ERROR_NONE);
    gossip_chatroom_set_status (chatroom, GOSSIP_CHATROOM_STATUS_INACTIVE);

    jid = gossip_jid_new (gossip_chatroom_get_id_str (chatroom));
        
    g_hash_table_remove (chatrooms->chatrooms_by_id, GINT_TO_POINTER (id));
    g_hash_table_remove (chatrooms->chatrooms_by_pointer, chatroom);
    g_hash_table_remove (chatrooms->chatrooms_by_jid, jid);
        
    g_object_unref (jid);
}

static void
create_instant_room (LmConnection   *connection,
                     GossipChatroom *chatroom)
{
    LmMessage     *m;
    LmMessageNode *node;
    GossipAccount *account;
    gchar         *from;
    const gchar   *to;

    m = lm_message_new_with_sub_type (NULL,
                                      LM_MESSAGE_TYPE_IQ,
                                      LM_MESSAGE_SUB_TYPE_SET);

    account = gossip_chatroom_get_account (chatroom);
    from = g_strconcat (gossip_account_get_id (account),
                        "/",
                        gossip_account_get_resource (account),
                        NULL);
    lm_message_node_set_attribute (m->node, "from", from);
    g_free (from);

    to = gossip_chatroom_get_id_str (chatroom);
    lm_message_node_set_attribute (m->node, "to", to);

    node = lm_message_node_add_child (m->node, "query", NULL);
    lm_message_node_set_attributes (node, "xmlns", XMPP_MUC_OWNER_XMLNS, NULL);

    node = lm_message_node_add_child (node, "x", NULL);
    lm_message_node_set_attributes (node, 
                                    "xmlns", "jabber:x:data", 
                                    "type", "submit",
                                    NULL);

    lm_connection_send (connection, m,  NULL);
    lm_message_unref (m);
}

static void
create_reserved_room (LmConnection   *connection,
                      GossipChatroom *chatroom)
{
    LmMessage     *m;
    LmMessageNode *node;
    LmMessageNode *child;
    GossipAccount *account;
    gchar         *from;
    const gchar   *to;
    const gchar   *name;
    const gchar   *password;

    m = lm_message_new_with_sub_type (NULL,
                                      LM_MESSAGE_TYPE_IQ,
                                      LM_MESSAGE_SUB_TYPE_SET);

    account = gossip_chatroom_get_account (chatroom);
    from = g_strconcat (gossip_account_get_id (account),
                        "/",
                        gossip_account_get_resource (account),
                        NULL);
    lm_message_node_set_attribute (m->node, "from", from);
    g_free (from);

    to = gossip_chatroom_get_id_str (chatroom);
    lm_message_node_set_attribute (m->node, "to", to);

    node = lm_message_node_add_child (m->node, "query", NULL);
    lm_message_node_set_attributes (node, "xmlns", XMPP_MUC_OWNER_XMLNS, NULL);

    node = lm_message_node_add_child (node, "x", NULL);
    lm_message_node_set_attributes (node, 
                                    "xmlns", "jabber:x:data", 
                                    "type", "submit",
                                    NULL);

    /* FIXME: This is a shortcut for now, we should use their forms */
    name = gossip_chatroom_get_name (chatroom);
    password = gossip_chatroom_get_password (chatroom);

    child = lm_message_node_add_child (node, "field", NULL);
    lm_message_node_set_attributes (child, "var", "muc#roomconfig_roomname", NULL);
    lm_message_node_add_child (child, "value", name);
        
    child = lm_message_node_add_child (node, "field", NULL);
    lm_message_node_set_attributes (child, "var", "muc#roomconfig_passwordprotectedroom", NULL);
    lm_message_node_add_child (child, "value", G_STR_EMPTY (password) ? "0" : "1");

    child = lm_message_node_add_child (node, "field", NULL);
    lm_message_node_set_attributes (child, "var", "muc#roomconfig_roomsecret", NULL);
    lm_message_node_add_child (child, "value", G_STR_EMPTY (password) ? "" : password);
        
    /* Finally send */
    lm_connection_send (connection, m,  NULL);
    lm_message_unref (m);
}

static void
request_reserved_room (LmConnection   *connection,
                       GossipChatroom *chatroom)
{
    LmMessage     *m;
    LmMessageNode *node;
    GossipAccount *account;
    gchar         *from;
    const gchar   *to;

    m = lm_message_new_with_sub_type (NULL,
                                      LM_MESSAGE_TYPE_IQ,
                                      LM_MESSAGE_SUB_TYPE_GET);

    account = gossip_chatroom_get_account (chatroom);
    from = g_strconcat (gossip_account_get_id (account),
                        "/",
                        gossip_account_get_resource (account),
                        NULL);
    lm_message_node_set_attribute (m->node, "from", from);
    g_free (from);

    to = gossip_chatroom_get_id_str (chatroom);
    lm_message_node_set_attribute (m->node, "to", to);

    node = lm_message_node_add_child (m->node, "query", NULL);
    lm_message_node_set_attributes (node, "xmlns", XMPP_MUC_OWNER_XMLNS, NULL);

    lm_connection_send (connection, m,  NULL);
    lm_message_unref (m);
}

static gboolean
join_finish (GossipJabberChatrooms *chatrooms,
             GossipChatroom        *chatroom,
             LmMessage             *m)
{
    GossipChatroomError   error;
    GossipChatroomStatus  status;
    GossipChatroomId      id;
    GossipCallbackData   *data;
    LmMessageSubType      type;
    LmMessageNode        *node = NULL;

    /* Get room id */
    id = gossip_chatroom_get_id (chatroom);

    /* Clean up the join timeout */
    g_hash_table_remove (chatrooms->join_timeouts, chatroom);

    /* Check status code */
    if (gossip_jabber_get_message_has_status (m, 201)) {
        /* Room was created for us */
        if (0) {
            request_reserved_room (chatrooms->connection, chatroom);
            create_instant_room (chatrooms->connection, chatroom); 
        } else {
            create_reserved_room (chatrooms->connection, chatroom);
        }
    }

    /* Check for error */
    type = lm_message_get_sub_type (m);
    if (type == LM_MESSAGE_SUB_TYPE_ERROR) {
        node = lm_message_node_get_child (m->node, "error");
    }

    if (node) {
        const gchar *xmlns = NULL;
        const gchar *code_str;
        gint         code;

        code_str = lm_message_node_get_attribute (node, "code");
        code = code_str ? atoi (code_str) : 0;

        node = lm_message_node_get_child (m->node, "x");
        if (node) {
            xmlns = lm_message_node_get_attribute (node, "xmlns");
        }

        error = error_from_code (xmlns, code);
        gossip_debug (DEBUG_DOMAIN, 
                      "ID[%d] %s", 
                      id, 
                      gossip_chatroom_error_to_string (error));

        /* Set room state */
        status = GOSSIP_CHATROOM_STATUS_ERROR;
    } else {
        error = GOSSIP_CHATROOM_ERROR_NONE;
        status = GOSSIP_CHATROOM_STATUS_ACTIVE;
    }

    gossip_chatroom_set_last_error (chatroom, error);
    gossip_chatroom_set_status (chatroom, status);

    data = g_hash_table_lookup (chatrooms->join_callbacks, chatroom);

    if (data && data->callback) {
        GossipChatroomJoinCb func;

        func = (GossipChatroomJoinCb) data->callback;

        gossip_debug (DEBUG_DOMAIN,
                      "ID[%d] Calling back...", 
                      id);
        (func) (GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
                id, 
                error, 
                data->user_data);
    }

    /* Clean up the user data */
    g_hash_table_remove (chatrooms->join_callbacks, chatroom);

    /* If we have an error, clean up */
    if (error != GOSSIP_CHATROOM_ERROR_NONE) {
        GossipJID        *jid;
        GossipChatroomId  id;

        /* Clean up */
        id = gossip_chatroom_get_id (chatroom);
        jid = gossip_jid_new (gossip_chatroom_get_id_str (chatroom));

        g_hash_table_remove (chatrooms->chatrooms_by_id, GINT_TO_POINTER (id));
        g_hash_table_remove (chatrooms->chatrooms_by_pointer, chatroom);
        g_hash_table_remove (chatrooms->chatrooms_by_jid, jid);

        g_object_unref (jid);

        return FALSE;
    }

    return TRUE;
}

static gchar *
get_new_id_for_new_nick (GossipContact *contact,
                         const gchar   *new_nick)
{
    GossipJID   *jid;
    const gchar *id_str;
    const gchar *id_str_without_resource;
    gchar       *id_str_with_new_nick;

    id_str = gossip_contact_get_id (contact);
    jid = gossip_jid_new (id_str);
    id_str_without_resource = gossip_jid_get_without_resource (jid);
    id_str_with_new_nick = g_strconcat (id_str_without_resource, "/", new_nick, NULL);
    g_object_unref (jid);

    return id_str_with_new_nick;
}

static LmHandlerResult
presence_handler (LmMessageHandler      *handler,
                  LmConnection          *connection,
                  LmMessage             *m,
                  GossipJabberChatrooms *chatrooms)
{
    const gchar               *from;
    GossipJID                 *jid;
    GossipContact             *own_contact;
    GossipContact             *contact;
    GossipPresence            *presence;
    GossipChatroom            *chatroom;
    GossipChatroomId           id;
    LmMessageSubType           type;
    LmMessageNode             *node;
    gboolean                   was_offline;
    LmMessageNode             *muc_user_node;
    GossipChatroomContactInfo  muc_contact_info;
    gchar                     *new_nick;

    from = lm_message_node_get_attribute (m->node, "from");
    jid = gossip_jid_new (from);

    chatroom = g_hash_table_lookup (chatrooms->chatrooms_by_jid, jid);

    if (!chatroom) {
        g_object_unref (jid);
        return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;   
    }

    gossip_debug (DEBUG_DOMAIN,
                  "Presence from:'%s'", 
                  from);

    /* If this JID matches a room we are joining, first call the
     * callback and clean up to show we are now joined and then
     * continue to handle the first presence message we were sent.
     */
    if (g_hash_table_lookup (chatrooms->join_timeouts, chatroom)) {
        /* If this returns FALSE, it means there was an
         * error, and we have handled it, so don't handle it
         * again here or any further messages from the room 
         */
        if (!join_finish (chatrooms, chatroom, m)) {
            g_object_unref (jid);
            return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;       
        }
    }

    id = gossip_chatroom_get_id (chatroom);

    type = lm_message_get_sub_type (m);
    switch (type) {
    case LM_MESSAGE_SUB_TYPE_AVAILABLE:
        /* Get details */
        contact = gossip_jabber_get_contact_from_jid (chatrooms->jabber, 
                                                      from, 
                                                      FALSE,
                                                      FALSE,
                                                      FALSE);

        presence = gossip_presence_new ();

        node = lm_message_node_get_child (m->node, "show");
        if (node) {
            GossipPresenceState state;

            state = gossip_jabber_presence_state_from_str (node->value);
            gossip_presence_set_state (presence, state);
        }

        node = lm_message_node_get_child (m->node, "status");
        if (node) {
            gossip_presence_set_status (presence, node->value);
        }

        /* Should signal joined if contact was found but offline */
        was_offline = !gossip_contact_is_online (contact);
        gossip_contact_add_presence (contact, presence);
        g_object_unref (presence);

        muc_user_node = find_muc_user_node (m->node);
        muc_contact_info.role = get_role (muc_user_node);
        muc_contact_info.affiliation = get_affiliation (muc_user_node);

        /* Is contact new or updated */
        if (was_offline) {
            gossip_debug (DEBUG_DOMAIN,
                          "ID[%d] Presence for new joining contact:'%s'",
                          id,
                          gossip_jid_get_full (jid));
            gossip_chatroom_contact_joined (chatroom,
                                            contact,
                                            &muc_contact_info);
        } else {
            gossip_chatroom_set_contact_info (chatroom,
                                              contact,
                                              &muc_contact_info);
        }
        break;

    case LM_MESSAGE_SUB_TYPE_UNAVAILABLE:
        contact = gossip_jabber_get_contact_from_jid (chatrooms->jabber, 
                                                      from, 
                                                      FALSE,
                                                      FALSE,
                                                      FALSE);

        new_nick = NULL;

        if (gossip_jabber_get_message_is_muc_new_nick (m, &new_nick)) {
            gchar *old_nick;
            gchar *new_id;

            old_nick = g_strdup (gossip_contact_get_name (contact));
            new_id = get_new_id_for_new_nick (contact, new_nick);
            gossip_contact_set_id (contact, new_id);
            gossip_contact_set_name (contact, new_nick);
            g_free (new_id);
            g_free (new_nick);

            gossip_debug (DEBUG_DOMAIN,
                          "[%d] Nick changed for contact:'%s', old nick:'%s', new nick:'%s'", 
                          id, 
                          gossip_contact_get_id (contact),
                          old_nick,
                          gossip_contact_get_name (contact));
                        
            g_signal_emit_by_name (chatrooms->jabber, 
                                   "chatroom-nick-changed", 
                                   id,
                                   contact,
                                   old_nick);
                        
            g_free (old_nick);
        } else {
            own_contact = gossip_chatroom_get_own_contact (chatroom);

            if (gossip_contact_equal (contact, own_contact)) {
                gossip_debug (DEBUG_DOMAIN, 
                              "ID[%d] We have been kicked!", 
                              id);

                gossip_chatroom_set_status (chatroom, 
                                            GOSSIP_CHATROOM_STATUS_INACTIVE);

                g_signal_emit_by_name (chatrooms->jabber, 
                                       "chatroom-kicked", 
                                       id);

                leave_chatroom (chatrooms, chatroom);
            } else {
                gossip_debug (DEBUG_DOMAIN,
                              "ID[%d] Contact left:'%s'",
                              id,
                              gossip_contact_get_id (contact));

                gossip_chatroom_contact_left (chatroom, contact);
            }
        }
        break;

    case LM_MESSAGE_SUB_TYPE_ERROR:
        node = lm_message_node_get_child (m->node, "error");
        if (node) {
            GossipChatroomError  error;
            const gchar         *xmlns = NULL;
            const gchar         *code_str;
            gint                 code;
                        
            code_str = lm_message_node_get_attribute (node, "code");
            code = code_str ? atoi (code_str) : 0;

            node = lm_message_node_get_child (m->node, "x");
            if (node) {
                xmlns = lm_message_node_get_attribute (node, "xmlns");
            }
                        
            error = error_from_code (xmlns, code);
            gossip_debug (DEBUG_DOMAIN,
                          "ID[%d] %s", 
                          id, 
                          gossip_chatroom_error_to_string (error));
                        
            g_signal_emit_by_name (chatrooms->jabber,
                                   "chatroom-error",
                                   id, 
                                   error);
        }
        break;

    default:
        gossip_debug (DEBUG_DOMAIN, 
                      "Presence not handled for:'%s'",
                      gossip_jid_get_full (jid));
        break;
    }

    g_object_unref (jid);

    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static void
join_timeout_destroy_notify_cb (gpointer data)
{
    guint timeout_id;

    timeout_id = GPOINTER_TO_UINT (data);
    g_source_remove (timeout_id);
}

static gboolean
join_timeout_cb (GossipCallbackData *timeout_data)
{
    GossipJID             *jid;
    GossipChatroomId       id;
    GossipChatroom        *chatroom;
    GossipJabberChatrooms *chatrooms;
    GossipChatroomError    error;
    GossipCallbackData    *data;

    chatrooms = timeout_data->data1;
    chatroom = timeout_data->data2;

    g_hash_table_remove (chatrooms->join_timeouts, chatroom);

    id = gossip_chatroom_get_id (chatroom);
    gossip_debug (DEBUG_DOMAIN,
                  "ID[%d] Join timed out (internally)", 
                  id);

    /* Set chatroom status and error */
    error = GOSSIP_CHATROOM_ERROR_TIMED_OUT;

    gossip_chatroom_set_last_error (chatroom, error);
    gossip_chatroom_set_status (chatroom, GOSSIP_CHATROOM_STATUS_ERROR);

    /* Call callback */
    data = g_hash_table_lookup (chatrooms->join_callbacks, chatroom);

    if (data && data->callback) {
        GossipChatroomJoinCb   func;
        GossipJabberChatrooms *chatrooms;

        func = (GossipChatroomJoinCb) data->callback;
        chatrooms = (GossipJabberChatrooms *) data->data1;

        gossip_debug (DEBUG_DOMAIN,
                      "ID[%d] Calling back... (timed out)", 
                      id);
        (func) (GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
                id, 
                error, 
                data->user_data);
    }

    /* Clean up the user data */
    g_hash_table_remove (chatrooms->join_callbacks, chatroom);

    /* Clean up */
    jid = gossip_jid_new (gossip_chatroom_get_id_str (chatroom));
        
    g_hash_table_remove (chatrooms->chatrooms_by_id, GINT_TO_POINTER (id));
    g_hash_table_remove (chatrooms->chatrooms_by_pointer, chatroom);
    g_hash_table_remove (chatrooms->chatrooms_by_jid, jid);
        
    g_object_unref (jid);

    gossip_callback_data_free (timeout_data);

    return FALSE;
}

GossipChatroomId
gossip_jabber_chatrooms_join (GossipJabberChatrooms *chatrooms,
                              GossipChatroom        *chatroom,
                              GossipChatroomJoinCb   callback,
                              gpointer               user_data)
{
    LmMessage            *m;
    LmMessageNode        *node;
    GossipContact        *own_contact;
    GossipChatroomId      id;
    gchar                *id_str;
    const gchar          *jid_str;
    const gchar          *show = NULL;
    const gchar          *password;
    guint                 timeout_id;

    g_return_val_if_fail (chatrooms != NULL, 0);
    g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), 0);
    g_return_val_if_fail (callback != NULL, 0);

    if (g_hash_table_lookup (chatrooms->chatrooms_by_pointer, chatroom)) {
        /* Duplicate room already exists. */
        id = gossip_chatroom_get_id (chatroom);

        gossip_debug (DEBUG_DOMAIN, 
                      "ID[%d] Join chatroom:'%s', room already exists.",
                      id,
                      gossip_chatroom_get_room (chatroom));

        (callback) (GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
                    id, 
                    GOSSIP_CHATROOM_ERROR_ALREADY_OPEN,
                    user_data);

        return id;
    }

    jid_str = gossip_chatroom_get_id_str (chatroom);

    /* Get real chatroom. */
    id = gossip_chatroom_get_id (chatroom);

    gossip_debug (DEBUG_DOMAIN,
                  "ID[%d] Join chatroom:'%s' on server:'%s'",
                  id,
                  gossip_chatroom_get_room (chatroom),
                  gossip_chatroom_get_server (chatroom));

    /* Add timeout for server response. */
    timeout_id = g_timeout_add (JOIN_TIMEOUT,
                                (GSourceFunc) join_timeout_cb,
                                gossip_callback_data_new (NULL, NULL, chatrooms, chatroom, NULL));

    g_hash_table_insert (chatrooms->join_timeouts, 
                         g_object_ref (chatroom),
                         GUINT_TO_POINTER (timeout_id));
    g_hash_table_insert (chatrooms->join_callbacks, 
                         g_object_ref (chatroom),
                         gossip_callback_data_new (callback, user_data, chatrooms, NULL, NULL));
    g_hash_table_insert (chatrooms->chatrooms_by_id, 
                         GINT_TO_POINTER (id),
                         g_object_ref (chatroom));
    g_hash_table_insert (chatrooms->chatrooms_by_pointer, 
                         g_object_ref (chatroom), 
                         GINT_TO_POINTER (1));
    g_hash_table_insert (chatrooms->chatrooms_by_jid,
                         gossip_jid_new (jid_str),
                         g_object_ref (chatroom));

    gossip_chatroom_set_last_error (chatroom, GOSSIP_CHATROOM_ERROR_NONE);
    gossip_chatroom_set_status (chatroom, GOSSIP_CHATROOM_STATUS_JOINING);

    /* The other hash table inserts MUST occur before this, since
     * we check to see if the contact is a chatroom contact and
     * that does a hash tabe lookup.
     */
    own_contact = gossip_chatroom_get_own_contact (chatroom);

    /* Compose message. */
    m = lm_message_new_with_sub_type (gossip_contact_get_id (own_contact),
                                      LM_MESSAGE_TYPE_PRESENCE,
                                      LM_MESSAGE_SUB_TYPE_AVAILABLE);

    node = lm_message_node_add_child (m->node, "x", NULL);
    lm_message_node_set_attribute (node, "xmlns", XMPP_MUC_XMLNS);

    /* If we have a password, set one */
    password = gossip_chatroom_get_password (chatroom);
    if (!G_STR_EMPTY (password)) {
        lm_message_node_add_child (node, "password", password);
    }

    show = gossip_jabber_presence_state_to_str (chatrooms->presence);

    if (show) {
        lm_message_node_add_child (m->node, "show", show);
    }

    id_str = g_strdup_printf ("muc_join_%d", id);
    lm_message_node_set_attribute (m->node, "id", id_str);
    g_free (id_str);

    lm_connection_send (chatrooms->connection, m,  NULL);
    lm_message_unref (m);

    return id;
}

void
gossip_jabber_chatrooms_cancel (GossipJabberChatrooms *chatrooms,
                                GossipChatroomId       id)
{
    GossipChatroom     *chatroom;
    GossipJID          *jid;
    GossipCallbackData *data;

    g_return_if_fail (chatrooms != NULL);

    chatroom = g_hash_table_lookup (chatrooms->chatrooms_by_id, GINT_TO_POINTER (id));
    if (!chatroom) {
        return;
    }

    gossip_debug (DEBUG_DOMAIN,
                  "ID[%d] Cancel joining room", 
                  id);
        
    g_hash_table_remove (chatrooms->join_timeouts, chatroom);

    gossip_chatroom_set_last_error (chatroom, GOSSIP_CHATROOM_ERROR_NONE);
    gossip_chatroom_set_status (chatroom, GOSSIP_CHATROOM_STATUS_INACTIVE);

    data = g_hash_table_lookup (chatrooms->join_callbacks, chatroom);

    if (data && data->callback) {
        GossipChatroomJoinCb func;

        func = (GossipChatroomJoinCb) data->callback;

        gossip_debug (DEBUG_DOMAIN,
                      "ID[%d] Calling back... (cancelled)", 
                      id);
        (func) (GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
                id, 
                GOSSIP_CHATROOM_ERROR_CANCELED, 
                data->user_data);
    }

    /* Clean up the user data */
    g_hash_table_remove (chatrooms->join_callbacks, chatroom);

    jid = gossip_jid_new (gossip_chatroom_get_id_str (chatroom));
        
    g_hash_table_remove (chatrooms->chatrooms_by_id, GINT_TO_POINTER (id));
    g_hash_table_remove (chatrooms->chatrooms_by_pointer, chatroom);
    g_hash_table_remove (chatrooms->chatrooms_by_jid, jid);
        
    g_object_unref (jid);
}

void
gossip_jabber_chatrooms_send (GossipJabberChatrooms *chatrooms,
                              GossipChatroomId       id,
                              const gchar           *message)
{
    LmMessage      *m;
    GossipChatroom *chatroom;
    const gchar    *jid_str;

    g_return_if_fail (chatrooms != NULL);
    g_return_if_fail (message != NULL);

    chatroom = g_hash_table_lookup (chatrooms->chatrooms_by_id, 
                                    GINT_TO_POINTER (id));

    if (!chatroom) {
        /* FIXME: Should error this up? */
        g_warning ("Could not send message, unknown chatroom id:%d", id);
        return;
    }

    gossip_debug (DEBUG_DOMAIN,
                  "ID[%d] Send message", 
                  id);

    jid_str = gossip_chatroom_get_id_str (chatroom);
    m = lm_message_new_with_sub_type (jid_str, 
                                      LM_MESSAGE_TYPE_MESSAGE,
                                      LM_MESSAGE_SUB_TYPE_GROUPCHAT);
    lm_message_node_add_child (m->node, "body", message);

    lm_connection_send (chatrooms->connection, m, NULL);
    lm_message_unref (m);
}

void
gossip_jabber_chatrooms_change_subject (GossipJabberChatrooms *chatrooms,
                                        GossipChatroomId       id,
                                        const gchar           *new_subject)
{
    LmMessage      *m;
    GossipChatroom *chatroom;
    const gchar    *jid_str;

    g_return_if_fail (chatrooms != NULL);
    g_return_if_fail (new_subject != NULL);

    chatroom = g_hash_table_lookup (chatrooms->chatrooms_by_id, 
                                    GINT_TO_POINTER (id));

    if (!chatroom) {
        /* FIXME: Should error this up? */
        g_warning ("Could not change subject, unknown chatroom id:%d", id);
        return;
    }

    gossip_debug (DEBUG_DOMAIN,
                  "ID[%d] Change subject to:'%s'",
                  id,
                  new_subject);

    jid_str = gossip_chatroom_get_id_str (chatroom);
    m = lm_message_new_with_sub_type (jid_str,
                                      LM_MESSAGE_TYPE_MESSAGE,
                                      LM_MESSAGE_SUB_TYPE_GROUPCHAT);

    lm_message_node_add_child (m->node, "subject", new_subject);

    lm_connection_send (chatrooms->connection, m, NULL);
    lm_message_unref (m);
}

void
gossip_jabber_chatrooms_change_nick (GossipJabberChatrooms *chatrooms,
                                     GossipChatroomId       id,
                                     const gchar           *new_nick)
{
    LmMessage      *m;
    GossipChatroom *chatroom;
    GossipContact  *own_contact;
    gchar          *new_id;

    g_return_if_fail (chatrooms != NULL);
    g_return_if_fail (new_nick != NULL);

    chatroom = g_hash_table_lookup (chatrooms->chatrooms_by_id, GINT_TO_POINTER (id));

    if (!chatroom) {
        /* FIXME: Should error this up? */
        g_warning ("Could not change nick, unknown chatroom id:%d", id);
        return;
    }

    own_contact = gossip_chatroom_get_own_contact (chatroom);
        
    if (!own_contact) {
        /* FIXME: Should error this up? */
        g_warning ("Could not get own contact, unknown chatroom id:%d", id);
        return;
    }

    gossip_debug (DEBUG_DOMAIN,
                  "ID[%d] Change chatroom nick to:'%s'",
                  id, 
                  new_nick);
        
    /* NOTE: Don't change the nick until we get confirmation from
     * the server that it has changed.
     */
    new_id = get_new_id_for_new_nick (own_contact, new_nick);
    m = lm_message_new (new_id, LM_MESSAGE_TYPE_PRESENCE);
    g_free (new_id);

    lm_connection_send (chatrooms->connection, m, NULL);
    lm_message_unref (m);
}

void
gossip_jabber_chatrooms_leave (GossipJabberChatrooms *chatrooms,
                               GossipChatroomId       id)
{
    LmMessage      *m;
    GossipChatroom *chatroom;
    GossipContact  *own_contact;

    g_return_if_fail (chatrooms != NULL);

    chatroom = g_hash_table_lookup (chatrooms->chatrooms_by_id, GINT_TO_POINTER (id));

    if (!chatroom) {
        /* FIXME: Should error this up? */
        g_warning ("Could not leave chatroom, unknown chatroom id:%d", id);
        return;
    }

    own_contact = gossip_chatroom_get_own_contact (chatroom);
        
    if (!own_contact) {
        /* FIXME: Should error this up? */
        g_warning ("Could not get own contact, unknown chatroom id:%d", id);
        return;
    }

    gossip_debug (DEBUG_DOMAIN,
                  "ID[%d] Leaving chatroom:'%s'",
                  id,
                  gossip_chatroom_get_id_str (chatroom));

    m = lm_message_new_with_sub_type (gossip_contact_get_id (own_contact),
                                      LM_MESSAGE_TYPE_PRESENCE,
                                      LM_MESSAGE_SUB_TYPE_UNAVAILABLE);

    lm_connection_send (chatrooms->connection, m, NULL);
    lm_message_unref (m);

    leave_chatroom (chatrooms, chatroom);
}

void
gossip_jabber_chatrooms_kick (GossipJabberChatrooms *chatrooms,
                              GossipChatroomId       id,
                              GossipContact         *contact,
                              const gchar           *reason)
{
    LmMessage      *m;
    LmMessageNode  *node;
    GossipAccount  *account;
    GossipChatroom *chatroom;
    GossipJID      *jid;
    gchar          *from;
    const gchar    *contact_id;
    const gchar    *to;
    const gchar    *nick;

    g_return_if_fail (chatrooms != NULL);
    g_return_if_fail (GOSSIP_IS_CONTACT (contact));

    chatroom = g_hash_table_lookup (chatrooms->chatrooms_by_id, GINT_TO_POINTER (id));

    if (!chatroom) {
        /* FIXME: Should error this up? */
        g_warning ("Could not kick:'%s', unknown chatroom id:%d", 
                   gossip_contact_get_id (contact), 
                   id);
        return;
    }

    m = lm_message_new_with_sub_type (NULL,
                                      LM_MESSAGE_TYPE_IQ,
                                      LM_MESSAGE_SUB_TYPE_SET);

    account = gossip_chatroom_get_account (chatroom);
    from = g_strconcat (gossip_account_get_id (account),
                        "/",
                        gossip_account_get_resource (account),
                        NULL);
    lm_message_node_set_attribute (m->node, "from", from);
    g_free (from);

    to = gossip_chatroom_get_id_str (chatroom);
    lm_message_node_set_attribute (m->node, "to", to);

    node = lm_message_node_add_child (m->node, "query", NULL);
    lm_message_node_set_attributes (node, "xmlns", XMPP_MUC_ADMIN_XMLNS, NULL);

    contact_id = gossip_contact_get_id (contact);
    jid = gossip_jid_new (contact_id);
    nick = gossip_jid_get_resource (jid);

    node = lm_message_node_add_child (node, "item", reason);
    lm_message_node_set_attributes (node, 
                                    "nick", nick, 
                                    "role", "none",
                                    NULL);
    g_object_unref (jid);
        
    lm_connection_send (chatrooms->connection, m, NULL);
    lm_message_unref (m);
}

GossipChatroom *
gossip_jabber_chatrooms_find_by_id (GossipJabberChatrooms *chatrooms,
                                    GossipChatroomId       id)
{
    return g_hash_table_lookup (chatrooms->chatrooms_by_id, GINT_TO_POINTER (id));
}

GossipChatroom *
gossip_jabber_chatrooms_find (GossipJabberChatrooms *chatrooms,
                              GossipChatroom        *chatroom)
{
    /* FIXME: What is the point of this now? */
    if (g_hash_table_lookup (chatrooms->chatrooms_by_pointer, chatroom)) {
        return chatroom;
    }
        
    return NULL;
}

void
gossip_jabber_chatrooms_invite (GossipJabberChatrooms *chatrooms,
                                GossipChatroomId       id,
                                GossipContact         *contact,
                                const gchar           *reason)
{
    LmMessage      *m;
    LmMessageNode  *parent;
    LmMessageNode  *node;
    GossipChatroom *chatroom;
    GossipContact  *own_contact;

    g_return_if_fail (chatrooms != NULL);
    g_return_if_fail (GOSSIP_IS_CONTACT (contact));

    chatroom = g_hash_table_lookup (chatrooms->chatrooms_by_id, GINT_TO_POINTER (id));

    if (!chatroom) {
        /* FIXME: Should error this up? */
        g_warning ("Could not invite:'%s', unknown chatroom id:%d", 
                   gossip_contact_get_id (contact),
                   id);
        return;
    }

    own_contact = gossip_chatroom_get_own_contact (chatroom);
        
    if (!own_contact) {
        /* FIXME: Should error this up? */
        g_warning ("Could not get own contact, unknown chatroom id:%d", id);
        return;
    }

    gossip_debug (DEBUG_DOMAIN,
                  "ID[%d] Invitation to contact:'%s' from:'%s'",
                  id,
                  gossip_contact_get_id (contact),
                  gossip_contact_get_id (own_contact));

    m = lm_message_new (gossip_chatroom_get_id_str (chatroom),
                        LM_MESSAGE_TYPE_MESSAGE);
    lm_message_node_set_attributes (m->node,
                                    "from", gossip_contact_get_id (own_contact),
                                    NULL);

    parent = lm_message_node_add_child (m->node, "x", NULL);
    lm_message_node_set_attributes (parent, "xmlns", XMPP_MUC_USER_XMLNS, NULL);

    node = lm_message_node_add_child (parent, "invite", NULL);
    lm_message_node_set_attributes (node, "to", gossip_contact_get_id (contact), NULL);

    lm_message_node_add_child (node, "reason", reason);

    lm_connection_send (chatrooms->connection, m, NULL);
    lm_message_unref (m);
}

void
gossip_jabber_chatrooms_invite_accept (GossipJabberChatrooms *chatrooms,
                                       GossipChatroomJoinCb   callback,
                                       GossipChatroomInvite  *invite,
                                       const gchar           *nickname)
{
    GossipSession         *session;
    GossipChatroom        *chatroom;
    GossipChatroomManager *chatroom_manager;
    GossipContact         *contact;
    gchar                 *room = NULL;
    const gchar           *id;
    const gchar           *server;

    g_return_if_fail (chatrooms != NULL);
    g_return_if_fail (invite != NULL);
    g_return_if_fail (callback != NULL);

    id = gossip_chatroom_invite_get_id (invite);
    contact = gossip_chatroom_invite_get_inviter (invite);

    server = strstr (id, "@");

    g_return_if_fail (server != NULL);
    g_return_if_fail (nickname != NULL);

    if (server) {
        room = g_strndup (id, server - id);
        server++;
    }

    session = _gossip_jabber_get_session (chatrooms->jabber);
    chatroom_manager = gossip_session_get_chatroom_manager (session);
    chatroom = gossip_chatroom_manager_find_or_create (chatroom_manager, 
                                                       gossip_contact_get_account (contact), 
                                                       server, 
                                                       room,
                                                       NULL);

    gossip_chatroom_set_nick (chatroom, nickname);

    gossip_jabber_chatrooms_join (chatrooms,
                                  chatroom,
                                  callback,
                                  NULL);

    g_object_unref (chatroom);
    g_free (room);
}

void
gossip_jabber_chatrooms_invite_decline (GossipJabberChatrooms *chatrooms,
                                        GossipChatroomInvite  *invite,
                                        const gchar           *reason)
{
    LmMessage     *m;
    LmMessageNode *n;
    GossipContact *own_contact;
    GossipContact *contact;
    const gchar   *id;

    g_return_if_fail (chatrooms != NULL);
    g_return_if_fail (invite != NULL);

    own_contact = gossip_jabber_get_own_contact (chatrooms->jabber);
    contact = gossip_chatroom_invite_get_inviter (invite);
    id = gossip_chatroom_invite_get_id (invite);

    gossip_debug (DEBUG_DOMAIN,
                  "Invitation decline to:'%s' into room:'%s'",
                  gossip_contact_get_id (contact), 
                  id);

    m = lm_message_new (id, LM_MESSAGE_TYPE_MESSAGE);
    lm_message_node_set_attributes (m->node,
                                    "from", gossip_contact_get_id (own_contact),
                                    NULL);

    n = lm_message_node_add_child (m->node, "x", NULL);
    lm_message_node_set_attributes (n, "xmlns", XMPP_MUC_USER_XMLNS, NULL);

    n = lm_message_node_add_child (n, "decline", NULL);
    lm_message_node_set_attributes (n, "to", gossip_contact_get_id (contact), NULL);

    n = lm_message_node_add_child (n, "reason", reason);

    lm_connection_send (chatrooms->connection, m, NULL);
    lm_message_unref (m);
}

static void
get_rooms_foreach (gpointer key,
                   gpointer value,
                   gpointer user_data)
{
    GList **list;

    list = (GList **) user_data;
    *list = g_list_prepend (*list, key);
}

GList *
gossip_jabber_chatrooms_get_rooms (GossipJabberChatrooms *chatrooms)
{
    GList *list = NULL;

    g_return_val_if_fail (chatrooms != NULL, NULL);

    g_hash_table_foreach (chatrooms->chatrooms_by_id,
                          get_rooms_foreach,
                          &list);

    list = g_list_reverse (list);

    return list;
}

static void
jabber_chatrooms_browse_rooms_cb (GossipJabberDisco     *disco,
                                  GossipJabberDiscoItem *item,
                                  gboolean               last_item,
                                  gboolean               timeout,
                                  GError                *error,
                                  GossipCallbackData    *data)
{
    GossipJID             *jid = NULL;
    GossipJabberChatrooms *chatrooms;
    GossipChatroom        *chatroom = NULL;
    GList                 *list;
    gchar                 *server;

    if (timeout && !last_item) {
        return;
    }

    chatrooms = data->data1;
    server = data->data2;
    list = data->data3;

    if (item) {
        jid = gossip_jabber_disco_item_get_jid (item);
        chatroom = g_hash_table_lookup (chatrooms->chatrooms_by_jid, jid);
    }

    if (!chatroom && !timeout && !error) {
        GossipSession         *session;
        GossipChatroomManager *chatroom_manager;
        GossipAccount         *account;
        const gchar           *server;
        gchar                 *room;
                
        gossip_debug (DEBUG_DOMAIN, 
                      "Chatroom found on server not set up here, creating for:'%s'...",
                      gossip_jid_get_full (jid));

        account = gossip_jabber_get_account (chatrooms->jabber);
        server = gossip_jid_get_part_host (jid);
        room = gossip_jid_get_part_name (jid);

        /* Create new chatroom */
        session = _gossip_jabber_get_session (chatrooms->jabber);
        chatroom_manager = gossip_session_get_chatroom_manager (session);
        chatroom = gossip_chatroom_manager_find_or_create (chatroom_manager, 
                                                           account, 
                                                           server, 
                                                           room,
                                                           NULL);

        g_free (room);
    }

    if (chatroom) {
        GossipChatroomFeature  features = 0;
        LmMessageNode         *node;
        const gchar           *name;

        name = gossip_jabber_disco_item_get_name (item);
        gossip_chatroom_set_name (chatroom, name);

        /* Sort ouf the features */
        if (gossip_jabber_disco_item_has_feature (item, "muc_hidden")) { 
            features |= GOSSIP_CHATROOM_FEATURE_HIDDEN;
        }

        if (gossip_jabber_disco_item_has_feature (item, "muc_membersonly")) { 
            features |= GOSSIP_CHATROOM_FEATURE_MEMBERS_ONLY;
        }

        if (gossip_jabber_disco_item_has_feature (item, "muc_moderated")) { 
            features |= GOSSIP_CHATROOM_FEATURE_MODERATED;
        }

        if (gossip_jabber_disco_item_has_feature (item, "muc_nonanonymous")) { 
            features |= GOSSIP_CHATROOM_FEATURE_NONANONYMOUS;
        }

        if (gossip_jabber_disco_item_has_feature (item, "muc_open")) { 
            features |= GOSSIP_CHATROOM_FEATURE_OPEN;
        }

        if (gossip_jabber_disco_item_has_feature (item, "muc_passwordprotected")) { 
            features |= GOSSIP_CHATROOM_FEATURE_PASSWORD_PROTECTED;
        }

        if (gossip_jabber_disco_item_has_feature (item, "muc_persistent")) { 
            features |= GOSSIP_CHATROOM_FEATURE_PERSISTENT;
        }

        if (gossip_jabber_disco_item_has_feature (item, "muc_public")) { 
            features |= GOSSIP_CHATROOM_FEATURE_PUBLIC;
        }

        if (gossip_jabber_disco_item_has_feature (item, "muc_semianonymous")) { 
            features |= GOSSIP_CHATROOM_FEATURE_SEMIANONYMOUS;
        }

        if (gossip_jabber_disco_item_has_feature (item, "muc_temporary")) { 
            features |= GOSSIP_CHATROOM_FEATURE_TEMPORARY;
        }

        if (gossip_jabber_disco_item_has_feature (item, "muc_unmoderated")) { 
            features |= GOSSIP_CHATROOM_FEATURE_PERSISTENT;
        }

        if (gossip_jabber_disco_item_has_feature (item, "muc_unsecured")) { 
            features |= GOSSIP_CHATROOM_FEATURE_UNSECURED;
        }

        gossip_chatroom_set_features (chatroom, features);

        /* Get the MUC specific data */
        node = gossip_jabber_disco_item_get_data (item);
        if (node) {
            node = node->children;

            while (node) {
                if (node->name && strcmp (node->name, "field") == 0) {
                    const gchar *var;
                    const gchar *val;

                    var = lm_message_node_get_attribute (node, "var");
                    val = lm_message_node_get_value (node->children);

                    if (var && val) {
                        if (strcmp (var, "muc#roominfo_description") == 0) {
                            gossip_chatroom_set_description (chatroom, val);
                        } 
                        else if (strcmp (var, "muc#roominfo_subject") == 0) {
                            gossip_chatroom_set_subject (chatroom, val);
                        }
                        else if (strcmp (var, "muc#roominfo_occupants") == 0) {
                            gossip_chatroom_set_occupants (chatroom, atoi (val));
                        }
                    }
                }

                node = node->next;
            }
        }

        gossip_debug (DEBUG_DOMAIN, 
                      "Chatroom:'%s' added to list found on server:'%s'...",
                      gossip_chatroom_get_room (chatroom),
                      gossip_chatroom_get_server (chatroom));

        list = g_list_prepend (list, chatroom);
        data->data3 = list;
    }

    if (last_item) {
        GossipChatroomBrowseCb callback;
                
        callback = data->callback;
        (callback)(GOSSIP_CHATROOM_PROVIDER (chatrooms->jabber),
                   server, list, error, data->user_data);

        g_list_foreach (list, (GFunc) g_object_unref, NULL);
        g_list_free (list);

        g_free (server);

        gossip_callback_data_free (data);
    }
}

void
gossip_jabber_chatrooms_browse_rooms (GossipJabberChatrooms  *chatrooms,
                                      const gchar            *server,
                                      GossipChatroomBrowseCb  callback,
                                      gpointer                user_data)
{
    GossipJabberDisco  *disco;
    GossipCallbackData *data;
        
    data = gossip_callback_data_new (callback, 
                                     user_data, 
                                     chatrooms, 
                                     g_strdup (server), 
                                     NULL);
        
    disco = gossip_jabber_disco_request (chatrooms->jabber,
                                         server,
                                         (GossipJabberDiscoItemFunc) 
                                         jabber_chatrooms_browse_rooms_cb,
                                         data);
}

static void
jabber_chatrooms_set_presence_foreach (gpointer key,
                                       gpointer value,
                                       gpointer user_data)
{
    LmConnection          *connection;
    LmMessage             *m;
    GossipJabberChatrooms *chatrooms;
    GossipChatroom        *chatroom;
    GossipPresence        *presence;
    const gchar           *show;
    const gchar           *status;

    chatroom = GOSSIP_CHATROOM (key);
    chatrooms = (GossipJabberChatrooms *) user_data;

    connection = chatrooms->connection;
    presence = chatrooms->presence;

    show = gossip_jabber_presence_state_to_str (presence);
    status = gossip_presence_get_status (presence);

    m = lm_message_new_with_sub_type (gossip_chatroom_get_id_str (chatroom),
                                      LM_MESSAGE_TYPE_PRESENCE,
                                      LM_MESSAGE_SUB_TYPE_AVAILABLE);

    if (show) {
        lm_message_node_add_child (m->node, "show", show);
    }

    if (status) {
        lm_message_node_add_child (m->node, "status", status);
    }

    lm_connection_send (connection, m, NULL);
    lm_message_unref (m);
}

void
gossip_jabber_chatrooms_set_presence (GossipJabberChatrooms  *chatrooms,
                                      GossipPresence         *presence)
{
    g_return_if_fail (chatrooms != NULL);

    if (chatrooms->presence) {
        g_object_unref (chatrooms->presence);
    }

    chatrooms->presence = g_object_ref (presence);

    g_hash_table_foreach (chatrooms->chatrooms_by_pointer,
                          jabber_chatrooms_set_presence_foreach,
                          chatrooms);
}

gboolean
gossip_jabber_chatrooms_get_jid_is_chatroom (GossipJabberChatrooms *chatrooms,
                                             const gchar           *jid_str)
{
    GossipJID *jid;
    gboolean   ret_val = FALSE;

    if (!chatrooms->chatrooms_by_jid) {
        return FALSE;
    }

    jid = gossip_jid_new (jid_str);

    if (g_hash_table_lookup (chatrooms->chatrooms_by_jid, jid)) {
        ret_val = TRUE;
    }

    g_object_unref (jid);

    return ret_val;
}
