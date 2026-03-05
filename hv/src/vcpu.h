#pragma once
#include <intrin.h>
#include "dbg.h"
#ifndef VCPU
#define VCPU
#include "ia32.h"
#include "ept.h"
#include <ntifs.h>

constexpr size_t vmm_stack_size = 0x6000;
constexpr uint32_t max_cpu_count = 64;

struct vcpu_t;
static vcpu_t* g_vcpus[ max_cpu_count ] = {};

typedef struct vcpu_t
{
	alignas( 0x1000 )vmx_msr_bitmap* msr_bitmap;
	uint64_t msr_bitmap_physical;
	uint64_t* vmm_stack;
	alignas( 0x1000 )vmcs_t* vmcs_region;
	alignas( 0x1000 )vmxon_t* vmxon_region;
	uint64_t vmcs_region_physical;
	uint64_t vmxon_region_physical;
	uint32_t queued_nmis;

	// Host-private GDT/IDT copies (isolation from guest modifications)
	uint8_t* host_gdt;
	uint8_t* host_idt;
	uint16_t host_gdt_limit;
	uint16_t host_idt_limit;

	// EPT (shared global, not per-core)
	uint64_t eptp;

	// per-vcpu cached values
	uint64_t feature_ctl_masked;
	uint16_t vpid;
};

namespace vcpu
{
	void set_msr_bitmap( vmx_msr_bitmap* bitmap, uint32_t msr, bool read, bool write )
	{
		if ( msr <= 0x1FFF )
		{
			if ( read ) bitmap->rdmsr_low[ msr / 8 ] |= ( 1 << ( msr % 8 ) );
			if ( write ) bitmap->wrmsr_low[ msr / 8 ] |= ( 1 << ( msr % 8 ) );
		}
		else if ( msr >= 0xC0000000 && msr <= 0xC0001FFF )
		{
			uint32_t idx = msr - 0xC0000000;
			if ( read ) bitmap->rdmsr_high[ idx / 8 ] |= ( 1 << ( idx % 8 ) );
			if ( write ) bitmap->wrmsr_high[ idx / 8 ] |= ( 1 << ( idx % 8 ) );
		}
	}

	vcpu_t* allocate_vcpu( )
	{
		vcpu_t* vcpu = ( vcpu_t* ) ExAllocatePool( NonPagedPool, sizeof( vcpu_t ) );
		PHYSICAL_ADDRESS phys;
		phys.QuadPart = MAXULONG64;

		vcpu->vmxon_region = (vmxon_t*)MmAllocateContiguousMemory( sizeof( vmxon_t ), phys );
		vcpu->vmcs_region = ( vmcs_t* ) MmAllocateContiguousMemory( sizeof( vmcs_t ), phys );

		if ( !vcpu->vmxon_region || !vcpu->vmcs_region )
			return nullptr;

		memset( vcpu->vmcs_region, 0x0, sizeof( vmcs_t ) );
		memset( vcpu->vmxon_region, 0x0, sizeof( vmxon_t ) );

		vcpu->vmcs_region_physical = ( uint64_t ) MmGetPhysicalAddress( vcpu->vmcs_region ).QuadPart;
		vcpu->vmxon_region_physical = ( uint64_t ) MmGetPhysicalAddress( vcpu->vmxon_region ).QuadPart;

		vcpu->msr_bitmap = ( vmx_msr_bitmap* ) MmAllocateContiguousMemory( 0x4000, phys );
		if ( !vcpu->msr_bitmap )
			return nullptr;

		memset( vcpu->msr_bitmap, 0x0, 0x4000 );

		// intercept FEATURE_CONTROL (hide VMX-outside-SMX) and EFER writes
		set_msr_bitmap( vcpu->msr_bitmap, IA32_FEATURE_CONTROL, true, true );
		set_msr_bitmap( vcpu->msr_bitmap, IA32_DEBUGCTL, false, true );
		set_msr_bitmap( vcpu->msr_bitmap, IA32_EFER, false, true );

		vcpu->msr_bitmap_physical = ( uint64_t ) MmGetPhysicalAddress( vcpu->msr_bitmap ).QuadPart;

		// mask out enable_vmx_outside_smx so guest doesn't see VMX active
		ia32_feature_control_register fc;
		fc.flags = __readmsr( IA32_FEATURE_CONTROL );
		fc.enable_vmx_outside_smx = 0;
		vcpu->feature_ctl_masked = fc.flags;


		uint32_t rev_id = g_msr_cache.initialized
			? g_msr_cache.vmx_basic.vmcs_revision_id
			: ( uint32_t ) ( __readmsr( IA32_VMX_BASIC ) & 0x7FFFFFFF );

		vcpu->vmcs_region->revision_id = rev_id;
		vcpu->vmxon_region->revision_id = rev_id;

		vcpu->vmm_stack = ( uint64_t* ) ExAllocatePool( NonPagedPool, vmm_stack_size );

		// Allocate host-private GDT/IDT (filled per-core in vmcs_setup_and_launch)
		vcpu->host_gdt = ( uint8_t* ) MmAllocateContiguousMemory( 0x1000, phys );
		vcpu->host_idt = ( uint8_t* ) MmAllocateContiguousMemory( 0x1000, phys );
		if ( !vcpu->host_gdt || !vcpu->host_idt )
			return nullptr;
		memset( vcpu->host_gdt, 0, 0x1000 );
		memset( vcpu->host_idt, 0, 0x1000 );

		// Use shared global EPT
		vcpu->eptp = ept::g_ept.eptp;

		return vcpu;
	}
}

#endif // VCPU
