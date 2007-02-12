/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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
 * 
 * Authors: Richard Hult <richard@imendio.com>
 */

#ifndef __GOSSIP_POPUP_BUTTON_H__
#define __GOSSIP_POPUP_BUTTON_H__

#include <gtk/gtktogglebutton.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_POPUP_BUTTON            (gossip_popup_button_get_type ())
#define GOSSIP_POPUP_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOSSIP_TYPE_POPUP_BUTTON, GossipPopupButton))
#define GOSSIP_POPUP_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GOSSIP_TYPE_POPUP_BUTTON, GossipPopupButtonClass))
#define GOSSIP_IS_POPUP_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOSSIP_TYPE_POPUP_BUTTON))
#define GOSSIP_IS_POPUP_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GOSSIP_TYPE_POPUP_BUTTON))
#define GOSSIP_POPUP_BUTTON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GOSSIP_TYPE_POPUP_BUTTON, GossipPopupButtonClass))


typedef struct _GossipPopupButton      GossipPopupButton;
typedef struct _GossipPopupButtonClass GossipPopupButtonClass;

struct _GossipPopupButton {
	GtkToggleButton  parent_instance;
};

struct _GossipPopupButtonClass {
	GtkToggleButtonClass parent_class;
};

GType      gossip_popup_button_get_type (void) G_GNUC_CONST;
GtkWidget *gossip_popup_button_new      (const gchar       *label);
void       gossip_popup_button_popup    (GossipPopupButton *button);
void       gossip_popup_button_popdown  (GossipPopupButton *button,
					 gboolean           ok);

G_END_DECLS

#endif /* __GOSSIP_POPUP_BUTTON_H__ */
