/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2004 Imendio HB
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

#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>

#include "gossip-app.h"
#include "gossip-roster.h"
#include "gossip-marshal.h"
#include "gossip-contact-info.h"
#include "gossip-edit-groups-dialog.h"
#include "gossip-stock.h"
#include "gossip-utils.h"
#include "gossip-roster-view.h"
#include "gossip-log.h"

#define d(x)

/* Flashing delay for icons (milliseconds). */
#define FLASH_TIMEOUT 500

/* 
 * Active users are those which have recently changed state
 * (e.g. online, offline or from normal to a busy state). 
 * 
 */

/* time user is shown as active */
#define ACTIVE_USER_SHOW_TIME 5000     
        
/* time after connecting which we wait before active users are enabled */
#define ACTIVE_USER_WAIT_TO_ENABLE_TIME 5000    


struct _GossipRosterViewPriv {
	GtkTreeModel        *model;
	GossipRoster        *roster;

	GtkItemFactory      *item_popup_factory;
	GtkItemFactory      *group_popup_factory;

	GHashTable          *flash_table;

	gboolean             show_offline;
	gboolean             show_active; /* show active users */

	GtkTreeRowReference *drag_row;
};

typedef union {
	GossipRosterItem  *item;
	GossipRosterGroup *group;
} RosterElement;

typedef struct {
	gboolean flash_on;
	guint    flash_timeout_id;
} FlashData;

/* Drag & Drop types */
enum DndDragType {
	DND_DRAG_TYPE_JID,
	NUM_DRAG_TYPES
};

typedef struct {
	GossipRosterItem  *item;
	GossipRosterView  *view;
	GossipRosterGroup *group;

	gboolean           remove;
} ShowActiveData;

static GtkTargetEntry drag_types[] = {
	{ "text/jid", 0, DND_DRAG_TYPE_JID },
};

static GdkAtom drag_atoms[NUM_DRAG_TYPES];

static void            roster_view_class_init                  (GossipRosterViewClass *klass);
static void            roster_view_init                        (GossipRosterView      *view);
static void            roster_view_finalize                    (GObject               *object);
static GtkTreeModel *  roster_view_create_store                (GossipRosterView      *view);
static void            roster_view_setup_tree                  (GossipRosterView      *view);
static void            roster_view_connected_cb                (GossipApp             *app,
								GossipRosterView      *view);
static gboolean        roster_view_show_active_users_cb        (GossipRosterView      *view);
static void            roster_view_item_added                  (GossipRoster          *roster,
								GossipRosterItem      *item,
								GossipRosterView      *view);
static void            roster_view_item_updated                (GossipRoster          *roster,
								GossipRosterItem      *item,
								GossipRosterView      *view);
static void            roster_view_item_presence_updated       (GossipRoster          *roster,
								GossipRosterItem      *item,
								GossipRosterView      *view);
static gboolean        roster_view_item_active_cb              (ShowActiveData        *active);
static ShowActiveData *roster_view_item_active_new             (GossipRosterView      *view,
								GossipRosterItem      *item,
								GossipRosterGroup     *group,
								gboolean               remove);
static void            roster_view_item_active_free            (ShowActiveData        *data);
static void            roster_view_item_removed                (GossipRoster          *roster,
								GossipRosterItem      *item,
								GossipRosterView      *view);
static void            roster_view_group_added                 (GossipRoster          *roster,
								GossipRosterGroup     *group,
								GossipRosterView      *view);
static void            roster_view_group_removed               (GossipRoster          *roster,
								GossipRosterGroup     *group,
								GossipRosterView      *view);
static gint            roster_view_iter_compare_func           (GtkTreeModel          *model,
								GtkTreeIter           *iter_a,
								GtkTreeIter           *iter_b,
								gpointer               user_data);
static void            roster_view_pixbuf_cell_data_func       (GtkTreeViewColumn     *tree_column,
								GtkCellRenderer       *cell,
								GtkTreeModel          *tree_model,
								GtkTreeIter           *iter,
								GossipRosterView      *view);
static void            roster_view_name_cell_data_func         (GtkTreeViewColumn     *tree_column,
								GtkCellRenderer       *cell,
								GtkTreeModel          *tree_model,
								GtkTreeIter           *iter,
								GossipRosterView      *view);
static void            roster_view_set_cell_background         (GossipRosterView      *view,
								GtkCellRenderer       *cell,
								gboolean               use_default);
static gboolean        roster_view_find_group                  (GossipRosterView      *view,
								GtkTreeIter           *iter,
								const gchar           *name);
static gboolean        roster_view_find_item                   (GossipRosterView      *view,
								GtkTreeIter           *iter,
								GossipRosterItem      *item,
								GossipRosterGroup     *group);
static gboolean        roster_view_button_press_event_cb       (GossipRosterView      *view,
								GdkEventButton        *event,
								gpointer               data);
static void            roster_view_row_activated_cb            (GossipRosterView      *view,
								GtkTreePath           *path,
								GtkTreeViewColumn     *col,
								gpointer               data);
static void            roster_view_item_menu_remove_cb         (gpointer               data,
								guint                  action,
								GtkWidget             *widget);
static void            roster_view_item_menu_info_cb           (gpointer               data,
								guint                  action,
								GtkWidget             *widget);
static void            roster_view_item_menu_log_cb            (gpointer               data,
								guint                  action,
								GtkWidget             *widget);
static void            roster_view_item_menu_rename_cb         (gpointer               data,
								guint                  action,
								GtkWidget             *widget);
static void            roster_view_item_menu_edit_groups_cb    (gpointer               data,
								guint                  action,
								GtkWidget             *widget);
static void            roster_view_group_menu_rename_cb        (gpointer               data,
								guint                  action,
								GtkWidget             *widget);
static gchar *         roster_view_item_factory_translate_func (const gchar           *path,
								gpointer               data);
static void            roster_view_flash_free_data             (FlashData             *data);
static void            roster_view_add_item                    (GossipRosterView      *view,
								GossipRosterItem      *item,
								GossipRosterGroup     *group);
static gboolean        roster_view_remove_item_with_iter       (GossipRosterView      *view,
								GtkTreeIter           *iter,
								GossipRosterItem      *item,
								GossipRosterGroup     *group);
static void            roster_view_remove_item                 (GossipRosterView      *view,
								GossipRosterItem      *item,
								GossipRosterGroup     *group);
static gboolean        roster_view_iter_equal_item             (GtkTreeModel          *model,
								GtkTreeIter           *iter,
								GossipRosterItem      *item);
static void            roster_view_drag_begin                  (GtkWidget             *widget,
								GdkDragContext        *context,
								gpointer               user_data);
static void            roster_view_drag_data_get               (GtkWidget             *widget,
								GdkDragContext        *contact,
								GtkSelectionData      *selection,
								guint                  info,
								guint                  time,
								gpointer               user_data);
static void            roster_view_drag_end                    (GtkWidget             *widget,
								GdkDragContext        *context,
								gpointer               user_data);






enum {
	CONTACT_ACTIVATED,
	LAST_SIGNAL
};

enum {
	ITEM_MENU_NONE,
	ITEM_MENU_REMOVE,
	ITEM_MENU_INFO,
	ITEM_MENU_RENAME,
	ITEM_MENU_EDIT_GROUPS,
	ITEM_MENU_LOG
};

enum {
	GROUP_MENU_NONE,
	GROUP_MENU_RENAME
};

/* Tree model */
enum {
	COL_IS_GROUP,
	COL_ELEMENT,
	NUMBER_OF_COLS
};

#define GIF_CB(x) ((GtkItemFactoryCallback)(x))
static GtkItemFactoryEntry item_menu_items[] = {
	{
		N_("/Contact _Information"),
		NULL,
		GIF_CB (roster_view_item_menu_info_cb),
		ITEM_MENU_INFO,
		"<Item>",
		NULL
	},
	{
		N_("/Re_name contact"),
		NULL,
		GIF_CB (roster_view_item_menu_rename_cb),
		ITEM_MENU_RENAME,
		"<Item>",
		NULL
	},
	{
		N_("/_Edit groups"),
		NULL,
		GIF_CB (roster_view_item_menu_edit_groups_cb),
		ITEM_MENU_EDIT_GROUPS,
		"<Item>",
		NULL
	},
	{
		N_("/Show _Log"),
		NULL,
		GIF_CB (roster_view_item_menu_log_cb),
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
		GIF_CB (roster_view_item_menu_remove_cb),
		ITEM_MENU_REMOVE,
		"<StockItem>",
		GTK_STOCK_REMOVE
	}
};

static GtkItemFactoryEntry group_menu_items[] = {
	{
		N_("/Re_name group"),
		NULL,
		GIF_CB (roster_view_group_menu_rename_cb),
		GROUP_MENU_RENAME,
		"<Item>",
		NULL
	}
};

/*
enum
{
  TARGET_GTK_TREE_MODEL_ROW
};

static GtkTargetEntry row_targets[] = {
	{ "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_APP,
		TARGET_GTK_TREE_MODEL_ROW }
};
*/

static guint signals[LAST_SIGNAL];

GType                
gossip_roster_view_get_type (void)
{
	static GType object_type = 0;
	
	if (!object_type) {
		static const GTypeInfo object_info = {
			sizeof (GossipRosterViewClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) roster_view_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GossipRosterView),
			0,              /* n_preallocs */
			(GInstanceInitFunc) roster_view_init,
		};

		object_type = g_type_register_static (GTK_TYPE_TREE_VIEW,
                                                      "GossipRosterView", 
                                                      &object_info, 0);
	}

	return object_type;
}

static void  
roster_view_class_init (GossipRosterViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = roster_view_finalize;

	signals[CONTACT_ACTIVATED] = 
		g_signal_new ("contact_activated",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);
}

static void
roster_view_init (GossipRosterView *view)
{
	GossipRosterViewPriv *priv;

	priv = g_new0 (GossipRosterViewPriv, 1);
	view->priv = priv;

	priv->show_offline = FALSE;
	
	priv->model = roster_view_create_store (view);
	priv->flash_table = g_hash_table_new_full (g_int_hash,
						   g_int_equal,
						   (GDestroyNotify) gossip_roster_item_unref,
						   (GDestroyNotify) roster_view_flash_free_data);

	gtk_tree_view_set_model (GTK_TREE_VIEW (view), priv->model);

	roster_view_setup_tree (view);

	/* Setup right-click menu. */
	priv->item_popup_factory = gtk_item_factory_new (GTK_TYPE_MENU,
							 "<main>", NULL);
	priv->group_popup_factory = gtk_item_factory_new (GTK_TYPE_MENU,
							  "<main>", NULL);
	
	gtk_item_factory_set_translate_func (priv->item_popup_factory,
					     roster_view_item_factory_translate_func,
					     NULL,
					     NULL);
	
	gtk_item_factory_set_translate_func (priv->group_popup_factory,
					     roster_view_item_factory_translate_func,
					     NULL,
					     NULL);

	gtk_item_factory_create_items (priv->item_popup_factory,
				       G_N_ELEMENTS (item_menu_items),
				       item_menu_items,
				       view);
	
	gtk_item_factory_create_items (priv->group_popup_factory,
				       G_N_ELEMENTS (group_menu_items),
				       group_menu_items,
				       view);

	/*
	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (view),
						GDK_BUTTON1_MASK,
						row_targets,
						G_N_ELEMENTS (row_targets),
						GDK_ACTION_MOVE | GDK_ACTION_COPY);

	gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (view),
					      row_targets,
					      G_N_ELEMENTS (row_targets),
					      GDK_ACTION_MOVE | GDK_ACTION_COPY);
	 */	
	g_signal_connect (view,
			  "button_press_event",
			  G_CALLBACK (roster_view_button_press_event_cb),
			  NULL);

	g_signal_connect (view,
			  "row_activated",
			  G_CALLBACK (roster_view_row_activated_cb),
			  NULL);

	g_signal_connect (gossip_app_get (), "connected",
			  G_CALLBACK (roster_view_connected_cb),
			  view);
}

static void
roster_view_connected_cb (GossipApp        *app, 
			  GossipRosterView *view)
{
	/* set timeout to enable active users */
	g_timeout_add (ACTIVE_USER_WAIT_TO_ENABLE_TIME, 
		       (GSourceFunc)roster_view_show_active_users_cb,
		       view);
}

static gboolean
roster_view_show_active_users_cb (GossipRosterView *view)
{
	GossipRosterViewPriv *priv = view->priv;

	priv->show_active = TRUE;

	return FALSE;
}

static void
roster_view_finalize (GObject *object)
{
	GossipRosterView     *view = GOSSIP_ROSTER_VIEW (object);
	GossipRosterViewPriv *priv = view->priv;

	g_object_unref (priv->roster);
	g_object_unref (priv->model);
	g_object_unref (priv->item_popup_factory);
	g_object_unref (priv->group_popup_factory);

	g_free (priv);
}

static GtkTreeModel *
roster_view_create_store (GossipRosterView *view)
{
	GtkTreeModel           *model;
	
	model = GTK_TREE_MODEL (gtk_tree_store_new (NUMBER_OF_COLS,
						    G_TYPE_BOOLEAN,
						    G_TYPE_POINTER));

	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (model),
						 roster_view_iter_compare_func,
						 view,
						 NULL);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
					      GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					      GTK_SORT_ASCENDING);

	return model;
}

static void 
roster_view_setup_tree (GossipRosterView *view)
{
	GossipRosterViewPriv *priv;
	GtkCellRenderer      *cell;
	GtkTreeViewColumn    *col;
	static int            setup = 0;

	priv = view->priv;
	
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);

	col = gtk_tree_view_column_new ();
	
	cell = gtk_cell_renderer_pixbuf_new ();
	g_object_set (cell, "xpad", (guint) 0, NULL);
	g_object_set (cell, "ypad", (guint) 1, NULL);
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (col, cell, 
						 (GtkTreeCellDataFunc) roster_view_pixbuf_cell_data_func,
						 view, 
						 NULL);

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "xpad", (guint) 4, NULL);
	g_object_set (cell, "ypad", (guint) 1, NULL);
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (col, cell,
						 (GtkTreeCellDataFunc) roster_view_name_cell_data_func,
						 view,
						 NULL);

	if (!setup) {
		int i;
		
		for (i = 0; i < NUM_DRAG_TYPES; ++i) {
			drag_atoms[i] = gdk_atom_intern (drag_types[i].target,
							 FALSE);
		}

		setup = 1;
	}

	gtk_tree_view_append_column (GTK_TREE_VIEW (view), col);

	gtk_drag_source_set (GTK_WIDGET (view), GDK_BUTTON1_MASK, 
			     drag_types, NUM_DRAG_TYPES, 
			     GDK_ACTION_COPY);

	g_signal_connect (GTK_TREE_VIEW (view), "drag-begin",
			  G_CALLBACK (roster_view_drag_begin),
			  NULL);
	g_signal_connect (GTK_TREE_VIEW (view), "drag-data-get",
			  G_CALLBACK (roster_view_drag_data_get),
			  NULL);
	g_signal_connect (GTK_TREE_VIEW (view), "drag-end",
			  G_CALLBACK (roster_view_drag_end),
			  NULL);
}

static void
roster_view_item_added (GossipRoster     *roster,
			GossipRosterItem *item,
			GossipRosterView *view)
{
	GossipRosterViewPriv *priv;
	GList                *l;
	
	priv = view->priv;

	d(g_print ("Item added '%s'\n", gossip_roster_item_get_name (item)));
	
	if (!priv->show_offline && gossip_roster_item_is_offline (item)) {
		d(g_print ("Offline item: %s\n",
			   gossip_roster_item_get_name (item)));
		return;
	}

	for (l = gossip_roster_item_get_groups (item); l; l = l->next) {
		GossipRosterGroup *group = l->data;

		roster_view_add_item (view, item, group);
	}
}

static void
roster_view_item_updated (GossipRoster     *roster,
			  GossipRosterItem *item,
			  GossipRosterView *view)
{
	GossipRosterViewPriv *priv;
	GList                *l;
	GtkTreeIter           group_iter, iter;
	
	d(g_print ("Item updated\n"));

	priv = view->priv;
	
	if (!gtk_tree_model_get_iter_first (priv->model, &group_iter)) {
		d(g_print ("Empty tree, should this be possible?\n"));
		return;
	}

	do {
		RosterElement *e;
		gboolean       is_group;

		gtk_tree_model_get (priv->model, &group_iter,
				    COL_IS_GROUP, &is_group,
				    COL_ELEMENT, &e,
				    -1);
		
		if (!gtk_tree_model_iter_children (priv->model, &iter,
						   &group_iter)) {
			continue;
		}

		do {
			GtkTreePath *path;
			gboolean     last_child;

			if (!roster_view_iter_equal_item (priv->model, &iter, item)) {
				continue;
			}

			path = gtk_tree_model_get_path (priv->model, &iter);
			gtk_tree_model_row_changed (priv->model, path, &iter);
			gtk_tree_path_free (path);

			last_child = FALSE;
			
			for (l = gossip_roster_item_get_groups (item); l; l = l->next) {
				GossipRosterGroup *group = l->data;

				if (strcmp (gossip_roster_group_get_name (e->group),
					    gossip_roster_group_get_name (group)) == 0) {
					continue;
				}
				
				if (!roster_view_remove_item_with_iter (view, &iter, item, e->group)) {
					/* Ugly, but we need to break out of
					 * both loops.
					 */
					last_child = TRUE;
					break;
				}
			}

			if (last_child) {
				break;
			}
		} while (gtk_tree_model_iter_next (priv->model, &iter));
	} while (gtk_tree_model_iter_next (priv->model, &group_iter));
	
	for (l = gossip_roster_item_get_groups (item); l; l = l->next) {
		GossipRosterGroup *group = l->data;
		GtkTreeIter        iter;

		if (roster_view_find_item (view, &iter, item, group)) {
			continue;
		}

		/* Item is not currently in the group */
		roster_view_add_item (view, item, group);
	}
}

static void  
roster_view_item_presence_updated (GossipRoster     *roster,
				   GossipRosterItem *item,
				   GossipRosterView *view)
{
	GossipRosterViewPriv *priv;
	GtkTreeIter           iter;
	GList                *l;
	gboolean              show_item = FALSE;
	
	d(g_print ("Item presence updated: %s\n", gossip_roster_item_get_name (item)));

	priv = view->priv;

	if (priv->show_offline || !gossip_roster_item_is_offline (item)) {
		show_item = TRUE;
	}

	for (l = gossip_roster_item_get_groups (item); l; l = l->next) {
		GossipRosterGroup *group = l->data;

		if (roster_view_find_item (view, &iter, item, group)) {
			GtkTreePath *path;

			if (!show_item) {
				if (priv->show_active) {
					ShowActiveData *data;

					data = roster_view_item_active_new (view, 
									    item, 
									    group, 
									    TRUE);

					gossip_roster_item_set_active (item, TRUE);  
					
					g_timeout_add (ACTIVE_USER_SHOW_TIME, 
						       (GSourceFunc) roster_view_item_active_cb,
						       data);

					/* update roster */
					d(g_print ("Update item (show who has disconnected)!\n")); 
					path = gtk_tree_model_get_path (priv->model, &iter);
					gtk_tree_model_row_changed (priv->model, path, &iter); 
					gtk_tree_path_free (path);						
				} else {
					/* Remove item */
					d(g_print ("Remove item!\n"));
					roster_view_remove_item (view, item, group);
				}
			} else { 
				if (priv->show_active) {
					ShowActiveData *data;
					
					data = roster_view_item_active_new (view, 
									    item, 
									    group, 
									    FALSE);
									
					gossip_roster_item_set_active (item, TRUE);  
					
					g_timeout_add (ACTIVE_USER_SHOW_TIME, 
						       (GSourceFunc) roster_view_item_active_cb,
						       data);
				}

				/* update roster */
				d(g_print ("Update item!\n"));
				path = gtk_tree_model_get_path (priv->model, &iter);
				gtk_tree_model_row_changed (priv->model, path, &iter); 
				gtk_tree_path_free (path);
			}

			continue;
		} else {
			if (priv->show_active) {
				ShowActiveData *data;

				data = roster_view_item_active_new (view, 
								    item, 
								    group, 
								    FALSE);
				
				gossip_roster_item_set_active (item, TRUE);   

				g_timeout_add (ACTIVE_USER_SHOW_TIME,  
					       (GSourceFunc) roster_view_item_active_cb, 
					       data);
			}

			if (show_item) {
				/* Add item */
				d(g_print ("Add item!\n"));
				roster_view_add_item (view, item, group);
			}

			continue;
		}	
	}
}

static ShowActiveData *
roster_view_item_active_new (GossipRosterView  *view, 
			     GossipRosterItem  *item,
			     GossipRosterGroup *group,
			     gboolean           remove)
{
	ShowActiveData *data;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (item != NULL, NULL);
	g_return_val_if_fail (group != NULL, NULL);

	data = g_new0 (ShowActiveData, 1);
	
	data->view = g_object_ref (view);
	
	data->item = gossip_roster_item_ref (item);
	data->group = gossip_roster_group_ref (group);

	data->remove = remove;

	return data;
}

static void
roster_view_item_active_free (ShowActiveData *data)
{
	g_return_if_fail (data != NULL);

	g_object_unref (data->view);

	gossip_roster_item_unref (data->item);
	gossip_roster_group_unref (data->group);

	g_free (data);
}

static gboolean
roster_view_item_active_cb (ShowActiveData *data)
{
	GossipRosterViewPriv *priv;

	g_return_val_if_fail (data != NULL, FALSE);

	priv = data->view->priv;

	if (data->remove &&
	    gossip_roster_item_is_offline (data->item)) {
		/* Remove item */
		d(g_print ("Remove item!\n"));
		roster_view_remove_item (data->view, 
					 data->item, 
					 data->group);
	} else {
		GtkTreeIter  iter;

		if (roster_view_find_item (data->view, 
					   &iter, 
					   data->item, 
					   data->group)) {
			GtkTreePath *path = gtk_tree_model_get_path (priv->model, &iter);
			
			gtk_tree_model_row_changed (priv->model, path, &iter); 
			gtk_tree_path_free (path);
		}
	}

	if (!gossip_roster_item_is_offline (data->item)) {
		gossip_roster_item_set_active (data->item, FALSE);  
	}

	roster_view_item_active_free (data);

	return FALSE;
}

static void
roster_view_item_removed (GossipRoster     *roster,
			  GossipRosterItem *item,
			  GossipRosterView *view)
{
	GossipRosterViewPriv *priv;
	GList                *l;

	priv = view->priv;

	d(g_print ("Item removed: %s\n", gossip_roster_item_get_name (item)));
	
	for (l = gossip_roster_item_get_groups (item); l; l = l->next) {
		GossipRosterGroup *group = l->data;

		roster_view_remove_item (view, item, group);
	}
}

static void
roster_view_group_added (GossipRoster      *roster,
			 GossipRosterGroup *group,
			 GossipRosterView  *view)
{
	GossipRosterViewPriv *priv;
	GtkTreeIter           iter;
	GtkTreePath          *path;
	RosterElement        *e;

	g_return_if_fail (GOSSIP_IS_ROSTER (roster));
	g_return_if_fail (GOSSIP_IS_ROSTER_VIEW (view));

	priv = view->priv;
	
	d(g_print ("Group added: %s\n", gossip_roster_group_get_name (group)));

	e = g_new0 (RosterElement, 1);
	e->group = gossip_roster_group_ref (group);
	
	gtk_tree_store_append (GTK_TREE_STORE (priv->model),
			       &iter, 
			       NULL);
	gtk_tree_store_set (GTK_TREE_STORE (priv->model),
			    &iter,
			    COL_IS_GROUP, TRUE,
			    COL_ELEMENT, e,
			    -1);

	path = gtk_tree_model_get_path (priv->model, &iter);
	gtk_tree_view_expand_row (GTK_TREE_VIEW (view), path, FALSE);
	gtk_tree_path_free (path);
}

static void
roster_view_group_removed (GossipRoster      *roster,
			   GossipRosterGroup *group,
			   GossipRosterView  *view)
{
	GossipRosterViewPriv *priv;
	GtkTreeIter           iter;
	gboolean              is_group;
	RosterElement        *e;

	g_return_if_fail (GOSSIP_IS_ROSTER (roster));
	g_return_if_fail (GOSSIP_IS_ROSTER_VIEW (view));

	priv = view->priv;
	
	d(g_print ("Group removed: %s\n", gossip_roster_group_get_name (group)));

	if (!roster_view_find_group (view, &iter, 
				     gossip_roster_group_get_name (group))) {
		d(g_assert_not_reached());
		return;
	}

	gtk_tree_model_get (priv->model, &iter,
			    COL_IS_GROUP, &is_group,
			    COL_ELEMENT, &e,
			    -1);

	if (!is_group) {
		return;
	}
	
	gtk_tree_store_remove (GTK_TREE_STORE (priv->model), &iter);

	gossip_roster_group_unref (e->group);
	g_free (e);
}

static gint
roster_view_iter_compare_func (GtkTreeModel *model,
			       GtkTreeIter  *iter_a,
			       GtkTreeIter  *iter_b,
			       gpointer      user_data)
{
	RosterElement     *e1, *e2;
	gboolean           a_is_group, b_is_group;
	const gchar       *name1, *name2;

	gtk_tree_model_get (model, iter_a, 
			    COL_IS_GROUP, &a_is_group,
			    COL_ELEMENT, &e1,
			    -1);
	gtk_tree_model_get (model, iter_b,
			    COL_IS_GROUP, &b_is_group,
			    COL_ELEMENT, &e2,
			    -1);

	/* Put items before groups */
	if (a_is_group && !b_is_group) {
		return 1;
	}

	if (!a_is_group && b_is_group) {
		return -1;
	}
	
	if (!a_is_group) {
		name1 = gossip_roster_item_get_name (e1->item);
		name2 = gossip_roster_item_get_name (e2->item);
	} else {
		name1 = gossip_roster_group_get_name (e1->group);
		name2 = gossip_roster_group_get_name (e2->group);
	}

	return g_ascii_strcasecmp (name1, name2);
}

static void  
roster_view_pixbuf_cell_data_func (GtkTreeViewColumn *tree_column,
				   GtkCellRenderer   *cell,
				   GtkTreeModel      *model,
				   GtkTreeIter       *iter,
				   GossipRosterView  *view)
{
	GossipRosterViewPriv *priv;
	gboolean              is_group;
	GdkPixbuf            *pixbuf;
	RosterElement        *e;
	FlashData            *flash;
	GtkTreeIter           parent;

	g_return_if_fail (GOSSIP_IS_ROSTER_VIEW (view));

	priv = view->priv;
	
	gtk_tree_model_get (model, iter, 
			    COL_IS_GROUP, &is_group,
			    COL_ELEMENT, &e,
			    -1);

	if (is_group) {
		g_object_set (cell, 
			      "visible", FALSE,
			      NULL);

		roster_view_set_cell_background (view, cell, TRUE);
		return;
	} 

	if (!gtk_tree_model_iter_parent (model, &parent, iter)) {
		pixbuf = gossip_utils_get_pixbuf_from_stock (GOSSIP_STOCK_MESSAGE);
	} else {
		flash = g_hash_table_lookup (priv->flash_table, e->item);
		
		if (flash && flash->flash_on) {
			pixbuf = gossip_utils_get_pixbuf_from_stock (GOSSIP_STOCK_MESSAGE);  
		} else if (gossip_roster_item_is_offline (e->item)) {
			pixbuf = gossip_utils_get_pixbuf_offline ();
		} else {
			GossipShow show = gossip_roster_item_get_show (e->item);
		
			pixbuf = gossip_utils_get_pixbuf_from_show (show);
		}
	}

	if (gossip_roster_item_get_active (e->item)) {
		roster_view_set_cell_background (view, cell, FALSE);
	} else {
		roster_view_set_cell_background (view, cell, TRUE);
	}

	g_object_set (cell,
		      "visible", TRUE,
		      "pixbuf", pixbuf,
		      NULL);

	if (pixbuf) {
		g_object_unref (pixbuf);
	}
}

static void
ellipsize_string (gchar *str, gint len)
{
	gchar *tmp;

	if (g_utf8_strlen (str, -1) > len + 4) {
		tmp = g_utf8_offset_to_pointer (str, len);

		tmp[0] = '.';
		tmp[1] = '.';
		tmp[2] = '.';
		tmp[3] = '\0';
	}
}

#define ELLIPSIS_MIN 6
#define ELLIPSIS_MAX 100
#define TREE_INDENT 30

static void
roster_view_ellipsize_item_string (GossipRosterView *view,
				   gchar            *str,
				   gint              width,
				   gboolean          smaller) 
{
	PangoLayout    *layout;
	PangoRectangle  rect;
	gint            len_str;
	gint            width_str;
	PangoAttrList  *attr_list = NULL;
	PangoAttribute *attr_size, *attr_style;
	
	len_str = g_utf8_strlen (str, -1);

	if (len_str < ELLIPSIS_MIN) {
		return;
	}

	len_str = MIN (len_str, ELLIPSIS_MAX);

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (view), NULL);
	
	pango_layout_set_text (layout, str, -1);
	pango_layout_get_extents (layout, NULL, &rect);
	width_str = rect.width / PANGO_SCALE;

	if (smaller) {
		/* Do the same as pango markup does for "smaller". */
		attr_list = pango_attr_list_new ();

		attr_style = pango_attr_style_new (PANGO_STYLE_ITALIC);
		attr_style->start_index = 0;
		attr_style->end_index = -1;
		pango_attr_list_insert (attr_list, attr_style);
		
		attr_size = pango_attr_size_new (
			pango_font_description_get_size (GTK_WIDGET (view)->style->font_desc) / 1.2);
		attr_size->start_index = 0;
		attr_size->end_index = -1;
		pango_attr_list_insert (attr_list, attr_size);
	}
	
	while (len_str >= ELLIPSIS_MIN && width_str > width) {
		len_str--;
		ellipsize_string (str, len_str);
		
		pango_layout_set_text (layout, str, -1);
		if (smaller) {
			pango_layout_set_attributes (layout, attr_list);
		}
		pango_layout_get_extents (layout, NULL, &rect);
		
		width_str = rect.width / PANGO_SCALE;
	}

	if (smaller) {
		pango_attr_list_unref (attr_list);
	}
	
	g_object_unref (layout);
}


static void
roster_view_set_cell_background (GossipRosterView *view,
			    GtkCellRenderer  *cell, 
			    gboolean          use_default)
{
	GdkColor  color;
	GtkStyle *style;
	
	g_return_if_fail (view != NULL);
	g_return_if_fail (cell != NULL);

	if (use_default) {
		g_object_set (cell, 
			      "cell-background-gdk", NULL, 
			      NULL);
	
		return;
	}

	style = gtk_widget_get_style (GTK_WIDGET (view));

/* 	color = style->base[GTK_STATE_SELECTED];  */
/* 	color = style->text_aa[GTK_STATE_NORMAL];  */
	color = style->bg[GTK_STATE_SELECTED];

	/* Here we take the current theme colour and add it to
	   the colour for white and average the two. This
	   gives a colour which is inline with the theme but
	   slightly whiter. */ 
	color.red = (color.red + (style->white).red) / 2;
	color.green = (color.green + (style->white).green) / 2;
	color.blue = (color.blue + (style->white).blue) / 2;

	g_object_set (cell, 
		      "cell-background-gdk", &color, 
		      NULL);
}

/* NOTE: We should write our own cell renderer instead of putting all these
 * nasty hacks here.
 */
static void  
roster_view_name_cell_data_func (GtkTreeViewColumn *tree_column,
				 GtkCellRenderer   *cell,
				 GtkTreeModel      *model,
				 GtkTreeIter       *iter,
				 GossipRosterView  *view)
{
	GossipRosterViewPriv *priv;
	gboolean              is_group;
	RosterElement        *e;
	GtkTreeIter           parent;
	const gchar          *tmp;
	gchar                *status, *name;
	gchar                *str;
	PangoAttrList        *attr_list;
	PangoAttribute       *attr_color, *attr_style, *attr_size;
	GtkStyle             *style;
	GdkColor              color;
	GtkTreeSelection     *selection;
	gint                  width;

	priv = view->priv;

	gtk_tree_model_get (model, iter, 
			    COL_IS_GROUP, &is_group, 
			    COL_ELEMENT, &e,
			    -1);

	if (is_group) {
		g_object_set (cell,
			      "attributes", NULL,
			      "weight", PANGO_WEIGHT_BOLD,
			      "text", gossip_roster_group_get_name (e->group),
			      NULL);

		roster_view_set_cell_background (view, cell, TRUE);
		return;
	} 

	if (gossip_roster_item_get_active (e->item)) {	
		roster_view_set_cell_background (view, cell, FALSE);
	} else {
		roster_view_set_cell_background (view, cell, TRUE);
	}

	/* FIXME: Figure out how to calculate the offset instead of
	 * hardcoding it here (icon width + padding + indentation).
	 */
	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &width, NULL);
	width = GTK_WIDGET (view)->allocation.width - (width + 4*2);

	if (!gtk_tree_model_iter_parent (model, &parent, iter)) {
		/* Inbox */
		name = g_strdup (gossip_roster_item_get_name (e->item));
		g_strdelimit (name, "\n\r\t", ' ');

		roster_view_ellipsize_item_string (view, name, width, FALSE);

		g_object_set (cell,
			      "attributes", NULL,
			      "weight", PANGO_WEIGHT_NORMAL,
			      "text", name,
			      NULL);

		g_free (name);
		return;
	}

	width -= TREE_INDENT;
	
	tmp = gossip_roster_item_get_status (e->item);

	if (!tmp || strcmp (tmp, "") == 0) {
		GossipShow show;

		if (!gossip_roster_item_is_offline (e->item)) {
			show = gossip_roster_item_get_show (e->item);
			status = g_strdup (gossip_utils_get_default_status (show));
		} else {
			status = g_strdup (_("Offline"));
		}
	} else {
		status = g_strdup (tmp);
	}

	name = g_strdup (gossip_roster_item_get_name (e->item));

	g_strdelimit (name, "\n\r\t", ' ');
	g_strdelimit (status, "\n\r\t", ' ');

	roster_view_ellipsize_item_string (view, name, width, FALSE);
	roster_view_ellipsize_item_string (view, status, width, TRUE);
	
	str = g_strdup_printf ("%s\n%s", name, status);

	style = gtk_widget_get_style (GTK_WIDGET (view));
	color = style->text_aa[GTK_STATE_NORMAL];

	attr_list = pango_attr_list_new ();

	attr_style = pango_attr_style_new (PANGO_STYLE_ITALIC);
	attr_style->start_index = strlen (name) + 1;
	attr_style->end_index = -1;
	pango_attr_list_insert (attr_list, attr_style);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

	if (!gtk_tree_selection_iter_is_selected (selection, iter)) {
		attr_color = pango_attr_foreground_new (color.red, color.green, color.blue);
		attr_color->start_index = attr_style->start_index;
		attr_color->end_index = -1;
		pango_attr_list_insert (attr_list, attr_color);
	}

	/* Do the same as pango markup does for "smaller". */
	attr_size = pango_attr_size_new (pango_font_description_get_size (style->font_desc) / 1.2);
	attr_size->start_index = attr_style->start_index;
	attr_size->end_index = -1;
	pango_attr_list_insert (attr_list, attr_size);
	
	g_object_set (cell,
		      "weight", PANGO_WEIGHT_NORMAL,
		      "text", str,
		      "attributes", attr_list,
		      NULL);

	pango_attr_list_unref (attr_list);

	g_free (name);
	g_free (status);
	g_free (str);
}

typedef struct {
	GossipRosterView  *view;
	const gchar       *name;
	gboolean           found;
	GtkTreeIter        found_iter;
} FindGroupData;

static gboolean
roster_view_find_group_cb (GtkTreeModel  *model,
			   GtkTreePath   *path,
			   GtkTreeIter   *iter,
			   FindGroupData *data)
{
	gboolean       is_group;
	RosterElement *e;
	
	gtk_tree_model_get (model, iter, 
			    COL_IS_GROUP, &is_group,
			    COL_ELEMENT, &e,
			    -1);
	
	if (!is_group) {
		return FALSE;
	}

	if (strcmp (gossip_roster_group_get_name (e->group), data->name) == 0) {
		data->found = TRUE;
		data->found_iter = *iter;
		return TRUE;
	}

	return FALSE;
}

static gboolean
roster_view_find_group (GossipRosterView  *view, 
			GtkTreeIter       *iter,
			const gchar       *name)
{
	GossipRosterViewPriv *priv;
	FindGroupData         data;

	priv = view->priv;

	data.view  = view;
	data.name  = name;
	data.found = FALSE;
	
	gtk_tree_model_foreach (priv->model,
				(GtkTreeModelForeachFunc) roster_view_find_group_cb,
				&data);

	if (data.found) {
		*iter = data.found_iter;
		return TRUE;
	}

	return FALSE;
}

static gboolean
roster_view_iter_equal_item (GtkTreeModel     *model,
			     GtkTreeIter      *iter,
			     GossipRosterItem *item) 
{
	gboolean       is_group;
	RosterElement *e;
	GossipJID     *jid_a, *jid_b;
	
	gtk_tree_model_get (model, iter, 
			    COL_IS_GROUP, &is_group,
			    COL_ELEMENT, &e,
			    -1);
	
	if (is_group) {
		return FALSE;
	}

	jid_a = gossip_roster_item_get_jid (e->item);
	jid_b = gossip_roster_item_get_jid (item);

	if (gossip_jid_equals_without_resource (jid_a, jid_b)) {
		return TRUE;
	}

	return FALSE;
}

static void
roster_view_drag_begin (GtkWidget      *widget,
			GdkDragContext *context,
			gpointer        user_data)
{
	GossipRosterViewPriv *priv;
	GtkTreeSelection     *selection;
	GtkTreeModel         *model;
	GtkTreePath          *path;
	GtkTreeIter           iter;

	priv = GOSSIP_ROSTER_VIEW (widget)->priv;
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return;
	}

	path = gtk_tree_model_get_path (model, &iter);
	priv->drag_row = gtk_tree_row_reference_new (model, path);
	gtk_tree_path_free (path);

	/* FIXME: Set a nice icon */
}

static void
roster_view_drag_data_get (GtkWidget             *widget,
			   GdkDragContext        *contact,
			   GtkSelectionData      *selection,
			   guint                  info,
			   guint                  time,
			   gpointer               user_data)
{
	GossipRosterViewPriv *priv;
	GtkTreePath          *src_path;
	GtkTreeIter           iter;
	GtkTreeModel         *model;
	RosterElement        *e;
	gboolean              is_group;
	GossipJID            *jid;
	const gchar          *str_jid;
	
	priv = GOSSIP_ROSTER_VIEW (widget)->priv;
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	if (!priv->drag_row) {
		return;
	}

	src_path = gtk_tree_row_reference_get_path (priv->drag_row);
	if (!src_path) {
		return;
	}

	if (!gtk_tree_model_get_iter (model, &iter, src_path)) {
		gtk_tree_path_free (src_path);
		return;
	}

	gtk_tree_path_free (src_path);

	gtk_tree_model_get (model, &iter,
			    COL_IS_GROUP, &is_group,
			    COL_ELEMENT, &e,
			    -1);
	
	if (is_group) {
		return;
	}

	jid = gossip_roster_item_get_jid (e->item);
	str_jid = gossip_jid_get_full (jid);
	
	switch (info) {
	case DND_DRAG_TYPE_JID:
		gtk_selection_data_set (selection, drag_atoms[info], 8, 
					str_jid, strlen (str_jid) + 1);
		break;
	default:
		return;
	}
	
}

static void
roster_view_drag_end (GtkWidget      *widget,
		      GdkDragContext *context,
		      gpointer        user_data)
{
	GossipRosterViewPriv *priv;

	priv = GOSSIP_ROSTER_VIEW (widget)->priv;

	if (priv->drag_row) {
		gtk_tree_row_reference_free (priv->drag_row);
		priv->drag_row = NULL;
	}
}


static gboolean
roster_view_find_item (GossipRosterView  *view,
		       GtkTreeIter       *iter,
		       GossipRosterItem  *item, 
		       GossipRosterGroup *group)
{
	GossipRosterViewPriv *priv;
	GtkTreeIter           i;

	priv = view->priv;
	
	if (group) {
		GtkTreeIter  parent;
		const gchar *group_name;

		group_name = gossip_roster_group_get_name (group);
		
		if (!roster_view_find_group (view, &parent, group_name)) {
			return FALSE;
		}
		
		if (!gtk_tree_model_iter_children (priv->model, &i, &parent)) {
			return FALSE;
		}
	} else {
		if (!gtk_tree_model_iter_children (priv->model, &i, NULL)) {
			return FALSE;
		}
	}

	do {
		if (roster_view_iter_equal_item (priv->model, &i, item)) {
			*iter = i;
			return TRUE;
		}
	} while (gtk_tree_model_iter_next (priv->model, &i));

	return FALSE;
}

static gboolean 
roster_view_button_press_event_cb (GossipRosterView *view,
				   GdkEventButton   *event,
				   gpointer          data)
{
	GossipRosterViewPriv *priv;

	priv = view->priv;
	
	if (event->button == 3) {
		GtkTreePath      *path;
		GtkItemFactory   *factory;
		GtkTreeSelection *selection;
		GtkTreeModel     *model;
		GtkTreeIter       iter;
		gboolean          row_exists;
		
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
		model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
		
		gtk_widget_grab_focus (GTK_WIDGET (view));

		row_exists = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (view),
							    event->x, event->y, 
							    &path, 
							    NULL, NULL, NULL);
		if (row_exists) {
			gboolean       is_group; 
			RosterElement *e;
			
			gtk_tree_selection_unselect_all (selection);
			gtk_tree_selection_select_path (selection, path);

			gtk_tree_model_get_iter (model, &iter, path);
			gtk_tree_path_free (path);

			gtk_tree_model_get (model, &iter,
					    COL_IS_GROUP, &is_group,
					    COL_ELEMENT, &e,
					    -1);

			if (is_group) {
				d(g_print ("This is a group!\n"));
				factory = priv->group_popup_factory;
			} else {
				gboolean   log_exists;
				GtkWidget *w;
				
				factory = priv->item_popup_factory;
				w = gtk_item_factory_get_item (factory,
							       "/Show Log");
				log_exists = gossip_log_exists (gossip_roster_item_get_jid (e->item));
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
roster_view_row_activated_cb (GossipRosterView  *view,
			      GtkTreePath       *path,
			      GtkTreeViewColumn *col,
			      gpointer           data)
{
	GossipRosterViewPriv *priv;
	GtkTreeIter           iter;
	gboolean              is_group;
	RosterElement        *e;
	GtkTreeModel         *model;

	priv = view->priv;
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
	
	gtk_tree_model_get_iter (model, &iter, path);
	
	gtk_tree_model_get (model, &iter, 
			    COL_IS_GROUP, &is_group,
			    COL_ELEMENT, &e,
			    -1);

	d(g_print ("Row activated!\n"));
	
	if (!is_group) {
		GossipContact *contact;

		contact = gossip_roster_get_contact_from_item (priv->roster,
							       e->item);
		g_signal_emit (view, signals[CONTACT_ACTIVATED], 0, contact);
	}
}

static void
roster_view_item_menu_remove_cb (gpointer   data,
				 guint      action,
				 GtkWidget *widget)
{
	GossipRosterView     *view;
	GossipRosterViewPriv *priv;
	GossipRosterItem     *item;
	GossipJID            *jid;
	gchar                *str;
	GtkWidget            *dialog;
	gint                  response;
	
	view = GOSSIP_ROSTER_VIEW (data);
	priv = view->priv;

	item = gossip_roster_view_get_selected_item (view);
	if (!item) {
		return;
	}

	jid = gossip_roster_item_get_jid (item);
	str = g_strdup_printf ("<b>%s</b>", 
			       gossip_jid_get_without_resource (jid));

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

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (gossip_app_get_window ()));
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (response != GTK_RESPONSE_YES) {
		return;
	}

	gossip_roster_remove_item (priv->roster, item);
}

static void
roster_view_item_menu_info_cb (gpointer   data,
			       guint      action,
			       GtkWidget *widget)
{
	GossipRosterItem *item;

	item = gossip_roster_view_get_selected_item (GOSSIP_ROSTER_VIEW (data));
	if (!item) {
		return;
	}
	
	gossip_contact_info_new (gossip_roster_item_get_jid (item),
				 gossip_roster_item_get_name (item));
}

static void
roster_view_item_menu_log_cb (gpointer   data,
			       guint      action,
			       GtkWidget *widget)
{
	GossipRosterItem *item;

	item = gossip_roster_view_get_selected_item (GOSSIP_ROSTER_VIEW (data));
	if (!item) {
		return;
	}
	
	gossip_log_show (gossip_roster_item_get_jid (item));	
	
}

static void
roster_view_rename_activate_cb (GtkWidget *entry, GtkDialog *dialog)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}


static void
roster_view_item_menu_rename_cb (gpointer   data,
				 guint      action,
				 GtkWidget *widget)
{
	GossipRosterView     *view;
	GossipRosterViewPriv *priv;
	GossipRosterItem     *item;
	GossipJID            *jid;
	gchar                *str;
	GtkWidget            *dialog;
	GtkWidget            *entry;
	GtkWidget            *hbox;
	
	view = GOSSIP_ROSTER_VIEW (data);
	priv = view->priv;

	item = gossip_roster_view_get_selected_item (view);
	if (!item) {
		return;
	}

	jid = gossip_roster_item_get_jid (item);
	str = g_strdup_printf ("<b>%s</b>", 
			       gossip_jid_get_without_resource (jid));

	/* Translator: %s denotes the Jabber ID */
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
			    gossip_roster_item_get_name (item));
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
	
	g_signal_connect (entry,
			  "activate",
			  G_CALLBACK (roster_view_rename_activate_cb),
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
	
	gossip_roster_rename_item (priv->roster, item, str);
}

static void
roster_view_item_menu_edit_groups_cb (gpointer   data,
				      guint      action,
				      GtkWidget *widget)
{
	GossipRosterItem *item;

	item = gossip_roster_view_get_selected_item (GOSSIP_ROSTER_VIEW (data));
	if (!item) {
		return;
	}

	gossip_edit_groups_new (gossip_roster_item_get_jid (item),
				gossip_roster_item_get_name (item),
				gossip_roster_item_get_groups (item));
}

static void 
roster_view_group_menu_rename_cb (gpointer   data,
				  guint      action,
				  GtkWidget *widget)
{
	GossipRosterView     *view;
	GossipRosterViewPriv *priv;
	gboolean              is_group;
	RosterElement        *e;
	gchar                *str, *tmp;
	GtkTreeSelection     *selection;
	GtkTreeIter           iter;
	GtkTreeModel         *model;
	GtkWidget            *dialog;
	GtkWidget            *entry;
	GtkWidget            *hbox;
	
	view = GOSSIP_ROSTER_VIEW (data);
	priv = view->priv;
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return;
	}

	gtk_tree_model_get (model, &iter,
			    COL_IS_GROUP, &is_group,
			    COL_ELEMENT, &e,
			    -1);

	if (!is_group) {
		return;
	}

	tmp = g_markup_escape_text (gossip_roster_group_get_name (e->group), -1);
	str = g_strdup_printf ("<b>%s</b>", tmp);
	g_free (tmp);

	/* Translator: %s denotes the group name */
	dialog = gtk_message_dialog_new (GTK_WINDOW (gossip_app_get_window ()),
					 0,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_OK_CANCEL,
					 _("Please enter a new name for the group\n%s"),
					 str);
	
	g_free (str);

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	
	g_object_set (GTK_MESSAGE_DIALOG (dialog)->label,
		      "use-markup", TRUE,
		      NULL);

	entry = gtk_entry_new ();
	gtk_widget_show (entry);

	gtk_entry_set_text (GTK_ENTRY (entry),  
			    gossip_roster_group_get_name (e->group));
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
	
	g_signal_connect (entry,
			  "activate",
			  G_CALLBACK (roster_view_rename_activate_cb),
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
	
	gossip_roster_rename_group (priv->roster, e->group, str);
}

static gchar *
roster_view_item_factory_translate_func (const gchar *path, gpointer data)
{
	return _((gchar *) path);
}

static void
roster_view_flash_free_data (FlashData *data)
{
	g_return_if_fail (data != NULL);

	if (data->flash_timeout_id) {
		g_source_remove (data->flash_timeout_id);
	}

	g_free (data);
}

#define EXPAND 1

static void
roster_view_add_item (GossipRosterView  *view,
		      GossipRosterItem  *item,
		      GossipRosterGroup *group)
{
	GossipRosterViewPriv *priv;
	GtkTreeIter           iter, group_iter;
	RosterElement        *e;
#if EXPAND
	gboolean              expand = FALSE;
#endif

	priv = view->priv;

	d(g_print ("Adding item: [%s] to group: [%s]\n", 
		   gossip_roster_item_get_name (item),
		   gossip_roster_group_get_name (group)));

	// koko
	if (!priv->show_offline && gossip_roster_item_is_offline (item)) {
		d(g_print ("Offline item: %s\n",
			   gossip_roster_item_get_name (item)));
		return;
	}
	
	if (!group) {
		gtk_tree_store_append (GTK_TREE_STORE (priv->model),
				       &iter, 
				       NULL);
	} else {
		const gchar *name;

		name = gossip_roster_group_get_name (group);

		if (!roster_view_find_group (view, &group_iter, name)) {
			roster_view_group_added (priv->roster, group, view);
		
			if (!roster_view_find_group (view, &group_iter, name)) {
				d(g_assert_not_reached ());
				return;
			}
		}

#if EXPAND
		if (!gtk_tree_model_iter_has_child (priv->model, &group_iter)) {
			expand = TRUE;
		}
#endif

		gtk_tree_store_append (GTK_TREE_STORE (priv->model),
				       &iter,
				       &group_iter);
	}

	e = g_new0 (RosterElement, 1);
	e->item = gossip_roster_item_ref (item);
 
	gtk_tree_store_set (GTK_TREE_STORE (priv->model),
			    &iter,
			    COL_IS_GROUP, FALSE,
			    COL_ELEMENT, e,
			    -1);
#if EXPAND
	if (expand) {
		GtkTreePath *path;

		path = gtk_tree_model_get_path (priv->model,
						&group_iter);
		if (path) {
			gtk_tree_view_expand_row (GTK_TREE_VIEW (view),
						  path, FALSE);
			gtk_tree_path_free (path);
		}
	}
#endif
}

static gboolean
roster_view_remove_item_with_iter (GossipRosterView  *view,
				   GtkTreeIter       *iter,
				   GossipRosterItem  *item,
				   GossipRosterGroup *group)
{
	GossipRosterViewPriv *priv;
	GtkTreeIter           parent;
	gboolean              ret;
	gboolean              is_group;
	RosterElement        *e;
	gint                  n_children;

	priv = view->priv;

	gtk_tree_model_get (priv->model, iter,
			    COL_IS_GROUP, &is_group,
			    COL_ELEMENT, &e,
			    -1);

	if (is_group) {
		return TRUE;
	}
	
	if (gtk_tree_model_iter_parent (priv->model, &parent, iter)) {
		n_children = gtk_tree_model_iter_n_children (priv->model,
							     &parent);
	} else {
		n_children = 0;
	}
			
	g_hash_table_remove (priv->flash_table, item);
	
	ret = gtk_tree_store_remove (GTK_TREE_STORE (priv->model), iter);

	gossip_roster_item_unref (e->item);
	g_free (e);

	/* If the group had one item, it's empty now so remove it. */
	if (n_children == 1) {
		roster_view_group_removed (priv->roster, group, view);
	}

	return ret;
}

static void
roster_view_remove_item (GossipRosterView  *view,
			 GossipRosterItem  *item,
			 GossipRosterGroup *group)
{
	GossipRosterViewPriv *priv;
	GtkTreeIter           iter;

	priv = view->priv;
	
	d(g_print ("Removing item: [%s] from group: [%s]\n", 
		   gossip_roster_item_get_name (item),
		   gossip_roster_group_get_name (group)));
			
	if (!roster_view_find_item (view, &iter, item, group)) {
		return;
	}

	roster_view_remove_item_with_iter (view, &iter, item, group);
}

GossipRosterView *
gossip_roster_view_new (GossipRoster *roster)
{
	GossipRosterView *view;
	GossipRosterViewPriv *priv;

	view = g_object_new (GOSSIP_TYPE_ROSTER_VIEW, NULL);
	priv = view->priv;
	
	if (roster) {
		priv->roster = g_object_ref (roster);
	} else {
		priv->roster = gossip_roster_new ();
	}
	
	g_signal_connect (priv->roster,
			  "item_added",
			  G_CALLBACK (roster_view_item_added),
			  view);
	g_signal_connect (priv->roster,
			  "item_updated",
			  G_CALLBACK (roster_view_item_updated),
			  view);
	g_signal_connect (priv->roster,
			  "item_presence_updated",
			  G_CALLBACK (roster_view_item_presence_updated),
			  view);
	g_signal_connect (priv->roster,
			  "item_removed",
			  G_CALLBACK (roster_view_item_removed),
			  view);

	return view;
}

GossipRosterItem * 
gossip_roster_view_get_selected_item (GossipRosterView *view)
{
	GossipRosterViewPriv *priv;
	GtkTreeSelection     *selection;
	GtkTreeIter           iter;
	GtkTreeModel         *model;
	gboolean              is_group;
	RosterElement        *e;
	
	g_return_val_if_fail (GOSSIP_IS_ROSTER_VIEW (view), NULL);
	
	priv = view->priv;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter,
			    COL_IS_GROUP, &is_group,
			    COL_ELEMENT, &e,
			    -1);

	if (!is_group) {
		return e->item;
	}

	return NULL;
}

typedef struct {
	GossipRosterView *view;
	GossipRosterItem *item;
} FlashTimeoutData;

static void
roster_view_free_flash_timeout_data (FlashTimeoutData *fdata) 
{
	g_return_if_fail (fdata != NULL);

	gossip_roster_item_unref (fdata->item);

	g_free (fdata);
}

static gboolean
roster_view_flash_timeout_func (FlashTimeoutData *fdata)
{
	GossipRosterView     *view;
	GossipRosterViewPriv *priv;
	FlashData            *flash;
	GtkTreeIter           iter;
	GtkTreePath          *path;
	gboolean              ret_val;
	GList                *l;
	
	view = fdata->view;
	priv = view->priv;

	flash = g_hash_table_lookup (priv->flash_table, fdata->item);
	if (!flash) {
		ret_val = FALSE;
	} else {
		flash->flash_on = !flash->flash_on;
		ret_val = TRUE;
	}

	for (l = gossip_roster_item_get_groups (fdata->item); l; l = l->next) {
		GossipRosterGroup *group = l->data;

		if (!roster_view_find_item (view, &iter, fdata->item, group)) {
			continue;
		}
		
		path = gtk_tree_model_get_path (priv->model, &iter);
		gtk_tree_model_row_changed (priv->model, path, &iter);
		gtk_tree_path_free (path);
	}
	
	return ret_val;
}

void
gossip_roster_view_flash_contact (GossipRosterView *view,
				  GossipContact    *contact,
				  gboolean          flash)
{
	GossipRosterViewPriv *priv;
	FlashData            *flash_data;
	GossipRosterItem     *item;
	
	g_return_if_fail (GOSSIP_IS_ROSTER_VIEW (view));
	g_return_if_fail (contact != NULL);

	priv = view->priv;
	
	item = gossip_roster_get_item (priv->roster, 
				       gossip_contact_get_jid (contact));

	if (!item) {
		item = gossip_roster_item_new (gossip_contact_get_jid (contact));
	}
	
	flash_data = g_hash_table_lookup (priv->flash_table, item);

	if (flash && !flash_data) {
		FlashTimeoutData *fdata;

		fdata = g_new0 (FlashTimeoutData, 1);
		fdata->view = view;
		fdata->item = gossip_roster_item_ref (item);
		
		flash_data = g_new0 (FlashData, 1);
		flash_data->flash_on = TRUE;
		flash_data->flash_timeout_id =
			g_timeout_add_full (G_PRIORITY_DEFAULT, FLASH_TIMEOUT,
					    (GSourceFunc) roster_view_flash_timeout_func,
					    fdata,
					    (GDestroyNotify) roster_view_free_flash_timeout_data);
		g_hash_table_insert (priv->flash_table, 
				     gossip_roster_item_ref (item),
				     flash_data); 

		/* Add to inbox */
		roster_view_add_item (view, item, NULL);
	}
	else if (!flash) {
		if (flash_data) {
			g_hash_table_remove (priv->flash_table, item);
		}
		
		/* Remove from inbox */
		roster_view_remove_item (view, item, NULL);
	}
}

void
gossip_roster_view_set_show_offline (GossipRosterView *view,
				     gboolean show_offline)
{
	GossipRoster         *roster;
	GossipRosterViewPriv *priv;
	GList                *items, *i;
	gboolean              show_active;

	g_return_if_fail (GOSSIP_IS_ROSTER_VIEW (view));

	priv = view->priv;
	priv->show_offline = show_offline;
	
	roster = priv->roster;
	show_active = priv->show_active; /* remember */

	/* disable temporarily */
	priv->show_active = FALSE;
	
	items = gossip_roster_get_all_items (gossip_app_get_roster ());
	for (i = items; i; i = i->next) {
		GossipRosterItem *item = i->data;

		roster_view_item_presence_updated (roster, item, view);
	}
	g_list_free (items);

	/* restore to original setting */
	priv->show_active = show_active;
}

gboolean
gossip_roster_view_get_show_offline (GossipRosterView *view)
{
	GossipRosterViewPriv *priv;

	/* TODO: or true? */
	g_return_val_if_fail (GOSSIP_IS_ROSTER_VIEW (view), FALSE);

	priv = view->priv;
	
	return priv->show_offline;
}
