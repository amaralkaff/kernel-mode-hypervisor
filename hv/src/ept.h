#pragma once
#ifndef EPT_H
#define EPT_H

#include "ia32.h"
#include "tools.h"
#include "dbg.h"
#include "structs.h"
#include <ntifs.h>

#ifndef MAXULONG64
#define MAXULONG64 0xffffffffffffffffULL
#endif

#define EPT_MAX_PDPT_ENTRIES 512
#define EPT_PD_ENTRIES       512

#ifndef IA32_MTRRCAP
#define IA32_MTRRCAP          0xFE
#endif
#define IA32_MTRR_DEF_TYPE_   0x2FF
#define IA32_MTRR_FIX64K      0x250
#define IA32_MTRR_FIX16K_80   0x258
#define IA32_MTRR_FIX16K_A0   0x259
#ifndef IA32_MTRR_FIX4K_BASE
#define IA32_MTRR_FIX4K_BASE  0x268
#endif
#ifndef IA32_MTRR_PHYSBASE0
#define IA32_MTRR_PHYSBASE0   0x200
#endif
#ifndef IA32_MTRR_PHYSMASK0
#define IA32_MTRR_PHYSMASK0   0x201
#endif

namespace ept
{
	struct mtrr_range
	{
		uint64_t phys_base;
		uint64_t phys_mask;
		uint64_t range_base;
		uint64_t range_end;
		uint8_t  type;
		bool     valid;
	};

	struct mtrr_state
	{
		uint8_t    default_type;
		bool       mtrr_enabled;
		bool       fixed_enabled;
		mtrr_range variable[20];
		uint32_t   variable_count;
	};

	inline mtrr_state g_mtrr = {};

	// parse hardware MTRRs (init only)
	inline void parse_mtrrs( )
	{
		uint64_t mtrrcap = __readmsr( IA32_MTRRCAP );
		uint32_t var_count = ( uint32_t ) ( mtrrcap & 0xFF );
		if ( var_count > 20 ) var_count = 20;

		uint64_t def_type_msr = __readmsr( IA32_MTRR_DEF_TYPE_ );
		g_mtrr.default_type = ( uint8_t ) ( def_type_msr & 0xFF );
		g_mtrr.mtrr_enabled = ( def_type_msr >> 11 ) & 1;
		g_mtrr.fixed_enabled = ( def_type_msr >> 10 ) & 1;
		g_mtrr.variable_count = var_count;

		if ( !g_mtrr.mtrr_enabled )
		{
			g_mtrr.default_type = MEMORY_TYPE_UNCACHEABLE;
			return;
		}

		for ( uint32_t i = 0; i < var_count; i++ )
		{
			uint64_t base_msr = __readmsr( IA32_MTRR_PHYSBASE0 + i * 2 );
			uint64_t mask_msr = __readmsr( IA32_MTRR_PHYSMASK0 + i * 2 );

			g_mtrr.variable[i].valid = ( mask_msr >> 11 ) & 1;
			if ( !g_mtrr.variable[i].valid )
				continue;

			g_mtrr.variable[i].type = ( uint8_t ) ( base_msr & 0xFF );
			g_mtrr.variable[i].phys_base = base_msr & ~0xFFFULL;
			g_mtrr.variable[i].phys_mask = mask_msr & ~0xFFFULL;

			uint64_t mask_bits = g_mtrr.variable[i].phys_mask;
			unsigned long lowest_bit;
			_BitScanForward64( &lowest_bit, mask_bits );
			uint64_t range_size = 1ULL << lowest_bit;

			g_mtrr.variable[i].range_base = g_mtrr.variable[i].phys_base;
			g_mtrr.variable[i].range_end = g_mtrr.variable[i].range_base + range_size - 1;
		}

		log::dbg_print( "mtrr: default_type=%d, var_count=%d, enabled=%d",
			g_mtrr.default_type, var_count, g_mtrr.mtrr_enabled );
		for ( uint32_t i = 0; i < var_count; i++ )
		{
			if ( !g_mtrr.variable[i].valid ) continue;
			log::dbg_print( "mtrr var[%d]: base=0x%llx end=0x%llx type=%d",
				i, g_mtrr.variable[i].range_base, g_mtrr.variable[i].range_end,
				g_mtrr.variable[i].type );
		}
	}

	// mtrr type for 2MB region — UC if mixed
	inline uint8_t get_mtrr_type_for_2mb( uint64_t phys_2mb_base )
	{
		if ( !g_mtrr.mtrr_enabled )
			return MEMORY_TYPE_UNCACHEABLE;

		uint64_t region_end = phys_2mb_base + ( 2ULL * 1024 * 1024 ) - 1;
		uint8_t result_type = 0xFF;
		bool found = false;

		for ( uint32_t i = 0; i < g_mtrr.variable_count; i++ )
		{
			if ( !g_mtrr.variable[i].valid )
				continue;

			uint64_t mtrr_base = g_mtrr.variable[i].range_base;
			uint64_t mtrr_end = g_mtrr.variable[i].range_end;

			if ( phys_2mb_base > mtrr_end || region_end < mtrr_base )
				continue;

			uint8_t mtrr_type = g_mtrr.variable[i].type;

			if ( mtrr_type == MEMORY_TYPE_UNCACHEABLE )
				return MEMORY_TYPE_UNCACHEABLE;

			if ( !found )
			{
				result_type = mtrr_type;
				found = true;
			}
			else if ( result_type != mtrr_type )
			{
				// WT + WB overlap -> WT
				if ( ( result_type == MEMORY_TYPE_WRITE_BACK && mtrr_type == MEMORY_TYPE_WRITE_THROUGH ) ||
					 ( result_type == MEMORY_TYPE_WRITE_THROUGH && mtrr_type == MEMORY_TYPE_WRITE_BACK ) )
				{
					result_type = MEMORY_TYPE_WRITE_THROUGH;
				}
				else
				{
					return MEMORY_TYPE_UNCACHEABLE;
				}
			}
		}

		if ( !found )
			return g_mtrr.default_type;

		return result_type;
	}

	struct ept_global
	{
		uint64_t* pml4;
		uint64_t* pdpt;
		uint64_t* pd[EPT_MAX_PDPT_ENTRIES];
		uint64_t  pml4_physical;
		uint64_t  eptp;
		bool      initialized;
	};

	inline ept_global g_ept = {};

	// per-core scratch pages — PTE remapping instead of MmMapIoSpace

	struct scratch_page
	{
		void* va;                   // page va
		volatile uint64_t* pte_va;  // pte for this page
		uint64_t orig_pte;          // original pte
	};

	inline scratch_page g_scratch[64] = {};
	inline bool g_scratch_ready = false;

	// walk cr3 tables to find PTE va (init only)
	inline uint64_t* find_pte_for_va_init( void* target_va )
	{
		uint64_t target = ( uint64_t ) target_va;
		uint64_t cr3 = __readcr3( ) & ~0xFFFULL;
		PHYSICAL_ADDRESS pa;

		// PML4
		pa.QuadPart = ( LONGLONG ) cr3;
		uint64_t* pml4 = ( uint64_t* ) MmMapIoSpace( pa, 0x1000, MmNonCached );
		if ( !pml4 ) return nullptr;
		uint64_t pml4e = pml4[( target >> 39 ) & 0x1FF];
		MmUnmapIoSpace( pml4, 0x1000 );
		if ( !( pml4e & 1 ) ) return nullptr;

		// PDPT
		pa.QuadPart = ( LONGLONG ) ( pml4e & 0xFFFFFFFFF000ULL );
		uint64_t* pdpt = ( uint64_t* ) MmMapIoSpace( pa, 0x1000, MmNonCached );
		if ( !pdpt ) return nullptr;
		uint64_t pdpte = pdpt[( target >> 30 ) & 0x1FF];
		MmUnmapIoSpace( pdpt, 0x1000 );
		if ( !( pdpte & 1 ) || ( pdpte & 0x80 ) ) return nullptr; // large page, no PTE

		// PD
		pa.QuadPart = ( LONGLONG ) ( pdpte & 0xFFFFFFFFF000ULL );
		uint64_t* pd = ( uint64_t* ) MmMapIoSpace( pa, 0x1000, MmNonCached );
		if ( !pd ) return nullptr;
		uint64_t pde = pd[( target >> 21 ) & 0x1FF];
		MmUnmapIoSpace( pd, 0x1000 );
		if ( !( pde & 1 ) || ( pde & 0x80 ) ) return nullptr; // large page, no PTE

		// permanent map for vmx root
		uint64_t pt_phys = pde & 0xFFFFFFFFF000ULL;
		pa.QuadPart = ( LONGLONG ) pt_phys;
		uint64_t* pt = ( uint64_t* ) MmMapIoSpace( pa, 0x1000, MmNonCached );
		if ( !pt ) return nullptr;

		uint32_t pt_idx = ( target >> 12 ) & 0x1FF;
		return &pt[pt_idx];
	}

	inline PMDL g_scratch_mdl[64] = {};

	// alloc scratch pages with guaranteed 4KB PTEs (call before virtualization)
	inline bool init_scratch_pages( uint32_t core_count )
	{
		if ( core_count > 64 ) core_count = 64;

		PHYSICAL_ADDRESS low, high, skip;
		low.QuadPart = 0;
		high.QuadPart = ~0ULL;
		skip.QuadPart = 0;

		for ( uint32_t i = 0; i < core_count; i++ )
		{
			PMDL mdl = MmAllocatePagesForMdl( low, high, skip, 0x1000 );
			if ( !mdl )
			{
				log::dbg_print( "scratch: MmAllocatePagesForMdl failed for core %u", i );
				return false;
			}

			void* va = MmMapLockedPagesSpecifyCache( mdl, KernelMode, MmNonCached, NULL, FALSE, NormalPagePriority );
			if ( !va )
			{
				MmFreePagesFromMdl( mdl );
				ExFreePool( mdl );
				log::dbg_print( "scratch: MmMapLockedPages failed for core %u", i );
				return false;
			}

			RtlZeroMemory( va, 0x1000 );

			volatile uint64_t* pte = ( volatile uint64_t* ) find_pte_for_va_init( va );
			if ( !pte )
			{
				MmUnmapLockedPages( va, mdl );
				MmFreePagesFromMdl( mdl );
				ExFreePool( mdl );
				log::dbg_print( "scratch: failed to find PTE for core %u", i );
				return false;
			}

			g_scratch_mdl[i] = mdl;
			g_scratch[i].va = va;
			g_scratch[i].pte_va = pte;
			g_scratch[i].orig_pte = *pte;
		}

		g_scratch_ready = true;
		log::dbg_print( "scratch: %u per-core pages ready", core_count );
		return true;
	}

	// remap scratch PTE to target phys page
	inline void* map_scratch( uint32_t core, uint64_t physical_address )
	{
		uint64_t phys_page = physical_address & ~0xFFFULL;
		uint64_t page_off  = physical_address & 0xFFF;

		// P|RW|PCD|A|D|NX
		uint64_t new_pte = ( phys_page & 0xFFFFFFFFF000ULL ) | 0x8000000000000073ULL;
		*g_scratch[core].pte_va = new_pte;
		__invlpg( g_scratch[core].va );

		return ( uint8_t* ) g_scratch[core].va + page_off;
	}

	inline void unmap_scratch( uint32_t core )
	{
		*g_scratch[core].pte_va = g_scratch[core].orig_pte;
		__invlpg( g_scratch[core].va );
	}

	inline uint64_t make_ept_entry( uint64_t phys_page, uint8_t rwx )
	{
		return ( phys_page & 0xFFFFFFFFF000ULL ) | ( rwx & 7 );
	}

	// 2MB large page entry
	inline uint64_t make_ept_pde_2mb( uint64_t phys_2mb, uint8_t mem_type )
	{
		return 0x7                                       // RWX
			| ( ( uint64_t ) ( mem_type & 7 ) << 3 )    // memory type
			| ( 1ULL << 7 )                              // large page
			| ( phys_2mb & 0xFFFFFFE00000ULL );          // 2MB-aligned PFN
	}

	inline bool init_ept( )
	{
		if ( g_ept.initialized )
			return true;

		PHYSICAL_ADDRESS phys_max;
		phys_max.QuadPart = MAXULONG64;

		// query physical address width from CPUID
		int regs[ 4 ];
		__cpuid( regs, 0x80000008 );
		uint32_t phys_bits = regs[ 0 ] & 0xFF;

		// use full physical address width — MMIO (GPU etc) lives above RAM
		if ( phys_bits > 48 ) phys_bits = 48; // EPT supports max 48-bit
		uint64_t max_physical = 1ULL << phys_bits;
		uint32_t gb_to_map = ( uint32_t ) ( max_physical >> 30 );
		if ( gb_to_map == 0 ) gb_to_map = 1;
		if ( gb_to_map > EPT_MAX_PDPT_ENTRIES ) gb_to_map = EPT_MAX_PDPT_ENTRIES;

		g_ept.pml4 = ( uint64_t* ) MmAllocateContiguousMemory( 0x1000, phys_max );
		g_ept.pdpt = ( uint64_t* ) MmAllocateContiguousMemory( 0x1000, phys_max );
		if ( !g_ept.pml4 || !g_ept.pdpt )
			return false;

		memset( g_ept.pml4, 0, 0x1000 );
		memset( g_ept.pdpt, 0, 0x1000 );

		g_ept.pml4_physical = MmGetPhysicalAddress( g_ept.pml4 ).QuadPart;
		uint64_t pdpt_physical = MmGetPhysicalAddress( g_ept.pdpt ).QuadPart;

		g_ept.pml4[0] = make_ept_entry( pdpt_physical, 7 );

		// simple identity map: 2MB large pages, WB everywhere (matches efi-hypervisor)
		for ( uint32_t i = 0; i < gb_to_map; i++ )
		{
			g_ept.pd[i] = ( uint64_t* ) MmAllocateContiguousMemory( 0x1000, phys_max );
			if ( !g_ept.pd[i] )
				return false;

			for ( uint32_t j = 0; j < EPT_PD_ENTRIES; j++ )
			{
				uint64_t phys_2mb = ( ( uint64_t ) i << 30 ) | ( ( uint64_t ) j << 21 );
				g_ept.pd[i][j] = make_ept_pde_2mb( phys_2mb, MEMORY_TYPE_WRITE_BACK );
			}

			uint64_t pd_physical = MmGetPhysicalAddress( g_ept.pd[i] ).QuadPart;
			g_ept.pdpt[i] = make_ept_entry( pd_physical, 7 );
		}

		g_ept.eptp = 6ULL | ( 3ULL << 3 ) | ( g_ept.pml4_physical & 0xFFFFFFFFF000ULL );
		g_ept.initialized = true;

		log::dbg_print( "ept: %u GB identity mapped, eptp=0x%llx", gb_to_map, g_ept.eptp );
		return true;
	}

	// read phys mem — scratch pages in vmx root, MmMapIoSpace fallback
	inline bool read_physical_memory( uint64_t physical_address, void* buffer, size_t size )
	{
		if ( !buffer || !size || size > 0x10000 || !physical_address )
			return false;

		if ( g_scratch_ready )
		{
			uint32_t core = ( uint32_t ) __readgsdword( 0x184 );
			if ( core >= 64 ) core = 0;
			if ( g_scratch[core].pte_va )
			{
				uint8_t* dst = ( uint8_t* ) buffer;
				size_t offset = 0;
				while ( offset < size )
				{
					uint64_t cur_phys = physical_address + offset;
					uint64_t page_off = cur_phys & 0xFFF;
					size_t chunk = min( ( size_t ) ( 0x1000 - page_off ), size - offset );
					void* mapped = map_scratch( core, cur_phys );
					memcpy( dst + offset, mapped, chunk );
					unmap_scratch( core );
					offset += chunk;
				}
				return true;
			}
		}

		// fallback
		PHYSICAL_ADDRESS phys;
		phys.QuadPart = ( LONGLONG ) physical_address;

		void* mapped = MmMapIoSpace( phys, size, MmNonCached );
		if ( !mapped )
			return false;

		memcpy( buffer, mapped, size );
		MmUnmapIoSpace( mapped, size );
		return true;
	}

	// translate guest va -> pa via cr3 page walk
	inline uint64_t translate_virtual_to_physical( uint64_t target_cr3, uint64_t virtual_address )
	{
		virt_addr_t va;
		va.value = virtual_address;

		uint64_t pml4_base = target_cr3 & ~0xFFFULL;

		pml4e pml4_entry = {};
		if ( !read_physical_memory( pml4_base + va.pml4e_index * sizeof( uint64_t ), &pml4_entry, sizeof( pml4_entry ) ) )
			return 0;
		if ( !pml4_entry.hard.present )
			return 0;

		uint64_t pdpt_base = pml4_entry.hard.pfn << 12;
		pdpte pdpt_entry = {};
		if ( !read_physical_memory( pdpt_base + va.pdpte_index * sizeof( uint64_t ), &pdpt_entry, sizeof( pdpt_entry ) ) )
			return 0;
		if ( !pdpt_entry.hard.present )
			return 0;

		if ( pdpt_entry.hard.page_size )
			return ( pdpt_entry.hard.pfn << 12 ) + ( virtual_address & 0x3FFFFFFF );

		uint64_t pd_base = pdpt_entry.hard.pfn << 12;
		pde pd_entry = {};
		if ( !read_physical_memory( pd_base + va.pde_index * sizeof( uint64_t ), &pd_entry, sizeof( pd_entry ) ) )
			return 0;
		if ( !pd_entry.hard.present )
			return 0;

		if ( pd_entry.hard.page_size )
			return ( pd_entry.hard.pfn << 12 ) + ( virtual_address & 0x1FFFFF );

		uint64_t pt_base = pd_entry.hard.pfn << 12;
		pte pt_entry = {};
		if ( !read_physical_memory( pt_base + va.pte_index * sizeof( uint64_t ), &pt_entry, sizeof( pt_entry ) ) )
			return 0;
		if ( !pt_entry.hard.present )
			return 0;

		return ( pt_entry.hard.pfn << 12 ) + va.offset;
	}

	// write phys mem — scratch pages in vmx root, MmMapIoSpace fallback
	inline bool write_physical_memory( uint64_t physical_address, const void* buffer, size_t size )
	{
		if ( !buffer || !size || size > 0x10000 || !physical_address )
			return false;

		if ( g_scratch_ready )
		{
			uint32_t core = ( uint32_t ) __readgsdword( 0x184 );
			if ( core >= 64 ) core = 0;
			if ( g_scratch[core].pte_va )
			{
				const uint8_t* src = ( const uint8_t* ) buffer;
				size_t offset = 0;
				while ( offset < size )
				{
					uint64_t cur_phys = physical_address + offset;
					uint64_t page_off = cur_phys & 0xFFF;
					size_t chunk = min( ( size_t ) ( 0x1000 - page_off ), size - offset );
					void* mapped = map_scratch( core, cur_phys );
					memcpy( mapped, src + offset, chunk );
					unmap_scratch( core );
					offset += chunk;
				}
				return true;
			}
		}

		// fallback
		PHYSICAL_ADDRESS phys;
		phys.QuadPart = ( LONGLONG ) physical_address;

		void* mapped = MmMapIoSpace( phys, size, MmNonCached );
		if ( !mapped )
			return false;

		memcpy( mapped, buffer, size );
		MmUnmapIoSpace( mapped, size );
		return true;
	}

	inline bool is_canonical( uint64_t addr )
	{
		return ( ( int64_t ) addr >> 47 ) == 0 || ( ( int64_t ) addr >> 47 ) == -1;
	}

	inline bool write_process_memory( uint64_t target_cr3, uint64_t dest_va, const void* src, size_t size )
	{
		if ( !target_cr3 || !dest_va || !src || !size || size > 0x10000 )
			return false;
		if ( !is_canonical( dest_va ) )
			return false;

		size_t bytes_written = 0;
		while ( bytes_written < size )
		{
			uint64_t current_va = dest_va + bytes_written;
			uint64_t page_offset = current_va & 0xFFF;
			size_t bytes_in_page = 0x1000 - page_offset;
			size_t bytes_to_write = min( bytes_in_page, size - bytes_written );

			uint64_t physical = translate_virtual_to_physical( target_cr3, current_va );
			if ( !physical )
				return false;

			if ( !write_physical_memory( physical, ( const uint8_t* ) src + bytes_written, bytes_to_write ) )
				return false;

			bytes_written += bytes_to_write;
		}

		return true;
	}

	inline bool read_process_memory( uint64_t target_cr3, uint64_t source_va, void* dest, size_t size )
	{
		if ( !target_cr3 || !source_va || !dest || !size || size > 0x10000 )
			return false;
		if ( !is_canonical( source_va ) )
			return false;

		size_t bytes_read = 0;
		while ( bytes_read < size )
		{
			uint64_t current_va = source_va + bytes_read;
			uint64_t page_offset = current_va & 0xFFF;
			size_t bytes_in_page = 0x1000 - page_offset;
			size_t bytes_to_read = min( bytes_in_page, size - bytes_read );

			uint64_t physical = translate_virtual_to_physical( target_cr3, current_va );
			if ( !physical )
				return false;

			if ( !read_physical_memory( physical, ( uint8_t* ) dest + bytes_read, bytes_to_read ) )
				return false;

			bytes_read += bytes_to_read;
		}

		return true;
	}
}

#endif // EPT_H
