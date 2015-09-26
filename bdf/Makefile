# $Id: Makefile,v 1.2 1995/07/01 18:29:14 vuori Exp $

CFLAGS	=	-O -Wall -DXFRM=1
SRCS	=	bdftoasc.c hextoi.c
LIBS	=
OBJS	=	bdftoasc.o hextoi.o
PROGRAM	=	bdftoasc

all:		$(PROGRAM)

$(PROGRAM):	$(OBJS)
		$(CC) $(CFLAGS) $(OBJS) -o $@ $(LIBS)

depend:
		mkdep $(SRCS)

proto:
		mkproto $(SRCS) > proto.h

clean:
		rm -f $(OBJS) $(PROGRAM)
