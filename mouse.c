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
 */

#include "config.h"

#include <machine/adbsys.h>
#include "mouse.h"
#include "mux.h"

static char rcsid[] = "$Id: mouse.c,v 1.4 1996/05/24 15:13:37 vuori Exp $";

 /* default to off for speed. */
int     mouse_on = 0;		/* is mouse on? */
int     mouse_vis = 1;		/* is mouse visible ? */

/*
	Simple routines for mouse acceleration based upon a lookup table
	of values.
*/
int 
mouse_accel(int d)
{
	int     acceltable[] = {0, 1, 3, 6, 10, 17, 25, 35, 47, 55, 60};
	int     mag, positive = 1;

	if (d < 0) {
		positive = -1;
		mag = d * -1;
	} else
		mag = d;

	if (mag < 9)
		return (positive * acceltable[mag]);
	else
		return (positive * 60);
}

void 
mouse_doevent(
    int mousex,
    int mousey,
    unsigned int buttons,
    int dpi)
{
	int     dx, dy;
	static int old_buttons = 0;
	int     mult;

	if (mouse_on) {
#ifdef HIDE_MOUSE
		if (!mouse_vis) {
			mux_mouseon(mouse_on);
			mouse_vis = 1;
		}
#endif				/* HIDE_MOUSE */

#ifdef MOUSE_ACCEL
		dx = mouse_accel(mousex);
		dy = mouse_accel(mousey);
#else
		mult = 22500 / dpi;	/* increased the speed somewhat */

		dx = mult * mousex / 100;
		dy = mult * mousey / 100;
#endif				/* MOUSE_ACCEL */

		mux_mousedelta(dx, dy);

		if (buttons != old_buttons) {
			if (buttons & 1)
				mux_startcopy();	/* buttondown */
			else
				mux_endcopy();	/* buttonup */
		} else
			if (buttons & 1)
				mux_contcopy();	/* mousemove button is down */
		old_buttons = buttons;
	}
}
