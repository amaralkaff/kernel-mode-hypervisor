#pragma once
#ifndef VMEXIT
#define VMEXIT

#include "handlers.h"
#include "dbg.h"

extern "C" void vm_restore_context( CONTEXT* ctx );
extern "C" void vmresume_fn( );

// returns 0 for normal vmresume
// for devirt: returns guest_rsp in rax, guest_rip in rdx (asm reads both)
extern "C" uint64_t vmentry_handler_cpp( guest_regs_t* regs )
{
    unsigned long reason = tools::vmread( VMCS_EXIT_REASON ) & 0xffff;
    uint32_t cpu = ( uint32_t ) __readgsdword( 0x184 );
    if ( cpu >= 64 ) cpu = 0;

    switch ( reason )
    {
    case VMX_EXIT_REASON_EXCEPTION_OR_NMI:
        handlers::handle_exception_or_nmi( cpu );
        break;
    case VMX_EXIT_REASON_NMI_WINDOW:
        handlers::handle_nmi_window( cpu );
        break;
    case VMX_EXIT_REASON_EXECUTE_CPUID:
        handlers::handle_cpuid( regs );
        break;
    case VMX_EXIT_REASON_MOV_CR:
        handlers::handle_cr_access( regs );
        break;
    case VMX_EXIT_REASON_EXECUTE_RDTSCP:
        handlers::handle_rdtscp( regs );
        break;
    case VMX_EXIT_REASON_EXECUTE_XSETBV:
        handlers::handle_xsetbv( regs );
        break;
    case VMX_EXIT_REASON_EXECUTE_GETSEC:
        handlers::handle_getsec( );
        break;
    case VMX_EXIT_REASON_EXECUTE_INVD:
        handlers::handle_invd( );
        break;
    case VMX_EXIT_REASON_EXECUTE_VMCALL:
        if ( handlers::handle_vmcall( regs ) )
        {
            // devirtualize this core
            uint64_t guest_rip = tools::vmread( VMCS_GUEST_RIP ) + tools::vmread( VMCS_VMEXIT_INSTRUCTION_LENGTH );
            uint64_t guest_rsp = tools::vmread( VMCS_GUEST_RSP );
            uint64_t guest_rflags = tools::vmread( VMCS_GUEST_RFLAGS );
            uint64_t guest_cr3 = tools::vmread( VMCS_GUEST_CR3 );
            __writecr3( guest_cr3 );
            regs->rdx = guest_rip;
            regs->rcx = guest_rflags;
            return guest_rsp;
        }
        break;
    case VMX_EXIT_REASON_EXECUTE_WRMSR:
        handlers::handle_wrmsr( regs );
        break;
    case VMX_EXIT_REASON_EXECUTE_RDMSR:
        handlers::handle_rdmsr( regs );
        break;
    case VMX_EXIT_REASON_EXECUTE_VMXON:
    case VMX_EXIT_REASON_EXECUTE_VMXOFF:
    case VMX_EXIT_REASON_EXECUTE_VMCLEAR:
    case VMX_EXIT_REASON_EXECUTE_VMPTRLD:
    case VMX_EXIT_REASON_EXECUTE_VMPTRST:
    case VMX_EXIT_REASON_EXECUTE_VMREAD:
    case VMX_EXIT_REASON_EXECUTE_VMWRITE:
    case VMX_EXIT_REASON_EXECUTE_VMLAUNCH:
    case VMX_EXIT_REASON_EXECUTE_VMRESUME:
    case VMX_EXIT_REASON_EXECUTE_INVEPT:
    case VMX_EXIT_REASON_EXECUTE_INVVPID:
    case VMX_EXIT_REASON_EXECUTE_VMFUNC:
        handlers::handle_vmx_instruction( );
        break;
    case VMX_EXIT_REASON_EPT_VIOLATION:
    case VMX_EXIT_REASON_EPT_MISCONFIGURATION:
        // EPT issue — skip instruction to avoid infinite loop
        handlers::move_rip( );
        break;
    case VMX_EXIT_REASON_TRIPLE_FAULT:
        KeBugCheck( HYPERVISOR_ERROR );
        break;
    case VMX_EXIT_REASON_EXECUTE_XSAVES:
    case VMX_EXIT_REASON_EXECUTE_XRSTORS:
    case VMX_EXIT_REASON_EXECUTE_IO_INSTRUCTION:
    case VMX_EXIT_REASON_EXECUTE_HLT:
    case VMX_EXIT_REASON_EXECUTE_PAUSE:
        handlers::move_rip( );
        break;
    default:
        // BSOD with exit reason so we can diagnose instead of silent freeze
        // param1=0xDEAD, param2=exit_reason, param3=guest_rip, param4=cpu
        KeBugCheckEx( 0xDEAD0000 | reason, reason,
            tools::vmread( VMCS_GUEST_RIP ),
            ( ULONG_PTR ) cpu, 0 );
        break;
    }

    return 0;
}

#endif // !VMEXIT
