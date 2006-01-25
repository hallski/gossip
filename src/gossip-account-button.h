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

#ifndef __GOSSIP_ACCOUNT_BUTTON_H__
#define __GOSSIP_ACCOUNT_BUTTON_H__

#include <gtk/gtk.h>

#define GOSSIP_TYPE_ACCOUNT_BUTTON \
  (gossip_account_button_get_type ())
#define GOSSIP_ACCOUNT_BUTTON(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_ACCOUNT_BUTTON, GossipAccountButton))
#define GOSSIP_ACCOUNT_BUTTON_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_ACCOUNT_BUTTON, GossipAccountButtonClass))
#define GOSSIP_IS_ACCOUNT_BUTTON(o) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_ACCOUNT_BUTTON))
#define GOSSIP_IS_ACCOUNT_BUTTON_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_ACCOUNT_BUTTON))
#define GOSSIP_ACCOUNT_BUTTON_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_ACCOUNT_BUTTON, GossipAccountButtonClass))

typedef struct _GossipAccountButton      GossipAccountButton;
typedef struct _GossipAccountButtonClass GossipAccountButtonClass;

struct _GossipAccountButton {
        GtkToggleToolButton parent;
};

struct _GossipAccountButtonClass {
        GtkToggleToolButtonClass parent_class;
};

GType      gossip_account_button_get_type         (void) G_GNUC_CONST;

GtkWidget *gossip_account_button_new              (void);

void       gossip_account_button_set_account      (GossipAccountButton *button,
						   GossipAccount       *account);
void       gossip_account_button_set_status       (GossipAccountButton *button,
						   gboolean             online);
gboolean   gossip_account_button_get_is_important (GossipAccountButton *button);



#endif /* __GOSSIP_ACCOUNT_BUTTON_H__ */

