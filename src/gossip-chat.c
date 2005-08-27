/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2005 Imendio AB
 * Copyright (C) 2003-2004 Geert-Jan Van den Bogaerde <geertjan@gnome.org>
 * Copyright (C) 2004      Martyn Russell <mr@gnome.org>
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
#include <stdlib.h>
#include <config.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <glib/gi18n.h>

#include "gossip-chat-window.h"
#include "gossip-marshal.h"
#include "gossip-spell.h"
#include "gossip-spell-dialog.h"
#include "gossip-ui-utils.h"
#include "gossip-chat.h"

#define d(x)


extern GConfClient *gconf_client;


struct _GossipChatPriv {
	GossipChatWindow *window;

	GossipSpell      *spell;

	/* Used to automatically shrink a window that has temporarily
	   grown due to long input */
	gint              padding_height;
	gint              default_window_height;
	gint              last_input_height;
};


typedef struct {
	GossipChat  *chat;
       	gchar       *word;

       	GtkTextIter  start;
       	GtkTextIter  end;
} GossipChatSpell;


static void      gossip_chat_class_init            (GossipChatClass *klass);
static void      gossip_chat_init                  (GossipChat      *chat);
static void      chat_finalize                     (GObject         *object);
static void      chat_input_text_buffer_changed_cb (GtkTextBuffer   *buffer,
						    GossipChat      *chat);
static void      chat_text_view_size_allocate_cb   (GtkWidget       *widget,
						    GtkAllocation   *allocation,
						    GossipChat      *chat);
static void      chat_text_populate_popup_cb       (GtkTextView     *view,
						    GtkMenu         *menu,
						    GossipChat      *chat);
static void      chat_text_check_word_spelling_cb  (GtkMenuItem     *menuitem,
						    GossipChatSpell *chat_spell);
GossipChatSpell *chat_spell_new                    (GossipChat      *chat,
						    const gchar     *word,
						    GtkTextIter      start,
						    GtkTextIter      end);
static void      chat_spell_free                   (GossipChatSpell *chat_spell);


enum {
	COMPOSING,
	NEW_MESSAGE,
	NAME_CHANGED,
	STATUS_CHANGED,
	LAST_SIGNAL
};


static GObjectClass *parent_class = NULL;
static guint         chat_signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (GossipChat, gossip_chat, G_TYPE_OBJECT);


static void
gossip_chat_class_init (GossipChatClass *klass)
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
gossip_chat_init (GossipChat *chat)
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

	g_signal_connect (GTK_TEXT_VIEW (chat->input_text_view),
			  "populate_popup",
			  G_CALLBACK (chat_text_populate_popup_cb),
			  chat);

	gossip_chat_view_set_margin (chat->view, 3);

	/* create misspelt words identification tag */
	gtk_text_buffer_create_tag (buffer,
				    "misspelled",
				    "underline", PANGO_UNDERLINE_ERROR,
				    NULL);

	priv->spell = gossip_spell_new (NULL);
}

static void
chat_finalize (GObject *object)
{
	GossipChat     *chat;
	GossipChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT (object));

	chat = GOSSIP_CHAT (object);
	priv = chat->priv;

	gossip_spell_unref (priv->spell);
	g_free (priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
chat_input_text_buffer_changed_cb (GtkTextBuffer *buffer, GossipChat *chat)
{
	GossipChatPriv *priv;

	GtkTextIter     start, end;
	gchar          *str;
	gboolean        spell_checker;

	priv = chat->priv;

 	spell_checker = gconf_client_get_bool (gconf_client, 
					       "/apps/gossip/conversation/enable_spell_checker", 
					       NULL); 

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

	gtk_text_buffer_get_start_iter (buffer, &start);

	if (!spell_checker) {
		gtk_text_buffer_get_end_iter (buffer, &end);
		gtk_text_buffer_remove_tag_by_name (buffer, "misspelled", &start, &end);
		return;
	}

	if (!gossip_spell_supported () || !gossip_spell_has_backend (priv->spell)) {
		return;
	}
	
	/* NOTE: this is really inefficient, we shouldn't have to
	   reiterate the whole buffer each time and check each work
	   every time. */
	while (TRUE) {
		gboolean correct = FALSE;

		/* if at start */
		if (gtk_text_iter_is_start (&start)) {
			end = start;

			if (!gtk_text_iter_forward_word_end (&end)) {
				/* no whole word yet */
				break;
			}
		} else {
			if (!gtk_text_iter_forward_word_end (&end)) {
				/* must be the end of the buffer */
				break;
			}

			start = end; 
			gtk_text_iter_backward_word_start (&start);
		}
	
		str = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

		/* spell check string */
		correct = gossip_spell_check (priv->spell, str);
		if (!correct) {
			gtk_text_buffer_apply_tag_by_name (buffer, "misspelled", &start, &end);
		} else {
			gtk_text_buffer_remove_tag_by_name (buffer, "misspelled", &start, &end);
		}

		g_free (str);

		/* set start iter to the end iters position */
		start = end; 
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

static void
chat_text_populate_popup_cb (GtkTextView *view,
			     GtkMenu     *menu,
			     GossipChat  *chat)
{
	GtkTextBuffer      *buffer;
	GtkTextTagTable    *table;
	GtkTextTag         *tag;
	gint                x, y;
	GtkTextIter         iter, start, end;
	GtkWidget          *item;
	gchar              *str = NULL;
	GossipChatSpell    *chat_spell;

	buffer = gtk_text_view_get_buffer (view);
	table = gtk_text_buffer_get_tag_table (buffer);
	
	/* handle misspelled tags */
	tag = gtk_text_tag_table_lookup (table, "misspelled");
	
	gtk_widget_get_pointer (GTK_WIDGET (view), &x, &y);
	
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (view), 
					       GTK_TEXT_WINDOW_WIDGET,
					       x, y,
					       &x, &y);
	
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &iter, x, y);

	start = end = iter;
	
	if (gtk_text_iter_backward_to_tag_toggle (&start, tag) &&
	    gtk_text_iter_forward_to_tag_toggle (&end, tag)) {
					
		str = gtk_text_buffer_get_text (buffer, 
						&start, &end, FALSE);
	}

	if (!str || strlen (str) == 0) {
		return;
	}

	chat_spell = chat_spell_new (chat, str, start, end);

	g_object_set_data_full (G_OBJECT (menu), 
				"chat_spell", chat_spell, 
				(GDestroyNotify) chat_spell_free);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
	
	item = gtk_menu_item_new_with_mnemonic (_("_Check Word Spelling..."));
	g_signal_connect (item,
			  "activate",
			  G_CALLBACK (chat_text_check_word_spelling_cb),
			  chat_spell);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
}

GossipChatSpell *
chat_spell_new (GossipChat  *chat, 
		const gchar *word,
		GtkTextIter  start,
		GtkTextIter  end)
{
	GossipChatSpell *chat_spell;

	g_return_val_if_fail (chat != NULL, NULL);
	g_return_val_if_fail (word != NULL, NULL);

	chat_spell = g_new0 (GossipChatSpell, 1);

	chat_spell->chat = g_object_ref (chat);


	chat_spell->word = g_strdup (word);

	chat_spell->start = start;
	chat_spell->end = end;
	
	return chat_spell;
}

static void
chat_spell_free (GossipChatSpell *chat_spell)
{
	g_object_unref (chat_spell->chat);
	g_free (chat_spell->word);
	g_free (chat_spell);
}

static void     
chat_text_check_word_spelling_cb (GtkMenuItem     *menuitem, 
				  GossipChatSpell *chat_spell)
{
	GossipChatPriv *priv;

	priv = chat_spell->chat->priv;

	gossip_spell_dialog_show (chat_spell->chat,
				  priv->spell,
				  chat_spell->start,
				  chat_spell->end,
				  chat_spell->word);
}

void
gossip_chat_correct_word (GossipChat  *chat,
			  GtkTextIter  start,
			  GtkTextIter  end,
			  const gchar *new_word)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (chat != NULL);
	g_return_if_fail (new_word != NULL);
	g_return_if_fail (strlen (new_word) > 0);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));

	gtk_text_buffer_delete (buffer, &start, &end);
	gtk_text_buffer_insert (buffer, &start, 
				new_word, 
				g_utf8_strlen (new_word, -1));
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

GossipContact *
gossip_chat_get_own_contact (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), NULL);

	if (GOSSIP_CHAT_GET_CLASS (chat)->get_own_contact) {
		return GOSSIP_CHAT_GET_CLASS (chat)->get_own_contact (chat);
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

gboolean
gossip_chat_get_group_chat (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), FALSE);

	if (GOSSIP_CHAT_GET_CLASS (chat)->get_group_chat) {
		return GOSSIP_CHAT_GET_CLASS (chat)->get_group_chat (chat);
	}

	return FALSE;
}

gboolean 
gossip_chat_get_show_contacts (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_CHAT (chat), FALSE);

	if (GOSSIP_CHAT_GET_CLASS (chat)->get_show_contacts) {
		return GOSSIP_CHAT_GET_CLASS (chat)->get_show_contacts (chat);
	}

	return FALSE;
}

void 
gossip_chat_set_show_contacts (GossipChat *chat, 
			       gboolean    show)
{
	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	if (GOSSIP_CHAT_GET_CLASS (chat)->set_show_contacts) {
		GOSSIP_CHAT_GET_CLASS (chat)->set_show_contacts (chat, show);
	}
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
		GossipChatWindow *window;
		gboolean          for_group_chat;

		for_group_chat = gossip_chat_get_group_chat (chat);

		/* get the default window for either group chats or
		   normal chats, we do not want to mix them */
		window = gossip_chat_window_get_default ();

		if (!window) {
			window = gossip_chat_window_new ();
		}

		gossip_chat_window_add_chat (window, chat);
        }

        gossip_chat_window_switch_to_chat (priv->window, chat);
	gossip_ui_utils_window_present (
		GTK_WINDOW (gossip_chat_window_get_dialog (priv->window)));

	gtk_widget_grab_focus (chat->input_text_view);
}

