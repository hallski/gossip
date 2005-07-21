/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio AB
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
#include "gossip-stock.h"

static GtkStockItem stock_items[] = {
	{ GOSSIP_STOCK_OFFLINE,                 NULL },     
	{ GOSSIP_STOCK_AVAILABLE,               NULL },
	{ GOSSIP_STOCK_BUSY,                    NULL },
	{ GOSSIP_STOCK_AWAY,                    NULL },
	{ GOSSIP_STOCK_EXT_AWAY,                NULL },    
	{ GOSSIP_STOCK_MESSAGE,                 NULL },
	{ GOSSIP_STOCK_TYPING,                  NULL },
	{ GOSSIP_STOCK_CONTACT_INFORMATION,     NULL },
};

void
gossip_stock_init (void)
{
       GtkIconFactory *icon_factory;
       GtkIconSet     *icon_set;
       GdkPixbuf      *pixbuf;
       gint            i;
       gchar          *filename;
       
       gtk_stock_add (stock_items, G_N_ELEMENTS (stock_items));
       
       icon_factory = gtk_icon_factory_new ();
       gtk_icon_factory_add_default (icon_factory);
       
       for (i = 0; i < G_N_ELEMENTS (stock_items); i++) {
	       filename = g_strdup_printf (IMAGEDIR "/%s.png", stock_items[i].stock_id);
	       pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
	       g_free (filename);

	       icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);
	       
	       gtk_icon_factory_add (icon_factory,
				     stock_items[i].stock_id,
				     icon_set);

	       g_object_unref (pixbuf);
       }
}

