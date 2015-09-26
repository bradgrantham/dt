/*
 * vt.c
 *
 * Virtual Terminal module
 *
 * The VT module will handle the interface to the tty, the VT100 emulation,
 * the handling of the current screen, and the scrollback.  In COOKED
 * mode, the VT sends the characters from the console to the tty.  In
 * RAW mode, it sends the raw characters, and sends a signal on
 * vt_newgrf().  (RAW mode is for X Windows, etc.)
 *
 * March 12th, 1994 -- Lawrence Kesteloot
 *   Original.  Split from console.c.  Implemented scrollback pool.
 *   Converted VT100 code to state machine and implemented VT102.
 *
 * June 12th, 1994 -- Lawrence Kesteloot
 *   Added cut-and-paste.
 *
 * June 26th, 1994 -- Lawrence Kesteloot
 *   Pulled out of the kernel and into user-land.
 */

#include <stdio.h>
#include <malloc.h>

#include "config.h"

#include "main.h"
#include "grf.h"
#include "vt.h"
#include "bell.h"

static char rcsid[] = "$Id: vt.c,v 1.7 1996/05/24 15:13:52 vuori Exp $";

/* The VT100 states for v->state: */
enum {
	ESnormal,		/* Nothing yet					 */
	ESesc,			/* Got ESC					 */
	ESsquare,		/* Got ESC [					 */
	ESgetpars,		/* About to get or getting the parameters	 */
	ESgotpars,		/* Finished getting the parameters		 */
	ESfunckey,		/* Function key					 */
	EShash,			/* DEC-specific stuff (screen align, etc.)	 */
	ESsetG0,		/* Specify the G0 character set			 */
	ESsetG1,		/* Specify the G1 character set			 */
	ESignore		/* Ignore this sequence				 */
};
/*
 * VTLINE() -- Returns the scrollback index for line y on the screen
 */

#define VTLINE(v,y)	(((v)->head - (v)->numtrows + (y) + VT_POOLSIZE) % \
			VT_POOLSIZE)

/*
 * VTLINESB() -- Same as VTLINE, but takes scrollback into account
 */

#define VTLINESB(v,y)	(((v)->head - (v)->numtrows - (v)->sb + (y) + \
			2 * VT_POOLSIZE) % VT_POOLSIZE)

/*
 * VTSCRSB() -- y = VTSCRSB (v, VTLINESB (v, y))
 */

#define VTSCRSB(v,y)	((v)->numtrows - ((v)->head - (y) + VT_POOLSIZE) % \
			VT_POOLSIZE + (v)->sb)

/*
 * Compare two points -- for cut and paste.
 */

#define PT_BEFORE(x1,y1,x2,y2)	((y1) < (y2) || ((y1) == (y2) && (x1) < (x2)))
#define PT_AFTER(x1,y1,x2,y2)	((y1) > (y2) || ((y1) == (y2) && (x1) > (x2)))
#define PT_EQUAL(x1,y1,x2,y2)	((x1) == (x2) && (y1) == (y2))

#define VT102ID		"\e[?6c"
#define CURSOROFF	0
#define CURSORON	1

struct vt_s vt[VT_MAXVT];

/*
 * The screen information is kept in one set of arrays for all
 * virtual terminals.  VT's can take from this pool for their scroll
 * back, etc.  This allows VT's that use a lot of scroll back to borrow
 * from those that don't.
 */

static char scr[VT_POOLSIZE][VT_MAXCOLS];	/* Character		 */
static char att[VT_POOLSIZE][VT_MAXCOLS];	/* Attribute		 */
static char col[VT_POOLSIZE][VT_MAXCOLS];	/* Color		 */

static int freepool;		/* First free line	 */

/********************************************************************/
/*                                                                  */
/*                    VT Generic Helper Routines                    */
/*                                                                  */
/********************************************************************/

#if 0
static void 
vt_debug(char ch)
{
	struct vt_s *v = &vt[1];
	static int x = 0;

	grf_writestr(v->grf, 1, &ch, x++, 0, 0, 0);
}
#endif

/*
 * clearline()
 *
 *   Clear a line of the modem pool to VT's current color.
 */

static void 
clearline(struct vt_s * v, int line)
{
	int     x, color;

	color = v->color;

	for (x = 0; x < VT_MAXCOLS; x++) {
		scr[line][x] = ' ';
		att[line][x] = T_NORMAL;	/* XXX Not always right	 */
		col[line][x] = color;
	}
}
/*
 * getnewline()
 *
 *   Finds the last line in the scrollback pool and returns an index
 *   to it.  Also informs a VT if a line was taken from it.  Parameter
 *   "v" is the current VT.  This will try not to allocate a line
 *   from the same VT, if it can.  This routine should *always* find a
 *   free (i.e., non-locked) line by the way that we've setup the
 *   size of the pool.  The new line is cleared according to the attribute
 *   and color of "v".
 */

static int 
getnewline(struct vt_s * v)
{
	struct vt_s *bestv, *tryv;
	int     i, line, longest;

	if (freepool < VT_POOLSIZE) {
		clearline(v, freepool);
		return freepool++;
	}
	longest = 0;
	bestv = NULL;
	for (i = 0; i < VT_MAXVT; i++) {
		tryv = &vt[i];
		if (tryv != v && tryv->size > tryv->locked &&
		    tryv->size > longest) {
			longest = tryv->size;
			bestv = tryv;
		}
	}

	if (bestv == NULL) {	/* No other VT had free line */
		if (v->size > v->locked) {
			longest = v->size;
			bestv = v;
		} else {
			/* Should never happen */
			fprintf(stderr, "vt: couldn't find free line.\n");
			exit(1);
		}
	}
	bestv->size--;
	line = bestv->line[bestv->tail];
	bestv->tail = (bestv->tail + 1) % VT_POOLSIZE;

	clearline(v, line);

	/* Inform bestv that it lost one line: */
	vt_scrollback(bestv - &vt[0], 0);

	return line;
}
/*
 * cursor()
 *
 *   Turns a VT's cursor on or off.  Use the constants CURSORON and
 *   CURSOROFF for "turnon".
 */

static void 
cursor(struct vt_s * v, int turnon)
{
	int     line;

	if (turnon == CURSORON) {
		if (v->focus) {
			turnon = GF_FOCUSCURSOR;
		} else {
			turnon = GF_UNFOCUSCURSOR;
		}
	} else {
		turnon = GF_ERASECURSOR;
	}

	line = v->line[VTLINE(v, v->y)];

	grf_drawcursor(v->grf, turnon, v->x, v->y + v->sb,
	    v->curscolor, v->curstype, scr[line][v->x],
	    att[line][v->x], col[line][v->x]);
}
/*
 * drawscreen()
 *
 *   Draws some part of the screen from a VT's scrollback buffer.
 */

static void 
drawscreen(struct vt_s * v, int y1, int y2)
{
	int     yline, line, y;

	yline = VTLINESB(v, y1);

	for (y = y1; y < y2; y++) {
		line = v->line[yline++ % VT_POOLSIZE];
		grf_writeline(v->grf, 0, y, v->numtcols, scr[line], att[line],
		    col[line]);
	}
}
/*
 * scrolldown()
 *
 *   Scroll a region y1 <= y < y2 of the screen down by n lines.
 */

static void 
scrolldown(struct vt_s * v, int y1, int y2, int n)
{
	int     y, line[VT_MAXROWS];

	for (y = 0; y < n; y++) {
		line[y] = v->line[VTLINE(v, y2 - 1 - y)];
	}
	for (y = y2 - 1; y >= y1 + n; y--) {
		v->line[VTLINE(v, y)] = v->line[VTLINE(v, y - n)];
	}
	for (y = 0; y < n; y++) {
		clearline(v, line[y]);
		v->line[VTLINE(v, y1 + y)] = line[y];
	}
	grf_scrollup(v->grf, 0, y1 + v->sb, v->numtcols - 1,
	    y2 - 1 + v->sb, -n, v->color);
}
/*
 * scrollup()
 *
 *   Scroll a region y1 <= y < y2 of the screen up by n lines.
 */

static void 
scrollup(struct vt_s * v, int y1, int y2, int n)
{
	int     y, line[VT_MAXROWS], i;

	/*
	 * The behavior here is that the scrollback is only stored
	 * when the whole screen is being scrolled.  The idea is that
	 * if you've got, say, one VT doing editing and another doing
	 * shell work, you don't want the scrolling in the editing
	 * session to reduce the scrollback in the shell session.  This
	 * doesn't come out quite that well, but it's probably good
	 * reasoning.
	 */

	if (y1 == 0 && y2 == v->numtrows) {
		for (i = 0; i < n; i++) {
			v->line[v->head] = getnewline(v);
			/*
			 * Because getnewline() calls vt_scrollback() which
			 * turns the cursor on.  There should be a better way
			 * to do this:
			 */
			cursor(v, CURSOROFF);
			v->head = (v->head + 1) % VT_POOLSIZE;
			/*
			 * Size will never get too big because of
			 * getnewline():
			 */
			v->size++;
		}
		grf_scrollup(v->grf, 0, y1, v->numtcols - 1, y2 - 1,
		    n, v->color);
	} else {
		for (y = 0; y < n; y++) {
			line[y] = v->line[VTLINE(v, y1 + y)];
		}
		for (y = y1; y < y2 - n; y++) {
			v->line[VTLINE(v, y)] = v->line[VTLINE(v, y + n)];
		}
		for (y = 0; y < n; y++) {
			clearline(v, line[y]);
			v->line[VTLINE(v, y2 - 1 - y)] = line[y];
		}
		grf_scrollup(v->grf, 0, y1 + v->sb, v->numtcols - 1,
		    y2 - 1 + v->sb, n, v->color);
	}

	drawscreen(v, y2 - n, y2);
}


/*
 * movecursordown()
 *
 *   Moves the cursor of "v" down, possibly scrolling also
 */

static void 
movecursordown(struct vt_s * v)
{
	if (v->y < v->bottrow - 1) {
		v->y++;
	} else {
		scrollup(v, v->toptrow, v->bottrow, 1);
	}
}
/*
 * movecursorup()
 *
 *   Moves the cursor of "v" up, possibly scrolling also
 */

static void 
movecursorup(struct vt_s * v)
{
	if (v->y > v->toptrow) {
		v->y--;
	} else {
		scrolldown(v, v->toptrow, v->bottrow, 1);
	}
}
/*
 * movecursorforward()
 *
 *   Moves the cursor of "v" forward, possibly hanging at the end.
 *   If x == numtcols - 1, then we were just on the last column.
 *   We should stay there until the next character.  If the next
 *   character is a C/R, then don't do the movecursordown().
 *   Otherwise, wrap around.
 */

static void 
movecursorforward(struct vt_s * v)
{
	if (v->hanging_cursor) {
		v->x = 0;
		movecursordown(v);
		v->hanging_cursor = 0;
	}
	if (v->x == v->numtcols - 1) {
		v->hanging_cursor = 1;
	} else {
		v->x++;
	}
}
/*
 * deleteline()
 *
 *   Deletes n lines at the current cursor location.
 */

static void 
deleteline(struct vt_s * v, int n)
{
	if (n == 0) {
		n = 1;
	}
	scrollup(v, v->y, v->bottrow, n);
}


/*
 * deletechar()
 *
 *   Deletes n characters at the current cursor location.
 */

static void 
deletechar(struct vt_s * v, int n)
{
	int     x, line;

	if (n == 0) {
		n = 1;
	}
	line = v->line[VTLINE(v, v->y)];

	for (x = v->x; x < VT_MAXCOLS - n; x++) {
		scr[line][x] = scr[line][x + n];
		att[line][x] = att[line][x + n];
		col[line][x] = col[line][x + n];
	}

	for (x = 0; x < n; x++) {
		scr[line][VT_MAXCOLS - 1 - x] = ' ';
		att[line][VT_MAXCOLS - 1 - x] = T_NORMAL;	/* XXX Check this */
		col[line][VT_MAXCOLS - 1 - x] = v->color;
	}

	line = VTSCRSB(v, VTLINE(v, v->y));
	drawscreen(v, line, line + 1);
}
/*
 * insertline()
 *
 *   Inserts n lines at the current cursor location.
 */

static void 
insertline(struct vt_s * v, int n)
{
	scrolldown(v, v->y, v->bottrow, (n == 0) ? 1 : n);
}


/*
 * insertchar()
 *
 *   Inserts n characters at the current cursor location.
 */

static void 
insertchar(struct vt_s * v, int n)
{
	int     x, line;

	if (n == 0) {
		n = 1;
	}
	line = v->line[VTLINE(v, v->y)];

	for (x = VT_MAXCOLS - 1; x >= v->x + n; x--) {
		scr[line][x] = scr[line][x - n];
		att[line][x] = att[line][x - n];
		col[line][x] = col[line][x - n];
	}

	for (x = 0; x < n; x++) {
		scr[line][v->x + x] = ' ';
		att[line][v->x + x] = T_NORMAL;	/* XXX Check this */
		col[line][v->x + x] = v->color;
	}

	line = VTSCRSB(v, VTLINE(v, v->y));
	drawscreen(v, line, line + 1);
}
/*
 * send_string()
 *
 *   Sends the string as if it had been typed at the keyboard.
 */

static void 
send_string(struct vt_s * v, u_char * s)
{
	int     vtnum = v - &vt[0];

	while (*s) {
		main_keyhit(vtnum, *s++);
	}
}
/*
 * send_number()
 *
 *   Sends the number as if it had been typed at the keyboard.
 */

static void 
send_number(struct vt_s * v, int num)
{
	char    buf[10];
	int     i = sizeof(buf);

	buf[--i] = '\0';
	do {
		buf[--i] = num % 10 + '0';
		num /= 10;
	} while (num != 0);

	send_string(v, buf + i);
}
/*
 * erasescreen()
 *
 *   Clears part of the screen.  The argument "which" can be 0 = to end
 *   of screen, 1 = to beginning of screen, and 2 = whole screen.
 */

static void 
erasescreen(struct vt_s * v, int which)
{
	int     start, end, y;

	switch (which) {
	case 0:
		start = v->y;
		end = v->numtrows;
		break;
	case 1:
		start = 0;
		end = v->y;	/* XXX Maybe y + 1?	 */
		break;
	case 2:
		start = 0;
		end = v->numtrows;
		break;
	default:
		return;
	}

	for (y = start; y < end; y++) {
		clearline(v, v->line[VTLINE(v, y)]);
	}

	grf_eraserect(v->grf, 0, start + v->sb, v->numtcols - 1,
	    end - 1 + v->sb, v->color);
}
/*
 * eraseline()
 *
 *   Erases part of the line.  The argument "which" can be 0 = to end
 *   of line, 1 = to beginning of line, and 2 = whole line.
 */

static void 
eraseline(struct vt_s * v, int which)
{
	int     start, end, x, line;

	switch (which) {
	case 0:
		start = v->x;
		end = v->numtcols;
		break;
	case 1:
		start = 0;
		end = v->x + 1;
		break;
	case 2:
		start = 0;
		end = v->numtcols;
		break;
	default:
		return;
	}

	line = v->line[VTLINE(v, v->y)];

	for (x = start; x < end; x++) {
		scr[line][x] = ' ';
		att[line][x] = T_NORMAL;	/* XXX not always right	 */
		col[line][x] = v->color;
	}

	grf_eraserect(v->grf, start, v->y + v->sb, end - 1, v->y + v->sb,
	    v->color);
}
/********************************************************************/
/*                                                                  */
/*                           VT100 Routines                         */
/*                                                                  */
/********************************************************************/

/*
 * respond_ID()
 *
 *   Sends the ID of the terminal.
 */

static void 
respond_ID(struct vt_s * v)
{
	send_string(v, VT102ID);
}
/*
 * status_report()
 *
 *   Sends a string to say that we're OK and alive
 */

static void 
status_report(struct vt_s * v)
{
	send_string(v, "\033[0n");
}
/*
 * cursor_report()
 *
 *   Sends the location of the cursor
 */

static void 
cursor_report(struct vt_s * v)
{
	send_string(v, "\e[");
	send_number(v, v->y + 1);
	send_string(v, ";");
	send_number(v, v->x + 1);
	send_string(v, "R");
}
/*
 * save_cur()
 *
 *   Saves the current state of the VT100 terminal.
 */

static void 
save_cur(struct vt_s * v)
{
	v->s_x = v->x;
	v->s_y = v->y;
	v->s_attr = v->attr;
	v->s_color = v->color;
}
/*
 * restore_cur()
 *
 *   Restores the previous state of the VT100 terminal.
 */

static void 
restore_cur(struct vt_s * v)
{
	v->x = v->s_x;
	v->y = v->s_y;
	v->attr = v->s_attr;
	v->color = v->s_color;
}
/*
 * setVT100color()
 *
 *   Sets the attribute and color of the VT according to the parameter of
 *   the VT100 command.
 */

static void 
setVT100color(struct vt_s * v, int parm)
{
	switch (parm) {
		case 0:v->attr = T_NORMAL;	/* Normal		 */
		SETCOLOR(v->color, C_BLACK, C_WHITE);
		break;
	case 1:
		v->attr |= T_BOLD;	/* Bright		 */
		break;
	case 2:
		v->attr &= ~T_BOLD;	/* Dim			 */
		break;
	case 4:
		v->attr |= T_UNDERLINE;	/* Underline		 */
		break;
	case 5:		/* Not supported *//* Blink		 */
		break;
	case 7:
		v->attr |= T_REVERSE;	/* Reverse		 */
		break;
	case 21:
	case 22:
		v->attr &= ~T_BOLD;	/* Normal intensity	 */
		break;
	case 24:
		v->attr &= ~T_UNDERLINE;	/* No underline		 */
		break;
	case 25:		/* Not supported *//* No blink		 */
		break;
	case 27:
		v->attr &= ~T_REVERSE;	/* No reverse		 */
		break;
	case 30:
		SETFG(v->color, C_BLACK);
		break;
	case 31:
		SETFG(v->color, C_RED);
		break;
	case 32:
		SETFG(v->color, C_GREEN);
		break;
	case 33:
		SETFG(v->color, C_YELLOW);
		break;
	case 34:
		SETFG(v->color, C_BLUE);
		break;
	case 35:
		SETFG(v->color, C_MAGENTA);
		break;
	case 36:
		SETFG(v->color, C_CYAN);
		break;
	case 37:
		SETFG(v->color, C_WHITE);
		break;
	case 39:
		SETFG(v->color, C_BLACK);	/* Default foreground */
		break;
	case 40:
		SETBG(v->color, C_BLACK);
		break;
	case 41:
		SETBG(v->color, C_RED);
		break;
	case 42:
		SETBG(v->color, C_GREEN);
		break;
	case 43:
		SETBG(v->color, C_YELLOW);
		break;
	case 44:
		SETBG(v->color, C_BLUE);
		break;
	case 45:
		SETBG(v->color, C_MAGENTA);
		break;
	case 46:
		SETBG(v->color, C_CYAN);
		break;
	case 47:
		SETBG(v->color, C_WHITE);
		break;
	case 49:
		SETBG(v->color, C_WHITE);	/* Default background */
		break;
	}
}
/*
 * putcharacter ()
 *
 *   Parses the character for possible VT100 command, then takes appropriate
 *   action.
 */

static void 
putcharacter(struct vt_s * v, u_char ch)
{
	register int line, i;

	if (v->state == ESnormal && ch >= ' ' && ch < 256) {
		if (v->hanging_cursor) {
			v->x = 0;
			movecursordown(v);
			v->hanging_cursor = 0;
		}
		if (v->attr & T_INSERT)
			insertchar(v, 1);
		line = v->line[v->yline];	/* XXX Set yline sometime */
		line = v->line[VTLINE(v, v->y)];
		scr[line][v->x] = ch;
		att[line][v->x] = v->attr;
		col[line][v->x] = v->color;
		grf_writechar(v->grf, ch, v->x, v->y + v->sb,
		    v->attr, v->color);
		movecursorforward(v);

		return;
	}
	/*
	 * Check control characters.  These can happen in the middle
	 * of a VT100 sequence.
	 */
	switch (ch) {
	case 0:
		return;		/* Ignore nuls (^@)	 */
	case 7:
		beep();		/* Bell (^G) */
		return;
	case 8:
		if (v->x > 0) {	/* Backspace (^H)	 */
			v->x--;
		}
		v->hanging_cursor = 0;
		return;
	case 9:
		v->hanging_cursor = 0;	/* Tab (^I)		 */
		do {
			putcharacter(v, ' ');
		} while ((v->tab_stop[v->x / 32] &
			(1 << (v->x % 32))) == 0);
		return;
	case 10:		/* Line feed (^J)	 */
	case 11:		/* Line feed (^K)	 */
	case 12:		/* Form feed (^L)	 */
		movecursordown(v);
		v->hanging_cursor = 0;	/* XXX Is this right?	 */
		return;
	case 13:
		v->x = 0;	/* Carriage return (^M)	 */
		v->hanging_cursor = 0;
		return;
	case 14:
		return;		/* Alternate font (^N)	 */
	case 15:
		return;		/* Normal font (^O)	 */
	case 24:		/* (^X)			 */
	case 26:
		v->state = ESnormal;	/* (^Z)			 */
		return;
	case 27:
		v->state = ESesc;	/* (^[)			 */
		return;
	case 127:
		return;		/* Rubout (Delete)	 */
	case 128 + 27:
		v->state = ESsquare;	/* Equivalent to ^[[	 */
		return;
	}

	switch (v->state) {
	case ESesc:
		v->state = ESnormal;
		switch (ch) {
		case '[':
			v->state = ESsquare;
			break;
		case 'E':	/* New line		 */
			putcharacter(v, '\r');
			putcharacter(v, '\n');
			break;
		case 'M':	/* Inverse line feed	 */
			movecursorup(v);
			break;
		case 'D':	/* Line feed		 */
			putcharacter(v, '\n');
			break;
		case 'H':	/* Set tab stop		 */
			v->tab_stop[v->x / 32] |=
			    (1 << (v->x % 32));
			break;
		case 'Z':	/* Send ID		 */
			respond_ID(v);
			break;
		case '7':	/* Save cursor pos	 */
			save_cur(v);
			break;
		case '8':	/* Restore cursor pos	 */
			restore_cur(v);
			break;
		case '(':	/* Set G0		 */
			v->state = ESsetG0;
			break;
		case ')':	/* Set G1		 */
			v->state = ESsetG1;
			break;
		case '#':	/* Hash			 */
			v->state = EShash;
			break;
		case 'c':	/* Reset terminal	 */
			/* XXX does this work ? */
			v->attr = T_NORMAL;
			v->curstype = C_BLOCK;
			v->hanging_cursor = 0;
			v->x = 0;
			v->y = 0;
			v->yline = 0;
			erasescreen(v, 2);
			break;
		case '>':	/* Numeric keypad	 */
			/* XXX */
			break;
		case '=':	/* Appl. keypad		 */
			/* XXX */
			break;
		}
		break;
	case ESsquare:
		for (i = 0; i < VT_MAXPARS; i++) {
			v->pars[i] = 0;
		}
		v->npars = 0;
		v->state = ESgetpars;
		if (ch == '[') {/* Function key		 */
			v->state = ESfunckey;
			break;
		}
		v->ques = (ch == '?');
		if (v->ques) {
			break;
		}
		/* FALL THROUGH */
	case ESgetpars:
		if (ch == ';' && v->npars < VT_MAXPARS - 1) {
			v->npars++;
			break;
		} else
			if (ch >= '0' && ch <= '9') {
				v->pars[v->npars] *= 10;
				v->pars[v->npars] += ch - '0';
				break;
			} else {
				v->npars++;
				v->state = ESgotpars;
			}
		/* FALL THROUGH */
	case ESgotpars:
		v->state = ESnormal;
		switch (ch) {
		case 'h':	/* Keyboard enable	 */
			/* XXX */
			break;
		case 'l':	/* Keyboard disable	 */
			/* XXX */
			break;
		case 'n':	/* Reports		 */
			if (!v->ques) {
				if (v->pars[0] == 5) {
					status_report(v);
				} else
					if (v->pars[0] == 6) {
						cursor_report(v);
					}
			}
			break;
		}
		if (v->ques) {
			v->ques = 0;
			break;
		}
		switch (ch) {
		case 'G':	/* Go to column		 */
		case '`':
			v->x = v->pars[0] - 1;
			break;
		case 'A':	/* Up			 */
			if (v->pars[0] == 0) {
				v->pars[0] = 1;
			}
			v->y -= v->pars[0];
			break;
		case 'B':	/* Down			 */
		case 'e':	/* Down			 */
			if (v->pars[0] == 0) {
				v->pars[0] = 1;
			}
			v->y += v->pars[0];
			break;
		case 'C':	/* Right		 */
		case 'a':	/* Right		 */
			if (v->pars[0] == 0) {
				v->pars[0] = 1;
			}
			v->x += v->pars[0];
			break;
		case 'D':	/* Left			 */
			if (v->pars[0] == 0) {
				v->pars[0] = 1;
			}
			v->x -= v->pars[0];
			break;
		case 'E':	/* Down, first column	 */
			if (v->pars[0] == 0) {
				v->pars[0] = 1;
			}
			v->y += v->pars[0];
			v->x = 0;
			break;
		case 'F':	/* Up, first column	 */
			if (v->pars[0] == 0) {
				v->pars[0] = 1;
			}
			v->y -= v->pars[0];
			v->x = 0;
			break;
		case 'd':	/* Go to line		 */
			v->y = v->pars[0] - 1;
			break;
		case 'h':	/* Enter insert mode	 */
			if (v->pars[0] == 4)
				v->attr |= T_INSERT;
			break;
		case 'l':	/* Exit insert mode	 */
			if (v->pars[0] == 4)
				v->attr &= ~T_INSERT;
			break;
		case 'H':	/* Go to location	 */
			v->x = v->pars[1] - 1;
			v->y = v->pars[0] - 1;
			/* XXX - Fix in all VT-100: */
			v->hanging_cursor = 0;
			break;
		case 'J':	/* Clear part of screen	 */
			erasescreen(v, v->pars[0]);
			break;
		case 'K':	/* Clear part of line	 */
			eraseline(v, v->pars[0]);
			break;
		case 'L':	/* Insert n lines	 */
			insertline(v, v->pars[0]);
			break;
		case 'M':	/* Delete n lines	 */
			deleteline(v, v->pars[0]);
			break;
		case 'P':	/* Delete n characters	 */
			deletechar(v, v->pars[0]);
			break;
		case 'c':	/* Respond with ID	 */
			if (v->pars[0] == 0) {
				respond_ID(v);
			}
			break;
		case 'g':	/* Clear tab stop(s)	 */
			if (v->pars[0]) {
				v->tab_stop[0] = 0;
				v->tab_stop[1] = 0;
				v->tab_stop[2] = 0;
				v->tab_stop[3] = 0;
				v->tab_stop[4] = 0;
			} else {
				v->tab_stop[v->x / 32] &=
				    ~(1 << (v->x % 32));
			}
			break;
		case 'm':	/* Set color		 */
			if (v->npars == 0) {
				v->npars = 1;
			}
			for (i = 0; i < v->npars; i++) {
				setVT100color(v, v->pars[i]);
			}
			break;
		case 'r':	/* Set region		 */
			if (v->pars[0] > 0) {
				v->pars[0]--;
			}
			if (v->pars[1] == 0) {
				v->pars[1] = v->numtrows;
			}
			if (v->pars[0] < v->pars[1] &&
			    v->pars[1] <= v->numtrows) {
				v->toptrow = v->pars[0];
				v->bottrow = v->pars[1];
				v->x = 0;
				v->y = v->toptrow;
			}
			break;
		case 's':	/* Save cursor pos	 */
			save_cur(v);
			break;
		case 'u':	/* Restore cursor pos	 */
			restore_cur(v);
			break;
		case '@':	/* Insert n characters	 */
			insertchar(v, v->pars[0]);
			break;
		case ']':	/* OS-defined		 */
			/* XXX */
			break;
		}
		break;
	case ESfunckey:
		/* XXX What's the point of this? */
		v->state = ESnormal;
		break;
	case EShash:
		v->state = ESnormal;
		if (ch == '8') {
			/* DEC screen alignment test */
		}
		break;
	case ESsetG0:
		if (ch == '0') {
			/* Set graphics character set */
		} else
			if (ch == 'B') {
				/* Set normal character set */
			} else
				if (ch == 'U') {
					/* Set null character set */
				} else
					if (ch == 'K') {
						/* Set user-defined character
						 * set */
					}
		/* If currently G0, then make active set */
		v->state = ESnormal;
		break;
	case ESsetG1:
		if (ch == '0') {
			/* Set graphics character set */
		} else
			if (ch == 'B') {
				/* Set normal character set */
			} else
				if (ch == 'U') {
					/* Set null character set */
				} else
					if (ch == 'K') {
						/* Set user-defined character
						 * set */
					}
		/* If currently G1, then make active set */
		v->state = ESnormal;
		break;
	default:
		v->state = ESnormal;
	}
	if (v->x < 0) {
		v->x = 0;
	}
	if (v->x >= v->numtcols) {
		v->x = v->numtcols - 1;
	}
/* from Francois Pays */
	if (v->y < 0) {
		v->y = 0;
	}
/* (whatever this fix was) */
	if (v->y >= v->bottrow) {
		v->y = v->bottrow - 1;
	}
}
/********************************************************************/
/*                                                                  */
/*                    Interface to rest of Desktop                  */
/*                                                                  */
/********************************************************************/


void 
vt_init(void)
{
	int     vtnum;

	/*
	 * Put all of the scrollback pool in the free list.
	 */

	freepool = 0;

	/*
	 * Initialize info for each VT.
	 */

	for (vtnum = 0; vtnum < VT_MAXVT; vtnum++) {
		struct vt_s *v = &vt[vtnum];

		/*
		 * Here we assume that mux will put us on grf 0 first, before
		 * we get switched to another grf.  This is a reasonable
		 * assumption and solves a lot of problems, like allocating
		 * a screen of a different size than it will be when displayed.
		 */

		grf_getsize(0, NULL, NULL, &v->numtcols, &v->numtrows,
		    NULL, NULL);
		v->head = 0;
		v->tail = 0;
		v->size = 0;
		v->locked = v->numtrows;
		v->sb = 0;

		while (v->size < v->locked) {
			v->line[v->head] = getnewline(v);
			v->head = (v->head + 1) % VT_POOLSIZE;
			v->size++;
		}

		v->x = 0;
		v->y = 0;
		v->attr = T_NORMAL;
		SETCOLOR(v->color, C_BLACK, C_WHITE);
		v->hanging_cursor = 0;
		v->grf = -1;
		v->focus = 0;
		SETCOLOR(v->color, C_WHITE, C_BLACK);
		v->curstype = C_BLOCK | C_SOLID;

		v->s_x = 0;
		v->s_y = 0;
		v->toptrow = 0;
		v->bottrow = v->numtrows;
		v->state = ESnormal;
		v->tab_stop[0] = 0x01010100;	/* First column not set	 */
		v->tab_stop[1] = 0x01010101;
		v->tab_stop[2] = 0x01010101;
		v->tab_stop[3] = 0x01010101;
		v->tab_stop[4] = 0x01010101;
		v->copying = 0;
	}
}

void 
vt_putchar(int vtnum, u_char buf[], int n)
{
	int     i;
	struct vt_s *v;

	if (vtnum < 0 || vtnum >= VT_MAXVT) {
		return;
	}
	v = &vt[vtnum];

	cursor(v, CURSOROFF);
	for (i = 0; i < n; i++) {
		putcharacter(v, (u_char) buf[i]);
	}
	grf_flush(v->grf);
	cursor(v, CURSORON);
}
/*
 * vt_scrollback()
 *
 *   Called by the console when the user wants to scroll back.  If numlines
 *   is > 0, then scroll back that many lines.  If numlines < 0, then scroll
 *   forward.  If numlines = 0, do nothing.  To bring the scrollback back to
 *   the bottom, use VT_SBBOTTOM.
 */

int 
vt_scrollback(int vtnum, int numlines)
{
	struct vt_s *v;
	int     oldsb, y1, y2;

	if (vtnum < 0 || vtnum >= VT_MAXVT) {
		return -1;
	}
	v = &vt[vtnum];
	oldsb = v->sb;

	v->sb += numlines;

	if (v->sb < 0) {
		v->sb = 0;
	}
	if (v->sb + v->numtrows > v->size) {
		v->sb = v->size - v->numtrows;
	}
	numlines = v->sb - oldsb;
	grf_scrollup(v->grf, 0, 0, v->numtcols - 1, v->numtrows - 1, -numlines,
	    v->color);

	if (numlines > 0) {
		y1 = 0;
		y2 = numlines;
		if (y2 >= v->numtrows) {
			y2 = v->numtrows;
		}
	} else {
		y1 = v->numtrows + numlines;
		y2 = v->numtrows;
		if (y1 < 0) {
			y1 = 0;
		}
	}

	drawscreen(v, y1, y2);
	cursor(v, CURSORON);

	return 0;
}
/*
 * vt_newgrf()
 *
 *   Called by the console if this VT is being reassigned a GRF device (or
 *   no GRF device at all).  The VT should figure out what just happened and
 *   resize/reconfigure itself properly.
 */

int 
vt_newgrf(int vtnum, int grf, int focus)
{
	struct vt_s *v;
	int     oldgrf, oldnumtrows, oldnumtcols;

	if (vtnum < 0 || vtnum >= VT_MAXVT) {
		return -1;
	}
	v = &vt[vtnum];

	if (v->grf != -1) {
		/* XXX Clean up. e.g., disable blinking cursor */
	}
	oldgrf = v->grf;
	v->grf = grf;
	v->focus = focus;

	if (v->grf == -1) {
		return 0;
	}
	oldnumtrows = v->numtrows;
	oldnumtcols = v->numtcols;

	grf_getsize(v->grf, NULL, NULL, &v->numtcols, &v->numtrows,
	    NULL, NULL);

	main_resize(vtnum, v->numtrows, v->numtcols);

	if (v->toptrow >= v->numtrows) {
		v->toptrow = v->numtrows - 1;
	}
	if (v->bottrow > v->numtrows || v->bottrow == oldnumtrows) {
		v->bottrow = v->numtrows;
	}
	/*
	 * XXX: When will locked ever not be equal to numtrows?
	 */

	v->locked = v->numtrows;

	while (v->size < v->locked) {
		v->line[v->head] = getnewline(v);
		v->head = (v->head + 1) % VT_POOLSIZE;
		v->size++;
	}

	if (v->grf != oldgrf || v->numtrows != oldnumtrows ||
	    v->numtcols != oldnumtcols) {
		v->y += v->numtrows - oldnumtrows;
		if (v->y < 0) {
			v->y = 0;
		}
		drawscreen(v, 0, v->numtrows);
	}
	cursor(v, CURSORON);

	return 0;
}

static void 
highlight(struct vt_s * v, int copying, int newx, int newy)
{
	int     x, y, width, x1, y1, x2, y2, top, bottom, line;

	width = v->numtcols;
	if (copying && !v->copying) {
		v->copying = 1;
		v->csx = newx;
		v->csy = newy;
		v->cex = newx;
		v->cey = newy;
		top = 0;
		bottom = 0;
	} else
		if (!copying && v->copying) {
			/*
			 * Unselect everything.  Points must be sorted.
			 */

			v->copying = 0;
			for (y = v->csy; y <= v->cey; y++) {
				line = v->line[y];
				for (x = 0; x < width; x++) {
					att[line][x] &= ~T_SELECTED;
				}
			}
			top = VTSCRSB(v, v->csy);
			bottom = VTSCRSB(v, v->cey) + 1;
		} else {
			if (PT_BEFORE(newx, newy, v->cex, v->cey)) {
				x1 = newx;
				y1 = newy;
				x2 = v->cex;
				y2 = v->cey;
			} else {
				x1 = v->cex;
				y1 = v->cey;
				x2 = newx;
				y2 = newy;
			}
			top = VTSCRSB(v, y1);
			bottom = VTSCRSB(v, y2) + 1;
			if (y1 == y2) {
				line = v->line[y1];
				for (x = x1; x < x2; x++) {
					att[line][x] ^= T_SELECTED;
				}
			} else {
				line = v->line[y1];
				for (x = x1; x < width; x++) {
					att[line][x] ^= T_SELECTED;
				}
				for (y = y1 + 1; y < y2; y++) {
					line = v->line[y];
					for (x = 0; x < width; x++) {
						att[line][x] ^= T_SELECTED;
					}
				}
				line = v->line[y2];
				for (x = 0; x < x2; x++) {
					att[line][x] ^= T_SELECTED;
				}
			}
			v->cex = newx;
			v->cey = newy;
		}

	if (top < 0) {
		top = 0;
	}
	if (bottom > v->numtrows) {
		bottom = v->numtrows;
	}
	drawscreen(v, top, bottom);
}
/*
 * vt_startcopy()
 *
 *   This is called when the user presses the mouse button.  x and y
 *   are in text coordinates and represent the space between the
 *   characters.  The coordinate (0,0) is to the left of the top-left
 *   character, etc.
 */

void 
vt_startcopy(int vtnum, int x, int y)
{
	struct vt_s *v;

	if (vtnum < 0 || vtnum >= VT_MAXVT) {
		return;
	}
	v = &vt[vtnum];

	if (v->copying) {
		highlight(v, 0, 0, 0);
	}
	highlight(v, 1, x, VTLINESB(v, y));
}
/*
 * vt_movecopy()
 *
 *   This is called when the user moves the mouse with the button down.
 *   x and y are in text coordinates and represent the current mouse
 *   position.
 */

void 
vt_movecopy(int vtnum, int x, int y)
{
	struct vt_s *v;
	int     line;

	if (vtnum < 0 || vtnum >= VT_MAXVT) {
		return;
	}
	v = &vt[vtnum];

	line = VTLINESB(v, y);
	if (!PT_EQUAL(x, line, v->cex, v->cey)) {
		highlight(v, 1, x, line);
	}
}
/*
 * vt_endcopy()
 *
 *   This is called when the user releases the button after selecting
 *   some text.
 */

u_char *
vt_endcopy(int vtnum, int x, int y, int *areasiz)
{
	struct vt_s *v;
	int     tmp, i, start, len, size, line;
	u_char *buf, *s;

	if (vtnum < 0 || vtnum >= VT_MAXVT) {
		return NULL;
	}
	v = &vt[vtnum];

	highlight(v, 1, x, VTLINESB(v, y));

	if (PT_BEFORE(v->cex, v->cey, v->csx, v->csy)) {
		tmp = v->cey;
		v->cey = v->csy;
		v->csy = tmp;
		tmp = v->cex;
		v->cex = v->csx;
		v->csx = tmp;
	}
	/* highlight (v, 0, x, VTLINESB (v, y)); */

	if (PT_EQUAL(v->csx, v->csy, v->cex, v->cey)) {
		return NULL;
	}
	size = (v->cey - v->csy + 1) * v->numtcols;

	buf = (u_char *) malloc(size);
	*areasiz = 0;
	if (buf == NULL) {
		printf("vt_endcopy (): could not malloc\n");
		return NULL;
	}
	*areasiz = size;

	line = v->csy;
	s = buf;
	do {
		line %= VT_POOLSIZE;
		for (len = v->numtcols - 1; len >= 0; len--) {
			if (scr[v->line[line]][len] != ' ') {
				break;
			}
		}
		len++;
		if (line == v->csy) {
			start = v->csx;
		} else {
			start = 0;
		}
		if (line == v->cey && v->cex < len) {
			len = v->cex;
		}
		for (i = start; i < len; i++) {
			*s++ = scr[v->line[line]][i];
		}
		if (line != v->cey) {
			*s++ = '\r';	/* Simulate Enter key	 */
		}
	} while (line++ != v->cey);

	*s++ = '\0';

	return buf;
}
