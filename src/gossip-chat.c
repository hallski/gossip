/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2004 Imendio HB
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

#include <config.h>
#include <gtk/gtk.h>
#include "gossip-chat.h"
#include "gossip-marshal.h"
#include "gossip-chat-window.h"

struct _GossipChatPriv {
	GossipChatWindow *window;

	/* Used to automatically shrink a window that has temporarily grown
	 * due to long input */
	gint              padding_height;
	gint              default_window_height;
	gint              last_input_height;
};

static void     chat_class_init                   (GossipChatClass *klass);
static void     chat_init                         (GossipChat      *chat);
static void     chat_finalize                     (GObject         *object);
static void     chat_input_text_buffer_changed_cb (GtkTextBuffer *buffer,
		                                   GossipChat    *chat);
static void     chat_text_view_size_allocate_cb   (GtkWidget       *widget,
		                                   GtkAllocation   *allocation,
					           GossipChat      *chat);

enum {
	COMPOSING,
	NEW_MESSAGE,
	NAME_CHANGED,
	STATUS_CHANGED,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;
static guint         chat_signals[LAST_SIGNAL] = { 0 };

GType
gossip_chat_get_type (void)
{
	static GType type_id = 0;

	if (type_id == 0) {
		const GTypeInfo type_info = {
			sizeof (GossipChatClass),
			NULL,
			NULL,
			(GClassInitFunc) chat_class_init,
			NULL,
			NULL,
			sizeof (GossipChat),
			0,
			(GInstanceInitFunc) chat_init
		};

		type_id = g_type_register_static (G_TYPE_OBJECT,
						  "GossipChat",
						  &type_info,
						  G_TYPE_FLAG_ABSTRACT);
	}

	return type_id;
}

static void
chat_class_init (GossipChatClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = chat_finalize;

        chat_signals[COMPOSING] =
                g_signal_new ("composing",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GossipChatClass, composing),
                              NULL, NULL,
			      gossip_marshal_VOID__BOOLEAN,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_BOOLEAN);

	chat_signals[NEW_MESSAGE] =
		g_signal_new ("new-message",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GossipChatClass, new_message),
			      NULL, NULL,
			      gossip_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	chat_signals[NAME_CHANGED] =
		g_signal_new ("name-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GossipChatClass, name_changed),
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	chat_signals[STATUS_CHANGED] =
		g_signal_new ("status-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GossipChatClass, status_changed),
			      NULL, NULL,
			      gossip_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
}

static void
chat_init (GossipChat *chat)
{
	GossipChatPriv *priv;
	GtkTextBuffer  *buffer;

	chat->view = gossip_chat_view_new ();
	chat->input_text_view = gtk_text_view_new ();
	chat->is_first_char = TRUE;

	g_object_set (G_OBJECT (chat->input_text_view),
		      "pixels-above-lines", 2,
		      "pixels-below-lines", 2,
		      "pixels-inside-wrap", 1,
		      "right-margin", 2,
		      "left-margin", 2,
		      "wrap-mode", GTK_WRAP_WORD,
		      NULL);

	priv = g_new0 (GossipChatPriv, 1);
	priv->default_window_height = -1;

	chat->priv = priv;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	g_signal_connect (buffer,
			  "changed",
			  G_CALLBACK (chat_input_text_buffer_changed_cb),
			  chat);

	g_signal_connect (chat->input_text_view,
			  "size_allocate",
			  G_CALLBACK (chat_text_view_size_allocate_cb),
			  chat);

	gossip_chat_view_set_margin (chat->view, 3);
}

static void
chat_finalize (GObject *object)
{
	GossipChat     *chat;
	GossipChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT (object));

	chat = GOSSIP_CHAT (object);
	priv = chat->priv;

	g_free (priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
chat_input_text_buffer_changed_cb (GtkTextBuffer     *buffer,
		                   GossipChat        *chat)
{
	GossipChatPriv *priv;

	priv = chat->priv;

	if (chat->is_first_char) {
		GtkRequisition  req;
		gint            window_height;
		GtkWidget      *dialog;
		GtkAllocation  *allocation;

		/* Save the window's size */
		dialog = gossip_chat_window_get_dialog (priv->window);
		gtk_window_get_size (GTK_WINDOW (dialog),
				     NULL, &window_height);

		gtk_widget_size_request (chat->input_text_view, &req);

		allocation = &GTK_WIDGET (chat->view)->allocation;

		priv->default_window_height = window_height;
		priv->last_input_height = req.height;
		priv->padding_height = window_height - req.height - allocation->height;

		chat->is_first_char = FALSE;
	}
}

typedef struct {
	GtkWidget *window;
	gint       width;
	gint       height;
} ChangeSizeData;

static gboolean
chat_change_size_in_idle_cb (ChangeSizeData *data)
{
	gtk_window_resize (GTK_WINDOW (data->window), 
			   data->width, data->height);

	return FALSE;
}

static void
chat_text_view_size_allocate_cb (GtkWidget     *widget,
		                 GtkAllocation *allocation,
				 GossipChat    *chat)
{
	GossipChatPriv   *priv;
	gint              width;
	GtkWidget        *dialog;
	ChangeSizeData   *data;
	gint              window_height;
	gint              new_height;
	GtkAllocation    *view_allocation;
	gint              current_height;
	gint              diff;

	priv = chat->priv;

	if (priv->default_window_height <= 0) {
		return;
	}

	if (priv->last_input_height <= allocation->height) {
		priv->last_input_height = allocation->height;
		return;
	}

	diff = priv->last_input_height - allocation->height;
	priv->last_input_height = allocation->height;
	
	view_allocation = &GTK_WIDGET (chat->view)->allocation;

	dialog = gossip_chat_window_get_dialog (priv->window);
	gtk_window_get_size (GTK_WINDOW (dialog), NULL, &current_height);

	new_height = view_allocation->height + priv->padding_height + allocation->height - diff;

	if (new_height <= priv->default_window_height) {
		window_height = priv->default_window_height;
	} else {
		window_height = new_height;
	}

	if (current_height <= window_height) {
		return;
	}

	/* Restore the window's size */
	gtk_window_get_size (GTK_WINDOW (dialog), &width, NULL);

	data = g_new0 (ChangeSizeData, 1);
	data->window = dialog;
	data->width  = width;
	data->height = window_height;

	g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
			 (GSourceFunc) chat_change_size_in_idle_cb,
			 data, g_free);
}

const gchar *
gossip_chat_get_name (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), NULL);

	if (GOSSIP_CHAT_GET_CLASS (chat)->get_name) {
		return GOSSIP_CHAT_GET_CLASS (chat)->get_name (chat);
	}

	return NULL;
}

gchar *
gossip_chat_get_tooltip (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), NULL);

	if (GOSSIP_CHAT_GET_CLASS (chat)->get_tooltip) {
		return GOSSIP_CHAT_GET_CLASS (chat)->get_tooltip (chat);
	}

	return NULL;
}

GdkPixbuf *
gossip_chat_get_status_pixbuf (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), NULL);

	if (GOSSIP_CHAT_GET_CLASS (chat)->get_status_pixbuf) {
		return GOSSIP_CHAT_GET_CLASS (chat)->get_status_pixbuf (chat);
	}

	return NULL;
}

GossipContact *
gossip_chat_get_contact (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), NULL);

	if (GOSSIP_CHAT_GET_CLASS (chat)->get_contact) {
		return GOSSIP_CHAT_GET_CLASS (chat)->get_contact (chat);
	}

	return NULL;
}

void
gossip_chat_get_geometry (GossipChat *chat,
		          int        *width,
			  int        *height)
{
	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	if (GOSSIP_CHAT_GET_CLASS (chat)->get_geometry) {
		GOSSIP_CHAT_GET_CLASS (chat)->get_geometry (chat, width, height);
	}
}

GtkWidget *
gossip_chat_get_widget (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), NULL);

	if (GOSSIP_CHAT_GET_CLASS (chat)->get_widget) {
		return GOSSIP_CHAT_GET_CLASS (chat)->get_widget (chat);
	}

	return NULL;
}

void
gossip_chat_clear (GossipChat *chat)
{
	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	gossip_chat_view_clear (chat->view);
}

void
gossip_chat_set_window (GossipChat *chat, GossipChatWindow *window)
{
	chat->priv->window = window;
}

GossipChatWindow *
gossip_chat_get_window (GossipChat *chat)
{
	return chat->priv->window;
}

void
gossip_chat_scroll_down (GossipChat *chat)
{
	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	gossip_chat_view_scroll_down (chat->view);
}

void
gossip_chat_cut (GossipChat *chat)
{
	GtkTextBuffer  *buffer;
	
	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	if (gtk_text_buffer_get_selection_bounds (buffer, NULL, NULL)) {
		GtkClipboard *clipboard;

		clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

		gtk_text_buffer_cut_clipboard (buffer, clipboard, TRUE);
	}
}

void
gossip_chat_copy (GossipChat *chat)
{
	GtkTextBuffer         *buffer;
	
	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	if (gossip_chat_view_get_selection_bounds (chat->view, NULL, NULL)) {
		gossip_chat_view_copy_clipboard (chat->view);
		return;
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	if (gtk_text_buffer_get_selection_bounds (buffer, NULL, NULL)) {
		GtkClipboard *clipboard;

		clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

		gtk_text_buffer_copy_clipboard (buffer, clipboard);
	}
}

void
gossip_chat_paste (GossipChat *chat)
{
	GtkTextBuffer         *buffer;
	GtkClipboard          *clipboard;

	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	gtk_text_buffer_paste_clipboard (buffer, clipboard, NULL, TRUE);
}

void
gossip_chat_present (GossipChat *chat)
{
	GossipChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	priv = chat->priv;
	
	if (priv->window == NULL) {
		gossip_chat_window_add_chat (
			gossip_chat_window_new (), chat);
        }

        gossip_chat_window_switch_to_chat (priv->window, chat);
        gtk_window_present (GTK_WINDOW (gossip_chat_window_get_dialog (priv->window)));

	gtk_widget_grab_focus (chat->input_text_view);
}

