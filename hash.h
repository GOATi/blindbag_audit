//------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------

inline uint64_t Fnv64a( const void* begin, const void* end, uint64_t offsetBasis = 0xcbf29ce484222325ULL )
{
	uint64_t prime = 0x100000001b3ULL;//magic prime
	uint64_t hash = offsetBasis;
	for( const uint8_t* b=(uint8_t*)begin; b != end; ++b )
	{
		hash ^= *b;
		hash *= prime;
	}
	return hash;
}
inline uint64_t Fnv64a( const void* begin, const size_t size, uint64_t offsetBasis = 0xcbf29ce484222325ULL )
{
	return Fnv64a(begin, ((uint8_t*)begin) + size, offsetBasis);
}
inline uint64_t Fnv64a( const char* text, uint64_t offsetBasis = 0xcbf29ce484222325ULL )
{
	uint64_t prime = 0x100000001b3ULL;//magic prime
	uint64_t hash = offsetBasis;
	if( text )
	{
		for( const uint8_t* b=(uint8_t*)text; *b; ++b )
		{
			hash ^= *b;
			hash *= prime;
		}
	}
	return hash;
}