#pragma once
#ifndef COMM_H
#define COMM_H

#include "ept.h"
#include "dbg.h"
#include <ntifs.h>

namespace comm
{
	// eprocess offsets (win10 19041+ / win11)
	namespace eproc
	{
		constexpr uint64_t dir_table    = 0x28;
		constexpr uint64_t unique_pid   = 0x440;
		constexpr uint64_t active_links = 0x448;
		constexpr uint64_t peb          = 0x550;
	}

	// cached at init before virtualization
	inline uint64_t g_system_cr3 = 0;
	inline uint64_t g_system_eprocess = 0;

	inline uint64_t g_target_cr3 = 0;
	inline uint32_t g_target_pid = 0;
	inline uint64_t g_target_peb = 0;

	// grabs system eprocess + cr3 so we can walk process list from vmx root
	inline bool cache_system_process( )
	{
		PEPROCESS sys = PsGetCurrentProcess( );
		if ( !sys )
			return false;

		g_system_eprocess = ( uint64_t ) sys;
		g_system_cr3 = *( uint64_t* ) ( g_system_eprocess + eproc::dir_table );

		log::dbg_print( "comm: system EPROCESS 0x%llx, CR3 0x%llx", g_system_eprocess, g_system_cr3 );
		return g_system_cr3 != 0;
	}

	// walk ActiveProcessLinks via physical mem to find eprocess by pid
	inline uint64_t find_eprocess_by_pid( uint32_t pid )
	{
		if ( !g_system_cr3 || !g_system_eprocess )
			return 0;

		uint64_t list_head = g_system_eprocess + eproc::active_links;
		uint64_t current_link = 0;
		if ( !ept::read_process_memory( g_system_cr3, list_head, &current_link, sizeof( current_link ) ) )
			return 0;

		for ( int i = 0; i < 1024 && current_link != list_head && current_link != 0; i++ )
		{
			uint64_t eprocess_va = current_link - eproc::active_links;
			uint64_t proc_pid = 0;
			if ( !ept::read_process_memory( g_system_cr3, eprocess_va + eproc::unique_pid, &proc_pid, sizeof( proc_pid ) ) )
				break;

			if ( ( uint32_t ) proc_pid == pid )
				return eprocess_va;

			uint64_t next_link = 0;
			if ( !ept::read_process_memory( g_system_cr3, current_link, &next_link, sizeof( next_link ) ) )
				break;
			current_link = next_link;
		}

		return 0;
	}

	// get cr3 by pid — tries phys walk first, falls back to PsLookup
	inline uint64_t get_process_cr3( uint32_t pid )
	{
		if ( g_system_cr3 )
		{
			uint64_t eprocess = find_eprocess_by_pid( pid );
			if ( eprocess )
			{
				uint64_t cr3 = 0;
				if ( ept::read_process_memory( g_system_cr3, eprocess + eproc::dir_table, &cr3, sizeof( cr3 ) ) && cr3 )
					return cr3;
			}
		}

		PEPROCESS process = nullptr;
		NTSTATUS status = PsLookupProcessByProcessId( ( HANDLE ) ( uint64_t ) pid, &process );
		if ( !NT_SUCCESS( status ) || !process )
			return 0;
		uint64_t cr3 = *( uint64_t* ) ( ( uint64_t ) process + eproc::dir_table );
		ObDereferenceObject( process );
		return cr3;
	}

	// attach to process — tries phys walk, falls back to PsLookup
	inline bool attach( uint32_t pid )
	{
		uint64_t cr3 = 0;
		uint64_t peb = 0;

		if ( g_system_cr3 )
		{
			uint64_t eprocess = find_eprocess_by_pid( pid );
			if ( eprocess )
			{
				ept::read_process_memory( g_system_cr3, eprocess + eproc::dir_table, &cr3, sizeof( cr3 ) );
				ept::read_process_memory( g_system_cr3, eprocess + eproc::peb, &peb, sizeof( peb ) );
			}
		}

		if ( !cr3 )
		{
			PEPROCESS process = nullptr;
			NTSTATUS status = PsLookupProcessByProcessId( ( HANDLE ) ( uint64_t ) pid, &process );
			if ( !NT_SUCCESS( status ) || !process )
			{
				log::dbg_print( "comm::attach failed -> pid %u", pid );
				return false;
			}
			cr3 = *( uint64_t* ) ( ( uint64_t ) process + eproc::dir_table );
			peb = *( uint64_t* ) ( ( uint64_t ) process + eproc::peb );
			ObDereferenceObject( process );
		}

		if ( !cr3 )
		{
			log::dbg_print( "comm::attach failed -> pid %u (CR3 is 0)", pid );
			return false;
		}

		g_target_cr3 = cr3;
		g_target_pid = pid;
		g_target_peb = peb;
		log::dbg_print( "comm::attach -> pid %u, cr3 0x%llx, peb 0x%llx", pid, cr3, peb );
		return true;
	}

	inline void detach( )
	{
		g_target_cr3 = 0;
		g_target_pid = 0;
		g_target_peb = 0;
	}

	inline bool read( uint64_t address, void* buffer, size_t size )
	{
		if ( !g_target_cr3 )
			return false;

		return ept::read_process_memory( g_target_cr3, address, buffer, size );
	}

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

	// walk PEB loader list to find module base by fnv1a hash
	inline uint64_t get_module_base( uint32_t name_hash )
	{
		if ( !g_target_cr3 || !g_target_peb )
			return 0;

		uint64_t peb_addr = g_target_peb;

		uint64_t ldr_addr = 0;
		if ( !ept::read_process_memory( g_target_cr3, peb_addr + 0x18, &ldr_addr, sizeof( ldr_addr ) ) )
			return 0;

		if ( !ldr_addr )
			return 0;

		uint64_t list_head = ldr_addr + 0x10; // InLoadOrderModuleList
		uint64_t list_entry = 0;
		if ( !ept::read_process_memory( g_target_cr3, list_head, &list_entry, sizeof( list_entry ) ) )
			return 0;

		for ( int i = 0; i < 256 && list_entry != list_head && list_entry != 0; i++ )
		{
			uint64_t dll_base = 0;
			if ( !ept::read_process_memory( g_target_cr3, list_entry + 0x30, &dll_base, sizeof( dll_base ) ) )
				break;

			uint16_t name_length = 0;
			uint64_t name_buffer = 0;
			if ( !ept::read_process_memory( g_target_cr3, list_entry + 0x58, &name_length, sizeof( name_length ) ) )
				break;
			if ( !ept::read_process_memory( g_target_cr3, list_entry + 0x60, &name_buffer, sizeof( name_buffer ) ) )
				break;

			if ( name_length > 0 && name_length < 512 && name_buffer )
			{
				wchar_t name_buf[256] = {};
				uint16_t read_len = min( name_length, ( uint16_t ) ( sizeof( name_buf ) - sizeof( wchar_t ) ) );

				if ( ept::read_process_memory( g_target_cr3, name_buffer, name_buf, read_len ) )
				{
					name_buf[read_len / sizeof( wchar_t )] = L'\0';

					if ( fnv1a_hash( name_buf ) == name_hash )
						return dll_base;
				}
			}

			uint64_t next = 0;
			if ( !ept::read_process_memory( g_target_cr3, list_entry, &next, sizeof( next ) ) )
				break;
			list_entry = next;
		}

		return 0;
	}
}

#endif // COMM_H
