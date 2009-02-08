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
 * Generic functions for the XV driver
 */

#include "xf86.h"
#include "xf86_OSlib.h"
#include "xf86xv.h"
#include "fourcc.h"

#include <X11/extensions/Xv.h>

#include "omapfb-driver.h"
#include "omapfb-xv-platform.h"
#include "image-format-conversions.h"

enum omapfb_color_format xv_to_omapfb_format(int format)
{
	switch (format)
	{
		case FOURCC_YUY2:
			return OMAPFB_COLOR_YUY422;
		case FOURCC_UYVY:
			return OMAPFB_COLOR_YUV422;
		case FOURCC_I420:
		case FOURCC_YV12:
			/* Unfortunately dispc doesn't support planar formats
			 * (at least currently) so we'll need to convert
			 * to packed (UYVY)
			 */
			return OMAPFB_COLOR_YUV422;
		default:
			return -1;
	}
	return -1;
}

int OMAPXVAllocPlane(ScrnInfoPtr pScrn)
{
	OMAPFBPtr ofb = OMAPFB(pScrn);

	/* The memory size is already set in OMAPFBXVQueryImageAttributes */
	if (ioctl(ofb->port->fd, OMAPFB_SETUP_MEM, &ofb->port->mem_info) != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		           "Failed to allocate video plane memory\n");
		return XvBadAlloc;
	}

	/* Map the framebuffer memory */
	ofb->port->fb = mmap (NULL, ofb->port->mem_info.size,
	                PROT_READ | PROT_WRITE, MAP_SHARED,
	                ofb->port->fd, 0);
	if (ofb->port->fb == NULL) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		           "Mapping video memory failed\n");
		return XvBadAlloc;
	}

	/* Update the state info */
	if (ioctl (ofb->port->fd, FBIOGET_VSCREENINFO, &ofb->port->state_info))
	{
		xf86Msg(X_ERROR, "%s: Reading state info failed\n", __FUNCTION__);
		return XvBadAlloc;
	}

	return Success;
}


int OMAPXVSetupVideoPlane(ScrnInfoPtr pScrn)
{
	OMAPFBPtr ofb = OMAPFB(pScrn);

	if (ioctl (ofb->port->fd, FBIOPUT_VSCREENINFO, &ofb->port->state_info))
	{
	        xf86Msg(X_ERROR, "%s: setting state info failed\n", __FUNCTION__);
	        return XvBadAlloc;
	}
	if (ioctl (ofb->port->fd, FBIOGET_VSCREENINFO, &ofb->port->state_info))
	{
		xf86Msg(X_ERROR, "%s: Reading state info failed\n", __FUNCTION__);
		return XvBadAlloc;
	}

	if(ioctl(ofb->port->fd, OMAPFB_SETUP_PLANE,
	   &ofb->port->plane_info) != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		           "Failed to enable video overlay: %s\n", strerror(errno));
		ofb->port->plane_info.enabled = 0;
		return XvBadAlloc;
	}

	return Success;
}

int OMAPFBXVPutImageGeneric (ScrnInfoPtr pScrn,
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

		if (OUTPUT_IS_OFFSCREEN)
		{
			xf86Msg(X_NOT_IMPLEMENTED,
			        "Partially offscreen video not supported yet\n");
			/* Stop video... */
			if (ofb->port->plane_info.enabled) {
				OMAPFBXVStopVideoBlizzard(pScrn, NULL, FALSE);
			}
			/* ..but return Success so that clients don't die
			 * in case this was just a temprorary thing.
			 */
			return Success;
		}

		/* If we don't have the plane running, enable it */
		if (!ofb->port->plane_info.enabled) {
			ret = OMAPXVAllocPlane(pScrn);
			if (ret != Success)
				return ret;
		}

		/* Set up the state info, xres and yres will be used for
		 * scaling to the values in the plane info struct
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

		/* Cap output to screen size */
		if (ofb->port->plane_info.out_width > ofb->state_info.xres) {
			ofb->port->plane_info.pos_x = 0;
			ofb->port->plane_info.out_width = ofb->state_info.xres;
		}
		if (ofb->port->plane_info.out_height > ofb->state_info.yres) {
			ofb->port->plane_info.pos_y = 0;
			ofb->port->plane_info.out_height = ofb->state_info.yres;
		}

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
		case FOURCC_I420:
			/* I420 has plane order Y, U, V */
		{
			int src_y_pitch = (src_w + 3) & ~3;
			int src_uv_pitch = (((src_y_pitch >> 1) + 3) & ~3);
			uint8_t *yb = buf;
			uint8_t *ub = yb + (src_y_pitch * src_h);
			uint8_t *vb = ub + (src_uv_pitch * (src_h / 2));
			uv12_to_uyvy(src_w & ~15,
			             src_h & ~15,
			             src_y_pitch,
			             src_uv_pitch,
			             yb, ub, vb,
			             (uint8_t*)ofb->port->fb);
			break;
		}
		case FOURCC_YV12:
			/* YV12 has plane order Y, V, U */
		{
			int src_y_pitch = (src_w + 3) & ~3;
			int src_uv_pitch = (((src_y_pitch >> 1) + 3) & ~3);
			uint8_t *yb = buf;
			uint8_t *vb = yb + (src_y_pitch * src_h);
			uint8_t *ub = vb + (src_uv_pitch * (src_h / 2));
			uv12_to_uyvy(src_w & ~15,
			             src_h & ~15,
			             src_y_pitch,
			             src_uv_pitch,
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

/* Stop video, only deinit overlay if cleanup is true */
int OMAPFBXVStopVideoGeneric (ScrnInfoPtr pScrn, pointer data, Bool cleanup)
{
	OMAPFBPtr ofb = OMAPFB(pScrn);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "XV: %s (%i)\n", __FUNCTION__, cleanup);

	if (ofb->port == NULL)
		return Success;

	if(ofb->port->plane_info.enabled) {
		if (ioctl (ofb->port->fd, OMAPFB_SYNC_GFX))
		{
			xf86Msg(X_ERROR, "%s: Graphics sync failed\n", __FUNCTION__);
			return 0;
		}

		if (ioctl (ofb->port->fd, OMAPFB_QUERY_PLANE, &ofb->port->plane_info)) {
	    		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    		           "Failed to query video plane info\n");
		}

		/* Disable the video plane */
		munmap(ofb->port->fb, ofb->port->mem_info.size);
		ofb->port->plane_info.enabled = 0;
		if (ioctl (ofb->port->fd, OMAPFB_SETUP_PLANE, &ofb->port->plane_info)) {
	    		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    		           "Failed to disable video plane\n");
		}
		if (ioctl (ofb->port->fd, OMAPFB_QUERY_PLANE, &ofb->port->plane_info)) {
    			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
    			           "Failed to query video plane info\n");
		}

		if (ioctl (ofb->port->fd, OMAPFB_SYNC_GFX))
		{
			xf86Msg(X_ERROR, "%s: Graphics sync failed\n", __FUNCTION__);
			return 0;
		}
	}

	if (cleanup == TRUE) {
		if(ioctl(ofb->port->fd, OMAPFB_QUERY_MEM, &ofb->port->mem_info) != 0) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			           "Failed to fetch memory info\n");
			return;
		}
		ofb->port->mem_info.size = 0;
		if(ioctl(ofb->port->fd, OMAPFB_SETUP_MEM, &ofb->port->mem_info) != 0) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			           "Failed to set memory info\n");
			return;
		}
	}

	return Success;
}

