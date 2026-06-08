/*
 * Shared memory definition file.
 *
 */
#ifndef __SDEF_H__
#define __SDEF_H__ 

/*
 * Modes that can be signalled by the KISS interface.
 * Should be moved to another header file.
 *
 */
 
typedef enum{
	B75N,
	B75S,
	B75L,
	B150N,
	B150S,
	B150L,
	B300N,
	B300S,
	B300L,
	B600N,
	B600S,
	B600L,
	B600U,
	B1200N,
	B1200S,
	B1200L,
	B1200U,
	B1800U,
	B2400N,
	B2400S,
	B2400L,
	B2400U,
	B3600U
}Kmode;

typedef struct{
	float frequency_error;
	float signal_quality;
	float center_frequency;
	Kmode tx_mode;
	Kmode rx_mode;
	int   ptt;
	int   dcd;
	int   auto_baud;
}SmParams;

#ifdef __4285__
#define NAME            "/usr/sbin/4285"
#endif 

#ifdef __4529__
#define NAME            "/usr/sbin/4529"
#endif 

#define SM_SIZE         sizeof(SmParams)
#define PROJ_FTOK_SM    'A'


#endif
