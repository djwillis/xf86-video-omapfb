/* Image format conversions
 * Copyright 2008 Kalle Vahlman, <zuh@iki.fi>
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

#ifndef __IMAGE_FORMAT_CONVERSIONS_H__
#define __IMAGE_FORMAT_CONVERSIONS_H__

/* Basic line-based copy for packed formats */
void packed_line_copy(int w, int h, int stride, uint8_t *src, uint8_t *dest);

/* Basic C implementation of YV12/I420 to UYVY conversion */
void uv12_to_uyvy(int w, int h, int y_pitch, int uv_pitch, uint8_t *y_p, uint8_t *u_p, uint8_t *v_p, uint8_t *dest);


#endif /* __IMAGE_FORMAT_CONVERSIONS_H__ */

