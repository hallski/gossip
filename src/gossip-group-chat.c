/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2006 Imendio AB
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
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libgossip/gossip-chatroom-contact.h>
#include <libgossip/gossip-chatroom-provider.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-message.h>

#include "gossip-app.h"
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
#include "gossip-log.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_GROUP_CHAT, GossipGroupChatPriv))

#define IS_ENTER(v) (v == GDK_Return || v == GDK_ISO_Enter || v == GDK_KP_Enter)

#define DEBUG_DOMAIN "GroupChat"

struct _GossipGroupChatPriv {
	GossipContact          *own_contact;

	GossipChatroomProvider *chatroom_provider;
	GossipChatroomId        chatroom_id;

	GtkWidget              *widget;
	GtkWidget              *textview_sw;
	GtkWidget              *topic_entry;
	GtkWidget              *treeview;
	GtkWidget              *contacts_sw;
	GtkWidget              *hpaned;

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

static void            group_chat_contact_list_iface_init     (GossipContactListIfaceClass  *iface);
static void            group_chat_finalize                    (GObject                      *object);
static void            group_chats_init                       (void);
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
static void            group_chat_create_gui                  (GossipGroupChat              *chat);
static void            group_chat_joined_cb                   (GossipChatroomProvider       *provider,
							       gint                          id,
							       GossipGroupChat              *chat);
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
static void            group_chat_contact_joined_cb           (GossipChatroomProvider       *provider,
							       gint                          id,
							       GossipContact                *contact,
							       GossipGroupChat              *chat);
static void            group_chat_contact_left_cb             (GossipChatroomProvider       *provider,
							       gint                          id,
							       GossipContact                *contact,
							       GossipGroupChat              *chat);
static void            group_chat_contact_presence_updated_cb (GossipChatroomProvider       *provider,
							       gint                          id,
							       GossipContact                *contact,
							       GossipGroupChat              *chat);
static void            group_chat_contact_updated_cb          (GossipChatroomProvider       *provider,
							       gint                          id,
							       GossipContact                *contact,
							       GossipGroupChat              *chat);
static void            group_chat_topic_activate_cb           (GtkEntry                     *entry,
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
static void            group_chat_roster_name_data_func       (GtkTreeViewColumn      *tree_column,
							      GtkCellRenderer        *cell,
							      GtkTreeModel           *model,
							      GtkTreeIter            *iter,
							      GossipGroupChat        *chat);


enum {
	COL_STATUS,
	COL_NAME,
	COL_CONTACT,
	NUMBER_OF_COLS
};

enum DndDragType {
	DND_DRAG_TYPE_CONTACT_ID,
};

static const GtkTargetEntry drop_types[] = {
	{ "text/contact-id", 0, DND_DRAG_TYPE_CONTACT_ID },
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
            
        object_class->finalize = group_chat_finalize;

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

	chat = GOSSIP_GROUP_CHAT (object);
	priv = GET_PRIV (chat);
	
	g_signal_handlers_disconnect_by_func (priv->chatroom_provider, 
					      group_chat_joined_cb, chat);
	g_signal_handlers_disconnect_by_func (priv->chatroom_provider, 
					      group_chat_new_message_cb, chat);
	g_signal_handlers_disconnect_by_func (priv->chatroom_provider,
					      group_chat_new_event_cb, 
					      chat);
	g_signal_handlers_disconnect_by_func (priv->chatroom_provider,
					      group_chat_topic_changed_cb, 
					      chat);
	g_signal_handlers_disconnect_by_func (priv->chatroom_provider,
					      group_chat_contact_joined_cb,
					      chat);
	g_signal_handlers_disconnect_by_func (priv->chatroom_provider,
					      group_chat_contact_left_cb,
					      chat);
	g_signal_handlers_disconnect_by_func (priv->chatroom_provider,
					      group_chat_contact_presence_updated_cb,
					      chat);
	g_signal_handlers_disconnect_by_func (priv->chatroom_provider,
					      group_chat_contact_updated_cb,
					      chat);
	
	g_free (priv->name);

	g_list_foreach (priv->private_chats, 
			(GFunc)group_chat_private_chat_stop_foreach, 
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
group_chats_init (void)
{
	static gboolean inited = FALSE;

	if (inited) {
		return;
	}
	
	inited = TRUE;
	
	group_chats = g_hash_table_new_full (NULL, 
					     NULL,
					     NULL,
					     (GDestroyNotify) g_object_unref);
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

/* 	gtk_widget_set_sensitive (GOSSIP_CHAT (chat)->input_text_view, TRUE); */

/* 	gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, _("Connected")); */

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

	gtk_widget_set_sensitive (GOSSIP_CHAT (chat)->input_text_view, FALSE);

	gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, _("Disconnected"));

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
		adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->textview_sw));
		gtk_adjustment_set_value (adj, adj->value - adj->page_size);

		return TRUE;
	}
	else if ((event->state & GDK_CONTROL_MASK) != GDK_CONTROL_MASK &&
		 event->keyval == GDK_Page_Down) {
		adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->textview_sw));
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
	GossipGroupChatPriv *priv;

	priv = GET_PRIV (chat);
	
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
	GossipGroupChatPriv *priv;
	GossipContact       *contact;
	const gchar         *id;
	gchar               *str;

	priv = GET_PRIV (chat);

	id = (const gchar*) selection->data;
	/*g_print ("Received drag & drop contact from roster with id:'%s'\n", id);*/

	contact = gossip_session_find_contact (gossip_app_get_session (), id);
	
	if (!contact) {
		/*g_print ("No contact found associated with drag & drop\n");*/
		return;
	}

	/* send event to chat window */
	str = g_strdup_printf (_("Invited %s to join this chat conference."),
			       gossip_contact_get_id (contact));
 	gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, str);
	g_free (str);

	gossip_chat_invite_dialog_show (contact, priv->chatroom_id);

	/* clean up dnd */
	gtk_drag_finish (context, TRUE, FALSE, GDK_CURRENT_TIME);
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

	priv = GET_PRIV (chat);
	
	gossip_chatroom_provider_leave (priv->chatroom_provider, priv->chatroom_id);
	
	g_hash_table_remove (group_chats, GINT_TO_POINTER (priv->chatroom_id));
}

static gint
group_chat_contacts_sort_func (GtkTreeModel *model,
			       GtkTreeIter  *iter_a,
			       GtkTreeIter  *iter_b,
			       gpointer      user_data)
{
	GossipContact      *contact_a, *contact_b;
	GossipChatroomRole  role_a, role_b;
	gint                ret_val;
	
	gtk_tree_model_get (model, iter_a, COL_CONTACT, &contact_a, -1);
	gtk_tree_model_get (model, iter_b, COL_CONTACT, &contact_b, -1);

	role_a = gossip_chatroom_contact_get_role (GOSSIP_CHATROOM_CONTACT (contact_a));
	role_b = gossip_chatroom_contact_get_role (GOSSIP_CHATROOM_CONTACT (contact_b));

	if (role_a == role_b) {
		const gchar *name_a;
		const gchar *name_b;

		name_a = gossip_contact_get_name (contact_a);
		name_b = gossip_contact_get_name (contact_b);

		ret_val = g_ascii_strcasecmp (name_a, name_b);
	} else {
		ret_val = role_a - role_b;
	}
	
	g_object_unref (contact_a);
	g_object_unref (contact_b);

	return ret_val;
}

static void
group_chat_contacts_setup (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GtkTreeView         *tree;
	GtkTextView         *tv;
	GtkListStore        *store;
	GtkCellRenderer     *cell;	
	GtkTreeViewColumn   *col;

	priv = GET_PRIV (chat);

	tree = GTK_TREE_VIEW (priv->treeview);
	tv = GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->view);

	store = gtk_list_store_new (NUMBER_OF_COLS,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_STRING,
				    G_TYPE_OBJECT);

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
	gtk_tree_view_column_add_attribute (col,
					    cell,
					    "pixbuf", COL_STATUS);

	cell = gossip_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);

	gtk_tree_view_column_set_cell_data_func (col, cell, 
						 (GtkTreeCellDataFunc) group_chat_roster_name_data_func,
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
group_chat_create_gui (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GladeXML            *glade;
	GtkWidget           *focus_vbox;
	GtkWidget           *input_textview_sw;
	GList               *list;

	priv = GET_PRIV (chat);
	
	glade = gossip_glade_get_file ("group-chat.glade",
				       "group_chat_widget",
				       NULL,
				       "group_chat_widget", &priv->widget,
				       "chat_view_sw", &priv->textview_sw,
				       "input_text_view_sw", &input_textview_sw,
				       "topic_entry", &priv->topic_entry,
				       "treeview", &priv->treeview,
				       "contacts_sw", &priv->contacts_sw,
				       "left_vbox", &focus_vbox,
				       "hpaned", &priv->hpaned,
				       NULL);

	gossip_glade_connect (glade,
			      chat,
			      "group_chat_widget", "destroy", group_chat_widget_destroy_cb,
			      "topic_entry", "activate", group_chat_topic_activate_cb,
			      NULL);

	g_object_set_data (G_OBJECT (priv->widget), "chat", chat);

	gtk_container_add (GTK_CONTAINER (priv->textview_sw), 
			   GTK_WIDGET (GOSSIP_CHAT (chat)->view));
	gtk_widget_show (GTK_WIDGET (GOSSIP_CHAT (chat)->view));

	gtk_container_add (GTK_CONTAINER (input_textview_sw),
			   GOSSIP_CHAT (chat)->input_text_view);
	gtk_widget_show (GOSSIP_CHAT (chat)->input_text_view);

	g_signal_connect (GOSSIP_CHAT (chat)->input_text_view,
			  "key_press_event",
			  G_CALLBACK (group_chat_key_press_event_cb),
			  chat);

	g_signal_connect (GOSSIP_CHAT (chat)->view,
			  "focus_in_event",
			  G_CALLBACK (group_chat_focus_in_event_cb),
			  chat);

	/* drag & drop */ 
	gtk_drag_dest_set (GTK_WIDGET (priv->treeview), 
			   GTK_DEST_DEFAULT_ALL, 
			   drop_types, 
			   G_N_ELEMENTS (drop_types),
			   GDK_ACTION_COPY);
		  
	g_signal_connect (priv->treeview,
			  "drag-data-received",
			  G_CALLBACK (group_chat_drag_data_received),
			  chat);

	g_object_unref (glade);

	list = NULL;
	list = g_list_append (list, GOSSIP_CHAT (chat)->input_text_view);
	list = g_list_append (list, priv->topic_entry);
	
	gtk_container_set_focus_chain (GTK_CONTAINER (focus_vbox), list);
	
	priv->completion = g_completion_new (NULL);
	g_completion_set_compare (priv->completion,
				  group_chat_contacts_completion_func);
		
	gtk_widget_grab_focus (GOSSIP_CHAT (chat)->input_text_view);
	group_chat_contacts_setup (chat);
}

static void 
group_chat_joined_cb (GossipChatroomProvider *provider,
		      gint                    id,
		      GossipGroupChat        *chat)
{
	GossipGroupChatPriv *priv;
	GossipChatView      *chatview;
	GtkTreeView         *view;
	GtkTreeModel        *model;
	GtkListStore        *store;
	
	priv = GET_PRIV (chat);

	if (priv->chatroom_id != id) {
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "[%d] Joined", id);

	chatview = GOSSIP_CHAT (chat)->view;

	/* Clear previous messages */
	gossip_chat_view_clear (chatview);

	/* Clear previous roster */
	view = GTK_TREE_VIEW (priv->treeview);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);
	gtk_list_store_clear (store);

	/* Allow use of the input textview */
 	gtk_widget_set_sensitive (GOSSIP_CHAT (chat)->input_text_view, TRUE); 
}

static void 
group_chat_new_message_cb (GossipChatroomProvider *provider,
			   gint                    id,
			   GossipMessage          *message,
			   GossipGroupChat        *chat)
{
	GossipGroupChatPriv  *priv;
	GossipChatroom       *chatroom;
	GossipChatroomInvite *invite;

	priv = GET_PRIV (chat);

	if (priv->chatroom_id != id) {
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

 	chatroom = gossip_chatroom_provider_find (priv->chatroom_provider, 
						  priv->chatroom_id);
	gossip_log_message_for_chatroom (chatroom, message, FALSE);

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

	if (priv->chatroom_id != id) {
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
	gchar               *str;

	priv = GET_PRIV (chat);

	if (priv->chatroom_id != id) {
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "[%d] Topic changed by:'%s' to:'%s'", 
		      id, gossip_contact_get_id (who), new_topic);
	
	gtk_entry_set_text (GTK_ENTRY (priv->topic_entry), new_topic);
	
	str = g_strdup_printf (_("%s has set the topic"),
				 gossip_contact_get_name (who));
	event = g_strconcat (str, ": ", new_topic, NULL);
	g_free (str);

	gossip_chat_view_append_event (GOSSIP_CHAT (chat)->view, event);
	g_free (event);
}

static void
group_chat_contact_joined_cb (GossipChatroomProvider *provider,
			      gint                    id,
			      GossipContact          *contact,
			      GossipGroupChat        *chat)
{
	GossipGroupChatPriv *priv;
	GtkTreeModel        *model;
	GtkTreeIter          iter;
	GdkPixbuf           *pixbuf;
		
	priv = GET_PRIV (chat);
	
	if (priv->chatroom_id != id) {
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "[%d] Contact joined:'%s'", 
		      id, gossip_contact_get_id (contact));

	pixbuf = gossip_pixbuf_for_contact (contact);
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model),
			    &iter,
			    COL_CONTACT, contact,
			    COL_NAME, gossip_contact_get_name (contact),
			    COL_STATUS, pixbuf,
			    -1);

	g_object_unref (pixbuf);

	g_signal_emit_by_name (chat, "contact_added", contact);
}

static void
group_chat_contact_left_cb (GossipChatroomProvider *provider,
			    gint                    id,
			    GossipContact          *contact,
			    GossipGroupChat        *chat)
{
	GossipGroupChatPriv *priv;
	GtkTreeIter          iter;

	priv = GET_PRIV (chat);
	
	if (priv->chatroom_id != id) {
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "[%d] Contact left:'%s'", 
		      id, gossip_contact_get_id (contact));
	
	if (group_chat_contacts_find (chat, contact, &iter)) {
		GtkTreeModel *model;
		model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

		g_signal_emit_by_name (chat, "contact_removed", contact);
	}
}

static void
group_chat_contact_presence_updated_cb (GossipChatroomProvider *provider,
					gint                    id,
					GossipContact          *contact,
					GossipGroupChat        *chat)
{
	GossipGroupChatPriv *priv;
	GtkTreeIter          iter;

	priv = GET_PRIV (chat);
	
	if (priv->chatroom_id != id) {
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "[%d] Contact Presence Updated:'%s'", 
		      id, gossip_contact_get_id (contact));

	if (group_chat_contacts_find (chat, contact, &iter)) {
		GdkPixbuf    *pixbuf;
		GtkTreeModel *model;
		
		pixbuf = gossip_pixbuf_for_contact (contact);

		model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));
		gtk_list_store_set (GTK_LIST_STORE (model),
				    &iter,
				    COL_STATUS, pixbuf,
				    -1);

		gdk_pixbuf_unref (pixbuf);
		
		g_signal_emit_by_name (chat, "contact_presence_updated",
				       contact);
	}
}

static void
group_chat_contact_updated_cb (GossipChatroomProvider *provider,
			       gint                    id,
			       GossipContact          *contact,
			       GossipGroupChat        *chat)
{
	GossipGroupChatPriv *priv;
	GtkTreeIter          iter;

	priv = GET_PRIV (chat);
	
	if (priv->chatroom_id != id) {
		return;
	}

	if (group_chat_contacts_find (chat, contact, &iter)) {
		GtkTreeModel *model;
		
		model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->treeview));
		gtk_list_store_set (GTK_LIST_STORE (model),
				    &iter,
				    COL_NAME, gossip_contact_get_name (contact),
				    -1);

		g_signal_emit_by_name (chat, "contact_updated", contact);
	}
}

static void
group_chat_topic_activate_cb (GtkEntry *entry, 
			      GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	
	priv = GET_PRIV (chat);
	
	gossip_chatroom_provider_change_topic (priv->chatroom_provider, 
					       priv->chatroom_id,
					       gtk_entry_get_text (entry));
	
	gtk_widget_grab_focus (GOSSIP_CHAT (chat)->input_text_view);
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

	if (msg == NULL || msg[0] == '\0') {
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
						      priv->chatroom_id,
						      nick);
		handled_command = TRUE;
	}
	else if (g_ascii_strncasecmp (msg, "/topic ", 7) == 0 && strlen (msg) > 7) {
		const gchar *topic;

		topic = msg + 7;
		gossip_chatroom_provider_change_topic (priv->chatroom_provider,
						       priv->chatroom_id,
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
		gossip_chatroom_provider_send (priv->chatroom_provider, priv->chatroom_id, msg);
		
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

	return g_strdup (priv->name);
}

static GdkPixbuf *
group_chat_get_status_pixbuf (GossipChat *chat)
{
	GossipGroupChat     *group_chat;
	GossipGroupChatPriv *priv;
	GossipChatroom      *chatroom;
	GdkPixbuf           *pixbuf = NULL;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), NULL);

	group_chat = GOSSIP_GROUP_CHAT (chat);
 	priv = GET_PRIV (group_chat); 

 	chatroom = gossip_chatroom_provider_find (priv->chatroom_provider, priv->chatroom_id);
	if (chatroom) {
		pixbuf = gossip_pixbuf_for_chatroom_status (chatroom, GTK_ICON_SIZE_MENU);
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

 	return gossip_chatroom_provider_find (priv->chatroom_provider, 
					      priv->chatroom_id);
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
		gtk_widget_show (priv->contacts_sw);
		gtk_paned_set_position (GTK_PANED (priv->hpaned),
					priv->contacts_width);
	} else {
		priv->contacts_width = gtk_paned_get_position (GTK_PANED (priv->hpaned));
		gtk_widget_hide (priv->contacts_sw);
	}
}

static gboolean
group_chat_is_group_chat (GossipChat *chat)
{
	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), FALSE);

	return TRUE;
}

static void
group_chat_roster_name_data_func (GtkTreeViewColumn *tree_column,
				  GtkCellRenderer   *cell,
				  GtkTreeModel      *model,
				  GtkTreeIter       *iter,
				  GossipGroupChat   *chat)
{
	GossipChatroomContact *contact;
	gchar                 *name;
	gchar                 *modified_name;

	gtk_tree_model_get (model, iter,
			    COL_NAME, &name,
			    COL_CONTACT, &contact,
			    -1);

	if (gossip_chatroom_contact_get_role (contact) == GOSSIP_CHATROOM_ROLE_MODERATOR) {
		modified_name = g_strdup_printf ("@%s", name);
		g_object_set (cell, "name", modified_name, NULL);
		g_free (modified_name);
	} else {
		g_object_set (cell, "name", name, NULL);
	}
	
	g_free (name);
	g_object_unref (contact);
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
		       GossipChatroomId        id)
{
	GossipGroupChat     *chat;
	GossipGroupChatPriv *priv;
	gchar               *casefolded_id;
 	gchar               *own_contact_id;
	GossipContact       *own_contact;
	GossipChatroom      *chatroom;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM_PROVIDER (provider), NULL);
	
	group_chats_init ();
	
	/* Check this group chat is not already shown */
	chat = g_hash_table_lookup (group_chats, GINT_TO_POINTER (id));
	if (chat) {
		gossip_chat_present (GOSSIP_CHAT (chat));
		return chat;
	}

	/* Get important details like own contact, etc */
	chatroom = gossip_chatroom_provider_find (provider, id);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

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

	priv->chatroom_id = id;
	priv->chatroom_provider = provider;

	priv->name = g_strdup (gossip_chatroom_get_name (chatroom));

	priv->private_chats = NULL;
	
	group_chat_create_gui (chat);

	g_hash_table_insert (group_chats, GINT_TO_POINTER (id), chat);

	g_signal_connect (provider, "chatroom-joined",
			  G_CALLBACK (group_chat_joined_cb),
			  chat);
	g_signal_connect (provider, "chatroom-new-message",
			  G_CALLBACK (group_chat_new_message_cb),
			  chat);
	g_signal_connect (provider, "chatroom-new-event",
			  G_CALLBACK (group_chat_new_event_cb),
			  chat);
	g_signal_connect (provider, "chatroom-topic-changed",
			  G_CALLBACK (group_chat_topic_changed_cb),
			  chat);
	g_signal_connect (provider, "chatroom-contact-joined",
			  G_CALLBACK (group_chat_contact_joined_cb),
			  chat);
	g_signal_connect (provider, "chatroom-contact-left",
			  G_CALLBACK (group_chat_contact_left_cb),
			  chat);
	g_signal_connect (provider, "chatroom-contact-presence-updated",
			  G_CALLBACK (group_chat_contact_presence_updated_cb),
			  chat);
	g_signal_connect (provider, "chatroom-contact-updated",
			  G_CALLBACK (group_chat_contact_updated_cb),
			  chat);

	gossip_chat_present (GOSSIP_CHAT (chat));

 	g_object_ref (chat);
	priv->scroll_idle_id = g_idle_add (
		(GSourceFunc) group_chat_scroll_down_idle_func, chat);
	
	return chat;
}

GossipChatroomId 
gossip_group_chat_get_chatroom_id (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;

 	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), 0); 

	priv = GET_PRIV (chat);	

	return priv->chatroom_id;
}

GossipChatroomProvider *
gossip_group_chat_get_chatroom_provider (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;

 	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), NULL); 

	priv = GET_PRIV (chat);	

 	return priv->chatroom_provider;
}
