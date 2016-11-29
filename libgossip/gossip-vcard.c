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
 *          Martyn Russell <martyn@imendio.com>
 */

#include <config.h>

#include "gossip-vcard.h"

#define GOSSIP_VCARD_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_VCARD, GossipVCardPrivate))

typedef struct _GossipVCardPrivate GossipVCardPrivate;

struct _GossipVCardPrivate {
    gchar        *name;
    gchar        *nickname;
    gchar        *birthday;
    gchar        *email;
    gchar        *url;
    gchar        *country;
    gchar        *description;
    GossipAvatar *avatar;
};

static void vcard_finalize     (GObject      *object);
static void vcard_get_property (GObject      *object,
                                guint         param_id,
                                GValue       *value,
                                GParamSpec   *pspec);
static void vcard_set_property (GObject      *object,
                                guint         param_id,
                                const GValue *value,
                                GParamSpec   *pspec);

enum {
    PROP_0,
    PROP_NAME,
    PROP_NICKNAME,
    PROP_BIRTHDAY,
    PROP_EMAIL,
    PROP_URL,
    PROP_COUNTRY,
    PROP_DESCRIPTION,
    PROP_AVATAR,
};

G_DEFINE_TYPE (GossipVCard, gossip_vcard, G_TYPE_OBJECT);

static void
gossip_vcard_class_init (GossipVCardClass *class)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (class);

    object_class->finalize              = vcard_finalize;
    object_class->get_property  = vcard_get_property;
    object_class->set_property  = vcard_set_property;

    g_object_class_install_property (object_class,
                                     PROP_NAME,
                                     g_param_spec_string ("name",
                                                          "Name field",
                                                          "The name field",
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_NICKNAME,
                                     g_param_spec_string ("nickname",
                                                          "Nickname field",
                                                          "The nickname field",
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_BIRTHDAY,
                                     g_param_spec_string ("birthday",
                                                          "Birthday field",
                                                          "The birthday field",
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_EMAIL,
                                     g_param_spec_string ("email",
                                                          "Email field",
                                                          "The email field",
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_URL,
                                     g_param_spec_string ("url",
                                                          "URL field",
                                                          "The URL field",
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_COUNTRY,
                                     g_param_spec_string ("country",
                                                          "Country field",
                                                          "The COUNTRY field",
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_DESCRIPTION,
                                     g_param_spec_string ("description",
                                                          "Description field",
                                                          "The description field",
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_AVATAR,
                                     g_param_spec_pointer ("avatar",
                                                           "Avatar image",
                                                           "The avatar image",
                                                           G_PARAM_READWRITE));

    g_type_class_add_private (object_class, sizeof (GossipVCardPrivate));
}

static void
gossip_vcard_init (GossipVCard *vcard)
{
    GossipVCardPrivate *priv;

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    priv->name        = NULL;
    priv->nickname    = NULL;
    priv->birthday    = NULL;
    priv->email       = NULL;
    priv->url         = NULL;
    priv->country     = NULL;
    priv->description = NULL;
    priv->avatar      = NULL;
}

static void
vcard_finalize (GObject *object)
{
    GossipVCardPrivate *priv;

    priv = GOSSIP_VCARD_GET_PRIVATE (object);

    g_free (priv->name);
    g_free (priv->nickname);
    g_free (priv->birthday);
    g_free (priv->email);
    g_free (priv->url);
    g_free (priv->country);
    g_free (priv->description);

    if (priv->avatar) {
        gossip_avatar_unref (priv->avatar);
    }

    (G_OBJECT_CLASS (gossip_vcard_parent_class)->finalize) (object);
}

static void
vcard_get_property (GObject    *object,
                    guint       param_id,
                    GValue     *value,
                    GParamSpec *pspec)
{
    GossipVCardPrivate *priv;

    priv = GOSSIP_VCARD_GET_PRIVATE (object);

    switch (param_id) {
    case PROP_NAME:
        g_value_set_string (value, priv->name);
        break;
    case PROP_NICKNAME:
        g_value_set_string (value, priv->nickname);
        break;
    case PROP_BIRTHDAY:
        g_value_set_string (value, priv->birthday);
        break;
    case PROP_EMAIL:
        g_value_set_string (value, priv->email);
        break;
    case PROP_URL:
        g_value_set_string (value, priv->url);
        break;
    case PROP_COUNTRY:
        g_value_set_string (value, priv->country);
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value, priv->description);
        break;
    case PROP_AVATAR:
        g_value_set_pointer (value, priv->avatar);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}

static void
vcard_set_property (GObject      *object,
                    guint         param_id,
                    const GValue *value,
                    GParamSpec   *pspec)
{
    GossipVCardPrivate *priv;

    priv = GOSSIP_VCARD_GET_PRIVATE (object);

    switch (param_id) {
    case PROP_NAME:
        gossip_vcard_set_name (GOSSIP_VCARD (object),
                               g_value_get_string (value));
        break;
    case PROP_NICKNAME:
        gossip_vcard_set_nickname (GOSSIP_VCARD (object),
                                   g_value_get_string (value));
        break;
    case PROP_BIRTHDAY:
        gossip_vcard_set_birthday (GOSSIP_VCARD (object),
                                   g_value_get_string (value));
        break;
    case PROP_EMAIL:
        gossip_vcard_set_email (GOSSIP_VCARD (object),
                                g_value_get_string (value));
        break;
    case PROP_URL:
        gossip_vcard_set_url (GOSSIP_VCARD (object),
                              g_value_get_string (value));
        break;
    case PROP_COUNTRY:
        gossip_vcard_set_country (GOSSIP_VCARD (object),
                                  g_value_get_string (value));
        break;
    case PROP_DESCRIPTION:
        gossip_vcard_set_description (GOSSIP_VCARD (object),
                                      g_value_get_string (value));
        break;
    case PROP_AVATAR:
        gossip_vcard_set_avatar (GOSSIP_VCARD (object),
                                 g_value_get_pointer (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}

GossipVCard *
gossip_vcard_new (void)
{
    return g_object_new (GOSSIP_TYPE_VCARD, NULL);
}

const gchar *
gossip_vcard_get_name (GossipVCard *vcard)
{
    GossipVCardPrivate *priv;

    g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    return priv->name;
}

const gchar *
gossip_vcard_get_nickname (GossipVCard *vcard)
{
    GossipVCardPrivate *priv;

    g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    return priv->nickname;
}

const gchar *
gossip_vcard_get_birthday (GossipVCard *vcard)
{
    GossipVCardPrivate *priv;

    g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    return priv->birthday;
}

const gchar *
gossip_vcard_get_email (GossipVCard *vcard)
{
    GossipVCardPrivate *priv;

    g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    return priv->email;
}

const gchar *
gossip_vcard_get_url (GossipVCard *vcard)
{
    GossipVCardPrivate *priv;

    g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    return priv->url;
}

const gchar *
gossip_vcard_get_country (GossipVCard *vcard)
{
    GossipVCardPrivate *priv;

    g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    return priv->country;
}

const gchar *
gossip_vcard_get_description (GossipVCard *vcard)
{
    GossipVCardPrivate *priv;

    g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    return priv->description;
}

GossipAvatar *
gossip_vcard_get_avatar (GossipVCard *vcard)
{
    GossipVCardPrivate *priv;

    g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    return priv->avatar;
}

GdkPixbuf *
gossip_vcard_create_avatar_pixbuf (GossipVCard *vcard)
{
    GossipVCardPrivate *priv;
    GdkPixbuf       *pixbuf;
    GdkPixbufLoader *loader;
    GossipAvatar    *avatar;
    GError          *error = NULL;

    g_return_val_if_fail (GOSSIP_IS_VCARD (vcard), NULL);

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    avatar = priv->avatar;
    if (!avatar) {
        return NULL;
    }

    if (avatar->format) {
        loader = gdk_pixbuf_loader_new_with_mime_type (avatar->format, &error);

        if (error) {
            g_warning ("Couldn't create GdkPixbuf loader for image format:'%s', %s",
                       avatar->format, 
                       error->message);
            g_error_free (error);
            return NULL;
        }
    } else {
        loader = gdk_pixbuf_loader_new ();
    }

    if (!gdk_pixbuf_loader_write (loader, avatar->data, avatar->len,
                                  &error)) {
        g_warning ("Couldn't write avatar image:%p with "
                   "length:%" G_GSIZE_FORMAT " to pixbuf loader, %s",
                   avatar->data, 
                   avatar->len, 
                   error->message);
        g_error_free (error);
        return NULL;
    }

    gdk_pixbuf_loader_close (loader, NULL);

    pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

    g_object_ref (pixbuf);
    g_object_unref (loader);

    return pixbuf;
}

void
gossip_vcard_set_name (GossipVCard *vcard,
                       const gchar *name)
{
    GossipVCardPrivate *priv;

    g_return_if_fail (GOSSIP_IS_VCARD (vcard));

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    g_free (priv->name);
    priv->name = g_strdup (name);

    g_object_notify (G_OBJECT (vcard), "name");
}

void
gossip_vcard_set_nickname (GossipVCard *vcard,
                           const gchar *nickname)
{
    GossipVCardPrivate *priv;

    g_return_if_fail (GOSSIP_IS_VCARD (vcard));

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    g_free (priv->nickname);
    priv->nickname = g_strdup (nickname);

    g_object_notify (G_OBJECT (vcard), "nickname");
}

void
gossip_vcard_set_birthday (GossipVCard *vcard,
                           const gchar *birthday)
{
    GossipVCardPrivate *priv;

    g_return_if_fail (GOSSIP_IS_VCARD (vcard));

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    g_free (priv->birthday);
    priv->birthday = g_strdup (birthday);

    g_object_notify (G_OBJECT (vcard), "birthday");
}

void
gossip_vcard_set_email (GossipVCard *vcard,
                        const gchar *email)
{
    GossipVCardPrivate *priv;

    g_return_if_fail (GOSSIP_IS_VCARD (vcard));

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    g_free (priv->email);
    priv->email = g_strdup (email);

    g_object_notify (G_OBJECT (vcard), "email");
}

void
gossip_vcard_set_url (GossipVCard *vcard,
                      const gchar *url)
{
    GossipVCardPrivate *priv;

    g_return_if_fail (GOSSIP_IS_VCARD (vcard));

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    g_free (priv->url);
    priv->url = g_strdup (url);

    g_object_notify (G_OBJECT (vcard), "url");
}

void
gossip_vcard_set_country (GossipVCard *vcard,
                          const gchar *country)
{
    GossipVCardPrivate *priv;

    g_return_if_fail (GOSSIP_IS_VCARD (vcard));

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    g_free (priv->country);
    priv->country = g_strdup (country);

    g_object_notify (G_OBJECT (vcard), "country");
}

void
gossip_vcard_set_description (GossipVCard *vcard,
                              const gchar *description)
{
    GossipVCardPrivate *priv;

    g_return_if_fail (GOSSIP_IS_VCARD (vcard));

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    g_free (priv->description);
    priv->description = g_strdup (description);

    g_object_notify (G_OBJECT (vcard), "description");
}

void
gossip_vcard_set_avatar (GossipVCard  *vcard,
                         GossipAvatar *avatar)
{
    GossipVCardPrivate *priv;

    g_return_if_fail (GOSSIP_IS_VCARD (vcard));

    priv = GOSSIP_VCARD_GET_PRIVATE (vcard);

    if (priv->avatar) {
        gossip_avatar_unref (priv->avatar);
        priv->avatar = NULL;
    }

    if (avatar) {
        priv->avatar = gossip_avatar_ref (avatar);
    }

    g_object_notify (G_OBJECT (vcard), "avatar");
}

