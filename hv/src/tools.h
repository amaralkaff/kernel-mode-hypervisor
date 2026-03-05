#ifndef TOOLS_H
#define TOOLS_H
#include <ntifs.h>
#include <cstdint>
#include "ia32.h"
#include <intrin.h>
// Cached MSR values — read once at init, used on every VMExit
struct msr_cache_t
{
	ia32_vmx_basic_register vmx_basic;
	uint64_t cr0_fixed0;
	uint64_t cr0_fixed1;
	uint64_t cr4_fixed0;
	uint64_t cr4_fixed1;
	uint32_t pin_ctls_msr;
	uint32_t proc_ctls_msr;
	uint32_t exit_ctls_msr;
	uint32_t entry_ctls_msr;
	bool initialized;
};

inline msr_cache_t g_msr_cache = {};

inline void init_msr_cache( )
{
	g_msr_cache.vmx_basic.flags = __readmsr( IA32_VMX_BASIC );
	g_msr_cache.cr0_fixed0 = __readmsr( IA32_VMX_CR0_FIXED0 );
	g_msr_cache.cr0_fixed1 = __readmsr( IA32_VMX_CR0_FIXED1 );
	g_msr_cache.cr4_fixed0 = __readmsr( IA32_VMX_CR4_FIXED0 );
	g_msr_cache.cr4_fixed1 = __readmsr( IA32_VMX_CR4_FIXED1 );

	bool use_true = g_msr_cache.vmx_basic.vmx_controls != 0;
	g_msr_cache.pin_ctls_msr = use_true ? IA32_VMX_TRUE_PINBASED_CTLS : IA32_VMX_PINBASED_CTLS;
	g_msr_cache.proc_ctls_msr = use_true ? IA32_VMX_TRUE_PROCBASED_CTLS : IA32_VMX_PROCBASED_CTLS;
	g_msr_cache.exit_ctls_msr = use_true ? IA32_VMX_TRUE_EXIT_CTLS : IA32_VMX_EXIT_CTLS;
	g_msr_cache.entry_ctls_msr = use_true ? IA32_VMX_TRUE_ENTRY_CTLS : IA32_VMX_ENTRY_CTLS;
	g_msr_cache.initialized = true;
}

inline uint32_t adjust_controls( uint32_t controls, uint32_t msr )
{
	ia32_vmx_true_ctls_register caps;
	caps.flags = __readmsr( msr );
	controls |= caps.allowed_0_settings;
	controls &= caps.allowed_1_settings;
	return controls;
}

namespace tools
{
	extern "C" uint64_t get_nt_base( );

	void* get_system_routine( const wchar_t* routine_name )
	{
		UNICODE_STRING uc;
		RtlInitUnicodeString( &uc, routine_name );
		return MmGetSystemRoutineAddress( &uc );

	}

	bool is_vmx_supported( ) {
		int cpuInfo[ 4 ] = { 0 };
		__cpuid( cpuInfo, 1 );

		return ( cpuInfo[ 2 ] & ( 1 << 5 ) ) != 0;
	}

	bool enable_vmx( )
	{
		ia32_feature_control_register feature_ctl;
		feature_ctl.flags = __readmsr( IA32_FEATURE_CONTROL );

		if ( !feature_ctl.lock_bit )
		{
			feature_ctl.enable_vmx_outside_smx = 1;
			feature_ctl.lock_bit = 1;
			__writemsr( IA32_FEATURE_CONTROL, feature_ctl.flags );
		}
		else if ( !feature_ctl.enable_vmx_outside_smx )
		{
			return false;
		}

		uint64_t cr0_fixed0 = __readmsr( IA32_VMX_CR0_FIXED0 ); 
		uint64_t cr0_fixed1 = __readmsr( IA32_VMX_CR0_FIXED1 );

		uint64_t cr4_fixed0 = __readmsr( IA32_VMX_CR4_FIXED0 );
		uint64_t cr4_fixed1 = __readmsr( IA32_VMX_CR4_FIXED1 ); 

		uint64_t cr0 = __readcr0( );
		cr0 |= cr0_fixed0;  
		cr0 &= cr0_fixed1;  
		__writecr0( cr0 );

		uint64_t cr4 = __readcr4( );
		cr4 |= cr4_fixed0;   
		cr4 &= cr4_fixed1;
		__writecr4( cr4 );

		return true;
	}

	uint64_t vmread( uint64_t field )
	{
		uint64_t result = 0;
		__vmx_vmread( field, &result );
		return result;
	}

	void vmwrite( uint64_t field, uint64_t value )
	{
		__vmx_vmwrite( field, value );
	}

	uint64_t get_kernel_cr3( )
	{
		uint64_t cr3 = *( uint64_t* ) ( ( uint64_t ) PsInitialSystemProcess + 0x28 );
		return cr3;
	}

}

#endif // TOOLS_H
