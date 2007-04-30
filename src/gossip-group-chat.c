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
 *
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libgossip/gossip-contact.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-protocol.h>
#include <libgossip/gossip-utils.h>
#include <libgossip/gossip-account.h>
#include <libgossip/gossip-log.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-message.h>

#include "gossip-app.h"
#include "gossip-cell-renderer-expander.h"
#include "gossip-cell-renderer-text.h"
#include "gossip-chat.h"
#include "gossip-chat-invite.h"
#include "gossip-chat-view.h"
#include "gossip-contact-list-iface.h"
#include "gossip-group-chat.h"
#include "gossip-private-chat.h"
#include "gossip-sound.h"
#include "gossip-stock.h"
#include "gossip-ui-utils.h"

#define DEBUG_DOMAIN "GroupChat"

#define IS_ENTER(v) (v == GDK_Return || v == GDK_ISO_Enter || v == GDK_KP_Enter)

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_GROUP_CHAT, GossipGroupChatPriv))

struct _GossipGroupChatPriv {
	GossipContact          *own_contact;

	GossipChatroomProvider *chatroom_provider;
	GossipChatroom         *chatroom;

	GtkWidget              *widget;
	GtkWidget              *hpaned;
	GtkWidget              *vbox_left;

	GtkWidget              *scrolled_window_chat;
	GtkWidget              *scrolled_window_input;
	GtkWidget              *scrolled_window_contacts;

	GtkWidget              *hbox_topic;
	GtkWidget              *label_topic;
	gchar                  *topic;

	GtkWidget              *treeview;

	gchar                  *name;

	GCompletion            *completion;

	GHashTable             *contacts;
	GList                  *private_chats;

	guint                   scroll_idle_id;

	gint                    contacts_width;
	gboolean                contacts_visible;
};

typedef struct {
	GossipGroupChat *chat;
	GossipContact   *contact;
	gboolean         found;
	GtkTreeIter      found_iter;
} ContactFindData;

typedef struct {
	GossipGroupChat *chat;
	GossipContact   *contact;
	GtkWidget       *entry;
} ChatInviteData;

typedef struct {
	GtkTreeIter        iter;
	GossipChatroomRole role;
	gboolean           found;
} FindRoleIterData;

static void            group_chat_contact_list_iface_init     (GossipContactListIfaceClass  *iface);
static void            group_chat_finalize                    (GObject                      *object);
static void            group_chat_retry_connection_clicked_cb (GtkWidget                    *button,
							       GossipGroupChat              *chat);
static void            group_chat_join                        (GossipGroupChat              *chat);
static void            group_chat_join_cb                     (GossipChatroomProvider       *provider,
							       GossipChatroomJoinResult      result,
							       GossipChatroomId              id,
							       gpointer                      user_data);
static void            group_chat_protocol_connected_cb       (GossipSession                *session,
							       GossipAccount                *account,
							       GossipProtocol               *protocol,
							       GossipGroupChat              *chat);
static void            group_chat_protocol_disconnected_cb    (GossipSession                *session,
							       GossipAccount                *account,
							       GossipProtocol               *protocol,
							       gint                          reason,
							       GossipGroupChat              *chat);
static gboolean        group_chat_key_press_event_cb          (GtkWidget                    *widget,
							       GdkEventKey                  *event,
							       GossipGroupChat              *chat);
static gboolean        group_chat_focus_in_event_cb           (GtkWidget                    *widget,
							       GdkEvent                     *event,
							       GossipGroupChat              *chat);
static void            group_chat_drag_data_received          (GtkWidget                    *widget,
							       GdkDragContext               *context,
							       int                           x,
							       int                           y,
							       GtkSelectionData             *selection,
							       guint                         info,
							       guint                         time,
							       GossipGroupChat              *chat);
static void            group_chat_row_activated_cb            (GtkTreeView                  *view,
							       GtkTreePath                  *path,
							       GtkTreeViewColumn            *col,
							       GossipGroupChat              *chat);
static void            group_chat_widget_destroy_cb           (GtkWidget                    *widget,
							       GossipGroupChat              *chat);
static gint            group_chat_contacts_sort_func          (GtkTreeModel                 *model,
							       GtkTreeIter                  *iter_a,
							       GtkTreeIter                  *iter_b,
							       gpointer                      user_data);
static void            group_chat_contacts_setup              (GossipGroupChat              *chat);
static gboolean        group_chat_contacts_find_foreach       (GtkTreeModel                 *model,
							       GtkTreePath                  *path,
							       GtkTreeIter                  *iter,
							       ContactFindData              *data);
static gboolean        group_chat_contacts_find               (GossipGroupChat              *chat,
							       GossipContact                *contact,
							       GtkTreeIter                  *iter);
static gint            group_chat_contacts_completion_func    (const gchar                  *s1,
							       const gchar                  *s2,
							       gsize                         n);
static void            group_chat_create_ui                   (GossipGroupChat              *chat);
static void            group_chat_new_message_cb              (GossipChatroomProvider       *provider,
							       gint                          id,
							       GossipMessage                *message,
							       GossipGroupChat              *chat);
static void            group_chat_new_event_cb                (GossipChatroomProvider       *provider,
							       gint                          id,
							       const gchar                  *event,
							       GossipGroupChat              *chat);
static void            group_chat_topic_changed_cb            (GossipChatroomProvider       *provider,
							       gint                          id,
							       GossipContact                *who,
							       const gchar                  *new_topic,
							       GossipGroupChat              *chat);
static void            group_chat_topic_entry_activate_cb     (GtkWidget                    *entry,
							       GtkDialog                    *dialog);
static void            group_chat_topic_response_cb           (GtkWidget                    *dialog,
							       gint                          response,
							       GossipGroupChat              *chat);
static void            group_chat_contact_joined_cb           (GossipChatroom               *chatroom,
							       GossipContact                *contact,
							       GossipGroupChat              *chat);
static void            group_chat_contact_left_cb             (GossipChatroom               *chatroom,
							       GossipContact                *contact,
							       GossipGroupChat              *chat);
static void            group_chat_contact_info_changed_cb     (GossipChatroom               *chatroom,
							       GossipContact                *contact,
							       GossipGroupChat              *chat);
static void            group_chat_contact_add                 (GossipGroupChat              *chat,
							       GossipContact                *contact);
static void            group_chat_contact_remove              (GossipGroupChat              *chat,
							       GossipContact                *contact);
static void            group_chat_contact_presence_updated_cb (GossipContact                *contact,
							       GParamSpec                   *param,
							       GossipGroupChat              *chat);
static void            group_chat_contact_updated_cb          (GossipContact                *contact,
							       GParamSpec                   *param,
							       GossipGroupChat              *chat);
static void            group_chat_private_chat_new            (GossipGroupChat              *chat,
							       GossipContact                *contact);
static void            group_chat_private_chat_removed        (GossipGroupChat              *chat,
							       GossipChat                   *private_chat);
static void            group_chat_private_chat_stop_foreach   (GossipChat                   *private_chat,
							       GossipGroupChat              *chat);
static void            group_chat_send                        (GossipGroupChat              *chat);
static gboolean        group_chat_get_nick_list_foreach       (GtkTreeModel                 *model,
							       GtkTreePath                  *path,
							       GtkTreeIter                  *iter,
							       GList                       **list);
static GList *         group_chat_get_nick_list               (GossipGroupChat              *chat);
static GtkWidget *     group_chat_get_widget                  (GossipChat                   *chat);
static const gchar *   group_chat_get_name                    (GossipChat                   *chat);
static gchar *         group_chat_get_tooltip                 (GossipChat                   *chat);
static GdkPixbuf *     group_chat_get_status_pixbuf           (GossipChat                   *chat);
static GossipContact * group_chat_get_own_contact             (GossipChat                   *chat);
static GossipChatroom *group_chat_get_chatroom                (GossipChat                   *chat);
static gboolean        group_chat_get_show_contacts           (GossipChat                   *chat);
static void            group_chat_set_show_contacts           (GossipChat                   *chat,
							       gboolean                      show);
static gboolean        group_chat_is_group_chat               (GossipChat                   *chat);
static gboolean        group_chat_is_connected                (GossipChat                   *chat);
static void            group_chat_get_role_iter               (GossipGroupChat              *chat,
							       GossipChatroomRole            role,
							       GtkTreeIter                  *iter);
static void            group_chat_cl_pixbuf_cell_data_func    (GtkTreeViewColumn            *tree_column,
							       GtkCellRenderer              *cell,
							       GtkTreeModel                 *model,
							       GtkTreeIter                  *iter,
							       GossipGroupChat              *chat);
static void            group_chat_cl_text_cell_data_func      (GtkTreeViewColumn            *tree_column,
							       GtkCellRenderer              *cell,
							       GtkTreeModel                 *model,
							       GtkTreeIter                  *iter,
							       GossipGroupChat              *chat);
static void            group_chat_cl_expander_cell_data_func  (GtkTreeViewColumn            *tree_column,
							       GtkCellRenderer              *cell,
							       GtkTreeModel                 *model,
							       GtkTreeIter                  *iter,
							       GossipGroupChat              *chat);
static void            group_chat_cl_set_background           (GossipGroupChat              *chat,
							       GtkCellRenderer              *cell,
							       gboolean                      is_header);

enum {
	COL_STATUS,
	COL_NAME,
	COL_CONTACT,
	COL_IS_HEADER,
	COL_HEADER_ROLE,
	NUMBER_OF_COLS
};

typedef enum {
	DND_DRAG_TYPE_CONTACT_ID,
} DndDragType;

static const GtkTargetEntry drag_types_dest[] = {
	{ "text/contact-id", GTK_TARGET_SAME_APP, DND_DRAG_TYPE_CONTACT_ID },
};

static GHashTable *group_chats = NULL;

G_DEFINE_TYPE_WITH_CODE (GossipGroupChat, gossip_group_chat,
			 GOSSIP_TYPE_CHAT,
			 G_IMPLEMENT_INTERFACE (GOSSIP_TYPE_CONTACT_LIST_IFACE,
						group_chat_contact_list_iface_init));

static void
gossip_group_chat_class_init (GossipGroupChatClass *klass)
{
	GObjectClass    *object_class;
	GossipChatClass *chat_class;

	object_class = G_OBJECT_CLASS (klass);
	chat_class = GOSSIP_CHAT_CLASS (klass);

	object_class->finalize        = group_chat_finalize;

	chat_class->get_name          = group_chat_get_name;
	chat_class->get_tooltip       = group_chat_get_tooltip;
	chat_class->get_status_pixbuf = group_chat_get_status_pixbuf;
	chat_class->get_contact       = NULL;
	chat_class->get_own_contact   = group_chat_get_own_contact;
	chat_class->get_chatroom      = group_chat_get_chatroom;
	chat_class->get_widget        = group_chat_get_widget;
	chat_class->get_show_contacts = group_chat_get_show_contacts;
	chat_class->set_show_contacts = group_chat_set_show_contacts;
	chat_class->is_group_chat     = group_chat_is_group_chat;
	chat_class->is_connected      = group_chat_is_connected;

	g_type_class_add_private (object_class, sizeof (GossipGroupChatPriv));
}

static void
gossip_group_chat_init (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GossipChatView      *chatview;

	priv = GET_PRIV (chat);

	priv->contacts_visible = TRUE;

	g_signal_connect_object (gossip_app_get_session (),
				 "protocol-connected",
				 G_CALLBACK (group_chat_protocol_connected_cb),
				 chat, 0);

	g_signal_connect_object (gossip_app_get_session (),
				 "protocol-disconnected",
				 G_CALLBACK (group_chat_protocol_disconnected_cb),
				 chat, 0);

	chatview = GOSSIP_CHAT_VIEW (GOSSIP_CHAT (chat)->view);
	gossip_chat_view_set_is_group_chat (chatview, TRUE);

	group_chat_create_ui (chat);
}

static void
group_chat_contact_list_iface_init (GossipContactListIfaceClass *iface)
{
	/* No functions for now */
}

static void
group_chat_finalize (GObject *object)
{
	GossipGroupChat     *chat;
	GossipGroupChatPriv *priv;
	GossipChatroomId     id;

	gossip_debug (DEBUG_DOMAIN, "Finalized:%p", object);

	chat = GOSSIP_GROUP_CHAT (object);
	priv = GET_PRIV (chat);
	
	/* Make absolutely sure we don't still have the chatroom ID in
	 * the hash table, this can happen when we have called
	 * g_object_unref (chat) too many times. 
	 *
	 * This shouldn't happen.
	 */
	id = gossip_chatroom_get_id (priv->chatroom);
	g_hash_table_steal (group_chats, GINT_TO_POINTER (id));

	if (priv->chatroom) {
		GossipChatroomStatus status;

		status = gossip_chatroom_get_status (priv->chatroom);
		if (status == GOSSIP_CHATROOM_STATUS_ACTIVE) {
			gossip_chatroom_provider_leave (priv->chatroom_provider,
							gossip_chatroom_get_id (priv->chatroom));
		} else if (status == GOSSIP_CHATROOM_STATUS_JOINING) {
			gossip_chatroom_provider_cancel (priv->chatroom_provider,
							 gossip_chatroom_get_id (priv->chatroom));
		}

		g_object_unref (priv->chatroom);
	}

	g_signal_handlers_disconnect_by_func (priv->chatroom_provider,
					      group_chat_new_message_cb, 
					      chat);
	g_signal_handlers_disconnect_by_func (priv->chatroom_provider,
					      group_chat_new_event_cb,
					      chat);
	g_signal_handlers_disconnect_by_func (priv->chatroom_provider,
					      group_chat_topic_changed_cb,
					      chat);
	g_signal_handlers_disconnect_by_func (priv->chatroom,
					      group_chat_contact_joined_cb,
					      chat);
	g_signal_handlers_disconnect_by_func (priv->chatroom,
					      group_chat_contact_left_cb,
					      chat);
	g_signal_handlers_disconnect_by_func (priv->chatroom,
					      group_chat_contact_info_changed_cb,
					      chat);
	
	g_object_unref (priv->chatroom_provider);

	g_free (priv->name);
	g_free (priv->topic);

	g_list_foreach (priv->private_chats,
			(GFunc) group_chat_private_chat_stop_foreach,
			chat);
	g_list_free (priv->private_chats);

	if (priv->own_contact) {
		g_object_unref (priv->own_contact);
	}

	if (priv->scroll_idle_id) {
		g_source_remove (priv->scroll_idle_id);
	}

	G_OBJECT_CLASS (gossip_group_chat_parent_class)->finalize (object);
}

static void
group_chat_retry_connection_clicked_cb (GtkWidget       *button,
					GossipGroupChat *chat)
{
	group_chat_join (chat);
	gtk_widget_set_sensitive (button, FALSE);
}

static void
group_chat_join (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;

	priv = GET_PRIV (chat);

	gossip_chatroom_provider_join (priv->chatroom_provider,
				       priv->chatroom,
				       (GossipChatroomJoinCb) group_chat_join_cb,
				       NULL);

	gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, 
				       _("Connecting..."));

	g_signal_emit_by_name (chat, "status-changed");
}

static void     
group_chat_join_cb (GossipChatroomProvider   *provider,
		    GossipChatroomJoinResult  result,
		    GossipChatroomId          id,
		    gpointer                  user_data)
{
	GossipGroupChatPriv *priv;
	GossipChat          *chat;
	GossipChatView      *chatview;
	GtkTreeView         *view;
	GtkTreeModel        *model;
	GtkTreeStore        *store;
	GtkWidget           *button;
	const gchar         *result_str;

	chat = g_hash_table_lookup (group_chats, GINT_TO_POINTER (id));
	if (!chat) {
		return;
	}

	priv = GET_PRIV (chat);

	result_str = gossip_chatroom_provider_join_result_as_str (result);

	gossip_debug (DEBUG_DOMAIN, 
		      "Join callback for id:%d, result:%d->'%s'", 
		      id, result, result_str);

	chatview = chat->view;
	g_signal_emit_by_name (chat, "status-changed");

	/* Check the result */
	switch (result) {
	case GOSSIP_CHATROOM_JOIN_OK:
	case GOSSIP_CHATROOM_JOIN_ALREADY_OPEN:
		break;

	case GOSSIP_CHATROOM_JOIN_NEED_PASSWORD:
	case GOSSIP_CHATROOM_JOIN_UNKNOWN_HOST:
		gossip_chat_view_append_event (chatview, result_str);
		return;

	case GOSSIP_CHATROOM_JOIN_NICK_IN_USE:
	case GOSSIP_CHATROOM_JOIN_TIMED_OUT:
	case GOSSIP_CHATROOM_JOIN_UNKNOWN_ERROR:
	case GOSSIP_CHATROOM_JOIN_CANCELED:
		/* FIXME: Need special case for nickname to put an
		 * entry in the chat view and to request a new nick.
		 */
		button = gtk_button_new_with_label (_("Retry connection"));
		g_signal_connect (button, "clicked", 
				  G_CALLBACK (group_chat_retry_connection_clicked_cb),
				  chat);

		gossip_chat_view_append_event (chatview, result_str);
		gossip_chat_view_append_button (chatview,
						NULL,
						button,
						NULL);

		return;
	}

	/* Clear previous roster */
	view = GTK_TREE_VIEW (priv->treeview);
	model = gtk_tree_view_get_model (view);
	store = GTK_TREE_STORE (model);
	gtk_tree_store_clear (store);
	
	/* Make widgets available */
	gtk_widget_set_sensitive (priv->hbox_topic, TRUE);
	gtk_widget_set_sensitive (priv->scrolled_window_contacts, TRUE);
	gtk_widget_set_sensitive (priv->scrolled_window_input, TRUE);

	gossip_chat_view_append_event (chatview, _("Connected"));

	gtk_widget_grab_focus (GOSSIP_CHAT (chat)->input_text_view);
}

static void
group_chat_protocol_connected_cb (GossipSession   *session,
				  GossipAccount   *account,
				  GossipProtocol  *protocol,
				  GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GossipAccount       *this_account;

	priv = GET_PRIV (chat);

	this_account = gossip_contact_get_account (priv->own_contact);
	if (!gossip_account_equal (this_account, account)) {
		return;
	}

	g_signal_emit_by_name (chat, "status-changed");

/* 	gtk_widget_set_sensitive (GOSSIP_CHAT (chat)->input_text_view, TRUE); */

/*	gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, _("Connected")); */

	/* FIXME: We should really attempt to re-join group chat here */
}

static void
group_chat_protocol_disconnected_cb (GossipSession   *session,
				     GossipAccount   *account,
				     GossipProtocol  *protocol,
				     gint             reason,
				     GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GossipAccount       *this_account;

	priv = GET_PRIV (chat);

	this_account = gossip_contact_get_account (priv->own_contact);
	if (!gossip_account_equal (this_account, account)) {
		return;
	}

	gtk_widget_set_sensitive (priv->hbox_topic, FALSE);
	gtk_widget_set_sensitive (priv->scrolled_window_contacts, FALSE);
	gtk_widget_set_sensitive (priv->scrolled_window_input, FALSE);

	/* i18n: Disconnected as in "was disconnected". */
	gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view,
				       _("Disconnected"));

	g_signal_emit_by_name (chat, "status-changed");
}

static gboolean
group_chat_key_press_event_cb (GtkWidget       *widget,
			       GdkEventKey     *event,
			       GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GtkAdjustment       *adj;
	gdouble              val;
	GtkTextBuffer       *buffer;
	GtkTextIter          start, current;
	gchar               *nick, *completed;
	gint                 len;
	GList               *list, *l, *completed_list;
	gboolean  is_start_of_buffer;

	priv = GET_PRIV (chat);

	/* Catch ctrl+up/down so we can traverse messages we sent */
	if ((event->state & GDK_CONTROL_MASK) && 
	    (event->keyval == GDK_Up || 
	     event->keyval == GDK_Down)) {
		GtkTextBuffer *buffer;
		const gchar   *str;

		buffer = gtk_text_view_get_buffer 
			(GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->input_text_view));

		if (event->keyval == GDK_Up) {
			str = gossip_chat_sent_message_get_next (GOSSIP_CHAT (chat));
		} else {
			str = gossip_chat_sent_message_get_last (GOSSIP_CHAT (chat));
		}

		gtk_text_buffer_set_text (buffer, str ? str : "", -1);

		return TRUE;    
	}

	/* Catch enter but not ctrl/shift-enter */
	if (IS_ENTER (event->keyval) && !(event->state & GDK_SHIFT_MASK)) {
		GossipChat   *parent;
		GtkTextView  *view;
		GtkIMContext *context;

		/* This is to make sure that kinput2 gets the enter. And if
		 * it's handled there we shouldn't send on it. This is because
		 * kinput2 uses Enter to commit letters. See:
		 * http://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=104299
		 */
		parent = GOSSIP_CHAT (chat);
		view = GTK_TEXT_VIEW (parent->input_text_view);
		context = view->im_context;

		if (gtk_im_context_filter_keypress (context, event)) {
			GTK_TEXT_VIEW (view)->need_im_reset = TRUE;
			return TRUE;
		}

		group_chat_send (chat);
		return TRUE;
	}

	if (IS_ENTER (event->keyval) && (event->state & GDK_SHIFT_MASK)) {
		/* Newline for shift-enter. */
		return FALSE;
	}
	else if ((event->state & GDK_CONTROL_MASK) != GDK_CONTROL_MASK &&
		 event->keyval == GDK_Page_Up) {
		adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->scrolled_window_chat));
		gtk_adjustment_set_value (adj, adj->value - adj->page_size);

		return TRUE;
	}
	else if ((event->state & GDK_CONTROL_MASK) != GDK_CONTROL_MASK &&
		 event->keyval == GDK_Page_Down) {
		adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->scrolled_window_chat));
		val = MIN (adj->value + adj->page_size, adj->upper - adj->page_size);
		gtk_adjustment_set_value (adj, val);

		return TRUE;
	}

	if ((event->state & GDK_CONTROL_MASK) != GDK_CONTROL_MASK &&
	    (event->state & GDK_SHIFT_MASK) != GDK_SHIFT_MASK &&
	    event->keyval == GDK_Tab) {
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->input_text_view));
		gtk_text_buffer_get_iter_at_mark (buffer, &current, gtk_text_buffer_get_insert (buffer));

		/* Get the start of the nick to complete. */
		gtk_text_buffer_get_iter_at_mark (buffer, &start, gtk_text_buffer_get_insert (buffer));
		gtk_text_iter_backward_word_start (&start);
		is_start_of_buffer = gtk_text_iter_is_start (&start);

		nick = gtk_text_buffer_get_text (buffer, &start, &current, FALSE);

		g_completion_clear_items (priv->completion);

		len = strlen (nick);

		list = group_chat_get_nick_list (chat);

		g_completion_add_items (priv->completion, list);

		completed_list = g_completion_complete (priv->completion,
							nick,
							&completed);

		g_free (nick);

		if (completed) {
			int       len;
			gchar    *text;

			gtk_text_buffer_delete (buffer, &start, &current);

			len = g_list_length (completed_list);

			if (len == 1) {
				/* If we only have one hit, use that text
				 * instead of the text in completed since the
				 * completed text will use the typed string
				 * which might be cased all wrong.
				 * Fixes #120876
				 * */
				text = (gchar *) completed_list->data;
			} else {
				text = completed;
			}

			gtk_text_buffer_insert_at_cursor (buffer, text, strlen (text));

			if (len == 1) {
				if (is_start_of_buffer) {
					gtk_text_buffer_insert_at_cursor (buffer, ", ", 2);
				}
			}

			g_free (completed);
		}

		g_completion_clear_items (priv->completion);

		for (l = list; l; l = l->next) {
			g_free (l->data);
		}

		g_list_free (list);

		return TRUE;
	}

	return FALSE;
}

static gboolean
group_chat_focus_in_event_cb (GtkWidget       *widget,
			      GdkEvent        *event,
			      GossipGroupChat *chat)
{
	gtk_widget_grab_focus (GOSSIP_CHAT (chat)->input_text_view);

	return TRUE;
}

static void
group_chat_drag_data_received (GtkWidget        *widget,
			       GdkDragContext   *context,
			       int               x,
			       int               y,
			       GtkSelectionData *selection,
			       guint             info,
			       guint             time,
			       GossipGroupChat  *chat)
{
	if (info == DND_DRAG_TYPE_CONTACT_ID) {
		GossipGroupChatPriv *priv;
		GossipContact       *contact;
		const gchar         *id;
		gchar               *str;

		priv = GET_PRIV (chat);
		
		id = (const gchar*) selection->data;
		
		contact = gossip_session_find_contact (gossip_app_get_session (), id);
		if (!contact) {
			gossip_debug (DEBUG_DOMAIN, 
				      "Drag data received, but no contact found by the id:'%s'", 
				      id);
			return;
		}
		
		gossip_debug (DEBUG_DOMAIN, 
			      "Drag data received, for contact:'%s'", 
			      id);
		
		/* Send event to chat window */
		str = g_strdup_printf (_("Invited %s to join this chat conference."),
				       gossip_contact_get_id (contact));
		gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, str);
		g_free (str);
		
		gossip_chat_invite_dialog_show (contact, gossip_chatroom_get_id (priv->chatroom));
		
		gtk_drag_finish (context, TRUE, FALSE, time);
	} else {
		gossip_debug (DEBUG_DOMAIN, "Received drag & drop from unknown source");
		gtk_drag_finish (context, FALSE, FALSE, time);
	}
}

static void
group_chat_row_activated_cb (GtkTreeView       *view,
			     GtkTreePath       *path,
			     GtkTreeViewColumn *col,
			     GossipGroupChat   *chat)
{
	GtkTreeModel  *model;
	GtkTreeIter    iter;
	GossipContact *contact;

	if (gtk_tree_path_get_depth (path) == 1) {
		/* Do nothing for role groups */
		return;
	}

	model = gtk_tree_view_get_model (view);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model,
			    &iter,
			    COL_CONTACT, &contact,
			    -1);

	group_chat_private_chat_new (chat, contact);

	g_object_unref (contact);
}

static void
group_chat_widget_destroy_cb (GtkWidget       *widget,
			      GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GossipChatroomId     id;

	gossip_debug (DEBUG_DOMAIN, "Destroyed, removing from hash table");

	priv = GET_PRIV (chat);
	id = gossip_chatroom_get_id (priv->chatroom);

	g_hash_table_remove (group_chats, GINT_TO_POINTER (id));
}

static gint
group_chat_contacts_sort_func (GtkTreeModel *model,
			       GtkTreeIter  *iter_a,
			       GtkTreeIter  *iter_b,
			       gpointer      user_data)
{
	gboolean is_header;

	gtk_tree_model_get (model, iter_a,
			    COL_IS_HEADER, &is_header,
			    -1);

	if (is_header) {
		GossipChatroomRole role_a, role_b;

		gtk_tree_model_get (model, iter_a,
				    COL_HEADER_ROLE, &role_a,
				    -1);
		gtk_tree_model_get (model, iter_b,
				    COL_HEADER_ROLE, &role_b,
				    -1);

		return role_a - role_b;
	} else {
		GossipContact *contact_a, *contact_b;
		const gchar   *name_a, *name_b;
		gint           ret_val;

		gtk_tree_model_get (model, iter_a,
				    COL_CONTACT, &contact_a,
				    -1);

		gtk_tree_model_get (model, iter_b,
				    COL_CONTACT, &contact_b,
				    -1);

		name_a = gossip_contact_get_name (contact_a);
		name_b = gossip_contact_get_name (contact_b);

		ret_val = g_ascii_strcasecmp (name_a, name_b);

		g_object_unref (contact_a);
		g_object_unref (contact_b);

		return ret_val;
	}
}

static void
group_chat_contacts_setup (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GtkTreeView         *tree;
	GtkTextView         *tv;
	GtkTreeStore        *store;
	GtkCellRenderer     *cell;
	GtkTreeViewColumn   *col;

	priv = GET_PRIV (chat);

	tree = GTK_TREE_VIEW (priv->treeview);
	tv = GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->view);

	g_object_set (tree,
		      "show-expanders", FALSE,
		      "reorderable", FALSE,
		      "headers-visible", FALSE,
		      NULL);

	store = gtk_tree_store_new (NUMBER_OF_COLS,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_STRING,
				    G_TYPE_OBJECT,
				    G_TYPE_BOOLEAN,
				    G_TYPE_INT);

	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store),
						 group_chat_contacts_sort_func,
						 chat,
						 NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					      GTK_SORT_ASCENDING);

	gtk_tree_view_set_model (tree, GTK_TREE_MODEL (store));

	col = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_sizing (col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);

	gtk_tree_view_column_set_cell_data_func (col, cell,
						 (GtkTreeCellDataFunc) group_chat_cl_pixbuf_cell_data_func,
						 chat, NULL);

	g_object_set (cell,
		      "xpad", 5,
		      "ypad", 1,
		      "visible", FALSE,
		      NULL);

	cell = gossip_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (col, cell,
						 (GtkTreeCellDataFunc) group_chat_cl_text_cell_data_func,
						 chat, NULL);
	gtk_tree_view_column_add_attribute (col, cell,
					    "name", COL_NAME);

	gtk_tree_view_column_add_attribute (col, cell,
					    "is_group", COL_IS_HEADER);

	cell = gossip_cell_renderer_expander_new ();
	gtk_tree_view_column_pack_end (col, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (
		col, cell,
		(GtkTreeCellDataFunc) group_chat_cl_expander_cell_data_func,
		chat, NULL);

	gtk_tree_view_append_column (tree, col);

	g_signal_connect (tree,
			  "row_activated",
			  G_CALLBACK (group_chat_row_activated_cb),
			  chat);
}

static gboolean
group_chat_contacts_find_foreach (GtkTreeModel    *model,
				  GtkTreePath     *path,
				  GtkTreeIter     *iter,
				  ContactFindData *data)
{
	GossipContact *contact;
	gboolean       equal;

	if (gtk_tree_path_get_depth (path) == 1) {
		/* No contacts on depth 1 */
		return FALSE;
	}

	gtk_tree_model_get (model,
			    iter,
			    COL_CONTACT, &contact,
			    -1);

	equal = gossip_contact_equal (data->contact, contact);
	g_object_unref (contact);

	if (equal) {
		data->found = TRUE;
		data->found_iter = *iter;
	}

	return equal;
}

static gboolean
group_chat_contacts_find (GossipGroupChat *chat,
			  GossipContact   *contact,
			  GtkTreeIter     *iter)
{
	GossipGroupChatPriv *priv;
	ContactFindData      data;
	GtkTreeModel        *model;

	priv = GET_PRIV (chat);

	data.found = FALSE;
	data.contact = contact;
	data.chat = chat;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) group_chat_contacts_find_foreach,
				&data);

	if (data.found) {
		*iter = data.found_iter;
	}

	return data.found;
}

static gint
group_chat_contacts_completion_func (const gchar *s1,
				    const gchar *s2,
				    gsize        n)
{
	gchar *tmp, *nick1, *nick2;
	gint   ret;

	tmp = g_utf8_normalize (s1, -1, G_NORMALIZE_DEFAULT);
	nick1 = g_utf8_casefold (tmp, -1);
	g_free (tmp);

	tmp = g_utf8_normalize (s2, -1, G_NORMALIZE_DEFAULT);
	nick2 = g_utf8_casefold (tmp, -1);
	g_free (tmp);

	ret = strncmp (nick1, nick2, n);

	g_free (nick1);
	g_free (nick2);

	return ret;
}

static void
group_chat_create_ui (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GladeXML            *glade;
 	GList               *list = NULL; 

	priv = GET_PRIV (chat);

	glade = gossip_glade_get_file ("group-chat.glade",
				       "group_chat_widget",
				       NULL,
				       "group_chat_widget", &priv->widget,
				       "hpaned", &priv->hpaned,
				       "vbox_left", &priv->vbox_left,
				       "scrolled_window_chat", &priv->scrolled_window_chat,
				       "scrolled_window_input", &priv->scrolled_window_input,
				       "hbox_topic", &priv->hbox_topic,
				       "label_topic", &priv->label_topic,
				       "scrolled_window_contacts", &priv->scrolled_window_contacts,
				       "treeview", &priv->treeview,
				       NULL);

	gossip_glade_connect (glade,
			      chat,
			      "group_chat_widget", "destroy", group_chat_widget_destroy_cb,
			      NULL);

	g_object_unref (glade);

	g_object_set_data (G_OBJECT (priv->widget), "chat", chat);

	/* Add room GtkTextView. */
	gtk_container_add (GTK_CONTAINER (priv->scrolled_window_chat),
			   GTK_WIDGET (GOSSIP_CHAT (chat)->view));
	gtk_widget_show (GTK_WIDGET (GOSSIP_CHAT (chat)->view));

	g_signal_connect (GOSSIP_CHAT (chat)->view,
			  "focus_in_event",
			  G_CALLBACK (group_chat_focus_in_event_cb),
			  chat);

	/* Add input GtkTextView */
	gtk_container_add (GTK_CONTAINER (priv->scrolled_window_input),
			   GOSSIP_CHAT (chat)->input_text_view);
	gtk_widget_show (GOSSIP_CHAT (chat)->input_text_view);

	g_signal_connect (GOSSIP_CHAT (chat)->input_text_view,
			  "key_press_event",
			  G_CALLBACK (group_chat_key_press_event_cb),
			  chat);

	/* Drag & drop */
	gtk_drag_dest_set (GTK_WIDGET (priv->treeview),
			   GTK_DEST_DEFAULT_ALL,
			   drag_types_dest,
			   G_N_ELEMENTS (drag_types_dest),
			   GDK_ACTION_MOVE);

	g_signal_connect (priv->treeview,
			  "drag-data-received",
			  G_CALLBACK (group_chat_drag_data_received),
			  chat);

	/* Add nick name completion */
	priv->completion = g_completion_new (NULL);
	g_completion_set_compare (priv->completion,
				  group_chat_contacts_completion_func);

	group_chat_contacts_setup (chat);

	/* Set widget focus order */
	list = g_list_append (NULL, priv->scrolled_window_input);
	gtk_container_set_focus_chain (GTK_CONTAINER (priv->vbox_left), list);
	g_list_free (list);

	list = g_list_append (NULL, priv->vbox_left);
	list = g_list_append (list, priv->scrolled_window_contacts);
	gtk_container_set_focus_chain (GTK_CONTAINER (priv->hpaned), list);
	g_list_free (list);

	list = g_list_append (NULL, priv->hpaned);
	list = g_list_append (list, priv->hbox_topic);
	gtk_container_set_focus_chain (GTK_CONTAINER (priv->widget), list);
	g_list_free (list);
}

static void
group_chat_new_message_cb (GossipChatroomProvider *provider,
			   gint                    id,
			   GossipMessage          *message,
			   GossipGroupChat        *chat)
{
	GossipGroupChatPriv  *priv;
	GossipChatroomInvite *invite;
	GossipLogManager     *log_manager;

	priv = GET_PRIV (chat);

	if (id != gossip_chatroom_get_id (priv->chatroom)) {
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "[%d] New message", id);

	invite = gossip_message_get_invite (message);
	if (invite) {
		gossip_chat_view_append_invite (GOSSIP_CHAT (chat)->view,
						message);
	} else {
		GossipContact *sender;

		sender = gossip_message_get_sender (message);
		if (gossip_contact_equal (sender, priv->own_contact)) {
			gossip_chat_view_append_message_from_self (
				GOSSIP_CHAT (chat)->view,
				message,
				priv->own_contact,
				NULL);
		} else {
			gossip_chat_view_append_message_from_other (
				GOSSIP_CHAT (chat)->view,
				message,
				priv->own_contact,
				NULL);
		}
	}

	if (gossip_chat_should_play_sound (GOSSIP_CHAT (chat)) &&
	    gossip_chat_should_highlight_nick (message, priv->own_contact)) {
		gossip_sound_play (GOSSIP_SOUND_CHAT);
	}

	log_manager = gossip_session_get_log_manager (gossip_app_get_session ());
	gossip_log_message_for_chatroom (log_manager, priv->chatroom, message, FALSE);

	g_signal_emit_by_name (chat, "new-message", message);
}

static void
group_chat_new_event_cb (GossipChatroomProvider *provider,
			 gint                    id,
			 const gchar            *event,
			 GossipGroupChat        *chat)
{
	GossipGroupChatPriv *priv;

	priv = GET_PRIV (chat);

	if (id != gossip_chatroom_get_id (priv->chatroom)) {
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "[%d] New event:'%s'", id, event);

	gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, event);
}

static void
group_chat_topic_changed_cb (GossipChatroomProvider *provider,
			     gint                    id,
			     GossipContact          *who,
			     const gchar            *new_topic,
			     GossipGroupChat        *chat)
{
	GossipGroupChatPriv *priv;
	gchar               *event;

	priv = GET_PRIV (chat);

	if (id != gossip_chatroom_get_id (priv->chatroom)) {
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "[%d] Topic changed by:'%s' to:'%s'",
		      id, gossip_contact_get_id (who), new_topic);

	g_free (priv->topic);
	priv->topic = g_strdup (new_topic);
	
	gtk_label_set_text (GTK_LABEL (priv->label_topic), new_topic);

	event = g_strdup_printf (_("%s has set the topic: %s"),
				 gossip_contact_get_name (who),
				 new_topic);
	gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, event);
	g_free (event);

	g_signal_emit_by_name (chat, "status-changed");
}

static void
group_chat_topic_entry_activate_cb (GtkWidget *entry,
				    GtkDialog *dialog)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static void
group_chat_topic_response_cb (GtkWidget       *dialog,
			      gint             response,			      
			      GossipGroupChat *chat)
{
	if (response == GTK_RESPONSE_OK) {
		GtkWidget   *entry;
		const gchar *topic;

		entry = g_object_get_data (G_OBJECT (dialog), "entry");
		topic = gtk_entry_get_text (GTK_ENTRY (entry));
		
		if (!G_STR_EMPTY (topic)) {
			GossipGroupChatPriv *priv;

			priv = GET_PRIV (chat);

			gossip_chatroom_provider_change_topic (priv->chatroom_provider,
							       gossip_chatroom_get_id (priv->chatroom),
							       topic);
		}
	}

	gtk_widget_destroy (dialog);
}

void
gossip_group_chat_set_topic (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GossipChatWindow    *chat_window;
	GtkWidget           *chat_dialog;
	GtkWidget           *dialog;
	GtkWidget           *entry;
	GtkWidget           *hbox;
	const gchar         *topic;


	priv = GET_PRIV (chat);

	chat_window = gossip_chat_get_window (GOSSIP_CHAT (chat));
	chat_dialog = gossip_chat_window_get_dialog (chat_window);

	dialog = gtk_message_dialog_new (GTK_WINDOW (chat_dialog),
					 0,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_OK_CANCEL,
					 _("Enter the new topic you want to set for this room:"));

	topic = gtk_label_get_text (GTK_LABEL (priv->label_topic));

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    hbox, FALSE, TRUE, 4);

	entry = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry), topic);
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
		    
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 4);

	g_object_set (GTK_MESSAGE_DIALOG (dialog)->label, "use-markup", TRUE, NULL);
	g_object_set_data (G_OBJECT (dialog), "entry", entry);

	g_signal_connect (entry, "activate",
			  G_CALLBACK (group_chat_topic_entry_activate_cb),
			  dialog);
	g_signal_connect (dialog, "response",
			  G_CALLBACK (group_chat_topic_response_cb),
			  chat);

	gtk_widget_show_all (dialog);
}

static void
group_chat_cl_pixbuf_cell_data_func (GtkTreeViewColumn *tree_column,
				     GtkCellRenderer   *cell,
				     GtkTreeModel      *model,
				     GtkTreeIter       *iter,
				     GossipGroupChat   *chat)
{
	GdkPixbuf *pixbuf;
	gboolean   is_header;

	gtk_tree_model_get (model, iter,
			    COL_IS_HEADER, &is_header,
			    COL_STATUS, &pixbuf,
			    -1);

	g_object_set (cell,
		      "visible", !is_header,
		      "pixbuf", pixbuf,
		      NULL);

	if (pixbuf) {
		g_object_unref (pixbuf);
	}

	group_chat_cl_set_background (chat, cell, is_header);
}

static void
group_chat_cl_text_cell_data_func (GtkTreeViewColumn *tree_column,
				   GtkCellRenderer   *cell,
				   GtkTreeModel      *model,
				   GtkTreeIter       *iter,
				   GossipGroupChat   *chat)
{
	gboolean is_header;

	gtk_tree_model_get (model, iter,
			    COL_IS_HEADER, &is_header,
			    -1);

	if (is_header) {
		GossipChatroomRole role;
		gint               nr;

		gtk_tree_model_get (model, iter,
				    COL_HEADER_ROLE, &role,
				    -1);

		nr = gtk_tree_model_iter_n_children (model, iter);

		g_object_set (cell,
			      "name", gossip_chatroom_role_to_string (role, nr),
			      NULL);
	} else {
		gchar *name;

		gtk_tree_model_get (model, iter,
				    COL_NAME, &name,
				    -1);
		g_object_set (cell,
			      "name", name,
			      NULL);

		g_free (name);
	}

	group_chat_cl_set_background (chat, cell, is_header);
}

static void
group_chat_cl_expander_cell_data_func (GtkTreeViewColumn      *column,
				       GtkCellRenderer        *cell,
				       GtkTreeModel           *model,
				       GtkTreeIter            *iter,
				       GossipGroupChat        *chat)
{
	gboolean is_header;

	gtk_tree_model_get (model, iter,
			    COL_IS_HEADER, &is_header,
			    -1);

	if (is_header) {
		GtkTreePath *path;
		gboolean     row_expanded;

		path = gtk_tree_model_get_path (model, iter);
		row_expanded = gtk_tree_view_row_expanded (GTK_TREE_VIEW (column->tree_view), path);
		gtk_tree_path_free (path);

		g_object_set (cell,
			      "visible", TRUE,
			      "expander-style", row_expanded ? GTK_EXPANDER_EXPANDED : GTK_EXPANDER_COLLAPSED,
			      NULL);
	} else {
		g_object_set (cell, "visible", FALSE, NULL);
	}

	group_chat_cl_set_background (chat, cell, is_header);
}

static void
group_chat_cl_set_background (GossipGroupChat *chat,
			      GtkCellRenderer *cell,
			      gboolean         is_header)
{
	GossipGroupChatPriv *priv;

	priv = GET_PRIV (chat);

	if (is_header) {
		GdkColor  color;
		GtkStyle *style;

		style = gtk_widget_get_style (GTK_WIDGET (priv->treeview));
		color = style->text_aa[GTK_STATE_INSENSITIVE];

		color.red = (color.red + (style->white).red) / 2;
		color.green = (color.green + (style->white).green) / 2;
		color.blue = (color.blue + (style->white).blue) / 2;

		g_object_set (cell,
			      "cell-background-gdk", &color,
			      NULL);
	} else {
		g_object_set (cell,
			      "cell-background-gdk", NULL,
			      NULL);
	}
}

static void
group_chat_contact_joined_cb (GossipChatroom  *chatroom,
			      GossipContact   *contact,
			      GossipGroupChat *chat)
{
	GossipGroupChatPriv       *priv;

	priv = GET_PRIV (chat);

	gossip_debug (DEBUG_DOMAIN, "Contact joined:'%s'",
		      gossip_contact_get_id (contact));

	group_chat_contact_add (chat, contact);

	g_signal_connect (contact, "notify::presences",
			  G_CALLBACK (group_chat_contact_presence_updated_cb),
			  chat);
	g_signal_connect (contact, "notify::name",
			  G_CALLBACK (group_chat_contact_updated_cb),
			  chat);

	g_signal_emit_by_name (chat, "contact_added", contact);
	
	/* Add event to chatroom */
	if (!gossip_contact_equal (priv->own_contact, contact)) {
		gchar *str;

		str = g_strdup_printf (_("%s has joined the room"),
				       gossip_contact_get_name (contact));
		gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, str);
		g_free (str);
	}
}

static void
group_chat_contact_left_cb (GossipChatroom  *chatroom,
			    GossipContact   *contact,
			    GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;

	priv = GET_PRIV (chat);

	gossip_debug (DEBUG_DOMAIN, "Contact left:'%s'",
		      gossip_contact_get_id (contact));

	g_signal_handlers_disconnect_by_func (contact,
					      group_chat_contact_updated_cb,
					      chat);
	g_signal_handlers_disconnect_by_func (contact,
					      group_chat_contact_presence_updated_cb,
					      chat);

	group_chat_contact_remove (chat, contact);

	g_signal_emit_by_name (chat, "contact_removed", contact);

	/* Add event to chatroom */
	if (!gossip_contact_equal (priv->own_contact, contact)) {
		gchar *str;
		str = g_strdup_printf (_("%s has left the room"),
				       gossip_contact_get_name (contact));
		gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, str);
		g_free (str);
	}
}

static void
group_chat_contact_info_changed_cb (GossipChatroom            *chatroom,
				    GossipContact             *contact,
				    GossipGroupChat           *chat)
{
	group_chat_contact_remove (chat, contact);
	group_chat_contact_add (chat, contact);
}

static void
group_chat_contact_add (GossipGroupChat *chat,
			GossipContact   *contact)
{
	GossipGroupChatPriv       *priv;
	GossipChatroomContactInfo *info;
	GossipChatroomRole         role = GOSSIP_CHATROOM_ROLE_NONE;
	GtkTreeModel              *model;
	GtkTreeIter                iter;
	GtkTreeIter                parent;
	GdkPixbuf                 *pixbuf;
	GtkTreePath               *path;

	priv = GET_PRIV (chat);

	pixbuf = gossip_pixbuf_for_contact (contact);

	info = gossip_chatroom_get_contact_info (priv->chatroom, contact);
	if (info) {
		role = info->role;
	}
	group_chat_get_role_iter (chat, role, &parent);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));
	gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent);

	gtk_tree_store_set (GTK_TREE_STORE (model),
			    &iter,
			    COL_CONTACT, contact,
			    COL_NAME, gossip_contact_get_name (contact),
			    COL_STATUS, pixbuf,
			    COL_IS_HEADER, FALSE,
			    -1);

	path = gtk_tree_model_get_path (model, &parent);
	gtk_tree_view_expand_row (GTK_TREE_VIEW (priv->treeview), path, TRUE);
	gtk_tree_path_free (path);

	g_object_unref (pixbuf);
}

static void
group_chat_contact_remove (GossipGroupChat *chat,
			   GossipContact   *contact)
{
	GossipGroupChatPriv *priv;
	GtkTreeIter          iter;

	priv = GET_PRIV (chat);

	if (group_chat_contacts_find (chat, contact, &iter)) {
		GtkTreeModel *model;
		GtkTreeIter   parent;

		model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));
		gtk_tree_model_iter_parent (model, &parent, &iter);

		gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

		if (!gtk_tree_model_iter_has_child (model, &parent)) {
			gtk_tree_store_remove (GTK_TREE_STORE (model), &parent);
		}
	}
}

static void
group_chat_contact_presence_updated_cb (GossipContact   *contact,
					GParamSpec      *param,
					GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GtkTreeIter          iter;

	priv = GET_PRIV (chat);

	gossip_debug (DEBUG_DOMAIN, "Contact Presence Updated:'%s'",
		      gossip_contact_get_id (contact));

	if (group_chat_contacts_find (chat, contact, &iter)) {
		GdkPixbuf    *pixbuf;
		GtkTreeModel *model;

		pixbuf = gossip_pixbuf_for_contact (contact);

		model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));
		gtk_tree_store_set (GTK_TREE_STORE (model),
				    &iter,
				    COL_STATUS, pixbuf,
				    -1);

		gdk_pixbuf_unref (pixbuf);
	} else {
		g_signal_handlers_disconnect_by_func (contact,
						      group_chat_contact_presence_updated_cb,
						      chat);
	}
}

static void
group_chat_contact_updated_cb (GossipContact   *contact,
			       GParamSpec      *param,
			       GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GtkTreeIter          iter;

	priv = GET_PRIV (chat);

	if (group_chat_contacts_find (chat, contact, &iter)) {
		GtkTreeModel *model;

		model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));
		gtk_tree_store_set (GTK_TREE_STORE (model),
				    &iter,
				    COL_NAME, gossip_contact_get_name (contact),
				    -1);
	} else {
		g_signal_handlers_disconnect_by_func (contact,
						      group_chat_contact_updated_cb,
						      chat);
	}
}

static void
group_chat_private_chat_new (GossipGroupChat *chat,
			     GossipContact   *contact)
{
	GossipGroupChatPriv *priv;
	GossipPrivateChat   *private_chat;
	GossipChatManager   *chat_manager;
	GtkWidget           *widget;

	priv = GET_PRIV (chat);

	chat_manager = gossip_app_get_chat_manager ();
	gossip_chat_manager_show_chat (chat_manager, contact);

	private_chat = gossip_chat_manager_get_chat (chat_manager, contact);
	priv->private_chats = g_list_prepend (priv->private_chats, private_chat);

	g_object_weak_ref (G_OBJECT (private_chat),
			   (GWeakNotify) group_chat_private_chat_removed,
			   chat);

	/* Set private chat sensitive, since previously we might have
	 * already had a private chat with this JID and in this room,
	 * and they would have been made insensitive when the last
	 * room was closed.
	 */
	widget = gossip_chat_get_widget (GOSSIP_CHAT (private_chat));
	gtk_widget_set_sensitive (widget, TRUE);
}

static void
group_chat_private_chat_removed (GossipGroupChat *chat,
				 GossipChat      *private_chat)
{
	GossipGroupChatPriv *priv;

	priv = GET_PRIV (chat);

	priv->private_chats = g_list_remove (priv->private_chats, private_chat);
}

static void
group_chat_private_chat_stop_foreach (GossipChat      *private_chat,
				      GossipGroupChat *chat)
{
	GtkWidget *widget;

	widget = gossip_chat_get_widget (private_chat);
	gtk_widget_set_sensitive (widget, FALSE);

	g_object_weak_unref (G_OBJECT (private_chat),
			     (GWeakNotify) group_chat_private_chat_removed,
			     chat);
}

static void
group_chat_send (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GtkTextBuffer       *buffer;
	GtkTextIter          start, end;
	gchar		    *msg;
	gboolean             handled_command = FALSE;

	priv = GET_PRIV (chat);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->input_text_view));

	gtk_text_buffer_get_bounds (buffer, &start, &end);
	msg = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	if (G_STR_EMPTY (msg)) {
		g_free (msg);
		return;
	}

	/* Clear the input field. */
	gtk_text_buffer_set_text (buffer, "", -1);

	/* Check for commands */
	if (g_ascii_strncasecmp (msg, "/nick ", 6) == 0 && strlen (msg) > 6) {
		const gchar *nick;

		nick = msg + 6;
		gossip_chatroom_provider_change_nick (priv->chatroom_provider,
						      gossip_chatroom_get_id (priv->chatroom),
						      nick);
		handled_command = TRUE;
	}
	else if (g_ascii_strncasecmp (msg, "/topic ", 7) == 0 && strlen (msg) > 7) {
		const gchar *topic;

		topic = msg + 7;
		gossip_chatroom_provider_change_topic (priv->chatroom_provider,
						       gossip_chatroom_get_id (priv->chatroom),
						       topic);
		handled_command = TRUE;
	}
	else if (g_ascii_strcasecmp (msg, "/clear") == 0) {
		GtkTextBuffer *buffer;

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->view));
		gtk_text_buffer_set_text (buffer, "", -1);

		handled_command = TRUE;
	}

	if (!handled_command) {
		gossip_chatroom_provider_send (priv->chatroom_provider, 
					       gossip_chatroom_get_id (priv->chatroom), 
					       msg);
		
		gossip_chat_sent_message_add (GOSSIP_CHAT (chat), msg);

		gossip_app_set_not_away ();
	}

	g_free (msg);

	GOSSIP_CHAT (chat)->is_first_char = TRUE;
}

static gboolean
group_chat_get_nick_list_foreach (GtkTreeModel  *model,
				  GtkTreePath   *path,
				  GtkTreeIter   *iter,
				  GList        **list)
{
	gchar *name;

	if (gtk_tree_path_get_depth (path) == 1) {
		/* No contacts on depth 1 */
		return FALSE;
	}

	gtk_tree_model_get (model,
			    iter,
			    COL_NAME, &name,
			    -1);

	*list = g_list_prepend (*list, name);

	return FALSE;
}

static GList *
group_chat_get_nick_list (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GtkTreeModel        *model;
	GList               *list = NULL;

	priv = GET_PRIV (chat);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) group_chat_get_nick_list_foreach,
				&list);

	return list;
}

static GtkWidget *
group_chat_get_widget (GossipChat *chat)
{
	GossipGroupChat     *group_chat;
	GossipGroupChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), NULL);

	group_chat = GOSSIP_GROUP_CHAT (chat);
	priv = GET_PRIV (group_chat);

	return priv->widget;
}

static const gchar *
group_chat_get_name (GossipChat *chat)
{
	GossipGroupChat     *group_chat;
	GossipGroupChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), NULL);

	group_chat = GOSSIP_GROUP_CHAT (chat);
	priv = GET_PRIV (group_chat);

	return priv->name;
}

static gchar *
group_chat_get_tooltip (GossipChat *chat)
{
	GossipGroupChat     *group_chat;
	GossipGroupChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), NULL);

	group_chat = GOSSIP_GROUP_CHAT (chat);
	priv = GET_PRIV (group_chat);

	if (priv->topic) {
		gchar *topic, *tmp;

		topic = g_strdup_printf (_("Topic: %s"), priv->topic);
		tmp = g_strdup_printf ("%s\n%s", priv->name, topic);
		g_free (topic);

		return tmp;
	}

	return g_strdup (priv->name);
}

static GdkPixbuf *
group_chat_get_status_pixbuf (GossipChat *chat)
{
	GossipGroupChat     *group_chat;
	GossipGroupChatPriv *priv;
	GdkPixbuf           *pixbuf = NULL;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), NULL);

	group_chat = GOSSIP_GROUP_CHAT (chat);
	priv = GET_PRIV (group_chat);

	if (priv->chatroom) {
		pixbuf = gossip_pixbuf_for_chatroom_status (priv->chatroom, GTK_ICON_SIZE_MENU);
	}

	if (!pixbuf) {
		pixbuf = gossip_pixbuf_from_stock (GOSSIP_STOCK_GROUP_MESSAGE,
						   GTK_ICON_SIZE_MENU);
	}

	return pixbuf;
}

static GossipContact *
group_chat_get_own_contact (GossipChat *chat)
{
	GossipGroupChat     *group_chat;
	GossipGroupChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), NULL);

	group_chat = GOSSIP_GROUP_CHAT (chat);
	priv = GET_PRIV (group_chat);

	return priv->own_contact;
}

static GossipChatroom *
group_chat_get_chatroom (GossipChat *chat)
{
	GossipGroupChat     *group_chat;
	GossipGroupChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), NULL);

	group_chat = GOSSIP_GROUP_CHAT (chat);
	priv = GET_PRIV (group_chat);

	return priv->chatroom;
}

static gboolean
group_chat_get_show_contacts (GossipChat *chat)
{
	GossipGroupChat     *group_chat;
	GossipGroupChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), FALSE);

	group_chat = GOSSIP_GROUP_CHAT (chat);
	priv = GET_PRIV (group_chat);

	return priv->contacts_visible;
}

static void
group_chat_set_show_contacts (GossipChat *chat,
			      gboolean    show)
{
	GossipGroupChat     *group_chat;
	GossipGroupChatPriv *priv;

	g_return_if_fail (GOSSIP_IS_GROUP_CHAT (chat));

	group_chat = GOSSIP_GROUP_CHAT (chat);
	priv = GET_PRIV (group_chat);

	priv->contacts_visible = show;

	if (show) {
		gtk_widget_show (priv->scrolled_window_contacts);
		gtk_paned_set_position (GTK_PANED (priv->hpaned),
					priv->contacts_width);
	} else {
		priv->contacts_width = gtk_paned_get_position (GTK_PANED (priv->hpaned));
		gtk_widget_hide (priv->scrolled_window_contacts);
	}
}

static gboolean
group_chat_is_group_chat (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), FALSE);

	return TRUE;
}

static gboolean
group_chat_is_connected (GossipChat *chat)
{
	GossipGroupChatPriv  *priv;
	GossipChatroomStatus  status;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), FALSE);
	
	priv = GET_PRIV (chat);
	
	if (!priv->chatroom) {
		return FALSE;
	}

	status = gossip_chatroom_get_status (priv->chatroom);

	return status == GOSSIP_CHATROOM_STATUS_ACTIVE;
}

static gboolean
group_chat_get_role_iter_foreach (GtkTreeModel     *model,
				  GtkTreePath      *path,
				  GtkTreeIter      *iter,
				  FindRoleIterData *fr)
{
	GossipChatroomRole role;
	gboolean           is_header;

	/* Headers are only at the top level. */
	if (gtk_tree_path_get_depth (path) != 1) {
		return FALSE;
	}

	gtk_tree_model_get (model, iter,
			    COL_IS_HEADER, &is_header,
			    COL_HEADER_ROLE, &role,
			    -1);

	if (is_header && role == fr->role) {
		fr->iter = *iter;
		fr->found = TRUE;
	}

	return fr->found;
}

static void
group_chat_get_role_iter (GossipGroupChat    *chat,
			  GossipChatroomRole  role,
			  GtkTreeIter        *iter)
{
	GossipGroupChatPriv *priv;
	GtkTreeModel        *model;
	FindRoleIterData     fr;

	priv = GET_PRIV (chat);

	fr.found = FALSE;
	fr.role = role;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));
	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) group_chat_get_role_iter_foreach,
				&fr);

	if (!fr.found) {
		gtk_tree_store_append (GTK_TREE_STORE (model), iter, NULL);
		gtk_tree_store_set (GTK_TREE_STORE (model), iter,
				    COL_IS_HEADER, TRUE,
				    COL_HEADER_ROLE, role,
				    -1);
	} else {
		*iter = fr.iter;
	}
}

/* Scroll down after the back-log has been received. */
static gboolean
group_chat_scroll_down_idle_func (GossipChat *chat)
{
	GossipGroupChatPriv *priv;

	priv = GET_PRIV (chat);

	gossip_chat_scroll_down (chat);
	g_object_unref (chat);

	priv->scroll_idle_id = 0;

	return FALSE;
}

/* Copied from the jabber backend for now since we don't have an abstraction for
 * "contacts" for group chats. Casefolds the node part (the part before @).
 */
static gchar *
jid_casefold_node (const gchar *str)
{
	gchar       *tmp;
	gchar       *ret;
	const gchar *at;

	at = strchr (str, '@');
	if (!at) {
		return g_strdup (str);
	}

	tmp = g_utf8_casefold (str, at - str);
	ret = g_strconcat (tmp, at, NULL);
	g_free (tmp);

	return ret;
}

GossipGroupChat *
gossip_group_chat_new (GossipChatroomProvider *provider,
		       GossipChatroom         *chatroom)
{
	GossipGroupChat     *chat;
	GossipGroupChatPriv *priv;
	gchar               *casefolded_id;
	gchar               *own_contact_id;
	GossipContact       *own_contact;
	GossipChatroom      *chatroom_found;
	GossipChatroomId     id;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider), NULL);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	if (!group_chats) {
		group_chats = g_hash_table_new_full (NULL,
						     NULL,
						     NULL,
						     (GDestroyNotify) g_object_unref);
	}

	/* Check this group chat is not already shown */
	id = gossip_chatroom_get_id (chatroom);
	chat = g_hash_table_lookup (group_chats, GINT_TO_POINTER (id));
	if (chat) {
		gossip_chat_present (GOSSIP_CHAT (chat));
		return chat;
	}

	chatroom_found = gossip_chatroom_provider_find (provider, chatroom);
	if (chatroom_found) {
		/* Check this group chat is not shown under another id */
		id = gossip_chatroom_get_id (chatroom_found);
		chat = g_hash_table_lookup (group_chats, GINT_TO_POINTER (id));
		if (chat) {
			gossip_chat_present (GOSSIP_CHAT (chat));
			return chat;
		}
	}

	/* FIXME: Jabberism --- Get important details like own contact, etc */
	casefolded_id = jid_casefold_node (gossip_chatroom_get_id_str (chatroom));
	own_contact_id = g_strdup_printf ("%s/%s",
					  casefolded_id,
					  gossip_chatroom_get_nick (chatroom));
	g_free (casefolded_id);

	own_contact = gossip_contact_new_full (GOSSIP_CONTACT_TYPE_TEMPORARY,
					       gossip_chatroom_get_account (chatroom),
					       own_contact_id,
					       gossip_chatroom_get_nick (chatroom));
	g_free (own_contact_id);

	/* Create new group chat object */
	chat = g_object_new (GOSSIP_TYPE_GROUP_CHAT, NULL);

	priv = GET_PRIV (chat);

	priv->own_contact = own_contact;

	priv->chatroom = g_object_ref (chatroom);
	priv->chatroom_provider = g_object_ref (provider);

	priv->name = g_strdup (gossip_chatroom_get_name (chatroom));

	priv->private_chats = NULL;

 	g_hash_table_insert (group_chats, GINT_TO_POINTER (id), chat);

	g_signal_connect (provider, "chatroom-new-message",
			  G_CALLBACK (group_chat_new_message_cb),
			  chat);
	g_signal_connect (provider, "chatroom-new-event",
			  G_CALLBACK (group_chat_new_event_cb),
			  chat);
	g_signal_connect (provider, "chatroom-topic-changed",
			  G_CALLBACK (group_chat_topic_changed_cb),
			  chat);
	g_signal_connect (chatroom, "contact-joined",
			  G_CALLBACK (group_chat_contact_joined_cb),
			  chat);
	g_signal_connect (chatroom, "contact-left",
			  G_CALLBACK (group_chat_contact_left_cb),
			  chat);
	g_signal_connect (chatroom, "contact-info-changed",
			  G_CALLBACK (group_chat_contact_info_changed_cb),
			  chat);

	/* Actually join the chat room */
	group_chat_join (chat);

	/* NOTE: We must start the join before here otherwise the
	 * chatroom doesn't exist for the provider to find it, and
	 * this is needed in the function below, since it calls
	 * gossip_chat_get_chatroom() which tries to find the chatroom
	 * from the provider by id and if we haven't started the join,
	 * it officially doesn't exist according to the backend.
	 */
	
 	gossip_chat_present (GOSSIP_CHAT (chat)); 

	priv->scroll_idle_id = g_idle_add ((GSourceFunc) 
					   group_chat_scroll_down_idle_func, 
					   g_object_ref (chat));

	return chat;
}

GossipChatroomId
gossip_group_chat_get_chatroom_id (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), 0);

	priv = GET_PRIV (chat);
	
	if (!priv->chatroom) {
		return 0;
	}

	return gossip_chatroom_get_id (priv->chatroom);
}

GossipChatroomProvider *
gossip_group_chat_get_chatroom_provider (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	return priv->chatroom_provider;
}

GossipChatroom * 
gossip_group_chat_get_chatroom (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	return priv->chatroom;
}
