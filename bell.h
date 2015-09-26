#ifdef __STDC__
#define	P(s) s
#else
#define P(s) ()
#endif


/* bell.c */
void bell_init P((int signo));
void bell_shutup P((int signo));
void beep P((void));

#undef P
