/*
 * Transmit module for STANAG 4285 modem.
 */
#include <math.h>
#include <string.h>
#include "general.h"

extern FComplex       tx_preamble_lookup[PREAMBLE_LENGTH];
extern float          tx_coffs[TX_FILTER_LENGTH];
extern       FComplex scrambler_table[SCRAMBLER_TABLE_LENGTH];
extern const FComplex symbol_psk2[2];
extern const FComplex symbol_psk4[4];
extern const FComplex symbol_psk8[8];

int            tx_sample_count;
int            tx_scramble_count;
unsigned short tx_samples[SAMPLE_BLOCK_SIZE * 4];
int            tx_mode;
int            tx_count;

static inline unsigned short float_to_twos(float in)
{
    unsigned short out;
    if (in < 0) {
        out = (unsigned short)(-in * 32768);
        out = (~out) & 0xFFFF;
    } else {
        out = (unsigned short)(in * 32768);
        out = out & 0x7FFF;
    }
    return out;
}

void tx_symbol(FComplex symbol)
{
    int             i, k;
    static FComplex buffer[TX_FILTER_LENGTH / 4];
    static float    acc;
    FComplex        output;
    float           sample;

    for (i = 0; i < (TX_FILTER_LENGTH / 4) - 1; i++)
        buffer[i] = buffer[i + 1];
    buffer[i] = symbol;

    for (k = 0; k < 4; k++) {
        output.real = buffer[0].real * tx_coffs[k];
        output.imag = buffer[0].imag * tx_coffs[k];
        for (i = 1; i < (TX_FILTER_LENGTH / 4); i++) {
            output.real += buffer[i].real * tx_coffs[(i * 4) + k];
            output.imag += buffer[i].imag * tx_coffs[(i * 4) + k];
        }
        sample  =  (float)cos(acc) * output.real;
        sample -= (float)sin(acc) * output.imag;
        sample *= 2.0f;
        acc += (float)(2 * PI * CENTER_FREQUENCY / SAMPLE_RATE);
        if      (acc >= (float)(2 * PI)) acc -= (float)(2 * PI);
        else if (acc <  0.0f)            acc += (float)(2 * PI);
        tx_samples[tx_sample_count++] = float_to_twos(sample);
    }
}

static void tx_preamble(void)
{
    int i;
    for (i = 0; i < PREAMBLE_LENGTH; i++)
        tx_symbol(tx_preamble_lookup[i]);
}

static void tx_probe(void)
{
    int i;
    for (i = 0; i < PROBE_LENGTH; i++) {
        tx_symbol(scrambler_table[tx_scramble_count]);
        tx_scramble_count++;
    }
}

static void tx_frame_state(void)
{
    switch (tx_count) {
    case 0:
        tx_preamble();
        tx_count += PREAMBLE_LENGTH;
        tx_scramble_count = 0;
        break;
    case (PREAMBLE_LENGTH + DATA_LENGTH):
        tx_probe();
        tx_count += PROBE_LENGTH;
        break;
    case (PREAMBLE_LENGTH + DATA_LENGTH + PROBE_LENGTH + DATA_LENGTH):
        tx_probe();
        tx_count += PROBE_LENGTH;
        break;
    case (PREAMBLE_LENGTH + DATA_LENGTH + PROBE_LENGTH + DATA_LENGTH + DATA_LENGTH + PROBE_LENGTH):
        tx_probe();
        tx_count += PROBE_LENGTH;
        break;
    default:
        break;
    }
    tx_count = (tx_count + 1) % SYMBOLS_PER_FRAME;
}

void tx_bpsk(int bit)
{
    FComplex symbol;
    tx_frame_state();
    symbol.real = cmultReal(symbol_psk2[bit], scrambler_table[tx_scramble_count]);
    symbol.imag = cmultImag(symbol_psk2[bit], scrambler_table[tx_scramble_count]);
    tx_scramble_count++;
    tx_symbol(symbol);
}

static void tx_qpsk(int dibit)
{
    FComplex symbol;
    tx_frame_state();
    symbol.real = cmultReal(symbol_psk4[dibit], scrambler_table[tx_scramble_count]);
    symbol.imag = cmultImag(symbol_psk4[dibit], scrambler_table[tx_scramble_count]);
    tx_scramble_count++;
    tx_symbol(symbol);
}

static void tx_psk8(int tribit)
{
    FComplex symbol;
    tx_frame_state();
    symbol.real = cmultReal(symbol_psk8[tribit], scrambler_table[tx_scramble_count]);
    symbol.imag = cmultImag(symbol_psk8[tribit], scrambler_table[tx_scramble_count]);
    tx_scramble_count++;
    tx_symbol(symbol);
}

void tx_octet(unsigned char octet)
{
    int bits[8];
    int bit1, bit2, bit3, bit4;
    int i, j;
    static int bitcount;
    static int tribit;

    bits[0] = octet & 0x01 ? 1 : 0;
    bits[1] = octet & 0x02 ? 1 : 0;
    bits[2] = octet & 0x04 ? 1 : 0;
    bits[3] = octet & 0x08 ? 1 : 0;
    bits[4] = octet & 0x10 ? 1 : 0;
    bits[5] = octet & 0x20 ? 1 : 0;
    bits[6] = octet & 0x40 ? 1 : 0;
    bits[7] = octet & 0x80 ? 1 : 0;

    switch (tx_mode & 0x000F) {
    case TX_75_BPS:
        for (i = 0; i < 8; i++) {
            convolutional_encode(bits[i], &bit1, &bit2);
            for (j = 0; j < 8; j++) {
                tx_bpsk(interleave(bit1));
                tx_bpsk(interleave(bit2));
            }
        }
        break;
    case TX_150_BPS:
        for (i = 0; i < 8; i++) {
            convolutional_encode(bits[i], &bit1, &bit2);
            for (j = 0; j < 4; j++) {
                tx_bpsk(interleave(bit1));
                tx_bpsk(interleave(bit2));
            }
        }
        break;
    case TX_300_BPS:
        for (i = 0; i < 8; i++) {
            convolutional_encode(bits[i], &bit1, &bit2);
            for (j = 0; j < 2; j++) {
                tx_bpsk(interleave(bit1));
                tx_bpsk(interleave(bit2));
            }
        }
        break;
    case TX_600_BPS:
        for (i = 0; i < 8; i++) {
            convolutional_encode(bits[i], &bit1, &bit2);
            tx_bpsk(interleave(bit1));
            tx_bpsk(interleave(bit2));
        }
        break;
    case TX_1200_BPS:
        for (i = 0; i < 8; i++) {
            convolutional_encode(bits[i], &bit1, &bit2);
            bit1 = interleave(bit1);
            bit2 = interleave(bit2);
            tx_qpsk((bit1 << 1) + bit2);
        }
        break;
    case TX_2400_BPS:
        for (i = 0; i < 4; i++) {
            convolutional_encode(bits[i * 2],       &bit1, &bit2);
            convolutional_encode(bits[(i * 2) + 1], &bit3, &bit4);
            bit1 = interleave(bit1);
            bit2 = interleave(bit2);
            bit3 = interleave(bit3);
            /* bit4 discarded per original design */
            tx_psk8((bit1 << 2) + (bit2 << 1) + bit3);
        }
        break;
    case TX_1200U_BPS:
        for (i = 0; i < 8; i++)
            tx_bpsk(bits[i]);
        break;
    case TX_2400U_BPS:
        for (i = 0; i < 4; i++)
            tx_qpsk((bits[i * 2] << 1) + bits[(i * 2) + 1]);
        break;
    case TX_3600U_BPS:
        for (i = 0; i < 8; i++) {
            tribit   = tribit << 1;
            tribit  += bits[i];
            bitcount = (bitcount + 1) % 3;
            if (bitcount == 0) tx_psk8(tribit & 7);
        }
        break;
    default:
        break;
    }
}
