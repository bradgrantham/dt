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


#ifdef HANDLE_ADB
#include <machine/adbsys.h>
#endif

#include "config.h"

#include "event.h"
#include "kbd.h"
#include "mouse.h"

static char rcsid[] = "$Id: event.c,v 1.4 1996/05/24 15:13:15 vuori Exp $";

void    kbd_doevent(int key);
void    mouse_doevent(int dx, int dy, unsigned int buttons, int dpi);


#ifdef HANDLE_ADB
void 
event_handle(
    adb_event_t * event)
{
	switch (event->def_addr) {
		case ADBADDR_KBD:
		kbd_doevent(event->u.k.key);
		break;

	case ADBADDR_MS:
		mouse_doevent(event->u.m.dx, event->u.m.dy,
		    event->u.m.buttons,
		    (event->hand_id == ADBMS_100DPI) ? 100 : 200);
		break;

	case ADBADDR_TABLET:
		/* Maybe I could do something about this someday. */
		break;
	}
}
#endif
