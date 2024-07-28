#include "trampoline.h"

#include "pml.h"
#include "pmm.h"
#include "utils.h"
#include "vmm.h"

#include <stdlib.h>
#include <string.h>

static void* backupBuffer;

void trampoline_init(void)
{
    pml_map(vmm_kernel_pml(), TRAMPOLINE_PHYSICAL_START, TRAMPOLINE_PHYSICAL_START, 1, PAGE_WRITE);

    backupBuffer = pmm_alloc();
    memcpy(backupBuffer, TRAMPOLINE_PHYSICAL_START, PAGE_SIZE);
    memcpy(TRAMPOLINE_PHYSICAL_START, trampoline_virtual_start, PAGE_SIZE);

    WRITE_64(TRAMPOLINE_PML_ADDRESS, VMM_HIGHER_TO_LOWER(vmm_kernel_pml()));
    WRITE_64(TRAMPOLINE_ENTRY_ADDRESS, smp_entry);
}

void trampoline_cpu_setup(cpu_t* cpu)
{
    WRITE_64(TRAMPOLINE_STACK_TOP_ADDRESS, (uint64_t)cpu->idleStack + CPU_IDLE_STACK_SIZE);
}

void trampoline_cleanup(void)
{
    memcpy(TRAMPOLINE_PHYSICAL_START, backupBuffer, PAGE_SIZE);
    pmm_free(backupBuffer);

    pml_unmap(vmm_kernel_pml(), TRAMPOLINE_PHYSICAL_START, 1);
}
