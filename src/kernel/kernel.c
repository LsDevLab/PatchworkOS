#include "kernel.h"

#include "apic.h"
#include "compositor.h"
#include "const.h"
#include "gdt.h"
#include "hpet.h"
#include "idt.h"
#include "madt.h"
#include "pic.h"
#include "pmm.h"
#include "ps2.h"
#include "ramfs.h"
#include "regs.h"
#include "rsdt.h"
#include "sched.h"
#include "simd.h"
#include "smp.h"
#include "sysfs.h"
#include "time.h"
#include "tty.h"
#include "vfs.h"
#include "vmm.h"

#include <libs/std/internal/init.h>

static void boot_info_deallocate(BootInfo* bootInfo)
{
    tty_start_message("Deallocating boot info");

    EfiMemoryMap* memoryMap = &bootInfo->memoryMap;
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        if (desc->type == EFI_MEMORY_TYPE_BOOT_INFO)
        {
            pmm_free_pages(desc->physicalStart, desc->amountOfPages);
        }
    }

    tty_end_message(TTY_MESSAGE_OK);
}

void kernel_init(BootInfo* bootInfo)
{
    pmm_init(&bootInfo->memoryMap);
    vmm_init(&bootInfo->memoryMap);

    _StdInit();

    tty_init(&bootInfo->gopBuffer, &bootInfo->font);
    tty_print("Hello from the kernel!\n");

    gdt_init();
    idt_init();

    rsdt_init(bootInfo->rsdp);
    hpet_init();
    madt_init();
    apic_init();

    smp_init();
    kernel_cpu_init();

    pic_init();
    time_init();

    sched_start();

    vfs_init();
    sysfs_init();
    ramfs_init(bootInfo->ramRoot);

    ps2_init();
    const_init();
    compositor_init(&bootInfo->gopBuffer);

    boot_info_deallocate(bootInfo);
}

void kernel_cpu_init(void)
{
    Cpu* cpu = smp_self_brute();
    msr_write(MSR_CPU_ID, cpu->id);

    local_apic_init();
    simd_init();

    gdt_load();
    idt_load();
    gdt_load_tss(&cpu->tss);

    cr4_write(cr4_read() | CR4_PAGE_GLOBAL_ENABLE);
}
