/*
    Copyright 2013 Renwick James Hudspith

    This file (read_headers.c) is part of GLU.

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
   @file read_headers.c
   @brief strips the ensemble information such as dimensions and checksums
 */

#include "common.h"

#include "chklat_stuff.h"  // for the get_* routines
#include "GLU_bswap.h"     // endian swapping
#include "Scidac.h"        // for the Scidac and ILDG files

///////////////////// HIREP HEADER /////////////////////////////
static int
get_header_data_HIREP( FILE *__restrict CONFIG ,
		       struct head_data *__restrict HEAD_DATA ,
		       const GLU_bool VERB )
{
  HEAD_DATA -> config_type = OUTPUT_HIREP ;

  // so HiRep's output is ALWAYS big_endian and double precision
  HEAD_DATA -> precision = DOUBLE_PREC ;
  HEAD_DATA -> endianess = B_ENDIAN ;

  // read the first bit - Navigation and such
  int NAV[ ND + 1 ] ;
  
  if( !fread( NAV , ( ND + 1 ) * sizeof ( int ) , 1 , CONFIG ) ) {
    printf( "[IO] Cannot understand navigation details .. Leaving \n" ) ;
    return FAILURE ;
  }
  
  if ( !WORDS_BIGENDIAN ) { bswap_32( ND + 1 , NAV ) ; } 
  
  if( NAV[ 0 ] != NC ) {
    printf( "[IO] NC mismatch! We are compiled for NC = %d \n\
The file we are trying to read is NC = %d \nLeaving in abject disgust .. \n" , NC , NAV[ 0 ] ) ;
    return FAILURE ; 
  } else {
    if( VERB == GLU_TRUE ) {
      printf("[IO] NC :: %d \n" , NAV[0] ) ;
    }
  }

  // convert to my dimensions , they have the time dim first, I have it last
  int mu ;
  for( mu = 0 ; mu < ND - 1 ; mu++ ) {
    Latt.dims[ mu ] = NAV[ mu + 2 ] ;
  }
  Latt.dims[ mu ] = NAV[1] ;

  double plaq[1] ;
  if( !fread( plaq , sizeof ( double ) , 1 , CONFIG ) ) {
    printf( "[IO] Cannot understand plaquette details .. Leaving \n" ) ;
    return FAILURE ;
  }
  
  // we save this check to the end .. as a big finale!
  if ( !WORDS_BIGENDIAN ) {
    bswap_64( 1 , plaq ) ;
  } 

  HEAD_DATA -> plaquette = 0. ;
  HEAD_DATA -> checksum = 0. ;
  HEAD_DATA -> trace = 0. ;
  HEAD_DATA -> plaquette = plaq[0] ;

  return SUCCESS ;
}

//////////////////////// MILC ////////////////////////////////////////
// read the header information, probably only for version 5 and     //
// does not support the "NODE DUMP" output, only the natural order  //
//////////////////////////////////////////////////////////////////////
static int
get_header_data_MILC( FILE *__restrict in , 
		      struct head_data *HEAD_DATA ,
		      const GLU_bool VERB ) 
{
  GLU_bool need_swap = GLU_FALSE ;

  int magic[ 1 ] = { 0 } ;
  if( fread( magic , sizeof(int) , 1 , in ) != 1 ) return FAILURE ;

  // start off assuming we are not big endian and the file is
  if( !WORDS_BIGENDIAN ) { 
    HEAD_DATA -> endianess = L_ENDIAN ; 
  } else { 
    HEAD_DATA -> endianess = B_ENDIAN ; 
  }

  // check the magic number ....
  if( magic[0] != 20103 && magic[0] != 20104 ) {
    bswap_32( 1 , magic ) ;
    if( magic[0] != 20103 && magic[0] != 20104 ) {
      printf( "[IO] Magic %d not understood in either endianness .. leaving \n" , magic[0] ) ;
      return FAILURE ;
    }
    need_swap = GLU_TRUE ;
    // if we need a swap then the endianness of the config differs from ours
    if( !WORDS_BIGENDIAN ) { 
      HEAD_DATA -> endianess = B_ENDIAN ; 
    } else { 
      HEAD_DATA -> endianess = L_ENDIAN ; 
    }
  }
  
  // read in ND ints ... even though MILC only supports ND == 4 !!
  if( fread( Latt.dims , sizeof(int) , ND , in ) != ND ) return FAILURE ;
  if( need_swap ) { bswap_32( ND , Latt.dims ) ; }

  // date and time of creation is 64 bytes long ...
  char str[65] ;
  if( fread( str , sizeof(char) , 64 , in ) != 64 ) return FAILURE ;
  str[64] = '\0' ;
  if( VERB == GLU_TRUE ) {
    printf( "[IO] Config creation date :: %s \n" , str ) ;
  }

  // read in the output type, ripped from chroma ...
  int type[ 1 ] = { -1 } ;
  if( fread( type , sizeof(int) , 1 , in ) != 1 ) return FAILURE ;
  if( need_swap ) { bswap_32( 1 , type ) ; }
  if( type[0] != 0 ) {
    printf( "[IO] I only understand MILC's non-sitelist ... Leaving \n" ) ;
    return FAILURE ;
  }

  // read the two checksums
  uint32_t sum29[1] = {} , sum31[1] = {} ;
  if( fread( sum29 , sizeof(uint32_t) , 1 , in ) != 1 ) return FAILURE ;
  if( fread( sum31 , sizeof(uint32_t) , 1 , in ) != 1 ) return FAILURE ;
  if( need_swap ) { 
    bswap_32( 1 , sum29 ) ;
    bswap_32( 1 , sum31 ) ;
  }
  // they only use binary data checksums rather than plaq/link and checksum ...
  if( VERB == GLU_TRUE ) {
    printf( "[IO] Checksums %x %x :: %d \n" , sum29[0] , sum31[0] , magic[0] ) ;
  }

  HEAD_DATA -> config_type = OUTPUT_NCxNC ;
  if( magic[0] == 20103 ) {
    HEAD_DATA -> precision = FLOAT_PREC ;
  } else {
    HEAD_DATA -> precision = DOUBLE_PREC ;
  }
  HEAD_DATA -> checksum  = sum29[0] ; // computed in readers.c
  HEAD_DATA -> checksumb = sum31[0] ;

  // the file is at the position for being read ...
  return SUCCESS ;
}

// reads the NERSC header and sets up our data ...
static int 
get_header_data_NERSC( FILE *__restrict CONFIG ,
		       struct head_data *__restrict HEAD_DATA ,
		       const GLU_bool VERB )
{
  struct QCDheader *get_header( ) , * hdr ; 
  char *str ; 

  hdr = get_header( CONFIG ) ; 
 
  // Look for basic info, reporting if avalailable
  get_string( "ENSEMBLE_LABEL" , hdr , &str ) ; 
  if (str == NULL) { str = "(not specified)" ; }

  get_string( "ENSEMBLE_ID" , hdr , &str ) ; 
  if (str == NULL) { str = "(not specified)" ; }

  int i = get_int( "SEQUENCE_NUMBER" , hdr , &Latt.flow ) ; 
  // Should be able to rip out a Latt.flow-value from the config ... ? TODO //
  if ( i == FAILURE || Latt.flow == 0 ) {
    printf( "[IO] Unknown sequence number.... \n" ) ; 
  }

  if( VERB == GLU_TRUE ) {
    printf( "[IO] Configuration number :: %d \n" , Latt.flow ) ; 
  }

  /* Get dimensions */
  int mu ;
  for( mu = 0 ; mu < ND ; mu++ ) {
    char str[ 64 ] ;
    sprintf( str , "DIMENSION_%d" , mu + 1 ) ; 
    if ( get_int( str , hdr , Latt.dims+mu ) == FAILURE ) {
      printf( "[IO] DIMENSION_%d not present\n" , mu ) ; 
      return FAILURE ;
    }
  }
  
  // What precision and type of storage are we reading? Don't recognise we leave
  i = get_string( "FLOATING_POINT" , hdr , &str ) ; 
  if( i == FAILURE ) {
    printf( "[IO] FP precision not recognised in file ... leaving \n ") ; 
    return FAILURE ;
  }

  // size 1 is 64bit end 1 is big-endian default output for us is big-endian
  if( i == strcmp(  " IEEE64BIG" , str ) ) {
    HEAD_DATA -> precision = DOUBLE_PREC ; 
    HEAD_DATA -> endianess = B_ENDIAN ; 
  } else if( i == strcmp(  " IEEE32BIG" , str ) ) {
    HEAD_DATA -> precision = FLOAT_PREC ; 
    HEAD_DATA -> endianess = B_ENDIAN ; 
  } else if( i == strcmp(  " IEEE32" , str ) ) {
    HEAD_DATA -> precision = FLOAT_PREC ; 
    HEAD_DATA -> endianess = B_ENDIAN ; 
  } else if( i == strcmp(  " IEEE64LITTLE" , str ) ){      
    HEAD_DATA -> precision = DOUBLE_PREC ; 
    HEAD_DATA -> endianess = L_ENDIAN ; 
  } else if(  i == strcmp(  " IEEE32LITTLE" , str ) ){
    HEAD_DATA -> precision = FLOAT_PREC ; 
    HEAD_DATA -> endianess = L_ENDIAN ; 
  } else {
    printf( "[IO] PRECISION and ENDIANESS %s unknown .. Leaving " , str ) ;
    return FAILURE ;
  }

  // look for the datatype, nersc has a few options ...
  if( ( i = get_string( "DATATYPE",hdr,&str ) ) == FAILURE  ) {
    printf( "[IO] You must be joking! no DATATYPE read!! ... Leaving \n" ) ; 
    return FAILURE ;
  }
  //more string compares to locate the storage type
  char dat[64] , dat1[64] , dat2[64] ; 
  sprintf( dat," %dD_SU%d_GAUGE_SMALL" , ND , NC ) ; 
  sprintf( dat1," %dD_SU%d_GAUGE" , ND , NC ) ; 
  sprintf( dat2," %dD_SU%d_GAUGE_%dx%d" , ND , NC , NC , NC ) ; 
  if( strcmp( dat , str ) == SUCCESS ) {
    HEAD_DATA -> config_type = OUTPUT_SMALL ; 
  } else if( strcmp( dat1 , str) == SUCCESS ) {
    HEAD_DATA -> config_type = OUTPUT_GAUGE ; 
  } else if( strcmp( dat2 , str ) == SUCCESS ) {
    HEAD_DATA -> config_type = OUTPUT_NCxNC ;
  } else {
    printf( "[IO] Storage type %s unrecognised \n" , str ) ;
    printf( "[IO] Reminder :: we are compiled for %dD-SU(%d)\n" , ND, NC ) ;
    return FAILURE ;
  }

  // plop the trace, plaq and checksum out here
  HEAD_DATA -> checksum = 0 ;
  uint32_t check ;
  if( get_uint32_t( "CHECKSUM" , hdr , &check ) == FAILURE ) {
    printf("[IO] Checksum not found in header ... Leaving\n" ) ;
    return FAILURE ;
  }
  HEAD_DATA -> checksum = check ;

  HEAD_DATA -> plaquette = 0. ;
  float plaq ;
  if( get_float( "PLAQUETTE" , hdr , &plaq ) == FAILURE ) {
    printf("[IO] Plaquette not found in header ... Leaving\n" ) ;
    return FAILURE ;
  }
  HEAD_DATA -> plaquette = (double)plaq ;
 
  HEAD_DATA -> trace = 0. ;
  float tr ;
  if( get_float("LINK_TRACE" , hdr , &tr ) == FAILURE ) {
    printf("[IO] Trace not found in header ... Leaving\n" ) ;
    return FAILURE ;
  }
  HEAD_DATA -> trace = (double)tr ;

  for( i = 0 ; i < (*hdr).ntoken ; i++ ) {
    free( hdr -> token[ i ] ) ;
    free( hdr -> value[ i ] ) ;
  }
  free( hdr -> value ) ;
  free( hdr -> token ) ;
  free( hdr ) ;

  return SUCCESS ;
}

// switch for reading the header file ...
int
read_header( FILE *__restrict infile ,
	     struct head_data *__restrict HEAD_DATA ,
	     const GLU_bool VERB )
{
  // switch on the header type
  switch( Latt.head ) {
  case NERSC_HEADER :
    return get_header_data_NERSC( infile , HEAD_DATA , VERB ) ;
  case HIREP_HEADER :
    return get_header_data_HIREP( infile , HEAD_DATA , VERB ) ;
  case MILC_HEADER :
    return get_header_data_MILC( infile , HEAD_DATA , VERB ) ;
  case LIME_HEADER : // the one to use if you don't care what is at the end
  case SCIDAC_HEADER : // usual scidac header type
  case ILDG_SCIDAC_HEADER : // ILDG and SCIDAC are basically the same ...
  case ILDG_BQCD_HEADER : // ILDG and SCIDAC are basically the same ...
    return get_header_data_SCIDAC( infile , HEAD_DATA ) ; // in Scidac.c
  default :
    return FAILURE ;
  }
  return FAILURE ;
}
