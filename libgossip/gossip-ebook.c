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
 * Author: Brian Pepple <bpepple@fedoraproject.org>
 *         Martyn Russell <martyn@imendio.com>
 */

#include <config.h>

#include <string.h>

#include <libebook/e-book.h>

#include "gossip-avatar.h"
#include "gossip-debug.h"
#include "gossip-ebook.h"

#define DEBUG_DOMAIN "EBook"

#define EVO_UNKNOWN_IMAGE "image/X-EVOLUTION-UNKNOWN"

static EContact *me = NULL;
static EBook    *book = NULL;

static void
ebook_init (void)
{
	GError *error = NULL;

	if (me || book) {
		/* Already initialised */
		return;
	}

	if (!e_book_get_self (&me, &book, &error)) {
		gossip_debug (DEBUG_DOMAIN,
			      "Failed to get address book: %s",
			      error->message);
		g_error_free (error);
	}
}

static void
ebook_term (void) 
{
	if (me) {
		g_object_unref (me);
		me = NULL;
	}

	if (book) {
		g_object_unref (book);
		book = NULL;
	}
}

/*
 * This function isn't probably needed, but for now 
 * I'll leave it here in case a use for it shows up.
 */
gboolean
gossip_ebook_set_jabber_id (const gchar *id)
{
	GError   *error = NULL;
	gboolean  success;

	g_return_val_if_fail (id != NULL, FALSE);

	ebook_init ();

	gossip_debug (DEBUG_DOMAIN, 
		      "Setting Jabber ID to '%s'", 
		      id);

	e_contact_set (me, E_CONTACT_IM_JABBER_HOME_1, (const gpointer) id);

	success = e_book_commit_contact (book, me, &error);

	if (!success) {
		gossip_debug (DEBUG_DOMAIN,
			      "Failed to set Jabber ID to '%s', %s",
			      id,
			      error->message);
		g_error_free (error);
	}

	ebook_term ();

	return success;
}

/*
 * Future TODO: Have this return a list of all the jabber id's,
 *              and use that to populate a Login ID combo entry box
 *              in the Accounts dialog.
 */
gchar *
gossip_ebook_get_jabber_id (void)
{
	gchar *id;

	ebook_init ();
	id = e_contact_get (me, E_CONTACT_IM_JABBER_HOME_1);
	ebook_term ();

	return id;
}

gboolean
gossip_ebook_set_name (const gchar *name)
{
	GError   *error = NULL;
	gboolean  success;

	g_return_val_if_fail (name != NULL, FALSE);

	ebook_init ();

	gossip_debug (DEBUG_DOMAIN, 
		      "Setting name to '%s'", 
		      name);

	e_contact_set (me, E_CONTACT_FULL_NAME, (const gpointer) name);

	success = e_book_commit_contact (book, me, &error);
	if (!success) {
		gossip_debug (DEBUG_DOMAIN,
			      "Failed to set name to '%s', %s",
			      name,
			      error->message);
		g_error_free (error);
	}

	ebook_term ();

	return success;
}

gchar *
gossip_ebook_get_name (void)
{
	gchar *name;

	ebook_init ();

	name = e_contact_get (me, E_CONTACT_FULL_NAME);

	ebook_term ();

	return name;
}

gboolean
gossip_ebook_set_nickname (const gchar *nickname)
{
	GError   *error = NULL;
	gboolean  success;

	g_return_val_if_fail (nickname != NULL, FALSE);

	ebook_init ();
	
	gossip_debug (DEBUG_DOMAIN, 
		      "Setting nickname to '%s'", 
		      nickname);

	e_contact_set (me, E_CONTACT_NICKNAME, (const gpointer) nickname);

	success = e_book_commit_contact (book, me, &error);
	
	if (!success) {
		gossip_debug (DEBUG_DOMAIN,
			      "Failed to set nickname to '%s', %s",
			      nickname,
			      error->message);
		g_error_free (error);
	}

	ebook_term ();

	return success;
}

gchar *
gossip_ebook_get_nickname (void)
{
	gchar *nickname;

	ebook_init ();

	nickname = e_contact_get (me, E_CONTACT_NICKNAME);
	
	ebook_term ();

	return nickname;
}


gboolean
gossip_ebook_set_email (const gchar *email)
{
	GError   *error = NULL;
	gboolean  success;

	g_return_val_if_fail (email != NULL, FALSE);

	ebook_init ();

	gossip_debug (DEBUG_DOMAIN, 
		      "Setting email to '%s'", 
		      email);

	e_contact_set (me, E_CONTACT_EMAIL_1, (const gpointer) email);

	success = e_book_commit_contact (book, me, &error);

	if (!success) {
		gossip_debug (DEBUG_DOMAIN,
			      "Failed to set email to '%s', %s",
			      email,
			      error->message);
		g_error_free (error);
	}

	ebook_term ();

	return success;
}

gchar *
gossip_ebook_get_email (void)
{
	gchar *email;

	ebook_init ();

	email = e_contact_get (me, E_CONTACT_EMAIL_1);

	ebook_term ();
	
	return email;
}

gboolean
gossip_ebook_set_website (const gchar *website)
{
	GError   *error = NULL;
	gboolean  success;

	g_return_val_if_fail (website != NULL, FALSE);

	ebook_init ();

	gossip_debug (DEBUG_DOMAIN, 
		      "Setting website to '%s'", 
		      website);
	
	e_contact_set (me, E_CONTACT_HOMEPAGE_URL, (const gpointer) website);

	success = e_book_commit_contact (book, me, &error);

	if (!success) {
		gossip_debug (DEBUG_DOMAIN,
			      "Failed to set website to '%s', %s",
			      website,
			      error->message);
		g_error_free (error);
	}

	ebook_term ();

	return success;
}

gchar *
gossip_ebook_get_website (void)
{
	gchar *website;

	ebook_init ();

	website = e_contact_get (me, E_CONTACT_HOMEPAGE_URL);
	
	ebook_term ();

	return website;
}

gchar *
gossip_ebook_get_birthdate (void)
{
	EContactDate *birthdate;
	gchar        *str;
	gint          year, month, day;
	GDate        *date;
	const gchar  *format = "%x"; /* Keep in variable get rid of warning. */
	gchar         buf[128];

	ebook_init ();
	
	birthdate = e_contact_get (me, E_CONTACT_BIRTH_DATE);

	if (!birthdate) {
		ebook_term ();
		return NULL;
	}

	str = e_contact_date_to_string (birthdate);
	e_contact_date_free (birthdate);

	/* FIXME: Needs to work with different localisations */
	if (sscanf (str, "%04d-%02d-%02d", &year, &month, &day) != 3) {
		ebook_term ();
		return NULL;
	}

	date = g_date_new ();
	g_date_set_dmy (date, day, month, year);

	if (g_date_strftime (buf, sizeof (buf), format, date) > 0) {
		g_date_free (date);
		ebook_term ();
		return g_strdup (buf);
	}

	g_date_free (date);
	ebook_term ();

	return NULL;
}

gboolean
gossip_ebook_set_avatar (GossipAvatar *avatar)
{
	EContactPhoto *photo;
	GError        *error = NULL;
	gboolean       success;

	g_return_val_if_fail (avatar != NULL, FALSE);

	photo = g_new0 (EContactPhoto, 1);
	photo->data.inlined.length = avatar->len;
	photo->data.inlined.mime_type = g_strdup (avatar->format);
	photo->data.inlined.data = g_memdup (avatar->data, 
					     avatar->len);
	
	ebook_init ();

	gossip_debug (DEBUG_DOMAIN, 
		      "Setting avatar");

	e_contact_set (me, E_CONTACT_PHOTO, photo);
	e_contact_photo_free (photo);

	success = e_book_commit_contact (book, me, &error);

	if (!success) {
		gossip_debug (DEBUG_DOMAIN,
			      "Failed to set avatar, %s",
			      error->message);
		g_error_free (error);
	}

	ebook_term ();

	return success;
}

GossipAvatar *
gossip_ebook_get_avatar (void)
{
	EContactPhoto *photo;
	GossipAvatar  *avatar;

	ebook_init ();

	photo = e_contact_get (me, E_CONTACT_PHOTO);

	if (!photo) {
		ebook_term ();
		return NULL;
	}

	/* Return NULL, if eds avatar is set to Unknown */
	if (strcmp (EVO_UNKNOWN_IMAGE, (gchar*) photo->data.inlined.mime_type) == 0) {
		e_contact_photo_free (photo);
		ebook_term ();
		return NULL;
	}

	avatar = gossip_avatar_new ((guchar*) photo->data.inlined.data,
				    photo->data.inlined.length,
				    (gchar*) photo->data.inlined.mime_type);

	e_contact_photo_free (photo);
	ebook_term ();

	return avatar;
}

