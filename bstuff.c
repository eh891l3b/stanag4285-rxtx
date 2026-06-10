/*
 * Bit stuffing module
 */
#include <stdio.h>
#include "general.h"
#include "bstuff.h"

static int           tx_one_count;
static unsigned long rx_crc;
static int           rx_length;
static unsigned char rx_data[MAX_PACKET_LENGTH];

void b_out_octet( unsigned char octet )
{
	write_input_queue( &octet, 1 );
}
/* 
 *
 * Transmit a raw bit 
 *
 */
void b_tx_bit( int bit )
{
	static unsigned char octet;
	static unsigned int  count;
	
	octet <<= 1;
	if( bit ) octet |= 1;

	count++;
	
	if( count == 8 )
	{
		b_out_octet(octet);
		count = 0;
		octet = 0;
	}
}
/*
 * Transmit a FLAG
 *
 */
void b_tx_flag( void )
{
	b_tx_bit( 0 );
	b_tx_bit( 1 );
	b_tx_bit( 1 );
	b_tx_bit( 1 );
	b_tx_bit( 1 );
	b_tx_bit( 1 );
	b_tx_bit( 1 );
	b_tx_bit( 0 );
        tx_one_count = 0;
}
/*
 *
 * Most significant bit transmitted first.
 *
 */
void b_tx_octet( unsigned char octet )
{
	int i;
	
	for( i=7; i>= 0; i-- )
	{
		if( octet & (1<<i) )
		{
			/* One */
			tx_one_count++;
			b_tx_bit( 1 );
			if( tx_one_count == 5 )
			{
				/* Stuff bit */
				b_tx_bit( 0 );
				tx_one_count = 0;			
			}
		}
		else
		{
			/* Zero */
			b_tx_bit( 0 );
			tx_one_count = 0;						
		}
	}
}
void b_in_flag( void )
{
	int temp;
	
	temp = rx_data[0];
	temp <<=8;
	temp += rx_data[1];
	
	if( (rx_length == (temp+CRC_LENGTH+2)) && ( rx_crc == 0 ) && ( temp <= MAX_PACKET_LENGTH ) )
	{
		p_write_data(&rx_data[2],temp);
	}
	rx_crc    = 0;
	rx_length = 0;
}
void b_in_octet( unsigned char octet )
{
	rx_data[rx_length++] = octet;
	rx_crc = crc_32( octet, rx_crc );
	if( rx_length == MAX_PACKET_LENGTH ) b_in_flag();
}
void b_in_abort( void )
{
	rx_crc    = 0;
	rx_length = 0;
}
void b_rx_octet( unsigned char octet )
{
	static int            one_in_count;
	static unsigned char  soctet;
	static int            soctet_count;
	int i;

	for( i=7; i >= 0; i-- )
	{
		if( octet & ( 1<<i ) )
		{
			/* One received */
			one_in_count++;
			soctet <<= 1;
			soctet |=   1;
			soctet_count++;
		}
		else
		{
			/* Zero */
			if( one_in_count < 5 )
			{
				/* Zero received most likely outcome */
				soctet <<= 1;
				soctet_count++;
			}
			else
			{
				if( one_in_count == 5 )
				{
					/* stuff 0 received, discard */ 
				}	
				else
				{
					if( one_in_count == 6 )
					{
						/* FLAG received */ 
				        soctet <<= 1;
						if( soctet == 0x7E )
						{ 
							b_in_flag();
						}
						soctet_count = 0;
						soctet       = 0;
					}
					else
					{	
						if( one_in_count >= 8 )
						{
							/* ABORT received */ 
							b_in_abort();
							soctet_count = 0;
							soctet       = 0;
						}
					}
				}			
			}
			one_in_count = 0;
		}
		if( ( soctet_count == 8 ) && ( one_in_count < 8 ) )
		{
			b_in_octet( soctet );
			soctet_count = 0;
			soctet       = 0;
		}
	}
}
