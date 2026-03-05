#pragma once
#ifndef HV_MEM_H
#define HV_MEM_H

#include <intrin.h>
#include <cstdint>
#include <cstring>
#include <Windows.h>

#define HV_CPUID_PING          0x13370000
#define HV_CPUID_ATTACH        0x13370001
#define HV_CPUID_READ4         0x13370002
#define HV_CPUID_MODULE_BASE   0x13370003
#define HV_CPUID_DETACH        0x13370004
#define HV_CPUID_SET_ADDR_HIGH 0x13370005
#define HV_CPUID_READ8         0x13370006
#define HV_CPUID_READ_BUF      0x13370007
#define HV_CPUID_COPY_CHUNK    0x13370008
#define HV_CPUID_WRITE4        0x13370009
#define HV_CPUID_WRITE8        0x1337000A
#define HV_CPUID_WRITE8_DATA   0x1337000B
#define HV_CPUID_DSE           0x1337000E
#define HV_SIGNATURE           0x7276'6D68  // 'hvmr'

namespace hv
{
	// must match kernel-side fnv1a
	inline uint32_t fnv1a_hash( const wchar_t* str )
	{
		uint32_t hash = 0x811C9DC5;
		while ( *str )
		{
			wchar_t c = *str++;
			if ( c >= L'A' && c <= L'Z' )
				c += 32;
			hash ^= ( uint8_t ) c;
			hash *= 0x01000193;
			if ( c > 0xFF )
			{
				hash ^= ( uint8_t ) ( c >> 8 );
				hash *= 0x01000193;
			}
		}
		return hash;
	}

	inline bool ping( )
	{
		int regs[4] = {};
		__cpuidex( regs, HV_CPUID_PING, 0 );
		return ( uint32_t ) regs[1] == HV_SIGNATURE;
	}

	inline bool attach( uint32_t pid )
	{
		int regs[4] = {};
		__cpuidex( regs, HV_CPUID_ATTACH, ( int ) pid );
		return regs[0] == 1;
	}

	inline void detach( )
	{
		int regs[4] = {};
		__cpuidex( regs, HV_CPUID_DETACH, 0 );
	}

	inline void set_addr_high( uint32_t high32 )
	{
		int regs[4] = {};
		__cpuidex( regs, HV_CPUID_SET_ADDR_HIGH, ( int ) high32 );
	}

	inline uint32_t set_base( uint64_t base )
	{
		set_addr_high( ( uint32_t ) ( base >> 32 ) );
		return ( uint32_t ) ( base & 0xFFFFFFFF );
	}

	inline uint32_t read4( uint32_t addr_low32 )
	{
		int regs[4] = {};
		__cpuidex( regs, HV_CPUID_READ4, ( int ) addr_low32 );
		return ( uint32_t ) regs[0];
	}

	inline uint64_t read8( uint32_t addr_low32 )
	{
		int regs[4] = {};
		__cpuidex( regs, HV_CPUID_READ8, ( int ) addr_low32 );
		return ( uint64_t ) ( uint32_t ) regs[0] | ( ( uint64_t ) ( uint32_t ) regs[1] << 32 );
	}

	inline uint32_t read4_at( uint64_t address )
	{
		set_addr_high( ( uint32_t ) ( address >> 32 ) );
		return read4( ( uint32_t ) ( address & 0xFFFFFFFF ) );
	}

	inline uint64_t read8_at( uint64_t address )
	{
		set_addr_high( ( uint32_t ) ( address >> 32 ) );
		return read8( ( uint32_t ) ( address & 0xFFFFFFFF ) );
	}

	template<typename T>
	inline T read( uint64_t address )
	{
		static_assert( sizeof( T ) <= 8, "Use read_buffer for types larger than 8 bytes" );
		set_addr_high( ( uint32_t ) ( address >> 32 ) );
		if constexpr ( sizeof( T ) <= 4 )
		{
			uint32_t val = read4( ( uint32_t ) ( address & 0xFFFFFFFF ) );
			T result;
			memcpy( &result, &val, sizeof( T ) );
			return result;
		}
		else
		{
			uint64_t val = read8( ( uint32_t ) ( address & 0xFFFFFFFF ) );
			T result;
			memcpy( &result, &val, sizeof( T ) );
			return result;
		}
	}

	// read in 4K chunks, copy back 16 bytes at a time via COPY_CHUNK
	inline bool read_buffer( uint64_t address, void* buffer, size_t size )
	{
		if ( !buffer || size == 0 )
			return false;

		uint8_t* dst = ( uint8_t* ) buffer;
		size_t offset = 0;

		while ( offset < size )
		{
			size_t chunk_size = min( size - offset, ( size_t ) 0x1000 );
			uint64_t current_addr = address + offset;

			set_addr_high( ( uint32_t ) ( current_addr >> 32 ) );

			int regs[4] = {};
			__cpuidex( regs, HV_CPUID_READ_BUF, ( int ) ( uint32_t ) ( current_addr & 0xFFFFFFFF ) );

			uint32_t bytes_cached = ( uint32_t ) regs[0];
			if ( bytes_cached == 0 )
				return false;

			size_t to_copy = min( chunk_size, ( size_t ) bytes_cached );
			uint32_t num_chunks = ( uint32_t ) ( ( to_copy + 15 ) / 16 );

			for ( uint32_t i = 0; i < num_chunks; i++ )
			{
				int chunk_regs[4] = {};
				__cpuidex( chunk_regs, HV_CPUID_COPY_CHUNK, ( int ) i );

				size_t remaining = to_copy - ( size_t ) i * 16;
				size_t copy_len = min( remaining, ( size_t ) 16 );
				memcpy( dst + offset + ( size_t ) i * 16, chunk_regs, copy_len );
			}

			offset += to_copy;
		}

		return true;
	}

	inline void stage_value( uint32_t value )
	{
		int regs[4] = {};
		__cpuidex( regs, HV_CPUID_WRITE8_DATA, ( int ) value );
	}

	inline bool write4_at( uint64_t address, uint32_t value )
	{
		set_addr_high( ( uint32_t ) ( address >> 32 ) );
		stage_value( value );
		int regs[4] = {};
		__cpuidex( regs, HV_CPUID_WRITE4, ( int ) ( uint32_t ) ( address & 0xFFFFFFFF ) );
		return regs[0] == 1;
	}

	inline bool write8_at( uint64_t address, uint64_t value )
	{
		uint32_t low = ( uint32_t ) ( value & 0xFFFFFFFF );
		uint32_t high = ( uint32_t ) ( value >> 32 );

		if ( !write4_at( address, low ) )
			return false;
		return write4_at( address + 4, high );
	}

	template<typename T>
	inline bool write( uint64_t address, T value )
	{
		static_assert( sizeof( T ) <= 8, "Use write4_at in a loop for types larger than 8 bytes" );
		if constexpr ( sizeof( T ) <= 4 )
		{
			uint32_t raw = 0;
			memcpy( &raw, &value, sizeof( T ) );
			return write4_at( address, raw );
		}
		else
		{
			uint64_t raw = 0;
			memcpy( &raw, &value, sizeof( T ) );
			return write8_at( address, raw );
		}
	}

	inline uint64_t get_module_base( const wchar_t* module_name )
	{
		uint32_t hash = fnv1a_hash( module_name );
		int regs[4] = {};
		__cpuidex( regs, HV_CPUID_MODULE_BASE, ( int ) hash );
		return ( uint64_t ) ( uint32_t ) regs[0] | ( ( uint64_t ) ( uint32_t ) regs[1] << 32 );
	}

	inline uint64_t get_module_base_hash( uint32_t hash )
	{
		int regs[4] = {};
		__cpuidex( regs, HV_CPUID_MODULE_BASE, ( int ) hash );
		return ( uint64_t ) ( uint32_t ) regs[0] | ( ( uint64_t ) ( uint32_t ) regs[1] << 32 );
	}

	inline DWORD_PTR pin_to_core( uint32_t core = 0 )
	{
		return SetThreadAffinityMask( GetCurrentThread( ), 1ULL << core );
	}

	inline void unpin( DWORD_PTR old_mask )
	{
		SetThreadAffinityMask( GetCurrentThread( ), old_mask );
	}

	// disable DSE (CiOptions = 0)
	inline bool dse_disable( )
	{
		int regs[4] = {};
		__cpuidex( regs, HV_CPUID_DSE, 0 );
		return regs[0] == 1;
	}

	// restore DSE (CiOptions = 0x6)
	inline bool dse_restore( )
	{
		int regs[4] = {};
		__cpuidex( regs, HV_CPUID_DSE, 1 );
		return regs[0] == 1;
	}

	// query CiOptions value
	inline uint32_t dse_query( )
	{
		int regs[4] = {};
		__cpuidex( regs, HV_CPUID_DSE, 2 );
		return ( uint32_t ) regs[0];
	}
}

#endif // HV_MEM_H
