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

#include "gossip-jabber-ft-utils.h"

#define DEBUG_MSG(x) 
/* #define DEBUG_MSG(args) g_printerr args ; g_printerr ("\n"); */


static void base64_init (void);


/*
 * taken from libgsf
 */

static guint8 base64_rank[256];
static char const *base64_alphabet =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* line length for base64 encoding, must be a multiple of 4. */
enum { BASE64_LINE_LEN = 76 };


static void
base64_init (void)
{
	static gboolean inited = FALSE;
	int             i;

	if (inited) {
		return;
	} else {
		inited = TRUE;
	}

	memset (base64_rank, 0xff, sizeof (base64_rank));
	for (i = 0; i < 64; i++) {
		base64_rank[(unsigned int)base64_alphabet[i]] = i;
	}

	base64_rank['='] = 0;
}

/* call this when finished encoding everything, to
   flush off the last little bit */
size_t
gossip_jabber_ft_base64_encode_close (guint8 const *in, 
				      size_t        inlen,
				      gboolean      break_lines, 
				      guint8       *out, 
				      int          *state,
				      unsigned int *save)
{
	int     c1, c2;
	guint8 *outptr = out;

 	base64_init ();

	if (inlen > 0) {
		outptr += gossip_jabber_ft_base64_encode_step (in, inlen, 
							       break_lines, outptr, 
							       state, save);
	}

	c1 = ((guint8 *)save)[1];
	c2 = ((guint8 *)save)[2];
	
	DEBUG_MSG (("Base64: mode = %d\nc1 = %c\nc2 = %c",
		    (int)((char *)save)[0],
		    (int)((char *)save)[1],
		    (int)((char *)save)[2]));
	
	switch (((char *)save)[0]) {
	case 2:
		outptr[2] = base64_alphabet[ ( (c2 &0x0f) << 2 ) ];
		g_assert(outptr[2] != 0);
		goto skip;
	case 1:
		outptr[2] = '=';
	skip:
		outptr[0] = base64_alphabet[ c1 >> 2 ];
		outptr[1] = base64_alphabet[ c2 >> 4 | ( (c1&0x3) << 4 )];
		outptr[3] = '=';
		outptr += 4;
		++*state;
		break;
	}
	if (break_lines && *state > 0)
		*outptr++ = '\n';

	*save = 0;
	*state = 0;

	return outptr-out;
}

/*
  performs an 'encode step', only encodes blocks of 3 characters to the
  output at a time, saves left-over state in state and save (initialise to
  0 on first invocation).
*/
size_t
gossip_jabber_ft_base64_encode_step (guint8 const *in, 
				     size_t        len,
				     gboolean      break_lines, 
				     guint8       *out, 
				     int          *state, 
				     unsigned int *save)
{
	register guint8 const *inptr;
	register guint8       *outptr;

 	base64_init ();

	if (len <= 0) {
		return 0;
	}

	inptr = in;
	outptr = out;

	DEBUG_MSG (("Base64: we have %d chars, and %d saved chars", len, ((char *)save)[0]));

	if (len + ((char *)save)[0] > 2) {
		guint8 const *inend = in+len-2;
		register      int c1, c2, c3;
		register      int already;

		already = *state;

		switch (((char *)save)[0]) {
		case 1:	c1 = ((guint8 *)save)[1]; goto skip1;
		case 2:	c1 = ((guint8 *)save)[1];
			c2 = ((guint8 *)save)[2]; goto skip2;
		}
		
		/* yes, we jump into the loop, no i'm not going to change it, it's beautiful! */
		while (inptr < inend) {
			c1 = *inptr++;
		skip1:
			c2 = *inptr++;
		skip2:
			c3 = *inptr++;
			*outptr++ = base64_alphabet[ c1 >> 2 ];
			*outptr++ = base64_alphabet[ c2 >> 4 | ( (c1&0x3) << 4 ) ];
			*outptr++ = base64_alphabet[ ((c2 &0x0f) << 2 ) | (c3 >> 6) ];
			*outptr++ = base64_alphabet[ c3 & 0x3f ];

			/* this is a bit ugly ... */
			if (break_lines && (++already) * 4 >= BASE64_LINE_LEN) {
				*outptr++ = '\n';
				already = 0;
			}
		}

		((char *)save)[0] = 0;
		len = 2-(inptr-inend);
		*state = already;
	}

	DEBUG_MSG (("Base64: state = %d, len = %d",
		    (int)((char *)save)[0], len));

	if (len > 0) {
		register char *saveout;

		/* points to the slot for the next char to save */
		saveout = & (((char *)save)[1]) + ((char *)save)[0];

		/* len can only be 0 1 or 2 */
		switch(len) {
		case 2:	*saveout++ = *inptr++;
		case 1:	*saveout++ = *inptr++;
		}
		((char*)save)[0] += len;
	}

	DEBUG_MSG (("Base64: mode = %d\nc1 = %c\nc2 = %c",
		    (int)((char *)save)[0],
		    (int)((char *)save)[1],
		    (int)((char *)save)[2]));
	
	return outptr-out;
}

size_t
gossip_jabber_ft_base64_decode_step (guint8 const *in, 
				     size_t        len,
				     guint8       *out,
				     int          *state, 
				     guint        *save)
{
	register guint8 const *inptr;
	register guint8       *outptr, c;
	register unsigned int  v;
	guint8 const          *inend;
	int                    i;

 	base64_init ();

	inend = in+len;
	outptr = out;

	/* convert 4 base64 bytes to 3 normal bytes */
	v = *save;
	i = *state;
	inptr = in;

	while (inptr < inend) {
		c = base64_rank[*inptr++];
		if (c != 0xff) {
			v = (v << 6) | c;
			i++;
			if (i == 4) {
				*outptr++ = v >> 16;
				*outptr++ = v >> 8;
				*outptr++ = v;
				i = 0;
			}
		}
	}

	*save = v;
	*state = i;

	/* quick scan back for '=' on the end somewhere */
	/* fortunately we can drop 1 output char for each trailing = (upto 2) */
	i = 2;
	while (inptr > in && i) {
		inptr--;
		if (base64_rank[*inptr] != 0xff) {
			if (*inptr == '=' && outptr > out)
				outptr--;
			i--;
		}
	}

	/* if i!= 0 then there is a truncation error! */
	return outptr-out;
}

guint8 *
gossip_jabber_ft_base64_encode_simple (guint8 const *data, 
				       size_t        len)
{
	guint8       *out;
	int           state = 0, outlen;
	unsigned int  save = 0;
	gboolean      break_lines = TRUE;

 	base64_init ();

	outlen = len * 4 / 3 + 5;
	if (break_lines) {
		outlen += outlen / BASE64_LINE_LEN + 1;
	}

	out = g_new (guint8, outlen);
	outlen = gossip_jabber_ft_base64_encode_close (data, len, break_lines,
						       out, &state, &save);
	out [outlen] = '\0';
	return out;
}

size_t
gossip_jabber_ft_base64_decode_simple (guint8 *data, 
				       size_t  len)
{
	int          state = 0;
	unsigned int save = 0;

	base64_init ();

	return gossip_jabber_ft_base64_decode_step (data, len, data, &state, &save);
}


