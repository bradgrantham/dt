/*
 * Graphics driver for "desktop".
 *
 * March 15th, 1994 -- Lawrence Kesteloot
 *   Original -- written in the grf driver of the kernel.
 *
 * June 18th, 1994 -- Lawrence Kesteloot
 *   Added support for non-8-pixel-wide fonts.
 *
 * June 26th, 1994 -- Lawrence Kesteloot
 *   Pulled out of the kernel into user-land.
 *
 * October 31st, 1994 -- Monroe Williams
 *   Added fixes for non-640x480 video cards. [Is this only in kernel's grf.c? --DJH]
 *   Changed struct grf_softc to grf_Softc.
 *   Added improvement for scrolling and screen-clearing in grf_scrollup
 *      and grf_blankscreen.
 */

/*
 *	Fearful commenting is leaden
 *		servitor to dull delay
 *	 (Richard III, Act IV, sc iii)
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#include "grf.h"
#include "blit.h"
#include "vt.h"

/* The fonts */
#include "8x13.h"
#include "8x13B.h"
#include "6x10.h"
#include "6x10B.h"

#if defined(USE_OLD_GRF) && !defined(GRF_COMPAT)
#define GRF_COMPAT
#endif

static char rcsid[] = "$Id: grf.c,v 1.17 1998/03/20 14:21:53 vuori Exp $";

static struct grf_softc grf_Softc[MAXGRF];
static int grf_fds[MAXGRF];
static int numgrf;
extern int scrn;

static u_short pointer_arrow[] = {
	0x0000,			/* ................ */
	0x4000,			/* .*.............. */
	0x6000,			/* .**............. */
	0x7000,			/* .***............ */
	0x7800,			/* .****........... */
	0x7C00,			/* .*****.......... */
	0x7E00,			/* .******......... */
	0x7F00,			/* .*******........ */
	0x7F80,			/* .********....... */
	0x7C00,			/* .*****.......... */
	0x6C00,			/* .**.**.......... */
	0x4600,			/* .*...**......... */
	0x0600,			/* .....**......... */
	0x0300,			/* ......**........ */
	0x0300,			/* ......**........ */
	0x0000			/* ................ */
};

static u_short pointer_arrow_mask[] = {
	0xC000,			/* **.............. */
	0xE000,			/* ***............. */
	0xF000,			/* ****............ */
	0xF800,			/* *****........... */
	0xFC00,			/* ******.......... */
	0xFE00,			/* *******......... */
	0xFF00,			/* ********........ */
	0xFF80,			/* *********....... */
	0xFFC0,			/* **********...... */
	0xFFE0,			/* ***********..... */
	0xFE00,			/* *******......... */
	0xEF00,			/* ***.****........ */
	0xCF00,			/* **..****........ */
	0x8780,			/* *....****....... */
	0x0780,			/* .....****....... */
	0x0380			/* ......***....... */
};

caddr_t grfcompat(struct grfmode * modep, int gfd);

/*
 * grf_init()
 *
 *   Probes all of the /dev/grf? devices and gets their information.
*/
void
grf_init(char *fontname, int scrn)
{
	int     f, i, j;
	char    devstr[128];
	caddr_t physscreen;
	struct grf_softc *gp;
	struct grfmouse *gm;
	struct grfmode *gi;
	struct grfterm *gt;

	numgrf = 0;
	for (j = 0, i = scrn; j < MAXGRF; j++, i = (++i) % MAXGRF) {
		grf_fds[i] = -1;
		physscreen = NULL;

		sprintf(devstr, "/dev/grf%x", i);
		f = open(devstr, O_RDWR);
		if (f == -1) {
			if (i == 0)
				perror("error opening grf0 (primary screen)");
			continue;
		}
		gp = &grf_Softc[numgrf];
		gt = &gp->g_term;
		gm = &gp->g_mouse;

#ifndef USE_OLD_GRF
		if (ioctl(f, GRFIOCGMODE, &gp->g_display) == -1) {
#endif /* USE_OLD_GRF */
#ifdef GRF_COMPAT
			physscreen = grfcompat(&gp->g_display, f);
			if (!physscreen) {
				close(f);
				continue;
			}
#else
			perror("GRFIOCGMODE");
			close(f);
			continue;
#endif				/* GRF_COMPAT */
#ifndef USE_OLD_GRF
		}
#endif /* USE_OLD_GRF */
		if (gp->g_display.psize != 1) {
			printf("grf%d: not in 1-bit mode (%d planes)\n", i, gp->g_display.psize);
			close(f);
			continue;
		}
#ifdef GRF_COMPAT
		if (!physscreen)
#endif
			if ((physscreen =
				mmap(0, gp->g_display.fbsize, PROT_READ | PROT_WRITE, MAP_SHARED, f,
				    gp->g_display.fboff)) == (caddr_t) - 1) {
				perror("mmap");
				close(f);
				continue;
			}
		grf_fds[i] = f;

		gi = &gp->g_display;
		gi->fbbase = (caddr_t) physscreen;

		if (strcmp(fontname, "default") == 0) {
			if (gi->width < 1280) {	/* I am not sight-impaired.
						 * Use a realistic font */
				fontname = "small";
			} else {
				fontname = "large";
			}
		}
		gt = &(gp->g_term);
		if (strcmp(fontname, "small") == 0) {
			gt->font.data = Font6x10;
			gt->font.width = 6;
			gt->font.height = 10;
			gt->bold.data = Font6x10B;
			gt->bold.width = 6;
			gt->bold.height = 10;
		} else {
			gt->font.data = Font8x13;
			gt->font.width = 8;
			gt->font.height = 13;
			gt->bold.data = Font8x13B;
			gt->bold.width = 8;
			gt->bold.height = 13;
		}
		gt->numtrows = gi->height / gt->font.height;
		gt->numtcols = gi->width / gt->font.width;
		gt->col[0] = 1;
		gt->col[1] = 1;
		gt->col[2] = 1;
		gt->col[3] = 1;
		gt->col[4] = 1;
		gt->col[5] = 1;
		gt->col[6] = 1;
		gt->col[7] = 0;
		gt->buflen = 0;

		/* print out some info */
		grf_blankscreen(numgrf);
		numgrf++;
	}

	if (numgrf == 0) {
		grf_exit();
		fprintf(stderr, "no usable grf-devices found\n");
		exit(1);
	}
}
/* if we're using the old grf-interface, get info about that and
 * stick it into the new grfmode-struct */
#ifdef GRF_COMPAT
caddr_t
grfcompat(struct grfmode * modep, int gfd)
{
	struct grfinfo olde;
	caddr_t screen;

	if (ioctl(gfd, GRFIOCGINFO, &olde) == -1) {
		perror("GRFIOCGINFO");
		return NULL;
	}
	modep->mode_id = olde.gd_id;
	modep->fbsize = olde.gd_fbsize;
	modep->fboff = 0;	/* XXX */
	modep->rowbytes = olde.gd_fbrowbytes;
	modep->width = olde.gd_fbwidth;
	modep->height = olde.gd_fbheight;
	modep->hres = 0;	/* XXX */
	modep->vres = 0;	/* XXX */
	modep->ptype = 0;
	modep->psize = olde.gd_planes;

	screen = (unsigned char *) 0x10000000 + numgrf * 0x1000000;
	if (ioctl(gfd, GRFIOCMAP, &screen) == -1) {
		perror("GRFIOCMAP");
		return NULL;
	}
	printf("using old grf-if\n");

	return screen;
}
#endif				/* GRF_COMPAT */

/*
 * showmouse()
 *
 *   Displays a mouse if that GRF's mouse should be on.
 */

static void
showmouse(struct grf_softc * gp)
{
	struct grfmouse *gm = &(gp->g_mouse);

	if (gm->on && !gm->disp) {
		blitRead(gp, gm->x, gm->y, 16, 16, gm->old);
		blitWrite(gp, gm->x, gm->y, 16, 16,
		    pointer_arrow, pointer_arrow_mask, 0);
		gm->disp = 1;
	}
}
/*
 * hidemouse()
 *
 *   Hides a mouse if that GRF's mouse is already on.
 */

static void
hidemouse(struct grf_softc * gp)
{
	struct grfmouse *gm = &(gp->g_mouse);

	if (gm->on && gm->disp) {
		blitWrite(gp, gm->x, gm->y, 16, 16, gm->old,
		    pointer_arrow_mask, 0);
		gm->disp = 0;
	}
}
/*
 * grf_writestr()
 *
 *   Writes the first "n" characters of string "s" at location "x", "y" with
 *   attribute "attr" and color "color".
 */

void
grf_writestr(int grf, int len, unsigned char *s, int x, int y, int attr,
    int color)
{
	struct grf_softc *gp;
	register u_char *uc, ul;
/* 	u_char			*sc, *ssc; */
	u_char  bitmask[20];
	register int i;
	int     fbrowbytes, reverse;
	int     width, height, numleft;
	struct font *f;

	u_long  buf[32 * 16];
	int     startx, endx, longstartx, longendx;
	int     numlongs, sh, startchar, endchar;
	register u_long *bufp;
	register int j;

	if (grf == -1) {
		return;
	}
	gp = &grf_Softc[grf];

	if (x < 0) {
		i = -x;
		x += i;
		s += i;
		len -= i;
	}
	i = gp->g_term.numtcols - x;
	if (len > i) {
		len = i;
	}
	if (len <= 0 || y < 0 || y >= gp->g_term.numtrows) {
		return;
	}
	hidemouse(gp);

#ifdef UNDERLINE
	if (attr & T_BOLD) {
#else
	if (attr & T_BOLD || attr & T_UNDERLINE) {
#endif /* UNDERLINE */
		f = &gp->g_term.bold;
	} else {
		f = &gp->g_term.font;
	}

	width = f->width;
	height = f->height;

	reverse = (!(attr & T_REVERSE) != !(attr & T_SELECTED));

#ifdef INVERSE
	reverse = !reverse;
#endif /* INVERSE */

	fbrowbytes = gp->g_display.rowbytes;

	startx = x * width;
	endx = (x + len) * width;
	y *= height;

	bitmask[0] = (1 << width) - 1;
	for (i = 1; i < height; i++) {
		bitmask[i] = bitmask[0];
	}
	ul = 255 & bitmask[0];

	/*
	 * Blit the beginning and end independently from middle
	 */

	longstartx = (startx + 31) & ~31;
	longendx = endx & ~31;

	numleft = (longstartx - startx + width - 1) / width;
	if (numleft > len) {
		numleft = len;
	}
	for (i = 0; i < numleft; i++) {
		uc = f->data + height * s[i];
#ifdef UNDERLINE
		if (attr & T_UNDERLINE)
			*(uc + height - 1) ^= ul;
#endif				/* UNDERLINE */
		blitWrite(gp, startx + i * width, y,
		    width, height, uc,
		    bitmask, reverse);
#ifdef UNDERLINE
		if (attr & T_UNDERLINE)
			*(uc + height - 1) ^= ul;
#endif				/* UNDERLINE */
	}

	/*
	 * Blit middle in one chunk
	 */

	numlongs = (longendx - longstartx) / 32;
	if (numlongs <= 0) {
		goto nomiddle;	/* Yes boys, LAK did a goto */
	}
	for (i = numlongs * height - 1; i >= 0; i--) {
		buf[i] = 0;
	}
	for (j = 0; j < numlongs; j++) {
		startchar = (j * 32 + longstartx - startx) /
		    width;
		endchar = (j * 32 + 31 + longstartx - startx)
		    / width;
		sh = 32 - width - startchar * width +
		    longstartx - startx + j * 32;
		for (i = startchar; i <= endchar;
		    i++, sh -= width) {
#ifdef UNDERLINE
			if (s[i] == ' ' && !(attr & T_UNDERLINE)) {
#else
			if (s[i] == ' ') {
#endif				/* UNDERLINE */
				continue;
			}
			uc = f->data + height * s[i];
#ifdef UNDERLINE
			if (attr & T_UNDERLINE)
				*(uc + height - 1) ^= ul;
#endif				/* UNDERLINE */
			bufp = &buf[j];
			if (sh < 0) {
				sh = -sh;
				UNROLL(height,
				    *bufp |= *uc++ >> sh;
				bufp += numlongs);
				sh = -sh;
			} else {
				UNROLL(height,
				    *bufp |= *uc++ << sh;
				bufp += numlongs);
			}
#ifdef UNDERLINE
			if (attr & T_UNDERLINE)
				*(uc - 1) ^= ul;
#endif				/* UNDERLINE */
		}
	}
	blitWriteAligned(gp, longstartx, y, numlongs * 32,
	    height, buf, reverse);

nomiddle:

	if (longendx >= longstartx) {
		numleft = (endx - longendx + width - 1) / width;
		if (numleft > len) {
			numleft = len;
		}
		for (i = len - numleft; i < len; i++) {
			uc = f->data + height * s[i];
#ifdef UNDERLINE
			if (attr & T_UNDERLINE)
				*(uc + height - 1) ^= ul;
#endif				/* UNDERLINE */
			blitWrite(gp, startx + i * width, y,
			    width, height, uc,
			    bitmask, reverse);
#ifdef UNDERLINE
			if (attr & T_UNDERLINE)
				*(uc + height - 1) ^= ul;
#endif				/* UNDERLINE */
		}
	}
	showmouse(gp);
}
/*
 * grf_flush()
 *
 *   Flushes the write-ahead buffer.
 */

void
grf_flush(int grf)
{
	struct grfterm *gt;
	struct grf_softc *gp;

	if (grf == -1) {
		return;
	}
	gp = &grf_Softc[grf];
	gt = &gp->g_term;

	if (gt->buflen > 0) {
		grf_writestr(grf, gt->buflen, gt->buf, gt->bufx, gt->bufy,
		    gt->bufatt, gt->bufcol);
		gt->buflen = 0;
	}
}
/*
 * grf_writechar()
 *
 *   Writes a character, buffering it if it can.
 */

void
grf_writechar(int grf, u_char ch, int x, int y, int att, int col)
{
	struct grfterm *gt;
	struct grf_softc *gp;

	if (grf == -1) {
		return;
	}
	gp = &grf_Softc[grf];
	gt = &gp->g_term;

	if (gt->buflen != 0 && (gt->bufy != y || gt->bufx + gt->buflen != x ||
		gt->bufatt != att || gt->bufcol != col ||
		gt->buflen == MAXTERMBUF)) {
		grf_flush(grf);
	}
	if (gt->buflen == 0) {
		gt->bufx = x;
		gt->bufy = y;
		gt->bufatt = att;
		gt->bufcol = col;
	}
	gt->buf[gt->buflen++] = ch;
}
/*
 * grf_eraserect()
 *
 *   Writes the background of color "color" into the rectangle (x1,y1)-(x2,y2).
 *   Note that the rectangle is inclusive.
 */

void
grf_eraserect(int grf, int x1, int y1, int x2, int y2, int color)
{
	struct grf_softc *gp;
	u_long *start, *end, longsperline, longspertline, x, y;
	register u_long *ptr, c;
	register int len, i;
	u_char *sc;
	struct font *f;

	if (grf == -1) {
		return;
	}
	grf_flush(grf);

	gp = &grf_Softc[grf];
	f = &gp->g_term.font;

	hidemouse(gp);

	if (x1 < 0) {
		x1 = 0;
	}
	if (y1 < 0) {
		y1 = 0;
	}
	if (x2 > gp->g_term.numtcols - 1) {
		x2 = gp->g_term.numtcols - 1;
	}
	if (y2 > gp->g_term.numtrows - 1) {
		y2 = gp->g_term.numtrows - 1;
	}
#ifdef INVERSE
	c = ~0L;
#else
	c = 0;			/* BG 04/15/94 - I'm not sure what the above
				 * is supposed */
	/* to be, but it's 0x7fffffff, which isn't quite right. */
#endif /* INVERSE */
	longsperline = gp->g_display.rowbytes / sizeof(long);
	longspertline = longsperline * gp->g_term.font.height;

	ptr = (u_long *) (gp->g_display.fbbase);
	start = ptr + y1 * longspertline;
	end = ptr + (y2 + 1) * longspertline;
	if (x1 == 0 && x2 == gp->g_term.numtcols - 1) {
		len = end - start;
		ptr = start;
		for (i = len / 8; i > 0; i--) {
			UNROLL(8, *ptr++ = c);
		}
		UNROLL(len % 8, *ptr++ = c);
	} else {
		if (gp->g_display.psize == 1) {
			if (gp->g_term.font.width == 8) {
				while (start < end) {
					sc = (u_char *) start + x1;
					for (x = x1; x <= x2; x++) {
						*sc++ = c;
					}
					start += longsperline;
				}
			} else {
				for (y = y1; y <= y2; y++) {
					for (x = x1; x <= x2; x++) {
						grf_writestr(grf, 1, " ", x,
						    y, T_NORMAL, color);
					}
				}
			}
		} else {
			for (y = y1; y <= y2; y++) {
				for (x = x1; x <= x2; x++) {
					grf_writestr(grf, 1, " ", x,
					    y, T_NORMAL, color);
				}
			}
		}
	}
	showmouse(gp);
}
/*
 * grf_blankscreen
 *
 *   Clear the grf to zeros or ones if INVERSE. (white)
 */

void
grf_blankscreen(int grf)
{
	long    longcnt;
/* MBW -- added the next two lines for the fix below */
	register long rowcnt, longstore, rowbytes;
	register unsigned long *rowptr;

	register unsigned long *screenptr;
	struct grf_softc *gp;

	if (grf == -1) {
		return;
	}
	grf_flush(grf);		/* don't want leftovers printing... */

	gp = &grf_Softc[grf];

	/*
	 * QuickDraw does not currently define a system for screens
	 * that do not have a row that is long aligned.
	 */

/* MBW -- fix the really inefficient screen-clearing */
	longstore = (gp->g_display.width) / 32;
	rowcnt = gp->g_display.height;
	rowptr = (unsigned long *) gp->g_display.fbbase;
	/* divide by 4 because rowbytes will be added to a (long *) */
	rowbytes = gp->g_display.rowbytes / sizeof(long);
	while (rowcnt--) {
		screenptr = rowptr;
		longcnt = longstore;
		while (longcnt--)
#ifdef INVERSE
			*screenptr++ = ~0L;
#else
			*screenptr++ = 0;
#endif /* INVERSE */
		rowptr += rowbytes;
	}
}
/*
 * grf_flashscreen (int grf)
 *
 *   Reverses the screen twice.
 */

void
grf_flashscreen(int grf)
{
	register int longcnt;
	register unsigned long *screenptr;
	struct grf_softc *gp;

	if (grf == -1) {
		return;
	}
	grf_flush(grf);		/* don't want leftovers printing... */

	gp = &grf_Softc[grf];

	/*
	 * QuickDraw does not currently define a system for screens
	 * that do not have a row that is long aligned.
	 */

	longcnt = gp->g_display.rowbytes * gp->g_display.height;
	screenptr = (unsigned long *) gp->g_display.fbbase;
	while (longcnt--) {
		*screenptr++ ^= 0xffffffff;
	}
	longcnt = gp->g_display.rowbytes * gp->g_display.height;
	screenptr = (unsigned long *) gp->g_display.fbbase;
	while (longcnt--) {
		*screenptr++ ^= 0xffffffff;
	}
}
/*
 * grf_writeline()
 *
 *   Writes the first "n" characters of ("src", "att", "col") at location
 *   "x", "y".  This does not seem terribly useful right now, but maybe
 *   we can optimize it some later.
 */

void
grf_writeline(int grf, int x, int y, int n, u_char * scr, u_char * att,
    u_char * col)
{
	struct grf_softc *gp;
	int     len, start;

	if (grf == -1) {
		return;
	}
	grf_flush(grf);

	gp = &grf_Softc[grf];

	start = 0;
	while (start < n) {
		len = 1;
		while (start + len < n &&
		    att[start] == att[start + len] &&
		    col[start] == col[start + len]) {
			len++;
		}
		grf_writestr(grf, len, scr + start, x + start, y,
		    att[start], col[start]);
		start += len;
	}
}
/*
 * grf_scrollup()
 *
 *   Scrolls the rectangle (x1,y1)-(x2,y2) (inclusive) up "numlines" lines,
 *   and filling in with the background of "color" at the bottom.  Note
 *   that "numlines" can be negative, in which case the rectangle is
 *   scrolled down.
 */

void
grf_scrollup(int grf, int x1, int y1, int x2, int y2, int numlines,
    int color)
{
	struct grf_softc *gp;
	register u_long *ptr1, *ptr2;
	u_long  longspertline /* , numlongs */ ;
/* MBW -- added the following line for the efficiency fix below */
	u_long  rowbytes, bytecount;
	long    rowcnt;

	if (grf == -1) {
		return;
	}
	grf_flush(grf);

	gp = &grf_Softc[grf];

	if (x1 < 0) {
		x1 = 0;
	}
	if (y1 < 0) {
		y1 = 0;
	}
	if (x2 > gp->g_term.numtcols - 1) {
		x2 = gp->g_term.numtcols - 1;
	}
	if (y2 > gp->g_term.numtrows - 1) {
		y2 = gp->g_term.numtrows - 1;
	}
	if (numlines >= y2 - y1 + 1) {
		grf_eraserect(grf, x1, y1, x2, y2, color);
		return;
	}
	hidemouse(gp);

	longspertline = gp->g_display.rowbytes * gp->g_term.font.height /
	    sizeof(long);
	/* MBW -- divide by 4 because it will be added to a (long*) */
	rowbytes = gp->g_display.rowbytes / sizeof(long);

	if (numlines > 0) {
		if (x1 == 0 && x2 == gp->g_term.numtcols - 1) {
			rowcnt = (y2 - y1 + 1 - numlines) * gp->g_term.font.height;
			if (rowcnt < 0) {
				grf_eraserect(grf, x1, y1, x2, y2, color);
				showmouse(gp);
				return;
			}
			bytecount = (7 + gp->g_term.font.width * gp->g_term.numtcols) >> 3;
			ptr1 = (u_long *) (gp->g_display.fbbase) +
			    y1 * longspertline;
			ptr2 = ptr1 + longspertline * numlines;
			while (rowcnt--) {
				bcopy(ptr2, ptr1, bytecount);
				ptr1 += rowbytes;
				ptr2 += rowbytes;
			}
		} else {
			/* XXX */
		}
		grf_eraserect(grf, x1, y2 - numlines + 1, x2, y2, color);
	} else
		if (numlines < 0) {
			if (x1 == 0 && x2 == gp->g_term.numtcols - 1) {
				rowcnt = (y2 - y1 + 1 + numlines) * gp->g_term.font.height;
				if (rowcnt < 0) {
					rowcnt = 0;
					grf_eraserect(grf, x1, y1, x2, y2, color);
					showmouse(gp);
					return;
				}
				bytecount = (7 + gp->g_term.font.width * gp->g_term.numtcols) >> 3;
				ptr1 = (u_long *) (gp->g_display.fbbase) +
				    (y2 + 1) * longspertline;
				ptr1 -= rowbytes;
				ptr2 = ptr1 + longspertline * numlines;
				while (rowcnt--) {
					bcopy(ptr2, ptr1, bytecount);
					ptr1 -= rowbytes;
					ptr2 -= rowbytes;
				}
			} else {
				/* XXX */
			}
			grf_eraserect(grf, x1, y1, x2, y1 - numlines - 1, color);
		}
	showmouse(gp);
}
/*
 * grf_scrollleft()
 *
 *   Scrolls the rectangle (x1,y1)-(x2,y2) (inclusive) left "numcols" columns,
 *   and filling in with the background of "color" at the right.  Note that
 *   "numcols" can be negative, in which case the rectangle is scrolled
 *   right.
 *
 *   NOTE: This function is not implemented and is not even used by vt.c.
 *   I think it would be slower to fiddle with the bits on the screen
 *   than to simply redraw the line from scratch, so vt.c does that
 *   in insertchar() and deletechar().
 */

void
grf_scrollleft(int grf, int x1, int y1, int x2, int y2, int numcols,
    int color)
{
	struct grf_softc *gp;
	int     i;

	if (grf == -1) {
		return;
	}
	if (numcols < -1) {
		/* XXX Hack, but will probably stay this way */
		for (i = 0; i < -numcols; i++) {
			grf_scrollleft(grf, x1, y1, x2, y2, -1, color);
		}
		return;
	}
	if (numcols > 1) {
		/* XXX Hack, but will probably stay this way */
		for (i = 0; i < numcols; i++) {
			grf_scrollleft(grf, x1, y1, x2, y2, 1, color);
		}
		return;
	}
	grf_flush(grf);

	gp = &grf_Softc[grf];

	hidemouse(gp);

	if (numcols == 1) {
	} else
		if (numcols == -1) {
		}
	showmouse(gp);
}
/*
 * grf_drawsquare()
 *
 *  Draws a square of color "color" at location (x,y).
 *  Probably should be implemented someday in case someone
 *  figures some use for GF_UNFOCUSCURSOR. Should take
 *  take the size of the square as an argument too (XXX)
 */

void
grf_drawsquare(int grf, int x, int y, int color)
{
	struct grf_softc *gp;

	if (grf == -1) {
		return;
	}
	gp = &grf_Softc[grf];

	/* XXX */
}
/*
 * grf_drawcursor()
 *
 *   Draws the cursor of type "type" at "x", "y" with color "curscolor" and
 *   shape "cursshape".  The character under the cursor is (ch, attr, color).
 *   The foreground of "curscolor" is the text, and the background of
 *   "curscolor" is the cursor.  For example, in a reversed cursor, the
 *   color "color" would simply be reversed.  "type" can be GF_ERASECURSOR,
 *   in which case the character is simply drawn.  It can be GF_FOCUSCURSOR,
 *   in which case the cursor is drawn with shape "cursshape" and color
 *   "curscolor".  It can be GF_UNFOCUSCURSOR, in which case the cursor
 *   is drawn with color "curscolor" and the shape of an outline.  "cursshape"
 *   has the C_BLOCK bit set, then the focus cursor is the full size of the
 *   character.  If it has the C_UNDERLINE bit set, then it is only the
 *   bottom two rows.
 */

void
grf_drawcursor(int grf, int type, int x, int y, int curscolor,
    int cursshape, u_char ch, int attr, int color)
{
	struct grf_softc *gp;

	if (grf == -1) {
		return;
	}
	gp = &grf_Softc[grf];

	switch (type) {
	case GF_ERASECURSOR:
		grf_writestr(grf, 1, &ch, x, y, attr, color);
		break;
	case GF_FOCUSCURSOR:
		attr ^= T_REVERSE;
		grf_writestr(grf, 1, &ch, x, y, attr, color);
		break;
	case GF_UNFOCUSCURSOR:
		grf_writestr(grf, 1, &ch, x, y, attr, color);
		grf_drawsquare(grf, x, y, color);
		break;
	}

	showmouse(gp);
}
/*
 * grf_movemouse()
 *
 *   Moves the mouse cursor.  "on" is true if the cursor should be on.
 *   x and y are in pixels.
 */

void
grf_movemouse(int grf, int on, int x, int y)
{
	struct grf_softc *gp;
	struct grfmouse *gm;

	if (grf == -1) {
		return;
	}
	gp = &grf_Softc[grf];
	gm = &gp->g_mouse;

	hidemouse(gp);

	gm->x = x;
	gm->y = y;
	gm->on = on;

	showmouse(gp);
}
/*
 * grf_getsize()
 *
 *   Returns the number of pixel and text columns and rows.  If "grf" is -1,
 *   then the maximum size of all GRF's is returned.  (Note that this maximum
 *   size may have, say, the width of one GRF and the height of another.)
 *   Send NULL for unwanted parameters.
 */

void
grf_getsize(int grf, int *numcols, int *numrows,
    int *numtcols, int *numtrows,
    int *fontcols, int *fontrows)
{
	struct grf_softc *gp;

	if (grf != -1) {
		gp = &grf_Softc[grf];

		if (numcols != NULL) {
			*numcols = gp->g_display.width;
		}
		if (numrows != NULL) {
			*numrows = gp->g_display.height;
		}
		if (numtcols != NULL) {
			*numtcols = gp->g_term.numtcols;
		}
		if (numtrows != NULL) {
			*numtrows = gp->g_term.numtrows;
		}
		if (fontcols != NULL) {
			*fontcols = gp->g_term.font.width;
		}
		if (fontrows != NULL) {
			*fontrows = gp->g_term.font.height;
		}
	}
}
/*
 * grf_numgrfs()
 *
 *   Returns the number of GRF devices detected.
 */

int
grf_numgrfs(void)
{
	/* Returns the number of graphics devices */

	return numgrf;
}
/*
 * grf_changefont ()
 *
 *   Toggles the font (small/large) of a GRF.
 */

void
grf_changefont(int grf)
{
	struct grfterm *gt;
	struct grf_softc *gp;
	struct grfmode *gi;

	if (grf == -1) {
		return;
	}
	gp = &grf_Softc[grf];
	gt = &gp->g_term;
	gi = &gp->g_display;

	if (gt->font.data == Font8x13) {
		gt->font.data = Font6x10;
		gt->font.width = 6;
		gt->font.height = 10;
		gt->bold.data = Font6x10B;
		gt->bold.width = 6;
		gt->bold.height = 10;
	} else {
		gt->font.data = Font8x13;
		gt->font.width = 8;
		gt->font.height = 13;
		gt->bold.data = Font8x13B;
		gt->bold.width = 8;
		gt->bold.height = 13;
	}
	gt->numtrows = gi->height / gt->font.height;
	gt->numtcols = gi->width / gt->font.width;
}
/*
 * grf_exit()
 *
 *   Clear the screens and close the file descriptors.
 */

void
grf_exit(void)
{
	int     grfnum;

	for (grfnum = 0; grfnum < numgrf; grfnum++) {
		if (grf_fds[grfnum] != -1) {
			grf_blankscreen(grfnum);
			close(grf_fds[grfnum]);
		}
	}
}
