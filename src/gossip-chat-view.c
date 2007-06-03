/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2007 Imendio AB
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

#include "config.h"

#include <sys/types.h>
#include <string.h>
#include <time.h>

#include <glib/gi18n.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtksizegroup.h>
#include <glade/glade.h>

#include <libgossip/gossip-utils.h>
#include <libgossip/gossip-account.h>
#include <libgossip/gossip-chatroom.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-chatroom-provider.h>
#include <libgossip/gossip-conf.h>

#include "gossip-app.h"
#include "gossip-preferences.h"
#include "gossip-smiley.h"
#include "gossip-theme-manager.h"
#include "gossip-text-iter.h"
#include "gossip-ui-utils.h"
#include "gossip-chat-view.h"

#define DEBUG_DOMAIN "ChatView"

#define MAX_LINES 800

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CHAT_VIEW, GossipChatViewPriv))

struct _GossipChatViewPriv {
	GtkTextBuffer *buffer;

	GossipTheme   *theme;
	gpointer       theme_context;

	time_t         last_timestamp;
	BlockType      last_block_type;
	gboolean       is_irc_style;

	gboolean       allow_scrolling;
	gboolean       is_group_chat;

	GtkTextMark   *find_mark_previous;
	GtkTextMark   *find_mark_next;
	gboolean       find_wrapped;
	gboolean       find_last_direction;

	/* This is for the group chat so we know if the "other" last contact
	 * changed, so we know whether to insert a header or not.
	 */
	GossipContact *last_contact;

	guint          notify_system_fonts_id;
	guint          notify_show_avatars_id;
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
static void     chat_view_system_font_update         (GossipChatView           *view);
static void     chat_view_notify_system_font_cb      (GossipConf               *conf,
						      const gchar              *key,
						      gpointer                  user_data);
static void     chat_view_notify_show_avatars_cb     (GossipConf               *conf,
						      const gchar              *key,
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
static void     chat_view_open_address_cb            (GtkMenuItem              *menuitem,
						      const gchar              *url);
static void     chat_view_copy_address_cb            (GtkMenuItem              *menuitem,
						      const gchar              *url);
static void     chat_view_clear_view_cb              (GtkMenuItem              *menuitem,
						      GossipChatView           *view);
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
	gboolean            show_avatars;

	priv = GET_PRIV (view);

	priv->buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	gossip_chat_view_set_last_block_type (view, BLOCK_TYPE_NONE);
	gossip_chat_view_set_last_timestamp (view, 0);

	priv->allow_scrolling = TRUE;

	priv->is_group_chat = FALSE;

	g_object_set (view,
		      "wrap-mode", GTK_WRAP_WORD_CHAR,
		      "editable", FALSE,
		      "cursor-visible", FALSE,
		      NULL);

	priv->notify_system_fonts_id =
		gossip_conf_notify_add (gossip_conf_get (),
					 "/desktop/gnome/interface/document_font_name",
					 chat_view_notify_system_font_cb,
					 view);
	chat_view_system_font_update (view);

	priv->notify_show_avatars_id =
		gossip_conf_notify_add (gossip_conf_get (),
					 GOSSIP_PREFS_UI_SHOW_AVATARS,
					 chat_view_notify_show_avatars_cb,
					 view);

	chat_view_setup_tags (view);

	gossip_theme_manager_apply_saved (gossip_theme_manager_get (), view);

	show_avatars = FALSE;
	gossip_conf_get_bool (gossip_conf_get (),
			       GOSSIP_PREFS_UI_SHOW_AVATARS,
			       &show_avatars);

	gossip_theme_manager_update_show_avatars (gossip_theme_manager_get (),
						  view, show_avatars);

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

	view = GOSSIP_CHAT_VIEW (object);
	priv = GET_PRIV (view);

	gossip_conf_notify_remove (gossip_conf_get (), priv->notify_system_fonts_id);
	gossip_conf_notify_remove (gossip_conf_get (), priv->notify_show_avatars_id);

	if (priv->last_contact) {
		g_object_unref (priv->last_contact);
	}

	if (priv->theme) {
		gossip_theme_context_free (priv->theme, priv->theme_context);
		g_object_unref (priv->theme);
	}

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

	return FALSE;
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
				    "invite",
				    NULL);

	/* FIXME: Move to the theme and come up with something that looks a bit
	 * nicer.
	 */
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
			  "motion-notify-event",
			  G_CALLBACK (chat_view_event_cb),
			  tag);
}

static void
chat_view_system_font_update (GossipChatView *view)
{
	PangoFontDescription *font_description = NULL;
	gchar                *font_name;

	if (gossip_conf_get_string (gossip_conf_get (),
				     "/desktop/gnome/interface/document_font_name",
				     &font_name) && font_name) {
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
chat_view_notify_system_font_cb (GossipConf  *conf,
				 const gchar *key,
				 gpointer     user_data)
{
	GossipChatView *view;
	gboolean        show_avatars = FALSE;

	view = user_data;

	chat_view_system_font_update (view);

	/* Ugly, again, to adjust the vertical position of the nick... Will fix
	 * this when reworking the theme manager so that view register
	 * themselves with it instead of the other way around.
	 */
	gossip_conf_get_bool (conf,
			       GOSSIP_PREFS_UI_SHOW_AVATARS,
			       &show_avatars);

	gossip_theme_manager_update_show_avatars (gossip_theme_manager_get (),
						  view, show_avatars);
}

static void
chat_view_notify_show_avatars_cb (GossipConf  *conf,
				  const gchar *key,
				  gpointer     user_data)
{
	GossipChatView     *view;
	GossipChatViewPriv *priv;
	gboolean            show_avatars = FALSE;

	view = user_data;
	priv = GET_PRIV (view);

	gossip_conf_get_bool (conf, key, &show_avatars);

	gossip_theme_manager_update_show_avatars (gossip_theme_manager_get (),
						  view, show_avatars);
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

	if (G_STR_EMPTY (str)) {
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

	if (gtk_text_iter_has_tag (&iter, tag)) {
		if (!hand) {
			hand = gdk_cursor_new (GDK_HAND2);
			beam = gdk_cursor_new (GDK_XTERM);
		}
		gdk_window_set_cursor (win, hand);
	} else {
		if (!beam) {
			beam = gdk_cursor_new (GDK_XTERM);
		}
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

			gossip_url_show (str);
			g_free (str);
		}
	}

	return FALSE;
}

static void
chat_view_open_address_cb (GtkMenuItem *menuitem, const gchar *url)
{
	gossip_url_show (url);
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
	GossipChatroom  *chatroom;
	GossipGroupChat *chat;

	chatroom = gossip_chatroom_provider_find_by_id (provider, id);
	chat = gossip_group_chat_new (provider, chatroom);
/* 	g_object_unref (chat); */
}

static void
chat_view_theme_changed_cb (GossipThemeManager *manager,
			    GossipChatView     *view)
{
	GossipChatViewPriv *priv;
	gboolean            show_avatars = FALSE;
	gboolean            theme_rooms = FALSE;

	priv = GET_PRIV (view);

	gossip_chat_view_set_last_block_type (view, BLOCK_TYPE_NONE);

	gossip_conf_get_bool (gossip_conf_get (),
			      GOSSIP_PREFS_CHAT_THEME_CHAT_ROOM,
			      &theme_rooms);
	if (!theme_rooms && priv->is_group_chat) {
		gossip_theme_manager_apply (manager, view, NULL);
	} else {
		gossip_theme_manager_apply_saved (manager, view);
	}

	/* Needed for now to update the "rise" property of the names to get it
	 * vertically centered.
	 */
	gossip_conf_get_bool (gossip_conf_get (),
			       GOSSIP_PREFS_UI_SHOW_AVATARS,
			       &show_avatars);
	gossip_theme_manager_update_show_avatars (manager, view, show_avatars);
}

GossipChatView *
gossip_chat_view_new (void)
{
	return g_object_new (GOSSIP_TYPE_CHAT_VIEW, NULL);
}

static void
chat_view_append_message (GossipChatView *view,
			  GossipMessage  *msg,
			  GossipContact  *contact,
			  GdkPixbuf      *avatar,
			  gboolean        from_self)
{
	GossipChatViewPriv *priv;
	gboolean            scroll_down;

	priv = GET_PRIV (view);

	if (!gossip_message_get_body (msg)) {
		return;
	}

	scroll_down = chat_view_is_scrolled_down (view);

	chat_view_maybe_trim_buffer (view);

	/* Handle action messages (/me) and normal messages, in combination with
	 * irc style and fancy style.
	 */
	if (gossip_message_is_action (msg)) {
		gossip_theme_append_action (priv->theme, priv->theme_context,
					    view, msg, from_self);
	} else {
		gossip_theme_append_message (priv->theme, priv->theme_context,
					     view, msg, from_self);
	}

	if (scroll_down) {
		gossip_chat_view_scroll_down (view);
	}
}

void
gossip_chat_view_append_message_from_self (GossipChatView *view,
					   GossipMessage  *msg,
					   GossipContact  *my_contact,
					   GdkPixbuf      *avatar)
{
	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));
	g_return_if_fail (GOSSIP_IS_MESSAGE (msg));
	g_return_if_fail (GOSSIP_IS_CONTACT (my_contact));
	
	chat_view_append_message (view, msg, my_contact, avatar, TRUE);
}

void
gossip_chat_view_append_message_from_other (GossipChatView *view,
					    GossipMessage  *msg,
					    GossipContact  *contact,
					    GdkPixbuf      *avatar)
{
	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));
	g_return_if_fail (GOSSIP_IS_MESSAGE (msg));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	chat_view_append_message (view, msg, contact, avatar, FALSE);
}

void
gossip_chat_view_append_event (GossipChatView *view,
			       const gchar    *str)
{
	GossipChatViewPriv *priv;
	gboolean            bottom;

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));
	g_return_if_fail (!G_STR_EMPTY (str));

	priv = GET_PRIV (view);

	bottom = chat_view_is_scrolled_down (view);

	chat_view_maybe_trim_buffer (view);

	gossip_theme_append_event (priv->theme, priv->theme_context,
				   view, str);

	if (bottom) {
		gossip_chat_view_scroll_down (view);
	}
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

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	priv = GET_PRIV (view);

	tag = "invite";

	bottom = chat_view_is_scrolled_down (view);

	sender = gossip_message_get_sender (message);
	invite = gossip_message_get_invite (message);
	body = gossip_message_get_body (message);

	gossip_theme_append_timestamp (priv->theme, priv->theme_context,
				       view, message, TRUE, TRUE);

	reason = gossip_chatroom_invite_get_reason (invite);

	s = g_string_new ("");
	if (!G_STR_EMPTY (body)) {
		s = g_string_append (s, body);
	}

	/* Make sure the reason is not the body (or in the body) */
	if (!G_STR_EMPTY (reason) && !strstr (body, reason)) {
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

	gossip_theme_append_text (priv->theme, priv->theme_context, 
				  view, s->str, tag);
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

	gossip_chat_view_set_last_block_type (view, BLOCK_TYPE_INVITE);
}

void
gossip_chat_view_append_button (GossipChatView *view,
				const gchar    *message,
				GtkWidget      *button1,
				GtkWidget      *button2)
{
	GossipChatViewPriv   *priv;
	GtkTextChildAnchor   *anchor;
	GtkTextIter           iter;
	gboolean              bottom;
	const gchar          *tag;

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));
	g_return_if_fail (button1 != NULL);

	priv = GET_PRIV (view);

	tag = "invite";

	bottom = chat_view_is_scrolled_down (view);

	gossip_theme_append_timestamp (priv->theme, priv->theme_context,
				       view, NULL,
				       TRUE, TRUE);

	if (message) {
		gossip_theme_append_text (priv->theme, priv->theme_context,
					  view, message, tag);
	}

	gtk_text_buffer_get_end_iter (priv->buffer, &iter);

	anchor = gtk_text_buffer_create_child_anchor (priv->buffer, &iter);
	gtk_text_view_add_child_at_anchor (GTK_TEXT_VIEW (view), button1, anchor);
	gtk_widget_show (button1);

	gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
						  &iter,
						  " ",
						  1,
						  tag,
						  NULL);

	if (button2) {
		gtk_text_buffer_get_end_iter (priv->buffer, &iter);
		
		anchor = gtk_text_buffer_create_child_anchor (priv->buffer, &iter);
		gtk_text_view_add_child_at_anchor (GTK_TEXT_VIEW (view), button2, anchor);
		gtk_widget_show (button2);
		
		gtk_text_buffer_insert_with_tags_by_name (priv->buffer,
							  &iter,
							  " ",
							  1,
							  tag,
							  NULL);
	}

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

	gossip_chat_view_set_last_block_type (view, BLOCK_TYPE_INVITE);
}

void
gossip_chat_view_scroll (GossipChatView *view,
			 gboolean        allow_scrolling)
{
	GossipChatViewPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

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

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

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

	gtk_text_buffer_delete_mark (buffer, mark);
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

	gossip_theme_view_cleared (priv->theme, priv->theme_context, view);

	gossip_chat_view_set_last_block_type (view, BLOCK_TYPE_NONE);
	gossip_chat_view_set_last_timestamp (view, 0);
}

gboolean
gossip_chat_view_find_previous (GossipChatView *view,
				const gchar    *search_criteria,
				gboolean        new_search)
{
	GossipChatViewPriv *priv;
	GtkTextBuffer      *buffer;
	GtkTextIter         iter_at_mark;
	GtkTextIter         iter_match_start;
	GtkTextIter         iter_match_end;
	gboolean            found;
	gboolean            from_start = FALSE;

	g_return_val_if_fail (GOSSIP_IS_CHAT_VIEW (view), FALSE);
	g_return_val_if_fail (search_criteria != NULL, FALSE);

	priv = GET_PRIV (view);

	buffer = priv->buffer;

	if (G_STR_EMPTY (search_criteria)) {
		if (priv->find_mark_previous) {
			gtk_text_buffer_get_start_iter (buffer, &iter_at_mark);

			gtk_text_buffer_move_mark (buffer,
						   priv->find_mark_previous,
						   &iter_at_mark);
			gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
						      priv->find_mark_previous,
						      0.0,
						      TRUE,
						      0.0,
						      0.0);
			gtk_text_buffer_select_range (buffer,
						      &iter_at_mark,
						      &iter_at_mark);
		}

		return FALSE;
	}

	if (new_search) {
		from_start = TRUE;
	}

	if (priv->find_mark_previous) {
		gtk_text_buffer_get_iter_at_mark (buffer,
						  &iter_at_mark,
						  priv->find_mark_previous);
	} else {
		gtk_text_buffer_get_end_iter (buffer, &iter_at_mark);
		from_start = TRUE;
	}

	priv->find_last_direction = FALSE;

	found = gossip_text_iter_backward_search (&iter_at_mark,
						  search_criteria,
						  &iter_match_start,
						  &iter_match_end,
						  NULL);

	if (!found) {
		gboolean result = FALSE;

		if (from_start) {
			return result;
		}

		/* Here we wrap around. */
		if (!new_search && !priv->find_wrapped) {
			priv->find_wrapped = TRUE;
			result = gossip_chat_view_find_previous (view, 
								 search_criteria, 
								 FALSE);
			priv->find_wrapped = FALSE;
		}

		return result;
	}

	/* Set new mark and show on screen */
	if (!priv->find_mark_previous) {
		priv->find_mark_previous = gtk_text_buffer_create_mark (buffer, NULL,
									&iter_match_start,
									TRUE);
	} else {
		gtk_text_buffer_move_mark (buffer,
					   priv->find_mark_previous,
					   &iter_match_start);
	}

	if (!priv->find_mark_next) {
		priv->find_mark_next = gtk_text_buffer_create_mark (buffer, NULL,
								    &iter_match_end,
								    TRUE);
	} else {
		gtk_text_buffer_move_mark (buffer,
					   priv->find_mark_next,
					   &iter_match_end);
	}

	gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
				      priv->find_mark_previous,
				      0.0,
				      TRUE,
				      0.5,
				      0.5);

	gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &iter_match_start);
	gtk_text_buffer_move_mark_by_name (buffer, "insert", &iter_match_end);

	return TRUE;
}

gboolean
gossip_chat_view_find_next (GossipChatView *view,
			    const gchar    *search_criteria,
			    gboolean        new_search)
{
	GossipChatViewPriv *priv;
	GtkTextBuffer      *buffer;
	GtkTextIter         iter_at_mark;
	GtkTextIter         iter_match_start;
	GtkTextIter         iter_match_end;
	gboolean            found;
	gboolean            from_start = FALSE;

	g_return_val_if_fail (GOSSIP_IS_CHAT_VIEW (view), FALSE);
	g_return_val_if_fail (search_criteria != NULL, FALSE);

	priv = GET_PRIV (view);

	buffer = priv->buffer;

	if (G_STR_EMPTY (search_criteria)) {
		if (priv->find_mark_next) {
			gtk_text_buffer_get_start_iter (buffer, &iter_at_mark);

			gtk_text_buffer_move_mark (buffer,
						   priv->find_mark_next,
						   &iter_at_mark);
			gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
						      priv->find_mark_next,
						      0.0,
						      TRUE,
						      0.0,
						      0.0);
			gtk_text_buffer_select_range (buffer,
						      &iter_at_mark,
						      &iter_at_mark);
		}

		return FALSE;
	}

	if (new_search) {
		from_start = TRUE;
	}

	if (priv->find_mark_next) {
		gtk_text_buffer_get_iter_at_mark (buffer,
						  &iter_at_mark,
						  priv->find_mark_next);
	} else {
		gtk_text_buffer_get_start_iter (buffer, &iter_at_mark);
		from_start = TRUE;
	}

	priv->find_last_direction = TRUE;

	found = gossip_text_iter_forward_search (&iter_at_mark,
						 search_criteria,
						 &iter_match_start,
						 &iter_match_end,
						 NULL);

	if (!found) {
		gboolean result = FALSE;

		if (from_start) {
			return result;
		}

		/* Here we wrap around. */
		if (!new_search && !priv->find_wrapped) {
			priv->find_wrapped = TRUE;
			result = gossip_chat_view_find_next (view, 
							     search_criteria, 
							     FALSE);
			priv->find_wrapped = FALSE;
		}

		return result;
	}

	/* Set new mark and show on screen */
	if (!priv->find_mark_next) {
		priv->find_mark_next = gtk_text_buffer_create_mark (buffer, NULL,
							       &iter_match_end,
							       TRUE);
	} else {
		gtk_text_buffer_move_mark (buffer,
					   priv->find_mark_next,
					   &iter_match_end);
	}

	if (!priv->find_mark_previous) {
		priv->find_mark_previous = gtk_text_buffer_create_mark (buffer, NULL,
									&iter_match_start,
									TRUE);
	} else {
		gtk_text_buffer_move_mark (buffer,
					   priv->find_mark_previous,
					   &iter_match_start);
	}

	gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
				      priv->find_mark_next,
				      0.0,
				      TRUE,
				      0.5,
				      0.5);

	gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &iter_match_start);
	gtk_text_buffer_move_mark_by_name (buffer, "insert", &iter_match_end);

	return TRUE;
}


void
gossip_chat_view_find_abilities (GossipChatView *view,
				 const gchar    *search_criteria,
				 gboolean       *can_do_previous,
				 gboolean       *can_do_next)
{
	GossipChatViewPriv *priv;
	GtkTextBuffer      *buffer;
	GtkTextIter         iter_at_mark;
	GtkTextIter         iter_match_start;
	GtkTextIter         iter_match_end;

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));
	g_return_if_fail (search_criteria != NULL);
	g_return_if_fail (can_do_previous != NULL && can_do_next != NULL);

	priv = GET_PRIV (view);

	buffer = priv->buffer;

	if (can_do_previous) {
		if (priv->find_mark_previous) {
			gtk_text_buffer_get_iter_at_mark (buffer,
							  &iter_at_mark,
							  priv->find_mark_previous);
		} else {
			gtk_text_buffer_get_start_iter (buffer, &iter_at_mark);
		}
		
		*can_do_previous = gossip_text_iter_backward_search (&iter_at_mark,
								     search_criteria,
								     &iter_match_start,
								     &iter_match_end,
								     NULL);
	}

	if (can_do_next) {
		if (priv->find_mark_next) {
			gtk_text_buffer_get_iter_at_mark (buffer,
							  &iter_at_mark,
							  priv->find_mark_next);
		} else {
			gtk_text_buffer_get_start_iter (buffer, &iter_at_mark);
		}
		
		*can_do_next = gossip_text_iter_forward_search (&iter_at_mark,
								search_criteria,
								&iter_match_start,
								&iter_match_end,
								NULL);
	}
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

	if (G_STR_EMPTY (text)) {
		return;
	}

	while (1) {
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

GossipTheme *
gossip_chat_view_get_theme (GossipChatView *view)
{
	GossipChatViewPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHAT_VIEW (view), NULL);

	priv = GET_PRIV (view);

	return priv->theme;
}

void
gossip_chat_view_set_theme (GossipChatView *view, GossipTheme *theme)
{
	GossipChatViewPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));
	g_return_if_fail (GOSSIP_IS_THEME (theme));

	priv = GET_PRIV (view);

	if (priv->theme) {
		gossip_theme_context_free (priv->theme, priv->theme_context);

		g_object_unref (priv->theme);
	}

	priv->theme = g_object_ref (theme);

	priv->theme_context = gossip_theme_setup_with_view (theme, view);

	/* FIXME: Possibly redraw the function and make it a property */
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

/* FIXME: Do we really need this? Better to do it internally only at setup time,
 * we will never change it on the fly.
 */
void
gossip_chat_view_set_is_group_chat (GossipChatView *view,
				    gboolean        is_group_chat)
{
	GossipChatViewPriv *priv;
	gboolean            theme_rooms = FALSE;

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	priv = GET_PRIV (view);

	priv->is_group_chat = is_group_chat;

	gossip_conf_get_bool (gossip_conf_get (),
			      GOSSIP_PREFS_CHAT_THEME_CHAT_ROOM,
			      &theme_rooms);

	if (!theme_rooms && is_group_chat) {
		gossip_theme_manager_apply (gossip_theme_manager_get (),
					    view,
					    NULL);
	} else {
		gossip_theme_manager_apply_saved (gossip_theme_manager_get (),
						  view);
	}
}

GossipContact *
gossip_chat_view_get_last_contact (GossipChatView *view)
{
	GossipChatViewPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHAT_VIEW (view), NULL);

	priv = GET_PRIV (view);

	return priv->last_contact;
}

void
gossip_chat_view_set_last_contact (GossipChatView *view, GossipContact *contact)
{
	GossipChatViewPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	priv = GET_PRIV (view);

	if (priv->last_contact) {
		g_object_unref (priv->last_contact);
		priv->last_contact = NULL;
	}

	if (contact) {
		priv->last_contact = g_object_ref (contact);
	}
}


BlockType
gossip_chat_view_get_last_block_type (GossipChatView *view)
{
	GossipChatViewPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHAT_VIEW (view), 0);

	priv = GET_PRIV (view);

	return priv->last_block_type;
}

void
gossip_chat_view_set_last_block_type (GossipChatView *view, 
				      BlockType       block_type)
{
	GossipChatViewPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	priv = GET_PRIV (view);

	priv->last_block_type = block_type;
}

time_t
gossip_chat_view_get_last_timestamp (GossipChatView *view)
{
	GossipChatViewPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHAT_VIEW (view), 0);

	priv = GET_PRIV (view);

	return priv->last_timestamp;
}

void
gossip_chat_view_set_last_timestamp (GossipChatView *view,
				     time_t          timestamp)
{
	GossipChatViewPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	priv = GET_PRIV (view);

	priv->last_timestamp = timestamp;
}

gboolean
gossip_chat_view_is_irc_style (GossipChatView *view)
{
	GossipChatViewPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHAT_VIEW (view), TRUE);

	priv = GET_PRIV (view);

	return priv->is_irc_style;
}

void
gossip_chat_view_set_is_irc_style (GossipChatView *view, gboolean is_irc_style)
{
	GossipChatViewPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT_VIEW (view));

	priv = GET_PRIV (view);

	priv->is_irc_style = is_irc_style;
}


