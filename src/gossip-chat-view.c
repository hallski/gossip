/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2006 Imendio AB
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
#include <glib/gi18n.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkscrolledwindow.h>

#ifdef USE_GNOMEVFS_FOR_URL
#include <libgnomevfs/gnome-vfs.h>
#else
#include <libgnome/gnome-url.h>
#endif

#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-time.h>
#include <libgossip/gossip-utils.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-chatroom-provider.h>

#include "gossip-app.h"
#include "gossip-chat-view.h"
#include "gossip-preferences.h"
#include "gossip-theme-manager.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj),	\
		       GOSSIP_TYPE_CHAT_VIEW, GossipChatViewPriv))

#define DEBUG_DOMAIN "ChatView"

/* Number of seconds between timestamps when using normal mode, 5 minutes. */
#define TIMESTAMP_INTERVAL 300

#define MAX_LINES 800

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

	gboolean       allow_scrolling;

	/* This is for the group chat so we know if the "other" last contact
	 * changed, so we know whether to insert a header or not.
	 */
	GossipContact *last_contact;

	guint          notify_system_fonts_id;
	guint          notify_show_avatars_id;
};

typedef struct {
	GossipSmiley  smiley;
	const gchar  *pattern;
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

static void     gossip_chat_view_class_init          (GossipChatViewClass      *klass);
static void     gossip_chat_view_init                (GossipChatView           *view);
static void     chat_view_finalize                   (GObject                  *object);
static gboolean chat_view_drag_motion                (GtkWidget                *widget,
						      GdkDragContext           *context,
						      gint                      x,
						      gint                      y,
						      guint                     time);
static void     chat_view_size_allocate              (GtkWidget                *widget,
						      GtkAllocation            *alloc);
static void     chat_view_setup_tags                 (GossipChatView           *view);
static void     chat_view_system_font_update         (GossipChatView           *view,
						      GConfClient              *gconf_client);
static void     chat_view_system_font_notify_cb      (GConfClient              *gconf_client,
						      guint                     id,
						      GConfEntry               *entry,
						      gpointer                  user_data);
static void     chat_view_show_avatars_notify_cb     (GConfClient              *gconf_client,
						      guint                     id,
						      GConfEntry               *entry,
						      gpointer                  user_data);
static void     chat_view_populate_popup             (GossipChatView           *view,
						      GtkMenu                  *menu,
						      gpointer                  user_data);
static gboolean chat_view_event_cb                   (GossipChatView           *view,
						      GdkEventMotion           *event,
						      GtkTextTag               *tag);
static gboolean chat_view_url_event_cb               (GtkTextTag               *tag,
						      GObject                  *object,
						      GdkEvent                 *event,
						      GtkTextIter              *iter,
						      GtkTextBuffer            *buffer);
static void     chat_view_open_address               (const gchar              *url);
static void     chat_view_open_address_cb            (GtkMenuItem              *menuitem,
						      const gchar              *url);
static void     chat_view_copy_address_cb            (GtkMenuItem              *menuitem,
						      const gchar              *url);
static void     chat_view_clear_view_cb              (GtkMenuItem              *menuitem,
						      GossipChatView           *view);
static void     chat_view_insert_text_with_emoticons (GtkTextBuffer            *buf,
						      GtkTextIter              *iter,
						      const gchar              *str);
static gboolean chat_view_is_scrolled_down           (GossipChatView           *view);
static void     chat_view_invite_accept_cb           (GtkWidget                *button,
						      gpointer                  user_data);
static void     chat_view_invite_decline_cb          (GtkWidget                *button,
						      gpointer                  user_data);
static void     chat_view_invite_join_cb             (GossipChatroomProvider   *provider,
						      GossipChatroomJoinResult  result,
						      gint                      id,
						      gpointer                  user_data);
static void     chat_view_theme_changed_cb           (GossipThemeManager       *manager,
						      GossipChatView           *view);
static void     chat_view_maybe_append_date_and_time (GossipChatView           *view,
						      GossipMessage            *msg);
static void     chat_view_append_spacing             (GossipChatView           *view);
static void     chat_view_append_text                (GossipChatView           *view,
						      const gchar              *body,
						      const gchar              *tag);
static void     chat_view_maybe_append_fancy_header  (GossipChatView           *view,
						      GossipMessage            *msg,
						      GossipContact            *my_contact,
						      gboolean                  from_self,
						      GdkPixbuf                *avatar);
static void     chat_view_append_irc_action          (GossipChatView           *view,
						      GossipMessage            *msg,
						      GossipContact            *my_contact,
						      gboolean                  from_self);
static void     chat_view_append_fancy_action        (GossipChatView           *view,
						      GossipMessage            *msg,
						      GossipContact            *my_contact,
						      gboolean                  from_self);
static void     chat_view_append_irc_message         (GossipChatView           *view,
						      GossipMessage            *msg,
						      GossipContact            *contact,
						      gboolean                  from_self);
static void     chat_view_append_fancy_message       (GossipChatView           *view,
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
	widget_class->drag_motion = chat_view_drag_motion;

	g_type_class_add_private (object_class, sizeof (GossipChatViewPriv));
}

static void
gossip_chat_view_init (GossipChatView *view)
{
	GossipChatViewPriv *priv;
	GConfClient        *gconf_client;

	priv = GET_PRIV (view);

	priv->buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	priv->last_block_type = BLOCK_TYPE_NONE;
	priv->last_timestamp = 0;

	priv->allow_scrolling = TRUE;

	g_object_set (view,
		      "wrap-mode", GTK_WRAP_WORD_CHAR,
		      "editable", FALSE,
		      "cursor-visible", FALSE,
		      NULL);

	gconf_client = gossip_app_get_gconf_client ();
	
	priv->notify_system_fonts_id = 
		gconf_client_notify_add (gconf_client,
					 "/desktop/gnome/interface/document_font_name",
					 chat_view_system_font_notify_cb,
					 view, NULL, NULL);
	chat_view_system_font_update (view, gconf_client);
	
	priv->notify_show_avatars_id = 
		gconf_client_notify_add (gconf_client,
					 GCONF_UI_SHOW_AVATARS,
					 chat_view_show_avatars_notify_cb,
					 view, NULL, NULL);

	chat_view_setup_tags (view);

	gossip_theme_manager_apply (gossip_theme_manager_get (), view);
	gossip_theme_manager_update_show_avatars (gossip_theme_manager_get (),
						  view,
						  gconf_client_get_bool (
							  gconf_client,
							  GCONF_UI_SHOW_AVATARS,
							  NULL));

	g_signal_connect (view,
			  "populate-popup",
			  G_CALLBACK (chat_view_populate_popup),
			  NULL);

	g_signal_connect_object (gossip_theme_manager_get (),
				 "theme-changed",
				 G_CALLBACK (chat_view_theme_changed_cb),
				 view,
				 0);
}

static void
chat_view_finalize (GObject *object)
{
	GossipChatView     *view;
	GossipChatViewPriv *priv;
	GConfClient        *gconf_client;

	view = GOSSIP_CHAT_VIEW (object);
	priv = GET_PRIV (view);

	gconf_client = gossip_app_get_gconf_client ();
	gconf_client_notify_remove (gconf_client, priv->notify_system_fonts_id);
	gconf_client_notify_remove (gconf_client, priv->notify_show_avatars_id);

	G_OBJECT_CLASS (gossip_chat_view_parent_class)->finalize (object);
}

static gboolean
chat_view_drag_motion (GtkWidget        *widget,
		       GdkDragContext   *context,
		       gint              x,
		       gint              y,
		       guint             time)
{
	/* Don't handle drag motion, since we don't want the view to scroll as
	 * the result of dragging something across it.
	 */
	
	return TRUE;
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

	priv = GET_PRIV (view);

	gtk_text_buffer_create_tag (priv->buffer,
				    "cut",
				    NULL);

	gtk_text_buffer_create_tag (priv->buffer,
				    "highlight",
				    "background", "yellow",
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
}

static void
chat_view_system_font_update (GossipChatView *view,
			      GConfClient    *gconf_client)
{
	PangoFontDescription *font_description = NULL;
	gchar                *font_name;

	font_name = gconf_client_get_string (gconf_client, 
					     "/desktop/gnome/interface/document_font_name", 
					     NULL);
	
	if (font_name) {
		font_description = pango_font_description_from_string (font_name);
		g_free (font_name);
	} else {
		font_description = NULL;
	}
	
	gtk_widget_modify_font (GTK_WIDGET (view), font_description);

	if (font_description) {
		pango_font_description_free (font_description);
	}
}

static void
chat_view_system_font_notify_cb (GConfClient    *gconf_client,
				 guint           id,
				 GConfEntry     *entry,
				 gpointer        user_data)
{
	GossipChatView *view;

	view = user_data;
	chat_view_system_font_update (view, gconf_client);
	
	/* Ugly, again, to adjust the vertical position of the nick... Will fix
	 * this when reworking the theme manager so that view register
	 * themselves with it instead of the other way around.
	 */
	gossip_theme_manager_update_show_avatars (gossip_theme_manager_get (),
						  view,
						  gconf_client_get_bool (
							  gconf_client,
							  GCONF_UI_SHOW_AVATARS,
							  NULL));
}

static void
chat_view_show_avatars_notify_cb (GConfClient    *gconf_client,
				  guint           id,
				  GConfEntry     *entry,
				  gpointer        user_data)
{
	GossipChatView     *view;
	GossipChatViewPriv *priv;
	GConfValue         *value;

	view = user_data;
	priv = GET_PRIV (view);

	value = gconf_entry_get_value (entry);
	
	gossip_theme_manager_update_show_avatars (gossip_theme_manager_get (),
						  view,
						  gconf_value_get_bool (value));
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

	priv = GET_PRIV (view);

	/* Clear menu item */
	if (gtk_text_buffer_get_char_count (priv->buffer) > 0) {
		item = gtk_menu_item_new ();
		gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);
		
		item = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLEAR, NULL);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);
		
		g_signal_connect (item,
				  "activate",
				  G_CALLBACK (chat_view_clear_view_cb),
				  view);
	}

	/* Link context menu items */
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

	if (!str || strlen (str) < 1) {
		g_free (str);
		return;
	}

	/* NOTE: Set data just to get the string freed when not needed. */
	g_object_set_data_full (G_OBJECT (menu),
				"url", str,
				(GDestroyNotify) g_free);

	item = gtk_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

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
	if (!win) {
		return FALSE;
	}

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
	}

	if (gtk_text_iter_has_tag (&iter, tag)) {
		gdk_window_set_cursor (win, hand);
	} else {
		gdk_window_set_cursor (win, NULL);
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
	gchar *real_url;

	/* The URL opening code can't handle schemeless strings, so we try to be
	 * smart and add http if there is no scheme or doesn't look like a mail
	 * address. This should work in most cases, and let us click on strings
	 * like "www.gnome.org".
	 */
	if (!g_str_has_prefix (url, "http://") &&
	    !strstr (url, ":/") &&
	    !strstr (url, "@")) {
		real_url = g_strdup_printf ("http://%s", url);
	} else {
		real_url = g_strdup (url);
	}

#ifdef USE_GNOMEVFS_FOR_URL
	{
		GnomeVFSResult result;
		
		result = gnome_vfs_url_show (real_url);
		if (result != GNOME_VFS_OK) {
			g_warning ("Couldn't show URL:'%s'", real_url);
		}
	}
#else
	{
		GError *error = NULL;
		
		gnome_url_show (real_url, &error);
		if (error) {
			g_warning ("Couldn't show URL:'%s'", real_url);
			g_error_free (error);
		}
	}
#endif

	g_free (real_url);
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
		GdkPixbuf   *pixbuf;
		gint         len;
		const gchar *start;

		for (i = 0; i < G_N_ELEMENTS (smileys); i++) {
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

				for (i = 0; i < G_N_ELEMENTS (smileys); i++) {
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

		if (match == -1) {
			gtk_text_buffer_insert (buf, iter, str, -1);
			return;
		}

		start = p - strlen (smileys[match].pattern);
		
		if (start > str) {
			len = start - str;
			gtk_text_buffer_insert (buf, iter, str, len);
		}
		
		pixbuf = gossip_chat_view_get_smiley_image (smileys[match].smiley);
		gtk_text_buffer_insert_pixbuf (buf, iter, pixbuf);
		
		gtk_text_buffer_insert (buf, iter, " ", 1);

		str = g_utf8_find_next_char (p, NULL);
	}
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

static void
chat_view_maybe_trim_buffer (GossipChatView *view)
{
	GossipChatViewPriv *priv;
	GtkTextIter         top, bottom;
	gint                line;
	gint                remove;
	GtkTextTagTable    *table;
	GtkTextTag         *tag;

	priv = GET_PRIV (view);

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

	priv = GET_PRIV (view);

	if (priv->irc_style) {
		tag = "irc-time";
	} else {
		tag = "fancy-time";
	}
	
	if (priv->last_block_type == BLOCK_TYPE_TIME) {
		return;
	}

	str = g_string_new (NULL);

	timestamp = 0;
	if (msg) {
		timestamp = gossip_message_get_timestamp (msg);
	}
	
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
			g_string_append (str, ", ");
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
	
	priv = GET_PRIV (view);

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

	priv = GET_PRIV (view);

	if (priv->irc_style) {
		link_tag = "irc-link";
	} else {
		link_tag = "fancy-link";
	}
	
	gtk_text_buffer_get_end_iter (priv->buffer, &start_iter);
	mark = gtk_text_buffer_create_mark (priv->buffer, NULL, &start_iter, TRUE);

	start = g_array_new (FALSE, FALSE, sizeof (gint));
	end = g_array_new (FALSE, FALSE, sizeof (gint));

	num_matches = gossip_regex_match (GOSSIP_REGEX_ALL, body, start, end);

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
				tmp = gossip_substring (body, last, s);

				gtk_text_buffer_get_end_iter (priv->buffer, &iter);
				chat_view_insert_text_with_emoticons (priv->buffer,
								      &iter,
								      tmp);
				g_free (tmp);
			}

			tmp = gossip_substring (body, s, e);

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
			tmp = gossip_substring (body, e, strlen (body));

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
				     gboolean        from_self,
				     GdkPixbuf      *avatar)
{
	GossipChatViewPriv *priv;
	GossipContact      *contact;
	const gchar        *name;
	gboolean            header;
	GtkTextIter         iter;
	gchar              *tmp;
	const gchar        *tag;
	const gchar        *avatar_tag;
	const gchar        *line_top_tag;
	const gchar        *line_bottom_tag;

	priv = GET_PRIV (view);

	contact = gossip_message_get_sender (msg);
	
	gossip_debug (DEBUG_DOMAIN, "Maybe add fancy header");

	if (from_self) {
		name = gossip_contact_get_name (my_contact);
		
		tag = "fancy-header-self";
		line_top_tag = "fancy-line-top-self";
		line_bottom_tag = "fancy-line-bottom-self";
	} else {
		name = gossip_contact_get_name (contact);
		
		tag = "fancy-header-other";
		line_top_tag = "fancy-line-top-other";
		line_bottom_tag = "fancy-line-bottom-other";
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
						  line_top_tag,
						  NULL);

	if (avatar) {
		GtkTextIter start;
		
		gtk_text_buffer_get_end_iter (priv->buffer, &iter);
 		gtk_text_buffer_insert_pixbuf (priv->buffer, &iter, avatar);

		gtk_text_buffer_get_end_iter (priv->buffer, &iter);
		start = iter;
		gtk_text_iter_backward_char (&start);

		if (from_self) {
			gtk_text_buffer_apply_tag_by_name (priv->buffer,
							   "fancy-avatar-self",
							   &start, &iter);
			avatar_tag = "fancy-header-self-avatar";
		} else {
			gtk_text_buffer_apply_tag_by_name (priv->buffer,
							   "fancy-avatar-other",
							   &start, &iter);
			avatar_tag = "fancy-header-other-avatar";
		}			
		
	} else {
		avatar_tag = NULL;
	}

	tmp = g_strdup_printf ("%s\n", name);

	gtk_text_buffer_get_end_iter (priv->buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
						  &iter,
						  tmp,
						  -1,
						  tag,
						  avatar_tag,
						  NULL);
	g_free (tmp);

	gtk_text_buffer_get_end_iter (priv->buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
						  &iter,
						  "\n",
						  -1,
						  line_bottom_tag,
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

	priv = GET_PRIV (view);

	gossip_debug (DEBUG_DOMAIN, "Add IRC action");

	/* Skip the "/me ". */
	if (from_self) {
		name = gossip_contact_get_name (my_contact);
		
		tag = "irc-action-self";
	} else {
		GossipContact *contact;

		contact = gossip_message_get_sender (msg);
		name = gossip_contact_get_name (contact);
		
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

	priv = GET_PRIV (view);

	gossip_debug (DEBUG_DOMAIN, "Add fancy action");

	contact = gossip_message_get_sender (msg);

	if (from_self) {
		name = gossip_contact_get_name (my_contact);
		
		tag = "fancy-action-self";
		line_tag = "fancy-line-self";
	} else {
		GossipContact *contact;

		contact = gossip_message_get_sender (msg);
		name = gossip_contact_get_name (contact);
		
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

	priv = GET_PRIV (view);

	gossip_debug (DEBUG_DOMAIN, "Add IRC message");

	body = gossip_message_get_body (msg);

	if (from_self) {
		name = gossip_contact_get_name (my_contact);
		
		nick_tag = "irc-nick-self";
		body_tag = "irc-body-self";
	} else {
		GossipContact *contact;

		contact = gossip_message_get_sender (msg);
		name = gossip_contact_get_name (contact);

		if (gossip_chat_should_highlight_nick (msg, my_contact)) {
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

	priv = GET_PRIV (view);
	
	if (from_self) {
		tag = "fancy-body-self";
	} else {
		tag = "fancy-body-other";

		/* FIXME: Might want to support nick highlighting here... */
	}
	
	body = gossip_message_get_body (msg);
	chat_view_append_text (view, body, tag);
}

static void
chat_view_invite_accept_cb (GtkWidget *button,
			    gpointer   user_data)
{
	GossipSession          *session;
	GossipAccount          *account;
	GossipContact          *contact;
	GossipChatroomProvider *provider;
	GossipChatroomInvite   *invite;
	GtkWidget              *other_button;
	const gchar            *nickname;
	const gchar            *reason;

	invite = g_object_get_data (G_OBJECT (button), "invite");
	other_button = g_object_get_data (G_OBJECT (button), "other_button");

	gtk_widget_set_sensitive (button, FALSE);
	gtk_widget_set_sensitive (other_button, FALSE);

	contact = gossip_chatroom_invite_get_invitor (invite);
	reason = gossip_chatroom_invite_get_reason (invite);

	session = gossip_app_get_session ();
	account = gossip_contact_get_account (contact);
	provider = gossip_session_get_chatroom_provider (session, account);
	nickname = gossip_session_get_nickname (session, account);

	gossip_chatroom_provider_invite_accept (provider,
						chat_view_invite_join_cb,
						invite,
						nickname);
}

static void
chat_view_invite_decline_cb (GtkWidget *button,
			     gpointer   user_data)
{
	GossipSession          *session;
	GossipAccount          *account;
	GossipContact          *contact;
	GossipChatroomProvider *provider;
	GossipChatroomInvite   *invite;
	GtkWidget              *other_button;
	const gchar            *reason;

	invite = g_object_get_data (G_OBJECT (button), "invite");
	other_button = g_object_get_data (G_OBJECT (button), "other_button");

	gtk_widget_set_sensitive (button, FALSE);
	gtk_widget_set_sensitive (other_button, FALSE);

	contact = gossip_chatroom_invite_get_invitor (invite);

	session = gossip_app_get_session ();
	account = gossip_contact_get_account (contact);
	provider = gossip_session_get_chatroom_provider (session, account);

	reason = _("Your invitation has been declined");
	gossip_chatroom_provider_invite_decline (provider, invite, reason);
}

static void
chat_view_invite_join_cb (GossipChatroomProvider   *provider,
			  GossipChatroomJoinResult  result,
			  gint                      id,
			  gpointer                  user_data)
{
	gossip_group_chat_new (provider, id);
}

static void
chat_view_theme_changed_cb (GossipThemeManager *manager,
			    GossipChatView     *view)
{
	GossipChatViewPriv *priv;
	
	priv = GET_PRIV (view);
	
	priv->last_block_type = BLOCK_TYPE_NONE;
	
	gossip_theme_manager_apply (manager, view);

	/* Needed for now to update the "rise" property of the names to get it
	 * vertically centered.
	 */
	gossip_theme_manager_update_show_avatars (manager,
						  view,
						  gconf_client_get_bool (
							  gossip_app_get_gconf_client (),
							  GCONF_UI_SHOW_AVATARS,
							  NULL));
}

GossipChatView *
gossip_chat_view_new (void)
{
	return g_object_new (GOSSIP_TYPE_CHAT_VIEW, NULL);
}

/* The name is optional, if NULL, the sender for msg is used. */
void
gossip_chat_view_append_message_from_self (GossipChatView *view,
					   GossipMessage  *msg,
					   GossipContact  *my_contact,
					   GdkPixbuf      *avatar)
{
	GossipChatViewPriv *priv;
	const gchar        *body;
	gboolean            scroll_down;

	priv = GET_PRIV (view);

	body = gossip_message_get_body (msg);
	if (!body) {
		return;
	}

	scroll_down = chat_view_is_scrolled_down (view);

	chat_view_maybe_trim_buffer (view);
	chat_view_maybe_append_date_and_time (view, msg);

	if (!priv->irc_style) {
		chat_view_maybe_append_fancy_header (view, msg,
						     my_contact,
						     TRUE, avatar);
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
					    GossipContact  *contact,
					    GdkPixbuf      *avatar)
{
	GossipChatViewPriv *priv;
	const gchar        *body;
	gboolean            scroll_down;

	priv = GET_PRIV (view);

	body = gossip_message_get_body (msg);
	if (!body) {
		return;
	}

	scroll_down = chat_view_is_scrolled_down (view);

	chat_view_maybe_trim_buffer (view);
	chat_view_maybe_append_date_and_time (view, msg);

	if (!priv->irc_style) {
		chat_view_maybe_append_fancy_header (view, msg,
						     contact, FALSE,
						     avatar);
	}
	
	/* Handle action messages (/me) and normal messages, in combination with
	 * irc style and fancy style.
	 */
	if (g_str_has_prefix (body, "/me ")) {
		if (priv->irc_style) {
			chat_view_append_irc_action (view, msg, contact, FALSE);
		} else {
			chat_view_append_fancy_action (view, msg, contact, FALSE);
		}
	} else {
		if (priv->irc_style) {
			chat_view_append_irc_message (view, msg, contact, FALSE);
		} else {
			chat_view_append_fancy_message (view, msg, contact, FALSE);
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
	gchar              *msg;
	const gchar        *tag;

	priv = GET_PRIV (view);
	
	bottom = chat_view_is_scrolled_down (view);

	chat_view_maybe_trim_buffer (view);

	if (priv->irc_style) {
		tag = "irc-event";
		msg = g_strdup_printf (" - %s\n", str);
	} else {
		tag = "fancy-event";
		msg = g_strdup_printf (" - %s\n", str);
	}
	
	if (priv->last_block_type != BLOCK_TYPE_EVENT) {
		//chat_view_append_spacing (view);
	}

	chat_view_maybe_append_date_and_time (view, NULL);

	gtk_text_buffer_get_end_iter (priv->buffer, &iter);

	gtk_text_buffer_insert_with_tags_by_name (priv->buffer, &iter,
						  msg, -1,
						  tag,
						  NULL);
	g_free (msg);

	if (bottom) {
		gossip_chat_view_scroll_down (view);
	}

	priv->last_block_type = BLOCK_TYPE_EVENT;
}

void
gossip_chat_view_append_invite (GossipChatView *view,
				GossipMessage  *message)
{
	GossipChatViewPriv   *priv;
	GossipContact        *sender;
	GossipChatroomInvite *invite;
	const gchar          *body;
	GtkTextChildAnchor   *anchor;
	GtkTextIter           iter;
	GtkWidget            *button_accept;
	GtkWidget            *button_decline;
	const gchar          *id;
	const gchar          *reason;
	gboolean              bottom;
	const gchar          *tag;
	GString              *s;

	priv = GET_PRIV (view);

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

	reason = gossip_chatroom_invite_get_reason (invite);

	s = g_string_new ("");
	if (body && strlen (body) > 0) {
		s = g_string_append (s, body);
	}

	/* Make sure the reason is not the body (or in the body) */
	if (reason && strlen (reason) > 0 && !strstr (body, reason)) {
		if (s->len > 0) {
			s = g_string_append_c (s, '\n');
		}

		s = g_string_append (s, reason);
	}
	
	if (s->len < 1) {
		s = g_string_append 
			(s, _("You have been invited to join a chat conference."));
	}

	/* Don't include the invite in the chat window if it is part of the
	 * actual request - some chat clients send this and it looks weird
	 * repeated.
	 */
	id = gossip_chatroom_invite_get_id (invite);

	if (!strstr (s->str, id)) {
		g_string_append_printf (s, "\n(%s)\n", id);
	}

	s = g_string_prepend_c (s, '\n');
	
	chat_view_append_text (view, s->str, tag);
	g_string_free (s, TRUE);

	gtk_text_buffer_get_end_iter (priv->buffer, &iter);

	anchor = gtk_text_buffer_create_child_anchor (priv->buffer, &iter);

	button_accept = gtk_button_new_with_label (_("Accept"));
	gtk_text_view_add_child_at_anchor (GTK_TEXT_VIEW (view),
					   button_accept,
					   anchor);

	gtk_widget_show (button_accept);

	gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
						  &iter,
						  " ",
						  1,
						  tag,
						  NULL);

	anchor = gtk_text_buffer_create_child_anchor (priv->buffer, &iter);

	button_decline = gtk_button_new_with_label (_("Decline"));
	gtk_text_view_add_child_at_anchor (GTK_TEXT_VIEW (view),
					   button_decline,
					   anchor);

	gtk_widget_show (button_decline);

	/* Set up data */
	g_object_set_data_full (G_OBJECT (button_accept), "invite",
				gossip_chatroom_invite_ref (invite), 
				(GDestroyNotify) gossip_chatroom_invite_unref);
 	g_object_set_data_full (G_OBJECT (button_accept), "other_button",
 				g_object_ref (button_decline), 
				g_object_unref);

	g_signal_connect (button_accept, "clicked",
			  G_CALLBACK (chat_view_invite_accept_cb),
			  NULL);

	g_object_set_data_full (G_OBJECT (button_decline), "invite",
				gossip_chatroom_invite_ref (invite), 
				(GDestroyNotify) gossip_chatroom_invite_unref);
 	g_object_set_data_full (G_OBJECT (button_decline), "other_button",
 				g_object_ref (button_accept), 
				g_object_unref);

	g_signal_connect (button_decline, "clicked",
			  G_CALLBACK (chat_view_invite_decline_cb),
			  NULL);

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

void
gossip_chat_view_scroll (GossipChatView *view,
			 gboolean        allow_scrolling)
{
	GossipChatViewPriv *priv;

	priv = GET_PRIV (view);

	priv->allow_scrolling = allow_scrolling;

	gossip_debug (DEBUG_DOMAIN, "Scrolling %s",
		      allow_scrolling ? "enabled" : "disabled");
}

void
gossip_chat_view_scroll_down (GossipChatView *view)
{
	GossipChatViewPriv *priv;
	GtkTextBuffer      *buffer;
	GtkTextIter         iter;
	GtkTextMark        *mark;

	priv = GET_PRIV (view);

	if (!priv->allow_scrolling) {
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "Scrolling down");

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
gossip_chat_view_clear (GossipChatView *view)
{
	GtkTextBuffer      *buffer;
	GossipChatViewPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	gtk_text_buffer_set_text (buffer, "", -1);

	/* We set these back to the initial values so we get
	 * timestamps when clearing the window to know when
	 * conversations start.
	 */
	priv = GET_PRIV (view);
	
	priv->last_block_type = BLOCK_TYPE_NONE;
	priv->last_timestamp = 0;
}

void
gossip_chat_view_find (GossipChatView *view, 
		       const gchar    *search_criteria,
		       gboolean        new_search)
{
	GtkTextBuffer      *buffer;
	GtkTextIter         iter_at_mark;
	GtkTextIter         iter_match_start;
	GtkTextIter         iter_match_end;
	static GtkTextMark *find_mark = NULL;
	static gboolean     wrapped = FALSE;
	gboolean            found;
	gboolean            from_start = FALSE;

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));
	g_return_if_fail (search_criteria != NULL);
	g_return_if_fail (strlen (search_criteria) > 0);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
    
	if (new_search) {
		find_mark = NULL;
		from_start = TRUE;
	}
     
	if (find_mark) {
		gtk_text_buffer_get_iter_at_mark (buffer, &iter_at_mark, find_mark);
		gtk_text_buffer_delete_mark (buffer, find_mark); 
		find_mark = NULL;
	} else {
		gtk_text_buffer_get_start_iter (buffer, &iter_at_mark);
		from_start = TRUE;
	}

	found = gossip_text_iter_forward_search (&iter_at_mark,
						 search_criteria,
						 &iter_match_start, 
						 &iter_match_end,
						 NULL);
    
	if (!found) {
		if (from_start) {
			return;
		}
	
		/* Here we wrap around. */
		if (!new_search && !wrapped) {
			wrapped = TRUE;
			gossip_chat_view_find (view, search_criteria, FALSE);
			wrapped = FALSE;
		}

		return;
	}
    
	/* Set new mark and show on screen */
	find_mark = gtk_text_buffer_create_mark (buffer, NULL, &iter_match_end, TRUE); 
	gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
				      find_mark,
				      0.0,
				      TRUE,
				      0.5,
				      0.5);

	gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &iter_match_start);
	gtk_text_buffer_move_mark_by_name (buffer, "insert", &iter_match_end);	
}

void
gossip_chat_view_highlight (GossipChatView *view, 
			    const gchar    *text)
{
	GtkTextBuffer *buffer;
	GtkTextIter    iter;
	GtkTextIter    iter_start;
	GtkTextIter    iter_end;
	GtkTextIter    iter_match_start;
	GtkTextIter    iter_match_end;
	gboolean       found;

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
    
	gtk_text_buffer_get_start_iter (buffer, &iter);

	gtk_text_buffer_get_bounds (buffer, &iter_start, &iter_end);
	gtk_text_buffer_remove_tag_by_name (buffer, "highlight", 
					    &iter_start, 
					    &iter_end);

	if (!text || strlen (text) < 1) {
		return;
	}

	while (TRUE) {
		found = gossip_text_iter_forward_search (&iter, 
							 text, 
							 &iter_match_start, 
							 &iter_match_end,
							 NULL);

		if (!found) {
			break;
		}

		gtk_text_buffer_apply_tag_by_name (buffer, "highlight", 
						   &iter_match_start, 
						   &iter_match_end);

		iter = iter_match_end;
		gtk_text_iter_forward_char (&iter);
	}
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

gboolean
gossip_chat_view_get_irc_style (GossipChatView *view)
{
	GossipChatViewPriv *priv;
	
	g_return_val_if_fail (GOSSIP_IS_CHAT_VIEW (view), FALSE);

	priv = GET_PRIV (view);

	return priv->irc_style;
}

void
gossip_chat_view_set_irc_style (GossipChatView *view,
				gboolean        irc_style)
{
	GossipChatViewPriv *priv;
	
	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	priv = GET_PRIV (view);

	priv->irc_style = irc_style;
}

void
gossip_chat_view_set_margin (GossipChatView *view,
			     gint            margin)
{
	GossipChatViewPriv *priv;
	
	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	priv = GET_PRIV (view);

	g_object_set (view,
		      "left-margin", margin,
		      "right-margin", margin,
		      NULL);
}

GdkPixbuf *
gossip_chat_view_get_smiley_image (GossipSmiley smiley)
{
	static GdkPixbuf *pixbufs[GOSSIP_SMILEY_COUNT];
	static gboolean   inited = FALSE;

	if (!inited) {
		gint i;

		for (i = 0; i < GOSSIP_SMILEY_COUNT; i++) {
			pixbufs[i] = gossip_pixbuf_from_smiley (i, GTK_ICON_SIZE_MENU);
		}

		inited = TRUE;
	}

	return pixbufs[smiley];
}

const gchar *
gossip_chat_view_get_smiley_text (GossipSmiley smiley)
{
	gint i;
	
	for (i = 0; i < G_N_ELEMENTS (smileys); i++) {
		if (smileys[i].smiley != smiley) {
			continue;
		}

		return smileys[i].pattern;
	}
	
	return NULL;
}

GtkWidget *
gossip_chat_view_get_smiley_menu (GCallback    callback, 
				  gpointer     user_data,
				  GtkTooltips *tooltips)
{
	GtkWidget *menu;
	gint       x;
	gint       y;
	gint       i;

	menu = gtk_menu_new ();

	for (i = 0, x = 0, y = 0; i < GOSSIP_SMILEY_COUNT; i++) {
		GtkWidget   *item;
		GtkWidget   *image;
		GdkPixbuf   *pixbuf;
		const gchar *smiley_text;

		pixbuf = gossip_chat_view_get_smiley_image (i);
		if (!pixbuf) {
			continue;
		}

		image = gtk_image_new_from_pixbuf (pixbuf);

		item = gtk_image_menu_item_new_with_label ("");
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

		gtk_menu_attach (GTK_MENU (menu), item,
				 x, x + 1, y, y + 1);

		smiley_text = gossip_chat_view_get_smiley_text (i);

		gtk_tooltips_set_tip (tooltips, 
				      item, 
				      smiley_text, 
				      NULL);

		g_object_set_data  (G_OBJECT (item), "smiley_text", (gpointer) smiley_text);
		g_signal_connect (item, "activate", callback, user_data);
		
		if (x > 3) {
			y++;
			x = 0;
		} else {
			x++;
		}
	}

	gtk_widget_show_all (menu);

	return menu;
}

