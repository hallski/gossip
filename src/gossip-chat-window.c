/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Geert-Jan Van den Bogaerde <gvdbogaerde@pandora.be>
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

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <libgnome/gnome-i18n.h>
#include <gconf/gconf-client.h>
#include "gossip-app.h"
#include "gossip-contact-info.h"
#include "gossip-preferences.h"
#include "gossip-chat-window.h"
#include "gossip-utils.h"
#include "gossip-stock.h"
#include "gossip-sound.h"

static void gossip_chat_window_class_init     (GossipChatWindowClass *klass);
static void gossip_chat_window_init	      (GossipChatWindow      *window);
static void gossip_chat_window_finalize       (GObject		     *object);
static void gossip_chat_window_set_property   (GObject		     *object,
					       guint		      prop_id,
					       const GValue	     *value,
					       GParamSpec	     *pspec);
static void gossip_chat_window_get_property   (GObject		     *object,
					       guint		      prop_id,
					       GValue		     *value,
					       GParamSpec	     *spec);
static gchar * chat_window_get_nick	      (GossipChatWindow	     *window,
					       GossipChat	     *chat);
static GdkPixbuf * 
chat_window_get_status_pixbuf		      (GossipChatWindow	     *window,
					       GossipChat	     *chat);
static GdkPixbuf *
chat_window_get_pixbuf_from_stock	      (GossipChatWindow	     *window,
					       const gchar           *stock);
static void chat_window_get_iter	      (GossipChatWindow      *window,
					       GossipChat	     *chat,
					       GtkTreeIter	     *iter);
static GossipChat *
chat_window_get_next_chat		      (GossipChatWindow      *window,
					       GossipChat	     *chat);
static GossipChat *
chat_window_get_prev_chat		      (GossipChatWindow      *window,
					       GossipChat	     *chat);
static gboolean 
chat_window_has_toplevel_focus                (GossipChatWindow	     *window);
static void chat_window_update_status	      (GossipChatWindow	     *window,
					       GossipChat	     *chat);
static void chat_window_update_title	      (GossipChatWindow	     *window);
static void chat_window_update_menu	      (GossipChatWindow	     *window);
static void chat_window_info_activate_cb      (GtkWidget	     *menuitem,
					       GossipChatWindow	     *window);
static void chat_window_close_activate_cb     (GtkWidget	     *menuitem,
					       GossipChatWindow      *window);
static void chat_window_as_list_activate_cb   (GtkWidget	     *menuitem,
					       GossipChatWindow      *window);
static void chat_window_as_win_activate_cb    (GtkWidget	     *menuitem,
					       GossipChatWindow	     *window);
static void chat_window_prev_conv_activate_cb (GtkWidget	     *menuitem,
					       GossipChatWindow      *window);
static void chat_window_next_conv_activate_cb (GtkWidget	     *menuitem,
					       GossipChatWindow      *window);
static void chat_window_detach_activate_cb    (GtkWidget	     *menuitem,
					       GossipChatWindow      *window);
static gboolean chat_window_delete_event_cb   (GtkWidget	     *dialog,
					       GdkEvent		     *event,
					       GossipChatWindow	     *window);
static void chat_window_presence_changed_cb   (GossipChat	     *chat,
					       GossipChatWindow	     *window);
static void chat_window_composing_cb	      (GossipChat	     *chat,
					       gboolean		      is_composing,
					       GossipChatWindow	     *window);
static void chat_window_new_message_cb	      (GossipChat	     *chat,
					       GossipChatWindow	     *window);
static void chat_window_selection_changed_cb  (GtkTreeSelection	     *sel,
					       GossipChatWindow	     *window);
static void chat_window_focus_in_event_cb     (GtkWidget	     *widget,
					       GdkEvent		     *event,
					       GossipChatWindow      *window);

struct _GossipChatWindowPriv {
	GossipChatWindowLayout  layout;
	GList                  *chats;
	GList                  *chats_new_msg;
	GList                  *chats_composing;
	GossipChat             *current_chat;
	gboolean		new_msg;

	GtkWidget              *dialog;
	GtkWidget              *paned;
	GtkWidget              *notebook;
	GtkWidget	       *scrolledwin;
	GtkWidget              *treeview;
	
	GtkListStore           *store;
	GtkTreeSelection       *sel;

	GtkAccelGroup	       *accel_group;

	/* menu items */
	GtkWidget	       *m_conv_info;
	GtkWidget	       *m_conv_detach;
	GtkWidget	       *m_conv_close;
	GtkWidget	       *m_view_as_windows;
	GtkWidget	       *m_view_as_list;
	GtkWidget	       *m_go_next;
	GtkWidget	       *m_go_prev;
};

/* Properties */
enum {
	PROP_0,
	PROP_LAYOUT
};

/* Signals */
enum {
	LAYOUT_CHANGED,
	LAST_SIGNAL
};

/* Treeview columns */
enum {
	COL_CHAT,
	COL_STATUS_ICON,
	COL_NICK,
	NUM_COLUMNS
};

static gint chat_window_signals[LAST_SIGNAL] = { 0 };
static gpointer parent_class;

static GList *chat_windows = NULL;

extern GConfClient *gconf_client;

GType
gossip_chat_window_get_type (void)
{
        static GType type_id = 0;

        if (type_id == 0) {
                static const GTypeInfo type_info = {
                        sizeof (GossipChatWindowClass),
                        NULL,
                        NULL,
                        (GClassInitFunc) gossip_chat_window_class_init,
                        NULL,
                        NULL,
                        sizeof (GossipChatWindow),
                        0,
                        (GInstanceInitFunc) gossip_chat_window_init
                };

                type_id = g_type_register_static (G_TYPE_OBJECT,
                                                  "GossipChatWindow",
                                                  &type_info, 0);
        }

        return type_id;
}

static void
gossip_chat_window_class_init (GossipChatWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->set_property = gossip_chat_window_set_property;
	object_class->get_property = gossip_chat_window_get_property;
	object_class->finalize	   = gossip_chat_window_finalize;

	g_object_class_install_property (object_class,
					 PROP_LAYOUT,
					 g_param_spec_int ("layout",
							   "window layout",
							   "The layout style of the window",
							   0,
							   G_MAXINT,
							   GOSSIP_CHAT_WINDOW_LAYOUT_LIST,
							   G_PARAM_READWRITE));

	chat_window_signals[LAYOUT_CHANGED] =
		g_signal_new ("layout-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GossipChatWindowClass, layout_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_INT);
}

static void
gossip_chat_window_init (GossipChatWindow *window)
{
	GladeXML          *glade;
	GtkCellRenderer   *cell;
	GtkTreeViewColumn *col;

	window->priv = g_new0 (GossipChatWindowPriv, 1);

	glade = gossip_glade_get_file (GLADEDIR "/chat.glade",
				       "chat_window",
				       NULL,
				       "chat_window", &window->priv->dialog,
				       "chats_paned", &window->priv->paned,
				       "chats_scrolledwindow", &window->priv->scrolledwin,
				       "chats_treeview", &window->priv->treeview,
				       "menu_conv_info", &window->priv->m_conv_info,
				       "menu_conv_detach", &window->priv->m_conv_detach,
				       "menu_conv_close", &window->priv->m_conv_close,
				       "menu_view_as_windows", &window->priv->m_view_as_windows,
				       "menu_view_as_list", &window->priv->m_view_as_list,
				       "menu_go_next", &window->priv->m_go_next,
				       "menu_go_prev", &window->priv->m_go_prev,
				       NULL);

	window->priv->store = gtk_list_store_new (NUM_COLUMNS,
						  GOSSIP_TYPE_CHAT,
						  GDK_TYPE_PIXBUF,
						  G_TYPE_STRING);

	cell = gtk_cell_renderer_pixbuf_new ();
	col = gtk_tree_view_column_new_with_attributes ("", cell, "pixbuf", COL_STATUS_ICON, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (window->priv->treeview), col);

	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes ("", cell, "text", COL_NICK, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (window->priv->treeview), col);

	gtk_tree_view_set_model (GTK_TREE_VIEW (window->priv->treeview),
				 GTK_TREE_MODEL (window->priv->store));

	window->priv->sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->priv->treeview));

	window->priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->priv->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (window->priv->notebook), FALSE);

	gtk_paned_pack2 (GTK_PANED (window->priv->paned),
			 window->priv->notebook,
			 TRUE, TRUE);

	gtk_widget_show (window->priv->notebook);

	window->priv->accel_group = gtk_accel_group_new ();

	g_signal_connect (window->priv->m_conv_info,
			  "activate",
			  G_CALLBACK (chat_window_info_activate_cb),
			  window);
	g_signal_connect (window->priv->m_conv_detach,
			  "activate",
			  G_CALLBACK (chat_window_detach_activate_cb),
			  window);
	g_signal_connect (window->priv->m_conv_close,
			  "activate",
			  G_CALLBACK (chat_window_close_activate_cb),
			  window);
	g_signal_connect (window->priv->m_view_as_list,
			  "activate",
			  G_CALLBACK (chat_window_as_list_activate_cb),
			  window);
	g_signal_connect (window->priv->m_view_as_windows,
			  "activate",
			  G_CALLBACK (chat_window_as_win_activate_cb),
			  window);
	g_signal_connect (window->priv->m_go_prev,
			  "activate",
			  G_CALLBACK (chat_window_prev_conv_activate_cb),
			  window);
	g_signal_connect (window->priv->m_go_next,
			  "activate",
			  G_CALLBACK (chat_window_next_conv_activate_cb),
			  window);
	g_signal_connect (window->priv->dialog,
			  "delete_event",
			  G_CALLBACK (chat_window_delete_event_cb),
			  window);
	g_signal_connect (window->priv->sel,
			  "changed",
			  G_CALLBACK (chat_window_selection_changed_cb),
			  window);
	g_signal_connect (window->priv->dialog,
			  "focus_in_event",
			  G_CALLBACK (chat_window_focus_in_event_cb),
			  window);

	chat_windows = g_list_prepend (chat_windows, window);

	window->priv->chats = NULL;
	window->priv->chats_new_msg = NULL;
	window->priv->chats_composing = NULL;
	window->priv->current_chat = NULL;
}

static void
gossip_chat_window_finalize (GObject *object)
{
	GossipChatWindow *window = GOSSIP_CHAT_WINDOW (object);

	chat_windows = g_list_remove (chat_windows, window);
	gtk_widget_destroy (window->priv->dialog);
	g_object_unref (window->priv->accel_group);
	g_free (window->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gossip_chat_window_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	GossipChatWindow *window = GOSSIP_CHAT_WINDOW (object);

	switch (prop_id) {
	case PROP_LAYOUT:
		gossip_chat_window_set_layout (window, g_value_get_int (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gossip_chat_window_get_property (GObject    *object,
				 guint       prop_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
	GossipChatWindow *window = GOSSIP_CHAT_WINDOW (object);

	switch (prop_id) {
	case PROP_LAYOUT:
		g_value_set_int (value, window->priv->layout);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static gchar *
chat_window_get_nick (GossipChatWindow *window,
		      GossipChat *chat)
{
	GossipJID	*jid;
	gchar		*nick;

	g_object_get (chat, "jid", &jid, NULL);
	nick = g_strdup (gossip_roster_old_get_nick_from_jid (gossip_app_get_roster (), jid));
	if (!nick) {
		nick = gossip_jid_get_part_name (jid);
	}

	return nick;
}

static GdkPixbuf *
chat_window_get_status_pixbuf (GossipChatWindow *window,
			       GossipChat *chat)
{
	GdkPixbuf       *pixbuf;
	GossipShow	 show;
	gboolean	 offline;
	const gchar	*icon;

	if (g_list_find (window->priv->chats_new_msg, chat)) {
		pixbuf = chat_window_get_pixbuf_from_stock (window, GOSSIP_STOCK_MESSAGE);
	}
	else if (g_list_find (window->priv->chats_composing, chat)) {
		pixbuf = chat_window_get_pixbuf_from_stock (window, GOSSIP_STOCK_TYPING);
	}
	else {
		g_object_get (chat, 
			      "other_offline", &offline,
			      "other_show", &show,
			      NULL);

		if (offline) {
			icon = GOSSIP_STOCK_OFFLINE;
		} else {
			icon = gossip_get_icon_for_show_string (gossip_show_to_string (show));
		}
		
		pixbuf = chat_window_get_pixbuf_from_stock (window, icon);
	}

	return pixbuf;
}

static GdkPixbuf *
chat_window_get_pixbuf_from_stock (GossipChatWindow *window,
				   const gchar	    *stock)
{
	return gtk_widget_render_icon (window->priv->dialog,
				       stock,
				       GTK_ICON_SIZE_MENU,
				       NULL);
}

static void
chat_window_get_iter (GossipChatWindow *window,
		      GossipChat       *chat,
		      GtkTreeIter      *iter)
{
	GtkTreeRowReference *ref;
	GtkTreePath	    *path;

	ref = g_object_get_data (G_OBJECT (chat), "chat-window-tree-row-ref");
	path = gtk_tree_row_reference_get_path (ref);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (window->priv->store),
			         iter, path);
	gtk_tree_path_free (path);
}

static GossipChat *
chat_window_get_next_chat (GossipChatWindow *window,
			   GossipChat	    *chat)
{
	GossipChat  *next;
	GtkTreeIter  iter;
	gboolean     has_next;

	chat_window_get_iter (window, chat, &iter);

	has_next = gtk_tree_model_iter_next (GTK_TREE_MODEL (window->priv->store),
					     &iter);

	if (!has_next) {
		return NULL;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (window->priv->store), &iter,
			    COL_CHAT, &next, -1);

	return next;
}

static GossipChat *
chat_window_get_prev_chat (GossipChatWindow *window,
			   GossipChat	    *chat)
{
	GossipChat  *next;
	GtkTreePath *path;
	GtkTreeIter  iter;
	gboolean     has_prev;

	chat_window_get_iter (window, chat, &iter);
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (window->priv->store),
					&iter);


	has_prev = gtk_tree_path_prev (path);

	if (!has_prev) {
		return NULL;
	}

	gtk_tree_model_get_iter (GTK_TREE_MODEL (window->priv->store),
				 &iter, path);

	gtk_tree_model_get (GTK_TREE_MODEL (window->priv->store), &iter,
			    COL_CHAT, &next, -1);

	gtk_tree_path_free (path);

	return next;	
}

static gboolean
chat_window_has_toplevel_focus (GossipChatWindow *window)
{
	gboolean focus = FALSE;

	/* The has-toplevel-focus property is new in GTK 2.2 so if we don't find it, we
	 * pretend that the window doesn't have focus.
	 */
	if (g_object_class_find_property (G_OBJECT_GET_CLASS (window->priv->dialog),
					  "has-toplevel-focus")) {
		g_object_get (window->priv->dialog, "has-toplevel-focus", &focus, NULL);
	}

	return focus;
}

static void
chat_window_update_status (GossipChatWindow *window,
			   GossipChat	    *chat)
{
	GtkTreeIter	     iter;
	GdkPixbuf           *pixbuf;
	
	pixbuf = chat_window_get_status_pixbuf (window, chat);
	
	chat_window_get_iter (window, chat, &iter);
	gtk_list_store_set (window->priv->store, &iter,
			    COL_STATUS_ICON, pixbuf,
			    -1);
}

static void
chat_window_update_title (GossipChatWindow *window)
{
	gchar    *nick;
	gchar    *title;

	nick = chat_window_get_nick (window, window->priv->current_chat);

	if (nick && nick[0]) {
		/* Translators: This is for the title of the chat window. The
		 * first %s is an "* " that gets displayed if the chat window
		 * has new messages in it. (Please complain if this doens't work well
		 * in your locale.)
		 */
		title = g_strdup_printf (_("%sChat - %s"), window->priv->new_msg ? "*" : "", nick);
	} else {
		/* Translators: see comments for "%sChat - %s" */
		title = g_strdup_printf (_("%sChat"), window->priv->new_msg ? "*" : "");
	}

	g_free (nick);

	gtk_window_set_title (GTK_WINDOW (window->priv->dialog), title);
	g_free (title);
}

static void
chat_window_update_menu (GossipChatWindow *window)
{
	GossipChat *next;
	GossipChat *prev;
	gboolean    has_next;
	gboolean    has_prev;

	next = chat_window_get_next_chat (window, window->priv->current_chat);
	prev = chat_window_get_prev_chat (window, window->priv->current_chat);

	has_next = (next != NULL) ? TRUE : FALSE;
	has_prev = (prev != NULL) ? TRUE : FALSE;	

	gtk_widget_set_sensitive (window->priv->m_go_next, has_next);
	gtk_widget_set_sensitive (window->priv->m_go_prev, has_prev);
	gtk_widget_set_sensitive (window->priv->m_conv_detach, has_prev || has_next);
}

static void
chat_window_info_activate_cb (GtkWidget        *menuitem,
			      GossipChatWindow *window)
{
	GossipRosterOld *roster;
	GossipJID	*jid;
	const gchar	*name;

	g_object_get (window->priv->current_chat, "jid", &jid, NULL);
	roster = gossip_app_get_roster ();
	name = gossip_roster_old_get_nick_from_jid (roster, jid);
	if (name && name[0]) {
		gossip_contact_info_new (gossip_app_get (), jid, name);
	}
}

static void
chat_window_close_activate_cb (GtkWidget        *menuitem,
			       GossipChatWindow *window)
{
	g_return_if_fail (window->priv->current_chat != NULL);
	gossip_chat_window_remove_chat (window, window->priv->current_chat);
}

static void
chat_window_as_list_activate_cb (GtkWidget        *menuitem,
				 GossipChatWindow *window)
{
	window->priv->layout = GOSSIP_CHAT_WINDOW_LAYOUT_LIST;
	gtk_widget_show (window->priv->scrolledwin);
	g_signal_emit (window, chat_window_signals[LAYOUT_CHANGED],
		       0, GOSSIP_CHAT_WINDOW_LAYOUT_LIST);
}

static void
chat_window_as_win_activate_cb (GtkWidget        *menuitem,
			        GossipChatWindow *window)
{
	GList *l = g_list_copy (window->priv->chats);

	l = g_list_next (l);
	for (; l != NULL; l = g_list_next (l)) {
		GossipChatWindow *new_window;
		GossipChat       *chat;

		chat = l->data;
		new_window = gossip_chat_window_new ();
		
		gossip_chat_window_set_layout (new_window, GOSSIP_CHAT_WINDOW_LAYOUT_WINDOW);
		gossip_chat_window_remove_chat (window, chat);
		gossip_chat_window_add_chat (new_window, chat);	
	}

	g_list_free (l);

	window->priv->layout = GOSSIP_CHAT_WINDOW_LAYOUT_WINDOW;
	gtk_widget_hide (window->priv->scrolledwin);

	g_signal_emit (window, chat_window_signals[LAYOUT_CHANGED],
		       0, GOSSIP_CHAT_WINDOW_LAYOUT_WINDOW);
}

static void
chat_window_prev_conv_activate_cb (GtkWidget        *menuitem,
				   GossipChatWindow *window)
{
	GossipChat *chat;

	chat = chat_window_get_prev_chat (window, window->priv->current_chat);
	if (chat != NULL) {
		gossip_chat_window_switch_to_chat (window, chat);
	}
}

static void
chat_window_next_conv_activate_cb (GtkWidget        *menuitem,
				   GossipChatWindow *window)
{
	GossipChat *chat;

	chat = chat_window_get_next_chat (window, window->priv->current_chat);
	if (chat != NULL) {
		gossip_chat_window_switch_to_chat (window, chat);
	}
}

static void
chat_window_detach_activate_cb (GtkWidget        *menuitem,
				GossipChatWindow *window)
{
	GossipChatWindow *new_window;
	GossipChat       *chat;
	
	chat = window->priv->current_chat;
	new_window = gossip_chat_window_new ();

	gossip_chat_window_remove_chat (window, chat);
	gossip_chat_window_add_chat (new_window, chat);
}

static gboolean
chat_window_delete_event_cb (GtkWidget        *dialog,
			     GdkEvent         *event,
			     GossipChatWindow *window)
{
	GList *l = g_list_copy (window->priv->chats);

	for (; l != NULL; l = g_list_next (l)) {
		GossipChat *chat = l->data;
		gossip_chat_window_remove_chat (window, chat);
	}

	g_list_free (l);

	return TRUE;
}

static void
chat_window_presence_changed_cb (GossipChat       *chat,
				 GossipChatWindow *window)
{
	chat_window_update_status (window, chat);
}

static void
chat_window_composing_cb (GossipChat       *chat,
			  gboolean          is_composing,
			  GossipChatWindow *window)
{
	if (is_composing && !g_list_find (window->priv->chats_composing, chat)) {
		window->priv->chats_composing = g_list_prepend (window->priv->chats_composing, chat);
	} else {
		window->priv->chats_composing = g_list_remove (window->priv->chats_composing, chat);
	}

	chat_window_update_status (window, chat);
}

static void
chat_window_new_message_cb (GossipChat       *chat,
			    GossipChatWindow *window)
{
	if (!chat_window_has_toplevel_focus (window)) {
		window->priv->new_msg = TRUE;
		chat_window_update_title (window);
		gossip_sound_play (GOSSIP_SOUND_CHAT);
	}

	if (chat == window->priv->current_chat) {
		return;
	}

	if (!g_list_find (window->priv->chats_new_msg, chat)) {
		window->priv->chats_new_msg = g_list_prepend (window->priv->chats_new_msg, chat);
		chat_window_update_status (window, chat);
	}
}

static void
chat_window_selection_changed_cb (GtkTreeSelection *sel,
				  GossipChatWindow *window)
{
	GossipChat  *chat;
	gboolean     selected;
	GtkTreeIter  iter;
	gint	     page_num;

	selected = gtk_tree_selection_get_selected (sel, NULL, &iter);

	if (!selected) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (window->priv->store), &iter,
			    COL_CHAT, &chat, -1);

	window->priv->current_chat = chat;
	window->priv->chats_new_msg = g_list_remove (window->priv->chats_new_msg, chat);

	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window->priv->notebook),
					  gossip_chat_get_widget (chat));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook), page_num);

	chat_window_update_title (window);
	chat_window_update_menu (window);
	chat_window_update_status (window, chat);
}

static void
chat_window_focus_in_event_cb (GtkWidget        *widget,
			       GdkEvent         *event,
			       GossipChatWindow *window)
{
	window->priv->new_msg = FALSE;
	chat_window_update_title (window);
}

GossipChatWindow *
gossip_chat_window_new (void)
{
	GossipChatWindow *window;
	gboolean	  open_in_list;

	open_in_list = gconf_client_get_bool (gconf_client,
					      "/apps/gossip/conversation/open_in_list",
					      NULL);

	window = g_object_new (GOSSIP_TYPE_CHAT_WINDOW, NULL);

	if (open_in_list) {
		gossip_chat_window_set_layout (window, GOSSIP_CHAT_WINDOW_LAYOUT_LIST);
	} else {
		gossip_chat_window_set_layout (window, GOSSIP_CHAT_WINDOW_LAYOUT_WINDOW);
	}

	return window;
}

GossipChatWindow *
gossip_chat_window_get_default (void)
{
	GossipChatWindow *window;
	gboolean	  open_in_list;

	open_in_list = gconf_client_get_bool (gconf_client,
					      "/apps/gossip/conversation/open_in_list",
					      NULL);

	if (!open_in_list || chat_windows == NULL) {
		window = gossip_chat_window_new ();
	} else {
		window = chat_windows->data;
	}

	return window;
}

GtkWidget *
gossip_chat_window_get_dialog (GossipChatWindow *window)
{
	return window->priv->dialog;
}

void
gossip_chat_window_set_layout (GossipChatWindow       *window,
			       GossipChatWindowLayout  layout)
{
	switch (layout) {
	case GOSSIP_CHAT_WINDOW_LAYOUT_LIST:
		gtk_menu_item_activate (GTK_MENU_ITEM (window->priv->m_view_as_list));
		break;
	case GOSSIP_CHAT_WINDOW_LAYOUT_WINDOW:
		gtk_menu_item_activate (GTK_MENU_ITEM (window->priv->m_view_as_windows));
		break;
	default:
		g_assert_not_reached ();
	}
}

GossipChatWindowLayout
gossip_chat_window_get_layout (GossipChatWindow *window)
{
	return window->priv->layout;
}

void
gossip_chat_window_add_chat (GossipChatWindow *window,
			     GossipChat	      *chat)
{
	GtkTreeIter          iter;
	GtkTreePath         *path;
	GtkTreeRowReference *ref;
	gchar		    *nick;
	GdkPixbuf	    *pixbuf;

	g_object_set (chat, "window", window, NULL);

	if (window->priv->chats != NULL &&
            window->priv->layout == GOSSIP_CHAT_WINDOW_LAYOUT_WINDOW) {
		gossip_chat_window_set_layout (window, GOSSIP_CHAT_WINDOW_LAYOUT_LIST);
	}
	
	window->priv->chats = g_list_append (window->priv->chats, chat);

	nick = chat_window_get_nick (window, chat);
	pixbuf = chat_window_get_status_pixbuf (window, chat);

	gtk_list_store_append (window->priv->store, &iter);
	gtk_list_store_set (window->priv->store, &iter,
			    COL_CHAT, chat,
			    COL_STATUS_ICON, pixbuf,
			    COL_NICK, nick,
			    -1);

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (window->priv->store),
					&iter);
	ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (window->priv->store),
					  path);

	g_object_set_data_full (G_OBJECT (chat), "chat-window-tree-row-ref", ref,
				(GDestroyNotify) gtk_tree_row_reference_free);

	gtk_notebook_insert_page (GTK_NOTEBOOK (window->priv->notebook),
				  gossip_chat_get_widget (chat),
				  NULL, -1);

	gossip_chat_window_switch_to_chat (window, chat);

	g_signal_connect (chat, "presence_changed",
			  G_CALLBACK (chat_window_presence_changed_cb),
			  window);
	g_signal_connect (chat, "composing",
			  G_CALLBACK (chat_window_composing_cb),
			  window);
	g_signal_connect (chat, "new_message",
			  G_CALLBACK (chat_window_new_message_cb),
			  window);

	gtk_tree_path_free (path);
	g_free (nick);
}

void
gossip_chat_window_remove_chat (GossipChatWindow *window,
				GossipChat	 *chat)
{
	GtkTreeIter          iter;
	GtkTreePath         *path;
	GtkTreeRowReference *ref;
	gint		     page_num;
	GossipChat	    *next;

	ref = g_object_get_data (G_OBJECT (chat), "chat-window-tree-row-ref");

	g_return_if_fail (ref != NULL);

	path = gtk_tree_row_reference_get_path (ref);

	g_return_if_fail (path != NULL);

	gtk_tree_model_get_iter (GTK_TREE_MODEL (window->priv->store),
				 &iter, path);

	next = chat_window_get_next_chat (window, chat);
	if (next == NULL) {
		next = chat_window_get_prev_chat (window, chat);
	}

	gtk_list_store_remove (window->priv->store, &iter);

	g_object_set_data (G_OBJECT (chat), "chat-window-tree-row-ref", NULL);
	gtk_tree_path_free (path);

	g_object_set (chat, "window", NULL, NULL);

	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window->priv->notebook),
					  gossip_chat_get_widget (chat));
	gtk_notebook_remove_page (GTK_NOTEBOOK (window->priv->notebook),
				  page_num);

	g_signal_handlers_disconnect_by_func (chat,
					      G_CALLBACK (chat_window_presence_changed_cb),
					      window);
	g_signal_handlers_disconnect_by_func (chat,
					      G_CALLBACK (chat_window_composing_cb),
					      window);
	g_signal_handlers_disconnect_by_func (chat,
					      G_CALLBACK (chat_window_new_message_cb),
					      window);

	window->priv->chats = g_list_remove (window->priv->chats, chat);

	if (next != NULL) {
		gossip_chat_window_switch_to_chat (window, next);
	}

	if (window->priv->chats == NULL) {
		g_object_unref (window);
	}
}

void
gossip_chat_window_switch_to_chat (GossipChatWindow *window,
				   GossipChat	    *chat)
{
	GtkTreeRowReference *ref;
	GtkTreePath *path;

	ref = g_object_get_data (G_OBJECT (chat), "chat-window-tree-row-ref");
	path = gtk_tree_row_reference_get_path (ref);
	gtk_tree_selection_select_path (window->priv->sel, path);
	gtk_tree_path_free (path);
}
