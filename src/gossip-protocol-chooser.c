/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Xavier Claessens <xclaesse@gmail.com>
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <protocols/telepathy/gossip-telepathy-cmgr.h>

#include "gossip-protocol-chooser.h"

enum {
	COL_PROTOCOL_LABEL,
	COL_PROTOCOL_PROTOCOL,
	COL_PROTOCOL_CMGR,
	COL_PROTOCOL_COUNT
};

void
gossip_protocol_chooser_get_protocol (GtkWidget  *widget,
				      gchar     **cmgr,
				      gchar     **protocol)
{
	GtkTreeModel *model;
	GtkTreeIter   iter;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter)) {
		gtk_tree_model_get (model, &iter,
				    COL_PROTOCOL_CMGR, cmgr,
				    COL_PROTOCOL_PROTOCOL, protocol,
				    -1);
	}
}

GtkWidget *
gossip_protocol_chooser_new (void)
{
	GSList          *cmgr_list, *l;
	GtkListStore    *store;
	GtkCellRenderer *renderer;
	GtkWidget       *combo_box;
	GtkTreeIter      iter;

	/* set up combo box with new store */
	store = gtk_list_store_new (COL_PROTOCOL_COUNT,
				    G_TYPE_STRING,   /* Label */
				    G_TYPE_STRING,   /* Protocol */
				    G_TYPE_STRING);  /* cmgr */
	combo_box = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"text", COL_PROTOCOL_LABEL,
					NULL);

	cmgr_list = gossip_telepathy_cmgr_list ();
	for (l = cmgr_list; l; l = l->next) {
		GSList *protocols, *ll;

		protocols = gossip_telepathy_cmgr_list_protocols (l->data);
		for (ll = protocols; ll; ll = ll->next) {
			gchar *protocol_label;

			protocol_label = g_strdup_printf ("%s (%s)",
							  (gchar *)ll->data,
							  (gchar *)l->data);

			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter, 
					    COL_PROTOCOL_LABEL, protocol_label, 
					    COL_PROTOCOL_PROTOCOL, ll->data,
					    COL_PROTOCOL_CMGR, l->data,
					    -1);
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo_box), &iter);

			g_free (protocol_label);
			g_free (ll->data);
		}

		g_slist_free (protocols);
		g_free (l->data);
	}

	g_slist_free (cmgr_list);
	g_object_unref (store);

	return combo_box;
}
