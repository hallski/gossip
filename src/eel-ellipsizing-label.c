/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-ellipsizing-label.c: Subclass of GtkLabel that ellipsizes the text.

   Copyright (C) 2001 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: John Sullivan <sullivan@eazel.com>,
 */

#include <config.h>
#include <string.h>
#include "eel-ellipsizing-label.h"

                                                                                                            
#define EEL_CLASS_BOILERPLATE(class_name, prefix, parent_class_type)          \
        EEL_BOILERPLATE (class_name, class_name, prefix, parent_class_type,   \
                         EEL_REGISTER_TYPE)
#define EEL_REGISTER_TYPE(class_name, corba_name)                             \
        g_type_register_static (parent_type, #class_name, &info, 0)
                                                                                                            
#define EEL_BOILERPLATE(class_name, corba_name, prefix, parent_class_type,    \
                        register_type)                                        \
                                                                              \
static gpointer parent_class;                                                 \
                                                                              \
GtkType                                                                       \
prefix##_get_type (void)                                                      \
{                                                                             \
        GtkType parent_type;                                                  \
        static GtkType type;                                                  \
                                                                              \
        if (type == 0) {                                                      \
                static GTypeInfo info = {                                     \
                        sizeof (class_name##Class),                           \
                        NULL, NULL,                                           \
                        (GClassInitFunc) prefix##_class_init,                 \
                        NULL, NULL,                                           \
                        sizeof (class_name), 0,                               \
                        (GInstanceInitFunc) prefix##_init,                    \
                        NULL                                                  \
                };                                                            \
                                                                              \
                parent_type = (parent_class_type);                            \
                type = register_type (class_name, corba_name);                \
                parent_class = g_type_class_ref (parent_type);                \
        }                                                                     \
                                                                              \
        return type;                                                          \
}

#define EEL_CALL_PARENT(parent_class_cast_macro, signal, parameters)          \
                                                                              \
G_STMT_START {                                                                \
        if (parent_class_cast_macro (parent_class)->signal != NULL) {         \
                (* parent_class_cast_macro (parent_class)->signal) parameters;\
        }                                                                     \
} G_STMT_END

#define ELLIPSIS "..."

/* Caution: this is an _expensive_ function */
static int
measure_string_width (const char  *string,
		      PangoLayout *layout)
{
	int width;
	
	pango_layout_set_text (layout, string, -1);
	pango_layout_get_pixel_size (layout, &width, NULL);

	return width;
}

/* this is also plenty slow */
static void
compute_character_widths (const char    *string,
			  PangoLayout   *layout,
			  int           *char_len_return,
			  int          **widths_return,
			  int          **cuts_return)
{
	int *widths;
	int *offsets;
	int *cuts;
	int char_len;
	int byte_len;
	const char *p;
	int i;
	PangoLayoutIter *iter;
	PangoLogAttr *attrs;
	
#define BEGINS_UTF8_CHAR(x) (((x) & 0xc0) != 0x80)
	
	char_len = g_utf8_strlen (string, -1);
	byte_len = strlen (string);
	
	widths = g_new (int, char_len);
	offsets = g_new (int, byte_len);

	/* Create a translation table from byte index to char offset */
	p = string;
	i = 0;
	while (*p) {
		int byte_index = p - string;
		
		if (BEGINS_UTF8_CHAR (*p)) {
			offsets[byte_index] = i;
			++i;
		} else {
			offsets[byte_index] = G_MAXINT; /* segv if we try to use this */
		}
		
		++p;
	}

	/* Now fill in the widths array */
	pango_layout_set_text (layout, string, -1);
	
	iter = pango_layout_get_iter (layout);

	do {
		PangoRectangle extents;
		int byte_index;

		byte_index = pango_layout_iter_get_index (iter);

		if (byte_index < byte_len) {
			pango_layout_iter_get_char_extents (iter, &extents);
			
			g_assert (BEGINS_UTF8_CHAR (string[byte_index]));
			g_assert (offsets[byte_index] < char_len);
			
			widths[offsets[byte_index]] = PANGO_PIXELS (extents.width);
		}
		
	} while (pango_layout_iter_next_char (iter));

	pango_layout_iter_free (iter);

	g_free (offsets);
	
	*widths_return = widths;

	/* Now compute character offsets that are legitimate places to
	 * chop the string
	 */
	attrs = g_new (PangoLogAttr, char_len + 1);
	
	pango_get_log_attrs (string, byte_len, -1,
			     pango_context_get_language (
				     pango_layout_get_context (layout)),
			     attrs,
			     char_len + 1);

	cuts = g_new (int, char_len);
	i = 0;
	while (i < char_len) {
		cuts[i] = attrs[i].is_cursor_position;

		++i;
	}

	g_free (attrs);

	*cuts_return = cuts;

	*char_len_return = char_len;
}

static char *
eel_string_ellipsize_end (const char *string, PangoLayout *layout, int width)
{
	int resulting_width;
	int *cuts;
	int *widths;
	int char_len;
	const char *p;
	int truncate_offset;
	char *result;
	
	/* See explanatory comments in ellipsize_start */
	
	if (*string == '\0')
		return g_strdup ("");

	resulting_width = measure_string_width (string, layout);
	
	if (resulting_width <= width) {
		return g_strdup (string);
	}

	width -= measure_string_width (ELLIPSIS, layout);

	if (width < 0) {
		return g_strdup ("");
	}
	
	compute_character_widths (string, layout, &char_len, &widths, &cuts);
	
        for (truncate_offset = char_len - 1; truncate_offset > 0; truncate_offset--) {
        	resulting_width -= widths[truncate_offset];
        	if (resulting_width <= width &&
		    cuts[truncate_offset]) {
			break;
        	}
        }

	g_free (cuts);
	g_free (widths);

	p = g_utf8_offset_to_pointer (string, truncate_offset);
	
	result = g_malloc ((p - string) + strlen (ELLIPSIS) + 1);
	memcpy (result, string, (p - string));
	strcpy (result + (p - string), ELLIPSIS);

	return result;
}

/**
 * eel_pango_layout_set_text_ellipsized
 *
 * @layout: a pango layout
 * @string: A a string to be ellipsized.
 * @width: Desired maximum width in points.
 * @mode: The desired ellipsizing mode.
 * 
 * Truncates a string if required to fit in @width and sets it on the
 * layout. Truncation involves removing characters from the start, middle or end
 * respectively and replacing them with "...". Algorithm is a bit
 * fuzzy, won't work 100%.
 * 
 */
static void
eel_pango_layout_set_text_ellipsized (PangoLayout  *layout,
				      const char   *string,
				      int           width)
{
	char *s;

	g_return_if_fail (PANGO_IS_LAYOUT (layout));
	g_return_if_fail (string != NULL);
	g_return_if_fail (width >= 0);
	
	s = eel_string_ellipsize_end (string, layout, width);
	pango_layout_set_text (layout, s, -1);
	g_free (s);
}

static int
eel_strcmp (const char *string_a, const char *string_b)
{
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL < ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	return strcmp (string_a == NULL ? "" : string_a,
		       string_b == NULL ? "" : string_b);
}

static gboolean
eel_str_is_equal (const char *string_a, const char *string_b)
{
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL != ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	return eel_strcmp (string_a, string_b) == 0;
}


struct EelEllipsizingLabelDetails
{
	char *full_text;
};

static void eel_ellipsizing_label_class_init (EelEllipsizingLabelClass *class);
static void eel_ellipsizing_label_init       (EelEllipsizingLabel      *label);

EEL_CLASS_BOILERPLATE (EelEllipsizingLabel, eel_ellipsizing_label, GTK_TYPE_LABEL)

static void
eel_ellipsizing_label_init (EelEllipsizingLabel *label)
{
	label->details = g_new0 (EelEllipsizingLabelDetails, 1);
}

static void
real_finalize (GObject *object)
{
	EelEllipsizingLabel *label;

	label = EEL_ELLIPSIZING_LABEL (object);

	g_free (label->details->full_text);
	g_free (label->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

GtkWidget*
eel_ellipsizing_label_new (const char *string)
{
	EelEllipsizingLabel *label;
  
	label = g_object_new (EEL_TYPE_ELLIPSIZING_LABEL, NULL);
	eel_ellipsizing_label_set_text (label, string);
  
	return GTK_WIDGET (label);
}

void
eel_ellipsizing_label_set_text (EelEllipsizingLabel *label, 
				const char          *string)
{
	g_return_if_fail (EEL_IS_ELLIPSIZING_LABEL (label));

	if (eel_str_is_equal (string, label->details->full_text)) {
		return;
	}

	g_free (label->details->full_text);
	label->details->full_text = g_strdup (string);

	/* Queues a resize as side effect */
	gtk_label_set_text (GTK_LABEL (label), label->details->full_text);
}

static void
real_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_request, (widget, requisition));

	/* Don't demand any particular width; will draw ellipsized into whatever size we're given */
	requisition->width = 0;
}

static void	
real_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	EelEllipsizingLabel *label;

	label = EEL_ELLIPSIZING_LABEL (widget);
	
	/* This is the bad hack of the century, using private
	 * GtkLabel layout object. If the layout is NULL
	 * then it got blown away since size request,
	 * we just punt in that case, I don't know what to do really.
	 */

	if (GTK_LABEL (label)->layout != NULL) {
		if (label->details->full_text == NULL) {
			pango_layout_set_text (GTK_LABEL (label)->layout, "", -1);
		} else {
			eel_pango_layout_set_text_ellipsized (GTK_LABEL (label)->layout,
							      label->details->full_text,
							      allocation->width);
		}
	}
	
	EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));
}

static gboolean
real_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
	EelEllipsizingLabel *label;
	GtkRequisition req;
	
	label = EEL_ELLIPSIZING_LABEL (widget);

	/* push/pop the actual size so expose draws in the right
	 * place, yes this is bad hack central. Here we assume the
	 * ellipsized text has been set on the layout in size_allocate
	 */
	EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_request, (widget, &req));
	widget->requisition.width = req.width;
	EEL_CALL_PARENT (GTK_WIDGET_CLASS, expose_event, (widget, event));
	widget->requisition.width = 0;

	return FALSE;
}


static void
eel_ellipsizing_label_class_init (EelEllipsizingLabelClass *klass)
{
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (klass);

	G_OBJECT_CLASS (klass)->finalize = real_finalize;

	widget_class->size_request = real_size_request;
	widget_class->size_allocate = real_size_allocate;
	widget_class->expose_event = real_expose_event;
}
