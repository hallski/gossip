/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003      Imendio HB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
 * Copyright (C) 2002-2003 Mikael Hallendal <micke@imendio.com>
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
#include "gossip-utils.h"
#include "gossip-app.h"
#include "gossip-chat.h"
#include "disclosure-widget.h"
#include "gossip-chat-view.h"
#include "gossip-group-chat.h"

/* Treeview columns */
enum {
	COL_STATUS,
	COL_NAME,
	COL_JID,
	NUMBER_OF_COLS
};

struct _GossipGroupChat {
	gboolean          inited;
	
	LmMessageHandler *message_handler;
	LmMessageHandler *presence_handler;
	
	GtkWidget        *window;
	GtkWidget        *text_view_sw;
	GtkWidget        *input_entry;
	GtkWidget        *input_text_view;
	GtkWidget        *topic_entry;
	GtkWidget        *tree;
	GtkWidget        *disclosure;
	GtkWidget        *single_hbox;
	GtkWidget        *multi_vbox;
	GtkWidget        *send_multi_button;

	GossipChatView   *view;
	
	GossipJID        *jid;
	gchar            *nick;

	GCompletion      *completion;

	GTimeVal          last_timestamp;
	GHashTable       *priv_chats;
};

typedef struct {
	GossipGroupChat *chat;
	GossipJID       *jid;
	gboolean         found;
	GtkTreeIter      found_iter;
} FindUserData;

static void             destroy_group_chat                      (GossipGroupChat   *chat);
static void             group_chat_window_destroy_cb            (GtkWidget         *widget,
								 GossipGroupChat   *chat);
static void             group_chat_init                         (void);
static GossipGroupChat *group_chat_get_for_jid                  (GossipJID         *jid);
static void             group_chat_create_gui                   (GossipGroupChat   *chat);
GtkWidget *             group_chat_create_disclosure            (gpointer           data);
static void             group_chat_disclosure_toggled_cb        (GtkToggleButton   *disclosure,
								 GossipGroupChat   *chat);
static void             group_chat_input_text_buffer_changed_cb (GtkTextBuffer     *buffer,
								 GossipGroupChat   *chat);
static void             group_chat_send                         (GossipGroupChat   *chat,
								 const gchar       *msg);
static void             group_chat_activate_cb                  (GtkWidget         *entry,
								 GossipGroupChat   *chat);
static void             group_chat_row_activated_cb             (GtkTreeView       *view,
								 GtkTreePath       *path,
								 GtkTreeViewColumn *col,
								 GossipGroupChat   *chat);
static void             group_chat_setup_tree                   (GossipGroupChat   *chat);
static gint             group_chat_get_start_of_word            (GtkEditable       *editable);
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
static GossipChat *     group_chat_priv_chat_new                (GossipGroupChat   *chat,
								 GossipJID         *jid);
static void             group_chat_priv_chat_incoming           (GossipGroupChat   *chat,
								 GossipJID         *jid,
								 LmMessage         *m);
static void             group_chat_priv_chat_destroyed          (GtkWidget         *dialog,
								 GossipGroupChat   *chat);
static void             group_chat_send_multi_clicked_cb        (GtkWidget         *unused,
								 GossipGroupChat   *chat);
static void             group_chat_priv_chats_disconnect        (GossipGroupChat   *chat);


static GHashTable *group_chats = NULL;


static void
destroy_group_chat (GossipGroupChat *chat)
{
	LmConnection     *connection;
	LmMessageHandler *handler;
	LmMessage        *m;
	const gchar      *without_resource;

	group_chat_priv_chats_disconnect (chat);
	
	connection = gossip_app_get_connection ();

	handler = chat->message_handler;
	lm_connection_unregister_message_handler (connection,
						  handler,
						  LM_MESSAGE_TYPE_MESSAGE);

	handler = chat->presence_handler;
	lm_connection_unregister_message_handler (connection,
						  handler,
						  LM_MESSAGE_TYPE_PRESENCE);

	without_resource = gossip_jid_get_without_resource (chat->jid); 
	m = lm_message_new_with_sub_type (without_resource,
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_UNAVAILABLE);
	lm_connection_send (connection, m, NULL);
	lm_message_unref (m);

	gossip_jid_unref (chat->jid);
	g_free (chat->nick);
	g_free (chat);

	g_hash_table_destroy (chat->priv_chats);
}

static void
group_chat_window_destroy_cb (GtkWidget *widget, GossipGroupChat *chat)
{
	g_hash_table_remove (group_chats,
			     gossip_jid_get_without_resource (chat->jid));
}

static void
group_chat_init (void)
{
	static gboolean inited = FALSE;

	if (inited) {
		return;
	}
	
	inited = TRUE;
	
	group_chats = g_hash_table_new_full (g_str_hash,
					     g_str_equal,
					     NULL,
					     (GDestroyNotify) destroy_group_chat);
}

static GossipGroupChat *
group_chat_get_for_jid (GossipJID *jid)
{
	GossipGroupChat  *chat;
	const gchar      *without_resource;
	LmMessageHandler *handler;
	LmConnection     *connection;
	
	group_chat_init ();

	without_resource = gossip_jid_get_without_resource (jid);
	chat = g_hash_table_lookup (group_chats, without_resource);
	if (chat) {
		return chat;
	}

	chat = g_new0 (GossipGroupChat, 1);
	
	chat->jid = gossip_jid_ref (jid);
	chat->inited = FALSE;
	chat->last_timestamp.tv_sec = chat->last_timestamp.tv_usec = 0;

	chat->priv_chats = g_hash_table_new_full (g_str_hash, g_str_equal,
						  g_free, NULL);
	
	group_chat_create_gui (chat);

	g_hash_table_insert (group_chats, (gchar *)without_resource, chat);

	connection = gossip_app_get_connection ();
	
	handler = lm_message_handler_new (group_chat_message_handler, chat, NULL);
	chat->message_handler = handler;
	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_MESSAGE,
						LM_HANDLER_PRIORITY_NORMAL);

	handler = lm_message_handler_new (group_chat_presence_handler, chat, NULL);
	chat->presence_handler = handler;
	lm_connection_register_message_handler (connection,
						handler,
						LM_MESSAGE_TYPE_PRESENCE,
						LM_HANDLER_PRIORITY_NORMAL);

	return chat;
}

static void
group_chat_create_gui (GossipGroupChat *chat)
{
	GladeXML      *glade;
	GtkWidget     *focus_vbox;
	GtkTextBuffer *buffer;
	GList         *list;
	gchar         *room;
	gchar         *str;

	glade = gossip_glade_get_file (GLADEDIR "/group-chat.glade",
				       "group_chat_window",
				       NULL,
				       "group_chat_window", &chat->window,
				       "chat_view_sw", &chat->text_view_sw,
				       "input_entry", &chat->input_entry,
				       "input_textview", &chat->input_text_view,
				       "topic_entry", &chat->topic_entry,
				       "treeview", &chat->tree,
				       "left_vbox", &focus_vbox,
				       "disclosure", &chat->disclosure,
				       "single_hbox", &chat->single_hbox,
				       "multi_vbox", &chat->multi_vbox,
				       "send_multi_button", &chat->send_multi_button,
				       NULL);
	
	gossip_glade_connect (glade,
			      chat,
			      "group_chat_window", "destroy", group_chat_window_destroy_cb,
			      "input_entry", "activate", group_chat_activate_cb,
			      "input_entry", "key_press_event", group_chat_key_press_event_cb,
			      "input_textview", "key_press_event", group_chat_key_press_event_cb,
			      "topic_entry", "activate", group_chat_topic_activate_cb,
			      "disclosure", "toggled", group_chat_disclosure_toggled_cb,
			      "send_multi_button", "clicked", group_chat_send_multi_clicked_cb,
			      NULL);

	chat->view = gossip_chat_view_new ();
	gtk_container_add (GTK_CONTAINER (chat->text_view_sw), 
			   GTK_WIDGET (chat->view));
	gtk_widget_show (GTK_WIDGET (chat->view));

	g_signal_connect (chat->view,
			  "focus_in_event",
			  G_CALLBACK (group_chat_focus_in_event_cb),
			  chat);
			  
	g_object_unref (glade);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	g_signal_connect (buffer,
			  "changed",
			  G_CALLBACK (group_chat_input_text_buffer_changed_cb),
			  chat);
	
	list = NULL;
	list = g_list_append (list, chat->topic_entry);
	list = g_list_append (list, chat->input_entry);
	
	gtk_container_set_focus_chain (GTK_CONTAINER (focus_vbox), list);
	
	room = gossip_jid_get_part_name (chat->jid);
	str = g_strconcat ("Gossip - ", room, NULL);
	gtk_window_set_title (GTK_WINDOW (chat->window), str);
	g_free (str);
	g_free (room);

	chat->completion = g_completion_new (NULL);
	g_completion_set_compare (chat->completion,
				  group_chat_completion_compare);
		
	gtk_widget_grab_focus (chat->input_entry);
	group_chat_setup_tree (chat);

	gossip_chat_view_set_margin (chat->view, 3);
}

GossipGroupChat *
gossip_group_chat_new (GossipJID   *jid,
		       const gchar *nick)
{
	GossipGroupChat *chat;

	chat = group_chat_get_for_jid (jid);
	chat->nick = g_strdup (nick);

	return chat;
}

GtkWidget *
group_chat_create_disclosure (gpointer data)
{
	GtkWidget *widget;
	
	widget = cddb_disclosure_new (NULL, NULL);

	gtk_widget_show (widget);

	return widget;
}

static void
group_chat_disclosure_toggled_cb (GtkToggleButton *disclosure,
				  GossipGroupChat *chat)
{
	GtkTextBuffer *buffer;
	GtkTextIter    start, end;
	const gchar   *const_str; 
	gchar         *str;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	
	if (gtk_toggle_button_get_active (disclosure)) {
		gtk_widget_show (chat->multi_vbox);
		gtk_widget_hide (chat->single_hbox);

		const_str = gtk_entry_get_text (GTK_ENTRY (chat->input_entry));
		gtk_text_buffer_set_text (buffer, const_str, -1);
	} else {
		gtk_widget_show (chat->single_hbox);
		gtk_widget_hide (chat->multi_vbox);

		gtk_text_buffer_get_bounds (buffer, &start, &end);
		str = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
		gtk_entry_set_text (GTK_ENTRY (chat->input_entry), str);
		g_free (str);
	}
}

static void
group_chat_input_text_buffer_changed_cb (GtkTextBuffer   *buffer,
					 GossipGroupChat *chat)
{
	if (gtk_text_buffer_get_line_count (buffer) > 1) {
		gtk_widget_set_sensitive (chat->disclosure, FALSE);
	} else {
		gtk_widget_set_sensitive (chat->disclosure, TRUE);
	}
}

static void
group_chat_send (GossipGroupChat *chat,
		 const gchar     *msg)
{
	LmConnection *connection;
	LmMessage    *m;
	const char   *without_resource;

	connection = gossip_app_get_connection ();

	without_resource = gossip_jid_get_without_resource (chat->jid);
	
	if (g_ascii_strncasecmp (msg, "/nick ", 6) == 0 && strlen (msg) > 6) {
		gchar *to;

		g_free (chat->nick);
		chat->nick = g_strdup (msg + 6);
	
		to = g_strdup_printf ("%s/%s",
				      without_resource,
				      chat->nick);
		
		m = lm_message_new (to, LM_MESSAGE_TYPE_PRESENCE);

		lm_connection_send (connection, m, NULL);
		lm_message_unref (m);
		g_free (to);
		
		return;
	}
	else if (g_ascii_strcasecmp (msg, "/clear") == 0) {
		GtkTextBuffer *buffer;

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->view));
		gtk_text_buffer_set_text (buffer, "", -1);
		
		return;
	}
	
	m = lm_message_new_with_sub_type (without_resource,
					  LM_MESSAGE_TYPE_MESSAGE,
					  LM_MESSAGE_SUB_TYPE_GROUPCHAT);
	
	lm_message_node_add_child (m->node, "body", msg);

	lm_connection_send (connection, m, NULL);
	lm_message_unref (m);
}

static void
group_chat_activate_cb (GtkWidget *entry, GossipGroupChat *chat)
{
	const gchar *msg;
	
	msg = gtk_entry_get_text (GTK_ENTRY (entry));
	
	group_chat_send (chat, msg);

	/* Clear the input field. */
	gtk_entry_set_text (GTK_ENTRY (chat->input_entry), "");
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
	GtkTreeView       *tree;
	GtkTextView       *tv;
	GtkListStore      *store;
	GtkCellRenderer   *cell;	
	GtkTreeViewColumn *col;

	tree = GTK_TREE_VIEW (chat->tree);
	tv = GTK_TEXT_VIEW (chat->view);

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

static gint
group_chat_get_start_of_word (GtkEditable *editable)
{
	gint      pos, start = 0;
	gchar    *tmp, *p;
	gunichar  c;
	
	pos = gtk_editable_get_position (editable);

	tmp = gtk_editable_get_chars (editable, 0, pos);

	p = tmp + strlen (tmp);
	while (p) {
		c = g_utf8_get_char (p);

		if (g_unichar_isspace (c) || g_unichar_ispunct (c)) {
			p = g_utf8_find_next_char (p, tmp);
			if (p) {
				start = g_utf8_pointer_to_offset (tmp, p);
			} else {
				start = 0;
			}
			break;
		}
		
		p = g_utf8_prev_char (p);
	}

	g_free (tmp);
	
	return start;
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
	gint   start_pos, pos;
	gchar *nick, *completed;
	gint   len;
	GList *list, *l, *completed_list;
	
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chat->disclosure))) {
		/* Multi line entry. */
		if ((event->state & GDK_CONTROL_MASK) &&
		    (event->keyval == GDK_Return ||
		     event->keyval == GDK_ISO_Enter ||
		     event->keyval == GDK_KP_Enter)) {
			gtk_widget_activate (chat->send_multi_button);
			return TRUE;
		}

		return FALSE;
	}

	if ((event->state & GDK_CONTROL_MASK) != GDK_CONTROL_MASK &&
	    (event->state & GDK_SHIFT_MASK) != GDK_SHIFT_MASK &&
	    event->keyval == GDK_Tab) {
		/* Get the start of the nick to complete. */
		start_pos = group_chat_get_start_of_word (GTK_EDITABLE (chat->input_entry));
		pos = gtk_editable_get_position (GTK_EDITABLE (chat->input_entry));

		nick = gtk_editable_get_chars (GTK_EDITABLE (chat->input_entry), start_pos, pos);

		g_completion_clear_items (chat->completion);
		
		len = strlen (nick);
		
		list = group_chat_get_nick_list (chat);

		g_completion_add_items (chat->completion, list);

		completed_list = g_completion_complete (chat->completion,
							nick,
							&completed);

		g_free (nick);

		if (completed) {
			gtk_editable_delete_text (GTK_EDITABLE (chat->input_entry),
						  start_pos,
						  pos);
			
			pos = start_pos;
				
			gtk_editable_insert_text (GTK_EDITABLE (chat->input_entry),
						  completed,
						  -1,
						  &pos);

			if (g_list_length (completed_list) == 1) {
				if (start_pos == 0) {
					gtk_editable_insert_text (GTK_EDITABLE (chat->input_entry),
								  ", ",
								  2,
								  &pos);
				}
			}
			
			gtk_editable_set_position (GTK_EDITABLE (chat->input_entry),
						   pos);
			
			g_free (completed);
		}

		g_completion_clear_items (chat->completion);

		for (l = list; l; l = l->next) {
			g_free (l->data);
		}
		
		g_list_free (list);
		
		return TRUE;
	}

	return FALSE;
}

static gint
group_chat_completion_compare (const gchar *s1,
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

static gboolean
group_chat_find_user (GossipGroupChat *chat,
		      GossipJID       *jid,
		      GtkTreeIter     *iter)
{
	FindUserData  data;
	GtkTreeModel *model;

	data.found = FALSE;
	data.jid = jid;
	data.chat = chat;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (chat->tree));

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
	GtkTreeModel *model;
	GList        *list;
	
	list = NULL;
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (chat->tree));
	
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
	gint pos;

	pos = gtk_editable_get_position (GTK_EDITABLE (chat->input_entry));

	gtk_widget_grab_focus (chat->input_entry);

	gtk_editable_select_region (GTK_EDITABLE (chat->input_entry), 0, 0);
	gtk_editable_set_position (GTK_EDITABLE (chat->input_entry), pos);
	
	return TRUE;
}

static LmHandlerResult
group_chat_message_handler (LmMessageHandler *handler,
			    LmConnection     *connection,
			    LmMessage        *m,
			    gpointer          user_data)
{
	GossipGroupChat  *chat = user_data;
	const gchar      *from;
	LmMessageSubType  type;
	GtkWidget        *dialog;
	gchar            *tmp, *str, *msg;
	GossipJID        *jid;
	LmMessageNode    *node;
	const gchar      *timestamp;

	from = lm_message_node_get_attribute (m->node, "from");
	type = lm_message_get_sub_type (m);

	jid  = gossip_jid_new (from);

	/* Check that the message is for this group chat. */
	if (!gossip_jid_equals_without_resource (jid, chat->jid)) {
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

		dialog = gtk_message_dialog_new (GTK_WINDOW (chat->window),
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
			gtk_entry_set_text (GTK_ENTRY (chat->topic_entry),
					    node->value);
		}
		
		node = lm_message_node_get_child (m->node, "body");
		if (node) {
			timestamp = gossip_utils_get_timestamp_from_message (m);
		
			gossip_chat_view_append_chat_message (chat->view,
							      timestamp,
							      chat->nick,
							      gossip_jid_get_resource (jid),
							      node->value);
		}
		
		break;
		
	default:
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	gossip_jid_unref (jid);

	if (!chat->inited) {
		chat->inited = TRUE;
		gtk_widget_show (chat->window);
	}
	
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult
group_chat_presence_handler (LmMessageHandler *handler,
			     LmConnection     *connection,
			     LmMessage        *m,
			     gpointer          user_data)
{
	GossipGroupChat  *chat = user_data;
	const gchar      *from;
	GossipJID        *jid;
	GtkTreeModel     *model;
	LmMessageSubType  type;
	gchar            *tmp, *str, *msg;
	GtkWidget        *dialog;
	GdkPixbuf        *pixbuf;
	GtkTreeIter       iter;
	LmMessageNode    *node;
	const gchar      *stock;

	from = lm_message_node_get_attribute (m->node, "from");
	jid = gossip_jid_new (from);

	/* Check that the presence is for this group chat. */
	if (!gossip_jid_equals_without_resource (jid, chat->jid)) {
		gossip_jid_unref (jid);

		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (chat->tree));
	
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

		dialog = gtk_message_dialog_new (GTK_WINDOW (chat->window),
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
		gtk_widget_destroy (chat->window);

		return LM_HANDLER_RESULT_REMOVE_MESSAGE;

	case LM_MESSAGE_SUB_TYPE_AVAILABLE:
		node = lm_message_node_get_child (m->node, "show");
		if (node) {
			stock = gossip_get_icon_for_show_string (node->value);
		} else {
			stock = gossip_get_icon_for_show_string (NULL);
		}
		
		pixbuf = gtk_widget_render_icon (chat->window,
						 stock,
						 GTK_ICON_SIZE_MENU,
						 NULL);
		
		if (group_chat_find_user (chat, jid, &iter)) {
			gtk_list_store_set (GTK_LIST_STORE (model),
					    &iter,
					    COL_STATUS, pixbuf,
					    -1);
		} else {
			gtk_list_store_append (GTK_LIST_STORE (model), &iter);
			gtk_list_store_set (GTK_LIST_STORE (model),
					    &iter,
					    COL_JID, gossip_jid_ref (jid),
					    COL_NAME, gossip_jid_get_resource (jid),
					    COL_STATUS, pixbuf,
					    -1);
		}
		
		g_object_unref (pixbuf);
		break;

	case LM_MESSAGE_SUB_TYPE_UNAVAILABLE:
		if (group_chat_find_user (chat, jid, &iter)) {
			gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		}
		break;
		
	default:
		gossip_jid_unref (jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}		

	gossip_jid_unref (jid);

	if (!chat->inited) {
		chat->inited = TRUE;
		gtk_widget_show (chat->window);
	}

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
group_chat_topic_activate_cb (GtkEntry *entry, GossipGroupChat *chat)
{
	LmMessage    *m;
	gchar        *str;
	const gchar  *without_resource;
	const gchar  *topic;
	LmConnection *connection;
	
	str = g_strdup_printf ("/me changed the topic to: %s", 
			       gtk_entry_get_text (entry));

	without_resource = gossip_jid_get_without_resource (chat->jid);
	
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
	
	gtk_widget_grab_focus (chat->input_entry);
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

static GossipChat *
group_chat_priv_chat_new (GossipGroupChat *chat, 
			  GossipJID       *jid)
{
	GossipChat  *priv_chat;
	const gchar *nick;
	gchar       *key;
	GtkWidget   *dialog;

	nick = gossip_jid_get_resource (jid);
	
	priv_chat = g_hash_table_lookup (chat->priv_chats, nick);
	if (priv_chat) {
		return priv_chat;
	}
	
	priv_chat = gossip_chat_get_for_group_chat (jid);

	key = g_strdup (nick);
	g_hash_table_insert (chat->priv_chats, key, priv_chat);

	dialog = gossip_chat_get_dialog (priv_chat);
	
	g_object_set_data (G_OBJECT (dialog), "key", key);

	g_signal_connect (dialog,
			  "destroy", 
			  G_CALLBACK (group_chat_priv_chat_destroyed),
			  chat);

	return priv_chat;
}

static void
group_chat_priv_chat_incoming (GossipGroupChat *chat,
			       GossipJID       *jid,
			       LmMessage       *m)
{
	GossipChat  *priv_chat;
	const gchar *nick;
	const gchar *from;

	nick = gossip_jid_get_resource (jid);
	priv_chat = g_hash_table_lookup (chat->priv_chats, nick);
	
	if (!priv_chat) {
		GossipJID *jid;

		from = lm_message_node_get_attribute (m->node, "from");
		jid = gossip_jid_new (from);

		priv_chat = group_chat_priv_chat_new (chat, jid);

		gossip_jid_unref (jid);
	}
	
	gossip_chat_append_message (priv_chat, m);
}

static void
group_chat_priv_chat_destroyed (GtkWidget *dialog, GossipGroupChat *chat)
{
	gchar *key;
	
	key = (gchar *) g_object_get_data (G_OBJECT (dialog), "key");
	
	g_hash_table_remove (chat->priv_chats, key);
}

static void
group_chat_send_multi_clicked_cb (GtkWidget       *unused,
				  GossipGroupChat *chat)
{
	GtkTextBuffer *buffer;
	GtkTextIter    start, end;
	gchar         *msg;
	
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	
	gtk_text_buffer_get_bounds (buffer, &start, &end);
	msg = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
	
	/* Clear the input field. */
	gtk_text_buffer_set_text (buffer, "", -1);

	group_chat_send (chat, msg);

	g_free (msg);
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

	group_chat_init ();
	
	m = lm_message_new_with_sub_type (NULL, 
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_AVAILABLE);
	
	show_str = gossip_show_to_string (show);
	if (show) {
		lm_message_node_add_child (m->node, "show", show_str);
	}

	/* FIXME set status string, lm_message_node_add_child (m->node, "status", "Online");*/
	g_hash_table_foreach (group_chats, set_status_foreach, m);

	lm_message_unref (m);
}

static void
disconnect_foreach (gpointer key,
		    gpointer value,
		    gpointer data)
{
	GossipGroupChat *chat;
	GossipChat      *priv_chat;
	GtkWidget       *dialog;

	chat = data;
	priv_chat = value;
	
	dialog = gossip_chat_get_dialog (value);

	/* FIXME: This should ideally just make the entries unsensitive. */
	gtk_widget_set_sensitive (dialog, FALSE);

	g_signal_handlers_disconnect_by_func (dialog,
					      group_chat_priv_chat_destroyed,
					      data);
}

static void
group_chat_priv_chats_disconnect (GossipGroupChat *chat) 
{
	g_hash_table_foreach (chat->priv_chats,
			      disconnect_foreach,
			      chat);
}
