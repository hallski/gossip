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

#define GOSSIP_PREFS_PATH "/apps/gossip"

#define GOSSIP_PREFS_SOUNDS_FOR_MESSAGES          GOSSIP_PREFS_PATH "/notifications/sounds_for_messages"
#define GOSSIP_PREFS_SOUNDS_WHEN_AWAY             GOSSIP_PREFS_PATH "/notifications/sounds_when_away"
#define GOSSIP_PREFS_SOUNDS_WHEN_BUSY             GOSSIP_PREFS_PATH "/notifications/sounds_when_busy"
#define GOSSIP_PREFS_POPUPS_WHEN_AVAILABLE        GOSSIP_PREFS_PATH "/notifications/popups_when_available"
#define GOSSIP_PREFS_CHAT_SHOW_SMILEYS            GOSSIP_PREFS_PATH "/conversation/graphical_smileys"
#define GOSSIP_PREFS_CHAT_THEME                   GOSSIP_PREFS_PATH "/conversation/theme"
#define GOSSIP_PREFS_CHAT_THEME_CHAT_ROOM         GOSSIP_PREFS_PATH "/conversation/theme_chat_room"
#define GOSSIP_PREFS_CHAT_SPELL_CHECKER_LANGUAGES GOSSIP_PREFS_PATH "/conversation/spell_checker_languages"
#define GOSSIP_PREFS_CHAT_SPELL_CHECKER_ENABLED   GOSSIP_PREFS_PATH "/conversation/spell_checker_enabled"
#define GOSSIP_PREFS_UI_SEPARATE_CHAT_WINDOWS     GOSSIP_PREFS_PATH "/ui/separate_chat_windows"
#define GOSSIP_PREFS_UI_MAIN_WINDOW_HIDDEN        GOSSIP_PREFS_PATH "/ui/main_window_hidden"
#define GOSSIP_PREFS_UI_AVATAR_DIRECTORY          GOSSIP_PREFS_PATH "/ui/avatar_directory"
#define GOSSIP_PREFS_UI_SHOW_AVATARS              GOSSIP_PREFS_PATH "/ui/show_avatars"

#define GOSSIP_PREFS_CONTACTS_SHOW_OFFLINE        GOSSIP_PREFS_PATH "/contacts/show_offline"

#define GOSSIP_PREFS_HINTS_CLOSE_MAIN_WINDOW      GOSSIP_PREFS_PATH "/hints/close_main_window"

void gossip_preferences_show               (void);

#endif /* __GOSSIP_PREFERENCES_H__ */


