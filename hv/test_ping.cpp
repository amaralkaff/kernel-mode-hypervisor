#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <intrin.h>

#define HV_CPUID_PING 0x13370000
#define HV_SIGNATURE  0x72766D68  // 'hvmr' little-endian

int main()
{
    int regs[4] = {};
    __cpuid(regs, HV_CPUID_PING);

    printf("CPUID 0x13370000 result:\n");
    printf("  EAX = 0x%08X\n", regs[0]);
    printf("  EBX = 0x%08X\n", regs[1]);
    printf("  ECX = 0x%08X\n", regs[2]);
    printf("  EDX = 0x%08X\n", regs[3]);

    if ((uint32_t)regs[1] == HV_SIGNATURE)
        printf("\n[+] Hypervisor detected! Ping OK.\n");
    else
        printf("\n[-] Hypervisor NOT detected. EBX=0x%08X, expected 0x%08X\n", regs[1], HV_SIGNATURE);

    system("pause");
    return 0;
}
