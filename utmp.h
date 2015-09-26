/* $Id: utmp.h,v 1.4 1997/04/24 12:27:48 vuori Exp $ */

#include <utmp.h>

#ifdef __STDC__
#define	P(s) s
#else
#define P(s) ()
#endif


/* utmp.c */
int utmp_init P((char *utmpname));
int utmp_addentry P((int, char *));
int utmp_delentry P((int));
void utmp_exit P((void));

#undef P

#include "vers.h"

#ifdef OJNK
#define UTMP_HOST "ojnk"
#else
#define UTMP_HOST VERSNUM
#endif				/* OJNK */
