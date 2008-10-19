/* Texas Instruments OMAP framebuffer driver for X.Org
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

/*
 * This header holds definitions of platform-specific and generic XV functions
 * Note that there shouldn't be any ifdefs here, ifdef in the implementation
 * if it's neccessary (for eg. unsupported assembly code)
 */

#ifndef __OMAPFB_XV_PLATFORM_H__
#define __OMAPFB_XV_PLATFORM_H__

#include "omapfb-driver.h"

enum omapfb_color_format xv_to_omapfb_format(int format);
int OMAPXVAllocPlane(ScrnInfoPtr pScrn);
int OMAPXVSetupVideoPlane(ScrnInfoPtr pScrn);

int OMAPFBXVPutImageGeneric (ScrnInfoPtr pScrn,
                             short src_x, short src_y, short drw_x, short drw_y,
                             short src_w, short src_h, short drw_w, short drw_h,
                             int image, char *buf, short width, short height,
                             Bool sync, RegionPtr clipBoxes, pointer data);
int OMAPFBXVStopVideoGeneric (ScrnInfoPtr pScrn, pointer data, Bool cleanup);

/* Blizzard is Epson S1D13745A01, found on eg. Nokia N8x0 */
int OMAPFBXVPutImageBlizzard (ScrnInfoPtr pScrn,
                             short src_x, short src_y, short drw_x, short drw_y,
                             short src_w, short src_h, short drw_w, short drw_h,
                             int image, char *buf, short width, short height,
                             Bool sync, RegionPtr clipBoxes, pointer data);
int OMAPFBXVStopVideoBlizzard (ScrnInfoPtr pScrn, pointer data, Bool cleanup);

#endif /* __OMAPFB_XV_PLATFORM_H__ */

