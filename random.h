#pragma once
#include <math.h>
#include <stdlib.h>

class XorShift64M
{
public:
	explicit XorShift64M(uint64_t seed) : m_state(seed) {}
	uint64_t m_state;

	uint64_t operator()()
	{
		uint64_t result = m_state * 0xd989bcacc137dcd5ull;
		m_state ^= m_state >> 11;
		m_state ^= m_state << 31;
		m_state ^= m_state >> 18;
		return result;
	}

	double RandomFraction()
	{
		uint64_t r = (uint32_t)(operator()() & 0xFFFFFFFFULL);
		return r / double(0xFFFFFFFFULL);
	}
	int RandomRange( int min, int max )
	{
		double d = RandomFraction();
		return min + (int)round((max - min)*d);
	}
	float RandomRange( float min, float max )
	{
		double d = RandomFraction();
		return min + (float)((max - min)*d);
	}
	uint64_t RandomIndex( uint64_t count )
	{
		eiASSERT( count );
		if(!count)
			return 0;
		//reject any that are within sum of overflowing to avoid bias
		constexpr uint64_t u64_max = 0xFFFFFFFFFFFFFFFFULL;
		uint64_t repeats = u64_max / count;
		uint64_t end_valid = repeats * count;
		uint64_t r;
		do
		{
			r = operator()();
		}while( r >= end_valid );
		r %= count;
		return r;
	}
};
