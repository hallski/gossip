/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio AB
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

#include "gossip-cell-renderer-text.h"

struct _GossipCellRendererTextPriv {
	gchar    *name;
	gchar    *status;

	gint      width;

	gboolean  dirty;

	gboolean  is_group;
};

static void gossip_cell_renderer_text_class_init (GossipCellRendererTextClass *klass);
static void gossip_cell_renderer_text_init       (GossipCellRendererText      *cell);
static void cell_renderer_text_finalize          (GObject                     *object);
static void cell_renderer_text_get_property      (GObject                     *object,
						  guint                        param_id,
						  GValue                      *value,
						  GParamSpec                  *pspec);
static void cell_renderer_text_set_property      (GObject                     *object,
						  guint                        param_id,
						  const GValue                *value,
						  GParamSpec                  *pspec);
static void cell_renderer_text_get_size          (GtkCellRenderer             *cell,
						  GtkWidget                   *widget,
						  GdkRectangle                *cell_area,
						  gint                        *x_offset,
						  gint                        *y_offset,
						  gint                        *width,
						  gint                        *height);
static void cell_renderer_text_render            (GtkCellRenderer             *cell,
						  GdkDrawable                 *window,
						  GtkWidget                   *widget,
						  GdkRectangle                *background_area,
						  GdkRectangle                *cell_area,
						  GdkRectangle                *expose_area,
						  GtkCellRendererState         flags);
static void cell_renderer_text_update_text       (GossipCellRendererText      *cell,
						  GtkWidget                   *widget,
						  gint                         new_width,
						  gboolean                     selected);


/* Properties */
enum {
	PROP_0,
	PROP_NAME,
	PROP_STATUS,
	PROP_IS_GROUP,
};


G_DEFINE_TYPE (GossipCellRendererText, gossip_cell_renderer_text, GTK_TYPE_CELL_RENDERER_TEXT);


static void 
gossip_cell_renderer_text_class_init (GossipCellRendererTextClass *klass)
{
	GObjectClass         *object_class;
	GtkCellRendererClass *cell_class;
	
	object_class = G_OBJECT_CLASS (klass);
	cell_class   = GTK_CELL_RENDERER_CLASS (klass);
	
	object_class->finalize = cell_renderer_text_finalize;

	object_class->get_property = cell_renderer_text_get_property;
	object_class->set_property = cell_renderer_text_set_property;

	cell_class->get_size = cell_renderer_text_get_size;
	cell_class->render = cell_renderer_text_render;
	
	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Name",
							      "Contact name",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_STATUS,
					 g_param_spec_string ("status",
							      "Status",
							      "Contact status string",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_IS_GROUP,
					 g_param_spec_boolean ("is_group",
							       "Is group",
							       "Whether this cell is a group",
							       FALSE,
							       G_PARAM_READWRITE));
}

static void
gossip_cell_renderer_text_init (GossipCellRendererText *cell)
{
	GossipCellRendererTextPriv *priv;

	g_object_set (cell,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);

	priv = g_new0 (GossipCellRendererTextPriv, 1);

	priv->name = g_strdup ("");
	priv->status = g_strdup ("");

	cell->priv = priv;
}

static void
cell_renderer_text_finalize (GObject *object)
{
	GossipCellRendererText     *cell;
	GossipCellRendererTextPriv *priv;

	cell = GOSSIP_CELL_RENDERER_TEXT (object);
	priv = cell->priv;

	g_free (priv->name);
	g_free (priv->status);

	g_free (priv);

	(G_OBJECT_CLASS (gossip_cell_renderer_text_parent_class)->finalize) (object);
}

static void
cell_renderer_text_get_property (GObject    *object,
				 guint       param_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
	GossipCellRendererText     *cell;
	GossipCellRendererTextPriv *priv;

	cell = GOSSIP_CELL_RENDERER_TEXT (object);
	priv = cell->priv;

	switch (param_id) {
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_STATUS:
		g_value_set_string (value, priv->status);
		break;
	case PROP_IS_GROUP:
		g_value_set_boolean (value, priv->is_group);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
cell_renderer_text_set_property (GObject      *object,
				 guint         param_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	GossipCellRendererText     *cell;
	GossipCellRendererTextPriv *priv;
	const gchar                *str;

	cell = GOSSIP_CELL_RENDERER_TEXT (object);
	priv = cell->priv;

	switch (param_id) {
	case PROP_NAME:
		g_free (priv->name);
		str = g_value_get_string (value);
		priv->name = g_strdup (str ? str : "");
		g_strdelimit (priv->name, "\n\r\t", ' ');
		break;
	case PROP_STATUS:
		g_free (priv->status);
		str = g_value_get_string (value);
		priv->status = g_strdup (str ? str : "");
		g_strdelimit (priv->status, "\n\r\t", ' ');
		break;
	case PROP_IS_GROUP:
		priv->is_group = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
cell_renderer_text_get_size (GtkCellRenderer *cell,
			     GtkWidget       *widget,
			     GdkRectangle    *cell_area,
			     gint            *x_offset,
			     gint            *y_offset,
			     gint            *width,
			     gint            *height)
{
	GossipCellRendererText     *celltext;
	GossipCellRendererTextPriv *priv;

	celltext = GOSSIP_CELL_RENDERER_TEXT (cell);
	priv     = celltext->priv;
	
	cell_renderer_text_update_text (celltext, widget, 0, 0);

	(GTK_CELL_RENDERER_CLASS (gossip_cell_renderer_text_parent_class)->get_size) (
		cell, widget,
		cell_area, 
		x_offset, y_offset,
		width, height);
}

static void
cell_renderer_text_render (GtkCellRenderer      *cell,
			   GdkWindow            *window,
			   GtkWidget            *widget,
			   GdkRectangle         *background_area,
			   GdkRectangle         *cell_area,
			   GdkRectangle         *expose_area,
			   GtkCellRendererState  flags)
{
	GossipCellRendererText     *celltext;
	GossipCellRendererTextPriv *priv;

	celltext = GOSSIP_CELL_RENDERER_TEXT (cell);
	priv     = celltext->priv;

	cell_renderer_text_update_text (celltext, widget, 
					cell_area->width,
					(flags & GTK_CELL_RENDERER_SELECTED));

	(GTK_CELL_RENDERER_CLASS (gossip_cell_renderer_text_parent_class)->render) (
		cell, window, 
		widget, 
		background_area,
		cell_area,
		expose_area, flags);
}

static void
cell_renderer_text_update_text (GossipCellRendererText *cell, 
				GtkWidget              *widget,
				gint                    new_width,
				gboolean                selected)
{
	GossipCellRendererTextPriv *priv;
	PangoAttrList              *attr_list;
	PangoAttribute             *attr_color, *attr_style, *attr_size;
	GtkStyle                   *style;
	GdkColor                    color;
	gchar                      *str;
	gboolean                    show_status = FALSE;

	priv = cell->priv;

	priv->width = new_width;

	attr_color = NULL;

	if (priv->is_group) {
		g_object_set (cell, 
			      "visible", TRUE,
			      "weight", PANGO_WEIGHT_BOLD, 
			      "text", priv->name,
			      "attributes", NULL,
			      NULL);
		return;
	}

	if (!priv->is_group && (priv->status && strlen (priv->status) > 0)) {
		show_status = TRUE;
	} 

	str = g_strdup_printf ("%s%s%s", 
			       priv->name, 
			       !show_status ? "" : "\n",
			       !show_status ? "" : priv->status);

 	style = gtk_widget_get_style (widget);
	color = style->text_aa[GTK_STATE_NORMAL];

	attr_list = pango_attr_list_new ();

	attr_style = pango_attr_style_new (PANGO_STYLE_ITALIC);
	attr_style->start_index = strlen (priv->name) + 1;
	attr_style->end_index = -1;
	pango_attr_list_insert (attr_list, attr_style);

  	if (!selected) {  
   		attr_color = pango_attr_foreground_new (color.red, color.green, color.blue);   
   		attr_color->start_index = attr_style->start_index;   
   		attr_color->end_index = -1;   
   		pango_attr_list_insert (attr_list, attr_color);   
   	}   

	attr_size = pango_attr_size_new (pango_font_description_get_size (style->font_desc) / 1.2);
	attr_size->start_index = attr_style->start_index;
	attr_size->end_index = -1;
	pango_attr_list_insert (attr_list, attr_size);

	g_object_set (cell,
		      "visible", TRUE,
		      "weight", PANGO_WEIGHT_NORMAL,
		      "text", str,
		      "attributes", attr_list,
		      NULL);
      
	pango_attr_list_unref (attr_list);

	g_free (str);
}

GtkCellRenderer * 
gossip_cell_renderer_text_new (void)
{
	return g_object_new (GOSSIP_TYPE_CELL_RENDERER_TEXT, NULL);
}
