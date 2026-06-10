/*
 * STANAG 4285 standalone modem — encode/decode one mode through audio.
 *
 * Usage:
 *   stanag4285 -tx [-mode <MODE>] [-wav <outfile.wav>]
 *   stanag4285 -rx [-mode <MODE>] [-wav <infile.wav>]
 *
 * TX: reads raw bytes from stdin, encodes, plays via speaker or writes WAV.
 * RX: decodes audio from mic or WAV file, writes raw bytes to stdout.
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
#include <fcntl.h>

#include "general.h"
#include "equalize.h"

/* ------------------------------------------------------------------ */
/* Externs from DSP modules                                            */
extern int            tx_sample_count;
extern unsigned short tx_samples[];
extern int            con_tx_octets_per_block;
extern int            con_rx_octets_per_block;
extern int            con_tx_flush_octets;

/* ------------------------------------------------------------------ */
static volatile int g_running = 1;

/* ------------------------------------------------------------------ */
/* TX audio ring buffer                                                */
#define TX_RING_SIZE (SAMPLE_BLOCK_SIZE * 128)
static short        g_tx_ring[TX_RING_SIZE];
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
/* RX audio ring buffer                                                */
#define RX_RING_SIZE (SAMPLE_BLOCK_SIZE * 128)
static short        g_rx_ring[RX_RING_SIZE];
static volatile int g_rx_head = 0;
static volatile int g_rx_tail = 0;

static inline int rxring_avail(void) {
    return (g_rx_head - g_rx_tail + RX_RING_SIZE) % RX_RING_SIZE;
}
static inline void rxring_write(const short *src, int n) {
    int space = RX_RING_SIZE - rxring_avail() - 1;
    if (n > space) n = space;
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

    if (!strcmp(buf, "75n"))   return B75N;
    if (!strcmp(buf, "75s"))   return B75S;
    if (!strcmp(buf, "75l"))   return B75L;
    if (!strcmp(buf, "150n"))  return B150N;
    if (!strcmp(buf, "150s"))  return B150S;
    if (!strcmp(buf, "150l"))  return B150L;
    if (!strcmp(buf, "300n"))  return B300N;
    if (!strcmp(buf, "300s"))  return B300S;
    if (!strcmp(buf, "300l"))  return B300L;
    if (!strcmp(buf, "600n"))  return B600N;
    if (!strcmp(buf, "600s"))  return B600S;
    if (!strcmp(buf, "600l"))  return B600L;
    if (!strcmp(buf, "1200n")) return B1200N;
    if (!strcmp(buf, "1200s")) return B1200S;
    if (!strcmp(buf, "1200l")) return B1200L;
    if (!strcmp(buf, "1200u")) return B1200U;
    if (!strcmp(buf, "2400n")) return B2400N;
    if (!strcmp(buf, "2400s")) return B2400S;
    if (!strcmp(buf, "2400l")) return B2400L;
    if (!strcmp(buf, "2400u")) return B2400U;
    if (!strcmp(buf, "3600u")) return B3600U;
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
    equalize_init();
    set_tx_mode(mode);
    set_rx_mode(mode);
}

/* ------------------------------------------------------------------ */
/* TX helpers                                                          */

/* Encode one block-worth of octets into tx_samples[].
   Pads with zeros if fewer than con_tx_octets_per_block bytes are
   available (keeps the frame structure intact).                       */
static void tx_encode_block(const unsigned char *data, int n)
{
    unsigned char buf[MAX_PACKET_LENGTH];
    int           pad = con_tx_octets_per_block;

    memset(buf, 0, pad);
    if (n > pad) n = pad;
    memcpy(buf, data, n);

    tx_sample_count = 0;
    for (int i = 0; i < pad; i++)
        tx_octet(buf[i]);
}

/* ------------------------------------------------------------------ */
/* Live TX loop                                                        */
static void run_tx_live(void)
{
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format    = ma_format_s16;
    cfg.playback.channels  = 1;
    cfg.sampleRate         = SAMPLE_RATE;
    cfg.dataCallback       = playback_callback;
    cfg.periodSizeInFrames = SAMPLE_BLOCK_SIZE;

    ma_device dev;
    if (ma_device_init(NULL, &cfg, &dev) != MA_SUCCESS) {
        fprintf(stderr, "Failed to open playback device\n"); exit(1);
    }

    /* Pre-buffer silence */
    {
        short silence[SAMPLE_BLOCK_SIZE * 2];
        memset(silence, 0, sizeof(silence));
        txring_write(silence, SAMPLE_BLOCK_SIZE * 2);
    }

    if (ma_device_start(&dev) != MA_SUCCESS) {
        fprintf(stderr, "Failed to start playback device\n"); exit(1);
    }

    /* Non-blocking stdin */
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    unsigned char buf[4096];
    while (g_running) {
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n > 0) {
            /* Encode in block-sized chunks */
            for (ssize_t off = 0; off < n; off += con_tx_octets_per_block) {
                int chunk = (int)(n - off);
                while (txring_space() < SAMPLE_BLOCK_SIZE * 2)
                    ma_sleep(1);
                tx_encode_block(buf + off, chunk);
                txring_write((short *)tx_samples, tx_sample_count);
            }
        } else {
            ma_sleep(10);
        }
    }

    while (txring_avail() > 0) ma_sleep(5);
    ma_sleep(500);
    ma_device_uninit(&dev);
}

/* WAV TX                                                              */
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

    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        for (ssize_t off = 0; off < n; off += con_tx_octets_per_block) {
            int chunk = (int)(n - off);
            tx_encode_block(buf + off, chunk);
            ma_encoder_write_pcm_frames(&enc, tx_samples, tx_sample_count, NULL);
        }
    }

    /* Flush: a few silent blocks so the receiver can drain */
    {
        short silence[SAMPLE_BLOCK_SIZE];
        memset(silence, 0, sizeof(silence));
        for (int i = 0; i < 8; i++)
            ma_encoder_write_pcm_frames(&enc, silence, SAMPLE_BLOCK_SIZE, NULL);
    }

    ma_encoder_uninit(&enc);
}

/* ------------------------------------------------------------------ */
/* Live RX loop                                                        */
static void run_rx_live(void)
{
    ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
    cfg.capture.format    = ma_format_s16;
    cfg.capture.channels  = 1;
    cfg.sampleRate        = SAMPLE_RATE;
    cfg.dataCallback      = capture_callback;
    cfg.periodSizeInFrames = SAMPLE_BLOCK_SIZE;

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
    }

    ma_device_uninit(&dev);
}

/* WAV RX                                                              */
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
    ma_uint64      frames_read;
    while (g_running) {
        ma_result r = ma_decoder_read_pcm_frames(
            &dec, block, SAMPLE_BLOCK_SIZE, &frames_read);
        if (r != MA_SUCCESS || frames_read == 0) break;
        if ((int)frames_read < SAMPLE_BLOCK_SIZE)
            memset(block + frames_read, 0,
                   (SAMPLE_BLOCK_SIZE - (int)frames_read) * sizeof(short));
        process_rx_block(block);
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
        else if (!strcmp(argv[i], "-mode") && i + 1 < argc)
            mode = parse_mode(argv[++i]);
        else if (!strcmp(argv[i], "-wav") && i + 1 < argc)
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

    setvbuf(stdout, NULL, _IONBF, 0);
    modem_init(mode);

    if (tx_flag) {
        fprintf(stderr, "[STANAG 4285 TX] mode=%s  output=%s\n",
                argv[0], wav_path ? wav_path : "live audio");
        if (wav_path) run_tx_wav(wav_path);
        else          run_tx_live();
    } else {
        fprintf(stderr, "[STANAG 4285 RX] mode=%s  input=%s\n",
                argv[0], wav_path ? wav_path : "live audio");
        if (wav_path) run_rx_wav(wav_path);
        else          run_rx_live();
    }

    return 0;
}
