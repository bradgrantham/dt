/* $Id: blit.h,v 1.4 1996/05/24 15:17:09 vuori Exp $ */

#define UNROLL(n,c) {							\
	register int	_count;						\
	switch (n) {							\
		default:						\
			_count = (n);					\
			while (_count-- > 16) {				\
				c;					\
			}						\
		case 16: c;						\
		case 15: c;						\
		case 14: c;						\
		case 13: c;						\
		case 12: c;						\
		case 11: c;						\
		case 10: c;						\
		case 9: c;						\
		case 8: c;						\
		case 7: c;						\
		case 6: c;						\
		case 5: c;						\
		case 4: c;						\
		case 3: c;						\
		case 2: c;						\
		case 1: c;						\
		case 0: ;						\
	}								\
}

void 
blitRead(struct grf_softc * gp, int x, int y, int sx, int sy,
    void *vbuf);
void 
blitWrite(struct grf_softc * gp, int x, int y, int sx, int sy,
    void *vbuf, void *vmask, int reverse);
void 
blitWriteAligned(struct grf_softc * gp, int x, int y, int sx, int sy,
    uint32_t * lbuf, int reverse);
