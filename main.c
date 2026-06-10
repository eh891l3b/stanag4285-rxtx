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
 * Default: 600l
 */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <stdint.h>
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
/* Byte framing                                                        */
/* ------------------------------------------------------------------ */

typedef enum { FRAMING_NONE, FRAMING_8N1, FRAMING_5N1 } FramingMode;

static FramingMode g_framing = FRAMING_NONE;

/*
 * The modem's data path operates on whole bytes at a time via tx_octet()
 * on TX and output_data() on RX.  Framing works at the *bit* level, so
 * we need two thin shims:
 *
 *   TX: accumulate individual framing bits into a byte, flush via
 *       tx_octet() every 8 bits.  The bit stream flows transparently
 *       through the modem's convolutional encoder and interleaver just
 *       like any other data; STANAG frame boundaries are irrelevant.
 *
 *   RX: output_data() delivers packed bytes.  Unpack them MSB→LSB and
 *       feed each bit into the deframer state machine.
 */

/* ------------------------------------------------------------------ */
/* TX bit accumulator                                                  */
/* ------------------------------------------------------------------ */

static uint8_t  g_tx_bit_buf   = 0;
static int      g_tx_bit_count = 0;

/* Queue one bit into the TX pipeline. */
static void tx_bit(int bit)
{
    g_tx_bit_buf |= (uint8_t)((bit & 1) << g_tx_bit_count);
    if (++g_tx_bit_count == 8) {
        tx_octet(g_tx_bit_buf);
        g_tx_bit_buf   = 0;
        g_tx_bit_count = 0;
    }
}

/* Flush any partial byte with mark (1) padding — call at end of TX block. */
static void tx_bit_flush(void)
{
    while (g_tx_bit_count != 0)
        tx_bit(1);   /* idle/mark padding */
}

/* ------------------------------------------------------------------ */
/* ITA2 (Baudot / CCITT-2) tables                                     */
/* ------------------------------------------------------------------ */

#define ITA2_LTRS 0x1F
#define ITA2_FIGS 0x1B
#define ITA2_SP   0x04
#define ITA2_CR   0x08
#define ITA2_LF   0x02
#define ITA2_NONE 0xFF

/* ASCII → ITA2 code, LTRS plane (0xFF = not in this plane) */
static const uint8_t ita2_ascii_to_ltrs[128] = {
    /* 0-7   */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* 8-15  */ 0xFF,0xFF,ITA2_LF,0xFF,0xFF,ITA2_CR,0xFF,0xFF,
    /* 16-23 */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* 24-31 */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* 32 SP */ ITA2_SP,
    /* 33-47 */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* 48-57 '0'-'9' digits only in FIGS */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* 58-64 */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* 65-90 'A'-'Z' */
    0x03,0x19,0x0E,0x09,0x01,0x0D,0x1A,0x14,0x06,0x0B,
    0x0F,0x12,0x1C,0x0C,0x18,0x16,0x17,0x0A,0x05,0x10,
    0x07,0x1E,0x13,0x1D,0x15,0x11,
    /* 91-96 */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* 97-122 'a'-'z' same codes as uppercase */
    0x03,0x19,0x0E,0x09,0x01,0x0D,0x1A,0x14,0x06,0x0B,
    0x0F,0x12,0x1C,0x0C,0x18,0x16,0x17,0x0A,0x05,0x10,
    0x07,0x1E,0x13,0x1D,0x15,0x11,
    /* 123-127 */ 0xFF,0xFF,0xFF,0xFF,0xFF
};

/* ASCII → ITA2 code, FIGS plane */
static const uint8_t ita2_ascii_to_figs[128] = {
    /* 0-7   */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* 8-15  */ 0xFF,0xFF,ITA2_LF,0xFF,0xFF,ITA2_CR,0xFF,0xFF,
    /* 16-31 */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* 32 SP */ ITA2_SP,
    /* 33 !  */ 0xFF, /* 34 "  */ 0x11, /* 35 #  */ 0xFF,
    /* 36 $  */ 0xFF, /* 37 %  */ 0xFF, /* 38 &  */ 0xFF,
    /* 39 '  */ 0xFF, /* 40 (  */ 0x16, /* 41 )  */ 0x15,
    /* 42 *  */ 0xFF, /* 43 +  */ 0xFF, /* 44 ,  */ 0x0C,
    /* 45 -  */ 0x03, /* 46 .  */ 0x1C, /* 47 /  */ 0x1D,
    /* 48 0  */ 0x16, /* 49 1  */ 0x17, /* 50 2  */ 0x13,
    /* 51 3  */ 0x01, /* 52 4  */ 0x0A, /* 53 5  */ 0x10,
    /* 54 6  */ 0x15, /* 55 7  */ 0x07, /* 56 8  */ 0x06,
    /* 57 9  */ 0x18,
    /* 58 :  */ 0x0E, /* 59 ;  */ 0xFF, /* 60 <  */ 0xFF,
    /* 61 =  */ 0xFF, /* 62 >  */ 0xFF, /* 63 ?  */ 0x19,
    /* 64 @  */ 0xFF,
    /* 65-90 'A'-'Z' not in FIGS */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* 91-96 */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* 97-122 'a'-'z' not in FIGS */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* 123-127 */ 0xFF,0xFF,0xFF,0xFF,0xFF
};

/* ITA2 code → ASCII, LTRS plane ('\0' = non-printing / shift) */
static const uint8_t ita2_ltrs_to_ascii[32] = {
    '\0','E','\n','A',' ','S','I','U',
    '\r','D','R','J','N','F','C','K',
    'T','Z','L','W','H','Y','P','Q',
    'O','B','G','\0','M','X','V','\0'   /* 0x1B=FIGS, 0x1F=LTRS */
};

/* ITA2 code → ASCII, FIGS plane */
static const uint8_t ita2_figs_to_ascii[32] = {
    '\0','3','\n','-',' ','\'','8','7',
    '\r','\0','4','\0',',','!',':','(',
    '5','"',')','2','#','6','0','1',
    '9','?','&','\0','.','/',';','\0'
};

/* ------------------------------------------------------------------ */
/* TX framing                                                          */
/* ------------------------------------------------------------------ */

/* Send one 5-bit ITA2 code as a 5N1 async frame (7 bits total). */
static void tx_5n1_code(uint8_t code)
{
    tx_bit(0);                          /* start bit (space) */
    for (int b = 0; b < 5; b++)
        tx_bit((code >> b) & 1);        /* data bits, LSB first */
    tx_bit(1);                          /* stop bit (mark) */
}

/* Send one byte as an 8N1 async frame (10 bits total). */
static void tx_8n1_byte(unsigned char byte)
{
    tx_bit(0);                          /* start bit */
    for (int b = 0; b < 8; b++)
        tx_bit((byte >> b) & 1);        /* data bits, LSB first */
    tx_bit(1);                          /* stop bit */
}

/* Send one ASCII character as ITA2 5N1, emitting shift codes as needed.
 * Returns updated figs state (0=LTRS, 1=FIGS).
 * Unsupported characters are silently dropped.                        */
static int tx_ita2_char(unsigned char ch, int in_figs)
{
    if (ch >= 128) return in_figs;

    uint8_t lc = ita2_ascii_to_ltrs[ch];
    uint8_t fc = ita2_ascii_to_figs[ch];

    /* Plane-agnostic characters (SP, CR, LF share the same code) */
    if (lc != ITA2_NONE && lc == fc) {
        tx_5n1_code(lc);
        return in_figs;
    }

    if (!in_figs && lc != ITA2_NONE) { tx_5n1_code(lc); return 0; }
    if ( in_figs && fc != ITA2_NONE) { tx_5n1_code(fc); return 1; }

    /* Need a shift */
    if (lc != ITA2_NONE) { tx_5n1_code(ITA2_LTRS); tx_5n1_code(lc); return 0; }
    if (fc != ITA2_NONE) { tx_5n1_code(ITA2_FIGS); tx_5n1_code(fc); return 1; }

    return in_figs; /* unsupported — drop */
}

/* ------------------------------------------------------------------ */
/* RX deframing                                                        */
/* ------------------------------------------------------------------ */

/* Feed one decoded bit into the 8N1 state machine. */
static void rx_bit_8n1(int bit)
{
    static enum { S_IDLE, S_DATA, S_STOP } state = S_IDLE;
    static int     bit_count = 0;
    static uint8_t accum     = 0;

    switch (state) {
        case S_IDLE:
            if (bit == 0) { bit_count = 0; accum = 0; state = S_DATA; }
            break;
        case S_DATA:
            accum |= (uint8_t)(bit << bit_count);
            if (++bit_count == 8) state = S_STOP;
            break;
        case S_STOP:
            if (bit == 1) { fwrite(&accum, 1, 1, stdout); fflush(stdout); }
            state = S_IDLE;
            break;
    }
}

/* Feed one decoded bit into the 5N1 ITA2 state machine. */
static void rx_bit_5n1(int bit)
{
    static enum { S_IDLE, S_DATA, S_STOP } state = S_IDLE;
    static int     bit_count = 0;
    static uint8_t accum     = 0;
    static int     in_figs   = 0;

    switch (state) {
        case S_IDLE:
            if (bit == 0) { bit_count = 0; accum = 0; state = S_DATA; }
            break;
        case S_DATA:
            accum |= (uint8_t)(bit << bit_count);
            if (++bit_count == 5) state = S_STOP;
            break;
        case S_STOP:
            if (bit == 1) {
                if      (accum == ITA2_LTRS) { in_figs = 0; }
                else if (accum == ITA2_FIGS) { in_figs = 1; }
                else {
                    uint8_t ch = in_figs ? ita2_figs_to_ascii[accum]
                                         : ita2_ltrs_to_ascii[accum];
                    if (ch != '\0') { fwrite(&ch, 1, 1, stdout); fflush(stdout); }
                }
            }
            state = S_IDLE;
            break;
    }
}

/* Unpack a buffer of packed bytes into individual bits and feed them
 * to the chosen deframer.  output_data() delivers real bytes (8 bits
 * each), so we must unpack before the state machines can see them.   */
static void rx_deframe(unsigned char *data, int length)
{
    for (int i = 0; i < length; i++)
        for (int b = 0; b < 8; b++) {
            int bit = (data[i] >> b) & 1;
            if (g_framing == FRAMING_8N1) rx_bit_8n1(bit);
            else                          rx_bit_5n1(bit);
        }
}

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
    fprintf(stderr, "Unknown mode '%s', defaulting to 600l\n", s);
    return B600L;
}

static FramingMode parse_framing(const char *s)
{
    if (!strcmp(s, "none")) return FRAMING_NONE;
    if (!strcmp(s, "8n1"))  return FRAMING_8N1;
    if (!strcmp(s, "5n1"))  return FRAMING_5N1;
    fprintf(stderr, "Unknown framing '%s', defaulting to none\n", s);
    return FRAMING_NONE;
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s -tx [-mode <MODE>] [-wav <outfile.wav>] [-framing none|8n1|5n1]\n"
        "  %s -rx [-mode <MODE>] [-wav <infile.wav>]  [-framing none|8n1|5n1]\n"
        "\n"
        "Modes: 75n/s/l  150n/s/l  300n/s/l  600n/s/l\n"
        "       1200n/s/l/u  2400n/s/l/u  3600u\n"
        "Default mode: 600l\n"
        "\n"
        "Framing:\n"
        "  none   Raw bytes, no framing (default)\n"
        "  8n1    Async 8-bit: 1 start + 8 data (LSB-first) + 1 stop\n"
        "  5n1    ITA2/Baudot: 1 start + 5 data (LSB-first) + 1 stop\n"
        "         TX only sends supported characters; unsupported are dropped\n"
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

    if (g_framing == FRAMING_NONE) {
        for (int i = 0; i < pad; i++)
            tx_octet(buf[i]);
    } else if (g_framing == FRAMING_8N1) {
        for (int i = 0; i < n; i++)
            tx_8n1_byte(buf[i]);
        tx_bit_flush();
    } else { /* FRAMING_5N1 */
        static int figs_state = 0;
        for (int i = 0; i < n; i++)
            figs_state = tx_ita2_char(buf[i], figs_state);
        /* End block: LTRS shift + flush with mark padding */
        tx_5n1_code(ITA2_LTRS);
        figs_state = 0;
        tx_bit_flush();
    }
}

/* ------------------------------------------------------------------ */
/* RX output dispatcher                                                */
void output_data_framed(unsigned char *data, int length)
{
    if (g_framing == FRAMING_NONE) {
        fwrite(data, 1, length, stdout);
        fflush(stdout);
    } else {
        rx_deframe(data, length);
    }
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
    Kmode mode     = B600L;
    char *wav_path = NULL;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "-tx"))  tx_flag = 1;
        else if (!strcmp(argv[i], "-rx"))  tx_flag = 0;
        else if (!strcmp(argv[i], "-mode") && i + 1 < argc)
            mode = parse_mode(argv[++i]);
        else if (!strcmp(argv[i], "-wav") && i + 1 < argc)
            wav_path = argv[++i];
        else if (!strcmp(argv[i], "-framing") && i + 1 < argc)
            g_framing = parse_framing(argv[++i]);
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

    const char *framing_name = (g_framing == FRAMING_8N1) ? "8n1"
                             : (g_framing == FRAMING_5N1) ? "5n1" : "none";

    if (tx_flag) {
        fprintf(stderr, "[STANAG 4285 TX] mode=%s  framing=%s  output=%s\n",
                argv[0], framing_name, wav_path ? wav_path : "live audio");
        if (wav_path) run_tx_wav(wav_path);
        else          run_tx_live();
    } else {
        fprintf(stderr, "[STANAG 4285 RX] mode=%s  framing=%s  input=%s\n",
                argv[0], framing_name, wav_path ? wav_path : "live audio");
        if (wav_path) run_rx_wav(wav_path);
        else          run_rx_live();
    }

    return 0;
}
