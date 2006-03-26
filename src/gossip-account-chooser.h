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

#ifndef __GOSSIP_ACCOUNT_CHOOSER_H__
#define __GOSSIP_ACCOUNT_CHOOSER_H__

#include <gtk/gtkcombobox.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-session.h>

#define GOSSIP_TYPE_ACCOUNT_CHOOSER (gossip_account_chooser_get_type ())
#define GOSSIP_ACCOUNT_CHOOSER(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_ACCOUNT_CHOOSER, GossipAccountChooser))
#define GOSSIP_ACCOUNT_CHOOSER_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_ACCOUNT_CHOOSER, GossipAccountChooserClass))
#define GOSSIP_IS_ACCOUNT_CHOOSER(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_ACCOUNT_CHOOSER))
#define GOSSIP_IS_ACCOUNT_CHOOSER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_ACCOUNT_CHOOSER))
#define GOSSIP_ACCOUNT_CHOOSER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_ACCOUNT_CHOOSER, GossipAccountChooserClass))

typedef struct _GossipAccountChooser      GossipAccountChooser;
typedef struct _GossipAccountChooserClass GossipAccountChooserClass;

struct _GossipAccountChooser {
        GtkComboBox parent;
};

struct _GossipAccountChooserClass {
        GtkComboBoxClass parent_class;
};

GType          gossip_account_chooser_get_type           (void) G_GNUC_CONST;

GtkWidget *    gossip_account_chooser_new                (GossipSession        *session);
gboolean       gossip_account_chooser_set_account        (GossipAccountChooser *account_chooser,
							  GossipAccount        *account);
GossipAccount *gossip_account_chooser_get_account        (GossipAccountChooser *chooser);
void           gossip_account_chooser_set_can_select_all (GossipAccountChooser *account_chooser,
							  gboolean              can_select_all);

#endif /* __GOSSIP_ACCOUNT_CHOOSER_H__ */

