/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003      Imendio HB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
 * Copyright (C) 2002-2003 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2002-2003 CodeFactory AB
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
#include <libgnome/gnome-i18n.h>
#include <loudmouth/loudmouth.h>
#include "gossip-contact-info.h"
#include "gossip-marshal.h"
#include "gossip-utils.h"
#include "gossip-stock.h"
#include "gossip-sound.h"
#include "gossip-roster-old.h"

#define d(x)

typedef enum {
        GOSSIP_STATUS_AVAILABLE,
        GOSSIP_STATUS_FREE,
        GOSSIP_STATUS_AWAY,
        GOSSIP_STATUS_EXT_AWAY,
        GOSSIP_STATUS_BUSY,
        GOSSIP_STATUS_OFFLINE
} GossipStatus;

typedef struct {
	GossipJID    *jid;
	gchar        *name;
	gchar        *group;
	gboolean      is_group;
	GossipStatus  status;
	gchar        *subscription;
	gchar        *ask;
	gchar        *status_str;
	guint         flash_timeout_id;
	gboolean      flash_on;
} GossipRosterOldItem;

struct _GossipRosterOldPriv {
	GossipApp        *app;
	LmConnection     *connection;
	LmMessageHandler *presence_handler;
	LmMessageHandler *iq_handler;

	GHashTable       *contacts;
	GList            *groups;

	GtkItemFactory   *popup_factory;

	gboolean          sound_enabled;
	guint             enable_sound_timeout_id;
};

typedef struct {
	GossipRosterOld *roster;
	GossipJID    *jid;
	gboolean      found;
	GtkTreeIter   found_iter;
} FindUserData;

typedef struct {
	GossipRosterOld *roster;
	gchar        *group;
	gboolean      found;
	GtkTreeIter   found_iter;
} FindGroupData;

/* Treeview columns */
enum {
	COL_ITEM,
	NUMBER_OF_COLS
};

/* Signals */
enum {
	USER_ACTIVATED,
	LAST_SIGNAL
};

enum {
	MENU_NONE,
	MENU_REMOVE,
	MENU_INFO,
	MENU_RENAME
};

/* The extra space is to differentiate the string from Offline as status. It
 * might need a different translation, at least in Swedish.
 */
#define OFFLINE_GROUP_N N_("Offline ")
#define OTHERS_GROUP_N N_("Others")

#define OFFLINE_GROUP _(OFFLINE_GROUP_N)
#define OTHERS_GROUP _(OTHERS_GROUP_N)

static void     roster_class_init            (GossipRosterOldClass *klass);
static void     roster_init                  (GossipRosterOld      *roster);
static void     roster_finalize              (GObject           *object);

static gchar *
roster_item_factory_translate_func           (const gchar       *path,
					      gpointer           data);

static void     roster_menu_remove_cb        (gpointer           callback_data,
					      guint              action,
					      GtkWidget         *widget);
static void     roster_menu_info_cb          (gpointer           callback_data,
					      guint              action,
					      GtkWidget         *widget);
static void     roster_menu_rename_cb        (gpointer           callback_data,
					      guint              action,
					      GtkWidget         *widget);
static gboolean roster_button_press_event_cb (GtkTreeView       *tree_view,
					      GdkEventButton    *event,
					      GossipRosterOld      *roster);

static void     roster_row_activated_cb      (GtkTreeView       *treeview,
					      GtkTreePath       *path,
					      GtkTreeViewColumn *col,
					      gpointer           data);
static GtkTreeStore *roster_create_store     (GossipRosterOld      *roster);
static GossipRosterOldItem *
roster_get_selected_item                     (GossipRosterOld      *roster);
static void     roster_reset_connection      (GossipRosterOld      *roster);
static void     roster_connected_cb          (GossipApp         *app,
					      GossipRosterOld      *roster);
static void     roster_disconnected_cb       (GossipApp         *app,
					      GossipRosterOld      *roster);
static LmHandlerResult
roster_presence_handler                      (LmMessageHandler  *handler,
					      LmConnection      *connection,
					      LmMessage         *m,
					      GossipRosterOld      *roster);
static LmHandlerResult
roster_iq_handler                            (LmMessageHandler  *handler,
					      LmConnection      *connection,
					      LmMessage         *m,
					      GossipRosterOld      *roster);
static gint     roster_iter_compare_func     (GtkTreeModel      *model,
					      GtkTreeIter       *iter_a,
					      GtkTreeIter       *iter_b,
					      gpointer           user_data);
static void     roster_clear                 (GossipRosterOld      *roster);
static void     roster_update_user           (GossipRosterOld      *roster, 
					      GossipJID         *jid,
					      const gchar       *name,
					      const gchar       *subscription,
					      const gchar       *ask,
					      const gchar       *group);
static void     roster_pixbuf_cell_data_func (GtkTreeViewColumn *tree_column,
					      GtkCellRenderer   *cell,
					      GtkTreeModel      *tree_model,
					      GtkTreeIter       *iter,
					      GossipRosterOld      *roster);
static void     roster_name_cell_data_func   (GtkTreeViewColumn *tree_column,
					      GtkCellRenderer   *cell,
					      GtkTreeModel      *tree_model,
					      GtkTreeIter       *iter,
					      GossipRosterOld      *roster);
static GossipRosterOldItem * roster_item_new    (GossipJID         *jid,
					      const gchar       *name,
					      const gchar       *subscription,
					      const gchar       *ask,
					      const gchar       *group,
					      gboolean           is_group);
static void     roster_item_update           (GossipRosterOldItem  *item,
					      GossipJID         *jid,
					      const gchar       *name,
					      const gchar       *subscription,
					      const gchar       *ask,
					      const gchar       *group);
static void     roster_item_free             (GossipRosterOldItem  *item);
static gboolean roster_str_equal             (gconstpointer      a,
					      gconstpointer      b);
static guint    roster_str_hash              (gconstpointer      key);
static GdkPixbuf * roster_get_pixbuf_from_stock (GossipRosterOld *roster,
						 const gchar     *stock);
static GdkPixbuf *roster_get_pixbuf_from_status (GossipRosterOld *roster,
						 GossipStatus     status);



#define GIF_CB(x) ((GtkItemFactoryCallback)(x))

static GtkItemFactoryEntry menu_items[] = {
	{
		N_("/Contact _Information"),
		NULL,
		GIF_CB (roster_menu_info_cb),
		MENU_INFO,
		"<Item>",
		NULL
	},
	{
		N_("/Re_name contact"),
		NULL,
		GIF_CB (roster_menu_rename_cb),
		MENU_RENAME,
		"<Item>",
		NULL
	},
	{
		"/sep1",
		NULL,
		NULL,
		0,
		"<Separator>",
		NULL
	},
	{
		N_("/_Remove contact"),
		NULL,
		GIF_CB (roster_menu_remove_cb),
		MENU_REMOVE,
		"<StockItem>",
		GTK_STOCK_REMOVE
	}
};


static GObjectClass *parent_class;
static guint signals[LAST_SIGNAL];


GType
gossip_roster_old_get_type (void)
{
	static GType object_type = 0;
	
	if (!object_type) {
		static const GTypeInfo object_info = {
			sizeof (GossipRosterOldClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) roster_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GossipRosterOld),
			0,              /* n_preallocs */
			(GInstanceInitFunc) roster_init,
		};

		object_type = g_type_register_static (GTK_TYPE_TREE_VIEW,
                                                      "GossipRosterOld", 
                                                      &object_info,
						      0);
	}

	return object_type;
}

static void
roster_class_init (GossipRosterOldClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
        parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
        
        object_class->finalize = roster_finalize;

	signals[USER_ACTIVATED] = 
		g_signal_new ("user_activated",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
}

static void
roster_init (GossipRosterOld *roster)
{
        GossipRosterOldPriv  *priv;
	GtkTreeStore      *store;
	GtkCellRenderer   *cell;	
	GtkTreeViewColumn *col;

        priv = g_new0 (GossipRosterOldPriv, 1);
        roster->priv = priv;

	priv->connection       = NULL;
	priv->presence_handler = NULL;
	priv->iq_handler       = NULL;

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (roster), FALSE);

	store = roster_create_store (roster);
	
	gtk_tree_view_set_model (GTK_TREE_VIEW (roster), 
				 GTK_TREE_MODEL (store));
	
	g_signal_connect (roster,
			  "row_activated",
			  G_CALLBACK (roster_row_activated_cb),
			  NULL);

	col = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_pixbuf_new ();
	g_object_set (cell, "ypad", (guint) 1, NULL);
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	
	gtk_tree_view_column_set_cell_data_func (col, cell, 
						 (GtkTreeCellDataFunc) roster_pixbuf_cell_data_func,
						 roster, NULL);
	
	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "ypad", (guint) 1, NULL);

	gtk_tree_view_column_pack_start (col, cell, TRUE);
	
	gtk_tree_view_column_set_cell_data_func (col, cell,
						 (GtkTreeCellDataFunc) roster_name_cell_data_func,
						 roster, NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (roster), col);

	/* Setup right-click menu. */
	priv->popup_factory = gtk_item_factory_new (GTK_TYPE_MENU, "<main>", NULL);
	
	gtk_item_factory_set_translate_func (priv->popup_factory,
					     roster_item_factory_translate_func,
					     NULL,
					     NULL);
	
	gtk_item_factory_create_items (priv->popup_factory,
				       G_N_ELEMENTS (menu_items),
				       menu_items,
				       roster);

	g_signal_connect (roster, "button_press_event",
			  G_CALLBACK (roster_button_press_event_cb),
			  roster);
}

static void
roster_free_string_list (GList *list) 
{
	GList *l;

	for (l = list; l; l = l->next) {
		g_free (l->data);
	}

	g_list_free (list);
}

static void
roster_finalize (GObject *object)
{
        GossipRosterOld     *roster;
        GossipRosterOldPriv *priv;
	
        roster = GOSSIP_ROSTER_OLD (object);
        priv = roster->priv;

	g_hash_table_destroy (priv->contacts);
	roster_free_string_list (priv->groups);

	roster_reset_connection (roster);
	
	if (priv->enable_sound_timeout_id) {
		g_source_remove (priv->enable_sound_timeout_id);
		priv->enable_sound_timeout_id = 0;
	}
	
	g_free (priv);
	roster->priv = NULL;

        if (G_OBJECT_CLASS (parent_class)->finalize) {
                (* G_OBJECT_CLASS (parent_class)->finalize) (object);
        }
}

static gchar *
roster_item_factory_translate_func (const gchar *path,
				    gpointer     data)
{
	return _((gchar *) path);
}

static void
roster_menu_remove_cb (gpointer   callback_data,
		       guint      action,
		       GtkWidget *widget)
{
	GossipRosterOld     *roster = callback_data;
	GossipRosterOldPriv *priv;
	GossipRosterOldItem *item;
	gchar            *str;
	GtkWidget        *dialog;
	gint              response;
	LmMessage        *m;
	LmMessageNode    *node;
	GossipJID        *jid;
	
	priv = roster->priv;

	item = roster_get_selected_item (roster);
	if (!item) {
		return;
	}

	str = g_strdup_printf ("<b>%s</b>", 
			       gossip_jid_get_without_resource (item->jid));

	/* Translator: %s denotes the Jabber ID */
	dialog = gtk_message_dialog_new (NULL,
					 0,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_YES_NO,
					 _("Do you want to remove the contact\n"
					   "%s\n"
					   "from your contact list?"),
					 str);
	
	g_free (str);
	
	g_object_set (GTK_MESSAGE_DIALOG (dialog)->label,
		      "use-markup", TRUE,
		      "wrap", FALSE,
		      NULL);
	
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (response != GTK_RESPONSE_YES) {
		return;
	}

	jid = gossip_jid_ref (item->jid);
	
	/* Remove from roster. */
 	m = lm_message_new_with_sub_type (NULL,
 					  LM_MESSAGE_TYPE_IQ,
 					  LM_MESSAGE_SUB_TYPE_SET);
	
 	node = lm_message_node_add_child (m->node, "query", NULL);
 	lm_message_node_set_attribute (node,
				       "xmlns",
				       "jabber:iq:roster");
	
 	node = lm_message_node_add_child (node, "item", NULL);
 	lm_message_node_set_attributes (node,
					"jid", gossip_jid_get_without_resource (jid),
					"subscription", "remove",
					NULL);
 	
 	lm_connection_send (priv->connection, m, NULL);
 	lm_message_unref (m);

	/* Unsubscribe. */
	m = lm_message_new_with_sub_type (gossip_jid_get_without_resource (jid),
					  LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_UNSUBSCRIBE);
	lm_connection_send (priv->connection, m, NULL);
	lm_message_unref (m);

	gossip_jid_unref (item->jid);

	/* FIXME: if the selected item is an agent, we need to unregister too. */
}

static void
roster_menu_info_cb (gpointer   callback_data,
		     guint      action,
		     GtkWidget *widget)
{
	GossipRosterOld      *roster = callback_data;
	GossipRosterOldPriv  *priv;
	GossipRosterOldItem  *item;
	GossipContactInfo *info;

	priv = roster->priv;
	item = roster_get_selected_item (roster);
	info = gossip_contact_info_new (priv->app, item->jid, item->name);
}

static void
roster_rename_activate_cb (GtkWidget *entry, GtkDialog *dialog)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static void
roster_menu_rename_cb (gpointer   callback_data,
		       guint      action,
		       GtkWidget *widget)
{
	GossipRosterOld     *roster = callback_data;
	GossipRosterOldPriv *priv;
	GossipRosterOldItem *item;
	GtkWidget        *dialog;
	GtkWidget        *entry, *hbox;
	gchar            *str;
	LmMessage        *m;
	LmMessageNode    *node;

	priv = roster->priv;
	
	item = roster_get_selected_item (roster);
	
	str = g_strdup_printf ("<b>%s</b>", 
			       gossip_jid_get_without_resource (item->jid));
       	
	/* Translator: %s denotes the Jabber ID */
	dialog = gtk_message_dialog_new (NULL,
					 0,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_OK_CANCEL,
					 _("Please enter a new nickname for the contact\n%s"),
					 str);

	g_free (str);
	
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	
	g_object_set (GTK_MESSAGE_DIALOG (dialog)->label,
		      "use-markup", TRUE,
		      NULL);

	entry = gtk_entry_new ();
	gtk_widget_show (entry);

	gtk_entry_set_text (GTK_ENTRY (entry), item->name);
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
	
	g_signal_connect (entry,
			  "activate",
			  G_CALLBACK (roster_rename_activate_cb),
			  dialog);
	
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, FALSE, TRUE, 4);
	
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		str = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
	} else {
		str = NULL;
	}
	
	gtk_widget_destroy (dialog);

	if (!str || !str[0]) {
		return;
	}
	
	m = lm_message_new_with_sub_type (NULL, 
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_SET);
	node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attributes (node,
					"xmlns", "jabber:iq:roster",
					NULL);

	node = lm_message_node_add_child (node, "item", NULL);
	lm_message_node_set_attributes (node, 
					"jid", gossip_jid_get_without_resource (item->jid),
					"name", str,
					NULL);
	
	if (item->group && item->group[0]) {
		lm_message_node_add_child (node, "group", item->group);
	}
	
	g_free (str);
	
	lm_connection_send (priv->connection, m, NULL);
	
	lm_message_unref (m);
}

static gboolean
roster_button_press_event_cb (GtkTreeView    *tree_view,
			      GdkEventButton *event,
			      GossipRosterOld   *roster)
{
	GtkTreePath      *path;
	GossipRosterOldPriv *priv;
	GtkItemFactory   *factory;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;

	priv = roster->priv;
	factory = priv->popup_factory;

	selection = gtk_tree_view_get_selection (tree_view);
	model = gtk_tree_view_get_model (tree_view);
	
	if (event->button == 3) {
		gtk_widget_grab_focus (GTK_WIDGET (tree_view));

		if (gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y, &path, NULL, NULL, NULL)) {
			gtk_tree_selection_unselect_all (selection);

			gtk_tree_selection_select_path (selection, path);

			gtk_tree_model_get_iter (model, &iter, path);

			if (gtk_tree_model_iter_has_child (model, &iter)) {
				return FALSE;
			}
			
			gtk_widget_set_sensitive (
				gtk_item_factory_get_widget_by_action (factory, MENU_REMOVE), TRUE);
			
			gtk_tree_path_free (path);

			gtk_item_factory_popup (factory,
						event->x_root, event->y_root,
						event->button, event->time);
			
			return TRUE;
		}
	}

	return FALSE;
}

static void
roster_row_activated_cb (GtkTreeView       *treeview,
			 GtkTreePath       *path,
			 GtkTreeViewColumn *col,
			 gpointer           data)
{
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	GossipRosterOldItem *item;

	model = gtk_tree_view_get_model (treeview);

	gtk_tree_model_get_iter (model, &iter, path);
	
	gtk_tree_model_get (model, &iter, COL_ITEM, &item, -1);

	if (!item->is_group) {
		g_signal_emit (treeview, signals[USER_ACTIVATED], 0, 
			       item->jid);
	}
}

static gboolean
roster_find_user_foreach (GtkTreeModel *model,
			  GtkTreePath  *path,
			  GtkTreeIter  *iter,
			  FindUserData *data)
{
	GossipRosterOldItem *item;
	gboolean          ret_val = FALSE;

	gtk_tree_model_get (model, iter, COL_ITEM, &item, -1);

	if (item->is_group) {
		return FALSE;
	}

	/*d(g_print ("==> "));

	d(g_print ("In foreach:: '%s' ?= '%s'\n", 
		   gossip_jid_get_without_resource (data->jid), 
		   gossip_jid_get_without_resource (item->jid)));*/
	
	if (gossip_jid_equals_without_resource (data->jid, item->jid)) {
		data->found = TRUE;
		data->found_iter = *iter;
		ret_val = TRUE;
	}

	return ret_val;
}

static GtkTreeStore *
roster_create_store (GossipRosterOld *roster)
{
	GossipRosterOldPriv *priv = roster->priv;
	GtkTreeStore     *store;

	if (priv->groups) {
		roster_free_string_list (priv->groups);
		priv->groups = NULL;
	}
	if (priv->contacts) {
		g_hash_table_destroy (priv->contacts);
	}

	priv->contacts = g_hash_table_new_full (roster_str_hash, roster_str_equal, 
						g_free, (GDestroyNotify) roster_item_free);
	
	store = gtk_tree_store_new (NUMBER_OF_COLS, G_TYPE_POINTER);
	
	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store),
						 roster_iter_compare_func,
						 roster,
						 NULL);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					      GTK_SORT_ASCENDING);
	return store;
}

static GossipRosterOldItem *
roster_get_selected_item (GossipRosterOld *roster)
{
	GossipRosterOldPriv *priv;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	GossipRosterOldItem *item;
	
	priv = roster->priv;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (roster));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter,
			    COL_ITEM, &item,
			    -1);

	return item;
}

static gboolean
roster_find_group_foreach (GtkTreeModel  *model,
			   GtkTreePath   *path,
			   GtkTreeIter   *iter,
			   FindGroupData *data)
{
	GossipRosterOldItem *item;
	gboolean  ret_val = FALSE;

	gtk_tree_model_get (model, iter, COL_ITEM, &item, -1);

	if (!item->is_group) {
		return FALSE;
	}
	
	if (data->group && g_ascii_strcasecmp (data->group, item->name) == 0) {
		data->found = TRUE;
		data->found_iter = *iter;

		ret_val = TRUE;
	}

	return ret_val;
}

static gboolean
roster_find_group (GossipRosterOld *roster,
		   const gchar  *group,
		   GtkTreeIter  *iter)
{
	GossipRosterOldPriv *priv;
	GtkTreeModel     *model;
	FindGroupData     data;
	
	priv = roster->priv;

	if (!group) {
		return FALSE;
	}
	
	data.found = FALSE;
	data.group = (gchar *) group;
	data.roster = roster;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (roster));

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) roster_find_group_foreach,
				&data);

	if (data.found) {
		*iter = data.found_iter;
		return TRUE;
	}

	return FALSE;
}

static gboolean
roster_enable_sound_cb (gpointer data)
{
	GossipRosterOld     *roster = data;
	GossipRosterOldPriv *priv;

	priv = roster->priv;

	priv->sound_enabled = TRUE;
	
	priv->enable_sound_timeout_id = 0;
	
	return FALSE;
}

static void
roster_reset_connection (GossipRosterOld *roster)
{
	GossipRosterOldPriv *priv;

	priv = roster->priv;

	if (priv->presence_handler) {
		lm_connection_unregister_message_handler (priv->connection,
							  priv->presence_handler,
							  LM_MESSAGE_TYPE_PRESENCE);

		lm_message_handler_unref (priv->presence_handler);
		priv->presence_handler = NULL;
	}

	if (priv->iq_handler) {
		lm_connection_unregister_message_handler (priv->connection,
							  priv->iq_handler,
							  LM_MESSAGE_TYPE_IQ);

		lm_message_handler_unref (priv->iq_handler);
		priv->iq_handler = NULL;
	}

	if (priv->connection) {
		lm_connection_unref (priv->connection);
		priv->connection = NULL;
	}
}

static void
roster_connected_cb (GossipApp *app, GossipRosterOld *roster)
{
	GossipRosterOldPriv *priv;
	LmMessage        *m;
 	LmMessageNode    *node;
	
	g_return_if_fail (GOSSIP_IS_APP (app));
	g_return_if_fail (GOSSIP_IS_ROSTER_OLD (roster));

	priv = roster->priv;

	priv->connection = lm_connection_ref (gossip_app_get_connection ());
	
	priv->presence_handler = 
		lm_message_handler_new ((LmHandleMessageFunction) roster_presence_handler,
					roster, NULL);
	
	lm_connection_register_message_handler (priv->connection, 
						priv->presence_handler,
						LM_MESSAGE_TYPE_PRESENCE,
						LM_HANDLER_PRIORITY_NORMAL);
	
	priv->iq_handler = 
		lm_message_handler_new ((LmHandleMessageFunction) roster_iq_handler,
					roster, NULL);
	
	lm_connection_register_message_handler (priv->connection, 
						priv->iq_handler,
						LM_MESSAGE_TYPE_IQ,
						LM_HANDLER_PRIORITY_NORMAL);

	m = lm_message_new_with_sub_type (NULL, 
					  LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_GET);
	node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attributes (node,
					"xmlns", "jabber:iq:roster",
					NULL);
	
	lm_connection_send_with_reply (priv->connection, m, 
				       priv->iq_handler, NULL);
	lm_message_unref (m);
	
	if (!priv->enable_sound_timeout_id) {
		priv->enable_sound_timeout_id =
			g_timeout_add (5000, roster_enable_sound_cb, roster);
	}
}

static void
roster_disconnected_cb (GossipApp *app, GossipRosterOld *roster)
{
	GossipRosterOldPriv *priv;
	
	g_return_if_fail (GOSSIP_IS_APP (app));
	g_return_if_fail (GOSSIP_IS_ROSTER_OLD (roster));

	priv = roster->priv;

	if (priv->enable_sound_timeout_id) {
		g_source_remove (priv->enable_sound_timeout_id);
		priv->enable_sound_timeout_id = 0;
		priv->sound_enabled = FALSE;
	}

	roster_reset_connection (roster);
	
	roster_clear (roster);
}

static GossipStatus
get_status_from_type_and_show (LmMessageSubType  type,
			       const gchar      *show)
{
	if (type == LM_MESSAGE_SUB_TYPE_UNAVAILABLE) {
		return GOSSIP_STATUS_OFFLINE;
	}

	if (!show) {
		return GOSSIP_STATUS_AVAILABLE;
	}
	else if (strcmp (show, "chat") == 0) {
		return GOSSIP_STATUS_AVAILABLE;
	}
	else if (strcmp (show, "away") == 0) {
		return GOSSIP_STATUS_AWAY;
	}
	else if (strcmp (show, "xa") == 0) {
		return GOSSIP_STATUS_EXT_AWAY;
	}
	else if (strcmp (show, "dnd") == 0) {
		return GOSSIP_STATUS_BUSY;
	}
	
	return GOSSIP_STATUS_AVAILABLE;
}

static LmHandlerResult
roster_presence_handler (LmMessageHandler *handler,
			 LmConnection     *connection,
			 LmMessage        *m,
			 GossipRosterOld     *roster)
{
	GossipRosterOldPriv *priv;
	GtkTreeModel     *model;
	GtkTreeIter       iter, group_iter, old_group_iter;
	FindUserData      data;
	const gchar      *from;
	LmMessageSubType  type;
	const gchar      *show = NULL;
	GossipStatus      status = GOSSIP_STATUS_AVAILABLE;
	LmMessageNode    *node;
	gchar            *group = "";
	gchar            *cur_group;
	gboolean          was_online, becomes_online = TRUE;
	GtkTreePath      *path;
	gboolean          expand = FALSE;
	GossipRosterOldItem *item, *old_g_item;

	d(g_print ("====> Roster presencehandler <====\n"));

	type = lm_message_get_sub_type (m);

	switch (type) {
	case LM_MESSAGE_SUB_TYPE_AVAILABLE:
	case LM_MESSAGE_SUB_TYPE_UNAVAILABLE:
		break;

	case LM_MESSAGE_SUB_TYPE_UNSUBSCRIBE:
		/* FIXME: Do we need to handle this here? */

		d(g_print ("Roster got 'unsubscribe'\n"));
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

	case LM_MESSAGE_SUB_TYPE_UNSUBSCRIBED:
		/* FIXME: We should probably remove the item from the list
		 * here?
		 */
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

	default:
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}
	
	priv = roster->priv;

	from = lm_message_node_get_attribute (m->node, "from");
	data.found = FALSE;
	data.jid = gossip_jid_new (from);
	data.roster = roster;

	d(g_print ("JID=%s\n", gossip_jid_get_full (data.jid)));

	item = g_hash_table_lookup (priv->contacts,
				    gossip_jid_get_without_resource (data.jid));
	
	if (!item) {
		d(g_print ("Didn't find the user in the hash table\n"));
		gossip_jid_unref (data.jid);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	roster_item_update (item, data.jid, NULL, NULL, NULL, NULL);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (roster));

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) roster_find_user_foreach,
				&data);

	gossip_jid_unref (data.jid);
	iter = data.found_iter;

	if (!data.found) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}
	
	if (gtk_tree_model_iter_parent (model, &old_group_iter, &iter)) {
		
		gtk_tree_model_get (model, &old_group_iter, 
				    COL_ITEM, &old_g_item,
				    -1);

		if (g_ascii_strcasecmp (OFFLINE_GROUP, old_g_item->name) == 0){
			was_online = FALSE;
		} else {
			was_online = TRUE;
		}
	} else {
		/* Silent warning. */
		was_online = FALSE;
		
		g_assert_not_reached ();
	}

	node = lm_message_node_get_child (m->node, "status");
	if (node) {
		g_free (item->status_str);
		item->status_str = g_strdup (node->value);
	}
	
	node = lm_message_node_get_child (m->node, "show");
	if (node) {
		show = node->value;
	}

	if (type == LM_MESSAGE_SUB_TYPE_UNSUBSCRIBED) {
		status = GOSSIP_STATUS_OFFLINE;
	} else {
		status = get_status_from_type_and_show (type, show);
	}
	
	item->status = status;

	if (status == GOSSIP_STATUS_OFFLINE) {
		becomes_online = FALSE;
	}
	
	gtk_tree_model_get (model, &iter, COL_ITEM, &item, -1);
	
	if (was_online && becomes_online) {
 		/* Change item and notify tree to update */
		item->status = status;
		if ((path = gtk_tree_model_get_path (model, &iter)))
			gtk_tree_model_row_changed (model, path, &iter);

		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	if (!becomes_online) {
		cur_group = g_strdup (OFFLINE_GROUP);
	} else if (group) {
		cur_group = g_strdup (item->group);
	} else {
		cur_group = g_strdup (OTHERS_GROUP);
	}
	
	if (!roster_find_group (roster, cur_group, &group_iter)) {
		GossipRosterOldItem *group_item;
		group_item = roster_item_new (NULL, cur_group, NULL, 
					      NULL, NULL, TRUE);
		priv->groups = g_list_prepend (priv->groups, 
					       g_strdup (cur_group));
		priv->groups = g_list_sort (priv->groups, 
					    (GCompareFunc) strcmp);
		d(g_print ("Creating a new group \n"));
		gtk_tree_store_append (GTK_TREE_STORE (model), 
				       &group_iter,
				       NULL);

		gtk_tree_store_set (GTK_TREE_STORE (model),
				    &group_iter,
				    COL_ITEM, group_item,
				    -1);
		if (g_ascii_strcasecmp (OFFLINE_GROUP, cur_group) != 0) {
			expand = TRUE;
		}
	}
	
	/* Remove the old entry */
 	gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

	/* Remove the group if it's empty */
	if (!gtk_tree_model_iter_has_child (model, &old_group_iter)) {
		gtk_tree_store_remove (GTK_TREE_STORE (model), 
				       &old_group_iter);
		old_g_item = NULL;
	}
	
	/* And add the new in the correct group */
	gtk_tree_store_append (GTK_TREE_STORE (model),
			       &iter,
			       &group_iter);

	d(g_print ("Inserting: %s\n", gossip_jid_get_full (item->jid)));
	
	gtk_tree_store_set (GTK_TREE_STORE (model),
			    &iter,
			    COL_ITEM, item,
			    -1);

	/* Expand the group when the first user is added. */
	if (expand) {
		path = gtk_tree_model_get_path (model, &group_iter);
		if (path) { 
			gtk_tree_view_expand_row (GTK_TREE_VIEW (roster),
						  path,
						  FALSE);
			gtk_tree_path_free (path);
		}
	}
	
	g_free (cur_group);

	if (priv->sound_enabled) {
		if (!was_online && becomes_online) {
			gossip_sound_play (GOSSIP_SOUND_ONLINE);
		}
		else if (was_online && !becomes_online) {
			gossip_sound_play (GOSSIP_SOUND_OFFLINE);
		}
	}
	
	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static LmHandlerResult
roster_iq_handler (LmMessageHandler *handler,
		   LmConnection     *connection,
		   LmMessage        *m,
		   GossipRosterOld     *roster)
{
	GossipRosterOldPriv *priv;
	LmMessageNode    *node;
	const gchar      *xmlns;

	priv = roster->priv;
	
	node = lm_message_node_get_child (m->node, "query");
	if (!node) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}
	
	xmlns = lm_message_node_get_attribute (node, "xmlns");

	if (!xmlns || strcmp (xmlns, "jabber:iq:roster") != 0) {
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	for (node = node->children; node; node = node->next) {
		if (strcmp (node->name, "item") == 0) {
			const gchar   *name;
			const gchar   *from;
			GossipJID     *jid;
			const gchar   *subscription;
			const gchar   *ask;
			const gchar   *group = NULL;
			LmMessageNode *group_node;
			
			from = lm_message_node_get_attribute (node, "jid");
			jid  = gossip_jid_new (from);
			name = lm_message_node_get_attribute (node, "name");
			subscription = lm_message_node_get_attribute (
				node, "subscription");
			ask = lm_message_node_get_attribute (node, "ask");

			if (strcmp (subscription, "remove") == 0) {
				GtkTreeModel *model;
				FindUserData  data;
				
				model = gtk_tree_view_get_model (GTK_TREE_VIEW (roster));
				
				data.found = FALSE;
				data.roster = roster;
				data.jid = jid;
				gtk_tree_model_foreach (
					model,
					(GtkTreeModelForeachFunc) roster_find_user_foreach,
					&data);

				if (data.found) {
					gtk_tree_store_remove (GTK_TREE_STORE (model), &data.found_iter);
				}
				
				g_hash_table_remove (priv->contacts,
						     gossip_jid_get_without_resource (jid));
			} else {
				group_node = lm_message_node_get_child (node, "group");
				if (group_node && group_node->value) {
					group = group_node->value;
				} else {
					group = _(OTHERS_GROUP);
				}

				if (!name) {
					gossip_jid_unref (jid);
					continue;
				}
				
				roster_update_user (roster, jid, name, 
						    subscription, ask, group);
				gossip_jid_unref (jid);
			}
		}
	}
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static gint
roster_iter_compare_func (GtkTreeModel *model,
			  GtkTreeIter  *iter_a,
			  GtkTreeIter  *iter_b,
			  gpointer      user_data)
{
	GossipRosterOldItem *item_a, *item_b;
	gint              ret_val;
       
	gtk_tree_model_get (model, iter_a, COL_ITEM, &item_a, -1);
	gtk_tree_model_get (model, iter_b, COL_ITEM, &item_b, -1);

	if (!item_a->is_group) {
		ret_val = g_ascii_strcasecmp (item_a->name, item_b->name);
	} else {
		if (g_ascii_strcasecmp (OFFLINE_GROUP, item_a->name) == 0) {
			ret_val = 1;
		}
		else if (g_ascii_strcasecmp (OFFLINE_GROUP, 
					     item_b->name) == 0) {
			ret_val = -1;
		} 
		else if (g_ascii_strcasecmp (OTHERS_GROUP, 
					     item_a->name) == 0) {
			ret_val = 1;
		}
		else if (g_ascii_strcasecmp (OTHERS_GROUP, 
					     item_b->name) == 0) {
			ret_val = -1;
		} else {
			ret_val = g_ascii_strcasecmp (item_a->name, 
						      item_b->name);
		}
	}
	
	return ret_val;
}


static void
roster_clear (GossipRosterOld *roster)
{
	GtkTreeModel *model;

	model = GTK_TREE_MODEL (roster_create_store (roster));
	
	gtk_tree_view_set_model (GTK_TREE_VIEW (roster), model);
}

static void
roster_update_user (GossipRosterOld *roster,
		    GossipJID    *jid,
		    const gchar  *name,
		    const gchar  *subscription,
		    const gchar  *ask,
		    const gchar  *group)
{
	GossipRosterOldPriv *priv;
	GtkTreeModel     *model;
	GossipRosterOldItem *item, *group_item;
	GtkTreeIter       parent, iter;
	gboolean          new_user = FALSE;
	
	priv = roster->priv;

	d(g_print ("update_user: %s\n", gossip_jid_get_full (jid)));

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (roster));

	item = g_hash_table_lookup (priv->contacts, 
				    gossip_jid_get_without_resource (jid));

	if (!item) {
		new_user = TRUE;
	}
	
	if (new_user) {
		item = roster_item_new (jid, name, subscription, 
					ask, group, FALSE);
		g_hash_table_insert (priv->contacts, 
				     g_strdup (gossip_jid_get_without_resource (item->jid)),
				     item);
		if (!roster_find_group(roster, OFFLINE_GROUP, &parent)) {
			group_item = roster_item_new (NULL, OFFLINE_GROUP,
						      NULL, NULL, NULL, TRUE);
			gtk_tree_store_append (GTK_TREE_STORE (model),
					       &parent, 
					       NULL);
			gtk_tree_store_set (GTK_TREE_STORE (model),
					    &parent,
					    COL_ITEM, group_item,
					    -1);
		} 
			
		gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent);
		d(g_print ("Insert2:: %s\n", gossip_jid_get_full (item->jid)));
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter, 
				    COL_ITEM, item,
				    -1);
		return;
	}
	
	/* Check if user is online, if not we don't have to care about the group
	 * setting. If yes we need to check if a group was changed.
	 */
	if (item->status == GOSSIP_STATUS_OFFLINE ||
	    strcmp (item->group, group) == 0) {
		/* Find the correct place in the tree and inform tree
		 * to update.
		 */
		roster_item_update (item, jid, name, subscription, ask, group);
		return;
	}

/*	if (strcmp (subscription, "remove") == 0) {
		find.roster = roster;
		find.jid = jid;
		gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) roster_find_user_foreach,
				&find);
		iter = find.found_iter;
		gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
		g_hash_table_remove (priv->contacts, gossip_jid_get_without_resource (jid));
	}
*/
	/* User is online and have changed group, we need to move him in the
	 * roster.
	 */
	
	/* Bug #483 */ 
}

static void
roster_pixbuf_cell_data_func (GtkTreeViewColumn *tree_column,
			      GtkCellRenderer   *cell,
			      GtkTreeModel      *tree_model,
			      GtkTreeIter       *iter,
			      GossipRosterOld      *roster)
{
	GossipRosterOldPriv *priv;
	GossipRosterOldItem *item;
	GdkPixbuf        *pixbuf;
	
	gtk_tree_model_get (tree_model, iter, COL_ITEM, &item, -1);

	priv = roster->priv;

	if (item->is_group) {
		g_object_set (cell,
			      "visible", FALSE,
			      NULL);
		return;
	}

	if (item->flash_on) {
		pixbuf = roster_get_pixbuf_from_stock (roster, GOSSIP_STOCK_MESSAGE);
	} else {
		pixbuf = roster_get_pixbuf_from_status (roster, item->status);
	}
	
	g_object_set (cell,
		      "visible", TRUE,
 		      "pixbuf", pixbuf,
		      NULL);

	g_object_unref (pixbuf);
}

static void
roster_name_cell_data_func (GtkTreeViewColumn *tree_column,
			    GtkCellRenderer   *cell,
			    GtkTreeModel      *tree_model,
			    GtkTreeIter       *iter,
			    GossipRosterOld      *roster)
{
	GossipRosterOldItem *item;
	gint              weight = PANGO_WEIGHT_NORMAL;
	
	gtk_tree_model_get (tree_model, iter, COL_ITEM, &item, -1);
	
	if (item->is_group) {
		weight = PANGO_WEIGHT_BOLD;
	}
	
	g_object_set (cell,
		      "weight", weight,
		      "text", item->name,
		      NULL);
}

static GossipRosterOldItem *
roster_item_new (GossipJID   *jid,
		 const gchar *name,
		 const gchar *subscription,
		 const gchar *ask,
		 const gchar *group,
		 gboolean     is_group)
{
	GossipRosterOldItem *item;
	
	item = g_new0 (GossipRosterOldItem, 1);
	
	if (jid) {
		item->jid = gossip_jid_ref (jid);
	} else {
		item->jid = NULL;
	}
	item->name = g_strdup (name);
	item->subscription = g_strdup (subscription);
	item->ask = g_strdup (ask);
	item->group = g_strdup (group);
	item->is_group = is_group;
	item->status_str = NULL;
	item->status = GOSSIP_STATUS_OFFLINE;

	return item;
}

static void
roster_item_update (GossipRosterOldItem *item,
		    GossipJID        *jid,
		    const gchar      *name,
		    const gchar      *subscription,
		    const gchar      *ask,
		    const gchar      *group)
{
	if (jid) {
		if (item->jid) {
			gossip_jid_unref (item->jid);
		}
		item->jid = gossip_jid_ref (jid);
	}
	if (name) {
		g_free (item->name);
		item->name = g_strdup (name);
	}
	if (subscription) {
		g_free (item->subscription);
		item->subscription = g_strdup (subscription);
	}
	if (ask) {
		g_free (item->ask);
		item->ask = g_strdup (ask);
	}
	if (group) {
		g_free (item->group);
		item->group = g_strdup (group);
	}
}

static void
roster_item_free (GossipRosterOldItem *item) 
{
	if (item->jid) {
		gossip_jid_unref (item->jid);
	}

	g_free (item->name);
	g_free (item->subscription);
	g_free (item->ask);
	g_free (item->group);
	g_free (item);

	if (item->flash_timeout_id) {
		g_source_remove (item->flash_timeout_id);
	}
}

GossipRosterOld *
gossip_roster_old_new (GossipApp *app)
{
	GossipRosterOld     *roster;
	GossipRosterOldPriv *priv;

	roster = g_object_new (GOSSIP_TYPE_ROSTER_OLD, NULL);
	priv   = roster->priv;

	priv->app = app;

	g_signal_connect (app, "connected",
			  G_CALLBACK (roster_connected_cb),
			  roster);

	g_signal_connect (app, "disconnected",
			  G_CALLBACK (roster_disconnected_cb),
			  roster);

	return roster;
}

const gchar *
gossip_roster_old_get_nick_from_jid (GossipRosterOld *roster, GossipJID *jid) 
{
	GossipRosterOldPriv *priv;
	GossipRosterOldItem *item;
	
	g_return_val_if_fail (GOSSIP_IS_ROSTER_OLD (roster), NULL);

	priv = roster->priv;
	
	item = g_hash_table_lookup (priv->contacts, 
				    gossip_jid_get_without_resource (jid));
	
	if (item) {
		return item->name;
	}

	return NULL;
}

GdkPixbuf *
gossip_roster_old_get_status_pixbuf_for_jid (GossipRosterOld *roster,
					     GossipJID    *jid)
{
	GossipRosterOldPriv *priv;
	GossipRosterOldItem *item;
	
	g_return_val_if_fail (GOSSIP_IS_ROSTER_OLD (roster), NULL);
	g_return_val_if_fail (jid != NULL, NULL);

	priv = roster->priv;

	item = g_hash_table_lookup (priv->contacts, 
				    gossip_jid_get_without_resource (jid));
	
	if (item) {
		return roster_get_pixbuf_from_status (roster, item->status);
	}
	
	return NULL;
}

gboolean  
gossip_roster_old_get_is_offline (GossipRosterOld *roster, GossipJID *jid)
{
	GossipRosterOldPriv *priv;
	GossipRosterOldItem *item;
	
	g_return_val_if_fail (GOSSIP_IS_ROSTER_OLD (roster), TRUE);
	g_return_val_if_fail (jid != NULL, TRUE);

	priv = roster->priv;

	item = g_hash_table_lookup (priv->contacts, 
				    gossip_jid_get_without_resource (jid));
	
	if (!item || item->status == GOSSIP_STATUS_OFFLINE) {
		return TRUE;
	}

	return FALSE;
}

GossipShow
gossip_roster_old_get_show (GossipRosterOld *roster, GossipJID *jid)
{
	GossipRosterOldPriv *priv;
	GossipRosterOldItem *item;

	priv = roster->priv;

	item = g_hash_table_lookup (priv->contacts,
				    gossip_jid_get_without_resource (jid));

	if (!item) {
		/* If there's no item yet is_offline will return true
		 * so we can return anything here. */
		return GOSSIP_STATUS_AVAILABLE;
	}

	/* This is somewhat of a hack until this status stuff gets sorted out. */
	switch (item->status) {
	case GOSSIP_STATUS_AVAILABLE:
	case GOSSIP_STATUS_FREE:
		return GOSSIP_SHOW_AVAILABLE;
	case GOSSIP_STATUS_BUSY:
		return GOSSIP_SHOW_BUSY;
	case GOSSIP_STATUS_AWAY:
		return GOSSIP_SHOW_AWAY;
	case GOSSIP_STATUS_EXT_AWAY:
		return GOSSIP_SHOW_EXT_AWAY;
	default:
		/* Offline status is checked through is_offline so
		 * we can return anything here. */
		return GOSSIP_STATUS_AVAILABLE;
	}
}

gboolean
gossip_roster_old_have_jid (GossipRosterOld *roster, GossipJID *jid)
{
	GossipRosterOldPriv *priv;
	
	g_return_val_if_fail (GOSSIP_IS_ROSTER_OLD (roster), FALSE);
	g_return_val_if_fail (jid != NULL, FALSE);
	
	priv = roster->priv;

	if (g_hash_table_lookup (priv->contacts, 
				 gossip_jid_get_without_resource (jid))) {
		return TRUE;
	}
	
	return FALSE;
}

GList *
gossip_roster_old_get_groups (GossipRosterOld *roster)
{
	GossipRosterOldPriv *priv;
	
	g_return_val_if_fail (GOSSIP_IS_ROSTER_OLD (roster), NULL);

	priv = roster->priv;

	return priv->groups;
}

static void
get_jids_foreach (gpointer   key,
		  gpointer   value,
		  GList    **list)
{
	GossipRosterOldItem *item = value;

	*list = g_list_prepend (*list, gossip_jid_ref (item->jid));
}

GList *
gossip_roster_old_get_jids (GossipRosterOld *roster)
{
	GossipRosterOldPriv *priv;
	GList            *jids = NULL;
	
	g_return_val_if_fail (GOSSIP_IS_ROSTER_OLD (roster), NULL);

	priv = roster->priv;

	g_hash_table_foreach (priv->contacts,
			      (GHFunc) get_jids_foreach,
			      &jids);
	
	return jids;
}

void
gossip_roster_old_free_jid_list (GList *jids)
{
	GList *l;

	for (l = jids; l; l = l->next) {
		gossip_jid_unref (l->data);
	}

	g_list_free (jids);
}

static gboolean
roster_str_equal (gconstpointer a, gconstpointer b)
{
	const gchar *string_a = a;
	const gchar *string_b = b;

	return g_strcasecmp (string_a, string_b) == 0;
}

static guint
roster_str_hash (gconstpointer key)
{
	const gchar *p = key;
	guint     h = g_ascii_tolower (*p);

	if (h) {
		for (p += 1; *p != '\0'; p++) {
			h = (h << 5) - h + g_ascii_tolower (*p);
		}
	}

	return h;
}

GossipJID *
gossip_roster_old_get_selected_jid (GossipRosterOld *roster)
{
	GossipRosterOldItem *item;

	item = roster_get_selected_item (roster);
	if (!item) {
		return NULL;
	}

	return item->jid;
}

static gboolean
roster_jid_updated (GossipRosterOld *roster, GossipJID *jid)
{
	GtkTreeModel     *model;
	FindUserData      find;
	GtkTreePath      *path;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (roster)); 
	
	find.roster = roster;
	find.jid = jid;

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) roster_find_user_foreach,
				&find);

	if (!find.found) {
		return FALSE;
	}

	path = gtk_tree_model_get_path (model, &find.found_iter);
	gtk_tree_model_row_changed (model, path, &find.found_iter);
	gtk_tree_path_free (path);

	return TRUE;
}

static gboolean
flash_timeout_func (gpointer data)
{
	GossipRosterOld     *roster;
	GossipRosterOldPriv *priv;
	GossipRosterOldItem *item = data;
	
	roster = gossip_app_get_roster ();
	priv = roster->priv;

	item->flash_on = !item->flash_on;

	if (!roster_jid_updated (roster, item->jid)) {
		item->flash_timeout_id = 0;
		item->flash_on = FALSE;
		return FALSE;
	}

	return TRUE;
}

void
gossip_roster_old_flash_jid (GossipRosterOld *roster,
			 GossipJID    *jid,
			 gboolean      flash)
{
	GossipRosterOldPriv *priv;
	GossipRosterOldItem *item;
	const gchar      *without_resource;

	priv = roster->priv;

	without_resource = gossip_jid_get_without_resource (jid);
	
	item = g_hash_table_lookup (priv->contacts, without_resource);
	if (!item) {
		return;
	}

	if (flash && !item->flash_timeout_id) {
		/* Start... */
		item->flash_timeout_id = g_timeout_add (350, flash_timeout_func, item);
	}
	else if (!flash && item->flash_timeout_id) {
		/* Stop... */
		g_source_remove (item->flash_timeout_id);
		item->flash_timeout_id = 0;
		item->flash_on = FALSE;

		roster_jid_updated (roster, jid);
	}
}

static GdkPixbuf *
roster_get_pixbuf_from_stock (GossipRosterOld *roster,
			      const gchar     *stock)
{
	return gtk_widget_render_icon (GTK_WIDGET (roster),
				       stock,
				       GTK_ICON_SIZE_MENU,
				       NULL);
}

static GdkPixbuf *
roster_get_pixbuf_from_status (GossipRosterOld *roster,
			       GossipStatus     status)
{
	const gchar *stock = NULL;
	
	switch (status) {
	case GOSSIP_STATUS_AVAILABLE:
	case GOSSIP_STATUS_FREE:
		stock = GOSSIP_STOCK_AVAILABLE;
		break;
	case GOSSIP_STATUS_OFFLINE:
		stock = GOSSIP_STOCK_OFFLINE;
		break;
	case GOSSIP_STATUS_BUSY:
		stock = GOSSIP_STOCK_BUSY;
		break;
	case GOSSIP_STATUS_AWAY:
		stock = GOSSIP_STOCK_AWAY;
		break;
	case GOSSIP_STATUS_EXT_AWAY:
		stock = GOSSIP_STOCK_EXT_AWAY;
		break;
	}

	return roster_get_pixbuf_from_stock (roster, stock);
}
