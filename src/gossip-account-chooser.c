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

#include <config.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libgossip/gossip-protocol.h>

#include "gossip-ui-utils.h"
#include "gossip-account-chooser.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_ACCOUNT_CHOOSER, GossipAccountChooserPriv))

typedef struct {
	GossipSession        *session;
	GossipAccountManager *account_manager;

	gboolean              set_active_item;
	gboolean              can_select_all;
} GossipAccountChooserPriv;

struct SetAccountData {
	GossipAccountChooser *account_chooser;
	GossipAccount        *account;
	gboolean              set;
};

enum {
	COL_ACCOUNT_IMAGE,
	COL_ACCOUNT_TEXT,
	COL_ACCOUNT_ENABLED, /* usually tied to connected state */
	COL_ACCOUNT_POINTER,
	COL_ACCOUNT_COUNT
};

static void     account_chooser_finalize                 (GObject               *object);
static void     account_chooser_setup                    (GossipAccountChooser  *account_chooser);
static void     account_chooser_account_added_cb         (GossipAccountManager  *manager,
							  GossipAccount         *account,
							  GossipAccountChooser  *account_chooser);
static void     account_chooser_account_add_foreach      (GossipAccount         *account,
							  GossipAccountChooser  *account_chooser);
static void     account_chooser_account_removed_cb       (GossipAccountManager  *manager,
							  GossipAccount         *account,
							  GossipAccountChooser  *account_chooser);
static void     account_chooser_account_remove_foreach   (GossipAccount         *account,
							  GossipAccountChooser  *account_chooser);
static gboolean account_chooser_account_name_foreach     (GtkTreeModel          *model,
							  GtkTreePath           *path,
							  GtkTreeIter           *iter,
							  struct SetAccountData *data);
static void     account_chooser_account_name_changed_cb  (GossipAccount         *account,
							  GParamSpec            *param,
							  GossipAccountChooser  *account_chooser);
static gboolean account_chooser_account_update_foreach   (GtkTreeModel          *model,
							  GtkTreePath           *path,
							  GtkTreeIter           *iter,
							  struct SetAccountData *data);
static void     account_chooser_protocol_connected_cb    (GossipSession         *session,
							  GossipAccount         *account,
							  GossipProtocol        *protocol,
							  GossipAccountChooser  *account_chooser);
static void     account_chooser_protocol_disconnected_cb (GossipSession         *session,
							  GossipAccount         *account,
							  GossipProtocol        *protocol,
							  gint                   reason,
							  GossipAccountChooser  *account_chooser);
static gboolean account_chooser_set_account_foreach      (GtkTreeModel          *model,
							  GtkTreePath           *path,
							  GtkTreeIter           *iter,
							  struct SetAccountData *data);
static gboolean account_chooser_set_enabled_foreach      (GtkTreeModel         *model,
							  GtkTreePath          *path,
							  GtkTreeIter          *iter,
							  GossipAccountChooser *account_chooser);

G_DEFINE_TYPE (GossipAccountChooser, gossip_account_chooser, GTK_TYPE_COMBO_BOX);

static void
gossip_account_chooser_class_init (GossipAccountChooserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = account_chooser_finalize;

	g_type_class_add_private (object_class, sizeof (GossipAccountChooserPriv));
}

static void
gossip_account_chooser_init (GossipAccountChooser *account_chooser)
{
}

static void
account_chooser_finalize (GObject *object)
{
	GossipAccountChooser     *account_chooser;
	GossipAccountChooserPriv *priv;
	GList                    *accounts;

	account_chooser = GOSSIP_ACCOUNT_CHOOSER (object);

	priv = GET_PRIV (object);

	accounts = gossip_account_manager_get_accounts (priv->account_manager);
	g_list_foreach (accounts, (GFunc)account_chooser_account_remove_foreach, account_chooser);
	g_list_foreach (accounts, (GFunc)g_object_unref, NULL);
	g_list_free (accounts);

	g_signal_handlers_disconnect_by_func (priv->account_manager,
					      account_chooser_account_added_cb,
					      account_chooser);
	g_signal_handlers_disconnect_by_func (priv->account_manager,
					      account_chooser_account_removed_cb,
					      account_chooser);

	g_signal_handlers_disconnect_by_func (priv->session,
					      account_chooser_protocol_connected_cb,
					      account_chooser);
	g_signal_handlers_disconnect_by_func (priv->session,
					      account_chooser_protocol_disconnected_cb,
					      account_chooser);

	g_object_unref (priv->session);
	g_object_unref (priv->account_manager);

	G_OBJECT_CLASS (gossip_account_chooser_parent_class)->finalize (object);
}

static void
account_chooser_setup (GossipAccountChooser *account_chooser)
{
	GossipAccountChooserPriv *priv;
	GList                    *accounts;

	GtkListStore             *store;
	GtkCellRenderer          *renderer;
	GtkComboBox              *combobox;

	priv = GET_PRIV (account_chooser);

	/* set up combo box with new store */
	combobox = GTK_COMBO_BOX (account_chooser);

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combobox));

	store = gtk_list_store_new (COL_ACCOUNT_COUNT,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_STRING,    /* name */
				    G_TYPE_BOOLEAN,
				    GOSSIP_TYPE_ACCOUNT);

	gtk_combo_box_set_model (combobox, GTK_TREE_MODEL (store));

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
					"pixbuf", COL_ACCOUNT_IMAGE,
					"sensitive", COL_ACCOUNT_ENABLED,
					NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
					"text", COL_ACCOUNT_TEXT,
					"sensitive", COL_ACCOUNT_ENABLED,
					NULL);

	/* populate accounts */
	accounts = gossip_account_manager_get_accounts (priv->account_manager);
	g_list_foreach (accounts, (GFunc)account_chooser_account_add_foreach, account_chooser);
	g_list_foreach (accounts, (GFunc)g_object_unref, NULL);
	g_list_free (accounts);

	g_object_unref (store);
}

static void
account_chooser_account_added_cb (GossipAccountManager  *manager,
				  GossipAccount         *account,
				  GossipAccountChooser  *account_chooser)
{
	account_chooser_account_add_foreach (account, account_chooser);
}

static void
account_chooser_account_add_foreach (GossipAccount        *account,
				     GossipAccountChooser *account_chooser)
{
	GossipAccountChooserPriv *priv;

	GtkListStore             *store;
	GtkComboBox              *combobox;
	GtkTreeIter               iter;

	GdkPixbuf                *pixbuf;
	gboolean                  is_connected;
	gboolean                  is_enabled;

	priv = GET_PRIV (account_chooser);

	combobox = GTK_COMBO_BOX (account_chooser);
	store = GTK_LIST_STORE (gtk_combo_box_get_model (combobox));

	is_connected = gossip_session_is_connected (priv->session, account);
	pixbuf = gossip_pixbuf_from_account_status (account,
						    GTK_ICON_SIZE_MENU,
						    is_connected);

	if (!priv->can_select_all && !is_connected) {
		is_enabled = FALSE;
	} else {
		is_enabled = TRUE;
	}

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    COL_ACCOUNT_IMAGE, pixbuf,
			    COL_ACCOUNT_TEXT, gossip_account_get_name (account),
			    COL_ACCOUNT_ENABLED, is_enabled,
			    COL_ACCOUNT_POINTER, account,
			    -1);

	g_object_unref (pixbuf);

	/* set first connected account as active account */
	if (priv->set_active_item == FALSE &&
	    ((priv->can_select_all == TRUE) ||
	     (priv->can_select_all == FALSE && is_connected == TRUE))) {
		priv->set_active_item = TRUE;
		gtk_combo_box_set_active_iter (combobox, &iter);
	}

	g_signal_connect (account, "notify::name",
			  G_CALLBACK (account_chooser_account_name_changed_cb),
			  account_chooser);
}

static void
account_chooser_account_removed_cb (GossipAccountManager  *manager,
				    GossipAccount         *account,
				    GossipAccountChooser  *account_chooser)
{
	account_chooser_account_remove_foreach (account, account_chooser);
}

static void
account_chooser_account_remove_foreach (GossipAccount        *account,
					GossipAccountChooser *account_chooser)
{
	g_signal_handlers_disconnect_by_func (account,
					      account_chooser_account_name_changed_cb,
					      account_chooser);
}

static gboolean
account_chooser_account_name_foreach (GtkTreeModel          *model,
				      GtkTreePath           *path,
				      GtkTreeIter           *iter,
				      struct SetAccountData *data)
{
	GossipAccount *account1, *account2;
	gboolean       equal;

	account1 = GOSSIP_ACCOUNT (data->account);
	gtk_tree_model_get (model, iter, COL_ACCOUNT_POINTER, &account2, -1);

	equal = gossip_account_equal (account1, account2);
	g_object_unref (account2);

	if (equal) {
		GtkListStore *store;

		store = GTK_LIST_STORE (model);
		gtk_list_store_set (store, iter,
				    COL_ACCOUNT_TEXT, gossip_account_get_name (account1),
				    -1);

		data->set = TRUE;
	}

	return equal;
}

static void
account_chooser_account_name_changed_cb (GossipAccount        *account,
					 GParamSpec           *param,
					 GossipAccountChooser *account_chooser)
{
	GtkComboBox           *combobox;
	GtkTreeModel          *model;
	GtkTreeIter            iter;

	struct SetAccountData  data;

	g_return_if_fail (GOSSIP_IS_ACCOUNT_CHOOSER (account_chooser));
	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	combobox = GTK_COMBO_BOX (account_chooser);
	model = gtk_combo_box_get_model (combobox);
	gtk_combo_box_get_active_iter (combobox, &iter);

	data.account_chooser = account_chooser;
	data.account = account;

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) account_chooser_account_name_foreach,
				&data);
}

static gboolean
account_chooser_account_update_foreach (GtkTreeModel          *model,
					GtkTreePath           *path,
					GtkTreeIter           *iter,
					struct SetAccountData *data)
{
	GossipAccount *account1, *account2;
	gboolean       equal;

	account1 = GOSSIP_ACCOUNT (data->account);
	gtk_tree_model_get (model, iter, COL_ACCOUNT_POINTER, &account2, -1);

	equal = gossip_account_equal (account1, account2);
	g_object_unref (account2);

	if (equal) {
		GossipAccountChooserPriv *priv;
		GtkListStore             *store;
		GdkPixbuf                *pixbuf;
		gboolean                  is_connected;
		gboolean                  is_enabled;

		priv = GET_PRIV (data->account_chooser);

		store = GTK_LIST_STORE (model);

		is_connected = gossip_session_is_connected (priv->session,
							    data->account);
		pixbuf = gossip_pixbuf_from_account_status (data->account,
							    GTK_ICON_SIZE_MENU,
							    is_connected);

		if (!priv->can_select_all && !is_connected) {
			is_enabled = FALSE;
		} else {
			is_enabled = TRUE;
		}

		gtk_list_store_set (store, iter,
				    COL_ACCOUNT_IMAGE, pixbuf,
				    COL_ACCOUNT_ENABLED, is_enabled,
				    -1);

		g_object_unref (pixbuf);

		data->set = TRUE;
	}

	return equal;
}

static void
account_chooser_protocol_connected_cb (GossipSession        *session,
				       GossipAccount        *account,
				       GossipProtocol       *protocol,
				       GossipAccountChooser *account_chooser)
{
	GtkComboBox           *combobox;
	GtkTreeModel          *model;
	GtkTreeIter            iter;

	struct SetAccountData  data;

	g_return_if_fail (GOSSIP_IS_ACCOUNT_CHOOSER (account_chooser));
	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	combobox = GTK_COMBO_BOX (account_chooser);
	model = gtk_combo_box_get_model (combobox);
	gtk_combo_box_get_active_iter (combobox, &iter);

	data.account_chooser = account_chooser;
	data.account = account;

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) account_chooser_account_update_foreach,
				&data);
}

static void
account_chooser_protocol_disconnected_cb (GossipSession        *session,
					  GossipAccount        *account,
					  GossipProtocol       *protocol,
					  gint                  reason,
					  GossipAccountChooser *account_chooser)
{
	GtkComboBox           *combobox;
	GtkTreeModel          *model;
	GtkTreeIter            iter;

	struct SetAccountData  data;

	g_return_if_fail (GOSSIP_IS_ACCOUNT_CHOOSER (account_chooser));
	g_return_if_fail (GOSSIP_IS_ACCOUNT (account));

	combobox = GTK_COMBO_BOX (account_chooser);
	model = gtk_combo_box_get_model (combobox);
	gtk_combo_box_get_active_iter (combobox, &iter);

	data.account_chooser = account_chooser;
	data.account = account;

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) account_chooser_account_update_foreach,
				&data);
}

GtkWidget *
gossip_account_chooser_new (GossipSession *session)
{
	GossipAccountChooserPriv *priv;
	GossipAccountManager     *account_manager;
	GtkWidget                *account_chooser;

	g_return_val_if_fail (GOSSIP_IS_SESSION (session), NULL);

	account_manager = gossip_session_get_account_manager (session);
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_MANAGER (account_manager), NULL);

	account_chooser = g_object_new (GOSSIP_TYPE_ACCOUNT_CHOOSER, NULL);

	priv = GET_PRIV (account_chooser);

	priv->session = g_object_ref (session);
	priv->account_manager = g_object_ref (account_manager);

	g_signal_connect (priv->account_manager, "account_added",
			  G_CALLBACK (account_chooser_account_added_cb),
			  account_chooser);

	g_signal_connect (priv->account_manager, "account_removed",
			  G_CALLBACK (account_chooser_account_removed_cb),
			  account_chooser);

	g_signal_connect (priv->session, "protocol-connected",
			  G_CALLBACK (account_chooser_protocol_connected_cb),
			  account_chooser);

	g_signal_connect (priv->session, "protocol-disconnected",
			  G_CALLBACK (account_chooser_protocol_disconnected_cb),
			  account_chooser);

	account_chooser_setup (GOSSIP_ACCOUNT_CHOOSER (account_chooser));

	return account_chooser;
}

GossipAccount *
gossip_account_chooser_get_account (GossipAccountChooser *account_chooser)
{
	GossipAccountChooserPriv *priv;
	GossipAccount            *account;
	GtkTreeModel             *model;
	GtkTreeIter               iter;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_CHOOSER (account_chooser), NULL);

	priv = GET_PRIV (account_chooser);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (account_chooser));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (account_chooser), &iter);

	gtk_tree_model_get (model, &iter, COL_ACCOUNT_POINTER, &account, -1);

	return account;
}

static gboolean
account_chooser_set_account_foreach (GtkTreeModel          *model,
				     GtkTreePath           *path,
				     GtkTreeIter           *iter,
				     struct SetAccountData *data)
{
	GossipAccount *account1, *account2;
	gboolean       equal;

	account1 = GOSSIP_ACCOUNT (data->account);
	gtk_tree_model_get (model, iter, COL_ACCOUNT_POINTER, &account2, -1);

	equal = gossip_account_equal (account1, account2);
	g_object_unref (account2);

	if (equal) {
		GtkComboBox *combobox;

		combobox = GTK_COMBO_BOX (data->account_chooser);
		gtk_combo_box_set_active_iter (combobox, iter);

		data->set = TRUE;
	}

	return equal;
}

gboolean
gossip_account_chooser_set_account (GossipAccountChooser *account_chooser,
				    GossipAccount        *account)
{
	GtkComboBox           *combobox;
	GtkTreeModel          *model;
	GtkTreeIter            iter;

	struct SetAccountData  data;

	g_return_val_if_fail (GOSSIP_IS_ACCOUNT_CHOOSER (account_chooser), FALSE);
	g_return_val_if_fail (GOSSIP_IS_ACCOUNT (account), FALSE);

	combobox = GTK_COMBO_BOX (account_chooser);
	model = gtk_combo_box_get_model (combobox);
	gtk_combo_box_get_active_iter (combobox, &iter);

	data.account_chooser = account_chooser;
	data.account = account;

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) account_chooser_set_account_foreach,
				&data);

	return data.set;
}

static gboolean
account_chooser_set_enabled_foreach (GtkTreeModel         *model,
				     GtkTreePath          *path,
				     GtkTreeIter          *iter,
				     GossipAccountChooser *account_chooser)
{
	GossipAccountChooserPriv *priv;
	GossipAccount            *account;
	GtkListStore             *store;
	gboolean                  is_connected;
	gboolean                  is_enabled;

	priv = GET_PRIV (account_chooser);

	store = GTK_LIST_STORE (model);

	gtk_tree_model_get (model, iter, COL_ACCOUNT_POINTER, &account, -1);

	is_connected = gossip_session_is_connected (priv->session, account);
	g_object_unref (account);

	if (!priv->can_select_all && !is_connected) {
		is_enabled = FALSE;
	} else {
		is_enabled = TRUE;
	}

	gtk_list_store_set (store, iter, COL_ACCOUNT_ENABLED, is_enabled, -1);

	if (priv->set_active_item == FALSE &&
	    ((priv->can_select_all == TRUE) ||
	     (priv->can_select_all == FALSE && is_connected == TRUE))) {
		GtkComboBox *combobox;

		combobox = GTK_COMBO_BOX (account_chooser);

		priv->set_active_item = TRUE;
		gtk_combo_box_set_active_iter (combobox, iter);
	}

	return FALSE;
}

void
gossip_account_chooser_set_can_select_all (GossipAccountChooser *account_chooser,
					   gboolean              can_select_all)
{
	GossipAccountChooserPriv *priv;
	GtkComboBox              *combobox;
	GtkTreeModel             *model;

	g_return_if_fail (GOSSIP_IS_ACCOUNT_CHOOSER (account_chooser));

	priv = GET_PRIV (account_chooser);

	combobox = GTK_COMBO_BOX (account_chooser);
	model = gtk_combo_box_get_model (combobox);

	priv->can_select_all = can_select_all;

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) account_chooser_set_enabled_foreach,
				account_chooser);
}
