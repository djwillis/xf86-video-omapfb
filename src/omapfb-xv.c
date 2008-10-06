/* Texas Instruments OMAP framebuffer driver for X.Org
 * Copyright 2008 Kalle Vahlman, <zuh@iki.fi>
 *                Joni Valtanen, <jvaltane@kapsi.fi> 
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <X11/extensions/Xv.h>

#include "omapfb-driver.h"
#include "omapfb.h"

#include "xf86.h"
#include "xf86_OSlib.h"
#include "xf86xv.h"
#include "fourcc.h"

/* FIXME: Not like this, take it from the xorg.conf or autodetect or whatever */
#define OMAP_FBDEV1_NAME "/dev/fb1"

/* Supported formats definitions */
static XF86VideoEncodingRec xv_encodings[] = {
    /* Max width and height are filled in later. */
    { 0, "XV_IMAGE", -1, -1, { 1, 1 } },
};

static XF86VideoFormatRec xv_formats[] = {
    { 16, TrueColor },
};

static XF86ImageRec xv_images[] = {
    XVIMAGE_YUY2, /* OMAPFB_COLOR_YUY422 */
    XVIMAGE_UYVY, /* OMAPFB_COLOR_YUV422 */
    XVIMAGE_I420, /* OMAPFB_COLOR_YUV420 */
    XVIMAGE_YV12, /* OMAPFB_COLOR_YUV420 */
};

static enum omapfb_color_format xv_to_omapfb_format(int format)
{
	switch (format)
	{
		case FOURCC_YUY2:
			return OMAPFB_COLOR_YUY422;
		case FOURCC_UYVY:
			return OMAPFB_COLOR_YUV422;
		case FOURCC_I420:
		case FOURCC_YV12:
			return OMAPFB_COLOR_YUV420;
		default:
			return -1;
	}
	return -1;
}

/* TODO: */
static XF86AttributeRec xv_attributes[] = {
    { XvSettable | XvGettable, 0, 0xffff, "XV_COLORKEY" },
};

/* Port */

static Bool OMAPFBPortGetRec(ScrnInfoPtr pScrn);
static void OMAPFBPortFreeRec(ScrnInfoPtr pScrn);
static int OMAPFBPortSetup (ScrnInfoPtr pScrn, const char *device);

/* XV interface functions */

/* Stop video, only deinit overlay if cleanup is true */
int OMAPFBXVStopVideo (ScrnInfoPtr pScrn, pointer data, Bool cleanup)
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

		/* Enable the regular plane
		 * FIXME: It *should* be ok to have them both enabled right?
		 */
		ofb->plane_info.enabled = 1;
		if(ioctl(ofb->fd, OMAPFB_SETUP_PLANE,
		   &ofb->plane_info) != 0) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			           "Failed to enable video overlay\n");
			return 0;
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

/* Set attributes */
int OMAPFBXVSetPortAttribute (ScrnInfoPtr pScrn,
                              Atom attribute,
                              INT32 value,
                              pointer data)
{
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "XV: %s\n", __FUNCTION__);
	return Success;
}

/* Get attributes */
int OMAPFBXVGetPortAttribute (ScrnInfoPtr pScrn,
                              Atom attribute,
                              INT32 *value,
                              pointer data)
{
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "XV: %s\n", __FUNCTION__);

	if (value != NULL)
		*value = 1;
	return Success;
}

/* Calculate best size for vid_w x vid_h video scaled to drw_w x drw_h on
 * screen. Basically we only need to clip it to screen size.
 */
int OMAPFBXVQueryBestSize (ScrnInfoPtr pScrn,
                           Bool motion, short vid_w, short vid_h,
                           short drw_w, short drw_h,
                           unsigned int *p_w, unsigned int *p_h, pointer data)
{
	OMAPFBPtr ofb = OMAPFB(pScrn);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "XV: %s\n", __FUNCTION__);

	*p_w = drw_w;
	*p_h = drw_h;

	if (drw_w > ofb->state_info.xres)
		*p_w = ofb->state_info.xres;
	if (drw_h > ofb->state_info.yres)
		*p_h = ofb->state_info.yres;

	return Success;
}

/**
 * Convertion from YV12 to YUV420, a reference implementation taken from xomap 
 * xserver sources (omap_video.c by Daniel Stone)
 */
void yv12_to_yuv420_xomap(CARD8 *dst, const CARD8 *y, const CARD8 *chroma, int w)
{
    int j;
    CARD32 *d2;
    CARD32 *sy = (CARD32 *)y;
    CARD16 *sc = (CARD16 *)chroma;
    CARD16 *d1 = (CARD16 *)dst;
    w >>= 2;

    for (j = 0; j < w; j++) {
        if (((unsigned long) d1) & 3) {
            /* Luma 1, chroma 1. */
            *d1++ = (*sy & 0x000000ff) | ((*sc & 0x00ff) << 8);
            /* Chroma 2, luma 2. */
            *d1++ = ((*sc & 0xff00) >> 8) | (*sy & 0x0000ff00);
        }
        else {
            d2 = (CARD32 *) d1;
            /* Luma 1, chroma 1, chroma 2, luma 2. */
            *d2++ = (*sy & 0x000000ff) | (*sc << 8) |
                    ((*sy & 0x0000ff00) << 16);
            d1 = (CARD16 *) d2;
        }
        /* Luma 4, luma 3. */
        *d1++ = ((*sy & 0xff000000) >> 24) | ((*sy & 0x00ff0000) >> 8);
        sy++;
        sc++;
    }
}

/* This is where the magic happens, pushes image data from buf to
 * screen.
 */
int OMAPFBXVPutImage (ScrnInfoPtr pScrn,
                      short src_x, short src_y, short drw_x, short drw_y,
                      short src_w, short src_h, short drw_w, short drw_h,
                      int image, char *buf, short width, short height,
                      Bool sync, RegionPtr clipBoxes, pointer data)
{
	OMAPFBPtr ofb = OMAPFB(pScrn);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "XV: %s\n", __FUNCTION__);

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
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Plane was dirty\n");
		ofb->port->update_window.x = src_x;
		ofb->port->update_window.y = src_y;
	 	ofb->port->update_window.width = src_w;
	 	ofb->port->update_window.height = src_h;
	 	ofb->port->update_window.format = xv_to_omapfb_format(image);
	 	ofb->port->update_window.out_x = drw_x;
	 	ofb->port->update_window.out_y = drw_y;
	 	ofb->port->update_window.out_width = drw_w;
	 	ofb->port->update_window.out_height = drw_h;
		if (!ofb->port->plane_info.enabled) {

			if (ioctl(ofb->port->fd, OMAPFB_SETUP_MEM, &ofb->port->mem_info) != 0) {
				xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				           "Failed to unallocate video plane memory\n");
				return 0;
			}

			if (ioctl (ofb->port->fd, FBIOGET_FSCREENINFO, &ofb->port->fixed_info))
			{
				xf86Msg(X_ERROR, "%s: Reading hardware info failed\n", __FUNCTION__);
				return 0;
			}

			ofb->port->fb = mmap (NULL, ofb->port->fixed_info.smem_len,
			                PROT_READ | PROT_WRITE, MAP_SHARED,
			                ofb->port->fd, 0);
			if (ofb->port->fb == NULL) {
				xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				           "Mapping video memory failed\n");
				return 0;
			}
			/* Disable the regular plane
			 * FIXME: It *should* be ok to have them both enabled right?
			 */
			ofb->plane_info.enabled = 0;
			if(ioctl(ofb->fd, OMAPFB_SETUP_PLANE,
			   &ofb->plane_info) != 0) {
				xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				           "Failed to disable base plane\n");
				return 0;
			}
/*
			if(ioctl(ofb->port->fd, OMAPFB_SET_UPDATE_MODE,
			   OMAPFB_MANUAL_UPDATE) != 0) {
				xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				           "Failed to set update mode\n");
				return 0;
			}
*/
			if (ioctl (ofb->port->fd, FBIOGET_FSCREENINFO, &ofb->port->fixed_info))
			{
				xf86Msg(X_ERROR, "%s: Reading hardware info failed\n", __FUNCTION__);
				return 0;
			}

			if (ioctl (ofb->port->fd, FBIOGET_VSCREENINFO, &ofb->port->state_info))
			{
				xf86Msg(X_ERROR, "%s: Reading state info failed\n", __FUNCTION__);
				return 0;
			}

			ofb->port->state_info.xres = drw_w & ~15;
			ofb->port->state_info.yres = drw_h & ~15;
			ofb->port->state_info.xres_virtual = ofb->state_info.xres;
			ofb->port->state_info.yres_virtual = ofb->state_info.yres;
			ofb->port->state_info.xoffset = 0;
			ofb->port->state_info.yoffset = 0;
			ofb->port->state_info.rotate = 0;
			ofb->port->state_info.grayscale = 0;
			ofb->port->state_info.activate = FB_ACTIVATE_NOW;
		        ofb->port->state_info.bits_per_pixel = 0;
		        ofb->port->state_info.nonstd = xv_to_omapfb_format(image);

			if (ioctl (ofb->port->fd, FBIOPUT_VSCREENINFO, &ofb->port->state_info))
			{
				xf86Msg(X_ERROR, "%s: setting state info failed\n", __FUNCTION__);
				return 0;
			}

			ofb->port->plane_info.enabled = 1;
			
			if(ioctl(ofb->port->fd, OMAPFB_SETUP_PLANE,
			   &ofb->port->plane_info) != 0) {
				xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				           "Failed to enable video overlay\n");
				return 0;
			}

			if (ioctl (ofb->port->fd, OMAPFB_SYNC_GFX))
			{
				xf86Msg(X_ERROR, "%s: Graphics sync failed\n", __FUNCTION__);
				return 0;
			}

		}

/*
		if(ioctl(ofb->port->fd, OMAPFB_UPDATE_WINDOW,
		   &ofb->port->update_window) != 0) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			           "Failed to setup video update window\n");
			return 0;
		}
*/
		
	}

	/* TODO: Actually convert and copy the data from buf to the framebuffer ;) */

	if (ioctl (ofb->port->fd, OMAPFB_SYNC_GFX))
	{
		xf86Msg(X_ERROR, "%s: Graphics sync failed\n", __FUNCTION__);
		return 0;
	}

	return Success;
}

/* Calculates and returns image size for different formats */
int OMAPFBXVQueryImageAttributes (ScrnInfoPtr pScrn,
                                  int id, short *width, short *height,
                                  int *pitches, int *offsets)
{
	int w, h;
	int size = 0;
	int tmp = 0;
	OMAPFBPtr ofb = OMAPFB(pScrn);

	if (*width > ofb->state_info.xres)
		*width = ofb->state_info.xres;
	if (*height > ofb->state_info.xres)
		*height = ofb->state_info.yres;

	w = *width;
	h = *height;

	if (offsets)
		offsets[0] = 0;

	switch (id)
	{
		case FOURCC_I420:
		case FOURCC_YV12:
			w = (w + 3) & ~3;
			h = (h + 1) & ~1;
			size = w;
			if (pitches)
				pitches[0] = size;
			size *= h;
			if (offsets)
				offsets[1] = size;
			tmp = w >> 1;
			tmp = (tmp + 3) & ~3;
			if (pitches)
				pitches[1] = pitches[2] = tmp;
			tmp *= h >> 1;
			size += tmp;
			if (offsets)
				offsets[2] = size;
				size += tmp;
			break;
		case FOURCC_UYVY:
		case FOURCC_YUY2:
		default:
			w = (w + 1) & ~1;
			size = w << 1;
			if (pitches)
				pitches[0] = size;
			size *= h;
			break;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "format is...");
	switch (id)
	{
		case FOURCC_I420:
			xf86DrvMsg(pScrn->scrnIndex, X_INFO, "I420\n");
			break;
		case FOURCC_YV12:
			xf86DrvMsg(pScrn->scrnIndex, X_INFO, "YV12\n");
			break;
		case FOURCC_UYVY:
			xf86DrvMsg(pScrn->scrnIndex, X_INFO, "UYVY\n");
			break;
		case FOURCC_YUY2:
			xf86DrvMsg(pScrn->scrnIndex, X_INFO, "YUY2\n");
			break;
		default:
			xf86DrvMsg(pScrn->scrnIndex, X_INFO, "unrecognized.\n");
			break;
	}
	
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	           "XV: %s returning %i for %ix%i image\n",
	           __FUNCTION__, size, w, h);

	/* This will make us only allocate as much as we need when the frames
	 * start coming in
	 */
	ofb->port->mem_info.size = size;
	return size;
}

/* Initialization */
int OMAPFBXVInit (ScrnInfoPtr pScrn,
                  XF86VideoAdaptorPtr **omap_adaptors)
{
	XF86VideoAdaptorPtr adaptor = NULL;
	int n_adaptors = 0;
	const char *name = "OMAP XV adaptor";
	OMAPFBPtr ofb = OMAPFB(pScrn);
	
	OMAPFBPortGetRec(pScrn);

	/* Do our hardware initialization */
	ofb->port->fd = open(OMAP_FBDEV1_NAME, O_RDWR);
	if(ofb->port->fd < 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		           "Failed to open %s\n", OMAP_FBDEV1_NAME);
		OMAPFBPortFreeRec(pScrn);
		return 0;
	}
	if(ioctl(ofb->port->fd, OMAPFB_QUERY_PLANE, &ofb->port->plane_info) != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		           "Failed to fetch plane info\n");
		OMAPFBPortFreeRec(pScrn);
		return 0;
	}
	ofb->port->plane_info.enabled = 0;
	if(ioctl(ofb->port->fd, OMAPFB_SETUP_PLANE, &ofb->port->plane_info) != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		           "Failed to setup plane\n");
		OMAPFBPortFreeRec(pScrn);
		return 0;
	}
	if(ioctl(ofb->port->fd, OMAPFB_QUERY_MEM, &ofb->port->mem_info) != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		           "Failed to fetch memory info\n");
		OMAPFBPortFreeRec(pScrn);
		return 0;
	}

	/* Deallocate existing memory */
	if(ofb->port->mem_info.size) {
		ofb->port->mem_info.size = 0;
		/* We probably would want to use SRAM here, but allocating it
		 * doesn't seem possible...
		 */
//		ofb->port->mem_info.type = OMAPFB_MEMTYPE_SRAM;
		if (ioctl(ofb->port->fd, OMAPFB_SETUP_MEM, &ofb->port->mem_info) != 0) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			           "Failed to unallocate video plane memory: %s\n",
			           strerror(errno));
			OMAPFBPortFreeRec(pScrn);
			return 0;
		}
	}
	if(ioctl(ofb->port->fd, OMAPFB_QUERY_PLANE, &ofb->port->plane_info) != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		           "Failed to fetch plane info\n");
		OMAPFBPortFreeRec(pScrn);
		return 0;
	}

	if (ioctl (ofb->port->fd, FBIOGET_FSCREENINFO, &ofb->port->fixed_info))
	{
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "%s: Reading hardware info failed\n", __FUNCTION__);
		OMAPFBPortFreeRec(pScrn);
		return 0;
	}
/* FIXME: this borks?
	if (ioctl (ofb->port->fd, FBIOGET_VSCREENINFO, &ofb->port->state_info))
	{
		xf86Msg(X_ERROR, "%s: Reading state info failed\n", __FUNCTION__);
		OMAPFBPortFreeRec(pScrn);
		return 0;
	}
*/
	adaptor = xf86XVAllocateVideoAdaptorRec(pScrn);
	if (adaptor == NULL)
	{
		OMAPFBPortFreeRec(pScrn);
		return 0;
	}

	xv_encodings[0].width = ofb->state_info.xres;
	xv_encodings[0].height = ofb->state_info.yres;

	adaptor->type = XvInputMask | XvImageMask | XvWindowMask;
	adaptor->flags = (VIDEO_OVERLAID_IMAGES);
	/* FIXME: why does this clip _everything_ away? | VIDEO_CLIP_TO_VIEWPORT); */
	adaptor->name = xstrdup(name);
	adaptor->nEncodings = 1;
	adaptor->pEncodings = xv_encodings;
	adaptor->nFormats = 1;
	adaptor->pFormats = xv_formats;
	adaptor->nPorts = 1;
	/* Place per-port data here */
	adaptor->pPortPrivates = (DevUnion *)(&adaptor[1]);
	adaptor->nAttributes = 1;
	adaptor->pAttributes = xv_attributes;
	adaptor->nImages = 4;
	adaptor->pImages = xv_images;
	adaptor->StopVideo = OMAPFBXVStopVideo;
	adaptor->SetPortAttribute = OMAPFBXVSetPortAttribute;
	adaptor->GetPortAttribute = OMAPFBXVGetPortAttribute;
	adaptor->QueryBestSize = OMAPFBXVQueryBestSize;
	adaptor->PutImage = OMAPFBXVPutImage;
	adaptor->QueryImageAttributes = OMAPFBXVQueryImageAttributes;
	
	n_adaptors++;
	
	*omap_adaptors = xnfcalloc(sizeof(XF86VideoAdaptorPtr*), n_adaptors);
	*omap_adaptors[0] = adaptor;
	
	return n_adaptors;
}

static Bool
OMAPFBPortGetRec(ScrnInfoPtr pScrn)
{
	OMAPFBPtr ofb = OMAPFB(pScrn);

	if (ofb->port != NULL)
		return TRUE;
	
	ofb->port = xnfcalloc(sizeof(OMAPFBPortRec), 1);

	return TRUE;
}

static void OMAPFBPortFreeRec(ScrnInfoPtr pScrn)
{
	OMAPFBPtr ofb = OMAPFB(pScrn);

	if (ofb->port == NULL)
		return;

	close(ofb->port->fd);
	/* FIXME: this apparently is not the right way to free the portrec
	xfree(ofb->port);
	*/
	
	ofb->port = NULL;
}


