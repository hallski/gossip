/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2005 Imendio AB
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
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <glib/gi18n.h>
#include <libgnome/gnome-url.h>

#include <libgossip/gossip-time.h>
#include <libgossip/gossip-utils.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-chatroom-provider.h>

#include "gossip-chat-view.h"
#include "gossip-app.h"

/* Number of seconds between timestamps when using normal mode, 5 minutes. */
#define TIMESTAMP_INTERVAL 300

/* Maximum lines in any chat buffer, see bug #141292. */
#define MAX_LINES 1000

typedef enum {
	BLOCK_TYPE_NONE,
	BLOCK_TYPE_SELF,
	BLOCK_TYPE_OTHER,
	BLOCK_TYPE_EVENT,
	BLOCK_TYPE_TIME,
	BLOCK_TYPE_INVITE
} BlockType;

struct _GossipChatViewPriv {
	GtkTextBuffer *buffer;

	gboolean       irc_style;
	time_t         last_timestamp;
	BlockType      last_block_type;

	/* This is for the group chat so we know if the "other" last contact
	 * changed, so we know whether to insert a header or not.
	 */
	GossipContact *last_contact;
};


typedef struct {
	GossipSmiley  smiley;
	gchar        *pattern;
	gint          index;
} GossipSmileyPattern;


static GossipSmileyPattern smileys[] = {
	{ GOSSIP_SMILEY_NORMAL,       ":)",  0 },
	{ GOSSIP_SMILEY_WINK,         ";)",  0 },
	{ GOSSIP_SMILEY_WINK,         ";-)", 0 },
	{ GOSSIP_SMILEY_BIGEYE,       "=)",  0 },
	{ GOSSIP_SMILEY_NOSE,         ":-)", 0 },
	{ GOSSIP_SMILEY_CRY,          ":'(", 0 },
	{ GOSSIP_SMILEY_SAD,          ":(",  0 },
	{ GOSSIP_SMILEY_SAD,          ":-(", 0 },
	{ GOSSIP_SMILEY_SCEPTICAL,    ":/",  0 },
	{ GOSSIP_SMILEY_SCEPTICAL,    ":\\", 0 },
	{ GOSSIP_SMILEY_BIGSMILE,     ":D",  0 },
	{ GOSSIP_SMILEY_BIGSMILE,     ":-D", 0 },
	{ GOSSIP_SMILEY_INDIFFERENT,  ":|",  0 },
	{ GOSSIP_SMILEY_TOUNGE,       ":p",  0 },
	{ GOSSIP_SMILEY_TOUNGE,       ":-p", 0 },
	{ GOSSIP_SMILEY_TOUNGE,       ":P",  0 },
	{ GOSSIP_SMILEY_TOUNGE,       ":-P", 0 },
	{ GOSSIP_SMILEY_TOUNGE,       ";p",  0 },
	{ GOSSIP_SMILEY_TOUNGE,       ";-p", 0 },
	{ GOSSIP_SMILEY_TOUNGE,       ";P",  0 },
	{ GOSSIP_SMILEY_TOUNGE,       ";-P", 0 },
	{ GOSSIP_SMILEY_SHOCKED,      ":o",  0 },
	{ GOSSIP_SMILEY_SHOCKED,      ":-o", 0 },
	{ GOSSIP_SMILEY_SHOCKED,      ":O",  0 },
	{ GOSSIP_SMILEY_SHOCKED,      ":-O", 0 },
	{ GOSSIP_SMILEY_COOL,         "8)",  0 },
	{ GOSSIP_SMILEY_COOL,         "B)",  0 },
	{ GOSSIP_SMILEY_SORRY,        "*|",  0 },
	{ GOSSIP_SMILEY_KISS,         ":*",  0 },
	{ GOSSIP_SMILEY_SHUTUP,       ":#",  0 },
	{ GOSSIP_SMILEY_SHUTUP,       ":-#", 0 },
	{ GOSSIP_SMILEY_YAWN,         "|O",  0 },
	{ GOSSIP_SMILEY_CONFUSED,     ":S",  0 },
	{ GOSSIP_SMILEY_CONFUSED,     ":s",  0 },
	{ GOSSIP_SMILEY_ANGEL,        "<)",  0 },
	{ GOSSIP_SMILEY_OOOH,         ":x",  0 },
	{ GOSSIP_SMILEY_LOOKAWAY,     "*)",  0 },
	{ GOSSIP_SMILEY_LOOKAWAY,     "*-)", 0 },
	{ GOSSIP_SMILEY_BLUSH,        "*S",  0 },
	{ GOSSIP_SMILEY_BLUSH,        "*s",  0 },
	{ GOSSIP_SMILEY_BLUSH,        "*$",  0 },
	{ GOSSIP_SMILEY_COOLBIGSMILE, "8D",  0 },
	{ GOSSIP_SMILEY_ANGRY,        ":@",  0 },
	{ GOSSIP_SMILEY_BOSS,         "@)",  0 },
	{ GOSSIP_SMILEY_MONKEY,       "#)",  0 },
	{ GOSSIP_SMILEY_SILLY,        "O)",  0 },
	{ GOSSIP_SMILEY_SICK,         "+o(", 0 },

	/* backward smiley's */
	{ GOSSIP_SMILEY_NORMAL,       "(:",  0 },
	{ GOSSIP_SMILEY_WINK,         "(;",  0 },
	{ GOSSIP_SMILEY_WINK,         "(-;", 0 },
	{ GOSSIP_SMILEY_BIGEYE,       "(=",  0 },
	{ GOSSIP_SMILEY_NOSE,         "(-:", 0 },
	{ GOSSIP_SMILEY_CRY,          ")':", 0 },
	{ GOSSIP_SMILEY_SAD,          "):",  0 },
	{ GOSSIP_SMILEY_SAD,          ")-:", 0 },
	{ GOSSIP_SMILEY_SCEPTICAL,    "/:",  0 },
	{ GOSSIP_SMILEY_SCEPTICAL,    "//:", 0 },
	{ GOSSIP_SMILEY_INDIFFERENT,  "|:",  0 },
	{ GOSSIP_SMILEY_TOUNGE,       "d:",  0 },
	{ GOSSIP_SMILEY_TOUNGE,       "d-:", 0 },
	{ GOSSIP_SMILEY_TOUNGE,       "d;",  0 },
	{ GOSSIP_SMILEY_TOUNGE,       "d-;", 0 },
	{ GOSSIP_SMILEY_SHOCKED,      "o:",  0 },
	{ GOSSIP_SMILEY_SHOCKED,      "O:",  0 },
	{ GOSSIP_SMILEY_COOL,         "(8",  0 },
	{ GOSSIP_SMILEY_COOL,         "(B",  0 },
	{ GOSSIP_SMILEY_SORRY,        "|*",  0 },
	{ GOSSIP_SMILEY_KISS,         "*:",  0 },
	{ GOSSIP_SMILEY_SHUTUP,       "#:",  0 },
	{ GOSSIP_SMILEY_SHUTUP,       "#-:", 0 },
	{ GOSSIP_SMILEY_YAWN,         "O|",  0 },
	{ GOSSIP_SMILEY_CONFUSED,     "S:",  0 },
	{ GOSSIP_SMILEY_CONFUSED,     "s:",  0 },
	{ GOSSIP_SMILEY_ANGEL,        "(>",  0 },
	{ GOSSIP_SMILEY_OOOH,         "x:",  0 },
	{ GOSSIP_SMILEY_LOOKAWAY,     "(*",  0 },
	{ GOSSIP_SMILEY_LOOKAWAY,     "(-*", 0 },
	{ GOSSIP_SMILEY_BLUSH,        "S*",  0 },
	{ GOSSIP_SMILEY_BLUSH,        "s*",  0 },
	{ GOSSIP_SMILEY_BLUSH,        "$*",  0 },
	{ GOSSIP_SMILEY_ANGRY,        "@:",  0 },
	{ GOSSIP_SMILEY_BOSS,         "(@",  0 },
	{ GOSSIP_SMILEY_MONKEY,       "#)",  0 },
	{ GOSSIP_SMILEY_SILLY,        "(O",  0 },
	{ GOSSIP_SMILEY_SICK,         ")o+", 0 }
};

static gint num_smileys = G_N_ELEMENTS (smileys);


static void       gossip_chat_view_class_init          (GossipChatViewClass      *klass);
static void       gossip_chat_view_init                (GossipChatView           *view);
static void       chat_view_finalize                   (GObject                  *object);
static void       chat_view_size_allocate              (GtkWidget                *widget,
							GtkAllocation            *alloc);
static void       chat_view_setup_tags                 (GossipChatView           *view);
static void       chat_view_populate_popup             (GossipChatView           *view,
							GtkMenu                  *menu,
							gpointer                  user_data);
static gboolean   chat_view_event_cb                   (GossipChatView           *view,
							GdkEventMotion           *event,
							GtkTextTag               *tag);
static gboolean   chat_view_url_event_cb               (GtkTextTag               *tag,
							GObject                  *object,
							GdkEvent                 *event,
							GtkTextIter              *iter,
							GtkTextBuffer            *buffer);
static void       chat_view_open_address               (const gchar              *url);
static void       chat_view_open_address_cb            (GtkMenuItem              *menuitem,
							const gchar              *url);
static void       chat_view_copy_address_cb            (GtkMenuItem              *menuitem,
							const gchar              *url);
static void       chat_view_clear_view_cb              (GtkMenuItem              *menuitem,
							GossipChatView           *view);
static void       chat_view_insert_text_with_emoticons (GtkTextBuffer            *buf,
							GtkTextIter              *iter,
							const gchar              *str);
static GdkPixbuf *chat_view_get_smiley                 (GossipSmiley              smiley);
static gboolean   chat_view_is_scrolled_down           (GossipChatView           *view);
static void       chat_view_invite_accept_cb           (GtkWidget                *button,
							gpointer                  user_data);
static void       chat_view_invite_join_cb             (GossipChatroomProvider   *provider,
							GossipChatroomJoinResult  result,
							gint                      id,
							gpointer                  user_data);
static void       chat_view_maybe_append_date_and_time (GossipChatView           *view,
							GossipMessage            *msg);
static void       chat_view_append_spacing             (GossipChatView           *view);
static void       chat_view_append_text                (GossipChatView           *view,
							const gchar              *body,
							const gchar              *tag);
static void       chat_view_maybe_append_fancy_header  (GossipChatView           *view,
							GossipMessage            *msg,
							GossipContact            *my_contact,
							gboolean                  from_self);
static void       chat_view_append_irc_action          (GossipChatView           *view,
							GossipMessage            *msg,
							GossipContact            *my_contact,
							gboolean                  from_self);
static void       chat_view_append_fancy_action        (GossipChatView           *view,
							GossipMessage            *msg,
							GossipContact            *my_contact,
							gboolean                  from_self);
static void       chat_view_append_irc_message         (GossipChatView           *view,
							GossipMessage            *msg,
							GossipContact            *contact,
							gboolean                  from_self);
static void       chat_view_append_fancy_message       (GossipChatView           *view,
							GossipMessage            *msg,
							GossipContact            *my_contact,
							gboolean                  from_self);


G_DEFINE_TYPE (GossipChatView, gossip_chat_view, GTK_TYPE_TEXT_VIEW);


static void
gossip_chat_view_class_init (GossipChatViewClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = chat_view_finalize;
	widget_class->size_allocate = chat_view_size_allocate;
}

static void
gossip_chat_view_init (GossipChatView *view)
{
	GossipChatViewPriv *priv;

	priv = g_new0 (GossipChatViewPriv, 1);
	view->priv = priv;

	priv->last_block_type = BLOCK_TYPE_NONE;
	priv->last_timestamp = 0;

	priv->buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	g_object_set (view,
		      "wrap-mode", GTK_WRAP_WORD_CHAR,
		      "editable", FALSE,
		      "cursor-visible", FALSE,
		      NULL);

	gossip_chat_view_set_irc_style (view, TRUE);
	
	chat_view_setup_tags (view);

	g_signal_connect (view,
			  "populate_popup",
			  G_CALLBACK (chat_view_populate_popup),
			  NULL);
}

static void
chat_view_finalize (GObject *object)
{
	GossipChatView     *view = GOSSIP_CHAT_VIEW (object);
	GossipChatViewPriv *priv;

	priv = view->priv;

	g_free (priv);

	G_OBJECT_CLASS (gossip_chat_view_parent_class)->finalize (object);
}

static void
chat_view_size_allocate (GtkWidget     *widget,
			 GtkAllocation *alloc)
{
	gboolean down;

	down = chat_view_is_scrolled_down (GOSSIP_CHAT_VIEW (widget));

	GTK_WIDGET_CLASS (gossip_chat_view_parent_class)->size_allocate (widget, alloc);

	if (down) {
		gossip_chat_view_scroll_down (GOSSIP_CHAT_VIEW (widget));
	}
}

static void
chat_view_setup_tags (GossipChatView *view)
{
	GossipChatViewPriv *priv;
	GtkTextTag         *tag;

	priv = view->priv;

	gtk_text_buffer_create_tag (priv->buffer,
				    "cut",
				    NULL);

	tag = gtk_text_buffer_create_tag (priv->buffer,
					  "link",
					  NULL);
	
	g_signal_connect (tag,
			  "event",
			  G_CALLBACK (chat_view_url_event_cb),
			  priv->buffer);

	g_signal_connect (view,
			  "event",
			  G_CALLBACK (chat_view_event_cb),
			  tag);

	/* Fancy style */
	
	gtk_text_buffer_create_tag (priv->buffer,
				    "fancy-spacing",
				    "size", 3000,
				    NULL);

#define FANCY_BODY_SELF "#dcdcdc"
#define FANCY_HEAD_SELF "#b9b9b9"
#define FANCY_LINE_SELF "#aeaeae"

	gtk_text_buffer_create_tag (priv->buffer,
				    "fancy-header-self",
				    "foreground", "black",
				    "paragraph-background", FANCY_HEAD_SELF,
				    "weight", PANGO_WEIGHT_BOLD,
				    "pixels-above-lines", 2,
				    "pixels-below-lines", 2,
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "fancy-line-self",
				    "size", 1,
				    "paragraph-background", FANCY_LINE_SELF,
				    NULL);
	
	gtk_text_buffer_create_tag (priv->buffer,
				    "fancy-body-self",
				    "foreground", "black",
				    "paragraph-background", FANCY_BODY_SELF,
				    "pixels-above-lines", 2,
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "fancy-action-self",
				    "foreground", "brown4",
				    "style", PANGO_STYLE_ITALIC,
				    "paragraph-background", FANCY_BODY_SELF,
				    "pixels-above-lines", 2,
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "fancy-highlight-self",
				    "foreground", "black",
				    "weight", PANGO_WEIGHT_BOLD,
				    "paragraph-background", FANCY_BODY_SELF,
				    "pixels-above-lines", 2,
				    NULL);			    
	
#define FANCY_BODY_OTHER "#adbdc8"
#define FANCY_HEAD_OTHER "#88a2b4"
#define FANCY_LINE_OTHER "#7f96a4"

	gtk_text_buffer_create_tag (priv->buffer,
				    "fancy-header-other",
				    "foreground", "black",
				    "paragraph-background", FANCY_HEAD_OTHER,
				    "weight", PANGO_WEIGHT_BOLD,
				    "pixels-above-lines", 2,
				    "pixels-below-lines", 2,
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "fancy-line-other",
				    "size", 1,
				    "paragraph-background", FANCY_LINE_OTHER,
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "fancy-body-other",
				    "foreground", "black",
				    "paragraph-background", FANCY_BODY_OTHER,
				    "pixels-above-lines", 2,
				    NULL);
	
	gtk_text_buffer_create_tag (priv->buffer,
				    "fancy-action-other",
				    "foreground", "brown4",
				    "style", PANGO_STYLE_ITALIC,
				    "paragraph-background", FANCY_BODY_OTHER,
				    "pixels-above-lines", 2,
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "fancy-highlight-other",
				    "foreground", "black",
				    "weight", PANGO_WEIGHT_BOLD,
				    "paragraph-background", FANCY_BODY_OTHER,
				    "pixels-above-lines", 2,
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "fancy-time",
				    "foreground", "darkgrey",
				    "justification", GTK_JUSTIFY_CENTER,
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "fancy-event",
				    "foreground", "darkgrey",
				    "justification", GTK_JUSTIFY_CENTER,
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "fancy-invite",
				    "foreground", "sienna",
				    NULL);
	
	gtk_text_buffer_create_tag (priv->buffer,
				    "fancy-link",
				    "foreground", "#49789e",
				    "underline", PANGO_UNDERLINE_SINGLE,
				    NULL);

	/* IRC style */

	gtk_text_buffer_create_tag (priv->buffer,
				    "irc-spacing",
				    "size", 2000,
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "irc-nick-self",
				    "foreground", "sea green",
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "irc-body-self",
				    "foreground", "black",
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "irc-action-self",
				    "foreground", "brown4",
				    "style", PANGO_STYLE_ITALIC,
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "irc-nick-highlight",
				    "foreground", "indian red",
				    "weight", PANGO_WEIGHT_BOLD,
				    NULL);			    
	
	gtk_text_buffer_create_tag (priv->buffer,
				    "irc-nick-other",
				    "foreground", "skyblue4",
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "irc-body-other",
				    "foreground", "black",
				    NULL);
	
	gtk_text_buffer_create_tag (priv->buffer,
				    "irc-action-other",
				    "foreground", "brown4",
				    "style", PANGO_STYLE_ITALIC,
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "irc-time",
				    "foreground", "darkgrey",
				    "justification", GTK_JUSTIFY_CENTER,
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "irc-event",
				    "foreground", "darkgrey",
				    "justification", GTK_JUSTIFY_CENTER,
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "irc-invite",
				    "foreground", "sienna",
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "irc-link",
				    "foreground", "steelblue",
				    "underline", PANGO_UNDERLINE_SINGLE,
				    NULL);
}

static void
chat_view_populate_popup (GossipChatView *view,
			  GtkMenu        *menu,
			  gpointer        user_data)
{
	GossipChatViewPriv *priv;
	GtkTextTagTable    *table;
	GtkTextTag         *tag;
	gint                x, y;
	GtkTextIter         iter, start, end;
	GtkWidget          *item;
	gchar              *str = NULL;

	priv = view->priv;

	table = gtk_text_buffer_get_tag_table (priv->buffer);
	tag = gtk_text_tag_table_lookup (table, "link");

	gtk_widget_get_pointer (GTK_WIDGET (view), &x, &y);

	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (view),
					       GTK_TEXT_WINDOW_WIDGET,
					       x, y,
					       &x, &y);

	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &iter, x, y);

	start = end = iter;

	if (gtk_text_iter_backward_to_tag_toggle (&start, tag) &&
	    gtk_text_iter_forward_to_tag_toggle (&end, tag)) {

		str = gtk_text_buffer_get_text (priv->buffer,
						&start, &end, FALSE);
	}

	item = gtk_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	item = gtk_menu_item_new_with_mnemonic (_("C_lear"));
	g_signal_connect (item,
			  "activate",
			  G_CALLBACK (chat_view_clear_view_cb),
			  view);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	if (!str || strlen (str) == 0) {
		return;
	}

	/* Set data just to get the string freed when not needed. */
	g_object_set_data_full (G_OBJECT (menu),
				"url", str,
				(GDestroyNotify) g_free);

	item = gtk_menu_item_new_with_mnemonic (_("_Copy Link Address"));
	g_signal_connect (item,
			  "activate",
			  G_CALLBACK (chat_view_copy_address_cb),
			  str);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	item = gtk_menu_item_new_with_mnemonic (_("_Open Link"));
	g_signal_connect (item,
			  "activate",
			  G_CALLBACK (chat_view_open_address_cb),
			  str);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
}

static gboolean
chat_view_event_cb (GossipChatView *view,
		    GdkEventMotion *event,
		    GtkTextTag     *tag)
{
	static GdkCursor  *hand = NULL;
	static GdkCursor  *beam = NULL;
	GtkTextWindowType  type;
	GtkTextIter        iter;
	GdkWindow         *win;
	gint               x, y, buf_x, buf_y;

	type = gtk_text_view_get_window_type (GTK_TEXT_VIEW (view),
					      event->window);

	if (type != GTK_TEXT_WINDOW_TEXT) {
		return FALSE;
	}

	/* Get where the pointer really is. */
	win = gtk_text_view_get_window (GTK_TEXT_VIEW (view), type);
	gdk_window_get_pointer (win, &x, &y, NULL);

	/* Get the iter where the cursor is at */
	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (view), type,
					       x, y,
					       &buf_x, &buf_y);

	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view),
					    &iter,
					    buf_x, buf_y);

	if (!hand) {
		hand = gdk_cursor_new (GDK_HAND2);
		beam = gdk_cursor_new (GDK_XTERM);
	}

	if (gtk_text_iter_has_tag (&iter, tag)) {
		gdk_window_set_cursor (win, hand);
	} else {
		gdk_window_set_cursor (win, beam);
	}

	return FALSE;
}

static gboolean
chat_view_url_event_cb (GtkTextTag    *tag,
			GObject       *object,
			GdkEvent      *event,
			GtkTextIter   *iter,
			GtkTextBuffer *buffer)
{
	GtkTextIter  start, end;
	gchar       *str;

	/* If the link is being selected, don't do anything. */
	gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
	if (gtk_text_iter_get_offset (&start) != gtk_text_iter_get_offset (&end)) {
		return FALSE;
	}

	if (event->type == GDK_BUTTON_RELEASE && event->button.button == 1) {
		start = end = *iter;

		if (gtk_text_iter_backward_to_tag_toggle (&start, tag) &&
		    gtk_text_iter_forward_to_tag_toggle (&end, tag)) {
			str = gtk_text_buffer_get_text (buffer,
							&start,
							&end,
							FALSE);

			chat_view_open_address (str);
			g_free (str);
		}
	}

	return FALSE;
}

static void
chat_view_open_address (const gchar *url)
{
	if (!url || strlen (url) == 0) {
		return;
	}

	/* gnome_url_show doesn't work when there's no protocol, so we might
	 * need to add one.
	 */
	if (strstr (url, "://") == NULL) {
		gchar *tmp;

		tmp = g_strconcat ("http://", url, NULL);
		gnome_url_show (tmp, NULL);
		g_free (tmp);
		return;
	}

	gnome_url_show (url, NULL);
}

static void
chat_view_open_address_cb (GtkMenuItem *menuitem, const gchar *url)
{
	chat_view_open_address (url);
}

static void
chat_view_copy_address_cb (GtkMenuItem *menuitem, const gchar *url)
{
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clipboard, url, -1);

	clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text (clipboard, url, -1);
}

static void
chat_view_clear_view_cb (GtkMenuItem *menuitem, GossipChatView *view)
{
	gossip_chat_view_clear (view);
}

static void
chat_view_insert_text_with_emoticons (GtkTextBuffer *buf,
				      GtkTextIter   *iter,
				      const gchar   *str)
{
	const gchar *p;
	gunichar     c, prev_c;
	gint         i;
	gint         match;
	gint         submatch;
	gboolean     use_smileys;

	use_smileys = gconf_client_get_bool (
		gossip_app_get_gconf_client (),
		"/apps/gossip/conversation/graphical_smileys",
		NULL);

	if (!use_smileys) {
		gtk_text_buffer_insert (buf, iter, str, -1);
		return;
	}

	while (*str) {
		for (i = 0; i < num_smileys; i++) {
			smileys[i].index = 0;
		}

		match = -1;
		submatch = -1;
		p = str;
		prev_c = 0;
		while (*p) {
			c = g_utf8_get_char (p);

			if (match != -1 && g_unichar_isspace (c)) {
				break;
			} else {
				match = -1;
			}

			if (submatch != -1 || prev_c == 0 || g_unichar_isspace (prev_c)) {
				submatch = -1;

				for (i = 0; i < num_smileys; i++) {
					if (smileys[i].pattern[smileys[i].index] == c) {
						submatch = i;

						smileys[i].index++;
						if (!smileys[i].pattern[smileys[i].index]) {
							match = i;
						}
					} else {
						smileys[i].index = 0;
					}
				}
			}

			prev_c = c;
			p = g_utf8_next_char (p);
		}

		if (match != -1) {
			GdkPixbuf   *pixbuf;
			gint         len;
			const gchar *start;

			start = p - strlen (smileys[match].pattern);

			if (start > str) {
				len = start - str;
				gtk_text_buffer_insert (buf, iter, str, len);
			}

			pixbuf = chat_view_get_smiley (smileys[match].smiley);
			gtk_text_buffer_insert_pixbuf (buf, iter, pixbuf);

			gtk_text_buffer_insert (buf, iter, " ", 1);
		} else {
			gtk_text_buffer_insert (buf, iter, str, -1);
			return;
		}

		str = g_utf8_find_next_char (p, NULL);
	}
}

static GdkPixbuf *
chat_view_get_smiley (GossipSmiley smiley)
{
	static GdkPixbuf *pixbufs[NUM_SMILEYS];
	static gboolean   inited = FALSE;

	if (!inited) {
		gint i;

		for (i = 0; i < NUM_SMILEYS; i++) {
			pixbufs[i] = gossip_ui_utils_get_pixbuf_from_smiley (
				i, GTK_ICON_SIZE_MENU);
		}

		inited = TRUE;
	}

	return pixbufs[smiley];
}

GossipChatView *
gossip_chat_view_new (void)
{
	return g_object_new (GOSSIP_TYPE_CHAT_VIEW, NULL);
}

static gboolean
chat_view_is_scrolled_down (GossipChatView *view)
{
	GtkWidget *sw;

	sw = gtk_widget_get_parent (GTK_WIDGET (view));
	if (GTK_IS_SCROLLED_WINDOW (sw)) {
		GtkAdjustment *vadj;

		vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (sw));

		if (vadj->value + vadj->page_size / 2 < vadj->upper - vadj->page_size) {
			return FALSE;
		}
	}

	return TRUE;
}

void
gossip_chat_view_scroll_down (GossipChatView *view)
{
	GtkTextBuffer *buffer;
	GtkTextIter    iter;
	GtkTextMark   *mark;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	gtk_text_buffer_get_end_iter (buffer, &iter);
	mark = gtk_text_buffer_create_mark (buffer,
					    NULL,
					    &iter,
					    FALSE);

	gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
				      mark,
				      0.0,
				      FALSE,
				      0,
				      0);
}

static void
chat_view_maybe_trim_buffer (GossipChatView *view)
{
	GossipChatViewPriv *priv;
	GtkTextIter         top, bottom;
	gint                line;
	gint                remove;
	GtkTextTagTable    *table;
	GtkTextTag         *tag;

	priv = view->priv;

	gtk_text_buffer_get_end_iter (priv->buffer, &bottom);
	line = gtk_text_iter_get_line (&bottom);
	if (line < MAX_LINES) {
		return;
	}

	remove = line - MAX_LINES;
	gtk_text_buffer_get_start_iter (priv->buffer, &top);

	bottom = top;
	if (!gtk_text_iter_forward_lines (&bottom, remove)) {
		return;
	}

	/* Track backwords to a place where we can safely cut, we don't do it in
	 * the middle of a tag.
	 */
	table = gtk_text_buffer_get_tag_table (priv->buffer);
	tag = gtk_text_tag_table_lookup (table, "cut");
	if (!tag) {
		return;
	}
	
	if (!gtk_text_iter_forward_to_tag_toggle (&bottom, tag)) {
		return;
	}

	if (!gtk_text_iter_equal (&top, &bottom)) {
		gtk_text_buffer_delete (priv->buffer, &top, &bottom);
	}
}

static const gchar *
chat_view_get_my_name (GossipContact *my_contact)
{
	if (my_contact) {
		return gossip_contact_get_name (my_contact);
	} else {
		return gossip_session_get_nickname (gossip_app_get_session ());
	}
}

static gboolean
chat_view_check_nick_highlight (const gchar *msg, const gchar *to)
{
	gboolean  ret_val;
	gchar    *cf_msg, *cf_to;
	gchar    *ch;

	ret_val = FALSE;

	cf_msg = g_utf8_casefold (msg, -1);
	cf_to = g_utf8_casefold (to, -1);

	ch = strstr (cf_msg, cf_to);

	if (ch == NULL) {
		goto finished;
	}

	if (ch != cf_msg) {
		/* Not first in the message */
		if ((*(ch - 1) != ' ') &&
		    (*(ch - 1) != ',') &&
		    (*(ch - 1) != '.')) {
			goto finished;
		}
	}

	ch = ch + g_utf8_strlen (cf_to, -1);
	if (ch >= cf_msg + g_utf8_strlen (cf_msg, -1)) {
		ret_val = TRUE;
		goto finished;
	}

	if ((*ch == ' ') ||
	    (*ch == ',') ||
	    (*ch == '.') ||
	    (*ch == ':')) {
		ret_val = TRUE;
		goto finished;
	}

finished:
	g_free (cf_msg);
	g_free (cf_to);

	return ret_val;
}

void
gossip_chat_view_append_invite (GossipChatView *view,
				GossipMessage  *message)
{
	GossipChatViewPriv *priv;
	GossipContact      *sender;
	const gchar        *invite;
	const gchar        *body;
	GtkTextChildAnchor *anchor;
	GtkTextIter         iter;
	GtkWidget          *widget;
	const gchar        *used_msg;
	const gchar        *used_invite;
	gchar              *str;
	gboolean            bottom;
	const gchar        *tag;

	priv = view->priv;

	if (priv->irc_style) {
		tag = "irc-invite";
	} else {
		tag = "fancy-invite";
	}
	
	bottom = chat_view_is_scrolled_down (view);

	sender = gossip_message_get_sender (message);
	invite = gossip_message_get_invite (message);
	body = gossip_message_get_body (message);
		
	chat_view_maybe_append_date_and_time (view, message);

	if (body && body[0]) {
		used_msg = body;
	} else {
		used_msg = _("You have been invited to join a chat conference.");
	}

	/* Don't include the invite in the chat window if it is part of the
	 * actual request - some chat clients send this and it looks weird
	 * repeated.
	 */
	if (strstr (body, invite)) {
		used_invite = NULL;
	} else {
		used_invite = invite;
	}

	str = g_strdup_printf ("\n%s\n%s%s%s%s\n",
			       used_msg,
			       used_invite ? "(" : "",
			       used_invite ? used_invite : "",
			       used_invite ? ")" : "",
			       used_invite ? "\n" : "");
	chat_view_append_text (view, str, tag);
	g_free (str);

	gtk_text_buffer_get_end_iter (priv->buffer, &iter);
	anchor = gtk_text_buffer_create_child_anchor (priv->buffer, &iter);

	widget = gtk_button_new_with_label (_("Accept"));
	g_object_set_data_full (G_OBJECT (widget), "invite",
				g_strdup (invite), g_free);
 	g_object_set_data_full (G_OBJECT (widget), "contact",
 				g_object_ref (sender), g_object_unref);

	g_signal_connect (widget,
			  "clicked",
			  G_CALLBACK (chat_view_invite_accept_cb),
			  NULL);

	gtk_text_view_add_child_at_anchor (GTK_TEXT_VIEW (view),
					   widget,
					   anchor);

	gtk_widget_show_all (widget);

	gtk_text_buffer_get_end_iter (priv->buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
						  &iter,
						  "\n\n",
						  2,
						  tag,
						  NULL);

	if (bottom) {
		gossip_chat_view_scroll_down (view);
	}

	priv->last_block_type = BLOCK_TYPE_INVITE;
}

static void
chat_view_maybe_append_date_and_time (GossipChatView *view,
				      GossipMessage  *msg)
{
	GossipChatViewPriv *priv;
	const gchar        *tag;
	time_t              timestamp;
	GDate              *date, *last_date;
	GtkTextIter         iter;
	gboolean            append_date, append_time;
	GString            *str;

	priv = view->priv;

	if (priv->irc_style) {
		tag = "irc-time";
	} else {
		tag = "fancy-time";
	}
	
	if (priv->last_block_type == BLOCK_TYPE_TIME) {
		return;
	}

	str = g_string_new (NULL);
	
	timestamp = gossip_message_get_timestamp (msg);
	if (timestamp <= 0) {
		timestamp = gossip_time_get_current ();
	}

	date = g_date_new ();
	g_date_set_time (date, timestamp);

	last_date = g_date_new ();
	g_date_set_time (last_date, priv->last_timestamp);

	append_date = FALSE;
	append_time = FALSE;

	if (g_date_compare (date, last_date) > 0) {
		append_date = TRUE;
		append_time = TRUE;
	}

	if (priv->last_timestamp + TIMESTAMP_INTERVAL < timestamp) {
		append_time = TRUE;
	}

	if (append_time || append_date) {
		chat_view_append_spacing (view);

		g_string_append (str, "- ");
	}
	
	if (append_date) {
		gchar buf[256];
		
		g_date_strftime (buf, 256, _("%A %d %B %Y"), date);
		g_string_append (str, buf);

		if (append_time) {
			g_string_append (str, " - ");
		}
	}

	g_date_free (date);
	g_date_free (last_date);
	
	if (append_time) {
		gchar *tmp;

		tmp = gossip_time_to_timestamp (timestamp);
		g_string_append (str, tmp);
		g_free (tmp);
	}

	if (append_time || append_date) {
		g_string_append (str, " -\n");

		gtk_text_buffer_get_end_iter (priv->buffer, &iter);
		gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
							  &iter,
							  str->str, -1,
							  tag,
							  NULL);
		
		priv->last_block_type = BLOCK_TYPE_TIME;
		priv->last_timestamp = timestamp;
	}

	g_string_free (str, TRUE);
}

static void
chat_view_append_spacing (GossipChatView *view)
{
	GossipChatViewPriv *priv;
	const gchar        *tag;
	GtkTextIter         iter;
	
	priv = view->priv;

	if (priv->irc_style) {
		tag = "irc-spacing";
	} else {
		tag = "fancy-spacing";
	}
	
	gtk_text_buffer_get_end_iter (priv->buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
						  &iter,
						  "\n",
						  -1,
						  "cut",
						  tag,
						  NULL);
}

static void
chat_view_append_text (GossipChatView *view,
		       const gchar    *body,
		       const gchar    *tag)
{
	GossipChatViewPriv *priv;
	GtkTextIter         start_iter, end_iter;
	GtkTextMark        *mark;
	GtkTextIter         iter;
	gint                num_matches, i;
	GArray             *start, *end;
	const gchar        *link_tag;

	priv = view->priv;

	if (priv->irc_style) {
		link_tag = "irc-link";
	} else {
		link_tag = "fancy-link";
	}
	
	gtk_text_buffer_get_end_iter (priv->buffer, &start_iter);
	mark = gtk_text_buffer_create_mark (priv->buffer, NULL, &start_iter, TRUE);

	start = g_array_new (FALSE, FALSE, sizeof (gint));
	end = g_array_new (FALSE, FALSE, sizeof (gint));

	num_matches = gossip_utils_url_regex_match (body, start, end);

	if (num_matches == 0) {
		gtk_text_buffer_get_end_iter (priv->buffer, &iter);
		chat_view_insert_text_with_emoticons (priv->buffer, &iter, body);
	} else {
		gint   last = 0;
		gint   s = 0, e = 0;
		gchar *tmp;

		for (i = 0; i < num_matches; i++) {
			s = g_array_index (start, gint, i);
			e = g_array_index (end, gint, i);

			if (s > last) {
				tmp = gossip_utils_substring (body, last, s);

				gtk_text_buffer_get_end_iter (priv->buffer, &iter);
				chat_view_insert_text_with_emoticons (priv->buffer,
								      &iter,
								      tmp);
				g_free (tmp);
			}

			tmp = gossip_utils_substring (body, s, e);

			gtk_text_buffer_get_end_iter (priv->buffer, &iter);
			gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
								  &iter,
								  tmp,
								  -1,
								  link_tag,
								  "link",
								  NULL);

			g_free (tmp);

			last = e;
		}

		if (e < strlen (body)) {
			tmp = gossip_utils_substring (body, e, strlen (body));

			gtk_text_buffer_get_end_iter (priv->buffer, &iter);
			chat_view_insert_text_with_emoticons (priv->buffer,
							      &iter,
							      tmp);
			g_free (tmp);
		}
	}

	g_array_free (start, TRUE);
	g_array_free (end, TRUE);

	gtk_text_buffer_get_end_iter (priv->buffer, &iter);
	gtk_text_buffer_insert (priv->buffer, &iter, "\n", 1);

	/* Apply the style to the inserted text. */
	gtk_text_buffer_get_iter_at_mark (priv->buffer, &start_iter, mark);
	gtk_text_buffer_get_end_iter (priv->buffer, &end_iter);
	
	gtk_text_buffer_apply_tag_by_name (priv->buffer,
					   tag,
					   &start_iter,
					   &end_iter);
	
	gtk_text_buffer_delete_mark (priv->buffer, mark);
}

static void
chat_view_maybe_append_fancy_header (GossipChatView *view,
				     GossipMessage  *msg,
				     GossipContact  *my_contact,
				     gboolean        from_self)
{
	GossipChatViewPriv *priv;
	GossipContact      *contact;
	const gchar        *name;
	gboolean            header;
	GtkTextIter         iter;
	gchar              *tmp;
	const gchar        *tag;
	const gchar        *line_tag;

	priv = view->priv;

	contact = gossip_message_get_sender (msg);
	
	if (from_self) {
		name = chat_view_get_my_name (my_contact);
		
		tag = "fancy-header-self";
		line_tag = "fancy-line-self";
	} else {
		name = gossip_contact_get_name (contact);
		
		tag = "fancy-header-other";
		line_tag = "fancy-line-other";
	}

	header = FALSE;

	/* Only insert a header if the previously inserted block is not the same
	 * as this one. This catches all the different cases:
	 */
	if (priv->last_block_type != BLOCK_TYPE_SELF &&
	    priv->last_block_type != BLOCK_TYPE_OTHER) {
		header = TRUE;
	}
	else if (from_self && priv->last_block_type == BLOCK_TYPE_OTHER) {
		header = TRUE;
	}
	else if (!from_self && priv->last_block_type == BLOCK_TYPE_SELF) {
		header = TRUE;
	}
	else if (!from_self && 
		 (!priv->last_contact ||
		  !gossip_contact_equal (contact, priv->last_contact))) {
		header = TRUE;
	}
		
	if (!header) {
		return;
	}
	
	chat_view_append_spacing (view);
		
	gtk_text_buffer_get_end_iter (priv->buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
						  &iter,
						  "\n",
						  -1,
						  line_tag,
						  NULL);

	tmp = g_strdup_printf ("%s\n", name);
	gtk_text_buffer_get_end_iter (priv->buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
						  &iter,
						  tmp,
						  -1,
						  tag,
						  NULL);
	g_free (tmp);
	
	gtk_text_buffer_get_end_iter (priv->buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
						  &iter,
						  "\n",
						  -1,
						  line_tag,
						  NULL);
}

static void
chat_view_append_irc_action (GossipChatView *view,
			     GossipMessage  *msg,
			     GossipContact  *my_contact,
			     gboolean        from_self)
{
	GossipChatViewPriv *priv;
	const gchar        *name;
	GtkTextIter         iter;
	const gchar        *body;
	gchar              *tmp;
	const gchar        *tag;

	priv = view->priv;

	/* Skip the "/me ". */
	if (from_self) {
		name = chat_view_get_my_name (my_contact);
		
		tag = "irc-action-self";
	} else {
		name = gossip_contact_get_name (gossip_message_get_sender (msg));
		
		tag = "irc-action-other";
	}

	if (priv->last_block_type != BLOCK_TYPE_SELF &&
	    priv->last_block_type != BLOCK_TYPE_OTHER) {
		chat_view_append_spacing (view);
	}
	
	gtk_text_buffer_get_end_iter (priv->buffer, &iter);

	tmp = g_strdup_printf (" * %s ", name);
	gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
						  &iter,
						  tmp,
						  -1,
						  "cut",
						  tag,
						  NULL);
	g_free (tmp);

	body = gossip_message_get_body (msg) + 4;
	chat_view_append_text (view, body, tag);
}

static void
chat_view_append_fancy_action (GossipChatView *view,
			       GossipMessage  *msg,
			       GossipContact  *my_contact,
			       gboolean        from_self)
{
	GossipChatViewPriv *priv;
	GossipContact      *contact;
	const gchar        *name;
	const gchar        *body;
	GtkTextIter         iter;
	gchar              *tmp;
	const gchar        *tag;
	const gchar        *line_tag;

	priv = view->priv;

	contact = gossip_message_get_sender (msg);
	
	if (from_self) {
		name = chat_view_get_my_name (my_contact);
		
		tag = "fancy-action-self";
		line_tag = "fancy-line-self";
	} else {
		name = gossip_contact_get_name (gossip_message_get_sender (msg));
		
		tag = "fancy-action-other";
		line_tag = "fancy-line-other";
	}

	tmp = g_strdup_printf (" * %s ", name);
	gtk_text_buffer_get_end_iter (priv->buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
						  &iter,
						  tmp,
						  -1,
						  tag,
						  NULL);
	g_free (tmp);

	body = gossip_message_get_body (msg) + 4;
	chat_view_append_text (view, body, tag);
}

static void
chat_view_append_irc_message (GossipChatView *view,
			      GossipMessage  *msg,
			      GossipContact  *my_contact,
			      gboolean        from_self)
{
	GossipChatViewPriv *priv;
	const gchar        *name;
	const gchar        *body;
	const gchar        *nick_tag;
	const gchar        *body_tag;
	GtkTextIter         iter;
	gchar              *tmp;

	priv = view->priv;

	body = gossip_message_get_body (msg);

	if (from_self) {
		name = chat_view_get_my_name (my_contact);
		
		nick_tag = "irc-nick-self";
		body_tag = "irc-body-self";
	} else {
		const gchar *my_name;

		name = gossip_contact_get_name (gossip_message_get_sender (msg));

		if (my_contact) {
			my_name = gossip_contact_get_name (my_contact);
		} else {
			my_name = gossip_contact_get_name (
				gossip_message_get_recipient (msg));
		}
		
		if (chat_view_check_nick_highlight (body, my_name)) {
			nick_tag = "irc-nick-highlight";
		} else {
			nick_tag = "irc-nick-other";
		}
		
		body_tag = "irc-body-other";
	}

	if (priv->last_block_type != BLOCK_TYPE_SELF &&
	    priv->last_block_type != BLOCK_TYPE_OTHER) {
		chat_view_append_spacing (view);
	}

	gtk_text_buffer_get_end_iter (priv->buffer, &iter);

	/* The nickname. */
	tmp = g_strdup_printf ("%s: ", name);
	gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
						  &iter,
						  tmp,
						  -1,
						  "cut",
						  nick_tag,
						  NULL);
	g_free (tmp);

	/* The text body. */
	chat_view_append_text (view, body, body_tag);
}

static void
chat_view_append_fancy_message (GossipChatView *view,
				GossipMessage  *msg,
				GossipContact  *my_contact,
				gboolean        from_self)
{
	GossipChatViewPriv *priv;
	const gchar        *body;
	const gchar        *tag;

	priv = view->priv;
	
	if (from_self) {
		tag = "fancy-body-self";
	} else {
		tag = "fancy-body-other";

		/* FIXME: Might want to support nick highlighting here... */
	}
	
	body = gossip_message_get_body (msg);
	chat_view_append_text (view, body, tag);
}

/* The name is optional, if NULL, the sender for msg is used. */
void
gossip_chat_view_append_message_from_self (GossipChatView *view,
					   GossipMessage  *msg,
					   GossipContact  *my_contact)
{
	GossipChatViewPriv *priv;
	const gchar        *body;
	gboolean            scroll_down;

	priv = view->priv;

	body = gossip_message_get_body (msg);
	if (!body) {
		return;
	}

	scroll_down = chat_view_is_scrolled_down (view);

	chat_view_maybe_trim_buffer (view);
	chat_view_maybe_append_date_and_time (view, msg);

	if (!priv->irc_style) {
		chat_view_maybe_append_fancy_header (view, msg, my_contact, TRUE);
	}
	
	/* Handle action messages (/me) and normal messages, in combination with
	 * irc style and fancy style.
	 */
	if (g_str_has_prefix (body, "/me ")) {
		if (priv->irc_style) {
			chat_view_append_irc_action (view, msg, my_contact, TRUE);
		} else {
			chat_view_append_fancy_action (view, msg, my_contact, TRUE);
		}
	} else {
		if (priv->irc_style) {
			chat_view_append_irc_message (view, msg, my_contact, TRUE);
		} else {
			chat_view_append_fancy_message (view, msg, my_contact, TRUE);
		}
	}

	priv->last_block_type = BLOCK_TYPE_SELF;

	/* Reset the last inserted contact, since it was from self. */
	if (priv->last_contact) {
		g_object_unref (priv->last_contact);
		priv->last_contact = NULL;
	}
	
	if (scroll_down) {
		gossip_chat_view_scroll_down (view);
	}
}

/* The name is optional, if NULL, the sender for msg is used. */
void
gossip_chat_view_append_message_from_other (GossipChatView *view,
					    GossipMessage  *msg,
					    GossipContact  *my_contact)
{
	GossipChatViewPriv *priv;
	const gchar        *body;
	gboolean            scroll_down;

	priv = view->priv;

	body = gossip_message_get_body (msg);
	if (!body) {
		return;
	}

	scroll_down = chat_view_is_scrolled_down (view);

	chat_view_maybe_trim_buffer (view);
	chat_view_maybe_append_date_and_time (view, msg);

	if (!priv->irc_style) {
		chat_view_maybe_append_fancy_header (view, msg, my_contact, FALSE);
	}
	
	/* Handle action messages (/me) and normal messages, in combination with
	 * irc style and fancy style.
	 */
	if (g_str_has_prefix (body, "/me ")) {
		if (priv->irc_style) {
			chat_view_append_irc_action (view, msg, my_contact, FALSE);
		} else {
			chat_view_append_fancy_action (view, msg, my_contact, FALSE);
		}
	} else {
		if (priv->irc_style) {
			chat_view_append_irc_message (view, msg, my_contact, FALSE);
		} else {
			chat_view_append_fancy_message (view, msg, my_contact, FALSE);
		}
	}

	priv->last_block_type = BLOCK_TYPE_OTHER;

	/* Update the last contact that sent something. */
	if (priv->last_contact) {
		g_object_unref (priv->last_contact);
	}
	priv->last_contact = g_object_ref (gossip_message_get_sender (msg));
	
	if (scroll_down) {
		gossip_chat_view_scroll_down (view);
	}
}

void
gossip_chat_view_append_event (GossipChatView *view,
			       const gchar    *str)
{
	GossipChatViewPriv *priv;
	gboolean            bottom;
	GtkTextIter         iter;
	time_t              timestamp;
	gchar              *stamp;
	gchar              *msg;
	const gchar        *tag;

	priv = view->priv;
	
	bottom = chat_view_is_scrolled_down (view);

	chat_view_maybe_trim_buffer (view);

	if (priv->irc_style) {
		tag = "irc-event";
	} else {
		tag = "fancy-event";
	}
	
	if (priv->last_block_type != BLOCK_TYPE_EVENT) {
		chat_view_append_spacing (view);
	}
		
	gtk_text_buffer_get_end_iter (priv->buffer, &iter);

	timestamp = gossip_time_get_current ();
	stamp = gossip_time_to_timestamp (-1);
	msg = g_strdup_printf ("- %s - %s -\n", str, stamp);
	g_free (stamp);

	gtk_text_buffer_insert_with_tags_by_name (priv->buffer, &iter,
						  msg, -1,
						  tag,
						  NULL);
	g_free (msg);

	if (bottom) {
		gossip_chat_view_scroll_down (view);
	}

	priv->last_block_type = BLOCK_TYPE_EVENT;
	priv->last_timestamp = timestamp;
}

void
gossip_chat_view_clear (GossipChatView *view)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	gtk_text_buffer_set_text (buffer, "", -1);
}

gboolean
gossip_chat_view_get_selection_bounds (GossipChatView *view,
				       GtkTextIter    *start,
				       GtkTextIter    *end)
{
	GtkTextBuffer *buffer;

	g_return_val_if_fail (GOSSIP_IS_CHAT_VIEW (view), FALSE);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	return gtk_text_buffer_get_selection_bounds (buffer, start, end);
}

void
gossip_chat_view_copy_clipboard (GossipChatView *view)
{
	GtkTextBuffer *buffer;
	GtkClipboard  *clipboard;

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	gtk_text_buffer_copy_clipboard (buffer, clipboard);
}

static void
chat_view_invite_accept_cb (GtkWidget *button,
			    gpointer   user_data)
{
	GossipSession          *session;
	GossipAccount          *account;
	GossipContact          *contact;
	GossipChatroomProvider *provider;
	const gchar            *nickname;
	const gchar            *invite;

	invite = g_object_get_data (G_OBJECT (button), "invite");
	contact = g_object_get_data (G_OBJECT (button), "contact");

	gtk_widget_set_sensitive (button, FALSE);

	session = gossip_app_get_session ();
	account = gossip_contact_get_account (contact);
	provider = gossip_session_get_chatroom_provider (session, account);
	nickname = gossip_session_get_nickname (session);

	gossip_chatroom_provider_invite_accept (provider,
						chat_view_invite_join_cb,
						nickname,
						invite);
}

static void
chat_view_invite_join_cb (GossipChatroomProvider   *provider,
			  GossipChatroomJoinResult  result,
			  gint                      id,
			  gpointer                  user_data)
{
	gossip_group_chat_show (provider, id);
}

void
gossip_chat_view_set_irc_style (GossipChatView *view,
				gboolean        irc_style)
{
	GossipChatViewPriv *priv;
	gint                margin;
	
	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	priv = view->priv;

	priv->irc_style = irc_style;

	if (priv->irc_style) {
		margin = 3;
	} else {
		margin = 0;
	}
	
	g_object_set (view,
		      "left-margin", margin,
		      "right-margin", margin,
		      NULL);
}

gboolean
gossip_chat_view_get_irc_style (GossipChatView *view)
{
	GossipChatViewPriv *priv;
	
	g_return_val_if_fail (GOSSIP_IS_CHAT_VIEW (view), FALSE);

	priv = view->priv;

	return priv->irc_style;
}
