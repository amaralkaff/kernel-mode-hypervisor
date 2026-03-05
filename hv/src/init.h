#pragma once
#ifndef INIT
#define INIT
#include "dbg.h"
#include "tools.h"
#include "vcpu.h"
#include "vmcs.h"
#include "comm.h"
#include "dse.h"
namespace init
{
	typedef void( ke_generic_call_dpc_t )( PKDEFERRED_ROUTINE, void* );
	typedef void ( ke_signal_call_dpc_sync_t )( void* );
	typedef void ( ke_signal_call_dpc_done_t )( void* );


	struct dpc_context
	{
		void* ke_signal_call_dpc_sync;
		void* ke_signal_call_dpc_done;
		vcpu_t** vcpus;
	};

	extern "C" void capture_ctx( vcpu_t* vcpu );

	void init_routine( PKDPC dpc, void* ctx, void* sysarg1, void* sysarg2 )
	{
		auto* context = static_cast< dpc_context* >( ctx );
		auto sync_dpc = ( ke_signal_call_dpc_sync_t* ) context->ke_signal_call_dpc_sync;
		auto call_done = ( ke_signal_call_dpc_done_t* ) context->ke_signal_call_dpc_done;

		uint64_t current_core = KeGetCurrentProcessorIndex( );

		if ( !tools::enable_vmx( ) )
		{
			sync_dpc( sysarg2 );
			call_done( sysarg1 );
			return;
		}

		vcpu_t* vcpu = context->vcpus[ current_core ];
		g_vcpus[ current_core ] = vcpu;

		if ( __vmx_on( &vcpu->vmxon_region_physical ) != 0 )
		{
			sync_dpc( sysarg2 );
			call_done( sysarg1 );
			return;
		}

		if ( __vmx_vmclear( &vcpu->vmcs_region_physical ) != 0 )
		{
			sync_dpc( sysarg2 );
			call_done( sysarg1 );
			return;
		}

		if ( __vmx_vmptrld( &vcpu->vmcs_region_physical ) != 0 )
		{
			sync_dpc( sysarg2 );
			call_done( sysarg1 );
			return;
		}

		capture_ctx( vcpu );

		sync_dpc( sysarg2 );
		call_done( sysarg1 );

		return;
	}

	bool virtualize_all_cores( )
	{
		static void* ke_generic_call_dpc = tools::get_system_routine( L"KeGenericCallDpc" );
		if ( !ke_generic_call_dpc )
			return false;

		// msr cache init
		if ( !g_msr_cache.initialized )
			init_msr_cache( );

		// parse MTRRs for correct EPT memory types (optional, WB fallback is fine)
		ept::parse_mtrrs( );

		// cache System EPROCESS + CR3 for physical memory walks from VMX root
		if ( !comm::cache_system_process( ) )
			log::dbg_print( "vmhv: cache_system_process failed (attach will use PsLookup fallback)" );

		// find CiOptions for DSE manipulation (scan wrapped in __try/__except)
		if ( !dse::init( ) )
			log::dbg_print( "vmhv: dse::init failed (DSE commands will be unavailable)" );

		uint32_t core_count = KeQueryActiveProcessorCountEx( ALL_PROCESSOR_GROUPS );

		// allocate per-core scratch pages for VMX-root safe physical memory access
		if ( !ept::init_scratch_pages( core_count ) )
			log::dbg_print( "vmhv: scratch pages failed (will use MmMapIoSpace fallback)" );

		vcpu_t** vcpus = ( vcpu_t** ) ExAllocatePool( NonPagedPool, sizeof( vcpu_t* ) * core_count );
		if ( !vcpus ) return false;

		RtlZeroMemory( vcpus, sizeof( vcpu_t* ) * core_count );
		for ( uint32_t i = 0; i < core_count; i++ )
		{
			vcpus[ i ] = vcpu::allocate_vcpu( );
			if ( !vcpus[ i ] )
			{
				ExFreePool( vcpus );
				return false;
			}
			vcpus[ i ]->vpid = ( uint16_t ) ( i + 1 );
		}

		void* sync_fn = tools::get_system_routine( L"KeSignalCallDpcSynchronize" );
		void* done_fn = tools::get_system_routine( L"KeSignalCallDpcDone" );
		if ( !sync_fn || !done_fn )
		{
			ExFreePool( vcpus );
			return false;
		}

		dpc_context ctx{
			sync_fn,
			done_fn,
			vcpus
		};

		ke_generic_call_dpc_t* send_dpc = ( ke_generic_call_dpc_t* ) ke_generic_call_dpc;

		send_dpc( init::init_routine, &ctx );

		return true;
	}
}

#endif // !INIT
