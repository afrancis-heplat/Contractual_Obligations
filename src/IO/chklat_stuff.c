/*
    Copyright 2013 Renwick James Hudspith

    This file (chklat_stuff.c) is part of GLU.

    GLU is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    GLU is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with GLU.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
   @file chklat_stuff.c
   @brief some functions I ripped out of chklat.c plus some stuff I didn't

   @warning Byte swaps assume IEEE754 floating point representation so that
   floats are 32 bit
 */

#include "common.h"

// get_string: find a token and return its value as a string 
int 
get_string( char *s , 
	    struct QCDheader *hdr ,
	    char **q )
{     
  /* find a token and return the value */
  int i ;
  for( i = 0 ; i < (*hdr).ntoken ; i++ ) {
    if( strcmp ( s , (*hdr).token[i] ) == 0 ) {
      *q = (*hdr).value[i] ;
      return SUCCESS ;
    }
  }
  *q = NULL ;
  return FAILURE ;
}

// get_float: return value as a float  
int 
get_float( char *s , 
	   struct QCDheader *hdr ,
	   float *q )
{
  char *p ;
  get_string( s , hdr , &p ) ;
  if( p == NULL ) {
    return ( FAILURE ) ;
  }
  sscanf( p , "%f" , q ) ;
  return SUCCESS ;
}

// get_int: return value as an int  
int 
get_int( char *s , 
	 struct QCDheader *hdr ,
	 int *q ) 
{
  char *p ;
  get_string( s , hdr , &p ) ;
  if( p == NULL ) {
    return ( FAILURE ) ;
  }
  sscanf( p , "%d" , q ) ;
  return SUCCESS ;
}

// get_header: read from stream, parsing tokens 
struct QCDheader * 
get_header( FILE *__restrict in )
{
  char line[MAX_LINE_LENGTH] ;
  int n , len ;
  struct QCDheader *hdr ;
  char **tokens , **values ;
  char *p , *q ;

  // Begin reading, and check for "BEGIN_HEADER" token 
  if( fgets( line , MAX_LINE_LENGTH , in ) == NULL ) {
    printf( "Header reading failed ... Leaving\n" ) ;
    exit( 1 ) ;
  }
  if( strcmp( line , "BEGIN_HEADER\n" ) != 0 ) {
    printf( "Missing \"BEGIN_HEADER\"; Is this a NERSC config?\n" ) ;
    exit( 1 ) ;
  }

  // Allocate space for QCDheader and its pointers 
  tokens = ( char ** ) malloc( MAX_TOKENS * sizeof( char * ) );
  values = ( char ** ) malloc( MAX_TOKENS * sizeof( char * ) );
  hdr = ( struct QCDheader * ) malloc( sizeof ( struct QCDheader ) ) ;
  (*hdr).token = tokens ;
  (*hdr).value = values ;

  // Begin loop on tokens 
  n = 0 ;
  while (1) {

    if( fgets( line , MAX_LINE_LENGTH , in ) == NULL ) {
      printf( "Header reading failed ... Leaving \n" ) ;
      exit( 1 ) ; 
    }
    if( strcmp ( line , "END_HEADER\n" ) == 0 ) {
      break;
    }

    /* Tokens are terminated by a space */
    q = index( line,' ' ) ;

    /* Overwrite space with a terminating null */
    *q = '\0';
    len = q - line ;

    /* allocate space and copy the token in to it */
    p = malloc( len+1 ) ;
    (*hdr).token[n] = p ;
    strcpy( p , line ) ;

    q = index(++q,'=') ;
    q++ ;

    len = strlen( q ) ;
    q[len-1] = 0;
    p = malloc( len ) ;
    (*hdr).value[n] = p ;
    strcpy( p , q ) ;
    n++ ;
  }
  (*hdr).ntoken = n ;
  return (hdr) ;
}

// get_uint32_t: return value as a uint32_t 
int 
get_uint32_t( char *s , 
	      struct QCDheader *hdr ,
	      uint32_t *q )
{
  char *p ;
  get_string( s , hdr , &p ) ;
  if( p == NULL ) { return FAILURE ; }

#ifdef INTS_ARE_32BIT
  sscanf( p , "%x" , q ) ;
#else
  int j ;
  sscanf( p , "%x" , &j ) ;
  *q = (uint32_t) j ;
#endif
  return SUCCESS;
}

// skips the header file because sometimes I don't care what's in it 
int 
skip_hdr( FILE *__restrict file )
{
  char line[MAX_LINE_LENGTH] ;
  // eww 
  while( GLU_TRUE ) {
    // if there is an error we leave
    if( fgets( line , MAX_LINE_LENGTH , file ) == NULL ) {
      printf( "[IO] Skip header failure ... Leaving \n" ) ;  
      return FAILURE ;
    } else if ( strcmp ( line , "END_HEADER\n" ) == SUCCESS ) {
      return SUCCESS ;
    }
  }
  return FAILURE ;
}
