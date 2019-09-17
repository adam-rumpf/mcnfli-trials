#pragma once
using namespace std;

class NetgenRandom
{
private:
	const long MULTIPLIER = 16807;
	const long MODULUS = 2147483647;
	long saved_seed;
public:
	NetgenRandom();
	NetgenRandom(long);
	void set_random(long);
	long random(long, long);
	long get_seed();
};