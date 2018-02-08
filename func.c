/* Handles function key macros */

#include "config.h"

#include <sys/types.h>
#include <pwd.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "func.h"
#include "main.h"

#define NUMFUNCS 12		/* how many function keys are used for macros */

static char rcsid[] = "$Id: func.c,v 1.3 1996/05/24 15:13:18 vuori Exp $";

extern int mux_curvt;

char   *func_macros[NUMFUNCS];

int 
func_dokey(int key)
{
	short   indx;

	if (!(func_macros[key]) || key >= NUMFUNCS)
		return -1;

	for (indx = 0; func_macros[key][indx]; indx++)
		main_keyhit(mux_curvt, func_macros[key][indx]);

	return 1;
}

int 
func_load(char *file)
{
	FILE   *funcrc;
	char    line[1025];
	short   len;
	short   fkey;

	if (!(funcrc = fopen(file, "r")))
		return -1;

	for (fkey = 0;;) {
		if (!(fgets(line, 1024, funcrc)))
			break;
		len = strlen(line);
		if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
			continue;
		if (fkey >= NUMFUNCS)
			break;

		killnl(line);
		func_macros[fkey] = (char *) malloc(len + 1);
		strncpy(func_macros[fkey], line, 1024);
		fkey++;
	}

	fclose(funcrc);
	return 1;
}

int 
func_init(char reload)
{
	struct passwd *weed;
	char    rcfile[_POSIX_PATH_MAX];
	short   key;

	if (reload) {
		for (key = 0; key < NUMFUNCS; key++)
			if (func_macros[key])
				free(func_macros[key]);
	}
	memset(func_macros, 0, NUMFUNCS * sizeof(char *));

	weed = getpwuid(getuid());
	sprintf(rcfile, "%s/.dtrc", weed->pw_dir);

	return (func_load(rcfile));
}

void 
killnl(char *str)
{
	while (*str) {
		if (*str == '\n' || *str == '\r')
			*str = 0;
		str++;
	}
}
