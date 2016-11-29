/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2007 Imendio AB
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

#ifndef __GOSSIP_THEME_H__
#define __GOSSIP_THEME_H__

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_THEME            (gossip_theme_get_type ())
#define GOSSIP_THEME(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_THEME, GossipTheme))
#define GOSSIP_THEME_CLASS(k)        (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_THEME, GossipThemeClass))
#define GOSSIP_IS_THEME(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_THEME))
#define GOSSIP_IS_THEME_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_THEME))
#define GOSSIP_THEME_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_THEME, GossipThemeClass))

typedef struct _GossipTheme      GossipTheme;
typedef struct _GossipThemeClass GossipThemeClass;

#include "gossip-chat-view.h"

struct _GossipTheme {
    GObject parent;
};

typedef void GossipThemeContext;

struct _GossipThemeClass {
    GObjectClass parent_class;

    /* <vtable> */
    GossipThemeContext * (*setup_with_view)  (GossipTheme        *theme,
                                              GossipChatView     *view);
    void                 (*detach_from_view) (GossipTheme        *theme,
                                              GossipThemeContext *context,
                                              GossipChatView     *view);
    void                 (*view_cleared)     (GossipTheme        *theme,
                                              GossipThemeContext *context,
                                              GossipChatView     *view);
    void                 (*append_message)   (GossipTheme        *theme,
                                              GossipThemeContext *context,
                                              GossipChatView     *view,
                                              GossipMessage      *message,
                                              gboolean            from_self);
    void                 (*append_action)    (GossipTheme        *theme,
                                              GossipThemeContext *context,
                                              GossipChatView     *view,
                                              GossipMessage      *message,
                                              gboolean            from_self);
    void                 (*append_event)     (GossipTheme        *theme,
                                              GossipThemeContext *context,
                                              GossipChatView     *view,
                                              const gchar        *str);
    void                 (*append_timestamp) (GossipTheme        *theme,
                                              GossipThemeContext *context,
                                              GossipChatView     *view,
                                              GossipMessage      *message,
                                              gboolean            show_date,
                                              gboolean            show_time);
    void                 (*append_spacing)   (GossipTheme        *theme,
                                              GossipThemeContext *context,
                                              GossipChatView     *view);
    void                 (*update_show_avatars) (GossipTheme     *theme,
                                                 GossipThemeContext *context,
                                                 GossipChatView  *view,
                                                 gboolean         show);
};

GType        gossip_theme_get_type              (void) G_GNUC_CONST;

GossipTheme *gossip_theme_new                   (void);

GossipThemeContext *
gossip_theme_setup_with_view       (GossipTheme        *theme,
                                    GossipChatView     *view);
void         gossip_theme_detach_from_view      (GossipTheme        *theme,
                                                 GossipThemeContext *context,
                                                 GossipChatView     *view);
void         gossip_theme_view_cleared          (GossipTheme        *theme,
                                                 GossipThemeContext *context,
                                                 GossipChatView     *view);
void         gossip_theme_append_message        (GossipTheme        *theme,
                                                 GossipThemeContext *context,
                                                 GossipChatView     *view,
                                                 GossipMessage      *msg,
                                                 gboolean            from_self);
void         gossip_theme_append_action         (GossipTheme        *theme,
                                                 GossipThemeContext *context,
                                                 GossipChatView     *view,
                                                 GossipMessage      *msg,
                                                 gboolean            from_self);
void         gossip_theme_append_text           (GossipTheme        *theme,
                                                 GossipThemeContext *context,
                                                 GossipChatView     *view,
                                                 const gchar        *body,
                                                 const gchar        *tag, 
                                                 const gchar        *link_tag);
void         gossip_theme_append_spacing        (GossipTheme        *theme,
                                                 GossipThemeContext *context,
                                                 GossipChatView     *view);
void         gossip_theme_append_event          (GossipTheme        *theme,
                                                 GossipThemeContext *context,
                                                 GossipChatView     *view,
                                                 const gchar        *str);
void         gossip_theme_append_timestamp      (GossipTheme        *theme,
                                                 GossipThemeContext *context,
                                                 GossipChatView     *view,
                                                 GossipMessage      *message,
                                                 gboolean            show_date,
                                                 gboolean            show_time);
void         gossip_theme_append_time_maybe     (GossipTheme        *theme,
                                                 GossipThemeContext *context,
                                                 GossipChatView     *view,
                                                 GossipMessage      *message);
gboolean     gossip_theme_get_show_avatars      (GossipTheme        *theme);
void         gossip_theme_set_show_avatars      (GossipTheme        *theme,
                                                 gboolean            show);

G_END_DECLS

#endif /* __GOSSIP_THEME_H__ */

