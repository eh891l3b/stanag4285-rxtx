/*
	Channel Equalizer.
	
	Written by C.H Brain G4GUO Aug 2000
	
*/
#include "math.h"
#include "general.h"
#include "equalize.h"
#include "kalman.h"

static FComplex d_eq[FF_EQ_LENGTH+FB_EQ_LENGTH];

extern FComplex c[FF_EQ_LENGTH+FB_EQ_LENGTH];

static FComplex equalize( FComplex *in )
{
	int      i;
	float    ii,qq,iq,qi;
	FComplex symbol;
	
	for( i = 0 ; i < FF_EQ_LENGTH ; i++ )
	{
		d_eq[i] = in[i];
	}
	/* Calculate the symbol */ 
	
	ii = d_eq[0].real*c[0].real;
	qq = d_eq[0].imag*c[0].imag;
	iq = d_eq[0].real*c[0].imag;
	qi = d_eq[0].imag*c[0].real;


	for( i=1; i < (FF_EQ_LENGTH+FB_EQ_LENGTH); i++ )
	{
		ii += d_eq[i].real*c[i].real;
		qq += d_eq[i].imag*c[i].imag;
		iq += d_eq[i].real*c[i].imag;
		qi += d_eq[i].imag*c[i].real;
	}

	symbol.real = ii - qq;
	symbol.imag = iq + qi;
		
	return symbol;
}

/*
 * Initialize the equalizer.
 */
void equalize_init(void)
{
	kalman_init();
}
/*
 *
 * Train the equalizer using known symbols
 *
 */
FComplex equalize_train( FComplex *in, FComplex train )
{
	int      i;
	FComplex error;
	FComplex symbol;
		
	symbol = equalize( in );	
	/* Calculate error */

	error.real = train.real - symbol.real;
	error.imag = train.imag - symbol.imag;

	/* Update the coefficients */
	kalman_update(d_eq,error);         

	/* Update the FB data */

	for( i = (FF_EQ_LENGTH+FB_EQ_LENGTH-1) ; i > FF_EQ_LENGTH ; i-- )
	{
		d_eq[i] = d_eq[i-1];
	}
	d_eq[FF_EQ_LENGTH] =   train; /* Update */

	return(symbol);
}
/*
 *
 * Equalize the data and train on the decision
 *
 */
FDemodulate equalize_data( FComplex *in, FDemodulate (*demodulate)(FComplex) )
{
	int         i;
	FComplex    symbol;
	FDemodulate decision;
	
	symbol = equalize( in );

	/* Decode */
	
	decision = demodulate( symbol );
		
	/* Update the coefficients */
	kalman_update(d_eq,decision.error);
	
	/* Update the FB data */
	
	for( i = (FF_EQ_LENGTH+FB_EQ_LENGTH-1) ; i > FF_EQ_LENGTH ; i-- )
	{
		d_eq[i] = d_eq[i-1];
	}
	d_eq[FF_EQ_LENGTH] =  decision.dx_symbol;

	return decision;
}