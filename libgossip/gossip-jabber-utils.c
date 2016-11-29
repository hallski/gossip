/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
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
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 */

#include <string.h>
#include <stdlib.h>

#include <glib/gi18n.h>

#include "gossip-debug.h"
#include "gossip-presence.h"
#include "gossip-time.h"
#include "gossip-chatroom.h"
#include "gossip-jabber.h"
#include "gossip-jabber-utils.h"
#include "gossip-jid.h"
#include "gossip-utils.h"

#define DEBUG_DOMAIN "JabberUtils"

GossipJabberAsyncData *
gossip_jabber_async_data_new (GossipJabber        *jabber,
                              GossipErrorCallback  callback,
                              gpointer             user_data)
{
    GossipJabberAsyncData *ad;

    ad = g_new0 (GossipJabberAsyncData, 1);

    ad->jabber = g_object_ref (jabber);

    ad->callback = callback;
    ad->user_data = user_data;

    return ad;
}

void
gossip_jabber_async_data_free (GossipJabberAsyncData *ad)
{
    if (!ad) {
        return;
    }

    if (ad->jabber) {
        g_object_unref (ad->jabber);
    }

    g_free (ad);
}

const gchar *
gossip_jabber_presence_state_to_str (GossipPresence *presence)
{
    switch (gossip_presence_get_state (presence)) {
    case GOSSIP_PRESENCE_STATE_BUSY:
        return "dnd";
    case GOSSIP_PRESENCE_STATE_AWAY:
        return "away";
    case GOSSIP_PRESENCE_STATE_EXT_AWAY:
        return "xa";
    default:
        return NULL;
    }

    return NULL;
}

GossipPresenceState
gossip_jabber_presence_state_from_str (const gchar *str)
{
    if (!str || !str[0]) {
        return GOSSIP_PRESENCE_STATE_AVAILABLE;
    } else if (strcmp (str, "dnd") == 0) {
        return GOSSIP_PRESENCE_STATE_BUSY;
    } else if (strcmp (str, "away") == 0) {
        return GOSSIP_PRESENCE_STATE_AWAY;
    } else if (strcmp (str, "xa") == 0) {
        return GOSSIP_PRESENCE_STATE_EXT_AWAY;
    } else if (strcmp (str, "chat") == 0) {
        /* We don't support chat, so treat it like available. */
        return GOSSIP_PRESENCE_STATE_AVAILABLE;
    }

    return GOSSIP_PRESENCE_STATE_AVAILABLE;
}

GossipTime
gossip_jabber_get_message_timestamp (LmMessage *m)
{
    LmMessageNode *node;
    const gchar   *xmlns;
    const gchar   *stamp;
    GossipTime     t;

    g_return_val_if_fail (m != NULL, gossip_time_get_current ());
    g_return_val_if_fail (m->node != NULL, gossip_time_get_current ());

    stamp = NULL;
    for (node = m->node->children; node && node->name; node = node->next) {
        if (strcmp (node->name, "x") != 0) {
            continue;
        }

        xmlns = lm_message_node_get_attribute (node, "xmlns");
        if (xmlns && strcmp (xmlns, "jabber:x:delay") == 0) {
            stamp = lm_message_node_get_attribute (node, "stamp");
            break;
        }
    }

    if (!stamp) {
        return gossip_time_get_current ();
    }

    t = gossip_time_parse (stamp);
    if (!t) {
        return gossip_time_get_current ();
    }

    return t;
}

GossipChatroomInvite *
gossip_jabber_get_message_conference (GossipJabber *jabber,
                                      LmMessage    *m)
{
    LmMessageNode        *node;
    GossipChatroomInvite *invite;
    GossipContact        *contact;
    const gchar          *contact_id;
    const gchar          *id;
    const gchar          *reason = NULL;

    g_return_val_if_fail (m != NULL, NULL);
    g_return_val_if_fail (m->node != NULL, NULL);
    g_return_val_if_fail (m->node->children != NULL, NULL);

    node = lm_message_node_find_child (m->node, "invite");
    if (!node) {
        return NULL;
    }

    contact_id = lm_message_node_get_attribute (node, "from");
    contact = gossip_jabber_get_contact_from_jid (jabber,
                                                  contact_id,
                                                  FALSE,
                                                  FALSE,
                                                  TRUE);

    id = lm_message_node_get_attribute (m->node, "from");

    node = lm_message_node_find_child (node, "reason");
    if (node) {
        reason = lm_message_node_get_value (node);
    }

    invite = gossip_chatroom_invite_new (contact, id, reason);
    g_object_unref (contact);

    return invite;
}

gboolean
gossip_jabber_get_message_is_event (LmMessage *m)
{
    LmMessageNode *node;
    const gchar   *xmlns;

    g_return_val_if_fail (m != NULL, FALSE);
    g_return_val_if_fail (m->node != NULL, FALSE);
    g_return_val_if_fail (m->node->children != NULL, FALSE);

    if (lm_message_node_find_child (m->node, "body")) {
        return FALSE;
    }

    for (node = m->node->children; node && node->name; node = node->next) {
        if (strcmp (node->name, "x") != 0) {
            continue;
        }

        xmlns = lm_message_node_get_attribute (node, "xmlns");
        if (xmlns && strcmp (xmlns, "jabber:x:event") == 0) {
            return TRUE;
        }
    }

    return FALSE;
}

gboolean
gossip_jabber_get_message_is_composing (LmMessage *m)
{
    LmMessageNode *node;
    const gchar   *xmlns;

    g_return_val_if_fail (m != NULL, FALSE);
    g_return_val_if_fail (m->node != NULL, FALSE);
    g_return_val_if_fail (m->node->children != NULL, FALSE);

    for (node = m->node->children; node && node->name; node = node->next) {
        if (strcmp (node->name, "x") != 0) {
            continue;
        }

        xmlns = lm_message_node_get_attribute (node, "xmlns");
        if (xmlns && strcmp (xmlns, "jabber:x:event") == 0) {
            LmMessageNode *composing;

            composing = lm_message_node_find_child (node, "composing");
            if (composing) {
                return TRUE;
            }
        }
    }

    return FALSE;
}

static GArray *
get_status (LmMessage *m)
{
    LmMessageNode *node;
    GArray        *status = NULL;

    if (!m) {
        return NULL;
    }

    node = lm_message_node_get_child (m->node, "x");
    if (!node) {
        return NULL;
    }

    node = node->children;

    while (node) {
        if (!node) {
            break;
        }

        if (node->name && strcmp (node->name, "status") == 0) {
            const gchar *code;

            code = lm_message_node_get_attribute (node, "code");
            if (code) {
                gint value;

                if (!status) {
                    status = g_array_new (FALSE, FALSE, sizeof (gint));
                }

                value = atoi (code);
                g_array_append_val (status, value);
            }
        }

        node = node->next;
    }

    return status;
}

gboolean 
gossip_jabber_get_message_has_status (LmMessage *m,
                                      gint       code)
{
    GArray   *codes;
    gboolean  found;
    gint      i;

    g_return_val_if_fail (m != NULL, FALSE);
    g_return_val_if_fail (code > 0, FALSE);

    codes = get_status (m);
    if (!codes) {
        return FALSE;
    }

    for (i = 0, found = FALSE; i < codes->len && !found; i++) {
        if (g_array_index (codes, gint, i) == code) {
            found = TRUE;
        }
    }

    g_array_free (codes, TRUE);
        
    return found;
}

gboolean
gossip_jabber_get_message_is_muc_new_nick (LmMessage  *m, 
                                           gchar     **new_nick)
{
    LmMessageNode *node;
    const gchar   *str;

    g_return_val_if_fail (m != NULL, FALSE);

    if (new_nick) {
        *new_nick = NULL;
    }

    /* 303 = nick changed */
    if (!gossip_jabber_get_message_has_status (m, 303)) {
        return FALSE;
    }
        
    if (!new_nick) {
        return TRUE;
    }

    node = lm_message_get_node (m);

    node = lm_message_node_find_child (node, "x");
    if (!node) {
        return FALSE;
    }

    str = lm_message_node_get_attribute (node, "xmlns");
    if (!str || strcmp (str, "http://jabber.org/protocol/muc#user") != 0) {
        return FALSE;
    }

    node = lm_message_node_find_child (node, "item");
    if (!node) {
        return FALSE;
    }

    /* Get old nick */
    str = lm_message_node_get_attribute (node, "nick");

    *new_nick = g_strdup (str);

    return TRUE;
}

gchar *
gossip_jabber_get_name_to_use (const gchar *jid_str,
                               const gchar *nickname,
                               const gchar *full_name,
                               const gchar *current_name)
{
    if (!G_STR_EMPTY (current_name)) {
        gchar     *part_name;
        gboolean   use_current_name;

        part_name = gossip_jid_string_get_part_name (jid_str);
                
        use_current_name = 
            g_ascii_strcasecmp (jid_str, current_name) != 0 && 
            g_ascii_strcasecmp (part_name, current_name) != 0;

        g_free (part_name); 

        if (use_current_name) {
            return g_strdup (current_name);
        }
    }

    if (!G_STR_EMPTY (nickname)) {
        return g_strdup (nickname);
    }

    if (!G_STR_EMPTY (full_name)) {
        return g_strdup (full_name);
    }

    if (!G_STR_EMPTY (jid_str)) {
        return gossip_jid_string_get_part_name (jid_str);
    }

    return g_strdup ("");
}

gchar *
gossip_jabber_get_display_id (const gchar *jid_str)
{
    gchar *unescaped;
    gchar *at;

    /* For more information about this,
     * see XEP-0106: JID Escaping. 
     */
        
    if (G_STR_EMPTY (jid_str)) {
        return g_strdup (jid_str);
    }

    unescaped = gossip_jid_string_unescape (jid_str);
        
    /* Find first '@' */
    at = strchr (unescaped, '@');

    if (!at) {
        return unescaped;
    }

    /* If we find another '@', it is likely the ID is something
     * like "martyn@hotmail.com@msn.jabber.org". In this case, we
     * drop anything after the second '@'. 
     *
     * This feels like a hack, not sure if there is a better way
     * round it. I thought you could query the service itself for
     * this stuff - msn.jabber.org for example.
     */
    at = strchr (at + 1, '@');
        
    if (at) {   
        *at = '\0';
    }

    return unescaped;
}

GError *
gossip_jabber_error_create (GossipJabberError  code,
                            const gchar       *reason)
{
    static GQuark  quark = 0;
    GError        *error;

    if (!quark) {
        quark = g_quark_from_static_string ("gossip-jabber");
    }

    error = g_error_new_literal (quark, code, reason);
    return error;
}

void
gossip_jabber_error (GossipJabber      *jabber, 
                     GossipJabberError  code)
{
    GossipAccount *account;
    GError        *error;
    const gchar   *message;

    g_return_if_fail (GOSSIP_IS_JABBER (jabber));

    account = gossip_jabber_get_account (jabber);

    message = gossip_jabber_error_to_string (code);
    gossip_debug (DEBUG_DOMAIN, "Error:%d->'%s'", code, message);

    error = gossip_jabber_error_create (code, message);
    g_signal_emit_by_name (jabber, "error", account, error);
    g_error_free (error);
}

const gchar *
gossip_jabber_error_to_string (GossipJabberError error)
{
    const gchar *str = _("An unknown error occurred.");

    switch (error) {
    case GOSSIP_JABBER_NO_CONNECTION:
        str = _("Connection refused.");
        break;
    case GOSSIP_JABBER_NO_SUCH_HOST:
        str = _("Server address could not be resolved.");
        break;
    case GOSSIP_JABBER_NO_PASSWORD:
        str = _("Account requires a password to authenticate and none was given.");
        break;
    case GOSSIP_JABBER_TIMED_OUT:
        str = _("Connection timed out.");
        break;
    case GOSSIP_JABBER_AUTH_FAILED:
        str = _("Authentication failed.");
        break;
    case GOSSIP_JABBER_DUPLICATE_USER:
        str = _("The username you are trying already exists.");
        break;
    case GOSSIP_JABBER_INVALID_USER:
        str = _("The username you are trying is not valid.");
        break;
    case GOSSIP_JABBER_UNAVAILABLE:
        str = _("This feature is unavailable.");
        break;
    case GOSSIP_JABBER_UNAUTHORIZED:
        str = _("This feature is unauthorized.");
        break;
    case GOSSIP_JABBER_SPECIFIC_ERROR:
        str = _("A specific protocol error occurred that was unexpected.");
        break;
    }

    return str;
}

