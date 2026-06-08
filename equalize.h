/*
 *
 * Channel equalizer header file.
 * Written by C.H Brain G4GUO Aug 2000
 *
 */

#ifndef __EQUALIZE_H__
#define __EQUALIZE_H__

typedef struct {
	int   data;
	FComplex rx_symbol;
	FComplex dx_symbol;
	FComplex error;
}FDemodulate;

#define FF_EQ_LENGTH 30
#define FB_EQ_LENGTH 14
#define LAMBDA       0.02

/****************************************/
/*                                      */
/*      Equalizer Prototypes            */
/*                                      */
/****************************************/

void        equalize_init( void );
void        equalize_reset( void );
FComplex    equalize_train( FComplex *in, FComplex train );
FDemodulate equalize_data( FComplex *in, FDemodulate (*demodulate)(FComplex) );
void        save_fb_data( void );
void 	    restore_fb_data( void );

#endif