/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Geert-Jan Van den Bogaerde <gvdbogaerde@pandora.be>
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
#include <gdk/gdkkeysyms.h>
#include <glade/glade.h>
#include <libgnome/gnome-i18n.h>
#include <gconf/gconf-client.h>
#include "gossip-app.h"
#include "gossip-contact-info.h"
#include "gossip-preferences.h"
#include "gossip-chat-window.h"
#include "gossip-notebook.h"
#include "gossip-utils.h"
#include "gossip-stock.h"
#include "gossip-sound.h"
#include "gossip-log.h"
#include "eel-ellipsizing-label.h"

static void gossip_chat_window_class_init     (GossipChatWindowClass *klass);
static void gossip_chat_window_init	      (GossipChatWindow      *window);
static void gossip_chat_window_finalize       (GObject		     *object);
static GdkPixbuf * 
chat_window_get_status_pixbuf		      (GossipChatWindow	     *window,
					       GossipChat	     *chat);
static void chat_window_accel_cb              (GtkAccelGroup         *accelgroup,
					       GObject               *object,
					       guint                  key,
					       GdkModifierType        mod,
					       GossipChatWindow      *window);
static gboolean 
chat_window_has_toplevel_focus                (GossipChatWindow	     *window);
static GtkWidget*
chat_window_create_label		      (GossipChatWindow	     *window,
					       GossipChat	     *chat);
static void chat_window_update_status	      (GossipChatWindow	     *window,
					       GossipChat	     *chat);
static void chat_window_update_title	      (GossipChatWindow	     *window);
static void chat_window_update_menu	      (GossipChatWindow	     *window);
static void chat_window_info_activate_cb      (GtkWidget	     *menuitem,
					       GossipChatWindow	     *window);
static void chat_window_log_activate_cb       (GtkWidget             *menuitem,
					       GossipChatWindow      *window);
static void chat_window_view_activate_cb      (GtkWidget             *menuitem,
					       GossipChatWindow      *window);
static void chat_window_close_activate_cb     (GtkWidget	     *menuitem,
					       GossipChatWindow      *window);
static void chat_window_tab_left_activate_cb  (GtkWidget	     *menuitem,
					       GossipChatWindow      *window);
static void chat_window_tab_right_activate_cb (GtkWidget	     *menuitem,
					       GossipChatWindow	     *window);
static void chat_window_detach_activate_cb    (GtkWidget	     *menuitem,
					       GossipChatWindow      *window);
static gboolean chat_window_delete_event_cb   (GtkWidget	     *dialog,
					       GdkEvent		     *event,
					       GossipChatWindow	     *window);
static void chat_window_presence_changed_cb   (GossipChat	     *chat,
					       GossipChatWindow	     *window);
static void chat_window_composing_cb	      (GossipChat	     *chat,
					       gboolean		      is_composing,
					       GossipChatWindow	     *window);
static void chat_window_new_message_cb	      (GossipChat	     *chat,
					       GossipChatWindow	     *window);
static void chat_window_switch_page_cb	      (GtkNotebook	     *notebook,
					       GtkNotebookPage	     *page,
					       gint		      page_num,
					       GossipChatWindow      *window);
static void chat_window_tab_added_cb	      (GossipNotebook        *notebook,
					       GtkWidget	     *child,
					       GossipChatWindow      *window);
static void chat_window_tab_removed_cb	      (GossipNotebook	     *notebook,
					       GtkWidget	     *child,
					       GossipChatWindow      *window);
static void chat_window_tab_detached_cb       (GossipNotebook        *notebook,
					       GtkWidget	     *widget,
					       GossipChatWindow      *window);
static void chat_window_tabs_reordered_cb     (GossipNotebook	     *notebook,
					       GossipChatWindow      *window);
static void chat_window_close_clicked_cb      (GtkWidget	     *button,
					       GossipChat	     *chat);
static gboolean
chat_window_focus_in_event_cb                 (GtkWidget	     *widget,
					       GdkEvent		     *event,
					       GossipChatWindow      *window); 
static const gchar *
chat_window_get_name                          (GossipChatWindow      *window, 
					       GossipChat            *chat);

/* Called from Glade, so it shouldn't be static */
GtkWidget * chat_window_create_notebook (gpointer data);

struct _GossipChatWindowPriv {
	GList                  *chats;
	GList                  *chats_new_msg;
	GList                  *chats_composing;
	GossipChat             *current_chat;
	gboolean		new_msg;

	GtkWidget              *dialog;
	GtkWidget              *notebook;
	
	/* menu items */
	GtkWidget	       *m_conv_close;
	GtkWidget	       *m_view_log;
	GtkWidget	       *m_view_info;
	GtkWidget	       *m_view_as_windows;
	GtkWidget	       *m_view_as_list;
	GtkWidget	       *m_tabs_next;
	GtkWidget	       *m_tabs_prev;
	GtkWidget	       *m_tabs_left;
	GtkWidget	       *m_tabs_right;
	GtkWidget	       *m_tabs_detach;
};

static gpointer parent_class;
static GList *chat_windows = NULL;

static guint tab_accel_keys[] = {
	GDK_1, GDK_2, GDK_3, GDK_4, GDK_5,
	GDK_6, GDK_7, GDK_8, GDK_9, GDK_0
};


GType
gossip_chat_window_get_type (void)
{
        static GType type_id = 0;

        if (type_id == 0) {
                static const GTypeInfo type_info = {
                        sizeof (GossipChatWindowClass),
                        NULL,
                        NULL,
                        (GClassInitFunc) gossip_chat_window_class_init,
                        NULL,
                        NULL,
                        sizeof (GossipChatWindow),
                        0,
                        (GInstanceInitFunc) gossip_chat_window_init
                };

                type_id = g_type_register_static (G_TYPE_OBJECT,
                                                  "GossipChatWindow",
                                                  &type_info, 0);
        }

        return type_id;
}

static void
gossip_chat_window_class_init (GossipChatWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize	   = gossip_chat_window_finalize;
}

static void
gossip_chat_window_init (GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GladeXML             *glade;
	GtkWidget            *menu_view;
	GtkAccelGroup        *accel_group;
	GClosure             *closure;
	gint                  i;

	priv = g_new0 (GossipChatWindowPriv, 1);
	window->priv = priv;

	glade = gossip_glade_get_file (GLADEDIR "/chat.glade",
				       "chat_window",
				       NULL,
				       "chat_window", &priv->dialog,
				       "chats_notebook", &priv->notebook,
				       "menu_conv_close", &priv->m_conv_close,
				       "menu_view", &menu_view,
				       "menu_view_info", &priv->m_view_info,
				       "menu_view_log", &priv->m_view_log,
				       "menu_tabs_next", &priv->m_tabs_next,
				       "menu_tabs_prev", &priv->m_tabs_prev,
				       "menu_tabs_left", &priv->m_tabs_left,
				       "menu_tabs_right", &priv->m_tabs_right,
				       "menu_tabs_detach", &priv->m_tabs_detach,
				       NULL);

	accel_group = gtk_accel_group_new ();
	gtk_window_add_accel_group (GTK_WINDOW (priv->dialog), accel_group);
	
	for (i = 0; i < G_N_ELEMENTS (tab_accel_keys); i++) {
		closure =  g_cclosure_new (G_CALLBACK (chat_window_accel_cb),
					   window,
					   NULL);
		gtk_accel_group_connect (accel_group,
					 tab_accel_keys[i],
					 GDK_MOD1_MASK,
					 0,
					 closure);
	}
	
	g_signal_connect (menu_view,
			  "activate",
			  G_CALLBACK (chat_window_view_activate_cb),
			  window);
	
	g_signal_connect (priv->m_conv_close,
			  "activate",
			  G_CALLBACK (chat_window_close_activate_cb),
			  window);
	g_signal_connect (priv->m_view_log,
			  "activate",
			  G_CALLBACK (chat_window_log_activate_cb),
			  window);
	g_signal_connect (priv->m_view_info,
			  "activate",
			  G_CALLBACK (chat_window_info_activate_cb),
			  window);
	g_signal_connect_swapped (priv->m_tabs_prev,
			          "activate",
			          G_CALLBACK (gtk_notebook_prev_page),
				  priv->notebook);
	g_signal_connect_swapped (priv->m_tabs_next,
			          "activate",
			          G_CALLBACK (gtk_notebook_next_page),
			          priv->notebook);
	g_signal_connect (priv->m_tabs_left,
			  "activate",
			  G_CALLBACK (chat_window_tab_left_activate_cb),
			  window);
	g_signal_connect (priv->m_tabs_right,
			  "activate",
			  G_CALLBACK (chat_window_tab_right_activate_cb),
			  window);
	g_signal_connect (priv->m_tabs_detach,
			  "activate",
			  G_CALLBACK (chat_window_detach_activate_cb),
			  window);
	g_signal_connect (priv->dialog,
			  "delete_event",
			  G_CALLBACK (chat_window_delete_event_cb),
			  window);
	g_signal_connect_after (priv->notebook,
				"switch_page",
				G_CALLBACK (chat_window_switch_page_cb),
				window);
	g_signal_connect (priv->dialog,
			  "focus_in_event",
			  G_CALLBACK (chat_window_focus_in_event_cb),
			  window);
	g_signal_connect (priv->notebook,
			  "tab_added",
			  G_CALLBACK (chat_window_tab_added_cb),
			  window);
	g_signal_connect (priv->notebook,
			  "tab_removed",
			  G_CALLBACK (chat_window_tab_removed_cb),
			  window);
	g_signal_connect (priv->notebook,
			  "tab_detached",
			  G_CALLBACK (chat_window_tab_detached_cb),
			  window);
	g_signal_connect (priv->notebook,
			  "tabs_reordered",
			  G_CALLBACK (chat_window_tabs_reordered_cb),
			  window);

	chat_windows = g_list_prepend (chat_windows, window);

	priv->chats = NULL;
	priv->chats_new_msg = NULL;
	priv->chats_composing = NULL;
	priv->current_chat = NULL;
}

static void
gossip_chat_window_finalize (GObject *object)
{
	GossipChatWindow *window = GOSSIP_CHAT_WINDOW (object);

	chat_windows = g_list_remove (chat_windows, window);
	gtk_widget_destroy (window->priv->dialog);
	g_free (window->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GdkPixbuf *
chat_window_get_status_pixbuf (GossipChatWindow *window,
			       GossipChat *chat)
{
	GdkPixbuf   *pixbuf;

	if (g_list_find (window->priv->chats_new_msg, chat)) {
		pixbuf = gossip_utils_get_pixbuf_from_stock (GOSSIP_STOCK_MESSAGE);
	}
	else if (g_list_find (window->priv->chats_composing, chat)) {
		pixbuf = gossip_utils_get_pixbuf_from_stock (GOSSIP_STOCK_TYPING);
	}
	else {
		GossipRosterItem *item;

		item = gossip_chat_get_item (chat);
		if (gossip_roster_item_is_offline (item)) {
			pixbuf = gossip_utils_get_pixbuf_offline ();
		} else {
			GossipShow show;

			show  = gossip_roster_item_get_show (item);
			pixbuf = gossip_utils_get_pixbuf_from_show (show);
		}
	}

	return pixbuf;
}

static void
chat_window_accel_cb (GtkAccelGroup    *accelgroup,
		      GObject          *object,
		      guint             key,
		      GdkModifierType   mod,
		      GossipChatWindow *window)
{
	gint i, num = -1;

	for (i = 0; i < G_N_ELEMENTS (tab_accel_keys); i++) {
		if (tab_accel_keys[i] == key) {
			num = i;
			break;
		}
	}

	if (num != -1) {
		gtk_notebook_set_current_page (
			GTK_NOTEBOOK (window->priv->notebook), num);
	}
}

static gboolean
chat_window_has_toplevel_focus (GossipChatWindow *window)
{
	gboolean focus = FALSE;
	
	/* The has-toplevel-focus property is new in GTK 2.2 so if we don't find it, we
	 * pretend that the window doesn't have focus.
	 */
	if (g_object_class_find_property (G_OBJECT_GET_CLASS (window->priv->dialog),
					  "has-toplevel-focus")) {
		g_object_get (window->priv->dialog, "has-toplevel-focus", &focus, NULL);
	}

	return focus;
}

#define TAB_MIN_WIDTH 130

#if 0
static void
tab_label_set_size (GtkWidget *window, GtkWidget *label_hbox)
{
	GtkStyle         *style;
	PangoFontMetrics *metrics;
	PangoContext     *context;
	PangoLanguage    *lang;
	gint              char_width;
	gint              max_width, min_width, width;
	GtkWidget        *label;

	GtkRequisition    requisition;
	
	label = g_object_get_data (G_OBJECT (label_hbox), "label");
	gtk_widget_size_request (label, &requisition);

	width = requisition.width;

	style = gtk_widget_get_style (label);
	context = gtk_widget_get_pango_context (label);
	lang = pango_context_get_language (context);
	metrics = pango_context_get_metrics (context, style->font_desc, lang);
	char_width = pango_font_metrics_get_approximate_char_width (metrics);
	pango_font_metrics_unref (metrics);
	
	max_width = MAX (PANGO_PIXELS (char_width * 20), window->allocation.width / 3);
	min_width = MIN (5, width);
	width = CLAMP (width, min_width, max_width);
	
	gtk_widget_set_size_request (label_hbox, width + 16 + 4 + 16, -1);
}

typedef struct {
	GtkWidget *tab;
	gint width;
} TabWidth;

static gint
width_compare_func (gconstpointer a, gconstpointer b)
{
	TabWidth ta = *(TabWidth*)a;
	TabWidth tb = *(TabWidth*)b;

	if (ta.width > tb.width) {
		return -1;
	}
	else if (ta.width < tb.width) {
		return 1;
	} else {
		return 0;
	}
}

static void
tab_notebook_size_request_cb (GtkNotebook    *notebook,
			      GtkRequisition *requisition,
			      gpointer        userdata)
{
	gint num_pages;
	gint thickness, focus_width;
	gint width;

	num_pages = gtk_notebook_get_n_pages (notebook);

	gtk_widget_style_get (GTK_WIDGET (notebook), "focus-line-width", &focus_width, NULL);
	thickness = GTK_WIDGET (notebook)->style->xthickness;
	
	width = 2 * thickness + num_pages * (TAB_MIN_WIDTH + 2 * thickness + 2 * focus_width);

	requisition->width = width;
}

static void
tab_label_size_request_cb (GtkWidget      *window,
			   GtkRequisition *requisition,
			   GtkNotebook    *notebook)
{
	gint           num_pages, num_overflow_pages, i;
	GtkWidget      *page;
	GtkWidget      *tab;
	GtkRequisition  tab_req;
	gint            total_width = 0;
	gint            notebook_width;
	gint            overflow;

	TabWidth tab_width;
	
	GArray         *array;

	gint thickness, focus_width;

	gtk_widget_style_get (GTK_WIDGET (notebook), "focus-line-width", &focus_width, NULL);
	thickness = GTK_WIDGET (notebook)->style->xthickness;
	
	num_pages = gtk_notebook_get_n_pages (notebook);
	array = g_array_new (FALSE, FALSE, sizeof (TabWidth));

	notebook_width = GTK_WIDGET (notebook)->allocation.width;

	num_overflow_pages = 0;
	
	for (i = 0; i < num_pages; i++) {
		page = gtk_notebook_get_nth_page (notebook, i);
		
		tab = gtk_notebook_get_tab_label (notebook, page);
		
		gtk_widget_set_size_request (tab, -1, -1);
		gtk_widget_size_request (tab, &tab_req);

		if (tab_req.width > TAB_MIN_WIDTH) {
			tab_width.tab = tab;
			tab_width.width = tab_req.width;
			
			g_array_append_val (array, tab_width);
			
			total_width += tab_req.width + (2 * thickness + 2 * focus_width);
			num_overflow_pages++;
		}
	}

	overflow = total_width - notebook_width;

	if (overflow > 0) {
		g_print ("overflow: %d\n", overflow);
	}
		
	g_array_sort (array, width_compare_func);
	
	if (overflow > 0) {
		for (i = 0; i < num_overflow_pages; i++) {
			gint delta;
			
			tab_width = g_array_index (array, TabWidth, i);

			delta = tab_width.width - TAB_MIN_WIDTH;

			overflow -= delta;

			if (overflow < 0) {
				delta += overflow;
			}
			
			gtk_widget_set_size_request (tab_width.tab, tab_width.width - delta, -1);
			
			if (overflow <= 0) {
				break;
			}
		}
	}

	g_array_free (array, TRUE);
}
#endif

static GtkWidget *
chat_window_create_label (GossipChatWindow *window,
			  GossipChat       *chat)
{
	GossipChatWindowPriv *priv;
	GtkWidget            *hbox;
	GtkWidget            *close_button;
	GtkWidget            *close_img;
	GtkWidget            *name_label;
	GtkWidget            *status_img;
	const gchar          *name;
	gint	              w, h;
	
	priv = window->priv;
	
	hbox = gtk_hbox_new (FALSE, 0);

	status_img = gtk_image_new ();
	g_object_set_data (G_OBJECT (chat), "chat-window-status-img", status_img);

	name = chat_window_get_name (window, chat);
	/*name_label = eel_ellipsizing_label_new (name);*/
	name_label = gtk_label_new (name);

	/* Set minimum size. */
	gtk_widget_set_size_request (hbox, TAB_MIN_WIDTH, -1);
	
	gtk_misc_set_padding (GTK_MISC (name_label), 2, 0);
	gtk_misc_set_alignment (GTK_MISC (name_label), 0.0, 0.5);

	g_object_set_data (G_OBJECT (hbox), "label", name_label);
		
/*	g_signal_connect (priv->dialog,
			  "size_request",
			  G_CALLBACK (tab_label_size_request_cb),
			  priv->notebook);

	g_signal_connect (priv->notebook,
			  "size_request",
			  G_CALLBACK (tab_notebook_size_request_cb),
			  NULL);
*/
	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);
	close_button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
	close_img = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
	gtk_widget_set_size_request (close_button, w, h);
	gtk_container_add (GTK_CONTAINER (close_button), close_img);

	gtk_box_pack_start (GTK_BOX (hbox), status_img, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), name_label, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);

	g_signal_connect (close_button,
			  "clicked",
			  G_CALLBACK (chat_window_close_clicked_cb),
			  chat);
	
	gtk_widget_show_all (hbox);

	return hbox;
}

static void
chat_window_update_status (GossipChatWindow *window,
			   GossipChat	    *chat)
{
	GtkImage	    *image;
	GdkPixbuf           *pixbuf;
	
	pixbuf = chat_window_get_status_pixbuf (window, chat);
	image = g_object_get_data (G_OBJECT (chat), "chat-window-status-img");
	gtk_image_set_from_pixbuf (image, pixbuf);
}

static void
chat_window_update_title (GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	gchar                *title; 

	priv = window->priv;

	title = g_strdup_printf (_("%sChat - %s"), 
				 window->priv->new_msg ? "*" : "", 
				 chat_window_get_name (window, 
						       priv->current_chat));

	gtk_window_set_title (GTK_WINDOW (window->priv->dialog), title);
	g_free (title);
}

static void
chat_window_update_menu (GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	gboolean              first_page;
	gboolean              last_page;
	gint                  num_pages;
	gint                  page_num;
	
	priv = window->priv;

	page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));
	num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook));
	first_page = (page_num == 0);
	last_page = (page_num == (num_pages - 1));

	gtk_widget_set_sensitive (priv->m_tabs_next, !last_page);
	gtk_widget_set_sensitive (priv->m_tabs_prev, !first_page);
	gtk_widget_set_sensitive (priv->m_tabs_detach, num_pages > 1);
	gtk_widget_set_sensitive (priv->m_tabs_left, !first_page);
	gtk_widget_set_sensitive (priv->m_tabs_right, !last_page);
}

static void
chat_window_log_activate_cb (GtkWidget        *menuitem,
			     GossipChatWindow *window)
{
	GossipChatWindowPriv *priv = window->priv;
	GossipRosterItem     *item;

	item = gossip_chat_get_item (priv->current_chat);
	
	gossip_log_show (gossip_roster_item_get_jid (item));
}

static void
chat_window_info_activate_cb (GtkWidget        *menuitem,
			      GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipRosterItem     *item;
	
	priv = window->priv;

	item = gossip_chat_get_item (priv->current_chat);

	gossip_contact_info_new (gossip_roster_item_get_jid (item),
				 chat_window_get_name (window, priv->current_chat));
}

static void
chat_window_view_activate_cb (GtkWidget        *menuitem,
			      GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipRosterItem     *item;
	gboolean              log_exists;

	priv = window->priv;
	
	item = gossip_chat_get_item (priv->current_chat);
	log_exists = gossip_log_exists (gossip_roster_item_get_jid (item));
	gtk_widget_set_sensitive (priv->m_view_log, log_exists);
}

static void
chat_window_close_activate_cb (GtkWidget        *menuitem,
			       GossipChatWindow *window)
{
	g_return_if_fail (window->priv->current_chat != NULL);
	
	gossip_chat_window_remove_chat (window, window->priv->current_chat);
}

static void
chat_window_tab_left_activate_cb (GtkWidget        *menuitem,
				  GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipChat           *chat;
	gint                  index;

	priv = window->priv;
	
	chat = priv->current_chat;
	index = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));
	if (index <= 0) {
		return;
	}
	
	gossip_notebook_move_page (GOSSIP_NOTEBOOK (priv->notebook),
				   GOSSIP_NOTEBOOK (priv->notebook),
				   gossip_chat_get_widget (chat),
				   index - 1);

	chat_window_update_title (window);
	chat_window_update_menu (window);
	chat_window_update_status (window, chat);
}

static void
chat_window_tab_right_activate_cb (GtkWidget        *menuitem,
				   GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipChat           *chat;
	gint                  index;

	priv = window->priv;
	
	chat = priv->current_chat;
	index = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));

	gossip_notebook_move_page (GOSSIP_NOTEBOOK (priv->notebook),
				   GOSSIP_NOTEBOOK (priv->notebook),
				   gossip_chat_get_widget (chat),
				   index + 1);

	chat_window_update_title (window);
	chat_window_update_menu (window);
	chat_window_update_status (window, chat);
}

static void
chat_window_detach_activate_cb (GtkWidget        *menuitem,
				GossipChatWindow *window)
{
	GossipChatWindow *new_window;
	GossipChat       *chat;
	
	chat = window->priv->current_chat;
	new_window = gossip_chat_window_new ();

	gossip_chat_window_remove_chat (window, chat);
	gossip_chat_window_add_chat (new_window, chat);
}

static gboolean
chat_window_delete_event_cb (GtkWidget        *dialog,
			     GdkEvent         *event,
			     GossipChatWindow *window)
{
	GList *list, *l;

	list = g_list_copy (window->priv->chats);

	for (l = list; l; l = l->next) {
		GossipChat *chat = l->data;
		gossip_chat_window_remove_chat (window, chat);
	}

	g_list_free (list);

	return TRUE;
}

static void
chat_window_presence_changed_cb (GossipChat       *chat,
				 GossipChatWindow *window)
{
	chat_window_update_status (window, chat);
}

static void
chat_window_composing_cb (GossipChat       *chat,
			  gboolean          is_composing,
			  GossipChatWindow *window)
{
	if (is_composing && !g_list_find (window->priv->chats_composing, chat)) {
		window->priv->chats_composing = g_list_prepend (window->priv->chats_composing, chat);
	} else {
		window->priv->chats_composing = g_list_remove (window->priv->chats_composing, chat);
	}

	chat_window_update_status (window, chat);
}

static void
chat_window_new_message_cb (GossipChat       *chat,
			    GossipChatWindow *window)
{
	if (!chat_window_has_toplevel_focus (window)) {
		window->priv->new_msg = TRUE;
		chat_window_update_title (window);
		gossip_sound_play (GOSSIP_SOUND_CHAT);
	}

	if (chat == window->priv->current_chat) {
		return;
	}

	if (!g_list_find (window->priv->chats_new_msg, chat)) {
		window->priv->chats_new_msg = g_list_prepend (window->priv->chats_new_msg, chat);
		chat_window_update_status (window, chat);
	}
}

static void
chat_window_switch_page_cb (GtkNotebook	     *notebook,
			    GtkNotebookPage  *page,
			    gint	      page_num,
			    GossipChatWindow *window)
{
	GossipChat *chat;
	GtkWidget  *child;

	child = gtk_notebook_get_nth_page (notebook, page_num); 	
	chat = g_object_get_data (G_OBJECT (child), "chat");

	if (window->priv->current_chat == chat) {
		return;
	}

	window->priv->current_chat = chat;
	window->priv->chats_new_msg = g_list_remove (window->priv->chats_new_msg, chat);

	chat_window_update_title (window);
	chat_window_update_menu (window);
	chat_window_update_status (window, chat);
}

static void
chat_window_tab_added_cb (GossipNotebook   *notebook,
			  GtkWidget	   *child,
			  GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipChat *chat;
	GtkWidget  *label;

	priv = window->priv;

	chat = g_object_get_data (G_OBJECT (child), "chat");

	gossip_chat_set_window (chat, window);

	priv->chats = g_list_append (priv->chats, chat);

	label = chat_window_create_label (window, chat);
	gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), child, label);

	g_signal_connect (chat, "presence_changed",
			  G_CALLBACK (chat_window_presence_changed_cb),
			  window);
	g_signal_connect (chat, "composing",
			  G_CALLBACK (chat_window_composing_cb),
			  window);
	g_signal_connect (chat, "new_message",
			  G_CALLBACK (chat_window_new_message_cb),
			  window);

	chat_window_update_status (window, chat);
	chat_window_update_title (window);
	chat_window_update_menu (window);
}

static void
chat_window_tab_removed_cb (GossipNotebook   *notebook,
			    GtkWidget	     *child,
			    GossipChatWindow *window)
{
	GossipChat *chat;

	chat = g_object_get_data (G_OBJECT (child), "chat");
	
	gossip_chat_set_window (chat, NULL);

	g_signal_handlers_disconnect_by_func (chat,
					      G_CALLBACK (chat_window_presence_changed_cb),
					      window);
	g_signal_handlers_disconnect_by_func (chat,
					      G_CALLBACK (chat_window_composing_cb),
					      window);
	g_signal_handlers_disconnect_by_func (chat,
					      G_CALLBACK (chat_window_new_message_cb),
					      window);

	window->priv->chats = g_list_remove (window->priv->chats, chat);

	if (window->priv->chats == NULL) {
		g_object_unref (window);
	} else {
		chat_window_update_menu (window);
	}
}

static void
chat_window_tab_detached_cb (GossipNotebook   *notebook,
			     GtkWidget	      *child,
			     GossipChatWindow *window)
{
	GossipChatWindow *new_window;
	GossipChat	 *chat;

	chat = g_object_get_data (G_OBJECT (child), "chat");
	new_window = gossip_chat_window_new ();

	gossip_chat_window_remove_chat (window, chat);
	gossip_chat_window_add_chat (new_window, chat);
}

static void
chat_window_tabs_reordered_cb (GossipNotebook   *notebook,
			       GossipChatWindow *window)
{
	chat_window_update_menu (window);
}

static void
chat_window_close_clicked_cb (GtkWidget  *button,
			      GossipChat *chat)
{
	GossipChatWindow *window;

	window = gossip_chat_get_window (chat);
	gossip_chat_window_remove_chat (window, chat);
}

static gboolean
chat_window_focus_in_event_cb (GtkWidget        *widget,
			       GdkEvent         *event,
			       GossipChatWindow *window)
{
	window->priv->new_msg = FALSE;
	chat_window_update_title (window);

	return FALSE;
}

static const gchar *
chat_window_get_name (GossipChatWindow *window, GossipChat *chat)
{
	GossipChatWindowPriv *priv;
	GossipRosterItem     *item;

	priv = window->priv;

	item = gossip_chat_get_item (chat);

	return gossip_roster_item_get_name (item);
}

GtkWidget *
chat_window_create_notebook (gpointer data)
{
	GtkWidget *notebook;

	notebook = gossip_notebook_new ();
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
	gtk_widget_show (notebook);

	return notebook;
}

GossipChatWindow *
gossip_chat_window_new (void)
{
	return GOSSIP_CHAT_WINDOW (g_object_new (GOSSIP_TYPE_CHAT_WINDOW, NULL));
}

GtkWidget *
gossip_chat_window_get_dialog (GossipChatWindow *window)
{
	g_return_val_if_fail (window != NULL, NULL);
	
	return window->priv->dialog;
}

void
gossip_chat_window_add_chat (GossipChatWindow *window,
			     GossipChat	      *chat)
{
	GtkWidget *label;

	label = chat_window_create_label (window, chat);

	gossip_notebook_insert_page (GOSSIP_NOTEBOOK (window->priv->notebook),
	  			     gossip_chat_get_widget (chat),
				     label,
				     GOSSIP_NOTEBOOK_INSERT_LAST,
				     TRUE);

	/* FIXME: somewhat ugly */
	g_object_ref (chat);
}

void
gossip_chat_window_remove_chat (GossipChatWindow *window,
				GossipChat	 *chat)
{
	gossip_notebook_remove_page (GOSSIP_NOTEBOOK (window->priv->notebook),
				     gossip_chat_get_widget (chat));
	/* FIXME: somewhat ugly */
	g_object_unref (chat);
}

void
gossip_chat_window_switch_to_chat (GossipChatWindow *window,
				   GossipChat	    *chat)
{
	gint page_num;

	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window->priv->notebook),
					  gossip_chat_get_widget (chat));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook),
				       page_num);
}
