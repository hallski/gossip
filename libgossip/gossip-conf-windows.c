/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2006 Imendio AB
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
 * Authors: Richard Hult <richard@imendio.com>
 */

#include <config.h>

#include <string.h>

#include <windows.h>
#include <winreg.h>

#include "gossip-conf.h"
#include "gossip-debug.h"
#include "gossip-conf.h"

#define DEBUG_DOMAIN "Config"

#define GOSSIP_CONF_ROOT       "/apps/gossip"
#define HTTP_PROXY_ROOT        "/system/http_proxy"
#define DESKTOP_INTERFACE_ROOT "/desktop/gnome/interface"

#define GOSSIP_CONF_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CONF, GossipConfPriv))

typedef struct {
    void                 *fixme;
} GossipConfPrivate;

typedef struct {
    GossipConf           *conf;
    GossipConfNotifyFunc  func;
    gpointer               user_data;
} GossipConfNotifyData;

static void conf_finalize (GObject *object);

G_DEFINE_TYPE (GossipConf, gossip_conf, G_TYPE_OBJECT);

static GossipConf *global_conf = NULL;

static void
gossip_conf_class_init (GossipConfClass *class)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (class);

    object_class->finalize = conf_finalize;

    g_type_class_add_private (object_class, sizeof (GossipConfPrivate));
}

static void
gossip_conf_init (GossipConf *conf)
{
    GossipConfPrivate *priv;

    priv = GOSSIP_CONF_GET_PRIVATE (conf);
}

static void
conf_finalize (GObject *object)
{
    GossipConfPrivate *priv;

    priv = GOSSIP_CONF_GET_PRIVATE (object);

    G_OBJECT_CLASS (gossip_conf_parent_class)->finalize (object);
}

GossipConf *
gossip_conf_get (void)
{
    if (!global_conf) {
        global_conf = g_object_new (GOSSIP_TYPE_CONF, NULL);
    }

    return global_conf;
}

void
gossip_conf_shutdown (void)
{
    if (global_conf) {
        g_object_unref (global_conf);
        global_conf = NULL;
    }
}

static gchar *
conf_get_reg_path_from_key (const gchar *key)
{
    gchar       **paths;
    gchar        *path = NULL;
    const gchar  *key_to_convert;

    /* What we do here is convert the GConf path to a Windows
     * registry path.
     */
    if (g_str_has_prefix (key, "/apps/")) {
        key_to_convert = key + 6;
        path = g_strdup ("Software"); 
    } else {
        key_to_convert = key;
    }

    paths = g_strsplit (key_to_convert, "/", -1);
    if (paths) {
        gint i, segments;

        segments = g_strv_length (paths);
                
        for (i = 0; i < (segments - 1) && paths[i]; i++) {
            if (!path) {
                path = g_strdup (paths[i]);
                continue;
            }

            path = g_strconcat (path, "\\", paths[i], NULL);
        }
    }
    g_strfreev (paths);

    gossip_debug (DEBUG_DOMAIN, 
                  "Using registry path:'%s' from string:'%s'", 
                  path,
                  key);
        
    return path;
}

static gchar *
conf_get_reg_key_from_key (const gchar *key)
{
    gchar *str;

    str = g_strrstr (key, "/");
    if (!str) {
        return NULL;
    }

    gossip_debug (DEBUG_DOMAIN, 
                  "Using registry key:'%s' from string:'%s'", 
                  str + 1,
                  key);

    return str + 1;
}

static HKEY
conf_create_hkey (HKEY         parent, 
                  const gchar *subkey)
{
    HKEY hk;
    LONG success;

    success = RegCreateKeyEx (parent ? parent : HKEY_CURRENT_USER, 
                              (LPCTSTR) subkey,
                              0,
                              NULL, 
                              REG_OPTION_NON_VOLATILE,
                              KEY_WRITE, 
                              NULL, 
                              &hk, 
                              NULL); 

    if (success != ERROR_SUCCESS) {
        g_warning ("Couldn't create the registry subkey:'%s', "
                   "RegCreateKeyEx returned %ld",
                   subkey,
                   success); 
        hk = NULL;
    }

    return hk;
}

static HKEY
conf_create_hkey_with_parents (const gchar *key)
{
    gchar **paths;
    gchar  *path = NULL;
    gint    i, segments;
    HKEY    hk = NULL;

    paths = g_strsplit (key, "\\", -1);
    if (!paths) {
        g_warning ("Couldn't split key, no '\\' characters found");
        return NULL;
    }

    segments = g_strv_length (paths);
        
    for (i = 0; i < segments && paths[i]; i++) {
        HKEY new_hk;

        if (strlen (paths[i]) < 1) {
            continue;
        }

        gossip_debug (DEBUG_DOMAIN, "Creating registry key for path:'%s'", paths[i]);

        new_hk = conf_create_hkey (hk, paths[i]);
        RegFlushKey (hk);
        RegCloseKey (hk);
                
        if (!new_hk) {
            hk = NULL;
            break;
        }

        hk = new_hk;
    }
        
    g_free (path);
    g_strfreev (paths);

    return hk;
}

static HKEY 
conf_get_hkey (const gchar *key, 
               GType        key_type,
               gboolean     create_if_not_exists)
{
    gchar    *reg_path;
    gchar    *reg_key;
    gboolean  correct_type;
    wchar_t  *wc_path, *wc_key;
    HKEY      hk = NULL;
    LONG      success;
    DWORD     type;
    DWORD     bytes;

    reg_path = conf_get_reg_path_from_key (key);
    wc_path = g_utf8_to_utf16 (reg_path, -1, NULL, NULL, NULL);
    gossip_debug (DEBUG_DOMAIN, "Getting registry key:'%s'", reg_path);

    success = RegOpenKeyExW (HKEY_CURRENT_USER, wc_path, 0, 
                             KEY_QUERY_VALUE | KEY_SET_VALUE,
                             &hk);
    g_free (wc_path);

    if (success == ERROR_FILE_NOT_FOUND) {
        if (create_if_not_exists) {
            /* Try to reopen it */ 
            gossip_debug (DEBUG_DOMAIN, "Registry key not found, attempting to create it...");
            hk = conf_create_hkey_with_parents (reg_path);
            g_free (reg_path);
            return hk;
        } else {
            gossip_debug (DEBUG_DOMAIN, "Registry key not found:'%s'", key);
            g_free (reg_path);
            return NULL;
        }
    }
    else if (success != ERROR_SUCCESS) {
        g_warning ("Couldn't open registry key, RegOpenKeyExW returned %ld", 
                   success);
        g_free (reg_path);
        return NULL;
    }

    g_free (reg_path);

    gossip_debug (DEBUG_DOMAIN, "Registry key opened");

    reg_key = conf_get_reg_key_from_key (key);
    wc_key = g_utf8_to_utf16 (reg_key, -1, NULL, NULL, NULL);
    g_free (reg_key);

    success = RegQueryValueExW (hk, wc_key, 0, &type, NULL, &bytes);
    g_free (wc_key);

    if (success == ERROR_FILE_NOT_FOUND) {
        gossip_debug (DEBUG_DOMAIN, "Registry key found, value isn't set");
        return hk;
    } else if (success != ERROR_SUCCESS) {
        g_warning ("Couldn't query registry value, RegQueryValueExW returned %ld", 
                   success);
        RegCloseKey (hk);
        return NULL;
    }

    switch (key_type) {
    case G_TYPE_STRING:
        correct_type = type == REG_SZ;
        break;
    case G_TYPE_INT:
    case G_TYPE_UINT:
    case G_TYPE_BOOLEAN:
        correct_type = type == REG_DWORD;
        break;
    default:
        g_warning ("Couldn't get registry key, %s not supported",
                   g_type_name (key_type));
        correct_type = FALSE;
        break;
    }
        
    if (!correct_type) {
        g_warning ("Couldn't get registry key, expected different type");
        RegCloseKey (hk);
        return NULL;
    }

    return hk;
}

gboolean
gossip_conf_set_int (GossipConf  *conf,
                     const gchar *key,
                     gint         value)
{
    GossipConfPrivate *priv;
    gchar          *reg_key;
    HKEY            hk;
    LONG            success;
    DWORD           reg_value;

    g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

    gossip_debug (DEBUG_DOMAIN, "Setting int:'%s' to %d", key, value);

    priv = GOSSIP_CONF_GET_PRIVATE (conf);

    hk = conf_get_hkey (key, G_TYPE_INT, TRUE);
    if (!hk) {
        return FALSE;
    }

    reg_key = conf_get_reg_key_from_key (key);
    reg_value = (DWORD) value;

    success = RegSetValueEx (hk,
                             reg_key, 
                             0,
                             REG_DWORD,
                             (LPBYTE) &value,
                             sizeof (DWORD));
    g_free (reg_key);

    if (success != ERROR_SUCCESS) {
        g_warning ("Couldn't set registry value, RegSetValueEx returned %ld",
                   success);
    }

    RegFlushKey (hk);
    RegCloseKey (hk);

    return success == ERROR_SUCCESS;
}

gboolean
gossip_conf_get_int (GossipConf  *conf,
                     const gchar *key,
                     gint        *value)
{
    GossipConfPrivate *priv;
    GError         *error = NULL;
    gchar          *reg_key;
    wchar_t        *wc_key;
    HKEY            hk;
    DWORD           type;
    DWORD           bytes;

    *value = 0;

    g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);
    g_return_val_if_fail (value != NULL, FALSE);

    priv = GOSSIP_CONF_GET_PRIVATE (conf);

    hk = conf_get_hkey (key, G_TYPE_INT, TRUE);
    if (!hk) {
        return FALSE;
    }

    reg_key = conf_get_reg_key_from_key (key);
    wc_key = g_utf8_to_utf16 (reg_key, -1, NULL, NULL, NULL);
    g_free (reg_key);

    RegQueryValueExW (hk, wc_key, 0, &type, (LPBYTE) value, &bytes);
    g_free (wc_key);

    RegCloseKey (hk);

    gossip_debug (DEBUG_DOMAIN, "Getting int:'%s' (=%d), error:'%s'",
                  key, *value, error ? error->message : "None");

    return TRUE;
}

gboolean
gossip_conf_set_bool (GossipConf  *conf,
                      const gchar *key,
                      gboolean     value)
{
    GossipConfPrivate *priv;
    gchar          *reg_key;
    HKEY            hk;
    LONG            success;
    DWORD           reg_value;

    g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

    gossip_debug (DEBUG_DOMAIN, "Setting bool:'%s' to %d ---> %s",
                  key, value, value ? "true" : "false");

    priv = GOSSIP_CONF_GET_PRIVATE (conf);

    hk = conf_get_hkey (key, G_TYPE_BOOLEAN, TRUE);
    if (!hk) {
        return FALSE;
    }

    reg_key = conf_get_reg_key_from_key (key);
    reg_value = value ? 1 : 0;

    success = RegSetValueEx (hk,
                             (LPCTSTR) reg_key, 
                             0,
                             REG_DWORD,
                             (LPBYTE) &reg_value,
                             sizeof (DWORD));
    g_free (reg_key);

    if (success != ERROR_SUCCESS) {
        gossip_debug (DEBUG_DOMAIN, 
                      "Couldn't set registry value, RegSetValueEx returned %ld",
                      success);
    }

    RegFlushKey (hk);
    RegCloseKey (hk);
        
    return TRUE;
}

gboolean
gossip_conf_get_bool (GossipConf  *conf,
                      const gchar *key,
                      gboolean    *value)
{
    GossipConfPrivate *priv;
    GError         *error = NULL;
    gchar          *reg_key;
    wchar_t        *wc_key;
    HKEY            hk;
    DWORD           type;
    DWORD           bytes;

    *value = FALSE;

    g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);
    g_return_val_if_fail (value != NULL, FALSE);

    priv = GOSSIP_CONF_GET_PRIVATE (conf);

    hk = conf_get_hkey (key, G_TYPE_BOOLEAN, TRUE);
    if (!hk) {
        return FALSE;
    }

    reg_key = conf_get_reg_key_from_key (key);
    wc_key = g_utf8_to_utf16 (reg_key, -1, NULL, NULL, NULL);
    g_free (reg_key);

    RegQueryValueExW (hk, wc_key, 0, &type, (LPBYTE) value, &bytes);
    g_free (wc_key);

    RegCloseKey (hk);

    gossip_debug (DEBUG_DOMAIN, "Getting bool:'%s' (=%d ---> %s), error:'%s'",
                  key, *value, *value ? "true" : "false",
                  error ? error->message : "None");

    return TRUE;
}

gboolean
gossip_conf_set_string (GossipConf  *conf,
                        const gchar *key,
                        const gchar *value)
{
    GossipConfPrivate *priv;
    gchar          *reg_key;
    HKEY            hk;
    LONG            success;

    g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

    gossip_debug (DEBUG_DOMAIN, "Setting string:'%s' to '%s'",
                  key, value);

    priv = GOSSIP_CONF_GET_PRIVATE (conf);

    hk = conf_get_hkey (key, G_TYPE_STRING, TRUE);
    if (!hk) {
        return FALSE;
    }

    reg_key = conf_get_reg_key_from_key (key);

    success = RegSetValueEx (hk,
                             (LPCTSTR) reg_key, 
                             0,
                             REG_SZ,
                             (LPBYTE) value,
                             (DWORD) strlen (value));
    g_free (reg_key);

    if (success != ERROR_SUCCESS) {
        gossip_debug (DEBUG_DOMAIN, 
                      "Couldn't set registry value, RegSetValueEx returned %ld",
                      success);
    }

    RegFlushKey (hk);
    RegCloseKey (hk);

    return TRUE;
}

gboolean
gossip_conf_get_string (GossipConf   *conf,
                        const gchar  *key,
                        gchar       **value)
{
    GossipConfPrivate *priv;
    GError         *error = NULL;
    gchar          *reg_key;
    wchar_t        *wc_key;
    wchar_t        *wc_tmp;
    HKEY            hk;
    DWORD           type;
    DWORD           bytes;
        
    *value = NULL;

    g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

    priv = GOSSIP_CONF_GET_PRIVATE (conf);

    hk = conf_get_hkey (key, G_TYPE_STRING, TRUE);
    if (!hk) {
        return FALSE;
    }

    reg_key = conf_get_reg_key_from_key (key);
    wc_key = g_utf8_to_utf16 (reg_key, -1, NULL, NULL, NULL);
    g_free (reg_key);

    /* Get size */
    RegQueryValueExW (hk, wc_key, 0, &type, NULL, &bytes);
    wc_tmp = g_new0 (wchar_t, (bytes + 1) / 2 + 1);
    RegQueryValueExW (hk, wc_key, 0, &type, (LPBYTE) wc_tmp, &bytes);
    g_free (wc_key);

    wc_tmp[bytes/2] = '\0';
    *value = g_utf16_to_utf8 (wc_tmp, -1, NULL, NULL, NULL);

    RegCloseKey (hk);

    gossip_debug (DEBUG_DOMAIN, "Getting string:'%s' (='%s'), error:'%s'",
                  key, *value ? *value : "", error ? error->message : "None");

    return TRUE;
}

gboolean
gossip_conf_set_string_list (GossipConf  *conf,
                             const gchar *key,
                             GSList      *value)
{
    GossipConfPrivate *priv;

    g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

    priv = GOSSIP_CONF_GET_PRIVATE (conf);

    /* FIXME: Set value */
    return TRUE;
}

gboolean
gossip_conf_get_string_list (GossipConf   *conf,
                             const gchar  *key,
                             GSList      **value)
{
    GossipConfPrivate *priv;
    GError          *error = NULL;

    *value = NULL;

    g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

    priv = GOSSIP_CONF_GET_PRIVATE (conf);

    /* FIXME: Get value */

    if (error) {
        g_error_free (error);
        return FALSE;
    }

    return TRUE;
}

/* static void */
/* conf_notify_data_free (GossipConfNotifyData *data) */
/* { */
/*      g_object_unref (data->conf); */
/*      g_slice_free (GossipConfNotifyData, data); */
/* } */

/* static void */
/* conf_notify_func (GConfClient *client, */
/*                guint        id, */
/*                GConfEntry  *entry, */
/*                gpointer     user_data) */
/* { */
/*      GossipConfNotifyData *data; */

/*      data = user_data; */

/*      data->func (data->conf, */
/*                  gconf_entry_get_key (entry), */
/*                  data->user_data); */
/* } */

guint
gossip_conf_notify_add (GossipConf           *conf,
                        const gchar          *key,
                        GossipConfNotifyFunc func,
                        gpointer              user_data)
{
    GossipConfPrivate    *priv;
    guint                  id;
    GossipConfNotifyData *data;

    g_return_val_if_fail (GOSSIP_IS_CONF (conf), 0);

    priv = GOSSIP_CONF_GET_PRIVATE (conf);

    data = g_slice_new (GossipConfNotifyData);
    data->func = func;
    data->user_data = user_data;
    data->conf = g_object_ref (conf);

    /* FIXME: Implement */
/*      id = gconf_client_notify_add (priv->gconf_client, */
/*                                    key, */
/*                                    conf_notify_func, */
/*                                    data, */
/*                                    (GFreeFunc) conf_notify_data_free, */
/*                                    NULL); */

    id = 1;

    return id;
}

gboolean
gossip_conf_notify_remove (GossipConf *conf,
                           guint       id)
{
    GossipConfPrivate *priv;

    g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

    priv = GOSSIP_CONF_GET_PRIVATE (conf);

    /* FIXME: Implement */
/*      gconf_client_notify_remove (priv->gconf_client, id); */

    return TRUE;
}

/* Special cased settings that also need to be in the backends. */

gboolean
gossip_conf_get_http_proxy (GossipConf  *conf,
                            gboolean    *use_http_proxy,
                            gchar      **host,
                            gint        *port,
                            gboolean    *use_auth,
                            gchar      **username,
                            gchar      **password)
{
    GossipConfPrivate *priv;

    g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

    priv = GOSSIP_CONF_GET_PRIVATE (conf);

    *use_http_proxy = FALSE;
    *host = NULL;
    *port = 0;
    *use_auth = FALSE;
    *username = NULL;
    *password = NULL;

    return TRUE;
}
