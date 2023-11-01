#include <sys/types.h>


#define	PRBS_IV		0x01


static uint32_t prbs(void);

static uint32_t lfsr;


static uint32_t
prbs(void)
{

	lfsr = (lfsr >> 1) ^ (-(lfsr & (uint32_t)1) &
	    (((uint32_t)1 << 30) | ((uint32_t)1 << 27)));
	
    return (~lfsr);
}