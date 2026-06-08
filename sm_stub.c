/*
 * Shared memory stub.
 * Replaces the IPC shared-memory block with a plain static struct
 * so the modem works as a standalone process with no external controller.
 */
#include <stdio.h>
#include "general.h"
#include "sdef.h"

static SmParams _sm;
SmParams *sm_params = &_sm;

void sm_init(void)
{
    _sm.center_frequency = CENTER_FREQUENCY;
    _sm.ptt              = 0;
    _sm.dcd              = 0;
    _sm.auto_baud        = 0;
    sm_receive();
}
void sm_delete(void)   {}
void sm_transmit(void) { _sm.ptt = 1; }
void sm_receive(void)  { _sm.ptt = 0; }
void sm_dcd_on(void)   { _sm.dcd = 1; }
void sm_dcd_off(void)  { _sm.dcd = 0; }

void sm_update_rxmode(Kmode mode)          { _sm.rx_mode = mode; }
void sm_update_txmode(Kmode mode)          { _sm.tx_mode = mode; }
void sm_update_frequency_error(float err)  { _sm.frequency_error = err; }
void sm_update_signal_quality(float q)     { _sm.signal_quality = q; }
void sm_poll_parameter_update(void)        {}  /* no external controller */
