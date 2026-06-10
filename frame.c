/*
 *
 * Framing module.
 *
 */
#include <stdio.h>
#include "general.h"
#include "bstuff.h"

extern int 		  con_tx_flush_octets;
extern int            con_tx_octets_per_block;

void reset_rx_frame(void)
{
}
void rx_frame( unsigned char *data, int length )
{
	int i;
	
	for( i = 0; i < length; i++ ) b_rx_octet( data[i] );
}

void tx_frame( unsigned char *data, int length )
{
	unsigned long crc;
	int i,temp;
	
	crc  = 0x000000000;

	if( get_input_queue_length() < con_tx_octets_per_block ) tx_start_frame();

	/* Start Flag */
	b_tx_flag();
	
	/* Send length */
	temp = length>>8;
	b_tx_octet( temp );
	crc = crc_32( temp, crc );
	temp = length&0xFF;

	b_tx_octet( temp );
	crc = crc_32( temp, crc );
			
	/* Data */
	for( i=0; i < length; i++ )
	{
		crc = crc_32( data[i], crc );
		b_tx_octet( data[i] );
	}        			

	/* send CRC */

	b_tx_octet(  crc & 0xFF  );
	b_tx_octet( (crc>>8)&0xFF );
	b_tx_octet( (crc>>16)&0xFF );
	b_tx_octet( (crc>>24)&0xFF );
		
}
void tx_stuff_frame( void )
{
	int i;

	/* End Flag */
	b_tx_flag();
	/* Flush zeros, to clear interleaver */
	for( i=0; i<con_tx_flush_octets; i++ ) b_tx_octet(0);
}
void tx_start_frame( void )
{
	int i;
	
	/* Flush zeros, for frequency tracking */
	for( i=0; i<con_tx_octets_per_block; i++ ) b_tx_octet(0);
	/* Start Flag */
	b_tx_flag();
}