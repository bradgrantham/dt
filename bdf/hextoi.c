#include "proto.h"

#include <ctype.h>

static char rcsid[] = "$Id: hextoi.c,v 1.2 1995/07/01 18:29:22 vuori Exp $";

int hextoi( char *hex )
{
	int number = 0, pos = strlen(hex), mult = 1;

	for( ; pos+1; pos-- )
	{
	 if( (hex[pos] >= 'a') && (hex[pos] <= 'f') ) hex[pos] = toupper( hex[pos] );
	 if( ((hex[pos] >= '0') && (hex[pos] <= '9')) )
	  { number += (hex[pos]-'0') * mult; mult *= 16; }
	 if( ((hex[pos] >= 'A') && (hex[pos] <= 'F')) )
	  { number += (hex[pos]-'7') * mult; mult *= 16; }
	}

	return number;
}
