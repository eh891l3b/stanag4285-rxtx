/*
 * Modem status stubs.
 * Standalone mode: all status updates are no-ops.
 */
#include "general.h"

void sm_transmit( void )                       {}
void sm_receive( void )                        {}
void sm_dcd_on( void )                         {}
void sm_dcd_off( void )                        {}
void sm_update_rxmode( Kmode mode )            { (void)mode; }
void sm_update_txmode( Kmode mode )            { (void)mode; }
void sm_update_frequency_error( float err )    { (void)err; }
void sm_update_signal_quality( float q )       { (void)q; }
