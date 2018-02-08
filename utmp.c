#include <sys/file.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "config.h"
#include "utmp.h"
#include "main.h"
#include "vt.h"

#define DEL_ALL			/* delete all matching entries, not just the
				 * first */

static char rcsid[] = "$Id: utmp.c,v 1.3 1996/05/24 15:13:44 vuori Exp $";

int     utmpfd = -1;

char   *
bname(char *str)
{
	int     len = strlen(str);

	while (len--)
		if (str[len] == '/')
			return str + len + 1;

	return str;
}

int 
utmp_addentry(int fd, char *host)
{
#ifdef HANDLE_UTMP
	char   *login = getlogin(), *tty;
	struct utmp entry;

	if (!login || utmpfd < 0)
		return 0;

	if ((tty = ttyname(fd)) == NULL)
		return 0;
	tty = bname(tty);

	memset((void *) &entry, 0, sizeof(entry));

	strncpy(entry.ut_line, tty, UT_LINESIZE);
	strncpy(entry.ut_name, login, UT_NAMESIZE);
	strncpy(entry.ut_host, host, UT_HOSTSIZE);
	entry.ut_time = time(0);

	if (flock(utmpfd, LOCK_EX | LOCK_NB) < 0)
		return 0;
	lseek(utmpfd, 0, SEEK_END);
	write(utmpfd, (void *) &entry, sizeof(entry));
	flock(utmpfd, LOCK_UN);
#endif

	return 1;
}

int 
utmp_delentry(int fd)
{
#ifdef HANDLE_UTMP
	char   *login = getlogin(), *tty;
	struct utmp entry;
	char    found = 0;
	short   entries = 0;

	if (!login || utmpfd < 0)
		return 0;

	if ((tty = ttyname(fd)) == NULL)
		return 0;
	tty = bname(tty);
	if (tty[0] == 'p')
		tty[0] = 't';

	if (flock(utmpfd, LOCK_EX | LOCK_NB) < 0)
		return 0;
	lseek(utmpfd, 0, SEEK_SET);
	while (read(utmpfd, &entry, sizeof(entry)) == sizeof(entry)) {
		if (strcmp(tty, entry.ut_line) == 0 && strcmp(login, entry.ut_name) == 0) {
			lseek(utmpfd, (long) -(sizeof(entry)), SEEK_CUR);
			memset((void *) &entry, 0, sizeof(entry));
			write(utmpfd, (void *) &entry, sizeof(entry));
#ifndef DEL_ALL
			return 1;
			flock(utmpfd, LOCK_UN);
#else
			found++;
#endif				/* DEL_ALL */
		}
		entries++;
	}

	flock(utmpfd, LOCK_UN);

	return found;
#else
        return 0;
#endif
}

int 
utmp_init(char *file)
{
#ifdef HANDLE_UTMP
	utmpfd = open(file, O_RDWR);

	setegid(getgid());

	return utmpfd;
#else
        return -1;
#endif
}

void 
utmp_exit()
{
#ifdef HANDLE_UTMP
	int     vtl = 0;

	for (vtl = 0; vtl < VT_MAXVT; vtl++)
		if (vt_to_pty[vtl] != -1)
			utmp_delentry(vt_to_pty[vtl]);
#endif
}
