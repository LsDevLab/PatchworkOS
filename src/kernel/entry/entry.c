#include <stdint.h>

#include <common/boot_info/boot_info.h>

#include "tty/tty.h"
#include "smp/smp.h"
#include "time/time.h"
#include "debug/debug.h"
#include "kernel/kernel.h"
#include "scheduler/scheduler.h"

void main(BootInfo* bootInfo)
{
    kernel_init(bootInfo);

    tty_acquire();

    for (uint64_t i = 0; i < 16; i++)
    {
        scheduler_spawn("ram:/bin/parent.elf");
    }

    tty_clear();
    tty_set_row(smp_cpu_amount() + 1);
    tty_release();

    //Exit init thread
    scheduler_thread()->state = THREAD_STATE_KILLED;
    scheduler_yield();
}
