# $Id: Makefile,v 1.10 1996/11/24 11:41:26 vuori Exp $

CFLAGS	=	-g -Wall -Wno-unused
#CFLAGS	=	-O3 -Wall -Wno-unused
SRCS	=	main.c vt.c grf.c blit.c event.c kbd.c mouse.c mux.c func.c utmp.c bell.c vers.c
LIBS	=
OBJS	=	main.o vt.o grf.o blit.o event.o kbd.o mouse.o mux.o func.o utmp.o bell.o vers.o
DT	=	dt
# where to install the executable
BINDIR	=	/usr/local/bin

all:		$(DT)

$(DT):		vers.h $(OBJS)
		$(CC) $(CFLAGS) $(OBJS) -o $@ $(LIBS)

vers.h:		$(SRCS)
		rm -f vers.h vers.o
		./version.sh > vers.h

depend:		vers.h
		mkdep $(SRCS)

clean:
		rm -f $(OBJS) $(DT) vers.h compile

install:	$(DT)
		install -c -m 2555 -o bin -g utmp $(DT) $(BINDIR)
