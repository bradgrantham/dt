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

#include <sys/types.h>
#include <ctype.h>

#define KEYBOARD_ARRAY

#ifdef FI_LAYOUT
 #include "fikeyboard.h"
#else  /* FI_LAYOUT */
 #ifdef FR_LAYOUT
  #include "frkeyboard.h"
 #else  /* FR_LAYOUT */
  #ifdef GER_LAYOUT
   #include "gerkeyboard.h"
  #else
   #include "uskeyboard.h"
  #endif /* GER_LAYOUT */
 #endif /* FR_LAYOUT */
#endif /* FI_LAYOUT */

#include "kbd.h"
#include "mux.h"
#include "main.h"
#include "func.h"
#include "digraph.h"

#ifndef APPLICATION
#define VTCHAR '['
#else				/* NORMAL */
#define VTCHAR 'O'
#endif

static char rcsid[] = "$Id: kbd.c,v 1.8 1996/06/09 12:32:25 vuori Exp $";

extern int mouse_on;
extern int mouse_vis;

 /* keep mem usage down -- kbd keys range from 0 to 0x7f: 16 * 8. */
static unsigned char kbd_keydown[16];
 /* Is key down? */
#define ISKEYDOWN(key) (kbd_keydown[(key)>> 3] & (1 << ((key) & 7)))
 /* Make key down */
#define KEYISDOWN(key) (kbd_keydown[(key)>>3] |= (1 << ((key) & 7)))
 /* Make key up */
#define KEYISUP(key) (kbd_keydown[(key)>>3] &= ~(1 << ((key) & 7)))
 /* Eight-bit compliant isalpha() */
#define isealpha(ch)  (((ch)>='A'&&(ch)<='Z')||((ch)>='a'&&(ch)<='z')||((ch)>=0xC0&&(ch)<=0xFF))

/* indices for modifier keys on the key map */
#define MAP_NORMAL 0
#define MAP_SHIFT 1
#define MAP_CTRL 2
#define MAP_OPTION 3
#define MAP_OPTSHFT 4

extern int mux_curvt;		/* saves us a little pain */

unsigned char indig = 0;	/* are we processing a digraph */

/*
 * Turn a scan code to characters, based on the status of keyboard
 *
 * How to handle function keys?
 * Should I mess with NumLock here?
 */
int 
kbd_scantokey(
    int key,
    u_char * chars)
{				/* chars must be able to hold at least 3 */
	/* but 255 might be safer, if Fkeys become */
	/* macros */
	int     keyssofar = 0;

	key &= 0x7f;		/* don't care about key ups */

	/* First, check for arrow keys: */
	switch (key) {
	case ADBK_LEFT:
		chars[0] = 27;	/* Left */
		chars[1] = VTCHAR;
		chars[2] = 'D';
		return (3);

	case ADBK_RIGHT:
		/* Right C */
		chars[0] = 27;
		chars[1] = VTCHAR;
		chars[2] = 'C';
		return (3);

	case ADBK_DOWN:
		/* Down B */
		chars[0] = 27;
		chars[1] = VTCHAR;
		chars[2] = 'B';
		return (3);

	case ADBK_UP:
		/* Up A */
		chars[0] = 27;
		chars[1] = VTCHAR;
		chars[2] = 'A';
		return (3);

	case ADBK_PGUP:
		/* pgup */
		chars[0] = 27;
		chars[1] = '[';
		chars[2] = '5';
		chars[3] = '~';
		return (4);

	case ADBK_PGDN:
		/* pgdn */
		chars[0] = 27;
		chars[1] = '[';
		chars[2] = '6';
		chars[3] = '~';
		return (4);

	case ADBK_HOME:
		/* home */
		chars[0] = 27;
		chars[1] = '[';
		chars[2] = '1';
		chars[3] = '~';
		return (4);

	case ADBK_END:
		/* end */
		chars[0] = 27;
		chars[1] = '[';
		chars[2] = '4';
		chars[3] = '~';
		return (4);

		/* function keys */
	case ADBK_F1:
		func_dokey(0);
		return 0;
	case ADBK_F2:
		func_dokey(1);
		return 0;
	case ADBK_F3:
		func_dokey(2);
		return 0;
	case ADBK_F4:
		func_dokey(3);
		return 0;
	case ADBK_F5:
		func_dokey(4);
		return 0;
	case ADBK_F6:
		func_dokey(5);
		return 0;
	case ADBK_F7:
		func_dokey(6);
		return 0;
	case ADBK_F8:
		func_dokey(7);
		return 0;
	case ADBK_F9:
		func_dokey(8);
		return 0;
	case ADBK_F10:
		func_dokey(9);
		return 0;
	case ADBK_F11:
		func_dokey(10);
		return 0;
	case ADBK_F12:
		func_dokey(11);
		return 0;
	case ADBK_F15:
		func_init(1);
		return 0;
	}

	if (keyboard[key][0] == 0)
		return (0);

#ifndef OPTSET
	if (ISKEYDOWN(ADBK_OPTION)) {	/* OPTION doubles for Meta */
		/* Meta means "prefix with ESC" -- Emacs */
		chars[keyssofar++] = 27;
	}
#endif				/* OPTSET */

	if (ISKEYDOWN(ADBK_CONTROL)) {	/* CTRL */
		if (keyboard[key][MAP_CTRL] == 0 && key != ADBK_SPACE)
			return 0;
		chars[keyssofar++] = keyboard[key][MAP_CTRL];
#ifdef OPTSET
	} else
		if (ISKEYDOWN(ADBK_OPTION) && ISKEYDOWN(ADBK_SHIFT)) {	/* OPTION + SHIFT */
			if (keyboard[key][MAP_OPTSHFT] == 0)
				return 0;
			chars[keyssofar++] = keyboard[key][MAP_OPTSHFT];
#endif				/* OPTSET */
		} else
			if (ISKEYDOWN(ADBK_SHIFT)) {	/* SHIFT */
				if (keyboard[key][MAP_SHIFT] == 0)
					return 0;
				chars[keyssofar++] = keyboard[key][MAP_SHIFT];
#ifdef OPTSET
			} else
				if (ISKEYDOWN(ADBK_OPTION)) {	/* OPTION */
					if (keyboard[key][MAP_OPTION] == 0)
						return 0;
					chars[keyssofar++] = keyboard[key][MAP_OPTION];
#endif				/* OPTSET */
				} else
					if (ISKEYDOWN(ADBK_CAPSLOCK)) {	/* CAPSLOCK */
						if (isealpha(keyboard[key][MAP_SHIFT]))
							chars[keyssofar++] = keyboard[key][MAP_SHIFT];
						else
							chars[keyssofar++] = keyboard[key][MAP_NORMAL];
					} else {	/* nothing */
						chars[keyssofar++] = keyboard[key][MAP_NORMAL];
					}

	return (keyssofar);
}


void 
kbd_setkeybits(
    int key)
{
	/*
	 * NumLock is handled differently here because it is a toggle
	 * key.  That is, one pair of DOWN-then-UP is really equal
	 * to DOWN, and another pair is equal to UP.  We don't have
	 * to do this with CapsLock because the key itself sticks
	 * down.
	 */

	switch (key & 0x7f) {	/* set key up and key down */
		case 0x47:	/* num lock */
		if (!(key & 0x80)) {
			if (ISKEYDOWN(key))
				KEYISUP(key);
			else
				KEYISDOWN(key);
		}
		break;
#ifdef LAME_OPTION
	case ADBK_OPTION:	/* work around brokedness in old adb.c */
		if (ISKEYDOWN(ADBK_OPTION)) {
			KEYISUP(ADBK_OPTION);
		} else {
			KEYISDOWN(ADBK_OPTION);
		}
		break;
#endif				/* LAME_OPTION */
	default:
		if (key & 0x80) {
			KEYISUP((key & 0x7f));
			/* if((key & 0x7f) == ADBK_CAPSLOCK)
			 * kbd_setlight(0x02); */
		} else {
			/* if((key & 0x7f) == ADBK_CAPSLOCK)
			 * kbd_setlight(0x0); */
			if ((key & 0x7f) == ADBK_OPTION && ISKEYDOWN(ADBK_OPTION))
				KEYISUP(ADBK_OPTION);
			else
				KEYISDOWN(key);
		}
		break;
	}
}
/* checks whether character ch starts a digraph */
unsigned char 
isdig(unsigned char ch)
{
	int     ndig = countdig(digs);

	while (ndig--)
		if (digs[ndig] == ch)
#ifdef MUNGE_DIGRAPH
			return ((ch >= MUNGEMIN) && (ch <= MUNGEMAX)) ? switch_digs[ch - 0x80] : ch;
#else
			return ch;
#endif
	return 0;
}
/* return the result of the digraph if available, otherwise return the
 * character that started the digraph */
unsigned char 
dodig(unsigned char ch, unsigned char *ochar)
{
	unsigned char uch, nch;

	if ((ch < 'A' || ch > 'Z') && (ch < 'a' || ch > 'z')) {
		nch = indig;
		indig = 0;
		return nch;
	}
	if (islower(ch))
		uch = toupper(ch);
	else
		uch = ch;

	uch -= 'A';

	switch (indig) {
	case 0xA8:
		nch = umldig[uch];
		break;
	case '`':
		nch = gravedig[uch];
		break;
	case 0xB4:
		nch = aguedig[uch];
		break;
	case '~':
		nch = tildedig[uch];
		break;
	case '^':
		nch = circumdig[uch];
		break;
	default:
		nch = 0;
		break;
	}

	if (nch == 0) {
		nch = indig;
		indig = 0;
		return nch;
	}
	if (islower(ch))
		nch = lodig(nch);

	indig = 0;
	*ochar = 0;
	return nch;
}
/*
 * Take care of storing keydown and keyup,
 * take care of flower/command keys, which control VTs and GRFs
 * directly, and also take care of distinguishing RAW from COOKED
 * key strokes, if the keypress can be sent on to VT.
 */
void 
kbd_doevent(
    int key)
{
	int     numchars;
	u_char  chars[256];
	u_char  ochar = 0;
	int     i;

	kbd_setkeybits(key);

	if (!ADBK_PRESS(key)) {
		/* Key up... */
		return;
	}
	numchars = kbd_scantokey(key, chars);

	if (ISKEYDOWN(ADBK_FLOWER)) {	/* flower/command */
		switch (key) {
		case ADBK_F:	/* font change */
			mux_changefont();
			break;

		case ADBK_P:	/* pointer */
			mouse_on = !mouse_on;
			mux_mouseon(mouse_on);
#ifdef HIDE_MOUSE
			mouse_vis = 1;	/* make sure we show the pointer when
					 * it's turned on */
#endif				/* HIDE_MOUSE */
			break;

		case ADBK_O:	/* open new vt */
			main_newvt();
			break;

		case ADBK_X:	/* old paste */
			mux_paste();
			break;

		case ADBK_C:	/* copy, mac-style */
			mux_copy();
			break;

		case ADBK_V:	/* paste, mac-style */
			mux_realpaste();
			break;

			/* Digits: */
		case ADBK_1:
		case ADBK_2:
		case ADBK_3:
		case ADBK_4:
		case ADBK_5:
		case ADBK_6:
		case ADBK_7:
		case ADBK_8:
		case ADBK_9:
			i = keyboard[key][0] - '1';
			if (ISKEYDOWN(ADBK_SHIFT)) {
				mux_vttogrf(i);
			} else {
				mux_switchtovt(i);
			}
			break;

		case ADBK_UP:	/* up */
			mux_vtscroll(1);
			break;
		case ADBK_DOWN:/* dn */
			mux_vtscroll(-1);
			break;
		case ADBK_PGUP:/* pgup */
			mux_vtpage(1);
			break;
		case ADBK_PGDN:/* pgdn */
			mux_vtpage(-1);
			break;
		case ADBK_END:	/* end */
			mux_vtbottom();
			break;
		case ADBK_HOME:/* home */
			mux_vttop();
			break;
		}
		return;
	}
#ifdef HIDE_MOUSE
	if (mouse_on && mouse_vis && numchars) {	/* we have actual
							 * typing, hide the
							 * mouse */
		mux_mouseon(!mouse_on);
		mouse_vis = 0;
	}
#endif				/* HIDE_MOUSE */

	for (i = 0; i < numchars; i++) {
		if (indig) {
			if (chars[i] != ' ')
				ochar = chars[i];
			else
				ochar = 0;
			chars[i] = dodig(chars[i], &ochar);
			goto hoppari;
		}		/* process a digraph */
		if ((indig = isdig(chars[i])))
			continue;	/* if we got a digraph key, don't say
					 * it yet */
hoppari:			/* I'm becoming a serious goto-user */
		main_keyhit(mux_curvt, chars[i]);
		if (ochar) {
			main_keyhit(mux_curvt, ochar);
			ochar = 0;
		}
	}
}
