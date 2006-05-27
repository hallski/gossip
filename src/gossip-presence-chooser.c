/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Imendio AB
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
#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gossip-marshal.h"
#include "gossip-app.h"
#include "gossip-ui-utils.h"
#include "gossip-stock.h"
#include "gossip-presence-chooser.h"

#include "gossip-status-presets.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_PRESENCE_CHOOSER, GossipPresenceChooserPriv))

typedef struct {
	GtkWidget           *hbox;
	GtkWidget           *image;
	GtkWidget           *label;
 
	GossipPresenceState  last_state;

	guint                flash_interval;

	GossipPresenceState  flash_state_1;
	GossipPresenceState  flash_state_2;

	guint                flash_timeout_id;
} GossipPresenceChooserPriv;

static void     presence_chooser_finalize               (GObject               *object);
static void     presence_chooser_set_state              (GossipPresenceChooser *chooser,
							 GossipPresenceState    state,
							 const gchar           *status,
							 gboolean               save);
static void     presence_chooser_dialog_response_cb     (GtkWidget             *dialog,
							 gint                   response,
							 GossipPresenceChooser *chooser);
static void     presence_chooser_show_dialog            (GossipPresenceChooser *chooser,
							 GossipPresenceState    state);
static void     presence_chooser_custom_activate_cb     (GtkWidget             *item,
							 GossipPresenceChooser *chooser);
static void     presence_chooser_clear_activate_cb      (GtkWidget             *item,
							 GossipPresenceChooser *chooser);
static void     presence_chooser_menu_add_item          (GossipPresenceChooser *chooser,
							 GtkWidget             *menu,
							 const gchar           *str,
							 GossipPresenceState    state,
							 gboolean               custom);
static void     presence_chooser_menu_align_func        (GtkMenu               *menu,
							 gint                  *x,
							 gint                  *y,
							 gboolean              *push_in,
							 GossipPresenceChooser *chooser);
static void     presence_chooser_menu_selection_done_cb (GtkMenuShell          *menushell,
							 GossipPresenceChooser *chooser);
static void     presence_chooser_menu_detach            (GtkWidget             *attach_widget,
							 GtkMenu               *menu);
static void     presence_chooser_menu_popup             (GossipPresenceChooser *chooser);
static void     presence_chooser_button_clicked_cb      (GtkWidget             *chooser,
							 gpointer               user_data);
static gboolean presence_chooser_flash_timeout_cb       (GossipPresenceChooser *chooser);

G_DEFINE_TYPE (GossipPresenceChooser, gossip_presence_chooser, GTK_TYPE_TOGGLE_BUTTON);

enum {
        CHANGED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
gossip_presence_chooser_class_init (GossipPresenceChooserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = presence_chooser_finalize;

	signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      gossip_marshal_VOID__INT_STRING,
			      G_TYPE_NONE, 2,
			      G_TYPE_INT, G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof (GossipPresenceChooserPriv));
}

static void
gossip_presence_chooser_init (GossipPresenceChooser *chooser)
{
	GossipPresenceChooserPriv *priv;
	GtkWidget                 *arrow;
	GtkWidget                 *alignment;

	priv = GET_PRIV (chooser);

	/* Default to 1/2 a second flash interval */
	priv->flash_interval = 500;

	gtk_button_set_relief (GTK_BUTTON (chooser), GTK_RELIEF_NONE);

	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_widget_show (alignment);
 	gtk_container_add (GTK_CONTAINER (chooser), alignment); 
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 2, 2);

 	priv->hbox = gtk_hbox_new (FALSE, 2); 
	gtk_widget_show (priv->hbox);
 	gtk_container_add (GTK_CONTAINER (alignment), priv->hbox); 

	priv->image = gtk_image_new ();
	gtk_widget_show (priv->image);
	gtk_box_pack_start (GTK_BOX (priv->hbox), priv->image, FALSE, TRUE, 0);

	priv->label = gtk_label_new ("Test");
	gtk_widget_show (priv->label);
	gtk_box_pack_start (GTK_BOX (priv->hbox), priv->label, FALSE, TRUE, 0);
 	gtk_label_set_ellipsize (GTK_LABEL (priv->label), PANGO_ELLIPSIZE_NONE); 
 	gtk_misc_set_alignment (GTK_MISC (priv->label), 0, 0.5); 

	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_widget_show (alignment);
	gtk_box_pack_start (GTK_BOX (priv->hbox), alignment, FALSE, FALSE, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 4, 0);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show (arrow);
	gtk_container_add (GTK_CONTAINER (alignment), arrow);

	/* We use this instead of "clicked" to get the button to disappear when
	 * clicking on the label inside the button.
	 */
        g_signal_connect (chooser, "clicked",
                          G_CALLBACK (presence_chooser_button_clicked_cb),
                          NULL);
}

static void
presence_chooser_finalize (GObject *object)
{
	GossipPresenceChooserPriv *priv;

	priv = GET_PRIV (object);

	if (priv->flash_timeout_id) {
		g_source_remove (priv->flash_timeout_id);
		priv->flash_timeout_id = 0;
	}

	G_OBJECT_CLASS (gossip_presence_chooser_parent_class)->finalize (object);
}

static void
presence_chooser_set_state (GossipPresenceChooser *chooser,
			    GossipPresenceState    state,
			    const gchar           *status,
			    gboolean               save)
{
        GossipPresenceChooserPriv *priv;
	const gchar               *default_status;

        priv = GET_PRIV (chooser);

	default_status = gossip_presence_state_get_default_status (state);

	if (strlen (status) < 1) {
		status = default_status;
	} else {
		/* Only store the value if it differs from the default ones. */
		if (save && strcmp (status, default_status) != 0) {
			gossip_status_presets_set_last (status, state);
		}
	}

	priv->last_state = state;

	g_signal_emit (chooser, signals[CHANGED], 0, state, status);
}

static void
presence_chooser_dialog_response_cb (GtkWidget             *dialog,
				     gint                   response,
				     GossipPresenceChooser *chooser)
{
	GtkWidget           *entry;
	GtkWidget           *checkbutton;
	GossipPresenceState  state;
	const gchar         *status;
	gboolean             save;

	if (response == GTK_RESPONSE_OK) {
		entry = g_object_get_data (G_OBJECT (dialog), "entry");
		status = gtk_entry_get_text (GTK_ENTRY (entry));

		state = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog), "state"));

		checkbutton = g_object_get_data (G_OBJECT (dialog), "checkbutton");
		save = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton));

		presence_chooser_set_state (chooser, state, status, save);
	}

	gtk_widget_destroy (dialog);
}

static void
presence_chooser_show_dialog (GossipPresenceChooser *chooser,
			      GossipPresenceState    state)
{
        GossipPresenceChooserPriv *priv;
	static GtkWidget          *dialog;
	GladeXML                  *glade;
	GtkWidget                 *image;
	GtkWidget                 *entry;
	GtkWidget                 *checkbutton;
	GtkTreeIter                iter;
	const gchar               *default_status;
	GtkEntryCompletion        *completion;
	GdkPixbuf                 *pixbuf;
	GtkListStore              *store;
	GList                     *presets, *l;
	GtkWidget                 *parent;
	gboolean                   visible;

        priv = GET_PRIV (chooser);

	if (dialog) {
		gtk_widget_destroy (dialog);
		dialog = NULL;
	}

	glade = gossip_glade_get_file (GLADEDIR "/main.glade",
				       "status_message_dialog",
				       NULL,
				       "status_message_dialog", &dialog,
				       "status_entry", &entry,
				       "status_image", &image,
				       "add_checkbutton", &checkbutton,
				       NULL);

	g_signal_connect (dialog,
			  "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &dialog);

	pixbuf = gossip_pixbuf_for_presence_state (state);
	gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
	g_object_unref (pixbuf);

	store = gtk_list_store_new (1, G_TYPE_STRING);

	presets = gossip_status_presets_get (state, -1);
	for (l = presets; l; l = l->next) {
		const gchar *status;

		status = l->data;
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, status, -1);
	}

	g_list_free (presets);

	default_status = gossip_presence_state_get_default_status (state);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, default_status, -1);
	gtk_entry_set_text (GTK_ENTRY (entry), default_status);

	completion = gtk_entry_completion_new ();
	gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (store));
	gtk_entry_completion_set_text_column (completion, 0);
	gtk_entry_set_completion (GTK_ENTRY (entry), completion);
	g_object_unref (completion);

	g_object_unref (store);

	/* We make the dialog transient to the app window if it's visible. */
	parent = gossip_app_get_window ();

	g_object_get (parent,
		      "visible", &visible,
		      NULL);

	visible = visible && !(gdk_window_get_state (parent->window) &
			       GDK_WINDOW_STATE_ICONIFIED);

	if (visible) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog),
					      GTK_WINDOW (parent));
	}

	g_object_set_data (G_OBJECT (dialog), "entry", entry);
	g_object_set_data (G_OBJECT (dialog), "checkbutton", checkbutton);
	g_object_set_data (G_OBJECT (dialog), "state", GINT_TO_POINTER (state));

	g_signal_connect (dialog,
			  "response",
			  G_CALLBACK (presence_chooser_dialog_response_cb),
			  chooser);

	gtk_widget_show_all (dialog);
}

static void
presence_chooser_custom_activate_cb (GtkWidget             *item,
				     GossipPresenceChooser *chooser)
{
	GossipPresenceState state;

	state = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "state"));

	presence_chooser_show_dialog (chooser, state);
}

static void
presence_chooser_noncustom_activate_cb (GtkWidget             *item,
					GossipPresenceChooser *chooser)
{
	GossipPresenceState  state;
	const gchar         *status;

	status = g_object_get_data (G_OBJECT (item), "status");
	state = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "state"));

	g_signal_emit (chooser, signals[CHANGED], 0, state, status);
}

static void
presence_chooser_clear_activate_cb (GtkWidget             *item,
				    GossipPresenceChooser *chooser)
{
	gossip_status_presets_reset ();
}

static void
presence_chooser_menu_add_item (GossipPresenceChooser *chooser,
				GtkWidget             *menu,
				const gchar           *str,
				GossipPresenceState    state,
				gboolean               custom)
{
	GtkWidget   *item;
	GtkWidget   *image;
	const gchar *stock;

	item = gtk_image_menu_item_new_with_label (str);

	switch (state) {
	case GOSSIP_PRESENCE_STATE_AVAILABLE:
		stock = GOSSIP_STOCK_AVAILABLE;
		break;

	case GOSSIP_PRESENCE_STATE_BUSY:
		stock = GOSSIP_STOCK_BUSY;
 		break;

	case GOSSIP_PRESENCE_STATE_AWAY:
		stock = GOSSIP_STOCK_AWAY;
		break;

	default:
		g_assert_not_reached ();
		stock = NULL;
		break;
	}

	if (custom) {
		g_signal_connect (
			item,
			"activate",
			G_CALLBACK (presence_chooser_custom_activate_cb),
			chooser);
	} else {
		g_signal_connect (
			item,
			"activate",
			G_CALLBACK (presence_chooser_noncustom_activate_cb),
			chooser);
	}

	image = gtk_image_new_from_stock (stock, GTK_ICON_SIZE_MENU);
	gtk_widget_show (image);

	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	gtk_widget_show (item);

	g_object_set_data_full (G_OBJECT (item),
				"status", g_strdup (str),
				(GDestroyNotify) g_free);

	g_object_set_data (G_OBJECT (item), "state", GINT_TO_POINTER (state));

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static void
presence_chooser_menu_align_func (GtkMenu               *menu,
				  gint                  *x,
				  gint                  *y,
				  gboolean              *push_in,
				  GossipPresenceChooser *chooser)
{
	GtkWidget      *widget;
	GtkRequisition  req;
	GdkScreen      *screen;
	gint            screen_height;

	widget = GTK_WIDGET (chooser);

	gtk_widget_size_request (GTK_WIDGET (menu), &req);

	gdk_window_get_origin (widget->window, x, y); 

	*x += widget->allocation.x + 1;
	*y += widget->allocation.y;

	screen = gtk_widget_get_screen (GTK_WIDGET (menu));
	screen_height = gdk_screen_get_height (screen);	

	if (req.height > screen_height) {
		/* Too big for screen height anyway. */
		*y = 0;
		return;
	}

	if ((*y + req.height + widget->allocation.height) > screen_height) {
		/* Can't put it below the button. */
		*y -= req.height;
		*y += 1;
	} else {
		/* Put menu below button. */
		*y += widget->allocation.height;
		*y -= 1;
	}

	*push_in = FALSE;
}

static void
presence_chooser_menu_selection_done_cb (GtkMenuShell          *menushell,
					 GossipPresenceChooser *chooser)
{
	gtk_widget_destroy (GTK_WIDGET (menushell));

	g_signal_handlers_block_by_func (chooser,
					 presence_chooser_button_clicked_cb,
					 NULL);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chooser), FALSE);

	g_signal_handlers_unblock_by_func (chooser,
					   presence_chooser_button_clicked_cb,
					   NULL);
}

static void   
presence_chooser_menu_detach (GtkWidget *attach_widget,
			      GtkMenu   *menu)
{
	/* We don't need to do anything, but attaching the menu means
	 * we don't own the ref count and it is cleaned up properly. 
	 */
}

static void
presence_chooser_menu_popup (GossipPresenceChooser *chooser)
{
	GossipPresenceChooserPriv *priv;
	GtkWidget                 *menu;

	priv = GET_PRIV (chooser);

	menu = gossip_presence_chooser_create_menu (chooser);
	
	g_signal_connect_after (menu, "selection-done", 
				G_CALLBACK (presence_chooser_menu_selection_done_cb),
				chooser);

	gtk_menu_attach_to_widget (GTK_MENU (menu), 
				   GTK_WIDGET (chooser), 
				   presence_chooser_menu_detach);

	gtk_menu_popup (GTK_MENU (menu),
			NULL, NULL,
			(GtkMenuPositionFunc) presence_chooser_menu_align_func,
			chooser,
			1,
			gtk_get_current_event_time ());
}

static void
presence_chooser_button_clicked_cb (GtkWidget *chooser,
				    gpointer   user_data)
{
	presence_chooser_menu_popup (GOSSIP_PRESENCE_CHOOSER (chooser));
}

GtkWidget *
gossip_presence_chooser_new (void)
{
	GtkWidget *chooser;

	chooser = g_object_new (GOSSIP_TYPE_PRESENCE_CHOOSER, NULL);

	return chooser;
}

GtkWidget *
gossip_presence_chooser_create_menu (GossipPresenceChooser *chooser)
{
	GtkWidget *menu;
	GtkWidget *item;
	GList     *list, *l;

	menu = gtk_menu_new ();

	presence_chooser_menu_add_item (chooser,
					menu,
					_("Available"),
					GOSSIP_PRESENCE_STATE_AVAILABLE,
					FALSE);

	list = gossip_status_presets_get (GOSSIP_PRESENCE_STATE_AVAILABLE, 5);
	for (l = list; l; l = l->next) {
		presence_chooser_menu_add_item (chooser,
						menu,
						l->data,
						GOSSIP_PRESENCE_STATE_AVAILABLE,
						FALSE);
	}

	g_list_free (list);

	presence_chooser_menu_add_item (chooser,
					menu,
					_("Custom message..."),
					GOSSIP_PRESENCE_STATE_AVAILABLE,
					TRUE);

	/* Separator. */
	item = gtk_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	presence_chooser_menu_add_item (chooser,
					menu,
					_("Busy"),
					GOSSIP_PRESENCE_STATE_BUSY,
					FALSE);

	list = gossip_status_presets_get (GOSSIP_PRESENCE_STATE_BUSY, 5);
	for (l = list; l; l = l->next) {
		presence_chooser_menu_add_item (chooser,
						menu,
						l->data,
						GOSSIP_PRESENCE_STATE_BUSY,
						FALSE);
	}

	g_list_free (list);

	presence_chooser_menu_add_item (chooser,
					menu,
					_("Custom message..."),
					GOSSIP_PRESENCE_STATE_BUSY,
					TRUE);

	/* Separator. */
	item = gtk_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	presence_chooser_menu_add_item (chooser,
					menu,
					_("Away"),
					GOSSIP_PRESENCE_STATE_AWAY,
					FALSE);

	list = gossip_status_presets_get (GOSSIP_PRESENCE_STATE_AWAY, 5);
	for (l = list; l; l = l->next) {
		presence_chooser_menu_add_item (chooser,
						menu,
						l->data,
						GOSSIP_PRESENCE_STATE_AWAY,
						FALSE);
	}

	g_list_free (list);

	presence_chooser_menu_add_item (chooser,
					menu,
					_("Custom message..."),
					GOSSIP_PRESENCE_STATE_AWAY,
					TRUE);

	/* Separator. */
	item = gtk_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	item = gtk_menu_item_new_with_label (_("Clear List"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	g_signal_connect (item,
			  "activate",
			  G_CALLBACK (presence_chooser_clear_activate_cb),
			  NULL);

	return menu;
}

void
gossip_presence_chooser_set_state (GossipPresenceChooser *chooser,
				   GossipPresenceState    state)
{
	GossipPresenceChooserPriv *priv;

	g_return_if_fail (GOSSIP_IS_PRESENCE_CHOOSER (chooser));

	priv = GET_PRIV (chooser);

	gossip_presence_chooser_flash_stop (chooser, state); 
}

void
gossip_presence_chooser_set_status (GossipPresenceChooser *chooser,
				    const gchar           *status)
{
	GossipPresenceChooserPriv *priv;

	g_return_if_fail (GOSSIP_IS_PRESENCE_CHOOSER (chooser));

	priv = GET_PRIV (chooser);

	gtk_label_set_text (GTK_LABEL (priv->label), status);
}

void       
gossip_presence_chooser_set_flash_interval (GossipPresenceChooser *chooser,
					    guint                  ms)
{
	GossipPresenceChooserPriv *priv;
	
	g_return_if_fail (GOSSIP_IS_PRESENCE_CHOOSER (chooser));
	g_return_if_fail (ms > 1 && ms < 30000);
	
	priv = GET_PRIV (chooser);

	priv->flash_interval = ms;
}

static gboolean
presence_chooser_flash_timeout_cb (GossipPresenceChooser *chooser)
{
	GossipPresenceChooserPriv *priv;
	GossipPresenceState        state;
	GdkPixbuf                 *pixbuf;
	static gboolean            on = FALSE;

	priv = GET_PRIV (chooser);
	
	if (on) {
		state = priv->flash_state_1;
	} else {
		state = priv->flash_state_2;
	}

	pixbuf = gossip_pixbuf_for_presence_state (state);
	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image), pixbuf);
	g_object_unref (pixbuf);

	on = !on;

	return TRUE;
}

void 
gossip_presence_chooser_flash_start (GossipPresenceChooser *chooser,
				     GossipPresenceState    state_1,
				     GossipPresenceState    state_2)
{
	GossipPresenceChooserPriv *priv;

	g_return_if_fail (GOSSIP_IS_PRESENCE_CHOOSER (chooser));

	priv = GET_PRIV (chooser);

	if (priv->flash_timeout_id != 0) {
		return;
	}

	priv->flash_state_1 = state_1;
	priv->flash_state_2 = state_2;

	priv->flash_timeout_id = g_timeout_add (priv->flash_interval,
						(GSourceFunc) presence_chooser_flash_timeout_cb,
						chooser);
}

void 
gossip_presence_chooser_flash_stop (GossipPresenceChooser *chooser,
				    GossipPresenceState    state)
{
	GossipPresenceChooserPriv *priv;
	GdkPixbuf                 *pixbuf;

	g_return_if_fail (GOSSIP_IS_PRESENCE_CHOOSER (chooser));

	priv = GET_PRIV (chooser);

	if (priv->flash_timeout_id) {
		g_source_remove (priv->flash_timeout_id);
		priv->flash_timeout_id = 0;
	}

	pixbuf = gossip_pixbuf_for_presence_state (state);
	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image), pixbuf);
	g_object_unref (pixbuf);

	priv->last_state = state;
}
				     
gboolean 
gossip_presence_chooser_is_flashing (GossipPresenceChooser *chooser)
{
	GossipPresenceChooserPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_PRESENCE_CHOOSER (chooser), FALSE);

	priv = GET_PRIV (chooser);

	if (priv->flash_timeout_id) {
		return TRUE;
	}

	return FALSE;
}
