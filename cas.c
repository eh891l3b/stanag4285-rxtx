/*
 * Channel access routines
 *
 */
#include <stdlib.h>
#include "general.h"

/* External variables */

extern int            con_tx_octets_per_block;
extern SmParams 	*sm_params;

/* Local variables */

typedef enum{CAS_DUPLEX,CAS_SIMPLEX_RX,CAS_SIMPLEX_TO_TX,CAS_SIMPLEX_TX,CAS_SIMPLEX_TO_RX}CState;

CState cstate;

/*
 * Time when queuing is permitted 
 *
 */
int is_input_queuing_allowed(void)
{
	if( cstate == CAS_SIMPLEX_TO_RX )
		return 0;
	else
		return 1;
}
int sound_samples_ready(void)
{
	int result;
	static int count;
	
	result = CAS_RECEIVE;
	
	switch(cstate)
	{
		case CAS_DUPLEX:
		    /* Duplex operation in force */
			if( get_input_queue_length() >= con_tx_octets_per_block ) 
				result = CAS_TRANSMIT;
			else 
				if( is_simplex() ) cstate = CAS_SIMPLEX_RX;			
			break;
		case CAS_SIMPLEX_RX:
		    /* Simplex receiving */
			if( get_input_queue_length() >= con_tx_octets_per_block )
			{ 
			    /* Set random counter */
				count = (rand()&0x1F)+2;
				cstate = CAS_SIMPLEX_TO_TX;
			}
			else
			{ 
				if( is_duplex() ) cstate = CAS_DUPLEX;			
		    }
			break;
		case CAS_SIMPLEX_TO_TX:
		    /* Checking channel for collisions */
			/* Decrement TX timeout */
			if( count > 0 ) count--;
		    if( is_dcd_detected() )
		    {
		    	cstate = CAS_SIMPLEX_RX;
		    }
		    else
		    {
		    	if( sm_params->auto_baud )
		    	{
		    		if( count == 1 )
		    		{ 
		    			ptt_transmit();
					}
					if( count <= 0 )
					{
						convolutional_encode_reset();
		    			cstate = CAS_SIMPLEX_TX;
			        	result = CAS_AUTOPROBE;				
					}
				}
				else
				{
					if( count <= 0 )
					{
		    			ptt_transmit();
						convolutional_encode_reset();
		    			cstate = CAS_SIMPLEX_TX;				
					}
				}
		    }
			break;
		case CAS_SIMPLEX_TX:
			if( get_input_queue_length() >= con_tx_octets_per_block )
			{
			    /* Transmit */ 
				result = CAS_TRANSMIT;
			}
			else
			{ 
					tx_stuff_frame();
					cstate = CAS_SIMPLEX_TO_RX;
					result = CAS_TRANSMIT;
		    }
			break;
		case CAS_SIMPLEX_TO_RX:
		    /* There is a hole in this part if another frame gets queued */
			if( get_input_queue_length() >= con_tx_octets_per_block )
			{
			    /* Transmit */ 
				result = CAS_TRANSMIT;
			}
			else
			{ 
					ptt_receive();
					cstate = CAS_SIMPLEX_RX;
		    }
			break;
		default:
			break;
	}
	return result;
}