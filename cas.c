/*
 * Channel access routines
 */
#include <stdlib.h>
#include "general.h"

extern int con_tx_octets_per_block;

typedef enum {
    CAS_DUPLEX,
    CAS_SIMPLEX_RX,
    CAS_SIMPLEX_TO_TX,
    CAS_SIMPLEX_TX,
    CAS_SIMPLEX_TO_RX
} CState;

static CState cstate;

int is_input_queuing_allowed(void)
{
    return (cstate != CAS_SIMPLEX_TO_RX);
}

int sound_samples_ready(void)
{
    int result = CAS_RECEIVE;
    static int count;

    switch (cstate)
    {
        case CAS_DUPLEX:
            if (get_input_queue_length() >= con_tx_octets_per_block)
                result = CAS_TRANSMIT;
            else
                if (is_simplex()) cstate = CAS_SIMPLEX_RX;
            break;

        case CAS_SIMPLEX_RX:
            if (get_input_queue_length() >= con_tx_octets_per_block) {
                count = (rand() & 0x1F) + 2;
                cstate = CAS_SIMPLEX_TO_TX;
            } else {
                if (is_duplex()) cstate = CAS_DUPLEX;
            }
            break;

        case CAS_SIMPLEX_TO_TX:
            if (count > 0) count--;
            if (is_dcd_detected()) {
                cstate = CAS_SIMPLEX_RX;
            } else {
                if (count <= 0) {
                    ptt_transmit();
                    convolutional_encode_reset();
                    cstate = CAS_SIMPLEX_TX;
                }
            }
            break;

        case CAS_SIMPLEX_TX:
            if (get_input_queue_length() >= con_tx_octets_per_block) {
                result = CAS_TRANSMIT;
            } else {
                tx_stuff_frame();
                cstate = CAS_SIMPLEX_TO_RX;
                result = CAS_TRANSMIT;
            }
            break;

        case CAS_SIMPLEX_TO_RX:
            if (get_input_queue_length() >= con_tx_octets_per_block) {
                result = CAS_TRANSMIT;
            } else {
                ptt_receive();
                cstate = CAS_SIMPLEX_RX;
            }
            break;

        default:
            break;
    }
    return result;
}
