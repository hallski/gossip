/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
 * 
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#ifndef __GOSSIP_TYPES_H__
#define __GOSSIP_TYPES_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS 

/*
 * Structures
 */
typedef struct _GossipAccount             GossipAccount;
typedef struct _GossipAccountManager      GossipAccountManager;
typedef struct _GossipAccountParam        GossipAccountParam;
typedef struct _GossipAvatar              GossipAvatar;
typedef struct _GossipChatroom            GossipChatroom;
typedef struct _GossipChatroomContactInfo GossipChatroomContactInfo;
typedef struct _GossipChatroomInvite      GossipChatroomInvite;
typedef struct _GossipChatroomManager     GossipChatroomManager;
typedef struct _GossipConf                GossipConf;
typedef struct _GossipContact             GossipContact;
typedef struct _GossipEvent               GossipEvent;
typedef struct _GossipEventManager        GossipEventManager;
typedef struct _GossipFTProvider          GossipFTProvider;
typedef struct _GossipLogManager          GossipLogManager;
typedef struct _GossipLogSearchHit        GossipLogSearchHit;
typedef struct _GossipMessage             GossipMessage;
typedef struct _GossipSession             GossipSession;
typedef struct _GossipVCard               GossipVCard;
typedef struct _GossipVersionInfo         GossipVersionInfo;
typedef struct _GossipPresence            GossipPresence;

/* Commonly used */
typedef struct {
	gpointer callback;
	gpointer user_data;
	gpointer data1;
	gpointer data2;
	gpointer data3;
} GossipCallbackData;


/*
 * Data types
 */
typedef gint GossipChatroomId;
typedef gint GossipEventId;
typedef long GossipTime;         /* Note: Always in UTC. */;


/*
 * Enumerations 
 */
typedef enum {
	GOSSIP_ACCOUNT_PARAM_FLAG_REQUIRED    = 1 << 0,
	GOSSIP_ACCOUNT_PARAM_FLAG_REGISTER    = 1 << 1,
	GOSSIP_ACCOUNT_PARAM_FLAG_HAS_DEFAULT = 1 << 2,
	GOSSIP_ACCOUNT_PARAM_FLAG_ALL         = (1 << 3) - 1
} GossipAccountParamFlags;

typedef enum {
	GOSSIP_ACCOUNT_TYPE_JABBER,
	GOSSIP_ACCOUNT_TYPE_AIM,
	GOSSIP_ACCOUNT_TYPE_ICQ,
	GOSSIP_ACCOUNT_TYPE_MSN,
	GOSSIP_ACCOUNT_TYPE_YAHOO,
	GOSSIP_ACCOUNT_TYPE_UNKNOWN,
	GOSSIP_ACCOUNT_TYPE_COUNT
} GossipAccountType;

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
	GOSSIP_CHATROOM_TYPE_NORMAL,
} GossipChatroomType;

typedef enum {
	GOSSIP_CONTACT_TYPE_TEMPORARY,
	GOSSIP_CONTACT_TYPE_CONTACTLIST,
	GOSSIP_CONTACT_TYPE_CHATROOM,
	GOSSIP_CONTACT_TYPE_USER           /* Represents the own user */
} GossipContactType;

typedef enum {
	GOSSIP_EVENT_NEW_MESSAGE,
	GOSSIP_EVENT_SUBSCRIPTION_REQUEST,
	GOSSIP_EVENT_SERVER_MESSAGE,
	GOSSIP_EVENT_FILE_TRANSFER_REQUEST,
	GOSSIP_EVENT_USER_ONLINE,
	GOSSIP_EVENT_USER_OFFLINE,
	GOSSIP_EVENT_ERROR
} GossipEventType;

typedef enum {
	GOSSIP_MESSAGE_TYPE_NORMAL,
	GOSSIP_MESSAGE_TYPE_CHAT_ROOM,
	GOSSIP_MESSAGE_TYPE_HEADLINE
} GossipMessageType;

typedef enum {
	GOSSIP_PRESENCE_STATE_AVAILABLE,
	GOSSIP_PRESENCE_STATE_BUSY,
	GOSSIP_PRESENCE_STATE_AWAY,
	GOSSIP_PRESENCE_STATE_EXT_AWAY,
	GOSSIP_PRESENCE_STATE_HIDDEN,      /* When you appear offline to others */
	GOSSIP_PRESENCE_STATE_UNAVAILABLE,
} GossipPresenceState;

typedef enum {
	GOSSIP_REGEX_AS_IS,
	GOSSIP_REGEX_BROWSER,
	GOSSIP_REGEX_EMAIL,
	GOSSIP_REGEX_OTHER,
	GOSSIP_REGEX_ALL,
} GossipRegExType;

typedef enum {
	GOSSIP_RESULT_OK,
	GOSSIP_RESULT_ERROR_INVALID_REPLY,
	GOSSIP_RESULT_ERROR_TIMEOUT,
	GOSSIP_RESULT_ERROR_FAILED,
	GOSSIP_RESULT_ERROR_UNAVAILABLE
} GossipResult;

typedef enum {
	GOSSIP_SUBSCRIPTION_NONE,
	GOSSIP_SUBSCRIPTION_TO,
	GOSSIP_SUBSCRIPTION_FROM,
	GOSSIP_SUBSCRIPTION_BOTH
} GossipSubscription;


/*
 * Function definitions.
 */
typedef void (*GossipAccountParamFunc) (GossipAccount      *account,
					const gchar        *param_name,
					GossipAccountParam *param,
					gpointer            user_data);

typedef void (* GossipEventActivateFunction) (GossipEventManager *manager,
					      GossipEvent        *event,
					      GObject            *object);


typedef void (*GossipCallback)        (GossipResult       result,
				       gpointer           user_data);

typedef void (*GossipErrorCallback)   (GossipResult       result,
				       GError            *error,
				       gpointer           user_data);

typedef void (*GossipVCardCallback)   (GossipResult       result,
				       GossipVCard       *vcard,
				       gpointer           user_data);

typedef void (*GossipVersionCallback) (GossipResult       result,
				       GossipVersionInfo *info,
				       gpointer           user_data);

typedef void (*GossipConfNotifyFunc) (GossipConf  *conf, 
				      const gchar *key,
				      gpointer     user_data);

typedef void (* GossipLogMessageFunc)  (GossipContact  *own_contact,
					GossipMessage  *message,
					gpointer        user_data);

G_END_DECLS 

#endif /* __GOSSIP_TYPES_H__ */
