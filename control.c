/*
 * Modem mode control — sets TX/RX rate, coding, and interleaving.
 * PTT, duplex/simplex, and auto-baud have been removed.
 */
#include "general.h"

extern int rx_mode;
extern int tx_mode;

int con_rx_octets_per_block;
int con_tx_octets_per_block;
int con_tx_flush_octets;
int con_bad_probe_threshold;

void set_tx_mode(int mode)
{
    switch (mode) {
    case B75N:
        interleaver_disable();
        tx_mode                 = TX_75_BPS;
        con_tx_octets_per_block = 1;
        con_tx_flush_octets     = 21;
        break;
    case B75S:
        interleaver_enable();
        interleaving_tx_600_75_short();
        tx_mode                 = TX_75_BPS;
        con_tx_octets_per_block = 1;
        con_tx_flush_octets     = 21;
        break;
    case B75L:
        interleaver_enable();
        interleaving_tx_600_75_long();
        tx_mode                 = TX_75_BPS;
        con_tx_octets_per_block = 1;
        con_tx_flush_octets     = 109;
        break;
    case B150N:
        interleaver_disable();
        tx_mode                 = TX_150_BPS;
        con_tx_octets_per_block = 2;
        con_tx_flush_octets     = 29;
        break;
    case B150S:
        interleaver_enable();
        interleaving_tx_600_75_short();
        tx_mode                 = TX_150_BPS;
        con_tx_octets_per_block = 2;
        con_tx_flush_octets     = 29;
        break;
    case B150L:
        interleaver_enable();
        interleaving_tx_600_75_long();
        tx_mode                 = TX_150_BPS;
        con_tx_octets_per_block = 2;
        con_tx_flush_octets     = 205;
        break;
    case B300N:
        interleaver_disable();
        tx_mode                 = TX_300_BPS;
        con_tx_octets_per_block = 4;
        con_tx_flush_octets     = 45;
        break;
    case B300S:
        interleaver_enable();
        interleaving_tx_600_75_short();
        tx_mode                 = TX_300_BPS;
        con_tx_octets_per_block = 4;
        con_tx_flush_octets     = 45;
        break;
    case B300L:
        interleaver_enable();
        interleaving_tx_600_75_long();
        tx_mode                 = TX_300_BPS;
        con_tx_octets_per_block = 4;
        con_tx_flush_octets     = 397;
        break;
    case B600N:
        interleaver_disable();
        tx_mode                 = TX_600_BPS;
        con_tx_octets_per_block = 8;
        con_tx_flush_octets     = 77;
        break;
    case B600S:
        interleaver_enable();
        interleaving_tx_600_75_short();
        tx_mode                 = TX_600_BPS;
        con_tx_octets_per_block = 8;
        con_tx_flush_octets     = 77;
        break;
    case B600L:
        interleaver_enable();
        interleaving_tx_600_75_long();
        tx_mode                 = TX_600_BPS;
        con_tx_octets_per_block = 8;
        con_tx_flush_octets     = 781;
        break;
    case B1200N:
        interleaver_disable();
        tx_mode                 = TX_1200_BPS;
        con_tx_octets_per_block = 16;
        con_tx_flush_octets     = 141;
        break;
    case B1200S:
        interleaver_enable();
        interleaving_tx_1200_short();
        tx_mode                 = TX_1200_BPS;
        con_tx_octets_per_block = 16;
        con_tx_flush_octets     = 141;
        break;
    case B1200L:
        interleaver_enable();
        interleaving_tx_1200_long();
        tx_mode                 = TX_1200_BPS;
        con_tx_octets_per_block = 16;
        con_tx_flush_octets     = 1549;
        break;
    case B1200U:
        tx_mode                 = TX_1200U_BPS;
        con_tx_octets_per_block = 16;
        con_tx_flush_octets     = 16;
        break;
    case B2400N:
        interleaver_disable();
        tx_mode                 = TX_2400_BPS;
        con_tx_octets_per_block = 32;
        con_tx_flush_octets     = 269;
        break;
    case B2400S:
        interleaver_enable();
        interleaving_tx_2400_short();
        tx_mode                 = TX_2400_BPS;
        con_tx_octets_per_block = 32;
        con_tx_flush_octets     = 269;
        break;
    case B2400L:
        interleaver_enable();
        interleaving_tx_2400_long();
        tx_mode                 = TX_2400_BPS;
        con_tx_octets_per_block = 32;
        con_tx_flush_octets     = 3085;
        break;
    case B2400U:
        tx_mode                 = TX_2400U_BPS;
        con_tx_octets_per_block = 32;
        con_tx_flush_octets     = 32;
        break;
    case B3600U:
        tx_mode                 = TX_3600U_BPS;
        con_tx_octets_per_block = 48;
        con_tx_flush_octets     = 48;
        break;
    default:
        break;
    }
}

void set_rx_mode(int mode)
{
    switch (mode) {
    case B75N:
        deinterleaver_disable();
        rx_mode                 = RX_75_BPS;
        con_rx_octets_per_block = 1;
        con_bad_probe_threshold = 1;
        break;
    case B75S:
        deinterleaver_enable();
        interleaving_rx_600_75_short();
        rx_mode                 = RX_75_BPS;
        con_rx_octets_per_block = 1;
        con_bad_probe_threshold = 5;
        viterbi_set_alternate_2_path_length();
        break;
    case B75L:
        deinterleaver_enable();
        interleaving_rx_600_75_long();
        rx_mode                 = RX_75_BPS;
        con_rx_octets_per_block = 1;
        con_bad_probe_threshold = 10;
        viterbi_set_normal_path_length();
        break;
    case B150N:
        deinterleaver_disable();
        rx_mode                 = RX_150_BPS;
        con_rx_octets_per_block = 1;
        con_bad_probe_threshold = 1;
        break;
    case B150S:
        deinterleaver_enable();
        interleaving_rx_600_75_short();
        rx_mode                 = RX_150_BPS;
        con_rx_octets_per_block = 2;
        con_bad_probe_threshold = 5;
        viterbi_set_alternate_1_path_length();
        break;
    case B150L:
        deinterleaver_enable();
        interleaving_rx_600_75_long();
        rx_mode                 = RX_150_BPS;
        con_rx_octets_per_block = 2;
        con_bad_probe_threshold = 10;
        viterbi_set_normal_path_length();
        break;
    case B300N:
        deinterleaver_disable();
        rx_mode                 = RX_300_BPS;
        con_rx_octets_per_block = 4;
        con_bad_probe_threshold = 1;
        break;
    case B300S:
        deinterleaver_enable();
        interleaving_rx_600_75_short();
        rx_mode                 = RX_300_BPS;
        con_rx_octets_per_block = 4;
        con_bad_probe_threshold = 2;
        viterbi_set_normal_path_length();
        break;
    case B300L:
        deinterleaver_enable();
        interleaving_rx_600_75_long();
        rx_mode                 = RX_300_BPS;
        con_rx_octets_per_block = 4;
        con_bad_probe_threshold = 10;
        viterbi_set_normal_path_length();
        break;
    case B600N:
        deinterleaver_disable();
        rx_mode                 = RX_600_BPS;
        con_rx_octets_per_block = 8;
        con_bad_probe_threshold = 1;
        break;
    case B600S:
        deinterleaver_enable();
        interleaving_rx_600_75_short();
        rx_mode                 = RX_600_BPS;
        con_rx_octets_per_block = 8;
        con_bad_probe_threshold = 2;
        viterbi_set_normal_path_length();
        break;
    case B600L:
        deinterleaver_enable();
        interleaving_rx_600_75_long();
        rx_mode                 = RX_600_BPS;
        con_rx_octets_per_block = 8;
        con_bad_probe_threshold = 10;
        viterbi_set_normal_path_length();
        break;
    case B1200N:
        deinterleaver_disable();
        rx_mode                 = RX_1200_BPS;
        con_rx_octets_per_block = 16;
        con_bad_probe_threshold = 1;
        break;
    case B1200S:
        deinterleaver_enable();
        interleaving_rx_1200_short();
        rx_mode                 = RX_1200_BPS;
        con_rx_octets_per_block = 16;
        con_bad_probe_threshold = 2;
        viterbi_set_normal_path_length();
        break;
    case B1200L:
        deinterleaver_enable();
        interleaving_rx_1200_long();
        rx_mode                 = RX_1200_BPS;
        con_rx_octets_per_block = 16;
        con_bad_probe_threshold = 10;
        viterbi_set_normal_path_length();
        break;
    case B1200U:
        rx_mode                 = RX_1200U_BPS;
        con_rx_octets_per_block = 16;
        con_bad_probe_threshold = 1;
        break;
    case B2400N:
        deinterleaver_disable();
        rx_mode                 = RX_2400_BPS;
        con_rx_octets_per_block = 32;
        con_bad_probe_threshold = 1;
        break;
    case B2400S:
        deinterleaver_enable();
        interleaving_rx_2400_short();
        rx_mode                 = RX_2400_BPS;
        con_rx_octets_per_block = 32;
        con_bad_probe_threshold = 2;
        viterbi_set_normal_path_length();
        break;
    case B2400L:
        deinterleaver_enable();
        interleaving_rx_2400_long();
        rx_mode                 = RX_2400_BPS;
        con_rx_octets_per_block = 32;
        con_bad_probe_threshold = 10;
        viterbi_set_normal_path_length();
        break;
    case B2400U:
        rx_mode                 = RX_2400U_BPS;
        con_rx_octets_per_block = 32;
        con_bad_probe_threshold = 1;
        break;
    case B3600U:
        rx_mode                 = RX_3600U_BPS;
        con_rx_octets_per_block = 48;
        con_bad_probe_threshold = 1;
        break;
    default:
        break;
    }
}
