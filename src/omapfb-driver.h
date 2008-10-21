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

#ifndef __OMAPFB_DRIVER_H__
#define __OMAPFB_DRIVER_H__

#include "xf86.h"
#include "exa.h"
#include "xf86xv.h"

#include <linux/fb.h>
#include "omapfb.h"

/* XV port */
typedef struct {
	int fd;
	unsigned char *fb;
	/* Non-changeable hardware info */
	struct fb_fix_screeninfo fixed_info;
	/* Per-mode state info */
	struct fb_var_screeninfo state_info;
	struct omapfb_mem_info mem_info;
	struct omapfb_caps caps;
	struct omapfb_plane_info plane_info;
	struct omapfb_update_window update_window;
} OMAPFBPortRec, *OMAPFBPortPtr;

typedef struct {
	int fd;
	unsigned char *fb;
	/* Non-changeable hardware info */
	struct fb_fix_screeninfo fixed_info;
	/* Per-mode state info */
	struct fb_var_screeninfo state_info;
	struct omapfb_mem_info mem_info;
	struct omapfb_caps caps;
	struct omapfb_plane_info plane_info;

	/* LCD controller name */
	char ctrl_name[32];
	
	OMAPFBPortPtr port;

	CloseScreenProcPtr CloseScreen;
	DisplayModeRec default_mode;

	ExaDriverPtr exa;
} OMAPFBRec, *OMAPFBPtr;

#define OMAPFB(p) ((OMAPFBPtr)((p)->driverPrivate))

void OMAPFBPrintCapabilities(ScrnInfoPtr pScrn,
                             struct omapfb_caps *caps,
                             const char *plane_name);

Bool OMAPFBSetupExa(OMAPFBPtr ofb);
int OMAPFBXVInit (ScrnInfoPtr pScrn, XF86VideoAdaptorPtr **omap_adaptors);

#endif /* __OMAPFB_DRIVER_H__ */

