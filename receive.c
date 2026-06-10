/*
 * Receive module for STANAG 4285 modem.
 */
#include <stdio.h>
#include <math.h>
#include "general.h"
#include "equalize.h"
#include "kalman.h"

extern FComplex rx_preamble_lookup[PREAMBLE_LENGTH];
extern float    rx_coffs[RX_FILTER_SIZE];

int rx_mode;
extern int con_bad_probe_threshold;

typedef enum { RX_HUNTING, RX_DATA } RxState;

static RxState  rx_state;
static float    rx_acc;                              /* NCO accumulator     */
static FComplex in_a[SAMPLE_BLOCK_SIZE + RX_FILTER_SIZE];
static FComplex in_b[3 * HALF_SAMPLE_BLOCK_SIZE];

static FComplex rx_filter(FComplex *in)
{
    int      i;
    FComplex out;

    out.real = in[0].real * rx_coffs[0];
    out.imag = in[0].imag * rx_coffs[0];

    for (i = 1; i < RX_FILTER_SIZE; i++) {
        out.real += in[i].real * rx_coffs[i];
        out.imag += in[i].imag * rx_coffs[i];
    }
    return out;
}

static float preamble_correlate(FComplex *in)
{
    int   i;
    float real, imag;

    real = in[0].real * rx_preamble_lookup[0].real;
    imag = in[0].imag * rx_preamble_lookup[0].real;

    for (i = 1; i < PREAMBLE_LENGTH; i++) {
        real += in[i * 2].real * rx_preamble_lookup[i].real;
        imag += in[i * 2].imag * rx_preamble_lookup[i].real;
    }
    return (real * real) + (imag * imag);
}

static int preamble_hunt(FComplex *in, float *mag)
{
    int   i, max_index = 0;
    float max_value = 0, val;

    for (i = 0; i < HALF_SAMPLE_BLOCK_SIZE; i++) {
        val = preamble_correlate(&in[i]);
        if (val > max_value) {
            max_value = val;
            max_index = i;
        }
    }
    *mag = max_value;
    return max_index;
}

static int preamble_check(FComplex *in)
{
    int   i;
    float real, imag, val_a, val_b, val_c;

    real = in[0].real * rx_preamble_lookup[0].real;
    imag = in[0].imag * rx_preamble_lookup[0].real;
    for (i = 1; i < 31; i++) {
        real += in[i * 2].real * rx_preamble_lookup[i].real;
        imag += in[i * 2].imag * rx_preamble_lookup[i].real;
    }
    val_a = (real * real) + (imag * imag);

    real = in[31].real * rx_preamble_lookup[0].real;
    imag = in[31].imag * rx_preamble_lookup[0].real;
    for (i = 1; i < 31; i++) {
        real += in[(i * 2) + 31].real * rx_preamble_lookup[i].real;
        imag += in[(i * 2) + 31].imag * rx_preamble_lookup[i].real;
    }
    val_b = ((real * real) + (imag * imag)) * 10;

    real = in[62].real * rx_preamble_lookup[0].real;
    imag = in[62].imag * rx_preamble_lookup[0].real;
    for (i = 1; i < 31; i++) {
        real += in[(i * 2) + 62].real * rx_preamble_lookup[i].real;
        imag += in[(i * 2) + 62].imag * rx_preamble_lookup[i].real;
    }
    val_c = (real * real) + (imag * imag);

    return ((val_a > val_b) && (val_c > val_b)) ? 1 : 0;
}

static inline float twos_to_float(unsigned short in)
{
    if (in & 0x8000)
        return -((~in) & 0x7FFF) * 0.000030517578125f;
    return in * 0.000030517578125f;
}

static inline FComplex agc(FComplex in)
{
    double        h, mag;
    static double hold = 1.0;

    mag  = (in.real * in.real) + (in.imag * in.imag);
    h    = (0.02 * mag) + (0.98 * hold);
    hold = h;
    h    = 1.0 / sqrt(h);
    in.real = (float)(in.real * h);
    in.imag = (float)(in.imag * h);
    return in;
}

static float initial_doppler_correct(FComplex *in, float *delta_out)
{
    int   i;
    float real, imag, error;

    real = cmultRealConj(in[0], in[62]);
    imag = cmultImagConj(in[0], in[62]);

    for (i = 2; i < (PREAMBLE_LENGTH - 31) * 2; i += 2) {
        real += cmultRealConj(in[i], in[i + 62]);
        imag += cmultImagConj(in[i], in[i + 62]);
    }
    if (real == 0.0f) real = 0.0000000001f;
    error      = (float)(atan2(imag, real) * 0.008064516129);
    *delta_out -= error;
    return error;
}

static float doppler_correct(FComplex *in, float *delta_out)
{
    int   i;
    float real, imag, error;

    real = cmultRealConj(in[0], in[62]);
    imag = cmultImagConj(in[0], in[62]);

    for (i = 2; i < (PREAMBLE_LENGTH - 31) * 2; i += 2) {
        real += cmultRealConj(in[i], in[i + 62]);
        imag += cmultImagConj(in[i], in[i + 62]);
    }
    error      = (float)(atan2(imag, real) * 0.008064516129);
    *delta_out -= error * 0.1f;

    /* Clamp to +/- 200 Hz */
#define SYNC_DELTA_MAX ((float)(2 * PI * 200.0 / SAMPLE_RATE))
    if      (*delta_out >  SYNC_DELTA_MAX) *delta_out =  SYNC_DELTA_MAX;
    else if (*delta_out < -SYNC_DELTA_MAX) *delta_out = -SYNC_DELTA_MAX;

    return error;
}

static int train_and_equalize_on_preamble(FComplex *in)
{
    int      i, count;
    FComplex symbol;

    for (i = 0, count = 0; i < PREAMBLE_LENGTH; i++) {
        symbol = equalize_train(&in[i * 2], rx_preamble_lookup[i]);
        if (symbol.real * rx_preamble_lookup[i].real > 0) count++;\
    }
    return count;
}

static void rx_downconvert(float *in, FComplex *outa, FComplex *outb)
{
    int i;

    for (i = 0; i < RX_FILTER_SIZE; i++)
        outa[i] = outa[i + SAMPLE_BLOCK_SIZE];

    for (i = 0; i < SAMPLE_BLOCK_SIZE; i++) {
        outa[i + RX_FILTER_SIZE].real =  (float)cos(rx_acc) * in[i];
        outa[i + RX_FILTER_SIZE].imag = -(float)sin(rx_acc) * in[i];
        rx_acc += CENTER_FREQ;
        if      (rx_acc >= (float)(2 * PI)) rx_acc -= (float)(2 * PI);
        else if (rx_acc <  0.0f)            rx_acc += (float)(2 * PI);
    }

    for (i = 0; i < SAMPLE_BLOCK_SIZE; i++)
        outb[i] = outb[i + HALF_SAMPLE_BLOCK_SIZE];

    for (i = 0; i < HALF_SAMPLE_BLOCK_SIZE; i++)
        outb[i + SAMPLE_BLOCK_SIZE] = agc(rx_filter(&outa[i * 2]));
}

static void rx_final_downconvert(FComplex *in, float delta_val)
{
    int          i;
    static float acc_val;
    FComplex     temp, osc;

    for (i = 0; i < HALF_SAMPLE_BLOCK_SIZE; i++) {
        osc.real  =  (float)cos(acc_val);
        osc.imag  = -(float)sin(acc_val);
        temp.real = cmultReal(osc, in[i]);
        temp.imag = cmultImag(osc, in[i]);
        in[i]     = temp;
        acc_val  += delta_val;
        if      (acc_val >= (float)(2 * PI)) acc_val -= (float)(2 * PI);
        else if (acc_val <  0.0f)            acc_val += (float)(2 * PI);
    }
}

/* Implemented in main.c — dispatches based on g_framing */
extern void output_data_framed(unsigned char *data, int length);

void output_data(unsigned char *data, int length)
{
    output_data_framed(data, length);
}

void process_rx_block(unsigned short *in)
{
    int   i, preamble_matches, start;
    float mag, max_mag;
    float bb[SAMPLE_BLOCK_SIZE];

    static int   bad_preamble_count;
    static int   preamble_start;
    static int   data_start;
    static float sync_delta;

    for (i = 0; i < SAMPLE_BLOCK_SIZE; i++)
        bb[i] = twos_to_float(in[i]);

    if (rx_state == RX_HUNTING) {
        rx_downconvert(bb, in_a, in_b);
        start = preamble_hunt(in_b, &max_mag);
        preamble_start = start;
        data_start     = preamble_start + (PREAMBLE_LENGTH * 2);

        kalman_reset_coffs();
        kalman_reset_ud();
        preamble_matches = train_and_equalize_on_preamble(&in_b[preamble_start]);

        if ((preamble_matches >= (PREAMBLE_LENGTH - 25)) &&
            (preamble_check(&in_b[preamble_start]) != 0)) {
            sync_delta = 0;
            initial_doppler_correct(&in_b[preamble_start], &sync_delta);
            sync_delta *= 2.0f;

            rx_final_downconvert(&in_b[0],                      sync_delta);
            rx_final_downconvert(&in_b[HALF_SAMPLE_BLOCK_SIZE], sync_delta);
            rx_final_downconvert(&in_b[SAMPLE_BLOCK_SIZE],      sync_delta);

            kalman_reset_coffs();
            kalman_reset_ud();
            train_and_equalize_on_preamble(&in_b[preamble_start]);

            bad_preamble_count = 0;
            rx_state           = RX_DATA;
            viterbi_decode_reset();
            deinterleaver_reset();

            demodulate_and_equalize(&in_b[data_start]);
        }
    } else {
        rx_downconvert(bb, in_a, in_b);
        rx_final_downconvert(&in_b[SAMPLE_BLOCK_SIZE], sync_delta);
        kalman_reset_ud();
        preamble_matches = train_and_equalize_on_preamble(&in_b[preamble_start]);
        demodulate_and_equalize(&in_b[data_start]);

        if ((preamble_matches < PREAMBLE_LENGTH - 25) &&
            (preamble_check(&in_b[preamble_start]) == 0)) {
            bad_preamble_count++;
            if (bad_preamble_count >= con_bad_probe_threshold)
                rx_state = RX_HUNTING;
            start = preamble_hunt(in_b, &mag);
            if (preamble_start != start) {
                preamble_start = start;
                data_start     = preamble_start + (PREAMBLE_LENGTH * 2);
                kalman_reset_coffs();
            }
        } else {
            doppler_correct(&in_b[preamble_start], &sync_delta);
            bad_preamble_count = 0;
        }
    }
}
