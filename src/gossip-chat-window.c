/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2006 Imendio AB
 * Copyright (C) 2003-2004 Geert-Jan Van den Bogaerde <geertjan@gnome.org>
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
#include <gdk/gdkkeysyms.h>
#include <glade/glade.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>

#include <libgossip/gossip-protocol.h>

#include "gossip-add-contact-window.h"
#include "gossip-app.h"
#include "gossip-chat-invite.h"
#include "gossip-chat-window.h"
#include "gossip-contact-info-dialog.h"
#include "gossip-log.h"
#include "gossip-log-window.h"
#include "gossip-notebook.h"
#include "gossip-preferences.h"
#include "gossip-private-chat.h"
#include "gossip-sound.h"
#include "gossip-stock.h"
#include "gossip-ui-utils.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CHAT_WINDOW, GossipChatWindowPriv))

#define DEBUG_MSG(x)
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n"); */

static void       gossip_chat_window_class_init         (GossipChatWindowClass *klass);
static void       gossip_chat_window_init               (GossipChatWindow      *window);
static void       gossip_chat_window_finalize           (GObject               *object);
static GdkPixbuf *chat_window_get_status_pixbuf         (GossipChatWindow      *window,
							 GossipChat            *chat);
static void       chat_window_accel_cb                  (GtkAccelGroup         *accelgroup,
							 GObject               *object,
							 guint                  key,
							 GdkModifierType        mod,
							 GossipChatWindow      *window);
static gboolean   chat_window_status_enter_notify_cb    (GtkWidget             *widget,
							 GdkEventCrossing      *event,
							 GossipChat            *chat);
static gboolean   chat_window_status_leave_notify_cb    (GtkWidget             *widget,
							 GdkEventCrossing      *event,
							 GossipChat            *chat);
static gboolean   chat_window_status_button_press_cb    (GtkWidget             *widget,
							 GdkEventButton        *event,
							 GossipChat            *chat);
static GtkWidget *chat_window_create_label              (GossipChatWindow      *window,
							 GossipChat            *chat);
static void       chat_window_update_status             (GossipChatWindow      *window,
							 GossipChat            *chat);
static void       chat_window_update_title              (GossipChatWindow      *window);
static void       chat_window_update_menu               (GossipChatWindow      *window);
static void       chat_window_invite_menu_setup         (GossipChatWindow      *window);
static void       chat_window_conv_activate_cb          (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_clear_activate_cb         (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_info_activate_cb          (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_add_contact_activate_cb   (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_log_activate_cb           (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_show_contacts_toggled_cb  (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_edit_activate_cb          (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_insert_smiley_activate_cb (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_close_activate_cb         (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_cut_activate_cb           (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_copy_activate_cb          (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_paste_activate_cb         (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_tabs_left_activate_cb     (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_tabs_right_activate_cb    (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_detach_activate_cb        (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static gboolean   chat_window_delete_event_cb           (GtkWidget             *dialog,
							 GdkEvent              *event,
							 GossipChatWindow      *window);
static void       chat_window_status_changed_cb         (GossipChat            *chat,
							 GossipChatWindow      *window);
static void       chat_window_update_tooltip            (GossipChatWindow      *window,
							 GossipChat            *chat);
static void       chat_window_name_changed_cb           (GossipChat            *chat,
							 const gchar           *name,
							 GossipChatWindow      *window);
static void       chat_window_composing_cb              (GossipChat            *chat,
							 gboolean               is_composing,
							 GossipChatWindow      *window);
static void       chat_window_new_message_cb            (GossipChat            *chat,
							 GossipChatWindow      *window);
static void       chat_window_disconnected_cb           (GossipApp             *app,
							 GossipChatWindow      *window);
static void       chat_window_switch_page_cb            (GtkNotebook           *notebook,
							 GtkNotebookPage       *page,
							 gint                   page_num,
							 GossipChatWindow      *window);
static void       chat_window_tab_added_cb              (GossipNotebook        *notebook,
							 GtkWidget             *child,
							 GossipChatWindow      *window);
static void       chat_window_tab_removed_cb            (GossipNotebook        *notebook,
							 GtkWidget             *child,
							 GossipChatWindow      *window);
static void       chat_window_tab_detached_cb           (GossipNotebook        *notebook,
							 GtkWidget             *widget,
							 GossipChatWindow      *window);
static void       chat_window_tabs_reordered_cb         (GossipNotebook        *notebook,
							 GossipChatWindow      *window);
static gboolean   chat_window_focus_in_event_cb         (GtkWidget             *widget,
							 GdkEvent              *event,
							 GossipChatWindow      *window);
static void       chat_window_drag_data_received        (GtkWidget             *widget,
							 GdkDragContext        *context,
							 int                    x,
							 int                    y,
							 GtkSelectionData      *selection,
							 guint                  info,
							 guint                  time,
							 GossipChatWindow      *window);

/* Called from Glade, so it shouldn't be static. */
GtkWidget * chat_window_create_notebook (gpointer data);

struct _GossipChatWindowPriv {
	GList       *chats;
	GList       *chats_new_msg;
	GList       *chats_composing;
	GossipChat  *current_chat;
	gboolean     new_msg;

	GtkWidget   *dialog;
	GtkWidget   *notebook;

	GtkTooltips *tooltips;

	/* Menu items. */
	GtkWidget   *menu_conv_clear;
	GtkWidget   *menu_conv_add_contact;
	GtkWidget   *menu_conv_log;
	GtkWidget   *menu_conv_info;
	GtkWidget   *menu_conv_show_contacts_sep;
	GtkWidget   *menu_conv_show_contacts;
	GtkWidget   *menu_conv_invite_contacts_sep;
	GtkWidget   *menu_conv_invite_contacts;
	GtkWidget   *menu_conv_close;
	GtkWidget   *menu_edit_insert_smiley;
	GtkWidget   *menu_edit_cut;
	GtkWidget   *menu_edit_copy;
	GtkWidget   *menu_edit_paste;
	GtkWidget   *menu_tabs_next;
	GtkWidget   *menu_tabs_prev;
	GtkWidget   *menu_tabs_left;
	GtkWidget   *menu_tabs_right;
	GtkWidget   *menu_tabs_detach;
};

static GList *chat_windows = NULL;

static guint tab_accel_keys[] = {
	GDK_1, GDK_2, GDK_3, GDK_4, GDK_5,
	GDK_6, GDK_7, GDK_8, GDK_9, GDK_0
};

enum DndDropType {
	DND_DROP_TYPE_CONTACT_ID,
};

static GtkTargetEntry drop_types[] = {
	{ "text/contact-id", 0, DND_DROP_TYPE_CONTACT_ID },
};

G_DEFINE_TYPE (GossipChatWindow, gossip_chat_window, G_TYPE_OBJECT);

static void
gossip_chat_window_class_init (GossipChatWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gossip_chat_window_finalize;

	g_type_class_add_private (object_class, sizeof (GossipChatWindowPriv));
}

static void
gossip_chat_window_init (GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GladeXML             *glade;
	GtkAccelGroup        *accel_group;
	GClosure             *closure;
	GtkWidget            *menu_conv;
	GtkWidget            *menu;
	gint                  i;

	priv = GET_PRIV (window);

	glade = gossip_glade_get_file (GLADEDIR "/chat.glade",
				       "chat_window",
				       NULL,
				       "chat_window", &priv->dialog,
				       "chats_notebook", &priv->notebook,
				       "menu_conv", &menu_conv,
				       "menu_conv_clear", &priv->menu_conv_clear,
				       "menu_conv_add_contact", &priv->menu_conv_add_contact,
				       "menu_conv_log", &priv->menu_conv_log,
				       "menu_conv_info", &priv->menu_conv_info,
				       "sep_conv_show_contacts", &priv->menu_conv_show_contacts_sep,
				       "menu_conv_show_contacts", &priv->menu_conv_show_contacts,
				       "sep_conv_invite_contacts", &priv->menu_conv_invite_contacts_sep,
				       "menu_conv_invite_contacts", &priv->menu_conv_invite_contacts,
				       "menu_conv_close", &priv->menu_conv_close,
				       "menu_edit_insert_smiley", &priv->menu_edit_insert_smiley,
				       "menu_edit_cut", &priv->menu_edit_cut,
				       "menu_edit_copy", &priv->menu_edit_copy,
				       "menu_edit_paste", &priv->menu_edit_paste,
				       "menu_tabs_next", &priv->menu_tabs_next,
				       "menu_tabs_prev", &priv->menu_tabs_prev,
				       "menu_tabs_left", &priv->menu_tabs_left,
				       "menu_tabs_right", &priv->menu_tabs_right,
				       "menu_tabs_detach", &priv->menu_tabs_detach,
				       NULL);

	gossip_glade_connect (glade, 
			      window,
			      "menu_conv", "activate", chat_window_conv_activate_cb,
			      "menu_conv_clear", "activate", chat_window_clear_activate_cb,
			      "menu_conv_add_contact", "activate", chat_window_add_contact_activate_cb,
			      "menu_conv_log", "activate", chat_window_log_activate_cb,
			      "menu_conv_info", "activate", chat_window_info_activate_cb,
			      "menu_conv_close", "activate", chat_window_close_activate_cb,
			      "menu_edit", "activate", chat_window_edit_activate_cb,
			      "menu_edit_cut", "activate", chat_window_cut_activate_cb,
			      "menu_edit_copy", "activate", chat_window_copy_activate_cb,
			      "menu_edit_paste", "activate", chat_window_paste_activate_cb,
			      "menu_tabs_left", "activate", chat_window_tabs_left_activate_cb,
			      "menu_tabs_right", "activate", chat_window_tabs_right_activate_cb,
			      "menu_tabs_detach", "activate", chat_window_detach_activate_cb,
			      NULL);

	g_object_unref (glade);

	/* Set up accels */
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

	/* Set up smiley menu */
	menu = gossip_chat_view_get_smiley_menu (
		G_CALLBACK (chat_window_insert_smiley_activate_cb), 
		window);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (priv->menu_edit_insert_smiley), menu);

	/* Set up signals we can't do with glade */
	g_signal_connect (priv->menu_conv_show_contacts, 
			  "toggled",
			  G_CALLBACK (chat_window_show_contacts_toggled_cb),
			  window);

	g_signal_connect_swapped (priv->menu_tabs_prev,
			          "activate",
			          G_CALLBACK (gtk_notebook_prev_page),
				  priv->notebook);
	g_signal_connect_swapped (priv->menu_tabs_next,
			          "activate",
			          G_CALLBACK (gtk_notebook_next_page),
			          priv->notebook);
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
	g_signal_connect (gossip_app_get_session (),
			  "disconnected",
			  G_CALLBACK (chat_window_disconnected_cb),
			  window);
	
	/* Set up drag and drop */
	gtk_drag_dest_set (GTK_WIDGET (priv->notebook), 
			   GTK_DEST_DEFAULT_ALL,
			   drop_types, 
			   G_N_ELEMENTS (drop_types),
			   GDK_ACTION_COPY);

	g_signal_connect (priv->notebook, 
			  "drag-data-received",
			  G_CALLBACK (chat_window_drag_data_received),
			  window);

	chat_windows = g_list_prepend (chat_windows, window);

	/* Set up private details */
	priv->chats = NULL;
	priv->chats_new_msg = NULL;
	priv->chats_composing = NULL;
	priv->current_chat = NULL;

	priv->tooltips = gtk_tooltips_new ();
}

/* Returns the window to open a new tab in if there is only one window
 * visble, otherwise, returns NULL indicating that a new window should
 * be added.
 */
GossipChatWindow *
gossip_chat_window_get_default (void)
{
	GList       *l;
        gboolean     separate_windows = TRUE;
        GConfClient *gconf;
        GError      *error;
        
        gconf = gconf_client_get_default ();

        error = NULL;
        separate_windows = gconf_client_get_bool (gconf,
                                                  "/apps/gossip/ui/separate_chat_windows",
                                                  &error);
        if (error) {
                separate_windows = FALSE;
                g_error_free (error);
        }

        if (separate_windows) {
                /* Always create a new window */
                return NULL;
        }

	for (l = chat_windows; l; l = l->next) {
		GossipChatWindow *chat_window;
		GtkWidget        *dialog;
		GdkWindow        *window;
		gboolean          visible;

		chat_window = l->data;
                
		dialog = gossip_chat_window_get_dialog (chat_window);
                window = dialog->window;

		g_object_get (dialog, 
			      "visible", &visible,
			      NULL);
		
		visible = visible && !(gdk_window_get_state (window) & GDK_WINDOW_STATE_ICONIFIED);
                
                if (visible) {
                        /* Found a visible window on this desktop */
                        return chat_window;
                }
	}

        return NULL;
}

static void
gossip_chat_window_finalize (GObject *object)
{
	GossipChatWindow     *window;
	GossipChatWindowPriv *priv;

	window = GOSSIP_CHAT_WINDOW (object);
	priv = GET_PRIV (window);

	g_signal_handlers_disconnect_by_func (gossip_app_get_session (),
					      chat_window_disconnected_cb,
					      window);

	chat_windows = g_list_remove (chat_windows, window);
	gtk_widget_destroy (priv->dialog);

	G_OBJECT_CLASS (gossip_chat_window_parent_class)->finalize (object);
}

static GdkPixbuf *
chat_window_get_status_pixbuf (GossipChatWindow *window,
			       GossipChat       *chat)
{
	GossipChatWindowPriv *priv;
	GdkPixbuf            *pixbuf;

	priv = GET_PRIV (window);

	if (g_list_find (priv->chats_new_msg, chat)) {
		pixbuf = gossip_pixbuf_from_stock (GOSSIP_STOCK_MESSAGE, 
						   GTK_ICON_SIZE_MENU);
	}
	else if (g_list_find (priv->chats_composing, chat)) {
		pixbuf = gossip_pixbuf_from_stock (GOSSIP_STOCK_TYPING, 
						   GTK_ICON_SIZE_MENU);
	}
	else {
		pixbuf = gossip_chat_get_status_pixbuf (chat);
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
	GossipChatWindowPriv *priv;
	gint                  i, num = -1;

	priv = GET_PRIV (window);

	for (i = 0; i < G_N_ELEMENTS (tab_accel_keys); i++) {
		if (tab_accel_keys[i] == key) {
			num = i;
			break;
		}
	}

	if (num != -1) {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), num);
	}	
}

static gboolean
chat_window_status_enter_notify_cb (GtkWidget        *widget,
				    GdkEventCrossing *event,
				    GossipChat       *chat)
{
	GossipChatWindow     *window;
	GossipChatWindowPriv *priv;
	GtkImage             *image;
	GdkPixbuf            *pixbuf;

	pixbuf = gossip_pixbuf_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
	image = g_object_get_data (G_OBJECT (chat), "chat-window-tab-image");
	gtk_image_set_from_pixbuf (image, pixbuf);

	/* Set tooltip */
	window = gossip_chat_get_window (chat);
	priv = GET_PRIV (window);

	gtk_tooltips_set_tip (priv->tooltips,
			      widget,
			      _("Close this chat window"),
			      NULL);

	/* Allow closing with left button click */
 	g_signal_connect (widget, "button_press_event", 
 			  G_CALLBACK (chat_window_status_button_press_cb), 
 			  chat); 

	return FALSE;
}

static gboolean
chat_window_status_leave_notify_cb (GtkWidget        *widget,
				    GdkEventCrossing *event,
				    GossipChat       *chat)
{
	GossipChatWindow *window;
	GtkImage         *image;
	GdkPixbuf        *pixbuf;

	image = g_object_get_data (G_OBJECT (chat), "chat-window-tab-image");

	window = gossip_chat_get_window (chat);
	pixbuf = chat_window_get_status_pixbuf (window, chat);
	gtk_image_set_from_pixbuf (image, pixbuf);
	
	/* Stop the close window left mouse click ability */
 	g_signal_handlers_disconnect_by_func (widget,
					      chat_window_status_button_press_cb, 
					      chat); 
	return FALSE;
}

static gboolean
chat_window_status_button_press_cb (GtkWidget      *widget,
				    GdkEventButton *event,
				    GossipChat     *chat)
{
	GossipChatWindow *window;

	if (event->button != 1) {
		return FALSE;
	}

	window = gossip_chat_get_window (chat);
	gossip_chat_window_remove_chat (window, chat);

	return FALSE;
}

static GtkWidget *
chat_window_create_label (GossipChatWindow *window,
			  GossipChat       *chat)
{
	GossipChatWindowPriv *priv;
	GtkWidget            *hbox;
	GtkWidget            *label;
	GtkWidget            *image;
	const gchar          *name;
	GtkWidget            *event_box_tab; 
	GtkWidget            *event_box_image;
	PangoAttrList        *attr_list;
	PangoAttribute       *attr;

	priv = GET_PRIV (window);

	/* Set up event box for tooltip */
 	event_box_tab = gtk_event_box_new (); 
 	gtk_event_box_set_visible_window (GTK_EVENT_BOX (event_box_tab), FALSE); 
  	g_object_set_data (G_OBJECT (chat), "chat-window-tab-tooltip-widget", event_box_tab);

	/* Set up label */
	name = gossip_chat_get_name (chat);
	label = gtk_label_new (name);

	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	
	attr_list = pango_attr_list_new ();
	attr = pango_attr_scale_new (1/1.2);
	attr->start_index = 0;
	attr->end_index = -1;
	pango_attr_list_insert (attr_list, attr);
	gtk_label_set_attributes (GTK_LABEL (label), attr_list);
	pango_attr_list_unref (attr_list);

	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	g_object_set_data (G_OBJECT (chat), "chat-window-tab-label", label); 

	/* Set up event box for label */
 	event_box_image = gtk_event_box_new ();
 	gtk_event_box_set_visible_window (GTK_EVENT_BOX (event_box_image), FALSE);

	/* Set up image */
	image = gtk_image_new ();
	g_object_set_data (G_OBJECT (chat), "chat-window-tab-image", image);

 	gtk_container_add (GTK_CONTAINER (event_box_image), image); 

	/* Add to the hbox */
	hbox = gtk_hbox_new (FALSE, 2);
  	gtk_box_pack_start (GTK_BOX (hbox), event_box_image, FALSE, FALSE, 0);  
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

	/* Add the hbox to the tooltip event box, this way both the
	 * image and the label get the tooltip text.
	 */
	gtk_container_add (GTK_CONTAINER (event_box_tab), hbox);

	/* Set up motion events for both event boxes */
	gtk_widget_add_events (event_box_image, 
			       GDK_ENTER_NOTIFY_MASK |
			       GDK_LEAVE_NOTIFY_MASK);
	g_signal_connect (event_box_image, "enter-notify-event",
			  G_CALLBACK (chat_window_status_enter_notify_cb),
			  chat);
	g_signal_connect (event_box_image, "leave-notify-event",
			  G_CALLBACK (chat_window_status_leave_notify_cb),
			  chat);

	/* Set up tooltip */
	chat_window_update_tooltip (window, chat);

	/* Make everything visible */
	gtk_widget_show_all (event_box_tab);

	return event_box_tab;
}

static void
chat_window_update_status (GossipChatWindow *window, 
			   GossipChat       *chat)
{
	GtkImage  *image;
	GdkPixbuf *pixbuf;

	pixbuf = chat_window_get_status_pixbuf (window, chat);
	image = g_object_get_data (G_OBJECT (chat), "chat-window-tab-image");
	gtk_image_set_from_pixbuf (image, pixbuf);
	
	chat_window_update_tooltip (window, chat);
}

static void
chat_window_update_title (GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	gchar                *title; 
	GdkPixbuf 	     *pixbuf;

	priv = GET_PRIV (window);

	title = g_strdup_printf (("%s%s"),
				 priv->new_msg ? "*" : "",
				 gossip_chat_get_name (priv->current_chat));

	gtk_window_set_title (GTK_WINDOW (priv->dialog), title);
	if (priv->new_msg) {
		pixbuf = gossip_pixbuf_from_stock (GOSSIP_STOCK_MESSAGE,
						   GTK_ICON_SIZE_MENU);
		gtk_window_set_icon (GTK_WINDOW (priv->dialog), pixbuf);
		gtk_window_set_urgency_hint (GTK_WINDOW (priv->dialog), TRUE);
        } else {
		gtk_window_set_icon (GTK_WINDOW (priv->dialog), NULL);
		gtk_window_set_urgency_hint (GTK_WINDOW (priv->dialog), FALSE);
	}
	g_free (title);
}

static void
chat_window_update_menu (GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipContact        *contact;
	gboolean              first_page;
	gboolean              last_page;
	gint                  num_pages;
	gint                  page_num;
	
	/* Group chat menu details */
	gboolean              is_group_chat;
	gboolean              show_contacts;

	priv = GET_PRIV (window);

	page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));
	num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook));
	first_page = (page_num == 0);
	last_page = (page_num == (num_pages - 1));

	contact = gossip_chat_get_contact (priv->current_chat);

	gtk_widget_set_sensitive (priv->menu_tabs_next, !last_page);
	gtk_widget_set_sensitive (priv->menu_tabs_prev, !first_page);
	gtk_widget_set_sensitive (priv->menu_tabs_detach, num_pages > 1);
	gtk_widget_set_sensitive (priv->menu_tabs_left, !first_page);
	gtk_widget_set_sensitive (priv->menu_tabs_right, !last_page);
	gtk_widget_set_sensitive (priv->menu_conv_info, contact != NULL);

	is_group_chat = gossip_chat_get_group_chat (priv->current_chat);
	if (is_group_chat) {
		gtk_widget_show (priv->menu_conv_show_contacts_sep);
		gtk_widget_show (priv->menu_conv_show_contacts);

		show_contacts = gossip_chat_get_show_contacts (priv->current_chat);

		/* We need to block the signal here because all we are
		 * really trying to do is check or uncheck the menu
		 * item. If we don't do this we get funny behaviour
		 * with 2 or more group chat windows where showing
		 * contacts doesn't do anything. 
		 */
		g_signal_handlers_block_by_func (priv->menu_conv_show_contacts, 
						 chat_window_show_contacts_toggled_cb, 
						 window);
		
		g_object_set (priv->menu_conv_show_contacts, 
			      "active", show_contacts,
			      NULL);

		g_signal_handlers_unblock_by_func (priv->menu_conv_show_contacts, 
						   chat_window_show_contacts_toggled_cb,
						   window);
	} else {
		GossipContactType type;

		gtk_widget_hide (priv->menu_conv_show_contacts_sep);
		gtk_widget_hide (priv->menu_conv_show_contacts);

		type = gossip_contact_get_type (contact);
		if (type != GOSSIP_CONTACT_TYPE_CONTACTLIST &&
		    type != GOSSIP_CONTACT_TYPE_USER) {
			gtk_widget_show (priv->menu_conv_add_contact);
		} else {
			gtk_widget_hide (priv->menu_conv_add_contact);
		}
	}

 	chat_window_invite_menu_setup (window);  
}

static void
chat_window_invite_menu_setup (GossipChatWindow *window) 
{
	GossipChatWindowPriv *priv;
	GossipContact        *contact;
	GtkWidget            *menu = NULL;
	gboolean              is_group_chat;

	priv = GET_PRIV (window);
	
	contact = gossip_chat_get_own_contact (priv->current_chat);
	
	if (!contact) {
		/* use the other contact if we don't have ourselves */
		contact = gossip_chat_get_contact (priv->current_chat);

		/* FIXME: Don't know if this is correct, but it fixes two
		 * critical warnings when joining a group chat.
		 */
		if (!contact) {
			return;
		}
	}

	is_group_chat = gossip_chat_get_group_chat (priv->current_chat);

	if (is_group_chat) {
		GossipChatroomId id;

		id = gossip_group_chat_get_room_id (GOSSIP_GROUP_CHAT (priv->current_chat));
		menu = gossip_chat_invite_groupchat_menu (contact, id);
	} else {
 		menu = gossip_chat_invite_contact_menu (contact); 
	}

	if (menu) {
		gtk_widget_show (priv->menu_conv_invite_contacts_sep);
		gtk_widget_show (priv->menu_conv_invite_contacts);

		gtk_widget_show_all (menu);
		
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (priv->menu_conv_invite_contacts),
					   menu);

	} else {
		gtk_menu_item_remove_submenu (GTK_MENU_ITEM (priv->menu_conv_invite_contacts));

		gtk_widget_hide (priv->menu_conv_invite_contacts_sep);
		gtk_widget_hide (priv->menu_conv_invite_contacts);
	}
}

static void
chat_window_insert_smiley_activate_cb (GtkWidget        *menuitem, 
				       GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipChat           *chat;
	GtkTextBuffer        *buffer;
	GtkTextIter           iter;
	const gchar          *smiley;

	priv = GET_PRIV (window);

	chat = priv->current_chat;
	
	smiley = g_object_get_data (G_OBJECT (menuitem), "smiley_text");

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert (buffer, &iter, 
				smiley, 
				g_utf8_strlen (smiley, -1));
}

static void
chat_window_clear_activate_cb (GtkWidget        *menuitem, 
			       GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	gossip_chat_clear (priv->current_chat);
}

static void
chat_window_add_contact_activate_cb (GtkWidget        *menuitem, 
				     GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipContact        *contact;

	priv = GET_PRIV (window);

	contact = gossip_chat_get_contact (priv->current_chat);

	gossip_add_contact_window_show (NULL, contact);
}

static void
chat_window_log_activate_cb (GtkWidget        *menuitem, 
			     GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipContact        *contact;

	priv = GET_PRIV (window);

	contact = gossip_chat_get_contact (priv->current_chat);

	gossip_log_window_show (contact);
}

static void
chat_window_info_activate_cb (GtkWidget        *menuitem, 
			      GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipContact        *contact;
	
	priv = GET_PRIV (window);

	contact = gossip_chat_get_contact (priv->current_chat);

	gossip_contact_info_dialog_show (contact);
}

static void
chat_window_conv_activate_cb (GtkWidget        *menuitem,
			      GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipContact        *contact;
	gboolean              log_exists;

	priv = GET_PRIV (window);
	
	contact = gossip_chat_get_contact (priv->current_chat);
	if (contact != NULL) {
		GossipLog *log;

		log = gossip_log_get (contact);
		log_exists = gossip_log_exists (log);
	} else {
		log_exists = FALSE;
	}

	gtk_widget_set_sensitive (priv->menu_conv_log, log_exists);
}

static void
chat_window_show_contacts_toggled_cb (GtkWidget        *menuitem,
				      GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	gboolean              show;

	priv = GET_PRIV (window);

	g_return_if_fail (priv->current_chat != NULL);
	
	show = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (priv->menu_conv_show_contacts)); 
	gossip_chat_set_show_contacts (priv->current_chat, show);
}

static void
chat_window_close_activate_cb (GtkWidget        *menuitem,
			       GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	g_return_if_fail (priv->current_chat != NULL);

	gossip_chat_window_remove_chat (window, priv->current_chat);
}

static void
chat_window_edit_activate_cb (GtkWidget        *menuitem,
			      GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipChat           *chat;
	GtkClipboard         *clipboard;
	GtkTextBuffer        *buffer;

	priv = GET_PRIV (window);

	g_return_if_fail (priv->current_chat != NULL);

	chat = priv->current_chat;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	if (gtk_text_buffer_get_selection_bounds (buffer, NULL, NULL)) {
		gtk_widget_set_sensitive (priv->menu_edit_copy, TRUE);
		gtk_widget_set_sensitive (priv->menu_edit_cut, TRUE);
	} else {
		gtk_widget_set_sensitive (priv->menu_edit_cut, FALSE);

		if (gossip_chat_view_get_selection_bounds (chat->view, NULL, NULL)) {
			gtk_widget_set_sensitive (priv->menu_edit_copy, TRUE);
		} else {
			gtk_widget_set_sensitive (priv->menu_edit_copy, FALSE);
		}
	}

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	if (gtk_clipboard_wait_is_text_available (clipboard)) {
		gtk_widget_set_sensitive (priv->menu_edit_paste, TRUE);
	} else {
		gtk_widget_set_sensitive (priv->menu_edit_paste, FALSE);
	}
}

static void
chat_window_cut_activate_cb (GtkWidget        *menuitem,
			     GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	
	g_return_if_fail (GOSSIP_IS_CHAT_WINDOW (window));

	priv = GET_PRIV (window);

	gossip_chat_cut (priv->current_chat);
}

static void
chat_window_copy_activate_cb (GtkWidget        *menuitem,
			      GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	
	g_return_if_fail (GOSSIP_IS_CHAT_WINDOW (window));

	priv = GET_PRIV (window);

	gossip_chat_copy (priv->current_chat);
}

static void
chat_window_paste_activate_cb (GtkWidget        *menuitem,
			       GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	
	g_return_if_fail (GOSSIP_IS_CHAT_WINDOW (window));
	
	priv = GET_PRIV (window);

	gossip_chat_paste (priv->current_chat);
}

static void
chat_window_tabs_left_activate_cb (GtkWidget        *menuitem,
				  GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipChat           *chat;
	gint                  index;

	priv = GET_PRIV (window);

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
chat_window_tabs_right_activate_cb (GtkWidget        *menuitem,
				   GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipChat           *chat;
	gint                  index;

	priv = GET_PRIV (window);

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
	GossipChatWindowPriv *priv;
	GossipChatWindow     *new_window;
	GossipChat           *chat;

	priv = GET_PRIV (window);

	chat = priv->current_chat;
	new_window = gossip_chat_window_new ();

	gossip_chat_window_remove_chat (window, chat);
	gossip_chat_window_add_chat (new_window, chat);
}

static gboolean
chat_window_delete_event_cb (GtkWidget        *dialog,
			     GdkEvent         *event,
			     GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GList                *list;
	GList                *l;

	priv = GET_PRIV (window);

	DEBUG_MSG (("Chat Window: Delete event received"));

	list = g_list_copy (priv->chats);

	for (l = list; l; l = l->next) {
		GossipChat *chat = l->data;
		gossip_chat_window_remove_chat (window, chat);
	}

	g_list_free (list);

	return TRUE;
}

static void
chat_window_status_changed_cb (GossipChat       *chat,
		               GossipChatWindow *window)
{
	chat_window_update_status (window, chat);
}

static void
chat_window_update_tooltip (GossipChatWindow *window,
			    GossipChat       *chat)
{
	GossipChatWindowPriv *priv;
	GtkWidget            *widget;
	gchar                *current_tooltip;
	gchar                *str;

	priv = GET_PRIV (window);

	current_tooltip = gossip_chat_get_tooltip (chat);
	
	if (g_list_find (priv->chats_composing, chat)) {
		str = g_strconcat (current_tooltip, "\n", _("Typing a message."), NULL);
		g_free (current_tooltip);
	} else {
		str = current_tooltip;
	}

	widget = g_object_get_data (G_OBJECT (chat), "chat-window-tab-tooltip-widget");
	gtk_tooltips_set_tip (priv->tooltips,
			      widget,
			      str,
			      NULL);

	g_free (str);
}

static void 
chat_window_name_changed_cb (GossipChat       *chat,
			     const gchar      *name,
			     GossipChatWindow *window)
{
	GtkLabel *label;

	label = g_object_get_data (G_OBJECT (chat), "chat-window-tab-label");

	gtk_label_set_text (label, name);
}

static void
chat_window_composing_cb (GossipChat       *chat,
			  gboolean          is_composing,
			  GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	if (is_composing && !g_list_find (priv->chats_composing, chat)) {
		priv->chats_composing = g_list_prepend (priv->chats_composing, chat);
	} else {
		priv->chats_composing = g_list_remove (priv->chats_composing, chat);
	}

	chat_window_update_status (window, chat);
}

static void
chat_window_new_message_cb (GossipChat       *chat,
			    GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	if (!gossip_chat_window_has_focus (window)) {
		priv->new_msg = TRUE;
		chat_window_update_title (window);
	}

	if (chat == priv->current_chat) {
		return;
	}

	if (!g_list_find (priv->chats_new_msg, chat)) {
		priv->chats_new_msg = g_list_prepend (priv->chats_new_msg, chat);
		chat_window_update_status (window, chat);
	}
}

static void
chat_window_disconnected_cb (GossipApp        *app,
			     GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GList		     *l;

	g_return_if_fail (GOSSIP_IS_CHAT_WINDOW (window));

	priv = GET_PRIV (window);

	/* FIXME: this works for now, but should operate on a PER
	   protocol basis not ALL connections since some tabs will
	   belong to contacts on accounts which might still be online */ 
	for (l = priv->chats; l != NULL; l = g_list_next (l)) {
		GossipChat *chat = GOSSIP_CHAT (l->data);
		chat_window_update_status (window, chat);
	}
}

static void
chat_window_switch_page_cb (GtkNotebook	     *notebook,
			    GtkNotebookPage  *page,
			    gint	      page_num,
			    GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipChat           *chat;
	GtkWidget            *child;

	priv = GET_PRIV (window);
	
	child = gtk_notebook_get_nth_page (notebook, page_num); 	
	chat = g_object_get_data (G_OBJECT (child), "chat");

	if (priv->current_chat == chat) {
		return;
	}

	priv->current_chat = chat;
	priv->chats_new_msg = g_list_remove (priv->chats_new_msg, chat);

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
	GossipChat           *chat;
	GtkWidget            *label;

	DEBUG_MSG (("Chat Window: Tab added"));

	priv = GET_PRIV (window);

	chat = g_object_get_data (G_OBJECT (child), "chat");

	gossip_chat_set_window (chat, window);

	priv->chats = g_list_append (priv->chats, chat);

	label = chat_window_create_label (window, chat);
	gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), child, label);

	gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (notebook), child,
					    TRUE, TRUE, GTK_PACK_START);

	g_signal_connect (chat, "status_changed",
			  G_CALLBACK (chat_window_status_changed_cb),
			  window);
	g_signal_connect (chat, "name_changed",
			  G_CALLBACK (chat_window_name_changed_cb),
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

	gossip_chat_scroll_down (chat);
}

static void
chat_window_tab_removed_cb (GossipNotebook   *notebook,
			    GtkWidget	     *child,
			    GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipChat           *chat;

	DEBUG_MSG (("Chat Window: Tab removed"));

	priv = GET_PRIV (window);

	chat = g_object_get_data (G_OBJECT (child), "chat");
	
	gossip_chat_set_window (chat, NULL);

	g_signal_handlers_disconnect_by_func (chat,
					      G_CALLBACK (chat_window_status_changed_cb),
					      window);
	g_signal_handlers_disconnect_by_func (chat,
					      G_CALLBACK (chat_window_name_changed_cb),
					      window);
	g_signal_handlers_disconnect_by_func (chat,
					      G_CALLBACK (chat_window_composing_cb),
					      window);
	g_signal_handlers_disconnect_by_func (chat,
					      G_CALLBACK (chat_window_new_message_cb),
					      window);

	priv->chats = g_list_remove (priv->chats, chat);

	if (priv->chats == NULL) {
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
	GossipChatWindowPriv *priv;
	GossipChatWindow     *new_window;
	GossipChat	     *chat;
	gint                  x, y;

	DEBUG_MSG (("Chat Window: Tab detached"));

	chat = g_object_get_data (G_OBJECT (child), "chat");
	new_window = gossip_chat_window_new ();

	priv = GET_PRIV (new_window);

	gdk_display_get_pointer (gdk_display_get_default (),
				 NULL,
				 &x,
				 &y,
				 NULL);
	
	gtk_window_move (GTK_WINDOW (priv->dialog), x, y);
	
	gossip_chat_window_remove_chat (window, chat);
	gossip_chat_window_add_chat (new_window, chat);

	gtk_widget_show (priv->dialog);
}

static void
chat_window_tabs_reordered_cb (GossipNotebook   *notebook,
			       GossipChatWindow *window)
{
	chat_window_update_menu (window);
}

static gboolean
chat_window_focus_in_event_cb (GtkWidget        *widget,
			       GdkEvent         *event,
			       GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	priv->new_msg = FALSE;

	chat_window_update_title (window);

	return FALSE;
}

static void
chat_window_drag_data_received (GtkWidget        *widget,
				GdkDragContext   *context,
				int               x,
				int               y, 
				GtkSelectionData *selection,
				guint             info, 
				guint             time, 
				GossipChatWindow *window)
{
	GossipChatManager *manager;
	GossipContact     *contact;
	GossipChat        *chat;
	GossipChatWindow  *old_window;
	const gchar       *id;

	id = (const gchar*) selection->data;
	DEBUG_MSG (("Chat Window: Received drag & drop contact "
		    "from roster with id:'%s'", 
		    id));

	contact = gossip_session_find_contact (gossip_app_get_session (), id);
	
	if (!contact) {
		DEBUG_MSG (("Chat Window: No contact found associated "
			    "with drag & drop"));
		return;
	}

	manager = gossip_app_get_chat_manager ();
	chat = GOSSIP_CHAT (gossip_chat_manager_get_chat (manager, contact));
	old_window = gossip_chat_get_window (chat);
	
	if (old_window) {
		if (old_window == window) {
			gtk_drag_finish (context, TRUE, FALSE, GDK_CURRENT_TIME);
			return;
		}
		
		gossip_chat_window_remove_chat (old_window, chat);
	}

	gossip_chat_window_add_chat (window, chat);

	/* added to take care of any outstanding chat events */
	gossip_chat_manager_show_chat (manager, contact);
	
	gtk_drag_finish (context, TRUE, FALSE, GDK_CURRENT_TIME);
}

GtkWidget *
chat_window_create_notebook (gpointer data)
{
	GtkWidget *notebook;

	notebook = gossip_notebook_new ();

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
	GossipChatWindowPriv *priv;

	g_return_val_if_fail (window != NULL, NULL);

	priv = GET_PRIV (window);
	
	return priv->dialog;
}

void
gossip_chat_window_add_chat (GossipChatWindow *window,
			     GossipChat	      *chat)
{
	GossipChatWindowPriv *priv;
	GtkWidget            *label;

	DEBUG_MSG (("Chat Window: Adding chat"));

	priv = GET_PRIV (window);
	
	label = chat_window_create_label (window, chat);

	if (g_list_length (priv->chats) == 0) {
		gint width, height;

		/* First chat, resize the window to its preferred size. */
		gossip_chat_get_geometry (chat, &width, &height);
		gtk_window_resize (GTK_WINDOW (priv->dialog), width, height);
	}

	gossip_notebook_insert_page (GOSSIP_NOTEBOOK (priv->notebook),
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
	GossipChatWindowPriv *priv;

	DEBUG_MSG (("Chat Window: Removing chat"));

	priv = GET_PRIV (window);

	gossip_notebook_remove_page (GOSSIP_NOTEBOOK (priv->notebook),
				     gossip_chat_get_widget (chat));
	/* FIXME: somewhat ugly */
	g_object_unref (chat);
}

void
gossip_chat_window_switch_to_chat (GossipChatWindow *window,
				   GossipChat	    *chat)
{
	GossipChatWindowPriv *priv;
	gint                  page_num;

	priv = GET_PRIV (window);

	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook),
					  gossip_chat_get_widget (chat));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook),
				       page_num);
}

gboolean
gossip_chat_window_has_focus (GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	gboolean              has_focus;

	g_return_val_if_fail (GOSSIP_IS_CHAT_WINDOW (window), FALSE);
	
	priv = GET_PRIV (window);

	g_object_get (priv->dialog, "has-toplevel-focus", &has_focus, NULL);

	return has_focus;
}

