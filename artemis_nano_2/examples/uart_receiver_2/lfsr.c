#include <sys/types.h>


#define	PRBS_IV		0x02


static uint32_t prbs(void);
static uint32_t bitcount(uint32_t val);

static uint32_t lfsr;


static uint32_t
prbs(void)
{

	lfsr = (lfsr >> 1) ^ (-(lfsr & (uint32_t)1) &
	    (((uint32_t)1 << 30) | ((uint32_t)1 << 27)));
	
    return (~lfsr);
}

static uint32_t
bitcount(uint32_t val)
{

	val = val - ((val >> 1) & 0x55555555);
	val = (val & 0x33333333) + ((val >> 2) & 0x33333333);
	return ((((val + (val >> 4)) & 0xf0f0f0f) * 0x1010101) >> 24);
}