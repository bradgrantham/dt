/* $Id: digraph.h,v 1.4 1996/05/24 15:17:16 vuori Exp $ */

#define countdig(digs)  (sizeof((digs)) / sizeof(unsigned char))
#define lodig(dig)	((dig) - 0xC4 + 0xE4)

unsigned char umldig[] = {
	0xC4,			/* A */
	0,			/* B */
	0,			/* C */
	0,			/* D */
	0xCB,			/* E */
	0,			/* F */
	0,			/* G */
	0,			/* H */
	0xCF,			/* I */
	0,			/* J */
	0,			/* K */
	0,			/* L */
	0,			/* M */
	0,			/* N */
	0xD6,			/* O */
	0,			/* P */
	0,			/* Q */
	0,			/* R */
	0,			/* S */
	0,			/* T */
	0xDC,			/* U */
	0,			/* V */
	0,			/* W */
	0,			/* X */
	0xDF,			/* Y */
0, /* Z */ };

unsigned char gravedig[] = {	/* accent like ` */
	0xC0,			/* A */
	0,			/* B */
	0,			/* C */
	0,			/* D */
	0xC8,			/* E */
	0,			/* F */
	0,			/* G */
	0,			/* H */
	0xCC,			/* I */
	0,			/* J */
	0,			/* K */
	0,			/* L */
	0,			/* M */
	0,			/* N */
	0xD2,			/* O */
	0,			/* P */
	0,			/* Q */
	0,			/* R */
	0,			/* S */
	0,			/* T */
	0xD9,			/* U */
	0,			/* V */
	0,			/* W */
	0,			/* X */
	0,			/* Y */
0, /* Z */ };

unsigned char aguedig[] = {	/* accent like ' */
	0xC1,			/* A */
	0,			/* B */
	0,			/* C */
	0,			/* D */
	0xC9,			/* E */
	0,			/* F */
	0,			/* G */
	0,			/* H */
	0xCD,			/* I */
	0,			/* J */
	0,			/* K */
	0,			/* L */
	0,			/* M */
	0,			/* N */
	0xD3,			/* O */
	0,			/* P */
	0,			/* Q */
	0,			/* R */
	0,			/* S */
	0,			/* T */
	0xDA,			/* U */
	0,			/* V */
	0,			/* W */
	0,			/* X */
	0xDD,			/* Y */
0, /* Z */ };

unsigned char tildedig[] = {	/* accent like ~ */
	0xC3,			/* A */
	0,			/* B */
	0,			/* C */
	0,			/* D */
	0,			/* E */
	0,			/* F */
	0,			/* G */
	0,			/* H */
	0,			/* I */
	0,			/* J */
	0,			/* K */
	0,			/* L */
	0,			/* M */
	0xD1,			/* N */
	0xD5,			/* O */
	0,			/* P */
	0,			/* Q */
	0,			/* R */
	0,			/* S */
	0,			/* T */
	0,			/* U */
	0,			/* V */
	0,			/* W */
	0,			/* X */
	0,			/* Y */
0, /* Z */ };

unsigned char circumdig[] = {	/* accent like ^ */
	0xC2,			/* A */
	0,			/* B */
	0,			/* C */
	0,			/* D */
	0xCA,			/* E */
	0,			/* F */
	0,			/* G */
	0,			/* H */
	0xCE,			/* I */
	0,			/* J */
	0,			/* K */
	0,			/* L */
	0,			/* M */
	0,			/* N */
	0xD4,			/* O */
	0,			/* P */
	0,			/* Q */
	0,			/* R */
	0,			/* S */
	0,			/* T */
	0xDB,			/* U */
	0,			/* V */
	0,			/* W */
	0,			/* X */
	0,			/* Y */
0, /* Z */ };
