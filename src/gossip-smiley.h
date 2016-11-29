/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2007 Imendio AB
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

#ifndef __GOSSIP_SMILEY_H__
#define __GOSSIP_SMILEY_H__

#include <gtk/gtk.h>

typedef enum {
    GOSSIP_SMILEY_NORMAL,       /*  :)   */
    GOSSIP_SMILEY_WINK,         /*  ;)   */
    GOSSIP_SMILEY_BIGEYE,       /*  =)   */
    GOSSIP_SMILEY_NOSE,         /*  :-)  */
    GOSSIP_SMILEY_CRY,          /*  :'(  */
    GOSSIP_SMILEY_SAD,          /*  :(   */
    GOSSIP_SMILEY_SCEPTICAL,    /*  :/   */
    GOSSIP_SMILEY_BIGSMILE,     /*  :D   */
    GOSSIP_SMILEY_INDIFFERENT,  /*  :|   */
    GOSSIP_SMILEY_TOUNGE,       /*  :p   */
    GOSSIP_SMILEY_SHOCKED,      /*  :o   */
    GOSSIP_SMILEY_COOL,         /*  8)   */
    GOSSIP_SMILEY_SORRY,        /*  *|   */
    GOSSIP_SMILEY_KISS,         /*  :*   */
    GOSSIP_SMILEY_SHUTUP,       /*  :#   */
    GOSSIP_SMILEY_YAWN,         /*  |O   */
    GOSSIP_SMILEY_CONFUSED,     /*  :$   */
    GOSSIP_SMILEY_ANGEL,        /*  <)   */
    GOSSIP_SMILEY_OOOH,         /*  :x   */
    GOSSIP_SMILEY_LOOKAWAY,     /*  *)   */
    GOSSIP_SMILEY_BLUSH,        /*  *S   */
    GOSSIP_SMILEY_COOLBIGSMILE, /*  8D   */
    GOSSIP_SMILEY_ANGRY,        /*  :@   */
    GOSSIP_SMILEY_BOSS,         /*  @)   */
    GOSSIP_SMILEY_MONKEY,       /*  #)   */
    GOSSIP_SMILEY_SILLY,        /*  O)   */
    GOSSIP_SMILEY_SICK,         /*  +o(  */

    GOSSIP_SMILEY_COUNT
} GossipSmiley;

typedef struct {
    GossipSmiley  smiley;
    const gchar  *pattern;
} GossipSmileyPattern;

static const GossipSmileyPattern smileys[] = {
    /* Forward smileys. */
    { GOSSIP_SMILEY_NORMAL,       ":)"  },
    { GOSSIP_SMILEY_WINK,         ";)"  },
    { GOSSIP_SMILEY_WINK,         ";-)" },
    { GOSSIP_SMILEY_BIGEYE,       "=)"  },
    { GOSSIP_SMILEY_NOSE,         ":-)" },
    { GOSSIP_SMILEY_CRY,          ":'(" },
    { GOSSIP_SMILEY_SAD,          ":("  },
    { GOSSIP_SMILEY_SAD,          ":-(" },
    { GOSSIP_SMILEY_SCEPTICAL,    ":/"  },
    { GOSSIP_SMILEY_SCEPTICAL,    ":\\" },
    { GOSSIP_SMILEY_BIGSMILE,     ":D"  },
    { GOSSIP_SMILEY_BIGSMILE,     ":-D" },
    { GOSSIP_SMILEY_INDIFFERENT,  ":|"  },
    { GOSSIP_SMILEY_TOUNGE,       ":p"  },
    { GOSSIP_SMILEY_TOUNGE,       ":-p" },
    { GOSSIP_SMILEY_TOUNGE,       ":P"  },
    { GOSSIP_SMILEY_TOUNGE,       ":-P" },
    { GOSSIP_SMILEY_TOUNGE,       ";p"  },
    { GOSSIP_SMILEY_TOUNGE,       ";-p" },
    { GOSSIP_SMILEY_TOUNGE,       ";P"  },
    { GOSSIP_SMILEY_TOUNGE,       ";-P" },
    { GOSSIP_SMILEY_SHOCKED,      ":o"  },
    { GOSSIP_SMILEY_SHOCKED,      ":-o" },
    { GOSSIP_SMILEY_SHOCKED,      ":O"  },
    { GOSSIP_SMILEY_SHOCKED,      ":-O" },
    { GOSSIP_SMILEY_COOL,         "8)"  },
    { GOSSIP_SMILEY_COOL,         "B)"  },
    { GOSSIP_SMILEY_SORRY,        "*|"  },
    { GOSSIP_SMILEY_KISS,         ":*"  },
    { GOSSIP_SMILEY_SHUTUP,       ":#"  },
    { GOSSIP_SMILEY_SHUTUP,       ":-#" },
    { GOSSIP_SMILEY_YAWN,         "|O"  },
    { GOSSIP_SMILEY_CONFUSED,     ":S"  },
    { GOSSIP_SMILEY_CONFUSED,     ":s"  },
    { GOSSIP_SMILEY_ANGEL,        "<)"  },
    { GOSSIP_SMILEY_OOOH,         ":x"  },
    { GOSSIP_SMILEY_LOOKAWAY,     "*)"  },
    { GOSSIP_SMILEY_LOOKAWAY,     "*-)" },
    { GOSSIP_SMILEY_BLUSH,        "*S"  },
    { GOSSIP_SMILEY_BLUSH,        "*s"  },
    { GOSSIP_SMILEY_BLUSH,        "*$"  },
    { GOSSIP_SMILEY_COOLBIGSMILE, "8D"  },
    { GOSSIP_SMILEY_ANGRY,        ":@"  },
    { GOSSIP_SMILEY_BOSS,         "@)"  },
    { GOSSIP_SMILEY_MONKEY,       "#)"  },
    { GOSSIP_SMILEY_SILLY,        "O)"  },
    { GOSSIP_SMILEY_SICK,         "+o(" },

    /* Backward smileys. */
    { GOSSIP_SMILEY_NORMAL,       "(:"  },
    { GOSSIP_SMILEY_WINK,         "(;"  },
    { GOSSIP_SMILEY_WINK,         "(-;" },
    { GOSSIP_SMILEY_BIGEYE,       "(="  },
    { GOSSIP_SMILEY_NOSE,         "(-:" },
    { GOSSIP_SMILEY_CRY,          ")':" },
    { GOSSIP_SMILEY_SAD,          "):"  },
    { GOSSIP_SMILEY_SAD,          ")-:" },
    { GOSSIP_SMILEY_SCEPTICAL,    "/:"  },
    { GOSSIP_SMILEY_SCEPTICAL,    "//:" },
    { GOSSIP_SMILEY_INDIFFERENT,  "|:"  },
    { GOSSIP_SMILEY_TOUNGE,       "d:"  },
    { GOSSIP_SMILEY_TOUNGE,       "d-:" },
    { GOSSIP_SMILEY_TOUNGE,       "d;"  },
    { GOSSIP_SMILEY_TOUNGE,       "d-;" },
    { GOSSIP_SMILEY_SHOCKED,      "o:"  },
    { GOSSIP_SMILEY_SHOCKED,      "O:"  },
    { GOSSIP_SMILEY_COOL,         "(8"  },
    { GOSSIP_SMILEY_COOL,         "(B"  },
    { GOSSIP_SMILEY_SORRY,        "|*"  },
    { GOSSIP_SMILEY_KISS,         "*:"  },
    { GOSSIP_SMILEY_SHUTUP,       "#:"  },
    { GOSSIP_SMILEY_SHUTUP,       "#-:" },
    { GOSSIP_SMILEY_YAWN,         "O|"  },
    { GOSSIP_SMILEY_CONFUSED,     "S:"  },
    { GOSSIP_SMILEY_CONFUSED,     "s:"  },
    { GOSSIP_SMILEY_ANGEL,        "(>"  },
    { GOSSIP_SMILEY_OOOH,         "x:"  },
    { GOSSIP_SMILEY_LOOKAWAY,     "(*"  },
    { GOSSIP_SMILEY_LOOKAWAY,     "(-*" },
    { GOSSIP_SMILEY_BLUSH,        "S*"  },
    { GOSSIP_SMILEY_BLUSH,        "s*"  },
    { GOSSIP_SMILEY_BLUSH,        "$*"  },
    { GOSSIP_SMILEY_ANGRY,        "@:"  },
    { GOSSIP_SMILEY_BOSS,         "(@"  },
    { GOSSIP_SMILEY_MONKEY,       "#)"  },
    { GOSSIP_SMILEY_SILLY,        "(O"  },
    { GOSSIP_SMILEY_SICK,         ")o+" }
};

GdkPixbuf *  gossip_chat_view_get_smiley_image (GossipSmiley  smiley);
const gchar *gossip_chat_view_get_smiley_text  (GossipSmiley  smiley);
GtkWidget *  gossip_chat_view_get_smiley_menu  (GCallback     callback,
                                                gpointer      user_data,
                                                GtkTooltips  *tooltips);
GdkPixbuf *  gossip_pixbuf_from_smiley         (GossipSmiley  type,
                                                GtkIconSize   icon_size);

G_END_DECLS

#endif /* __GOSSIP_SMILEY_H__ */
