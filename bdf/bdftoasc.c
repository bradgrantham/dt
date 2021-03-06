/* convert a bdf file to a C header file
 * syntax: bdftoasc bdf-file h-file
 *  Valtteri Vuorikoski vuori@sci.fi 1995 */

#include "proto.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <stdlib.h>

#undef DEBUG

static char rcsid[] = "$Id: bdftoasc.c,v 1.2 1995/07/01 18:29:18 vuori Exp $";

char *pname;
FILE *src, *dst;
int height;

int main( int argc, char **argv )
{
	char *p;
	int fonts;

	fprintf( stderr, "leetin bdf-to-asc converter Valtteri Vuorikoski 1995\n" );

	if( argc != 3 )
	{
		fprintf( stderr, "not enough arguments, syntax bdftoasc src dst\n" );
		exit(1);
	}

	pname = argv[0];

	if( (src = fopen( argv[1], "r" )) == NULL )
	{
		fprintf( stderr, "couldn't open source file %s\n", argv[1] );
		exit(1);
	}

	if( (dst = fopen( argv[2], "w" )) == NULL )
	{
		fprintf( stderr, "couldn't open destination file %s\n", argv[2] );
		exit(1);
	}

	fprintf( dst, "/* This file was generated by bdftoasc, a BDF to C header
 * file converter by Valtteri Vuorikoski, from the file %s.
 * The command was: %s %s %s */\n\n", argv[1], pname, argv[1], argv[2] );

	GetComment();

	p = argv[1];
	fprintf( dst, "static unsigned char Font%s[] = {\n", strsep( &p, "." ) );

	if( !GetStarted() )
	{
		fprintf( stderr, "couldn't find start of the font. Is this a valid bdf ?\n" );
		exit(1);
	}

	if( (fonts = GetFonts()) < 1 )
	{
		fprintf( stderr, "couldn't find any fonts in the file, werd\n" );
		exit(1);
	} else {
		fprintf( stderr, "\nfound and wrote %d non-empty fonts\n", fonts );
		CloseUp();
		exit(0);
	}

/* NOTREACHED */
	exit(0);
}

void GetComment( void )
{
	char buf[256];

#ifdef DEBUG
	fprintf( stderr, "getting comments from font file\n" );
#endif

	fprintf( dst, "/* Comments from the font file:\n" );

	while( fgets( buf, 255, src ) )
	{
		if( strncmp( buf, "COMMENT", 7 ) == 0 )
		 { fprintf( dst, "%s", buf+8 ); continue; }
		if( strncmp( buf, "FONT ", 5 ) == 0 )
		 fprintf( dst, "\nThis font is: %s\n", buf+5 );
	}

	fprintf( dst, "*/\n\n" );
	rewind( src );
}

int GetStarted( void )
{
	char buf[256];

	while( fgets( buf, 255, src ) )
	{
	 if( strncmp( buf, "STARTFONT", 9 ) == 0 )
	 {
#ifdef DEBUG
	  fprintf( stderr, "found font start, version %s", buf+10 );
#endif /* DEBUG */
	  return( GetSize() );
	 }
	}

	return 0;
}

int GetSize( void )
{
	char buf[256];
	char *siz;

	while( fgets( buf, 255, src ) )
	{
	 if( strncmp( buf, "SIZE", 4 ) == 0 )
	 {
	  siz = buf+5;
	  height = atoi( strsep( &siz, " " ) );
#ifdef DEBUG
	  fprintf( stderr, "got font height int = %d\n", height );
#endif /* DEBUG */
	  return 1;
	 }
	}

	return 0;
}

int GetFonts( void )
{
	char buf[256];
	int num = 0;
	int amt = 1;

	while( fgets( buf, 255, src ) )
	{
		if( strncmp( buf, "STARTCHAR", 9 ) == 0 )
		{
		 GetEncoding( &num );
		 NewFont( &num, buf+10 );
		 amt++;
		}
	}

	return amt;
}

void GetEncoding( int *num )
{
	char buf[256];
	int enc;

	while( fgets( buf, 255, src ) )
	{
		if( strncmp( buf, "ENCODING", 8 ) == 0 )
		{
		 enc = atoi( buf+9 );
#ifdef DEBUG
		 fprintf( stderr, "got font encoding %d,", enc );
#endif /* DEBUG */
		 if( (enc - *num) > 1 ) { MakeFonts( (enc - *num), *num ); *num = enc; }
		 return;
		}
	}
}

void MakeFonts( int amo, int num )
{
	int htleft;

#ifdef DEBUG
	fprintf( stderr, "making %d empty fonts\n", amo );
#endif /* DEBUG */

	for( ; amo; amo--, num++ )
	{
	 fprintf( dst, "\n  0x00,		/* empty font - char %d (0x%02x) */\n", num, num );
	 for( htleft = height; htleft-1; htleft-- )
	  fprintf( dst, "  0x00,\n" );
	}

	fprintf( dst, "\n" );
}

void NewFont( int *num, char *name )
{
	char buf[256];
	unsigned char number;

#ifdef DEBUG
	fprintf( stderr, "num %d\r", *num );
#else
	fprintf( stderr, "getting font %d\r", *num );
#endif /* DEBUG */

	KillNl( name );
	fprintf( dst, "		/* char %d (0x%02x), name %s */\n", *num, *num, name );

	while( 1 ) 
	{
	 if( !fgets( buf, 255, src ))
	 { fprintf( stderr, "unexpected eof: no bitmaps found\n" ); exit(1); }
	 if( strncmp( buf, "BITMAP", 6 ) == 0 )
	  break;
	}

	while( 1 )
	{
	 if( !fgets( buf, 255, src ) )
	 { fprintf( stderr, "unexpected eof: no end of bitmap found\n" ); exit(1); }
	 if( strncmp( buf, "ENDCHAR", 7 ) != 0 )
	 { number = (unsigned char) hextoi( buf ); 
#ifdef XFRM /* shift the font two to the left or it'll be aligned to the right, which is not what we want */
	   number >>= XFRM;
#endif /* XFRM */
	   fprintf( dst, "  0x%02X,", number );
	   DrawLine( number ); }
	 else
	  break;
	}

	fprintf( dst, "\n" );

	(*num)++;
}

void CloseUp()
{
	fprintf( dst, "};
/* That's all, folks */\n" );

	fclose( src ); fclose( dst );
}

void KillNl( char *blah )
{
	while( *blah )
	{
	 if( *blah == '\n' )  *blah = 0;
	 blah++;
	}
}

void DrawLine( unsigned char number )
{
	int ctr = sizeof(unsigned char)*8;

	fprintf( dst, "		/* " );

	for( ; ctr; ctr-- )
	 fprintf( dst, "%c", ((number>>ctr)&1?'#':'.') );

	fprintf( dst, " */\n" );
}

