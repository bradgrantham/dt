/* $Id: main.h,v 1.3 1996/05/24 15:17:47 vuori Exp $ */

#include <sys/types.h>

extern int vt_to_pty[];

void    main_keyhit(int vtnum, u_char ch);
void    main_resize(int vtnum, int rows, int cols);
void    main_newvt(void);
void    main_strhit(int, u_char *, int);
