/*
 * vt.h
 *
 * $Id: vt.h,v 1.5 1996/06/09 12:32:33 vuori Exp $
 */

#include <sys/types.h>

#define VT_MAXVT	9
#define	VT_MAXCOLS	132
#define	VT_MAXROWS	100
#define	VT_POOLSIZE	(VT_MAXVT * (VT_MAXROWS + 1))

#define	VT_BUFLEN	128	/* Buffered chars for output	 */
#define	VT_MAXPARS	16	/* Max number of VT100 pars	 */
#define	VT_SBBOTTOM	(-VT_POOLSIZE)	/* Brings sb back to the bottom	 */

/* Text attributes */
#define T_NORMAL	0x00
#define T_BOLD		0x01
#define T_UNDERLINE	0x02
#define T_REVERSE	0x04
#define T_SELECTED	0x08
#define T_INSERT	0x10

/* Cursor types */
#define C_BLOCK		0x01
#define C_UNDERLINE	0x02
#define C_SOLID		0x04
#define C_SLOW		0x08
#define C_FAST		0x10

/* Cursor masks */
#define C_MSHAPE	(C_BLOCK|C_UNDERLINE)
#define C_MBLINK	(C_SOLID|C_SLOW|C_FAST)

/* Color constants and macros */

#define C_BLACK		0
#define	C_RED		1
#define	C_GREEN		2
#define	C_YELLOW	3
#define	C_BLUE		4
#define	C_MAGENTA	5
#define	C_CYAN		6
#define	C_WHITE		7

#define C_BRIGHT	8	/* Different from C_BOLD	 */

#define	SETFG(x,c)	((x) = ((x) & 0xF0) | (c))
#define	SETBG(x,c)	((x) = ((x) & 0x0F) | ((c) << 4))
#define	SETCOLOR(x,f,b)	((x) = (f) | ((b) << 4))

struct vt_s {
	/* Scrollback information */
	int     line[VT_POOLSIZE];	/* Lines in the scrollback pool	 */
	int     head;		/* Lowest line on screen + 1	 */
	int     tail;		/* Last line in scroll back	 */
	int     size;		/* Number of lines altogether	 */
	int     locked;		/* Number of lines locked	 */
	int     sb;		/* Scrolled back lines		 */

	/* Current screen information */
	int     numtcols;	/* Number of text columns	 */
	int     numtrows;	/* Number of text rows		 */
	int     x, y;		/* Cursor location [0..numt-1]	 */
	int     yline;		/* Line in "line" for y		 */
	int     attr, color;	/* Current attribute and color	 */
	int     hanging_cursor;	/* Cursor at last column	 */
	int     grf;		/* Current graphics device	 */
	int     focus;		/* If we have keyboard focus	 */
	int     curscolor;	/* Cursor color			 */
	int     curstype;	/* Cursor shape and blink	 */

	/* Cut and paste information */
	int     copying;	/* If copying			 */
	int     csx, csy;	/* Start of copy region		 */
	int     cex, cey;	/* End of copy region		 */
	int     cdy1, cdy2;	/* What's already been reversed	 */

	/* VT100 information */
	int     s_x, s_y;	/* Saved cursor location	 */
	int     s_attr;		/* Saved attribute		 */
	int     s_color;	/* Saved color			 */
	int     toptrow;	/* Top line of text scroll	 */
	int     bottrow;	/* Bottom line of text scroll	 */
	int     state;		/* State of VT100 parsing	 */
	int     npars;		/* Number of parameters		 */
	int     pars[VT_MAXPARS];	/* VT100 parameters		 */
	int     ques;		/* If ? in VT100 string		 */
	u_int   tab_stop[5];	/* Bit field for tab stops	 */
};

extern struct vt_s vt[VT_MAXVT];

void    vt_init(void);
int     vt_newgrf(int vtnum, int grf, int focus);
void    vt_putchar(int vtnum, u_char buf[], int n);
int     vt_scrollback(int vtnum, int numlines);
void    vt_startcopy(int vtnum, int x, int y);
void    vt_movecopy(int vtnum, int x, int y);
u_char *vt_endcopy(int vtnum, int x, int y, int *areasiz);
