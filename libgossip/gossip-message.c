/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
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
 * Authors: Mikael Hallendal <micke@imendio.com>
 */

#include "config.h"

#include "gossip-types.h"

#include "gossip-chatroom.h"
#include "gossip-contact.h"
#include "gossip-message.h"
#include "gossip-time.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_MESSAGE, GossipMessagePriv))

typedef struct _GossipMessagePriv GossipMessagePriv;

struct _GossipMessagePriv {
	GossipContact        *recipient;
	gchar                *resource;

	GossipContact        *sender;

	gchar                *subject;
	gchar                *body;
	gchar                *thread;

	GossipTime            timestamp;

	GossipChatroomInvite *invite;

	gboolean              request_composing;

	GossipMessageType     type;
};

static void gossip_message_class_init (GossipMessageClass *class);
static void gossip_message_init       (GossipMessage      *message);
static void gossip_message_finalize   (GObject            *object);
static void message_get_property      (GObject            *object,
				       guint               param_id,
				       GValue             *value,
				       GParamSpec         *pspec);
static void message_set_property      (GObject            *object,
				       guint               param_id,
				       const GValue       *value,
				       GParamSpec         *pspec);

enum {
	PROP_0,
	PROP_RECIPIENT,
	PROP_SENDER,
	PROP_TYPE,
	PROP_RESOURCE,
	PROP_SUBJECT,
	PROP_BODY,
	PROP_THREAD,
	PROP_TIMESTAMP,
	PROP_REQUEST_COMPOSING
};

static gpointer parent_class = NULL;

GType
gossip_message_get_gtype (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GossipMessageClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) gossip_message_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GossipMessage),
			0,    /* n_preallocs */
			(GInstanceInitFunc) gossip_message_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "GossipMessage",
					       &info, 0);
	}

	return type;
}

static void
gossip_message_class_init (GossipMessageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	object_class->finalize     = gossip_message_finalize;
	object_class->get_property = message_get_property;
	object_class->set_property = message_set_property;

	g_object_class_install_property (object_class,
					 PROP_RECIPIENT,
					 g_param_spec_object ("recipient",
							      "Message Recipient",
							      "The recipient of the message",
							      GOSSIP_TYPE_CONTACT,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SENDER,
					 g_param_spec_object ("sender",
							      "Message Sender",
							      "The sender of the message",
							      GOSSIP_TYPE_CONTACT,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_TYPE,
					 g_param_spec_int ("type",
							   "Message Type",
							   "The type of message",
							   GOSSIP_MESSAGE_TYPE_NORMAL,
							   GOSSIP_MESSAGE_TYPE_HEADLINE,
							   GOSSIP_MESSAGE_TYPE_NORMAL,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_RESOURCE,
					 g_param_spec_string ("resource",
							      "Resource",
							      "When sending message this is the recipients resource and when receiving it's the senders resource",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_SUBJECT,
					 g_param_spec_string ("subject",
							      "Subject",
							      "The message subject",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_BODY,
					 g_param_spec_string ("body",
							      "Message Body",
							      "The content of the message",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_THREAD,
					 g_param_spec_string ("thread",
							      "Message Thread",
							      "The message thread",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_TIMESTAMP,
					 g_param_spec_long ("timestamp",
							    "timestamp",
							    "timestamp",
							    -1,
							    G_MAXLONG,
							    -1,
							    G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_REQUEST_COMPOSING,
					 g_param_spec_boolean ("request-composing",
							       "Request Composing",
							       "This indicates that sender wants composing events",
							       FALSE,
							       G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GossipMessagePriv));

}

static void
gossip_message_init (GossipMessage *message)
{
	GossipMessagePriv *priv;

	priv = GET_PRIV (message);

	priv->recipient = NULL;
	priv->sender = NULL;
	priv->resource = NULL;

	priv->subject = NULL;
	priv->body = NULL;
	priv->thread = NULL;

	priv->timestamp = gossip_time_get_current ();

	priv->request_composing = FALSE;

	priv->invite = NULL;
}

static void
gossip_message_finalize (GObject *object)
{
	GossipMessagePriv *priv;

	priv = GET_PRIV (object);

	if (priv->recipient) {
		g_object_unref (priv->recipient);
	}

	if (priv->sender) {
		g_object_unref (priv->sender);
	}

	g_free (priv->resource);

	g_free (priv->subject);
	g_free (priv->body);
	g_free (priv->thread);

	if (priv->invite) {
		gossip_chatroom_invite_unref (priv->invite);
	}

	(G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
message_get_property (GObject    *object,
		      guint       param_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	GossipMessagePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_RECIPIENT:
		g_value_set_object (value, priv->recipient);
		break;
	case PROP_SENDER:
		g_value_set_object (value, priv->sender);
		break;
	case PROP_TYPE:
		g_value_set_int (value, priv->type);
		break;
	case PROP_RESOURCE:
		g_value_set_string (value, priv->resource);
		break;
	case PROP_SUBJECT:
		g_value_set_string (value, priv->subject);
		break;
	case PROP_BODY:
		g_value_set_string (value, priv->body);
		break;
	case PROP_THREAD:
		g_value_set_string (value, priv->thread);
		break;
	case PROP_REQUEST_COMPOSING:
		g_value_set_boolean (value, priv->request_composing);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
message_set_property (GObject      *object,
		      guint         param_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	GossipMessagePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_RECIPIENT:
		gossip_message_set_recipient (GOSSIP_MESSAGE (object),
					      GOSSIP_CONTACT (g_value_get_object (value)));
		break;
	case PROP_SENDER:
		gossip_message_set_sender (GOSSIP_MESSAGE (object),
					   GOSSIP_CONTACT (g_value_get_object (value)));
		break;
	case PROP_TYPE:
		priv->type = g_value_get_int (value);
		break;
	case PROP_RESOURCE:
		gossip_message_set_explicit_resource (GOSSIP_MESSAGE (object),
						      g_value_get_string (value));
		break;
	case PROP_SUBJECT:
		gossip_message_set_subject (GOSSIP_MESSAGE (object),
					    g_value_get_string (value));
		break;
	case PROP_BODY:
		gossip_message_set_body (GOSSIP_MESSAGE (object),
					 g_value_get_string (value));
		break;
	case PROP_THREAD:
		gossip_message_set_thread (GOSSIP_MESSAGE (object),
					   g_value_get_string (value));
		break;
	case PROP_REQUEST_COMPOSING:
		priv->request_composing = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

GossipMessage *
gossip_message_new (GossipMessageType  type,
		    GossipContact     *recipient)
{
	return g_object_new (GOSSIP_TYPE_MESSAGE,
			     "type", type,
			     "recipient", recipient,
			     NULL);
}

GossipMessageType
gossip_message_get_type (GossipMessage *message)
{
	GossipMessagePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_MESSAGE (message),
			      GOSSIP_MESSAGE_TYPE_NORMAL);

	priv = GET_PRIV (message);

	return priv->type;
}

GossipContact *
gossip_message_get_recipient (GossipMessage *message)
{
	GossipMessagePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_MESSAGE (message), NULL);

	priv = GET_PRIV (message);

	return priv->recipient;
}

void
gossip_message_set_recipient (GossipMessage *message, GossipContact *contact)
{
	GossipMessagePriv *priv;
	GossipContact     *old_recipient;

	g_return_if_fail (GOSSIP_IS_MESSAGE (message));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (message);

	old_recipient = priv->recipient;
	priv->recipient = g_object_ref (contact);

	if (old_recipient) {
		g_object_unref (old_recipient);
	}

	g_object_notify (G_OBJECT (message), "recipient");
}

const gchar *
gossip_message_get_explicit_resource (GossipMessage *message)
{
	GossipMessagePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_MESSAGE (message), "");

	priv = GET_PRIV (message);

	return priv->resource;
}

void
gossip_message_set_explicit_resource (GossipMessage *message,
				      const gchar   *resource)
{
	GossipMessagePriv *priv;

	g_return_if_fail (GOSSIP_IS_MESSAGE (message));

	priv = GET_PRIV (message);

	g_free (priv->resource);

	if (resource) {
		priv->resource = g_strdup (resource);
	} else {
		priv->resource = g_strdup ("");
	}

	g_object_notify (G_OBJECT (message), "resource");
}

GossipContact *
gossip_message_get_sender (GossipMessage *message)
{
	GossipMessagePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_MESSAGE (message), NULL);

	priv = GET_PRIV (message);

	return priv->sender;
}

void
gossip_message_set_sender (GossipMessage *message, GossipContact *contact)
{
	GossipMessagePriv *priv;
	GossipContact     *old_sender;

	g_return_if_fail (GOSSIP_IS_MESSAGE (message));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (message);

	old_sender = priv->sender;
	priv->sender = g_object_ref (contact);

	if (old_sender) {
		g_object_unref (old_sender);
	}

	g_object_notify (G_OBJECT (message), "sender");
}

const gchar *
gossip_message_get_subject (GossipMessage *message)
{
	GossipMessagePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_MESSAGE (message), NULL);

	priv = GET_PRIV (message);

	return priv->subject;
}

void
gossip_message_set_subject (GossipMessage *message,
			    const gchar   *subject)
{
	GossipMessagePriv *priv;

	g_return_if_fail (GOSSIP_IS_MESSAGE (message));

	priv = GET_PRIV (message);

	g_free (priv->subject);

	if (subject) {
		priv->subject = g_strdup (subject);
	} else {
		priv->subject = NULL;
	}

	g_object_notify (G_OBJECT (message), "subject");
}

const gchar *
gossip_message_get_body (GossipMessage *message)
{
	GossipMessagePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_MESSAGE (message), NULL);

	priv = GET_PRIV (message);

	return priv->body;
}

void
gossip_message_set_body (GossipMessage *message,
			 const gchar   *body)
{
	GossipMessagePriv *priv;

	g_return_if_fail (GOSSIP_IS_MESSAGE (message));

	priv = GET_PRIV (message);

	g_free (priv->body);

	if (body) {
		priv->body = g_strdup (body);
	} else {
		priv->body = NULL;
	}

	g_object_notify (G_OBJECT (message), "body");
}

const gchar *
gossip_message_get_thread (GossipMessage *message)
{
	GossipMessagePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_MESSAGE (message), NULL);

	priv = GET_PRIV (message);

	return priv->thread;
}

void
gossip_message_set_thread (GossipMessage *message,
			   const gchar   *thread)
{
	GossipMessagePriv *priv;

	g_return_if_fail (GOSSIP_IS_MESSAGE (message));

	priv = GET_PRIV (message);

	g_free (priv->thread);

	if (thread) {
		priv->thread = g_strdup (thread);
	} else {
		priv->thread = NULL;
	}

	g_object_notify (G_OBJECT (message), "thread");
}

GossipTime
gossip_message_get_timestamp (GossipMessage *message)
{
	GossipMessagePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_MESSAGE (message), -1);

	priv = GET_PRIV (message);

	return priv->timestamp;
}

void
gossip_message_set_timestamp (GossipMessage *message,
			      GossipTime     timestamp)
{
	GossipMessagePriv *priv;

	g_return_if_fail (GOSSIP_IS_MESSAGE (message));
	g_return_if_fail (timestamp >= -1);

	priv = GET_PRIV (message);

	if (timestamp <= 0) {
		priv->timestamp = gossip_time_get_current ();
	} else {
		priv->timestamp = timestamp;
	}

	g_object_notify (G_OBJECT (message), "timestamp");
}

GossipChatroomInvite *
gossip_message_get_invite (GossipMessage *message)
{
	GossipMessagePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_MESSAGE (message), NULL);

	priv = GET_PRIV (message);

	return priv->invite;
}

void
gossip_message_set_invite (GossipMessage        *message,
			   GossipChatroomInvite *invite)
{
	GossipMessagePriv    *priv;
	GossipChatroomInvite *old_invite;

	g_return_if_fail (GOSSIP_IS_MESSAGE (message));

	priv = GET_PRIV (message);

	old_invite = priv->invite;

	if (invite) {
		priv->invite = gossip_chatroom_invite_ref (invite);
	} else {
		priv->invite = NULL;
	}

	if (old_invite) {
		gossip_chatroom_invite_unref (old_invite);
	}

	g_object_notify (G_OBJECT (message), "invite");
}

void
gossip_message_request_composing (GossipMessage *message)
{
	GossipMessagePriv *priv;

	g_return_if_fail (GOSSIP_IS_MESSAGE (message));

	priv = GET_PRIV (message);

	priv->request_composing = TRUE;

	g_object_notify (G_OBJECT (message), "request_composing");
}

gboolean
gossip_message_is_requesting_composing (GossipMessage *message)
{
	GossipMessagePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_MESSAGE (message), FALSE);

	priv = GET_PRIV (message);

	return priv->request_composing;
}

