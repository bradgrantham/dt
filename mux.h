/* $Id: mux.h,v 1.3 1996/05/24 15:17:54 vuori Exp $ */

void    mux_mouseon(int turnon);
void    mux_mousedelta(int x, int y);
void    mux_startcopy(void);
void    mux_contcopy(void);
void    mux_endcopy(void);
void    mux_paste(void);
void    mux_vtscroll(int lines);
void    mux_vtpage(int lines);
void    mux_vtbottom(void);
void    mux_vttop(void);
void    mux_vttogrf(int grfnum);
void    mux_switchtovt(int grfnum);
void    mux_changefont(void);
void    mux_init(void);
void    mux_realpaste(void);
void    mux_copy(void);
