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

#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-conf.h>
#include <libgossip/gossip-protocol.h>

#include "gossip-add-contact-dialog.h"
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

#define DEBUG_DOMAIN "ChatWindow"

#define URGENCY_TIMEOUT 60*1000

struct _GossipChatWindowPriv {
	GList       *chats;
	GList       *chats_new_msg;
	GList       *chats_composing;

	GossipChat  *current_chat;

	gboolean     new_msg;

	guint        urgency_timeout_id;

	GtkWidget   *dialog;
	GtkWidget   *notebook;

	GtkTooltips *tooltips;

	/* Menu items. */
	GtkWidget   *menu_conv_clear;
	GtkWidget   *menu_conv_log;
	GtkWidget   *menu_conv_separator;
	GtkWidget   *menu_conv_add_contact;
	GtkWidget   *menu_conv_info;
	GtkWidget   *menu_conv_close;

	GtkWidget   *menu_room;
	GtkWidget   *menu_room_invite;
	GtkWidget   *menu_room_add;
	GtkWidget   *menu_room_show_contacts;

	GtkWidget   *menu_edit_insert_smiley;
	GtkWidget   *menu_edit_cut;
	GtkWidget   *menu_edit_copy;
	GtkWidget   *menu_edit_paste;

	GtkWidget   *menu_tabs_next;
	GtkWidget   *menu_tabs_prev;
	GtkWidget   *menu_tabs_left;
	GtkWidget   *menu_tabs_right;
	GtkWidget   *menu_tabs_detach;

	guint        save_geometry_id;
};

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
static void       chat_window_avatar_changed_cb         (GossipContact         *contact,
							 GParamSpec            *param,
							 GossipChatWindow      *window);
static void       chat_window_close_clicked_cb          (GtkWidget             *button,
							 GossipChat            *chat);
static GtkWidget *chat_window_create_label              (GossipChatWindow      *window,
							 GossipChat            *chat);
static void       chat_window_update_status             (GossipChatWindow      *window,
							 GossipChat            *chat);
static void       chat_window_update_title              (GossipChatWindow      *window,
							 GossipChat            *chat);
static void       chat_window_update_menu               (GossipChatWindow      *window);
static gboolean   chat_window_save_geometry_timeout_cb  (GossipChatWindow      *window);
static gboolean   chat_window_configure_event_cb        (GtkWidget             *widget,
							 GdkEventConfigure     *event,
							 GossipChatWindow      *window);
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
static void       chat_window_room_invite_activate_cb   (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_room_add_activate_cb      (GtkWidget             *menuitem,
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
							 GossipMessage         *message,
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
static void       chat_window_set_urgency_hint          (GossipChatWindow      *window,
							 gboolean               urgent);


static GList *chat_windows = NULL;

static const guint tab_accel_keys[] = {
	GDK_1, GDK_2, GDK_3, GDK_4, GDK_5,
	GDK_6, GDK_7, GDK_8, GDK_9, GDK_0
};

typedef enum {
	DND_DROP_TYPE_CONTACT_ID,
} DndDropType;

static const GtkTargetEntry drop_types[] = {
	{ "text/contact-id", 0, DND_DROP_TYPE_CONTACT_ID },
};

G_DEFINE_TYPE (GossipChatWindow, gossip_chat_window, G_TYPE_OBJECT);

static void
gossip_chat_window_class_init (GossipChatWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gossip_chat_window_finalize;

	g_type_class_add_private (object_class, sizeof (GossipChatWindowPriv));

	/* Set up a style for the close button with no focus padding. */
	gtk_rc_parse_string (
		"style \"gossip-close-button-style\"\n"
		"{\n"
		"  GtkWidget::focus-padding = 0\n"
		"  xthickness = 0\n"
		"  ythickness = 0\n"
		"}\n"
		"widget \"*.gossip-close-button\" style \"gossip-close-button-style\"");
}

static void
gossip_chat_window_init (GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GladeXML             *glade;
	GtkAccelGroup        *accel_group;
	GtkWidget            *image;
	GClosure             *closure;
	GtkWidget            *menu_conv;
	GtkWidget            *menu;
	gint                  i;
	GtkWidget            *chat_vbox;

	priv = GET_PRIV (window);

	priv->tooltips = g_object_ref (gtk_tooltips_new ());
	gtk_object_sink (GTK_OBJECT (priv->tooltips));

	glade = gossip_glade_get_file ("chat.glade",
				       "chat_window",
				       NULL,
				       "chat_window", &priv->dialog,
				       "chat_vbox", &chat_vbox,
				       "menu_conv", &menu_conv,
				       "menu_conv_clear", &priv->menu_conv_clear,
				       "menu_conv_log", &priv->menu_conv_log,
				       "menu_conv_separator", &priv->menu_conv_separator,
				       "menu_conv_add_contact", &priv->menu_conv_add_contact,
				       "menu_conv_info", &priv->menu_conv_info,
				       "menu_conv_close", &priv->menu_conv_close,
				       "menu_room", &priv->menu_room,
				       "menu_room_invite", &priv->menu_room_invite,
				       "menu_room_add", &priv->menu_room_add,
				       "menu_room_show_contacts", &priv->menu_room_show_contacts,
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
			      "chat_window", "configure-event", chat_window_configure_event_cb,
			      "menu_conv", "activate", chat_window_conv_activate_cb,
			      "menu_conv_clear", "activate", chat_window_clear_activate_cb,
			      "menu_conv_log", "activate", chat_window_log_activate_cb,
			      "menu_conv_add_contact", "activate", chat_window_add_contact_activate_cb,
			      "menu_conv_info", "activate", chat_window_info_activate_cb,
			      "menu_conv_close", "activate", chat_window_close_activate_cb,
			      "menu_room_invite", "activate", chat_window_room_invite_activate_cb,
			      "menu_room_add", "activate", chat_window_room_add_activate_cb,
			      "menu_edit", "activate", chat_window_edit_activate_cb,
			      "menu_edit_cut", "activate", chat_window_cut_activate_cb,
			      "menu_edit_copy", "activate", chat_window_copy_activate_cb,
			      "menu_edit_paste", "activate", chat_window_paste_activate_cb,
			      "menu_tabs_left", "activate", chat_window_tabs_left_activate_cb,
			      "menu_tabs_right", "activate", chat_window_tabs_right_activate_cb,
			      "menu_tabs_detach", "activate", chat_window_detach_activate_cb,
			      NULL);

	g_object_unref (glade);

	priv->notebook = gossip_notebook_new ();
	gtk_widget_show (priv->notebook);
	gtk_box_pack_start (GTK_BOX (chat_vbox), priv->notebook, TRUE, TRUE, 0);

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

	g_object_unref (accel_group);

	/* Set the contact information menu item image to the Gossip
	 * stock image
	 */
	image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (priv->menu_conv_info));
	gtk_image_set_from_stock (GTK_IMAGE (image),
				  GOSSIP_STOCK_CONTACT_INFORMATION,
				  GTK_ICON_SIZE_MENU);

	/* Set up smiley menu */
	menu = gossip_chat_view_get_smiley_menu (
		G_CALLBACK (chat_window_insert_smiley_activate_cb),
		window,
		priv->tooltips);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (priv->menu_edit_insert_smiley), menu);

	/* Set up signals we can't do with glade since we may need to
	 * block/unblock them at some later stage.
	 */
	g_signal_connect (gossip_app_get_session (),
			  "disconnected",
			  G_CALLBACK (chat_window_disconnected_cb),
			  window);

	g_signal_connect (priv->dialog,
			  "delete_event",
			  G_CALLBACK (chat_window_delete_event_cb),
			  window);

	g_signal_connect (priv->menu_room_show_contacts,
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
}

/* Returns the window to open a new tab in if there is only one window
 * visble, otherwise, returns NULL indicating that a new window should
 * be added.
 */
GossipChatWindow *
gossip_chat_window_get_default (void)
{
	GList    *l;
	gboolean  separate_windows = TRUE;

	gossip_conf_get_bool (gossip_conf_get (),
			      GOSSIP_PREFS_UI_SEPARATE_CHAT_WINDOWS,
			      &separate_windows);

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

	if (priv->save_geometry_id != 0) {
		g_source_remove (priv->save_geometry_id);
	}

	if (priv->urgency_timeout_id != 0) {
		g_source_remove (priv->urgency_timeout_id);
	}

	chat_windows = g_list_remove (chat_windows, window);
	gtk_widget_destroy (priv->dialog);

	g_object_unref (priv->tooltips);

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
	gint                  num = -1;
	gint                  i;

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

static void
chat_window_avatar_changed_cb (GossipContact    *contact,
			       GParamSpec       *param,
			       GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipContact        *current_contact;

	priv = GET_PRIV (window);

	current_contact = gossip_chat_get_contact (priv->current_chat);
	if (!gossip_contact_equal (current_contact, contact)) {
		return;
	}

	chat_window_update_title (window, priv->current_chat);
}

static void
chat_window_close_clicked_cb (GtkWidget  *button,
			      GossipChat *chat)
{
	GossipChatWindow *window;

	window = gossip_chat_get_window (chat);
	gossip_chat_window_remove_chat (window, chat);
}

static GtkWidget *
chat_window_create_label (GossipChatWindow *window,
			  GossipChat       *chat)
{
	GossipChatWindowPriv *priv;
	GtkWidget            *hbox;
	GtkWidget            *name_label;
	GtkWidget            *status_image;
	GtkWidget            *close_button;
	GtkWidget            *close_image;
	const gchar          *name;
	GtkWidget            *event_box;
	GtkWidget            *event_box_hbox;
	PangoAttrList        *attr_list;
	PangoAttribute       *attr;
	GtkRequisition        size;

	priv = GET_PRIV (window);

	/* The spacing between the button and the label. */
	hbox = gtk_hbox_new (FALSE, 0);

	event_box = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (event_box), FALSE);

	name = gossip_chat_get_name (chat);
	name_label = gtk_label_new (name);

	gtk_label_set_ellipsize (GTK_LABEL (name_label), PANGO_ELLIPSIZE_END);

	attr_list = pango_attr_list_new ();
	attr = pango_attr_scale_new (1/1.2);
	attr->start_index = 0;
	attr->end_index = -1;
	pango_attr_list_insert (attr_list, attr);
	gtk_label_set_attributes (GTK_LABEL (name_label), attr_list);
	pango_attr_list_unref (attr_list);

	gtk_misc_set_alignment (GTK_MISC (name_label), 0.0, 0.5);
	g_object_set_data (G_OBJECT (chat), "chat-window-tab-label", name_label);

	status_image = gtk_image_new ();

	/* Spacing between the icon and label. */
	event_box_hbox = gtk_hbox_new (FALSE, 2);

	gtk_box_pack_start (GTK_BOX (event_box_hbox), status_image, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (event_box_hbox), name_label, TRUE, TRUE, 0);

	g_object_set_data (G_OBJECT (chat), "chat-window-tab-image", status_image);
	g_object_set_data (G_OBJECT (chat), "chat-window-tab-tooltip-widget", event_box);

	chat_window_update_tooltip (window, chat);

	close_button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);

	/* We don't want focus/keynav for the button to avoid clutter, and
	 * Ctrl-W works anyway.
	 */
	GTK_WIDGET_UNSET_FLAGS (close_button, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS (close_button, GTK_CAN_DEFAULT);

	/* Set the name to make the special rc style match. */
	gtk_widget_set_name (close_button, "gossip-close-button");

	close_image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);

	gtk_widget_size_request (close_image, &size);
	gtk_widget_set_size_request (close_button, size.width, size.height);

	gtk_container_add (GTK_CONTAINER (close_button), close_image);
	gtk_container_set_border_width (GTK_CONTAINER (close_button), 0);

	gtk_container_add (GTK_CONTAINER (event_box), event_box_hbox);
	gtk_box_pack_start (GTK_BOX (hbox), event_box, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);

	g_signal_connect (close_button,
			  "clicked",
			  G_CALLBACK (chat_window_close_clicked_cb),
			  chat);

	/* Set up tooltip */
	chat_window_update_tooltip (window, chat);

	gtk_widget_show_all (hbox);

	return hbox;
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

	g_object_unref (pixbuf);

	chat_window_update_tooltip (window, chat);
}

static void
chat_window_update_title (GossipChatWindow *window,
			  GossipChat       *chat)
{
	GossipChatWindowPriv *priv;
	GossipContact        *contact;
	const gchar          *name;
	GdkPixbuf 	     *pixbuf = NULL;

	priv = GET_PRIV (window);

	/* Use the name from the supplied chat, which happens when we get a
	 * message from an tab other than the active one. This makes it easier
	 * to see who talked to you when you have multiple tabs.
	 */
	if (chat) {
		name = gossip_chat_get_name (chat);
	} else {
		name = gossip_chat_get_name (priv->current_chat);
	}

	gtk_window_set_title (GTK_WINDOW (priv->dialog), name);

	if (priv->new_msg) {
		pixbuf = gossip_pixbuf_from_stock (GOSSIP_STOCK_MESSAGE,
						   GTK_ICON_SIZE_MENU);
	} else {
		/* Update the avatar if we have one */
		contact = gossip_chat_get_contact (priv->current_chat);
		pixbuf = gossip_pixbuf_avatar_from_contact (contact);

		chat_window_set_urgency_hint (window, FALSE);
	}

	gtk_window_set_icon (GTK_WINDOW (priv->dialog), pixbuf);
	if (pixbuf) {
		g_object_unref (pixbuf);
	}
}

static void
chat_window_update_menu (GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipContact        *contact;
	GossipContact        *own_contact;
	gboolean              show_contacts;
	gboolean              first_page;
	gboolean              last_page;
	gint                  num_pages;
	gint                  page_num;

	priv = GET_PRIV (window);

	page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));
	num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook));
	first_page = (page_num == 0);
	last_page = (page_num == (num_pages - 1));

	contact = gossip_chat_get_contact (priv->current_chat);
	own_contact = gossip_chat_get_own_contact (priv->current_chat);

	if (!own_contact) {
		return;
	}

	gtk_widget_set_sensitive (priv->menu_tabs_next, !last_page);
	gtk_widget_set_sensitive (priv->menu_tabs_prev, !first_page);
	gtk_widget_set_sensitive (priv->menu_tabs_detach, num_pages > 1);
	gtk_widget_set_sensitive (priv->menu_tabs_left, !first_page);
	gtk_widget_set_sensitive (priv->menu_tabs_right, !last_page);
	gtk_widget_set_sensitive (priv->menu_conv_info, contact != NULL);

	if (gossip_chat_is_group_chat (priv->current_chat)) {
		GossipGroupChat       *group_chat;
		GossipChatroomManager *manager;
		GossipChatroomId       id;

		group_chat = GOSSIP_GROUP_CHAT (priv->current_chat);
		id = gossip_group_chat_get_chatroom_id (group_chat);

		/* Show / Hide widgets */
		gtk_widget_show (priv->menu_room);

		gtk_widget_hide (priv->menu_conv_separator);
		gtk_widget_hide (priv->menu_conv_add_contact);
		gtk_widget_hide (priv->menu_conv_info);

		/* Can we add this room to our favourites? */
		manager = gossip_app_get_chatroom_manager ();
		if (gossip_chatroom_manager_find (manager, id)) {
			gtk_widget_set_sensitive (priv->menu_room_add, FALSE);
		} else {
			gtk_widget_set_sensitive (priv->menu_room_add, TRUE);
		}

		/* We need to block the signal here because all we are
		 * really trying to do is check or uncheck the menu
		 * item. If we don't do this we get funny behaviour
		 * with 2 or more group chat windows where showing
		 * contacts doesn't do anything.
		 */
		show_contacts = gossip_chat_get_show_contacts (priv->current_chat);

		g_signal_handlers_block_by_func (priv->menu_room_show_contacts,
						 chat_window_show_contacts_toggled_cb,
						 window);

		g_object_set (priv->menu_room_show_contacts,
			      "active", show_contacts,
			      NULL);

		g_signal_handlers_unblock_by_func (priv->menu_room_show_contacts,
						   chat_window_show_contacts_toggled_cb,
						   window);
	} else {
		GossipContactType type;

		/* Show / Hide widgets */
		gtk_widget_hide (priv->menu_room);

		type = gossip_contact_get_type (contact);
		if (type != GOSSIP_CONTACT_TYPE_CONTACTLIST &&
		    type != GOSSIP_CONTACT_TYPE_USER) {
			gtk_widget_show (priv->menu_conv_add_contact);
		} else {
			gtk_widget_hide (priv->menu_conv_add_contact);
		}

		gtk_widget_show (priv->menu_conv_separator);
		gtk_widget_show (priv->menu_conv_info);
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
				smiley, -1);
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

	gossip_add_contact_dialog_show (NULL, contact);
}

static void
chat_window_log_activate_cb (GtkWidget        *menuitem,
			     GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	if (gossip_chat_is_group_chat (priv->current_chat)) {
		GossipGroupChat        *group_chat;
		GossipChatroomProvider *provider;
		GossipChatroom         *chatroom;
		GossipChatroomId        id;

		group_chat = GOSSIP_GROUP_CHAT (priv->current_chat);

		id = gossip_group_chat_get_chatroom_id (group_chat);
		provider = gossip_group_chat_get_chatroom_provider (group_chat);

		chatroom = gossip_chatroom_provider_find (provider, id);
		gossip_log_window_show (NULL, chatroom);
	} else {
		GossipContact *contact;

		contact = gossip_chat_get_contact (priv->current_chat);
		gossip_log_window_show (contact, NULL);
	}
}

static void
chat_window_info_activate_cb (GtkWidget        *menuitem,
			      GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipContact        *contact;

	priv = GET_PRIV (window);

	contact = gossip_chat_get_contact (priv->current_chat);

	gossip_contact_info_dialog_show (contact,
					 GTK_WINDOW (priv->dialog));
}

static gboolean
chat_window_save_geometry_timeout_cb (GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	gint                  x, y, w, h;

	priv = GET_PRIV (window);

	gtk_window_get_size (GTK_WINDOW (priv->dialog), &w, &h);
	gtk_window_get_position (GTK_WINDOW (priv->dialog), &x, &y);

	gossip_chat_save_geometry (priv->current_chat, x, y, w, h);

	priv->save_geometry_id = 0;

	return FALSE;
}

static gboolean
chat_window_configure_event_cb (GtkWidget         *widget,
				GdkEventConfigure *event,
				GossipChatWindow  *window)
{
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	/* Only save geometry information if there is ONE chat visible. */
	if (g_list_length (priv->chats) > 1) {
		return FALSE;
	}

	if (priv->save_geometry_id != 0) {
		g_source_remove (priv->save_geometry_id);
	}

	priv->save_geometry_id =
		g_timeout_add (500,
			       (GSourceFunc) chat_window_save_geometry_timeout_cb,
			       window);

	return FALSE;
}

static void
chat_window_conv_activate_cb (GtkWidget        *menuitem,
			      GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	gboolean              log_exists = FALSE;

	priv = GET_PRIV (window);

	if (gossip_chat_is_group_chat (priv->current_chat)) {
		GossipGroupChat        *group_chat;
		GossipChatroomProvider *provider;
		GossipChatroom         *chatroom;
		GossipChatroomId        id;

		group_chat = GOSSIP_GROUP_CHAT (priv->current_chat);

		id = gossip_group_chat_get_chatroom_id (group_chat);
		provider = gossip_group_chat_get_chatroom_provider (group_chat);

		chatroom = gossip_chatroom_provider_find (provider, id);
		if (chatroom) {
			log_exists = gossip_log_exists_for_chatroom (chatroom);
		}
	} else {
		GossipContact *contact;

		contact = gossip_chat_get_contact (priv->current_chat);
		if (contact) {
			log_exists = gossip_log_exists_for_contact (contact);
		}
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

	show = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (priv->menu_room_show_contacts));
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
chat_window_room_invite_activate_cb (GtkWidget        *menuitem,
				     GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipContact        *own_contact;
	GossipChatroomId      id = 0;

	priv = GET_PRIV (window);

	own_contact = gossip_chat_get_own_contact (priv->current_chat);

	if (gossip_chat_is_group_chat (priv->current_chat)) {
		GossipGroupChat *group_chat;

		group_chat = GOSSIP_GROUP_CHAT (priv->current_chat);
		id = gossip_group_chat_get_chatroom_id (group_chat);
	}

	gossip_chat_invite_dialog_show (own_contact, id);
}

static void
chat_window_room_add_activate_cb (GtkWidget        *menuitem,
				  GossipChatWindow *window)
{
	GossipChatWindowPriv   *priv;
	GossipGroupChat        *group_chat;
	GossipChatroomManager  *manager;
	GossipChatroomProvider *provider;
	GossipChatroom         *chatroom;
	GossipChatroomId        id;

	priv = GET_PRIV (window);

	g_return_if_fail (priv->current_chat != NULL);

	if (!gossip_chat_is_group_chat (priv->current_chat)) {
		return;
	}

	group_chat = GOSSIP_GROUP_CHAT (priv->current_chat);
	id = gossip_group_chat_get_chatroom_id (group_chat);
	provider = gossip_group_chat_get_chatroom_provider (group_chat);

	chatroom = gossip_chatroom_provider_find (provider, id);

	manager = gossip_app_get_chatroom_manager ();
	gossip_chatroom_manager_add (manager, chatroom);
	gossip_chatroom_manager_store (manager);

	chat_window_update_menu (window);
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

	chat_window_update_title (window, NULL);
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

	chat_window_update_title (window, NULL);
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

	gossip_debug (DEBUG_DOMAIN, "Delete event received");

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
			    GossipMessage    *message,
			    GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	gossip_request_user_attention ();

	if (!gossip_chat_window_has_focus (window)) {
		GossipContact *own_contact;

		gossip_debug (DEBUG_DOMAIN, "New message, no focus");

		priv->new_msg = TRUE;
		chat_window_update_title (window, chat);

		if (gossip_chat_is_group_chat (chat)) {
			own_contact = gossip_chat_get_own_contact (chat);

			if (gossip_chat_should_highlight_nick (message, own_contact)) {
				gossip_debug (DEBUG_DOMAIN, "Highlight this nick");
				chat_window_set_urgency_hint (window, TRUE);
			}
		} else {
			chat_window_set_urgency_hint (window, TRUE);
		}
	} else {
		gossip_debug (DEBUG_DOMAIN, "New message, we have focus");
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

	/* FIXME: This works for now, but should operate on a PER
	 * protocol basis not ALL connections since some tabs will
	 * belong to contacts on accounts which might still be online.
	 */
	for (l = priv->chats; l != NULL; l = g_list_next (l)) {
		GossipChat *chat = GOSSIP_CHAT (l->data);
		chat_window_update_status (window, chat);
	}

	chat_window_update_menu (window);
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

	gossip_debug (DEBUG_DOMAIN, "Tab switched");

	priv = GET_PRIV (window);

	child = gtk_notebook_get_nth_page (notebook, page_num);
	chat = g_object_get_data (G_OBJECT (child), "chat");

	if (priv->current_chat == chat) {
		return;
	}

	priv->current_chat = chat;
	priv->chats_new_msg = g_list_remove (priv->chats_new_msg, chat);

	chat_window_update_title (window, NULL);
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
	gboolean              expand = TRUE;

	gossip_debug (DEBUG_DOMAIN, "Tab added");

	priv = GET_PRIV (window);

	chat = g_object_get_data (G_OBJECT (child), "chat");

	gossip_chat_set_window (chat, window);

	priv->chats = g_list_append (priv->chats, chat);

	label = chat_window_create_label (window, chat);
	gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), child, label);

	gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (notebook), child,
					    expand, TRUE, GTK_PACK_START);

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
	chat_window_update_title (window, NULL);
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

	gossip_debug (DEBUG_DOMAIN, "Tab removed");

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

	gossip_debug (DEBUG_DOMAIN, "Tab detached");

	chat = g_object_get_data (G_OBJECT (child), "chat");
	new_window = gossip_chat_window_new ();

	priv = GET_PRIV (new_window);

	gdk_display_get_pointer (gdk_display_get_default (),
				 NULL,
				 &x,
				 &y,
				 NULL);

	gossip_chat_window_remove_chat (window, chat);
	gossip_chat_window_add_chat (new_window, chat);

	gtk_window_move (GTK_WINDOW (priv->dialog), x, y);

	gtk_widget_show (priv->dialog);
}

static void
chat_window_tabs_reordered_cb (GossipNotebook   *notebook,
			       GossipChatWindow *window)
{
	gossip_debug (DEBUG_DOMAIN, "Tabs reordered");

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

	chat_window_update_title (window, NULL);

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
	gossip_debug (DEBUG_DOMAIN, "Received drag & drop contact "
		      "from roster with id:'%s'",
		      id);

	contact = gossip_session_find_contact (gossip_app_get_session (), id);

	if (!contact) {
		gossip_debug (DEBUG_DOMAIN, "No contact found associated "
			      "with drag & drop");
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

static gboolean
chat_window_urgency_timeout_func (GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	gossip_debug (DEBUG_DOMAIN, "Turning off urgency hint");
	gtk_window_set_urgency_hint (GTK_WINDOW (priv->dialog), FALSE);

	priv->urgency_timeout_id = 0;

	return FALSE;
}

static void
chat_window_set_urgency_hint (GossipChatWindow *window,
			      gboolean          urgent)
{
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	if (!urgent) {
		/* Remove any existing hint and timeout. */
		if (priv->urgency_timeout_id) {
			gossip_debug (DEBUG_DOMAIN, "Turning off urgency hint");
			gtk_window_set_urgency_hint (GTK_WINDOW (priv->dialog), FALSE);
			g_source_remove (priv->urgency_timeout_id);
			priv->urgency_timeout_id = 0;
		}
		return;
	}

	/* Add a new hint and renew any exising timeout or add a new one. */
	if (priv->urgency_timeout_id) {
		g_source_remove (priv->urgency_timeout_id);
	} else {
		gossip_debug (DEBUG_DOMAIN, "Turning on urgency hint");
		gtk_window_set_urgency_hint (GTK_WINDOW (priv->dialog), TRUE);
	}

	priv->urgency_timeout_id = g_timeout_add (
		URGENCY_TIMEOUT,
		(GSourceFunc) chat_window_urgency_timeout_func,
		window);
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
	GossipContact        *contact;
	GtkWidget            *label;

	gossip_debug (DEBUG_DOMAIN, "Adding chat");

	priv = GET_PRIV (window);

	label = chat_window_create_label (window, chat);

	if (g_list_length (priv->chats) == 0) {
		gint x, y, w, h;

		gossip_chat_load_geometry (chat, &x, &y, &w, &h);

		if (x >= 1 && y >= 1) {
			/* Let the window manager position it if we
			 * don't have good x, y coordinates.
			 */
			gtk_window_move (GTK_WINDOW (priv->dialog), x, y);
		}

		if (w >= 1 && h >= 1) {
			/* Use the defaults from the glade file if we
			 * don't have good w, h geometry.
			 */
			gtk_window_resize (GTK_WINDOW (priv->dialog), w, h);
		}
	}

	gossip_notebook_insert_page (GOSSIP_NOTEBOOK (priv->notebook),
				     gossip_chat_get_widget (chat),
				     label,
				     GOSSIP_NOTEBOOK_INSERT_LAST,
				     TRUE);

	contact = gossip_chat_get_contact (chat);
	if (contact) {
		g_signal_connect (contact, 
				  "notify::avatar",
				  G_CALLBACK (chat_window_avatar_changed_cb),
				  window);
	}

	/* FIXME: somewhat ugly */
	g_object_ref (chat);
}

void
gossip_chat_window_remove_chat (GossipChatWindow *window,
				GossipChat	 *chat)
{
	GossipChatWindowPriv *priv;
	GossipContact        *contact;

	gossip_debug (DEBUG_DOMAIN, "Removing chat");

	priv = GET_PRIV (window);

	gossip_notebook_remove_page (GOSSIP_NOTEBOOK (priv->notebook),
				     gossip_chat_get_widget (chat));

	contact = gossip_chat_get_contact (chat);
	if (contact) {
		g_signal_handlers_disconnect_by_func 
			(contact, 
			 chat_window_avatar_changed_cb,
			 window);
	}

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
