/*
    Copyright 2013 Renwick James Hudspith

    This file (readers.c) is part of GLU.

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
   @file readers.c
   @brief binary data file reader supports #config_size outputs

   TODO :: should probably stop computing the CRCs if they aren't used
   @warning some of this is done in parallel using OpenMP
 */

#include "common.h"

#include "GLU_bswap.h"
#include "gramschmidt.h"
#include "crc32.h" // for the scidac circular checksum

// complete the full matrix
static inline void 
complete_NCxNC( O , uout )
     double complex *__restrict O ; 
     const double *__restrict uout ;
{
#if NC == 3
  O[0] = uout[0] + I * uout[1] ; 
  O[1] = uout[2] + I * uout[3] ; 
  O[2] = uout[4] + I * uout[5] ; 
  O[3] = uout[6] + I * uout[7] ; 
  O[4] = uout[8] + I * uout[9] ; 
  O[5] = uout[10] + I * uout[11] ; 
  O[6] = uout[12] + I * uout[13] ; 
  O[7] = uout[14] + I * uout[15] ; 
  O[8] = uout[16] + I * uout[17] ; 
#elif NC == 2
  O[0] = uout[0] + I * uout[1] ; 
  O[1] = uout[2] + I * uout[3] ; 
  O[2] = uout[4] + I * uout[5] ; 
  O[3] = uout[6] + I * uout[7] ; 
#else
  // implementation of this is simple
  int j ;
  for( j = 0 ; j < NCNC ; j++ ) {
    O[j] = uout[ 2 * j ] + I * uout[ 2 * j + 1 ] ;
  }
#endif
  return ;
}

// recombines O from the top NC-1 rows of matrix
static inline void 
complete_top( O , uout )
     double complex *__restrict O ; 
     const double *__restrict uout ; 
{
#if NC == 3
  O[0] = uout[0] + I * uout[1] ; 
  O[1] = uout[2] + I * uout[3] ; 
  O[2] = uout[4] + I * uout[5] ; 
  O[3] = uout[6] + I * uout[7] ; 
  O[4] = uout[8] + I * uout[9] ; 
  O[5] = uout[10] + I * uout[11] ; 
  O[6] = uout[2] * uout[10] - uout[3] * uout[11] - I * ( uout[2] * uout[11] + uout[3] * uout[10] ) ; 
  O[6] -= uout[4] * uout[8] - uout[5] * uout[9] - I * ( uout[4] * uout[9] + uout[5] * uout[8] ) ; 
  O[7] = uout[1] * uout[11] - uout[0] * uout[10] + I * ( uout[0] * uout[11] + uout[1] * uout[10] ) ; 
  O[7] -= uout[5] * uout[7] - uout[4] * uout[6] + I * ( uout[4] * uout[7] + uout[5] * uout[6] ) ; 
  O[8] = uout[0] * uout[8] - uout[1] * uout[9] - I * ( uout[0] * uout[9] + uout[1] * uout[8] ) ; 
  O[8] -= uout[2] * uout[6] - uout[3] * uout[7] - I * ( uout[2] * uout[7] + uout[3] * uout[6] ) ; 
#elif NC == 2
  O[0] = uout[0] + I * uout[1] ;
  O[1] = uout[2] + I * uout[3] ;
  O[2] = -conj( O[1] ) ;  
  O[3] = conj( O[0] ) ; 
#else
  // not implemented but shouldn't be problematic will do now
  int i ;
  for( i = 0 ; i < NCNC - NC ; i++ ) {
    O[i] = uout[ 2*i ] + I * uout[ 2*i + 1 ] ;
  }
  // and complete, taken from the gramschmidt code, should consider a minors function ?
  complex array[ ( NC - 1 ) * ( NC - 1 ) ] ;
  for( i = (NCNC-NC) ; i < NCNC ; i++ ) { // our bona-fide minor index
    int idx = 0 , j ;
    for( j = 0 ; j < ( NCNC - NC ) ; j++ ) {
      if( ( j%NC != i%NC ) ) { // remove columns and rows
	// pack array
	array[idx] = O[j] ;
	idx ++ ;
      } 
    }
    // compute the minors of the bottom row
    #if ( NC%2 == 0 )
    register const real mulfact = ( i % 2 == 0 ) ? -1.0 : 1.0 ; 
    #else
    register const real mulfact = ( i % 2 == 0 ) ? 1.0 : -1.0 ; 
    #endif
    O[i]= conj( mulfact * (complex)LU_det( NC-1 , array ) ) ;
  }
#endif
  return ;
}

// construct some common LOOP variables
static int
construct_loop_variables( LATT_LOOP , LOOP_VAR , type )
     int *LATT_LOOP , *LOOP_VAR ;
     const int type ;
{
  // dump it all in memory...
  switch( type ) {
  case OUTPUT_SMALL :
    *LATT_LOOP = ND * LOOP_SMALL * LVOLUME ;
    *LOOP_VAR = LOOP_SMALL ;
    break ;
  case OUTPUT_GAUGE : 
    *LATT_LOOP = ND * LOOP_GAUGE * LVOLUME ;
    *LOOP_VAR = LOOP_GAUGE ;
    break ;      
  case OUTPUT_NCxNC :
    *LATT_LOOP = ND * LOOP_NCxNC * LVOLUME ;
    *LOOP_VAR = LOOP_NCxNC ;
    break ;
  default :
    // actually should try and read an NCxNC config perhaps?
    printf( "[IO] Unrecognised input type .. Leaving in disgust\n" ) ;
    return FAILURE ;
  }
  return SUCCESS ;
}

// rebuilt using the switch for the different allowd formats
static inline void
rebuild_lat( link , utemp , type ) 
     double complex *__restrict link ;
     const double *__restrict utemp ;
     const GLU_output type ;
{
  // smash all the read values into lat
  switch( type ) { 
  case OUTPUT_GAUGE :
    complete_top( link , utemp ) ;
    return ;
  case OUTPUT_NCxNC :
    complete_NCxNC( link , utemp ) ;
    return ;
  default : return ;
  }  
  return ;
}

//////////// MEMSPENSIVE VERSION ///////////////
uint32_t
lattice_reader_suNC( struct site *__restrict lat , 
		     FILE *__restrict in , 
		     const struct head_data HEAD_DATA )
{
  // this is checked previously, nice to be certain though
  if( in == NULL ) {
    printf( "[IO] Error opening config file!!... Returning with error \n" ) ; 
    return FAILURE ; 
  }

  // loop variables
  int LOOP_VAR , LATT_LOOP ;
  if( construct_loop_variables( &LATT_LOOP , &LOOP_VAR , 
				HEAD_DATA.config_type ) == FAILURE ) {
    return FAILURE ;
  }

  static double *uind , *p ; 
  static float *uin , *q ; 
  uint32_t CRCsum29 = 0 , CRCsum31 = 0 , CRC_BQCD = 0 ;

  if( HEAD_DATA.precision == DOUBLE_PREC ) {
    uind = ( double* )malloc( LATT_LOOP * sizeof( double ) ) ; 
    if( fread( uind , sizeof( double ) , LATT_LOOP , in ) != LATT_LOOP ) {
      printf( "[IO] Configuration File reading failure .. Leaving \n" ) ;
      free( uind ) ;
      return FAILURE ;
    }
    // scidac checksum is on the RAW binary data, not the byteswapped
    if( Latt.head == ILDG_BQCD_HEADER || Latt.head == SCIDAC_HEADER ||
	Latt.head == ILDG_SCIDAC_HEADER ) {
      int i ;
      for( i = 0 ; i < LVOLUME ; i++ ) {
	DML_checksum_accum( &CRCsum29 , &CRCsum31 , 
			    i , (char*)( uind + ( i * ND * LOOP_VAR ) ) ,
			    sizeof(double) * ND * LOOP_VAR ) ;
	// BQCD's
	CKSUM_ADD( ( uind + ( i * ND * LOOP_VAR ) ) , 
		   sizeof(double) * ND * LOOP_VAR ) ;
      }
      // BQCD checksum is just the crc of the whole thing
      uint32_t nbytes ;
      CKSUM_GET( &CRC_BQCD , &nbytes ) ;
    }
    // and then we do the byte swap
    if( HEAD_DATA.endianess != WORDS_BIGENDIAN ) {
      bswap_64( LATT_LOOP , uind ) ;  
    }
    p = uind ; 
  } else {
    uin = ( float* )malloc( LATT_LOOP * sizeof( float ) ) ; 
    if( fread( uin , sizeof( float ) , LATT_LOOP  , in ) != LATT_LOOP ) {
      printf("[IO] Configuration File reading failure .. Leaving \n" ) ;
      free( uin ) ;
      return FAILURE ;
    }
    // BQCD checksum is just the crc of the whole thing and is kinda expensive
    if( Latt.head == ILDG_BQCD_HEADER || Latt.head == SCIDAC_HEADER ||
	Latt.head == ILDG_SCIDAC_HEADER ) {
      int i ;
      for( i = 0 ; i < LVOLUME ; i++ ) {
	DML_checksum_accum( &CRCsum29 , &CRCsum31 , 
			    i , (char*)( uin + ( i * ND * LOOP_VAR ) ) ,
			    sizeof(float) * ND * LOOP_VAR ) ;
	// BQCD's
	CKSUM_ADD( ( uin + ( i * ND * LOOP_VAR ) ) , 
		   sizeof(float) * ND * LOOP_VAR ) ;
      }
      uint32_t nbytes ;
      CKSUM_GET( &CRC_BQCD , &nbytes ) ;
    }
    // and byteswap if necessary
    if( HEAD_DATA.endianess != WORDS_BIGENDIAN ) {
      bswap_32( LATT_LOOP , uin ) ;
    }
    q = uin ;
  }

  // compute the crcs
  uint32_t k = 0 , sum29 = 0 , sum31 = 0 ;
  int i ;
  #pragma omp parallel for private(i) reduction(+:k) reduction(^:sum29) reduction(^:sum31)
  for( i = 0 ; i < LVOLUME ; i++ ) {
    // t is the AntiHermitian_projised idx
    int t = ND * LOOP_VAR * i ;
    int rank29 = t % 29 ;
    int rank31 = t % 31 ;
    register uint32_t k_loc = 0 ;
    register uint32_t sum29_loc = 0 , sum31_loc = 0 ;

    // general variables ...
    double utemp[ LOOP_VAR ] ;
    uint32_t res = 0 ;
    int mu , j , count = 0 ;
    for( mu = 0 ; mu < ND ; mu++ ) {
      for( j = 0 ; j < LOOP_VAR ; j++ ) {
	// should I shuffle this around ?
	if( HEAD_DATA.precision == DOUBLE_PREC ) {
	  // compute the checksum ...
	  uint32_t *buf = ( uint32_t* )( p + t ) ; 
	  res = *buf + *( buf + 1 ) ; 
	  // put value into temporary
	  *( utemp + j ) = ( double )*( p + t ) ;
	} else {
	  // nersc checksum ...
	  res = *( uint32_t *)( q + t ) ;
	  // and put the value in the temporary
	  *( utemp + j ) = ( double )*( q + t ) ; 
	}
	count++ ;

	// milc checksums ...
	sum29_loc ^= (uint32_t)( res << rank29 | res >> ( 32 - rank29 ) ) ;
	sum31_loc ^= (uint32_t)( res << rank31 | res >> ( 32 - rank31 ) ) ;

	/// and perform the mods
	rank29 = ( rank29 < 28 ) ? rank29 + 1 : 0 ;
	rank31 = ( rank31 < 30 ) ? rank31 + 1 : 0 ;

	// local sum
	k_loc += res ; 

	t++ ;
      }
      // smash all the read values into lat
      rebuild_lat( lat[i].O[mu] , utemp , HEAD_DATA.config_type ) ;
    }
    // reductions go here ...
    // nersc
    k = k + (uint32_t)k_loc ;

    // milc
    sum29 = sum29 ^ (uint32_t)sum29_loc ;
    sum31 = sum31 ^ (uint32_t)sum31_loc ;
  }

#ifdef DEBUG
  printf( "[IO] NERSC cksum   :: %x \n" , k ) ;
  printf( "[IO] MILC cksums   :: %x %x \n" , sum29 , sum31 ) ;
  printf( "[IO] SCIDAC cksums :: %x %x \n" , CRCsum29 , CRCsum31 ) ;
  printf( "[IO] BQCD cksum    :: %x \n" , CRC_BQCD ) ;
#endif

  if( HEAD_DATA.precision == DOUBLE_PREC ) {
    free( uind ) ; 
  } else {
    // reunitarise up to working precision
    #ifndef SINGLE_PREC
    latt_reunitU( lat ) ;
    #endif
    free( uin ) ;
  }

  // if we are reading a MILC file we output the sum29 checksum
  switch( Latt.head ) {
  case NERSC_HEADER : return k ;
  case MILC_HEADER : return sum29 ;
  case ILDG_SCIDAC_HEADER :
  case SCIDAC_HEADER : return CRCsum29 ;
  case ILDG_BQCD_HEADER : return CRC_BQCD ;
  case LIME_HEADER : return SUCCESS ;
  default : return FAILURE ; // should print an error here
  }
}

// MEMCHEAP READER
uint32_t
lattice_reader_suNC_cheaper( struct site *__restrict lat , 
			     FILE *__restrict in , 
			     const struct head_data HEAD_DATA )
{
  // this is checked previously, nice to be certain though
  if( in == NULL ) {
    printf( "[IO] Error opening config file!!... Returning with error \n" ) ; 
    return FAILURE ; 
  }

  // loop variables
  int LOOP_VAR , LATT_LOOP ;
  if( construct_loop_variables( &LATT_LOOP , &LOOP_VAR , 
				HEAD_DATA.config_type ) == FAILURE ) {
    return FAILURE ;
  }

  static double *uind , *p ; 
  static float *uin , *q ; 

  if( HEAD_DATA.precision == DOUBLE_PREC ) {
    uind = ( double* )malloc( ND*LOOP_VAR * sizeof( double ) ) ; 
  } else {
    uin = ( float* )malloc( ND*LOOP_VAR * sizeof( float ) ) ; 
  }

  uint32_t k = 0 , sum29 = 0 , sum31 = 0 ; // NERSC & MILC checksums ...
  uint32_t CRCsum29 = 0 , CRCsum31 = 0 ;
  int i ;
  // do this in serial reads in site by site
  for( i = 0 ; i < LVOLUME ; i++ ) {

    // read in a site, could we not use fseek to allow this to be done in parallel?
    if( HEAD_DATA.precision == DOUBLE_PREC ) {
      if( fread( uind , sizeof( double ) , ND*LOOP_VAR , in ) != ND*LOOP_VAR ) {
	printf("[IO] Configuration File reading failure .. Leaving \n" ) ;
	free( uind ) ;
	return FAILURE ;
      }
      DML_checksum_accum( &CRCsum29 , &CRCsum31 , 
			  i , (char*)( uind ) ,
			  sizeof(double) * ND * LOOP_VAR ) ;
      // BQCD's
      CKSUM_ADD( uind , sizeof(double) * ND * LOOP_VAR ) ;
      if( HEAD_DATA.endianess != WORDS_BIGENDIAN ) {
	bswap_64( ND*LOOP_VAR , uind ) ;  
      }
      p = uind ;
    } else { 
      if( fread( uin , sizeof( float ) , ND*LOOP_VAR , in ) != ND*LOOP_VAR ) {
	printf("[IO] Configuration File reading failure .. Leaving \n" ) ;
	free( uin ) ;
	return FAILURE ;
      }
      DML_checksum_accum( &CRCsum29 , &CRCsum31 , 
			  i , (char*)( uin ) ,
			  sizeof(float) * ND * LOOP_VAR ) ;
      // BQCD's
      CKSUM_ADD( uin , sizeof(float) * ND * LOOP_VAR ) ;
      if( HEAD_DATA.endianess != WORDS_BIGENDIAN ) {
	bswap_32( ND*LOOP_VAR , uin ) ;
      }
      q = uin ;
    }
    int t = 0 ;

    register uint32_t k_loc = 0 , sum29_loc = 0 , sum31_loc = 0 ;
    int rank29 = ( ND * LOOP_VAR * i ) % 29 ;
    int rank31 = ( ND * LOOP_VAR * i ) % 31 ;

    // general variables ...
    double utemp[ LOOP_VAR ] ;
    uint32_t res = 0 ;
    int mu , j ;
    for( mu = 0 ; mu < ND ; mu++ ) {
      for( j = 0 ; j < LOOP_VAR ; j++ ) {
	// should I shuffle this around ?
	if( HEAD_DATA.precision == DOUBLE_PREC ) {
	  // compute the checksum ...
	  uint32_t *buf = ( uint32_t* )( p + t ) ; 
	  res = *buf + *( buf + 1 ) ; 
	  // put value into temporary
	  *( utemp + j ) = ( double )*( p + t ) ;
	} else {
	  // nersc checksum ...
	  res = *( uint32_t *)( q + t ) ;
	  // and put the value in the temporary
	  *( utemp + j ) = ( double )*( q + t ) ; 
	} 
	// local computations
	sum29_loc ^= (uint32_t)( res << rank29 | res >> ( 32 - rank29 ) ) ;
	sum31_loc ^= (uint32_t)( res << rank31 | res >> ( 32 - rank31 ) ) ;

	/// and perform the mods
	rank29 = ( rank29 < 28 ) ? rank29 + 1 : 0 ;
	rank31 = ( rank31 < 30 ) ? rank31 + 1 : 0 ;

	// local sum
	k_loc += res ; 

	t++ ;
      }
      // smash all the read values into lat
      rebuild_lat( lat[i].O[mu] , utemp , HEAD_DATA.config_type ) ;
    }
    // nersc
    k = k + (uint32_t)k_loc ;

    // milc
    sum29 = sum29 ^ (uint32_t)sum29_loc ;
    sum31 = sum31 ^ (uint32_t)sum31_loc ;
  }

  // BQCD checksum is just the crc of the whol thing
  uint32_t CRC_BQCD , nbytes ;
  CKSUM_GET( &CRC_BQCD , &nbytes ) ;

#ifdef DEBUG
  printf( "[IO] NERSC cksum   :: %x \n" , k ) ;
  printf( "[IO] MILC cksums   :: %x %x \n" , sum29 , sum31 ) ;
  printf( "[IO] SCIDAC cksums :: %x %x \n" , CRCsum29 , CRCsum31 ) ;
  printf( "[IO] BQCD cksum    :: %x \n" , CRC_BQCD ) ;
#endif

  if( HEAD_DATA.precision == DOUBLE_PREC ) {
    free( uind ) ; 
  } else {
    // reunitarise up to working precision
    #ifndef SINGLE_PREC
    latt_reunitU( lat ) ;
    #endif
    free( uin ) ;
  }

  // if we are reading a MILC file we output the sum29 checksum
  switch( Latt.head ) {
  case NERSC_HEADER : return k ;
  case MILC_HEADER : return sum29 ;
  case ILDG_SCIDAC_HEADER :
  case SCIDAC_HEADER : return CRCsum29 ;
  case ILDG_BQCD_HEADER : return CRC_BQCD ;
  case LIME_HEADER : return SUCCESS ;
  default : return FAILURE ; // should print an error here
  }
}

// clean this up for scope
#ifdef DEBUG
  #undef DEBUG
#endif
