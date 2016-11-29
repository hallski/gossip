/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
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
 */

#include <config.h>

#include <ctype.h>
#include <string.h>

#include <libgossip/gossip-utils.h>

#include "gossip-jid.h"

#define GOSSIP_JID_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_JID, GossipJIDPrivate))

typedef struct _GossipJIDPrivate GossipJIDPrivate;

struct _GossipJIDPrivate {
    gchar       *full;
    gchar       *no_resource;
    const gchar *resource;
};

static void         gossip_jid_class_init (GossipJIDClass *class);
static void         gossip_jid_init       (GossipJID      *jid);
static void         gossip_jid_finalize   (GObject        *object);
static void         jid_get_property      (GObject        *object,
                                           guint           param_id,
                                           GValue         *value,
                                           GParamSpec     *pspec);
static const gchar *jid_locate_resource   (const gchar    *str);

enum {
    PROP_0,
    PROP_FULL,
    PROP_WITHOUT_RESOURCE,
    PROP_RESOURCE
};

G_DEFINE_TYPE (GossipJID, gossip_jid, G_TYPE_OBJECT);

static void
gossip_jid_class_init (GossipJIDClass *class)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (class);

    object_class->finalize     = gossip_jid_finalize;
    object_class->get_property = jid_get_property;

    g_object_class_install_property (object_class,
                                     PROP_FULL,
                                     g_param_spec_string ("full",
                                                          "Full JID",
                                                          "Full JID",
                                                          NULL,
                                                          G_PARAM_READABLE));
    g_object_class_install_property (object_class,
                                     PROP_WITHOUT_RESOURCE,
                                     g_param_spec_string ("without-resource",
                                                          "JID without the resource",
                                                          "JID without the resource",
                                                          NULL,
                                                          G_PARAM_READABLE));
    g_object_class_install_property (object_class,
                                     PROP_RESOURCE,
                                     g_param_spec_string ("resource",
                                                          "Resource",
                                                          "Resource",
                                                          NULL,
                                                          G_PARAM_READABLE));

    g_type_class_add_private (object_class, sizeof (GossipJIDPrivate));
}

static void
gossip_jid_init (GossipJID *jid)
{
}

static void
gossip_jid_finalize (GObject *object)
{
    GossipJIDPrivate *priv;

    priv = GOSSIP_JID_GET_PRIVATE (object);

    g_free (priv->full);
    g_free (priv->no_resource);

    (G_OBJECT_CLASS (gossip_jid_parent_class)->finalize) (object);
}

static void
jid_get_property (GObject    *object,
                  guint       param_id,
                  GValue     *value,
                  GParamSpec *pspec)
{
    GossipJIDPrivate *priv;

    priv = GOSSIP_JID_GET_PRIVATE (object);

    switch (param_id) {
    case PROP_FULL:
        g_value_set_string (value, priv->full);
        break;
    case PROP_WITHOUT_RESOURCE:
        g_value_set_string (value, priv->no_resource);
        break;
    case PROP_RESOURCE:
        g_value_set_string (value, priv->resource);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static const gchar *
jid_locate_resource (const gchar *str)
{
    gchar *ch;

    ch = strchr (str, '/');
    if (ch) {
        return (const gchar *) (ch + 1);
    }

    return NULL;
}

/* Casefolds the node part (the part before @). */
static gchar *
jid_casefold_node (const gchar *str)
{
    gchar       *tmp;
    gchar       *ret;
    const gchar *at;

    at = strchr (str, '@');
    if (!at) {
        return g_strdup (str);
    }

    tmp = g_utf8_casefold (str, at - str);
    ret = g_strconcat (tmp, at, NULL);
    g_free (tmp);

    return ret;
}

GossipJID *
gossip_jid_new (const gchar *id)
{
    GossipJID     *jid;
    GossipJIDPrivate *priv;

    g_return_val_if_fail (id != NULL, NULL);

    jid = g_object_new (GOSSIP_TYPE_JID, NULL);
        
    priv = GOSSIP_JID_GET_PRIVATE (jid);

    priv->full = jid_casefold_node (id);
    priv->resource = jid_locate_resource (priv->full);

    if (priv->resource) {
        priv->no_resource = g_strndup (priv->full, 
                                       priv->resource - 1 - priv->full);
    } else {
        priv->no_resource = g_strdup (priv->full);
    }

    g_object_notify (G_OBJECT (jid), "full");
    g_object_notify (G_OBJECT (jid), "resource");
    g_object_notify (G_OBJECT (jid), "without-resource");

    return jid;
}

void
gossip_jid_set_without_resource (GossipJID   *jid, 
                                 const gchar *str)
{
    GossipJIDPrivate *priv;
    gchar         *resource = NULL;

    g_return_if_fail (GOSSIP_IS_JID (jid));

    priv = GOSSIP_JID_GET_PRIVATE (jid);

    if (priv->resource) {
        resource = g_strdup (priv->resource);
    }

    g_free (priv->full);
    g_free (priv->no_resource);

    priv->no_resource = jid_casefold_node (str);

    if (resource) {
        priv->full = g_strdup_printf ("%s/%s",
                                      priv->no_resource, 
                                      resource);

        g_free (resource);
        priv->resource = jid_locate_resource (priv->full);

        g_object_notify (G_OBJECT (jid), "full");
        g_object_notify (G_OBJECT (jid), "resource");
    } else {
        priv->full = g_strdup (priv->no_resource);
        g_object_notify (G_OBJECT (jid), "full");
    }

    g_object_notify (G_OBJECT (jid), "without-resource");
}

void
gossip_jid_set_resource (GossipJID   *jid, 
                         const gchar *resource)
{
    GossipJIDPrivate *priv;

    g_return_if_fail (GOSSIP_IS_JID (jid));

    priv = GOSSIP_JID_GET_PRIVATE (jid);

    g_free (priv->full);

    priv->full = g_strdup_printf ("%s/%s", priv->no_resource, resource);
    priv->resource = jid_locate_resource (priv->full);

    g_object_notify (G_OBJECT (jid), "full");
    g_object_notify (G_OBJECT (jid), "resource");
}

const gchar *
gossip_jid_get_full (GossipJID *jid)
{
    GossipJIDPrivate *priv;

    g_return_val_if_fail (GOSSIP_IS_JID (jid), "");

    priv = GOSSIP_JID_GET_PRIVATE (jid);

    return priv->full;
}

const gchar *
gossip_jid_get_without_resource (GossipJID *jid)
{
    GossipJIDPrivate *priv;

    g_return_val_if_fail (GOSSIP_IS_JID (jid), "");

    priv = GOSSIP_JID_GET_PRIVATE (jid);

    if (priv->no_resource) {
        return priv->no_resource;
    }

    return priv->full;
}

const gchar *
gossip_jid_get_resource (GossipJID *jid)
{
    GossipJIDPrivate *priv;

    g_return_val_if_fail (GOSSIP_IS_JID (jid), NULL);

    priv = GOSSIP_JID_GET_PRIVATE (jid);

    if (priv->resource) {
        return priv->resource;
    }

    return NULL;
}

gboolean
gossip_jid_is_service (GossipJID *jid)
{
    GossipJIDPrivate *priv;
    gchar         *ch;

    g_return_val_if_fail (GOSSIP_IS_JID (jid), FALSE);

    /* This basically checks to see if there is an '@' sign in the
     * jid, if not, we assume it is a component or service (for
     * example msn.jabber.org.uk).
     */

    priv = GOSSIP_JID_GET_PRIVATE (jid);

    ch = strchr (priv->full, '@');
    if (!ch) {
        return TRUE;
    } else {
        return FALSE;
    }
}

gchar *
gossip_jid_get_part_name (GossipJID *jid)
{
    GossipJIDPrivate *priv;
    gchar         *ch;

    g_return_val_if_fail (GOSSIP_IS_JID (jid), g_strdup (""));

    priv = GOSSIP_JID_GET_PRIVATE (jid);

    for (ch = priv->full; *ch; ++ch) {
        if (*ch == '@') {
            return g_strndup (priv->full, ch - priv->full);
        }
    }

    return g_strdup ("");
}

const gchar *
gossip_jid_get_part_host (GossipJID *jid)
{
    const gchar *ch;

    g_return_val_if_fail (GOSSIP_IS_JID (jid), g_strdup (""));

    for (ch = gossip_jid_get_without_resource (jid); *ch; ++ch) {
        if (*ch == '@') {
            return ch + 1;
        }
    }

    return "";
}

gboolean
gossip_jid_equals (GossipJID *jid_a,
                   GossipJID *jid_b)
{
    GossipJIDPrivate *priv_a;
    GossipJIDPrivate *priv_b;

    g_return_val_if_fail (GOSSIP_IS_JID (jid_a), FALSE);
    g_return_val_if_fail (GOSSIP_IS_JID (jid_b), FALSE);

    /* NOTE: This is not strictly correct, since the node and resource are
     * UTF8, and the domain have other rules. The node is also already
     * casefolded.
     */

    priv_a = GOSSIP_JID_GET_PRIVATE (jid_a);
    priv_b = GOSSIP_JID_GET_PRIVATE (jid_b);

    if (g_ascii_strcasecmp (priv_a->full, priv_b->full) == 0) {
        return TRUE;
    }

    return FALSE;
}

gboolean
gossip_jid_equals_without_resource (GossipJID *jid_a,
                                    GossipJID *jid_b)
{
    const gchar *a, *b;

    g_return_val_if_fail (GOSSIP_IS_JID (jid_a), FALSE);
    g_return_val_if_fail (GOSSIP_IS_JID (jid_b), FALSE);

    a = gossip_jid_get_without_resource (jid_a);
    b = gossip_jid_get_without_resource (jid_b);

    /* NOTE: This is not strictly correct, since the node and resource are
     * UTF8, and the domain have other rules. The node is also already
     * casefolded.
     */
    if (g_ascii_strcasecmp (a, b) == 0) {
        return TRUE;
    }

    return FALSE;
}

gboolean
gossip_jid_string_is_valid (const gchar *str,
                            gboolean     with_resource)
{
    const gchar *at;
    const gchar *dot;
    const gchar *slash;
    gint         len;

    if (!str || strlen (str) < 1) {
        return FALSE;
    }

    len = strlen (str);

    /* check for the '@' sign and make sure it isn't at the start
       of the string or the last character */
    at = strchr (str, '@');
    if (!at ||
        at == str ||
        at == str + len - 1) {
        return FALSE;
    }

    /* check for the '.' character and if it exists make sure it
       is not directly after the '@' sign or the last character. */
    dot = strchr (at, '.');
    if (dot == at + 1 ||
        dot == str + len - 1 ||
        dot == str + len - 2) {
        return FALSE;
    }

    /* check the '/' character exists (if we are checking with
       resource) and make sure it is not after the '@' sign or the
       last character */
    slash = strchr (at, '/');
    if (with_resource &&
        (slash == NULL ||
         slash == at + 1 ||
         slash == str + len - 1)) {
        return FALSE;
    }

    /* if slash exists and we are expecting a JID without the
       resource then we return FALSE */
    if (!with_resource && slash) {
        return FALSE;
    }

    return TRUE;
}

gchar *
gossip_jid_string_get_part_name (const gchar *str)
{
    const gchar *ch;

    g_return_val_if_fail (str != NULL, "");

    for (ch = str; *ch; ++ch) {
        if (*ch == '@') {
            return g_strndup (str, ch - str);
        }
    }

    return g_strdup ("");
}

gchar *
gossip_jid_string_get_part_host (const gchar *str)
{
    const gchar *r_loc;
    const gchar *ch;

    g_return_val_if_fail (str != NULL, "");

    r_loc = gossip_jid_string_get_part_resource (str);
    for (ch = str; *ch; ++ch) {
        if (*ch == '@') {
            ch++;

            if (r_loc) {
                return g_strndup (ch, r_loc - 1 - ch);
            }

            return g_strdup (ch);
        }
    }

    return g_strdup ("");
}

const gchar *
gossip_jid_string_get_part_resource (const gchar *str)
{
    gchar *ch;

    ch = strchr (str, '/');
    if (ch) {
        return (const gchar *) (ch + 1);
    }

    return NULL;
}

gchar *                
gossip_jid_string_escape (const gchar *jid_str)
{
    GString *s;
    gchar    c;

    /* Conversions are:
     *
     *   <space>  \20 *
     *   "            \22
     *   &            \26
     *   '            \27
     *   /            \2f
     *   :            \3a
     *   <            \3c
     *   >            \3e
     *   @            \40
     *   \            \5c
     */
        
    if (!jid_str) {
        return NULL;
    }
        
    s = g_string_new_len ("", strlen (jid_str) + 1);
        
    while ((c = *jid_str++)) {
        switch (c) {
        case ' ':
            s = g_string_append (s, "\20");
            break;
        case '"':
            s = g_string_append (s, "\22");
            break;
        case '&':
            s = g_string_append (s, "\26");
            break;
        case '\'':
            s = g_string_append (s, "\27");
            break;
        case '/':
            s = g_string_append (s, "\2f");
            break;
        case ':':
            s = g_string_append (s, "\3a");
            break;
        case '<':
            s = g_string_append (s, "\3c");
            break;
        case '>':
            s = g_string_append (s, "\3e");
            break;
        case '@':
            s = g_string_append (s, "\40");
            break;
        case '\\':
            s = g_string_append (s, "\5c");
            break;
        default:
            s = g_string_append_c (s, c);
            break;
        }

    }
        
    return g_string_free (s, FALSE);
}

gchar *                
gossip_jid_string_unescape (const gchar *jid_str)
{
    gchar *text, *p, c;

    /* Conversions are:
     *
     *   <space>  \20 *
     *   "            \22
     *   &            \26
     *   '            \27
     *   /            \2f
     *   :            \3a
     *   <            \3c
     *   >            \3e
     *   @            \40
     *   \            \5c
     */
        
    if (!jid_str) {
        return NULL;
    }
        
    p = text = g_malloc (strlen (jid_str) + 1);
        
    while ((c = *jid_str++)) {
        if (G_LIKELY (c != '\\')) {
            *p++ = c;
            continue;
        }

        if (!memcmp (jid_str, "20", 2)) {
            *p++ = ' ';
            jid_str += 2;
        } else if (!memcmp (jid_str, "22", 2)) {
            *p++ = '"';
            jid_str += 2;
        } else if (!memcmp (jid_str, "26", 2)) {
            *p++ = '&';
            jid_str += 2;
        } else if (!memcmp (jid_str, "27", 2)) {
            *p++ = '\'';
            jid_str += 2;
        } else if (!memcmp (jid_str, "2f", 2)) {
            *p++ = '/';
            jid_str += 2;
        } else if (!memcmp (jid_str, "3a", 2)) {
            *p++ = ':';
            jid_str += 2;
        } else if (!memcmp (jid_str, "3c", 2)) {
            *p++ = '<';
            jid_str += 2;
        } else if (!memcmp (jid_str, "3e", 2)) {
            *p++ = '>';
            jid_str += 2;
        } else if (!memcmp (jid_str, "40", 2)) {
            *p++ = '@';
            jid_str += 2;
        } else if (!memcmp (jid_str, "5c", 2)) {
            *p++ = '\\';
            jid_str += 2;
        }
    }

    *p = 0;
        
    return text;
}

gint
gossip_jid_case_compare (gconstpointer a,
                         gconstpointer b)
{
    const gchar *str_a, *str_b;

    str_a = gossip_jid_get_without_resource ((GossipJID *) a);
    str_b = gossip_jid_get_without_resource ((GossipJID *) b);

    return gossip_strncasecmp (str_a, str_b, -1);
}

gboolean
gossip_jid_equal (gconstpointer v1,
                  gconstpointer v2)
{
    const gchar *a, *b;

    g_return_val_if_fail (v1 != NULL, FALSE);
    g_return_val_if_fail (v2 != NULL, FALSE);

    a = gossip_jid_get_full (GOSSIP_JID (v1));
    b = gossip_jid_get_full (GOSSIP_JID (v2));

    /* NOTE: This is not strictly correct, since the node and resource are
     * UTF8, and the domain have other rules. The node is also already
     * casefolded.
     */
    return g_ascii_strcasecmp (a, b) == 0;
}

guint
gossip_jid_hash (gconstpointer key)
{
    GossipJID *jid;
    gchar     *lower;
    guint      ret_val;

    /* NOTE: This is not strictly correct, since the node and resource are
     * UTF8, and the domain have other rules. The node is also already
     * casefolded.
     */
    jid = GOSSIP_JID (key);
    lower = g_ascii_strdown (gossip_jid_get_full (jid), -1);
    ret_val = g_str_hash (lower);
    g_free (lower);

    return ret_val;
}

gboolean
gossip_jid_equal_without_resource (gconstpointer v1,
                                   gconstpointer v2)
{
    const gchar *a, *b;

    g_return_val_if_fail (v1 != NULL, FALSE);
    g_return_val_if_fail (v2 != NULL, FALSE);

    a = gossip_jid_get_without_resource (GOSSIP_JID (v1));
    b = gossip_jid_get_without_resource (GOSSIP_JID (v2));

    /* NOTE: This is not strictly correct, since the node and resource are
     * UTF8, and the domain have other rules. The node is also already
     * casefolded.
     */
    return g_ascii_strcasecmp (a, b) == 0;
}

guint
gossip_jid_hash_without_resource (gconstpointer key)
{
    GossipJID *jid;
    gchar     *lower;
    guint      ret_val;

    /* NOTE: This is not strictly correct, since the node and resource are
     * UTF8, and the domain have other rules. The node is also already
     * casefolded.
     */
    jid = GOSSIP_JID (key);
    lower = g_ascii_strdown (gossip_jid_get_without_resource (jid), -1);
    ret_val = g_str_hash (lower);
    g_free (lower);

    return ret_val;
}

const gchar *
gossip_jid_get_example_string (void)
{
    return "user@jabber.org";
}
