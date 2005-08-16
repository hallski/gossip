/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2003 Imendio AB
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

#ifndef __GOSSIP_CHAT_VIEW_H__
#define __GOSSIP_CHAT_VIEW_H__

#include <gtk/gtktextview.h>

#include <libgossip/gossip-message.h>

#define GOSSIP_TYPE_CHAT_VIEW         (gossip_chat_view_get_type ())
#define GOSSIP_CHAT_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CHAT_VIEW, GossipChatView))
#define GOSSIP_CHAT_VIEW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_CHAT_VIEW, GossipChatViewClass))
#define GOSSIP_IS_CHAT_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CHAT_VIEW))
#define GOSSIP_IS_CHAT_VIEW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CHAT_VIEW))
#define GOSSIP_CHAT_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CHAT_VIEW, GossipChatViewClass))


typedef struct _GossipChatView      GossipChatView;
typedef struct _GossipChatViewClass GossipChatViewClass;
typedef struct _GossipChatViewPriv  GossipChatViewPriv;


struct _GossipChatView {
	GtkTextView         parent;

	GossipChatViewPriv *priv;
};


struct _GossipChatViewClass {
	GtkTextViewClass    parent_class;
};


GType            gossip_chat_view_get_type             (void) G_GNUC_CONST;
GossipChatView * gossip_chat_view_new                  (void);
void             gossip_chat_view_append_chat_message  (GossipChatView *view,
							gossip_time_t   timestamp,
							const gchar    *to,
							const gchar    *from,
							const gchar    *msg);
void             gossip_chat_view_append_invite_message (GossipChatView *view,
							 GossipContact  *contact,
							 gossip_time_t   timestamp,
							 const gchar    *invite,
							 const gchar    *msg);
void             gossip_chat_view_append_event_message (GossipChatView *view,
							const gchar    *str,
							gboolean        timestamp);
void             gossip_chat_view_set_margin           (GossipChatView *view,
							gint            margin);
void             gossip_chat_view_clear                (GossipChatView *view);
void             gossip_chat_view_scroll_down          (GossipChatView *view);
gboolean         gossip_chat_view_get_selection_bounds (GossipChatView *view,
							GtkTextIter    *start,
							GtkTextIter    *end);
void             gossip_chat_view_copy_clipboard       (GossipChatView *view);

#endif /* __GOSSIP_CHAT_VIEW_H__ */
