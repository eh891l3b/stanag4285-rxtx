/*
 *
 * Kalman header file.
 *
 */
#ifndef __KALMAN_H__
#define __KALMAN_H__

typedef struct{
	double real;
	double imag;
}DComplex;

/****************************************/
/*                                      */
/*       Kalman prototypes              */
/*                                      */
/****************************************/

void kalman_init( void );
void kalman_calculate( FComplex *x );
void kalman_update( FComplex *data, FComplex error );
void kalman_reset_coffs( void );
void kalman_reset_ud( void );

#endif