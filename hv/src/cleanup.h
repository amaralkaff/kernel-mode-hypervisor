#pragma once
#include <ntifs.h>
#include <ntimage.h>
#include <cstdint>

namespace cleanup
{
    typedef struct _PIDDB_CACHE_ENTRY
    {
        LIST_ENTRY List;
        UNICODE_STRING DriverName;
        ULONG TimeDateStamp;
        NTSTATUS LoadStatus;
        char _pad[16];
    } PIDDB_CACHE_ENTRY, *PPIDDB_CACHE_ENTRY;

    typedef struct _MM_UNLOADED_DRIVER
    {
        UNICODE_STRING Name;
        PVOID ModuleStart;
        PVOID ModuleEnd;
        LARGE_INTEGER UnloadTime;
    } MM_UNLOADED_DRIVER, *PMM_UNLOADED_DRIVER;

    inline size_t get_image_size( uint64_t base )
    {
        auto dos = ( IMAGE_DOS_HEADER* ) base;
        if ( dos->e_magic != IMAGE_DOS_SIGNATURE ) return 0;
        auto nt = ( IMAGE_NT_HEADERS64* ) ( base + dos->e_lfanew );
        if ( nt->Signature != IMAGE_NT_SIGNATURE ) return 0;
        return nt->OptionalHeader.SizeOfImage;
    }

    // x=match, ?=wildcard
    inline uint8_t* find_pattern( uint8_t* base, size_t size, const uint8_t* sig, const char* mask )
    {
        size_t pat_len = 0;
        while ( mask[pat_len] ) pat_len++;
        if ( pat_len == 0 || size < pat_len ) return nullptr;

        for ( size_t i = 0; i <= size - pat_len; i++ )
        {
            if ( base[i] != sig[0] ) continue; // quick reject on first byte
            bool match = true;
            for ( size_t j = 1; j < pat_len; j++ )
            {
                if ( mask[j] == 'x' && base[i + j] != sig[j] )
                {
                    match = false;
                    break;
                }
            }
            if ( match ) return &base[i];
        }
        return nullptr;
    }

    inline uint64_t resolve_rip( uint8_t* instr, int disp_offset, int instr_len )
    {
        int32_t rel = *( int32_t* ) ( instr + disp_offset );
        return ( uint64_t ) ( instr + instr_len + rel );
    }

    struct resolved_ptrs
    {
        PRTL_AVL_TABLE piddb_table;
        PERESOURCE piddb_lock;
        PMM_UNLOADED_DRIVER* mm_drivers;
        PULONG mm_last;
        bool piddb_found;
        bool mm_found;
    };

    inline bool find_piddb( uint8_t* nt_base, size_t nt_size, PRTL_AVL_TABLE* out_table, PERESOURCE* out_lock )
    {
        // lea rcx,[PiDDBCacheTable]; call PiLookupInDDBCache; cmp eax,0x8708
        static const uint8_t piddb_sig[] = {
            0x48, 0x8D, 0x0D, 0x00, 0x00, 0x00, 0x00,
            0xE8, 0x00, 0x00, 0x00, 0x00,
            0x3D, 0x08, 0x87, 0x00, 0x00
        };
        static const char piddb_mask[] = "xxx????x????xxxxx";

        uint8_t* match = find_pattern( nt_base, nt_size, piddb_sig, piddb_mask );
        if ( !match ) return false;

        *out_table = ( PRTL_AVL_TABLE ) resolve_rip( match, 3, 7 );

        // search backward for PiDDBLock
        *out_lock = nullptr;
        size_t max_back = ( size_t ) ( match - nt_base );
        if ( max_back > 50 ) max_back = 50;

        for ( size_t i = 3; i <= max_back; i++ )
        {
            if ( match[-( int ) i] == 0x48 && match[-( int ) i + 1] == 0x8D && match[-( int ) i + 2] == 0x0D )
            {
                uint64_t candidate = resolve_rip( match - ( int ) i, 3, 7 );
                if ( candidate > ( uint64_t ) nt_base && candidate < ( uint64_t ) nt_base + nt_size )
                {
                    *out_lock = ( PERESOURCE ) candidate;
                    break;
                }
            }
        }

        return ( *out_lock != nullptr );
    }

    inline bool find_mm_unloaded( uint8_t* nt_base, size_t nt_size, PMM_UNLOADED_DRIVER** out_drivers, PULONG* out_last )
    {
        // mov r10,[MmUnloadedDrivers]; mov r9,rcx; test r10,r10; jz
        static const uint8_t mm_sig[] = {
            0x4C, 0x8B, 0x15, 0x00, 0x00, 0x00, 0x00,
            0x4C, 0x8B, 0xC9,
            0x4D, 0x85, 0xD2,
            0x74
        };
        static const char mm_mask[] = "xxx????xxxxxxx";

        uint8_t* match = find_pattern( nt_base, nt_size, mm_sig, mm_mask );
        if ( !match ) return false;

        *out_drivers = ( PMM_UNLOADED_DRIVER* ) resolve_rip( match, 3, 7 );

        // search forward for MmLastUnloadedDriver
        *out_last = nullptr;
        size_t max_fwd = nt_size - ( size_t ) ( match - nt_base );
        if ( max_fwd > 128 ) max_fwd = 128;

        for ( size_t i = 0; i + 5 < max_fwd; i++ )
        {
            if ( match[i] == 0xFF && match[i + 1] == 0x05 )
            {
                *out_last = ( PULONG ) resolve_rip( match + i, 2, 6 );
                break;
            }
            if ( match[i] == 0x8B && ( match[i + 1] == 0x0D || match[i + 1] == 0x05 ) )
            {
                *out_last = ( PULONG ) resolve_rip( match + i, 2, 6 );
                break;
            }
        }

        return ( *out_last != nullptr );
    }

    inline resolved_ptrs resolve_all( uint64_t nt_base )
    {
        resolved_ptrs p = {};
        size_t nt_size = get_image_size( nt_base );
        if ( !nt_size ) return p;

        p.piddb_found = find_piddb( ( uint8_t* ) nt_base, nt_size, &p.piddb_table, &p.piddb_lock );
        p.mm_found = find_mm_unloaded( ( uint8_t* ) nt_base, nt_size, &p.mm_drivers, &p.mm_last );
        return p;
    }

    // kdu driver names to scrub
    static const wchar_t* vuln_drivers[] = {
        L"RTCore64.sys",       // KDU #1
        L"EneIo64.sys",        // KDU #6
        L"EneTechIo64.sys",    // KDU #8
        L"NalDrv.sys",         // KDU #0
        L"Gdrv.sys",           // KDU #2
        L"ATSZIO.sys",         // KDU #3
        L"MsIo64.sys",         // KDU #4
        L"GLCKIo2.sys",        // KDU #5
        L"WinRing0x64.sys",    // KDU #7
        L"phymemx64.sys",      // KDU #9
        L"rtkio64.sys",        // KDU #10
        L"AsrDrv106.sys",      // KDU #17
        L"PROCEXP152.sys",     // KDU victim driver
        L"NeacSafe64.sys",     // KDU #54
        L"ThrottleStop.sys",   // KDU #55
        L"TPwSav.sys",         // KDU #56
        L"dbk64.sys",          // KDU #14 (Cheat Engine)
        nullptr
    };

    inline void clean_traces( uint64_t nt_base )
    {
        resolved_ptrs p = resolve_all( nt_base );

        if ( p.piddb_found )
        {
            // non-blocking — skip if EAC holds it
            if ( !ExAcquireResourceExclusiveLite( p.piddb_lock, FALSE ) )
                goto skip_piddb;

            PVOID to_delete[32] = {};
            int del_count = 0;

            PVOID entry = RtlEnumerateGenericTableAvl( p.piddb_table, TRUE );
            while ( entry && del_count < 32 )
            {
                PPIDDB_CACHE_ENTRY cache_entry = ( PPIDDB_CACHE_ENTRY ) entry;
                for ( int i = 0; vuln_drivers[i]; i++ )
                {
                    UNICODE_STRING target;
                    RtlInitUnicodeString( &target, vuln_drivers[i] );
                    if ( RtlCompareUnicodeString( &cache_entry->DriverName, &target, TRUE ) == 0 )
                    {
                        to_delete[del_count++] = entry;
                        break;
                    }
                }
                entry = RtlEnumerateGenericTableAvl( p.piddb_table, FALSE );
            }

            for ( int i = 0; i < del_count; i++ )
                RtlDeleteElementGenericTableAvl( p.piddb_table, to_delete[i] );

            ExReleaseResourceLite( p.piddb_lock );
skip_piddb:;
        }

        if ( p.mm_found && *p.mm_drivers )
        {
            PMM_UNLOADED_DRIVER drivers = *p.mm_drivers;
            for ( ULONG i = 0; i < 50; i++ )
            {
                if ( !drivers[i].Name.Buffer ) continue;

                for ( int j = 0; vuln_drivers[j]; j++ )
                {
                    UNICODE_STRING target;
                    RtlInitUnicodeString( &target, vuln_drivers[j] );
                    if ( RtlCompareUnicodeString( &drivers[i].Name, &target, TRUE ) == 0 )
                    {
                        // Don't RtlFreeUnicodeString — these buffers aren't heap-allocated
                        // and freeing them causes BSOD. Just zero the entry.
                        drivers[i].Name.Length = 0;
                        drivers[i].Name.MaximumLength = 0;
                        drivers[i].Name.Buffer = nullptr;
                        RtlZeroMemory( &drivers[i].ModuleStart, sizeof( MM_UNLOADED_DRIVER ) - offsetof( MM_UNLOADED_DRIVER, ModuleStart ) );
                        break;
                    }
                }
            }
        }
    }
}
