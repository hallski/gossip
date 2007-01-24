/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2006 Imendio AB
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
#include <unistd.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#ifdef HAVE_GNOME
#include <libgnomeui/libgnomeui.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#endif

#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-conf.h>
#include <libgossip/gossip-protocol.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-utils.h>
#include <libgossip/gossip-vcard.h>

#include "gossip-account-chooser.h"
#include "gossip-app.h"
#include "gossip-vcard-dialog.h"
#include "gossip-image-chooser.h"
#include "gossip-preferences.h"
#include "gossip-popup-button.h"

#define DEBUG_DOMAIN "VCardDialog"

#define VCARD_TIMEOUT     20000
#define SAVED_TIMEOUT     10000

#define AVATAR_MAX_HEIGHT 96
#define AVATAR_MAX_WIDTH  96

typedef struct {
	GtkWidget *dialog;

	GtkWidget *hbox_account;
	GtkWidget *label_account;
	GtkWidget *account_chooser;

	GtkWidget *vbox_details;

	GtkWidget *label_name;
	GtkWidget *label_nickname;
	GtkWidget *label_web_site;
	GtkWidget *label_email;
	GtkWidget *label_description;

	GtkWidget *entry_name;
	GtkWidget *entry_nickname;
	GtkWidget *entry_web_site;
	GtkWidget *entry_email;

	GtkWidget *textview_description;

	GtkWidget *entry_birthday;
	GtkWidget *button_birthday;

	GtkWidget *calendar;

	GtkWidget *button_cancel;
	GtkWidget *button_save;

	GtkWidget *button_image;
	GtkWidget *avatar_chooser;

	guint      timeout_id;
	gboolean   requesting_vcard;
	gint       last_account_selected;
	gchar     *avatar_format;

#ifdef HAVE_GNOME
	GnomeThumbnailFactory *thumbs;
#endif
} GossipVCardDialog;

static void       vcard_dialog_create_avatar_chooser      (GossipVCardDialog *dialog);
static void       vcard_dialog_avatar_chooser_response_cb (GtkWidget         *widget,
							   gint               response,
							   GossipVCardDialog *dialog);
static void       vcard_dialog_avatar_clicked_cb          (GtkWidget         *button,
							   GossipVCardDialog *dialog);
static void       vcard_dialog_avatar_update_preview_cb   (GtkFileChooser    *chooser,
							   GossipVCardDialog *dialog);
static GtkWidget *vcard_dialog_birthday_button_popup_cb   (GossipPopupButton *popup_button,
							   GossipVCardDialog *dialog);
static void       vcard_dialog_birthday_button_popdown_cb (GossipPopupButton *popup_button,
							   GtkWidget         *widget,
							   gboolean           ok,
							   GossipVCardDialog *dialog);
static gboolean   vcard_dialog_birthday_parse_string      (const gchar       *str,
							   gint              *year,
							   gint              *month,
							   gint              *day);
static gchar *    vcard_dialog_birthday_string_to_server  (const gchar       *str);
static gchar *    vcard_dialog_birthday_string_from_server(const gchar       *str);
static void       vcard_dialog_set_account_to_last        (GossipVCardDialog *dialog);
static void       vcard_dialog_lookup_start               (GossipVCardDialog *dialog);
static void       vcard_dialog_lookup_stop                (GossipVCardDialog *dialog);
static void       vcard_dialog_get_vcard_cb               (GossipResult       result,
							   GossipVCard       *vcard,
							   GossipVCardDialog *dialog);
static void       vcard_dialog_set_vcard                  (GossipVCardDialog *dialog);
static void       vcard_dialog_set_vcard_cb               (GossipResult       result,
							   gpointer           user_data);
static gboolean   vcard_dialog_timeout_cb                 (GossipVCardDialog *dialog);
static void       vcard_dialog_account_changed_cb         (GtkWidget         *combo_box,
							   GossipVCardDialog *dialog);
static void       vcard_dialog_response_cb                (GtkDialog         *widget,
							   gint               response,
							   GossipVCardDialog *dialog);
static void       vcard_dialog_destroy_cb                 (GtkWidget         *widget,
							   GossipVCardDialog *dialog);

static GossipVCardDialog *dialog = NULL;

static void
vcard_dialog_create_avatar_chooser (GossipVCardDialog *dialog)
{
	GossipAccountChooser *account_chooser;
	GossipAccount        *account;
	guint                 min_width;
	guint                 min_height;
	guint                 max_width;
	guint                 max_height;
	gsize                 max_size;

	dialog->avatar_chooser = gossip_image_chooser_new ();
	account_chooser = GOSSIP_ACCOUNT_CHOOSER (dialog->account_chooser);
	account = gossip_account_chooser_get_account (account_chooser);

	gossip_session_get_avatar_requirements (gossip_app_get_session (),
						account,
						&min_width, &min_height,
						&max_width, &max_height,
						&max_size,  &dialog->avatar_format);

	gossip_image_chooser_set_requirements (GOSSIP_IMAGE_CHOOSER (dialog->avatar_chooser),
					       min_width, min_height,
					       max_width, max_height,
					       max_size,  dialog->avatar_format);

	gtk_widget_set_size_request (dialog->avatar_chooser,
				     AVATAR_MAX_WIDTH / 2,
				     AVATAR_MAX_HEIGHT / 2);

	gtk_container_add (GTK_CONTAINER (dialog->button_image),
			   dialog->avatar_chooser);
	gtk_widget_show_all (dialog->avatar_chooser);
}

static void
vcard_dialog_avatar_chooser_response_cb (GtkWidget         *widget,
					 gint               response,
					 GossipVCardDialog *dialog)
{
	if (response == GTK_RESPONSE_OK) {
		gchar *filename;
		gchar *path;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
		gossip_image_chooser_set_from_file
			(GOSSIP_IMAGE_CHOOSER (dialog->avatar_chooser), filename);
		g_free (filename);

		path = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (widget));
		if (path) {
			gossip_conf_set_string (gossip_conf_get (),
						 GOSSIP_PREFS_UI_AVATAR_DIRECTORY,
						 path);
			g_free (path);
		}
	}
	else if (response == GTK_RESPONSE_NO) {
		gossip_image_chooser_set_image_data
			(GOSSIP_IMAGE_CHOOSER (dialog->avatar_chooser), NULL, 0);
	}

	gtk_widget_destroy (widget);
}

static void
vcard_dialog_avatar_clicked_cb (GtkWidget         *button,
				GossipVCardDialog *dialog)
{
	GtkWidget *chooser_dialog;
	gchar     *path;
	GtkWidget *avatar_chooser;
	GtkWidget *image;

	avatar_chooser = dialog->avatar_chooser;

	chooser_dialog = gtk_file_chooser_dialog_new (_("Select Your Avatar Image"),
						      GTK_WINDOW (dialog->dialog),
						      GTK_FILE_CHOOSER_ACTION_OPEN,
						      _("No Image"),
						      GTK_RESPONSE_NO,
						      GTK_STOCK_CANCEL,
						      GTK_RESPONSE_CANCEL,
						      GTK_STOCK_OPEN,
						      GTK_RESPONSE_OK,
						      NULL);

	gtk_window_set_transient_for (GTK_WINDOW (chooser_dialog),
				      GTK_WINDOW (dialog->dialog));

	gtk_dialog_set_default_response (GTK_DIALOG (chooser_dialog), GTK_RESPONSE_ACCEPT);

	path = NULL;
	gossip_conf_get_string (gossip_conf_get (),
				 GOSSIP_PREFS_UI_AVATAR_DIRECTORY,
				 &path);

	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser_dialog),
					     path ? path : g_get_home_dir ());
	g_free (path);

	gtk_file_chooser_set_use_preview_label (GTK_FILE_CHOOSER (chooser_dialog), FALSE);

	image = gtk_image_new ();
	gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (chooser_dialog), image);
	gtk_widget_set_size_request (image, AVATAR_MAX_WIDTH, AVATAR_MAX_HEIGHT);
	gtk_widget_show (image);

	g_signal_connect (chooser_dialog, "update-preview",
			  G_CALLBACK (vcard_dialog_avatar_update_preview_cb),
			  dialog);

	g_signal_connect (chooser_dialog, "response",
			  G_CALLBACK (vcard_dialog_avatar_chooser_response_cb),
			  dialog);

	gtk_widget_show (chooser_dialog);
}

#ifdef HAVE_GNOME
static GdkPixbuf *
vcard_dialog_scale_down_to_width (GdkPixbuf *pixbuf, gint wanted_width)
{
	gint      width, height;
	gdouble   factor;

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	if (width > wanted_width) {
		factor = (gdouble) wanted_width / MAX (width, height);

		width = width * factor;
		height = height * factor;

		return gdk_pixbuf_scale_simple (pixbuf,
						width, height,
						GDK_INTERP_BILINEAR);
	}

	return g_object_ref (pixbuf);
}
#endif

static void
vcard_dialog_avatar_update_preview_cb (GtkFileChooser    *chooser,
				       GossipVCardDialog *dialog)
{
#ifdef HAVE_GNOME
	gchar *uri;

	uri = gtk_file_chooser_get_preview_uri (chooser);

	if (uri) {
		GtkWidget *image;
		GdkPixbuf *pixbuf;
		GdkPixbuf *scaled_pixbuf;
		gchar     *mime_type;

		if (!dialog->thumbs) {
			dialog->thumbs =
				gnome_thumbnail_factory_new (GNOME_THUMBNAIL_SIZE_NORMAL);
		}

		mime_type = gnome_vfs_get_mime_type (uri);
		pixbuf = gnome_thumbnail_factory_generate_thumbnail (dialog->thumbs,
								     uri,
								     mime_type);
		image = gtk_file_chooser_get_preview_widget (chooser);

		if (pixbuf) {
			scaled_pixbuf = vcard_dialog_scale_down_to_width (
				pixbuf, AVATAR_MAX_WIDTH);
			gtk_image_set_from_pixbuf (GTK_IMAGE (image), scaled_pixbuf);
			g_object_unref (scaled_pixbuf);
			g_object_unref (pixbuf);
		} else {
			gtk_image_set_from_stock (GTK_IMAGE (image),
						  "gtk-dialog-question",
						  GTK_ICON_SIZE_DIALOG);
		}

		g_free (mime_type);
	}

	gtk_file_chooser_set_preview_widget_active (chooser, TRUE);
#endif
}


static void
vcard_dialog_birthday_cancel_clicked_cb (GtkWidget         *button,
					 GossipPopupButton *popup_button)
{
	gossip_popup_button_popdown (popup_button, FALSE);
}

static void
vcard_dialog_birthday_ok_clicked_cb (GtkWidget         *button,
				     GossipPopupButton *popup_button)
{
	gossip_popup_button_popdown (popup_button, TRUE);
}

static gboolean
vcard_dialog_birthday_parse_string (const gchar *str,
				    gint        *year,
				    gint        *month,
				    gint        *day)
{
	GDate *date;

	date = g_date_new ();
	g_date_set_parse (date, str);

	if (!g_date_valid (date)) {
		g_date_free (date);
		return FALSE;
	}

	*year = g_date_get_year (date);
	*month = g_date_get_month (date);
	*day = g_date_get_day (date);

	g_date_free (date);

	return TRUE;
}

static gchar *
vcard_dialog_birthday_string_to_server (const gchar *str)
{
	gint year, month, day;

	if (vcard_dialog_birthday_parse_string (str, &year, &month, &day)) {
		return g_strdup_printf ("%04d-%02d-%02d", year, month, day);
	}

	return NULL;
}

static gchar *
vcard_dialog_birthday_string_from_server (const gchar *str)
{
	gint         year, month, day;
	GDate       *date;
	const gchar *format = "%x"; /* Keep in variable get rid of warning. */
	gchar        buf[128];

	if (sscanf (str, "%04d-%02d-%02d", &year, &month, &day) != 3) {
		return NULL;
	}

	date = g_date_new ();
	g_date_set_dmy (date, day, month, year);

	if (g_date_strftime (buf, sizeof (buf), format, date) > 0) {
		g_date_free (date);
		return g_strdup (buf);
	}

	g_date_free (date);

	return NULL;
}

static void
vcard_dialog_birthday_double_click_cb (GtkWidget         *calendar,
				       GossipPopupButton *popup_button)
{
	gossip_popup_button_popdown (popup_button, TRUE);
}

static void
vcard_dialog_birthday_button_popdown_cb (GossipPopupButton *button,
					 GtkWidget         *widget,
					 gboolean           ok,
					 GossipVCardDialog *dialog)
{
	gint         year, month, day;
	GDate       *date;
	const gchar *format = "%x"; /* Keep in variable get rid of warning. */
	gchar        buf[128];

	if (ok) {
		gtk_calendar_get_date (GTK_CALENDAR (dialog->calendar),
				       &year, &month, &day);

		date = g_date_new ();
		g_date_set_dmy (date, day, month + 1, year);

		if (g_date_strftime (buf, sizeof (buf), format, date) > 0) {
			gtk_entry_set_text (GTK_ENTRY (dialog->entry_birthday), buf);
		}

		g_date_free (date);
	}

	dialog->calendar = NULL;
	gtk_widget_destroy (widget);
}

static GtkWidget *
vcard_dialog_birthday_button_popup_cb (GossipPopupButton *popup_button,
				       GossipVCardDialog *dialog)
{
	GtkWidget   *frame;
	GtkWidget   *vbox;
	GtkWidget   *bbox;
	GtkWidget   *button;
	const gchar *str;
	gint         year, month, day;

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);

	dialog->calendar = gtk_calendar_new ();
	gtk_box_pack_start (GTK_BOX (vbox), dialog->calendar, TRUE, TRUE, 0);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_birthday));
	if (vcard_dialog_birthday_parse_string (str, &year, &month, &day)) {
		gtk_calendar_select_month (GTK_CALENDAR (dialog->calendar), month - 1, year);
		gtk_calendar_select_day (GTK_CALENDAR (dialog->calendar), day);
	}

	bbox = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);

	button = gtk_button_new_with_label (_("Cancel"));
	gtk_container_add (GTK_CONTAINER (bbox), button);
	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (vcard_dialog_birthday_cancel_clicked_cb),
			  popup_button);

	button = gtk_button_new_with_label (_("OK"));
	gtk_container_add (GTK_CONTAINER (bbox), button);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (vcard_dialog_birthday_ok_clicked_cb),
			  popup_button);

	g_signal_connect (dialog->calendar,
			  "day-selected-double-click",
			  G_CALLBACK (vcard_dialog_birthday_double_click_cb),
			  popup_button);

	return frame;
}

static void
vcard_dialog_set_account_to_last (GossipVCardDialog *dialog)
{
	g_signal_handlers_block_by_func (dialog->account_chooser,
					 vcard_dialog_account_changed_cb,
					 dialog);

	gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->account_chooser),
				  dialog->last_account_selected);

	g_signal_handlers_unblock_by_func (dialog->account_chooser,
					   vcard_dialog_account_changed_cb,
					   dialog);
}

static void
vcard_dialog_lookup_start (GossipVCardDialog *dialog)
{
	GossipSession        *session;
	GossipAccount        *account;
	GossipAccountChooser *account_chooser;

	gtk_widget_set_sensitive (dialog->vbox_details, FALSE);
	gtk_widget_set_sensitive (dialog->account_chooser, FALSE);
	gtk_widget_set_sensitive (dialog->button_save, FALSE);

	dialog->timeout_id = g_timeout_add (VCARD_TIMEOUT,
					    (GSourceFunc)vcard_dialog_timeout_cb,
					    dialog);

	session = gossip_app_get_session ();
	account_chooser = GOSSIP_ACCOUNT_CHOOSER (dialog->account_chooser);
	account = gossip_account_chooser_get_account (account_chooser);

	dialog->requesting_vcard = TRUE;

	gossip_session_get_vcard (session,
				  account,
				  NULL,
				  (GossipVCardCallback) vcard_dialog_get_vcard_cb,
				  dialog,
				  NULL);

	g_object_unref (account);
}

static void
vcard_dialog_lookup_stop (GossipVCardDialog *dialog)
{
	dialog->requesting_vcard = FALSE;

	gtk_widget_set_sensitive (dialog->vbox_details, TRUE);
	gtk_widget_set_sensitive (dialog->account_chooser, TRUE);
	gtk_widget_set_sensitive (dialog->button_save, TRUE);

	if (dialog->timeout_id != 0) {
		g_source_remove (dialog->timeout_id);
		dialog->timeout_id = 0;
	}
}

static void
vcard_dialog_get_vcard_cb (GossipResult       result,
			   GossipVCard       *vcard,
			   GossipVCardDialog *user_data)
{
	GtkComboBox   *combo_box;
	GtkTextBuffer *buffer;
	const gchar   *str;
	GossipAvatar  *avatar;

	gossip_debug (DEBUG_DOMAIN, "Received VCard response");

	if (!dialog) {
		return;
	}

	vcard_dialog_lookup_stop (dialog);

	if (result != GOSSIP_RESULT_OK) {
		gossip_debug (DEBUG_DOMAIN, "Received VCard response was not good");
		return;
	}

	str = gossip_vcard_get_name (vcard);
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_name), G_STR_EMPTY (str) ? "" : str);

	str = gossip_vcard_get_nickname (vcard);
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_nickname), G_STR_EMPTY (str) ? "" : str);

	str = gossip_vcard_get_birthday (vcard);
	if (str) {
		gchar *date;

		date = vcard_dialog_birthday_string_from_server (str);
		gtk_entry_set_text (GTK_ENTRY (dialog->entry_birthday), G_STR_EMPTY (date) ? "" : date);
		g_free (date);
	}

	str = gossip_vcard_get_email (vcard);
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_email), G_STR_EMPTY (str) ? "" : str);

	str = gossip_vcard_get_url (vcard);
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_web_site), G_STR_EMPTY (str) ? "" : str);

	str = gossip_vcard_get_description (vcard);
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->textview_description));
	gtk_text_buffer_set_text (buffer, G_STR_EMPTY (str) ? "" : str, -1);

	avatar = gossip_vcard_get_avatar (vcard);

	if (avatar) {
		gossip_image_chooser_set_image_data (GOSSIP_IMAGE_CHOOSER (dialog->avatar_chooser),
						     (gchar*) avatar->data, avatar->len);
	} else {
		gossip_image_chooser_set_image_data (GOSSIP_IMAGE_CHOOSER (dialog->avatar_chooser),
						     NULL, 0);
	}

	/* Save position incase the next lookup fails. */
	combo_box = GTK_COMBO_BOX (dialog->account_chooser);
	dialog->last_account_selected = gtk_combo_box_get_active (combo_box);
}

static void
vcard_dialog_set_vcard_cb (GossipResult result,
			   gpointer     user_data)
{
	gossip_debug (DEBUG_DOMAIN, "Received VCard response");

	if (!dialog) {
		return;
	}

	gtk_widget_destroy (dialog->dialog);
}

static void
vcard_dialog_set_vcard (GossipVCardDialog *dialog)
{
	GossipVCard          *vcard;
	GossipAccount        *account;
	GossipAccountChooser *account_chooser;
	GossipProtocol       *protocol;
	GossipContact        *contact;
	GError               *error = NULL;
	GtkTextBuffer        *buffer;
	GtkTextIter           iter_begin, iter_end;
	gchar                *description;
	const gchar          *str;
	gchar                *avatar_data;
	gsize                 avatar_size;
	GossipAvatar         *avatar = NULL;
	gchar                *birthday;

	if (!gossip_app_is_connected ()) {
		gossip_debug (DEBUG_DOMAIN, "Not connected, not setting VCard");
		return;
	}

	vcard = gossip_vcard_new ();

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_name));
	gossip_vcard_set_name (vcard, str);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_nickname));
	gossip_vcard_set_nickname (vcard, str);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_birthday));
	birthday = vcard_dialog_birthday_string_to_server (str);
	if (birthday) {
		gossip_vcard_set_birthday (vcard, birthday);
		g_free (birthday);
	}

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_web_site));
	gossip_vcard_set_url (vcard, str);

	str = gtk_entry_get_text (GTK_ENTRY (dialog->entry_email));
	gossip_vcard_set_email (vcard, str);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->textview_description));
	gtk_text_buffer_get_bounds (buffer, &iter_begin, &iter_end);
	description = gtk_text_buffer_get_text (buffer, &iter_begin, &iter_end, FALSE);
	gossip_vcard_set_description (vcard, description);
	g_free (description);

	gossip_image_chooser_get_image_data (GOSSIP_IMAGE_CHOOSER (dialog->avatar_chooser),
					     &avatar_data, &avatar_size);
	if (avatar_data) {
		avatar = gossip_avatar_new (avatar_data, avatar_size, dialog->avatar_format);
		gossip_vcard_set_avatar (vcard, avatar);
	}

	/* NOTE: if account is NULL, all accounts will get the same vcard */
	account_chooser = GOSSIP_ACCOUNT_CHOOSER (dialog->account_chooser);
	account = gossip_account_chooser_get_account (account_chooser);

	gossip_session_set_vcard (gossip_app_get_session (),
				  account,
				  vcard,
				  (GossipResultCallback) vcard_dialog_set_vcard_cb,
				  NULL, &error);

	protocol = gossip_session_get_protocol (gossip_app_get_session (), account);
	contact = gossip_protocol_get_own_contact (protocol);
	gossip_contact_set_avatar (GOSSIP_CONTACT (contact), avatar);

	if (avatar) {
		gossip_avatar_unref (avatar);
	}

	g_object_unref (account);
}

static gboolean
vcard_dialog_timeout_cb (GossipVCardDialog *dialog)
{
	GtkWidget *md;

	gossip_debug (DEBUG_DOMAIN, "Received VCard lookup timeout");

	if (!dialog) {
		return FALSE;
	}

	vcard_dialog_lookup_stop (dialog);

	/* Select last successfull account */
	vcard_dialog_set_account_to_last (dialog);

	/* Show message dialog and the account dialog */
	md = gtk_message_dialog_new_with_markup (GTK_WINDOW (dialog->dialog),
						 GTK_DIALOG_MODAL |
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_INFO,
						 GTK_BUTTONS_CLOSE,
						 "<b>%s</b>\n\n%s",
						 _("The server does not seem to be responding."),
						 _("Try again later."));

	g_signal_connect_swapped (md, "response",
				  G_CALLBACK (gtk_widget_destroy), md);
	gtk_widget_show (md);

	return FALSE;
}

static void
vcard_dialog_account_changed_cb (GtkWidget         *combo_box,
				 GossipVCardDialog *dialog)
{
	vcard_dialog_lookup_start (dialog);
}

static void
vcard_dialog_response_cb (GtkDialog         *widget,
			  gint               response,
			  GossipVCardDialog *dialog)
{
	switch (response) {
	case GTK_RESPONSE_OK:
		gtk_widget_set_sensitive (dialog->button_save, FALSE);
		vcard_dialog_set_vcard (dialog);
		return;

	case GTK_RESPONSE_CANCEL:
		if (dialog->requesting_vcard) {
			/* Change widgets so they are unsensitive */
			vcard_dialog_lookup_stop (dialog);

			/* Select last successfull account */
			vcard_dialog_set_account_to_last (dialog);
			return;
		}

	default:
		break;
	}

	gtk_widget_destroy (dialog->dialog);
}

static void
vcard_dialog_destroy_cb (GtkWidget         *widget,
			 GossipVCardDialog *dialog)
{
	vcard_dialog_lookup_stop (dialog);

	g_free (dialog);
	dialog = NULL;
}

void
gossip_vcard_dialog_show (GtkWindow *parent)
{
	GossipSession *session;
	GladeXML      *glade;
	GList         *accounts;
	GtkWidget     *birthday_placeholder;

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->dialog));
		return;
	}

	dialog = g_new0 (GossipVCardDialog, 1);

	glade = gossip_glade_get_file ("main.glade",
				       "vcard_dialog",
				       NULL,
				       "vcard_dialog", &dialog->dialog,
				       "hbox_account", &dialog->hbox_account,
				       "label_account", &dialog->label_account,
				       "vbox_details", &dialog->vbox_details,
				       "label_name", &dialog->label_name,
				       "label_nickname", &dialog->label_nickname,
				       "label_web_site", &dialog->label_web_site,
				       "label_email", &dialog->label_email,
				       "label_description", &dialog->label_description,
				       "entry_name", &dialog->entry_name,
				       "entry_nickname", &dialog->entry_nickname,
				       "entry_web_site", &dialog->entry_web_site,
				       "entry_email", &dialog->entry_email,
				       "entry_birthday", &dialog->entry_birthday,
				       "box_birthday_placeholder", &birthday_placeholder,
				       "textview_description", &dialog->textview_description,
				       "button_cancel", &dialog->button_cancel,
				       "button_save", &dialog->button_save,
				       "button_image", &dialog->button_image,
				       NULL);

	gossip_glade_connect (glade,
			      dialog,
			      "vcard_dialog", "destroy", vcard_dialog_destroy_cb,
			      "vcard_dialog", "response", vcard_dialog_response_cb,
			      "button_image", "clicked", vcard_dialog_avatar_clicked_cb,
			      NULL);

	g_object_add_weak_pointer (G_OBJECT (dialog->dialog), (gpointer) &dialog);

	gossip_glade_setup_size_group (glade,
				       GTK_SIZE_GROUP_HORIZONTAL,
				       "label_account",
				       "label_name",
				       "label_nickname",
				       "label_email",
				       "label_web_site",
				       "label_birthday",
				       "label_avatar",
				       "label_description",
				       NULL);

	g_object_unref (glade);

	/* Birthday chooser */
	dialog->button_birthday = gossip_popup_button_new (_("Change"));
	gtk_box_pack_start (GTK_BOX (birthday_placeholder), dialog->button_birthday, 0, FALSE, FALSE);
	gtk_widget_show (dialog->button_birthday);

	g_signal_connect (dialog->button_birthday,
			  "popup",
			  G_CALLBACK (vcard_dialog_birthday_button_popup_cb),
			  dialog);

	g_signal_connect (dialog->button_birthday,
			  "popdown",
			  G_CALLBACK (vcard_dialog_birthday_button_popdown_cb),
			  dialog);

	/* Sort out accounts */
	session = gossip_app_get_session ();

	dialog->account_chooser = gossip_account_chooser_new (session);
	gtk_box_pack_start (GTK_BOX (dialog->hbox_account),
			    dialog->account_chooser,
			    TRUE, TRUE, 0);

	g_signal_connect (dialog->account_chooser, "changed",
			  G_CALLBACK (vcard_dialog_account_changed_cb),
			  dialog);

	gtk_widget_show (dialog->account_chooser);

	/* Create the avatar chooser */
	vcard_dialog_create_avatar_chooser (dialog);

	/* Select first */
	accounts = gossip_session_get_accounts (session);
	if (g_list_length (accounts) > 1) {
		gtk_widget_show (dialog->hbox_account);
	} else {
		/* Show no accounts combo box */
		gtk_widget_hide (dialog->hbox_account);
	}

	g_list_foreach (accounts, (GFunc) g_object_unref, NULL);
	g_list_free (accounts);

	/* Set up transient parent */
	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog), parent);
	}

	gtk_widget_show (dialog->dialog);

	vcard_dialog_lookup_start (dialog);
}
