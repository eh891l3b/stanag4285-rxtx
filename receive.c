/*
 *
 * Receive module for serial tone HF voice modem.
 *
 *
 */
#include <stdio.h>
#include <math.h>
#include "general.h"
#include "equalize.h"
#include "kalman.h"

extern FComplex rx_preamble_lookup[PREAMBLE_LENGTH]; 
extern float    rx_coffs[RX_FILTER_SIZE];

/* Globally visible */

int rx_mode;
extern int con_bad_probe_threshold;
extern SmParams *sm_params;

/* Local defines */

#define LOW_RX_CHAN 0
#define MID_RX_CHAN 1
#define HIH_RX_CHAN 2
#define RX_CHANNELS 3
typedef enum{RX_HUNTING,RX_DATA}RxState;

/* Local */

static  RxState rx_state;
static  float   delta[RX_CHANNELS];
static  float   acc[RX_CHANNELS];

static FComplex in_a[RX_CHANNELS][SAMPLE_BLOCK_SIZE+RX_FILTER_SIZE];
static FComplex in_b[RX_CHANNELS][3*HALF_SAMPLE_BLOCK_SIZE];

static void report_frequency_error( float error, int channel )
{
        float frequency;
        
	switch(channel)
	{
		case 0:
		    frequency = - SEEK_FREQ;
		    break;
		case 1:
		    frequency = 0;
		    break;
		case 2:
		    frequency = + SEEK_FREQ;
		    break;
		default:
		    frequency = 0;
		    break;
	}
	frequency -= error*SAMPLE_RATE/(2.0*PI);
	sm_update_frequency_error( frequency );
	//printf("Frequency error = %f\n",frequency);
}
static FComplex rx_filter(FComplex *in)
{
	int      i;
	FComplex out;
	
	out.real = in[0].real*rx_coffs[0];
	out.imag = in[0].imag*rx_coffs[0];
	
	for( i = 1; i < RX_FILTER_SIZE; i++ )
	{
		out.real += in[i].real*rx_coffs[i];
		out.imag += in[i].imag*rx_coffs[i];
	}
	return(out);
}
static float preamble_correlate(FComplex *in)
{
	int i;
	float real,imag;
	
	real = in[0].real*rx_preamble_lookup[0].real;
	imag = in[0].imag*rx_preamble_lookup[0].real;
	
	for( i = 1; i < PREAMBLE_LENGTH; i++ )
	{
		real += in[i*2].real*rx_preamble_lookup[i].real;
		imag += in[i*2].imag*rx_preamble_lookup[i].real;
	}
	return((real*real)+(imag*imag));
}
static int preamble_hunt( FComplex *in, float *mag )
{
	int i;
	int	 	max_index = 0;
	float 		max_value = 0;
	float       val;
	
	/* We are still hunting for the transmission */
	/* Search the entire frame for the preamble  */
	for( i = 0; i < HALF_SAMPLE_BLOCK_SIZE; i++ )
	{
		val = preamble_correlate(&in[i]);
		if( val > max_value )
		{
			max_value = val;
			max_index = i;
		}
	}
	*mag = max_value;
	return max_index;
}
static int preamble_check( FComplex *in )
{
	int i;
	float real,imag;
	float val_a;
	float val_b;
	float val_c;

	/* Break the preamble into two 31 chip sequences */
	/* Compare aligned and non aligned magnitudes    */
	/* make decision.                                */
	
	real = in[0].real*rx_preamble_lookup[0].real;
	imag = in[0].imag*rx_preamble_lookup[0].real;
	
	for( i = 1; i < 31 ; i++ )
	{
		real += in[i*2].real*rx_preamble_lookup[i].real;
		imag += in[i*2].imag*rx_preamble_lookup[i].real;
	}
	val_a = (real*real)+(imag*imag);

	real = in[31].real*rx_preamble_lookup[0].real;
	imag = in[31].imag*rx_preamble_lookup[0].real;
	
	for( i = 1; i < 31 ; i++ )
	{
		real += in[(i*2)+31].real*rx_preamble_lookup[i].real;
		imag += in[(i*2)+31].imag*rx_preamble_lookup[i].real;
	}
	val_b = ((real*real)+(imag*imag))*10;

	real = in[62].real*rx_preamble_lookup[0].real;
	imag = in[62].imag*rx_preamble_lookup[0].real;
	
	for( i = 1; i < 31 ; i++ )
	{
		real += in[(i*2)+62].real*rx_preamble_lookup[i].real;
		imag += in[(i*2)+62].imag*rx_preamble_lookup[i].real;
	}
	val_c = (real*real)+(imag*imag);

	if( ( val_a > val_b ) && ( val_c > val_b ) )
	{
		sm_update_signal_quality( val_a + val_c );
		return 1;
	}
	else
		return 0;
}
static inline float twos_to_float( unsigned short in )
{
	float val;
	
	if(in&0x8000)
        {
        	val = -((~in)&0x7FFF)*0.000030517578125;
        }
        else
        {
                val = in*0.000030517578125;
        }
        return(val);
}
/*
 *
 * Automatic Gain control.
 *
 */
static inline FComplex agc( FComplex in )
{
	double        h,mag;
	static double hold=1.0;
	
	mag = (in.real*in.real)+(in.imag*in.imag);
				
	h = (LAMBDA*mag) + (1.0-LAMBDA)*hold;
	
	hold = h;		

	h       = 1.0/sqrt(h);	
	in.real = (in.real*h);
	in.imag = (in.imag*h);

	return in;
}
int is_dcd_detected( void )
{
	if(  rx_state == RX_HUNTING )
		return 0;
	else
		return 1;
}
void reset_receive( int mode )
{
	rx_state = RX_HUNTING;
	rx_mode  = mode;
	
	delta[LOW_RX_CHAN] = LO_FREQ;
	delta[MID_RX_CHAN] = CENTER_FREQ;
	delta[HIH_RX_CHAN] = HI_FREQ;
	equalize_init();
	kalman_reset_coffs();
	kalman_reset_ud();
	kalman_init();
}
static float initial_doppler_correct( FComplex *in , float *delta )
{
	int i;
	float real,imag,error;	

	real = cmultRealConj(in[0],in[62]);	
	imag = cmultImagConj(in[0],in[62]);	

	for( i = 2 ; i < (PREAMBLE_LENGTH-31)*2 ; i+=2 )
	{
		real += cmultRealConj(in[i],in[i+62]);	
		imag += cmultImagConj(in[i],in[i+62]);	
	}
	if( real == 0.0 ) real = 0.0000000001;/* No divide by zero */
	error  = atan2(imag,real)*0.008064516129;
	*delta -= error;
	
	return error;
}
static float doppler_correct( FComplex *in , float *delta )
{
	int i;
	float real,imag,error;	

	real = cmultRealConj(in[0],in[62]);	
	imag = cmultImagConj(in[0],in[62]);	

	for( i = 2 ; i < (PREAMBLE_LENGTH-31)*2 ; i+=2 )
	{
		real += cmultRealConj(in[i],in[i+62]);	
		imag += cmultImagConj(in[i],in[i+62]);	
	}

	error   = atan2(imag,real)*0.008064516129;
	*delta -= error*0.1;
	
	return error;
}
static int  train_and_equalize_on_preamble( FComplex *in )
{
	int       i;
	int       count;
	FComplex  symbol;
    	
	for( i = 0, count = 0 ; i < PREAMBLE_LENGTH; i++ )
	{
		symbol = equalize_train( &in[(i*2)], rx_preamble_lookup[i] );	
		if(symbol.real*rx_preamble_lookup[i].real > 0) count++;	
	}	
	return count;
}
static void rx_downconvert( float *in, FComplex *outa, FComplex *outb, float *acc, float delta )
{
	int i;
	/* Update Received Data, in place of circular addressing  */
	for( i = 0; i < RX_FILTER_SIZE; i++ )
	{
		outa[i] =  outa[i+SAMPLE_BLOCK_SIZE];
	}
	for( i = 0; i < SAMPLE_BLOCK_SIZE; i++ )
	{
		/* Update with new samples */
		outa[i+RX_FILTER_SIZE].real =  cos(*acc)*in[i];
		outa[i+RX_FILTER_SIZE].imag = -sin(*acc)*in[i];
		*acc         +=  delta;
		if( *acc >= 2*PI ) *acc -= 2*PI;			
	}
	/* Update Received Data, in place of circular addressing  */
	for( i = 0; i < SAMPLE_BLOCK_SIZE ; i++ )
	{
		outb[i] = outb[i+HALF_SAMPLE_BLOCK_SIZE];
	}
	/* Decimate by two and filter */
	for( i = 0; i < HALF_SAMPLE_BLOCK_SIZE; i++ )
	{
		/* Run the channel filter  */
		outb[i+SAMPLE_BLOCK_SIZE] = agc(rx_filter(&outa[i*2]));
	}
}
/*
 *
 * This is a final post mix downconvert.
 * It is a bit of a kludge.
 *
 */
static void rx_final_downconvert( FComplex *in, float delta )
{
	int i;
	static float acc;
	FComplex temp,osc;
	
	for( i = 0; i < HALF_SAMPLE_BLOCK_SIZE; i++ )
	{
		/* Update with new samples */
		osc.real =  cos(acc);
		osc.imag =  -sin(acc);
		temp.real = cmultReal(osc,in[i]);
		temp.imag = cmultImag(osc,in[i]);
		in[i] = temp;		
		acc         +=  delta;
		if( acc >= 2*PI ) acc -= 2*PI;			
	}
}
void process_rx_block( unsigned short *in )
{
	int   		 i;
	int   		 preamble_matches;
	int          start;
	float        mag,max_mag;
	float        bb[SAMPLE_BLOCK_SIZE];
	float        doppler_error;
	
	static int   bad_preamble_count;
	static int   preamble_start;
	static int   data_start;
	static int   rx_chan;
	static float sync_delta;
	
	/* Convert to floating point */
	for( i = 0; i < SAMPLE_BLOCK_SIZE; i++ )
	{
		/* Update with new samples */
		bb[i] = twos_to_float(in[i]);
	}
	
	if( rx_state == RX_HUNTING )
	{
		delta[LOW_RX_CHAN] = LO_FREQ;
		delta[MID_RX_CHAN] = CENTER_FREQ;
		delta[HIH_RX_CHAN] = HI_FREQ;
		max_mag            = 0;

		/* Check all three channels */
		
		for( i=0; i< RX_CHANNELS; i++ )
		{
			rx_downconvert( bb, in_a[i], in_b[i], &acc[i], delta[i] );
	       		start = preamble_hunt( in_b[i], &mag );       
	       	
			if( mag > max_mag )
			{
				preamble_start = start;
				data_start     = preamble_start + (PREAMBLE_LENGTH*2);
				rx_chan        = i;
				max_mag        = mag;
			}
		}	                
		
		/* Train on the probe sequence of the best channel */		
		kalman_reset_coffs();
		kalman_reset_ud();
		preamble_matches = train_and_equalize_on_preamble( &in_b[rx_chan][preamble_start] );

		//printf("INDEX %3d MATCHES %3d\n",preamble_start,preamble_matches);

		if( ( preamble_matches >= ( PREAMBLE_LENGTH - 25 ) ) && ( preamble_check( &in_b[rx_chan][preamble_start] ) != 0 ) )
		{
		    /* Find frequency error */
			sync_delta          = 0;
			doppler_error       = initial_doppler_correct( &in_b[rx_chan][preamble_start], &sync_delta );
			report_frequency_error( doppler_error, rx_chan );
			/* Adjust for decimation ratio */
			sync_delta          = sync_delta*2.0;
			/* correct entire buffered samples */
			rx_final_downconvert( &in_b[rx_chan][0], sync_delta );
			rx_final_downconvert( &in_b[rx_chan][HALF_SAMPLE_BLOCK_SIZE], sync_delta );
			rx_final_downconvert( &in_b[rx_chan][SAMPLE_BLOCK_SIZE], sync_delta );
			/* re-equalize */
			kalman_reset_coffs();
			kalman_reset_ud();
			/* Train and equalize on the frequency corrected preamble */
			train_and_equalize_on_preamble( &in_b[rx_chan][preamble_start] );
			/* Actions done whatsoever */
			bad_preamble_count  = 0;
			rx_state            = RX_DATA;
			sm_dcd_on();
			viterbi_decode_reset();
			sync_deinterleaver();
			deinterleaver_reset();

			if( sm_params->auto_baud )
			{
				/* Look for autobaud information */
				demodulate_and_autoprobe( &in_b[rx_chan][data_start] );
			}
			else
			{
				/* Normal data reception */
				demodulate_and_equalize( &in_b[rx_chan][data_start] );			
			}
		}
	}
	else
	{
		rx_downconvert( bb, in_a[rx_chan], in_b[rx_chan], &acc[rx_chan], delta[rx_chan] );
		rx_final_downconvert( &in_b[rx_chan][SAMPLE_BLOCK_SIZE], sync_delta );
		kalman_reset_ud();
		preamble_matches = train_and_equalize_on_preamble( &in_b[rx_chan][preamble_start] );
		sync_deinterleaver();
		demodulate_and_equalize( &in_b[rx_chan][data_start] );

		//printf("INDEX %3d MATCHES %3d    ",preamble_start,preamble_matches);

		if( ( preamble_matches < PREAMBLE_LENGTH - 25 ) && ( preamble_check( &in_b[rx_chan][preamble_start] ) == 0 ) )
		{
			bad_preamble_count++;
			if( bad_preamble_count >= con_bad_probe_threshold )
			{
			    rx_state = RX_HUNTING;
			    sm_dcd_off();
			}
			/* See if sync position has changed, update if it has */
        		start = preamble_hunt( in_b[rx_chan], &mag);       
			if( preamble_start != start )
			{
				preamble_start = start;
			    data_start     = preamble_start + (PREAMBLE_LENGTH*2);
			    kalman_reset_coffs();
			}
		}
		else
		{
			doppler_correct( &in_b[rx_chan][preamble_start], &sync_delta );
			bad_preamble_count = 0;
		}	
	}
}
