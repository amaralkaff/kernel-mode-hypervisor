#include "init.h"
#include "vmexit.h"
#include "cleanup.h"

static void devirt_dpc( PKDPC dpc, void* ctx, void* sysarg1, void* sysarg2 )
{
	auto sync_dpc = ( init::ke_signal_call_dpc_sync_t* ) ( ( init::dpc_context* ) ctx )->ke_signal_call_dpc_sync;
	auto call_done = ( init::ke_signal_call_dpc_done_t* ) ( ( init::dpc_context* ) ctx )->ke_signal_call_dpc_done;

	uint64_t result = 0;
	__try
	{
		result = handlers::devirt_vmcall( );
	}
	__except ( EXCEPTION_EXECUTE_HANDLER )
	{
		result = 0;
	}

	log::dbg_print( "devirtualized core %u (result=%llu)", KeGetCurrentProcessorIndex( ), result );

	sync_dpc( sysarg2 );
	call_done( sysarg1 );
}

static void driver_unload( driver_object_t* drv )
{
	static void* ke_generic_call_dpc = tools::get_system_routine( L"KeGenericCallDpc" );
	if ( !ke_generic_call_dpc )
		return;

	init::dpc_context ctx{
		tools::get_system_routine( L"KeSignalCallDpcSynchronize" ),
		tools::get_system_routine( L"KeSignalCallDpcDone" ),
		nullptr
	};

	auto send_dpc = ( init::ke_generic_call_dpc_t* ) ke_generic_call_dpc;
	send_dpc( devirt_dpc, &ctx );

	log::dbg_print( "all cores devirtualized" );
}

nt_status_t driver_entry( driver_object_t* drv, unicode_string_t* reg )
{
	log::dbg_print( "vmhv: driver_entry called" );

	if ( !tools::is_vmx_supported( ) )
	{
		log::dbg_print( "vmhv: VMX not supported!" );
		return nt_status_t::unsuccessful;
	}
	log::dbg_print( "vmhv: VMX supported" );

	if ( !ept::init_ept( ) )
	{
		log::dbg_print( "vmhv: EPT init failed!" );
		return nt_status_t::unsuccessful;
	}
	log::dbg_print( "vmhv: EPT init OK" );

	// Set driver_unload for clean devirtualization via sc stop
	if ( drv )
		drv->driver_unload = ( void* ) driver_unload;

	log::dbg_print( "vmhv: starting virtualization..." );
	if ( !init::virtualize_all_cores( ) )
	{
		log::dbg_print( "vmhv: virtualize_all_cores FAILED!" );
		return nt_status_t::unsuccessful;
	}
	log::dbg_print( "vmhv: all cores virtualized OK" );

	return nt_status_t::success;
}