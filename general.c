/*
 * 
 * General module for serial tone HF modem.
 *
 */
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <signal.h>
#include <ctype.h>

#include "sdef.h"
#include "general.h"
#include "equalize.h"
#include "kalman.h"


/* Global */
extern int            tx_sample_count;
extern unsigned short tx_samples[SAMPLE_BLOCK_SIZE*10];
extern int            con_rx_octets_per_block;
extern int            con_tx_octets_per_block;
extern int 		  con_tx_flush_octets;

/* Local variables */
static unsigned long  ticks;

void fp_error( int val )
{
	kalman_init();
}
/*
 * Used for timer in collision advoidance.
*/
void update_tick_counter(void)
{
	ticks++;
}
#ifdef __LOOPBACK__
void output_data_stats( unsigned char *data, int length)
{
	int     i;
	int     temp;
	static int error;
	static int count;
	static int nr_times;
	float  val;

	for( i = 0; i < length; i++ )
	{
		temp = data[i]^0x50;
		temp&0x80?printf(" "),error++:printf("#");
		temp&0x40?printf(" "),error++:printf("#");
		temp&0x20?printf(" "),error++:printf("#");
		temp&0x10?printf(" "),error++:printf("#");
		temp&0x08?printf(" "),error++:printf("#");
		temp&0x04?printf(" "),error++:printf("#");
		temp&0x02?printf(" "),error++:printf("#");
		temp&0x01?printf(" "),error++:printf("#");
	}

	++nr_times;
	if( nr_times == 100 )
	{
		count = error=0;
	}
	count += (8*length);
	val = (float)error/count;
	printf(" %f %d\n",val,nr_times);
}
#endif

void output_data( unsigned char *data, int length)
{
#ifdef __LOOPBACK__

	output_data_stats( data,length);

#else
#ifdef __TRACE__
	int i;
	for( i= 0; i<length; i++) printf("%.2X ",data[i]);
	printf("\n");
#endif
	/* Write directly to stdout the moment bytes emerge from the
	 * demodulator/deinterleaver.  No HDLC buffering, so output
	 * appears in real-time on every sync-frame boundary.          */
	fwrite(data, 1, length, stdout);
	fflush(stdout);

#endif
}
