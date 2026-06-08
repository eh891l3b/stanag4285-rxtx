/*
 *
 * General header file for serial tone HF voice modem.
 *
 */
#ifndef __GENERAL_H__
#define __GENERAL_H__

#define __4285__

#include "sdef.h"

#define SYMBOLS_PER_FRAME      256
#define SCRAMBLER_TABLE_LENGTH 176
#define SAMPLE_BLOCK_SIZE      SYMBOLS_PER_FRAME*4
#define HALF_SAMPLE_BLOCK_SIZE SAMPLE_BLOCK_SIZE/2
#define SAMPLE_RATE            9600
#define CENTER_FREQUENCY       1800
#define PI                     3.1415926535897932384
#define PROBE_LENGTH           16
#define DATA_LENGTH            32
#define PREAMBLE_LENGTH        80
#define DATA_SYMBOLS_PER_FRAME (SYMBOLS_PER_FRAME-PREAMBLE_LENGTH)
#define MAX_PACKET_LENGTH      10000

#define SEEK_FREQ              35
#define CENTER_FREQ            (2*PI*(CENTER_FREQUENCY))/SAMPLE_RATE;
#define HI_FREQ                (2*PI*(CENTER_FREQUENCY+SEEK_FREQ))/SAMPLE_RATE;
#define LO_FREQ                (2*PI*(CENTER_FREQUENCY-SEEK_FREQ))/SAMPLE_RATE;

#define GOOD_PROBE_THRESHOLD   1
#define BAD_PROBE_THRESHOLD    2

#define TX_FILTER_LENGTH       44
#define RX_FILTER_SIZE 		   43

#define RX_75_BPS              0x0000
#define RX_150_BPS             0x0010
#define RX_300_BPS             0x0020
#define RX_600_BPS             0x0030
#define RX_1200_BPS            0x0040
#define RX_2400_BPS            0x0050
#define RX_1200U_BPS           0x0060
#define RX_2400U_BPS           0x0070
#define RX_3600U_BPS           0x0080

#define TX_75_BPS              0x0000
#define TX_150_BPS             0x0001
#define TX_300_BPS             0x0002
#define TX_600_BPS             0x0003
#define TX_1200_BPS            0x0004
#define TX_2400_BPS            0x0005
#define TX_1200U_BPS           0x0006
#define TX_2400U_BPS           0x0007
#define TX_3600U_BPS           0x0008

#define FFLAG                  0x7E
#define FFESC                  0x7D

#define CRC_LENGTH             4

typedef enum{CAS_RECEIVE,CAS_AUTOPROBE,CAS_TRANSMIT}Cas_mode;
	
typedef struct{
	float real;
	float imag;
}FComplex;

#define cmultReal(x,y)     (x.real*y.real)-(x.imag*y.imag)
#define cmultImag(x,y)     (x.real*y.imag)+(x.imag*y.real) 
#define cmultRealConj(x,y) (x.real*y.real)+(x.imag*y.imag)
#define cmultImagConj(x,y) (x.imag*y.real)-(x.real*y.imag) 

/* Prototypes */

/* Transmit prototypes */
void tx_bpsk( int bit );
/* Autoprobe module */
void tx_auto_probe( void );
int process_rx_autoprobe( float *input );

void reset_input_queue( void );
void reset_output_queue( void );
int  get_input_queue_length( void );
int  get_output_queue_length( void );
int  write_input_queue( unsigned char *data, int length );
int  write_output_queue( unsigned char *data, int length );
int  read_input_queue( unsigned char *data, int length );
int  read_output_queue( unsigned char *data, int length );

/* Packet Routines */
void rx_frame( unsigned char *octet, int length );
void tx_frame( unsigned char *data, int length );
void tx_stuff_frame( void );
void tx_start_frame( void );
int  is_dcd_detected( void );

/* CRC routines */
void reset_crc( unsigned long *crcSum );
void update_crc( unsigned char x, unsigned long *crcSum );
unsigned long crc_32( unsigned char data, unsigned long crc );

/* Psuedo TTY interface */

void p_control( unsigned char *msg, int msg_pointer );
int  p_init( char *device );
void p_poll( void );
void p_write( unsigned char *msg, int length );
void p_write_data( unsigned char *msg, int length );

void build_scrambler_table( void );
void output_data( unsigned char *data, int length);
void tx_octet( unsigned char octet );
void process_rx_block( unsigned short *in );
void reset_transmit( int mode );
void reset_receive( int mode );
void demodulate_and_equalize( FComplex *in );
void demodulate_and_autoprobe( FComplex *in );

void interleaving_tx_2400_short( void );
void interleaving_rx_2400_short( void );
void interleaving_tx_2400_long( void );
void interleaving_rx_2400_long( void );
void interleaving_tx_1200_short( void );
void interleaving_rx_1200_short( void );
void interleaving_tx_1200_long( void );
void interleaving_rx_1200_long( void );
void interleaving_tx_600_75_short( void );
void interleaving_rx_600_75_short( void );
void interleaving_tx_600_75_long( void );
void interleaving_rx_600_75_long( void );

void interleaver_enable( void );
void interleaver_disable( void );
void deinterleaver_enable( void );
void deinterleaver_disable( void );
int  interleave( int input );
float deinterleave( float );
void sync_interleaver( void );
void sync_deinterleaver( void );
void deinterleaver_reset( void );

void convolutional_init( void );
void convolutional_encode_reset( void );
void convolutional_encode( int in, int *bit1, int *bit2 );
void viterbi_decode_reset( void );
void viterbi_set_normal_path_length( void );
void viterbi_set_alternate_1_path_length( void );
void viterbi_set_alternate_2_path_length( void );
int  viterbi_decode( float metric1, float metric2 );

/* Channel access routines */

int sound_samples_ready( void );
int is_input_queuing_allowed( void );

/*
 * Modes that the modem can assume.
 *
 */

void set_duplex( void );
void set_simplex( void );
int  is_duplex( void );
int  is_simplex( void );
void set_vox_ptt( void );
void set_rts_ptt( void );
void set_dtr_ptt( void );
void ptt_transmit( void );
void ptt_receive( void );
void set_tx_mode( int );
void set_rx_mode( int );


/*
 * Shared Memory Routines.
 */
void sm_init( void );
void sm_delete( void );
void sm_transmit( void );
void sm_receive( void );
void sm_update_rxmode( Kmode mode );
void sm_update_txmode( Kmode mode );
void sm_update_frequency_error( float  error  );
void sm_update_signal_quality( float  quality );
void sm_poll_parameter_update( void );
void sm_dcd_on( void );
void sm_dcd_off( void );

/* stdio I/O (io_stdio.c) */
void io_stdio_init( void );
void flush_rx_to_stdout( void );

#endif 