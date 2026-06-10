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
void output_data(unsigned char *data, int length)
{
    fwrite(data, 1, length, stdout);
    fflush(stdout);
}
