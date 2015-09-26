/*-
 * Copyright (C) 1993, 1994	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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

/*
 * June 19th, 1994 -- Lawrence Kesteloot
 *   Original
 */


#include <sys/types.h>

#include "config.h"

#include "grf.h"
#include "blit.h"

static char rcsid[] = "$Id: blit.c,v 1.10 1996/06/09 12:32:12 vuori Exp $";

/*
 * blitRead()
 *
 * Reads a bitmap from the screen.
 */

void 
blitRead(struct grf_softc * gp, int x, int y, int sx, int sy,
    void *vbuf)
{
	int     sh, longsperline;
	u_long *sc, *lbuf = vbuf;
	u_short *sbuf = vbuf;
	u_char *cbuf = vbuf;

	if (y + sy > gp->g_display.height) {
		sy = gp->g_display.height - y;
	}
	if (sy < 0) {
		sy = 0;
	}
	if (gp->g_display.psize == 1) {
		longsperline = gp->g_display.rowbytes / sizeof(long);
		sc = (u_long *) (gp->g_display.fbbase) +
		    y * longsperline + x / 32;
		sh = 32 - sx - x % 32;
		if (sh < 0) {	/* Split across long-word boundry */
			if (sx <= 8) {
				while (sy--) {
					*cbuf++ = (*sc << -sh) |
					    (*(sc + 1) >> (32 + sh));
					sc += longsperline;
				}
			} else
				if (sx <= 16) {
					while (sy--) {
						*sbuf++ = (*sc << -sh) |
						    (*(sc + 1) >> (32 + sh));
						sc += longsperline;
					}
				} else {
					while (sy--) {
						*lbuf++ = (*sc << -sh) |
						    (*(sc + 1) >> (32 + sh));
						sc += longsperline;
					}
				}
		} else {
			if (sx <= 8) {
				while (sy--) {
					*cbuf++ = *sc >> sh;
					sc += longsperline;
				}
			} else
				if (sx <= 16) {
					while (sy--) {
						*sbuf++ = *sc >> sh;
						sc += longsperline;
					}
				} else {
					while (sy--) {
						*lbuf++ = *sc >> sh;
						sc += longsperline;
					}
				}
		}
	}
}
/*
 * blitWrite()
 *
 * Writes a bitmap to the screen.
 */

inline void 
blitWrite(struct grf_softc * gp, int x, int y, int sx, int sy,
    void *vbuf, void *vmask, register int reverse)
{
	int     sh, longsperline;
	u_long *sc, *lbuf = vbuf, *lmask = vmask, ubuf, umask;
	u_short *sbuf = vbuf, *smask = vmask;
	u_char *cbuf = vbuf, *cmask = vmask;

	if (reverse) {
		if (sx == 32) {
			reverse = 0xffffffff;
		} else {
			reverse = (1 << sx) - 1;
		}
	}
	if (y + sy > gp->g_display.height) {
		sy = gp->g_display.height - y;
	}
	if (y < 0) {
		y = 0;
	}

	longsperline = gp->g_display.rowbytes / sizeof(long);
	sc = (u_long *) (gp->g_display.fbbase) +
	    y * longsperline + x / 32;
	sh = 32 - sx - x % 32;
	if (sh < 0) {		/* Split across long-word boundry */
		if (sx <= 8) {
			while (sy--) {
				umask = (u_long) * cmask++;
				ubuf = (u_long) * cbuf++ ^ reverse;
				*sc = (*sc & ~(umask >> -sh)) |
				    (ubuf >> -sh);
				*(sc + 1) = (*(sc + 1) &
				    ~(umask << (32 + sh)))
				    | (ubuf << (32 + sh));
				sc += longsperline;
			}
		} else
			if (sx <= 16) {
				while (sy--) {
					umask = (u_long) * smask++;
					ubuf = (u_long) * sbuf++ ^ reverse;
					*sc = (*sc & ~(umask >> -sh)) |
					    (ubuf >> -sh);
					*(sc + 1) = (*(sc + 1) &
					    ~(umask << (32 + sh)))
					    | (ubuf << (32 + sh));
					sc += longsperline;
				}
			} else {
				while (sy--) {
					umask = (u_long) * lmask++;
					ubuf = (u_long) * lbuf++ ^ reverse;
					*sc = (*sc & ~(umask >> -sh)) |
					    (ubuf >> -sh);
					*(sc + 1) = (*(sc + 1) &
					    ~(umask << (32 + sh)))
					    | (ubuf << (32 + sh));
					sc += longsperline;
				}
			}
	} else {
		if (sx <= 8) {
			while (sy--) {
				umask = (u_long) * cmask++;
				ubuf = (u_long) * cbuf++ ^ reverse;
				*sc = (*sc & ~(umask << sh)) |
				    (ubuf << sh);
				sc += longsperline;
			}
		} else
			if (sx <= 16) {
				while (sy--) {
					umask = (u_long) * smask++;
					ubuf = (u_long) * sbuf++ ^ reverse;
					*sc = (*sc & ~(umask << sh)) |
					    (ubuf << sh);
					sc += longsperline;
				}
			} else {
				while (sy--) {
					umask = (u_long) * lmask++;
					ubuf = (u_long) * lbuf++ ^ reverse;
					*sc = (*sc & ~(umask << sh)) |
					    (ubuf << sh);
					sc += longsperline;
				}
			}
	}
}
/*
 * blitWriteAligned()
 *
 * Writes a longword-aligned bitmap to the screen.
 */

void 
blitWriteAligned(struct grf_softc * gp, int x, int y, int sx, int sy,
    register u_long * lbuf, register int reverse)
{
	int     longsperline;
	register u_long *sc;
	u_long *ssc;

	if (y + sy > gp->g_display.height) {
		sy = gp->g_display.height - y;
	}
	if (y < 0) {
		y = 0;
	}
	longsperline = gp->g_display.rowbytes / sizeof(long);
	ssc = (u_long *) (gp->g_display.fbbase) +
	    y * longsperline + x / 32;
	sx /= 32;
	if (reverse) {
		while (sy-- > 0) {
			sc = ssc;
			UNROLL(sx, *sc++ = *lbuf++ ^ 0xffffffff);
			ssc += longsperline;
		}
	} else {
		while (sy-- > 0) {
			sc = ssc;
			UNROLL(sx, *sc++ = *lbuf++);
			ssc += longsperline;
		}
	}
}
