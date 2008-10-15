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
 * PutImage implementation for the Epson S1D13745 aka Blizzard LCD controller,
 * found on eg. Nokia N8x0
 */

#include "xf86.h"
#include "xf86_OSlib.h"
#include "xf86xv.h"
#include "fourcc.h"

#include <X11/extensions/Xv.h>

#include "omapfb-driver.h"
#include "omapfb-xv-platform.h"

/* Blizzard is Epson S1D13745A01, found on eg. Nokia N8x0 */
int OMAPFBXVPutImageBlizzard (ScrnInfoPtr pScrn,
                              short src_x, short src_y, short drw_x, short drw_y,
                              short src_w, short src_h, short drw_w, short drw_h,
                              int image, char *buf, short width, short height,
                              Bool sync, RegionPtr clipBoxes, pointer data)
{
	OMAPFBPtr ofb = OMAPFB(pScrn);

	if (!ofb->port->plane_info.enabled
	 || ofb->port->update_window.x != src_x
	 || ofb->port->update_window.y != src_y
	 || ofb->port->update_window.width != src_w
	 || ofb->port->update_window.height != src_h
	 || ofb->port->update_window.format != xv_to_omapfb_format(image)
	 || ofb->port->update_window.out_x != drw_x
	 || ofb->port->update_window.out_y != drw_y
	 || ofb->port->update_window.out_width != drw_w
	 || ofb->port->update_window.out_height != drw_h)
	{
		int ret;
		
		/* Currently this is only used to track the plane state */
		ofb->port->update_window.x = src_x;
		ofb->port->update_window.y = src_y;
	 	ofb->port->update_window.width = src_w;
	 	ofb->port->update_window.height = src_h;
	 	ofb->port->update_window.format = xv_to_omapfb_format(image);
	 	ofb->port->update_window.out_x = drw_x;
	 	ofb->port->update_window.out_y = drw_y;
	 	ofb->port->update_window.out_width = drw_w;
	 	ofb->port->update_window.out_height = drw_h;

		/* If we don't have the plane running, enable it */
		if (!ofb->port->plane_info.enabled) {
			ret = OMAPXVAllocPlane(pScrn);
			if (ret != Success)
				return ret;
		}

		/* Set up the state info, xres and yres will be used for
		 * scaling to the values in the plane info strurct
		 */
		ofb->port->state_info.xres = src_w & ~15;
		ofb->port->state_info.yres = src_h & ~15;
		ofb->port->state_info.xres_virtual = 0;
		ofb->port->state_info.yres_virtual = 0;
		ofb->port->state_info.xoffset = 0;
		ofb->port->state_info.yoffset = 0;
		ofb->port->state_info.rotate = 0;
		ofb->port->state_info.grayscale = 0;
		ofb->port->state_info.activate = FB_ACTIVATE_NOW;
		ofb->port->state_info.bits_per_pixel = 0;
		ofb->port->state_info.nonstd = xv_to_omapfb_format(image);

		/* Set up the video plane info */
		ofb->port->plane_info.enabled = 1;
		ofb->port->plane_info.pos_x = drw_x;
		ofb->port->plane_info.pos_y = drw_y;
		ofb->port->plane_info.out_width = drw_w & ~15;
		ofb->port->plane_info.out_height = drw_h & ~15;

		ret = OMAPXVSetupVideoPlane(pScrn);
		if (ret != Success)
			return ret;

	}

	switch (image)
	{
		/* Packed formats carry the YUV (luma and 2 chroma values, ie.
		 * brightness and 2 color description values) packed in
		 * two-byte macropixels. Each macropixel translates to two
		 * pixels on screen.
		 */
		case FOURCC_UYVY:
			/* UYVY is packed like this: [U Y1 | V Y2] */
		case FOURCC_YUY2:
			/* YUY2 is packed like this: [Y1 U | Y2 V] */
		{
			packed_line_copy(src_w & ~15,
			                 src_h & ~15,
			                 ((src_w + 1) & ~1) * 2,
			                 (uint8_t*)buf,
			                 (uint8_t*)ofb->port->fb);
			break;
		}

		/* Planar formats (as the name says) have the YUV colorspace
		 * components separated to individual planes. The Y plane is
		 * full resolution, while the U and V planes are 1/4th (both
		 * dimensions divided by 2) so a macropixel translates to
		 * 2x2 pixels on screen
		 */

		/* TODO: these should convert to the custom format of
		 * blizzard, since it supports something resembling
		 * YUV420. Comment excerpt from xomap:
			 
 * Copy I420 data to the custom 'YUV420' format, which is actually:
 * y11 u11,u12,u21,u22 u13,u14,u23,u24 y12 y14 y13
 * y21 v11,v12,v21,v22 v13,v14,v23,v24 y22 y24 y23
 *
 * The third and fourth luma components are swapped.  Yes, this is weird.

		 */

		case FOURCC_I420:
			/* I420 has plane order Y, U, V */
		{
			uint8_t *yb = buf;
			uint8_t *ub = yb + (src_w * src_h);
			uint8_t *vb = ub + ((src_w / 2) * (src_h / 2));
			uv12_to_uyvy(src_w & ~15,
			             src_h & ~15,
			             yb, ub, vb,
			             (uint8_t*)ofb->port->fb);
			break;
		}
		case FOURCC_YV12:
			/* YV12 has plane order Y, V, U */
		{
			uint8_t *yb = buf;
			uint8_t *vb = yb + (src_w * src_h);
			uint8_t *ub = vb + ((src_w / 2) * (src_h / 2));
			uv12_to_uyvy(src_w & ~15,
			             src_h & ~15,
			             yb, ub, vb,
			             (uint8_t*)ofb->port->fb);
			break;
		}
		default:
			break;
	}

	if (sync) {
		if (ioctl (ofb->port->fd, OMAPFB_SYNC_GFX))
		{
			xf86Msg(X_ERROR, "%s: Graphics sync failed\n", __FUNCTION__);
			return XvBadAlloc;
		}
	}
	
	return Success;
}


