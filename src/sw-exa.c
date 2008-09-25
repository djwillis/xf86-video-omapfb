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


#include <X11/X.h>
#include "omapfb-driver.h"

#include "exa.h"

#ifdef LOG_CALLS
# define FALLBACK do { ErrorF("Fallback from %s\n", __FUNCTION__); return FALSE; } while (0)
#else
# define FALLBACK return FALSE
#endif

/*** Solid fill */

static Bool
SWPrepareSolid(PixmapPtr pPixmap, int alu, Pixel planemask, Pixel fg)
{
	FALLBACK;
}

static Bool
SWSolid(PixmapPtr pPixmap, int x1, int y1, int x2, int y2)
{
	FALLBACK;
}

static Bool
SWDoneSolid(PixmapPtr pPixmap)
{
	FALLBACK;
}

/*** Copy */

static Bool
SWPrepareCopy(PixmapPtr pSrcPixmap, PixmapPtr pDstPixmap, int dx, int dy, int alu, Pixel planemask) 
{
	FALLBACK;
}

static Bool
SWCopy(PixmapPtr pDstPixmap, int srcX, int srcY, int dstX, int dstY, int width, int height) 
{
	FALLBACK;
}

static Bool
SWDoneCopy(PixmapPtr pDstPixmap) 
{
	FALLBACK;
}

/*** Composite */

static Bool
SWCheckComposite(int op, PicturePtr pSrcPicture, PicturePtr pMaskPicture, PicturePtr pDstPicture) 
{
	FALLBACK;
}

static Bool
SWPrepareComposite(int op, PicturePtr pSrcPicture, PicturePtr pMaskPicture, PicturePtr pDstPicture, PixmapPtr pSrc, PixmapPtr pMask, PixmapPtr pDst)
{
	FALLBACK;
}

static void
SWComposite(PixmapPtr pDst, int srcX, int srcY, int maskX, int maskY, int dstX, int dstY, int width, int height)
{
}

static void
SWDoneComposite(PixmapPtr pDst)
{
}

/*** General */

static void
SWWaitMarker(ScreenPtr pScreen, int marker)
{
}

static Bool
SWPrepareAccess(PixmapPtr pPix, int index)
{
	return TRUE;
}
static void
SWFinishAccess(PixmapPtr pPix, int index)
{
}

/*** Setup */

Bool OMAPFBSetupExa(OMAPFBPtr ofb)
{
	ofb->exa->exa_major = 2;
	ofb->exa->exa_minor = 2;
	
	ofb->exa->memoryBase = ofb->fb;
	ofb->exa->memorySize = ofb->mem_info.size;

	/* TODO: maybe we could over-allocate the memory for the fb
	 * to enable offscreen pixmaps? Dunno if it matters...
	 */	
	ofb->exa->offScreenBase = 0;

	ofb->exa->maxX = ofb->state_info.xres;
	ofb->exa->maxY = ofb->state_info.yres;

#define EXA_FUNC(s) ofb->exa->s = SW ## s
	
	EXA_FUNC(PrepareSolid);
	EXA_FUNC(Solid);
	EXA_FUNC(DoneSolid);

	EXA_FUNC(PrepareCopy);
	EXA_FUNC(Copy);
	EXA_FUNC(DoneCopy);

	EXA_FUNC(CheckComposite);
	EXA_FUNC(PrepareComposite);
	EXA_FUNC(Composite);
	EXA_FUNC(DoneComposite);

	EXA_FUNC(WaitMarker);
	EXA_FUNC(PrepareAccess);
	EXA_FUNC(FinishAccess);
	
	/* TODO: we'd need a place to store EXA-specific data */
	
	return TRUE;
}

