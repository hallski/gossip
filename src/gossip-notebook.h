/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * The source code in this file is based on code contained in the file
 * ephy-notebook.h, as part of the Epiphany web browser.
 * Changes from the original source code are:
 * 
 * Copyright (C) 2003 Geert-Jan Van den Bogaerde <gvdbogaerde@pandora.be>
 *
 * The original source code is:
 *
 * Copyright (C) 2002 Christophe Fergeau
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

#ifndef __GOSSIP_NOTEBOOK_H__
#define __GOSSIP_NOTEBOOK_H__

#include <glib.h>
#include <gtk/gtknotebook.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_NOTEBOOK         (gossip_notebook_get_type ())
#define GOSSIP_NOTEBOOK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_NOTEBOOK, GossipNotebook))
#define GOSSIP_NOTEBOOK_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_NOTEBOOK, GossipNotebookClass))
#define GOSSIP_IS_NOTEBOOK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_NOTEBOOK))
#define GOSSIP_IS_NOTEBOOK_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_NOTEBOOK))
#define GOSSIP_NOTEBOOK_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_NOTEBOOK, GossipNotebookClass))

typedef struct _GossipNotebook GossipNotebook;
typedef struct _GossipNotebookClass GossipNotebookClass;
typedef struct _GossipNotebookPriv GossipNotebookPriv;

enum
{
        GOSSIP_NOTEBOOK_INSERT_LAST = -1,
};

struct _GossipNotebook
{
        GtkNotebook parent;
        GossipNotebookPriv *priv;
};

struct _GossipNotebookClass
{
        GtkNotebookClass parent_class;

        /* Signals */
        void (* tab_added)      (GossipNotebook *notebook,
                                 GtkWidget      *child);
        void (* tab_removed)    (GossipNotebook *notebook,
                                 GtkWidget      *child);
        void (* tab_detached)   (GossipNotebook *notebook,
                                 GtkWidget      *child);
        void (* tabs_reordered) (GossipNotebook *notebook);
};

GType           gossip_notebook_get_type        (void);

GtkWidget      *gossip_notebook_new             (void);

void            gossip_notebook_insert_page     (GossipNotebook *notebook,
                                                 GtkWidget      *child,
                                                 GtkWidget      *label,
                                                 gint            position,
                                                 gboolean        jump_to);

void            gossip_notebook_remove_page     (GossipNotebook *notebook,
                                                 GtkWidget      *child);

void            gossip_notebook_move_page       (GossipNotebook *notebook,
                                                 GossipNotebook *dest,
                                                 GtkWidget      *child,
                                                 gint            dest_page);

void            gossip_notebook_set_show_tabs   (GossipNotebook *notebook,
                                                 gboolean        show_tabs);

GtkWidget      *gossip_notebook_get_tab_label   (GossipNotebook *notebook,
                                                 GtkWidget      *child);

void            gossip_notebook_set_tab_label   (GossipNotebook *notebook,
                                                 GtkWidget      *child,
                                                 GtkWidget      *label);

G_END_DECLS

#endif /* __GOSSIP_NOTEBOOK_H__ */
