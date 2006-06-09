/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Imendio AB
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

#ifndef __GOSSIP_PREFERENCES_H__
#define __GOSSIP_PREFERENCES_H__

#define GCONF_PATH "/apps/gossip"

#define GCONF_SOUNDS_FOR_MESSAGES          GCONF_PATH "/notifications/sounds_for_messages"
#define GCONF_SOUNDS_WHEN_AWAY             GCONF_PATH "/notifications/sounds_when_away"
#define GCONF_SOUNDS_WHEN_BUSY             GCONF_PATH "/notifications/sounds_when_busy"
#define GCONF_POPUPS_WHEN_AVAILABLE        GCONF_PATH "/notifications/popups_when_available"
#define GCONF_CHAT_SHOW_SMILEYS            GCONF_PATH "/conversation/graphical_smileys"
#define GCONF_CHAT_THEME                   GCONF_PATH "/conversation/theme"
#define GCONF_CHAT_SPELL_CHECKER_LANGUAGES GCONF_PATH "/conversation/spell_checker_languages"

#define GCONF_UI_SEPARATE_CHAT_WINDOWS     GCONF_PATH "/ui/separate_chat_windows"
#define GCONF_UI_MAIN_WINDOW_HIDDEN        GCONF_PATH "/ui/main_window_hidden"
#define GCONF_UI_AVATAR_DIRECTORY          GCONF_PATH "/ui/avatar_directory"
#define GCONF_UI_SHOW_AVATARS              GCONF_PATH "/ui/show_avatars"

#define GCONF_CONTACTS_SHOW_OFFLINE        GCONF_PATH "/contacts/show_offline"

void gossip_preferences_show               (void);
void gossip_preferences_show_status_editor (void);

#endif /* __GOSSIP_PREFERENCES_H__ */
