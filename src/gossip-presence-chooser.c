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
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gossip-marshal.h"
#include "gossip-app.h"
#include "gossip-ui-utils.h"
#include "gossip-stock.h"
#include "gossip-presence-chooser.h"

#include "gossip-status-presets.h"


#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
	GOSSIP_TYPE_PRESENCE_CHOOSER, GossipPresenceChooserPriv))

typedef struct {
	GtkWidget  *button;
	GtkWidget  *label;
} GossipPresenceChooserPriv;


enum {
        CHANGED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];


static void presence_chooser_finalize                  (GObject               *object);
static void presence_chooser_show_dialog               (GossipPresenceChooser *chooser,
							GossipPresenceState    state);
static gboolean presence_chooser_button_press_event_cb (GtkButton             *button,
							GdkEventButton        *event,
							GossipPresenceChooser *chooser);


G_DEFINE_TYPE (GossipPresenceChooser, gossip_presence_chooser, GTK_TYPE_HBOX);

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
	GtkWidget                 *hbox;
	GtkWidget                 *arrow;

	priv = GET_PRIV (chooser);

	gtk_box_set_spacing (GTK_BOX (chooser), 4);

	priv->button = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (chooser), priv->button, TRUE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, 0);

	priv->label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->label), 0, 0.5);
	gtk_label_set_ellipsize (GTK_LABEL (priv->label), PANGO_ELLIPSIZE_END);
	gtk_box_pack_start (GTK_BOX (hbox), priv->label, TRUE, TRUE, 0);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_box_pack_start (GTK_BOX (hbox), arrow, FALSE, FALSE, 0);

	gtk_container_add (GTK_CONTAINER (priv->button), hbox);
	
	gtk_widget_show_all (priv->button);

	/* We use this instead of "clicked" to get the button to disappear when
	 * clicking on the label inside the button.
	 */
	g_signal_connect (priv->button,
			  "button-press-event",
			  G_CALLBACK (presence_chooser_button_press_event_cb),
			  chooser);
}

static void
presence_chooser_finalize (GObject *object)
{
	GossipPresenceChooserPriv *priv;

	priv = GET_PRIV (object);

	G_OBJECT_CLASS (gossip_presence_chooser_parent_class)->finalize (object);
}

static void
presence_chooser_dialog_response_cb (GtkWidget             *dialog,
				     gint                   response,
				     GossipPresenceChooser *chooser)
{
	GtkWidget           *entry;
	GossipPresenceState  state;
	const gchar         *status;
	const gchar         *default_status;
	
	if (response == GTK_RESPONSE_OK) {
		entry = g_object_get_data (G_OBJECT (dialog), "entry");
		state = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog), "state"));

		default_status = gossip_presence_state_get_default_status (state);
		
		status = gtk_entry_get_text (GTK_ENTRY (entry));
		if (strcmp (status, "") == 0) {
			status = default_status;
		} else {
			/* Only store the value if it differs from the default
			 * ones.
			 */
			if (strcmp (status, default_status) != 0) {
				gossip_status_presets_set_last (status, state);
			}
		}
		
		g_signal_emit (chooser, signals[CHANGED], 0,
			       state, status);
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
				       NULL);
	
	g_signal_connect (dialog,
			  "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &dialog);
		
	pixbuf = gossip_ui_utils_get_pixbuf_for_presence_state (state);
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
	const gchar         *status;
	GossipPresenceState  state;

	status = g_object_get_data (G_OBJECT (item), "status");
	state = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "state"));

	g_signal_emit (chooser, signals[CHANGED], 0,
		       state, status);
}

static void
presence_chooser_clear_activate_cb (GtkWidget             *item,
				    GossipPresenceChooser *chooser)
{
	gossip_status_presets_reset ();
}

static void
presence_chooser_add_item (GossipPresenceChooser *chooser,
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
presence_chooser_align_menu_func (GtkMenu  *menu,
				  gint     *x,
				  gint     *y,
				  gboolean *push_in,
				  gpointer  user_data)
{
	GossipPresenceChooser     *chooser;
	GossipPresenceChooserPriv *priv;
	GtkWidget                 *button;
	GtkRequisition             req;
	GdkScreen                 *screen;
	gint                       width, height;
	gint                       screen_height;

	chooser = user_data;
	priv = GET_PRIV (chooser);
	
	button = priv->button;
	
	gtk_widget_size_request (GTK_WIDGET (menu), &req);
  
	gdk_window_get_origin (GTK_BUTTON (button)->event_window, x, y);
	gdk_drawable_get_size (GTK_BUTTON (button)->event_window, &width, &height);

	*x -= req.width - width;
	*y += height;

	screen = gtk_widget_get_screen (GTK_WIDGET (menu));
	
	/* Clamp to screen size. */
	screen_height = gdk_screen_get_height (screen) - *y;	
	if (req.height > screen_height) {
		/* It doesn't fit, so we see if we have the minimum space
		 * needed.
		 */
		if (req.height > screen_height && *y - height > screen_height) {
			/* Put the menu above the button instead. */
			screen_height = *y - height;
			*y -= (req.height + height);
			if (*y < 0) {
				*y = 0;
			}
		}
	}
}

static void
presence_chooser_show_popup (GossipPresenceChooser *chooser)
{
	GossipPresenceChooserPriv *priv;
	GtkWidget                 *menu;

	priv = GET_PRIV (chooser);
	
	menu = gossip_presence_chooser_create_menu (chooser);

	gtk_widget_set_size_request (menu,
				     priv->button->allocation.width,
				     -1);
	
	gtk_menu_popup (GTK_MENU (menu),
			NULL, NULL,
			presence_chooser_align_menu_func,
			chooser,
			1,
			gtk_get_current_event_time ());
}

static gboolean
presence_chooser_button_press_event_cb (GtkButton             *button,
					GdkEventButton        *event,
					GossipPresenceChooser *chooser)
{
	if (event->type == GDK_2BUTTON_PRESS ||
	    event->type == GDK_3BUTTON_PRESS) {
		return FALSE;
	}
	
	if (event->button != 1) {
		return FALSE;
	}
	
	presence_chooser_show_popup (chooser);

	return TRUE;
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

	list = gossip_status_presets_get (GOSSIP_PRESENCE_STATE_AVAILABLE, 4);
	for (l = list; l; l = l->next) {
		presence_chooser_add_item (chooser,
					   menu,
					   l->data,
					   GOSSIP_PRESENCE_STATE_AVAILABLE,
					   FALSE);
	}
	g_list_free (list);
	
	presence_chooser_add_item (chooser,
				   menu,
				   _("Available..."),
				   GOSSIP_PRESENCE_STATE_AVAILABLE,
				   TRUE);
	
	/* Separator. */
	item = gtk_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	list = gossip_status_presets_get (GOSSIP_PRESENCE_STATE_BUSY, 4);
	for (l = list; l; l = l->next) {
		presence_chooser_add_item (chooser,
					   menu,
					   l->data,
					   GOSSIP_PRESENCE_STATE_BUSY,
					   FALSE);
	}
	g_list_free (list);

	presence_chooser_add_item (chooser,
				   menu,
				   _("Busy..."),
				   GOSSIP_PRESENCE_STATE_BUSY,
				   TRUE);
	
	/* Separator. */
	item = gtk_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	list = gossip_status_presets_get (GOSSIP_PRESENCE_STATE_AWAY, 4);
	for (l = list; l; l = l->next) {
		presence_chooser_add_item (chooser,
					   menu,
					   l->data,
					   GOSSIP_PRESENCE_STATE_AWAY,
					   FALSE);
	}
	g_list_free (list);

	presence_chooser_add_item (chooser,
				   menu,
				   _("Away..."),
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
gossip_presence_chooser_set_status (GossipPresenceChooser *chooser,
				    const gchar           *status)
{
	GossipPresenceChooserPriv *priv;

	priv = GET_PRIV (chooser);
	
	gtk_label_set_text (GTK_LABEL (priv->label), status);
}

