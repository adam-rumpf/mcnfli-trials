/*
Modified by Adam Rumpf 2017 to work in C++.
*/

/*** This is a portable random number generator whose origins are
*** unknown.  As far as can be told, this is public domain software.


/*** portable random number generator */

/*** Note that every variable used here must have at least 31 bits
*** of precision, exclusive of sign.  Long integers should be enough.
*** The generator is the congruential:  i = 7**5 * i mod (2^31-1).
***/

#include "NetgenRandom.h"
using namespace std;

/*** set_random - initialize constants and seed */

NetgenRandom::NetgenRandom()
{
	saved_seed = 0;
}

NetgenRandom::NetgenRandom(long seed)
{
	saved_seed = seed;
}

void NetgenRandom::set_random(long seed)
{
	saved_seed = seed;
}


/*** random - generate a random integer in the interval [a,b] (b >= a >= 0) */

long NetgenRandom::random(long a, long b)
{
	long hi, lo;

	hi = MULTIPLIER * (saved_seed >> 16);
	lo = MULTIPLIER * (saved_seed & 0xffff);
	hi += (lo >> 16);
	lo &= 0xffff;
	lo += (hi >> 15);
	hi &= 0x7fff;
	lo -= MODULUS;
	if ((saved_seed = (hi << 16) + lo) < 0)
		saved_seed += MODULUS;

	if (b <= a)
		return b;
	return a + saved_seed % (b - a + 1);
}

long NetgenRandom::get_seed()
{
	return saved_seed;
}