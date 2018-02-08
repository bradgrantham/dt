
/*
 * "desktop" program main module
 *
 * Lawrence Kesteloot
 * June 26th, 1994
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/tty.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#ifdef HANDLE_ADB
#include <machine/adbsys.h>
#endif
#include <sys/wait.h>

#include "config.h"

#include "main.h"
#include "vt.h"
#include "event.h"
#include "grf.h"
#include "mux.h"
#include "func.h"
#include "bell.h"

#include "utmp.h"

static char rcsid[] = "$Id: main.c,v 1.10 1998/03/20 14:20:58 vuori Exp $";

/*
 * This is the number of bytes we get from the pty's at a time.  Keep
 * this small so that we can poll the keyboard regularly.
 */

#define BUFSIZE		128

#define SHELLBUFFSIZE	256

int     vt_to_pty[VT_MAXVT], pty_to_vt[64];
static int numtty, shells, scrn;
static char *fontname;
static char *shell, shellbuff[SHELLBUFFSIZE];
static fd_set initfd;

void 
usage(void)
{
	fprintf(stderr,
	    "usage:  dt [-n numvt] [-f font] [-s scrn]\n"
	    "    -n numvt	Number of virtual terminals\n"
	    "    -f font	Name of the font to use.  This can currently be \"large\"\n"
	    "		or \"small\".  It defaults to the best font for the screen\n"
	    "		size.\n"
	    "    -s scrn	Screen number to use virtual terminal on.  e.g. 1 to /dev/grf1\n");
	exit(1);
}

static void 
main_init(int argc, char *argv[])
{
	int     i, c;
	extern char *optarg;
	extern int optind;

	numtty = DEFAULT_NUMVT;
	fontname = "default";

	while ((c = getopt(argc, argv, "n:f:s:")) != -1) {
		switch (c) {
		case 'n':
			numtty = atoi(optarg);
			if (numtty <= 0 || numtty >= VT_MAXVT) {
				usage();
			}
			break;
		case 'f':
			fontname = optarg;
			break;
		case 's':
			scrn = atoi(optarg);
			if (scrn >= MAXGRF) {
				usage();
			}
			break;
		case '?':
			usage();
			break;
		}
	}

	argc -= optind;
	argv += optind;

	for (i = 0; i < VT_MAXVT; i++) {
		vt_to_pty[i] = -1;
		pty_to_vt[i] = -1;
	}

	shells = 0;
}

static void 
runshell(int vtnum, char *shell, char ttych, int ttynum)
{
	int     f;
	char    devstr[128], *shellname;
#ifdef LOGIN_SHELL
	char    dashname[64];
#endif				/* LOGIN_SHELL */
	struct termios termmode;

	sprintf(devstr, "/dev/tty%c%x", ttych, ttynum);

	f = open(devstr, O_RDWR);

	if (f == -1) {
		perror(devstr);
		return;
	}
	/*
	 * Create a new session
	 */

	setsid();

	/*
	 * Make it controlling terminal
	 */

	if (ioctl(f, TIOCSCTTY, 0) != 0) {
		perror("TIOCSCTTY");
		return;
	}
	dup2(f, 0);
	dup2(f, 1);
	dup2(f, 2);

	sprintf(devstr, "%s:%d", UTMP_HOST, vtnum + 1);
	utmp_addentry(f, devstr);

	tcgetattr(f, &termmode);/* don't strip 8th bit */
	termmode.c_iflag &= ~ISTRIP;
	tcsetattr(f, TCSANOW, &termmode);

	close(f);

	if (vtnum == 0) {
		printf("Desktop -- Press Command-1 to Command-%d to switch "
		    "virtual consoles\n", numtty);
	}
	printf("Virtual console number %d\n\n", vtnum + 1);

	shellname = strrchr(shell, '/');
	if (shellname == NULL) {
		shellname = shell;
	} else {
		shellname++;
	}
#ifdef LOGIN_SHELL
	strcpy(dashname, "-");
	strcat(dashname, shellname);

	execl(shell, dashname, NULL);
#else
	execl(shell, shellname, NULL);
#endif				/* LOGIN_SHELL */

	perror(shell);

	return;
}

void 
main_keyhit(int vtnum, u_char ch)
{
	int     pty;

	if (vtnum < 0 || vtnum >= VT_MAXVT) {
		return;
	}
	pty = vt_to_pty[vtnum];
	if (pty == -1) {
		return;
	}
	write(pty, &ch, sizeof(ch));
}

void 
main_resize(int vtnum, int rows, int cols)
{
	struct winsize ws;
	int     pty;

	if (vtnum < 0 || vtnum >= VT_MAXVT) {
		return;
	}
	pty = vt_to_pty[vtnum];
	if (pty == -1) {
		return;
	}
	ioctl(pty, TIOCGWINSZ, &ws);
	ws.ws_row = rows;
	ws.ws_col = cols;
	ioctl(pty, TIOCSWINSZ, &ws);
}

#ifdef HANDLE_ADB
int     adb;			/* make the fd global */
#endif

void 
main_newvt(void)
{
	int     f, i, ri, first, child;
	char    ttystr[128], ttylet = 'p';
	struct termios termmode;

	for (first = 0; first < VT_MAXVT; first++) {
		if (vt_to_pty[first] == -1) {
			break;
		}
	}

	if (first == VT_MAXVT) {
		return;
	}
	for (i = 0, ri = 0; i < 64; i++, ri++) {
		if (i == 16) {
			ri = 0;
			ttylet = 'q';
		} else
			if (i == 32) {
				ri = 0;
				ttylet = 'r';
			} else
				if (i == 48) {
					ri = 0;
					ttylet = 's';
				} else
					if (i == 64) {
						ri = 0;
						ttylet = 't';
					}
		sprintf(ttystr, "/dev/pty%c%x", ttylet, ri);
		f = open(ttystr, O_RDWR);
		if (f != -1) {
			break;
		}
	}

	if (i == 64) {
		return;
	}
	vt_to_pty[first] = f;
	pty_to_vt[f] = first;

	tcgetattr(f, &termmode);/* don't strip 8th bit */
	termmode.c_iflag &= ~ISTRIP;
	tcsetattr(f, TCSANOW, &termmode);

	child = fork();
	if (child == 0) {
#ifdef HANDLE_ADB
		close(adb);	/* we don't want to pass the fd to
				 * subprocesses */
#endif
		runshell(first, shell, ttylet, ri);
	}
	fcntl(f, F_SETFL, O_NDELAY);	/* use nonblocking io for the ptys to
					 * prevent deadlocks */
	FD_SET(f, &initfd);
	shells++;

	/*
	 * Must sleep a bit here to give the shell the time to start
	 * up.  If we don't, then the main_resize() will happen too
	 * soon and the tty won't get it.
	 */

	usleep(200000);

	mux_switchtovt(first);
}


void 
term_handle(int signo)
{
#ifdef HANDLE_ADB
	close(adb);
#endif
	grf_exit();
	exit(0);
}

int 
main(int argc, char *argv[])
{
	int     n, i, ccons;
#ifdef HANDLE_ADB
	adb_event_t event;
#endif
	fd_set  fd;
	char    buf[BUFSIZE];
	char   *login;
	struct passwd *passwd;

	main_init(argc, argv);

#ifdef HANDLE_ADB
	adb = open("/dev/adb", O_RDONLY);
	if (adb == -1) {
		perror("/dev/adb");
		switch (errno) {
		case EBUSY:
			fprintf(stderr, "Make sure X Windows or "
			    "another copy of \"desktop\" isn't "
			    "already running.\n");
			break;
		case ENOENT:
			fprintf(stderr, "The device /dev/adb "
			    "must be created in order to run "
			    "the desktop program.\n");
			fprintf(stderr, "Type the following command "
			    "as root:\n\n"
			    "\tmknod /dev/adb c 23 0\n");
			break;
		}
		exit(1);
	}
#endif
	shell = "/bin/sh";

	login = getlogin();
	if (login != NULL) {
		passwd = getpwnam(login);
		if (passwd != NULL) {
			strncpy(shellbuff, passwd->pw_shell, SHELLBUFFSIZE);
			shell = shellbuff;
			/* The passwd entry returned by getpwnam is static, and
			 * can be modified out from under us. Matters if we are
			 * one of many accounts w/ same user # (say
			 * toor and root) */
		}
	}
	FD_ZERO(&initfd);
#ifdef HANDLE_ADB
	FD_SET(adb, &initfd);
#endif

#ifdef UTMP
	utmp_init(_PATH_UTMP);	/* open the utmp file */
#endif				/* UTMP */

	for (i = 0; i < numtty; i++) {
		main_newvt();
	}

	grf_init(fontname, scrn);
	vt_init();
	mux_init();
	func_init(0);		/* initialize function key macros */
#ifdef BEEP
	bell_init(0);		/* open ite for beeping */
#endif				/* BEEP */

	/*
	 * Redirect console output to VT 1.
	 */

	ccons = 1;
	ioctl(vt_to_pty[0], TIOCCONS, &ccons);

	/*
	 * Ignore children's deaths -- we catch them at the select().
	 */

	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, term_handle);

	/* turn beeps on/off */
	signal(SIGUSR1, bell_init);
	signal(SIGUSR2, bell_shutup);

	signal(SIGTERM, term_handle);

	while (shells > 0) {
		fd = initfd;
		if (select(FD_SETSIZE, &fd, NULL, NULL, NULL) > 0) {
#ifdef HANDLE_ADB
			if (FD_ISSET(adb, &fd)) {
				read(adb, &event, sizeof(event));
				event_handle(&event);
			}
#endif
			for (i = 0; i < VT_MAXVT; i++) {
				if (vt_to_pty[i] != -1 &&
				    FD_ISSET(vt_to_pty[i], &fd)) {
					n = read(vt_to_pty[i], buf, BUFSIZE);
					if (n == 0) {
						/* Shell has exited */
						FD_CLR(vt_to_pty[i], &initfd);
						utmp_delentry(vt_to_pty[i]);
						waitpid(-1, NULL, WNOHANG);
						if (close(vt_to_pty[i]) == -1)
							perror("close");
						vt_to_pty[i] = -1;
						shells--;
					} else
						if (n == -1) {
							continue;
						} else {
							vt_putchar(i, (unsigned char *) buf, n);
						}
				}
			}
		}
	}

#ifdef HANDLE_ADB
	close(adb);
#endif
	grf_exit();
	utmp_exit();

	exit(0);
}
