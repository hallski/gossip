/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2003 Imendio AB
 * Copyright (C) 2003-2004 Geert-Jan Van den Bogaerde <geertjan@gnome.org>
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

#ifndef __GOSSIP_CHAT_H__
#define __GOSSIP_CHAT_H__

#include <glib-object.h>

#include <libgossip/gossip-contact.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_CHAT         (gossip_chat_get_type ())
#define GOSSIP_CHAT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CHAT, GossipChat))
#define GOSSIP_CHAT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_CHAT, GossipChatClass))
#define GOSSIP_IS_CHAT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CHAT))
#define GOSSIP_IS_CHAT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CHAT))
#define GOSSIP_CHAT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CHAT, GossipChatClass))


typedef struct _GossipChat GossipChat;
typedef struct _GossipChatClass GossipChatClass;
typedef struct _GossipChatPriv GossipChatPriv;

#include "gossip-chat-view.h"
#include "gossip-chat-window.h"
#include "gossip-spell.h"


struct _GossipChat {
        GObject         parent;

	/* protected */
	GossipChatView *view;
	GtkWidget      *input_text_view;
	gboolean        is_first_char;

	/* private */
        GossipChatPriv *priv;
};


struct _GossipChatClass {
        GObjectClass parent;

        /* Signals */

	void             (*new_message)      (GossipChat  *chat);
        void             (*composing)        (GossipChat  *chat, 
					      gboolean     composing);
	void             (*name_changed)     (GossipChat  *chat,
				              const gchar *name);
	void             (*status_changed)   (GossipChat  *chat);

	/* vtable */

	const gchar *    (*get_name)         (GossipChat  *chat);
	gchar *          (*get_tooltip)      (GossipChat  *chat);
	GdkPixbuf *      (*get_status_pixbuf)(GossipChat  *chat);
	GossipContact *  (*get_contact)      (GossipChat  *chat);
	GossipContact *  (*get_own_contact)  (GossipChat  *chat);
	void             (*get_geometry)     (GossipChat  *chat,
					      gint        *width,
					      gint        *height);
	GtkWidget *      (*get_widget)       (GossipChat  *chat);
	gboolean         (*get_group_chat)   (GossipChat  *chat); 

	gboolean         (*get_show_contacts)(GossipChat  *chat);
	void             (*set_show_contacts)(GossipChat  *chat,
					      gboolean     show);
};


GType             gossip_chat_get_type           (void);

GossipChatView *  gossip_chat_get_view           (GossipChat       *chat);
GossipChatWindow *gossip_chat_get_window         (GossipChat       *chat);
void              gossip_chat_set_window         (GossipChat       *chat,
						  GossipChatWindow *window);
void              gossip_chat_present            (GossipChat       *chat);
void              gossip_chat_clear              (GossipChat       *chat);
void              gossip_chat_scroll_down        (GossipChat       *chat);
void              gossip_chat_cut                (GossipChat       *chat);
void              gossip_chat_copy               (GossipChat       *chat);
void              gossip_chat_paste              (GossipChat       *chat);

const gchar *     gossip_chat_get_name		 (GossipChat       *chat);
gchar *           gossip_chat_get_tooltip        (GossipChat       *chat);
GdkPixbuf *       gossip_chat_get_status_pixbuf  (GossipChat       *chat);
GossipContact *   gossip_chat_get_contact        (GossipChat       *chat);
GossipContact *   gossip_chat_get_own_contact    (GossipChat       *chat);
void              gossip_chat_get_geometry       (GossipChat       *chat,
		                                  int              *width,
						  int              *height);
GtkWidget *       gossip_chat_get_widget         (GossipChat       *chat);
gboolean          gossip_chat_get_group_chat     (GossipChat       *chat);
gboolean          gossip_chat_get_show_contacts  (GossipChat       *chat);
void              gossip_chat_set_show_contacts  (GossipChat       *chat,
						  gboolean          show);

/* for spell checker dialog to correct the misspelled word */
void              gossip_chat_correct_word       (GossipChat       *chat,
						  GtkTextIter       start,
						  GtkTextIter       end,
						  const gchar      *new_word);

G_END_DECLS

#endif /* __GOSSIP_CHAT_H__ */
