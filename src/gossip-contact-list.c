/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio HB
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

#include <config.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gossip-app.h"
#include "gossip-cell-renderer-text.h"
#include "gossip-contact-info.h"
#include "gossip-edit-groups-dialog.h"
#include "gossip-log.h"
#include "gossip-marshal.h"
#include "gossip-session.h"
#include "gossip-stock.h"

#include "gossip-contact-list.h"

/* Flashing delay for icons (milliseconds). */
#define FLASH_TIMEOUT 500

struct _GossipContactListPriv {
	gboolean        show_offline;

	GHashTable     *groups;

        GtkItemFactory *item_popup_factory;
        GtkItemFactory *group_popup_factory;

	GHashTable     *flash_table;
};

typedef struct {
	gboolean flash_on;
	guint    flash_timeout_id;
} FlashData;


typedef struct {
	gchar       *name;
	gboolean     found;
	GtkTreeIter  iter;
} FindGroup;


typedef struct {
	GossipContact *contact;
	gboolean       found;
	GList         *iters;
} FindContact;


/* -- Static functions -- */
static void gossip_contact_list_class_init (GossipContactListClass *klass);
static void gossip_contact_list_init       (GossipContactList      *list);
static void contact_list_finalize          (GObject                *object);
static void contact_list_get_property      (GObject              *object,
					    guint                 param_id,
					    GValue               *value,
					    GParamSpec           *pspec);
static void contact_list_set_property      (GObject              *object,
					    guint                 param_id,
					    const GValue         *value,
					    GParamSpec           *pspec);

static void contact_list_connected_cb      (GossipSession          *session,
					    GossipContactList      *list);
static void contact_list_contact_added_cb  (GossipSession          *session,
					    GossipContact          *contact,
					    GossipContactList      *list);
static void contact_list_contact_updated_cb (GossipSession         *session,
					     GossipContact         *contact,
					     GossipContactList     *list);
static void     contact_list_contact_presence_updated_cb (GossipSession          *session,
					     GossipContact         *contact,
					     GossipContactList     *list);
static void contact_list_contact_removed_cb (GossipSession         *session,
					     GossipContact         *contact,
					     GossipContactList     *list);
static void     contact_list_get_group                   (GossipContactList      *list,
							  const gchar            *name,
							  GtkTreeIter            *iter_to_set,
							  gboolean               *created);
static gboolean contact_list_get_group_foreach           (GtkTreeModel           *model,
							  GtkTreePath            *path,
							  GtkTreeIter            *iter,
							  FindGroup              *fg);
static void contact_list_add_contact        (GossipContactList     *list,
					     GossipContact         *contact);
static void contact_list_remove_contact     (GossipContactList     *list,
					     GossipContact         *contact);
static void contact_list_create_model       (GossipContactList     *list);
static void contact_list_setup_view         (GossipContactList     *list);
static void     contact_list_pixbuf_cell_data_func       (GtkTreeViewColumn      *tree_column,
							  GtkCellRenderer        *cell,
							  GtkTreeModel           *model,
							  GtkTreeIter            *iter,
							  gpointer                user_data);
static gboolean contact_list_button_press_event_cb       (GossipContactList      *list,
						GdkEventButton    *event,
						gpointer           unused);
static void contact_list_row_activated_cb      (GossipContactList *list,
						GtkTreePath       *path,
						GtkTreeViewColumn *col,
						gpointer           unused);
static gint contact_list_sort_func             (GtkTreeModel      *model,
						GtkTreeIter       *iter_a,
						GtkTreeIter       *iter_b,
						gpointer           unused);
static GList *  contact_list_find_contact                (GossipContactList      *list,
							  GossipContact          *contact);
static gboolean contact_list_find_contact_foreach        (GtkTreeModel           *model,
							  GtkTreePath            *path,
						GtkTreeIter       *iter,
							  FindContact            *fc);
static gchar *  contact_list_item_factory_translate_func (const gchar            *path,
                                                gpointer           data);
static void contact_list_item_menu_info_cb     (gpointer               data,
                                                guint                  action,
                                                GtkWidget             *widget);
static void contact_list_item_menu_rename_cb   (gpointer               data,
                                                guint                  action,
                                                GtkWidget             *widget);
static void     contact_list_item_menu_edit_groups_cb    (gpointer                data,
                                                guint                  action,
                                                GtkWidget             *widget);
static void contact_list_item_menu_log_cb      (gpointer               data,
                                                guint                  action,
                                                GtkWidget             *widget);
static void contact_list_item_menu_remove_cb   (gpointer               data,
                                                guint                  action,
                                                GtkWidget             *widget);
static void contact_list_group_menu_rename_cb  (gpointer               data,
                                                guint                  action,
                                                GtkWidget             *widget);
static void contact_list_event_added_cb        (GossipEventManager    *manager,
						GossipEvent           *event,
						GossipContactList     *list);
static void contact_list_event_removed_cb      (GossipEventManager    *manager,
						GossipEvent           *event,
						GossipContactList     *list);
static void contact_list_flash_free_data       (FlashData             *data);

/* -- Signals -- */
enum {
        CONTACT_ACTIVATED,
        LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

/* -- Model columns -- */
enum {
	MODEL_COL_PIXBUF,
	MODEL_COL_NAME,
	MODEL_COL_STATUS,
	MODEL_COL_CONTACT,
	MODEL_COL_IS_GROUP,
	NUMBER_OF_COLS
};

/* -- Properties -- */
enum {
	PROP_0,
	PROP_SHOW_OFFLINE
};

/* -- Item context menu -- */
enum {
        ITEM_MENU_NONE,
        ITEM_MENU_REMOVE,
        ITEM_MENU_INFO,
        ITEM_MENU_RENAME,
        ITEM_MENU_EDIT_GROUPS,
        ITEM_MENU_LOG
};

#define GIF_CB(x) ((GtkItemFactoryCallback)(x))
static GtkItemFactoryEntry item_menu_items[] = {
	{
		N_("/Contact _Information"),
		NULL,
		GIF_CB (contact_list_item_menu_info_cb),
		ITEM_MENU_INFO,
		"<Item>",
		NULL
	},
	{
		N_("/Re_name contact"),
		NULL,
		GIF_CB (contact_list_item_menu_rename_cb),
		ITEM_MENU_RENAME,
		"<Item>",
		NULL
	},
	{
		N_("/_Edit groups"),
		NULL,
		GIF_CB (contact_list_item_menu_edit_groups_cb),
		ITEM_MENU_EDIT_GROUPS,
		"<Item>",
		NULL
	},
	{
		N_("/Show _Log"),
		NULL,
		GIF_CB (contact_list_item_menu_log_cb),
		ITEM_MENU_LOG,
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
		GIF_CB (contact_list_item_menu_remove_cb),
		ITEM_MENU_REMOVE,
		"<StockItem>",
		GTK_STOCK_REMOVE
	}
};

/* -- Group context menu -- */
enum {
        GROUP_MENU_NONE,
        GROUP_MENU_RENAME
};

static GtkItemFactoryEntry group_menu_items[] = {
	{
		N_("/Re_name group"),
		NULL,
		GIF_CB (contact_list_group_menu_rename_cb),
		GROUP_MENU_RENAME,
		"<Item>",
		NULL
	}
};

G_DEFINE_TYPE (GossipContactList, gossip_contact_list, GTK_TYPE_TREE_VIEW);

static void
gossip_contact_list_class_init (GossipContactListClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = contact_list_finalize;
	object_class->get_property = contact_list_get_property;
	object_class->set_property = contact_list_set_property;

        signals[CONTACT_ACTIVATED] =
                g_signal_new ("contact-activated",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              gossip_marshal_VOID__POINTER,
                              G_TYPE_NONE,
                              1, G_TYPE_POINTER);

	g_object_class_install_property (object_class,
					 PROP_SHOW_OFFLINE,
					 g_param_spec_boolean ("show_offline",
							       "Show Offline",
							       "Whether contact list should display offline contacts",
							       FALSE,
							       G_PARAM_READWRITE));
}

static void
gossip_contact_list_init (GossipContactList *list)
{
	GossipContactListPriv *priv;
	
	priv = g_new0 (GossipContactListPriv, 1);
	list->priv = priv;

	priv->groups = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, NULL);

	priv->flash_table = g_hash_table_new_full (gossip_contact_hash,
						   gossip_contact_equal,
						   (GDestroyNotify) g_object_unref,
						   (GDestroyNotify) contact_list_flash_free_data);
	
	contact_list_create_model (list);
	contact_list_setup_view (list);

        /* -- Context menues -- */
        priv->item_popup_factory = gtk_item_factory_new (GTK_TYPE_MENU,
							 "<main>", NULL);
	priv->group_popup_factory = gtk_item_factory_new (GTK_TYPE_MENU,
							  "<main>", NULL);
	
	gtk_item_factory_set_translate_func (priv->item_popup_factory,
                                             contact_list_item_factory_translate_func,
					     NULL,
					     NULL);
	
	gtk_item_factory_set_translate_func (priv->group_popup_factory,
					     contact_list_item_factory_translate_func,
					     NULL,
					     NULL);

	gtk_item_factory_create_items (priv->item_popup_factory,
				       G_N_ELEMENTS (item_menu_items),
				       item_menu_items,
                                       list);
	
	gtk_item_factory_create_items (priv->group_popup_factory,
				       G_N_ELEMENTS (group_menu_items),
				       group_menu_items,
				       list);
        
        /* -- Signal connection  -- */
	g_signal_connect (gossip_app_get_session (),
			  "connected",
			  G_CALLBACK (contact_list_connected_cb),
			  list);
	g_signal_connect (gossip_app_get_session (),
			  "contact-added",
			  G_CALLBACK (contact_list_contact_added_cb),
			  list);
	g_signal_connect (gossip_app_get_session (),
			  "contact-updated",
			  G_CALLBACK (contact_list_contact_updated_cb),
			  list);
	g_signal_connect (gossip_app_get_session (),
			  "contact-presence-updated",
			  G_CALLBACK (contact_list_contact_presence_updated_cb),
			  list);
	g_signal_connect (gossip_app_get_session (),
			  "contact-removed",
			  G_CALLBACK (contact_list_contact_removed_cb),
			  list);

	/* -- Connect to event manager signals -- */
	g_signal_connect (gossip_app_get_event_manager (),
			  "event-added",
			  G_CALLBACK (contact_list_event_added_cb),
			  list);
	g_signal_connect (gossip_app_get_event_manager (),
			  "event-removed",
			  G_CALLBACK (contact_list_event_removed_cb),
			  list);

	/* Connect to tree view signals rather than override */
	g_signal_connect (list,
			  "button-press-event",
			  G_CALLBACK (contact_list_button_press_event_cb),
			  NULL);
	g_signal_connect (list,
			  "row-activated",
			  G_CALLBACK (contact_list_row_activated_cb),
			  NULL);
}

static void
contact_list_finalize (GObject *object)
{
}

static void
contact_list_get_property (GObject    *object,
			   guint       param_id,
			   GValue     *value,
			   GParamSpec *pspec)
{
	GossipContactListPriv *priv;

	priv = GOSSIP_CONTACT_LIST (object)->priv;

	switch (param_id) {
	case PROP_SHOW_OFFLINE:
		g_value_set_boolean (value, priv->show_offline);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void 
contact_list_set_property (GObject      *object,
			   guint         param_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
	GossipContactListPriv *priv;

	priv = GOSSIP_CONTACT_LIST (object)->priv;

	switch (param_id) {
	case PROP_SHOW_OFFLINE:
		gossip_contact_list_set_show_offline (GOSSIP_CONTACT_LIST (object),
						      g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
contact_list_connected_cb (GossipSession *session, GossipContactList *list)
{
	g_print ("Contact List: Connected\n");
}

static void
contact_list_contact_added_cb (GossipSession     *session,
			       GossipContact     *contact,
			       GossipContactList *list)
{
	GossipContactListPriv *priv;
	GtkTreeModel          *model;
	GList                 *groups;

	priv = list->priv;

	g_print ("Contact List: Contact added: %s\n",
		 gossip_contact_get_name (contact));

	if (!priv->show_offline && !gossip_contact_is_online (contact)) {
		return;
	}

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));

	groups = gossip_contact_get_groups (contact);

	contact_list_add_contact (list, contact);
}

static void
contact_list_contact_updated_cb (GossipSession     *session,
				 GossipContact     *contact,
				 GossipContactList *list)
{
	GtkTreeModel   *model;
	GossipPresence *presence;
	GList          *iters, *l;

	model    = gtk_tree_view_get_model (GTK_TREE_VIEW (list));
	presence = gossip_contact_get_presence (contact);

	iters = contact_list_find_contact (list, contact);
	if (!iters) {
		return;
	}

	for (l = iters; l; l = l->next) {
		gtk_tree_store_set (GTK_TREE_STORE (model), l->data,
			    MODEL_COL_NAME, gossip_contact_get_name (contact),
			    -1);
	}

	g_print ("Contact List: Contact updated: %s\n",
		 gossip_contact_get_name (contact));
	
	g_list_foreach (iters, (GFunc)gtk_tree_iter_free, NULL);
	g_list_free (iters);
}

static void 
contact_list_contact_presence_updated_cb (GossipSession     *session,
					  GossipContact     *contact,
					  GossipContactList *list)
{
	GossipContactListPriv *priv;
	GtkTreeModel          *model;
	gboolean               in_list;
	gboolean               should_be_in_list;
	GList                 *iters, *l;

	priv = list->priv;
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));

	iters = contact_list_find_contact (list, contact);
	if (!iters) {
		in_list = FALSE;
	} else {
		in_list = TRUE;
	}

	if (priv->show_offline || gossip_contact_is_online (contact)) {
		should_be_in_list = TRUE;
	} else {
		should_be_in_list = FALSE;
	}

	if (!in_list && !should_be_in_list) {
		/* Nothing to do */
		g_list_foreach (iters, (GFunc)gtk_tree_iter_free, NULL);
		g_list_free (iters);
		return;
	}
	else if (in_list && !should_be_in_list) {
		contact_list_remove_contact (list, contact);
	}
	else if (!in_list && should_be_in_list) {
		contact_list_add_contact (list, contact);
	} else {
		for (l = iters; l; l = l->next) {
			gtk_tree_store_set (GTK_TREE_STORE (model), l->data,
				    MODEL_COL_PIXBUF, gossip_contact_get_pixbuf (contact),
				    MODEL_COL_STATUS, gossip_contact_get_status (contact),
				    -1);
	}
	}
		
	g_print ("Contact List: Contact presence updated: %s '%s'\n",
		 gossip_contact_get_name (contact),
		 gossip_contact_get_status (contact));

	g_list_foreach (iters, (GFunc)gtk_tree_iter_free, NULL);
	g_list_free (iters);
}

static void
contact_list_contact_removed_cb (GossipSession     *session,
				 GossipContact     *contact,
				 GossipContactList *list)
{
	g_print ("Contact List: Contact removed: %s\n",
		 gossip_contact_get_name (contact));

	contact_list_remove_contact (list, contact);
}

static void
contact_list_get_group (GossipContactList *list, 
			const gchar       *name,
			GtkTreeIter       *iter_to_set, 
			gboolean          *created) 
{
	GtkTreeModel *model;
	FindGroup    *fg;

	fg = g_new0 (FindGroup, 1);

	fg->name = g_strdup (name);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));
	gtk_tree_model_foreach (model, 
				(GtkTreeModelForeachFunc) contact_list_get_group_foreach, 
				fg);

	if (!fg->found) {
		if (created) {
			*created = TRUE;
		}
		
		gtk_tree_store_append (GTK_TREE_STORE (model), iter_to_set, NULL);
		gtk_tree_store_set (GTK_TREE_STORE (model), iter_to_set,
				    MODEL_COL_PIXBUF, NULL,
				    MODEL_COL_NAME, name,
				    MODEL_COL_IS_GROUP, TRUE,
				    -1);
		
	} else {
		if (created) {
			*created = FALSE;
		}

		*iter_to_set = fg->iter;
	}

	g_free (fg->name);
	g_free (fg);
}

static gboolean
contact_list_get_group_foreach (GtkTreeModel *model,
				GtkTreePath  *path,
				GtkTreeIter  *iter,
				FindGroup    *fg)
{
	gchar    *str;
	gboolean  is_group;

	/* groups are only at the top level */
	if (gtk_tree_path_get_depth (path) != 1) {
		return FALSE;
	}
	
	gtk_tree_model_get (model, iter, 
			    MODEL_COL_NAME, &str, 
			    MODEL_COL_IS_GROUP, &is_group,
			    -1);
	if (is_group && strcmp (str, fg->name) == 0) {
		fg->found = TRUE;
		fg->iter = *iter;
	}

	g_free (str);

	return fg->found;
}

static void
contact_list_add_contact (GossipContactList *list, GossipContact *contact)
{
	GossipContactListPriv *priv;
	GtkTreeIter            iter, iter_group;
	GtkTreeModel          *model;
	GList                 *l, *groups;

	priv = list->priv;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));

	/* if no groups just add it at the top level */
	groups = gossip_contact_get_groups (contact);
	if (!groups) {
		gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			    MODEL_COL_PIXBUF, gossip_contact_get_pixbuf (contact),
				    MODEL_COL_NAME, gossip_contact_get_name (contact),
				    MODEL_COL_STATUS, gossip_contact_get_status (contact),
				    MODEL_COL_CONTACT, g_object_ref (contact),
				    MODEL_COL_IS_GROUP, FALSE,
				    -1);
	}

	/* else add to each group */
	for (l = groups; l; l = l->next) {
		const gchar *name;
		gboolean     created;

		name = l->data;
		contact_list_get_group (list, name, &iter_group, &created);
			    
		gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &iter_group);
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
				    MODEL_COL_PIXBUF, gossip_contact_get_pixbuf (contact),
			    MODEL_COL_NAME, gossip_contact_get_name (contact),
			    MODEL_COL_STATUS, gossip_contact_get_status (contact),
			    MODEL_COL_CONTACT, g_object_ref (contact),
				    MODEL_COL_IS_GROUP, FALSE,
			    -1);
	}

	/* is this the right place for this? */
	gtk_tree_view_expand_all (GTK_TREE_VIEW (list));
}

static void
contact_list_remove_contact (GossipContactList *list, GossipContact *contact)
{
	GossipContactListPriv *priv;
	GtkTreeModel          *model;
	GList                 *iters, *l;

	priv = list->priv;
	
	iters = contact_list_find_contact (list, contact);
	if (!iters) {
		return;
	}

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));

	for (l = iters; l; l = l->next) {
		GtkTreeIter parent;

		if (gtk_tree_model_iter_parent (model, &parent, l->data) &&
		    gtk_tree_model_iter_n_children (model, &parent) <= 1) {
			gtk_tree_store_remove (GTK_TREE_STORE (model), &parent);
		} else {
			gtk_tree_store_remove (GTK_TREE_STORE (model), l->data);
		}
	}

	g_list_foreach (iters, (GFunc)gtk_tree_iter_free, NULL);
	g_list_free (iters);

	g_hash_table_remove (priv->flash_table, contact);
	g_object_unref (contact);
}

static void
contact_list_create_model (GossipContactList *list)
{
	GtkTreeModel *model;

	model = GTK_TREE_MODEL (gtk_tree_store_new (NUMBER_OF_COLS,
						    GDK_TYPE_PIXBUF,
						    G_TYPE_STRING,
						    G_TYPE_STRING,
						    G_TYPE_POINTER,
						    G_TYPE_BOOLEAN));
	
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (model),
					 MODEL_COL_NAME,
					 contact_list_sort_func,
					 list, NULL);
	
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
					      MODEL_COL_NAME, 
					      GTK_SORT_ASCENDING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (list), model);
}

static void 
contact_list_setup_view (GossipContactList *list)
{
	GtkCellRenderer   *cell;
	GtkTreeViewColumn *col;

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (list), FALSE);

	col  = gtk_tree_view_column_new ();
	
	cell = gtk_cell_renderer_pixbuf_new ();
	g_object_set (cell, 
		      "xpad", (guint) 0,
		      "ypad", (guint) 1,
		      "visible", FALSE,
		      NULL);
	gtk_tree_view_column_pack_start (col, cell, FALSE);

	gtk_tree_view_column_set_cell_data_func (col, cell, 
						 contact_list_pixbuf_cell_data_func, 
						 NULL, NULL);
					    
	/* FIXME: Write a cell renderer that handles our name/status cell */
	cell = gossip_cell_renderer_text_new ();
	g_object_set (cell,
		      "xpad", (guint) 4, 
		      "ypad", (guint) 1,
		      NULL);
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	gtk_tree_view_column_add_attribute (col, cell, 
					    "name", MODEL_COL_NAME);
	gtk_tree_view_column_add_attribute (col, cell,
					    "status", MODEL_COL_STATUS);
	gtk_tree_view_column_add_attribute (col, cell,
					    "is_group", MODEL_COL_IS_GROUP);
	/* FIXME: Add drag'n'drop support back */

	gtk_tree_view_append_column (GTK_TREE_VIEW (list), col);
}

static void  
contact_list_pixbuf_cell_data_func (GtkTreeViewColumn *tree_column,
				    GtkCellRenderer   *cell,
				    GtkTreeModel      *model,
				    GtkTreeIter       *iter,
				    gpointer           user_data)
{
	GdkPixbuf *pixbuf;
	gboolean   is_group;

	gtk_tree_model_get (model, iter, 
			    MODEL_COL_IS_GROUP, &is_group, 
			    MODEL_COL_PIXBUF, &pixbuf,
			    -1);

	g_object_set (cell, 
		      "visible", !is_group,
		      "pixbuf", pixbuf,
		      NULL); 

	if (pixbuf) {
		g_object_unref (pixbuf); 
	}
}

static gboolean 
contact_list_button_press_event_cb (GossipContactList *list,
				    GdkEventButton    *event,
				    gpointer           unused)
{
	GossipContactListPriv *priv;

	priv = list->priv;
	
	if (event->button == 3) {
		GtkTreePath      *path;
		GtkItemFactory   *factory;
		GtkTreeSelection *selection;
		GtkTreeModel     *model;
		GtkTreeIter       iter;
		gboolean          row_exists;
		
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list));
		model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));
		
		gtk_widget_grab_focus (GTK_WIDGET (list));

		row_exists = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (list),
							    event->x, event->y, 
							    &path, 
							    NULL, NULL, NULL);
		if (row_exists) {
                        GossipContact *contact;
			
			gtk_tree_selection_unselect_all (selection);
			gtk_tree_selection_select_path (selection, path);

			gtk_tree_model_get_iter (model, &iter, path);
			gtk_tree_path_free (path);

			gtk_tree_model_get (model, &iter,
                                            MODEL_COL_CONTACT, &contact,
                                            -1);

			if (!contact) {
				factory = priv->group_popup_factory;
			} else {
				gboolean   log_exists;
				GtkWidget *w;
				
				factory = priv->item_popup_factory;
				w = gtk_item_factory_get_item (factory,
							       "/Show Log");
				log_exists = gossip_log_exists (contact);
				gtk_widget_set_sensitive (w, log_exists);
			}
			
			gtk_item_factory_popup (factory,
						event->x_root, event->y_root,
						event->button, event->time);
			
			return TRUE;
		}
	}

	return FALSE;
}

static void 
contact_list_row_activated_cb (GossipContactList *list,
			       GtkTreePath       *path,
			       GtkTreeViewColumn *col,
			       gpointer           unused)
{
	GossipContactListPriv *priv;
	GtkTreeModel          *model;
	GossipContact         *contact;
	GtkTreeIter            iter;

	priv = list->priv;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));

	gtk_tree_model_get_iter (model, &iter, path);

	gtk_tree_model_get (model, &iter,
			    MODEL_COL_CONTACT, &contact,
			    -1);

	if (!contact) { 
		/* This is a group */
		return;
	}

	g_signal_emit (list, signals[CONTACT_ACTIVATED], 0, contact);
}

static gint
contact_list_sort_func (GtkTreeModel *model,
			GtkTreeIter  *iter_a,
			GtkTreeIter  *iter_b,
			gpointer      unused)
{
	gchar         *name_a, *name_b;
	GossipContact *contact_a, *contact_b;
	gint           ret_val;

	gtk_tree_model_get (model, iter_a,
			    MODEL_COL_NAME, &name_a,
			    MODEL_COL_CONTACT, &contact_a, 
			    -1);
	gtk_tree_model_get (model, iter_b,
			    MODEL_COL_NAME, &name_b,
			    MODEL_COL_CONTACT, &contact_b,
			    -1);
	
	/* If contact is NULL it means it's a group */

	if (!contact_a && contact_b) {
		return 1;
	}

	if (contact_a && !contact_b) {
		return -1;
	}

	ret_val = g_ascii_strcasecmp (name_a, name_b);
	g_free (name_a);
	g_free (name_b);
	
	return ret_val;
}

static gboolean
contact_list_iter_equal_contact (GtkTreeModel  *model,
				 GtkTreeIter   *iter,
				 GossipContact *contact)
{
	GossipContact *c;
	
	gtk_tree_model_get (model, iter, 
			    MODEL_COL_CONTACT, &c,
			    -1);
	
	if (!c) {
		return FALSE;
	}

	if (gossip_contact_compare (c, contact) == 0) {
		return TRUE;
	}

	return FALSE;
	
}	

static GList *
contact_list_find_contact (GossipContactList *list,
			   GossipContact     *contact)
{
	GtkTreeModel *model;

	FindContact  *fc;
	GList        *l = NULL;

	fc = g_new0 (FindContact, 1);
	
	fc->contact = g_object_ref (contact);
		
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));
	gtk_tree_model_foreach (model, 
				(GtkTreeModelForeachFunc) contact_list_find_contact_foreach, 
				fc);

	if (fc->found) {
		l = fc->iters;
		}

	g_object_unref (fc->contact);
	g_free (fc);

	return l;
}

static gboolean
contact_list_find_contact_foreach (GtkTreeModel *model,
				   GtkTreePath  *path,
				   GtkTreeIter  *iter,
				   FindContact  *fc)
{
	if (contact_list_iter_equal_contact (model, iter, fc->contact)) {
		fc->found = TRUE;
		fc->iters = g_list_append (fc->iters, gtk_tree_iter_copy (iter));
	}
	
	return fc->found;
}

static gchar *
contact_list_item_factory_translate_func (const gchar *path, gpointer data)
{
	return _((gchar *) path);
}

static void
contact_list_item_menu_info_cb (gpointer   data,
                                guint      action,
                                GtkWidget *widget)
{
        GossipContact *contact;

        contact = gossip_contact_list_get_selected (GOSSIP_CONTACT_LIST (data));
        if (!contact) {
                return;
        }

        gossip_contact_info_new (contact);
}

static void
contact_list_rename_entry_activate_cb (GtkWidget *entry, GtkDialog *dialog)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static void
contact_list_item_menu_rename_cb (gpointer   data,
                                  guint      action,
                                  GtkWidget *widget)
{
	GossipContactList *list;
        GossipContact     *contact;
	gchar             *str;
	GtkWidget         *dialog;
	GtkWidget         *entry;
	GtkWidget         *hbox;
	
        list = GOSSIP_CONTACT_LIST (data);

        contact = gossip_contact_list_get_selected (list);
	if (!contact) {
		return;
	}

	str = g_strdup_printf ("<b>%s</b>", gossip_contact_get_id (contact));

	/* Translator: %s denotes the contact ID */
	dialog = gtk_message_dialog_new (GTK_WINDOW (gossip_app_get_window ()),
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

	gtk_entry_set_text (GTK_ENTRY (entry), 
                            gossip_contact_get_name (contact));
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
	
	g_signal_connect (entry,
			  "activate",
			  G_CALLBACK (contact_list_rename_entry_activate_cb),
			  dialog);
	
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), 
			    hbox, FALSE, TRUE, 4);
	
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		str = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
	} else {
		str = NULL;
	}
	
	gtk_widget_destroy (dialog);

	if (!str || !str[0]) {
		return;
	}
	
	gossip_session_rename_contact (gossip_app_get_session (), contact, str);
}

static void 
contact_list_item_menu_edit_groups_cb (gpointer   data,
                                       guint      action,
                                       GtkWidget *widget)
{
        GossipContact *contact;

        contact = gossip_contact_list_get_selected (GOSSIP_CONTACT_LIST (data));
        if (!contact) {
                return;
        }

        gossip_edit_groups_new (contact);
}

static void
contact_list_item_menu_log_cb (gpointer   data,
                               guint      action,
                               GtkWidget *widget)
{
        GossipContact *contact;

        contact = gossip_contact_list_get_selected (GOSSIP_CONTACT_LIST (data));
        if (!contact) {
                return;
        }

        gossip_log_show (contact);
}

static void
contact_list_item_menu_remove_cb (gpointer   data,
                                  guint      action,
                                  GtkWidget *widget)
{
        GossipContact *contact;
	gchar         *str;
	GtkWidget     *dialog;
	gint           response;
	
        contact = gossip_contact_list_get_selected (GOSSIP_CONTACT_LIST (data));
        if (!contact) {
                return;
        }

	str = g_strdup_printf ("<b>%s</b>", 
                               gossip_contact_get_id (contact));

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

	gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                      GTK_WINDOW (gossip_app_get_window ()));
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (response != GTK_RESPONSE_YES) {
		return;
	}

	gossip_session_remove_contact (gossip_app_get_session (), contact);
}

static void
contact_list_group_menu_rename_cb (gpointer   data,
                                   guint      action,
                                   GtkWidget *widget)
{
        g_print ("FIXME: Implement group::rename\n");
}

typedef struct {
	GossipContactList *list;
	GossipContact     *contact;
} FlashTimeoutData;

static void
contact_list_free_flash_timeout_data (FlashTimeoutData *t_data)
{
	g_return_if_fail (t_data != NULL);

	g_object_unref (t_data->contact);

	g_free (t_data);
}

static void
contact_list_flash_free_data (FlashData *data)
{
	g_return_if_fail (data != NULL);

	if (data->flash_timeout_id) {
		g_source_remove (data->flash_timeout_id);
	}

	g_free (data);
}

static gboolean
contact_list_flash_timeout_func (FlashTimeoutData *t_data)
{
	GossipContactList     *list;
	GossipContactListPriv *priv;
	FlashData             *data;
	gboolean               ret_val;
	GossipContact         *contact;
	GdkPixbuf             *pixbuf;
	GtkTreeModel          *model;
	GList                 *l, *iters;
	
	list = t_data->list;
	priv = list->priv;
	
	contact = t_data->contact;

	pixbuf = gossip_contact_get_pixbuf (contact);
		
	iters = contact_list_find_contact (list, contact);
	if (!iters) {
		return FALSE;
	}
	
	data = g_hash_table_lookup (priv->flash_table, contact);
	if (!data) {
		ret_val = FALSE;
	} else {
		data->flash_on = !data->flash_on;

		if (data->flash_on) {
			pixbuf = gossip_utils_get_pixbuf_from_stock (GOSSIP_STOCK_MESSAGE);
		} 

		ret_val = TRUE;
	}

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));

	for (l = iters; l; l = l->next) {
	gtk_tree_store_set (GTK_TREE_STORE (model),
				    l->data, 
			    MODEL_COL_PIXBUF, pixbuf,
			    -1);
	}

	g_list_foreach (iters, (GFunc)gtk_tree_iter_free, NULL);
	g_list_free (iters);

	return ret_val;
}


static void
contact_list_event_added_cb (GossipEventManager *manager,
			     GossipEvent        *event,
			     GossipContactList  *list)
{
	GossipContactListPriv *priv;
	GossipContact         *contact;
	FlashData             *data;
	FlashTimeoutData      *t_data;
	
	priv = list->priv;
	
	if (gossip_event_get_event_type (event) != GOSSIP_EVENT_NEW_MESSAGE) {
		return;
	}

	contact = GOSSIP_CONTACT (gossip_event_get_data (event));
	data = g_hash_table_lookup (priv->flash_table, contact);

	if (data) {
		/* Already flashing this item */
		return;
	}

	t_data = g_new0 (FlashTimeoutData, 1);
	t_data->list    = list;
	t_data->contact = g_object_ref (contact);

	data = g_new0 (FlashData, 1);
	data->flash_on = TRUE;
	data->flash_timeout_id = 
		g_timeout_add_full (G_PRIORITY_DEFAULT, FLASH_TIMEOUT,
				    (GSourceFunc) contact_list_flash_timeout_func,
				    t_data, 
				    (GDestroyNotify) contact_list_free_flash_timeout_data);
	
	g_hash_table_insert (priv->flash_table, g_object_ref (contact), data);
}

static void
contact_list_event_removed_cb (GossipEventManager *manager,
			       GossipEvent        *event,
			       GossipContactList  *list)
{
	GossipContactListPriv *priv;
	FlashData             *data;
	GossipContact         *contact;
	GdkPixbuf             *pixbuf;
	GtkTreeModel          *model;
	GList                 *iters, *l;
	
	if (gossip_event_get_event_type (event) != GOSSIP_EVENT_NEW_MESSAGE) {
		return;
	}
	
	priv = list->priv;

	contact = GOSSIP_CONTACT (gossip_event_get_data (event));

	data = g_hash_table_lookup (priv->flash_table, contact);
	if (!data) {
		/* Not flashing this contact */
		return;
	}

	g_hash_table_remove (priv->flash_table, contact);
	
	iters = contact_list_find_contact (list, contact);
	if (!iters) {
		return;
	}
	
	pixbuf = gossip_contact_get_pixbuf (contact);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));

	for (l = iters; l; l = l->next) {
	gtk_tree_store_set (GTK_TREE_STORE (model),
				    l->data, 
			    MODEL_COL_PIXBUF, pixbuf,
			    -1);
	}

 	
	g_list_foreach (iters, (GFunc)gtk_tree_iter_free, NULL);
	g_list_free (iters);
	g_object_unref (pixbuf); 
}

GossipContactList *
gossip_contact_list_new (void)
{
	return g_object_new (GOSSIP_TYPE_CONTACT_LIST, NULL);
}

GossipContact *
gossip_contact_list_get_selected (GossipContactList *list)
{
        GossipContactListPriv *priv;
	GtkTreeSelection      *selection;
	GtkTreeIter            iter;
	GtkTreeModel          *model;
	GossipContact         *contact;
	
	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST (list), NULL);
	
	priv = list->priv;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list));
        if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
                return NULL;
        }

        gtk_tree_model_get (model, &iter,
                            MODEL_COL_CONTACT, &contact,
			    -1);

	return contact;
}

gboolean
gossip_contact_list_get_show_offline (GossipContactList *list)
{
	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST (list), FALSE);

	return list->priv->show_offline;
}

void         
gossip_contact_list_set_show_offline (GossipContactList *list,
				      gboolean           show_offline)
{
	GossipContactListPriv *priv;
	GossipSession         *session;
	const GList           *contacts;
	const GList           *l;
		
	g_return_if_fail (GOSSIP_IS_CONTACT_LIST (list));

	priv = list->priv;

	priv->show_offline = show_offline;

	session = gossip_app_get_session ();
	contacts = gossip_session_get_contacts (session);
	for (l = contacts; l; l = l->next) {
		GossipContact *contact;

		contact = GOSSIP_CONTACT (l->data);

		contact_list_contact_presence_updated_cb (session, contact,
							  list);
	}
}
