/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 CodeFactory AB
 * Copyright (C) 2003 Mikael Hallendal <micke@imendio.com>
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
#include <gtk/gtk.h>
#include <libgnomeui/gnome-druid.h>
#include <libgnome/gnome-i18n.h>
#include <loudmouth/loudmouth.h>
#include "gossip-utils.h"
#include "gossip-app.h"
#include "gossip-add-contact.h"

typedef struct {
	LmConnection *connection;
	
	GtkWidget *dialog;
	GtkWidget *druid;
	
	/* Page one */
	GtkWidget *one_page;
	GtkWidget *one_system_option_menu;
	GtkWidget *one_id_label;
	GtkWidget *one_id_entry;
	GtkWidget *one_search_button;

	/* Page two */
	GtkWidget *two_page;
	GtkWidget *two_id_label;
	GtkWidget *two_name_label;
	GtkWidget *two_email_label;
	GtkWidget *two_country_label;
	GtkWidget *two_nick_entry;
	GtkWidget *two_group_combo;
	GtkWidget *two_group_entry;
	
	/* Page three */
	GtkWidget *three_page;
	GtkWidget *three_message_label;
	GtkWidget *three_message_text_view;

	GtkWidget *last_page;
	GtkWidget *last_label;
	
	GCompletion *group_completion;
	guint        idle_complete;
} GossipAddContact;

enum {
	COL_NAME,
	NUM_OF_COLS
};

static void      add_contact_dialog_destroyed (GtkWidget        *unused,
					       GossipAddContact *contact);
static void      add_contact_cancel        (GtkWidget        *unused,
					    GossipAddContact *contact);
static void      add_contact_prepare_page_1        (GnomeDruidPage   *page,
					    GnomeDruid       *druid,
					    GossipAddContact *contact);
static void      add_contact_prepare_page_2        (GnomeDruidPage   *page,
					    GnomeDruid       *druid,
					    GossipAddContact *contact);
static LmHandlerResult * 
add_contact_page_2_vcard_handler            (LmMessageHandler *handler,
					     LmConnection     *connection,
					     LmMessage        *message,
					     GossipAddContact *contact);

static void      add_contact_prepare_page_3        (GnomeDruidPage   *page,
					    GnomeDruid       *druid,
					    GossipAddContact *contact);
static void      add_contact_prepare_page_last (GnomeDruidPage   *page,
						GnomeDruid       *druid,
						GossipAddContact *contact);
static void      add_contact_last_page_finished (GnomeDruidPage   *page,
						 GnomeDruid       *druid,
						 GossipAddContact *contact);

static void      add_contact_1_id_entry_changed (GtkEntry *entry,
						 GossipAddContact *contact);
static void      add_contact_1_search_button_clicked (GtkButton *button,
						      GossipAddContact *contact);
static void      add_contact_2_nick_entry_changed  (GtkEntry *entry,
						    GossipAddContact *contact);
static gboolean  add_contact_2_nick_entry_key_pressed (GtkWidget *entry,
						       GdkEvent  *event,
						       GossipAddContact *contact);
static void      add_contact_2_group_entry_text_inserted (GtkEntry         *entry, 
							  const gchar      *text,
							  gint              length,
							  gint             *position,
							  GossipAddContact *contact);

static void
add_contact_dialog_destroyed (GtkWidget *unused, GossipAddContact *contact)
{
	g_free (contact);
}

static void
add_contact_cancel (GtkWidget *widget, GossipAddContact *contact)
{
	gtk_widget_destroy (contact->dialog);
}

static void
add_contact_prepare_page_1 (GnomeDruidPage   *page, 
			    GnomeDruid       *druid, 
			    GossipAddContact *contact)
{
	const gchar *str;
	
	str = gtk_entry_get_text (GTK_ENTRY (contact->one_id_entry));

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (contact->druid),
					   FALSE,
					   gossip_jid_string_is_valid_jid (str),
					   TRUE,
					   FALSE);
		
	gtk_widget_grab_focus (contact->one_id_entry);
}

static void
add_contact_prepare_page_2 (GnomeDruidPage   *page,
			    GnomeDruid       *druid, 
			    GossipAddContact *contact)
{
	const gchar      *str_jid;
	gchar            *str;
	gint              changed;
	GList            *items = NULL;
	LmMessage        *m;
	LmMessageNode    *node;
	LmMessageHandler *handler;
	
	str_jid = gtk_entry_get_text (GTK_ENTRY (contact->one_id_entry));
	gtk_label_set_text (GTK_LABEL (contact->two_id_label), str_jid);
	
	changed = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (contact->two_nick_entry),
						     "changed"));

	handler = lm_message_handler_new 
		((LmHandleMessageFunction) add_contact_page_2_vcard_handler,
		 contact, NULL);
	
	m = lm_message_new_with_sub_type (str_jid, 
					  LM_MESSAGE_TYPE_IQ, 
					  LM_MESSAGE_SUB_TYPE_GET);
	node = lm_message_node_add_child (m->node, "vCard", NULL);
	lm_message_node_set_attributes (node, "xmlns", "vcard-temp", NULL);
	
	lm_connection_send_with_reply (contact->connection, m, handler, NULL);
	
	lm_message_unref (m);
	
	if (!changed) {
		GossipJID *jid;
		
		jid = gossip_jid_new (str_jid);
		str = gossip_jid_get_part_name (jid);
		gtk_entry_set_text (GTK_ENTRY (contact->two_nick_entry), str);
		g_free (str);
		gossip_jid_unref (jid);
	}

	items = g_list_append (items, "CodeFactory");
	items = g_list_append (items, "Kompisar");
	items = g_list_append (items, "Euroling");

	g_completion_clear_items (contact->group_completion);
	g_completion_add_items (contact->group_completion, items);
	
	gtk_combo_set_popdown_strings (GTK_COMBO (contact->two_group_combo),
				       items);

	gtk_entry_set_text (GTK_ENTRY (contact->two_group_entry), "");

	gtk_widget_grab_focus (contact->two_nick_entry);

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (contact->druid),
					   TRUE,
					   TRUE,
					   TRUE,
					   FALSE);
	/* Check Jabber ID, if not valid show dialog */
}

static LmHandlerResult * 
add_contact_page_2_vcard_handler (LmMessageHandler *handler,
				  LmConnection     *connection,
				  LmMessage        *m,
				  GossipAddContact *contact)
{
	LmMessageNode *node;
	
	if (lm_message_get_sub_type (m) == LM_MESSAGE_SUB_TYPE_ERROR) {
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	}

 	node = lm_message_node_find_child (m->node, "fn");
	if (node) {
		gtk_label_set_text (GTK_LABEL (contact->two_name_label), 
				    node->value);
	}

	node = lm_message_node_find_child (m->node, "email");
	if (node) {
		gtk_label_set_text (GTK_LABEL (contact->two_email_label), 
				    node->value);
	}

	node = lm_message_node_find_child (m->node, "country");
	if (node) {
		gtk_label_set_text (GTK_LABEL (contact->two_country_label), 
				    node->value);
	}

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
add_contact_prepare_page_3 (GnomeDruidPage   *page,
			    GnomeDruid       *druid, 
			    GossipAddContact *contact)
{
	gchar *str;
	
	str = g_strdup_printf (_("What request message do you want to send to <b>%s</b>"),
			       gtk_entry_get_text (GTK_ENTRY (contact->two_nick_entry)));

	gtk_label_set_markup (GTK_LABEL (contact->three_message_label), str);
	g_free (str);
}

static void
add_contact_prepare_page_last (GnomeDruidPage   *page,
			       GnomeDruid       *druid,
			       GossipAddContact *contact)
{
	gchar       *str;
	gchar       *str1;
	const gchar *nick;

  	gnome_druid_set_show_finish (GNOME_DRUID (contact->druid), TRUE);
	
	nick = gtk_entry_get_text (GTK_ENTRY (contact->two_nick_entry));
	str = g_strdup_printf (_("%s will be added to your contact list."),
			       nick);
	
	str1 = g_strdup_printf ("<b>%s</b>\n%s",
				_("Contact will be added to your contact list"),
				str);
	g_free (str);

	gtk_label_set_markup (GTK_LABEL (contact->last_label), str1);
	g_free (str1);
}

static void
add_contact_last_page_finished (GnomeDruidPage   *page,
				GnomeDruid       *druid,
				GossipAddContact *contact)
{
 	LmMessage     *m;
 	const gchar   *group;
 	LmMessageNode *node;
	const gchar   *jid;
	GtkTextBuffer *buffer;
	GtkTextIter    start, end;
	gchar         *message;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (contact->three_message_text_view));
	gtk_text_buffer_get_bounds (buffer, &start, &end);

	message = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);

	jid = gtk_label_get_text (GTK_LABEL (contact->two_id_label));
	
	/* Request subscribe */
	m = lm_message_new_with_sub_type (jid,LM_MESSAGE_TYPE_PRESENCE,
					  LM_MESSAGE_SUB_TYPE_SUBSCRIBE);
	lm_message_node_add_child (m->node, "status", message);
	g_free (message);

	lm_connection_send (contact->connection, m, NULL);
	lm_message_unref (m);
	
	/* Add to roster */
	group = gtk_entry_get_text (GTK_ENTRY (contact->two_group_entry));
	m = lm_message_new_with_sub_type (NULL, LM_MESSAGE_TYPE_IQ,
					  LM_MESSAGE_SUB_TYPE_SET);
	node = lm_message_node_add_child (m->node, "query", NULL);
	lm_message_node_set_attributes (node,
					"xmlns", "jabber:iq:roster", NULL);
	node = lm_message_node_add_child (node, "item", NULL);
	lm_message_node_set_attributes (node, 
					"jid", jid,
					"subscription", "none",
					"ask", "subscribe",
					"name", gtk_entry_get_text (GTK_ENTRY (contact->two_nick_entry)),
					NULL);
	if (strcmp (group, "") != 0) {
		lm_message_node_add_child (node, "group", group);
	}
	
	lm_connection_send (contact->connection, m, NULL);
	lm_message_unref (m);

	gtk_widget_destroy (contact->dialog);
}

static void
add_contact_1_id_entry_changed (GtkEntry *entry, GossipAddContact *contact)
{
	const gchar *str;
	
	str = gtk_entry_get_text (GTK_ENTRY (entry));	

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (contact->druid),
					   FALSE,
					   gossip_jid_string_is_valid_jid (str),
					   TRUE,
					   FALSE);
}

static void
add_contact_1_search_button_clicked (GtkButton        *button,
				     GossipAddContact *contact)
{
}

static void
add_contact_2_nick_entry_changed (GtkEntry         *entry,
				    GossipAddContact *contact)
{
	const gchar     *str;
	gboolean         forward_sensitive = TRUE;
	
	str = gtk_entry_get_text (GTK_ENTRY (entry));;
	if (!str || strcmp (str, "") == 0) {
		forward_sensitive = FALSE;
	}

	gnome_druid_set_buttons_sensitive (GNOME_DRUID (contact->druid),
					   TRUE,
					   forward_sensitive,
					   TRUE,
					   FALSE);
}
 
static gboolean
add_contact_2_nick_entry_key_pressed (GtkWidget        *entry,
				      GdkEvent         *event,
				      GossipAddContact *contact)
{
	g_object_set_data (G_OBJECT (entry), 
			   "changed", GINT_TO_POINTER (1));

	return FALSE;
}

static gboolean 
add_contact_complete_group_idle (GossipAddContact *contact)
{
	GtkEntry    *entry = GTK_ENTRY (contact->two_group_entry);
	const gchar *prefix;
	gchar       *new_prefix;
	gint         text_len;

	prefix = gtk_entry_get_text (entry);
	text_len = strlen (prefix);

	g_completion_complete (contact->group_completion, 
			       (gchar *)prefix, 
			       &new_prefix);

	if (new_prefix) {
		g_signal_handlers_block_by_func (entry,
						 add_contact_2_group_entry_text_inserted, contact);
		
  		gtk_entry_set_text (entry, new_prefix); 
					  
		g_signal_handlers_unblock_by_func (entry, 
						   add_contact_2_group_entry_text_inserted, contact);

		gtk_editable_set_position (GTK_EDITABLE (entry), text_len);
		gtk_editable_select_region (GTK_EDITABLE (entry),
					    text_len, -1);
		g_free (new_prefix);
	}

	contact->idle_complete = 0;
	return FALSE;
}

static void
add_contact_2_group_entry_text_inserted (GtkEntry         *entry, 
					 const gchar      *text,
					 gint              length,
					 gint             *position,
					 GossipAddContact *contact)
{
	if (!contact->idle_complete) {
		contact->idle_complete = 
			g_idle_add ((GSourceFunc) add_contact_complete_group_idle,
				    contact);
	}
}

void
gossip_add_contact_new (LmConnection *connection, GossipJID *jid)
{
	GossipAddContact *contact;
	GladeXML         *glade;

	g_return_if_fail (connection != NULL);
	
	contact = g_new0 (GossipAddContact, 1);
	
	contact->connection = lm_connection_ref (connection);
	contact->group_completion = g_completion_new (NULL);
	
	glade = gossip_glade_get_file (
		GLADEDIR "/main.glade",
		"add_contact_dialog",
		NULL,
		"add_contact_dialog", &contact->dialog,
		"add_contact_druid", &contact->druid,
		"1_page", &contact->one_page,
		"1_system_option_menu", &contact->one_system_option_menu,
		"1_id_label", &contact->one_id_label,
		"1_id_entry", &contact->one_id_entry,
		"1_search_button", &contact->one_search_button,
		"2_page", &contact->two_page,
		"2_id_label", &contact->two_id_label,
		"2_name_label", &contact->two_name_label,
		"2_email_label", &contact->two_email_label,
		"2_country_label", &contact->two_country_label,
		"2_nick_entry", &contact->two_nick_entry,
		"2_group_combo", &contact->two_group_combo,
		"2_group_entry", &contact->two_group_entry,
		"3_page", &contact->three_page,
		"3_message_label", &contact->three_message_label,
		"3_message_text_view", &contact->three_message_text_view,
		"last_page", &contact->last_page,
		"last_label", &contact->last_label,
		NULL);
	
	gossip_glade_connect (
		glade, contact,
		"add_contact_dialog", "destroy", add_contact_dialog_destroyed,
		"add_contact_druid", "cancel", add_contact_cancel,
		"1_id_entry", "changed", add_contact_1_id_entry_changed,
		"1_search_button", "clicked", add_contact_1_search_button_clicked,
		"2_nick_entry", "changed", add_contact_2_nick_entry_changed,
		"2_nick_entry", "key_press_event", add_contact_2_nick_entry_key_pressed,
		"last_page", "finish", add_contact_last_page_finished,
		NULL);
	
	g_object_unref (glade);
	
	g_signal_connect_after (contact->one_page, "prepare",
				G_CALLBACK (add_contact_prepare_page_1),
				contact);
	g_signal_connect_after (contact->two_page, "prepare",
				G_CALLBACK (add_contact_prepare_page_2),
				contact);
	g_signal_connect_after (contact->three_page, "prepare",
				G_CALLBACK (add_contact_prepare_page_3),
				contact);
	g_signal_connect_after (contact->last_page, "prepare",
				G_CALLBACK (add_contact_prepare_page_last),
				contact);
	g_signal_connect_after (contact->two_group_entry, "insert_text",
				G_CALLBACK (add_contact_2_group_entry_text_inserted),
				contact);

	g_object_set_data (G_OBJECT (contact->two_nick_entry), 
			   "changed", GINT_TO_POINTER (0));

	if (jid) {
		gtk_entry_set_text (GTK_ENTRY (contact->one_id_entry), 
				    gossip_jid_get_without_resource (jid));
		gnome_druid_set_page (GNOME_DRUID (contact->druid),
				      GNOME_DRUID_PAGE (contact->two_page));
		add_contact_prepare_page_2 (GNOME_DRUID_PAGE (contact->two_page),
					    GNOME_DRUID (contact->druid), contact);
	} else {
		add_contact_prepare_page_1 (GNOME_DRUID_PAGE (contact->one_page), 
					    GNOME_DRUID (contact->druid), contact);
	}

	gtk_widget_show (contact->dialog);
}
