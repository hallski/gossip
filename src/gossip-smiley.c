/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Imendio AB
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

#include "gossip-smiley.h"

GdkPixbuf *
gossip_chat_view_get_smiley_image (GossipSmiley smiley)
{
	static GdkPixbuf *pixbufs[GOSSIP_SMILEY_COUNT];
	static gboolean   inited = FALSE;

	if (!inited) {
		gint i;

		for (i = 0; i < GOSSIP_SMILEY_COUNT; i++) {
			pixbufs[i] = gossip_pixbuf_from_smiley (i, GTK_ICON_SIZE_MENU);
		}

		inited = TRUE;
	}

	return pixbufs[smiley];
}

const gchar *
gossip_chat_view_get_smiley_text (GossipSmiley smiley)
{
	gint i;

	for (i = 0; i < G_N_ELEMENTS (smileys); i++) {
		if (smileys[i].smiley != smiley) {
			continue;
		}

		return smileys[i].pattern;
	}

	return NULL;
}

GtkWidget *
gossip_chat_view_get_smiley_menu (GCallback    callback,
				  gpointer     user_data,
				  GtkTooltips *tooltips)
{
	GtkWidget *menu;
	gint       x;
	gint       y;
	gint       i;

	g_return_val_if_fail (callback != NULL, NULL);

	menu = gtk_menu_new ();

	for (i = 0, x = 0, y = 0; i < GOSSIP_SMILEY_COUNT; i++) {
		GtkWidget   *item;
		GtkWidget   *image;
		GdkPixbuf   *pixbuf;
		const gchar *smiley_text;

		pixbuf = gossip_chat_view_get_smiley_image (i);
		if (!pixbuf) {
			continue;
		}

		image = gtk_image_new_from_pixbuf (pixbuf);

		item = gtk_image_menu_item_new_with_label ("");
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

		gtk_menu_attach (GTK_MENU (menu), item,
				 x, x + 1, y, y + 1);

		smiley_text = gossip_chat_view_get_smiley_text (i);

		gtk_tooltips_set_tip (tooltips,
				      item,
				      smiley_text,
				      NULL);

		g_object_set_data  (G_OBJECT (item), "smiley_text", (gpointer) smiley_text);
		g_signal_connect (item, "activate", callback, user_data);

		if (x > 3) {
			y++;
			x = 0;
		} else {
			x++;
		}
	}

	gtk_widget_show_all (menu);

	return menu;
}

GdkPixbuf *
gossip_pixbuf_from_smiley (GossipSmiley type,
			   GtkIconSize  icon_size)
{
	GtkIconTheme  *theme;
	GdkPixbuf     *pixbuf = NULL;
	GError        *error = NULL;
	gint           w, h;
	gint           size;
	const gchar   *icon_id;

	theme = gtk_icon_theme_get_default ();

	if (!gtk_icon_size_lookup (icon_size, &w, &h)) {
		size = 16;
	} else {
		size = (w + h) / 2;
	}

	switch (type) {
	case GOSSIP_SMILEY_NORMAL:       /*  :)   */
		icon_id = "stock_smiley-1";
		break;
	case GOSSIP_SMILEY_WINK:         /*  ;)   */
		icon_id = "stock_smiley-3";
		break;
	case GOSSIP_SMILEY_BIGEYE:       /*  =)   */
		icon_id = "stock_smiley-2";
		break;
	case GOSSIP_SMILEY_NOSE:         /*  :-)  */
		icon_id = "stock_smiley-7";
		break;
	case GOSSIP_SMILEY_CRY:          /*  :'(  */
		icon_id = "stock_smiley-11";
		break;
	case GOSSIP_SMILEY_SAD:          /*  :(   */
		icon_id = "stock_smiley-4";
		break;
	case GOSSIP_SMILEY_SCEPTICAL:    /*  :/   */
		icon_id = "stock_smiley-9";
		break;
	case GOSSIP_SMILEY_BIGSMILE:     /*  :D   */
		icon_id = "stock_smiley-6";
		break;
	case GOSSIP_SMILEY_INDIFFERENT:  /*  :|   */
		icon_id = "stock_smiley-8";
		break;
	case GOSSIP_SMILEY_TOUNGE:       /*  :p   */
		icon_id = "stock_smiley-10";
		break;
	case GOSSIP_SMILEY_SHOCKED:      /*  :o   */
		icon_id = "stock_smiley-5";
		break;
	case GOSSIP_SMILEY_COOL:         /*  8)   */
		icon_id = "stock_smiley-15";
		break;
	case GOSSIP_SMILEY_SORRY:        /*  *|   */
		icon_id = "stock_smiley-12";
		break;
	case GOSSIP_SMILEY_KISS:         /*  :*   */
		icon_id = "stock_smiley-13";
		break;
	case GOSSIP_SMILEY_SHUTUP:       /*  :#   */
		icon_id = "stock_smiley-14";
		break;
	case GOSSIP_SMILEY_YAWN:         /*  |O   */
		icon_id = "";
		break;
	case GOSSIP_SMILEY_CONFUSED:     /*  :$   */
		icon_id = "stock_smiley-17";
		break;
	case GOSSIP_SMILEY_ANGEL:        /*  O)   */
		icon_id = "stock_smiley-18";
		break;
	case GOSSIP_SMILEY_OOOH:         /*  :x   */
		icon_id = "stock_smiley-19";
		break;
	case GOSSIP_SMILEY_LOOKAWAY:     /*  *)   */
		icon_id = "stock_smiley-20";
		break;
	case GOSSIP_SMILEY_BLUSH:        /*  *S   */
		icon_id = "stock_smiley-23";
		break;
	case GOSSIP_SMILEY_COOLBIGSMILE: /*  8D   */
		icon_id = "stock_smiley-25";
		break;
	case GOSSIP_SMILEY_ANGRY:        /*  :@   */
		icon_id = "stock_smiley-16";
		break;
	case GOSSIP_SMILEY_BOSS:         /*  @)   */
		icon_id = "stock_smiley-21";
		break;
	case GOSSIP_SMILEY_MONKEY:       /*  #)   */
		icon_id = "stock_smiley-22";
		break;
	case GOSSIP_SMILEY_SILLY:        /*  O)   */
		icon_id = "stock_smiley-24";
		break;
	case GOSSIP_SMILEY_SICK:         /*  +o(  */
		icon_id = "stock_smiley-26";
		break;

	default:
		g_assert_not_reached ();
		icon_id = NULL;
	}

	pixbuf = gtk_icon_theme_load_icon (theme,
					   icon_id,     /* icon name */
					   size,        /* size */
					   0,           /* flags */
					   &error);

	return pixbuf;
}


