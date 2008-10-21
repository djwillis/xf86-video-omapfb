/* Texas Instruments OMAP framebuffer driver for X.Org
 * Copyright 2008 Kalle Vahlman, <zuh@iki.fi>
 *                Joni Valtanen, <jvaltane@kapsi.fi> 
 *                Ilpo Ruotsalainen, <lonewolf@iki.fi> 
 *                Tuomas Kulve, <tuomas@kulve.fi> 
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
#include "omapfb-xv-platform.h"
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

/* TODO: */
static XF86AttributeRec xv_attributes[] = {
    { XvSettable | XvGettable, 0, 0xffff, "XV_COLORKEY" },
};

/* Port */

static Bool OMAPFBPortGetRec(ScrnInfoPtr pScrn);
static void OMAPFBPortFreeRec(ScrnInfoPtr pScrn);
static int OMAPFBPortSetup (ScrnInfoPtr pScrn, const char *device);

/* XV interface functions */

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

	w = *width;
	h = *height;

	w = (w + 1) & ~1;
	ofb->port->mem_info.size = w << 1;
	ofb->port->mem_info.size *= h;

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
		           "Failed to open %s: %s\n", OMAP_FBDEV1_NAME, strerror(errno));
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

	adaptor = xf86XVAllocateVideoAdaptorRec(pScrn);
	if (adaptor == NULL)
	{
		OMAPFBPortFreeRec(pScrn);
		return 0;
	}

	xv_encodings[0].width = ofb->state_info.xres;
	xv_encodings[0].height = ofb->state_info.yres;

	adaptor->type = XvInputMask | XvImageMask | XvWindowMask;
	adaptor->flags = (VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT);
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
	adaptor->SetPortAttribute = OMAPFBXVSetPortAttribute;
	adaptor->GetPortAttribute = OMAPFBXVGetPortAttribute;
	adaptor->QueryBestSize = OMAPFBXVQueryBestSize;
	adaptor->QueryImageAttributes = OMAPFBXVQueryImageAttributes;

	/* Generic implementation */
	adaptor->PutImage = OMAPFBXVPutImageGeneric;
	adaptor->StopVideo = OMAPFBXVStopVideoGeneric;

	/* Allow customized functionality for different CPU revisions
	 * and LCD controller chips
	 */
	if (strncmp(ofb->ctrl_name, "blizzard", 8) == 0) {
		/* Blizzard is Epson S1D13745A01, found on eg. Nokia N8x0 */
		adaptor->PutImage = OMAPFBXVPutImageBlizzard;
		adaptor->StopVideo = OMAPFBXVStopVideoBlizzard;
	}
	
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
	memset(&ofb->port->update_window, 0, sizeof(struct omapfb_update_window));
	REGION_EMPTY(pScrn, &ofb->port->current_clip);

	return TRUE;
}

static void OMAPFBPortFreeRec(ScrnInfoPtr pScrn)
{
	OMAPFBPtr ofb = OMAPFB(pScrn);

	if (ofb->port == NULL)
		return;

	close(ofb->port->fd);
	xfree(ofb->port);
	
	ofb->port = NULL;
}


