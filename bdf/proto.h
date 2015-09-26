#ifdef __STDC__
# define	P(s) s
#else
# define P(s) ()
#endif


/* bdftoasc.c */
int main P((int argc , char **argv ));
void GetComment P((void ));
int GetStarted P((void ));
int GetSize P((void ));
int GetFonts P((void ));
void GetEncoding P((int *num ));
void MakeFonts P((int amo , int num ));
void NewFont P((int *num , char *name ));
void CloseUp P((void ));
void KillNl P((char *blah ));
void DrawLine P((unsigned char number ));

/* hextoi.c */
int hextoi P((char *hex ));

#undef P
