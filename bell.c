#include <sys/file.h>
#ifdef HANDLE_BELL
#include <machine/iteioctl.h>
#endif
#include <unistd.h>

#include "bell.h"

#include "config.h"

static char rcsid[] = "$Id: bell.c,v 1.3 1996/05/24 15:13:07 vuori Exp $";

int     itefd = -1;

void 
bell_init(int signo)
{
	if (itefd == -1)
		itefd = open("/dev/ttye0", O_RDONLY);
}

void 
bell_shutup(int signo)
{
	close(itefd);
	itefd = -1;
}

void 
beep(void)
{
#ifdef HANDLE_BELL
	ioctl(itefd, ITEIOC_RINGBELL);
#endif
}
