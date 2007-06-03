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
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include "gossip-ft.h"
#include "gossip-debug.h"

#define DEBUG_DOMAIN "GossipFT"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_FT, GossipFTPriv))

typedef struct _GossipFTPriv GossipFTPriv;

struct _GossipFTPriv {
	GossipFTId     id;

	GossipFTType   type;

	GossipContact *contact;

	gchar         *file_name;
	guint          file_size;
	gchar         *file_mime_type;
	gchar         *sid;
	gchar         *location;
};

static void ft_class_init   (GossipFTClass *class);
static void ft_init         (GossipFT      *ft);
static void ft_finalize     (GObject       *object);
static void ft_get_property (GObject       *object,
			     guint          param_id,
			     GValue        *value,
			     GParamSpec    *pspec);
static void ft_set_property (GObject       *object,
			     guint          param_id,
			     const GValue  *value,
			     GParamSpec    *pspec);

enum {
	PROP_0,
	PROP_ID,
	PROP_TYPE,
	PROP_CONTACT,
	PROP_FILE_NAME,
	PROP_FILE_SIZE,
	PROP_FILE_MIME_TYPE,
	PROP_SID,
	PROP_LOCATION
};

static gpointer parent_class = NULL;

GType
gossip_ft_get_gtype (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GossipFTClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ft_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GossipFT),
			0,    /* n_preallocs */
			(GInstanceInitFunc) ft_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "GossipFT",
					       &info, 0);
	}

	return type;

}

static void
ft_class_init (GossipFTClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	object_class->finalize     = ft_finalize;
	object_class->get_property = ft_get_property;
	object_class->set_property = ft_set_property;

	g_object_class_install_property (object_class,
					 PROP_ID,
					 g_param_spec_int ("id",
							   "Transaction ID",
							   "The file transfer transaction id",
							   0,
							   G_MAXINT,
							   0,
							   G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_TYPE,
					 g_param_spec_int ("type",
							   "Transaction Type",
							   "The type of transaction (send or receive)",
							   GOSSIP_FT_TYPE_RECEIVING,
							   GOSSIP_FT_TYPE_SENDING,
							   GOSSIP_FT_TYPE_RECEIVING,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_CONTACT,
					 g_param_spec_object ("contact",
							      "Transaction Contact",
							      "The sender or receiver of the file transfer",
							      GOSSIP_TYPE_CONTACT,
							      G_PARAM_READWRITE));


	g_object_class_install_property (object_class,
					 PROP_FILE_NAME,
					 g_param_spec_string ("file-name",
							      "File Name",
							      "The file being transferred",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_FILE_SIZE,
					 g_param_spec_uint64 ("file-size",
							      "File Size",
							      "The file size in bytes",
							      0,
							      G_MAXUINT64,
							      0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_FILE_MIME_TYPE,
					 g_param_spec_string ("file-mime-type",
							      "File Mime Type",
							      "The mime type (for example text/plain)",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SID,
					 g_param_spec_string ("sid",
							      "Session ID",
							      "ID to match stanzas assosiated to this file",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SID,
					 g_param_spec_string ("location",
							      "Location",
							      "Where to store the file on the file system (URI)",
							      NULL,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GossipFTPriv));
}

static void
ft_init (GossipFT *ft)
{
	static GossipFTId  id = 1;
	GossipFTPriv      *priv;

	priv = GET_PRIV (ft);

	priv->id             = id++;

	priv->type           = GOSSIP_FT_TYPE_RECEIVING;

	priv->contact        = NULL;

	priv->file_name      = NULL;
	priv->file_size      = 0;
	priv->file_mime_type = NULL;

	priv->sid            = NULL;
	priv->location       = NULL;

	gossip_debug (DEBUG_DOMAIN, "Initializing GossipFT with id:%d", id);
}

static void
ft_finalize (GObject *object)
{
	GossipFTPriv *priv;

	priv = GET_PRIV (object);

	gossip_debug (DEBUG_DOMAIN, "Finalizing GossipFT with id:%d", priv->id);

	if (priv->contact) {
		g_object_unref (priv->contact);
	}

	g_free (priv->file_name);
	g_free (priv->file_mime_type);
	g_free (priv->sid);
	g_free (priv->location);

	(G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
ft_get_property (GObject    *object,
		      guint       param_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	GossipFTPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ID:
		g_value_set_int (value, priv->id);
		break;
	case PROP_TYPE:
		g_value_set_int (value, priv->type);
		break;
	case PROP_CONTACT:
		g_value_set_object (value, priv->contact);
		break;
	case PROP_FILE_NAME:
		g_value_set_string (value, priv->file_name);
		break;
	case PROP_FILE_SIZE:
		g_value_set_int (value, priv->file_size);
		break;
	case PROP_FILE_MIME_TYPE:
		g_value_set_string (value, priv->file_mime_type);
		break;
	case PROP_SID:
		g_value_set_string (value, priv->sid);
		break;
	case PROP_LOCATION:
		g_value_set_string (value, priv->location);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
ft_set_property (GObject      *object,
		 guint         param_id,
		 const GValue *value,
		 GParamSpec   *pspec)
{
	GossipFTPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_TYPE:
		gossip_ft_set_type (GOSSIP_FT (object),
				    g_value_get_int (value));
		break;
	case PROP_CONTACT:
		gossip_ft_set_contact (GOSSIP_FT (object),
				       GOSSIP_CONTACT (g_value_get_object (value)));
		break;
	case PROP_FILE_NAME:
		gossip_ft_set_file_name (GOSSIP_FT (object),
					 g_value_get_string (value));
		break;
	case PROP_FILE_SIZE:
		gossip_ft_set_file_size (GOSSIP_FT (object),
					 g_value_get_uint64 (value));
		break;
	case PROP_FILE_MIME_TYPE:
		gossip_ft_set_file_mime_type (GOSSIP_FT (object),
					      g_value_get_string (value));
		break;
	case PROP_SID:
		gossip_ft_set_sid (GOSSIP_FT (object),
				   g_value_get_string (value));
		break;
	case PROP_LOCATION:
		gossip_ft_set_location (GOSSIP_FT (object),
					g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

GossipFT *
gossip_ft_new (void)
{
	return g_object_new (GOSSIP_TYPE_FT, NULL);
}

GossipFTId
gossip_ft_get_id (GossipFT *ft)
{
	GossipFTPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_FT (ft), 0);

	priv = GET_PRIV (ft);

	return priv->id;
}

GossipFTType
gossip_ft_get_type (GossipFT *ft)
{
	GossipFTPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_FT (ft), 0);

	priv = GET_PRIV (ft);

	return priv->type;
}

void
gossip_ft_set_type (GossipFT     *ft,
		    GossipFTType  type)
{
	GossipFTPriv *priv;

	g_return_if_fail (GOSSIP_IS_FT (ft));
	g_return_if_fail (type >= GOSSIP_FT_TYPE_RECEIVING ||
			  type <= GOSSIP_FT_TYPE_SENDING);

	priv = GET_PRIV (ft);

	priv->type = type;
}

GossipContact *
gossip_ft_get_contact (GossipFT *ft)
{
	GossipFTPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_FT (ft), NULL);

	priv = GET_PRIV (ft);

	return priv->contact;
}

void
gossip_ft_set_contact (GossipFT      *ft,
		       GossipContact *contact)
{
	GossipFTPriv *priv;

	g_return_if_fail (GOSSIP_IS_FT (ft));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (ft);

	if (priv->contact) {
		g_object_unref (priv->contact);
	}

	priv->contact = g_object_ref (contact);
}

const gchar *
gossip_ft_get_file_name (GossipFT *ft)
{
	GossipFTPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_FT (ft), NULL);

	priv = GET_PRIV (ft);

	return priv->file_name;
}

void
gossip_ft_set_file_name (GossipFT    *ft,
			 const gchar *file_name)
{
	GossipFTPriv *priv;

	g_return_if_fail (GOSSIP_IS_FT (ft));

	priv = GET_PRIV (ft);

	g_free (priv->file_name);
	priv->file_name = g_strdup (file_name);
}

guint64
gossip_ft_get_file_size (GossipFT *ft)
{
	GossipFTPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_FT (ft), 0);

	priv = GET_PRIV (ft);

	return priv->file_size;
}

void
gossip_ft_set_file_size (GossipFT *ft,
			 guint64   file_size)
{
	GossipFTPriv *priv;

	g_return_if_fail (GOSSIP_IS_FT (ft));

	priv = GET_PRIV (ft);

	priv->file_size = file_size;
}

const gchar *
gossip_ft_get_file_mime_type (GossipFT *ft)
{
	GossipFTPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_FT (ft), NULL);

	priv = GET_PRIV (ft);

	return priv->file_mime_type;
}

void
gossip_ft_set_file_mime_type (GossipFT    *ft,
			      const gchar *file_mime_type)
{
	GossipFTPriv *priv;

	g_return_if_fail (GOSSIP_IS_FT (ft));

	priv = GET_PRIV (ft);

	g_free (priv->file_mime_type);
	priv->file_mime_type = g_strdup (file_mime_type);
}

const gchar *
gossip_ft_get_sid (GossipFT *ft)
{
	GossipFTPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_FT (ft), NULL);

	priv = GET_PRIV (ft);

	return priv->sid;
}

void
gossip_ft_set_sid (GossipFT    *ft,
		   const gchar *sid)
{
	GossipFTPriv *priv;

	g_return_if_fail (GOSSIP_IS_FT (ft));

	priv = GET_PRIV (ft);

	g_free (priv->sid);
	priv->sid = g_strdup (sid);
}

const gchar *
gossip_ft_get_location (GossipFT *ft)
{
	GossipFTPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_FT (ft), NULL);

	priv = GET_PRIV (ft);

	return priv->location;
}

void
gossip_ft_set_location (GossipFT    *ft,
			const gchar *location)
{
	GossipFTPriv *priv;

	g_return_if_fail (GOSSIP_IS_FT (ft));

	priv = GET_PRIV (ft);

	g_free (priv->location);
	priv->location = g_strdup (location);
}

gboolean
gossip_ft_equal (gconstpointer a,
		 gconstpointer b)
{
	GossipFTPriv *priv1;
	GossipFTPriv *priv2;

	g_return_val_if_fail (GOSSIP_IS_FT (a), FALSE);
	g_return_val_if_fail (GOSSIP_IS_FT (b), FALSE);

	priv1 = GET_PRIV (a);
	priv2 = GET_PRIV (b);

	if (priv1->id == priv2->id) {
		return TRUE;
	}

	return FALSE;
}


