/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio HB
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

#include "eggtreemodelfilter.h"
#include "gossip-app.h"
#include "gossip-roster.h"
#include "gossip-marshal.h"
#include "gossip-contact-info.h"
#include "gossip-stock.h"
#include "gossip-utils.h"
#include "gossip-roster-view.h"
#include "gossip-log.h"

#define d(x) 

struct _GossipRosterViewPriv {
	GtkTreeModel   *model;
	GtkTreeModel   *filtered_model;
	GossipRoster   *roster;

	GtkItemFactory *item_popup_factory;
	GtkItemFactory *group_popup_factory;

	gboolean show_offline;
};

typedef struct {
	gboolean flash_on;
	guint    flash_timeout_id;
} FlashData;

static void  roster_view_class_init         (GossipRosterViewClass *klass);
static void  roster_view_init               (GossipRosterView      *view);
static void  roster_view_finalize           (GObject               *object); 
static GtkTreeModel * 
roster_view_create_store                    (GossipRosterView      *view);
static void roster_view_setup_tree          (GossipRosterView      *view);
static void  roster_view_item_updated       (GossipRoster          *roster,
					     GossipRosterItem      *item,
					     GossipRosterView      *view);
static void  roster_view_group_added        (GossipRoster          *roster,
					     GossipRosterGroup     *group,
					     GossipRosterView      *view);
static void  roster_view_group_removed      (GossipRoster          *roster,
					     GossipRosterGroup     *group,
					     GossipRosterView      *view);
static void  roster_view_group_item_added   (GossipRoster          *roster,
					     GossipRosterGroup     *group,
					     GossipRosterItem      *item,
					     GossipRosterView      *view);
static void  roster_view_group_item_removed (GossipRoster          *roster,
					     GossipRosterGroup     *group,
					     GossipRosterItem      *item,
					     GossipRosterView      *view);
static gint roster_view_iter_compare_func   (GtkTreeModel          *model,
					     GtkTreeIter           *iter_a,
					     GtkTreeIter           *iter_b,
					     gpointer               user_data);
static void  
roster_view_pixbuf_cell_data_func           (GtkTreeViewColumn     *tree_column,
					     GtkCellRenderer       *cell,
					     GtkTreeModel          *tree_model,
					     GtkTreeIter           *iter,
					     GossipRosterView      *view);
static void  
roster_view_name_cell_data_func             (GtkTreeViewColumn     *tree_column,
					     GtkCellRenderer       *cell,
					     GtkTreeModel          *tree_model,
					     GtkTreeIter           *iter,
					     GossipRosterView      *view);
static gboolean roster_view_find_group      (GossipRosterView      *view,
					     GtkTreeIter           *iter,
					     const gchar           *name);
static gboolean roster_view_find_item       (GossipRosterView      *view,
					     GtkTreeIter           *iter,
					     GossipRosterItem      *item,
					     GossipRosterGroup     *group);
static gboolean 
roster_view_button_press_event_cb           (GossipRosterView      *view,
					     GdkEventButton        *event,
					     gpointer               data);
static void roster_view_row_activated_cb    (GossipRosterView      *view,
					     GtkTreePath           *path,
					     GtkTreeViewColumn     *col,
					     gpointer               data);
static void     
roster_view_item_menu_remove_cb             (gpointer               data,
					     guint                  action,
					     GtkWidget             *widget);
static void
roster_view_item_menu_info_cb               (gpointer               data,
					     guint                  action,
					     GtkWidget             *widget);
static void 
roster_view_item_menu_log_cb               (gpointer               data,
					     guint                  action,
					     GtkWidget             *widget);
static void 
roster_view_item_menu_rename_cb             (gpointer               data,
					     guint                  action,
					     GtkWidget             *widget);
static void 
roster_view_group_menu_rename_cb            (gpointer               data,
					     guint                  action,
					     GtkWidget             *widget);
static gchar *
roster_view_item_factory_translate_func     (const gchar           *path,
					     gpointer               data);
static gboolean
roster_view_filter_visible_func             (GtkTreeModel          *model,
					     GtkTreeIter           *iter,
					     GossipRosterView      *view);

enum {
	ITEM_ACTIVATED,
	LAST_SIGNAL
};

enum {
	ITEM_MENU_NONE,
	ITEM_MENU_REMOVE,
	ITEM_MENU_INFO,
	ITEM_MENU_RENAME,
	ITEM_MENU_LOG
};

enum {
	GROUP_MENU_NONE,
	GROUP_MENU_RENAME
};

/* Tree model */
enum {
	COL_IS_GROUP,
	COL_POINTER,
	COL_FLASH,
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

GObjectClass *parent_class;
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

	signals[ITEM_ACTIVATED] = 
		g_signal_new ("item_activated",
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
	priv->filtered_model = egg_tree_model_filter_new (priv->model, NULL);

	gtk_tree_view_set_model (GTK_TREE_VIEW (view), priv->filtered_model);

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

	egg_tree_model_filter_set_visible_func (EGG_TREE_MODEL_FILTER (priv->filtered_model),
						(EggTreeModelFilterVisibleFunc) roster_view_filter_visible_func,
						view,
						NULL); 
	
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
						    G_TYPE_POINTER,
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

	priv = view->priv;
	
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);
	/* gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (view), TRUE); */

	col = gtk_tree_view_column_new ();
	
	cell = gtk_cell_renderer_pixbuf_new ();
	g_object_set (cell, "xpad", (guint) 2, NULL);
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (col, cell, 
						 (GtkTreeCellDataFunc) roster_view_pixbuf_cell_data_func,
						 view, 
						 NULL);

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "xpad", (guint) 0, NULL);
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (col, cell,
						 (GtkTreeCellDataFunc) roster_view_name_cell_data_func,
						 view,
						 NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (view), col);
}

static void
roster_view_item_updated (GossipRoster     *roster,
			  GossipRosterItem *item,
			  GossipRosterView *view)
{
	GossipRosterViewPriv *priv;
	GtkTreeIter           iter;
	GtkTreePath          *path;
	GList                *l;
	
	d(g_print ("Item updated: %s\n", gossip_roster_item_get_name (item)));

	priv = view->priv;

	for (l = gossip_roster_item_get_groups (item); l; l = l->next) {
		GossipRosterGroup *group = (GossipRosterGroup *) l->data;
		
		if (!roster_view_find_item (view, &iter, item, group)) {
			d(g_assert_not_reached ());
			return;
		}
	
		path = gtk_tree_model_get_path (priv->model, &iter);
		gtk_tree_model_row_changed (priv->model, path, &iter); 
		gtk_tree_path_free (path);
	}
}

static void
roster_view_group_added   (GossipRoster      *roster,
			   GossipRosterGroup *group,
			   GossipRosterView  *view)
{
	GossipRosterViewPriv *priv;
	GtkTreeIter           iter;
	GtkTreePath          *path;

	g_return_if_fail (GOSSIP_IS_ROSTER (roster));
	g_return_if_fail (GOSSIP_IS_ROSTER_VIEW (view));

	priv = view->priv;
	
	d(g_print ("Group added: %s\n", gossip_roster_group_get_name (group)));

	gtk_tree_store_append (GTK_TREE_STORE (priv->model),
			       &iter, 
			       NULL);
	gtk_tree_store_set (GTK_TREE_STORE (priv->model),
			    &iter,
			    COL_IS_GROUP, TRUE,
			    COL_POINTER, gossip_roster_group_ref (group),
			    COL_FLASH, NULL,
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

	g_return_if_fail (GOSSIP_IS_ROSTER (roster));
	g_return_if_fail (GOSSIP_IS_ROSTER_VIEW (view));

	priv = view->priv;
	
	d(g_print ("Group removed: %s\n", gossip_roster_group_get_name (group)));

	if (!roster_view_find_group (view, &iter, 
				     gossip_roster_group_get_name (group))) {
		d(g_assert_not_reached());
		return;
	}

	gtk_tree_store_remove (GTK_TREE_STORE (priv->model), &iter);
}

static void
roster_view_group_item_added (GossipRoster      *roster,
			      GossipRosterGroup *group,
			      GossipRosterItem  *item,
			      GossipRosterView  *view)
{
	GossipRosterViewPriv *priv;
	GtkTreeIter           group_iter;
	GtkTreeIter           iter;
	gboolean              expand = FALSE;
	const gchar          *name;
	
	d(g_print ("Item '%s' added to group '%s'\n",
		   gossip_jid_get_full (gossip_roster_item_get_jid(item)),
		   gossip_roster_group_get_name (group)));
		
	priv = view->priv;

	name = gossip_roster_group_get_name (group);
		
	if (!roster_view_find_group (view, &group_iter, name)) {
		d(g_assert_not_reached());
		return;
	}

	if (!gtk_tree_model_iter_has_child (priv->model, &group_iter)) {
		expand = TRUE;
	}

	gtk_tree_store_append (GTK_TREE_STORE (priv->model),
			       &iter,
			       &group_iter);

	gtk_tree_store_set (GTK_TREE_STORE (priv->model),
			    &iter,
			    COL_IS_GROUP, FALSE,
			    COL_POINTER, gossip_roster_item_ref (item),
			    COL_FLASH, NULL,
			    -1);

	if (expand) {
		GtkTreePath *path;

		path = gtk_tree_model_get_path (priv->model, &group_iter);
		if (path) {
			gtk_tree_view_expand_row (GTK_TREE_VIEW (view),
						  path, FALSE);
			gtk_tree_path_free (path);
		}
	}
}

static void
roster_view_group_item_removed (GossipRoster      *roster,
				GossipRosterGroup *group,
				GossipRosterItem  *item,
				GossipRosterView  *view)
{
	GossipRosterViewPriv *priv;
	GtkTreeIter           iter;
	FlashData            *flash;

	d(g_print ("Item '%s' removed from group '%s'\n", 
		   gossip_jid_get_full (gossip_roster_item_get_jid(item)),
		   gossip_roster_group_get_name (group)));
	
	priv = view->priv;
	
	if (!roster_view_find_item (view, &iter, item, group)) {
		d(g_assert_not_reached());
		return;
	}

	gtk_tree_model_get (priv->model, &iter,
			    COL_FLASH, &flash,
			    -1);

	if (flash) {
		if (flash->flash_timeout_id != 0) {
			g_source_remove (flash->flash_timeout_id);
		}
		g_free (flash);
	}

	gtk_tree_store_remove (GTK_TREE_STORE (priv->model), &iter);
	gossip_roster_item_unref (item);
}

static gint
roster_view_iter_compare_func (GtkTreeModel *model,
			       GtkTreeIter  *iter_a,
			       GtkTreeIter  *iter_b,
			       gpointer      user_data)
{
	gpointer a, b;
	gboolean is_group;
	GossipRosterGroup *group_a, *group_b;

	gtk_tree_model_get (model, iter_a, 
			    COL_IS_GROUP, &is_group,
			    COL_POINTER, &a,
			    -1);
	gtk_tree_model_get (model, iter_b,
			    COL_POINTER, &b,
			    -1);
	
	if (!is_group) {
		GossipRosterItem *item_a = (GossipRosterItem *) a;
		GossipRosterItem *item_b = (GossipRosterItem *) b;
		return g_ascii_strcasecmp (gossip_roster_item_get_name (item_a),
					   gossip_roster_item_get_name (item_b));
	}

	group_a = (GossipRosterGroup *) a;
	group_b = (GossipRosterGroup *) b;

	return g_ascii_strcasecmp (gossip_roster_group_get_name (group_a),
				   gossip_roster_group_get_name (group_b));
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
	GossipRosterItem     *item;
	FlashData            *flash;
	
	g_return_if_fail (GOSSIP_IS_ROSTER_VIEW (view));

	priv = view->priv;
	
	gtk_tree_model_get (model, iter, 
			    COL_IS_GROUP, &is_group, 
			    -1);

	if (is_group) {
		g_object_set (cell, "visible", FALSE, NULL);
		return;
	} 
		
	gtk_tree_model_get (model, iter, 
			    COL_POINTER, &item, 
			    COL_FLASH, &flash,
			    -1);

	if (flash && flash->flash_on) {
		pixbuf = gossip_utils_get_pixbuf_from_stock (GOSSIP_STOCK_MESSAGE); 
	} else if (gossip_roster_item_is_offline (item)) {
		pixbuf = gossip_utils_get_pixbuf_offline ();
	} else {
		GossipShow show = gossip_roster_item_get_show (item);
		
		pixbuf = gossip_utils_get_pixbuf_from_show (show);
	}

	g_object_set (cell,
		      "visible", TRUE,
		      "pixbuf", pixbuf,
		      NULL);
	g_object_unref (pixbuf);
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

#define ELLIPSIS_LIMIT 6

static void
roster_view_ellipsize_item_strings (GossipRosterView *view,
				    gchar            *name,
				    gchar            *status,
				    gint              width)
{
	PangoLayout    *layout;
	PangoRectangle  rect;
	gint            len_name, len_status;
	gint            width_name, width_status;

	len_name = g_utf8_strlen (name, -1);
	len_status = g_utf8_strlen (status, -1);

	/* Don't bother if we already have short strings. */
	if (len_name < ELLIPSIS_LIMIT && len_status < ELLIPSIS_LIMIT) {
		return;
	}
	
	layout = gtk_widget_create_pango_layout (GTK_WIDGET (view), NULL);
	
	pango_layout_set_text (layout, name, -1);
	pango_layout_get_extents (layout, NULL, &rect);
	width_name = rect.width / PANGO_SCALE;

	/* Note: if we ever use something more advanced than italic for the
	 * status, like a smaller font, we need to take that in consideration
	 * here.
	 */
	pango_layout_set_text (layout, status, -1);
	pango_layout_get_extents (layout, NULL, &rect);
	width_status = rect.width / PANGO_SCALE;

	while (len_name >= ELLIPSIS_LIMIT && width_name > width) {
		len_name--;
		ellipsize_string (name, len_name);
		
		pango_layout_set_text (layout, name, -1);
		pango_layout_get_extents (layout, NULL, &rect);
		
		width_name = rect.width / PANGO_SCALE;
	}

	while (len_status >= ELLIPSIS_LIMIT && width_status > width) {
		len_status--;
		ellipsize_string (status, len_status);

		pango_layout_set_text (layout, status, -1);
		pango_layout_get_extents (layout, NULL, &rect);
		
		width_status = rect.width / PANGO_SCALE;
	}
	
	g_object_unref (layout);
}

static void  
roster_view_name_cell_data_func (GtkTreeViewColumn *tree_column,
				 GtkCellRenderer   *cell,
				 GtkTreeModel      *model,
				 GtkTreeIter       *iter,
				 GossipRosterView  *view)
{
	GossipRosterViewPriv *priv;
	gboolean              is_group;
	gpointer              pointer;
	
	priv = view->priv;

	gtk_tree_model_get (model, iter, 
			    COL_IS_GROUP, &is_group, 
			    COL_POINTER, &pointer,
			    -1);

	if (is_group) {
		GossipRosterGroup *group = (GossipRosterGroup *) pointer;
		
		g_object_set (cell,
			      "weight", PANGO_WEIGHT_BOLD,
			      "markup", gossip_roster_group_get_name (group),
			      NULL);
	} else {
		GossipRosterItem *item = (GossipRosterItem *) pointer;
		const gchar      *tmp;
		gchar            *status, *name;
		gchar            *escaped_status, *escaped_name;
		gchar            *markup;
		
		tmp = gossip_roster_item_get_status (item);

		if (!tmp || strcmp (tmp, "") == 0) {
			GossipShow show;

			if (!gossip_roster_item_is_offline (item)) {
				show = gossip_roster_item_get_show (item);
				status = g_strdup (gossip_utils_get_default_status (show));
			} else {
				status = g_strdup (_("Offline"));
			}
		} else {
			status = g_strdup (tmp);
		}
			
		name = g_strdup (gossip_roster_item_get_name (item));

		g_strdelimit (name, "\n\r\t", ' ');
		g_strdelimit (status, "\n\r\t", ' ');
		
		/* FIXME: Figure out how to calculate the offset instead of
		 * hardcoding it here (icon width + padding + indentation).
		 */
		roster_view_ellipsize_item_strings (view, name, status,
						    GTK_WIDGET (view)->allocation.width -
						    (16 + 2*2 + 35));

		escaped_name = g_markup_escape_text (name, -1);
		escaped_status = g_markup_escape_text (status, -1);
		
		markup = g_strdup_printf ("%s\n<span foreground=\"gray\" "
					  "style=\"italic\">%s</span>",
					  escaped_name, escaped_status);

		g_object_set (cell,
			      "weight", PANGO_WEIGHT_NORMAL,
			      "markup", markup,
			      NULL);

		g_free (name);
		g_free (status);
		g_free (escaped_name);
		g_free (escaped_status);
		g_free (markup);
	}
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
	gboolean           is_group;
	gpointer           pointer;
	GossipRosterGroup *group;
	
	gtk_tree_model_get (model, iter, 
			    COL_IS_GROUP, &is_group,
			    COL_POINTER, &pointer,
			    -1);
	
	if (!is_group) {
		return FALSE;
	}

	group = (GossipRosterGroup *) pointer;

	if (strcmp (gossip_roster_group_get_name (group), data->name) == 0) {
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
	gboolean          is_group;
	GossipRosterItem *iter_item;
	GossipJID        *jid_a, *jid_b;
	
	gtk_tree_model_get (model, iter, 
			    COL_IS_GROUP, &is_group,
			    COL_POINTER, &iter_item,
			    -1);
	
	if (is_group) {
		return FALSE;
	}

	jid_a = gossip_roster_item_get_jid (iter_item);
	jid_b = gossip_roster_item_get_jid (item);

	if (gossip_jid_equals_without_resource (jid_a, jid_b)) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
roster_view_find_item (GossipRosterView  *view,
		       GtkTreeIter       *iter,
		       GossipRosterItem  *item, 
		       GossipRosterGroup *group)
{
	GossipRosterViewPriv *priv;
	GtkTreeIter           i, parent;
	const gchar          *group_name;

	priv = view->priv;

	group_name = gossip_roster_group_get_name (group);
		
	if (!roster_view_find_group (view, &parent, group_name)) {
		return FALSE;
	}

	if (!gtk_tree_model_iter_children (priv->model, &i, &parent)) {
		return FALSE;
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
			gtk_tree_selection_unselect_all (selection);
			gtk_tree_selection_select_path (selection, path);

			gtk_tree_model_get_iter (model, &iter, path);
			gtk_tree_path_free (path);

			if (gtk_tree_model_iter_has_child (model, &iter)) {
				d(g_print ("This is a group!\n"));
				factory = priv->group_popup_factory;
			} else {
				factory = priv->item_popup_factory;
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
	GossipRosterItem     *item;
	GtkTreeModel         *model;

	priv = view->priv;
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
	
	gtk_tree_model_get_iter (model, &iter, path);
	
	gtk_tree_model_get (model, &iter, 
			    COL_IS_GROUP, &is_group,
			    COL_POINTER, &item,
			    -1);

	d(g_print ("Row activated!\n"));
	
	if (!is_group) {
		g_signal_emit (view, signals[ITEM_ACTIVATED], 0, item);
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
roster_view_group_menu_rename_cb (gpointer   data,
				  guint      action,
				  GtkWidget *widget)
{
	GossipRosterView     *view;
	GossipRosterViewPriv *priv;
	GossipRosterGroup    *group;
	gboolean              is_group;
	gchar                *str;
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
			    COL_POINTER, &group,
			    -1);

	if (!is_group) {
		return;
	}

	str = g_strdup_printf ("<b>%s</b>", 
			       gossip_roster_group_get_name (group));

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
			    gossip_roster_group_get_name (group));
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
	
	gossip_roster_rename_group (priv->roster, group, str);
}

static gchar *
roster_view_item_factory_translate_func (const gchar *path, gpointer data)
{
	return _((gchar *) path);
}

static gboolean
roster_view_filter_visible_func (GtkTreeModel     *model,
				 GtkTreeIter      *iter,
				 GossipRosterView *view)
{
	GossipRosterViewPriv *priv;
	GossipRosterItem     *item;
	gboolean              is_group;

	priv = view->priv;

	gtk_tree_model_get (model, iter,
			    COL_IS_GROUP, &is_group,
			    COL_POINTER, &item,
			    -1);

	if (is_group) {
		d(g_print ("is_group=t\n"));
		return TRUE;
	}
	
	if (!item) {
		return FALSE;
	}

	d(g_print("is_group=f\n"));

	if (!priv->show_offline && gossip_roster_item_is_offline (item)) {
		d(g_print ("returning false\n"));
		return FALSE;
	}

	d(g_print ("returning true\n"));

	return TRUE;
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
			  "item_updated",
			  G_CALLBACK (roster_view_item_updated),
			  view);
	
	g_signal_connect (priv->roster,
			  "group_added",
			  G_CALLBACK (roster_view_group_added),
			  view);

	g_signal_connect (priv->roster,
			  "group_removed",
			  G_CALLBACK (roster_view_group_removed),
			  view);

	g_signal_connect (priv->roster,
			  "group_item_added",
			  G_CALLBACK (roster_view_group_item_added),
			  view);

	g_signal_connect (priv->roster,
			  "group_item_removed",
			  G_CALLBACK (roster_view_group_item_removed),
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
	gpointer              item;
	
	g_return_val_if_fail (GOSSIP_IS_ROSTER_VIEW (view), NULL);
	
	priv = view->priv;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter,
			    COL_IS_GROUP, &is_group,
			    COL_POINTER, &item,
			    -1);

	if (!is_group) {
		return (GossipRosterItem *) item;
	}

	return NULL;
}

typedef struct {
	GossipRosterView  *view;
	GossipRosterItem  *item;
	GossipRosterGroup *group;
} FlashTimeoutData;

static gboolean
roster_view_flash_timeout_func (gpointer data)
{
	GossipRosterView     *view;
	GossipRosterViewPriv *priv;
	FlashTimeoutData     *fdata = (FlashTimeoutData *) data;
	GtkTreeIter           iter;
	FlashData            *flash;
	GtkTreePath          *path;
	
	view = fdata->view;
	priv = view->priv;

	if (!roster_view_find_item (view, &iter, fdata->item, fdata->group)) {
		d(g_assert_not_reached());
		return FALSE;
	}

	gtk_tree_model_get (priv->model, &iter,
			    COL_FLASH, &flash,
			    -1);
	
	flash->flash_on = !flash->flash_on;

	path = gtk_tree_model_get_path (priv->model, &iter);
	gtk_tree_model_row_changed (priv->model, path, &iter);
	gtk_tree_path_free (path);
	
	return TRUE;
}

void
gossip_roster_view_flash_item (GossipRosterView *view,
			       GossipRosterItem *item,
			       gboolean          flash)
{
	GossipRosterViewPriv *priv;
	GList                *l;
	
	g_return_if_fail (GOSSIP_IS_ROSTER_VIEW (view));
	g_return_if_fail (item != NULL);
	
	priv = view->priv;
	
	for (l = gossip_roster_item_get_groups (item); l; l = l->next) {
		GossipRosterGroup *group = (GossipRosterGroup *) l->data;
		GtkTreeIter        iter;
		FlashData         *flash_data;
		
		if (!roster_view_find_item (view, &iter, item, group)) {
			d(g_assert_not_reached());
			return;
		}

		gtk_tree_model_get (priv->model, &iter,
				    COL_FLASH, &flash_data,
				    -1);

		if (flash && !flash_data) {
			FlashTimeoutData  *data;
			
			data = g_new0 (FlashTimeoutData, 1);
			data->view  = view;
			data->item  = item;
			data->group = group;
			
			flash_data = g_new0 (FlashData, 1);
			flash_data->flash_on = TRUE;
			flash_data->flash_timeout_id =
				g_timeout_add_full (G_PRIORITY_DEFAULT, 350,
						    roster_view_flash_timeout_func,
						    data,
						    g_free);
			gtk_tree_store_set (GTK_TREE_STORE (priv->model), &iter,
					    COL_FLASH, flash_data,
					    -1);
		}
		else if (!flash && flash_data) {
			GtkTreePath *path;
			g_source_remove (flash_data->flash_timeout_id);
			gtk_tree_store_set (GTK_TREE_STORE (priv->model), &iter,
					    COL_FLASH, NULL,
					    -1);
			g_free (flash_data);

			path = gtk_tree_model_get_path (priv->model, &iter);
			gtk_tree_model_row_changed (priv->model, path, &iter);
			gtk_tree_path_free (path);
		}
	}
}

void
gossip_roster_view_set_show_offline (GossipRosterView *view,
				     gboolean show_offline)
{
	GossipRosterViewPriv *priv;

	g_return_if_fail (GOSSIP_IS_ROSTER_VIEW (view));

	priv = view->priv;
	
	priv->show_offline = show_offline;
	egg_tree_model_filter_refilter (EGG_TREE_MODEL_FILTER (priv->filtered_model));
}

gboolean
gossip_roster_view_get_show_offline (GossipRosterView *view)
{
	GossipRosterViewPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_ROSTER_VIEW (view), FALSE); /* TODO: or true? */

	priv = view->priv;
	
	return priv->show_offline;
}
