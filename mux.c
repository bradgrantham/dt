/*-
 * Copyright (C) 1994 Bradley A. Grantham and Lawrence A. Kesteloot
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: mux.c,v 1.6 1996/06/09 12:32:29 vuori Exp $
 *
 */


#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#include "vt.h"
#include "grf.h"
#include "main.h"
#include "mux.h"

/* if you experience crashes when moving the mouse beyond the lower-right
 * corner, change the following 0 to 1 */
#if 0
#define quickfix
#endif

static char rcsid[] = "$Id: mux.c,v 1.6 1996/06/09 12:32:29 vuori Exp $";

int     mux_curvt = -1;		/* VT with focus		 */
int     mux_curgrf = 0;		/* GRF with focus		 */
int     mux_vt2grf[VT_MAXVT];	/* GRF for each VT		 */
int     mux_grf2vt[MAXGRF];	/* VT for each GRF		 */
int     mux_grfmx[MAXGRF];
int     mux_grfmy[MAXGRF];
int     mux_cutpasting = 0;	/* Whether cutting and pasting	 */
unsigned char *mux_pastebuffer = NULL;
unsigned char *mux_copybuf = NULL;
int     areasiz;		/* size of selected area */


void 
mux_mouseon(
    int turnon)
{
	grf_movemouse(mux_curgrf, turnon, mux_grfmx[mux_curgrf],
	    mux_grfmy[mux_curgrf]);
}


void 
mux_mousedelta(
    int x,
    int y)
{
	int     newx, newy;
	int     grf;
	int     lines;
	int     charwidth, charheight;
	int     gwidth, gheight;

	/* ignore buttons for now */

	grf = mux_curgrf;

	grf_getsize(grf, &gwidth, &gheight, NULL, NULL, &charwidth,
	    &charheight);

	newx = mux_grfmx[grf] + x;
	newy = mux_grfmy[grf] + y;

#ifdef quickfix
	if ((newx > gwidth - 16) && (newy > gheight - 16))
		return;
#endif

	if (newx < 0)
		newx = 0;
	if (newx > gwidth - 1)
		newx = gwidth - 1;

	if (newy < 0) {
		/* scroll up */
		lines = -y / charheight + 1;
		vt_scrollback(mux_curvt, lines);
		newy = 0;
	}
	if (newy > gheight - 1) {
		/* scroll down */
		lines = y / charheight + 1;
		vt_scrollback(mux_curvt, -lines);
		newy = gheight - 1;
	}
	mux_grfmx[grf] = newx;
	mux_grfmy[grf] = newy;
	grf_movemouse(grf, 1, newx, newy);
}


void 
mux_startcopy(
    void)
{
	int     cx, cy;

	grf_getsize(mux_curgrf, NULL, NULL, NULL, NULL, &cx, &cy);
	vt_startcopy(mux_curvt, (mux_grfmx[mux_curgrf] + cx / 2) / cx,
	    mux_grfmy[mux_curgrf] / cy);
	mux_cutpasting = 1;
}


void 
mux_contcopy(
    void)
{
	int     cx, cy;

	grf_getsize(mux_curgrf, NULL, NULL, NULL, NULL, &cx, &cy);
	vt_movecopy(mux_curvt, (mux_grfmx[mux_curgrf] + cx / 2) / cx,
	    mux_grfmy[mux_curgrf] / cy);
}


void 
mux_endcopy(
    void)
{
	int     cx, cy;

	mux_cutpasting = 0;

	grf_getsize(mux_curgrf, NULL, NULL, NULL, NULL, &cx, &cy);
	if (mux_pastebuffer != NULL) {
		free(mux_pastebuffer);
		mux_pastebuffer = NULL;
	}
	mux_pastebuffer = vt_endcopy(mux_curvt,
	    (mux_grfmx[mux_curgrf] + cx / 2) / cx,
	    mux_grfmy[mux_curgrf] / cy, &areasiz);
}


void 
mux_paste(
    void)
{
	unsigned char *cp;

	if (mux_pastebuffer) {
		for (cp = mux_pastebuffer; *cp; cp++) {
			main_keyhit(mux_curvt, *cp);
		}
	}
}

void 
mux_copy(void)
{				/* down with xterminism, up with mac */
	if (mux_copybuf != NULL) {
		free(mux_copybuf);
		mux_copybuf = NULL;
	}
	if (areasiz < 1)
		return;
	if (!mux_pastebuffer)
		return;

	mux_copybuf = malloc(areasiz);
	if (mux_copybuf == NULL) {
		printf("couldn't allocate space for mux_copybuf\n");
		return;
	}
	memcpy(mux_copybuf, mux_pastebuffer, areasiz);
}

void 
mux_realpaste(void)
{				/* see the comment above */
	unsigned char *realbuf = mux_pastebuffer;

	if (!mux_copybuf)
		return;
	mux_pastebuffer = mux_copybuf;
	mux_paste();

	mux_pastebuffer = realbuf;
}

void 
mux_vtscroll(
    int lines)
{
	vt_scrollback(mux_curvt, lines);
}


void 
mux_vtpage(
    int pages)
{
	int     grows;

	grf_getsize(mux_curgrf, NULL, NULL, NULL, &grows, NULL, NULL);
	vt_scrollback(mux_curvt, pages * grows / 2);
}


void 
mux_vtbottom(
    void)
{
	vt_scrollback(mux_curvt, VT_SBBOTTOM);
}


void 
mux_vttop(
    void)
{
	vt_scrollback(mux_curvt, -VT_SBBOTTOM);
}


void 
mux_switchtovt(
    int vtnum)
{
	int     thegrf;

	if (mux_cutpasting || vtnum == mux_curvt || vtnum >= VT_MAXVT) {
		return;
	}
	thegrf = mux_vt2grf[vtnum];

	if (thegrf != mux_curgrf) {
		vt_newgrf(mux_curvt, mux_curgrf, 0);
		vt_newgrf(mux_grf2vt[thegrf], -1, 0);
		mux_curvt = vtnum;
		mux_curgrf = thegrf;
		vt_newgrf(mux_curvt, mux_curgrf, 1);
	} else {
		vt_newgrf(mux_curvt, -1, 0);
		mux_curvt = vtnum;
		vt_newgrf(mux_curvt, mux_curgrf, 1);
	}

	mux_grf2vt[mux_curgrf] = mux_curvt;
}


void 
mux_vttogrf(
    int grfnum)
{
	int     i;

	if (mux_cutpasting || grfnum == mux_curgrf || grfnum >= grf_numgrfs()) {
		return;
	}
	if (mux_grf2vt[grfnum] != -1) {
		vt_newgrf(mux_grf2vt[grfnum], -1, 0);
	}
	mux_grf2vt[grfnum] = mux_curvt;
	mux_vt2grf[mux_curvt] = grfnum;
	vt_newgrf(mux_curvt, grfnum, 1);

	for (i = 0; i < VT_MAXVT; i++) {
		if (mux_vt2grf[i] == mux_curgrf) {
			break;
		}
	}

	if (i < VT_MAXVT) {
		mux_grf2vt[mux_curgrf] = i;
		vt_newgrf(i, mux_curgrf, 0);
	} else {
		mux_grf2vt[mux_curgrf] = -1;
		grf_blankscreen(mux_curgrf);
	}

	mux_curgrf = grfnum;
}

void 
mux_changefont(
    void)
{
	grf_changefont(mux_curgrf);
	grf_blankscreen(mux_curgrf);
	vt_newgrf(mux_curvt, mux_curgrf, 1);
}

void 
mux_init(
    void)
{
	int     i, gwidth, gheight;

	for (i = 0; i < MAXGRF; i++) {
		grf_getsize(i, &gwidth, &gheight, NULL, NULL, NULL, NULL);
		mux_grfmx[i] = gwidth / 2;
		mux_grfmy[i] = gheight / 2;
		mux_grf2vt[i] = -1;
	}

	for (i = 0; i < VT_MAXVT; i++) {
		mux_vt2grf[i] = 0;
	}

	mux_curvt = 0;
	mux_curgrf = 0;

	mux_grf2vt[mux_curgrf] = mux_curvt;
	mux_vt2grf[mux_curvt] = mux_curgrf;
	vt_newgrf(mux_curvt, mux_curgrf, 1);
}
