/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2006 Imendio AB
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

#ifndef __GOSSIP_CHATROOM_H__
#define __GOSSIP_CHATROOM_H__

#include <glib-object.h>

#include "gossip-account.h"
#include "gossip-contact.h"

G_BEGIN_DECLS

typedef enum {
	GOSSIP_CHATROOM_AFFILIATION_OWNER,
	GOSSIP_CHATROOM_AFFILIATION_ADMIN,
	GOSSIP_CHATROOM_AFFILIATION_MEMBER,
	GOSSIP_CHATROOM_AFFILIATION_OUTCAST,
	GOSSIP_CHATROOM_AFFILIATION_NONE
} GossipChatroomAffiliation;

typedef enum {
	GOSSIP_CHATROOM_ROLE_MODERATOR,
	GOSSIP_CHATROOM_ROLE_PARTICIPANT,
	GOSSIP_CHATROOM_ROLE_VISITOR,
	GOSSIP_CHATROOM_ROLE_NONE
} GossipChatroomRole;

typedef enum {
	GOSSIP_CHATROOM_STATUS_INACTIVE,
	GOSSIP_CHATROOM_STATUS_JOINING,
	GOSSIP_CHATROOM_STATUS_ACTIVE,
	GOSSIP_CHATROOM_STATUS_ERROR,
	GOSSIP_CHATROOM_STATUS_UNKNOWN,
} GossipChatroomStatus;

typedef enum {
	GOSSIP_CHATROOM_FEATURE_HIDDEN               = 1 << 0,
	GOSSIP_CHATROOM_FEATURE_MEMBERS_ONLY         = 1 << 1,
	GOSSIP_CHATROOM_FEATURE_MODERATED            = 1 << 2,
	GOSSIP_CHATROOM_FEATURE_NONANONYMOUS         = 1 << 3,
	GOSSIP_CHATROOM_FEATURE_OPEN                 = 1 << 4,
	GOSSIP_CHATROOM_FEATURE_PASSWORD_PROTECTED   = 1 << 5,
	GOSSIP_CHATROOM_FEATURE_PERSISTENT           = 1 << 6,
	GOSSIP_CHATROOM_FEATURE_PUBLIC               = 1 << 7,
	GOSSIP_CHATROOM_FEATURE_SEMIANONYMOUS        = 1 << 8,
	GOSSIP_CHATROOM_FEATURE_TEMPORARY            = 1 << 9,
	GOSSIP_CHATROOM_FEATURE_UNMODERATED          = 1 << 10,
	GOSSIP_CHATROOM_FEATURE_UNSECURED            = 1 << 11
} GossipChatroomFeature;

/*
 * GossipChatroomError
 */ 
#define GOSSIP_TYPE_CHATROOM_ERROR (gossip_chatroom_error_get_type ())

typedef enum {
	GOSSIP_CHATROOM_ERROR_NONE,

	/* MUC errors */
	GOSSIP_CHATROOM_ERROR_PASSWORD_INVALID_OR_MISSING,
	GOSSIP_CHATROOM_ERROR_USER_BANNED,
	GOSSIP_CHATROOM_ERROR_ROOM_NOT_FOUND,
	GOSSIP_CHATROOM_ERROR_ROOM_CREATION_RESTRICTED,
	GOSSIP_CHATROOM_ERROR_USE_RESERVED_ROOM_NICK,
	GOSSIP_CHATROOM_ERROR_NOT_ON_MEMBERS_LIST,
	GOSSIP_CHATROOM_ERROR_NICK_IN_USE,
	GOSSIP_CHATROOM_ERROR_MAXIMUM_USERS_REACHED,
	GOSSIP_CHATROOM_ERROR_UNAUTHORIZED_REQUEST,
	GOSSIP_CHATROOM_ERROR_FORBIDDEN,

	/* Internal errors */
	GOSSIP_CHATROOM_ERROR_ALREADY_OPEN,
	GOSSIP_CHATROOM_ERROR_TIMED_OUT,
	GOSSIP_CHATROOM_ERROR_CANCELED,
	GOSSIP_CHATROOM_ERROR_BAD_REQUEST,
	GOSSIP_CHATROOM_ERROR_UNKNOWN
} GossipChatroomError;

GType gossip_chatroom_error_get_type (void) G_GNUC_CONST;

/*
 * GossipChatroom
 */
#define GOSSIP_TYPE_CHATROOM             (gossip_chatroom_get_type ())
#define GOSSIP_CHATROOM(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CHATROOM, GossipChatroom))
#define GOSSIP_CHATROOM_CLASS(k)         (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_CHATROOM, GossipChatroomClass))
#define GOSSIP_IS_CHATROOM(o)            (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CHATROOM))
#define GOSSIP_IS_CHATROOM_CLASS(k)      (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CHATROOM))
#define GOSSIP_CHATROOM_GET_CLASS(o)     (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CHATROOM, GossipChatroomClass))

typedef struct _GossipChatroom            GossipChatroom;
typedef struct _GossipChatroomClass       GossipChatroomClass;
typedef struct _GossipChatroomContactInfo GossipChatroomContactInfo;
typedef gint                              GossipChatroomId;

/*
 * Privilege  	                        None  	Visitor  Participant  	Moderator

 * Present in Room  	                No  	Yes  	 Yes  		Yes
 * Receive Messages 	                No 	Yes 	 Yes 		Yes
 * Change Availability Status 	        No 	Yes 	 Yes 		Yes
 * Change Room Nickname 	        No 	Yes* 	 Yes 		Yes
 * Send Private Messages 	        No 	Yes* 	 Yes 		Yes
 * Invite Other Users 	                No 	Yes* 	 Yes* 		Yes
 * Send Messages to All 	        No 	No** 	 Yes 		Yes
 * Modify Subject 	                No 	No* 	 Yes* 		Yes
 * Kick Participants and Visitors 	No 	No 	 No 		Yes
 * Grant Voice 	                        No 	No 	 No 		Yes
 * Revoke Voice 	                No 	No 	 No 		Yes***
 *
 * - *   Default; configuration settings MAY modify this privilege.
 * - **  An implementation MAY grant voice by default to visitors in unmoderated rooms.
 * - *** A moderator MUST NOT be able to revoke voice privileges from an admin or owner.
 */

struct _GossipChatroom {
	GObject      parent;
};

struct _GossipChatroomClass {
	GObjectClass parent_class;
};

struct _GossipChatroomContactInfo {
	GossipChatroomRole        role;
	GossipChatroomAffiliation affiliation;
};

/* Chatroom */
GType          gossip_chatroom_get_type                (void) G_GNUC_CONST;
GossipChatroom *
               gossip_chatroom_new                     (GossipAccount             *account,
							const gchar               *server,
							const gchar               *room);
GossipAccount * 
               gossip_chatroom_get_account             (GossipChatroom            *chatroom);
GossipChatroomId 
               gossip_chatroom_get_id                  (GossipChatroom            *chatroom);
const gchar *  gossip_chatroom_get_id_str              (GossipChatroom            *chatroom);
const gchar *  gossip_chatroom_get_name                (GossipChatroom            *chatroom);
const gchar *  gossip_chatroom_get_description         (GossipChatroom            *chatroom);
const gchar *  gossip_chatroom_get_subject             (GossipChatroom            *chatroom);
const gchar *  gossip_chatroom_get_nick                (GossipChatroom            *chatroom);
const gchar *  gossip_chatroom_get_server              (GossipChatroom            *chatroom);
const gchar *  gossip_chatroom_get_room                (GossipChatroom            *chatroom);
const gchar *  gossip_chatroom_get_password            (GossipChatroom            *chatroom);
gboolean       gossip_chatroom_get_auto_connect        (GossipChatroom            *chatroom);
gboolean       gossip_chatroom_get_favorite            (GossipChatroom            *chatroom);

GossipChatroomFeature
               gossip_chatroom_get_features            (GossipChatroom            *chatroom);
GossipChatroomStatus
               gossip_chatroom_get_status              (GossipChatroom            *chatroom);
guint          gossip_chatroom_get_occupants           (GossipChatroom            *chatroom);
GossipChatroomError
               gossip_chatroom_get_last_error          (GossipChatroom            *chatroom);
GossipChatroomContactInfo *
               gossip_chatroom_get_contact_info        (GossipChatroom            *chatroom,
							GossipContact             *contact);
GList *        gossip_chatroom_get_contacts            (GossipChatroom            *chatroom);

GossipContact *gossip_chatroom_get_own_contact         (GossipChatroom            *chatroom);
const gchar *  gossip_chatroom_get_own_contact_id_str  (GossipChatroom            *chatroom);
void           gossip_chatroom_set_account             (GossipChatroom            *chatroom,
							GossipAccount             *account);
void           gossip_chatroom_set_name                (GossipChatroom            *chatroom,
							const gchar               *name);
void           gossip_chatroom_set_description         (GossipChatroom            *chatroom,
							const gchar               *description);
void           gossip_chatroom_set_subject             (GossipChatroom            *chatroom,
							const gchar               *subject);
void           gossip_chatroom_set_nick                (GossipChatroom            *chatroom,
							const gchar               *nick);
void           gossip_chatroom_set_server              (GossipChatroom            *chatroom,
							const gchar               *server);
void           gossip_chatroom_set_room                (GossipChatroom            *chatroom,
							const gchar               *room);
void           gossip_chatroom_set_password            (GossipChatroom            *chatroom,
							const gchar               *password);
void           gossip_chatroom_set_auto_connect        (GossipChatroom            *chatroom,
							gboolean                   auto_connect);
void           gossip_chatroom_set_favorite            (GossipChatroom            *chatroom,
							gboolean                   favorite);
void           gossip_chatroom_set_features            (GossipChatroom            *chatroom,
							GossipChatroomFeature      features);
void           gossip_chatroom_set_status              (GossipChatroom            *chatroom,
							GossipChatroomStatus       status);
void           gossip_chatroom_set_occupants           (GossipChatroom            *chatroom,
							guint                      occupants);
void           gossip_chatroom_set_last_error          (GossipChatroom            *chatroom,
							GossipChatroomError        last_error);
void           gossip_chatroom_set_contact_info        (GossipChatroom            *chatroom,
							GossipContact             *contact,
							GossipChatroomContactInfo *info);
void           gossip_chatroom_set_own_contact         (GossipChatroom            *chatroom,
							GossipContact             *contact);

/* Utils */
guint          gossip_chatroom_hash                    (gconstpointer              key);
gboolean       gossip_chatroom_equal                   (gconstpointer              v1,
							gconstpointer              v2);
gboolean       gossip_chatroom_equal_full              (gconstpointer              v1,
							gconstpointer              v2);
const gchar *  gossip_chatroom_status_to_string        (GossipChatroomStatus       status);
const gchar *  gossip_chatroom_error_to_string         (GossipChatroomError        error);
const gchar *  gossip_chatroom_role_to_string          (GossipChatroomRole         role,
							gint                       nr);
const gchar *  gossip_chatroom_affiliation_to_string   (GossipChatroomAffiliation  affiliation,
							gint                       nr);

/* Actions */
void           gossip_chatroom_contact_joined          (GossipChatroom            *chatroom,
							GossipContact             *contact,
							GossipChatroomContactInfo *info);
void           gossip_chatroom_contact_left            (GossipChatroom            *chatroom,
							GossipContact             *contact);

/* Privileges */
gboolean       gossip_chatroom_contact_can_message_all (GossipChatroom            *chatroom,
							GossipContact             *contact);
gboolean       gossip_chatroom_contact_can_change_subject 
                                                        (GossipChatroom           *chatroom,
							 GossipContact            *contact);
gboolean       gossip_chatroom_contact_can_kick         (GossipChatroom           *chatroom,
							 GossipContact            *contact);
gboolean       gossip_chatroom_contact_can_change_role  (GossipChatroom           *chatroom,
							 GossipContact            *contact);

G_BEGIN_DECLS

#endif /* __GOSSIP_CHATROOM_H__ */
