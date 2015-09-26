#include <stdio.h>

main()
{
	int i;

	for( i = 1; i < 256; i++ )
	{
	 printf( "%d (%x): ", i, i );
	 if( iscntrl(i) ) printf( "^%c", i+64 ); else printf( "%c", i );
	 if( i&1 ) printf( "\n" );
	}
}
