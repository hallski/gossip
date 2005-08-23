/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * The source code in this file is based on code contained in the file
 * ephy-notebook.c as part of the Epiphany web browser.
 * Changes from the original source code are:
 * 
 * Copyright (C) 2003 Geert-Jan Van den Bogaerde <gvdbogaerde@pandora.be>
 *
 * The original source code is:
 *
 * Copyright (C) 2002 Christophe Fergeau
 * Copyright (C) 2003 Christian Persch
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
#include "gossip-notebook.h"

#define AFTER_ALL_TABS -1
#define NOT_IN_APP_WINDOWS -2
#define TAB_MIN_SIZE 60
#define TAB_NB_MAX 8


static void gossip_notebook_init                (GossipNotebook      *notebook);
static void gossip_notebook_class_init          (GossipNotebookClass *klass);
static void gossip_notebook_finalize            (GObject             *object);

static gboolean is_in_notebook_window           (GossipNotebook      *notebook,
                                                 gint                 abs_x,
                                                 gint                 abs_y);
static GossipNotebook *find_notebook_at_pointer (gint                 abs_x,
                                                 gint                 abs_y);
static gint find_tab_num_at_pos                 (GossipNotebook      *notebook,
                                                 gint                 abs_x,
                                                 gint                 abs_y);
static gint find_notebook_and_tab_at_pos        (gint                 abs_x,
                                                 gint                 abs_y,
                                                 GossipNotebook     **notebook,
                                                 gint                *page_num);
static void move_tab                            (GossipNotebook      *notebook,
                                                 gint                 dest_page_num);
static void move_tab_to_another_notebook        (GossipNotebook      *src,
                                                 GossipNotebook     *dest,
                                                 gint                dest_page);
static void drag_start                          (GossipNotebook     *notebook,
                                                 GossipNotebook     *src_notebook,
                                                 gint                src_page);
static void drag_stop                           (GossipNotebook     *notebook);

/* callbacks */

static gboolean motion_notify_cb                (GossipNotebook     *notebook,
                                                 GdkEventMotion     *event,
                                                 gpointer            data);
static gboolean button_release_cb               (GossipNotebook     *notebook,
                                                 GdkEventButton     *event,
                                                 gpointer            data);
static gboolean button_press_cb                 (GossipNotebook     *notebook,
                                                 GdkEventButton     *event,
                                                 gpointer            data);

struct _GossipNotebookPriv
{
        gulong          motion_notify_handler_id;
        gboolean        drag_in_progress;
	GossipNotebook *last_dest;
        GossipNotebook *src_notebook;
        gint            src_page;
        gint            x_start;
        gint            y_start;
};

enum
{
        TAB_ADDED,
        TAB_REMOVED,
        TABS_REORDERED,
        TAB_DETACHED,
        LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

static GdkCursor *cursor = NULL;
static GList *notebooks = NULL;

GType
gossip_notebook_get_type (void)
{
        static GType type_id = 0;

        if (type_id == 0)
        {
                static const GTypeInfo type_info =
                {
                        sizeof (GossipNotebookClass),
                        NULL,
                        NULL,
                        (GClassInitFunc) gossip_notebook_class_init,
                        NULL,
                        NULL,
                        sizeof (GossipNotebook),
                        0,
                        (GInstanceInitFunc) gossip_notebook_init
                };

                type_id = g_type_register_static (GTK_TYPE_NOTEBOOK,
                                                  "GossipNotebook",
                                                  &type_info, 0);
        }

        return type_id;
}

static void
gossip_notebook_class_init (GossipNotebookClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = gossip_notebook_finalize;

        signals[TAB_ADDED] =
                g_signal_new ("tab_added",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GossipNotebookClass, tab_added),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE,
                              1,
                              GTK_TYPE_WIDGET);

        signals[TAB_REMOVED] =
                g_signal_new ("tab_removed",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GossipNotebookClass, tab_removed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE,
                              1,
                              GTK_TYPE_WIDGET);

        signals[TAB_DETACHED] =
                g_signal_new ("tab_detached",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GossipNotebookClass, tab_detached),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE,
                              1,
                              GTK_TYPE_WIDGET);

        signals[TABS_REORDERED] =
                g_signal_new ("tabs_reordered",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GossipNotebookClass, tabs_reordered),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0);
}

static void
gossip_notebook_init (GossipNotebook *notebook)
{
        notebook->priv = g_new0 (GossipNotebookPriv, 1);

        notebook->priv->drag_in_progress = FALSE;
        notebook->priv->motion_notify_handler_id = 0;
        notebook->priv->src_notebook = NULL;
        notebook->priv->src_page = -1;

        notebooks = g_list_append (notebooks, notebook);

        g_signal_connect (G_OBJECT (notebook),
                          "button-press-event",
                          G_CALLBACK (button_press_cb),
                          NULL);

        g_signal_connect (G_OBJECT (notebook),
                          "button-release-event",
                          G_CALLBACK (button_release_cb),
                          NULL);

        gtk_widget_add_events (GTK_WIDGET (notebook), GDK_BUTTON1_MOTION_MASK);
}

static void
gossip_notebook_finalize (GObject *object)
{
        GossipNotebook *notebook = GOSSIP_NOTEBOOK (object);
        notebooks = g_list_remove (notebooks, notebook);
        g_free (notebook->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
is_in_notebook_window (GossipNotebook *notebook,
                       gint            abs_x,
                       gint            abs_y)
{
        gint x, y;
        gint rel_x, rel_y;
        gint width, height;
        GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (notebook));
        GdkWindow *window = GTK_WIDGET (toplevel)->window;

        gdk_window_get_origin (window, &x, &y);
        rel_x = abs_x - x;
        rel_y = abs_y - y;

        x      = GTK_WIDGET (notebook)->allocation.x;
        y      = GTK_WIDGET (notebook)->allocation.y;
        height = GTK_WIDGET (notebook)->allocation.height;
        width  = GTK_WIDGET (notebook)->allocation.width;

        return ((rel_x >= x) && (rel_y >= y) && (rel_x <= x + width) && (rel_y <= y + height));
}

static GossipNotebook *
find_notebook_at_pointer (gint abs_x, gint abs_y)
{
        GList     *l;
        gint       x, y;
        GdkWindow *win_at_pointer;
        GdkWindow *parent_at_pointer = NULL;

	win_at_pointer = gdk_window_at_pointer (&x, &y);

        if (win_at_pointer == NULL) {
                /* We are outside all windows containing a notebook */
                return NULL;
        }

        win_at_pointer = gdk_window_get_toplevel (win_at_pointer);
        /* When we are in the notebook event window, win_at_pointer will be
         * this event window, and the toplevel window we are interested in
         * will be its parent.
         */
        parent_at_pointer = gdk_window_get_parent (win_at_pointer);

        for (l = notebooks; l != NULL; l = l->next) {
                GossipNotebook *nb = GOSSIP_NOTEBOOK (l->data);
                GdkWindow *win = GTK_WIDGET (nb)->window;
		
                win = gdk_window_get_toplevel (win);
                if (((win == win_at_pointer) || (win == parent_at_pointer)) &&
                    is_in_notebook_window (nb, abs_x, abs_y)) {
                        return nb;
                }
        }

        return NULL;
}

static gint
find_tab_num_at_pos (GossipNotebook *notebook,
                     gint            abs_x,
                     gint            abs_y)
{
        GtkPositionType tab_pos;
        int page_num = 0;
        GtkWidget *page;

        tab_pos = gtk_notebook_get_tab_pos (GTK_NOTEBOOK (notebook));

        if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) == 0) {
                return AFTER_ALL_TABS;
        }

        /* For some reason unfullscreen + quick click can
         * cause a wrong click event to be reported to the tab */
        if (!is_in_notebook_window (notebook, abs_x, abs_y)) {
                return NOT_IN_APP_WINDOWS;
        }

        while ((page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), page_num))) {
                GtkWidget *tab;
                gint max_x, max_y;
                gint x_root, y_root;

                tab = gtk_notebook_get_tab_label (GTK_NOTEBOOK (notebook), page);
                g_return_val_if_fail (tab != NULL, -1);

                if (!GTK_WIDGET_MAPPED (tab)) {
                        page_num++;
                        continue;
                }

                gdk_window_get_origin (GDK_WINDOW (tab->window),
                                       &x_root, &y_root);

                max_x = x_root + tab->allocation.x + tab->allocation.width;
                max_y = y_root + tab->allocation.y + tab->allocation.height;

                if (((tab_pos == GTK_POS_TOP) ||
                     (tab_pos == GTK_POS_BOTTOM)) &&
                    (abs_x <= max_x)) {
                        return page_num;
                }
                else if (((tab_pos == GTK_POS_LEFT) ||
                          (tab_pos == GTK_POS_RIGHT)) &&
                          (abs_y <= max_y)) {
                        return page_num;
                }

                page_num++;
        }

        return AFTER_ALL_TABS;
}

static gint
find_notebook_and_tab_at_pos (gint             abs_x,
                              gint             abs_y,
                              GossipNotebook **notebook,
                              gint            *page_num)
{
        *notebook = find_notebook_at_pointer (abs_x, abs_y);
        
        if (*notebook == NULL) {
                return NOT_IN_APP_WINDOWS;
        }

        *page_num = find_tab_num_at_pos (*notebook, abs_x, abs_y);

        if (*page_num < 0) {
                return *page_num;
        }
        else {
                return 0;
        }
}

/* This function is only called during dnd, we don't need to emit TABS_REORDERED
 * here, instead we do it on drag_stop. */
static void
move_tab (GossipNotebook *notebook, gint dest_page_num)
{
        gint cur_page_num;

        cur_page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));

        if (dest_page_num != cur_page_num) {
                GtkWidget *cur_page;
                cur_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook),
                                                      cur_page_num);
                gossip_notebook_move_page (notebook, NULL, 
                                           cur_page, dest_page_num);
        }
}

static void
move_tab_to_another_notebook (GossipNotebook *src,
                              GossipNotebook *dest,
                              gint            dest_page)
{
        GtkWidget *child;
        gint cur_page;

        /* This is getting tricky, the tab was dragged in a notebook
         * in another window of the same app, we move the tab
         * to the new notebook, and let this notebook handle the
         * drag. */
        g_assert (GOSSIP_IS_NOTEBOOK (dest));
        g_assert (dest != src);

        /* Move the widgets (tab label and tab content) to the new
         * notebook. */
        cur_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (src));
        child    = gtk_notebook_get_nth_page (GTK_NOTEBOOK (src), cur_page);

 	drag_stop (src); 
	
        gossip_notebook_move_page (src, dest, child, dest_page);
}

static void
drag_start (GossipNotebook *notebook,
            GossipNotebook *src_notebook,
            gint            src_page)
{
        notebook->priv->drag_in_progress = TRUE;
	notebook->priv->last_dest        = NULL;
        notebook->priv->src_notebook     = src_notebook;
        notebook->priv->src_page         = src_page;

        /* get a new cursor, if necessary */
        if (!cursor) {
                cursor = gdk_cursor_new (GDK_FLEUR);
        }

        /* grab the pointer */
        gtk_grab_add (GTK_WIDGET (notebook));

        if (!gdk_pointer_is_grabbed ()) {
                gdk_pointer_grab (GTK_WIDGET (notebook)->window,
                                  FALSE,
                                  GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
                                  NULL, cursor, GDK_CURRENT_TIME);
        }
}

static void
drag_stop (GossipNotebook *notebook)
{
        if (notebook->priv->drag_in_progress) {
                g_signal_emit (G_OBJECT (notebook), signals[TABS_REORDERED], 0);
        }

        notebook->priv->drag_in_progress = FALSE;
	notebook->priv->last_dest        = NULL;
        notebook->priv->src_notebook     = NULL;
        notebook->priv->src_page         = -1;

        if (notebook->priv->motion_notify_handler_id != 0) {
                g_signal_handler_disconnect (G_OBJECT (notebook),
                                             notebook->priv->motion_notify_handler_id);
                notebook->priv->motion_notify_handler_id = 0;
        }

	/* ungrab the pointer if it's grabbed. */
 	gtk_grab_remove (GTK_WIDGET (notebook));

	if (gdk_pointer_is_grabbed ()) {
		gdk_pointer_ungrab (GDK_CURRENT_TIME);
	}
}

static gboolean
motion_notify_cb (GossipNotebook *notebook,
                  GdkEventMotion *event,
                  gpointer        data)
{
        GossipNotebook *dest;
        gint            page;
        gint            result;
	gboolean        highlighted = FALSE;

	if (notebook->priv->drag_in_progress == FALSE) {
		if (gtk_drag_check_threshold (GTK_WIDGET (notebook),
					      notebook->priv->x_start,
					      notebook->priv->y_start,
					      event->x_root, event->y_root)) {
			gint cur_page;
			
			cur_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
			drag_start (notebook, notebook, cur_page);
		} else {
			return FALSE;
		}
	}
	
        result = find_notebook_and_tab_at_pos ((gint) event->x_root,
                                               (gint) event->y_root,
                                               &dest, &page);

        if (result != NOT_IN_APP_WINDOWS) {
                if (dest != notebook) {
			if (notebook->priv->last_dest != NULL && 
			    notebook->priv->last_dest != dest) {
				gtk_drag_unhighlight (GTK_WIDGET (notebook->priv->last_dest));
			}

			gtk_drag_highlight (GTK_WIDGET (dest));

			notebook->priv->last_dest = dest;

			highlighted = TRUE;
                } else {
                        g_assert (page >= -1);
                        move_tab (notebook, page);
                }
        }

	if (!highlighted) {
		if (notebook->priv->last_dest) {
			gtk_drag_unhighlight (GTK_WIDGET (notebook->priv->last_dest));
		}
		
		notebook->priv->last_dest = NULL;
	}

        return FALSE;
}

static gboolean
button_release_cb (GossipNotebook *notebook,
                   GdkEventButton *event,
                   gpointer        data)
{
        if (notebook->priv->drag_in_progress) {
                gint cur_page_num;
                GtkWidget *cur_page;
		GossipNotebook *dest;
		gint            page;

                cur_page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
                cur_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 
                                                      cur_page_num);

		find_notebook_and_tab_at_pos ((gint) event->x_root,
					      (gint) event->y_root,
					      &dest, &page);
		
		if (GOSSIP_IS_NOTEBOOK (dest)) {
			gtk_drag_unhighlight (GTK_WIDGET (dest));
			
			if (dest != notebook) {
				move_tab_to_another_notebook (notebook, dest, page);
			}
			else {
				g_assert (page >= -1);
				move_tab (notebook, page);
			}
		} else {
			if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) > 1) {
				/* detach tab */
				g_signal_emit (G_OBJECT (notebook),
					       signals[TAB_DETACHED],
					       0, cur_page);
			}
		}
	}

        /* this must be called even if a drag isn't happening. */
        drag_stop (notebook);

        return FALSE;
}

static gboolean
button_press_cb (GossipNotebook *notebook,
                 GdkEventButton *event,
                 gpointer        data)
{
        gint tab_clicked = find_tab_num_at_pos (notebook,
                                                event->x_root,
                                                event->y_root);

        if (notebook->priv->drag_in_progress) {
                return TRUE;
        }

        if ((event->button == 1) && (event->type == GDK_BUTTON_PRESS) &&
            (tab_clicked >= 0)) {
                notebook->priv->x_start = event->x_root;
                notebook->priv->y_start = event->y_root;
                notebook->priv->motion_notify_handler_id =
                        g_signal_connect (G_OBJECT (notebook),
                                          "motion-notify-event",
                                          G_CALLBACK (motion_notify_cb),
                                          NULL);
        }

        return FALSE;
}

GtkWidget *
gossip_notebook_new (void)
{
        return GTK_WIDGET (g_object_new (GOSSIP_TYPE_NOTEBOOK, NULL));
}

void
gossip_notebook_insert_page (GossipNotebook *notebook,
                             GtkWidget      *child,
                             GtkWidget      *label,
                             int             position,
                             gboolean        jump_to)
{
        gtk_notebook_insert_page (GTK_NOTEBOOK (notebook),
                                  child,
                                  label,
                                  position);

        g_signal_emit (notebook, signals[TAB_ADDED], 0, child);

        if (jump_to == TRUE) {
                gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook),
                                               position);
        }
}

void
gossip_notebook_remove_page (GossipNotebook *notebook,
                             GtkWidget      *child)
{
        int position;

        position = gtk_notebook_page_num (GTK_NOTEBOOK (notebook),
                                          child);

        /* We ref the child so it's still alive while the tab_removed
         * signal is processed. */
        g_object_ref (child);

        gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), position);

        g_signal_emit (notebook,
                       signals[TAB_REMOVED],
                       0, child);

        g_object_unref (child); 
}

void
gossip_notebook_move_page (GossipNotebook *src,
                           GossipNotebook *dest,
                           GtkWidget      *src_page,
                           gint            dest_page)
{
        if (dest == NULL || src == dest) {
                gtk_notebook_reorder_child (GTK_NOTEBOOK (src), src_page, dest_page);

                if (src->priv->drag_in_progress == FALSE) {
                        g_signal_emit (G_OBJECT (src),
                                       signals[TABS_REORDERED],
                                       0);
                }
        }
        else {
                GtkWidget *src_label;

		src_label = gossip_notebook_get_tab_label (src, src_page);

                /* Make sure the child and label aren't destroyed while we move
                 * them. */
		g_object_ref (G_OBJECT (src_page));
                g_object_ref (G_OBJECT (src_label));

		/* Remove the label so we can insert it in it's new parent. */
		gtk_container_remove (
			GTK_CONTAINER (gtk_widget_get_parent (src_label)),
			src_label);
		
                gossip_notebook_remove_page (src, src_page);
                gossip_notebook_insert_page (dest, src_page, src_label, 
                                             dest_page, TRUE);

                g_object_unref (G_OBJECT (src_page));
                g_object_unref (G_OBJECT (src_label));
        } 
}

void
gossip_notebook_set_show_tabs (GossipNotebook *notebook,
                               gboolean        show_tabs)
{
        gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), show_tabs); 
}

GtkWidget *
gossip_notebook_get_tab_label (GossipNotebook *notebook,
                               GtkWidget      *child)
{
        GtkWidget *label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (notebook),
                                                       child);
        GList *children = gtk_container_get_children (GTK_CONTAINER (label));
        return g_list_nth_data (children, 0);
}

void
gossip_notebook_set_tab_label (GossipNotebook *notebook,
                               GtkWidget      *child,
                               GtkWidget      *label)
{
        gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook),
                                    child, label);
}
