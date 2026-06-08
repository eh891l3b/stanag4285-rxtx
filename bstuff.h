/*
 * Bit stuffing module
 */

#ifndef __BSTUFF_H__
#define __BSTUFF_H__

void b_tx_flag( void );
void b_tx_frame( void );
void b_tx_octet( unsigned char octet );
void b_rx_octet( unsigned char octet );

#endif
