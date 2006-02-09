/*
 * @file gaim_buffer.h Buffer Utility Functions
 * @ingroup core
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "internal.h"

#include "gaim_buffer.h"

#define DEFAULT_BUF_SIZE 256

GaimCircBuffer *
gaim_circ_buffer_new(gsize growsize) {
	GaimCircBuffer *buf = g_new0(GaimCircBuffer, 1);
	buf->growsize = growsize ? growsize : DEFAULT_BUF_SIZE;
	return buf;
}

void gaim_circ_buffer_destroy(GaimCircBuffer *buf) {
	g_free(buf->buffer);
	g_free(buf);
}

static void grow_circ_buffer(GaimCircBuffer *buf, gsize len) {
	int in_offset = 0, out_offset = 0;
	int start_buflen = buf->buflen;

	while ((buf->buflen - buf->bufused) < len)
		buf->buflen += buf->growsize;

	if (buf->inptr != NULL) {
		in_offset = buf->inptr - buf->buffer;
		out_offset = buf->outptr - buf->buffer;
	}
	buf->buffer = g_realloc(buf->buffer, buf->buflen);

	/* adjust the fill and remove pointer locations */
	if (buf->inptr == NULL) {
		buf->inptr = buf->outptr = buf->buffer;
	} else {
		buf->inptr = buf->buffer + in_offset;
		buf->outptr = buf->buffer + out_offset;
	}

	/* If the fill pointer is wrapped to before the remove
	 * pointer, we need to shift the data */
	if (in_offset < out_offset) {
		int shift_n = MIN(buf->buflen - start_buflen,
			in_offset);
		memcpy(buf->buffer + start_buflen, buf->buffer,
			shift_n);

		/* If we couldn't fit the wrapped read buffer
		 * at the end */
		if (shift_n < in_offset) {
			memmove(buf->buffer,
				buf->buffer + shift_n,
				in_offset - shift_n);
			buf->inptr = buf->buffer +
				(in_offset - shift_n);
		} else {
			buf->inptr = buf->buffer +
				start_buflen + in_offset;
		}
	}
}

void gaim_circ_buffer_append(GaimCircBuffer *buf, gconstpointer src, gsize len) {

	int len_stored;

	/* Grow the buffer, if necessary */
	if ((buf->buflen - buf->bufused) < len)
		grow_circ_buffer(buf, len);

	/* If we may need to wrap */
	if ((buf->inptr - buf->outptr) >= 0)
		len_stored = MIN(len, buf->buflen
			- (buf->inptr - buf->buffer));
	else
		len_stored = len;

	memcpy(buf->inptr, src, len_stored);

	if (len_stored < len) {
		memcpy(buf->buffer, src + len_stored, len - len_stored);
		buf->inptr = buf->buffer + (len - len_stored);
	} else if ((buf->buffer - buf->inptr) == len_stored) {
		buf->inptr = buf->buffer;
	} else {
		buf->inptr += len_stored;
	}

	buf->bufused += len;
}

gsize gaim_circ_buffer_get_max_read(GaimCircBuffer *buf) {
	int max_read;

	if (buf->bufused == 0)
		max_read = 0;
	else if ((buf->outptr - buf->inptr) >= 0)
		max_read = buf->buflen - (buf->outptr - buf->buffer);
	else
		max_read = buf->inptr - buf->outptr;

	return max_read;
}

gboolean gaim_circ_buffer_mark_read(GaimCircBuffer *buf, gsize len) {
	g_return_val_if_fail(gaim_circ_buffer_get_max_read(buf) >= len, FALSE);

	buf->outptr += len;
	buf->bufused -= len;
	/* wrap to the start if we're at the end */
	if ((buf->outptr - buf->buffer) == buf->buflen)
		buf->outptr = buf->buffer;

	return TRUE;
}

