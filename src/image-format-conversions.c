/* Image format conversions
 * Copyright 2008 Kalle Vahlman, <zuh@iki.fi>
 *                Ilpo Ruotsalainen, <lonewolf@iki.fi>
 *                Tuomas Kulve, <tuomas.kulve@movial.com>
 *                Ian Rickards, <ian.rickards@arm.com>
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
	int len = w * 2;
	for (i = 0; i < h; i++)
	{
		memcpy(dest + i * len, src + i * stride, len);
	}
}

/* Basic C implementation of YV12/I420 to UYVY conversion */
void uv12_to_uyvy(int w, int h, int y_pitch, int uv_pitch, uint8_t *y_p, uint8_t *u_p, uint8_t *v_p, uint8_t *dest)
{
	int x, y;
	uint8_t *dest_even = dest;
	uint8_t *dest_odd = dest + w * 2;
	uint8_t *y_p_even = y_p;
	uint8_t *y_p_odd = y_p + y_pitch;

	/*ErrorF("in uv12_to_uyvy, w: %d, pitch: %d\n", w, pitch);*/
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

		u_p += ((uv_pitch << 1) - w) >> 1;
		v_p += ((uv_pitch << 1) - w) >> 1;

		y_p_even += (y_pitch - w) + y_pitch;
		y_p_odd += (y_pitch - w) + y_pitch;
	}
}

#ifdef HAVE_NEON

void uv12_to_uyvy_neon(int w, int h, int y_pitch, int uv_pitch, uint8_t *y_p, uint8_t 
*u_p, uint8_t *v_p, uint8_t *dest)
{
    int x, y;
    uint8_t *dest_even = dest;
    uint8_t *dest_odd = dest + w * 2;
    uint8_t *y_p_even = y_p;
    uint8_t *y_p_odd = y_p + y_pitch;

    /*ErrorF("in uv12_to_uyvy, w: %d, pitch: %d\n", w, pitch);*/
    if (w<16)
    {
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

            u_p += ((uv_pitch << 1) - w) >> 1;
            v_p += ((uv_pitch << 1) - w) >> 1;

            y_p_even += (y_pitch - w) + y_pitch;
            y_p_odd += (y_pitch - w) + y_pitch;
        }
    }
    else
    {
        for (y=0; y<h; y+=2)
        {
            x=w;
            do {
                // avoid using d8-d15 (q4-q7) aapcs callee-save registers
                asm volatile (
                        "1:\n\t"
                        "vld1.u8   {d0}, [%[u_p]]!\n\t"
                        "sub       %[x],%[x],#16\n\t"
                        "cmp       %[x],#16\n\t"
                        "vld1.u8   {d1}, [%[v_p]]!\n\t"
                        "vld1.u8   {q1}, [%[y_p_even]]!\n\t"
                        "vzip.u8   d0, d1\n\t"
                        "vld1.u8   {q2}, [%[y_p_odd]]!\n\t"
                // use 2-element struct stores to zip up y with y&v
                        "vst2.u8   {q0,q1}, [%[dest_even]]!\n\t"
                        "vmov.u8   q1, q2\n\t"
                        "vst2.u8   {q0,q1}, [%[dest_odd]]!\n\t"
                        "bhs       1b\n\t"
                        : [u_p] "+r" (u_p), [v_p] "+r" (v_p), [y_p_even] "+r" (y_p_even), [y_p_odd] "+r" (y_p_odd),
                          [dest_even] "+r" (dest_even), [dest_odd] "+r" (dest_odd),
                          [x] "+r" (x)
                        :
                        : "cc", "memory", "d0","d1","d2","d3","d4","d5"
                        );
                if (x!=0)
                {
                    // overlap final 16-pixel block to process requested width exactly
                    x = 16-x;
                    u_p -= x/2;
                    v_p -= x/2;
                    y_p_even -= x;
                    y_p_odd -= x;
                    dest_even -= x*2;
                    dest_odd -= x*2;
                    x = 16;
                    // do another 16-pixel block
                }
            }
            while (x!=0);

            dest_even += w * 2;
            dest_odd += w * 2;

            u_p += ((uv_pitch << 1) - w) >> 1;
            v_p += ((uv_pitch << 1) - w) >> 1;

            y_p_even += (y_pitch - w) + y_pitch;
            y_p_odd += (y_pitch - w) + y_pitch;
        }
    }
}

#endif /* HAVE_NEON */
