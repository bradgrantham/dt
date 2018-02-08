/* $Id: grf.h,v 1.7 1998/03/20 14:21:00 vuori Exp $ */

/*
 * grf.h
 */

#include <sys/types.h>
#include <inttypes.h>

#ifdef GRF_COMPAT
#include "grfioctl.h"
#else
#include <machine/grfioctl.h>
#endif				/* GRF_COMPAT */

#define MAXTERMBUF	128
#define MAXGRF		16

/* font info */
struct font {
	unsigned char *data;	/* Character bitmaps		 */
	int     width;		/* Pixels across		 */
	int     height;		/* Pixels down			 */
};
/* terminal info */
struct grfterm {
	int     numtrows;	/* Number of text rows		 */
	int     numtcols;	/* Number of text columns	 */
	struct font font;	/* Normal font			 */
	struct font bold;	/* Bold font			 */
	u_int   col[8];		/* Color for the 8 main colors	 */
	uint32_t backcol[8];	/* Like "col" but shifted	 */
	char    buf[MAXTERMBUF];/* Write-ahead buffer		 */
	int     buflen;		/* buf's length			 */
	int     bufx, bufy;	/* Location of buf's start	 */
	int     bufatt, bufcol;	/* Attribute and color of buf	 */
};

struct grfmouse {
	int     x, y;		/* Location and whether drawn	 */
	int     on;		/* If the cursor should be on	 */
	int     disp;		/* If the cursor is visible	 */
	u_short old[16];	/* Save old screen content	 */
};
/* per display info */
struct grf_softc {
	struct grfmode g_display;	/* hardware description (for ioctl) */
	struct grfterm g_term;	/* terminal info (font, size, etc.) */
	struct grfmouse g_mouse;/* mouse information */
};
/* cursor types */
#define	GF_ERASECURSOR		0
#define	GF_FOCUSCURSOR		1
#define	GF_UNFOCUSCURSOR	2

/* display types - indices into grfdev */
#define	GT_MAC	0

/* hardware ids */
#define GID_MAC	1

/* software ids defined in grfioctl.h */

/* requests to mode routine */
#define GM_GRFON	1
#define GM_GRFOFF	2
#define GM_GRFOVON	3
#define GM_GRFOVOFF	4

void    grf_init(char *fontname, int scrn);
void 
grf_writestr(int grf, int len, unsigned char *s, int x, int y, int attr,
    int color);
void    grf_flush(int grf);
void    grf_writechar(int grf, u_char ch, int x, int y, int att, int col);
void    grf_eraserect(int grf, int x1, int y1, int x2, int y2, int color);
void 
grf_writeline(int grf, int x, int y, int n, u_char * scr, u_char * att,
    u_char * col);
void 
grf_scrollup(int grf, int x1, int y1, int x2, int y2, int numlines,
    int color);
void 
grf_scrollleft(int grf, int x1, int y1, int x2, int y2, int numcols,
    int color);
void 
grf_drawcursor(int grf, int type, int x, int y, int curscolor,
    int cursshape, u_char ch, int attr, int color);
void    grf_movemouse(int grf, int on, int x, int y);
void 
grf_getsize(int grf, int *numcols, int *numrows,
    int *numtcols, int *numtrows,
    int *fontcols, int *fontrows);
void    grf_blankscreen(int grf);
int     grf_numgrfs(void);
void    grf_changefont(int grf);
void    grf_exit(void);
