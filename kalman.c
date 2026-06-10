//
// Root Kalman Equalizer.
//
#include <math.h>
#include "general.h"
#include "equalize.h"
#include "kalman.h"

#define KN (FF_EQ_LENGTH+FB_EQ_LENGTH)

FComplex  c[KN];

/*
 *
 * Local variables
 *
 */
static FComplex  f[KN];
static FComplex  g[KN];
static FComplex  u[KN][KN];
static FComplex  h[KN];
static float     d[KN];
static float     a[KN];
static float     y;
static float     q;
static float     E;

/*
 *
 * Diagnostic routine
 *
 *
 */


void kalman_reset_coffs(void)
{
	int i;
	
	for( i=0; i < KN ; i++ )
	{
		c[i].real = 0.0;
		c[i].imag = 0.0;
	}
}
void kalman_reset_ud( void )
{
	int i,j;

	for( j = 0 ; j < KN ; j++ )
	{
		for( i = 0; i < j ; i++ )
		{
			u[i][j].real = 0.0;
			u[i][j].imag = 0.0;			
		}
		d[j]         = 1.0;			
	}
}


/*
 *
 *
 * Initialise this module
*
 *
 */
void kalman_init( void )
{
	q      = 0.008;
	E      = 0.01;

	kalman_reset_ud();
	kalman_reset_coffs();
}

/*
 *
 *
 * Modified Root Kalman gain Vector estimator
 *
 *
 */
 
void kalman_calculate( FComplex *x )
{
	int        i,j;
	FComplex   B0;
	float      hq;
	float      B;
	float      ht;

    	f[0].real =  x[0].real;               // 6.2
	f[0].imag = -x[0].imag;

	for( j = 1; j < KN ; j++)              // 6.3
	{
		f[j].real  = cmultRealConj(u[0][j],x[0]) + x[j].real; 
		f[j].imag  = cmultImagConj(u[0][j],x[0]) - x[j].imag;
		 
		for( i = 1 ; i < j ; i++ )
		{			
			f[j].real += cmultRealConj(u[i][j],x[i]);
			f[j].imag += cmultImagConj(u[i][j],x[i]); 
		}
	}

	for( j = 0; j < KN ; j++)                // 6.4
	{
		g[j].real = d[j]*f[j].real;
		g[j].imag = d[j]*f[j].imag;
	}


        a[0] = E + cmultRealConj(g[0],f[0]); 	 // 6.5

	for( j = 1; j < KN ; j++ ) // 6.6
	{
		a[j] = a[j-1] + cmultRealConj(g[j],f[j]);
	}
	
	hq  = 1 + q;                              // 6.7
	ht  = a[KN-1]*q;

	y = 1.0/(a[0]+ht);                       // 6.19
	
	d[0] = d[0] * hq * ( E + ht ) * y;       // 6.20

	// 6.10 - 6.16 (Calculate recursively)

	for( j = 1; j < KN ; j++ )
	{
		B = a[j-1] + ht;                 // 6.21

		h[j].real = -f[j].real*y;        // 6.11
		h[j].imag = -f[j].imag*y;

		y = 1.0/(a[j]+ht);               // 6.22

		d[j] = d[j]*hq*B*y;              // 6.13
		
 		for( i = 0; i < j ; i++ )
		{
			B0           =  u[i][j];
			u[i][j].real =  B0.real + cmultRealConj(h[j],g[i]); // 6.15
			u[i][j].imag =  B0.imag + cmultImagConj(h[j],g[i]);

			g[i].real +=  cmultRealConj(g[j],B0);               // 6.16
			g[i].imag +=  cmultImagConj(g[j],B0);
		}
	}
}
/*
 *
 * Update the filter coefficients using the Kalman gain vector and 
 * the error 
 *
 */
void kalman_update( FComplex *data, FComplex error )
{
	int i;
	//
    	// Calculate the new Kalman gain vector 
	//

	kalman_calculate( data );

	//
	// Update the filter coefficients using the gain vector
	// and the error.
	//

	error.real *= y;
	error.imag *= y;

	for( i = 0; i < KN ; i++ )
	{
		c[i].real  += cmultReal(error,g[i]);
		c[i].imag  += cmultImag(error,g[i]);
	}
}
