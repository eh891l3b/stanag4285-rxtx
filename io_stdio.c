/*
 * stdio I/O module — replaces psuedo.c
 *
 * TX data path:
 *   main() reads stdin → tx_frame() → b_tx_octet() → write_input_queue()
 *   → drained by tx_octet() in main's pump loop → audio out
 *
 * RX data path:
 *   process_rx_block() → output_data() → write_output_queue()
 *   → main() calls rx_frame() → b_rx_octet() → b_in_flag() → p_write_data()
 *   → fwrite(stdout)
 */
#include <stdio.h>
#include <unistd.h>
#include "general.h"

/*
 * io_stdio_init — call once at startup.
 * Sets stdout to fully unbuffered so decoded bytes appear immediately
 * even when piped or redirected.
 */
void io_stdio_init(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
}

/*
 * p_write_data — called by bstuff.c when a complete HDLC frame passes
 * its CRC check.  Write the raw payload bytes straight to stdout.
 */
void p_write_data(unsigned char *msg, int length)
{
    fwrite(msg, 1, length, stdout);
    fflush(stdout);
}

/* Stubs — not used in standalone mode */
void p_write(unsigned char *msg, int length)    { (void)msg; (void)length; }
void p_control(unsigned char *msg, int mp)      { (void)msg; (void)mp; }
int  p_init(char *device)                       { (void)device; return 0; }
void p_poll(void)                               {}
void flush_rx_to_stdout(void)                   {}
