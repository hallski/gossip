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

#include <libgnome/gnome-i18n.h>

#include "gossip-cell-renderer-text.h"

struct _GossipCellRendererTextPriv {
	gchar    *name;
	gchar    *status;

	gint      width;

	gboolean  dirty;

	gboolean  is_group;
};

static void gossip_cell_renderer_text_class_init (GossipCellRendererTextClass *klass);
static void gossip_cell_renderer_text_init  (GossipCellRendererText *cell);
static void cell_renderer_text_finalize     (GObject                *object);
static void cell_renderer_text_get_property (GObject                *object,
					     guint                   param_id,
					     GValue                 *value,
					     GParamSpec             *pspec);
static void cell_renderer_text_set_property (GObject                *object,
					     guint                   param_id,
					     const GValue           *value,
					     GParamSpec             *pspec);
static void cell_renderer_text_get_size     (GtkCellRenderer        *cell,
					     GtkWidget              *widget,
					     GdkRectangle           *cell_area,
					     gint                   *x_offset,
					     gint                   *y_offset,
					     gint                   *width,
					     gint                   *height);
static void cell_renderer_text_render       (GtkCellRenderer      *cell,
					     GdkDrawable          *window,
					     GtkWidget            *widget,
					     GdkRectangle         *background_area,
					     GdkRectangle         *cell_area,
					     GdkRectangle         *expose_area,
					     GtkCellRendererState  flags);
static void cell_renderer_text_update_text  (GossipCellRendererText *cell,
					     GtkWidget              *widget,
					     gint                    new_width,
					     gboolean                selected);
static void
cell_renderer_text_ellipsize_string         (GtkWidget              *widget,
					     gchar                  *str,
					     gint                    width,
					     gboolean                smaller);
/* -- Properties -- */
enum {
	PROP_0,
	PROP_NAME,
	PROP_STATUS,
	PROP_IS_GROUP
};

G_DEFINE_TYPE (GossipCellRendererText, gossip_cell_renderer_text, GTK_TYPE_CELL_RENDERER_TEXT);

static gpointer parent_class;
static void 
gossip_cell_renderer_text_class_init (GossipCellRendererTextClass *klass)
{
	GObjectClass         *object_class;
	GtkCellRendererClass *cell_class;
	
	object_class = G_OBJECT_CLASS (klass);
	cell_class   = GTK_CELL_RENDERER_CLASS (klass);
	
	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = cell_renderer_text_finalize;

	object_class->get_property = cell_renderer_text_get_property;
	object_class->set_property = cell_renderer_text_set_property;

	cell_class->get_size = cell_renderer_text_get_size;
	cell_class->render = cell_renderer_text_render;
	
	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      _("Name"),
							      _("Contact name"),
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_STATUS,
					 g_param_spec_string ("status",
							      _("Status"),
							      _("Contact status string"),
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_IS_GROUP,
					 g_param_spec_boolean ("is_group",
							       _("Is group"),
							       _("Whether this cell is a group"),
							       FALSE,
							       G_PARAM_READWRITE));
}

static void
gossip_cell_renderer_text_init (GossipCellRendererText *cell)
{
	GossipCellRendererTextPriv *priv;

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

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
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
	
	(GTK_CELL_RENDERER_CLASS (parent_class)->get_size) (cell, widget,
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
//	PangoLayout                *layout;

	celltext = GOSSIP_CELL_RENDERER_TEXT (cell);
	priv     = celltext->priv;

	cell_renderer_text_update_text (celltext, widget, 
					cell_area->width,
					(flags & GTK_CELL_RENDERER_SELECTED));

//	layout = cell_renderer_text_get_layout (celltext, widget, TRUE, flags);

	(GTK_CELL_RENDERER_CLASS (parent_class)->render) (cell, window, 
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
	gchar                      *name;
	gchar                      *status;
	gchar                      *str;

	priv = cell->priv;

	priv->width = new_width;

	attr_color = NULL;

	name = g_strdup (priv->name);
	cell_renderer_text_ellipsize_string (widget, name, new_width, FALSE);

	if (priv->is_group) {
		g_object_set (cell, 
			      "visible", TRUE,
			      "weight", PANGO_WEIGHT_BOLD, 
			      "text", name,
			      "attributes", NULL,
			      NULL);
		return;
	}

	status = g_strdup (priv->status);
	cell_renderer_text_ellipsize_string (widget, status, new_width, TRUE);
	
	str = g_strdup_printf ("%s%s%s", 
			       name, 
			       priv->is_group ? "" : "\n",
			       priv->is_group ? "" : status);

 	style = gtk_widget_get_style (widget);
	color = style->text_aa[GTK_STATE_NORMAL];

	attr_list = pango_attr_list_new ();

	attr_style = pango_attr_style_new (PANGO_STYLE_ITALIC);
	attr_style->start_index = strlen (name) + 1;
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

	g_free (name);
	g_free (status);
	g_free (str);

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
cell_renderer_text_ellipsize_string (GtkWidget *widget,
				     gchar     *str,
				     gint       width,
				     gboolean   smaller) 
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

	layout = gtk_widget_create_pango_layout (widget, NULL);
	
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
			pango_font_description_get_size (widget->style->font_desc) / 1.2);
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
#if 0
static PangoLayout*
cell_renderer_text_get_layout (GossipCellRendererText *cell,
			       GtkWidget              *widget,
			       gboolean                will_render,
			       GtkCellRendererState    flags)
{
	/*
	PangoAttrList              *attr_list;
	*/
	PangoLayout                *layout;
	/*
	 PangoUnderline              uline;
	 */
	GossipCellRendererTextPriv *priv;

	priv = cell->priv;
  
	layout = gtk_widget_create_pango_layout (widget, priv->name);

	//add_attr (attr_list, pango_attr_font_desc_new (celltext->font));

/*	if (celltext->scale_set && celltext->font_scale != 1.0) {
		add_attr (attr_list, 
			  pango_attr_scale_new (celltext->font_scale));
	}
  */
	/*
	if (celltext->underline_set) {
		uline = celltext->underline_style;
	} else {
		uline = PANGO_UNDERLINE_NONE;
	}
*/
	/*
	if (priv->language_set) {
		add_attr (attr_list, pango_attr_language_new (priv->language));
	}
*/
	/*
	if ((flags & GTK_CELL_RENDERER_PRELIT) == GTK_CELL_RENDERER_PRELIT) {
		switch (uline) {
		case PANGO_UNDERLINE_NONE:
			uline = PANGO_UNDERLINE_SINGLE;
			break;
		case PANGO_UNDERLINE_SINGLE:
			uline = PANGO_UNDERLINE_DOUBLE;
			break;
		default:
			break;
		}
	}
*/
	/*
	if (uline != PANGO_UNDERLINE_NONE) {
		add_attr (attr_list, pango_attr_underline_new (celltext->underline_style));
	}

	if (celltext->rise_set) {
		add_attr (attr_list, pango_attr_rise_new (celltext->rise));
	}
	pango_layout_set_attributes (layout, attr_list);
*/
	pango_layout_set_width (layout, -1);
	/*
	pango_attr_list_unref (attr_list);
	*/
	return layout;
}
#endif 

GtkCellRenderer * 
gossip_cell_renderer_text_new (void)
{
	return g_object_new (GOSSIP_TYPE_CELL_RENDERER_TEXT, NULL);
}
