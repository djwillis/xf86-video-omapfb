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
 *
 * Known features/limitations:
 *  - update window size and position must be divisible by 2 in both directions
 *  - 
 */

#include "xf86.h"
#include "xf86_OSlib.h"
#include "xf86xv.h"
#include "fourcc.h"

#include <X11/extensions/Xv.h>

#include "omapfb-driver.h"
#include "omapfb-xv-platform.h"

int OMAPFBXVApplyClip(ScrnInfoPtr pScrn, RegionPtr clipBoxes)
{
	double xscale, yscale;
	int xoffset, yoffset;
	BoxPtr clip;
	OMAPFBPtr ofb = OMAPFB(pScrn);

	/* We can do rectangular clipping directly on the plane, but complex
	 * clipping would need colorkeying & overlay windows.
	 */
	if (REGION_NUM_RECTS(clipBoxes) > 1)
		return XvBadAlloc;

	clip = REGION_RECTS(clipBoxes);

	/* Calculate scaling factors for source data */	
	xscale = (double)ofb->port->state_info.xres / (double)ofb->port->plane_info.out_width;
	yscale = (double)ofb->port->state_info.yres / (double)ofb->port->plane_info.out_height;
	if (xscale > 1.0)
		xscale = 1.0;
	if (yscale > 1.0)
		yscale = 1.0;
	
	/* First calculate the output values, clipping is expressed in
	 * destination pixels.
	 */
	xoffset = clip->x1 - ofb->port->plane_info.pos_x;
	yoffset = clip->y1 - ofb->port->plane_info.pos_y;
	ofb->port->plane_info.pos_x = clip->x1 & ~1;
	ofb->port->plane_info.pos_y = clip->y1 & ~1;
	ofb->port->plane_info.out_width = (clip->x2 - clip->x1) & ~1;
	ofb->port->plane_info.out_height = (clip->y2 - clip->y1) & ~1;

	/* Calculate visible plane size and offset (the original source size 
	 * is used as the virtual size
	 */
	ofb->port->state_info.xoffset = (int)(xoffset * xscale) & ~1;
	ofb->port->state_info.yoffset = (int)(yoffset * yscale) & ~1;
	ofb->port->state_info.xres = (int)(ofb->port->plane_info.out_width * xscale) & ~3;
	ofb->port->state_info.yres = (int)(ofb->port->plane_info.out_height * yscale) & ~3;

	return Success;
}

/* Blizzard is Epson S1D13745A01, found on eg. Nokia N8x0 */
int OMAPFBXVPutImageBlizzard (ScrnInfoPtr pScrn,
                              short src_x, short src_y, short drw_x, short drw_y,
                              short src_w, short src_h, short drw_w, short drw_h,
                              int image, char *buf, short width, short height,
                              Bool sync, RegionPtr clipBoxes, pointer data)
{
	struct omapfb_update_window w;
	OMAPFBPtr ofb = OMAPFB(pScrn);
	int do_clip = !REGION_EQUAL(pScrn, &ofb->port->current_clip, clipBoxes);

	if (!ofb->port->plane_info.enabled
	 || ofb->port->update_window.x != src_x
	 || ofb->port->update_window.y != src_y
	 || ofb->port->update_window.width != src_w
	 || ofb->port->update_window.height != src_h
	 || ofb->port->update_window.format != xv_to_omapfb_format(image)
	 || ofb->port->update_window.out_x != drw_x
	 || ofb->port->update_window.out_y != drw_y
	 || ofb->port->update_window.out_width != drw_w
	 || ofb->port->update_window.out_height != drw_h
	 || do_clip)
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
				ofb->port->plane_info.enabled = 0;
				if (ioctl (ofb->port->fd, OMAPFB_SETUP_PLANE, &ofb->port->plane_info)) {
			    		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			    		           "Failed to disable video plane\n");
				}
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
		 * scaling to the values in the plane info strurct
		 */
		ofb->port->state_info.xres = src_w & ~3;
		ofb->port->state_info.yres = src_h & ~3;
		ofb->port->state_info.xres_virtual = src_w & ~3;
		ofb->port->state_info.yres_virtual = src_h & ~3;
		ofb->port->state_info.xoffset = 0;
		ofb->port->state_info.yoffset = 0;
		ofb->port->state_info.rotate = 0;
		ofb->port->state_info.grayscale = 0;
		ofb->port->state_info.activate = FB_ACTIVATE_NOW;
		ofb->port->state_info.bits_per_pixel = 0;
		ofb->port->state_info.nonstd = xv_to_omapfb_format(image);

		/* Set up the video plane info */
		ofb->port->plane_info.enabled = 1;
		ofb->port->plane_info.pos_x = drw_x & ~1;
		ofb->port->plane_info.pos_y = drw_y & ~1;
		ofb->port->plane_info.out_width = drw_w & ~1;
		ofb->port->plane_info.out_height = drw_h & ~1;

		if (do_clip) {
	 		REGION_COPY(pScrn, &ofb->port->current_clip, clipBoxes);
			ret = OMAPFBXVApplyClip(pScrn, clipBoxes);
			if (ret != Success) {
				xf86Msg(X_NOT_IMPLEMENTED,
				        "Complex clipping of video not supported yet\n");
				/* Stop video... */
				if (ofb->port->plane_info.enabled) {
					ofb->port->plane_info.enabled = 0;
					if (ioctl (ofb->port->fd, OMAPFB_SETUP_PLANE, &ofb->port->plane_info)) {
				    		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				    		           "Failed to disable video plane\n");
					}
				}
				/* ..but return Success so that clients don't die
				 * in case this was just a temprorary thing.
				 */
				return Success;
			}
		}
		
		ret = OMAPXVSetupVideoPlane(pScrn);
		if (ret != Success)
			return ret;
		
		ret = OMAPFB_MANUAL_UPDATE;
		if (ioctl (ofb->port->fd, OMAPFB_SET_UPDATE_MODE, &ret))
		{
			xf86Msg(X_ERROR, "%s: Failed to set manual update mode:"
			                 " %s\n", __FUNCTION__, strerror(errno));
			return XvBadAlloc;
		}

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
			packed_line_copy(src_w & ~3,
			                 src_h & ~3,
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

		/* We don't actually support planar formats, as the blizzard
		 * has (apparently) due to endianness incompatibilities a
		 * quirky YUV420 format. Fortunately the conversion to packed
		 * formats is cheap enough to do smooth 512x288@24fps on N800,
		 * making support for the "custom" format unattractive. That,
		 * and the fact that I've tried to use it (there's code around
		 * to do that conversion) and failed :)
		 */

		case FOURCC_I420:
			/* I420 has plane order Y, U, V */
		{
			uint8_t *yb = buf;
			uint8_t *ub = yb + (src_w * src_h);
			uint8_t *vb = ub + ((src_w / 2) * (src_h / 2));
			uv12_to_uyvy(src_w & ~3,
			             src_h & ~3,
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
			uv12_to_uyvy(src_w & ~3,
			             src_h & ~3,
			             yb, ub, vb,
			             (uint8_t*)ofb->port->fb);
			break;
		}
		default:
			break;
	}

	w.x = 0;
	w.y = 0;
	w.width = ofb->state_info.xres;
	w.height = ofb->state_info.yres;
	w.format = 0;
	w.out_x = 0;
	w.out_y = 0;
	w.out_width = ofb->state_info.xres;
	w.out_height = ofb->state_info.yres;

	if (ioctl (ofb->fd, OMAPFB_UPDATE_WINDOW, &w))
	{
		xf86Msg(X_ERROR, "%s: Failed to update screen:"
		                 " %s\n", __FUNCTION__, strerror(errno));
		return XvBadAlloc;
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
int OMAPFBXVStopVideoBlizzard (ScrnInfoPtr pScrn, pointer data, Bool cleanup)
{
	OMAPFBPtr ofb = OMAPFB(pScrn);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "XV: %s (%i)\n", __FUNCTION__, cleanup);

	if (ofb->port == NULL)
		return Success;

	if(ofb->port->plane_info.enabled) {
		int mode;
		struct omapfb_update_window w;
		w.x = 0;
		w.y = 0;
		w.width = ofb->state_info.xres;
		w.height = ofb->state_info.yres;
		w.format = 0;
		w.out_x = 0;
		w.out_y = 0;
		w.out_width = ofb->state_info.xres;
		w.out_height = ofb->state_info.yres;
		
		if (ioctl (ofb->port->fd, OMAPFB_SYNC_GFX))
		{
			xf86Msg(X_ERROR, "%s: Graphics sync failed\n", __FUNCTION__);
			return 0;
		}

		if (ioctl (ofb->fd, OMAPFB_UPDATE_WINDOW, &w))
		{
			xf86Msg(X_ERROR, "%s: Failed to update screen:"
			                 " %s\n", __FUNCTION__, strerror(errno));
			return XvBadAlloc;
		}

		mode = OMAPFB_AUTO_UPDATE;
		if (ioctl (ofb->port->fd, OMAPFB_SET_UPDATE_MODE, &mode))
		{
			xf86Msg(X_ERROR, "%s: Failed to set auto update mode:"
			                 " %s\n", __FUNCTION__, strerror(errno));
			return XvBadAlloc;
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

