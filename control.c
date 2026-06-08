/*
 * 
 * This module controls the Stanag 4285 modem parameters.
 *
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "sdef.h"
#include "general.h"

extern int rx_mode;
extern int tx_mode;

int con_rx_octets_per_block;
int con_tx_octets_per_block;
int con_tx_flush_octets;
int con_bad_probe_threshold;

static enum {DUPLEX_MODE,SIMPLEX_MODE}duplex_mode;
static enum {VOX_PTT,RTS_PTT,DTR_PTT}ptt_type;
static int  serial_fd;

void set_vox_ptt(void)
{
	ptt_type = VOX_PTT;
}
void set_rts_ptt(void)
{
	int param;
	ptt_type = RTS_PTT;

        if ((serial_fd = open("/dev/ttyS1", O_RDWR | O_NONBLOCK)) < 0) 
        {
                perror("serial open");
        }
        else
        {
        	param  = TIOCM_RTS;
		ioctl(serial_fd, TIOCMBIC, &param);
        	param  = TIOCM_DTR;
		ioctl(serial_fd, TIOCMBIC, &param);        
        }
}
void set_dtr_ptt(void)
{
	int param;
	ptt_type = DTR_PTT;

        if ((serial_fd = open("/dev/ttyS1", O_RDWR | O_NONBLOCK)) < 0) 
        {
                perror("serial open");
        }
        else
        {
        	param  = TIOCM_RTS;
		ioctl(serial_fd, TIOCMBIC, &param);
        	param  = TIOCM_DTR;
		ioctl(serial_fd, TIOCMBIC, &param);        
        }
}
void ptt_transmit(void)
{
	int param;
	
	switch(ptt_type)
	{
		case VOX_PTT:
			break;
		case RTS_PTT:
        		param  = TIOCM_RTS;
			ioctl(serial_fd, TIOCMBIS, &param);
			break;
		case DTR_PTT:
     			param  = TIOCM_DTR;
			ioctl(serial_fd, TIOCMBIS, &param);
			break;
		default:
			break;
	}
	sm_transmit();
}
void ptt_receive(void)
{
	int param;
	
	switch(ptt_type)
	{
		case VOX_PTT:
			break;
		case RTS_PTT:
        		param  = TIOCM_RTS;
			ioctl(serial_fd, TIOCMBIC, &param);
			break;
		case DTR_PTT:
     			param  = TIOCM_DTR;
			ioctl(serial_fd, TIOCMBIC, &param);
			break;
		default:
			break;
	}
	sm_receive();
}
void set_duplex(void)
{
	duplex_mode = DUPLEX_MODE;
}
void set_simplex(void)
{
	duplex_mode = SIMPLEX_MODE;
}
int is_duplex(void)
{
	return ( duplex_mode == DUPLEX_MODE ? 1 : 0 );
}
int is_simplex(void)
{
	return ( duplex_mode == SIMPLEX_MODE ? 1 : 0 );
}
void set_tx_mode( int mode )
{
	switch( mode )
	{
		case B75N:
			interleaver_disable();
			tx_mode             = TX_75_BPS;
			con_tx_octets_per_block = 1;
			con_tx_flush_octets     = 21;
			sm_update_txmode( mode );
	  		break;
		case B75S:
			interleaver_enable();
			interleaving_tx_600_75_short();
			tx_mode             = TX_75_BPS;
			con_tx_octets_per_block = 1;
			con_tx_flush_octets     = 21;
			sm_update_txmode( mode );
			break;
		case B75L:
			interleaver_enable();
			interleaving_tx_600_75_long();
			tx_mode             = TX_75_BPS;
			con_tx_octets_per_block = 1;
			con_tx_flush_octets     = 109;
			sm_update_txmode( mode );
			break;
		case B150N:
			interleaver_disable();
			tx_mode             = TX_150_BPS;
			con_tx_octets_per_block = 2;
			con_tx_flush_octets     = 29;
			sm_update_txmode( mode );
			break;							
		case B150S:
			interleaver_enable();
			interleaving_tx_600_75_short();
			tx_mode             = TX_150_BPS;
			con_tx_octets_per_block = 2;
			con_tx_flush_octets     = 29;
			sm_update_txmode( mode );
			break;
		case B150L:
			interleaver_enable();
			interleaving_tx_600_75_long();
			tx_mode             = TX_150_BPS;
			con_tx_octets_per_block = 2;
			con_tx_flush_octets     = 205;
			sm_update_txmode( mode );
			break;
		case B300N:
			interleaver_disable();
			tx_mode = TX_300_BPS;
			con_tx_octets_per_block = 4;
			con_tx_flush_octets     = 45;
			sm_update_txmode( mode );
			break;
		case B300S:
			interleaver_enable();
			interleaving_tx_600_75_short();
			tx_mode = TX_300_BPS;
			con_tx_octets_per_block = 4;
			con_tx_flush_octets     = 45;
			sm_update_txmode( mode );
			break;
		case B300L:
			interleaver_enable();
			interleaving_tx_600_75_long();
			tx_mode = TX_300_BPS;
			con_tx_octets_per_block = 4;
			con_tx_flush_octets     = 397;
			sm_update_txmode( mode );
			break;
		case B600N:
			interleaver_disable();
			tx_mode = TX_600_BPS;
			con_tx_octets_per_block = 8;
			con_tx_flush_octets     = 77;
			sm_update_txmode( mode );
			break;
		case B600S:
			interleaver_enable();
			interleaving_tx_600_75_short();
			tx_mode = TX_600_BPS;
			con_tx_octets_per_block = 8;
			con_tx_flush_octets     = 77;
			sm_update_txmode( mode );
			break;
		case B600L:
			interleaver_enable();
			interleaving_tx_600_75_long();
			tx_mode = TX_600_BPS;
			con_tx_octets_per_block = 8;
			con_tx_flush_octets     = 781;
			sm_update_txmode( mode );
			break;
		case B600U:
			/* Not supported */
			break;
		case B1200N:
			interleaver_disable();
			tx_mode = TX_1200_BPS;
			con_tx_octets_per_block = 16;
			con_tx_flush_octets     = 141;
			sm_update_txmode( mode );
			break;
		case B1200S:
			interleaver_enable();
			interleaving_tx_1200_short();
			tx_mode = TX_1200_BPS;
			con_tx_octets_per_block = 16;
			con_tx_flush_octets     = 141;
			sm_update_txmode( mode );
			break;
		case B1200L:
			interleaver_enable();
			interleaving_tx_1200_long();
			tx_mode = TX_1200_BPS;
			con_tx_octets_per_block = 16;
			con_tx_flush_octets     = 1549;
			sm_update_txmode( mode );
			break;
		case B1200U:
			tx_mode = TX_1200U_BPS;
			con_tx_octets_per_block = 16;
			con_tx_flush_octets     = 16;
			sm_update_txmode( mode );
			break;
		case B1800U:
			/* Not supported */
			break;
		case B2400N:
			interleaver_disable();
			tx_mode = TX_2400_BPS;
			con_tx_octets_per_block = 32;
			con_tx_flush_octets     = 269;
			sm_update_txmode( mode );
			break;
		case B2400S:
			interleaver_enable();
			interleaving_tx_2400_short();
			tx_mode = TX_2400_BPS;
			con_tx_octets_per_block = 32;
			con_tx_flush_octets     = 269;
			sm_update_txmode( mode );
			break;
		case B2400L:
			interleaver_enable();
			interleaving_tx_2400_long();
			tx_mode = TX_2400_BPS;
			con_tx_octets_per_block = 32;
			con_tx_flush_octets     = 3085;
			sm_update_txmode( mode );
			break;
		case B2400U:
			tx_mode = TX_2400U_BPS;
			con_tx_octets_per_block = 32;
			con_tx_flush_octets     = 32;
			sm_update_txmode( mode );
			break;
		case B3600U:
			tx_mode = TX_3600U_BPS;
			con_tx_octets_per_block = 48;
			con_tx_flush_octets     = 48;
			sm_update_txmode( mode );
			break;
		default:
			break;
	}
}
void set_rx_mode( int mode )
{
	switch( mode )
	{
		case B75N:
			deinterleaver_disable();
			rx_mode                 = RX_75_BPS;
			con_rx_octets_per_block = 1;
			con_bad_probe_threshold = 1;
			sm_update_rxmode( mode );
	  		break;
		case B75S:
			deinterleaver_enable();
			interleaving_rx_600_75_short();
			rx_mode             = RX_75_BPS;
			con_rx_octets_per_block = 1;
			con_bad_probe_threshold = 5;
			viterbi_set_alternate_2_path_length();
			sm_update_rxmode( mode );
			break;
		case B75L:
			deinterleaver_enable();
			interleaving_rx_600_75_long();
			rx_mode             = RX_75_BPS;
			con_rx_octets_per_block = 1;
			con_bad_probe_threshold = 10;
			viterbi_set_normal_path_length();
			sm_update_rxmode( mode );
			break;
		case B150N:
			deinterleaver_disable();
			rx_mode             = RX_150_BPS;
			con_rx_octets_per_block = 1;
			sm_update_rxmode( mode );
			break;							
		case B150S:
			deinterleaver_enable();
			interleaving_rx_600_75_short();
			rx_mode             = RX_150_BPS;
			con_rx_octets_per_block = 2;
			con_bad_probe_threshold = 5;
			viterbi_set_alternate_1_path_length();
			sm_update_rxmode( mode );
			break;
		case B150L:
			deinterleaver_enable();
			interleaving_rx_600_75_long();
			rx_mode             = RX_150_BPS;
			con_rx_octets_per_block = 2;
			con_bad_probe_threshold = 10;
			viterbi_set_normal_path_length();
			sm_update_rxmode( mode );
			break;
		case B300N:
			deinterleaver_disable();
			rx_mode = RX_300_BPS;
			con_rx_octets_per_block = 4;
			con_bad_probe_threshold = 1;
			sm_update_rxmode( mode );
			break;
		case B300S:
			deinterleaver_enable();
			interleaving_rx_600_75_short();
			rx_mode = RX_300_BPS;
			con_rx_octets_per_block = 4;
			con_bad_probe_threshold = 2;
			viterbi_set_normal_path_length();
			sm_update_rxmode( mode );
			break;
		case B300L:
			deinterleaver_enable();
			interleaving_rx_600_75_long();
			rx_mode = RX_300_BPS;
			con_rx_octets_per_block = 4;
			con_bad_probe_threshold = 10;
			viterbi_set_normal_path_length();
			sm_update_rxmode( mode );
			break;
		case B600N:
			deinterleaver_disable();
			rx_mode = RX_600_BPS;
			con_rx_octets_per_block = 8;
			con_bad_probe_threshold = 1;
			sm_update_rxmode( mode );
			break;
		case B600S:
			deinterleaver_enable();
			interleaving_rx_600_75_short();
			rx_mode = RX_600_BPS;
			con_rx_octets_per_block = 8;
			con_bad_probe_threshold = 2;
			viterbi_set_normal_path_length();
			sm_update_rxmode( mode );
			break;
		case B600L:
			deinterleaver_enable();
			interleaving_rx_600_75_long();
			rx_mode = RX_600_BPS;
			con_rx_octets_per_block = 8;
			con_bad_probe_threshold = 10;
			viterbi_set_normal_path_length();
			sm_update_rxmode( mode );
			break;
		case B600U:
			/* Not supported */
			break;
		case B1200N:
			deinterleaver_disable();
			rx_mode = RX_1200_BPS;
			con_rx_octets_per_block = 16;
			con_bad_probe_threshold = 1;
			sm_update_rxmode( mode );
			break;
		case B1200S:
			deinterleaver_enable();
			interleaving_rx_1200_short();
			rx_mode = RX_1200_BPS;
			con_rx_octets_per_block = 16;
			con_bad_probe_threshold = 2;
			viterbi_set_normal_path_length();
			sm_update_rxmode( mode );
			break;
		case B1200L:
			deinterleaver_enable();
			interleaving_rx_1200_long();
			rx_mode = RX_1200_BPS;
			con_rx_octets_per_block = 16;
			con_bad_probe_threshold = 10;
			viterbi_set_normal_path_length();
			sm_update_rxmode( mode );
			break;
		case B1200U:
			rx_mode = RX_1200U_BPS;
			con_rx_octets_per_block = 16;
			con_bad_probe_threshold = 1;
			sm_update_rxmode( mode );
			break;
		case B1800U:
			/* Not supported */
			break;
		case B2400N:
			deinterleaver_disable();
			rx_mode = RX_2400_BPS;
			con_rx_octets_per_block = 32;
			con_bad_probe_threshold = 1;
			sm_update_rxmode( mode );
			break;
		case B2400S:
			deinterleaver_enable();
			interleaving_rx_2400_short();
			rx_mode = RX_2400_BPS;
			con_rx_octets_per_block = 32;
			con_bad_probe_threshold = 2;
			viterbi_set_normal_path_length();
			sm_update_rxmode( mode );
			break;
		case B2400L:
			deinterleaver_enable();
			interleaving_rx_2400_long();
			rx_mode = RX_2400_BPS;
			con_rx_octets_per_block = 32;
			con_bad_probe_threshold = 10;
			viterbi_set_normal_path_length();
			sm_update_rxmode( mode );
			break;
		case B2400U:
			rx_mode = RX_2400U_BPS;
			con_rx_octets_per_block = 32;
			con_bad_probe_threshold = 1;
			sm_update_rxmode( mode );
			break;
		case B3600U:
			rx_mode = RX_3600U_BPS;
			con_rx_octets_per_block = 48;
			con_bad_probe_threshold = 1;
			sm_update_rxmode( mode );
			break;
		default:
			break;
	}
}
