/* $Id: func.h,v 1.3 1996/05/24 15:17:33 vuori Exp $ */

#ifdef __STDC__
#define	P(s) s
#else
#define P(s) ()
#endif


/* func.c */
int func_dokey P((int key));
int func_load P((char *file));
int func_init P((char reload));
void killnl P((char *str));

#undef P
