#pragma once
#include "tools.h"
#include "ia32.h"
#include "structs.h"
#include "comm.h"
#include "dse.h"
static uint32_t g_queued_nmis[ 64 ] = {};

// CPUID magic leaves for hypervisor communication
#define HV_CPUID_PING          0x13370000  // -> EBX = signature
#define HV_CPUID_ATTACH        0x13370001  // ECX = PID -> EAX = ok
#define HV_CPUID_READ4         0x13370002  // ECX = low32 of VA -> EAX = uint32 value
#define HV_CPUID_MODULE_BASE   0x13370003  // ECX = name hash -> EAX/EBX = base
#define HV_CPUID_DETACH        0x13370004  // detach
#define HV_CPUID_SET_ADDR_HIGH 0x13370005  // ECX = high32 of address base (per-core)
#define HV_CPUID_READ8         0x13370006  // ECX = low32 of VA -> EAX/EBX = uint64 value
#define HV_CPUID_READ_BUF      0x13370007  // ECX = low32 of src VA, subleaf triggers chunked read
#define HV_CPUID_COPY_CHUNK    0x13370008  // ECX = chunk index -> EAX/EBX/ECX/EDX = 16 bytes
#define HV_CPUID_WRITE4        0x13370009  // ECX = low32 of VA, EDX = uint32 value
#define HV_CPUID_WRITE8        0x1337000A  // ECX = low32 of VA; value set via WRITE8_DATA first
#define HV_CPUID_WRITE8_DATA   0x1337000B  // ECX = low32, EDX = high32 of the 8-byte value to write
#define HV_CPUID_MTRR_INFO    0x1337000C  // ECX = 0: summary, ECX = N: variable MTRR entry N
#define HV_CPUID_MTRR_RAW     0x1337000D  // ECX = MSR index -> EAX/EBX = raw MSR value
#define HV_CPUID_DSE           0x1337000E  // ECX=0: disable DSE, ECX=1: restore DSE, ECX=2: query status
#define HV_SIGNATURE           0x7276'6D68 // 'hvmr' in little-endian

// Per-core state for address high bits and pending buffer reads
static uint32_t g_addr_high[64] = {};
static uint8_t  g_read_cache[64][0x1000] = {};
static uint32_t g_read_cache_size[64] = {};
static uint64_t g_write8_pending[64] = {};  // pending 8-byte write value per core

// TSC compensation: accumulated overhead from VMExits
// Subtracted from RDTSC via VMCS TSC_OFFSET so anti-cheat timing checks
// see consistent timing as if no hypervisor were present.
// Uses hardware TSC offsetting (no RDTSC VMExit needed = zero perf cost).
static int64_t g_tsc_overhead[64] = {};

namespace handlers
{
    extern "C"
    {
        void memcpy_s( exception_info_t& e, void* dst, void const* src, size_t size );

        void xsetbv_s( exception_info_t& e, uint32_t idx, uint64_t value );

        void wrmsr_s( exception_info_t& e, uint32_t msr, uint64_t value );

        uint64_t rdmsr_s( exception_info_t& e, uint32_t msr );

        uint64_t devirt_vmcall( );
    }

    inline bool is_canonical( uint64_t addr )
    {
        return ( ( int64_t ) addr >> 47 ) == 0 || ( ( int64_t ) addr >> 47 ) == -1;
    }

    inline bool is_valid_msr( uint32_t msr )
    {
        return ( msr <= 0x1FFF ) || ( msr >= 0xC0000000 && msr <= 0xC0001FFF );
    }

    inline void move_rip( )
    {
        uint64_t rip = tools::vmread( VMCS_GUEST_RIP );
        uint64_t len = tools::vmread( VMCS_VMEXIT_INSTRUCTION_LENGTH );
        tools::vmwrite( VMCS_GUEST_RIP, rip + len );
    }

    inline void inject_hw( uint32_t vector, bool has_error = false, uint32_t error = 0 )
    {
        vmentry_interrupt_information interrupt = {};
        interrupt.vector = vector;
        interrupt.interruption_type = 3;
        interrupt.deliver_error_code = has_error ? 1 : 0;
        interrupt.valid = 1;
        tools::vmwrite( VMCS_CTRL_VMENTRY_INTERRUPTION_INFORMATION_FIELD, interrupt.flags );
        if ( has_error )
            tools::vmwrite( VMCS_CTRL_VMENTRY_EXCEPTION_ERROR_CODE, error );
    }

    inline void inject_nmi( )
    {
        vmentry_interrupt_information nmi = {};
        nmi.vector = 2;
        nmi.interruption_type = 2; 
        nmi.valid = 1;
        tools::vmwrite( VMCS_CTRL_VMENTRY_INTERRUPTION_INFORMATION_FIELD, nmi.flags );
    }

    inline void handle_rdtscp( guest_regs_t* regs )
    {
        uint32_t aux = 0;
        uint64_t tsc = __rdtscp( &aux );
        regs->rax = ( uint32_t ) tsc;
        regs->rdx = ( uint32_t ) ( tsc >> 32 );
        regs->rcx = aux;
        move_rip( );
    }

    inline void compensate_tsc( uint32_t core, uint64_t tsc_start )
    {
        uint64_t tsc_end = __rdtsc( );
        uint64_t overhead = tsc_end - tsc_start;
        g_tsc_overhead[core] += overhead;
        tools::vmwrite( VMCS_CTRL_TSC_OFFSET, -( int64_t ) g_tsc_overhead[core] );
    }

    inline void handle_cpuid( guest_regs_t* regs )
    {
        uint32_t leaf = ( uint32_t ) regs->rax;
        uint32_t core = ( uint32_t ) __readgsdword( 0x184 );
        if ( core >= 64 ) core = 0;

        // only handle 0x1337xxxx from usermode (CPL 3)
        uint64_t cs_sel = tools::vmread( VMCS_GUEST_CS_SELECTOR );
        bool from_usermode = ( cs_sel & 3 ) == 3;

        if ( from_usermode && ( leaf & 0xFFFF0000 ) == 0x13370000 )
        {
            switch ( leaf )
            {
            case HV_CPUID_PING:
            {
                uint32_t sub = ( uint32_t ) regs->rcx;
                if ( sub == 0 )
                {
                    regs->rax = 1;
                    regs->rbx = HV_SIGNATURE;
                    regs->rcx = 0x19; // version
                    regs->rdx = 0;
                }
                else if ( sub == 0xFFFE )
                {
                    // ept cap diag
                    uint64_t ept_cap = __readmsr( 0x48C );
                    regs->rax = ( uint32_t ) ( ept_cap & 0xFFFFFFFF );
                    regs->rbx = ( uint32_t ) ( ept_cap >> 32 );
                    ia32_vmx_procbased_ctls2_register req = {};
                    req.enable_ept = 1;
                    req.enable_vpid = 1;
                    req.enable_rdtscp = 1;
                    req.enable_xsaves = 1;
                    uint32_t adjusted = adjust_controls( req.flags, IA32_VMX_PROCBASED_CTLS2 );
                    regs->rcx = adjusted;
                    regs->rdx = 0xEEEE; // sentinel
                }
                else if ( sub == 0xFFFD )
                {
                    // eptp diag
                    regs->rax = ( uint32_t ) ( ept::g_ept.eptp & 0xFFFFFFFF );
                    regs->rbx = ( uint32_t ) ( ept::g_ept.eptp >> 32 );
                    regs->rcx = ( uint32_t ) ( ept::g_ept.pml4_physical & 0xFFFFFFFF );
                    regs->rdx = ( uint32_t ) ( ept::g_ept.pml4_physical >> 32 );
                }
                else if ( sub == 0xFFFF )
                {
                    // msr diag
                    uint64_t cap = __readmsr( 0xFE );  // IA32_MTRRCAP
                    uint64_t def = __readmsr( 0x2FF );  // IA32_MTRR_DEF_TYPE
                    regs->rax = ( uint32_t ) ( cap & 0xFFFFFFFF );
                    regs->rbx = ( uint32_t ) ( cap >> 32 );
                    regs->rcx = ( uint32_t ) ( def & 0xFFFFFFFF );
                    regs->rdx = 0xDEAD; // sentinel
                }
                else if ( sub >= 1 && sub <= 0x1000 )
                {
                    if ( is_valid_msr( sub ) )
                    {
                        exception_info_t ex = {};
                        uint64_t val = rdmsr_s( ex, sub );
                        if ( !ex.exception_occurred )
                        {
                            regs->rax = ( uint32_t ) ( val & 0xFFFFFFFF );
                            regs->rbx = ( uint32_t ) ( val >> 32 );
                        }
                        else
                        {
                            regs->rax = 0;
                            regs->rbx = 0;
                        }
                    }
                    else
                    {
                        regs->rax = 0;
                        regs->rbx = 0;
                    }
                    regs->rcx = sub;
                    regs->rdx = 0xBEEF; // sentinel
                }
                else
                {
                    regs->rax = 0;
                    regs->rbx = 0;
                    regs->rcx = 0;
                    regs->rdx = 0xCAFE; // unknown sub sentinel
                }
                break;
            }
            case HV_CPUID_ATTACH:
            {
                uint32_t pid = ( uint32_t ) regs->rcx;
                bool ok = comm::attach( pid );
                regs->rax = ok ? 1 : 0;
                regs->rbx = 0;
                regs->rcx = 0;
                regs->rdx = 0;
                break;
            }
            case HV_CPUID_READ4:
            {
                // Read 4 bytes: full VA = (g_addr_high[core] << 32) | ECX
                uint64_t va = ( ( uint64_t ) g_addr_high[core] << 32 ) | ( uint32_t ) regs->rcx;
                uint64_t cr3_local = comm::g_target_cr3;
                uint32_t val = 0;
                if ( cr3_local && ept::read_process_memory( cr3_local, va, &val, 4 ) )
                    regs->rax = val;
                else
                    regs->rax = 0;
                regs->rbx = 0;
                regs->rcx = 0;
                regs->rdx = 0;
                break;
            }
            case HV_CPUID_MODULE_BASE:
            {
                uint32_t hash = ( uint32_t ) regs->rcx;
                uint64_t base = comm::get_module_base( hash );
                regs->rax = ( uint32_t ) ( base & 0xFFFFFFFF );
                regs->rbx = ( uint32_t ) ( base >> 32 );
                regs->rcx = 0;
                regs->rdx = 0;
                break;
            }
            case HV_CPUID_DETACH:
            {
                comm::detach( );
                regs->rax = 1;
                regs->rbx = 0;
                regs->rcx = 0;
                regs->rdx = 0;
                break;
            }
            case HV_CPUID_SET_ADDR_HIGH:
            {
                // Set the high 32 bits of addresses for subsequent reads on this core
                g_addr_high[core] = ( uint32_t ) regs->rcx;
                regs->rax = 1;
                regs->rbx = 0;
                regs->rcx = 0;
                regs->rdx = 0;
                break;
            }
            case HV_CPUID_READ8:
            {
                // Read 8 bytes: full VA = (g_addr_high[core] << 32) | ECX
                uint64_t va = ( ( uint64_t ) g_addr_high[core] << 32 ) | ( uint32_t ) regs->rcx;
                uint64_t cr3_local = comm::g_target_cr3;
                uint64_t val = 0;
                if ( cr3_local && ept::read_process_memory( cr3_local, va, &val, 8 ) )
                {
                    regs->rax = ( uint32_t ) ( val & 0xFFFFFFFF );
                    regs->rbx = ( uint32_t ) ( val >> 32 );
                }
                else
                {
                    regs->rax = 0;
                    regs->rbx = 0;
                }
                regs->rcx = 0;
                regs->rdx = 0;
                break;
            }
            case HV_CPUID_READ_BUF:
            {
                // Buffered read: ECX = low32 of src VA
                // Always reads 4096 bytes into per-core cache, retrieve with COPY_CHUNK
                uint64_t va = ( ( uint64_t ) g_addr_high[core] << 32 ) | ( uint32_t ) regs->rcx;
                uint32_t size = 0x1000;
                uint64_t cr3_local = comm::g_target_cr3;

                if ( cr3_local && size > 0 &&
                     ept::read_process_memory( cr3_local, va, g_read_cache[core], size ) )
                {
                    g_read_cache_size[core] = size;
                    regs->rax = size;
                }
                else
                {
                    g_read_cache_size[core] = 0;
                    regs->rax = 0;
                }
                regs->rbx = 0;
                regs->rcx = 0;
                regs->rdx = 0;
                break;
            }
            case HV_CPUID_COPY_CHUNK:
            {
                // Copy 16 bytes from read cache at offset = ECX * 16
                uint32_t chunk_idx = ( uint32_t ) regs->rcx;
                // Bounds check: max valid chunk_idx for 4KB cache is 255 (255*16+16 = 4096)
                if ( chunk_idx > 0xFF )
                {
                    regs->rax = 0;
                    regs->rbx = 0;
                    regs->rcx = 0;
                    regs->rdx = 0;
                    break;
                }
                uint32_t offset = chunk_idx * 16;

                if ( offset + 16 <= g_read_cache_size[core] )
                {
                    uint32_t* src = ( uint32_t* ) &g_read_cache[core][offset];
                    regs->rax = src[0];
                    regs->rbx = src[1];
                    regs->rcx = src[2];
                    regs->rdx = src[3];
                }
                else
                {
                    regs->rax = 0;
                    regs->rbx = 0;
                    regs->rcx = 0;
                    regs->rdx = 0;
                }
                break;
            }
            case HV_CPUID_WRITE8_DATA:
            {
                // Stage write value: ECX = subleaf = value to store
                // Called as __cpuidex(regs, WRITE8_DATA, value_low32)
                // For 4-byte writes: just stages low32
                // For 8-byte writes: call twice — first with low32, second with high32
                //   The handler packs them: first call sets low, second shifts and ORs high
                // Protocol: bit 0 of EAX return = which half was set (0=low, 1=high)
                // Actually simplest: just store the 32-bit subleaf as pending value
                g_write8_pending[core] = ( uint32_t ) regs->rcx;
                regs->rax = 1;
                regs->rbx = 0;
                regs->rcx = 0;
                regs->rdx = 0;
                break;
            }
            case HV_CPUID_WRITE4:
            {
                // Write 4 bytes: VA = (g_addr_high[core] << 32) | ECX
                // Value = g_write8_pending[core] (staged via WRITE8_DATA)
                uint64_t va = ( ( uint64_t ) g_addr_high[core] << 32 ) | ( uint32_t ) regs->rcx;
                uint64_t cr3_local = comm::g_target_cr3;
                uint32_t val = ( uint32_t ) g_write8_pending[core];
                if ( cr3_local && ept::write_process_memory( cr3_local, va, &val, 4 ) )
                    regs->rax = 1;
                else
                    regs->rax = 0;
                regs->rbx = 0;
                regs->rcx = 0;
                regs->rdx = 0;
                break;
            }
            case HV_CPUID_WRITE8:
            {
                // Write 8 bytes: VA = (g_addr_high[core] << 32) | ECX
                // Value = g_write8_pending[core] (must be staged as full 64-bit via two WRITE8_DATA calls)
                // For simplicity, the usermode side does two 4-byte writes instead
                uint64_t va = ( ( uint64_t ) g_addr_high[core] << 32 ) | ( uint32_t ) regs->rcx;
                uint64_t cr3_local = comm::g_target_cr3;
                uint64_t val = g_write8_pending[core];
                if ( cr3_local && ept::write_process_memory( cr3_local, va, &val, 8 ) )
                    regs->rax = 1;
                else
                    regs->rax = 0;
                regs->rbx = 0;
                regs->rcx = 0;
                regs->rdx = 0;
                break;
            }
            case HV_CPUID_MTRR_RAW:
            {
                // ECX=0xFFFF: diagnostic test — return hardcoded MTRRCAP
                // Otherwise: read MSR index from ECX
                uint32_t msr_idx = ( uint32_t ) regs->rcx;
                if ( msr_idx == 0xFFFF )
                {
                    // Hardcoded read of IA32_MTRRCAP (0xFE) as diagnostic
                    uint64_t cap = __readmsr( 0xFE );
                    uint64_t def = __readmsr( 0x2FF );
                    regs->rax = ( uint32_t ) ( cap & 0xFFFFFFFF );
                    regs->rbx = ( uint32_t ) ( cap >> 32 );
                    regs->rcx = ( uint32_t ) ( def & 0xFFFFFFFF );
                    regs->rdx = 0xDEAD; // sentinel to prove handler ran
                }
                else
                {
                    if ( is_valid_msr( msr_idx ) )
                    {
                        exception_info_t ex = {};
                        uint64_t val = rdmsr_s( ex, msr_idx );
                        if ( !ex.exception_occurred )
                        {
                            regs->rax = ( uint32_t ) ( val & 0xFFFFFFFF );
                            regs->rbx = ( uint32_t ) ( val >> 32 );
                        }
                        else
                        {
                            regs->rax = 0;
                            regs->rbx = 0;
                        }
                    }
                    else
                    {
                        regs->rax = 0;
                        regs->rbx = 0;
                    }
                    regs->rcx = 0;
                    regs->rdx = 0xBEEF; // sentinel
                }
                break;
            }
            case HV_CPUID_MTRR_INFO:
            {
                uint32_t idx = ( uint32_t ) regs->rcx;
                if ( idx == 0 )
                {
                    // Summary: EAX = var count, EBX = default type, ECX = enabled, EDX = fixed_enabled
                    regs->rax = ept::g_mtrr.variable_count;
                    regs->rbx = ept::g_mtrr.default_type;
                    regs->rcx = ept::g_mtrr.mtrr_enabled ? 1 : 0;
                    regs->rdx = ept::g_mtrr.fixed_enabled ? 1 : 0;
                }
                else if ( idx <= ept::g_mtrr.variable_count )
                {
                    // Variable MTRR entry (1-based index)
                    auto& r = ept::g_mtrr.variable[idx - 1];
                    regs->rax = r.valid ? 1 : 0;
                    regs->rbx = r.type;
                    // Pack range_base into ECX (high 32 bits) — shift right by 20 to fit in 32 bits (MB granularity)
                    regs->rcx = ( uint32_t ) ( r.range_base >> 20 );
                    regs->rdx = ( uint32_t ) ( r.range_end >> 20 );
                }
                else
                {
                    regs->rax = 0;
                    regs->rbx = 0;
                    regs->rcx = 0;
                    regs->rdx = 0;
                }
                break;
            }
            case HV_CPUID_DSE:
            {
                uint32_t op = ( uint32_t ) regs->rcx;
                if ( op == 0 )
                {
                    // disable DSE
                    regs->rax = dse::disable( ) ? 1 : 0;
                }
                else if ( op == 1 )
                {
                    // restore DSE
                    regs->rax = dse::restore( ) ? 1 : 0;
                }
                else if ( op == 2 )
                {
                    // query: EAX = CiOptions value, EBX = CiOptions address low32
                    if ( dse::g_ci_options_va )
                    {
                        regs->rax = *( uint32_t* ) dse::g_ci_options_va;
                        regs->rbx = ( uint32_t ) ( dse::g_ci_options_va & 0xFFFFFFFF );
                    }
                    else
                    {
                        regs->rax = 0xFFFFFFFF;
                        regs->rbx = 0;
                    }
                }
                regs->rcx = 0;
                regs->rdx = 0xD5E0; // dse sentinel
                break;
            }
            default:
            {
                regs->rax = 0;
                regs->rbx = 0;
                regs->rcx = 0;
                regs->rdx = 0;
                break;
            }
            }
            move_rip( );
            return;
        }

        // Standard CPUID passthrough
        int result[ 4 ] = {};
        __cpuidex( result, ( int ) regs->rax, ( int ) regs->rcx );

        // STEALTH: Clear hypervisor present bit (ECX bit 31) so OS/anti-cheat
        // doesn't see a hypervisor. This makes us transparent to checks like
        // IsProcessorFeaturePresent and direct CPUID leaf 1 queries.
        if ( leaf == 1 )
            result[ 2 ] &= ~( 1u << 31 );

        // STEALTH: Suppress entire hypervisor leaf range. Return zeros as if
        // no hypervisor is present — matches bare-metal behavior.
        if ( leaf >= 0x40000000 && leaf <= 0x4FFFFFFF )
        {
            regs->rax = 0;
            regs->rbx = 0;
            regs->rcx = 0;
            regs->rdx = 0;
            move_rip( );
            return;
        }

        regs->rax = ( uint32_t ) result[ 0 ];
        regs->rbx = ( uint32_t ) result[ 1 ];
        regs->rcx = ( uint32_t ) result[ 2 ];
        regs->rdx = ( uint32_t ) result[ 3 ];

        move_rip( );
    }

    inline void handle_rdmsr( guest_regs_t* regs )
    {
        LARGE_INTEGER msr = { 0 };
        uint32_t msr_id = ( uint32_t ) regs->rcx;

        // FEATURE_CONTROL: return masked value (hide VMX-outside-SMX)
        if ( msr_id == IA32_FEATURE_CONTROL )
        {
            uint32_t core = ( uint32_t ) __readgsdword( 0x184 );
            if ( core >= 64 ) core = 0;
            vcpu_t* vcpu = g_vcpus[ core ];
            msr.QuadPart = vcpu ? vcpu->feature_ctl_masked : __readmsr( IA32_FEATURE_CONTROL );
            regs->rax = msr.LowPart;
            regs->rdx = msr.HighPart;
            move_rip( );
            return;
        }

        // invalid MSR range → #GP(0), matches bare-metal
        if ( !( ( msr_id <= 0x00001FFF ) || ( msr_id >= 0xC0000000 && msr_id <= 0xC0001FFF )
            || ( msr_id >= 0x40000000 && msr_id <= 0x400000F0 ) ) )
        {
            inject_hw( 13, true, 0 );
            return;
        }

        msr.QuadPart = __readmsr( msr_id );

        regs->rax = msr.LowPart;
        regs->rdx = msr.HighPart;

        move_rip( );
    }



    inline void handle_wrmsr( guest_regs_t* regs )
    {
        uint32_t msr_id = ( uint32_t ) regs->rcx;

        // block writes to FEATURE_CONTROL — guest shouldn't toggle VMX
        if ( msr_id == IA32_FEATURE_CONTROL )
        {
            inject_hw( general_protection, true, 0 );
            return;
        }

        // validate EFER writes — can't disable IA32e mode while paging is on
        if ( msr_id == IA32_EFER )
        {
            ia32_efer_register efer;
            efer.flags = ( ( uint64_t ) ( uint32_t ) regs->rdx << 32 ) | ( uint32_t ) regs->rax;
            if ( !efer.ia32e_mode_enable || !efer.ia32e_mode_active )
            {
                inject_hw( general_protection, true, 0 );
                return;
            }
            __writemsr( IA32_EFER, efer.flags );
            move_rip( );
            return;
        }

        // invalid MSR range → #GP(0), same as bare metal
        if ( !( ( msr_id <= 0x00001FFF ) || ( msr_id >= 0xC0000000 && msr_id <= 0xC0001FFF )
            || ( msr_id >= 0x40000000 && msr_id <= 0x400000F0 ) ) )
        {
            inject_hw( 13, true, 0 );
            return;
        }

        LARGE_INTEGER msr;
        msr.LowPart = ( ULONG ) regs->rax;
        msr.HighPart = ( ULONG ) regs->rdx;
        __writemsr( msr_id, msr.QuadPart );

        move_rip( );
    }

    inline void handle_cr_access( guest_regs_t* regs )
    {
        uint64_t qualification = tools::vmread( VMCS_EXIT_QUALIFICATION );
        uint64_t cr_num = qualification & 0xF;
        uint64_t access = ( qualification >> 4 ) & 0x3;
        uint64_t gpr_idx = ( qualification >> 8 ) & 0xF;

        auto read_gpr = [&]( uint64_t idx ) -> uint64_t
            {
                switch ( idx )
                {
                case 0:  return regs->rax;
                case 1:  return regs->rcx;
                case 2:  return regs->rdx;
                case 3:  return regs->rbx;
                case 4:  return tools::vmread( VMCS_GUEST_RSP );
                case 5:  return regs->rbp;
                case 6:  return regs->rsi;
                case 7:  return regs->rdi;
                case 8:  return regs->r8;
                case 9:  return regs->r9;
                case 10: return regs->r10;
                case 11: return regs->r11;
                case 12: return regs->r12;
                case 13: return regs->r13;
                case 14: return regs->r14;
                case 15: return regs->r15;
                default: return 0;
                }
            };

        auto write_gpr = [&]( uint64_t idx, uint64_t val )
            {
                switch ( idx )
                {
                case 0:  regs->rax = val; break;
                case 1:  regs->rcx = val; break;
                case 2:  regs->rdx = val; break;
                case 3:  regs->rbx = val; break;
                case 4:  tools::vmwrite( VMCS_GUEST_RSP, val ); break;
                case 5:  regs->rbp = val; break;
                case 6:  regs->rsi = val; break;
                case 7:  regs->rdi = val; break;
                case 8:  regs->r8 = val; break;
                case 9:  regs->r9 = val; break;
                case 10: regs->r10 = val; break;
                case 11: regs->r11 = val; break;
                case 12: regs->r12 = val; break;
                case 13: regs->r13 = val; break;
                case 14: regs->r14 = val; break;
                case 15: regs->r15 = val; break;
                }
            };

        if ( access == 0 ) 
        {
            uint64_t val = read_gpr( gpr_idx );
            switch ( cr_num )
            {
            case 0:
            {
                cr0 new_cr0;
                new_cr0.flags = val;
                if ( !new_cr0.paging_enable || !new_cr0.protection_enable )
                {
                    inject_hw( 13, true, 0 );
                    return;
                }
                tools::vmwrite( VMCS_CTRL_CR0_READ_SHADOW, val );
                // Apply fixed bits for VMCS guest CR0
                val |= g_msr_cache.cr0_fixed0;
                val &= g_msr_cache.cr0_fixed1;
                tools::vmwrite( VMCS_GUEST_CR0, val );
                break;
            }
            case 3:
                tools::vmwrite( VMCS_GUEST_CR3, val );
                break;
            case 4:
            {
                cr4 new_cr4;
                new_cr4.flags = val;
                if ( !new_cr4.physical_address_extension )
                {
                    inject_hw( 13, true, 0 );
                    return;
                }


                // Shadow strips VMXE (bit 13) so guest doesn't see it
                tools::vmwrite( VMCS_CTRL_CR4_READ_SHADOW, new_cr4.flags );

                new_cr4.flags |= g_msr_cache.cr4_fixed0;
                new_cr4.flags &= g_msr_cache.cr4_fixed1;
                tools::vmwrite( VMCS_GUEST_CR4, new_cr4.flags );
                break;
            }
            }
        }
        else if ( access == 1 )
        {
            uint64_t val = 0;
            switch ( cr_num )
            {
            case 0: val = tools::vmread( VMCS_CTRL_CR0_READ_SHADOW ); break;
            case 3: val = tools::vmread( VMCS_GUEST_CR3 ); break;
            case 4: val = tools::vmread( VMCS_CTRL_CR4_READ_SHADOW ); break;
            }
            write_gpr( gpr_idx, val );
        }
        else if ( access == 2 )
        {
            uint64_t cr0_shadow = tools::vmread( VMCS_CTRL_CR0_READ_SHADOW );
            cr0_shadow &= ~CR0_TASK_SWITCHED_FLAG;
            tools::vmwrite( VMCS_CTRL_CR0_READ_SHADOW, cr0_shadow );
            uint64_t cr0_val = cr0_shadow;
            cr0_val |= g_msr_cache.cr0_fixed0;
            cr0_val &= g_msr_cache.cr0_fixed1;
            tools::vmwrite( VMCS_GUEST_CR0, cr0_val );
        }
        
        move_rip( );
    }

    inline void handle_xsetbv( guest_regs_t* regs )
    {

        auto mask = tools::vmread( VMCS_CTRL_CR4_GUEST_HOST_MASK );

        cr4 cr4;
        cr4.flags = ( tools::vmread( VMCS_CTRL_CR4_READ_SHADOW ) & mask ) | ( tools::vmread( VMCS_GUEST_CR4 ) & ~mask );

        if ( !cr4.os_xsave )
        {
            inject_hw( invalid_opcode );
            return;
        }

        if ( regs->rcx != 0 )
        {
            inject_hw( general_protection );
            return;
        }

        xcr0 new_xcr0;
        new_xcr0.flags = ( regs->rdx << 32 ) | regs->rax;

        cpuid_eax_0d_ecx_00 cpuid_0d;
        __cpuidex( reinterpret_cast< int* >( &cpuid_0d ), 0x0D, 0x00 );

        uint64_t xcr0_unsupported_mask = ~( ( static_cast< uint64_t >(
            cpuid_0d.edx.flags ) << 32 ) | cpuid_0d.eax.flags );


        if ( new_xcr0.flags & xcr0_unsupported_mask ) {
            inject_hw( general_protection );
            return;
        }

        if ( !new_xcr0.x87 ) {
            inject_hw( general_protection );
            return;
        }

        if ( new_xcr0.avx && !new_xcr0.sse ) {
            inject_hw( general_protection );
            return;
        }

        if ( !new_xcr0.avx && ( new_xcr0.opmask || new_xcr0.zmm_hi256 || new_xcr0.zmm_hi16 ) ) {
            inject_hw( general_protection );
            return;
        }

        if ( new_xcr0.bndreg != new_xcr0.bndcsr ) {
            inject_hw( general_protection );
            return;
        }

        if ( new_xcr0.opmask != new_xcr0.zmm_hi256 || new_xcr0.zmm_hi256 != new_xcr0.zmm_hi16 ) {
            inject_hw( general_protection );
            return;
        }

        _xsetbv( ( uint32_t ) regs->rcx, new_xcr0.flags );

        move_rip( );
    }

    inline void handle_getsec( )
    {
        inject_hw( 13, true, 0 );
    }

    inline void handle_invd( )
    {
        __wbinvd( );
        move_rip( );
    }

    inline void handle_vmx_instruction( )
    {
        inject_hw( 6 ); // #ud
    }

    // returns true if devirtualizing (caller must handle VMXOFF)
    inline bool handle_vmcall( guest_regs_t* regs )
    {
        // devirt: r10=0x1377, r8=0xBEEF
        if ( regs->r10 == 0x1377 && regs->r8 == 0xBEEF )
        {
            regs->rax = 1;
            return true;
        }

        inject_hw( 6 ); // #ud
        return false;
    }

    void handle_nmi_window( uint64_t core )
    {
        if ( g_queued_nmis[ core ] == 0 ) return;
        g_queued_nmis[ core ]--;
        inject_nmi( );

        if ( g_queued_nmis[ core ] == 0 )
        {
            ia32_vmx_procbased_ctls_register procbased;
            procbased.flags = tools::vmread( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS );
            procbased.nmi_window_exiting = 0;
            tools::vmwrite( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, procbased.flags );
        }

        if ( g_queued_nmis[ core ] > 0 )
        {
            ia32_vmx_procbased_ctls_register procbased;
            procbased.flags = tools::vmread( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS );
            procbased.nmi_window_exiting = 1;
            tools::vmwrite( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, procbased.flags );
        }
    }

    inline void handle_exception_or_nmi( uint64_t core )
    {
        vmentry_interrupt_information info;
        info.flags = tools::vmread( VMCS_VMEXIT_INTERRUPTION_INFORMATION );

        if ( info.vector != 2 )
        {
            inject_hw( info.vector, info.deliver_error_code, tools::vmread( VMCS_VMEXIT_INTERRUPTION_ERROR_CODE ) );
            return;
        }
        g_queued_nmis[ core ]++;


        ia32_vmx_procbased_ctls_register procbased;
        procbased.flags = tools::vmread( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS );
        procbased.nmi_window_exiting = 1;
        tools::vmwrite( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, procbased.flags );
    }

    inline void handle_dr_access( guest_regs_t* regs )
    {
        // STEALTH: Intercept MOV DR to hide debug register state from anti-cheat.
        // Return zeros for all DRs so EAC doesn't see hardware breakpoints.
        uint64_t qualification = tools::vmread( VMCS_EXIT_QUALIFICATION );
        uint64_t dr_num = qualification & 0x7;
        uint64_t direction = ( qualification >> 4 ) & 0x1;  // 0 = MOV to DR, 1 = MOV from DR
        uint64_t gpr_idx = ( qualification >> 8 ) & 0xF;

        auto read_gpr = [&]( uint64_t idx ) -> uint64_t
            {
                switch ( idx )
                {
                case 0:  return regs->rax;
                case 1:  return regs->rcx;
                case 2:  return regs->rdx;
                case 3:  return regs->rbx;
                case 4:  return tools::vmread( VMCS_GUEST_RSP );
                case 5:  return regs->rbp;
                case 6:  return regs->rsi;
                case 7:  return regs->rdi;
                case 8:  return regs->r8;
                case 9:  return regs->r9;
                case 10: return regs->r10;
                case 11: return regs->r11;
                case 12: return regs->r12;
                case 13: return regs->r13;
                case 14: return regs->r14;
                case 15: return regs->r15;
                default: return 0;
                }
            };

        auto write_gpr = [&]( uint64_t idx, uint64_t val )
            {
                switch ( idx )
                {
                case 0:  regs->rax = val; break;
                case 1:  regs->rcx = val; break;
                case 2:  regs->rdx = val; break;
                case 3:  regs->rbx = val; break;
                case 4:  tools::vmwrite( VMCS_GUEST_RSP, val ); break;
                case 5:  regs->rbp = val; break;
                case 6:  regs->rsi = val; break;
                case 7:  regs->rdi = val; break;
                case 8:  regs->r8 = val; break;
                case 9:  regs->r9 = val; break;
                case 10: regs->r10 = val; break;
                case 11: regs->r11 = val; break;
                case 12: regs->r12 = val; break;
                case 13: regs->r13 = val; break;
                case 14: regs->r14 = val; break;
                case 15: regs->r15 = val; break;
                }
            };

        if ( direction == 1 ) // MOV from DR -> GPR
        {
            // Return 0 for DR0-DR3 (address breakpoints) and DR6 (status)
            // Pass through DR7 (control) since OS needs it for exception dispatch
            uint64_t val = 0;
            if ( dr_num == 7 )
                val = __readdr( 7 );
            write_gpr( gpr_idx, val );
        }
        else // MOV to DR
        {
            uint64_t val = read_gpr( gpr_idx );
            switch ( dr_num )
            {
            case 0: __writedr( 0, val ); break;
            case 1: __writedr( 1, val ); break;
            case 2: __writedr( 2, val ); break;
            case 3: __writedr( 3, val ); break;
            case 6: __writedr( 6, val ); break;
            case 7: __writedr( 7, val ); break;
            }
        }

        move_rip( );
    }

    inline void handle_external_interrupt( )
    {
        // With "acknowledge interrupt on exit" enabled, the vector is in exit interruption info.
        // Re-inject as external interrupt into the guest. No move_rip — the interrupted
        // instruction was not completed.
        vmentry_interrupt_information info;
        info.flags = tools::vmread( VMCS_VMEXIT_INTERRUPTION_INFORMATION );

        vmentry_interrupt_information inject = {};
        inject.vector = info.vector;
        inject.interruption_type = 0; // external interrupt
        inject.valid = 1;
        tools::vmwrite( VMCS_CTRL_VMENTRY_INTERRUPTION_INFORMATION_FIELD, inject.flags );
    }

    inline void handle_ept_violation( )
    {
        // EPT violation — guest tried to access a physical address not mapped in EPT.
        // This shouldn't happen with a full identity map. Log and skip the instruction
        // to avoid an immediate crash, though the guest may misbehave.
        uint64_t gpa = tools::vmread( VMCS_GUEST_PHYSICAL_ADDRESS );
        uint64_t gla = tools::vmread( VMCS_EXIT_QUALIFICATION );
        log::dbg_print( "EPT VIOLATION: gpa=0x%llx qual=0x%llx rip=0x%llx",
            gpa, gla, tools::vmread( VMCS_GUEST_RIP ) );
        // Skip the faulting instruction to prevent infinite loop
        move_rip( );
    }

    inline void handle_ept_misconfiguration( )
    {
        uint64_t gpa = tools::vmread( VMCS_GUEST_PHYSICAL_ADDRESS );
        log::dbg_print( "EPT MISCONFIG: gpa=0x%llx rip=0x%llx",
            gpa, tools::vmread( VMCS_GUEST_RIP ) );
        // Can't easily recover — inject #GP
        inject_hw( 13, true, 0 );
    }
}