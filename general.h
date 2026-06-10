/*
 * General header file for STANAG 4285 modem.
 */
#ifndef __GENERAL_H__
#define __GENERAL_H__

#define __4285__

#define SYMBOLS_PER_FRAME      256
#define SCRAMBLER_TABLE_LENGTH 176
#define SAMPLE_BLOCK_SIZE      (SYMBOLS_PER_FRAME*4)
#define HALF_SAMPLE_BLOCK_SIZE (SAMPLE_BLOCK_SIZE/2)
#define SAMPLE_RATE            9600
#define CENTER_FREQUENCY       1800
#define PI                     3.1415926535897932384
#define PROBE_LENGTH           16
#define DATA_LENGTH            32
#define PREAMBLE_LENGTH        80
#define DATA_SYMBOLS_PER_FRAME (SYMBOLS_PER_FRAME-PREAMBLE_LENGTH)
#define MAX_PACKET_LENGTH      10000

#define SEEK_FREQ   35
#define CENTER_FREQ (2*PI*(CENTER_FREQUENCY))/SAMPLE_RATE
#define HI_FREQ     (2*PI*(CENTER_FREQUENCY+SEEK_FREQ))/SAMPLE_RATE
#define LO_FREQ     (2*PI*(CENTER_FREQUENCY-SEEK_FREQ))/SAMPLE_RATE

#define TX_FILTER_LENGTH 44
#define RX_FILTER_SIZE   43

/* RX mode nibble (bits 7:4) */
#define RX_75_BPS    0x0000
#define RX_150_BPS   0x0010
#define RX_300_BPS   0x0020
#define RX_600_BPS   0x0030
#define RX_1200_BPS  0x0040
#define RX_2400_BPS  0x0050
#define RX_1200U_BPS 0x0060
#define RX_2400U_BPS 0x0070
#define RX_3600U_BPS 0x0080

/* TX mode nibble (bits 3:0) */
#define TX_75_BPS    0x0000
#define TX_150_BPS   0x0001
#define TX_300_BPS   0x0002
#define TX_600_BPS   0x0003
#define TX_1200_BPS  0x0004
#define TX_2400_BPS  0x0005
#define TX_1200U_BPS 0x0006
#define TX_2400U_BPS 0x0007
#define TX_3600U_BPS 0x0008

typedef struct {
    float real;
    float imag;
} FComplex;

#define cmultReal(x,y)     ((x.real*y.real)-(x.imag*y.imag))
#define cmultImag(x,y)     ((x.real*y.imag)+(x.imag*y.real))
#define cmultRealConj(x,y) ((x.real*y.real)+(x.imag*y.imag))
#define cmultImagConj(x,y) ((x.imag*y.real)-(x.real*y.imag))

/*
 * Mode enum passed to set_tx_mode() / set_rx_mode().
 */
typedef enum {
    B75N=0, B75S, B75L,
    B150N,  B150S, B150L,
    B300N,  B300S, B300L,
    B600N,  B600S, B600L, B600U,
    B1200N, B1200S, B1200L, B1200U,
    B1800U,
    B2400N, B2400S, B2400L, B2400U,
    B3600U
} Kmode;

/* ------------------------------------------------------------------ */
/* Transmit                                                            */
/* ------------------------------------------------------------------ */
void tx_bpsk(int bit);
void tx_octet(unsigned char octet);

/* ------------------------------------------------------------------ */
/* Receive                                                             */
/* ------------------------------------------------------------------ */
void process_rx_block(unsigned short *in);
void output_data(unsigned char *data, int length);

/* ------------------------------------------------------------------ */
/* Modem mode control                                                  */
/* ------------------------------------------------------------------ */
void set_tx_mode(int mode);
void set_rx_mode(int mode);

/* ------------------------------------------------------------------ */
/* DSP support                                                         */
/* ------------------------------------------------------------------ */
void build_scrambler_table(void);
void demodulate_and_equalize(FComplex *in);

/* Convolutional codec */
void convolutional_init(void);
void convolutional_encode_reset(void);
void convolutional_encode(int in, int *bit1, int *bit2);
void viterbi_decode_reset(void);
void viterbi_set_normal_path_length(void);
void viterbi_set_alternate_1_path_length(void);
void viterbi_set_alternate_2_path_length(void);
int  viterbi_decode(float metric1, float metric2);

/* Interleaver */
void interleaving_tx_2400_short(void);
void interleaving_rx_2400_short(void);
void interleaving_tx_2400_long(void);
void interleaving_rx_2400_long(void);
void interleaving_tx_1200_short(void);
void interleaving_rx_1200_short(void);
void interleaving_tx_1200_long(void);
void interleaving_rx_1200_long(void);
void interleaving_tx_600_75_short(void);
void interleaving_rx_600_75_short(void);
void interleaving_tx_600_75_long(void);
void interleaving_rx_600_75_long(void);
void interleaver_enable(void);
void interleaver_disable(void);
void deinterleaver_enable(void);
void deinterleaver_disable(void);
int   interleave(int input);
float deinterleave(float input);
void  deinterleaver_reset(void);

/* Equalizer */
void equalize_init(void);

#endif /* __GENERAL_H__ */
