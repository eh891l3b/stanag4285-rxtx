/*
 * STANAG 4285 standalone modem
 *
 * Replaces general.c's main() and sound I/O with miniaudio.
 * Supports live audio (mic/speaker) or WAV file I/O.
 *
 * Usage:
 *   stanag4285 -tx [-mode <MODE>] [-wav <outfile.wav>]
 *   stanag4285 -rx [-mode <MODE>] [-wav <infile.wav>]
 *
 * TX: reads raw bytes from stdin, HDLC-frames + encodes, plays audio.
 * RX: decodes audio, HDLC-deframes, writes raw bytes to stdout.
 *
 * Modes: 75n/s/l  150n/s/l  300n/s/l  600n/s/l
 *        1200n/s/l/u  2400n/s/l/u  3600u
 * Default: 600s
 */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "general.h"
#include "equalize.h"
#include "sdef.h"

/* ------------------------------------------------------------------ */
/* Externs from DSP modules                                            */
extern int            tx_sample_count;
extern unsigned short tx_samples[];
extern int            con_tx_octets_per_block;
extern int            con_rx_octets_per_block;
extern int            con_tx_flush_octets;

/* ------------------------------------------------------------------ */
/* Global state                                                        */
static volatile int g_running = 1;

/* ------------------------------------------------------------------ */
/* TX audio ring buffer (filled by modem, drained by audio callback)  */
#define TX_RING_SIZE (SAMPLE_BLOCK_SIZE * 128)
static short     g_tx_ring[TX_RING_SIZE];
static volatile int g_tx_head = 0;
static volatile int g_tx_tail = 0;

static inline int txring_space(void) {
    int used = (g_tx_head - g_tx_tail + TX_RING_SIZE) % TX_RING_SIZE;
    return TX_RING_SIZE - used - 1;
}
static inline int txring_avail(void) {
    return (g_tx_head - g_tx_tail + TX_RING_SIZE) % TX_RING_SIZE;
}
static inline void txring_write(const short *src, int n) {
    for (int i = 0; i < n; i++) {
        g_tx_ring[g_tx_head] = src[i];
        g_tx_head = (g_tx_head + 1) % TX_RING_SIZE;
    }
}
static inline int txring_read(short *dst, int n) {
    int avail = txring_avail();
    if (n > avail) n = avail;
    for (int i = 0; i < n; i++) {
        dst[i] = g_tx_ring[g_tx_tail];
        g_tx_tail = (g_tx_tail + 1) % TX_RING_SIZE;
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* RX audio ring buffer (filled by audio callback, drained by modem)  */
#define RX_RING_SIZE (SAMPLE_BLOCK_SIZE * 128)
static short     g_rx_ring[RX_RING_SIZE];
static volatile int g_rx_head = 0;
static volatile int g_rx_tail = 0;

static inline int rxring_avail(void) {
    return (g_rx_head - g_rx_tail + RX_RING_SIZE) % RX_RING_SIZE;
}
static inline void rxring_write(const short *src, int n) {
    int space = RX_RING_SIZE - rxring_avail() - 1;
    if (n > space) n = space;  /* drop if full */
    for (int i = 0; i < n; i++) {
        g_rx_ring[g_rx_head] = src[i];
        g_rx_head = (g_rx_head + 1) % RX_RING_SIZE;
    }
}
static inline int rxring_read(short *dst, int n) {
    int avail = rxring_avail();
    if (n > avail) n = avail;
    for (int i = 0; i < n; i++) {
        dst[i] = g_rx_ring[g_rx_tail];
        g_rx_tail = (g_rx_tail + 1) % RX_RING_SIZE;
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* miniaudio callbacks                                                 */

static void playback_callback(ma_device *dev, void *pOut,
                               const void *pIn, ma_uint32 frames)
{
    (void)dev; (void)pIn;
    short *out = (short *)pOut;
    int got = txring_read(out, (int)frames);
    memset(out + got, 0, ((int)frames - got) * sizeof(short));
}

static void capture_callback(ma_device *dev, void *pOut,
                              const void *pIn, ma_uint32 frames)
{
    (void)dev; (void)pOut;
    rxring_write((const short *)pIn, (int)frames);
}

/* ------------------------------------------------------------------ */
/* Mode parser                                                         */
static Kmode parse_mode(const char *s)
{
    char buf[32];
    strncpy(buf, s, 31); buf[31] = '\0';
    for (int i = 0; buf[i]; i++) buf[i] = (char)tolower((unsigned char)buf[i]);

    if (!strcmp(buf,"75n"))    return B75N;
    if (!strcmp(buf,"75s"))    return B75S;
    if (!strcmp(buf,"75l"))    return B75L;
    if (!strcmp(buf,"150n"))   return B150N;
    if (!strcmp(buf,"150s"))   return B150S;
    if (!strcmp(buf,"150l"))   return B150L;
    if (!strcmp(buf,"300n"))   return B300N;
    if (!strcmp(buf,"300s"))   return B300S;
    if (!strcmp(buf,"300l"))   return B300L;
    if (!strcmp(buf,"600n"))   return B600N;
    if (!strcmp(buf,"600s"))   return B600S;
    if (!strcmp(buf,"600l"))   return B600L;
    if (!strcmp(buf,"1200n"))  return B1200N;
    if (!strcmp(buf,"1200s"))  return B1200S;
    if (!strcmp(buf,"1200l"))  return B1200L;
    if (!strcmp(buf,"1200u"))  return B1200U;
    if (!strcmp(buf,"2400n"))  return B2400N;
    if (!strcmp(buf,"2400s"))  return B2400S;
    if (!strcmp(buf,"2400l"))  return B2400L;
    if (!strcmp(buf,"2400u"))  return B2400U;
    if (!strcmp(buf,"3600u"))  return B3600U;
    fprintf(stderr, "Unknown mode '%s', defaulting to 600s\n", s);
    return B600S;
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s -tx [-mode <MODE>] [-wav <outfile.wav>]\n"
        "  %s -rx [-mode <MODE>] [-wav <infile.wav>]\n"
        "\n"
        "Modes: 75n/s/l  150n/s/l  300n/s/l  600n/s/l\n"
        "       1200n/s/l/u  2400n/s/l/u  3600u\n"
        "Default mode: 600s\n"
        "\n"
        "TX: reads raw bytes from stdin, encodes, plays via speaker or -wav file\n"
        "RX: decodes audio from mic or -wav file, writes raw bytes to stdout\n",
        prog, prog);
}

static void handle_sigint(int sig) { (void)sig; g_running = 0; }

/* ------------------------------------------------------------------ */
/* Modem init                                                          */
static void modem_init(Kmode mode)
{
    build_scrambler_table();
    convolutional_init();
    convolutional_encode_reset();
    viterbi_decode_reset();
    reset_input_queue();
    reset_output_queue();
    equalize_init();
    set_tx_mode(mode);
    set_rx_mode(mode);
}

/* ------------------------------------------------------------------ */
/* TX: read stdin → tx_frame() → drain input queue → tx_octet()       */
/*                                                                     */
/* tx_frame() HDLC-encodes data into the modem's input queue.         */
/* We then drain that queue in blocks via tx_octet(), which produces  */
/* baseband samples into tx_samples[].  Those go into our TX ring.    */
/* ------------------------------------------------------------------ */

/* Pump one block-worth of samples from the input queue through the
   modem modulator into our ring buffer.  Returns number of octets
   consumed from the modem input queue.                               */
static int tx_pump_one_block(void)
{
    unsigned char data[MAX_PACKET_LENGTH];
    int n = con_tx_octets_per_block;

    /* If not enough data in the modem input queue, send zeros        */
    int qlen = get_input_queue_length();
    if (qlen < n) {
        memset(data, 0, n);
        if (qlen > 0) read_input_queue(data, qlen);
    } else {
        read_input_queue(data, n);
    }

    tx_sample_count = 0;
    for (int i = 0; i < n; i++) tx_octet(data[i]);
    return n;
}

/* Drain all pending input queue content into the ring buffer,
   sending complete blocks until nothing is left.                     */
static void tx_drain_queue(void)
{
    while (get_input_queue_length() >= con_tx_octets_per_block) {
        /* wait for ring space */
        while (txring_space() < tx_sample_count + SAMPLE_BLOCK_SIZE * 2)
            ma_sleep(1);
        tx_pump_one_block();
        txring_write((short *)tx_samples, tx_sample_count);
    }
}

/* Feed one "message" worth of stdin bytes through the whole TX chain */
static void tx_send_stdin_data(void)
{
    unsigned char buf[4096];
    ssize_t n;

    /* Non-blocking stdin read: grab whatever is available             */
    n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n > 0) {
        /* tx_frame() HDLC-wraps buf and pushes into the modem input queue */
        tx_frame(buf, (int)n);
        tx_stuff_frame();   /* end flag + interleaver flush zeros        */
        tx_drain_queue();
    }
}

/* ------------------------------------------------------------------ */
/* Live TX loop                                                        */
static void run_tx_live(void)
{
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format        = ma_format_s16;
    cfg.playback.channels      = 1;
    cfg.sampleRate             = SAMPLE_RATE;
    cfg.dataCallback           = playback_callback;
    cfg.periodSizeInFrames     = SAMPLE_BLOCK_SIZE;

    ma_device dev;
    if (ma_device_init(NULL, &cfg, &dev) != MA_SUCCESS) {
        fprintf(stderr, "Failed to open playback device\n"); exit(1);
    }

    /* Pre-buffer some silence so the device starts cleanly */
    {
        short silence[SAMPLE_BLOCK_SIZE * 2];
        memset(silence, 0, sizeof(silence));
        txring_write(silence, SAMPLE_BLOCK_SIZE * 2);
    }

    if (ma_device_start(&dev) != MA_SUCCESS) {
        fprintf(stderr, "Failed to start playback device\n"); exit(1);
    }

    /* Set stdin non-blocking */
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    while (g_running) {
        tx_send_stdin_data();
        ma_sleep(10);
    }

    /* Flush the ring before stopping */
    while (txring_avail() > 0) ma_sleep(5);
    ma_sleep(500);
    ma_device_uninit(&dev);
}

/* WAV TX: no audio device — just write encoded samples directly      */
static void run_tx_wav(const char *outfile)
{
    ma_encoder_config enc_cfg = ma_encoder_config_init(
        ma_encoding_format_wav, ma_format_s16, 1, SAMPLE_RATE);
    ma_encoder enc;
    if (ma_encoder_init_file(outfile, &enc_cfg, &enc) != MA_SUCCESS) {
        fprintf(stderr, "Failed to open WAV '%s' for writing\n", outfile);
        exit(1);
    }

    unsigned char buf[4096];
    ssize_t n;

    /* Read all of stdin first, then encode and write                  */
    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        tx_frame(buf, (int)n);
        tx_stuff_frame();

        /* Drain entire input queue to WAV */
        while (get_input_queue_length() > 0) {
            unsigned char data[MAX_PACKET_LENGTH];
            int qn = get_input_queue_length();
            if (qn > con_tx_octets_per_block) qn = con_tx_octets_per_block;
            if (qn == 0) break;
            read_input_queue(data, qn);
            /* Pad to block boundary */
            while (qn < con_tx_octets_per_block) data[qn++] = 0;
            tx_sample_count = 0;
            for (int i = 0; i < con_tx_octets_per_block; i++) tx_octet(data[i]);
            ma_encoder_write_pcm_frames(&enc, tx_samples, tx_sample_count, NULL);
        }
    }

    /* Write a few extra silent blocks so the receiver can fully flush */
    {
        short silence[SAMPLE_BLOCK_SIZE];
        memset(silence, 0, sizeof(silence));
        for (int i = 0; i < 8; i++)
            ma_encoder_write_pcm_frames(&enc, silence, SAMPLE_BLOCK_SIZE, NULL);
    }

    ma_encoder_uninit(&enc);
}

/* ------------------------------------------------------------------ */
/* RX path:                                                            */
/*  process_rx_block() → output queue → rx_frame() → b_rx_octet()    */
/*  → b_in_flag() → p_write_data() → stdout                          */
/* ------------------------------------------------------------------ */

static void rx_process_output_queue(void)
{
    int length = get_output_queue_length();
    if (length <= 0) return;
    unsigned char data[4096];
    while (length > 0) {
        int chunk = length < (int)sizeof(data) ? length : (int)sizeof(data);
        int got   = read_output_queue(data, chunk);
        if (got <= 0) break;
        /* Feed through HDLC deframer → p_write_data() → stdout       */
        rx_frame(data, got);
        length -= got;
    }
}

static void run_rx_live(void)
{
    ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
    cfg.capture.format        = ma_format_s16;
    cfg.capture.channels      = 1;
    cfg.sampleRate            = SAMPLE_RATE;
    cfg.dataCallback          = capture_callback;
    cfg.periodSizeInFrames    = SAMPLE_BLOCK_SIZE;

    ma_device dev;
    if (ma_device_init(NULL, &cfg, &dev) != MA_SUCCESS) {
        fprintf(stderr, "Failed to open capture device\n"); exit(1);
    }
    if (ma_device_start(&dev) != MA_SUCCESS) {
        fprintf(stderr, "Failed to start capture device\n"); exit(1);
    }

    unsigned short block[SAMPLE_BLOCK_SIZE];
    while (g_running) {
        while (rxring_avail() < SAMPLE_BLOCK_SIZE && g_running)
            ma_sleep(1);
        if (!g_running) break;
        rxring_read((short *)block, SAMPLE_BLOCK_SIZE);
        process_rx_block(block);
        rx_process_output_queue();
    }

    ma_device_uninit(&dev);
}

static void run_rx_wav(const char *infile)
{
    ma_decoder_config dec_cfg = ma_decoder_config_init(
        ma_format_s16, 1, SAMPLE_RATE);
    ma_decoder dec;
    if (ma_decoder_init_file(infile, &dec_cfg, &dec) != MA_SUCCESS) {
        fprintf(stderr, "Failed to open WAV '%s'\n", infile);
        exit(1);
    }

    unsigned short block[SAMPLE_BLOCK_SIZE];
    ma_uint64 frames_read;
    while (g_running) {
        ma_result r = ma_decoder_read_pcm_frames(
            &dec, block, SAMPLE_BLOCK_SIZE, &frames_read);
        if (r != MA_SUCCESS || frames_read == 0) break;
        if ((int)frames_read < SAMPLE_BLOCK_SIZE)
            memset(block + frames_read, 0,
                   (SAMPLE_BLOCK_SIZE - (int)frames_read) * sizeof(short));
        process_rx_block(block);
        rx_process_output_queue();
    }

    ma_decoder_uninit(&dec);
}

/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    int   tx_flag  = -1;
    Kmode mode     = B600S;
    char *wav_path = NULL;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "-tx"))  tx_flag = 1;
        else if (!strcmp(argv[i], "-rx"))  tx_flag = 0;
        else if (!strcmp(argv[i], "-mode") && i+1 < argc)
            mode = parse_mode(argv[++i]);
        else if (!strcmp(argv[i], "-wav") && i+1 < argc)
            wav_path = argv[++i];
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_usage(argv[0]); return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            print_usage(argv[0]); return 1;
        }
    }

    if (tx_flag < 0) {
        fprintf(stderr, "Must specify -tx or -rx\n");
        print_usage(argv[0]); return 1;
    }

    signal(SIGINT,  handle_sigint);
    signal(SIGTERM, handle_sigint);

    io_stdio_init();   /* unbuffer stdout for realtime decoded output */
    modem_init(mode);

    if (tx_flag) {
        const char *src = wav_path ? wav_path : "live audio";
        fprintf(stderr, "[STANAG 4285 TX] mode=%s  output=%s\n",
                argv[0], src);
        if (wav_path) run_tx_wav(wav_path);
        else          run_tx_live();
    } else {
        const char *src = wav_path ? wav_path : "live audio";
        fprintf(stderr, "[STANAG 4285 RX] mode=%s  input=%s\n",
                argv[0], src);
        if (wav_path) run_rx_wav(wav_path);
        else          run_rx_live();
    }

    return 0;
}
