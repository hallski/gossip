/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2004 Imendio HB
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
#include <gconf/gconf-client.h>
#include <loudmouth/loudmouth.h>
#include <libgnome/gnome-i18n.h>

#include "gossip-contact-list.h"
#include "gossip-utils.h"
#include "gossip-app.h"
#include "gossip-chat.h"
#include "gossip-chat-view.h"
#include "gossip-group-chat.h"
#include "gossip-private-chat.h"

#define IS_ENTER(v) (v == GDK_Return || v == GDK_ISO_Enter || v == GDK_KP_Enter)

/* Treeview columns */
enum {
	COL_STATUS,
	COL_NAME,
	COL_JID,
	NUMBER_OF_COLS
};

struct _GossipGroupChatPriv {
	gboolean          inited;
	
	LmMessageHandler *message_handler;
	LmMessageHandler *presence_handler;
	
	GtkWidget        *widget;
	GtkWidget        *text_view_sw;
	GtkWidget        *topic_entry;
	GtkWidget        *tree;

	GossipJID        *jid;
	gchar            *name;
	gchar            *nick;

	GCompletion      *completion;

	GTimeVal          last_timestamp;

	GHashTable       *contacts;
	GList            *priv_chats;
};

typedef struct {
	GossipGroupChat *chat;
	GossipJID       *jid;
	gboolean         found;
	GtkTreeIter      found_iter;
} FindUserData;

static void             group_chat_contact_list_init            (GossipContactListClass *iface);

static void             group_chat_finalize                     (GObject          *object);
static void             group_chat_widget_destroy_cb            (GtkWidget         *widget,
								 GossipGroupChat   *chat);
static void             group_chats_init                        (void);
static void             group_chat_create_gui                   (GossipGroupChat   *chat);
static void             group_chat_send                         (GossipGroupChat   *chat,
								 const gchar       *msg);
static void             group_chat_row_activated_cb             (GtkTreeView       *view,
								 GtkTreePath       *path,
								 GtkTreeViewColumn *col,
								 GossipGroupChat   *chat);
static void             group_chat_setup_tree                   (GossipGroupChat   *chat);
static gboolean         group_chat_find_user_foreach            (GtkTreeModel      *model,
								 GtkTreePath       *path,
								 GtkTreeIter       *iter,
								 FindUserData      *data);
static gboolean         group_chat_key_press_event_cb           (GtkWidget         *widget,
								 GdkEventKey       *event,
								 GossipGroupChat   *chat);
static gint             group_chat_completion_compare           (const gchar       *s1,
								 const gchar       *s2,
								 gsize              n);
static gboolean         group_chat_find_user                    (GossipGroupChat   *chat,
								 GossipJID         *jid,
								 GtkTreeIter       *iter);
static GList *          group_chat_get_nick_list                (GossipGroupChat   *chat);
static gboolean         group_chat_focus_in_event_cb            (GtkWidget         *widget,
								 GdkEvent          *event,
								 GossipGroupChat   *chat);
static LmHandlerResult  group_chat_message_handler              (LmMessageHandler  *handler,
								 LmConnection      *connection,
								 LmMessage         *m,
								 gpointer           user_data);
static LmHandlerResult  group_chat_presence_handler             (LmMessageHandler  *handler,
								 LmConnection      *connection,
								 LmMessage         *m,
								 gpointer           user_data);
static void             group_chat_topic_activate_cb            (GtkEntry          *entry,
								 GossipGroupChat   *chat);
static gint             group_chat_iter_compare_func            (GtkTreeModel      *model,
								 GtkTreeIter       *iter_a,
								 GtkTreeIter       *iter_b,
								 gpointer           user_data);
static GossipPrivateChat *     
			group_chat_priv_chat_new                (GossipGroupChat   *chat,
								 GossipJID         *jid);
static void             group_chat_priv_chat_incoming           (GossipGroupChat   *chat,
								 GossipJID         *jid,
								 LmMessage         *m);
static void             group_chat_priv_chat_removed            (GossipGroupChat   *chat,
								 GossipChat        *priv_chat);
static void		group_chat_input_text_view_send         (GossipGroupChat   *chat);
static void             group_chat_priv_chats_disconnect        (GossipGroupChat   *chat);
static gchar *          group_chat_create_contact_name          (GossipJID         *jid);
static GtkWidget *      group_chat_get_widget                   (GossipChat        *chat);
static const gchar *    group_chat_get_name                     (GossipChat        *chat);
static gchar *          group_chat_get_tooltip                  (GossipChat        *chat);
static GdkPixbuf *      group_chat_get_status_pixbuf            (GossipChat        *chat);
static GossipContact *  group_chat_get_contact                  (GossipChat        *chat);
static void             group_chat_get_geometry                 (GossipChat        *chat,
		                                                 gint              *width,
								 gint              *height);


static GHashTable *group_chats = NULL;

G_DEFINE_TYPE_WITH_CODE (GossipGroupChat, gossip_group_chat, 
			 GOSSIP_TYPE_CHAT,
			 G_IMPLEMENT_INTERFACE (GOSSIP_TYPE_CONTACT_LIST,
						group_chat_contact_list_init));

static void
gossip_group_chat_class_init (GossipGroupChatClass *klass)
{
        GObjectClass    *object_class = G_OBJECT_CLASS (klass);
	GossipChatClass *chat_class   = GOSSIP_CHAT_CLASS (klass);
                                                                                
        object_class->finalize = group_chat_finalize;

	chat_class->get_name          = group_chat_get_name;
	chat_class->get_tooltip       = group_chat_get_tooltip;
	chat_class->get_status_pixbuf = group_chat_get_status_pixbuf;
	chat_class->get_contact       = group_chat_get_contact;
	chat_class->get_geometry      = group_chat_get_geometry;
	chat_class->get_widget        = group_chat_get_widget;
}

static void
gossip_group_chat_init (GossipGroupChat *chat)
{
        GossipGroupChatPriv *priv;
                                                                                
        priv = g_new0 (GossipGroupChatPriv, 1);
                                                                                
        chat->priv = priv;
                                                                                
        /*
	g_signal_connect_object (gossip_app_get (),
                                 "connected",
                                 G_CALLBACK (chat_connected_cb),
                                 chat, 0);
                                                                                
        g_signal_connect_object (gossip_app_get (),
                                 "disconnected",
                                 G_CALLBACK (chat_disconnected_cb),
                                 chat, 0);
	 */
}

static void
group_chat_contact_list_init (GossipContactListClass *iface)
{
	/* No functions for now */
}

static void
group_chat_finalize (GObject *object)
{
	GossipGroupChat     *chat;
	GossipGroupChatPriv *priv;
	LmConnection        *connection;
	LmMessageHandler    *handler;
	LmMessage           *m;
	const gchar         *without_resource;

	chat = GOSSIP_GROUP_CHAT (object);
	priv = chat->priv;
	
	group_chat_priv_chats_disconnect (chat);
	
	connection = gossip_app_get_connection ();

	handler = priv->message_handler;
	lm_connection_unregister_message_handler (connection,
						  handler,
						  LM_MESSAGE_TYPE_MESSAGE);

	handler = priv->presence_handler;
	lm_connection_unregister_message_handler (connection,
						  handler,
						  LM_MESSAGE_TYPE_PRESENCE);

	without_resource = gossip_jid_get_without_resource (priv->jid); 
	m = lm_message_new_with_sub_type (without_resource,
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_UNAVAILABLE);
	lm_connection_send (connection, m, NULL);
	lm_message_unref (m);

	gossip_jid_unref (priv->jid);
	g_free (priv->nick);
	g_free (priv->name);
	g_list_free (priv->priv_chats);
	
	g_free (priv);
}

static void
group_chat_widget_destroy_cb (GtkWidget *widget, GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;

	priv = chat->priv;
	
	g_object_ref (chat);

	g_hash_table_remove (group_chats,
			     gossip_jid_get_without_resource (priv->jid));
	
	/* chat and priv will most likely be unvalid after this */
	g_object_unref (chat); 
}

static void
group_chats_init (void)
{
	static gboolean inited = FALSE;

	if (inited) {
		return;
	}
	
	inited = TRUE;
	
	group_chats = g_hash_table_new_full (g_str_hash,
					     g_str_equal,
					     NULL,
					     (GDestroyNotify) g_object_unref);
}

static void
group_chat_create_gui (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GladeXML            *glade;
	GtkWidget           *focus_vbox;
	GtkWidget           *input_text_view_sw;
	GList               *list;

	priv = chat->priv;
	
	glade = gossip_glade_get_file (GLADEDIR "/group-chat.glade",
				       "group_chat_widget",
				       NULL,
				       "group_chat_widget", &priv->widget,
				       "chat_view_sw", &priv->text_view_sw,
				       "input_text_view_sw", &input_text_view_sw,
				       "topic_entry", &priv->topic_entry,
				       "treeview", &priv->tree,
				       "left_vbox", &focus_vbox,
				       NULL);
	
	gossip_glade_connect (glade,
			      chat,
			      "group_chat_widget", "destroy", group_chat_widget_destroy_cb,
			      "topic_entry", "activate", group_chat_topic_activate_cb,
			      NULL);

	g_object_set_data (G_OBJECT (priv->widget), "chat", chat);

	gtk_container_add (GTK_CONTAINER (priv->text_view_sw), 
			   GTK_WIDGET (GOSSIP_CHAT (chat)->view));
	gtk_widget_show (GTK_WIDGET (GOSSIP_CHAT (chat)->view));

	gtk_container_add (GTK_CONTAINER (input_text_view_sw),
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
			  
	g_object_unref (glade);

	list = NULL;
	list = g_list_append (list, priv->topic_entry);
	list = g_list_append (list, GOSSIP_CHAT (chat)->input_text_view);
	
	gtk_container_set_focus_chain (GTK_CONTAINER (focus_vbox), list);
	
	priv->completion = g_completion_new (NULL);
	g_completion_set_compare (priv->completion,
				  group_chat_completion_compare);
		
	gtk_widget_grab_focus (GOSSIP_CHAT (chat)->input_text_view);
	group_chat_setup_tree (chat);

	gossip_chat_view_set_margin (GOSSIP_CHAT (chat)->view, 3);
}

GossipGroupChat *
gossip_group_chat_show (GossipJID *jid, const gchar *nick)
{
	GossipGroupChat     *chat;
	GossipGroupChatPriv *priv;
	const gchar         *without_resource;
	LmMessageHandler    *handler;
	LmConnection        *connection;
	
	group_chats_init ();

	without_resource = gossip_jid_get_without_resource (jid);
	chat = g_hash_table_lookup (group_chats, without_resource);
	if (chat) {
		priv = chat->priv;

		if (priv->nick) {
			g_free (priv->nick);
		}

		priv->nick = g_strdup (nick);

		gossip_chat_present (GOSSIP_CHAT (chat));
		
		return chat;
	}

	chat = g_object_new (GOSSIP_TYPE_GROUP_CHAT, NULL);
	priv = chat->priv;
	
	priv->jid = gossip_jid_ref (jid);
	priv->name = gossip_jid_get_part_name (jid);
	priv->nick = g_strdup (nick);

	priv->inited = FALSE;
	priv->last_timestamp.tv_sec = priv->last_timestamp.tv_usec = 0;

	priv->priv_chats = NULL;
	
	group_chat_create_gui (chat);

	g_hash_table_insert (group_chats, (gchar *)without_resource,
			     g_object_ref (chat));

	connection = gossip_app_get_connection ();
	
	handler = lm_message_handler_new (group_chat_message_handler, chat, NULL);
	priv->message_handler = handler;
	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_MESSAGE,
						LM_HANDLER_PRIORITY_NORMAL);

	handler = lm_message_handler_new (group_chat_presence_handler, chat, NULL);
	priv->presence_handler = handler;
	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_PRESENCE,
						LM_HANDLER_PRIORITY_NORMAL);

	gossip_chat_present (GOSSIP_CHAT (chat));

	return chat;
}

static void
group_chat_send (GossipGroupChat *chat, const gchar *msg)
{
	GossipGroupChatPriv *priv;
	LmConnection        *connection;
	LmMessage           *m;
	const char          *without_resource;

	priv = chat->priv;
	
	connection = gossip_app_get_connection ();
	without_resource = gossip_jid_get_without_resource (priv->jid);

	if (g_ascii_strncasecmp (msg, "/nick ", 6) == 0 && strlen (msg) > 6) {
		gchar *to;

		g_free (priv->nick);
		priv->nick = g_strdup (msg + 6);
	
		to = g_strdup_printf ("%s/%s",
				      without_resource,
				      priv->nick);
		
		m = lm_message_new (to, LM_MESSAGE_TYPE_PRESENCE);

		lm_connection_send (connection, m, NULL);
		lm_message_unref (m);
		g_free (to);
		
		return;
	}
	else if (g_ascii_strcasecmp (msg, "/clear") == 0) {
		GtkTextBuffer *buffer;

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->view));
		gtk_text_buffer_set_text (buffer, "", -1);
		
		return;
	}
	
	m = lm_message_new_with_sub_type (without_resource,
					  LM_MESSAGE_TYPE_MESSAGE,
					  LM_MESSAGE_SUB_TYPE_GROUPCHAT);
	
	lm_message_node_add_child (m->node, "body", msg);

	lm_connection_send (connection, m, NULL);
	lm_message_unref (m);

	gossip_app_force_non_away ();
}

static void
group_chat_row_activated_cb (GtkTreeView       *view,
			     GtkTreePath       *path,
			     GtkTreeViewColumn *col,
			     GossipGroupChat   *chat)
{
	GtkTreeModel *model;
	GtkTreeIter   iter;
	GossipJID    *jid;

	model = gtk_tree_view_get_model (view);

	gtk_tree_model_get_iter (model, &iter, path);
	
	gtk_tree_model_get (model,
			    &iter,
			    COL_JID,  &jid,
			    -1);

	group_chat_priv_chat_new (chat, jid);
}

static void
group_chat_setup_tree (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GtkTreeView         *tree;
	GtkTextView         *tv;
	GtkListStore        *store;
	GtkCellRenderer     *cell;	
	GtkTreeViewColumn   *col;

	priv = chat->priv;
	tree = GTK_TREE_VIEW (priv->tree);
	tv = GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->view);

	gtk_tree_view_set_headers_visible (tree, FALSE);
	
	store = gtk_list_store_new (NUMBER_OF_COLS,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_STRING,
				    G_TYPE_POINTER);

	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store),
						 group_chat_iter_compare_func,
						 chat,
						 NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					      GTK_SORT_ASCENDING);
	
	gtk_tree_view_set_model (tree, GTK_TREE_MODEL (store));
	
	cell = gtk_cell_renderer_pixbuf_new ();
	col = gtk_tree_view_column_new_with_attributes ("",
							cell,
							"pixbuf", COL_STATUS,
							NULL);
	gtk_tree_view_append_column (tree, col);

	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes ("",
							cell,
							"text", COL_NAME,
							NULL);
	
	gtk_tree_view_append_column (tree, col);

	g_signal_connect (tree,
			  "row_activated",
			  G_CALLBACK (group_chat_row_activated_cb),
			  chat);
}

static gboolean
group_chat_find_user_foreach (GtkTreeModel *model,
			      GtkTreePath  *path,
			      GtkTreeIter  *iter,
			      FindUserData *data)
{
	GossipJID *jid;

	gtk_tree_model_get (model,
			    iter,
			    COL_JID, &jid,
			    -1);

	if (gossip_jid_equals (data->jid, jid)) {
		data->found = TRUE;
		data->found_iter = *iter;
		return TRUE;
	}
	
	return FALSE;
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

	priv = chat->priv;
	
	/* Catch enter but not ctrl/shift-enter */
	if (IS_ENTER (event->keyval) && !(event->state & GDK_SHIFT_MASK)) {
		/* This is to make sure that kinput2 gets the enter. And if
                 * it's handled there we shouldn't send on it. This is because
                 * kinput2 uses Enter to commit letters. See:
                 * http://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=104299
                 */
		if (gtk_im_context_filter_keypress (GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->input_text_view)->im_context, event)) {
			GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->input_text_view)->need_im_reset = TRUE;
			return TRUE;
		}
		
		group_chat_input_text_view_send (chat);
		return TRUE;
	}
	
	if (IS_ENTER (event->keyval) && (event->state & GDK_SHIFT_MASK)) {
		/* Newline for shift-enter. */
		return FALSE;
	}
	else if (event->keyval == GDK_Page_Up) {
		adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->text_view_sw));
		gtk_adjustment_set_value (adj, adj->value - adj->page_size);

		return TRUE;
	}
	else if (event->keyval == GDK_Page_Down) {
		adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->text_view_sw));
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

static gint
group_chat_completion_compare (const gchar *s1, const gchar *s2, gsize n)
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

static gboolean
group_chat_find_user (GossipGroupChat *chat, GossipJID *jid, GtkTreeIter *iter)
{
	GossipGroupChatPriv *priv;
	FindUserData         data;
	GtkTreeModel        *model;

	priv = chat->priv;
	
	data.found = FALSE;
	data.jid = jid;
	data.chat = chat;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) group_chat_find_user_foreach,
				&data);

	if (data.found) {
		*iter = data.found_iter;
	}

	return data.found;
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
	GList               *list;

	priv = chat->priv;
	list = NULL;
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	
	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) group_chat_get_nick_list_foreach,
				&list);

	return list;
}

static gboolean
group_chat_focus_in_event_cb (GtkWidget       *widget,
			      GdkEvent        *event,
			      GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;

	priv = chat->priv;
	
	gtk_widget_grab_focus (GOSSIP_CHAT (chat)->input_text_view);

	return TRUE;
}

static LmHandlerResult
group_chat_message_handler (LmMessageHandler *handler,
			    LmConnection     *connection,
			    LmMessage        *m,
			    gpointer          user_data)
{
	GossipGroupChat     *chat;
	GossipGroupChatPriv *priv;
	GossipChatWindow    *window;
	const gchar         *from;
	LmMessageSubType     type;
	GtkWidget           *dialog;
	gchar               *tmp, *str, *msg;
	GossipJID           *jid;
	LmMessageNode       *node;
	const gchar         *timestamp;

	chat = GOSSIP_GROUP_CHAT (user_data);
	priv = chat->priv;
	
	from = lm_message_node_get_attribute (m->node, "from");
	type = lm_message_get_sub_type (m);

	jid  = gossip_jid_new (from);

	/* Check that the message is for this group chat. */
	if (!gossip_jid_equals_without_resource (jid, priv->jid)) {
		gossip_jid_unref (jid);
		
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}
	
	switch (type) {
	case LM_MESSAGE_SUB_TYPE_ERROR:
		tmp = g_strdup_printf ("<b>%s</b>", gossip_jid_get_part_name (jid));
		str = g_strdup_printf (_("An error occurred when chatting in the group chat %s."), tmp);
		g_free (tmp);
		
		node = lm_message_node_get_child (m->node, "error");
		if (node && node->value && node->value[0]) {
			msg = g_strconcat (str, "\n\n", _("Details:"), " ", node->value, NULL);
			g_free (str);
		} else {
			msg = str;
		}

		window = gossip_chat_get_window (GOSSIP_CHAT (chat));

		dialog = gtk_message_dialog_new (GTK_WINDOW (gossip_chat_window_get_dialog (window)),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 msg);

		gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
		
		g_object_set (GTK_MESSAGE_DIALOG (dialog)->label,
			      "use-markup", TRUE,
			      NULL);
		
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		g_free (msg);
		gossip_jid_unref (jid);

		return LM_HANDLER_RESULT_REMOVE_MESSAGE;

	case LM_MESSAGE_SUB_TYPE_CHAT:
		group_chat_priv_chat_incoming (chat, jid, m);
		break;

	case LM_MESSAGE_SUB_TYPE_GROUPCHAT:
		node = lm_message_node_get_child (m->node, "subject");
		if (node) {
			gtk_entry_set_text (GTK_ENTRY (priv->topic_entry),
					    node->value);
		}
		
		node = lm_message_node_get_child (m->node, "body");
		if (node) {
			timestamp = gossip_utils_get_timestamp_from_message (m);
		
			gossip_chat_view_append_chat_message (GOSSIP_CHAT (chat)->view,
							      timestamp,
							      priv->nick,
							      gossip_jid_get_resource (jid),
							      node->value);
		}
		
		break;
		
	default:
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	gossip_jid_unref (jid);

	if (!priv->inited) {
		priv->inited = TRUE;
		gossip_chat_present (GOSSIP_CHAT (chat));
	}
	
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult
group_chat_presence_handler (LmMessageHandler *handler,
			     LmConnection     *connection,
			     LmMessage        *m,
			     gpointer          user_data)
{
	GossipGroupChat     *chat;
	GossipGroupChatPriv *priv;
	GossipChatWindow    *window;
	const gchar         *from;
	GossipJID           *jid;
	GtkTreeModel        *model;
	LmMessageSubType     type;
	gchar               *tmp, *str, *msg;
	GtkWidget           *dialog;
	GdkPixbuf           *pixbuf;
	GtkTreeIter          iter;
	LmMessageNode       *node;
	GossipContact       *contact;
	GossipPresence      *presence;
	GossipPresenceType   p_type;
	gchar               *name;
	
	chat = GOSSIP_GROUP_CHAT (user_data);
	priv = chat->priv;

	from = lm_message_node_get_attribute (m->node, "from");
	jid = gossip_jid_new (from);

	/* Check that the presence is for this group chat. */
	if (!gossip_jid_equals_without_resource (jid, priv->jid)) {
		gossip_jid_unref (jid);

		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	
 	type = lm_message_get_sub_type (m);
	switch (type) {
	case LM_MESSAGE_SUB_TYPE_ERROR:
		tmp = g_strdup_printf ("<b>%s</b>", gossip_jid_get_part_name (jid));
		str = g_strdup_printf (_("Unable to enter the group chat %s."), tmp);
		g_free (tmp);
		
		node = lm_message_node_get_child (m->node, "error");
		if (node && node->value && node->value[0]) {
			msg = g_strconcat (str, "\n\n", _("Details:"), " ", node->value, NULL);
			g_free (str);
		} else {
			msg = str;
		}

		window = gossip_chat_get_window (GOSSIP_CHAT (chat));

		dialog = gtk_message_dialog_new (GTK_WINDOW (gossip_chat_window_get_dialog (window)),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 msg);

		gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
		
		g_object_set (GTK_MESSAGE_DIALOG (dialog)->label,
			      "use-markup", TRUE,
			      NULL);
		
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		g_free (msg);

		gossip_jid_unref (jid);
		gossip_chat_window_remove_chat (window, GOSSIP_CHAT (chat));

		return LM_HANDLER_RESULT_REMOVE_MESSAGE;

	case LM_MESSAGE_SUB_TYPE_AVAILABLE:
		name = group_chat_create_contact_name (jid);
		
		contact = gossip_contact_new_full (GOSSIP_CONTACT_TYPE_GROUPCHAT,
						   jid, name);
		g_free (name);

		node = lm_message_node_get_child (m->node, "show");
		if (node) {
			p_type = gossip_utils_get_presence_type_from_show_string (node->value);
		} else {
			p_type = gossip_utils_get_presence_type_from_show_string (NULL);
		}
		presence = gossip_presence_new (GOSSIP_PRESENCE_STATE_ONLINE);
		gossip_presence_set_type (presence, p_type);

		pixbuf = gossip_presence_get_pixbuf (presence);
		/*gossip_utils_gtk_widget_render_icon (priv->window,
						 stock,
						 GTK_ICON_SIZE_MENU,
						 NULL); */
		gossip_contact_set_presence (contact, presence);
		gossip_presence_unref (presence);
		
		if (group_chat_find_user (chat, jid, &iter)) {
			gtk_list_store_set (GTK_LIST_STORE (model),
					    &iter,
					    COL_STATUS, pixbuf,
					    -1);
			g_signal_emit_by_name (chat, "contact_presence_updated",
					       contact);
		} else {
			gtk_list_store_append (GTK_LIST_STORE (model), &iter);
			gtk_list_store_set (GTK_LIST_STORE (model),
					    &iter,
					    COL_JID, gossip_jid_ref (jid),
					    COL_NAME, gossip_jid_get_resource (jid),
					    COL_STATUS, pixbuf,
					    -1);
			g_signal_emit_by_name (chat, "contact_added", contact);
		}

		gossip_contact_unref (contact);
		g_object_unref (pixbuf);
		break;

	case LM_MESSAGE_SUB_TYPE_UNAVAILABLE:
		if (group_chat_find_user (chat, jid, &iter)) {
			GossipContact *contact;
			
			gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

			contact = gossip_contact_new_full (GOSSIP_CONTACT_TYPE_GROUPCHAT,
							   jid, NULL);
			g_signal_emit_by_name (chat, 
					       "contact_removed", contact);
			gossip_contact_unref (contact);
		}
		break;
		
	default:
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}		

	gossip_jid_unref (jid);

	if (!priv->inited) {
		priv->inited = TRUE;
		gossip_chat_present (GOSSIP_CHAT (chat));
	}

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
group_chat_topic_activate_cb (GtkEntry *entry, GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	LmMessage           *m;
	gchar               *str;
	const gchar         *without_resource;
	const gchar         *topic;
	LmConnection        *connection;
	
	priv = chat->priv;
	
	str = g_strdup_printf ("/me changed the topic to: %s", 
			       gtk_entry_get_text (entry));

	without_resource = gossip_jid_get_without_resource (priv->jid);
	
	m = lm_message_new_with_sub_type (without_resource,
					  LM_MESSAGE_TYPE_MESSAGE,
					  LM_MESSAGE_SUB_TYPE_GROUPCHAT);
	lm_message_node_add_child (m->node, "body", str);
	g_free (str);

	topic = gtk_entry_get_text (entry);
	lm_message_node_add_child (m->node, "subject", topic);

	connection = gossip_app_get_connection ();
	lm_connection_send (connection, m, NULL);
	lm_message_unref (m);
	
	gtk_widget_grab_focus (GOSSIP_CHAT (chat)->input_text_view);
}

static gint
group_chat_iter_compare_func (GtkTreeModel *model,
			      GtkTreeIter  *iter_a,
			      GtkTreeIter  *iter_b,
			      gpointer      user_data)
{
	gchar *name_a, *name_b;
	gint   ret_val;
	
	gtk_tree_model_get (model, iter_a, COL_NAME, &name_a, -1);
	gtk_tree_model_get (model, iter_b, COL_NAME, &name_b, -1);
	
	ret_val = g_ascii_strcasecmp (name_a, name_b);

	g_free (name_a);
	g_free (name_b);

	return ret_val;
}

static GossipPrivateChat *
group_chat_priv_chat_new (GossipGroupChat *chat, GossipJID *jid)
{
	GossipGroupChatPriv *priv;
	GossipPrivateChat   *priv_chat = NULL;
	GList               *l;
	GossipContact       *contact;
	gchar               *name;

	priv = chat->priv;
	
	for (l = priv->priv_chats; l; l = l->next) {
		GossipPrivateChat *p_chat = GOSSIP_PRIVATE_CHAT (l->data);
		GossipContact     *contact;
		GossipJID         *j;
		
		contact = gossip_chat_get_contact (GOSSIP_CHAT (p_chat));
		j = gossip_contact_get_jid (contact);
		
		if (gossip_jid_equals (j, jid)) {
			priv_chat = p_chat;
			break;
		}
	}
	
	if (priv_chat) {
		return priv_chat;
	}

	name = group_chat_create_contact_name (jid);
	
	contact = gossip_contact_new_full (GOSSIP_CONTACT_TYPE_GROUPCHAT,
					   jid, name);
	g_free (name);

	priv_chat = gossip_private_chat_get_for_group_chat (contact, chat);
	gossip_contact_unref (contact);

	priv->priv_chats = g_list_prepend (priv->priv_chats, priv_chat);
	
	g_object_weak_ref (G_OBJECT (priv_chat),
			   (GWeakNotify) group_chat_priv_chat_removed,
			   chat);

	gossip_chat_present (GOSSIP_CHAT (priv_chat));

	g_object_unref (priv_chat);
	
	return priv_chat;
}

static void
group_chat_priv_chat_incoming (GossipGroupChat *chat,
			       GossipJID       *jid,
			       LmMessage       *m)
{
	GossipPrivateChat  *priv_chat;

	priv_chat = group_chat_priv_chat_new (chat, jid);

	gossip_private_chat_append_message (priv_chat, m);
}

static void
group_chat_priv_chat_removed (GossipGroupChat *chat, GossipChat *priv_chat)
{
	GossipGroupChatPriv *priv;

	priv = chat->priv;
	
	priv->priv_chats = g_list_remove (priv->priv_chats, priv_chat);
}

static void
group_chat_input_text_view_send (GossipGroupChat *chat)
{
	GossipGroupChatPriv *priv;
	GtkTextBuffer       *buffer;
	GtkTextIter          start, end;
	gchar		    *msg;

	priv = chat->priv;
	
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (GOSSIP_CHAT (chat)->input_text_view));

	gtk_text_buffer_get_bounds (buffer, &start, &end);
	msg = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
	
	/* Clear the input field. */
	gtk_text_buffer_set_text (buffer, "", -1);

	group_chat_send (chat, msg);

	g_free (msg);

	GOSSIP_CHAT (chat)->is_first_char = TRUE;
}

static void
set_status_foreach (gpointer key,
		    gpointer value,
		    gpointer data)
{
	LmMessage    *m = data;
	LmConnection *connection;

	connection = gossip_app_get_connection ();
	
	lm_message_node_set_attributes (m->node, "to", key, NULL);
	lm_connection_send (connection, m, NULL);
}

void
gossip_group_chat_set_show (GossipShow show)
{
	LmMessage   *m;
	const gchar *show_str;

	group_chats_init ();
	
	m = lm_message_new_with_sub_type (NULL, 
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_AVAILABLE);
	
	show_str = gossip_utils_show_to_string (show);
	if (show) {
		lm_message_node_add_child (m->node, "show", show_str);
	}

	/* FIXME set status string, lm_message_node_add_child (m->node, "status", "Online");*/
	g_hash_table_foreach (group_chats, set_status_foreach, m);

	lm_message_unref (m);
}

static void
group_chat_priv_chats_disconnect (GossipGroupChat *chat) 
{
	GossipGroupChatPriv *priv;
	GList               *l;

	priv = chat->priv;

	for (l = priv->priv_chats; l; l = l->next) {
		GossipChat *priv_chat = GOSSIP_CHAT (l->data);
		
		gtk_widget_set_sensitive (gossip_chat_get_widget (priv_chat),
					  FALSE);
		g_object_weak_unref (G_OBJECT(priv_chat),
				     (GWeakNotify) group_chat_priv_chat_removed,
				     chat);
	}
}	

static gchar *
group_chat_create_contact_name (GossipJID *jid)
{
	gchar *g_chat_name;
	gchar *name;

	g_chat_name = gossip_jid_get_part_name (jid);
	
	name = g_strdup_printf ("%s@%s", gossip_jid_get_resource (jid),
				g_chat_name);
	
	g_free (g_chat_name);

	return name;
}

static GtkWidget *
group_chat_get_widget (GossipChat *chat)
{
	GossipGroupChat     *g_chat;
	GossipGroupChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), NULL);

	g_chat = GOSSIP_GROUP_CHAT (chat);
	priv   = g_chat->priv;

	return priv->widget;
}

static const gchar *
group_chat_get_name (GossipChat *chat)
{
	GossipGroupChat     *g_chat;
	GossipGroupChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), NULL);

	g_chat = GOSSIP_GROUP_CHAT (chat);
	priv   = g_chat->priv;

	return priv->name;
}

static gchar *
group_chat_get_tooltip (GossipChat *chat)
{
	GossipGroupChat     *g_chat;
	GossipGroupChatPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_GROUP_CHAT (chat), NULL);

	g_chat = GOSSIP_GROUP_CHAT (chat);
	priv   = g_chat->priv;

	return g_strdup (gossip_jid_get_without_resource (priv->jid));
}

static GdkPixbuf *
group_chat_get_status_pixbuf (GossipChat *chat)
{
	static GdkPixbuf    *pixbuf = NULL;

	if (pixbuf == NULL) {
		/* FIXME: need a better icon than this */
		pixbuf = gdk_pixbuf_new_from_file (IMAGEDIR "/gossip-group-message.png", NULL);
	}

	return pixbuf;
}

static GossipContact *
group_chat_get_contact (GossipChat *chat)
{
	return NULL;
}

static void
group_chat_get_geometry (GossipChat *chat,
		         gint       *width,
		 	 gint       *height)
{
	*width  = 600;
	*height = 400;
}
