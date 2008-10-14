/* Image format conversions
 * Copyright 2008 Kalle Vahlman, <zuh@iki.fi>
 *                Ilpo Ruotsalainen, <lonewolf@iki.fi>
 *                
 *
 * Permission to use, copy, modify, distribute and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of the authors and/or copyright holders
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  The authors and
 * copyright holders make no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without any express
 * or implied warranty.
 *
 * THE AUTHORS AND COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file contains conversion functions for different image formats
 *
 */

#include <stdint.h>
#include <string.h>

/* Basic line-based copy for packed formats */
void packed_line_copy(int w, int h, int stride, uint8_t *src, uint8_t *dest)
{
	int i;
	for (i = 0; i < h; i++)
	{
		int len = w * 2;
		memcpy(dest + i * len, src + i * stride, len);
	}
}

/* Basic C implementation of YV12/I420 to UYVY conversion */
void uv12_to_uyvy(int w, int h, uint8_t *y_p, uint8_t *u_p, uint8_t *v_p, uint8_t *dest)
{
	int x, y;
	uint8_t *dest_even = dest;
	uint8_t *dest_odd = dest + w * 2;
	uint8_t *y_p_even = y_p;
	uint8_t *y_p_odd = y_p + w;

	for (y=0; y<h; y+=2)
	{
		for (x=0; x<w; x+=2)
		{
			/* Output two 2x1 macroblocks to form a 2x2 block from input */
			uint8_t u_val = *u_p++;
			uint8_t v_val = *v_p++;

			/* Even row, first pixel */
			*dest_even++ = u_val;
			*dest_even++ = *y_p_even++;

			/* Even row, second pixel */
			*dest_even++ = v_val;
			*dest_even++ = *y_p_even++;

			/* Odd row, first pixel */
			*dest_odd++ = u_val;
			*dest_odd++ = *y_p_odd++;

			/* Odd row, second pixel */
			*dest_odd++ = v_val;
			*dest_odd++ = *y_p_odd++;
		}

		dest_even += w * 2;
		dest_odd += w * 2;

		y_p_even += w;
		y_p_odd += w;
	}
}

