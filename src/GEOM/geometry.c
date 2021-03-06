/*
    Copyright 2013 Renwick James Hudspith

    This file (geometry.c) is part of GLU.

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
   @file geometry.c
   @brief lattice geometry functions for both configuration and momentum space
 */

#include "common.h"

// computes the lexicographical site index from the position vector in x
int
gen_site( const int x[ ND ] )
{
  int res = x[ ND - 1 ] ;
  int mu ;
  for( mu = ND - 1 ; mu > 0 ; mu-- ) {
    res = Latt.dims[ mu - 1 ] * res + x[ mu - 1 ] ;
  }
  return res ;
}

// up to the dimension DIMS, allowing for spatial hypercubes and whatever
// returns the site on the -pi to pi momentum space lattice from a momenta
// defined in the 0 -> 2pi BZ.
int
get_site_2piBZ( int x[ ND ] , 
		const int DIMS )
{
  int temp[ND] , mu ;
  for( mu = 0 ; mu < ND ; mu ++ ) {
    temp[mu] = x[mu] ;
    if( mu != DIMS ) {
      if( temp[ mu ] >= ( Latt.dims[ mu ] >> 1 ) ) {
	temp [ mu ] -= Latt.dims[ mu ] ;
      } if( temp[ mu ] < ( Latt.dims[ mu ] >> 1 ) ) {
	temp [ mu ] += ( Latt.dims[ mu ] >> 1 ) ;
      }
    } else { // fill the rest with 0's
      temp[ mu ] = 0 ;
    }
  }
  return gen_site( temp ) ;
}

// returns the site on the 0-> 2pi Bz 
// from the momentum defined in the -Pi to Pi BZ
int
get_site_pipiBZ( int x[ ND ] , 
		 const int DIMS )
{
  int temp[ND] , mu ;
  for( mu = 0 ; mu < ND ; mu ++ ) {
    temp[mu] = x[mu] ; 
    if( mu != DIMS ) {
      if(  temp[ mu ] < 0 ) {
	temp[ mu ] += Latt.dims[ mu ] ; 
      }
    } else { // fill the rest with 0's
      temp[ mu ] = 0 ;
    }
  }
  return gen_site( temp ) ;
}

// gets the momentum in the -Pi -> Pi BZ in terms of the momentum space lattice
void 
get_mom_pipi( int x[ ND ] , 
	      const int i , 
	      const int DIMS )
{
  int mu , subvol = 1 ;
  for( mu = 0 ; mu < ND ; mu++ ) {
    if( mu != DIMS ) {
      x[ mu ] = ( ( i - i % subvol ) / subvol ) % Latt.dims[ mu ] - ( Latt.dims[ mu ] >> 1 ) ;
      subvol *= Latt.dims[ mu ] ;
    } else {
      x[ mu ] = 0 ;
    }
  }
  return ;
}

// generic version of the below
void 
get_mom_2piBZ( int x[ ND ] , 
	       const int i , 
	       const int DIMS )
{
  int mu , subvol = 1 ;
  for( mu = 0 ; mu < ND ; mu++ ) {
    if( mu != DIMS ) {
      x[ mu ] = ( ( i - i % subvol ) / subvol ) % Latt.dims[ mu ] ;
      subvol *= Latt.dims[ mu ] ;
    } else {// set it to 0?
      x[ mu ] = 0 ;
    }
  }
  return ;
}

// inline for the conversion between 2pi and -pi,pi BZ's
void
TwoPI_mpipi_momconv( int MOM[ ND ] , 
		     const int i , 
		     const int DIR )
{
  get_mom_2piBZ( MOM , i , DIR ) ; 
  const int newi = get_site_2piBZ( MOM , DIR ) ;
  get_mom_pipi( MOM , newi , DIR ) ; 
  return ;
}

// translator for the HiRep geometry ...
void 
get_mom_2piBZ_hirep2( int x[ ND ] , 
		      const int i )
{
  int mu , subvol = 1 ;
  for( mu = ND - 1 ; mu > 0 ; mu-- ) {
    x[ mu - 1 ] = ( ( i - i % subvol ) / subvol ) % Latt.dims[ mu - 1 ] ;
    subvol *= Latt.dims[ mu - 1 ] ;
  }
  x[ ND - 1 ] = ( ( i - i % subvol ) / subvol ) % Latt.dims[ ND - 1 ] ;
  return ;
}

// quick physical p calculator
void
compute_p( double p[ ND ] ,
	   const int n[ ND ] ,
	   const int DIMS )
{
  int mu ;
  for( mu = 0 ; mu < ND ; mu++ ) {
    if( mu < DIMS ) {
      p[ mu ] = n[mu] * Latt.twiddles[mu] ; 
      #ifdef SIN_MOM
      p[ mu ] = 2. * sin ( p[mu] * 0.5 ) ; 
      #endif
    } else {
      p[ mu ] = 0.0 ;
    }
  }
  return ;
}

// returns the psq ...
double 
gen_p_sq( const int i , 
	  const int DIMS )
{
  int n[ ND ] ;
  double kcos = 0. ; 
  int mu ;
  //mapped between 0 to 2Pi -> Quicker and for cos does not matter p^2 symmetric
  get_mom_2piBZ( n , i , DIMS ) ; 
  for( mu = 0 ; mu < DIMS ; mu++ ) {
    kcos += cos( n[mu] * Latt.twiddles[mu] ) ; 
  }
  if( kcos == (double)DIMS ) {
    return 1.0 ; 
  } else {
    return 2.0 * ( (double)DIMS - kcos ) ;
  }
}

// feynman gauge psq thing
double 
gen_p_sq_feyn( const int i , 
	       int *flag )
{
  int n[ ND ] ; 
  //mapped between 0 to 2Pi
  get_mom_2piBZ( n , i , ND ) ; 
  *flag = 1 ;
  double kcos = 0. ;
  int mu ;
  for( mu = 0 ; mu < ND ; mu++ ) {
    kcos += cos( n[mu] * Latt.twiddles[mu] ) ; 
    // if all the spatial terms are zero set the flag to zero
    if( ( *flag != 0 ) && ( mu < ( ND-1 ) ) ) { *flag = ( abs( n[mu] ) > 0 ) ? 0 : 1 ; }
  }
  if( kcos == (double)ND ) {
    return 1.0 ;
  } else {
    return 2.0 * ( (double)ND - kcos ) ;
  }
}

// ND - generic momentum getter for lattice momentum
void 
gen_get_p( double p[ ND ] , 
	   const int i , 
	   const int DIMS )
{
  int n[ ND ] , mu ;
  //mapped between 0 to 2Pi
  get_mom_pipi( n , i , DIMS ) ; 
  for( mu = 0 ; mu < ND ; mu++ ) {
    p[ mu ] = n[mu] * Latt.twiddles[mu] ; 
    #ifdef SIN_MOM
    p[ mu ] = 2. * sin ( p[mu] * 0.5 ) ; 
    #endif
  }
  return ;
}

//// CONFIG-SPACE routines ////

// gives the lattice vector from the origin in x
// assumes periodicity
void
get_vec_from_origin( int n[ ND ] , 
		     const int i , 
		     const int DIMS )
{
  get_mom_2piBZ( n , i , DIMS ) ;

  // periodicity enforcement
  int mu ;
  for( mu = 0 ; mu < DIMS ; mu++ ) {
    if( n[mu] > ( Latt.dims[mu]>>1 ) ) {
      n[mu] -= ( Latt.dims[mu] ) ;
    }
  }

  return ;
}

// compute radial separation squared in configuration space
int
compute_rsq( const int site , 
	     const int DIMS )
{
  // compute "r^2"
  int r[ ND ] ;
  get_vec_from_origin( r , site , DIMS ) ;
  register int sumsq = 0 , mu ;
  for( mu = 0 ; mu < DIMS ; mu++ ) {
    sumsq += r[mu] * r[mu] ;
  }
  return sumsq ; 
}

// computes the lattice index of the spatial hypercube of a
// point that is separation away from k
int
compute_spacing( const int separation[ ND ] ,
		 const int k ,
		 const int DIMS )
{
  int n[ ND ] , mu ;
  get_mom_2piBZ( n , k , DIMS ) ;
  for( mu = 0 ; mu < ND ; mu++ ) {
    if( mu < DIMS ) {
      n[mu] = ( n[mu] + separation[mu] ) % Latt.dims[mu] ;
    } else {
      n[ mu ] = 0 ;
    }
  }
  return gen_site( n ) ;
}

// This is the new bona-fide generic shifting code
int 
gen_shift( const int i , 
	   const int dir )
{
  int x[ ND ] ; 
  get_mom_2piBZ( x , i , ND ) ;
  if( dir >= 0 ) {
    x[ dir ] = ( x[ dir ] + 1 ) % Latt.dims[ dir ] ;
  } else {
    register const int numu = -dir - 1 ;
    if( x[ numu ] == 0 ) {
      x[ numu ] = x[ numu ] - 1 + Latt.dims[ numu ] ;
    } else {
      x[ numu ] = ( x[ numu ] - 1 ) % Latt.dims[ numu ] ;
    }
  }
  return gen_site( x ) ; 
}

// initialises the navigation for our lattice fields
void 
init_navig( struct site *__restrict lat )
{
  int i ; 
#pragma omp parallel for private(i)
  for(  i = 0 ; i < LVOLUME ; i++  )  {
    int mu ;
    for(  mu = 0 ; mu < ND ; mu++  )	{
      lat[i].neighbor[mu] = gen_shift( i , mu ) ; 
      lat[i].back[mu] = gen_shift( i , -mu - 1 ) ;  
    }
  }
  return ;
}

// initialise the lattice information stored in Latt
void
init_geom( void )
{
  // these are neccessary for geometry and stuff ::  x,y,z,t geometry
  printf( "\n[DIMENSIONS] ( %d x" , Latt.dims[0] ) ;
#if ND == 2
  Latt.Lcu = Latt.dims[0] ;
  Latt.Volume = Latt.Lcu * Latt.dims[1] ;
  printf( " %d )\n\n" , Latt.dims[1] ) ; 
  return ;
#else
  int mu ;
  Latt.Lsq = Latt.dims[0] ; // defined :: LSQ
  for( mu = 1 ; mu < ND-2 ; mu++ ) {
    printf( " %d x" , Latt.dims[ mu ] ) ;
    Latt.Lsq *= Latt.dims[mu] ;
  } 
  Latt.Lcu = Latt.Lsq * Latt.dims[ ND - 2 ] ;     // defined :: LCU
  Latt.Volume = Latt.Lcu * Latt.dims[ ND - 1 ] ;  // defined :: LVOLUME
  printf( " %d x %d )\n\n" , Latt.dims[mu] , Latt.dims[ mu+1 ] ) ; 
#endif
  // precompute the twiddle factors
  for( mu = 0 ; mu < ND ; mu++ ) {
    Latt.twiddles[ mu ] = TWOPI / (double)Latt.dims[ mu ] ;
  }
  return ;
}
