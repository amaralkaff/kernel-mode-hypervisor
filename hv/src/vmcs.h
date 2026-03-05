#pragma once
#ifndef VMCS
#define VMCS

#include "tools.h"
#include "vcpu.h"
#include "segments.h"

extern "C" void vmwrite_rsp_rip( uint64_t rsp, uint64_t rip )
{
	tools::vmwrite( VMCS_GUEST_RSP, rsp );
	tools::vmwrite( VMCS_GUEST_RIP, rip );
}
extern "C" void restore_ctx( void );

namespace vmcs
{
	extern "C" void vmexit_handler( void );

	extern "C" bool vmcs_setup_and_launch( vcpu_t* vcpu, uint64_t guest_rsp )
	{
		// Use cached MSR values (read once at init, not per-core)
		auto& mc = g_msr_cache;

		// --- Guest CR state ---

		uint64_t guest_cr0 = __readcr0( );
		guest_cr0 |= mc.cr0_fixed0;
		guest_cr0 &= mc.cr0_fixed1;

		uint64_t guest_cr4 = __readcr4( );
		guest_cr4 |= mc.cr4_fixed0;
		guest_cr4 &= mc.cr4_fixed1;

		tools::vmwrite( VMCS_GUEST_CR0, guest_cr0 );
		tools::vmwrite( VMCS_GUEST_CR3, __readcr3( ) );
		tools::vmwrite( VMCS_GUEST_CR4, guest_cr4 );
		tools::vmwrite( VMCS_GUEST_RFLAGS, __readeflags( ) );

		// --- Guest segments ---

		segment_descriptor_register_64_t gdtr, idtr;
		segments::_sgdt( &gdtr );
		__sidt( &idtr );

		tools::vmwrite( VMCS_GUEST_CS_SELECTOR, segments::get_cs( ).flags );
		tools::vmwrite( VMCS_GUEST_SS_SELECTOR, segments::get_ss( ).flags );
		tools::vmwrite( VMCS_GUEST_DS_SELECTOR, segments::get_ds( ).flags );
		tools::vmwrite( VMCS_GUEST_ES_SELECTOR, segments::get_es( ).flags );
		tools::vmwrite( VMCS_GUEST_FS_SELECTOR, segments::get_fs( ).flags );
		tools::vmwrite( VMCS_GUEST_GS_SELECTOR, segments::get_gs( ).flags );
		tools::vmwrite( VMCS_GUEST_LDTR_SELECTOR, segments::get_ldtr( ).flags );
		tools::vmwrite( VMCS_GUEST_TR_SELECTOR, segments::get_tr( ).flags );

		tools::vmwrite( VMCS_GUEST_CS_BASE, segments::segment_base( gdtr, segments::get_cs( ) ) );
		tools::vmwrite( VMCS_GUEST_SS_BASE, segments::segment_base( gdtr, segments::get_ss( ) ) );
		tools::vmwrite( VMCS_GUEST_DS_BASE, segments::segment_base( gdtr, segments::get_ds( ) ) );
		tools::vmwrite( VMCS_GUEST_ES_BASE, segments::segment_base( gdtr, segments::get_es( ) ) );
		tools::vmwrite( VMCS_GUEST_FS_BASE, __readmsr( IA32_FS_BASE ) );
		tools::vmwrite( VMCS_GUEST_GS_BASE, __readmsr( IA32_GS_BASE ) );
		tools::vmwrite( VMCS_GUEST_LDTR_BASE, segments::segment_base( gdtr, segments::get_ldtr( ) ) );
		tools::vmwrite( VMCS_GUEST_TR_BASE, segments::segment_base( gdtr, segments::get_tr( ) ) );

		tools::vmwrite( VMCS_GUEST_CS_LIMIT, __segmentlimit( segments::get_cs( ).flags ) );
		tools::vmwrite( VMCS_GUEST_SS_LIMIT, __segmentlimit( segments::get_ss( ).flags ) );
		tools::vmwrite( VMCS_GUEST_DS_LIMIT, __segmentlimit( segments::get_ds( ).flags ) );
		tools::vmwrite( VMCS_GUEST_ES_LIMIT, __segmentlimit( segments::get_es( ).flags ) );
		tools::vmwrite( VMCS_GUEST_FS_LIMIT, __segmentlimit( segments::get_fs( ).flags ) );
		tools::vmwrite( VMCS_GUEST_GS_LIMIT, __segmentlimit( segments::get_gs( ).flags ) );
		tools::vmwrite( VMCS_GUEST_LDTR_LIMIT, __segmentlimit( segments::get_ldtr( ).flags ) );
		tools::vmwrite( VMCS_GUEST_TR_LIMIT, __segmentlimit( segments::get_tr( ).flags ) );

		tools::vmwrite( VMCS_GUEST_CS_ACCESS_RIGHTS, segments::segment_access( gdtr, segments::get_cs( ) ).flags );
		tools::vmwrite( VMCS_GUEST_SS_ACCESS_RIGHTS, segments::segment_access( gdtr, segments::get_ss( ) ).flags );
		tools::vmwrite( VMCS_GUEST_DS_ACCESS_RIGHTS, segments::segment_access( gdtr, segments::get_ds( ) ).flags );
		tools::vmwrite( VMCS_GUEST_ES_ACCESS_RIGHTS, segments::segment_access( gdtr, segments::get_es( ) ).flags );
		tools::vmwrite( VMCS_GUEST_FS_ACCESS_RIGHTS, segments::segment_access( gdtr, segments::get_fs( ) ).flags );
		tools::vmwrite( VMCS_GUEST_GS_ACCESS_RIGHTS, segments::segment_access( gdtr, segments::get_gs( ) ).flags );
		tools::vmwrite( VMCS_GUEST_LDTR_ACCESS_RIGHTS, segments::segment_access( gdtr, segments::get_ldtr( ) ).flags );
		tools::vmwrite( VMCS_GUEST_TR_ACCESS_RIGHTS, segments::segment_access( gdtr, segments::get_tr( ) ).flags );

		tools::vmwrite( VMCS_GUEST_GDTR_BASE, gdtr.base_address );
		tools::vmwrite( VMCS_GUEST_GDTR_LIMIT, gdtr.limit );
		tools::vmwrite( VMCS_GUEST_IDTR_BASE, idtr.base_address );
		tools::vmwrite( VMCS_GUEST_IDTR_LIMIT, idtr.limit );

		// --- Guest non-register state ---

		tools::vmwrite( VMCS_GUEST_DEBUGCTL, __readmsr( IA32_DEBUGCTL ) );
		tools::vmwrite( VMCS_GUEST_SYSENTER_CS, __readmsr( IA32_SYSENTER_CS ) );
		tools::vmwrite( VMCS_GUEST_SYSENTER_ESP, __readmsr( IA32_SYSENTER_ESP ) );
		tools::vmwrite( VMCS_GUEST_SYSENTER_EIP, __readmsr( IA32_SYSENTER_EIP ) );
		tools::vmwrite( VMCS_GUEST_PAT, __readmsr( IA32_PAT ) );
		tools::vmwrite( VMCS_GUEST_EFER, __readmsr( IA32_EFER ) );
		tools::vmwrite( VMCS_GUEST_VMCS_LINK_POINTER, MAXULONG64 );
		tools::vmwrite( VMCS_GUEST_ACTIVITY_STATE, vmx_active );
		tools::vmwrite( VMCS_GUEST_INTERRUPTIBILITY_STATE, 0 );
		tools::vmwrite( VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS, 0 );

		// --- Host CR state ---

		uint64_t host_cr0 = __readcr0( );
		host_cr0 |= mc.cr0_fixed0;
		host_cr0 &= mc.cr0_fixed1;

		uint64_t host_cr4 = __readcr4( );
		host_cr4 |= mc.cr4_fixed0;
		host_cr4 &= mc.cr4_fixed1;

		tools::vmwrite( VMCS_HOST_CR0, host_cr0 );
		tools::vmwrite( VMCS_HOST_CR3, tools::get_kernel_cr3( ) );
		tools::vmwrite( VMCS_HOST_CR4, host_cr4 );

		// --- Host RSP/RIP ---

		auto rsp = ( ( uint64_t ) vcpu->vmm_stack + vmm_stack_size );
		rsp &= ~0xFULL;
		rsp -= 8;

		tools::vmwrite( VMCS_HOST_RSP, rsp );
		tools::vmwrite( VMCS_HOST_RIP, ( uint64_t ) vmexit_handler );

		// --- Host segments ---

		tools::vmwrite( VMCS_HOST_CS_SELECTOR, segments::get_cs( ).flags & 0xF8 );
		tools::vmwrite( VMCS_HOST_SS_SELECTOR, segments::get_ss( ).flags & 0xF8 );
		tools::vmwrite( VMCS_HOST_DS_SELECTOR, segments::get_ds( ).flags & 0xF8 );
		tools::vmwrite( VMCS_HOST_ES_SELECTOR, segments::get_es( ).flags & 0xF8 );
		tools::vmwrite( VMCS_HOST_FS_SELECTOR, segments::get_fs( ).flags & 0xF8 );
		tools::vmwrite( VMCS_HOST_GS_SELECTOR, segments::get_gs( ).flags & 0xF8 );
		tools::vmwrite( VMCS_HOST_TR_SELECTOR, segments::get_tr( ).flags & 0xF8 );

		tools::vmwrite( VMCS_HOST_FS_BASE, __readmsr( IA32_FS_BASE ) );
		tools::vmwrite( VMCS_HOST_GS_BASE, __readmsr( IA32_GS_BASE ) );
		tools::vmwrite( VMCS_HOST_TR_BASE, segments::segment_base( gdtr, segments::get_tr( ) ) );
		tools::vmwrite( VMCS_HOST_SYSENTER_CS, __readmsr( IA32_SYSENTER_CS ) );
		tools::vmwrite( VMCS_HOST_SYSENTER_EIP, __readmsr( IA32_SYSENTER_EIP ) );
		tools::vmwrite( VMCS_HOST_SYSENTER_ESP, __readmsr( IA32_SYSENTER_ESP ) );

		tools::vmwrite( VMCS_HOST_GDTR_BASE, gdtr.base_address );
		tools::vmwrite( VMCS_HOST_IDTR_BASE, idtr.base_address );

		ia32_pat_register pat;
		pat.flags = 0;
		pat.pa0 = MEMORY_TYPE_WRITE_BACK;
		pat.pa1 = MEMORY_TYPE_WRITE_THROUGH;
		pat.pa2 = MEMORY_TYPE_UNCACHEABLE_MINUS;
		pat.pa3 = MEMORY_TYPE_UNCACHEABLE;
		pat.pa4 = MEMORY_TYPE_WRITE_BACK;
		pat.pa5 = MEMORY_TYPE_WRITE_THROUGH;
		pat.pa6 = MEMORY_TYPE_UNCACHEABLE_MINUS;
		pat.pa7 = MEMORY_TYPE_UNCACHEABLE;
		tools::vmwrite( VMCS_HOST_PAT, pat.flags );
		tools::vmwrite( VMCS_HOST_EFER, __readmsr( IA32_EFER ) );

		// --- Control fields ---

		tools::vmwrite( VMCS_CTRL_TSC_OFFSET, 0 );

		// CR masks: intercept OSXSAVE (bit 18) only (match efi-hypervisor)
		tools::vmwrite( VMCS_CTRL_CR0_GUEST_HOST_MASK, 0 );
		tools::vmwrite( VMCS_CTRL_CR4_GUEST_HOST_MASK, ( 1ULL << 18 ) );
		tools::vmwrite( VMCS_CTRL_CR0_READ_SHADOW, guest_cr0 );
		tools::vmwrite( VMCS_CTRL_CR4_READ_SHADOW, guest_cr4 );

		tools::vmwrite( VMCS_CTRL_PAGEFAULT_ERROR_CODE_MASK, 0 );
		tools::vmwrite( VMCS_CTRL_PAGEFAULT_ERROR_CODE_MATCH, 0 );
		tools::vmwrite( VMCS_CTRL_EXCEPTION_BITMAP, 0 );
		tools::vmwrite( VMCS_CTRL_VMEXIT_MSR_STORE_COUNT, 0 );
		tools::vmwrite( VMCS_CTRL_VMEXIT_MSR_LOAD_COUNT, 0 );
		tools::vmwrite( VMCS_CTRL_VMEXIT_MSR_LOAD_ADDRESS, 0 );
		tools::vmwrite( VMCS_CTRL_VMENTRY_MSR_LOAD_COUNT, 0 );
		tools::vmwrite( VMCS_CTRL_VMENTRY_MSR_LOAD_ADDRESS, 0 );

		// Pin-based: defaults only (external interrupt + NMI exiting forced on by HW)
		ia32_vmx_pinbased_ctls_register pin = {};
		tools::vmwrite( VMCS_CTRL_PIN_BASED_VM_EXECUTION_CONTROLS,
			adjust_controls( pin.flags, mc.pin_ctls_msr ) );

		// Primary proc-based (match efi-hypervisor: no TSC offset, no DR intercept)
		ia32_vmx_procbased_ctls_register proc = {};
		proc.use_msr_bitmaps = 1;
		proc.activate_secondary_controls = 1;
		tools::vmwrite( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS,
			adjust_controls( proc.flags, mc.proc_ctls_msr ) );

		tools::vmwrite( VMCS_CTRL_MSR_BITMAP_ADDRESS, vcpu->msr_bitmap_physical );

		// Secondary proc-based
		ia32_vmx_procbased_ctls2_register proc2 = {};
		proc2.enable_rdtscp = 1;
		proc2.enable_xsaves = 1;
		proc2.enable_ept = 1;
		proc2.enable_vpid = 1;
		tools::vmwrite( VMCS_CTRL_SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS,
			adjust_controls( proc2.flags, IA32_VMX_PROCBASED_CTLS2 ) );

		tools::vmwrite( VMCS_CTRL_EPT_POINTER, vcpu->eptp );
		tools::vmwrite( VMCS_CTRL_VIRTUAL_PROCESSOR_IDENTIFIER, vcpu->vpid );

		// Exit controls — request PAT+EFER save/load, adjust_controls strips unsupported bits
		ia32_vmx_exit_ctls_register exit_ctls = {};
		exit_ctls.host_address_space_size = 1;
		exit_ctls.save_ia32_pat = 1;
		exit_ctls.load_ia32_pat = 1;
		exit_ctls.save_ia32_efer = 1;
		exit_ctls.load_ia32_efer = 1;
		tools::vmwrite( VMCS_CTRL_PRIMARY_VMEXIT_CONTROLS,
			adjust_controls( exit_ctls.flags, mc.exit_ctls_msr ) );

		// Entry controls — request PAT+EFER load
		ia32_vmx_entry_ctls_register entry_ctls = {};
		entry_ctls.ia32e_mode_guest = 1;
		entry_ctls.load_ia32_pat = 1;
		entry_ctls.load_ia32_efer = 1;
		tools::vmwrite( VMCS_CTRL_VMENTRY_CONTROLS,
			adjust_controls( entry_ctls.flags, mc.entry_ctls_msr ) );

		// --- Launch ---

		tools::vmwrite( VMCS_GUEST_RSP, guest_rsp );
		tools::vmwrite( VMCS_GUEST_RIP, ( uint64_t ) restore_ctx );

		__vmx_vmlaunch( );
		log::dbg_print( "vmlaunch error -> 0x%llx", tools::vmread( VMCS_VM_INSTRUCTION_ERROR ) );
		__vmx_off( );
		return true;
	}
}

#endif // !VMCS
